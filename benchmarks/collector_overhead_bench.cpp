/**
 * @file collector_overhead_bench.cpp
 * @brief Benchmark for system resource collectors
 * @details Measures overhead of various metric collectors
 *
 * Target Metrics:
 * - Collection overhead: < 1% of CPU time
 * - Collection latency: < 1ms
 * - Memory footprint: minimal
 *
 * Phase 0, Task 0.2: Baseline Performance Benchmarking
 */

#include <benchmark/benchmark.h>
#include <kcenon/monitoring/collectors/system_resource_collector.h>
#include <memory>
#include <chrono>
#include <atomic>

using namespace monitoring_system;

//-----------------------------------------------------------------------------
// System Resource Collection Latency
//-----------------------------------------------------------------------------

static void BM_SystemResourceCollection(benchmark::State& state) {
    auto collector = std::make_shared<system_resource_collector>();

    for (auto _ : state) {
        auto metrics = collector->collect();
        benchmark::DoNotOptimize(metrics);
    }

    state.SetLabel("cpu_memory_collection");
}
BENCHMARK(BM_SystemResourceCollection);

//-----------------------------------------------------------------------------
// Collection Frequency Impact
//-----------------------------------------------------------------------------

static void BM_CollectionFrequency(benchmark::State& state) {
    auto collector = std::make_shared<system_resource_collector>();
    const auto interval = std::chrono::milliseconds(state.range(0));

    size_t collections = 0;
    auto last_collection = std::chrono::steady_clock::now();

    for (auto _ : state) {
        auto now = std::chrono::steady_clock::now();

        if (now - last_collection >= interval) {
            auto metrics = collector->collect();
            benchmark::DoNotOptimize(metrics);
            last_collection = now;
            collections++;
        }

        // Simulate work
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    state.counters["collections"] = collections;
    state.counters["interval_ms"] = state.range(0);
}
BENCHMARK(BM_CollectionFrequency)
    ->Arg(10)      // 10ms
    ->Arg(100)     // 100ms
    ->Arg(1000)    // 1s
    ->MinTime(2.0);

//-----------------------------------------------------------------------------
// Concurrent Collection
//-----------------------------------------------------------------------------

static void BM_ConcurrentCollection(benchmark::State& state) {
    static std::shared_ptr<system_resource_collector> shared_collector;

    if (state.thread_index() == 0) {
        shared_collector = std::make_shared<system_resource_collector>();
    }

    std::atomic<size_t> collections{0};

    for (auto _ : state) {
        auto metrics = shared_collector->collect();
        benchmark::DoNotOptimize(metrics);
        collections.fetch_add(1, std::memory_order_relaxed);
    }

    state.SetItemsProcessed(collections.load());
    state.counters["thread_count"] = state.threads();

    if (state.thread_index() == 0) {
        shared_collector.reset();
    }
}
BENCHMARK(BM_ConcurrentCollection)
    ->Threads(2)
    ->Threads(4)
    ->Threads(8)
    ->UseRealTime();

//-----------------------------------------------------------------------------
// Collection with Simulated Workload
//-----------------------------------------------------------------------------

static void BM_CollectionWithWorkload_NoMonitoring(benchmark::State& state) {
    std::atomic<uint64_t> counter{0};

    for (auto _ : state) {
        // Simulated CPU-intensive work
        for (int i = 0; i < 1000; ++i) {
            counter.fetch_add(i, std::memory_order_relaxed);
        }
    }

    state.SetLabel("baseline_no_monitoring");
}
BENCHMARK(BM_CollectionWithWorkload_NoMonitoring);

static void BM_CollectionWithWorkload_WithMonitoring(benchmark::State& state) {
    auto collector = std::make_shared<system_resource_collector>();
    std::atomic<uint64_t> counter{0};

    const auto interval = std::chrono::milliseconds(100);
    auto last_collection = std::chrono::steady_clock::now();

    for (auto _ : state) {
        // Simulated CPU-intensive work
        for (int i = 0; i < 1000; ++i) {
            counter.fetch_add(i, std::memory_order_relaxed);
        }

        // Periodic collection
        auto now = std::chrono::steady_clock::now();
        if (now - last_collection >= interval) {
            auto metrics = collector->collect();
            benchmark::DoNotOptimize(metrics);
            last_collection = now;
        }
    }

    state.SetLabel("with_monitoring_100ms");
}
BENCHMARK(BM_CollectionWithWorkload_WithMonitoring);

//-----------------------------------------------------------------------------
// Memory Overhead of Collectors
//-----------------------------------------------------------------------------

static void BM_CollectorMemoryOverhead(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        auto collector = std::make_shared<system_resource_collector>();
        state.ResumeTiming();

        // Collect multiple times to accumulate history
        for (int i = 0; i < state.range(0); ++i) {
            auto metrics = collector->collect();
            benchmark::DoNotOptimize(metrics);
        }

        state.PauseTiming();
        collector.reset();
        state.ResumeTiming();
    }

    state.SetLabel(std::to_string(state.range(0)) + "_collections");
}
BENCHMARK(BM_CollectorMemoryOverhead)
    ->Arg(10)
    ->Arg(100)
    ->Arg(1000);

//-----------------------------------------------------------------------------
// Collection Accuracy vs Performance
//-----------------------------------------------------------------------------

static void BM_CollectionAccuracy(benchmark::State& state) {
    auto collector = std::make_shared<system_resource_collector>();

    // Warm up
    for (int i = 0; i < 10; ++i) {
        collector->collect();
    }

    std::vector<double> cpu_readings;
    cpu_readings.reserve(state.max_iterations);

    for (auto _ : state) {
        auto metrics = collector->collect();
        cpu_readings.push_back(metrics.cpu_usage_percent);
    }

    // Calculate variance (lower is better for stability)
    if (!cpu_readings.empty()) {
        double mean = std::accumulate(cpu_readings.begin(), cpu_readings.end(), 0.0) /
                     cpu_readings.size();
        double variance = 0.0;
        for (double reading : cpu_readings) {
            variance += (reading - mean) * (reading - mean);
        }
        variance /= cpu_readings.size();

        state.counters["mean_cpu"] = mean;
        state.counters["variance"] = variance;
    }
}
BENCHMARK(BM_CollectionAccuracy)->Iterations(100);

//-----------------------------------------------------------------------------
// Collector Enable/Disable Overhead
//-----------------------------------------------------------------------------

static void BM_CollectorEnableDisable(benchmark::State& state) {
    auto collector = std::make_shared<system_resource_collector>();

    for (auto _ : state) {
        collector->enable();
        collector->disable();
    }

    state.SetLabel("enable_disable_cycle");
}
BENCHMARK(BM_CollectorEnableDisable);

//-----------------------------------------------------------------------------
// Multiple Collectors Running
//-----------------------------------------------------------------------------

static void BM_MultipleCollectors(benchmark::State& state) {
    const int collector_count = state.range(0);

    std::vector<std::shared_ptr<system_resource_collector>> collectors;
    collectors.reserve(collector_count);

    for (int i = 0; i < collector_count; ++i) {
        collectors.push_back(std::make_shared<system_resource_collector>());
    }

    size_t total_collections = 0;

    for (auto _ : state) {
        for (auto& collector : collectors) {
            auto metrics = collector->collect();
            benchmark::DoNotOptimize(metrics);
            total_collections++;
        }
    }

    state.SetItemsProcessed(total_collections);
    state.counters["collector_count"] = collector_count;
}
BENCHMARK(BM_MultipleCollectors)
    ->Arg(1)
    ->Arg(5)
    ->Arg(10)
    ->Arg(20);

//-----------------------------------------------------------------------------
// Collection Throughput
//-----------------------------------------------------------------------------

static void BM_CollectionThroughput(benchmark::State& state) {
    auto collector = std::make_shared<system_resource_collector>();
    size_t collections = 0;

    for (auto _ : state) {
        auto metrics = collector->collect();
        benchmark::DoNotOptimize(metrics);
        collections++;
    }

    state.SetItemsProcessed(collections);
    state.counters["collections/sec"] = benchmark::Counter(
        collections, benchmark::Counter::kIsRate);
}
BENCHMARK(BM_CollectionThroughput);

//-----------------------------------------------------------------------------
// Sustained Collection Load
//-----------------------------------------------------------------------------

static void BM_SustainedCollection(benchmark::State& state) {
    auto collector = std::make_shared<system_resource_collector>();
    size_t collections = 0;
    auto start_time = std::chrono::high_resolution_clock::now();

    for (auto _ : state) {
        auto metrics = collector->collect();
        benchmark::DoNotOptimize(metrics);
        collections++;

        // Simulate realistic interval
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(
        end_time - start_time).count();

    state.counters["total_collections"] = collections;
    state.counters["duration_sec"] = duration;
    state.counters["avg_rate"] = collections / static_cast<double>(duration);
}
BENCHMARK(BM_SustainedCollection)->MinTime(5.0);
