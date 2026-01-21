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

#include <kcenon/monitoring/plugins/container/container_plugin.h>

namespace kcenon {
namespace monitoring {
namespace plugins {
namespace {

// Test fixture for container plugin tests
class ContainerPluginTest : public ::testing::Test {
   protected:
    void SetUp() override {
        plugin_ = container_plugin::create();
        ASSERT_NE(plugin_, nullptr);
    }

    std::unique_ptr<container_plugin> plugin_;
};

// Test basic creation
TEST_F(ContainerPluginTest, CreatesSuccessfully) {
    EXPECT_NE(plugin_, nullptr);
    EXPECT_EQ(plugin_->get_name(), "container_plugin");
}

// Test creation with custom configuration
TEST_F(ContainerPluginTest, CreatesWithCustomConfig) {
    container_plugin_config config;
    config.enable_docker = true;
    config.enable_kubernetes = false;
    config.enable_cgroup = true;
    config.docker_socket = "/var/run/docker.sock";

    auto custom_plugin = container_plugin::create(config);
    ASSERT_NE(custom_plugin, nullptr);

    auto retrieved_config = custom_plugin->get_config();
    EXPECT_EQ(retrieved_config.enable_docker, true);
    EXPECT_EQ(retrieved_config.enable_kubernetes, false);
    EXPECT_EQ(retrieved_config.enable_cgroup, true);
    EXPECT_EQ(retrieved_config.docker_socket, "/var/run/docker.sock");
}

// Test initialization from map
TEST_F(ContainerPluginTest, InitializesFromConfigMap) {
    std::unordered_map<std::string, std::string> config = {{"enable_docker", "true"},
                                                           {"enable_kubernetes", "false"},
                                                           {"collect_network", "true"},
                                                           {"collect_blkio", "false"}};

    EXPECT_TRUE(plugin_->initialize(config));
}

// Test metric types returned
TEST_F(ContainerPluginTest, ReturnsMetricTypes) {
    auto metric_types = plugin_->get_metric_types();

    // Should return container metric types
    EXPECT_FALSE(metric_types.empty());

    auto contains = [&metric_types](const std::string& type) {
        return std::find(metric_types.begin(), metric_types.end(), type) != metric_types.end();
    };

    EXPECT_TRUE(contains("container_cpu_usage_percent"));
    EXPECT_TRUE(contains("container_memory_usage_bytes"));
}

// Test health check
TEST_F(ContainerPluginTest, HealthCheck) {
    // Plugin should be healthy after creation
    EXPECT_TRUE(plugin_->is_healthy());
}

// Test statistics tracking
TEST_F(ContainerPluginTest, TracksStatistics) {
    auto stats = plugin_->get_statistics();

    // Should have expected statistics keys
    EXPECT_TRUE(stats.find("total_collections") != stats.end());
    EXPECT_TRUE(stats.find("collection_errors") != stats.end());
    EXPECT_TRUE(stats.find("containers_found") != stats.end());
}

// Test collect metrics (graceful degradation outside containers)
TEST_F(ContainerPluginTest, CollectMetrics) {
    auto metrics = plugin_->collect();

    // May return empty outside container environment
    // Just verify it doesn't crash
    auto stats = plugin_->get_statistics();
    EXPECT_GE(stats["total_collections"], 0.0);
}

// Test container_runtime enum values
TEST(ContainerRuntimeTest, EnumValues) {
    EXPECT_EQ(static_cast<int>(container_runtime::auto_detect), 0);
    EXPECT_EQ(static_cast<int>(container_runtime::docker), 1);
    EXPECT_EQ(static_cast<int>(container_runtime::containerd), 2);
    EXPECT_EQ(static_cast<int>(container_runtime::podman), 3);
    EXPECT_EQ(static_cast<int>(container_runtime::cri_o), 4);
}

// Test static detection methods
TEST(ContainerPluginStaticTest, IsRunningInContainerDetection) {
    // Should not crash - returns true/false based on environment
    bool in_container = container_plugin::is_running_in_container();
    (void)in_container;  // Use variable to avoid warning
}

TEST(ContainerPluginStaticTest, IsKubernetesEnvironmentDetection) {
    // Should not crash - returns true/false based on environment
    bool in_k8s = container_plugin::is_kubernetes_environment();
    (void)in_k8s;  // Use variable to avoid warning
}

TEST(ContainerPluginStaticTest, DetectRuntimeDetection) {
    // Should not crash - returns runtime type based on environment
    auto runtime = container_plugin::detect_runtime();
    (void)runtime;  // Use variable to avoid warning
}

// Test availability checks
TEST_F(ContainerPluginTest, AvailabilityChecks) {
    // These may return true or false depending on environment
    // Just verify they don't crash
    bool docker_available = plugin_->is_docker_available();
    bool k8s_available = plugin_->is_kubernetes_available();
    bool cgroup_available = plugin_->is_cgroup_available();

    (void)docker_available;
    (void)k8s_available;
    (void)cgroup_available;
}

// Test container_plugin_config default values
TEST(ContainerPluginConfigTest, DefaultValues) {
    container_plugin_config config;

    EXPECT_EQ(config.runtime, container_runtime::auto_detect);
    EXPECT_TRUE(config.enable_docker);
    EXPECT_FALSE(config.enable_kubernetes);
    EXPECT_TRUE(config.enable_cgroup);
    EXPECT_EQ(config.docker_socket, "/var/run/docker.sock");
    EXPECT_TRUE(config.kubeconfig_path.empty());
    EXPECT_TRUE(config.namespace_filter.empty());
    EXPECT_TRUE(config.collect_network_metrics);
    EXPECT_TRUE(config.collect_blkio_metrics);
    EXPECT_TRUE(config.collect_pid_metrics);
}

}  // namespace
}  // namespace plugins
}  // namespace monitoring
}  // namespace kcenon
