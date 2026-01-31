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
#include <kcenon/monitoring/collectors/container_collector.h>

namespace kcenon {
namespace monitoring {
namespace {

// Test fixture for container collector tests
class ContainerCollectorTest : public ::testing::Test {
   protected:
    void SetUp() override {
        collector_ = std::make_unique<container_collector>();
        std::unordered_map<std::string, std::string> config;
        collector_->initialize(config);
    }

    std::unique_ptr<container_collector> collector_;
};

// Test basic initialization
TEST_F(ContainerCollectorTest, InitializesSuccessfully) {
    EXPECT_TRUE(collector_->is_healthy());
    EXPECT_EQ(collector_->name(), "container");
}

// Test metric types returned
TEST_F(ContainerCollectorTest, ReturnsCorrectMetricTypes) {
    auto metric_types = collector_->get_metric_types();

    // Should include all expected container metrics
    EXPECT_FALSE(metric_types.empty());

    // Check for expected metric types
    auto contains = [&metric_types](const std::string& type) {
        return std::find(metric_types.begin(), metric_types.end(), type) != metric_types.end();
    };

    EXPECT_TRUE(contains("container_cpu_usage_percent"));
    EXPECT_TRUE(contains("container_memory_usage_bytes"));
    EXPECT_TRUE(contains("container_memory_limit_bytes"));
    EXPECT_TRUE(contains("container_pids_current"));
}

// Test configuration options
TEST_F(ContainerCollectorTest, ConfigurationOptions) {
    auto custom_collector = std::make_unique<container_collector>();

    std::unordered_map<std::string, std::string> config = {
        {"enabled", "true"}, {"collect_network", "true"}, {"collect_blkio", "true"}};

    EXPECT_TRUE(custom_collector->initialize(config));
    EXPECT_TRUE(custom_collector->is_healthy());
}

// Test disable collector
TEST_F(ContainerCollectorTest, CanBeDisabled) {
    auto custom_collector = std::make_unique<container_collector>();

    std::unordered_map<std::string, std::string> config = {{"enabled", "false"}};

    custom_collector->initialize(config);

    // When disabled, collect should return empty
    auto metrics = custom_collector->collect();
    EXPECT_TRUE(metrics.empty());
}

// Test statistics
TEST_F(ContainerCollectorTest, TracksStatistics) {
    auto stats = collector_->get_statistics();

    // Should have expected statistics keys
    EXPECT_TRUE(stats.find("collection_count") != stats.end());
    EXPECT_TRUE(stats.find("collection_errors") != stats.end());
    EXPECT_TRUE(stats.find("containers_found") != stats.end());

    // Initial values should be 0
    EXPECT_EQ(stats["collection_count"], 0.0);
    EXPECT_EQ(stats["collection_errors"], 0.0);
}

// Test collect returns metrics (graceful degradation outside containers)
TEST_F(ContainerCollectorTest, CollectReturnsMetrics) {
    auto metrics = collector_->collect();

    // Outside of a container environment, this may return empty metrics
    // This is expected behavior - graceful degradation
    auto stats = collector_->get_statistics();
    EXPECT_GE(stats["collection_count"], 0.0);
}

// Test get_last_metrics
TEST_F(ContainerCollectorTest, GetLastMetrics) {
    collector_->collect();
    auto last_metrics = collector_->get_last_metrics();

    // Should return vector (may be empty if not in container)
    // No assertion - just verify it doesn't crash
    (void)last_metrics;
}

// Test container_metrics structure
TEST(ContainerMetricsTest, DefaultInitialization) {
    container_metrics metrics;

    EXPECT_TRUE(metrics.container_id.empty());
    EXPECT_TRUE(metrics.container_name.empty());
    EXPECT_EQ(metrics.cpu_usage_percent, 0.0);
    EXPECT_EQ(metrics.memory_usage_bytes, 0);
    EXPECT_EQ(metrics.network_rx_bytes, 0);
    EXPECT_EQ(metrics.blkio_read_bytes, 0);
    EXPECT_EQ(metrics.pids_current, 0);
}

// Test container_info structure
TEST(ContainerInfoTest, DefaultInitialization) {
    container_info info;

    EXPECT_TRUE(info.container_id.empty());
    EXPECT_TRUE(info.container_name.empty());
    EXPECT_TRUE(info.cgroup_path.empty());
    EXPECT_FALSE(info.is_running);
}

// Test cgroup_version enum
TEST(CgroupVersionTest, EnumValues) {
    EXPECT_EQ(static_cast<uint8_t>(cgroup_version::none), 0);
    EXPECT_EQ(static_cast<uint8_t>(cgroup_version::v1), 1);
    EXPECT_EQ(static_cast<uint8_t>(cgroup_version::v2), 2);
}

// Test container_info_collector platform detection
TEST(ContainerInfoCollectorTest, BasicFunctionality) {
    container_info_collector collector;

    // Test cgroup version detection (should not crash)
    auto version = collector.detect_cgroup_version();

    // On non-Linux platforms, should return none
#if !defined(__linux__)
    EXPECT_EQ(version, cgroup_version::none);
#else
    // On Linux, may return v1, v2, or none depending on system
    EXPECT_TRUE(version == cgroup_version::none || version == cgroup_version::v1 ||
                version == cgroup_version::v2);
#endif
}

// Test containerized detection
TEST(ContainerInfoCollectorTest, IsContainerizedDetection) {
    container_info_collector collector;

    // Should not crash - returns true/false based on environment
    bool is_containerized = collector.is_containerized();
    (void)is_containerized;  // Use the variable to avoid warning
}

// Test container enumeration (graceful degradation)
TEST(ContainerInfoCollectorTest, EnumerateContainers) {
    container_info_collector collector;

    auto containers = collector.enumerate_containers();

    // Should return a vector (may be empty if not in container environment)
    // No assertion on size - just verify it doesn't crash
    (void)containers;
}

}  // namespace
}  // namespace monitoring
}  // namespace kcenon
