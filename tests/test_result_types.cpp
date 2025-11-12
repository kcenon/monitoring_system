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

#include <gtest/gtest.h>
#include "kcenon/monitoring/core/result_types.h"
#include "kcenon/monitoring/core/error_codes.h"
#include "kcenon/monitoring/interfaces/monitoring_interface.h"

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
    auto result = make_success<int>(42);

    EXPECT_TRUE(result.is_ok());
    EXPECT_FALSE(result.is_err());
    EXPECT_EQ(result.value(), 42);
}

TEST_F(ResultTypesTest, ErrorResultContainsError) {
    auto result = make_error<int>(monitoring_error_code::collector_not_found, "Test error");

    EXPECT_FALSE(result.is_ok());
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(static_cast<monitoring_error_code>(result.error().code), monitoring_error_code::collector_not_found);
    EXPECT_EQ(result.error().message, "Test error");
}

TEST_F(ResultTypesTest, ValueOrReturnsDefaultOnError) {
    auto error_result = make_error<int>(monitoring_error_code::unknown_error);
    EXPECT_EQ(error_result.value_or(100), 100);
    
    auto success_result = make_success<int>(42);
    EXPECT_EQ(success_result.value_or(100), 42);
}

TEST_F(ResultTypesTest, MapTransformsSuccessValue) {
    auto result = make_success<int>(10);
    auto mapped = result.map([](int x) { return x * 2; });

    EXPECT_TRUE(mapped.is_ok());
    EXPECT_EQ(mapped.value(), 20);
}

TEST_F(ResultTypesTest, MapPropagatesError) {
    auto result = make_error<int>(monitoring_error_code::invalid_configuration);
    auto mapped = result.map([](int x) { return x * 2; });

    EXPECT_FALSE(mapped.is_ok());
    EXPECT_EQ(static_cast<monitoring_error_code>(mapped.error().code), monitoring_error_code::invalid_configuration);
}

TEST_F(ResultTypesTest, AndThenChainsOperations) {
    auto result = make_success<int>(10);
    auto chained = result.and_then([](int x) {
        if (x > 5) {
            return make_success<std::string>("Large");
        }
        return make_error<std::string>(monitoring_error_code::invalid_configuration);
    });

    EXPECT_TRUE(chained.is_ok());
    EXPECT_EQ(chained.value(), "Large");
}

TEST_F(ResultTypesTest, ResultVoidSuccess) {
    auto result = make_void_success();

    EXPECT_TRUE(result.is_ok());
    EXPECT_FALSE(result.is_err());
}

TEST_F(ResultTypesTest, ResultVoidError) {
    auto result = make_void_error(monitoring_error_code::storage_full, "Storage is full");

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
    auto result = make_error_with_context<int>(
        monitoring_error_code::collection_failed,
        "Failed to collect metrics",
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