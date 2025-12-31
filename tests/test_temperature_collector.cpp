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
#include <kcenon/monitoring/collectors/temperature_collector.h>

namespace kcenon {
namespace monitoring {
namespace {

// Test fixture for temperature collector tests
class TemperatureCollectorTest : public ::testing::Test {
   protected:
    void SetUp() override {
        collector_ = std::make_unique<temperature_collector>();
        std::unordered_map<std::string, std::string> config;
        collector_->initialize(config);
    }

    std::unique_ptr<temperature_collector> collector_;
};

// Test basic initialization
TEST_F(TemperatureCollectorTest, InitializesSuccessfully) {
    EXPECT_TRUE(collector_->is_healthy());
    EXPECT_EQ(collector_->get_name(), "temperature_collector");
}

// Test metric types returned
TEST_F(TemperatureCollectorTest, ReturnsCorrectMetricTypes) {
    auto metric_types = collector_->get_metric_types();

    // Should include all expected temperature metrics
    EXPECT_FALSE(metric_types.empty());

    // Check for expected metric types
    auto contains = [&metric_types](const std::string& type) {
        return std::find(metric_types.begin(), metric_types.end(), type) != metric_types.end();
    };

    EXPECT_TRUE(contains("temperature_celsius"));
    EXPECT_TRUE(contains("temperature_critical_threshold"));
    EXPECT_TRUE(contains("temperature_warning_threshold"));
    EXPECT_TRUE(contains("temperature_is_critical"));
    EXPECT_TRUE(contains("temperature_is_warning"));
}

// Test configuration options
TEST_F(TemperatureCollectorTest, ConfigurationOptions) {
    auto custom_collector = std::make_unique<temperature_collector>();

    std::unordered_map<std::string, std::string> config = {
        {"enabled", "true"}, {"collect_thresholds", "true"}, {"collect_warnings", "true"}};

    EXPECT_TRUE(custom_collector->initialize(config));
    EXPECT_TRUE(custom_collector->is_healthy());
}

// Test disable collector
TEST_F(TemperatureCollectorTest, CanBeDisabled) {
    auto custom_collector = std::make_unique<temperature_collector>();

    std::unordered_map<std::string, std::string> config = {{"enabled", "false"}};

    custom_collector->initialize(config);

    // When disabled, collect should return empty
    auto metrics = custom_collector->collect();
    EXPECT_TRUE(metrics.empty());
}

// Test statistics
TEST_F(TemperatureCollectorTest, TracksStatistics) {
    auto stats = collector_->get_statistics();

    // Should have expected statistics keys
    EXPECT_TRUE(stats.find("collection_count") != stats.end());
    EXPECT_TRUE(stats.find("collection_errors") != stats.end());
    EXPECT_TRUE(stats.find("sensors_found") != stats.end());

    // Initial values should be 0
    EXPECT_EQ(stats["collection_count"], 0.0);
    EXPECT_EQ(stats["collection_errors"], 0.0);
}

// Test collect returns metrics (graceful degradation when sensors unavailable)
TEST_F(TemperatureCollectorTest, CollectReturnsMetrics) {
    auto metrics = collector_->collect();

    // This may return empty metrics if thermal sensors are not available
    // This is expected behavior - graceful degradation
    auto stats = collector_->get_statistics();
    EXPECT_GE(stats["collection_count"], 1.0);
}

// Test get_last_readings
TEST_F(TemperatureCollectorTest, GetLastReadings) {
    collector_->collect();
    auto last_readings = collector_->get_last_readings();

    // Should return vector (may be empty if thermal not available)
    // No assertion - just verify it doesn't crash
    (void)last_readings;
}

// Test is_thermal_available
TEST_F(TemperatureCollectorTest, ThermalAvailabilityCheck) {
    // Should not crash - returns true/false based on sensor availability
    bool available = collector_->is_thermal_available();
    (void)available;  // Use the variable to avoid warning
}

// Test temperature_reading structure
TEST(TemperatureReadingTest, DefaultInitialization) {
    temperature_reading reading;

    EXPECT_TRUE(reading.sensor.id.empty());
    EXPECT_TRUE(reading.sensor.name.empty());
    EXPECT_EQ(reading.sensor.type, sensor_type::unknown);
    EXPECT_EQ(reading.temperature_celsius, 0.0);
    EXPECT_EQ(reading.critical_threshold_celsius, 0.0);
    EXPECT_EQ(reading.warning_threshold_celsius, 0.0);
    EXPECT_FALSE(reading.thresholds_available);
    EXPECT_FALSE(reading.is_critical);
    EXPECT_FALSE(reading.is_warning);
}

// Test temperature_sensor_info structure
TEST(TemperatureSensorInfoTest, DefaultInitialization) {
    temperature_sensor_info info;

    EXPECT_TRUE(info.id.empty());
    EXPECT_TRUE(info.name.empty());
    EXPECT_TRUE(info.zone_path.empty());
    EXPECT_EQ(info.type, sensor_type::unknown);
}

// Test sensor_type_to_string conversion
TEST(SensorTypeTest, TypeToStringConversion) {
    EXPECT_EQ(sensor_type_to_string(sensor_type::cpu), "cpu");
    EXPECT_EQ(sensor_type_to_string(sensor_type::gpu), "gpu");
    EXPECT_EQ(sensor_type_to_string(sensor_type::motherboard), "motherboard");
    EXPECT_EQ(sensor_type_to_string(sensor_type::storage), "storage");
    EXPECT_EQ(sensor_type_to_string(sensor_type::ambient), "ambient");
    EXPECT_EQ(sensor_type_to_string(sensor_type::other), "other");
    EXPECT_EQ(sensor_type_to_string(sensor_type::unknown), "unknown");
}

// Test temperature_info_collector basic functionality
TEST(TemperatureInfoCollectorTest, BasicFunctionality) {
    temperature_info_collector collector;

    // Test thermal availability check (should not crash)
    bool available = collector.is_thermal_available();

    // If thermal is not available, enumerate_sensors should still work
    if (!available) {
        auto sensors = collector.enumerate_sensors();
        EXPECT_TRUE(sensors.empty());  // No sensors without thermal
    } else {
        // If thermal is available, we might get some sensors
        auto sensors = collector.enumerate_sensors();
        // Just verify it doesn't crash - actual sensor count depends on system
        (void)sensors;
    }
}

// Test sensor enumeration (graceful degradation)
TEST(TemperatureInfoCollectorTest, EnumerateSensors) {
    temperature_info_collector collector;

    auto sensors = collector.enumerate_sensors();

    // Should return a vector (may be empty if thermal not available)
    // No assertion on size - just verify it doesn't crash
    (void)sensors;
}

// Test multiple collections don't cause issues
TEST_F(TemperatureCollectorTest, MultipleCollectionsAreStable) {
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
TEST_F(TemperatureCollectorTest, MetricsHaveCorrectTags) {
    auto metrics = collector_->collect();

    for (const auto& m : metrics) {
        // If we have any metrics, check they have the expected tags
        if (!m.name.empty()) {
            EXPECT_TRUE(m.tags.find("sensor_id") != m.tags.end());
            EXPECT_TRUE(m.tags.find("sensor_name") != m.tags.end());
            EXPECT_TRUE(m.tags.find("sensor_type") != m.tags.end());
        }
    }
}

// Test read_all_temperatures
TEST(TemperatureInfoCollectorTest, ReadAllTemperatures) {
    temperature_info_collector collector;

    auto readings = collector.read_all_temperatures();

    // Should return a vector (may be empty if thermal not available)
    // No assertion on size - just verify it doesn't crash
    for (const auto& reading : readings) {
        // If we got readings, they should have valid sensor info
        EXPECT_FALSE(reading.sensor.id.empty());
    }
}

}  // namespace
}  // namespace monitoring
}  // namespace kcenon
