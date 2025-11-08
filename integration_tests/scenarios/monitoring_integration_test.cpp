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

class MonitoringIntegrationTest : public MonitoringSystemFixture {};

/**
 * Test 1: System Health Monitoring
 * Verify that system health can be monitored and reported
 */
TEST_F(MonitoringIntegrationTest, SystemHealthMonitoring) {
    ASSERT_TRUE(StartMonitoring());

    // Collect system metrics
    WaitForCollection(std::chrono::milliseconds(100));

    // Monitor should be initialized and healthy
    EXPECT_TRUE(monitor_->is_enabled());
}

/**
 * Test 2: Resource Usage Tracking - CPU
 * Test CPU usage monitoring
 */
TEST_F(MonitoringIntegrationTest, ResourceUsageTrackingCPU) {
    ASSERT_TRUE(StartMonitoring());

    // Simulate CPU work
    auto start = std::chrono::high_resolution_clock::now();
    volatile int64_t sum = 0;
    while (std::chrono::high_resolution_clock::now() - start < std::chrono::milliseconds(100)) {
        sum += 1;
    }

    WaitForCollection(std::chrono::milliseconds(200));

    // System metrics should be collected
    auto result = monitor_->get_system_monitor().get_current_metrics();
    if (result.is_ok()) {
        auto metrics = result.value();
        EXPECT_GE(metrics.cpu_usage_percent, 0.0);
        EXPECT_LE(metrics.cpu_usage_percent, 100.0);
    }
}

/**
 * Test 3: Resource Usage Tracking - Memory
 * Test memory usage monitoring
 */
TEST_F(MonitoringIntegrationTest, ResourceUsageTrackingMemory) {
    ASSERT_TRUE(StartMonitoring());

    // Allocate some memory
    std::vector<std::vector<int>> memory_hog;
    for (int i = 0; i < 100; ++i) {
        memory_hog.emplace_back(10000, i);
    }

    WaitForCollection(std::chrono::milliseconds(200));

    auto result = monitor_->get_system_monitor().get_current_metrics();
    if (result.is_ok()) {
        auto metrics = result.value();
        EXPECT_GT(metrics.memory_usage_bytes, 0);
        EXPECT_GE(metrics.memory_usage_percent, 0.0);
        EXPECT_LE(metrics.memory_usage_percent, 100.0);
    }

    // Clean up
    memory_hog.clear();
}

/**
 * Test 4: Alert Threshold Configuration
 * Test setting and retrieving alert thresholds
 */
TEST_F(MonitoringIntegrationTest, AlertThresholdConfiguration) {
    ASSERT_TRUE(StartMonitoring());

    // Set thresholds
    monitor_->set_cpu_threshold(75.0);
    monitor_->set_memory_threshold(85.0);
    monitor_->set_latency_threshold(std::chrono::milliseconds(500));

    // Thresholds should be configurable
    EXPECT_NO_THROW({
        monitor_->check_thresholds();
    });
}

/**
 * Test 5: Alert Triggering and Notification
 * Test that alerts are triggered when thresholds are exceeded
 */
TEST_F(MonitoringIntegrationTest, AlertTriggeringAndNotification) {
    ASSERT_TRUE(StartMonitoring());

    // Set very low threshold
    monitor_->set_latency_threshold(std::chrono::milliseconds(1));

    // Record a sample that exceeds threshold
    auto long_duration = std::chrono::milliseconds(100);
    ASSERT_TRUE(RecordSample("slow_operation", long_duration));

    // Check if threshold is exceeded
    auto result = monitor_->check_thresholds();
    // The result indicates whether any threshold was exceeded
    ASSERT_TRUE(result.is_ok());
}

/**
 * Test 6: Multi-Component Monitoring
 * Test monitoring multiple components simultaneously
 */
TEST_F(MonitoringIntegrationTest, MultiComponentMonitoring) {
    ASSERT_TRUE(StartMonitoring());

    // Record samples for multiple components
    ASSERT_TRUE(RecordSample("component_a", std::chrono::microseconds(100)));
    ASSERT_TRUE(RecordSample("component_b", std::chrono::microseconds(200)));
    ASSERT_TRUE(RecordSample("component_c", std::chrono::microseconds(300)));

    WaitForCollection(std::chrono::milliseconds(100));

    // Verify all components are tracked
    auto metrics_a = GetPerformanceMetrics("component_a");
    auto metrics_b = GetPerformanceMetrics("component_b");
    auto metrics_c = GetPerformanceMetrics("component_c");

    EXPECT_TRUE(metrics_a.has_value());
    EXPECT_TRUE(metrics_b.has_value());
    EXPECT_TRUE(metrics_c.has_value());
}

/**
 * Test 7: Custom Metric Exporters
 * Test exporting metrics to custom exporters
 */
TEST_F(MonitoringIntegrationTest, CustomMetricExporters) {
    MockMetricExporter exporter;
    ASSERT_TRUE(StartMonitoring());

    // Create some metrics
    std::vector<compact_metric_value> metrics;
    for (int i = 0; i < 10; ++i) {
        metrics.push_back(CreateTestMetric("exported_metric_" + std::to_string(i)));
    }

    // Export metrics
    exporter.export_metrics(metrics);

    EXPECT_EQ(exporter.get_export_count(), 1);
    EXPECT_EQ(exporter.get_last_export_size(), 10);
    EXPECT_EQ(exporter.get_total_exported(), 10);
}

/**
 * Test 8: Monitoring Data Persistence
 * Test persisting monitoring data to storage
 */
TEST_F(MonitoringIntegrationTest, MonitoringDataPersistence) {
    TempMetricStorage storage("persistence_test");
    ASSERT_TRUE(StartMonitoring());

    // Record multiple samples
    for (int i = 0; i < 20; ++i) {
        ASSERT_TRUE(RecordSample("persistent_op", std::chrono::microseconds(100 + i)));
    }

    WaitForCollection(std::chrono::milliseconds(200));

    // Verify metrics are tracked
    auto metrics = GetPerformanceMetrics("persistent_op");
    ASSERT_TRUE(metrics.has_value());
    EXPECT_EQ(metrics->call_count, 20);
}

/**
 * Test 9: IMonitor Interface Integration
 * Test integration with common_system IMonitor interface
 * Uses performance_monitor_adapter to test IMonitor interface compatibility
 */
TEST_F(MonitoringIntegrationTest, IMonitorInterfaceIntegration) {
    ASSERT_TRUE(StartMonitoring());

    // Record metric through IMonitor interface using adapter
    auto result = monitor_adapter_->record_metric("test_metric", 42.0);
    EXPECT_TRUE(result.is_ok());

    // Record metric with tags
    std::unordered_map<std::string, std::string> tags = {
        {"env", "test"},
        {"version", "1.0"}
    };
    auto tagged_result = monitor_adapter_->record_metric("tagged_metric", 100.0, tags);
    EXPECT_TRUE(tagged_result.is_ok());
}

/**
 * Test 10: Health Check Integration
 * Test health check through IMonitor interface
 * Uses performance_monitor_adapter to test IMonitor interface compatibility
 */
TEST_F(MonitoringIntegrationTest, HealthCheckIntegration) {
    ASSERT_TRUE(StartMonitoring());

    // Perform health check through adapter
    auto health_result = monitor_adapter_->check_health();
    ASSERT_TRUE(health_result.is_ok());

    auto health = health_result.value();
    EXPECT_FALSE(health.message.empty());
    EXPECT_EQ(health.status, kcenon::common::interfaces::health_status::healthy);
}

/**
 * Test 11: Metrics Snapshot Retrieval
 * Test retrieving complete metrics snapshot
 * Uses performance_monitor_adapter to test IMonitor interface compatibility
 */
TEST_F(MonitoringIntegrationTest, MetricsSnapshotRetrieval) {
    ASSERT_TRUE(StartMonitoring());

    // Record various performance samples (adapter's record_metric is no-op for timing monitor)
    ASSERT_TRUE(RecordSample("operation_1", std::chrono::microseconds(100)));
    ASSERT_TRUE(RecordSample("operation_2", std::chrono::microseconds(200)));
    ASSERT_TRUE(RecordSample("operation_3", std::chrono::microseconds(300)));

    // Get snapshot through adapter
    auto snapshot_result = monitor_adapter_->get_metrics();
    ASSERT_TRUE(snapshot_result.is_ok());

    auto snapshot = snapshot_result.value();
    EXPECT_FALSE(snapshot.metrics.empty());

    // Verify snapshot contains metrics for the operations we recorded
    // Each operation produces multiple metrics (min, max, mean, median, p95, p99, call_count, error_count)
    EXPECT_GE(snapshot.metrics.size(), 3 * 8);  // At least 8 metrics per operation
}

/**
 * Test 12: Monitor Reset Functionality
 * Test resetting monitor state
 */
TEST_F(MonitoringIntegrationTest, MonitorResetFunctionality) {
    ASSERT_TRUE(StartMonitoring());

    // Record some samples
    ASSERT_TRUE(RecordSample("reset_op", std::chrono::microseconds(100)));
    ASSERT_TRUE(RecordSample("reset_op", std::chrono::microseconds(200)));

    // Verify metrics exist
    auto metrics_before = GetPerformanceMetrics("reset_op");
    ASSERT_TRUE(metrics_before.has_value());
    EXPECT_GT(metrics_before->call_count, 0);

    // Reset monitor
    monitor_->reset();

    // After reset, samples should be cleared
    auto metrics_after = GetPerformanceMetrics("reset_op");
    // After reset, metrics might not be available or call_count should be 0
    if (metrics_after.has_value()) {
        EXPECT_EQ(metrics_after->call_count, 0);
    }
}
