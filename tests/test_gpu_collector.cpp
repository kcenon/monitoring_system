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
#include <kcenon/monitoring/collectors/gpu_collector.h>

namespace kcenon {
namespace monitoring {
namespace {

// Test fixture for GPU collector tests
class GpuCollectorTest : public ::testing::Test {
   protected:
    void SetUp() override {
        collector_ = std::make_unique<gpu_collector>();
        std::unordered_map<std::string, std::string> config;
        collector_->initialize(config);
    }

    std::unique_ptr<gpu_collector> collector_;
};

// Test basic initialization
TEST_F(GpuCollectorTest, InitializesSuccessfully) {
    EXPECT_TRUE(collector_->is_healthy());
    EXPECT_EQ(collector_->get_name(), "gpu_collector");
}

// Test metric types returned
TEST_F(GpuCollectorTest, ReturnsCorrectMetricTypes) {
    auto metric_types = collector_->get_metric_types();

    // Should include all expected GPU metrics
    EXPECT_FALSE(metric_types.empty());

    // Check for expected metric types
    auto contains = [&metric_types](const std::string& type) {
        return std::find(metric_types.begin(), metric_types.end(), type) != metric_types.end();
    };

    EXPECT_TRUE(contains("gpu_utilization_percent"));
    EXPECT_TRUE(contains("gpu_memory_used_bytes"));
    EXPECT_TRUE(contains("gpu_memory_total_bytes"));
    EXPECT_TRUE(contains("gpu_memory_usage_percent"));
    EXPECT_TRUE(contains("gpu_temperature_celsius"));
    EXPECT_TRUE(contains("gpu_power_watts"));
    EXPECT_TRUE(contains("gpu_clock_mhz"));
    EXPECT_TRUE(contains("gpu_fan_speed_percent"));
}

// Test configuration options
TEST_F(GpuCollectorTest, ConfigurationOptions) {
    auto custom_collector = std::make_unique<gpu_collector>();

    std::unordered_map<std::string, std::string> config = {
        {"enabled", "true"},
        {"collect_utilization", "true"},
        {"collect_memory", "true"},
        {"collect_temperature", "true"},
        {"collect_power", "true"},
        {"collect_clock", "true"},
        {"collect_fan", "true"}};

    EXPECT_TRUE(custom_collector->initialize(config));
    EXPECT_TRUE(custom_collector->is_healthy());
}

// Test disable collector
TEST_F(GpuCollectorTest, CanBeDisabled) {
    auto custom_collector = std::make_unique<gpu_collector>();

    std::unordered_map<std::string, std::string> config = {{"enabled", "false"}};

    custom_collector->initialize(config);

    // When disabled, collect should return empty
    auto metrics = custom_collector->collect();
    EXPECT_TRUE(metrics.empty());
}

// Test statistics
TEST_F(GpuCollectorTest, TracksStatistics) {
    auto stats = collector_->get_statistics();

    // Should have expected statistics keys
    EXPECT_TRUE(stats.find("collection_count") != stats.end());
    EXPECT_TRUE(stats.find("collection_errors") != stats.end());
    EXPECT_TRUE(stats.find("gpus_found") != stats.end());

    // Initial values should be 0
    EXPECT_EQ(stats["collection_count"], 0.0);
    EXPECT_EQ(stats["collection_errors"], 0.0);
}

// Test collect returns metrics (graceful degradation when GPUs unavailable)
TEST_F(GpuCollectorTest, CollectReturnsMetrics) {
    auto metrics = collector_->collect();

    // This may return empty metrics if GPUs are not available
    // This is expected behavior - graceful degradation
    auto stats = collector_->get_statistics();
    EXPECT_GE(stats["collection_count"], 1.0);
}

// Test get_last_readings
TEST_F(GpuCollectorTest, GetLastReadings) {
    collector_->collect();
    auto last_readings = collector_->get_last_readings();

    // Should return vector (may be empty if GPU not available)
    // No assertion - just verify it doesn't crash
    (void)last_readings;
}

// Test is_gpu_available
TEST_F(GpuCollectorTest, GpuAvailabilityCheck) {
    // Should not crash - returns true/false based on GPU availability
    bool available = collector_->is_gpu_available();
    (void)available;  // Use the variable to avoid warning
}

// Test gpu_reading structure
TEST(GpuReadingTest, DefaultInitialization) {
    gpu_reading reading;

    EXPECT_TRUE(reading.device.id.empty());
    EXPECT_TRUE(reading.device.name.empty());
    EXPECT_EQ(reading.device.vendor, gpu_vendor::unknown);
    EXPECT_EQ(reading.device.type, gpu_type::unknown);
    EXPECT_EQ(reading.utilization_percent, 0.0);
    EXPECT_EQ(reading.memory_used_bytes, 0u);
    EXPECT_EQ(reading.memory_total_bytes, 0u);
    EXPECT_EQ(reading.temperature_celsius, 0.0);
    EXPECT_EQ(reading.power_watts, 0.0);
    EXPECT_EQ(reading.clock_mhz, 0.0);
    EXPECT_EQ(reading.fan_speed_percent, 0.0);
    EXPECT_FALSE(reading.utilization_available);
    EXPECT_FALSE(reading.memory_available);
    EXPECT_FALSE(reading.temperature_available);
    EXPECT_FALSE(reading.power_available);
    EXPECT_FALSE(reading.clock_available);
    EXPECT_FALSE(reading.fan_available);
}

// Test gpu_device_info structure
TEST(GpuDeviceInfoTest, DefaultInitialization) {
    gpu_device_info info;

    EXPECT_TRUE(info.id.empty());
    EXPECT_TRUE(info.name.empty());
    EXPECT_TRUE(info.device_path.empty());
    EXPECT_TRUE(info.driver_version.empty());
    EXPECT_EQ(info.vendor, gpu_vendor::unknown);
    EXPECT_EQ(info.type, gpu_type::unknown);
    EXPECT_EQ(info.device_index, 0u);
}

// Test gpu_vendor_to_string conversion
TEST(GpuVendorTest, VendorToStringConversion) {
    EXPECT_EQ(gpu_vendor_to_string(gpu_vendor::nvidia), "nvidia");
    EXPECT_EQ(gpu_vendor_to_string(gpu_vendor::amd), "amd");
    EXPECT_EQ(gpu_vendor_to_string(gpu_vendor::intel), "intel");
    EXPECT_EQ(gpu_vendor_to_string(gpu_vendor::apple), "apple");
    EXPECT_EQ(gpu_vendor_to_string(gpu_vendor::other), "other");
    EXPECT_EQ(gpu_vendor_to_string(gpu_vendor::unknown), "unknown");
}

// Test gpu_type_to_string conversion
TEST(GpuTypeTest, TypeToStringConversion) {
    EXPECT_EQ(gpu_type_to_string(gpu_type::discrete), "discrete");
    EXPECT_EQ(gpu_type_to_string(gpu_type::integrated), "integrated");
    EXPECT_EQ(gpu_type_to_string(gpu_type::virtual_gpu), "virtual");
    EXPECT_EQ(gpu_type_to_string(gpu_type::unknown), "unknown");
}

// Test gpu_info_collector basic functionality
TEST(GpuInfoCollectorTest, BasicFunctionality) {
    gpu_info_collector collector;

    // Test GPU availability check (should not crash)
    bool available = collector.is_gpu_available();

    // If GPU is not available, enumerate_gpus should still work
    if (!available) {
        auto gpus = collector.enumerate_gpus();
        // Empty GPUs without GPU is acceptable
        (void)gpus;
    } else {
        // If GPU is available, we might get some devices
        auto gpus = collector.enumerate_gpus();
        // Just verify it doesn't crash - actual device count depends on system
        (void)gpus;
    }
}

// Test GPU enumeration (graceful degradation)
TEST(GpuInfoCollectorTest, EnumerateGpus) {
    gpu_info_collector collector;

    auto gpus = collector.enumerate_gpus();

    // Should return a vector (may be empty if GPU not available)
    // No assertion on size - just verify it doesn't crash
    (void)gpus;
}

// Test multiple collections don't cause issues
TEST_F(GpuCollectorTest, MultipleCollectionsAreStable) {
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
TEST_F(GpuCollectorTest, MetricsHaveCorrectTags) {
    auto metrics = collector_->collect();

    for (const auto& m : metrics) {
        // If we have any metrics, check they have the expected tags
        if (!m.name.empty()) {
            EXPECT_TRUE(m.tags.find("gpu_id") != m.tags.end());
            EXPECT_TRUE(m.tags.find("gpu_name") != m.tags.end());
            EXPECT_TRUE(m.tags.find("gpu_vendor") != m.tags.end());
            EXPECT_TRUE(m.tags.find("gpu_type") != m.tags.end());
            EXPECT_TRUE(m.tags.find("gpu_index") != m.tags.end());
        }
    }
}

// Test read_all_gpu_metrics
TEST(GpuInfoCollectorTest, ReadAllGpuMetrics) {
    gpu_info_collector collector;

    auto readings = collector.read_all_gpu_metrics();

    // Should return a vector (may be empty if GPU not available)
    // No assertion on size - just verify it doesn't crash
    for (const auto& reading : readings) {
        // If we got readings, they should have valid device info
        EXPECT_FALSE(reading.device.id.empty());
    }
}

// Test selective metric collection
TEST_F(GpuCollectorTest, SelectiveMetricCollection) {
    auto custom_collector = std::make_unique<gpu_collector>();

    // Only collect temperature
    std::unordered_map<std::string, std::string> config = {
        {"enabled", "true"},
        {"collect_utilization", "false"},
        {"collect_memory", "false"},
        {"collect_temperature", "true"},
        {"collect_power", "false"},
        {"collect_clock", "false"},
        {"collect_fan", "false"}};

    EXPECT_TRUE(custom_collector->initialize(config));
    
    // Collection should work
    auto metrics = custom_collector->collect();
    (void)metrics;  // Just verify it runs
}

// Test that collector handles reinitialize gracefully
TEST_F(GpuCollectorTest, ReinitializeHandledGracefully) {
    // First initialization
    std::unordered_map<std::string, std::string> config1 = {{"enabled", "true"}};
    EXPECT_TRUE(collector_->initialize(config1));
    collector_->collect();

    // Second initialization with different config
    std::unordered_map<std::string, std::string> config2 = {{"enabled", "false"}};
    EXPECT_TRUE(collector_->initialize(config2));

    // Should now return empty
    auto metrics = collector_->collect();
    EXPECT_TRUE(metrics.empty());
}

}  // namespace
}  // namespace monitoring
}  // namespace kcenon
