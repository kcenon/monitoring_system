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

#include <kcenon/monitoring/plugins/collector_registry.h>

namespace kcenon {
namespace monitoring {
namespace {

// Mock collector plugin for testing
class mock_collector_plugin : public collector_plugin {
public:
    explicit mock_collector_plugin(std::string_view name,
                                   plugin_category category = plugin_category::custom,
                                   bool available = true)
        : name_(name), category_(category), available_(available) {}

    auto name() const -> std::string_view override { return name_; }

    auto collect() -> std::vector<metric> override {
        ++collect_count_;
        return {};
    }

    auto interval() const -> std::chrono::milliseconds override {
        return std::chrono::seconds(1);
    }

    auto is_available() const -> bool override { return available_; }

    auto get_metadata() const -> plugin_metadata override {
        return plugin_metadata{
            .name = name_,
            .description = "Mock plugin for testing",
            .category = category_,
            .version = "1.0.0",
            .dependencies = {},
            .requires_platform_support = false
        };
    }

    auto initialize(const config_map& /* config */) -> bool override {
        initialized_ = true;
        return true;
    }

    void shutdown() override { shutdown_called_ = true; }

    auto get_metric_types() const -> std::vector<std::string> override {
        return {"test_metric"};
    }

    // Test helpers
    bool is_initialized() const { return initialized_; }
    bool is_shutdown_called() const { return shutdown_called_; }
    size_t get_collect_count() const { return collect_count_; }

private:
    std::string name_;
    plugin_category category_;
    bool available_;
    bool initialized_{false};
    bool shutdown_called_{false};
    mutable size_t collect_count_{0};
};

// Test fixture
class CollectorRegistryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear registry before each test
        collector_registry::instance().clear();
    }

    void TearDown() override {
        // Clean up after each test
        collector_registry::instance().clear();
    }
};

// Test singleton instance
TEST_F(CollectorRegistryTest, SingletonInstance) {
    auto& registry1 = collector_registry::instance();
    auto& registry2 = collector_registry::instance();

    EXPECT_EQ(&registry1, &registry2);
}

// Test plugin registration
TEST_F(CollectorRegistryTest, RegisterPlugin) {
    auto& registry = collector_registry::instance();

    auto plugin = std::make_unique<mock_collector_plugin>("test_plugin");
    EXPECT_TRUE(registry.register_plugin(std::move(plugin)));

    EXPECT_TRUE(registry.has_plugin("test_plugin"));
    EXPECT_EQ(registry.plugin_count(), 1);
}

// Test duplicate registration fails
TEST_F(CollectorRegistryTest, RejectsDuplicateRegistration) {
    auto& registry = collector_registry::instance();

    auto plugin1 = std::make_unique<mock_collector_plugin>("test_plugin");
    EXPECT_TRUE(registry.register_plugin(std::move(plugin1)));

    auto plugin2 = std::make_unique<mock_collector_plugin>("test_plugin");
    EXPECT_FALSE(registry.register_plugin(std::move(plugin2)));

    EXPECT_EQ(registry.plugin_count(), 1);
}

// Test unavailable plugins are rejected
TEST_F(CollectorRegistryTest, RejectsUnavailablePlugin) {
    auto& registry = collector_registry::instance();

    auto plugin = std::make_unique<mock_collector_plugin>("unavailable", plugin_category::custom, false);
    EXPECT_FALSE(registry.register_plugin(std::move(plugin)));

    EXPECT_FALSE(registry.has_plugin("unavailable"));
    EXPECT_EQ(registry.plugin_count(), 0);
}

// Test plugin retrieval
TEST_F(CollectorRegistryTest, GetPlugin) {
    auto& registry = collector_registry::instance();

    auto plugin = std::make_unique<mock_collector_plugin>("test_plugin");
    auto* plugin_ptr = plugin.get();
    registry.register_plugin(std::move(plugin));

    auto* retrieved = registry.get_plugin("test_plugin");
    EXPECT_EQ(retrieved, plugin_ptr);

    auto* not_found = registry.get_plugin("nonexistent");
    EXPECT_EQ(not_found, nullptr);
}

// Test get all plugins
TEST_F(CollectorRegistryTest, GetAllPlugins) {
    auto& registry = collector_registry::instance();

    registry.register_plugin(std::make_unique<mock_collector_plugin>("plugin1"));
    registry.register_plugin(std::make_unique<mock_collector_plugin>("plugin2"));
    registry.register_plugin(std::make_unique<mock_collector_plugin>("plugin3"));

    auto plugins = registry.get_plugins();
    EXPECT_EQ(plugins.size(), 3);
}

// Test get plugins by category
TEST_F(CollectorRegistryTest, GetPluginsByCategory) {
    auto& registry = collector_registry::instance();

    registry.register_plugin(std::make_unique<mock_collector_plugin>("hw1", plugin_category::hardware));
    registry.register_plugin(std::make_unique<mock_collector_plugin>("hw2", plugin_category::hardware));
    registry.register_plugin(std::make_unique<mock_collector_plugin>("sys1", plugin_category::system));

    auto hw_plugins = registry.get_plugins_by_category(plugin_category::hardware);
    EXPECT_EQ(hw_plugins.size(), 2);

    auto sys_plugins = registry.get_plugins_by_category(plugin_category::system);
    EXPECT_EQ(sys_plugins.size(), 1);

    auto net_plugins = registry.get_plugins_by_category(plugin_category::network);
    EXPECT_EQ(net_plugins.size(), 0);
}

// Test plugin unregistration
TEST_F(CollectorRegistryTest, UnregisterPlugin) {
    auto& registry = collector_registry::instance();

    auto plugin = std::make_unique<mock_collector_plugin>("test_plugin");
    registry.register_plugin(std::move(plugin));

    EXPECT_TRUE(registry.has_plugin("test_plugin"));
    EXPECT_TRUE(registry.unregister_plugin("test_plugin"));
    EXPECT_FALSE(registry.has_plugin("test_plugin"));
}

// Test unregistering nonexistent plugin
TEST_F(CollectorRegistryTest, UnregisterNonexistentPlugin) {
    auto& registry = collector_registry::instance();

    EXPECT_FALSE(registry.unregister_plugin("nonexistent"));
}

// Test initialize all plugins
TEST_F(CollectorRegistryTest, InitializeAllPlugins) {
    auto& registry = collector_registry::instance();

    auto plugin1 = std::make_unique<mock_collector_plugin>("plugin1");
    auto plugin2 = std::make_unique<mock_collector_plugin>("plugin2");
    auto* ptr1 = plugin1.get();
    auto* ptr2 = plugin2.get();

    registry.register_plugin(std::move(plugin1));
    registry.register_plugin(std::move(plugin2));

    auto initialized_count = registry.initialize_all();
    EXPECT_EQ(initialized_count, 2);

    EXPECT_TRUE(ptr1->is_initialized());
    EXPECT_TRUE(ptr2->is_initialized());
}

// Test shutdown all plugins
TEST_F(CollectorRegistryTest, ShutdownAllPlugins) {
    auto& registry = collector_registry::instance();

    auto plugin1 = std::make_unique<mock_collector_plugin>("plugin1");
    auto plugin2 = std::make_unique<mock_collector_plugin>("plugin2");
    auto* ptr1 = plugin1.get();
    auto* ptr2 = plugin2.get();

    registry.register_plugin(std::move(plugin1));
    registry.register_plugin(std::move(plugin2));
    registry.initialize_all();

    registry.shutdown_all();

    EXPECT_TRUE(ptr1->is_shutdown_called());
    EXPECT_TRUE(ptr2->is_shutdown_called());
}

// Test shutdown is called on unregistration
TEST_F(CollectorRegistryTest, ShutdownOnUnregister) {
    auto& registry = collector_registry::instance();

    auto plugin = std::make_unique<mock_collector_plugin>("test_plugin");
    auto* ptr = plugin.get();

    registry.register_plugin(std::move(plugin));
    registry.initialize_all();

    EXPECT_TRUE(ptr->is_initialized());
    EXPECT_FALSE(ptr->is_shutdown_called());

    registry.unregister_plugin("test_plugin");

    EXPECT_TRUE(ptr->is_shutdown_called());
}

// Test registry statistics
TEST_F(CollectorRegistryTest, GetRegistryStats) {
    auto& registry = collector_registry::instance();

    registry.register_plugin(std::make_unique<mock_collector_plugin>("hw1", plugin_category::hardware));
    registry.register_plugin(std::make_unique<mock_collector_plugin>("hw2", plugin_category::hardware));
    registry.register_plugin(std::make_unique<mock_collector_plugin>("sys1", plugin_category::system));

    auto stats = registry.get_registry_stats();

    EXPECT_EQ(stats["total_plugins"], 3);
    EXPECT_EQ(stats["available_plugins"], 3);
    EXPECT_EQ(stats["category_hardware_count"], 2);
    EXPECT_EQ(stats["category_system_count"], 1);
}

// Test factory registration
TEST_F(CollectorRegistryTest, RegisterFactory) {
    auto& registry = collector_registry::instance();

    registry.register_factory<mock_collector_plugin>("lazy_plugin");

    EXPECT_TRUE(registry.has_plugin("lazy_plugin"));
    EXPECT_EQ(registry.plugin_count(), 1);

    // Factory should not have instantiated yet
    auto stats = registry.get_registry_stats();
    EXPECT_EQ(stats["total_plugins"], 0);
}

// Test factory instantiation on get
TEST_F(CollectorRegistryTest, FactoryInstantiatesOnGet) {
    auto& registry = collector_registry::instance();

    registry.register_factory<mock_collector_plugin>("lazy_plugin");

    auto* plugin = registry.get_plugin("lazy_plugin");
    EXPECT_NE(plugin, nullptr);

    // Now it should be instantiated
    auto stats = registry.get_registry_stats();
    EXPECT_EQ(stats["total_plugins"], 1);
}

// Test clear removes all plugins
TEST_F(CollectorRegistryTest, ClearRemovesAllPlugins) {
    auto& registry = collector_registry::instance();

    registry.register_plugin(std::make_unique<mock_collector_plugin>("plugin1"));
    registry.register_plugin(std::make_unique<mock_collector_plugin>("plugin2"));

    EXPECT_EQ(registry.plugin_count(), 2);

    registry.clear();

    EXPECT_EQ(registry.plugin_count(), 0);
}

} // namespace
} // namespace monitoring
} // namespace kcenon
