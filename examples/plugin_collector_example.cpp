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

#include <chrono>
#include <iostream>
#include <thread>

#include <kcenon/monitoring/collectors/logger_system_collector.h>
#include <kcenon/monitoring/collectors/plugin_metric_collector.h>
#include <kcenon/monitoring/collectors/system_resource_collector.h>
#include <kcenon/monitoring/collectors/thread_system_collector.h>

using namespace kcenon::monitoring;

int main() {
    std::cout << "=== Plugin-based Metric Collector Example ===" << std::endl;

    // Create plugin collector with configuration
    plugin_collector_config config;
    config.collection_interval = std::chrono::milliseconds(1000);
    config.enable_caching = true;
    config.enable_streaming = false;
    config.worker_threads = 2;

    auto collector = std::make_unique<plugin_metric_collector>(config);

    // Create and register system resource collector
    auto sys_collector = std::make_unique<system_resource_collector>();
    if (sys_collector->initialize({})) {
        std::cout << "System resource collector initialized" << std::endl;
        collector->register_plugin(std::move(sys_collector));
    }

    // Create and register thread system collector
    auto thread_collector = std::make_unique<thread_system_collector>();
    if (thread_collector->initialize({})) {
        std::cout << "Thread system collector initialized" << std::endl;
        collector->register_plugin(std::move(thread_collector));
    }

    // Create and register logger system collector
    auto logger_collector = std::make_unique<logger_system_collector>();
    if (logger_collector->initialize({})) {
        std::cout << "Logger system collector initialized" << std::endl;
        collector->register_plugin(std::move(logger_collector));
    }

    // List registered plugins
    std::cout << "\nRegistered plugins:" << std::endl;
    for (const auto& plugin_name : collector->get_registered_plugins()) {
        std::cout << "  - " << plugin_name << std::endl;
    }

    // Start collection
    if (collector->start()) {
        std::cout << "\nCollection started successfully" << std::endl;
    } else {
        std::cerr << "Failed to start collection" << std::endl;
        return 1;
    }

    // Run for a few seconds and collect metrics
    std::cout << "\nCollecting metrics for 5 seconds..." << std::endl;
    for (int i = 0; i < 5; ++i) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Force immediate collection
        auto metrics = collector->force_collect();
        std::cout << "Collected " << metrics.size() << " metrics" << std::endl;

        // Display some metrics
        for (const auto& metric : metrics) {
            if (i == 0) {  // Only show first iteration to avoid clutter
                std::cout << "  " << metric.name << ": " << metric.value << " " << metric.unit << std::endl;
            }
        }
    }

    // Get cached metrics
    auto cached = collector->get_cached_metrics();
    std::cout << "\nTotal cached metrics: " << cached.size() << std::endl;

    // Stop collection
    collector->stop();
    std::cout << "Collection stopped" << std::endl;

    return 0;
}