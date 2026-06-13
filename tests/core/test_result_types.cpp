// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include <gtest/gtest.h>
#include "kcenon/monitoring/core/result_types.h"
#include "kcenon/monitoring/core/error_codes.h"
#include "kcenon/monitoring/interfaces/monitoring_core.h"
#include <kcenon/common/error/error_codes.h>

using namespace kcenon::monitoring;

/**
 * @brief Test basic Result pattern functionality
 */
class ResultTypesTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ResultTypesTest, SuccessResultContainsValue) {
    auto result = kcenon::common::ok(42);

    EXPECT_TRUE(result.is_ok());
    EXPECT_FALSE(result.is_err());
    EXPECT_EQ(result.value(), 42);
}

TEST_F(ResultTypesTest, ErrorResultContainsError) {
    auto result = kcenon::common::make_error<int>(static_cast<int>(monitoring_error_code::collector_not_found), "Test error");

    EXPECT_FALSE(result.is_ok());
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(static_cast<monitoring_error_code>(result.error().code), monitoring_error_code::collector_not_found);
    EXPECT_EQ(result.error().message, "Test error");
}

TEST_F(ResultTypesTest, ValueOrReturnsDefaultOnError) {
    auto error_result = kcenon::common::make_error<int>(static_cast<int>(monitoring_error_code::unknown_error), "");
    EXPECT_EQ(error_result.value_or(100), 100);

    auto success_result = kcenon::common::ok(42);
    EXPECT_EQ(success_result.value_or(100), 42);
}

TEST_F(ResultTypesTest, MapTransformsSuccessValue) {
    auto result = kcenon::common::ok(10);
    auto mapped = result.map([](int x) { return x * 2; });

    EXPECT_TRUE(mapped.is_ok());
    EXPECT_EQ(mapped.value(), 20);
}

TEST_F(ResultTypesTest, MapPropagatesError) {
    auto result = kcenon::common::make_error<int>(static_cast<int>(monitoring_error_code::invalid_configuration), "");
    auto mapped = result.map([](int x) { return x * 2; });

    EXPECT_FALSE(mapped.is_ok());
    EXPECT_EQ(static_cast<monitoring_error_code>(mapped.error().code), monitoring_error_code::invalid_configuration);
}

TEST_F(ResultTypesTest, AndThenChainsOperations) {
    auto result = kcenon::common::ok(10);
    auto chained = result.and_then([](int x) {
        if (x > 5) {
            return kcenon::common::ok(std::string("Large"));
        }
        return kcenon::common::make_error<std::string>(static_cast<int>(monitoring_error_code::invalid_configuration), "");
    });

    EXPECT_TRUE(chained.is_ok());
    EXPECT_EQ(chained.value(), "Large");
}

TEST_F(ResultTypesTest, ResultVoidSuccess) {
    auto result = kcenon::common::ok();

    EXPECT_TRUE(result.is_ok());
    EXPECT_FALSE(result.is_err());
}

TEST_F(ResultTypesTest, ResultVoidError) {
    auto result = kcenon::common::VoidResult::err(static_cast<int>(monitoring_error_code::storage_full), "Storage is full");

    EXPECT_FALSE(result.is_ok());
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(static_cast<monitoring_error_code>(result.error().code), monitoring_error_code::storage_full);
}

TEST_F(ResultTypesTest, ErrorCodeToString) {
    EXPECT_EQ(error_code_to_string(monitoring_error_code::success), "Success");
    EXPECT_EQ(error_code_to_string(monitoring_error_code::collector_not_found), "Collector not found");
    EXPECT_EQ(error_code_to_string(monitoring_error_code::storage_full), "Storage is full");
    EXPECT_EQ(error_code_to_string(monitoring_error_code::invalid_configuration), "Invalid configuration");
}

TEST_F(ResultTypesTest, ErrorInfoWithContext) {
    auto result = kcenon::common::make_error<int>(
        static_cast<int>(monitoring_error_code::collection_failed),
        "Failed to collect metrics",
        "",
        "CPU collector timeout"
    );

    EXPECT_FALSE(result.is_ok());
    EXPECT_EQ(static_cast<monitoring_error_code>(result.error().code), monitoring_error_code::collection_failed);
    EXPECT_EQ(result.error().message, "Failed to collect metrics");
    EXPECT_TRUE(result.error().details.has_value());
    EXPECT_EQ(result.error().details.value(), "CPU collector timeout");
}

TEST_F(ResultTypesTest, MetricsSnapshotOperations) {
    metrics_snapshot snapshot;
    snapshot.add_metric("cpu_usage", 45.5);
    snapshot.add_metric("memory_usage", 2048.0);
    
    EXPECT_EQ(snapshot.metrics.size(), 2);
    
    auto cpu = snapshot.get_metric("cpu_usage");
    EXPECT_TRUE(cpu.has_value());
    EXPECT_EQ(cpu.value(), 45.5);
    
    auto unknown = snapshot.get_metric("unknown_metric");
    EXPECT_FALSE(unknown.has_value());
}

TEST_F(ResultTypesTest, MonitoringConfigValidation) {
    monitoring_config config;

    // Valid configuration
    config.history_size = 100;
    config.collection_interval = std::chrono::milliseconds(100);
    config.buffer_size = 1000;
    auto result = config.validate();
    EXPECT_TRUE(result.is_ok());

    // Invalid history size
    config.history_size = 0;
    result = config.validate();
    EXPECT_FALSE(result.is_ok());
    EXPECT_EQ(static_cast<monitoring_error_code>(result.error().code), monitoring_error_code::invalid_capacity);

    // Invalid interval
    config.history_size = 100;
    config.collection_interval = std::chrono::milliseconds(5);
    result = config.validate();
    EXPECT_FALSE(result.is_ok());
    EXPECT_EQ(static_cast<monitoring_error_code>(result.error().code), monitoring_error_code::invalid_interval);

    // Invalid buffer size
    config.collection_interval = std::chrono::milliseconds(100);
    config.buffer_size = 50; // Less than history_size
    result = config.validate();
    EXPECT_FALSE(result.is_ok());
    EXPECT_EQ(static_cast<monitoring_error_code>(result.error().code), monitoring_error_code::invalid_capacity);
}

/**
 * @brief Verify monitoring error codes land in common's reserved
 *        negative band [-399, -300] and classify as "MonitoringSystem".
 *
 * Regression guard for issue #697: monitoring_error_code was a positive
 * enum, which common's classifier (after #698) rejects as "Invalid" because
 * any code > 0 is out of range. Codes must be negative and within
 * common_system's reserved monitoring band so they are correctly attributed.
 */
TEST_F(ResultTypesTest, ErrorCodesAreInCommonMonitoringBand) {
    // Representative codes spanning several category sub-bands.
    const monitoring_error_code samples[] = {
        monitoring_error_code::collector_not_found,
        monitoring_error_code::storage_full,
        monitoring_error_code::invalid_configuration,
        monitoring_error_code::circuit_breaker_open,
        monitoring_error_code::transaction_failed,
        monitoring_error_code::unknown_error,
    };

    for (auto code : samples) {
        const int value = static_cast<int>(code);
        EXPECT_LT(value, 0) << "code must be negative";
        EXPECT_GE(value, -399) << "code must be >= -399";
        EXPECT_LE(value, -300) << "code must be <= -300";
        EXPECT_EQ(kcenon::common::error::get_category_name(value),
                  "MonitoringSystem")
            << "common must classify code " << value << " as MonitoringSystem";
    }
}

/**
 * @brief Verify to_common_error() preserves the negative code unchanged
 *        and that the result classifies as "MonitoringSystem".
 */
TEST_F(ResultTypesTest, ToCommonErrorPreservesNegativeCode) {
    error_info err(monitoring_error_code::collection_failed,
                   "Failed to collect");
    kcenon::common::error_info common_err = err.to_common_error();

    EXPECT_EQ(common_err.code,
              static_cast<int>(monitoring_error_code::collection_failed));
    EXPECT_LT(common_err.code, 0);
    EXPECT_EQ(kcenon::common::error::get_category_name(common_err.code),
              "MonitoringSystem");
}

TEST_F(ResultTypesTest, HealthCheckResult) {
    health_check_result health;
    
    EXPECT_EQ(health.status, health_status::unknown);
    EXPECT_FALSE(health.is_healthy());
    
    health.status = health_status::healthy;
    EXPECT_TRUE(health.is_healthy());
    
    health.status = health_status::degraded;
    health.issues.push_back("High memory usage");
    EXPECT_FALSE(health.is_healthy());
    EXPECT_EQ(health.issues.size(), 1);
}