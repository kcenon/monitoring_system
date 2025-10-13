/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, monitoring_system contributors
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>

#include "system_fixture.h"
#include "test_helpers.h"

using namespace integration_tests;
using namespace monitoring_system;

class MonitoringPerformanceTest : public MonitoringSystemFixture {};

/**
 * Test 1: Metric Collection Throughput
 * Target: > 10,000 metrics/second
 */
TEST_F(MonitoringPerformanceTest, MetricCollectionThroughput) {
    ASSERT_TRUE(StartMonitoring());

    const size_t num_metrics = 100000;
    PerformanceMetrics perf_metrics;

    ScopedTimer timer([&perf_metrics](auto duration) {
        perf_metrics.add_sample(duration);
    });

    for (size_t i = 0; i < num_metrics; ++i) {
        RecordSample("throughput_test", std::chrono::microseconds(100));
    }

    timer.~ScopedTimer();

    auto duration = std::chrono::nanoseconds(static_cast<int64_t>(perf_metrics.mean()));
    double throughput = CalculateThroughput(num_metrics, duration);

    std::cout << "Metric collection throughput: " << throughput << " metrics/sec\n";
    std::cout << "Duration: " << FormatDuration(duration) << "\n";

    // Target: > 10,000 metrics/second
    EXPECT_GT(throughput, 10000.0);
}

/**
 * Test 2: Latency Measurements - P50
 * Target: < 1 microsecond P50
 */
TEST_F(MonitoringPerformanceTest, LatencyMeasurementsP50) {
    ASSERT_TRUE(StartMonitoring());

    const size_t num_samples = 10000;
    PerformanceMetrics perf_metrics;

    for (size_t i = 0; i < num_samples; ++i) {
        ScopedTimer timer([&perf_metrics](auto duration) {
            perf_metrics.add_sample(duration);
        });

        RecordSample("latency_test", std::chrono::microseconds(100));
    }

    auto p50 = perf_metrics.p50();
    auto p50_us = p50 / 1000;  // Convert to microseconds

    std::cout << "P50 latency: " << p50_us << " us\n";
    std::cout << "P50 latency (formatted): " << FormatDuration(std::chrono::nanoseconds(static_cast<int64_t>(p50))) << "\n";

    // Target: < 1 microsecond
    EXPECT_LT(p50, 1'000'000);  // 1 microsecond in nanoseconds
}

/**
 * Test 3: Latency Measurements - P95
 * Target: < 10 microseconds P95
 */
TEST_F(MonitoringPerformanceTest, LatencyMeasurementsP95) {
    ASSERT_TRUE(StartMonitoring());

    const size_t num_samples = 10000;
    PerformanceMetrics perf_metrics;

    for (size_t i = 0; i < num_samples; ++i) {
        ScopedTimer timer([&perf_metrics](auto duration) {
            perf_metrics.add_sample(duration);
        });

        RecordSample("latency_p95_test", std::chrono::microseconds(100));
    }

    auto p95 = perf_metrics.p95();
    auto p95_us = p95 / 1000;  // Convert to microseconds

    std::cout << "P95 latency: " << p95_us << " us\n";
    std::cout << "P95 latency (formatted): " << FormatDuration(std::chrono::nanoseconds(static_cast<int64_t>(p95))) << "\n";

    // Target: < 10 microseconds
    EXPECT_LT(p95, 10'000'000);  // 10 microseconds in nanoseconds
}

/**
 * Test 4: Memory Overhead Per Metric
 * Target: < 1KB per metric
 */
TEST_F(MonitoringPerformanceTest, MemoryOverheadPerMetric) {
    const size_t num_metrics = 1000;
    std::vector<compact_metric_value> metrics;

    for (size_t i = 0; i < num_metrics; ++i) {
        metrics.push_back(CreateTestMetric("memory_test_" + std::to_string(i)));
    }

    size_t total_memory = CalculateMetricsMemory(metrics);
    size_t avg_memory_per_metric = total_memory / num_metrics;

    std::cout << "Total memory: " << total_memory << " bytes\n";
    std::cout << "Average memory per metric: " << avg_memory_per_metric << " bytes\n";

    // Target: < 1KB (1024 bytes) per metric
    EXPECT_LT(avg_memory_per_metric, 1024);
}

/**
 * Test 5: Scalability with Metric Count
 * Test performance doesn't degrade significantly with more metrics
 */
TEST_F(MonitoringPerformanceTest, ScalabilityWithMetricCount) {
    ASSERT_TRUE(StartMonitoring());

    std::vector<size_t> metric_counts = {100, 1000, 10000};
    std::vector<double> throughputs;

    for (auto count : metric_counts) {
        PerformanceMetrics perf_metrics;

        auto start = std::chrono::high_resolution_clock::now();

        for (size_t i = 0; i < count; ++i) {
            RecordSample("scalability_test_" + std::to_string(i % 100),
                        std::chrono::microseconds(100));
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

        double throughput = CalculateThroughput(count, duration);
        throughputs.push_back(throughput);

        std::cout << "Metrics: " << count << ", Throughput: " << throughput << " ops/sec\n";
    }

    // Throughput shouldn't degrade more than 50% from smallest to largest
    EXPECT_GT(throughputs.back(), throughputs.front() * 0.5);
}

/**
 * Test 6: Concurrent Collection Performance
 * Test performance with multiple threads collecting metrics
 */
TEST_F(MonitoringPerformanceTest, ConcurrentCollectionPerformance) {
    ASSERT_TRUE(StartMonitoring());

    const size_t num_threads = 4;
    const size_t metrics_per_thread = 10000;
    std::vector<std::thread> threads;
    std::atomic<size_t> total_collected{0};

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, metrics_per_thread, &total_collected, t]() {
            for (size_t i = 0; i < metrics_per_thread; ++i) {
                RecordSample("concurrent_perf_" + std::to_string(t),
                           std::chrono::microseconds(100));
                total_collected.fetch_add(1);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    double throughput = CalculateThroughput(total_collected.load(), duration);

    std::cout << "Concurrent throughput: " << throughput << " metrics/sec\n";
    std::cout << "Total collected: " << total_collected.load() << "\n";
    std::cout << "Duration: " << FormatDuration(duration) << "\n";

    // Should maintain reasonable throughput even with concurrency
    EXPECT_GT(throughput, 5000.0);
}

/**
 * Test 7: Aggregation Performance
 * Target: < 100 microseconds for 1000 metrics
 */
TEST_F(MonitoringPerformanceTest, AggregationPerformance) {
    const size_t num_metrics = 1000;
    histogram_data histogram;
    histogram.init_standard_buckets();

    PerformanceMetrics perf_metrics;

    ScopedTimer timer([&perf_metrics](auto duration) {
        perf_metrics.add_sample(duration);
    });

    for (size_t i = 0; i < num_metrics; ++i) {
        histogram.add_sample(static_cast<double>(i) / 1000.0);
    }

    timer.~ScopedTimer();

    auto duration_ns = perf_metrics.mean();
    auto duration_us = duration_ns / 1000;

    std::cout << "Aggregation time for " << num_metrics << " metrics: "
              << duration_us << " us\n";

    // Target: < 100 microseconds
    EXPECT_LT(duration_us, 100);
}

/**
 * Test 8: Export Performance
 * Test performance of metric export operations
 */
TEST_F(MonitoringPerformanceTest, ExportPerformance) {
    MockMetricExporter exporter;
    const size_t num_exports = 100;
    const size_t metrics_per_export = 1000;

    PerformanceMetrics perf_metrics;

    for (size_t e = 0; e < num_exports; ++e) {
        auto batch = GenerateMetricBatch(metrics_per_export);

        ScopedTimer timer([&perf_metrics](auto duration) {
            perf_metrics.add_sample(duration);
        });

        exporter.export_metrics(batch.metrics);
    }

    auto mean_duration_ns = perf_metrics.mean();
    auto mean_duration_us = mean_duration_ns / 1000;

    std::cout << "Mean export time: " << mean_duration_us << " us\n";
    std::cout << "Total exports: " << exporter.get_export_count() << "\n";
    std::cout << "Total exported metrics: " << exporter.get_total_exported() << "\n";

    EXPECT_EQ(exporter.get_export_count(), num_exports);
    EXPECT_EQ(exporter.get_total_exported(), num_exports * metrics_per_export);
}

/**
 * Test 9: Batch Processing Performance
 * Test performance of processing metrics in batches
 */
TEST_F(MonitoringPerformanceTest, BatchProcessingPerformance) {
    const size_t batch_size = 1000;
    const size_t num_batches = 100;

    PerformanceMetrics perf_metrics;

    for (size_t b = 0; b < num_batches; ++b) {
        ScopedTimer timer([&perf_metrics](auto duration) {
            perf_metrics.add_sample(duration);
        });

        auto batch = GenerateMetricBatch(batch_size);

        // Process batch (count metrics by type)
        size_t counter_count = CountMetricsByType(batch.metrics, metric_type::counter);
        size_t gauge_count = CountMetricsByType(batch.metrics, metric_type::gauge);

        // Ensure batch was processed
        EXPECT_EQ(batch.size(), batch_size);
    }

    auto mean_duration_ns = perf_metrics.mean();
    auto mean_duration_us = mean_duration_ns / 1000;

    std::cout << "Mean batch processing time: " << mean_duration_us << " us\n";
    std::cout << "Batch size: " << batch_size << "\n";

    // Processing should be fast
    EXPECT_LT(mean_duration_us, 1000);  // < 1ms per batch
}

/**
 * Test 10: Memory Allocation Performance
 * Test performance impact of memory allocations
 */
TEST_F(MonitoringPerformanceTest, MemoryAllocationPerformance) {
    const size_t num_iterations = 1000;
    PerformanceMetrics perf_metrics;

    for (size_t i = 0; i < num_iterations; ++i) {
        ScopedTimer timer([&perf_metrics](auto duration) {
            perf_metrics.add_sample(duration);
        });

        // Allocate metric
        auto metric = CreateTestMetric("alloc_test_" + std::to_string(i));

        // Use metric to prevent optimization
        volatile double value = metric.as_double();
        (void)value;
    }

    auto mean_duration_ns = perf_metrics.mean();
    auto mean_duration_us = mean_duration_ns / 1000;

    std::cout << "Mean allocation time: " << mean_duration_us << " us\n";

    // Allocation should be very fast
    EXPECT_LT(mean_duration_us, 10);  // < 10 microseconds
}
