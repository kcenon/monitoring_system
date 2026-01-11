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
#include <chrono>
#include <unordered_set>

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

// Test get_enum
TEST_F(ConfigParserTest, GetEnum) {
    config_map config = {{"level", "debug"}, {"invalid_level", "unknown"}};

    std::unordered_set<std::string> allowed = {"debug", "info", "warning", "error"};

    EXPECT_EQ(config_parser::get_enum<std::string>(config, "level", "info", allowed), "debug");
    EXPECT_EQ(config_parser::get_enum<std::string>(config, "invalid_level", "info", allowed), "info");
    EXPECT_EQ(config_parser::get_enum<std::string>(config, "missing", "info", allowed), "info");
}

// Test get_enum with integer
TEST_F(ConfigParserTest, GetEnumInteger) {
    config_map config = {{"priority", "1"}, {"invalid_priority", "5"}};

    std::unordered_set<int> allowed = {0, 1, 2, 3};

    EXPECT_EQ(config_parser::get_enum<int>(config, "priority", 0, allowed), 1);
    EXPECT_EQ(config_parser::get_enum<int>(config, "invalid_priority", 0, allowed), 0);
}

// Test get_matching (regex validation)
TEST_F(ConfigParserTest, GetMatching) {
    config_map config = {
        {"valid_email", "test@example.com"},
        {"invalid_email", "not-an-email"},
        {"ipv4", "192.168.1.1"}
    };

    // Simple email pattern
    std::string email_pattern = R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})";
    EXPECT_EQ(config_parser::get_matching(config, "valid_email", "", email_pattern), "test@example.com");
    EXPECT_EQ(config_parser::get_matching(config, "invalid_email", "default@test.com", email_pattern), "default@test.com");

    // IPv4 pattern
    std::string ipv4_pattern = R"(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3})";
    EXPECT_EQ(config_parser::get_matching(config, "ipv4", "0.0.0.0", ipv4_pattern), "192.168.1.1");
}

// Test get_validated with custom validator
TEST_F(ConfigParserTest, GetValidated) {
    config_map config = {{"port", "8080"}, {"invalid_port", "70000"}};

    auto port_validator = [](const int& value) { return value > 0 && value < 65536; };

    EXPECT_EQ(config_parser::get_validated<int>(config, "port", 80, port_validator), 8080);
    EXPECT_EQ(config_parser::get_validated<int>(config, "invalid_port", 80, port_validator), 80);
    EXPECT_EQ(config_parser::get_validated<int>(config, "missing", 80, port_validator), 80);
}

// Test get_validated with size validation
TEST_F(ConfigParserTest, GetValidatedSizeConstraint) {
    config_map config = {{"buffer_size", "1024"}, {"too_small", "10"}};

    auto min_size_validator = [](const size_t& value) { return value >= 100; };

    EXPECT_EQ(config_parser::get_validated<size_t>(config, "buffer_size", 512, min_size_validator), 1024u);
    EXPECT_EQ(config_parser::get_validated<size_t>(config, "too_small", 512, min_size_validator), 512u);
}

// Test get_duration with various suffixes
TEST_F(ConfigParserTest, GetDurationMilliseconds) {
    config_map config = {
        {"plain", "1000"},
        {"ms", "500ms"},
        {"seconds", "2s"},
        {"minutes", "1m"},
        {"hours", "1h"}
    };

    using ms = std::chrono::milliseconds;

    EXPECT_EQ(config_parser::get_duration<ms>(config, "plain", ms(0)).count(), 1000);
    EXPECT_EQ(config_parser::get_duration<ms>(config, "ms", ms(0)).count(), 500);
    EXPECT_EQ(config_parser::get_duration<ms>(config, "seconds", ms(0)).count(), 2000);
    EXPECT_EQ(config_parser::get_duration<ms>(config, "minutes", ms(0)).count(), 60000);
    EXPECT_EQ(config_parser::get_duration<ms>(config, "hours", ms(0)).count(), 3600000);
}

// Test get_duration with seconds as target type
TEST_F(ConfigParserTest, GetDurationSeconds) {
    config_map config = {
        {"ms", "5000ms"},
        {"sec", "30sec"},
        {"min", "2min"}
    };

    using sec = std::chrono::seconds;

    EXPECT_EQ(config_parser::get_duration<sec>(config, "ms", sec(0)).count(), 5);
    EXPECT_EQ(config_parser::get_duration<sec>(config, "sec", sec(0)).count(), 30);
    EXPECT_EQ(config_parser::get_duration<sec>(config, "min", sec(0)).count(), 120);
}

// Test get_duration with default value
TEST_F(ConfigParserTest, GetDurationDefault) {
    config_map config = {{"invalid", "not_a_duration"}};

    using ms = std::chrono::milliseconds;

    EXPECT_EQ(config_parser::get_duration<ms>(config, "missing", ms(100)).count(), 100);
    EXPECT_EQ(config_parser::get_duration<ms>(config, "invalid", ms(100)).count(), 100);
}

// Test get_list with integers
TEST_F(ConfigParserTest, GetListInt) {
    config_map config = {
        {"ports", "80, 443, 8080"},
        {"single", "9000"},
        {"empty", ""}
    };

    auto ports = config_parser::get_list<int>(config, "ports", {});
    EXPECT_EQ(ports.size(), 3u);
    EXPECT_EQ(ports[0], 80);
    EXPECT_EQ(ports[1], 443);
    EXPECT_EQ(ports[2], 8080);

    auto single = config_parser::get_list<int>(config, "single", {});
    EXPECT_EQ(single.size(), 1u);
    EXPECT_EQ(single[0], 9000);

    auto empty = config_parser::get_list<int>(config, "empty", {100});
    EXPECT_EQ(empty.size(), 1u);  // Uses default
    EXPECT_EQ(empty[0], 100);
}

// Test get_list with strings
TEST_F(ConfigParserTest, GetListString) {
    config_map config = {{"tags", "cpu, memory, disk"}};

    auto tags = config_parser::get_list<std::string>(config, "tags", {});
    EXPECT_EQ(tags.size(), 3u);
    EXPECT_EQ(tags[0], "cpu");
    EXPECT_EQ(tags[1], "memory");
    EXPECT_EQ(tags[2], "disk");
}

// Test get_list with default values
TEST_F(ConfigParserTest, GetListDefault) {
    config_map config = {};

    std::vector<int> defaults = {1, 2, 3};
    auto result = config_parser::get_list<int>(config, "missing", defaults);
    EXPECT_EQ(result.size(), 3u);
    EXPECT_EQ(result[0], 1);
    EXPECT_EQ(result[1], 2);
    EXPECT_EQ(result[2], 3);
}

}  // namespace
}  // namespace kcenon::monitoring
