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
 * @file system_collectors_example.cpp
 * @brief Demonstrates unified system collectors usage
 *
 * This example shows how to use the consolidated system collectors:
 * - system_resource_collector (CPU, memory, disk)
 * - network_metrics_collector (socket buffers, TCP states)
 * - process_metrics_collector (file descriptors, inodes, context switches)
 * - thread_system_collector integration
 * - logger_system_collector integration
 * - Collector lifecycle management (start/stop/collect)
 */

#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>

#include "kcenon/monitoring/collectors/system_resource_collector.h"
#include "kcenon/monitoring/collectors/network_metrics_collector.h"
#include "kcenon/monitoring/collectors/process_metrics_collector.h"

#ifdef THREAD_SYSTEM_AVAILABLE
#include "kcenon/monitoring/collectors/thread_system_collector.h"
#endif

#ifdef LOGGER_SYSTEM_AVAILABLE
#include "kcenon/monitoring/collectors/logger_system_collector.h"
#endif

using namespace kcenon::monitoring;
using namespace std::chrono_literals;

/**
 * Display system resource metrics in a formatted manner
 */
void display_system_metrics(const system_resources& resources) {
    std::cout << "\n=== System Resource Metrics ===" << std::endl;

    // CPU metrics
    std::cout << "CPU:" << std::endl;
    std::cout << "  Usage: " << std::fixed << std::setprecision(2)
              << resources.cpu.usage_percent << "%" << std::endl;
    std::cout << "  User: " << resources.cpu.user_percent << "%" << std::endl;
    std::cout << "  System: " << resources.cpu.system_percent << "%" << std::endl;
    std::cout << "  Idle: " << resources.cpu.idle_percent << "%" << std::endl;
    std::cout << "  Core Count: " << resources.cpu.count << std::endl;
    std::cout << "  Load Average: "
              << resources.cpu.load.one_min << " (1m), "
              << resources.cpu.load.five_min << " (5m), "
              << resources.cpu.load.fifteen_min << " (15m)" << std::endl;

    // Memory metrics
    std::cout << "\nMemory:" << std::endl;
    std::cout << "  Total: " << (resources.memory.total_bytes / (1024.0 * 1024.0 * 1024.0))
              << " GB" << std::endl;
    std::cout << "  Used: " << (resources.memory.used_bytes / (1024.0 * 1024.0 * 1024.0))
              << " GB (" << resources.memory.usage_percent << "%)" << std::endl;
    std::cout << "  Available: " << (resources.memory.available_bytes / (1024.0 * 1024.0 * 1024.0))
              << " GB" << std::endl;
    std::cout << "  Swap Used: " << (resources.memory.swap.used_bytes / (1024.0 * 1024.0 * 1024.0))
              << " GB (" << resources.memory.swap.usage_percent << "%)" << std::endl;

    // Disk metrics
    std::cout << "\nDisk:" << std::endl;
    std::cout << "  Total: " << (resources.disk.total_bytes / (1024.0 * 1024.0 * 1024.0))
              << " GB" << std::endl;
    std::cout << "  Used: " << (resources.disk.used_bytes / (1024.0 * 1024.0 * 1024.0))
              << " GB (" << resources.disk.usage_percent << "%)" << std::endl;
    std::cout << "  I/O Read: " << (resources.disk.io.read_bytes_per_sec / (1024.0 * 1024.0))
              << " MB/s (" << resources.disk.io.read_ops_per_sec << " ops/s)" << std::endl;
    std::cout << "  I/O Write: " << (resources.disk.io.write_bytes_per_sec / (1024.0 * 1024.0))
              << " MB/s (" << resources.disk.io.write_ops_per_sec << " ops/s)" << std::endl;

    // Network metrics
    std::cout << "\nNetwork:" << std::endl;
    std::cout << "  RX: " << (resources.network.rx_bytes_per_sec / (1024.0 * 1024.0))
              << " MB/s (" << resources.network.rx_packets_per_sec << " packets/s)" << std::endl;
    std::cout << "  TX: " << (resources.network.tx_bytes_per_sec / (1024.0 * 1024.0))
              << " MB/s (" << resources.network.tx_packets_per_sec << " packets/s)" << std::endl;
    std::cout << "  Errors: RX=" << resources.network.rx_errors
              << ", TX=" << resources.network.tx_errors << std::endl;
    std::cout << "  Dropped: RX=" << resources.network.rx_dropped
              << ", TX=" << resources.network.tx_dropped << std::endl;

    // Process metrics
    std::cout << "\nProcess:" << std::endl;
    std::cout << "  Count: " << resources.process.count << std::endl;
    std::cout << "  Threads: " << resources.process.thread_count << std::endl;
    std::cout << "  Handles: " << resources.process.handle_count << std::endl;
    std::cout << "  Open FDs: " << resources.process.open_file_descriptors << std::endl;

    // Context switches
    std::cout << "\nContext Switches:" << std::endl;
    std::cout << "  Total: " << resources.context_switches.total << std::endl;
    std::cout << "  Per Second: " << resources.context_switches.per_sec << std::endl;
    std::cout << "  Voluntary: " << resources.context_switches.voluntary << std::endl;
    std::cout << "  Non-voluntary: " << resources.context_switches.nonvoluntary << std::endl;
}

/**
 * Display network metrics from network_metrics_collector
 */
void display_network_collector_metrics(const std::vector<metric>& metrics) {
    std::cout << "\n=== Network Collector Metrics ===" << std::endl;

    for (const auto& m : metrics) {
        std::cout << "  " << m.name << ": ";
        std::visit([](const auto& val) { std::cout << val; }, m.value);
        auto unit_it = m.tags.find("unit");
        if (unit_it != m.tags.end() && !unit_it->second.empty()) {
            std::cout << " " << unit_it->second;
        }
        std::cout << std::endl;
    }
}

/**
 * Display process metrics from process_metrics_collector
 */
void display_process_collector_metrics(const std::vector<metric>& metrics) {
    std::cout << "\n=== Process Collector Metrics ===" << std::endl;

    for (const auto& m : metrics) {
        std::cout << "  " << m.name << ": ";
        std::visit([](const auto& val) { std::cout << val; }, m.value);
        auto unit_it = m.tags.find("unit");
        if (unit_it != m.tags.end() && !unit_it->second.empty()) {
            std::cout << " " << unit_it->second;
        }
        std::cout << std::endl;
    }
}

int main() {
    std::cout << "=== System Collectors Example ===" << std::endl;

    try {
        // Step 1: Create and configure system_resource_collector
        std::cout << "\n1. Creating system_resource_collector..." << std::endl;

        system_metrics_config sys_config;
        sys_config.collect_cpu = true;
        sys_config.collect_memory = true;
        sys_config.collect_disk = true;
        sys_config.collect_network = true;
        sys_config.collect_process = true;
        sys_config.enable_load_history = true;
        sys_config.load_history_max_samples = 100;
        sys_config.interval = std::chrono::milliseconds(1000);

        system_resource_collector sys_collector(sys_config);

        // Initialize the collector
        std::unordered_map<std::string, std::string> init_config;
        if (!sys_collector.initialize(init_config)) {
            std::cerr << "Failed to initialize system_resource_collector" << std::endl;
            return 1;
        }

        std::cout << "   Initialized: " << sys_collector.get_name() << std::endl;
        std::cout << "   Health: " << (sys_collector.is_healthy() ? "OK" : "UNHEALTHY") << std::endl;

        // Step 2: Create network_metrics_collector
        std::cout << "\n2. Creating network_metrics_collector..." << std::endl;

        network_metrics_collector net_collector;
        if (!net_collector.initialize(init_config)) {
            std::cerr << "Failed to initialize network_metrics_collector" << std::endl;
            return 1;
        }

        std::cout << "   Initialized: " << net_collector.get_name() << std::endl;
        std::cout << "   Health: " << (net_collector.is_healthy() ? "OK" : "UNHEALTHY") << std::endl;

        // Step 3: Create process_metrics_collector
        std::cout << "\n3. Creating process_metrics_collector..." << std::endl;

        process_metrics_collector proc_collector;
        if (!proc_collector.initialize(init_config)) {
            std::cerr << "Failed to initialize process_metrics_collector" << std::endl;
            return 1;
        }

        std::cout << "   Initialized: " << proc_collector.get_name() << std::endl;
        std::cout << "   Health: " << (proc_collector.is_healthy() ? "OK" : "UNHEALTHY") << std::endl;

#ifdef THREAD_SYSTEM_AVAILABLE
        // Step 4: Create thread_system_collector (if available)
        std::cout << "\n4. Creating thread_system_collector..." << std::endl;

        thread_system_collector thread_collector;
        if (!thread_collector.initialize(init_config)) {
            std::cerr << "Failed to initialize thread_system_collector" << std::endl;
            return 1;
        }

        std::cout << "   Initialized: " << thread_collector.get_name() << std::endl;
        std::cout << "   Health: " << (thread_collector.is_healthy() ? "OK" : "UNHEALTHY") << std::endl;
#else
        std::cout << "\n4. thread_system_collector not available (THREAD_SYSTEM_AVAILABLE not defined)" << std::endl;
#endif

#ifdef LOGGER_SYSTEM_AVAILABLE
        // Step 5: Create logger_system_collector (if available)
        std::cout << "\n5. Creating logger_system_collector..." << std::endl;

        logger_system_collector logger_collector;
        if (!logger_collector.initialize(init_config)) {
            std::cerr << "Failed to initialize logger_system_collector" << std::endl;
            return 1;
        }

        std::cout << "   Initialized: " << logger_collector.get_name() << std::endl;
        std::cout << "   Health: " << (logger_collector.is_healthy() ? "OK" : "UNHEALTHY") << std::endl;
#else
        std::cout << "\n5. logger_system_collector not available (LOGGER_SYSTEM_AVAILABLE not defined)" << std::endl;
#endif

        // Step 6: Collector lifecycle demonstration
        std::cout << "\n6. Demonstrating collector lifecycle (3 iterations)..." << std::endl;

        for (int i = 0; i < 3; ++i) {
            std::cout << "\n--- Iteration " << (i + 1) << "/3 ---" << std::endl;

            // Collect system resources
            auto sys_metrics = sys_collector.collect();
            std::cout << "System metrics collected: " << sys_metrics.size() << std::endl;

            // Get last collected resources for detailed display
            auto resources = sys_collector.get_last_resources();
            display_system_metrics(resources);

            // Collect network metrics
            auto net_metrics = net_collector.collect();
            std::cout << "Network metrics collected: " << net_metrics.size() << std::endl;
            display_network_collector_metrics(net_metrics);

            // Collect process metrics
            auto proc_metrics = proc_collector.collect();
            std::cout << "Process metrics collected: " << proc_metrics.size() << std::endl;
            display_process_collector_metrics(proc_metrics);

#ifdef THREAD_SYSTEM_AVAILABLE
            // Collect thread system metrics
            auto thread_metrics = thread_collector.collect();
            std::cout << "Thread system metrics collected: " << thread_metrics.size() << std::endl;
#endif

#ifdef LOGGER_SYSTEM_AVAILABLE
            // Collect logger system metrics
            auto logger_metrics = logger_collector.collect();
            std::cout << "Logger system metrics collected: " << logger_metrics.size() << std::endl;
#endif

            // Wait before next collection
            if (i < 2) {
                std::cout << "\nWaiting 2 seconds before next collection..." << std::endl;
                std::this_thread::sleep_for(2s);
            }
        }

        // Step 7: Display collector statistics
        std::cout << "\n7. Collector Statistics:" << std::endl;

        auto sys_stats = sys_collector.get_statistics();
        std::cout << "\nSystem Resource Collector:" << std::endl;
        for (const auto& [key, value] : sys_stats) {
            std::cout << "  " << key << ": " << value << std::endl;
        }

        auto net_stats = net_collector.get_statistics();
        std::cout << "\nNetwork Metrics Collector:" << std::endl;
        for (const auto& [key, value] : net_stats) {
            std::cout << "  " << key << ": " << value << std::endl;
        }

        auto proc_stats = proc_collector.get_statistics();
        std::cout << "\nProcess Metrics Collector:" << std::endl;
        for (const auto& [key, value] : proc_stats) {
            std::cout << "  " << key << ": " << value << std::endl;
        }

        // Step 8: Load history demonstration (if enabled)
        if (sys_config.enable_load_history) {
            std::cout << "\n8. Load Average History:" << std::endl;

            auto load_history = sys_collector.get_all_load_history();
            std::cout << "   Total samples: " << load_history.size() << std::endl;

            if (!load_history.empty()) {
                auto load_stats = sys_collector.get_all_load_statistics();
                std::cout << "   1-min avg: " << load_stats.load_1m_stats.avg << std::endl;
                std::cout << "   5-min avg: " << load_stats.load_5m_stats.avg << std::endl;
                std::cout << "   15-min avg: " << load_stats.load_15m_stats.avg << std::endl;
            }
        }

        std::cout << "\n=== Example completed successfully ===" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
