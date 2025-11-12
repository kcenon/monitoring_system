// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
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
 * @file metric_collection_bench.cpp
 * @brief Benchmark for metric collection performance
 * @details Measures overhead of performance profiler operations
 *
 * Target Metrics:
 * - Sample recording latency: < 100ns
 * - Metrics retrieval latency: < 1μs
 * - Concurrent recording performance
 *
 * Phase 0, Task 0.2: Baseline Performance Benchmarking
 */

#include <benchmark/benchmark.h>
#include <kcenon/monitoring/core/performance_monitor.h>
#include <memory>
#include <string>
#include <chrono>
#include <thread>
#include <atomic>

using namespace kcenon::monitoring;

//-----------------------------------------------------------------------------
// Metric Recording Latency
//-----------------------------------------------------------------------------

static void BM_ProfilerRecording_Single(benchmark::State& state) {
    performance_profiler profiler;
    size_t count = 0;

    for (auto _ : state) {
        profiler.record_sample("test_operation", std::chrono::nanoseconds(100));
        count++;
    }

    state.SetItemsProcessed(count);
    state.SetLabel("single_sample");
}
BENCHMARK(BM_ProfilerRecording_Single);

static void BM_ProfilerRecording_Multiple(benchmark::State& state) {
    performance_profiler profiler;
    size_t count = 0;

    for (auto _ : state) {
        profiler.record_sample("op_1", std::chrono::nanoseconds(100));
        profiler.record_sample("op_2", std::chrono::nanoseconds(200));
        profiler.record_sample("op_3", std::chrono::nanoseconds(300));
        profiler.record_sample("op_4", std::chrono::nanoseconds(400));
        profiler.record_sample("op_5", std::chrono::nanoseconds(500));
        count += 5;
    }

    state.SetItemsProcessed(count);
    state.SetLabel("multiple_samples");
}
BENCHMARK(BM_ProfilerRecording_Multiple);

//-----------------------------------------------------------------------------
// Metrics Retrieval Performance
//-----------------------------------------------------------------------------

static void BM_ProfilerRetrieval_Single(benchmark::State& state) {
    performance_profiler profiler;

    // Pre-populate with samples
    for (int i = 0; i < 1000; ++i) {
        profiler.record_sample("test_op", std::chrono::nanoseconds(100 + i));
    }

    size_t count = 0;
    for (auto _ : state) {
        auto result = profiler.get_metrics("test_op");
        benchmark::DoNotOptimize(result);
        count++;
    }

    state.SetItemsProcessed(count);
    state.SetLabel("retrieve_metrics");
}
BENCHMARK(BM_ProfilerRetrieval_Single);

static void BM_ProfilerRetrieval_All(benchmark::State& state) {
    performance_profiler profiler;

    // Pre-populate multiple operations
    for (int op = 0; op < 10; ++op) {
        std::string op_name = "operation_" + std::to_string(op);
        for (int i = 0; i < 100; ++i) {
            profiler.record_sample(op_name, std::chrono::nanoseconds(100 + i));
        }
    }

    size_t count = 0;
    for (auto _ : state) {
        auto metrics = profiler.get_all_metrics();
        benchmark::DoNotOptimize(metrics);
        count++;
    }

    state.SetItemsProcessed(count);
    state.SetLabel("retrieve_all_metrics");
}
BENCHMARK(BM_ProfilerRetrieval_All);

//-----------------------------------------------------------------------------
// Concurrent Recording
//-----------------------------------------------------------------------------

static void BM_ProfilerRecording_Concurrent(benchmark::State& state) {
    static performance_profiler profiler;
    std::string op_name = "thread_" + std::to_string(state.thread_index());

    size_t count = 0;
    for (auto _ : state) {
        profiler.record_sample(op_name, std::chrono::nanoseconds(100));
        count++;
    }

    state.SetItemsProcessed(count);
}
BENCHMARK(BM_ProfilerRecording_Concurrent)->Threads(4);

//-----------------------------------------------------------------------------
// Scoped Timer Overhead
//-----------------------------------------------------------------------------

static void BM_ScopedTimer_Overhead(benchmark::State& state) {
    performance_profiler profiler;
    size_t count = 0;

    for (auto _ : state) {
        scoped_timer timer(&profiler, "scoped_op");
        // Simulate minimal work
        benchmark::DoNotOptimize(count);
        count++;
    }

    state.SetItemsProcessed(count);
    state.SetLabel("scoped_timer");
}
BENCHMARK(BM_ScopedTimer_Overhead);
