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
#include <kcenon/monitoring/collectors/power_collector.h>

namespace kcenon {
namespace monitoring {
namespace {

// Test fixture for power collector tests
class PowerCollectorTest : public ::testing::Test {
   protected:
    void SetUp() override {
        collector_ = std::make_unique<power_collector>();
        std::unordered_map<std::string, std::string> config;
        collector_->initialize(config);
    }

    std::unique_ptr<power_collector> collector_;
};

// Test basic initialization
TEST_F(PowerCollectorTest, InitializesSuccessfully) {
    EXPECT_TRUE(collector_->is_healthy());
    EXPECT_EQ(collector_->get_name(), "power_collector");
}

// Test metric types returned
TEST_F(PowerCollectorTest, ReturnsCorrectMetricTypes) {
    auto metric_types = collector_->get_metric_types();

    // Should include all expected power metrics
    EXPECT_FALSE(metric_types.empty());

    // Check for expected metric types
    auto contains = [&metric_types](const std::string& type) {
        return std::find(metric_types.begin(), metric_types.end(), type) != metric_types.end();
    };

    EXPECT_TRUE(contains("power_consumption_watts"));
    EXPECT_TRUE(contains("energy_consumed_joules"));
    EXPECT_TRUE(contains("power_limit_watts"));
    EXPECT_TRUE(contains("battery_percent"));
    EXPECT_TRUE(contains("battery_is_charging"));
}

// Test configuration options
TEST_F(PowerCollectorTest, ConfigurationOptions) {
    auto custom_collector = std::make_unique<power_collector>();

    std::unordered_map<std::string, std::string> config = {
        {"enabled", "true"}, {"collect_battery", "true"}, {"collect_rapl", "true"}};

    EXPECT_TRUE(custom_collector->initialize(config));
    EXPECT_TRUE(custom_collector->is_healthy());
}

// Test disable collector
TEST_F(PowerCollectorTest, CanBeDisabled) {
    auto custom_collector = std::make_unique<power_collector>();

    std::unordered_map<std::string, std::string> config = {{"enabled", "false"}};

    custom_collector->initialize(config);

    // When disabled, collect should return empty
    auto metrics = custom_collector->collect();
    EXPECT_TRUE(metrics.empty());
}

// Test statistics
TEST_F(PowerCollectorTest, TracksStatistics) {
    auto stats = collector_->get_statistics();

    // Should have expected statistics keys
    EXPECT_TRUE(stats.find("collection_count") != stats.end());
    EXPECT_TRUE(stats.find("collection_errors") != stats.end());
    EXPECT_TRUE(stats.find("sources_found") != stats.end());

    // Initial values should be 0
    EXPECT_EQ(stats["collection_count"], 0.0);
    EXPECT_EQ(stats["collection_errors"], 0.0);
}

// Test collect returns metrics (graceful degradation when sources unavailable)
TEST_F(PowerCollectorTest, CollectReturnsMetrics) {
    auto metrics = collector_->collect();

    // This may return empty metrics if power sources are not available
    // This is expected behavior - graceful degradation
    auto stats = collector_->get_statistics();
    EXPECT_GE(stats["collection_count"], 1.0);
}

// Test get_last_readings
TEST_F(PowerCollectorTest, GetLastReadings) {
    collector_->collect();
    auto last_readings = collector_->get_last_readings();

    // Should return vector (may be empty if power not available)
    // No assertion - just verify it doesn't crash
    (void)last_readings;
}

// Test is_power_available
TEST_F(PowerCollectorTest, PowerAvailabilityCheck) {
    // Should not crash - returns true/false based on power source availability
    bool available = collector_->is_power_available();
    (void)available;  // Use the variable to avoid warning
}

// Test power_reading structure
TEST(PowerReadingTest, DefaultInitialization) {
    power_reading reading;

    EXPECT_TRUE(reading.source.id.empty());
    EXPECT_TRUE(reading.source.name.empty());
    EXPECT_EQ(reading.source.type, power_source_type::unknown);
    EXPECT_EQ(reading.power_watts, 0.0);
    EXPECT_EQ(reading.energy_joules, 0.0);
    EXPECT_EQ(reading.power_limit_watts, 0.0);
    EXPECT_EQ(reading.voltage_volts, 0.0);
    EXPECT_EQ(reading.battery_percent, 0.0);
    EXPECT_FALSE(reading.power_available);
    EXPECT_FALSE(reading.battery_available);
    EXPECT_FALSE(reading.is_charging);
    EXPECT_FALSE(reading.is_discharging);
}

// Test power_source_info structure
TEST(PowerSourceInfoTest, DefaultInitialization) {
    power_source_info info;

    EXPECT_TRUE(info.id.empty());
    EXPECT_TRUE(info.name.empty());
    EXPECT_TRUE(info.path.empty());
    EXPECT_EQ(info.type, power_source_type::unknown);
}

// Test power_source_type_to_string conversion
TEST(PowerSourceTypeTest, TypeToStringConversion) {
    EXPECT_EQ(power_source_type_to_string(power_source_type::battery), "battery");
    EXPECT_EQ(power_source_type_to_string(power_source_type::ac), "ac");
    EXPECT_EQ(power_source_type_to_string(power_source_type::usb), "usb");
    EXPECT_EQ(power_source_type_to_string(power_source_type::wireless), "wireless");
    EXPECT_EQ(power_source_type_to_string(power_source_type::cpu), "cpu");
    EXPECT_EQ(power_source_type_to_string(power_source_type::gpu), "gpu");
    EXPECT_EQ(power_source_type_to_string(power_source_type::memory), "memory");
    EXPECT_EQ(power_source_type_to_string(power_source_type::package), "package");
    EXPECT_EQ(power_source_type_to_string(power_source_type::platform), "platform");
    EXPECT_EQ(power_source_type_to_string(power_source_type::other), "other");
    EXPECT_EQ(power_source_type_to_string(power_source_type::unknown), "unknown");
}

// Test power_info_collector basic functionality
TEST(PowerInfoCollectorTest, BasicFunctionality) {
    power_info_collector collector;

    // Test power availability check (should not crash)
    bool available = collector.is_power_available();

    // If power is not available, enumerate_sources should still work
    if (!available) {
        auto sources = collector.enumerate_sources();
        // Empty sources without power is acceptable
        (void)sources;
    } else {
        // If power is available, we might get some sources
        auto sources = collector.enumerate_sources();
        // Just verify it doesn't crash - actual source count depends on system
        (void)sources;
    }
}

// Test source enumeration (graceful degradation)
TEST(PowerInfoCollectorTest, EnumerateSources) {
    power_info_collector collector;

    auto sources = collector.enumerate_sources();

    // Should return a vector (may be empty if power not available)
    // No assertion on size - just verify it doesn't crash
    (void)sources;
}

// Test multiple collections don't cause issues
TEST_F(PowerCollectorTest, MultipleCollectionsAreStable) {
    for (int i = 0; i < 5; ++i) {
        auto metrics = collector_->collect();
        // Should not crash on repeated calls
        (void)metrics;
    }

    auto stats = collector_->get_statistics();
    EXPECT_GE(stats["collection_count"], 5.0);
    EXPECT_EQ(stats["collection_errors"], 0.0);
}

// Test that metrics have correct tags when collected
TEST_F(PowerCollectorTest, MetricsHaveCorrectTags) {
    auto metrics = collector_->collect();

    for (const auto& m : metrics) {
        // If we have any metrics, check they have the expected tags
        if (!m.name.empty()) {
            EXPECT_TRUE(m.tags.find("source_id") != m.tags.end());
            EXPECT_TRUE(m.tags.find("source_name") != m.tags.end());
            EXPECT_TRUE(m.tags.find("source_type") != m.tags.end());
        }
    }
}

// Test read_all_power
TEST(PowerInfoCollectorTest, ReadAllPower) {
    power_info_collector collector;

    auto readings = collector.read_all_power();

    // Should return a vector (may be empty if power not available)
    // No assertion on size - just verify it doesn't crash
    for (const auto& reading : readings) {
        // If we got readings, they should have valid source info
        EXPECT_FALSE(reading.source.id.empty());
    }
}

// Test battery-specific configuration
TEST_F(PowerCollectorTest, BatteryConfigurationDisabled) {
    auto custom_collector = std::make_unique<power_collector>();

    std::unordered_map<std::string, std::string> config = {
        {"enabled", "true"}, {"collect_battery", "false"}};

    EXPECT_TRUE(custom_collector->initialize(config));
    EXPECT_TRUE(custom_collector->is_healthy());

    // Collection should still work but may filter battery sources
    auto metrics = custom_collector->collect();
    (void)metrics;  // Just verify it runs
}

// Test RAPL-specific configuration
TEST_F(PowerCollectorTest, RaplConfigurationDisabled) {
    auto custom_collector = std::make_unique<power_collector>();

    std::unordered_map<std::string, std::string> config = {
        {"enabled", "true"}, {"collect_rapl", "false"}};

    EXPECT_TRUE(custom_collector->initialize(config));
    EXPECT_TRUE(custom_collector->is_healthy());

    // Collection should still work but may filter RAPL sources
    auto metrics = custom_collector->collect();
    (void)metrics;  // Just verify it runs
}

}  // namespace
}  // namespace monitoring
}  // namespace kcenon
