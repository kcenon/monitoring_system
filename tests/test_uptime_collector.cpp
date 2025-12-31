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
#include <kcenon/monitoring/collectors/uptime_collector.h>

#include <thread>

namespace kcenon {
namespace monitoring {
namespace {

// Test fixture for uptime_collector tests
class UptimeCollectorTest : public ::testing::Test {
   protected:
    void SetUp() override {
        collector_ = std::make_unique<uptime_collector>();
        std::unordered_map<std::string, std::string> config;
        collector_->initialize(config);
    }

    std::unique_ptr<uptime_collector> collector_;
};

// Test basic initialization
TEST_F(UptimeCollectorTest, InitializesSuccessfully) {
    EXPECT_NE(collector_, nullptr);
    EXPECT_EQ(collector_->get_name(), "uptime_collector");
}

// Test metric types returned
TEST_F(UptimeCollectorTest, ReturnsCorrectMetricTypes) {
    auto types = collector_->get_metric_types();
    EXPECT_FALSE(types.empty());

    // Check for expected metric types
    std::vector<std::string> expected = {
        "system_uptime_seconds",
        "system_boot_timestamp",
        "system_idle_seconds"
    };

    for (const auto& expected_type : expected) {
        EXPECT_NE(std::find(types.begin(), types.end(), expected_type), types.end())
            << "Missing metric type: " << expected_type;
    }
}

// Test configuration options
TEST_F(UptimeCollectorTest, ConfigurationOptions) {
    auto collector = std::make_unique<uptime_collector>();
    std::unordered_map<std::string, std::string> config;
    config["collect_idle_time"] = "false";
    EXPECT_TRUE(collector->initialize(config));

    auto stats = collector->get_statistics();
    EXPECT_DOUBLE_EQ(stats["collect_idle_time"], 0.0);
}

// Test disable collector
TEST_F(UptimeCollectorTest, CanBeDisabled) {
    auto collector = std::make_unique<uptime_collector>();
    std::unordered_map<std::string, std::string> config;
    config["enabled"] = "false";
    collector->initialize(config);

    auto metrics = collector->collect();
    EXPECT_TRUE(metrics.empty());

    auto stats = collector->get_statistics();
    EXPECT_DOUBLE_EQ(stats["enabled"], 0.0);
}

// Test statistics
TEST_F(UptimeCollectorTest, TracksStatistics) {
    // Collect some metrics
    collector_->collect();
    collector_->collect();

    auto stats = collector_->get_statistics();
    EXPECT_GE(stats["collection_count"], 2.0);
    EXPECT_GE(stats["collection_errors"], 0.0);
}

// Test collect returns metrics (graceful degradation when unavailable)
TEST_F(UptimeCollectorTest, CollectReturnsMetrics) {
    // Should not throw even if platform-specific metrics fail
    EXPECT_NO_THROW(collector_->collect());
}

// Test get_last_metrics
TEST_F(UptimeCollectorTest, GetLastMetrics) {
    collector_->collect();
    auto last = collector_->get_last_metrics();

    // Timestamp should be set
    auto now = std::chrono::system_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - last.timestamp);
    EXPECT_LT(diff.count(), 10);  // Within 10 seconds
}

// Test is_uptime_monitoring_available
TEST_F(UptimeCollectorTest, UptimeMonitoringAvailabilityCheck) {
    // This should return true or false depending on platform
    // Either result is valid - we just want to ensure it doesn't crash
    EXPECT_NO_THROW(collector_->is_uptime_monitoring_available());
}

// Test uptime_metrics structure
TEST(UptimeMetricsTest, DefaultInitialization) {
    uptime_metrics metrics;
    EXPECT_DOUBLE_EQ(metrics.uptime_seconds, 0.0);
    EXPECT_EQ(metrics.boot_timestamp, 0);
    EXPECT_DOUBLE_EQ(metrics.idle_seconds, 0.0);
    EXPECT_FALSE(metrics.metrics_available);
}

// Test uptime_info_collector basic functionality
TEST(UptimeInfoCollectorTest, BasicFunctionality) {
    uptime_info_collector collector;

    // Test availability check
    EXPECT_NO_THROW(collector.is_uptime_monitoring_available());

    // Test metrics collection
    auto metrics = collector.collect_metrics();

    // Timestamp should be set
    auto now = std::chrono::system_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - metrics.timestamp);
    EXPECT_LT(diff.count(), 10);
}

// Test multiple collections are stable
TEST_F(UptimeCollectorTest, MultipleCollectionsAreStable) {
    for (int i = 0; i < 10; ++i) {
        auto metrics = collector_->collect();
        // Should not crash or throw
        EXPECT_NO_THROW(collector_->get_statistics());
    }

    auto stats = collector_->get_statistics();
    EXPECT_GE(stats["collection_count"], 10.0);
}

// Test that metrics have correct tags when collected
TEST_F(UptimeCollectorTest, MetricsHaveCorrectTags) {
    auto metrics = collector_->collect();

    for (const auto& m : metrics) {
        // All metrics should have the collector tag
        auto it = m.tags.find("collector");
        if (it != m.tags.end()) {
            EXPECT_EQ(it->second, "uptime_collector");
        }
    }
}

// Test is_healthy reflects actual state
TEST_F(UptimeCollectorTest, IsHealthyReflectsState) {
    // When enabled, health depends on availability
    EXPECT_NO_THROW(collector_->is_healthy());

    // When disabled, collector is considered healthy (no errors)
    auto disabled_collector = std::make_unique<uptime_collector>();
    std::unordered_map<std::string, std::string> config;
    config["enabled"] = "false";
    disabled_collector->initialize(config);
    EXPECT_TRUE(disabled_collector->is_healthy());
}

// All platforms should have uptime monitoring available
TEST_F(UptimeCollectorTest, UptimeMonitoringAvailable) {
    EXPECT_TRUE(collector_->is_uptime_monitoring_available());
}

// Uptime metrics should be available on all platforms
TEST(UptimeInfoCollectorTest, ReturnsMetrics) {
    uptime_info_collector collector;

    if (collector.is_uptime_monitoring_available()) {
        auto metrics = collector.collect_metrics();
        EXPECT_TRUE(metrics.metrics_available);
    }
}

// Uptime should be positive and reasonable
TEST(UptimeInfoCollectorTest, UptimeIsPositive) {
    uptime_info_collector collector;

    if (collector.is_uptime_monitoring_available()) {
        auto metrics = collector.collect_metrics();
        EXPECT_GT(metrics.uptime_seconds, 0.0);
        // Reasonable upper bound: 10 years in seconds
        EXPECT_LT(metrics.uptime_seconds, 10.0 * 365.25 * 24.0 * 3600.0);
    }
}

// Boot timestamp should be in the past
TEST(UptimeInfoCollectorTest, BootTimestampInPast) {
    uptime_info_collector collector;

    if (collector.is_uptime_monitoring_available()) {
        auto metrics = collector.collect_metrics();
        auto now_epoch = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        EXPECT_LT(metrics.boot_timestamp, now_epoch);
        // Boot time should be after year 2000 (946684800 = 2000-01-01)
        EXPECT_GT(metrics.boot_timestamp, 946684800);
    }
}

// Test uptime consistency across multiple collections
TEST(UptimeInfoCollectorTest, UptimeIncreases) {
    uptime_info_collector collector;

    if (collector.is_uptime_monitoring_available()) {
        auto first = collector.collect_metrics();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        auto second = collector.collect_metrics();

        // Uptime should have increased (or stayed same within measurement precision)
        EXPECT_GE(second.uptime_seconds, first.uptime_seconds);
    }
}

#if defined(__linux__)
// Linux-specific test: Idle time should be available
TEST(UptimeInfoCollectorTest, LinuxIdleTimeAvailable) {
    uptime_info_collector collector;

    if (collector.is_uptime_monitoring_available()) {
        auto metrics = collector.collect_metrics();
        // On Linux, idle_seconds should be available and >= 0
        EXPECT_GE(metrics.idle_seconds, 0.0);
    }
}
#endif

}  // namespace
}  // namespace monitoring
}  // namespace kcenon
