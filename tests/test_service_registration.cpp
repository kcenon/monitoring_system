/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, monitoring_system contributors
All rights reserved.
*****************************************************************************/

/**
 * @file test_service_registration.cpp
 * @brief Unit tests for monitoring_system DI service registration
 *
 * Tests the integration between monitoring_system and common_system's
 * service container.
 *
 * @see TICKET-103 for integration requirements.
 */

#include <gtest/gtest.h>

#ifdef BUILD_WITH_COMMON_SYSTEM

#include "kcenon/monitoring/di/service_registration.h"
#include <kcenon/common/di/service_container.h>
#include <kcenon/common/interfaces/monitoring_interface.h>
#include <thread>
#include <vector>
#include <atomic>

namespace {

using namespace kcenon::monitoring;
using namespace kcenon::common;

/**
 * Test fixture for service registration tests
 */
class MonitorServiceRegistrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a fresh container for each test
        container_ = std::make_unique<di::service_container>();
    }

    void TearDown() override {
        container_->clear();
        container_.reset();
    }

    std::unique_ptr<di::service_container> container_;
};

/**
 * Test basic service registration with default configuration
 */
TEST_F(MonitorServiceRegistrationTest, RegisterWithDefaultConfig) {
    auto result = di::register_monitor_services(*container_);

    ASSERT_TRUE(result.is_ok()) << "Failed to register monitor services";
    EXPECT_TRUE(container_->is_registered<interfaces::IMonitor>());
}

/**
 * Test service resolution after registration
 */
TEST_F(MonitorServiceRegistrationTest, ResolveRegisteredService) {
    auto reg_result = di::register_monitor_services(*container_);
    ASSERT_TRUE(reg_result.is_ok());

    auto resolve_result = container_->resolve<interfaces::IMonitor>();
    ASSERT_TRUE(resolve_result.is_ok()) << "Failed to resolve IMonitor";

    auto monitor = resolve_result.value();
    EXPECT_NE(monitor, nullptr);
}

/**
 * Test that singleton lifetime returns same instance
 */
TEST_F(MonitorServiceRegistrationTest, SingletonLifetime) {
    di::monitor_registration_config config;
    config.lifetime = di::service_lifetime::singleton;

    auto reg_result = di::register_monitor_services(*container_, config);
    ASSERT_TRUE(reg_result.is_ok());

    auto monitor1 = container_->resolve<interfaces::IMonitor>().value();
    auto monitor2 = container_->resolve<interfaces::IMonitor>().value();

    EXPECT_EQ(monitor1, monitor2) << "Singleton should return same instance";
}

/**
 * Test custom configuration
 */
TEST_F(MonitorServiceRegistrationTest, CustomConfiguration) {
    di::monitor_registration_config config;
    config.monitor_name = "custom_test_monitor";
    config.cpu_threshold = 95.0;
    config.memory_threshold = 85.0;
    config.latency_threshold = std::chrono::milliseconds{500};
    config.enable_system_monitoring = false;

    auto result = di::register_monitor_services(*container_, config);
    ASSERT_TRUE(result.is_ok());

    auto monitor = container_->resolve<interfaces::IMonitor>().value();
    EXPECT_NE(monitor, nullptr);

    // Verify configuration was applied by checking the underlying monitor
    auto perf_monitor = di::get_underlying_performance_monitor(monitor);
    ASSERT_NE(perf_monitor, nullptr);

    auto thresholds = perf_monitor->get_thresholds();
    EXPECT_DOUBLE_EQ(thresholds.cpu_threshold, 95.0);
    EXPECT_DOUBLE_EQ(thresholds.memory_threshold, 85.0);
    EXPECT_EQ(thresholds.latency_threshold, std::chrono::milliseconds{500});
}

/**
 * Test double registration fails
 */
TEST_F(MonitorServiceRegistrationTest, DoubleRegistrationFails) {
    auto result1 = di::register_monitor_services(*container_);
    ASSERT_TRUE(result1.is_ok());

    auto result2 = di::register_monitor_services(*container_);
    EXPECT_TRUE(result2.is_err()) << "Double registration should fail";
}

/**
 * Test unregistration
 */
TEST_F(MonitorServiceRegistrationTest, UnregisterService) {
    auto reg_result = di::register_monitor_services(*container_);
    ASSERT_TRUE(reg_result.is_ok());
    ASSERT_TRUE(container_->is_registered<interfaces::IMonitor>());

    auto unreg_result = di::unregister_monitor_services(*container_);
    ASSERT_TRUE(unreg_result.is_ok());
    EXPECT_FALSE(container_->is_registered<interfaces::IMonitor>());
}

/**
 * Test register pre-configured instance
 */
TEST_F(MonitorServiceRegistrationTest, RegisterInstance) {
    auto monitor = std::make_shared<performance_monitor>("test_instance_monitor");
    monitor->set_cpu_threshold(70.0);

    auto result = di::register_monitor_instance(*container_, monitor);
    ASSERT_TRUE(result.is_ok());

    auto resolved = container_->resolve<interfaces::IMonitor>().value();
    EXPECT_NE(resolved, nullptr);

    auto perf_monitor = di::get_underlying_performance_monitor(resolved);
    ASSERT_NE(perf_monitor, nullptr);

    auto thresholds = perf_monitor->get_thresholds();
    EXPECT_DOUBLE_EQ(thresholds.cpu_threshold, 70.0);
}

/**
 * Test registering null instance fails
 */
TEST_F(MonitorServiceRegistrationTest, RegisterNullInstanceFails) {
    auto result = di::register_monitor_instance(*container_, nullptr);
    EXPECT_TRUE(result.is_err()) << "Registering null instance should fail";
}

/**
 * Test IMonitor interface functionality
 */
TEST_F(MonitorServiceRegistrationTest, IMonitorInterface) {
    auto reg_result = di::register_monitor_services(*container_);
    ASSERT_TRUE(reg_result.is_ok());

    auto monitor = container_->resolve<interfaces::IMonitor>().value();
    ASSERT_NE(monitor, nullptr);

    // Test record_metric
    auto record_result = monitor->record_metric("test_metric", 42.0);
    EXPECT_TRUE(record_result.is_ok());

    // Test get_metrics
    auto metrics_result = monitor->get_metrics();
    EXPECT_TRUE(metrics_result.is_ok());

    // Test check_health
    auto health_result = monitor->check_health();
    ASSERT_TRUE(health_result.is_ok());
    EXPECT_TRUE(health_result.value().is_operational());

    // Test reset
    auto reset_result = monitor->reset();
    EXPECT_TRUE(reset_result.is_ok());
}

/**
 * Test get_underlying_performance_monitor utility
 */
TEST_F(MonitorServiceRegistrationTest, GetUnderlyingMonitor) {
    auto reg_result = di::register_monitor_services(*container_);
    ASSERT_TRUE(reg_result.is_ok());

    auto imonitor = container_->resolve<interfaces::IMonitor>().value();
    auto perf_monitor = di::get_underlying_performance_monitor(imonitor);

    ASSERT_NE(perf_monitor, nullptr);
    EXPECT_EQ(perf_monitor->get_name(), "default_performance_monitor");
}

/**
 * Test thread safety of service resolution
 */
TEST_F(MonitorServiceRegistrationTest, ThreadSafeResolution) {
    di::monitor_registration_config config;
    config.lifetime = di::service_lifetime::singleton;

    auto reg_result = di::register_monitor_services(*container_, config);
    ASSERT_TRUE(reg_result.is_ok());

    std::vector<std::thread> threads;
    std::vector<std::shared_ptr<interfaces::IMonitor>> results(10);
    std::atomic<int> success_count{0};

    for (size_t i = 0; i < 10; ++i) {
        threads.emplace_back([this, &results, &success_count, i]() {
            auto result = container_->resolve<interfaces::IMonitor>();
            if (result.is_ok()) {
                results[i] = result.value();
                ++success_count;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), 10);

    // All threads should get the same singleton instance
    auto first = results[0];
    for (const auto& result : results) {
        EXPECT_EQ(result, first) << "All threads should get same singleton";
    }
}

/**
 * Test lock-free mode configuration
 */
TEST_F(MonitorServiceRegistrationTest, LockFreeMode) {
    di::monitor_registration_config config;
    config.enable_lock_free = true;

    auto result = di::register_monitor_services(*container_, config);
    ASSERT_TRUE(result.is_ok());

    auto monitor = container_->resolve<interfaces::IMonitor>().value();
    auto perf_monitor = di::get_underlying_performance_monitor(monitor);

    ASSERT_NE(perf_monitor, nullptr);
    EXPECT_TRUE(perf_monitor->get_profiler().is_lock_free_mode());
}

/**
 * Test using global container
 */
TEST(MonitorGlobalContainerTest, RegisterWithGlobalContainer) {
    auto& global = di::service_container::global();

    // Clean up any previous registrations
    if (global.is_registered<interfaces::IMonitor>()) {
        di::unregister_monitor_services(global);
    }

    auto result = di::register_monitor_services(global);
    ASSERT_TRUE(result.is_ok());

    auto monitor = global.resolve<interfaces::IMonitor>().value();
    EXPECT_NE(monitor, nullptr);

    // Clean up
    di::unregister_monitor_services(global);
    EXPECT_FALSE(global.is_registered<interfaces::IMonitor>());
}

} // anonymous namespace

#else // BUILD_WITH_COMMON_SYSTEM

// Stub test when common_system is not available
TEST(MonitorServiceRegistrationSkipTest, CommonSystemNotAvailable) {
    GTEST_SKIP() << "common_system integration not available - skipping service registration tests";
}

#endif // BUILD_WITH_COMMON_SYSTEM
