/**
 * @file metric_collection_bench.cpp
 * @brief Benchmark for metric collection performance
 * @details Measures overhead of metric collection on monitored operations
 *
 * Target Metrics:
 * - Collection overhead: < 1% of operation time
 * - Metric recording latency: < 100ns
 * - Concurrent collection performance
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

using namespace monitoring_system;

//-----------------------------------------------------------------------------
// Metric Recording Latency
//-----------------------------------------------------------------------------

static void BM_MetricRecording_Single(benchmark::State& state) {
    auto monitor = std::make_shared<performance_monitor>();
    size_t count = 0;

    for (auto _ : state) {
        monitor->record_metric("test_metric", 42.0);
        count++;
    }

    state.SetItemsProcessed(count);
    state.SetLabel("single_metric");
}
BENCHMARK(BM_MetricRecording_Single);

static void BM_MetricRecording_Multiple(benchmark::State& state) {
    auto monitor = std::make_shared<performance_monitor>();
    size_t count = 0;

    for (auto _ : state) {
        monitor->record_metric("metric_1", 1.0);
        monitor->record_metric("metric_2", 2.0);
        monitor->record_metric("metric_3", 3.0);
        monitor->record_metric("metric_4", 4.0);
        monitor->record_metric("metric_5", 5.0);
        count += 5;
    }

    state.SetItemsProcessed(count);
    state.SetLabel("five_metrics");
}
BENCHMARK(BM_MetricRecording_Multiple);

//-----------------------------------------------------------------------------
// Collection Overhead on Actual Operations
//-----------------------------------------------------------------------------

static void BM_CollectionOverhead_NoMonitoring(benchmark::State& state) {
    // Simulate a monitored operation without monitoring
    std::atomic<uint64_t> counter{0};

    for (auto _ : state) {
        // Simulated work
        for (int i = 0; i < 100; ++i) {
            counter.fetch_add(1, std::memory_order_relaxed);
        }
    }

    state.SetLabel("baseline_no_monitoring");
}
BENCHMARK(BM_CollectionOverhead_NoMonitoring);

static void BM_CollectionOverhead_WithMonitoring(benchmark::State& state) {
    auto monitor = std::make_shared<performance_monitor>();
    std::atomic<uint64_t> counter{0};

    for (auto _ : state) {
        auto start = std::chrono::high_resolution_clock::now();

        // Simulated work
        for (int i = 0; i < 100; ++i) {
            counter.fetch_add(1, std::memory_order_relaxed);
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
            end - start).count();

        monitor->record_metric("operation_duration", static_cast<double>(duration));
    }

    state.SetLabel("with_monitoring");
}
BENCHMARK(BM_CollectionOverhead_WithMonitoring);

//-----------------------------------------------------------------------------
// Performance Metrics Calculation
//-----------------------------------------------------------------------------

static void BM_PerformanceMetrics_Update(benchmark::State& state) {
    performance_metrics metrics;
    metrics.operation_name = "test_operation";

    std::vector<std::chrono::nanoseconds> durations;
    durations.reserve(state.range(0));

    // Generate sample durations
    for (int i = 0; i < state.range(0); ++i) {
        durations.push_back(std::chrono::nanoseconds(1000 + (i * 10)));
    }

    for (auto _ : state) {
        metrics.update_statistics(durations);
    }

    state.SetItemsProcessed(state.iterations() * state.range(0));
    state.SetLabel(std::to_string(state.range(0)) + "_samples");
}
BENCHMARK(BM_PerformanceMetrics_Update)
    ->Arg(10)
    ->Arg(100)
    ->Arg(1000)
    ->Arg(10000);

//-----------------------------------------------------------------------------
// System Metrics Collection
//-----------------------------------------------------------------------------

static void BM_SystemMetrics_Collection(benchmark::State& state) {
    auto monitor = std::make_shared<performance_monitor>();

    for (auto _ : state) {
        auto metrics = monitor->get_system_metrics();
        benchmark::DoNotOptimize(metrics);
    }

    state.SetLabel("cpu_memory_threads");
}
BENCHMARK(BM_SystemMetrics_Collection);

//-----------------------------------------------------------------------------
// Concurrent Metric Recording
//-----------------------------------------------------------------------------

static void BM_ConcurrentMetricRecording(benchmark::State& state) {
    static std::shared_ptr<performance_monitor> shared_monitor;

    if (state.thread_index() == 0) {
        shared_monitor = std::make_shared<performance_monitor>();
    }

    std::atomic<size_t> count{0};
    const int thread_id = state.thread_index();

    for (auto _ : state) {
        std::string metric_name = "thread_" + std::to_string(thread_id) + "_metric";
        shared_monitor->record_metric(metric_name, static_cast<double>(count++));
    }

    state.SetItemsProcessed(count.load());
    state.counters["thread_count"] = state.threads();

    if (state.thread_index() == 0) {
        shared_monitor.reset();
    }
}
BENCHMARK(BM_ConcurrentMetricRecording)
    ->Threads(2)
    ->Threads(4)
    ->Threads(8)
    ->Threads(16)
    ->UseRealTime();

//-----------------------------------------------------------------------------
// Metric Retrieval Performance
//-----------------------------------------------------------------------------

static void BM_MetricRetrieval(benchmark::State& state) {
    auto monitor = std::make_shared<performance_monitor>();

    // Populate with metrics
    for (int i = 0; i < state.range(0); ++i) {
        std::string metric_name = "metric_" + std::to_string(i);
        monitor->record_metric(metric_name, static_cast<double>(i));
    }

    size_t retrievals = 0;

    for (auto _ : state) {
        auto metrics = monitor->get_all_metrics();
        benchmark::DoNotOptimize(metrics);
        retrievals++;
    }

    state.SetItemsProcessed(retrievals * state.range(0));
    state.SetLabel(std::to_string(state.range(0)) + "_metrics");
}
BENCHMARK(BM_MetricRetrieval)
    ->Arg(10)
    ->Arg(100)
    ->Arg(1000)
    ->Arg(10000);

//-----------------------------------------------------------------------------
// Metric Aggregation
//-----------------------------------------------------------------------------

static void BM_MetricAggregation(benchmark::State& state) {
    auto monitor = std::make_shared<performance_monitor>();

    // Record multiple values for same metric
    for (int i = 0; i < state.range(0); ++i) {
        monitor->record_metric("aggregated_metric", static_cast<double>(i));
    }

    for (auto _ : state) {
        auto aggregated = monitor->get_aggregated_metric("aggregated_metric");
        benchmark::DoNotOptimize(aggregated);
    }

    state.SetLabel(std::to_string(state.range(0)) + "_values");
}
BENCHMARK(BM_MetricAggregation)
    ->Arg(100)
    ->Arg(1000)
    ->Arg(10000);

//-----------------------------------------------------------------------------
// Scoped Performance Measurement
//-----------------------------------------------------------------------------

static void BM_ScopedMeasurement(benchmark::State& state) {
    auto monitor = std::make_shared<performance_monitor>();

    for (auto _ : state) {
        auto scope = monitor->measure_scope("scoped_operation");

        // Simulated work
        std::this_thread::sleep_for(std::chrono::microseconds(10));
    }

    state.SetLabel("10us_operation");
}
BENCHMARK(BM_ScopedMeasurement);

//-----------------------------------------------------------------------------
// Throughput with Monitoring
//-----------------------------------------------------------------------------

static void BM_ThroughputWithMonitoring(benchmark::State& state) {
    auto monitor = std::make_shared<performance_monitor>();
    std::atomic<uint64_t> operations{0};

    for (auto _ : state) {
        // Simulate operation
        operations.fetch_add(1, std::memory_order_relaxed);

        // Record metric
        monitor->record_metric("operations_total", static_cast<double>(operations.load()));
    }

    state.SetItemsProcessed(operations.load());
    state.counters["ops/sec"] = benchmark::Counter(
        operations.load(), benchmark::Counter::kIsRate);
}
BENCHMARK(BM_ThroughputWithMonitoring);

//-----------------------------------------------------------------------------
// Memory Overhead
//-----------------------------------------------------------------------------

static void BM_MemoryOverhead(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        auto monitor = std::make_shared<performance_monitor>();
        state.ResumeTiming();

        // Record metrics
        for (int i = 0; i < state.range(0); ++i) {
            std::string metric_name = "metric_" + std::to_string(i);
            monitor->record_metric(metric_name, static_cast<double>(i));
        }

        state.PauseTiming();
        monitor.reset();
        state.ResumeTiming();
    }

    state.SetLabel(std::to_string(state.range(0)) + "_metrics");
}
BENCHMARK(BM_MemoryOverhead)
    ->Arg(10)
    ->Arg(100)
    ->Arg(1000);
