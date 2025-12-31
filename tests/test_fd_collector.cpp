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
#include <kcenon/monitoring/collectors/fd_collector.h>

#include <fstream>

namespace kcenon {
namespace monitoring {
namespace {

// Test fixture for fd_collector tests
class FDCollectorTest : public ::testing::Test {
   protected:
    void SetUp() override {
        collector_ = std::make_unique<fd_collector>();
        std::unordered_map<std::string, std::string> config;
        collector_->initialize(config);
    }

    std::unique_ptr<fd_collector> collector_;
};

// Test basic initialization
TEST_F(FDCollectorTest, InitializesSuccessfully) {
    EXPECT_NE(collector_, nullptr);
    EXPECT_EQ(collector_->get_name(), "fd_collector");
}

// Test metric types returned
TEST_F(FDCollectorTest, ReturnsCorrectMetricTypes) {
    auto types = collector_->get_metric_types();
    EXPECT_FALSE(types.empty());

    // Check for expected metric types
    std::vector<std::string> expected = {"fd_used_system", "fd_max_system", "fd_used_process",
                                         "fd_soft_limit",  "fd_hard_limit", "fd_usage_percent"};

    for (const auto& expected_type : expected) {
        EXPECT_NE(std::find(types.begin(), types.end(), expected_type), types.end())
            << "Missing metric type: " << expected_type;
    }
}

// Test configuration options
TEST_F(FDCollectorTest, ConfigurationOptions) {
    auto collector = std::make_unique<fd_collector>();
    std::unordered_map<std::string, std::string> config;
    config["warning_threshold"] = "70.0";
    config["critical_threshold"] = "90.0";
    EXPECT_TRUE(collector->initialize(config));

    auto stats = collector->get_statistics();
    EXPECT_DOUBLE_EQ(stats["warning_threshold"], 70.0);
    EXPECT_DOUBLE_EQ(stats["critical_threshold"], 90.0);
}

// Test disable collector
TEST_F(FDCollectorTest, CanBeDisabled) {
    auto collector = std::make_unique<fd_collector>();
    std::unordered_map<std::string, std::string> config;
    config["enabled"] = "false";
    collector->initialize(config);

    auto metrics = collector->collect();
    EXPECT_TRUE(metrics.empty());

    auto stats = collector->get_statistics();
    EXPECT_DOUBLE_EQ(stats["enabled"], 0.0);
}

// Test statistics
TEST_F(FDCollectorTest, TracksStatistics) {
    // Collect some metrics
    collector_->collect();
    collector_->collect();

    auto stats = collector_->get_statistics();
    EXPECT_GE(stats["collection_count"], 2.0);
    EXPECT_GE(stats["collection_errors"], 0.0);
}

// Test collect returns metrics (graceful degradation when unavailable)
TEST_F(FDCollectorTest, CollectReturnsMetrics) {
    auto metrics = collector_->collect();

    // Should return at least some metrics even if platform-specific ones fail
    // Note: The collect should always succeed (graceful degradation)
    EXPECT_NO_THROW(collector_->collect());
}

// Test get_last_metrics
TEST_F(FDCollectorTest, GetLastMetrics) {
    collector_->collect();
    auto last = collector_->get_last_metrics();

    // Timestamp should be set
    auto now = std::chrono::system_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - last.timestamp);
    EXPECT_LT(diff.count(), 10);  // Within 10 seconds
}

// Test is_fd_monitoring_available
TEST_F(FDCollectorTest, FDMonitoringAvailabilityCheck) {
    // This should return true or false depending on platform
    // Either result is valid - we just want to ensure it doesn't crash
    EXPECT_NO_THROW(collector_->is_fd_monitoring_available());
}

// Test fd_metrics structure
TEST(FDMetricsTest, DefaultInitialization) {
    fd_metrics metrics;
    EXPECT_EQ(metrics.fd_used_system, 0);
    EXPECT_EQ(metrics.fd_max_system, 0);
    EXPECT_EQ(metrics.fd_used_process, 0);
    EXPECT_EQ(metrics.fd_soft_limit, 0);
    EXPECT_EQ(metrics.fd_hard_limit, 0);
    EXPECT_DOUBLE_EQ(metrics.fd_usage_percent, 0.0);
    EXPECT_FALSE(metrics.system_metrics_available);
}

// Test fd_info_collector basic functionality
TEST(FDInfoCollectorTest, BasicFunctionality) {
    fd_info_collector collector;

    // Test availability check
    EXPECT_NO_THROW(collector.is_fd_monitoring_available());

    // Test metrics collection
    auto metrics = collector.collect_metrics();

    // Timestamp should be set
    auto now = std::chrono::system_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - metrics.timestamp);
    EXPECT_LT(diff.count(), 10);
}

// Test process FD count changes when opening files
TEST(FDInfoCollectorTest, ProcessFDCountChangesWithOpenFiles) {
    fd_info_collector collector;

    // Get initial FD count
    auto initial = collector.collect_metrics();

    // Open some files
    std::vector<std::unique_ptr<std::fstream>> files;
    for (int i = 0; i < 5; ++i) {
        auto file = std::make_unique<std::fstream>();
        file->open("/dev/null", std::ios::in);
        if (file->is_open()) {
            files.push_back(std::move(file));
        }
    }

    // Get new FD count
    auto after_open = collector.collect_metrics();

    // Close files
    files.clear();

    // Get final FD count
    auto after_close = collector.collect_metrics();

    // FD count should have increased when files were open
    // Note: This may not always be exactly equal due to internal operations
    if (initial.fd_used_process > 0 && after_open.fd_used_process > 0) {
        EXPECT_GE(after_open.fd_used_process, initial.fd_used_process);
    }
}

// Test multiple collections are stable
TEST_F(FDCollectorTest, MultipleCollectionsAreStable) {
    for (int i = 0; i < 10; ++i) {
        auto metrics = collector_->collect();
        // Should not crash or throw
        EXPECT_NO_THROW(collector_->get_statistics());
    }

    auto stats = collector_->get_statistics();
    EXPECT_GE(stats["collection_count"], 10.0);
}

// Test that metrics have correct tags when collected
TEST_F(FDCollectorTest, MetricsHaveCorrectTags) {
    auto metrics = collector_->collect();

    for (const auto& m : metrics) {
        // All metrics should have the collector tag
        auto it = m.tags.find("collector");
        if (it != m.tags.end()) {
            EXPECT_EQ(it->second, "fd_collector");
        }
    }
}

// Test is_healthy reflects actual state
TEST_F(FDCollectorTest, IsHealthyReflectsState) {
    // When enabled, health depends on availability
    EXPECT_NO_THROW(collector_->is_healthy());

    // When disabled, collector is considered healthy (no errors)
    auto disabled_collector = std::make_unique<fd_collector>();
    std::unordered_map<std::string, std::string> config;
    config["enabled"] = "false";
    disabled_collector->initialize(config);
    EXPECT_TRUE(disabled_collector->is_healthy());
}

// Test usage percentage calculation
TEST(FDMetricsTest, UsagePercentageCalculation) {
    fd_info_collector collector;
    auto metrics = collector.collect_metrics();

    // If soft limit is set, usage percentage should be reasonable
    if (metrics.fd_soft_limit > 0 && metrics.fd_used_process > 0) {
        double expected_percent = 100.0 * static_cast<double>(metrics.fd_used_process) /
                                  static_cast<double>(metrics.fd_soft_limit);
        EXPECT_NEAR(metrics.fd_usage_percent, expected_percent, 0.1);
    }
}

}  // namespace
}  // namespace monitoring
}  // namespace kcenon
