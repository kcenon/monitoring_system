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
using namespace monitoring_system;

class ErrorHandlingTest : public MonitoringSystemFixture {};

/**
 * Test 1: Invalid Metric Types
 * Test handling of invalid metric type values
 */
TEST_F(ErrorHandlingTest, InvalidMetricTypes) {
    // Create metric with valid types
    auto counter = CreateMetric("test", metric_type::counter, 10.0);
    auto gauge = CreateMetric("test", metric_type::gauge, 20.0);

    EXPECT_EQ(counter.metadata.type, metric_type::counter);
    EXPECT_EQ(gauge.metadata.type, metric_type::gauge);

    // Invalid type conversion should be handled gracefully
    auto type_str = metric_type_to_string(static_cast<metric_type>(255));
    EXPECT_STREQ(type_str, "unknown");
}

/**
 * Test 2: Duplicate Metric Registration
 * Test handling duplicate metric names
 */
TEST_F(ErrorHandlingTest, DuplicateMetricRegistration) {
    ASSERT_TRUE(StartMonitoring());

    // Register same metric multiple times
    ASSERT_TRUE(RecordSample("duplicate_test", std::chrono::microseconds(100)));
    ASSERT_TRUE(RecordSample("duplicate_test", std::chrono::microseconds(200)));
    ASSERT_TRUE(RecordSample("duplicate_test", std::chrono::microseconds(300)));

    // Should accumulate samples, not reject duplicates
    auto metrics = GetPerformanceMetrics("duplicate_test");
    ASSERT_TRUE(metrics.has_value());
    EXPECT_EQ(metrics->call_count, 3);
}

/**
 * Test 3: Missing Metric Errors
 * Test accessing metrics that don't exist
 */
TEST_F(ErrorHandlingTest, MissingMetricErrors) {
    ASSERT_TRUE(StartMonitoring());

    // Try to get non-existent metric
    auto metrics = GetPerformanceMetrics("non_existent_metric");
    EXPECT_FALSE(metrics.has_value());

    // Try to get metric value that doesn't exist
    auto value = GetMetricValue("non_existent_value");
    EXPECT_FALSE(value.has_value());
}

/**
 * Test 4: Storage Failures
 * Test handling of storage-related failures
 */
TEST_F(ErrorHandlingTest, StorageFailures) {
    TempMetricStorage storage("storage_failure_test");

    // Storage should be created
    EXPECT_FALSE(storage.path().empty());

    // Reading empty storage should return empty string
    auto contents = storage.read();
    EXPECT_TRUE(contents.empty());

    // Size of non-existent storage should be 0
    EXPECT_EQ(storage.size(), 0);
}

/**
 * Test 5: Export Failures and Retry
 * Test handling of export failures
 */
TEST_F(ErrorHandlingTest, ExportFailuresAndRetry) {
    MockMetricExporter exporter;

    // Set exporter to unhealthy
    exporter.set_healthy(false);
    EXPECT_FALSE(exporter.is_healthy());

    // Try to export while unhealthy
    auto batch = GenerateMetricBatch(10);
    exporter.export_metrics(batch.metrics);

    // Export should still be attempted
    EXPECT_EQ(exporter.get_export_count(), 1);

    // Set exporter back to healthy
    exporter.set_healthy(true);
    exporter.export_metrics(batch.metrics);

    EXPECT_EQ(exporter.get_export_count(), 2);
    EXPECT_TRUE(exporter.is_healthy());
}

/**
 * Test 6: Resource Exhaustion - Too Many Metrics
 * Test handling when too many metrics are created
 */
TEST_F(ErrorHandlingTest, ResourceExhaustionTooManyMetrics) {
    ASSERT_TRUE(StartMonitoring());

    // Create a large number of metrics
    const size_t large_count = 100000;

    for (size_t i = 0; i < large_count; ++i) {
        RecordSample("metric_" + std::to_string(i % 1000),
                    std::chrono::microseconds(100));
    }

    // System should still be operational
    auto metrics = GetPerformanceMetrics("metric_0");
    EXPECT_TRUE(metrics.has_value());
}

/**
 * Test 7: Corrupted Monitoring Data
 * Test handling of corrupted metric data
 */
TEST_F(ErrorHandlingTest, CorruptedMonitoringData) {
    // Create metric with extreme values
    auto extreme_metric = CreateMetric("extreme", metric_type::gauge,
                                      std::numeric_limits<double>::max());

    EXPECT_TRUE(std::isfinite(extreme_metric.as_double()));

    // Create metric with NaN (if supported)
    auto metadata = create_metric_metadata("nan_test", metric_type::gauge);
    auto nan_metric = compact_metric_value(metadata, std::numeric_limits<double>::quiet_NaN());

    // System should handle NaN gracefully
    EXPECT_TRUE(std::isnan(nan_metric.as_double()));
}

/**
 * Test 8: Alert Notification Failures
 * Test handling when alert notifications fail
 */
TEST_F(ErrorHandlingTest, AlertNotificationFailures) {
    ASSERT_TRUE(StartMonitoring());

    // Set very strict thresholds
    monitor_->set_cpu_threshold(0.01);
    monitor_->set_memory_threshold(0.01);
    monitor_->set_latency_threshold(std::chrono::milliseconds(1));

    // Record samples that will exceed thresholds
    RecordSample("alert_test", std::chrono::milliseconds(100));

    // Check thresholds (might fail notifications internally)
    auto result = monitor_->check_thresholds();

    // Should return a result even if notifications fail
    EXPECT_TRUE(result);
}

/**
 * Test 9: Concurrent Access Errors
 * Test handling of concurrent access errors
 */
TEST_F(ErrorHandlingTest, ConcurrentAccessErrors) {
    ASSERT_TRUE(StartMonitoring());

    const size_t num_threads = 10;
    std::vector<std::thread> threads;
    std::atomic<size_t> error_count{0};

    for (size_t t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, &error_count]() {
            for (size_t i = 0; i < 100; ++i) {
                try {
                    RecordSample("concurrent_error_test",
                               std::chrono::microseconds(100));
                } catch (...) {
                    error_count.fetch_add(1);
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Should have minimal or no errors
    EXPECT_EQ(error_count.load(), 0);
}

/**
 * Test 10: Invalid Configuration Errors
 * Test handling of invalid configuration parameters
 */
TEST_F(ErrorHandlingTest, InvalidConfigurationErrors) {
    collection_config config;

    // Valid configuration
    config.interval = std::chrono::seconds(1);
    config.batch_size = 100;
    auto result1 = config.validate();
    EXPECT_TRUE(result1.is_success());

    // Invalid interval
    config.interval = std::chrono::milliseconds(-1);
    auto result2 = config.validate();
    EXPECT_FALSE(result2.is_success());

    // Invalid batch size
    config.interval = std::chrono::seconds(1);
    config.batch_collection = true;
    config.batch_size = 0;
    auto result3 = config.validate();
    EXPECT_FALSE(result3.is_success());
}

/**
 * Test 11: Memory Allocation Failures
 * Test handling when memory allocation fails
 */
TEST_F(ErrorHandlingTest, MemoryAllocationFailures) {
    // Try to create very large batch
    metric_batch large_batch;
    large_batch.reserve(1000000);

    // Should not crash
    for (size_t i = 0; i < 1000; ++i) {
        large_batch.add_metric(CreateTestMetric("large_batch_" + std::to_string(i)));
    }

    EXPECT_GE(large_batch.size(), 1000);
}

/**
 * Test 12: Error Code Conversion
 * Test error code to string conversions
 */
TEST_F(ErrorHandlingTest, ErrorCodeConversion) {
    // Test various error codes
    EXPECT_STREQ(error_code_to_string(monitoring_error_code::success).c_str(),
                 "Success");

    EXPECT_STREQ(error_code_to_string(monitoring_error_code::collector_not_found).c_str(),
                 "Collector not found");

    EXPECT_STREQ(error_code_to_string(monitoring_error_code::storage_full).c_str(),
                 "Storage is full");

    EXPECT_STREQ(error_code_to_string(monitoring_error_code::invalid_configuration).c_str(),
                 "Invalid configuration");

    EXPECT_STREQ(error_code_to_string(monitoring_error_code::metric_not_found).c_str(),
                 "Metric not found");

    // Test error details
    auto details = get_error_details(monitoring_error_code::storage_full);
    EXPECT_FALSE(details.empty());
    EXPECT_NE(details.find("capacity"), std::string::npos);
}
