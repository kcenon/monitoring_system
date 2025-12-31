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
#include <kcenon/monitoring/collectors/inode_collector.h>

namespace kcenon {
namespace monitoring {
namespace {

// Test fixture for inode_collector tests
class InodeCollectorTest : public ::testing::Test {
   protected:
    void SetUp() override {
        collector_ = std::make_unique<inode_collector>();
        std::unordered_map<std::string, std::string> config;
        collector_->initialize(config);
    }

    std::unique_ptr<inode_collector> collector_;
};

// Test basic initialization
TEST_F(InodeCollectorTest, InitializesSuccessfully) {
    EXPECT_NE(collector_, nullptr);
    EXPECT_EQ(collector_->get_name(), "inode_collector");
}

// Test metric types returned
TEST_F(InodeCollectorTest, ReturnsCorrectMetricTypes) {
    auto types = collector_->get_metric_types();
    EXPECT_FALSE(types.empty());

    // Check for expected metric types
    std::vector<std::string> expected = {
        "inodes_total",
        "inodes_used",
        "inodes_free",
        "inodes_usage_percent",
        "inodes_max_usage_percent",
        "inodes_average_usage_percent",
        "inodes_filesystem_count"
    };

    for (const auto& expected_type : expected) {
        EXPECT_NE(std::find(types.begin(), types.end(), expected_type), types.end())
            << "Missing metric type: " << expected_type;
    }
}

// Test configuration options
TEST_F(InodeCollectorTest, ConfigurationOptions) {
    auto collector = std::make_unique<inode_collector>();
    std::unordered_map<std::string, std::string> config;
    config["warning_threshold"] = "70.0";
    config["critical_threshold"] = "90.0";
    EXPECT_TRUE(collector->initialize(config));

    auto stats = collector->get_statistics();
    EXPECT_DOUBLE_EQ(stats["warning_threshold"], 70.0);
    EXPECT_DOUBLE_EQ(stats["critical_threshold"], 90.0);
}

// Test disable collector
TEST_F(InodeCollectorTest, CanBeDisabled) {
    auto collector = std::make_unique<inode_collector>();
    std::unordered_map<std::string, std::string> config;
    config["enabled"] = "false";
    collector->initialize(config);

    auto metrics = collector->collect();
    EXPECT_TRUE(metrics.empty());

    auto stats = collector->get_statistics();
    EXPECT_DOUBLE_EQ(stats["enabled"], 0.0);
}

// Test statistics
TEST_F(InodeCollectorTest, TracksStatistics) {
    // Collect some metrics
    collector_->collect();
    collector_->collect();

    auto stats = collector_->get_statistics();
    EXPECT_GE(stats["collection_count"], 2.0);
    EXPECT_GE(stats["collection_errors"], 0.0);
}

// Test collect returns metrics (graceful degradation when unavailable)
TEST_F(InodeCollectorTest, CollectReturnsMetrics) {
    // Should not throw even if platform-specific metrics fail
    EXPECT_NO_THROW(collector_->collect());
}

// Test get_last_metrics
TEST_F(InodeCollectorTest, GetLastMetrics) {
    collector_->collect();
    auto last = collector_->get_last_metrics();

    // Timestamp should be set
    auto now = std::chrono::system_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - last.timestamp);
    EXPECT_LT(diff.count(), 10);  // Within 10 seconds
}

// Test is_inode_monitoring_available
TEST_F(InodeCollectorTest, InodeMonitoringAvailabilityCheck) {
    // This should return true or false depending on platform
    // Either result is valid - we just want to ensure it doesn't crash
    EXPECT_NO_THROW(collector_->is_inode_monitoring_available());
}

// Test inode_metrics structure
TEST(InodeMetricsTest, DefaultInitialization) {
    inode_metrics metrics;
    EXPECT_TRUE(metrics.filesystems.empty());
    EXPECT_EQ(metrics.total_inodes, 0);
    EXPECT_EQ(metrics.total_inodes_used, 0);
    EXPECT_EQ(metrics.total_inodes_free, 0);
    EXPECT_DOUBLE_EQ(metrics.average_usage_percent, 0.0);
    EXPECT_DOUBLE_EQ(metrics.max_usage_percent, 0.0);
    EXPECT_TRUE(metrics.max_usage_mount_point.empty());
    EXPECT_FALSE(metrics.metrics_available);
}

// Test filesystem_inode_info structure
TEST(FilesystemInodeInfoTest, DefaultInitialization) {
    filesystem_inode_info info;
    EXPECT_TRUE(info.mount_point.empty());
    EXPECT_TRUE(info.filesystem_type.empty());
    EXPECT_TRUE(info.device.empty());
    EXPECT_EQ(info.inodes_total, 0);
    EXPECT_EQ(info.inodes_used, 0);
    EXPECT_EQ(info.inodes_free, 0);
    EXPECT_DOUBLE_EQ(info.inodes_usage_percent, 0.0);
}

// Test inode_info_collector basic functionality
TEST(InodeInfoCollectorTest, BasicFunctionality) {
    inode_info_collector collector;

    // Test availability check
    EXPECT_NO_THROW(collector.is_inode_monitoring_available());

    // Test metrics collection
    auto metrics = collector.collect_metrics();

    // Timestamp should be set
    auto now = std::chrono::system_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - metrics.timestamp);
    EXPECT_LT(diff.count(), 10);
}

// Test multiple collections are stable
TEST_F(InodeCollectorTest, MultipleCollectionsAreStable) {
    for (int i = 0; i < 10; ++i) {
        auto metrics = collector_->collect();
        // Should not crash or throw
        EXPECT_NO_THROW(collector_->get_statistics());
    }

    auto stats = collector_->get_statistics();
    EXPECT_GE(stats["collection_count"], 10.0);
}

// Test that metrics have correct tags when collected
TEST_F(InodeCollectorTest, MetricsHaveCorrectTags) {
    auto metrics = collector_->collect();

    for (const auto& m : metrics) {
        // All metrics should have the collector tag
        auto it = m.tags.find("collector");
        if (it != m.tags.end()) {
            EXPECT_EQ(it->second, "inode_collector");
        }
    }
}

// Test is_healthy reflects actual state
TEST_F(InodeCollectorTest, IsHealthyReflectsState) {
    // When enabled, health depends on availability
    EXPECT_NO_THROW(collector_->is_healthy());

    // When disabled, collector is considered healthy (no errors)
    auto disabled_collector = std::make_unique<inode_collector>();
    std::unordered_map<std::string, std::string> config;
    config["enabled"] = "false";
    disabled_collector->initialize(config);
    EXPECT_TRUE(disabled_collector->is_healthy());
}

#if defined(__linux__) || defined(__APPLE__)
// Platform-specific test: On Unix-like systems, inode monitoring should be available
TEST_F(InodeCollectorTest, UnixInodeMonitoringAvailable) {
    EXPECT_TRUE(collector_->is_inode_monitoring_available());
}

// Platform-specific test: Should have at least one filesystem on Unix
TEST(InodeInfoCollectorTest, HasFilesystemsOnUnix) {
    inode_info_collector collector;
    
    if (collector.is_inode_monitoring_available()) {
        auto metrics = collector.collect_metrics();
        EXPECT_TRUE(metrics.metrics_available);
        // Should have at least root filesystem
        EXPECT_FALSE(metrics.filesystems.empty());
    }
}

// Platform-specific test: Root filesystem should have valid inode info
TEST(InodeInfoCollectorTest, RootFilesystemHasValidInodes) {
    inode_info_collector collector;
    
    if (collector.is_inode_monitoring_available()) {
        auto metrics = collector.collect_metrics();
        
        // Find root filesystem
        for (const auto& fs : metrics.filesystems) {
            if (fs.mount_point == "/") {
                EXPECT_GT(fs.inodes_total, 0);
                EXPECT_GE(fs.inodes_free, 0);
                EXPECT_LE(fs.inodes_usage_percent, 100.0);
                EXPECT_GE(fs.inodes_usage_percent, 0.0);
                break;
            }
        }
    }
}
#endif

#if defined(_WIN32)
// Platform-specific test: On Windows, inode monitoring should not be available
TEST_F(InodeCollectorTest, WindowsInodeMonitoringUnavailable) {
    EXPECT_FALSE(collector_->is_inode_monitoring_available());
}

// Platform-specific test: Windows metrics should indicate unavailability
TEST(InodeInfoCollectorTest, WindowsReturnsUnavailableMetrics) {
    inode_info_collector collector;
    auto metrics = collector.collect_metrics();
    EXPECT_FALSE(metrics.metrics_available);
    EXPECT_TRUE(metrics.filesystems.empty());
}
#endif

}  // namespace
}  // namespace monitoring
}  // namespace kcenon
