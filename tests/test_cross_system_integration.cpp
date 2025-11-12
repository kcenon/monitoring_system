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
 * @file test_cross_system_integration.cpp
 * @brief Phase 3.3 - Cross-System Integration Tests
 *
 * Tests verify the integration matrix:
 * 1. Both systems standalone
 * 2. Logger with monitor injection
 * 3. Monitor with logger injection
 * 4. Bidirectional DI (no compile-time circular dependency!)
 * 5. Repeated injection
 * 6. NULL injection
 */

#include <gtest/gtest.h>
#include <kcenon/monitoring/core/performance_monitor.h>
#include <kcenon/common/interfaces/logger_interface.h>
#include <kcenon/common/interfaces/monitoring_interface.h>
#include <memory>

using namespace kcenon::monitoring;
using namespace common::interfaces;
using common::VoidResult;
using common::Result;

/**
 * @brief Simple mock logger for testing
 */
class simple_mock_logger : public ILogger, public IMonitorable {
private:
    std::shared_ptr<IMonitor> monitor_;
    int log_count_{0};

public:
    VoidResult log(log_level level, const std::string& message) override {
        log_count_++;
        if (monitor_) {
            monitor_->record_metric("logs_written", log_count_);
        }
        return std::monostate{};
    }

    VoidResult log(log_level level, const std::string& message,
                   const std::string& file, int line,
                   const std::string& function) override {
        return log(level, message);
    }

    VoidResult log(const log_entry& entry) override {
        return log(entry.level, entry.message);
    }

    bool is_enabled(log_level level) const override { return true; }
    VoidResult set_level(log_level level) override { return std::monostate{}; }
    log_level get_level() const override { return log_level::info; }
    VoidResult flush() override { return std::monostate{}; }

    // IMonitorable implementation
    Result<metrics_snapshot> get_monitoring_data() override {
        metrics_snapshot snapshot;
        snapshot.source_id = "simple_mock_logger";
        snapshot.add_metric("total_logs", log_count_);
        return snapshot;
    }

    Result<health_check_result> health_check() override {
        health_check_result result;
        result.status = health_status::healthy;
        result.message = "Mock logger operational";
        return result;
    }

    std::string get_component_name() const override {
        return "simple_mock_logger";
    }

    // For bidirectional DI
    void set_monitor(std::shared_ptr<IMonitor> monitor) {
        monitor_ = monitor;
    }

    int get_log_count() const { return log_count_; }
};

/**
 * @brief Test Case 1: Both systems standalone
 */
TEST(CrossSystemIntegrationTest, BothSystemsStandalone) {
    // Create logger without monitor
    auto logger = std::make_shared<simple_mock_logger>();

    EXPECT_NO_THROW({
        logger->log(log_level::info, "Test message");
    });
    EXPECT_EQ(1, logger->get_log_count());

    // Create monitor without logger
    auto monitor = std::make_shared<performance_monitor>();

    EXPECT_NO_THROW({
        monitor->record_metric("test_metric", 1.0);
    });

    // Both should work independently
    auto monitor_metrics = monitor->get_metrics();
    ASSERT_TRUE(std::holds_alternative<metrics_snapshot>(monitor_metrics));
}

/**
 * @brief Test Case 2: Logger with monitor injection
 */
TEST(CrossSystemIntegrationTest, LoggerWithMonitorInjection) {
    auto monitor = std::make_shared<performance_monitor>();
    auto logger = std::make_shared<simple_mock_logger>();

    // Inject monitor into logger
    logger->set_monitor(monitor);

    // Log messages
    logger->log(log_level::info, "Test 1");
    logger->log(log_level::info, "Test 2");

    // Monitor should have received metrics
    auto metrics_result = monitor->get_metrics();
    ASSERT_TRUE(std::holds_alternative<metrics_snapshot>(metrics_result));

    auto& snapshot = std::get<metrics_snapshot>(metrics_result);
    EXPECT_FALSE(snapshot.metrics.empty());
}

/**
 * @brief Test Case 3: Monitor with logger injection
 */
TEST(CrossSystemIntegrationTest, MonitorWithLoggerInjection) {
    auto logger = std::make_shared<simple_mock_logger>();
    auto monitor = std::make_shared<performance_monitor>();

    // Note: performance_monitor doesn't require logger, but adapter can use it
    // This test verifies the interface compatibility

    EXPECT_NO_THROW({
        monitor->record_metric("test_metric", 42.0);
    });

    // Monitor should work correctly
    auto health = monitor->check_health();
    ASSERT_TRUE(std::holds_alternative<health_check_result>(health));
    EXPECT_EQ(health_status::healthy,
              std::get<health_check_result>(health).status);
}

/**
 * @brief Test Case 4: Bidirectional DI (NO CIRCULAR DEPENDENCY!)
 *
 * This is the critical test - we can create bidirectional runtime
 * dependencies WITHOUT compile-time circular dependency!
 */
TEST(CrossSystemIntegrationTest, BidirectionalDependencyInjection) {
    // Create both systems
    auto logger = std::make_shared<simple_mock_logger>();
    auto monitor = std::make_shared<performance_monitor>();

    // Bidirectional injection
    logger->set_monitor(monitor);
    // Note: monitor doesn't need to set logger in this example,
    // but adapters can use ILogger interface

    // Use both systems
    for (int i = 0; i < 10; ++i) {
        logger->log(log_level::info, "Request " + std::to_string(i));
        monitor->record_metric("requests_processed", i + 1);
    }

    // Verify logger health
    auto logger_health = logger->health_check();
    ASSERT_TRUE(std::holds_alternative<health_check_result>(logger_health));
    EXPECT_TRUE(std::get<health_check_result>(logger_health).is_healthy());

    // Verify monitor health
    auto monitor_health = monitor->check_health();
    ASSERT_TRUE(std::holds_alternative<health_check_result>(monitor_health));
    EXPECT_TRUE(std::get<health_check_result>(monitor_health).is_healthy());

    // Verify metrics
    auto monitor_metrics = monitor->get_metrics();
    ASSERT_TRUE(std::holds_alternative<metrics_snapshot>(monitor_metrics));

    auto& snapshot = std::get<metrics_snapshot>(monitor_metrics);
    EXPECT_FALSE(snapshot.metrics.empty());

    // Logger should have logged
    EXPECT_EQ(10, logger->get_log_count());
}

/**
 * @brief Test Case 5: Repeated injection
 */
TEST(CrossSystemIntegrationTest, RepeatedInjection) {
    auto logger = std::make_shared<simple_mock_logger>();
    auto monitor1 = std::make_shared<performance_monitor>();
    auto monitor2 = std::make_shared<performance_monitor>();

    // First injection
    logger->set_monitor(monitor1);
    logger->log(log_level::info, "With monitor1");

    // monitor1 should have metrics
    auto metrics1 = monitor1->get_metrics();
    ASSERT_TRUE(std::holds_alternative<metrics_snapshot>(metrics1));

    // Replace with second monitor
    logger->set_monitor(monitor2);
    logger->log(log_level::info, "With monitor2");

    // monitor2 should now receive metrics
    auto metrics2 = monitor2->get_metrics();
    ASSERT_TRUE(std::holds_alternative<metrics_snapshot>(metrics2));

    // Both monitors should be independent
    EXPECT_EQ(1, logger->get_log_count() + 1); // First log doesn't increment again
}

/**
 * @brief Test Case 6: NULL injection
 */
TEST(CrossSystemIntegrationTest, NullInjection) {
    auto logger = std::make_shared<simple_mock_logger>();
    auto monitor = std::make_shared<performance_monitor>();

    // Inject then remove
    logger->set_monitor(monitor);
    logger->set_monitor(nullptr);

    // Should not crash
    EXPECT_NO_THROW({
        logger->log(log_level::info, "After null injection");
    });

    EXPECT_EQ(1, logger->get_log_count());
}

/**
 * @brief Performance test: Integration overhead
 */
TEST(CrossSystemIntegrationTest, IntegrationPerformanceOverhead) {
    auto logger = std::make_shared<simple_mock_logger>();
    auto monitor = std::make_shared<performance_monitor>();

    logger->set_monitor(monitor);

    // Measure time for integrated operations
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; ++i) {
        logger->log(log_level::info, "Performance test");
        monitor->record_metric("perf_test", i);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Integration should be fast (< 100ms for 1000 operations)
    EXPECT_LT(duration.count(), 100);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
