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
 * @file adaptive_monitor_bench.cpp
 * @brief Benchmark for adaptive monitoring performance
 * @details Measures overhead of adaptive monitoring operations including
 *          load level calculation, sampling decisions, and adaptation cycles.
 *
 * Target Metrics:
 * - Load level calculation: < 50ns
 * - Sampling decision: < 20ns
 * - Adaptation cycle: < 1μs
 * - Concurrent collection overhead: < 5% vs non-adaptive
 *
 * Phase 2, ARC-002: Performance Benchmarks
 */

#include <benchmark/benchmark.h>
#include <kcenon/monitoring/adaptive/adaptive_monitor.h>
#include <kcenon/monitoring/core/performance_monitor.h>
#include <memory>
#include <string>
#include <chrono>
#include <thread>
#include <atomic>

using namespace kcenon::monitoring;

//-----------------------------------------------------------------------------
// Helper: Create a mock metrics collector for testing
//-----------------------------------------------------------------------------

class mock_metrics_collector : public metrics_collector {
private:
    bool enabled_{true};

public:
    result<metrics_snapshot> collect() override {
        metrics_snapshot snapshot;
        snapshot.add_metric("cpu_usage", 50.0);
        snapshot.add_metric("memory_usage", 60.0);
        return make_success(std::move(snapshot));
    }

    std::string get_name() const override {
        return "mock_collector";
    }

    bool is_enabled() const override {
        return enabled_;
    }

    result_void set_enabled(bool enable) override {
        enabled_ = enable;
        return make_void_success();
    }

    result_void initialize() override {
        return make_void_success();
    }

    result_void cleanup() override {
        return make_void_success();
    }
};

//-----------------------------------------------------------------------------
// Adaptive Configuration Benchmark
//-----------------------------------------------------------------------------

static void BM_AdaptiveConfig_GetIntervalForLoad(benchmark::State& state) {
    adaptive_config config;
    size_t count = 0;

    for (auto _ : state) {
        auto interval = config.get_interval_for_load(load_level::moderate);
        benchmark::DoNotOptimize(interval);
        count++;
    }

    state.SetItemsProcessed(count);
    state.SetLabel("config_interval_lookup");
}
BENCHMARK(BM_AdaptiveConfig_GetIntervalForLoad);

static void BM_AdaptiveConfig_GetSamplingRateForLoad(benchmark::State& state) {
    adaptive_config config;
    size_t count = 0;

    for (auto _ : state) {
        auto rate = config.get_sampling_rate_for_load(load_level::high);
        benchmark::DoNotOptimize(rate);
        count++;
    }

    state.SetItemsProcessed(count);
    state.SetLabel("config_sampling_rate_lookup");
}
BENCHMARK(BM_AdaptiveConfig_GetSamplingRateForLoad);

//-----------------------------------------------------------------------------
// Adaptive Collector Benchmarks
//-----------------------------------------------------------------------------

static void BM_AdaptiveCollector_Collect_Enabled(benchmark::State& state) {
    auto mock_collector = std::make_shared<mock_metrics_collector>();
    adaptive_config config;
    config.idle_sampling_rate = 1.0;  // Always sample
    adaptive_collector collector(mock_collector, config);

    size_t count = 0;
    for (auto _ : state) {
        auto result = collector.collect();
        benchmark::DoNotOptimize(result);
        count++;
    }

    state.SetItemsProcessed(count);
    state.SetLabel("adaptive_collect_enabled");
}
BENCHMARK(BM_AdaptiveCollector_Collect_Enabled);

static void BM_AdaptiveCollector_Collect_Disabled(benchmark::State& state) {
    auto mock_collector = std::make_shared<mock_metrics_collector>();
    adaptive_config config;
    adaptive_collector collector(mock_collector, config);
    collector.set_enabled(false);  // Disable adaptive behavior

    size_t count = 0;
    for (auto _ : state) {
        auto result = collector.collect();
        benchmark::DoNotOptimize(result);
        count++;
    }

    state.SetItemsProcessed(count);
    state.SetLabel("adaptive_collect_disabled");
}
BENCHMARK(BM_AdaptiveCollector_Collect_Disabled);

static void BM_AdaptiveCollector_Adapt(benchmark::State& state) {
    auto mock_collector = std::make_shared<mock_metrics_collector>();
    adaptive_collector collector(mock_collector);

    system_metrics sys_metrics;
    sys_metrics.cpu_usage_percent = 45.0;
    sys_metrics.memory_usage_percent = 55.0;

    size_t count = 0;
    for (auto _ : state) {
        collector.adapt(sys_metrics);
        count++;
    }

    state.SetItemsProcessed(count);
    state.SetLabel("adapt_cycle");
}
BENCHMARK(BM_AdaptiveCollector_Adapt);

static void BM_AdaptiveCollector_Adapt_LoadTransition(benchmark::State& state) {
    auto mock_collector = std::make_shared<mock_metrics_collector>();
    adaptive_collector collector(mock_collector);

    double cpu_levels[] = {10.0, 30.0, 50.0, 70.0, 90.0, 70.0, 50.0, 30.0, 10.0};
    size_t level_idx = 0;

    size_t count = 0;
    for (auto _ : state) {
        system_metrics sys_metrics;
        sys_metrics.cpu_usage_percent = cpu_levels[level_idx % 9];
        sys_metrics.memory_usage_percent = 50.0;

        collector.adapt(sys_metrics);
        level_idx++;
        count++;
    }

    state.SetItemsProcessed(count);
    state.SetLabel("adapt_with_transitions");
}
BENCHMARK(BM_AdaptiveCollector_Adapt_LoadTransition);

static void BM_AdaptiveCollector_GetStats(benchmark::State& state) {
    auto mock_collector = std::make_shared<mock_metrics_collector>();
    adaptive_collector collector(mock_collector);

    // Populate some stats
    for (int i = 0; i < 100; ++i) {
        system_metrics sys_metrics;
        sys_metrics.cpu_usage_percent = static_cast<double>(i % 100);
        sys_metrics.memory_usage_percent = 50.0;
        collector.adapt(sys_metrics);
    }

    size_t count = 0;
    for (auto _ : state) {
        auto stats = collector.get_stats();
        benchmark::DoNotOptimize(stats);
        count++;
    }

    state.SetItemsProcessed(count);
    state.SetLabel("get_stats");
}
BENCHMARK(BM_AdaptiveCollector_GetStats);

//-----------------------------------------------------------------------------
// Concurrent Adaptive Collection
//-----------------------------------------------------------------------------

static void BM_AdaptiveCollector_Concurrent_Collect(benchmark::State& state) {
    static auto mock_collector = std::make_shared<mock_metrics_collector>();
    static adaptive_collector collector(mock_collector);

    size_t count = 0;
    for (auto _ : state) {
        auto result = collector.collect();
        benchmark::DoNotOptimize(result);
        count++;
    }

    state.SetItemsProcessed(count);
}
BENCHMARK(BM_AdaptiveCollector_Concurrent_Collect)->Threads(4);

static void BM_AdaptiveCollector_Concurrent_CollectAndAdapt(benchmark::State& state) {
    static auto mock_collector = std::make_shared<mock_metrics_collector>();
    static adaptive_collector collector(mock_collector);

    size_t count = 0;
    for (auto _ : state) {
        if (state.thread_index() == 0) {
            // Thread 0: adaptation
            system_metrics sys_metrics;
            sys_metrics.cpu_usage_percent = 50.0;
            sys_metrics.memory_usage_percent = 50.0;
            collector.adapt(sys_metrics);
        } else {
            // Other threads: collection
            auto result = collector.collect();
            benchmark::DoNotOptimize(result);
        }
        count++;
    }

    state.SetItemsProcessed(count);
}
BENCHMARK(BM_AdaptiveCollector_Concurrent_CollectAndAdapt)->Threads(4);

//-----------------------------------------------------------------------------
// Adaptive Monitor Lifecycle
//-----------------------------------------------------------------------------

static void BM_AdaptiveMonitor_RegisterCollector(benchmark::State& state) {
    adaptive_monitor monitor;
    auto mock_collector = std::make_shared<mock_metrics_collector>();

    size_t count = 0;
    for (auto _ : state) {
        std::string name = "collector_" + std::to_string(count);
        auto result = monitor.register_collector(name, mock_collector);
        benchmark::DoNotOptimize(result);

        // Clean up to avoid unbounded growth
        monitor.unregister_collector(name);
        count++;
    }

    state.SetItemsProcessed(count);
    state.SetLabel("register_unregister");
}
BENCHMARK(BM_AdaptiveMonitor_RegisterCollector);

static void BM_AdaptiveMonitor_GetAllStats(benchmark::State& state) {
    adaptive_monitor monitor;

    // Register multiple collectors
    for (int i = 0; i < 10; ++i) {
        auto mock_collector = std::make_shared<mock_metrics_collector>();
        monitor.register_collector("collector_" + std::to_string(i), mock_collector);
    }

    size_t count = 0;
    for (auto _ : state) {
        auto all_stats = monitor.get_all_stats();
        benchmark::DoNotOptimize(all_stats);
        count++;
    }

    state.SetItemsProcessed(count);
    state.SetLabel("get_all_stats_10_collectors");
}
BENCHMARK(BM_AdaptiveMonitor_GetAllStats);

//-----------------------------------------------------------------------------
// Strategy Comparison
//-----------------------------------------------------------------------------

static void BM_AdaptiveCollector_Strategy_Conservative(benchmark::State& state) {
    auto mock_collector = std::make_shared<mock_metrics_collector>();
    adaptive_config config;
    config.strategy = adaptation_strategy::conservative;
    adaptive_collector collector(mock_collector, config);

    system_metrics sys_metrics;
    sys_metrics.cpu_usage_percent = 60.0;
    sys_metrics.memory_usage_percent = 50.0;

    size_t count = 0;
    for (auto _ : state) {
        collector.adapt(sys_metrics);
        auto result = collector.collect();
        benchmark::DoNotOptimize(result);
        count++;
    }

    state.SetItemsProcessed(count);
    state.SetLabel("conservative_strategy");
}
BENCHMARK(BM_AdaptiveCollector_Strategy_Conservative);

static void BM_AdaptiveCollector_Strategy_Aggressive(benchmark::State& state) {
    auto mock_collector = std::make_shared<mock_metrics_collector>();
    adaptive_config config;
    config.strategy = adaptation_strategy::aggressive;
    adaptive_collector collector(mock_collector, config);

    system_metrics sys_metrics;
    sys_metrics.cpu_usage_percent = 60.0;
    sys_metrics.memory_usage_percent = 50.0;

    size_t count = 0;
    for (auto _ : state) {
        collector.adapt(sys_metrics);
        auto result = collector.collect();
        benchmark::DoNotOptimize(result);
        count++;
    }

    state.SetItemsProcessed(count);
    state.SetLabel("aggressive_strategy");
}
BENCHMARK(BM_AdaptiveCollector_Strategy_Aggressive);

//-----------------------------------------------------------------------------
// Memory Pressure Scenarios
//-----------------------------------------------------------------------------

static void BM_AdaptiveCollector_HighMemoryPressure(benchmark::State& state) {
    auto mock_collector = std::make_shared<mock_metrics_collector>();
    adaptive_collector collector(mock_collector);

    system_metrics sys_metrics;
    sys_metrics.cpu_usage_percent = 30.0;
    sys_metrics.memory_usage_percent = 90.0;  // Critical memory

    size_t count = 0;
    for (auto _ : state) {
        collector.adapt(sys_metrics);
        auto result = collector.collect();
        benchmark::DoNotOptimize(result);
        count++;
    }

    state.SetItemsProcessed(count);
    state.SetLabel("high_memory_pressure");
}
BENCHMARK(BM_AdaptiveCollector_HighMemoryPressure);
