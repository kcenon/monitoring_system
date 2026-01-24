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

/**
 * @file collector_factory_example.cpp
 * @brief Demonstrates metric_factory usage for collector creation
 *
 * This example shows how to use the unified collector factory:
 * - metric_factory initialization and singleton access
 * - Configuration-based collector creation
 * - Dynamic collector instantiation
 * - Collector registration and discovery
 * - Custom collector registration
 */

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "kcenon/monitoring/factory/metric_factory.h"
#include "kcenon/monitoring/collectors/system_resource_collector.h"
#include "kcenon/monitoring/collectors/network_metrics_collector.h"
#include "kcenon/monitoring/collectors/process_metrics_collector.h"
#include "kcenon/monitoring/collectors/platform_metrics_collector.h"

using namespace kcenon::monitoring;

/**
 * Example custom collector for demonstration
 */
class custom_example_collector : public metric_collector_plugin {
   public:
    custom_example_collector() : metric_collector_plugin() {}

    bool initialize(const std::unordered_map<std::string, std::string>& config) override {
        std::cout << "  Custom collector initialized with " << config.size() << " config params" << std::endl;

        for (const auto& [key, value] : config) {
            std::cout << "    " << key << " = " << value << std::endl;
        }

        initialized_ = true;
        return true;
    }

    std::vector<metric> collect() override {
        collection_count_++;

        std::vector<metric> metrics;

        // Create sample metrics
        metric m1;
        m1.name = "custom.example.count";
        m1.value = static_cast<double>(collection_count_);
        m1.unit = "count";
        m1.timestamp = std::chrono::system_clock::now();
        metrics.push_back(m1);

        metric m2;
        m2.name = "custom.example.status";
        m2.value = 1.0; // 1.0 = healthy
        m2.unit = "";
        m2.timestamp = std::chrono::system_clock::now();
        metrics.push_back(m2);

        return metrics;
    }

    std::string get_name() const override {
        return "custom_example_collector";
    }

    std::vector<std::string> get_metric_types() const override {
        return {"custom.example.count", "custom.example.status"};
    }

    bool is_healthy() const override {
        return initialized_;
    }

    std::unordered_map<std::string, double> get_statistics() const override {
        return {
            {"collection_count", static_cast<double>(collection_count_)},
            {"initialized", initialized_ ? 1.0 : 0.0}
        };
    }

   private:
    bool initialized_{false};
    int collection_count_{0};
};

/**
 * Demonstrate factory singleton access
 */
void demonstrate_factory_singleton() {
    std::cout << "\n=== Factory Singleton Access ===" << std::endl;

    // Get factory instance
    auto& factory = metric_factory::instance();
    std::cout << "Factory instance obtained" << std::endl;

    // Verify singleton behavior
    auto& factory2 = metric_factory::instance();
    std::cout << "Same instance: " << (&factory == &factory2 ? "Yes" : "No") << std::endl;

    // List available collectors
    auto available = factory.get_available_collectors();
    std::cout << "\nAvailable collectors (" << available.size() << "):" << std::endl;
    for (const auto& name : available) {
        std::cout << "  - " << name << std::endl;
    }
}

/**
 * Demonstrate configuration-based collector creation
 */
void demonstrate_config_based_creation() {
    std::cout << "\n=== Configuration-Based Collector Creation ===" << std::endl;

    auto& factory = metric_factory::instance();

    // Create system resource collector with configuration
    std::cout << "\n1. Creating system_resource_collector with config..." << std::endl;

    config_map sys_config = {
        {"collect_cpu", "true"},
        {"collect_memory", "true"},
        {"collect_disk", "false"},  // Disable disk metrics
        {"collect_network", "true"},
        {"interval_ms", "1000"}
    };

    auto sys_result = factory.create("system_resource_collector", sys_config);
    if (sys_result) {
        std::cout << "   Success: " << sys_result.collector->get_name() << std::endl;
        std::cout << "   Health: " << (sys_result.collector->is_healthy() ? "OK" : "UNHEALTHY") << std::endl;

        auto metric_types = sys_result.collector->get_metric_types();
        std::cout << "   Metric types (" << metric_types.size() << "): ";
        for (size_t i = 0; i < std::min(metric_types.size(), size_t(3)); ++i) {
            std::cout << metric_types[i];
            if (i < std::min(metric_types.size(), size_t(3)) - 1) std::cout << ", ";
        }
        if (metric_types.size() > 3) {
            std::cout << ", ... (+" << (metric_types.size() - 3) << " more)";
        }
        std::cout << std::endl;
    } else {
        std::cerr << "   Failed: " << sys_result.error_message << std::endl;
    }

    // Create network metrics collector
    std::cout << "\n2. Creating network_metrics_collector..." << std::endl;

    config_map net_config = {
        {"enabled", "true"}
    };

    auto net_result = factory.create("network_metrics_collector", net_config);
    if (net_result) {
        std::cout << "   Success: " << net_result.collector->get_name() << std::endl;
        std::cout << "   Health: " << (net_result.collector->is_healthy() ? "OK" : "UNHEALTHY") << std::endl;
    } else {
        std::cerr << "   Failed: " << net_result.error_message << std::endl;
    }
}

/**
 * Demonstrate dynamic collector instantiation
 */
void demonstrate_dynamic_instantiation() {
    std::cout << "\n=== Dynamic Collector Instantiation ===" << std::endl;

    auto& factory = metric_factory::instance();

    // List of collector names to create dynamically
    std::vector<std::string> collector_names = {
        "system_resource_collector",
        "network_metrics_collector",
        "process_metrics_collector",
        "platform_metrics_collector"
    };

    std::vector<std::unique_ptr<collector_interface>> collectors;

    std::cout << "\nCreating collectors dynamically:" << std::endl;

    for (const auto& name : collector_names) {
        auto result = factory.create(name);

        if (result) {
            std::cout << "  ✓ Created: " << result.collector->get_name() << std::endl;
            collectors.push_back(std::move(result.collector));
        } else {
            std::cout << "  ✗ Failed: " << name << " - " << result.error_message << std::endl;
        }
    }

    std::cout << "\nTotal collectors created: " << collectors.size() << std::endl;

    // Try to create a non-existent collector
    std::cout << "\nAttempting to create non-existent collector:" << std::endl;
    auto invalid_result = factory.create("nonexistent_collector");
    if (!invalid_result) {
        std::cout << "  Expected failure: " << invalid_result.error_message << std::endl;
    }
}

/**
 * Demonstrate collector registration and discovery
 */
void demonstrate_registration_and_discovery() {
    std::cout << "\n=== Collector Registration and Discovery ===" << std::endl;

    auto& factory = metric_factory::instance();

    // Check if collectors are already registered (they should be from builtin_collectors)
    std::cout << "\n1. Initial registered collectors:" << std::endl;
    auto initial_collectors = factory.get_available_collectors();
    std::cout << "   Count: " << initial_collectors.size() << std::endl;

    // Check if specific collector is available
    std::cout << "\n2. Checking specific collector availability:" << std::endl;
    std::cout << "   system_resource_collector: "
              << (factory.is_available("system_resource_collector") ? "Available" : "Not available")
              << std::endl;
    std::cout << "   custom_example_collector: "
              << (factory.is_available("custom_example_collector") ? "Available" : "Not available")
              << std::endl;

    // Get collector metadata if available
    std::cout << "\n3. Getting collector metadata:" << std::endl;

    for (const auto& name : {"system_resource_collector", "network_metrics_collector"}) {
        if (factory.is_available(name)) {
            auto metadata = factory.get_collector_metadata(name);
            std::cout << "   " << name << ":" << std::endl;

            if (metadata.description.empty()) {
                std::cout << "     (No metadata available)" << std::endl;
            } else {
                std::cout << "     Description: " << metadata.description << std::endl;
                std::cout << "     Required config: ";
                for (const auto& req : metadata.required_config_keys) {
                    std::cout << req << " ";
                }
                std::cout << std::endl;
            }
        }
    }
}

/**
 * Demonstrate custom collector registration
 */
void demonstrate_custom_registration() {
    std::cout << "\n=== Custom Collector Registration ===" << std::endl;

    auto& factory = metric_factory::instance();

    // Register custom collector
    std::cout << "\n1. Registering custom_example_collector..." << std::endl;

    bool registered = factory.register_collector<custom_example_collector>("custom_example_collector");

    if (registered) {
        std::cout << "   Registration successful" << std::endl;
    } else {
        std::cout << "   Registration failed (may already be registered)" << std::endl;
    }

    // Verify registration
    std::cout << "\n2. Verifying registration:" << std::endl;
    std::cout << "   Is available: "
              << (factory.is_available("custom_example_collector") ? "Yes" : "No")
              << std::endl;

    // Create instance of custom collector
    std::cout << "\n3. Creating custom collector instance..." << std::endl;

    config_map custom_config = {
        {"sample_param", "sample_value"},
        {"enable_logging", "true"}
    };

    auto custom_result = factory.create("custom_example_collector", custom_config);

    if (custom_result) {
        std::cout << "   Created: " << custom_result.collector->get_name() << std::endl;
        std::cout << "   Health: " << (custom_result.collector->is_healthy() ? "OK" : "UNHEALTHY") << std::endl;

        auto metric_types = custom_result.collector->get_metric_types();
        std::cout << "   Metric types: ";
        for (const auto& type : metric_types) {
            std::cout << type << " ";
        }
        std::cout << std::endl;
    } else {
        std::cerr << "   Failed: " << custom_result.error_message << std::endl;
    }

    // Unregister custom collector
    std::cout << "\n4. Unregistering custom collector..." << std::endl;
    bool unregistered = factory.unregister_collector("custom_example_collector");
    std::cout << "   Unregistration " << (unregistered ? "successful" : "failed") << std::endl;

    // Verify unregistration
    std::cout << "   Is still available: "
              << (factory.is_available("custom_example_collector") ? "Yes" : "No")
              << std::endl;
}

/**
 * Demonstrate batch collector creation from configuration
 */
void demonstrate_batch_creation() {
    std::cout << "\n=== Batch Collector Creation ===" << std::endl;

    auto& factory = metric_factory::instance();

    // Define multiple collectors to create
    std::vector<std::pair<std::string, config_map>> batch_configs = {
        {"system_resource_collector", {{"collect_cpu", "true"}, {"collect_memory", "true"}}},
        {"network_metrics_collector", {{"enabled", "true"}}},
        {"process_metrics_collector", {{"enabled", "true"}}},
        {"platform_metrics_collector", {{"collect_uptime", "true"}}}
    };

    std::cout << "\nCreating " << batch_configs.size() << " collectors in batch:" << std::endl;

    int success_count = 0;
    int failure_count = 0;

    for (const auto& [name, config] : batch_configs) {
        auto result = factory.create(name, config);

        if (result) {
            std::cout << "  ✓ " << name << std::endl;
            success_count++;
        } else {
            std::cout << "  ✗ " << name << " - " << result.error_message << std::endl;
            failure_count++;
        }
    }

    std::cout << "\nResults: " << success_count << " succeeded, " << failure_count << " failed" << std::endl;
}

int main() {
    std::cout << "=== Collector Factory Example ===" << std::endl;

    try {
        // Step 1: Demonstrate factory singleton
        demonstrate_factory_singleton();

        // Step 2: Demonstrate configuration-based creation
        demonstrate_config_based_creation();

        // Step 3: Demonstrate dynamic instantiation
        demonstrate_dynamic_instantiation();

        // Step 4: Demonstrate registration and discovery
        demonstrate_registration_and_discovery();

        // Step 5: Demonstrate custom registration
        demonstrate_custom_registration();

        // Step 6: Demonstrate batch creation
        demonstrate_batch_creation();

        std::cout << "\n=== Example completed successfully ===" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
