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

/**
 * @file plugin_loader_example.cpp
 * @brief Example demonstrating dynamic plugin loading
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
