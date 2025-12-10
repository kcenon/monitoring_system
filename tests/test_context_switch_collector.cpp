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
#include <kcenon/monitoring/collectors/context_switch_collector.h>

namespace kcenon {
namespace monitoring {
namespace {

// Test fixture for context_switch_collector tests
class ContextSwitchCollectorTest : public ::testing::Test {
   protected:
    void SetUp() override {
        collector_ = std::make_unique<context_switch_collector>();
        std::unordered_map<std::string, std::string> config;
        collector_->initialize(config);
    }

    std::unique_ptr<context_switch_collector> collector_;
};

// Test basic initialization
TEST_F(ContextSwitchCollectorTest, InitializesSuccessfully) {
    EXPECT_NE(collector_, nullptr);
    EXPECT_EQ(collector_->get_name(), "context_switch_collector");
}

// Test metric types returned
TEST_F(ContextSwitchCollectorTest, ReturnsCorrectMetricTypes) {
    auto types = collector_->get_metric_types();
    EXPECT_FALSE(types.empty());

    // Check for expected metric types
    std::vector<std::string> expected = {
        "context_switches_total",
        "context_switches_per_sec",
        "voluntary_context_switches",
        "nonvoluntary_context_switches",
        "process_context_switches_total"
    };

    for (const auto& expected_type : expected) {
        EXPECT_NE(std::find(types.begin(), types.end(), expected_type), types.end())
            << "Missing metric type: " << expected_type;
    }
}

// Test configuration options
TEST_F(ContextSwitchCollectorTest, ConfigurationOptions) {
    auto collector = std::make_unique<context_switch_collector>();
    std::unordered_map<std::string, std::string> config;
    config["rate_warning_threshold"] = "50000.0";
    config["collect_process_metrics"] = "true";
    EXPECT_TRUE(collector->initialize(config));

    auto stats = collector->get_statistics();
    EXPECT_DOUBLE_EQ(stats["rate_warning_threshold"], 50000.0);
    EXPECT_DOUBLE_EQ(stats["collect_process_metrics"], 1.0);
}

// Test disable collector
TEST_F(ContextSwitchCollectorTest, CanBeDisabled) {
    auto collector = std::make_unique<context_switch_collector>();
    std::unordered_map<std::string, std::string> config;
    config["enabled"] = "false";
    collector->initialize(config);

    auto metrics = collector->collect();
    EXPECT_TRUE(metrics.empty());

    auto stats = collector->get_statistics();
    EXPECT_DOUBLE_EQ(stats["enabled"], 0.0);
}

// Test statistics
TEST_F(ContextSwitchCollectorTest, TracksStatistics) {
    // Collect some metrics
    collector_->collect();
    collector_->collect();

    auto stats = collector_->get_statistics();
    EXPECT_GE(stats["collection_count"], 2.0);
    EXPECT_GE(stats["collection_errors"], 0.0);
}

// Test collect returns metrics (graceful degradation when unavailable)
TEST_F(ContextSwitchCollectorTest, CollectReturnsMetrics) {
    // Should not throw even if platform-specific metrics fail
    EXPECT_NO_THROW(collector_->collect());
}

// Test get_last_metrics
TEST_F(ContextSwitchCollectorTest, GetLastMetrics) {
    collector_->collect();
    auto last = collector_->get_last_metrics();

    // Timestamp should be set
    auto now = std::chrono::system_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - last.timestamp);
    EXPECT_LT(diff.count(), 10);  // Within 10 seconds
}

// Test is_context_switch_monitoring_available
TEST_F(ContextSwitchCollectorTest, ContextSwitchMonitoringAvailabilityCheck) {
    // This should return true or false depending on platform
    // Either result is valid - we just want to ensure it doesn't crash
    EXPECT_NO_THROW(collector_->is_context_switch_monitoring_available());
}

// Test context_switch_metrics structure
TEST(ContextSwitchMetricsTest, DefaultInitialization) {
    context_switch_metrics metrics;
    EXPECT_EQ(metrics.system_context_switches_total, 0);
    EXPECT_DOUBLE_EQ(metrics.context_switches_per_sec, 0.0);
    EXPECT_FALSE(metrics.metrics_available);
    EXPECT_FALSE(metrics.rate_available);
}

// Test process_context_switch_info structure
TEST(ProcessContextSwitchInfoTest, DefaultInitialization) {
    process_context_switch_info info;
    EXPECT_EQ(info.voluntary_switches, 0);
    EXPECT_EQ(info.nonvoluntary_switches, 0);
    EXPECT_EQ(info.total_switches, 0);
}

// Test context_switch_info_collector basic functionality
TEST(ContextSwitchInfoCollectorTest, BasicFunctionality) {
    context_switch_info_collector collector;

    // Test availability check
    EXPECT_NO_THROW(collector.is_context_switch_monitoring_available());

    // Test metrics collection
    auto metrics = collector.collect_metrics();

    // Timestamp should be set
    auto now = std::chrono::system_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - metrics.timestamp);
    EXPECT_LT(diff.count(), 10);
}

// Test multiple collections are stable
TEST_F(ContextSwitchCollectorTest, MultipleCollectionsAreStable) {
    for (int i = 0; i < 10; ++i) {
        auto metrics = collector_->collect();
        // Should not crash or throw
        EXPECT_NO_THROW(collector_->get_statistics());
    }

    auto stats = collector_->get_statistics();
    EXPECT_GE(stats["collection_count"], 10.0);
}

// Test that metrics have correct tags when collected
TEST_F(ContextSwitchCollectorTest, MetricsHaveCorrectTags) {
    auto metrics = collector_->collect();

    for (const auto& m : metrics) {
        // All metrics should have the collector tag
        auto it = m.tags.find("collector");
        if (it != m.tags.end()) {
            EXPECT_EQ(it->second, "context_switch_collector");
        }
    }
}

// Test is_healthy reflects actual state
TEST_F(ContextSwitchCollectorTest, IsHealthyReflectsState) {
    // When enabled, health depends on availability
    EXPECT_NO_THROW(collector_->is_healthy());

    // When disabled, should not be healthy
    auto disabled_collector = std::make_unique<context_switch_collector>();
    std::unordered_map<std::string, std::string> config;
    config["enabled"] = "false";
    disabled_collector->initialize(config);
    EXPECT_FALSE(disabled_collector->is_healthy());
}

#if defined(__linux__) || defined(__APPLE__)
// Platform-specific test: On Unix-like systems, context switch monitoring should be available
TEST_F(ContextSwitchCollectorTest, UnixContextSwitchMonitoringAvailable) {
    EXPECT_TRUE(collector_->is_context_switch_monitoring_available());
}

// Platform-specific test: Should return valid metrics on Unix
TEST(ContextSwitchInfoCollectorTest, ReturnsMetricsOnUnix) {
    context_switch_info_collector collector;
    
    if (collector.is_context_switch_monitoring_available()) {
        auto metrics = collector.collect_metrics();
        EXPECT_TRUE(metrics.metrics_available);
    }
}

// Platform-specific test: Process context switches should be non-negative
TEST(ContextSwitchInfoCollectorTest, ProcessSwitchesNonNegative) {
    context_switch_info_collector collector;
    
    if (collector.is_context_switch_monitoring_available()) {
        auto metrics = collector.collect_metrics();
        EXPECT_GE(metrics.process_info.voluntary_switches, 0);
        EXPECT_GE(metrics.process_info.nonvoluntary_switches, 0);
        EXPECT_GE(metrics.process_info.total_switches, 0);
    }
}
#endif

#if defined(_WIN32)
// Platform-specific test: On Windows, context switch monitoring should not be available (stub)
TEST_F(ContextSwitchCollectorTest, WindowsContextSwitchMonitoringUnavailable) {
    EXPECT_FALSE(collector_->is_context_switch_monitoring_available());
}

// Platform-specific test: Windows metrics should indicate unavailability
TEST(ContextSwitchInfoCollectorTest, WindowsReturnsUnavailableMetrics) {
    context_switch_info_collector collector;
    auto metrics = collector.collect_metrics();
    EXPECT_FALSE(metrics.metrics_available);
}
#endif

}  // namespace
}  // namespace monitoring
}  // namespace kcenon
