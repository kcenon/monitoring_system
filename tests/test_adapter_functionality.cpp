/**
 * @file test_adapter_functionality.cpp
 * @brief Phase 3.2 - Adapter Functionality Verification Tests
 *
 * Tests verify that adapters work correctly with:
 * - NULL loggers
 * - Mock loggers (interface-only)
 * - Real loggers (full integration)
 * - Runtime logger injection
 */

#include <gtest/gtest.h>
#include <kcenon/monitoring/adapters/logger_system_adapter.h>
#include <kcenon/monitoring/core/event_bus.h>
#include <kcenon/common/interfaces/logger_interface.h>
#include <memory>
#include <atomic>

using namespace kcenon::monitoring;
using namespace common::interfaces;
using common::VoidResult;
using common::Result;

/**
 * @brief Mock logger for testing (does not depend on logger_system)
 */
class mock_logger : public ILogger {
private:
    std::atomic<int> log_count_{0};
    log_level min_level_{log_level::trace};

public:
    int get_call_count() const { return log_count_.load(); }

    VoidResult log(log_level level, const std::string& message) override {
        log_count_++;
        return std::monostate{};
    }

    VoidResult log(log_level level, const std::string& message,
                   const std::string& file, int line,
                   const std::string& function) override {
        log_count_++;
        return std::monostate{};
    }

    VoidResult log(const log_entry& entry) override {
        log_count_++;
        return std::monostate{};
    }

    bool is_enabled(log_level level) const override {
        return level >= min_level_;
    }

    VoidResult set_level(log_level level) override {
        min_level_ = level;
        return std::monostate{};
    }

    log_level get_level() const override {
        return min_level_;
    }

    VoidResult flush() override {
        return std::monostate{};
    }
};

/**
 * @brief Test Scenario 1: Adapter with NULL logger
 */
TEST(AdapterFunctionalityTest, WorksWithoutLogger) {
    auto bus = std::make_shared<event_bus>();
    auto adapter = std::make_shared<logger_system_adapter>(bus, nullptr);

    // Should not crash with null logger
    EXPECT_FALSE(adapter->is_logger_system_available());
    EXPECT_NO_THROW({
        auto metrics = adapter->collect_metrics();
        EXPECT_TRUE(std::holds_alternative<std::vector<metric>>(metrics));
    });

    // Get log rate should return 0
    EXPECT_EQ(0.0, adapter->get_current_log_rate());
}

/**
 * @brief Test Scenario 2: Adapter with mock logger
 */
TEST(AdapterFunctionalityTest, WorksWithMockLogger) {
    auto bus = std::make_shared<event_bus>();
    auto logger = std::make_shared<mock_logger>();
    auto adapter = std::make_shared<logger_system_adapter>(bus, logger);

    EXPECT_TRUE(adapter->is_logger_system_available());

    // Adapter should be able to work with the logger interface
    EXPECT_NO_THROW({
        auto metrics = adapter->collect_metrics();
        EXPECT_TRUE(std::holds_alternative<std::vector<metric>>(metrics));
    });

    // Get logger should return the same instance
    auto retrieved = adapter->get_logger();
    EXPECT_EQ(logger, retrieved);
}

/**
 * @brief Test Scenario 3: Runtime logger injection
 */
TEST(AdapterFunctionalityTest, RuntimeLoggerInjection) {
    auto bus = std::make_shared<event_bus>();
    auto adapter = std::make_shared<logger_system_adapter>(bus, nullptr);

    EXPECT_FALSE(adapter->is_logger_system_available());

    // Inject logger at runtime
    auto logger = std::make_shared<mock_logger>();
    adapter->set_logger(logger);

    EXPECT_TRUE(adapter->is_logger_system_available());

    // Collect metrics should work now
    EXPECT_NO_THROW({
        auto metrics = adapter->collect_metrics();
        EXPECT_TRUE(std::holds_alternative<std::vector<metric>>(metrics));
    });

    // Replace with another logger
    auto logger2 = std::make_shared<mock_logger>();
    adapter->set_logger(logger2);

    auto retrieved = adapter->get_logger();
    EXPECT_EQ(logger2, retrieved);
    EXPECT_NE(logger, retrieved);
}

/**
 * @brief Test Scenario 4: Adapter with IMonitorable logger
 */
TEST(AdapterFunctionalityTest, WorksWithMonitorableLogger) {
    /**
     * This test demonstrates that the adapter can work with any
     * ILogger that also implements IMonitorable, without requiring
     * the concrete logger_system classes.
     */
    class monitorable_mock_logger : public mock_logger, public IMonitorable {
    public:
        Result<metrics_snapshot> get_monitoring_data() override {
            metrics_snapshot snapshot;
            snapshot.source_id = "mock_logger";
            snapshot.add_metric("messages_logged", 42.0);
            return snapshot;
        }

        Result<health_check_result> health_check() override {
            health_check_result result;
            result.status = health_status::healthy;
            result.message = "Mock logger operational";
            return result;
        }

        std::string get_component_name() const override {
            return "monitorable_mock_logger";
        }
    };

    auto bus = std::make_shared<event_bus>();
    auto logger = std::make_shared<monitorable_mock_logger>();
    auto adapter = std::make_shared<logger_system_adapter>(bus, logger);

    EXPECT_TRUE(adapter->is_logger_system_available());

    // Collect metrics should retrieve from IMonitorable
    auto metrics_result = adapter->collect_metrics();
    ASSERT_TRUE(std::holds_alternative<std::vector<metric>>(metrics_result));

    auto& metrics = std::get<std::vector<metric>>(metrics_result);
    EXPECT_FALSE(metrics.empty());

    // Should have the metric from the mock
    bool found_metric = false;
    for (const auto& m : metrics) {
        if (m.name == "messages_logged" && m.value == 42.0) {
            found_metric = true;
            break;
        }
    }
    EXPECT_TRUE(found_metric);
}

/**
 * @brief Test Scenario 5: Multiple adapters with different loggers
 */
TEST(AdapterFunctionalityTest, MultipleAdaptersIndependent) {
    auto bus = std::make_shared<event_bus>();

    auto logger1 = std::make_shared<mock_logger>();
    auto logger2 = std::make_shared<mock_logger>();

    auto adapter1 = std::make_shared<logger_system_adapter>(bus, logger1);
    auto adapter2 = std::make_shared<logger_system_adapter>(bus, logger2);

    EXPECT_TRUE(adapter1->is_logger_system_available());
    EXPECT_TRUE(adapter2->is_logger_system_available());

    EXPECT_NE(adapter1->get_logger(), adapter2->get_logger());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
