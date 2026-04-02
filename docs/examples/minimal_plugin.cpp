// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file minimal_plugin.cpp
 * @brief Minimal collector plugin example
 *
 * This example shows the minimum required code to create a functioning
 * collector plugin. It demonstrates the essential interface methods that
 * must be implemented.
 *
 * NOTE: This is a documentation example. To compile and run, integrate it
 * into the monitoring_system build system using CMake.
 *
 * This is a built-in plugin example (not dynamically loaded).
 * For dynamic plugin loading, see advanced_plugin.cpp and
 * examples/plugin_example/
 */

#include "kcenon/monitoring/plugins/collector_plugin.h"
#include "kcenon/monitoring/plugins/collector_registry.h"

#include <iostream>

namespace kcenon::monitoring {

/**
 * @class minimal_collector
 * @brief Minimal implementation of a collector plugin
 *
 * This plugin collects a single example metric and demonstrates
 * the minimum interface requirements.
 */
class minimal_collector : public collector_plugin {
public:
    /**
     * @brief Return unique plugin name
     */
    auto name() const -> std::string_view override {
        return "minimal_collector";
    }

    /**
     * @brief Collect metrics
     */
    auto collect() -> std::vector<metric> override {
        std::vector<metric> metrics;

        // Create a simple metric
        metric m;
        m.name = "example_value";
        m.value = 42.0;
        m.timestamp = std::chrono::system_clock::now();
        m.tags["unit"] = "count";

        metrics.push_back(m);
        return metrics;
    }

    /**
     * @brief Collection interval (5 seconds)
     */
    auto interval() const -> std::chrono::milliseconds override {
        return std::chrono::seconds(5);
    }

    /**
     * @brief Always available (no platform restrictions)
     */
    auto is_available() const -> bool override {
        return true;
    }

    /**
     * @brief Return metric types produced
     */
    auto get_metric_types() const -> std::vector<std::string> override {
        return {"example_value"};
    }
};

} // namespace kcenon::monitoring

/**
 * @brief Example usage
 */
int main() {
    using namespace kcenon::monitoring;

    std::cout << "=== Minimal Collector Plugin Example ===\n\n";

    // Get registry instance
    auto& registry = collector_registry::instance();

    // Create and register plugin
    auto plugin = std::make_unique<minimal_collector>();
    std::cout << "Registering plugin: " << plugin->name() << "\n";

    if (!registry.register_plugin(std::move(plugin))) {
        std::cerr << "Failed to register plugin\n";
        return 1;
    }

    // Retrieve plugin
    auto* registered = registry.get_plugin("minimal_collector");
    if (!registered) {
        std::cerr << "Plugin not found in registry\n";
        return 1;
    }

    std::cout << "Plugin registered successfully\n\n";

    // Collect metrics
    std::cout << "Collecting metrics...\n";
    auto metrics = registered->collect();

    // Display collected metrics
    std::cout << "Collected " << metrics.size() << " metric(s):\n";
    for (const auto& m : metrics) {
        std::cout << "  - " << m.name << ": " << m.value;

        // Display tags
        if (!m.tags.empty()) {
            std::cout << " [";
            for (const auto& [key, value] : m.tags) {
                std::cout << key << "=" << value << " ";
            }
            std::cout << "]";
        }

        std::cout << "\n";
    }

    std::cout << "\n=== Example Complete ===\n";

    return 0;
}
