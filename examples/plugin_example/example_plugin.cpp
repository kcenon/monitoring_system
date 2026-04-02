// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file example_plugin.cpp
 * @brief Example dynamically loaded collector plugin
 *
 * This file demonstrates how to create a collector plugin that can be
 * loaded as a shared library at runtime.
 *
 * Build:
 *   g++ -shared -fPIC -o libexample_plugin.so example_plugin.cpp \
 *       -I../../include -std=c++20
 *
 * Usage:
 *   auto& registry = collector_registry::instance();
 *   registry.load_plugin("./libexample_plugin.so");
 */

#include "kcenon/monitoring/plugins/collector_plugin.h"
#include "kcenon/monitoring/plugins/plugin_api.h"

#include <chrono>
#include <random>

using namespace kcenon::monitoring;

/**
 * @class example_plugin
 * @brief Example collector plugin that generates dummy metrics
 */
class example_plugin : public collector_plugin {
public:
    example_plugin() : generator_(std::random_device{}()) {}

    auto name() const -> std::string_view override {
        return "example_plugin";
    }

    auto initialize(const config_map& config) -> bool override {
        (void)config;  // Unused
        initialized_ = true;
        return true;
    }

    auto shutdown() -> void override {
        initialized_ = false;
    }

    auto collect() -> std::vector<metric_data> override {
        std::vector<metric_data> metrics;

        if (!initialized_) {
            return metrics;
        }

        // Generate some dummy metrics
        metric_data cpu_metric;
        cpu_metric.name = "example.cpu_usage";
        cpu_metric.value = generate_random_value(0.0, 100.0);
        cpu_metric.unit = "%";
        cpu_metric.timestamp = std::chrono::system_clock::now();
        cpu_metric.labels["plugin"] = "example";
        cpu_metric.labels["type"] = "cpu";
        metrics.push_back(cpu_metric);

        metric_data memory_metric;
        memory_metric.name = "example.memory_usage";
        memory_metric.value = generate_random_value(0.0, 1024.0);
        memory_metric.unit = "MB";
        memory_metric.timestamp = std::chrono::system_clock::now();
        memory_metric.labels["plugin"] = "example";
        memory_metric.labels["type"] = "memory";
        metrics.push_back(memory_metric);

        metric_data counter_metric;
        counter_metric.name = "example.request_count";
        counter_metric.value = static_cast<double>(++request_counter_);
        counter_metric.unit = "requests";
        counter_metric.timestamp = std::chrono::system_clock::now();
        counter_metric.labels["plugin"] = "example";
        counter_metric.labels["type"] = "counter";
        metrics.push_back(counter_metric);

        return metrics;
    }

    auto is_available() const -> bool override {
        // This plugin is available on all platforms
        return true;
    }

    auto get_metadata() const -> plugin_metadata_t override {
        return plugin_metadata_t{
            plugin_category::custom,
            plugin_type::collector,
            "Example Plugin",
            "1.0.0",
            "Demonstrates dynamic plugin loading"
        };
    }

private:
    auto generate_random_value(double min, double max) -> double {
        std::uniform_real_distribution<double> dist(min, max);
        return dist(generator_);
    }

    bool initialized_{false};
    uint64_t request_counter_{0};
    std::mt19937 generator_;
};

// Export plugin using the IMPLEMENT_PLUGIN macro
IMPLEMENT_PLUGIN(
    example_plugin,
    "example_plugin",
    "1.0.0",
    "Example dynamically loaded collector plugin",
    "kcenon",
    "custom"
)
