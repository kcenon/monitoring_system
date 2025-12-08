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
#include <kcenon/monitoring/collectors/interrupt_collector.h>

#include <thread>  // for std::this_thread::sleep_for

namespace kcenon {
namespace monitoring {
namespace {

// Test fixture for interrupt_collector tests
class InterruptCollectorTest : public ::testing::Test {
   protected:
    void SetUp() override {
        collector_ = std::make_unique<interrupt_collector>();
        std::unordered_map<std::string, std::string> config;
        collector_->initialize(config);
    }

    std::unique_ptr<interrupt_collector> collector_;
};

// Test basic initialization
TEST_F(InterruptCollectorTest, InitializesSuccessfully) {
    EXPECT_NE(collector_, nullptr);
    EXPECT_EQ(collector_->get_name(), "interrupt_collector");
}

// Test metric types returned
TEST_F(InterruptCollectorTest, ReturnsCorrectMetricTypes) {
    auto types = collector_->get_metric_types();
    EXPECT_FALSE(types.empty());

    // Check for expected metric types
    std::vector<std::string> expected = {
        "interrupts_total",
        "interrupts_per_sec",
        "soft_interrupts_total",
        "soft_interrupts_per_sec"
    };

    for (const auto& expected_type : expected) {
        EXPECT_NE(std::find(types.begin(), types.end(), expected_type), types.end())
            << "Missing metric type: " << expected_type;
    }
}

// Test configuration options
TEST_F(InterruptCollectorTest, ConfigurationOptions) {
    auto collector = std::make_unique<interrupt_collector>();
    std::unordered_map<std::string, std::string> config;
    config["collect_per_cpu"] = "true";
    config["collect_soft_interrupts"] = "false";
    EXPECT_TRUE(collector->initialize(config));

    auto stats = collector->get_statistics();
    EXPECT_DOUBLE_EQ(stats["collect_per_cpu"], 1.0);
    EXPECT_DOUBLE_EQ(stats["collect_soft_interrupts"], 0.0);
}

// Test disable collector
TEST_F(InterruptCollectorTest, CanBeDisabled) {
    auto collector = std::make_unique<interrupt_collector>();
    std::unordered_map<std::string, std::string> config;
    config["enabled"] = "false";
    collector->initialize(config);

    auto metrics = collector->collect();
    EXPECT_TRUE(metrics.empty());

    auto stats = collector->get_statistics();
    EXPECT_DOUBLE_EQ(stats["enabled"], 0.0);
}

// Test statistics
TEST_F(InterruptCollectorTest, TracksStatistics) {
    // Collect some metrics
    collector_->collect();
    collector_->collect();

    auto stats = collector_->get_statistics();
    EXPECT_GE(stats["collection_count"], 2.0);
    EXPECT_GE(stats["collection_errors"], 0.0);
}

// Test collect returns metrics (graceful degradation when unavailable)
TEST_F(InterruptCollectorTest, CollectReturnsMetrics) {
    // Should not throw even if platform-specific metrics fail
    EXPECT_NO_THROW(collector_->collect());
}

// Test get_last_metrics
TEST_F(InterruptCollectorTest, GetLastMetrics) {
    collector_->collect();
    auto last = collector_->get_last_metrics();

    // Timestamp should be set
    auto now = std::chrono::system_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - last.timestamp);
    EXPECT_LT(diff.count(), 10);  // Within 10 seconds
}

// Test is_interrupt_monitoring_available
TEST_F(InterruptCollectorTest, InterruptMonitoringAvailabilityCheck) {
    // This should return true or false depending on platform
    // Either result is valid - we just want to ensure it doesn't crash
    EXPECT_NO_THROW(collector_->is_interrupt_monitoring_available());
}

// Test interrupt_metrics structure
TEST(InterruptMetricsTest, DefaultInitialization) {
    interrupt_metrics metrics;
    EXPECT_EQ(metrics.interrupts_total, 0);
    EXPECT_DOUBLE_EQ(metrics.interrupts_per_sec, 0.0);
    EXPECT_EQ(metrics.soft_interrupts_total, 0);
    EXPECT_DOUBLE_EQ(metrics.soft_interrupts_per_sec, 0.0);
    EXPECT_TRUE(metrics.per_cpu.empty());
    EXPECT_FALSE(metrics.metrics_available);
    EXPECT_FALSE(metrics.soft_interrupts_available);
}

// Test cpu_interrupt_info structure
TEST(CpuInterruptInfoTest, DefaultInitialization) {
    cpu_interrupt_info info;
    EXPECT_EQ(info.cpu_id, 0);
    EXPECT_EQ(info.interrupt_count, 0);
    EXPECT_DOUBLE_EQ(info.interrupts_per_sec, 0.0);
}

// Test interrupt_info_collector basic functionality
TEST(InterruptInfoCollectorTest, BasicFunctionality) {
    interrupt_info_collector collector;

    // Test availability check
    EXPECT_NO_THROW(collector.is_interrupt_monitoring_available());

    // Test metrics collection
    auto metrics = collector.collect_metrics();

    // Timestamp should be set
    auto now = std::chrono::system_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - metrics.timestamp);
    EXPECT_LT(diff.count(), 10);
}

// Test multiple collections are stable
TEST_F(InterruptCollectorTest, MultipleCollectionsAreStable) {
    for (int i = 0; i < 10; ++i) {
        auto metrics = collector_->collect();
        // Should not crash or throw
        EXPECT_NO_THROW(collector_->get_statistics());
    }

    auto stats = collector_->get_statistics();
    EXPECT_GE(stats["collection_count"], 10.0);
}

// Test that metrics have correct tags when collected
TEST_F(InterruptCollectorTest, MetricsHaveCorrectTags) {
    auto metrics = collector_->collect();

    for (const auto& m : metrics) {
        // All metrics should have the collector tag
        auto it = m.tags.find("collector");
        if (it != m.tags.end()) {
            EXPECT_EQ(it->second, "interrupt_collector");
        }
    }
}

// Test is_healthy reflects actual state
TEST_F(InterruptCollectorTest, IsHealthyReflectsState) {
    // When enabled, health depends on availability
    EXPECT_NO_THROW(collector_->is_healthy());

    // When disabled, should not be healthy
    auto disabled_collector = std::make_unique<interrupt_collector>();
    std::unordered_map<std::string, std::string> config;
    config["enabled"] = "false";
    disabled_collector->initialize(config);
    EXPECT_FALSE(disabled_collector->is_healthy());
}

#if defined(__linux__) || defined(__APPLE__)
// Platform-specific test: On Unix-like systems, interrupt monitoring should be available
TEST_F(InterruptCollectorTest, UnixInterruptMonitoringAvailable) {
    EXPECT_TRUE(collector_->is_interrupt_monitoring_available());
}

// Platform-specific test: Should have positive interrupt count on Unix
TEST(InterruptInfoCollectorTest, HasInterruptsOnUnix) {
    interrupt_info_collector collector;
    
    if (collector.is_interrupt_monitoring_available()) {
        auto metrics = collector.collect_metrics();
        EXPECT_TRUE(metrics.metrics_available);
        // Interrupt count should be positive on a running system
        EXPECT_GT(metrics.interrupts_total, 0);
    }
}

// Platform-specific test: Rate calculation works after multiple samples
TEST(InterruptInfoCollectorTest, RateCalculationWorks) {
    interrupt_info_collector collector;
    
    if (collector.is_interrupt_monitoring_available()) {
        // First sample - rate should be 0
        auto first = collector.collect_metrics();
        EXPECT_DOUBLE_EQ(first.interrupts_per_sec, 0.0);

        // Brief pause to allow some interrupts
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Second sample - rate should be calculated (may still be 0 if no change)
        auto second = collector.collect_metrics();
        // Rate is computed, no crash expected
        EXPECT_GE(second.interrupts_per_sec, 0.0);
    }
}
#endif

#if defined(_WIN32)
// Platform-specific test: On Windows, interrupt monitoring is not yet available
TEST_F(InterruptCollectorTest, WindowsInterruptMonitoringUnavailable) {
    EXPECT_FALSE(collector_->is_interrupt_monitoring_available());
}

// Platform-specific test: Windows metrics should indicate unavailability
TEST(InterruptInfoCollectorTest, WindowsReturnsUnavailableMetrics) {
    interrupt_info_collector collector;
    auto metrics = collector.collect_metrics();
    EXPECT_FALSE(metrics.metrics_available);
}
#endif

}  // namespace
}  // namespace monitoring
}  // namespace kcenon
