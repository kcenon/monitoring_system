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
#include <kcenon/monitoring/collectors/battery_collector.h>

#include <thread>

namespace kcenon {
namespace monitoring {
namespace {

// Test fixture for battery_collector tests
class BatteryCollectorTest : public ::testing::Test {
   protected:
    void SetUp() override {
        collector_ = std::make_unique<battery_collector>();
        std::unordered_map<std::string, std::string> config;
        collector_->initialize(config);
    }

    std::unique_ptr<battery_collector> collector_;
};

// Test basic initialization
TEST_F(BatteryCollectorTest, InitializesSuccessfully) {
    EXPECT_NE(collector_, nullptr);
    EXPECT_EQ(collector_->name(), "battery");
}

// Test metric types returned
TEST_F(BatteryCollectorTest, ReturnsCorrectMetricTypes) {
    auto types = collector_->get_metric_types();
    EXPECT_FALSE(types.empty());

    // Check for expected metric types
    std::vector<std::string> expected = {
        "battery_level_percent",
        "battery_charging",
        "battery_time_to_empty_seconds",
        "battery_time_to_full_seconds",
        "battery_health_percent",
        "battery_voltage_volts",
        "battery_power_watts",
        "battery_cycle_count",
        "battery_temperature_celsius"
    };

    for (const auto& expected_type : expected) {
        EXPECT_NE(std::find(types.begin(), types.end(), expected_type), types.end())
            << "Missing metric type: " << expected_type;
    }
}

// Test configuration options
TEST_F(BatteryCollectorTest, ConfigurationOptions) {
    auto collector = std::make_unique<battery_collector>();
    std::unordered_map<std::string, std::string> config;
    config["collect_health"] = "false";
    config["collect_thermal"] = "false";
    EXPECT_TRUE(collector->initialize(config));

    auto stats = collector->get_statistics();
    EXPECT_DOUBLE_EQ(stats["collect_health"], 0.0);
    EXPECT_DOUBLE_EQ(stats["collect_thermal"], 0.0);
}

// Test disable collector
TEST_F(BatteryCollectorTest, CanBeDisabled) {
    auto collector = std::make_unique<battery_collector>();
    std::unordered_map<std::string, std::string> config;
    config["enabled"] = "false";
    collector->initialize(config);

    auto metrics = collector->collect();
    EXPECT_TRUE(metrics.empty());

    auto stats = collector->get_statistics();
    EXPECT_DOUBLE_EQ(stats["enabled"], 0.0);
}

// Test statistics
TEST_F(BatteryCollectorTest, TracksStatistics) {
    // Collect some metrics
    collector_->collect();
    collector_->collect();

    auto stats = collector_->get_statistics();
    EXPECT_GE(stats["collection_count"], 2.0);
    EXPECT_GE(stats["collection_errors"], 0.0);
}

// Test collect returns metrics (graceful degradation when unavailable)
TEST_F(BatteryCollectorTest, CollectDoesNotThrow) {
    // Should not throw even if no battery is present
    EXPECT_NO_THROW(collector_->collect());
}

// Test get_last_readings
TEST_F(BatteryCollectorTest, GetLastReadings) {
    collector_->collect();
    auto readings = collector_->get_last_readings();

    // If no battery, readings will be empty - that's OK
    // If battery present, readings should have data
    for (const auto& reading : readings) {
        // Timestamp should be set
        auto now = std::chrono::system_clock::now();
        auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - reading.timestamp);
        EXPECT_LT(diff.count(), 10);  // Within 10 seconds
    }
}

// Test is_battery_available
TEST_F(BatteryCollectorTest, BatteryAvailabilityCheck) {
    // This should return true or false depending on hardware
    // Either result is valid - we just want to ensure it doesn't crash
    EXPECT_NO_THROW(collector_->is_battery_available());
}

// Test battery_info structure
TEST(BatteryInfoTest, DefaultInitialization) {
    battery_info info;
    EXPECT_TRUE(info.id.empty());
    EXPECT_TRUE(info.name.empty());
    EXPECT_TRUE(info.path.empty());
    EXPECT_TRUE(info.manufacturer.empty());
    EXPECT_TRUE(info.model.empty());
    EXPECT_TRUE(info.serial.empty());
    EXPECT_TRUE(info.technology.empty());
}

// Test battery_reading structure
TEST(BatteryReadingTest, DefaultInitialization) {
    battery_reading reading;
    EXPECT_DOUBLE_EQ(reading.level_percent, 0.0);
    EXPECT_EQ(reading.status, battery_status::unknown);
    EXPECT_FALSE(reading.is_charging);
    EXPECT_FALSE(reading.ac_connected);
    EXPECT_EQ(reading.time_to_empty_seconds, -1);
    EXPECT_EQ(reading.time_to_full_seconds, -1);
    EXPECT_DOUBLE_EQ(reading.design_capacity_wh, 0.0);
    EXPECT_DOUBLE_EQ(reading.full_charge_capacity_wh, 0.0);
    EXPECT_DOUBLE_EQ(reading.current_capacity_wh, 0.0);
    EXPECT_DOUBLE_EQ(reading.health_percent, 0.0);
    EXPECT_DOUBLE_EQ(reading.voltage_volts, 0.0);
    EXPECT_DOUBLE_EQ(reading.current_amps, 0.0);
    EXPECT_DOUBLE_EQ(reading.power_watts, 0.0);
    EXPECT_DOUBLE_EQ(reading.temperature_celsius, 0.0);
    EXPECT_FALSE(reading.temperature_available);
    EXPECT_EQ(reading.cycle_count, -1);
    EXPECT_FALSE(reading.battery_present);
    EXPECT_FALSE(reading.metrics_available);
}

// Test battery_status enum conversion
TEST(BatteryStatusTest, ToStringConversion) {
    EXPECT_EQ(battery_status_to_string(battery_status::unknown), "unknown");
    EXPECT_EQ(battery_status_to_string(battery_status::charging), "charging");
    EXPECT_EQ(battery_status_to_string(battery_status::discharging), "discharging");
    EXPECT_EQ(battery_status_to_string(battery_status::not_charging), "not_charging");
    EXPECT_EQ(battery_status_to_string(battery_status::full), "full");
}

// Test battery_info_collector basic functionality
TEST(BatteryInfoCollectorTest, BasicFunctionality) {
    battery_info_collector collector;

    // Test availability check
    EXPECT_NO_THROW(collector.is_battery_available());

    // Test enumeration
    EXPECT_NO_THROW(collector.enumerate_batteries());

    // Test read all
    EXPECT_NO_THROW(collector.read_all_batteries());
}

// Test multiple collections are stable
TEST_F(BatteryCollectorTest, MultipleCollectionsAreStable) {
    for (int i = 0; i < 10; ++i) {
        auto metrics = collector_->collect();
        // Should not crash or throw
        EXPECT_NO_THROW(collector_->get_statistics());
    }

    auto stats = collector_->get_statistics();
    EXPECT_GE(stats["collection_count"], 10.0);
}

// Test that metrics have correct tags when collected
TEST_F(BatteryCollectorTest, MetricsHaveCorrectTags) {
    auto metrics = collector_->collect();

    for (const auto& m : metrics) {
        // All metrics should have the collector tag
        auto it = m.tags.find("collector");
        if (it != m.tags.end()) {
            EXPECT_EQ(it->second, "battery_collector");
        }
    }
}

// Test is_healthy reflects actual state
TEST_F(BatteryCollectorTest, IsHealthyReflectsState) {
    // When enabled, health depends on battery availability
    EXPECT_NO_THROW(collector_->is_healthy());

    // When disabled, collector is considered healthy (no errors)
    auto disabled_collector = std::make_unique<battery_collector>();
    std::unordered_map<std::string, std::string> config;
    config["enabled"] = "false";
    disabled_collector->initialize(config);
    EXPECT_TRUE(disabled_collector->is_healthy());
}

// Test that battery metrics have battery_id tag
TEST_F(BatteryCollectorTest, MetricsHaveBatteryIdTag) {
    auto metrics = collector_->collect();

    // If we have metrics, they should have battery_id tag
    for (const auto& m : metrics) {
        auto it = m.tags.find("battery_id");
        // Battery metrics should have battery_id tag
        if (m.name.find("battery_") == 0) {
            EXPECT_NE(it, m.tags.end()) << "Missing battery_id tag for: " << m.name;
        }
    }
}

// Test reading all batteries when battery is present
TEST(BatteryInfoCollectorTest, ReadAllBatteriesWhenPresent) {
    battery_info_collector collector;

    if (collector.is_battery_available()) {
        auto readings = collector.read_all_batteries();
        EXPECT_FALSE(readings.empty());

        for (const auto& reading : readings) {
            EXPECT_TRUE(reading.battery_present);
            EXPECT_TRUE(reading.metrics_available);
            // Level should be in valid range
            EXPECT_GE(reading.level_percent, 0.0);
            EXPECT_LE(reading.level_percent, 100.0);
        }
    }
}

// Test battery level is in valid range when present
TEST_F(BatteryCollectorTest, BatteryLevelInValidRange) {
    auto readings = collector_->get_last_readings();
    collector_->collect();
    readings = collector_->get_last_readings();

    for (const auto& reading : readings) {
        if (reading.metrics_available) {
            EXPECT_GE(reading.level_percent, 0.0);
            EXPECT_LE(reading.level_percent, 100.0);
        }
    }
}

// Test health percentage is valid when available
TEST_F(BatteryCollectorTest, HealthPercentageIsValid) {
    collector_->collect();
    auto readings = collector_->get_last_readings();

    for (const auto& reading : readings) {
        if (reading.metrics_available && reading.health_percent > 0.0) {
            // Health should be between 0 and ~150% (batteries can sometimes exceed design capacity)
            EXPECT_GE(reading.health_percent, 0.0);
            EXPECT_LE(reading.health_percent, 150.0);
        }
    }
}

// Test voltage is positive when available
TEST_F(BatteryCollectorTest, VoltageIsPositive) {
    collector_->collect();
    auto readings = collector_->get_last_readings();

    for (const auto& reading : readings) {
        if (reading.metrics_available && reading.voltage_volts > 0.0) {
            // Voltage should be reasonable (0-50V covers most battery types)
            EXPECT_GT(reading.voltage_volts, 0.0);
            EXPECT_LT(reading.voltage_volts, 50.0);
        }
    }
}

// Test status consistency
TEST_F(BatteryCollectorTest, StatusConsistency) {
    collector_->collect();
    auto readings = collector_->get_last_readings();

    for (const auto& reading : readings) {
        if (reading.metrics_available) {
            // If is_charging is true, status should be charging
            if (reading.is_charging) {
                EXPECT_EQ(reading.status, battery_status::charging);
            }
            // If status is full, level should be high
            if (reading.status == battery_status::full) {
                EXPECT_GE(reading.level_percent, 90.0);
            }
        }
    }
}

// Test time estimates are reasonable
TEST_F(BatteryCollectorTest, TimeEstimatesAreReasonable) {
    collector_->collect();
    auto readings = collector_->get_last_readings();

    for (const auto& reading : readings) {
        if (reading.metrics_available) {
            // Time to empty should be reasonable if present (max 72 hours)
            if (reading.time_to_empty_seconds > 0) {
                EXPECT_LT(reading.time_to_empty_seconds, 72 * 3600);
            }
            // Time to full should be reasonable if present (max 24 hours)
            if (reading.time_to_full_seconds > 0) {
                EXPECT_LT(reading.time_to_full_seconds, 24 * 3600);
            }
        }
    }
}

// Test cycle count is non-negative when available
TEST_F(BatteryCollectorTest, CycleCountIsNonNegative) {
    collector_->collect();
    auto readings = collector_->get_last_readings();

    for (const auto& reading : readings) {
        if (reading.metrics_available && reading.cycle_count >= 0) {
            // Cycle count should be reasonable (max 10000)
            EXPECT_LT(reading.cycle_count, 10000);
        }
    }
}

// Test temperature is reasonable when available
TEST_F(BatteryCollectorTest, TemperatureIsReasonable) {
    collector_->collect();
    auto readings = collector_->get_last_readings();

    for (const auto& reading : readings) {
        if (reading.metrics_available && reading.temperature_available) {
            // Temperature should be reasonable (-40 to 100 Celsius)
            EXPECT_GT(reading.temperature_celsius, -40.0);
            EXPECT_LT(reading.temperature_celsius, 100.0);
        }
    }
}

}  // namespace
}  // namespace monitoring
}  // namespace kcenon
