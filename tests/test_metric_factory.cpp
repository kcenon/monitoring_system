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

#include <algorithm>

#include <kcenon/monitoring/factory/builtin_collectors.h>
#include <kcenon/monitoring/factory/metric_factory.h>
#include <kcenon/monitoring/utils/config_parser.h>

namespace kcenon::monitoring {
namespace {

// Test fixture for metric_factory tests
class MetricFactoryTest : public ::testing::Test {
   protected:
    void SetUp() override {
        // Clear and re-register collectors for each test
        metric_factory::instance().clear();
        register_builtin_collectors();
    }

    void TearDown() override { metric_factory::instance().clear(); }
};

// Test singleton instance
TEST_F(MetricFactoryTest, SingletonInstance) {
    auto& instance1 = metric_factory::instance();
    auto& instance2 = metric_factory::instance();
    EXPECT_EQ(&instance1, &instance2);
}

// Test builtin collectors registration
TEST_F(MetricFactoryTest, BuiltinCollectorsRegistered) {
    auto& factory = metric_factory::instance();

    auto names = get_builtin_collector_names();
    for (const auto& name : names) {
        EXPECT_TRUE(factory.is_registered(name)) << "Collector not registered: " << name;
    }
}

// Test create system_resource_collector
TEST_F(MetricFactoryTest, CreateSystemResourceCollector) {
    auto& factory = metric_factory::instance();

    auto result = factory.create("system_resource_collector", {});
    EXPECT_TRUE(result) << result.error_message;
    EXPECT_NE(result.collector, nullptr);
    EXPECT_EQ(result.collector->get_name(), "system_resource_collector");
}

// Test create vm_collector
TEST_F(MetricFactoryTest, CreateVmCollector) {
    auto& factory = metric_factory::instance();

    auto result = factory.create("vm_collector", {});
    EXPECT_TRUE(result) << result.error_message;
    EXPECT_NE(result.collector, nullptr);
    EXPECT_EQ(result.collector->get_name(), "vm_collector");
}

// Test create with configuration
TEST_F(MetricFactoryTest, CreateWithConfiguration) {
    auto& factory = metric_factory::instance();

    config_map config = {{"enabled", "true"}};
    auto result = factory.create("uptime_collector", config);
    EXPECT_TRUE(result) << result.error_message;
    EXPECT_TRUE(result.collector->is_healthy());
}

// Test create unknown collector
TEST_F(MetricFactoryTest, CreateUnknownCollectorFails) {
    auto& factory = metric_factory::instance();

    auto result = factory.create("nonexistent_collector", {});
    EXPECT_FALSE(result);
    EXPECT_EQ(result.collector, nullptr);
    EXPECT_FALSE(result.error_message.empty());
}

// Test create_or_null
TEST_F(MetricFactoryTest, CreateOrNull) {
    auto& factory = metric_factory::instance();

    auto collector = factory.create_or_null("system_resource_collector", {});
    EXPECT_NE(collector, nullptr);

    auto null_collector = factory.create_or_null("nonexistent", {});
    EXPECT_EQ(null_collector, nullptr);
}

// Test get_registered_collectors
TEST_F(MetricFactoryTest, GetRegisteredCollectors) {
    auto& factory = metric_factory::instance();

    auto registered = factory.get_registered_collectors();
    EXPECT_FALSE(registered.empty());

    // Check that all builtin collectors are in the list
    auto builtin = get_builtin_collector_names();
    for (const auto& name : builtin) {
        bool found = std::find(registered.begin(), registered.end(), name) != registered.end();
        EXPECT_TRUE(found) << "Builtin collector not in registered list: " << name;
    }
}

// Test unregister_collector
TEST_F(MetricFactoryTest, UnregisterCollector) {
    auto& factory = metric_factory::instance();

    EXPECT_TRUE(factory.is_registered("vm_collector"));
    EXPECT_TRUE(factory.unregister_collector("vm_collector"));
    EXPECT_FALSE(factory.is_registered("vm_collector"));

    // Unregistering again should fail
    EXPECT_FALSE(factory.unregister_collector("vm_collector"));
}

// Test custom collector registration
class MockCollector : public collector_interface {
   public:
    bool initialize(const config_map& /* config */) override { return true; }
    [[nodiscard]] std::string get_name() const override { return "mock_collector"; }
    [[nodiscard]] bool is_healthy() const override { return true; }
    [[nodiscard]] std::vector<std::string> get_metric_types() const override { return {"mock.metric"}; }
};

TEST_F(MetricFactoryTest, RegisterCustomCollector) {
    auto& factory = metric_factory::instance();

    bool registered = factory.register_collector("mock_collector", []() {
        return std::make_unique<MockCollector>();
    });
    EXPECT_TRUE(registered);
    EXPECT_TRUE(factory.is_registered("mock_collector"));

    auto result = factory.create("mock_collector", {});
    EXPECT_TRUE(result);
    EXPECT_EQ(result.collector->get_name(), "mock_collector");
}

// Test duplicate registration fails
TEST_F(MetricFactoryTest, DuplicateRegistrationFails) {
    auto& factory = metric_factory::instance();

    EXPECT_TRUE(factory.is_registered("vm_collector"));
    bool registered = factory.register_collector("vm_collector", []() {
        return std::make_unique<MockCollector>();
    });
    EXPECT_FALSE(registered);
}

// Test create_multiple
TEST_F(MetricFactoryTest, CreateMultiple) {
    auto& factory = metric_factory::instance();

    std::unordered_map<std::string, config_map> configs = {
        {"system_resource_collector", {}},
        {"vm_collector", {}},
        {"uptime_collector", {{"enabled", "true"}}}};

    auto collectors = factory.create_multiple(configs);
    EXPECT_EQ(collectors.size(), 3);
}

// Test fixture for config_parser tests
class ConfigParserTest : public ::testing::Test {};

// Test boolean parsing
TEST_F(ConfigParserTest, ParseBool) {
    config_map config = {{"enabled", "true"},
                         {"disabled", "false"},
                         {"one", "1"},
                         {"zero", "0"},
                         {"yes", "yes"},
                         {"no", "no"},
                         {"on", "on"},
                         {"off", "off"},
                         {"TRUE", "TRUE"},
                         {"FALSE", "FALSE"}};

    EXPECT_TRUE(config_parser::get<bool>(config, "enabled", false));
    EXPECT_FALSE(config_parser::get<bool>(config, "disabled", true));
    EXPECT_TRUE(config_parser::get<bool>(config, "one", false));
    EXPECT_FALSE(config_parser::get<bool>(config, "zero", true));
    EXPECT_TRUE(config_parser::get<bool>(config, "yes", false));
    EXPECT_FALSE(config_parser::get<bool>(config, "no", true));
    EXPECT_TRUE(config_parser::get<bool>(config, "on", false));
    EXPECT_FALSE(config_parser::get<bool>(config, "off", true));
    EXPECT_TRUE(config_parser::get<bool>(config, "TRUE", false));
    EXPECT_FALSE(config_parser::get<bool>(config, "FALSE", true));
}

// Test integer parsing
TEST_F(ConfigParserTest, ParseInt) {
    config_map config = {{"positive", "42"},
                         {"negative", "-10"},
                         {"zero", "0"},
                         {"large", "1000000"}};

    EXPECT_EQ(config_parser::get<int>(config, "positive", 0), 42);
    EXPECT_EQ(config_parser::get<int>(config, "negative", 0), -10);
    EXPECT_EQ(config_parser::get<int>(config, "zero", 1), 0);
    EXPECT_EQ(config_parser::get<int>(config, "large", 0), 1000000);
}

// Test size_t parsing
TEST_F(ConfigParserTest, ParseSizeT) {
    config_map config = {{"samples", "1000"}, {"max", "9999999999"}};

    EXPECT_EQ(config_parser::get<size_t>(config, "samples", 0), 1000u);
    EXPECT_EQ(config_parser::get<size_t>(config, "max", 0), 9999999999u);
}

// Test double parsing
TEST_F(ConfigParserTest, ParseDouble) {
    config_map config = {{"threshold", "0.75"}, {"negative", "-1.5"}, {"integer", "10"}};

    EXPECT_DOUBLE_EQ(config_parser::get<double>(config, "threshold", 0.0), 0.75);
    EXPECT_DOUBLE_EQ(config_parser::get<double>(config, "negative", 0.0), -1.5);
    EXPECT_DOUBLE_EQ(config_parser::get<double>(config, "integer", 0.0), 10.0);
}

// Test string parsing
TEST_F(ConfigParserTest, ParseString) {
    config_map config = {{"name", "test_collector"}, {"empty", ""}};

    EXPECT_EQ(config_parser::get<std::string>(config, "name", ""), "test_collector");
    EXPECT_EQ(config_parser::get<std::string>(config, "empty", "default"), "");
}

// Test default value for missing key
TEST_F(ConfigParserTest, DefaultValueForMissingKey) {
    config_map config = {};

    EXPECT_TRUE(config_parser::get<bool>(config, "missing", true));
    EXPECT_EQ(config_parser::get<int>(config, "missing", 42), 42);
    EXPECT_EQ(config_parser::get<std::string>(config, "missing", "default"), "default");
}

// Test default value for invalid parsing
TEST_F(ConfigParserTest, DefaultValueForInvalidParsing) {
    config_map config = {{"invalid_int", "not_a_number"}, {"invalid_double", "abc"}};

    EXPECT_EQ(config_parser::get<int>(config, "invalid_int", 100), 100);
    EXPECT_DOUBLE_EQ(config_parser::get<double>(config, "invalid_double", 1.5), 1.5);
}

// Test has_key
TEST_F(ConfigParserTest, HasKey) {
    config_map config = {{"exists", "value"}};

    EXPECT_TRUE(config_parser::has_key(config, "exists"));
    EXPECT_FALSE(config_parser::has_key(config, "missing"));
}

// Test get_optional
TEST_F(ConfigParserTest, GetOptional) {
    config_map config = {{"exists", "42"}};

    auto value = config_parser::get_optional<int>(config, "exists");
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(*value, 42);

    auto missing = config_parser::get_optional<int>(config, "missing");
    EXPECT_FALSE(missing.has_value());
}

// Test get_clamped
TEST_F(ConfigParserTest, GetClamped) {
    config_map config = {{"low", "5"}, {"high", "150"}, {"normal", "50"}};

    EXPECT_EQ(config_parser::get_clamped<int>(config, "low", 50, 10, 100), 10);
    EXPECT_EQ(config_parser::get_clamped<int>(config, "high", 50, 10, 100), 100);
    EXPECT_EQ(config_parser::get_clamped<int>(config, "normal", 0, 10, 100), 50);
}

}  // namespace
}  // namespace kcenon::monitoring
