// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file plugin_loader_example.cpp
 * @brief Example demonstrating dynamic plugin loading
 * @example plugin_example/plugin_loader_example.cpp
 *
 * This program shows how to:
 * 1. Load a plugin from a shared library
 * 2. Initialize and use the plugin
 * 3. Collect metrics from the plugin
 * 4. Unload the plugin
 *
 * Build:
 *   g++ -o plugin_loader_example plugin_loader_example.cpp \
 *       -I../../include -L../../build -lmonitoring_system -std=c++20
 *
 * Run:
 *   ./plugin_loader_example ./libexample_plugin.so
 */

#include "kcenon/monitoring/plugins/collector_registry.h"
#include "kcenon/monitoring/plugins/plugin_loader.h"

#include <iostream>
#include <thread>
#include <chrono>

using namespace kcenon::monitoring;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <plugin_path>\n";
        std::cerr << "Example: " << argv[0] << " ./libexample_plugin.so\n";
        return 1;
    }

    std::string plugin_path = argv[1];

    std::cout << "=== Dynamic Plugin Loading Example ===\n\n";

    // Get the collector registry instance
    auto& registry = collector_registry::instance();

    // Load the plugin from shared library
    std::cout << "Loading plugin from: " << plugin_path << "\n";
    if (!registry.load_plugin(plugin_path)) {
        std::cerr << "Failed to load plugin: "
                  << registry.get_plugin_loader_error() << "\n";
        return 1;
    }
    std::cout << "Plugin loaded successfully\n\n";

    // Get the loaded plugin
    auto* plugin = registry.get_plugin("example_plugin");
    if (!plugin) {
        std::cerr << "Plugin not found in registry\n";
        return 1;
    }

    // Display plugin metadata
    auto metadata = plugin->get_metadata();
    std::cout << "Plugin Metadata:\n";
    std::cout << "  Name: " << plugin->name() << "\n";
    std::cout << "  Description: " << metadata.description << "\n";
    std::cout << "  Version: " << metadata.version << "\n";
    std::cout << "  Available: " << (plugin->is_available() ? "yes" : "no") << "\n\n";

    // Initialize the plugin
    std::cout << "Initializing plugin...\n";
    config_map config;  // Empty config for this example
    if (!plugin->initialize(config)) {
        std::cerr << "Failed to initialize plugin\n";
        return 1;
    }
    std::cout << "Plugin initialized\n\n";

    // Collect metrics from the plugin
    std::cout << "Collecting metrics (5 iterations)...\n";
    for (int i = 0; i < 5; ++i) {
        std::cout << "\nIteration " << (i + 1) << ":\n";

        auto metrics = plugin->collect();
        for (const auto& metric : metrics) {
            std::cout << "  " << metric.name << ": " << metric.value
                      << " " << metric.unit;

            if (!metric.labels.empty()) {
                std::cout << " [";
                bool first = true;
                for (const auto& [key, value] : metric.labels) {
                    if (!first) std::cout << ", ";
                    std::cout << key << "=" << value;
                    first = false;
                }
                std::cout << "]";
            }
            std::cout << "\n";
        }

        // Wait before next collection
        if (i < 4) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    // Shutdown and unload the plugin
    std::cout << "\nShutting down plugin...\n";
    plugin->shutdown();

    std::cout << "Unloading plugin...\n";
    if (!registry.unload_plugin("example_plugin")) {
        std::cerr << "Failed to unload plugin\n";
        return 1;
    }
    std::cout << "Plugin unloaded successfully\n";

    std::cout << "\n=== Example Complete ===\n";
    return 0;
}
