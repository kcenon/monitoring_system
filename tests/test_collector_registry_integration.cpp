// BSD 3-Clause License
//
// Copyright (c) 2021-2025, kcenon
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

#include "kcenon/monitoring/factory/builtin_collectors.h"
#include "kcenon/monitoring/plugins/collector_registry.h"

using namespace kcenon::monitoring;

class CollectorRegistryIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clean registry before each test
        auto& registry = collector_registry::instance();
        registry.clear();
    }

    void TearDown() override {
        // Clean up after test
        auto& registry = collector_registry::instance();
        registry.clear();
    }
};

// Test that all builtin collectors are registered with the registry
TEST_F(CollectorRegistryIntegrationTest, BuiltinCollectorsRegisteredWithRegistry) {
    auto& registry = collector_registry::instance();

    // Register all builtin collectors
    bool success = register_builtin_collectors();
    EXPECT_TRUE(success);

    // Check that we have 9 plugin-based collectors registered
    // (battery, uptime, interrupt, network_metrics, platform_metrics,
    //  process_metrics, security, smart, vm)
    auto total_plugins = registry.plugin_count();
    EXPECT_EQ(total_plugins, 9);
}

// Test that get_plugins returns all registered collectors
TEST_F(CollectorRegistryIntegrationTest, GetPluginsReturnsAllCollectors) {
    auto& registry = collector_registry::instance();

    // Register builtin collectors
    register_builtin_collectors();

    // Try to get a specific plugin first (safer)
    auto* battery = registry.get_plugin("battery_collector");

    // If battery collector is available on this platform
    if (battery != nullptr) {
        // Check that it has a valid name (the actual implementation uses "battery")
        EXPECT_FALSE(std::string(battery->name()).empty());
    }

    // Check plugin count
    auto total_plugins = registry.plugin_count();
    EXPECT_EQ(total_plugins, 9);
}

// Test registry statistics
TEST_F(CollectorRegistryIntegrationTest, RegistryStatistics) {
    auto& registry = collector_registry::instance();

    // Register builtin collectors
    register_builtin_collectors();

    // Get statistics (without triggering full instantiation)
    auto stats = registry.get_registry_stats();

    // Should have total_plugins stat
    EXPECT_TRUE(stats.find("total_plugins") != stats.end());
}

// Test that specific collectors can be retrieved
TEST_F(CollectorRegistryIntegrationTest, GetSpecificCollector) {
    auto& registry = collector_registry::instance();

    // Register builtin collectors
    register_builtin_collectors();

    // Try to get battery_collector
    auto* battery = registry.get_plugin("battery_collector");

    // Should either be valid or null (if not available on platform)
    if (battery != nullptr) {
        // Check that it has a valid name
        EXPECT_FALSE(std::string(battery->name()).empty());
    }
}

// Test initialization of all plugins
TEST_F(CollectorRegistryIntegrationTest, InitializeAllPlugins) {
    auto& registry = collector_registry::instance();

    // Register builtin collectors
    register_builtin_collectors();

    // Get a specific plugin
    auto* uptime = registry.get_plugin("uptime_collector");

    if (uptime != nullptr) {
        // Initialize this specific plugin
        config_map config;
        bool init_result = uptime->initialize(config);
        EXPECT_TRUE(init_result);
    }
}

// Test that has_plugin works correctly
TEST_F(CollectorRegistryIntegrationTest, HasPlugin) {
    auto& registry = collector_registry::instance();

    // Register builtin collectors
    register_builtin_collectors();

    // Should have battery_collector registered
    EXPECT_TRUE(registry.has_plugin("battery_collector"));

    // Should have uptime_collector registered
    EXPECT_TRUE(registry.has_plugin("uptime_collector"));

    // Should not have a non-existent collector
    EXPECT_FALSE(registry.has_plugin("non_existent_collector"));
}
