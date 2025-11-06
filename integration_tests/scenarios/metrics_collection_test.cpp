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

#include "system_fixture.h"
#include "test_helpers.h"

using namespace integration_tests;
using namespace kcenon::monitoring;

class MetricsCollectionTest : public MonitoringSystemFixture {};

/**
 * Test 1: Metric Registration and Initialization
 * Verify that metrics can be registered and initialized properly
 */
TEST_F(MetricsCollectionTest, MetricRegistrationAndInitialization) {
    ASSERT_TRUE(StartMonitoring());

    // Record some samples
    auto duration = std::chrono::microseconds(100);
    ASSERT_TRUE(RecordSample("test_operation", duration));

    // Verify metric was registered
    auto metrics = GetPerformanceMetrics("test_operation");
    ASSERT_TRUE(metrics.has_value());
    EXPECT_EQ(metrics->operation_name, "test_operation");
    EXPECT_EQ(metrics->call_count, 1);
}

/**
 * Test 2: Counter Operations
 * Test basic counter metric operations
 */
TEST_F(MetricsCollectionTest, CounterOperations) {
    auto counter1 = CreateMetric("counter_test", metric_type::counter, 0.0);
    auto counter2 = CreateMetric("counter_test", metric_type::counter, 10.0);
    auto counter3 = CreateMetric("counter_test", metric_type::counter, 25.0);

    EXPECT_EQ(counter1.metadata.type, metric_type::counter);
    EXPECT_DOUBLE_EQ(counter1.as_double(), 0.0);
    EXPECT_DOUBLE_EQ(counter2.as_double(), 10.0);
    EXPECT_DOUBLE_EQ(counter3.as_double(), 25.0);

    // Verify counter monotonicity
    EXPECT_LT(counter1.as_double(), counter2.as_double());
    EXPECT_LT(counter2.as_double(), counter3.as_double());
}

/**
 * Test 3: Gauge Operations
 * Test gauge metric operations that can increase and decrease
 */
TEST_F(MetricsCollectionTest, GaugeOperations) {
    auto gauge1 = CreateMetric("gauge_test", metric_type::gauge, 50.0);
    auto gauge2 = CreateMetric("gauge_test", metric_type::gauge, 75.0);
    auto gauge3 = CreateMetric("gauge_test", metric_type::gauge, 25.0);

    EXPECT_EQ(gauge1.metadata.type, metric_type::gauge);
    EXPECT_DOUBLE_EQ(gauge1.as_double(), 50.0);
    EXPECT_DOUBLE_EQ(gauge2.as_double(), 75.0);
    EXPECT_DOUBLE_EQ(gauge3.as_double(), 25.0);

    // Gauge can go up and down
    EXPECT_GT(gauge2.as_double(), gauge1.as_double());
    EXPECT_LT(gauge3.as_double(), gauge1.as_double());
}

/**
 * Test 4: Histogram Operations
 * Test histogram metric operations with buckets
 */
TEST_F(MetricsCollectionTest, HistogramOperations) {
    histogram_data histogram;
    histogram.init_standard_buckets();

    // Add samples
    histogram.add_sample(0.003);
    histogram.add_sample(0.015);
    histogram.add_sample(0.055);
    histogram.add_sample(0.5);
    histogram.add_sample(1.5);

    EXPECT_EQ(histogram.total_count, 5);
    EXPECT_GT(histogram.sum, 0.0);
    EXPECT_GT(histogram.mean(), 0.0);

    // Verify buckets are populated
    EXPECT_GT(histogram.buckets[0].count, 0);  // 0.005 bucket
}

/**
 * Test 5: Multiple Metric Instances
 * Test managing multiple independent metric instances
 */
TEST_F(MetricsCollectionTest, MultipleMetricInstances) {
    ASSERT_TRUE(StartMonitoring());

    // Record samples for different operations
    ASSERT_TRUE(RecordSample("operation_a", std::chrono::microseconds(100)));
    ASSERT_TRUE(RecordSample("operation_b", std::chrono::microseconds(200)));
    ASSERT_TRUE(RecordSample("operation_c", std::chrono::microseconds(300)));

    // Verify all operations are tracked independently
    auto metrics_a = GetPerformanceMetrics("operation_a");
    auto metrics_b = GetPerformanceMetrics("operation_b");
    auto metrics_c = GetPerformanceMetrics("operation_c");

    ASSERT_TRUE(metrics_a.has_value());
    ASSERT_TRUE(metrics_b.has_value());
    ASSERT_TRUE(metrics_c.has_value());

    EXPECT_EQ(metrics_a->operation_name, "operation_a");
    EXPECT_EQ(metrics_b->operation_name, "operation_b");
    EXPECT_EQ(metrics_c->operation_name, "operation_c");
}

/**
 * Test 6: Metric Label/Tag Management
 * Test metrics with labels and tags
 */
TEST_F(MetricsCollectionTest, MetricLabelTagManagement) {
    auto metadata = create_metric_metadata("labeled_metric", metric_type::gauge, 3);

    EXPECT_EQ(metadata.tag_count, 3);
    EXPECT_EQ(metadata.type, metric_type::gauge);

    auto metric = compact_metric_value(metadata, 42.0);
    EXPECT_DOUBLE_EQ(metric.as_double(), 42.0);
    EXPECT_EQ(metric.metadata.tag_count, 3);
}

/**
 * Test 7: Time-Series Data Collection
 * Test collecting metrics over time
 */
TEST_F(MetricsCollectionTest, TimeSeriesDataCollection) {
    ASSERT_TRUE(StartMonitoring());

    // Record samples at different times
    for (int i = 0; i < 10; ++i) {
        auto duration = std::chrono::microseconds(100 + i * 10);
        ASSERT_TRUE(RecordSample("time_series_test", duration));
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    auto metrics = GetPerformanceMetrics("time_series_test");
    ASSERT_TRUE(metrics.has_value());
    EXPECT_EQ(metrics->call_count, 10);
    EXPECT_GT(metrics->max_duration, metrics->min_duration);
}

/**
 * Test 8: Metric Aggregation
 * Test aggregating multiple metric values
 */
TEST_F(MetricsCollectionTest, MetricAggregation) {
    summary_data summary;

    // Add multiple samples
    summary.add_sample(10.0);
    summary.add_sample(20.0);
    summary.add_sample(30.0);
    summary.add_sample(40.0);
    summary.add_sample(50.0);

    EXPECT_EQ(summary.count, 5);
    EXPECT_DOUBLE_EQ(summary.sum, 150.0);
    EXPECT_DOUBLE_EQ(summary.mean(), 30.0);
    EXPECT_DOUBLE_EQ(summary.min_value, 10.0);
    EXPECT_DOUBLE_EQ(summary.max_value, 50.0);
}

/**
 * Test 9: Concurrent Metric Updates
 * Test updating metrics from multiple threads
 */
TEST_F(MetricsCollectionTest, ConcurrentMetricUpdates) {
    ASSERT_TRUE(StartMonitoring());

    const size_t num_threads = 4;
    const size_t samples_per_thread = 100;
    std::vector<std::thread> threads;

    for (size_t t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, samples = samples_per_thread]() {
            for (size_t i = 0; i < samples; ++i) {
                auto duration = std::chrono::microseconds(100 + i);
                RecordSample("concurrent_test", duration);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    auto metrics = GetPerformanceMetrics("concurrent_test");
    ASSERT_TRUE(metrics.has_value());
    EXPECT_EQ(metrics->call_count, num_threads * samples_per_thread);
}

/**
 * Test 10: Metric Type Validation
 * Test that metric types are properly validated
 */
TEST_F(MetricsCollectionTest, MetricTypeValidation) {
    auto counter = CreateMetric("test_counter", metric_type::counter, 10.0);
    auto gauge = CreateMetric("test_gauge", metric_type::gauge, 20.0);
    auto timer = CreateMetric("test_timer", metric_type::timer, 30.0);

    EXPECT_EQ(counter.metadata.type, metric_type::counter);
    EXPECT_EQ(gauge.metadata.type, metric_type::gauge);
    EXPECT_EQ(timer.metadata.type, metric_type::timer);

    // Verify type strings
    EXPECT_STREQ(metric_type_to_string(metric_type::counter), "counter");
    EXPECT_STREQ(metric_type_to_string(metric_type::gauge), "gauge");
    EXPECT_STREQ(metric_type_to_string(metric_type::timer), "timer");
}

/**
 * Test 11: Metric Batch Processing
 * Test collecting and processing metrics in batches
 */
TEST_F(MetricsCollectionTest, MetricBatchProcessing) {
    const size_t batch_size = 100;
    auto batch = GenerateMetricBatch(batch_size);

    EXPECT_EQ(batch.size(), batch_size);
    EXPECT_FALSE(batch.empty());
    EXPECT_GT(batch.memory_footprint(), 0);

    // Clear and verify
    batch.clear();
    EXPECT_EQ(batch.size(), 0);
    EXPECT_TRUE(batch.empty());
}

/**
 * Test 12: Metric Memory Footprint
 * Test calculating memory usage of metrics
 */
TEST_F(MetricsCollectionTest, MetricMemoryFootprint) {
    auto metric_double = compact_metric_value(
        create_metric_metadata("test", metric_type::gauge),
        42.0
    );

    auto metric_int = compact_metric_value(
        create_metric_metadata("test", metric_type::counter),
        static_cast<int64_t>(100)
    );

    auto metric_string = compact_metric_value(
        create_metric_metadata("test", metric_type::set),
        std::string("test_value_12345")
    );

    // Numeric metrics should have smaller footprint than string metrics
    EXPECT_GT(metric_double.memory_footprint(), 0);
    EXPECT_GT(metric_int.memory_footprint(), 0);
    EXPECT_GT(metric_string.memory_footprint(), metric_double.memory_footprint());
}

/**
 * Test 13: Metric Value Conversions
 * Test converting metric values between different types
 */
TEST_F(MetricsCollectionTest, MetricValueConversions) {
    auto metric_double = compact_metric_value(
        create_metric_metadata("test", metric_type::gauge),
        42.5
    );

    auto metric_int = compact_metric_value(
        create_metric_metadata("test", metric_type::counter),
        static_cast<int64_t>(100)
    );

    // Test conversions
    EXPECT_DOUBLE_EQ(metric_double.as_double(), 42.5);
    EXPECT_EQ(metric_double.as_int64(), 42);
    EXPECT_FALSE(metric_double.as_string().empty());

    EXPECT_EQ(metric_int.as_int64(), 100);
    EXPECT_DOUBLE_EQ(metric_int.as_double(), 100.0);
    EXPECT_FALSE(metric_int.as_string().empty());
}

/**
 * Test 14: Metric Timestamp Management
 * Test metric timestamp tracking
 */
TEST_F(MetricsCollectionTest, MetricTimestampManagement) {
    // Add small margin to account for Release build optimizations
    auto before = std::chrono::system_clock::now() - std::chrono::microseconds(1);

    auto metric = CreateMetric("timestamped_metric", metric_type::gauge, 42.0);

    auto after = std::chrono::system_clock::now() + std::chrono::microseconds(1);
    auto timestamp = metric.get_timestamp();

    // Timestamp should be between before and after (with tolerance)
    EXPECT_GE(timestamp, before);
    EXPECT_LE(timestamp, after);
}

/**
 * Test 15: Metric Hash Function
 * Test metric name hashing for fast lookup
 */
TEST_F(MetricsCollectionTest, MetricHashFunction) {
    auto hash1 = hash_metric_name("metric_one");
    auto hash2 = hash_metric_name("metric_two");
    auto hash3 = hash_metric_name("metric_one");  // Same as hash1

    // Different names should have different hashes (with high probability)
    EXPECT_NE(hash1, hash2);

    // Same name should always produce same hash
    EXPECT_EQ(hash1, hash3);

    // Hash should be non-zero
    EXPECT_NE(hash1, 0);
}
