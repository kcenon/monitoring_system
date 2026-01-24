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
 */

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "kcenon/monitoring/factory/metric_factory.h"

using namespace kcenon::monitoring;

int main() {
    std::cout << "=== Collector Factory Example ===" << std::endl;

    try {
        // Step 1: Get factory singleton instance
        std::cout << "\n1. Getting factory singleton instance..." << std::endl;

        auto& factory = metric_factory::instance();
        auto& factory2 = metric_factory::instance();

        std::cout << "   Factory instance obtained" << std::endl;
        std::cout << "   Same instance: " << (&factory == &factory2 ? "Yes" : "No") << std::endl;

        // Step 2: List registered collectors
        std::cout << "\n2. Listing registered collectors..." << std::endl;

        auto registered = factory.get_registered_collectors();
        std::cout << "   Registered collectors (" << registered.size() << "):" << std::endl;
        for (const auto& name : registered) {
            std::cout << "     - " << name << std::endl;
        }

        // Step 3: Check if specific collectors are registered
        std::cout << "\n3. Checking specific collector registration..." << std::endl;

        std::vector<std::string> check_collectors = {
            "system_resource_collector",
            "network_metrics_collector",
            "process_metrics_collector",
            "platform_metrics_collector",
            "nonexistent_collector"
        };

        for (const auto& name : check_collectors) {
            bool is_reg = factory.is_registered(name);
            std::cout << "   " << name << ": " << (is_reg ? "Registered" : "Not registered") << std::endl;
        }

        // Step 4: Create collectors with configuration
        std::cout << "\n4. Creating collectors with configuration..." << std::endl;

        // Create system resource collector
        config_map sys_config = {
            {"collect_cpu", "true"},
            {"collect_memory", "true"}
        };

        auto sys_result = factory.create("system_resource_collector", sys_config);
        if (sys_result) {
            std::cout << "   ✓ Created: " << sys_result.collector->get_name() << std::endl;
            std::cout << "     Health: " << (sys_result.collector->is_healthy() ? "OK" : "UNHEALTHY") << std::endl;

            auto metric_types = sys_result.collector->get_metric_types();
            std::cout << "     Metric types: " << metric_types.size() << std::endl;
        } else {
            std::cout << "   ✗ Failed: " << sys_result.error_message << std::endl;
        }

        // Step 5: Dynamic collector instantiation
        std::cout << "\n5. Dynamic collector instantiation..." << std::endl;

        std::vector<std::string> to_create = {
            "network_metrics_collector",
            "process_metrics_collector",
            "platform_metrics_collector"
        };

        std::vector<std::unique_ptr<collector_interface>> collectors;

        for (const auto& name : to_create) {
            auto result = factory.create(name);
            if (result) {
                std::cout << "   ✓ Created: " << result.collector->get_name() << std::endl;
                collectors.push_back(std::move(result.collector));
            } else {
                std::cout << "   ✗ Failed: " << name << " - " << result.error_message << std::endl;
            }
        }

        std::cout << "   Total collectors created: " << collectors.size() << std::endl;

        // Step 6: Attempt to create non-existent collector
        std::cout << "\n6. Attempting to create non-existent collector..." << std::endl;

        auto invalid_result = factory.create("nonexistent_collector");
        if (!invalid_result) {
            std::cout << "   Expected failure: " << invalid_result.error_message << std::endl;
        }

        std::cout << "\n=== Example completed successfully ===" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
