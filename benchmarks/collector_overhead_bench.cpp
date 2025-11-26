// BSD 3-Clause License
//
// Copyright (c) 2021-2025, kcenon
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/**
 * @file collector_overhead_bench.cpp
 * @brief Comprehensive benchmark for collector overhead measurement
 * @details Measures collection overhead for thread-local buffers and central collector
 *
 * Target Metrics (ARC-004):
 * - Thread-local buffer record: < 50ns
 * - Thread-local buffer + flush cycle: < 1us average
 * - Central collector batch receive: < 10us per batch
 * - Collection overhead: < 1% of monitored workload
 * - Concurrent collection performance: > 80% scaling at 8 threads
 *
 * Phase 2: Metric Collection Overhead Optimization
 */

#include <benchmark/benchmark.h>
#include <kcenon/monitoring/core/thread_local_buffer.h>
#include <kcenon/monitoring/core/central_collector.h>
#include <memory>
#include <string>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>

using namespace kcenon::monitoring;

//-----------------------------------------------------------------------------
// Thread-Local Buffer Benchmarks
//-----------------------------------------------------------------------------

/**
 * @brief Measure raw record latency (buffer not full)
 * Target: < 50ns per record
 */
static void BM_TLSBuffer_Record_Single(benchmark::State& state) {
    auto collector = std::make_shared<central_collector>();
    thread_local_buffer buffer(256, collector);

    metric_sample sample("test_operation", std::chrono::nanoseconds(100), true);
    size_t count = 0;

    for (auto _ : state) {
        buffer.record(sample);
        ++count;

        // Flush periodically to prevent buffer full
        if (count % 200 == 0) {
            state.PauseTiming();
            buffer.flush();
            state.ResumeTiming();
        }
    }

    state.SetItemsProcessed(count);
    state.SetBytesProcessed(count * sizeof(metric_sample));
}
BENCHMARK(BM_TLSBuffer_Record_Single);

/**
 * @brief Measure record with auto-flush (realistic usage)
 * Target: < 100ns average including flush amortization
 */
static void BM_TLSBuffer_Record_AutoFlush(benchmark::State& state) {
    auto collector = std::make_shared<central_collector>();
    thread_local_buffer buffer(256, collector);

    metric_sample sample("test_operation", std::chrono::nanoseconds(100), true);
    size_t count = 0;

    for (auto _ : state) {
        buffer.record_auto_flush(sample);
        ++count;
    }

    state.SetItemsProcessed(count);
    state.SetLabel("with_auto_flush");
}
BENCHMARK(BM_TLSBuffer_Record_AutoFlush);

/**
 * @brief Measure flush latency for full buffer
 * Target: < 5us for 256 samples
 */
static void BM_TLSBuffer_Flush(benchmark::State& state) {
    auto collector = std::make_shared<central_collector>();
    thread_local_buffer buffer(256, collector);

    // Pre-fill buffer
    metric_sample sample("test_operation", std::chrono::nanoseconds(100), true);

    size_t count = 0;
    for (auto _ : state) {
        state.PauseTiming();
        // Fill buffer
        for (size_t i = 0; i < 256; ++i) {
            buffer.record(sample);
        }
        state.ResumeTiming();

        buffer.flush();
        ++count;
    }

    state.SetItemsProcessed(count * 256);
    state.SetLabel("flush_256_samples");
}
BENCHMARK(BM_TLSBuffer_Flush);

/**
 * @brief Measure various buffer sizes impact on performance
 */
static void BM_TLSBuffer_BufferSize(benchmark::State& state) {
    const size_t buffer_size = state.range(0);
    auto collector = std::make_shared<central_collector>();
    thread_local_buffer buffer(buffer_size, collector);

    metric_sample sample("test_operation", std::chrono::nanoseconds(100), true);
    size_t count = 0;

    for (auto _ : state) {
        buffer.record_auto_flush(sample);
        ++count;
    }

    state.SetItemsProcessed(count);
    state.SetLabel("buffer_size_" + std::to_string(buffer_size));
}
BENCHMARK(BM_TLSBuffer_BufferSize)->Arg(64)->Arg(128)->Arg(256)->Arg(512)->Arg(1024);

//-----------------------------------------------------------------------------
// Central Collector Benchmarks
//-----------------------------------------------------------------------------

/**
 * @brief Measure batch receive latency
 * Target: < 10us per batch of 256 samples
 */
static void BM_CentralCollector_ReceiveBatch(benchmark::State& state) {
    const size_t batch_size = state.range(0);
    central_collector collector;

    // Prepare batch
    std::vector<metric_sample> batch;
    batch.reserve(batch_size);
    for (size_t i = 0; i < batch_size; ++i) {
        batch.emplace_back("operation_" + std::to_string(i % 10),
                          std::chrono::nanoseconds(100 + i),
                          true);
    }

    size_t count = 0;
    for (auto _ : state) {
        collector.receive_batch(batch);
        ++count;
    }

    state.SetItemsProcessed(count * batch_size);
    state.SetLabel("batch_" + std::to_string(batch_size));
}
BENCHMARK(BM_CentralCollector_ReceiveBatch)->Arg(64)->Arg(128)->Arg(256)->Arg(512);

/**
 * @brief Measure central collector with single operation (best case)
 * Tests hot path optimization for existing profiles
 */
static void BM_CentralCollector_SingleOperation(benchmark::State& state) {
    central_collector collector;

    std::vector<metric_sample> batch;
    batch.reserve(256);
    for (size_t i = 0; i < 256; ++i) {
        batch.emplace_back("single_op", std::chrono::nanoseconds(100 + i), true);
    }

    size_t count = 0;
    for (auto _ : state) {
        collector.receive_batch(batch);
        ++count;
    }

    state.SetItemsProcessed(count * 256);
    state.SetLabel("single_operation_hot_path");
}
BENCHMARK(BM_CentralCollector_SingleOperation);

/**
 * @brief Measure central collector with many operations (worst case)
 * Tests profile lookup and creation overhead
 */
static void BM_CentralCollector_ManyOperations(benchmark::State& state) {
    const size_t num_operations = state.range(0);
    central_collector collector;

    std::vector<metric_sample> batch;
    batch.reserve(256);
    for (size_t i = 0; i < 256; ++i) {
        batch.emplace_back("operation_" + std::to_string(i % num_operations),
                          std::chrono::nanoseconds(100 + i),
                          true);
    }

    size_t count = 0;
    for (auto _ : state) {
        collector.receive_batch(batch);
        ++count;
    }

    state.SetItemsProcessed(count * 256);
}
BENCHMARK(BM_CentralCollector_ManyOperations)->Arg(10)->Arg(50)->Arg(100)->Arg(500);

/**
 * @brief Measure profile retrieval latency
 * Target: < 1us for single profile lookup
 */
static void BM_CentralCollector_GetProfile(benchmark::State& state) {
    central_collector collector;

    // Pre-populate
    std::vector<metric_sample> batch;
    for (size_t i = 0; i < 100; ++i) {
        batch.emplace_back("target_op", std::chrono::nanoseconds(100 + i), true);
    }
    collector.receive_batch(batch);

    size_t count = 0;
    for (auto _ : state) {
        auto result = collector.get_profile("target_op");
        benchmark::DoNotOptimize(result);
        ++count;
    }

    state.SetItemsProcessed(count);
    state.SetLabel("profile_lookup");
}
BENCHMARK(BM_CentralCollector_GetProfile);

//-----------------------------------------------------------------------------
// Concurrent Collection Benchmarks
//-----------------------------------------------------------------------------

/**
 * @brief Measure concurrent TLS buffer performance
 * Target: > 80% linear scaling at 8 threads
 */
static void BM_Concurrent_TLSBuffer(benchmark::State& state) {
    static auto collector = std::make_shared<central_collector>();
    thread_local_buffer buffer(256, collector);

    std::string op_name = "thread_" + std::to_string(state.thread_index());
    metric_sample sample(op_name, std::chrono::nanoseconds(100), true);

    size_t count = 0;
    for (auto _ : state) {
        buffer.record_auto_flush(sample);
        ++count;
    }

    state.SetItemsProcessed(count);
}
BENCHMARK(BM_Concurrent_TLSBuffer)->Threads(1)->Threads(2)->Threads(4)->Threads(8);

/**
 * @brief Measure concurrent flush to central collector
 * Tests lock contention under concurrent flush
 */
static void BM_Concurrent_Flush(benchmark::State& state) {
    static auto collector = std::make_shared<central_collector>();
    thread_local_buffer buffer(64, collector);

    std::string op_name = "thread_" + std::to_string(state.thread_index());
    metric_sample sample(op_name, std::chrono::nanoseconds(100), true);

    size_t count = 0;
    for (auto _ : state) {
        // Fill and flush
        for (size_t i = 0; i < 64; ++i) {
            buffer.record(sample);
        }
        buffer.flush();
        ++count;
    }

    state.SetItemsProcessed(count * 64);
    state.SetLabel("concurrent_flush");
}
BENCHMARK(BM_Concurrent_Flush)->Threads(1)->Threads(2)->Threads(4)->Threads(8);

//-----------------------------------------------------------------------------
// Workload Overhead Measurement
//-----------------------------------------------------------------------------

/**
 * @brief Baseline: workload without monitoring
 */
static void BM_Workload_Baseline(benchmark::State& state) {
    volatile int result = 0;
    size_t count = 0;

    for (auto _ : state) {
        // Simulate computation workload
        int sum = 0;
        for (int i = 0; i < 100; ++i) {
            sum += i * i;
        }
        result = sum;
        benchmark::DoNotOptimize(result);
        ++count;
    }

    state.SetItemsProcessed(count);
    state.SetLabel("no_monitoring");
}
BENCHMARK(BM_Workload_Baseline);

/**
 * @brief Workload with monitoring overhead
 * Target: < 1% overhead vs baseline
 */
static void BM_Workload_WithMonitoring(benchmark::State& state) {
    auto collector = std::make_shared<central_collector>();
    thread_local_buffer buffer(256, collector);

    volatile int result = 0;
    size_t count = 0;

    for (auto _ : state) {
        auto start = std::chrono::steady_clock::now();

        // Simulate computation workload
        int sum = 0;
        for (int i = 0; i < 100; ++i) {
            sum += i * i;
        }
        result = sum;
        benchmark::DoNotOptimize(result);

        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

        metric_sample sample("workload_op", duration, true);
        buffer.record_auto_flush(sample);
        ++count;
    }

    state.SetItemsProcessed(count);
    state.SetLabel("with_monitoring");
}
BENCHMARK(BM_Workload_WithMonitoring);

/**
 * @brief I/O-like workload baseline
 */
static void BM_IO_Workload_Baseline(benchmark::State& state) {
    size_t count = 0;

    for (auto _ : state) {
        // Simulate I/O delay (1us)
        std::this_thread::sleep_for(std::chrono::microseconds(1));
        ++count;
    }

    state.SetItemsProcessed(count);
    state.SetLabel("io_no_monitoring");
}
BENCHMARK(BM_IO_Workload_Baseline);

/**
 * @brief I/O-like workload with monitoring
 * Target: < 1% overhead for I/O operations
 */
static void BM_IO_Workload_WithMonitoring(benchmark::State& state) {
    auto collector = std::make_shared<central_collector>();
    thread_local_buffer buffer(256, collector);

    size_t count = 0;

    for (auto _ : state) {
        auto start = std::chrono::steady_clock::now();

        // Simulate I/O delay (1us)
        std::this_thread::sleep_for(std::chrono::microseconds(1));

        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

        metric_sample sample("io_op", duration, true);
        buffer.record_auto_flush(sample);
        ++count;
    }

    state.SetItemsProcessed(count);
    state.SetLabel("io_with_monitoring");
}
BENCHMARK(BM_IO_Workload_WithMonitoring);

//-----------------------------------------------------------------------------
// Memory Efficiency Benchmarks
//-----------------------------------------------------------------------------

/**
 * @brief Measure memory overhead per sample
 */
static void BM_Memory_SampleSize(benchmark::State& state) {
    auto collector = std::make_shared<central_collector>();
    thread_local_buffer buffer(1024, collector);

    std::string op_name = "test_op_with_moderate_length_name";
    metric_sample sample(op_name, std::chrono::nanoseconds(100), true);

    size_t count = 0;
    for (auto _ : state) {
        buffer.record(sample);
        ++count;

        if (count % 900 == 0) {
            state.PauseTiming();
            buffer.flush();
            state.ResumeTiming();
        }
    }

    state.SetItemsProcessed(count);
    state.SetBytesProcessed(count * sizeof(metric_sample));
    state.SetLabel("sample_size_bytes");
}
BENCHMARK(BM_Memory_SampleSize);

// Note: main_bench.cpp provides the main entry point
