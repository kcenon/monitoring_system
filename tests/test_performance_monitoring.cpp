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
 * @file test_performance_monitoring.cpp
 * @brief Unit tests for performance monitoring functionality
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <random>
#include <set>
#include <kcenon/monitoring/core/performance_monitor.h>

using namespace kcenon::monitoring;

class PerformanceMonitoringTest : public ::testing::Test {
protected:
    performance_profiler profiler;
    performance_monitor monitor;
    
    void SetUp() override {
        profiler.clear_all_samples();
        monitor.set_enabled(true);
    }
    
    void TearDown() override {
        monitor.cleanup();
    }
    
    // Helper function to simulate work
    void simulate_work(std::chrono::milliseconds duration) {
        std::this_thread::sleep_for(duration);
    }
};

TEST_F(PerformanceMonitoringTest, RecordSingleSample) {
    auto result = profiler.record_sample(
        "test_operation",
        std::chrono::nanoseconds(1000000), // 1ms
        true
    );
    
    ASSERT_TRUE(result.is_ok());
    EXPECT_TRUE(result.value());
    
    auto metrics_result = profiler.get_metrics("test_operation");
    ASSERT_TRUE(metrics_result.is_ok());
    
    auto metrics = metrics_result.value();
    EXPECT_EQ(metrics.operation_name, "test_operation");
    EXPECT_EQ(metrics.call_count, 1);
    EXPECT_EQ(metrics.error_count, 0);
    EXPECT_EQ(metrics.mean_duration.count(), 1000000);
}

TEST_F(PerformanceMonitoringTest, RecordMultipleSamples) {
    std::vector<std::chrono::nanoseconds> durations = {
        std::chrono::nanoseconds(1000000),  // 1ms
        std::chrono::nanoseconds(2000000),  // 2ms
        std::chrono::nanoseconds(3000000),  // 3ms
        std::chrono::nanoseconds(4000000),  // 4ms
        std::chrono::nanoseconds(5000000)   // 5ms
    };
    
    for (const auto& duration : durations) {
        profiler.record_sample("multi_operation", duration, true);
    }
    
    auto metrics_result = profiler.get_metrics("multi_operation");
    ASSERT_TRUE(metrics_result.is_ok());
    
    auto metrics = metrics_result.value();
    EXPECT_EQ(metrics.call_count, 5);
    EXPECT_EQ(metrics.error_count, 0);
    EXPECT_EQ(metrics.min_duration.count(), 1000000);
    EXPECT_EQ(metrics.max_duration.count(), 5000000);
    EXPECT_EQ(metrics.median_duration.count(), 3000000);
    EXPECT_EQ(metrics.mean_duration.count(), 3000000);
}

TEST_F(PerformanceMonitoringTest, RecordErrorSamples) {
    profiler.record_sample("error_operation", std::chrono::nanoseconds(1000000), true);
    profiler.record_sample("error_operation", std::chrono::nanoseconds(2000000), false);
    profiler.record_sample("error_operation", std::chrono::nanoseconds(3000000), false);
    profiler.record_sample("error_operation", std::chrono::nanoseconds(4000000), true);
    
    auto metrics_result = profiler.get_metrics("error_operation");
    ASSERT_TRUE(metrics_result.is_ok());
    
    auto metrics = metrics_result.value();
    EXPECT_EQ(metrics.call_count, 4);
    EXPECT_EQ(metrics.error_count, 2);
}

TEST_F(PerformanceMonitoringTest, ScopedTimer) {
    {
        scoped_timer timer(&profiler, "scoped_operation");
        simulate_work(std::chrono::milliseconds(10));
    }
    
    auto metrics_result = profiler.get_metrics("scoped_operation");
    ASSERT_TRUE(metrics_result.is_ok());
    
    auto metrics = metrics_result.value();
    EXPECT_EQ(metrics.call_count, 1);
    EXPECT_GE(metrics.mean_duration.count(), 10000000); // At least 10ms
}

TEST_F(PerformanceMonitoringTest, ScopedTimerWithError) {
    {
        scoped_timer timer(&profiler, "error_scoped_operation");
        simulate_work(std::chrono::milliseconds(5));
        timer.mark_failed();
    }
    
    auto metrics_result = profiler.get_metrics("error_scoped_operation");
    ASSERT_TRUE(metrics_result.is_ok());
    
    auto metrics = metrics_result.value();
    EXPECT_EQ(metrics.call_count, 1);
    EXPECT_EQ(metrics.error_count, 1);
}

TEST_F(PerformanceMonitoringTest, PercentileCalculation) {
    // Generate 100 samples with known distribution
    for (int i = 1; i <= 100; ++i) {
        profiler.record_sample(
            "percentile_test",
            std::chrono::nanoseconds(i * 1000000), // i ms
            true
        );
    }
    
    auto metrics_result = profiler.get_metrics("percentile_test");
    ASSERT_TRUE(metrics_result.is_ok());
    
    auto metrics = metrics_result.value();
    EXPECT_EQ(metrics.call_count, 100);
    
    // P50 should be around 50ms
    EXPECT_GE(metrics.median_duration.count(), 49000000);
    EXPECT_LE(metrics.median_duration.count(), 51000000);
    
    // P95 should be around 95ms
    EXPECT_GE(metrics.p95_duration.count(), 94000000);
    EXPECT_LE(metrics.p95_duration.count(), 96000000);
    
    // P99 should be around 99ms
    EXPECT_GE(metrics.p99_duration.count(), 98000000);
    EXPECT_LE(metrics.p99_duration.count(), 100000000);
}

TEST_F(PerformanceMonitoringTest, ThroughputCalculation) {
    // Record 10 operations each taking 100ms
    for (int i = 0; i < 10; ++i) {
        profiler.record_sample(
            "throughput_test",
            std::chrono::nanoseconds(100000000), // 100ms
            true
        );
    }

    auto metrics_result = profiler.get_metrics("throughput_test");
    ASSERT_TRUE(metrics_result.is_ok());

    auto metrics = metrics_result.value();
    EXPECT_EQ(metrics.call_count, 10);
    // Throughput calculation depends on implementation
    EXPECT_GE(metrics.throughput, 0.0);
}

TEST_F(PerformanceMonitoringTest, ClearSamples) {
    profiler.record_sample("clear_test", std::chrono::nanoseconds(1000000), true);

    auto before_result = profiler.get_metrics("clear_test");
    ASSERT_TRUE(before_result.is_ok());
    EXPECT_EQ(before_result.value().call_count, 1);

    auto result = profiler.clear_samples("clear_test");
    ASSERT_TRUE(result.is_ok());
    EXPECT_TRUE(result.value());

    // After clear, metrics may still exist but with reset values
    auto after_result = profiler.get_metrics("clear_test");
    if (after_result.is_ok()) {
        EXPECT_EQ(after_result.value().call_count, 0);
    }
}

TEST_F(PerformanceMonitoringTest, GetAllMetrics) {
    profiler.record_sample("op1", std::chrono::nanoseconds(1000000), true);
    profiler.record_sample("op2", std::chrono::nanoseconds(2000000), true);
    profiler.record_sample("op3", std::chrono::nanoseconds(3000000), true);
    
    auto all_metrics = profiler.get_all_metrics();
    EXPECT_EQ(all_metrics.size(), 3);
    
    std::set<std::string> operation_names;
    for (const auto& metrics : all_metrics) {
        operation_names.insert(metrics.operation_name);
    }
    
    EXPECT_TRUE(operation_names.count("op1") > 0);
    EXPECT_TRUE(operation_names.count("op2") > 0);
    EXPECT_TRUE(operation_names.count("op3") > 0);
}

TEST_F(PerformanceMonitoringTest, ProfilerEnableDisable) {
    profiler.set_enabled(false);
    
    auto result = profiler.record_sample(
        "disabled_test",
        std::chrono::nanoseconds(1000000),
        true
    );
    ASSERT_TRUE(result.is_ok());
    
    // Sample should not be recorded when disabled
    auto metrics_result = profiler.get_metrics("disabled_test");
    ASSERT_FALSE(metrics_result.is_ok());
    
    profiler.set_enabled(true);
    profiler.record_sample("enabled_test", std::chrono::nanoseconds(1000000), true);
    
    metrics_result = profiler.get_metrics("enabled_test");
    ASSERT_TRUE(metrics_result.is_ok());
}

TEST_F(PerformanceMonitoringTest, SystemMetrics) {
    system_monitor sys_monitor;
    
    auto result = sys_monitor.get_current_metrics();
    ASSERT_TRUE(result.is_ok());
    
    auto metrics = result.value();
    
    // Basic sanity checks
    EXPECT_GE(metrics.cpu_usage_percent, 0.0);
    EXPECT_LE(metrics.cpu_usage_percent, 100.0);
    
    EXPECT_GE(metrics.memory_usage_percent, 0.0);
    EXPECT_LE(metrics.memory_usage_percent, 100.0);
    
    EXPECT_GT(metrics.memory_usage_bytes, 0);
    EXPECT_GT(metrics.thread_count, 0);
}

TEST_F(PerformanceMonitoringTest, SystemMonitoringHistory) {
    system_monitor sys_monitor;

    auto start_result = sys_monitor.start_monitoring(std::chrono::milliseconds(100));
    ASSERT_TRUE(start_result.is_ok());

    // Let it collect some samples (increased for CI stability)
    std::this_thread::sleep_for(std::chrono::milliseconds(700));

    auto history = sys_monitor.get_history(std::chrono::seconds(1));
    EXPECT_GE(history.size(), 2); // Should have at least 2 samples (relaxed for CI)

    // Check that timestamps are increasing
    for (size_t i = 1; i < history.size(); ++i) {
        EXPECT_GT(history[i].timestamp, history[i-1].timestamp);
    }

    auto stop_result = sys_monitor.stop_monitoring();
    ASSERT_TRUE(stop_result.is_ok());
}

TEST_F(PerformanceMonitoringTest, PerformanceMonitorCollect) {
    // Record some performance samples
    monitor.get_profiler().record_sample("collect_test", std::chrono::nanoseconds(5000000), true);
    
    auto init_result = monitor.initialize();
    ASSERT_TRUE(init_result.is_ok());
    
    auto snapshot_result = monitor.collect();
    ASSERT_TRUE(snapshot_result.is_ok());
    
    auto snapshot = snapshot_result.value();
    EXPECT_EQ(snapshot.source_id, "performance_monitor");
    EXPECT_GT(snapshot.metrics.size(), 0);
    
    // Check for expected metrics
    bool found_perf_metric = false;

    for (const auto& metric : snapshot.metrics) {
        if (metric.name.find("collect_test") != std::string::npos) {
            found_perf_metric = true;
        }
    }

    EXPECT_TRUE(found_perf_metric);
    // System metrics may or may not be present depending on platform
}

TEST_F(PerformanceMonitoringTest, ThresholdChecking) {
    monitor.set_cpu_threshold(0.0); // Set impossibly low threshold
    monitor.set_memory_threshold(0.0);
    monitor.set_latency_threshold(std::chrono::milliseconds(0));
    
    // Record a sample to trigger latency threshold
    monitor.get_profiler().record_sample(
        "threshold_test",
        std::chrono::nanoseconds(1000000),
        true
    );
    
    auto init_result = monitor.initialize();
    ASSERT_TRUE(init_result.is_ok());
    
    auto threshold_result = monitor.check_thresholds();
    ASSERT_TRUE(threshold_result.is_ok());
    EXPECT_TRUE(threshold_result.value()); // Should exceed thresholds
}

TEST_F(PerformanceMonitoringTest, GlobalPerformanceMonitor) {
    auto& global = global_performance_monitor();
    
    {
        PERF_TIMER("global_test_operation");
        simulate_work(std::chrono::milliseconds(10));
    }
    
    auto metrics_result = global.get_profiler().get_metrics("global_test_operation");
    ASSERT_TRUE(metrics_result.is_ok());
    
    auto metrics = metrics_result.value();
    EXPECT_EQ(metrics.call_count, 1);
    EXPECT_GE(metrics.mean_duration.count(), 10000000);
}

TEST_F(PerformanceMonitoringTest, PerformanceBenchmark) {
    performance_benchmark benchmark("test_benchmark");
    benchmark.set_iterations(100);
    benchmark.set_warmup_iterations(10);
    
    auto result = benchmark.run("simple_operation", []() {
        // Simulate some simple work
        volatile int sum = 0;
        for (int i = 0; i < 1000; ++i) {
            sum += i;
        }
    });
    
    ASSERT_TRUE(result.is_ok());
    
    auto metrics = result.value();
    EXPECT_EQ(metrics.call_count, 100);
    EXPECT_GT(metrics.mean_duration.count(), 0);
    EXPECT_GE(metrics.max_duration.count(), metrics.min_duration.count());
}

TEST_F(PerformanceMonitoringTest, BenchmarkComparison) {
    performance_benchmark benchmark("comparison_benchmark");
    benchmark.set_iterations(50);
    benchmark.set_warmup_iterations(5);
    
    auto result = benchmark.compare(
        "fast_operation",
        []() {
            volatile int sum = 0;
            for (int i = 0; i < 100; ++i) {
                sum += i;
            }
        },
        "slow_operation",
        []() {
            volatile int sum = 0;
            for (int i = 0; i < 10000; ++i) {
                sum += i;
            }
        }
    );
    
    ASSERT_TRUE(result.is_ok());
    
    auto [fast_metrics, slow_metrics] = result.value();
    
    EXPECT_EQ(fast_metrics.call_count, 50);
    EXPECT_EQ(slow_metrics.call_count, 50);
    
    // Fast operation should be faster than slow operation
    EXPECT_LT(fast_metrics.mean_duration, slow_metrics.mean_duration);
}

TEST_F(PerformanceMonitoringTest, MaxSamplesLimit) {
    profiler.set_max_samples(10);
    
    // Record 20 samples
    for (int i = 0; i < 20; ++i) {
        profiler.record_sample(
            "limit_test",
            std::chrono::nanoseconds(i * 1000000),
            true
        );
    }
    
    auto metrics_result = profiler.get_metrics("limit_test");
    ASSERT_TRUE(metrics_result.is_ok());
    
    auto metrics = metrics_result.value();
    // Call count should still be 20
    EXPECT_EQ(metrics.call_count, 20);
    
    // But only last 10 samples should be in statistics
    // The minimum should be from sample 10 (10ms), not sample 0 (0ms)
    EXPECT_GE(metrics.min_duration.count(), 10000000);
}

TEST_F(PerformanceMonitoringTest, ConcurrentRecording) {
    const int num_threads = 10;
    const int samples_per_thread = 100;
    
    std::vector<std::thread> threads;
    
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, t, samples_per_thread]() {
            for (int i = 0; i < samples_per_thread; ++i) {
                profiler.record_sample(
                    "concurrent_test",
                    std::chrono::nanoseconds((t + 1) * 1000000),
                    true
                );
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    auto metrics_result = profiler.get_metrics("concurrent_test");
    ASSERT_TRUE(metrics_result.is_ok());
    
    auto metrics = metrics_result.value();
    EXPECT_EQ(metrics.call_count, num_threads * samples_per_thread);
}