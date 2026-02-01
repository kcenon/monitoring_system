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
#include <kcenon/monitoring/collectors/smart_collector.h>

namespace kcenon {
namespace monitoring {
namespace {

// Test fixture for SMART collector tests
class SmartCollectorTest : public ::testing::Test {
   protected:
    void SetUp() override {
        collector_ = std::make_unique<smart_collector>();
        std::unordered_map<std::string, std::string> config;
        collector_->initialize(config);
    }

    std::unique_ptr<smart_collector> collector_;
};

// Test basic initialization
TEST_F(SmartCollectorTest, InitializesSuccessfully) {
    EXPECT_TRUE(collector_->is_healthy());
    EXPECT_EQ(collector_->name(), "smart_collector");
}

// Test metric types returned
TEST_F(SmartCollectorTest, ReturnsCorrectMetricTypes) {
    auto metric_types = collector_->get_metric_types();

    // Should include all expected SMART metrics
    EXPECT_FALSE(metric_types.empty());

    // Check for expected metric types
    auto contains = [&metric_types](const std::string& type) {
        return std::find(metric_types.begin(), metric_types.end(), type) != metric_types.end();
    };

    EXPECT_TRUE(contains("smart_health_ok"));
    EXPECT_TRUE(contains("smart_temperature_celsius"));
    EXPECT_TRUE(contains("smart_reallocated_sectors"));
    EXPECT_TRUE(contains("smart_power_on_hours"));
    EXPECT_TRUE(contains("smart_power_cycle_count"));
    EXPECT_TRUE(contains("smart_pending_sectors"));
    EXPECT_TRUE(contains("smart_uncorrectable_errors"));
}

// Test configuration options
TEST_F(SmartCollectorTest, ConfigurationOptions) {
    auto custom_collector = std::make_unique<smart_collector>();

    std::unordered_map<std::string, std::string> config = {
        {"enabled", "true"}, {"collect_temperature", "true"}, {"collect_error_rates", "true"}};

    EXPECT_TRUE(custom_collector->initialize(config));
    EXPECT_TRUE(custom_collector->is_healthy());
}

// Test disable collector
TEST_F(SmartCollectorTest, CanBeDisabled) {
    auto custom_collector = std::make_unique<smart_collector>();

    std::unordered_map<std::string, std::string> config = {{"enabled", "false"}};

    custom_collector->initialize(config);

    // When disabled, collect should return empty
    auto metrics = custom_collector->collect();
    EXPECT_TRUE(metrics.empty());
}

// Test statistics
TEST_F(SmartCollectorTest, TracksStatistics) {
    auto stats = collector_->get_statistics();

    // Should have expected statistics keys
    EXPECT_TRUE(stats.find("collection_count") != stats.end());
    EXPECT_TRUE(stats.find("collection_errors") != stats.end());
    EXPECT_TRUE(stats.find("disks_found") != stats.end());

    // Initial values should be 0
    EXPECT_EQ(stats["collection_count"], 0.0);
    EXPECT_EQ(stats["collection_errors"], 0.0);
}

// Test collect returns metrics (graceful degradation without smartctl)
TEST_F(SmartCollectorTest, CollectReturnsMetrics) {
    auto metrics = collector_->collect();

    // This may return empty metrics if smartctl is not available
    // This is expected behavior - graceful degradation
    auto stats = collector_->get_statistics();
    EXPECT_GE(stats["collection_count"], 0.0);
}

// Test get_last_metrics
TEST_F(SmartCollectorTest, GetLastMetrics) {
    collector_->collect();
    auto last_metrics = collector_->get_last_metrics();

    // Should return vector (may be empty if smartctl not available)
    // No assertion - just verify it doesn't crash
    (void)last_metrics;
}

// Test is_smart_available
TEST_F(SmartCollectorTest, SmartAvailabilityCheck) {
    // Should not crash - returns true/false based on smartctl availability
    bool available = collector_->is_smart_available();
    (void)available;  // Use the variable to avoid warning
}

// Test smart_disk_metrics structure
TEST(SmartDiskMetricsTest, DefaultInitialization) {
    smart_disk_metrics metrics;

    EXPECT_TRUE(metrics.device_path.empty());
    EXPECT_TRUE(metrics.model_name.empty());
    EXPECT_TRUE(metrics.serial_number.empty());
    EXPECT_FALSE(metrics.smart_supported);
    EXPECT_FALSE(metrics.smart_enabled);
    EXPECT_TRUE(metrics.health_ok);  // Default to healthy
    EXPECT_EQ(metrics.temperature_celsius, 0.0);
    EXPECT_EQ(metrics.reallocated_sectors, 0);
    EXPECT_EQ(metrics.power_on_hours, 0);
    EXPECT_EQ(metrics.power_cycle_count, 0);
    EXPECT_EQ(metrics.pending_sectors, 0);
    EXPECT_EQ(metrics.uncorrectable_errors, 0);
}

// Test disk_info structure
TEST(DiskInfoTest, DefaultInitialization) {
    disk_info info;

    EXPECT_TRUE(info.device_path.empty());
    EXPECT_TRUE(info.device_type.empty());
    EXPECT_FALSE(info.smart_available);
}

// Test smart_info_collector platform detection
TEST(SmartInfoCollectorTest, BasicFunctionality) {
    smart_info_collector collector;

    // Test smartctl availability check (should not crash)
    bool available = collector.is_smartctl_available();

    // If smartctl is not available, enumerate_disks should still work
    if (!available) {
        auto disks = collector.enumerate_disks();
        EXPECT_TRUE(disks.empty());  // No disks without smartctl
    } else {
        // If smartctl is available, we might get some disks
        auto disks = collector.enumerate_disks();
        // Just verify it doesn't crash - actual disk count depends on system
        (void)disks;
    }
}

// Test disk enumeration (graceful degradation)
TEST(SmartInfoCollectorTest, EnumerateDisks) {
    smart_info_collector collector;

    auto disks = collector.enumerate_disks();

    // Should return a vector (may be empty if smartctl not available)
    // No assertion on size - just verify it doesn't crash
    (void)disks;
}

// Test metric collection for non-existent disk
TEST(SmartInfoCollectorTest, CollectMetricsNonExistentDisk) {
    smart_info_collector collector;

    disk_info fake_disk;
    fake_disk.device_path = "/dev/nonexistent_disk_xyz";
    fake_disk.device_type = "auto";
    fake_disk.smart_available = false;

    // Should not crash, just return empty/default metrics
    auto metrics = collector.collect_smart_metrics(fake_disk);

    EXPECT_EQ(metrics.device_path, "/dev/nonexistent_disk_xyz");
    // SMART should not be "supported" for a non-existent disk
    EXPECT_FALSE(metrics.smart_supported);
}

// Test multiple collections don't cause issues
TEST_F(SmartCollectorTest, MultipleCollectionsAreStable) {
    for (int i = 0; i < 5; ++i) {
        auto metrics = collector_->collect();
        // Should not crash on repeated calls
        (void)metrics;
    }

    auto stats = collector_->get_statistics();
    EXPECT_GE(stats["collection_count"], 5.0);
    EXPECT_EQ(stats["collection_errors"], 0.0);
}

}  // namespace
}  // namespace monitoring
}  // namespace kcenon
