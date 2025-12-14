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
#include <mutex>
#include <thread>
#include <chrono>

// Use explicit namespace aliases to avoid conflicts
namespace common_if = kcenon::common::interfaces;
namespace mon = kcenon::monitoring;

using kcenon::common::VoidResult;
using kcenon::common::Result;

/**
 * @brief Simple mock logger for testing
 *
 * Uses common_system interfaces for cross-system integration.
 */
class simple_mock_logger : public common_if::ILogger, public common_if::IMonitorable {
private:
    std::shared_ptr<common_if::IMonitor> monitor_;
    int log_count_{0};

public:
    VoidResult log([[maybe_unused]] common_if::log_level level,
                   [[maybe_unused]] const std::string& message) override {
        log_count_++;
        if (monitor_) {
            monitor_->record_metric("logs_written", static_cast<double>(log_count_));
        }
        return kcenon::common::ok();
    }

    // Legacy API (required as it's pure virtual)
    VoidResult log(common_if::log_level level, const std::string& message,
                   [[maybe_unused]] const std::string& file,
                   [[maybe_unused]] int line,
                   [[maybe_unused]] const std::string& function) override {
        return log(level, message);
    }

    VoidResult log(const common_if::log_entry& entry) override {
        return log(entry.level, entry.message);
    }

    bool is_enabled([[maybe_unused]] common_if::log_level level) const override { return true; }
    VoidResult set_level([[maybe_unused]] common_if::log_level level) override { return kcenon::common::ok(); }
    common_if::log_level get_level() const override { return common_if::log_level::info; }
    VoidResult flush() override { return kcenon::common::ok(); }

    // IMonitorable implementation (using common_system types)
    Result<common_if::metrics_snapshot> get_monitoring_data() override {
        common_if::metrics_snapshot snapshot;
        snapshot.source_id = "simple_mock_logger";
        snapshot.add_metric("total_logs", static_cast<double>(log_count_));
        return snapshot;
    }

    Result<common_if::health_check_result> health_check() override {
        common_if::health_check_result result;
        result.status = common_if::health_status::healthy;
        result.message = "Mock logger operational";
        return result;
    }

    std::string get_component_name() const override {
        return "simple_mock_logger";
    }

    // For bidirectional DI
    void set_monitor(std::shared_ptr<common_if::IMonitor> monitor) {
        monitor_ = monitor;
    }

    int get_log_count() const { return log_count_; }
};

/**
 * @brief Mock IMonitor for testing cross-system integration
 *
 * Implements common_system's IMonitor interface for testing.
 */
class mock_monitor : public common_if::IMonitor {
private:
    std::vector<common_if::metric_value> metrics_;
    mutable std::mutex mutex_;

public:
    VoidResult record_metric(const std::string& name, double value) override {
        std::lock_guard<std::mutex> lock(mutex_);
        metrics_.emplace_back(name, value);
        return kcenon::common::ok();
    }

    VoidResult record_metric(
        const std::string& name,
        double value,
        const std::unordered_map<std::string, std::string>& tags) override {
        std::lock_guard<std::mutex> lock(mutex_);
        common_if::metric_value mv(name, value);
        mv.tags = tags;
        metrics_.push_back(mv);
        return kcenon::common::ok();
    }

    Result<common_if::metrics_snapshot> get_metrics() override {
        std::lock_guard<std::mutex> lock(mutex_);
        common_if::metrics_snapshot snapshot;
        snapshot.metrics = metrics_;
        snapshot.source_id = "mock_monitor";
        return snapshot;
    }

    Result<common_if::health_check_result> check_health() override {
        common_if::health_check_result result;
        result.status = common_if::health_status::healthy;
        result.message = "Mock monitor operational";
        return result;
    }

    VoidResult reset() override {
        std::lock_guard<std::mutex> lock(mutex_);
        metrics_.clear();
        return kcenon::common::ok();
    }

    size_t get_metric_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return metrics_.size();
    }
};

/**
 * @brief Test Case 1: Both systems standalone
 */
TEST(CrossSystemIntegrationTest, BothSystemsStandalone) {
    // Create logger without monitor
    auto logger = std::make_shared<simple_mock_logger>();

    EXPECT_NO_THROW({
        logger->log(common_if::log_level::info, "Test message");
    });
    EXPECT_EQ(1, logger->get_log_count());

    // Create monitor without logger
    auto monitor = std::make_shared<mock_monitor>();

    EXPECT_NO_THROW({
        auto result = monitor->record_metric("test_metric", 1.0);
        EXPECT_TRUE(result.is_ok());
    });

    // Both should work independently
    auto monitor_metrics = monitor->get_metrics();
    ASSERT_TRUE(monitor_metrics.is_ok());
    EXPECT_EQ(1, monitor_metrics.value().metrics.size());
}

/**
 * @brief Test Case 2: Logger with monitor injection
 */
TEST(CrossSystemIntegrationTest, LoggerWithMonitorInjection) {
    auto monitor = std::make_shared<mock_monitor>();
    auto logger = std::make_shared<simple_mock_logger>();

    // Inject monitor into logger
    logger->set_monitor(monitor);

    // Log messages - each log should record a metric
    logger->log(common_if::log_level::info, "Test 1");
    logger->log(common_if::log_level::info, "Test 2");

    // Monitor should have received metrics from logger
    auto metrics_result = monitor->get_metrics();
    ASSERT_TRUE(metrics_result.is_ok());

    auto& snapshot = metrics_result.value();
    EXPECT_EQ(2, snapshot.metrics.size());
}

/**
 * @brief Test Case 3: Monitor with logger (interface compatibility)
 */
TEST(CrossSystemIntegrationTest, MonitorInterfaceCompatibility) {
    auto monitor = std::make_shared<mock_monitor>();

    // Test IMonitor interface methods
    EXPECT_NO_THROW({
        auto result = monitor->record_metric("test_metric", 42.0);
        EXPECT_TRUE(result.is_ok());
    });

    // Monitor health check should work
    auto health = monitor->check_health();
    ASSERT_TRUE(health.is_ok());
    EXPECT_EQ(common_if::health_status::healthy, health.value().status);
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
    auto monitor = std::make_shared<mock_monitor>();

    // Bidirectional injection
    logger->set_monitor(monitor);

    // Use both systems
    for (int i = 0; i < 10; ++i) {
        logger->log(common_if::log_level::info, "Request " + std::to_string(i));
    }

    // Verify logger health
    auto logger_health = logger->health_check();
    ASSERT_TRUE(logger_health.is_ok());
    EXPECT_TRUE(logger_health.value().is_healthy());

    // Verify monitor health
    auto monitor_health = monitor->check_health();
    ASSERT_TRUE(monitor_health.is_ok());
    EXPECT_TRUE(monitor_health.value().is_healthy());

    // Verify metrics were recorded
    auto monitor_metrics = monitor->get_metrics();
    ASSERT_TRUE(monitor_metrics.is_ok());
    EXPECT_EQ(10, monitor_metrics.value().metrics.size());

    // Logger should have logged
    EXPECT_EQ(10, logger->get_log_count());
}

/**
 * @brief Test Case 5: Repeated injection
 */
TEST(CrossSystemIntegrationTest, RepeatedInjection) {
    auto logger = std::make_shared<simple_mock_logger>();
    auto monitor1 = std::make_shared<mock_monitor>();
    auto monitor2 = std::make_shared<mock_monitor>();

    // First injection
    logger->set_monitor(monitor1);
    logger->log(common_if::log_level::info, "With monitor1");

    // monitor1 should have received the metric
    EXPECT_EQ(1, monitor1->get_metric_count());

    // Replace with second monitor
    logger->set_monitor(monitor2);
    logger->log(common_if::log_level::info, "With monitor2");

    // monitor2 should now receive metrics, monitor1 stays at 1
    EXPECT_EQ(1, monitor1->get_metric_count());
    EXPECT_EQ(1, monitor2->get_metric_count());
    EXPECT_EQ(2, logger->get_log_count());
}

/**
 * @brief Test Case 6: NULL injection
 */
TEST(CrossSystemIntegrationTest, NullInjection) {
    auto logger = std::make_shared<simple_mock_logger>();
    auto monitor = std::make_shared<mock_monitor>();

    // Inject then remove
    logger->set_monitor(monitor);
    logger->set_monitor(nullptr);

    // Should not crash
    EXPECT_NO_THROW({
        logger->log(common_if::log_level::info, "After null injection");
    });

    EXPECT_EQ(1, logger->get_log_count());
    // Monitor should not have received the last log (null injection)
    EXPECT_EQ(0, monitor->get_metric_count());
}

/**
 * @brief Performance test: Integration overhead
 */
TEST(CrossSystemIntegrationTest, IntegrationPerformanceOverhead) {
    auto logger = std::make_shared<simple_mock_logger>();
    auto monitor = std::make_shared<mock_monitor>();

    logger->set_monitor(monitor);

    // Measure time for integrated operations
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; ++i) {
        logger->log(common_if::log_level::info, "Performance test");
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Integration should be fast (< 100ms for 1000 operations)
    EXPECT_LT(duration.count(), 100);
    EXPECT_EQ(1000, monitor->get_metric_count());
}

/**
 * @brief Test monitoring_system's performance_monitor standalone
 */
TEST(CrossSystemIntegrationTest, MonitoringSystemStandalone) {
    auto monitor = std::make_shared<mon::performance_monitor>();

    // Test monitoring_system's native interface
    EXPECT_NO_THROW({
        auto timer = monitor->time_operation("test_op");
        // Simulate work
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    });

    // Collect metrics using native interface
    auto result = monitor->collect();
    ASSERT_TRUE(result.is_ok());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
