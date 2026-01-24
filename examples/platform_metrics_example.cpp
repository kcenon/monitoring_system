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
 * @file platform_metrics_example.cpp
 * @brief Demonstrates platform_metrics_collector usage
 *
 * This example shows how to use the unified platform metrics collector:
 * - platform_metrics_collector initialization
 * - Platform-specific metric access patterns
 * - Strategy pattern usage for OS abstraction
 * - Handling platform-specific features gracefully
 * - Cross-platform metric normalization
 */

#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>

#include "kcenon/monitoring/collectors/platform_metrics_collector.h"

using namespace kcenon::monitoring;
using namespace std::chrono_literals;

/**
 * Display platform information
 */
void display_platform_info(const platform_info& info) {
    std::cout << "\n=== Platform Information ===" << std::endl;

    if (info.available) {
        std::cout << "Platform: " << info.name << std::endl;
        std::cout << "Version: " << info.version << std::endl;
        std::cout << "Architecture: " << info.architecture << std::endl;
    } else {
        std::cout << "Platform information not available" << std::endl;
    }
}

/**
 * Display uptime metrics
 */
void display_uptime_metrics(const platform_uptime& uptime) {
    std::cout << "\n=== Uptime Metrics ===" << std::endl;

    if (uptime.available) {
        // Convert seconds to days, hours, minutes
        int64_t seconds = uptime.uptime_seconds;
        int64_t days = seconds / (24 * 3600);
        seconds %= (24 * 3600);
        int64_t hours = seconds / 3600;
        seconds %= 3600;
        int64_t minutes = seconds / 60;
        seconds %= 60;

        std::cout << "System Uptime: " << days << "d " << hours << "h "
                  << minutes << "m " << seconds << "s" << std::endl;
        std::cout << "Total Uptime: " << uptime.uptime_seconds << " seconds" << std::endl;
        std::cout << "Idle Time: " << uptime.idle_seconds << " seconds" << std::endl;

        // Display boot timestamp as ISO 8601 format if available
        if (uptime.boot_timestamp > 0) {
            auto boot_time = std::chrono::system_clock::from_time_t(uptime.boot_timestamp);
            auto boot_tt = std::chrono::system_clock::to_time_t(boot_time);
            std::cout << "Boot Time: " << std::put_time(std::localtime(&boot_tt), "%Y-%m-%d %H:%M:%S") << std::endl;
        }
    } else {
        std::cout << "Uptime metrics not available on this platform" << std::endl;
    }
}

/**
 * Display context switch statistics
 */
void display_context_switch_stats(const platform_context_switches& switches) {
    std::cout << "\n=== Context Switch Statistics ===" << std::endl;

    if (switches.available) {
        std::cout << "Total Switches: " << switches.total_switches << std::endl;
        std::cout << "Voluntary Switches: " << switches.voluntary_switches << std::endl;
        std::cout << "Involuntary Switches: " << switches.involuntary_switches << std::endl;
        std::cout << "Switches Per Second: " << std::fixed << std::setprecision(2)
                  << switches.switches_per_second << std::endl;
    } else {
        std::cout << "Context switch statistics not available on this platform" << std::endl;
    }
}

/**
 * Display TCP connection state information
 */
void display_tcp_info(const platform_tcp_info& tcp) {
    std::cout << "\n=== TCP Connection States ===" << std::endl;

    if (tcp.available) {
        std::cout << "ESTABLISHED: " << tcp.established << std::endl;
        std::cout << "SYN_SENT: " << tcp.syn_sent << std::endl;
        std::cout << "SYN_RECV: " << tcp.syn_recv << std::endl;
        std::cout << "FIN_WAIT1: " << tcp.fin_wait1 << std::endl;
        std::cout << "FIN_WAIT2: " << tcp.fin_wait2 << std::endl;
        std::cout << "TIME_WAIT: " << tcp.time_wait << std::endl;
        std::cout << "CLOSE_WAIT: " << tcp.close_wait << std::endl;
        std::cout << "LISTEN: " << tcp.listen << std::endl;
        std::cout << "Total Connections: " << tcp.total << std::endl;
    } else {
        std::cout << "TCP state information not available on this platform" << std::endl;
    }
}

/**
 * Display socket buffer information
 */
void display_socket_info(const platform_socket_info& socket) {
    std::cout << "\n=== Socket Buffer Information ===" << std::endl;

    if (socket.available) {
        std::cout << "RX Buffer Size: " << (socket.rx_buffer_size / 1024.0) << " KB" << std::endl;
        std::cout << "TX Buffer Size: " << (socket.tx_buffer_size / 1024.0) << " KB" << std::endl;
        std::cout << "RX Buffer Used: " << (socket.rx_buffer_used / 1024.0) << " KB ("
                  << std::fixed << std::setprecision(1)
                  << (socket.rx_buffer_size > 0 ? (socket.rx_buffer_used * 100.0 / socket.rx_buffer_size) : 0.0)
                  << "%)" << std::endl;
        std::cout << "TX Buffer Used: " << (socket.tx_buffer_used / 1024.0) << " KB ("
                  << (socket.tx_buffer_size > 0 ? (socket.tx_buffer_used * 100.0 / socket.tx_buffer_size) : 0.0)
                  << "%)" << std::endl;
    } else {
        std::cout << "Socket buffer information not available on this platform" << std::endl;
    }
}

/**
 * Display interrupt statistics
 */
void display_interrupt_info(const platform_interrupt_info& interrupts) {
    std::cout << "\n=== Interrupt Statistics ===" << std::endl;

    if (interrupts.available) {
        std::cout << "Total Interrupts: " << interrupts.total_interrupts << std::endl;
    } else {
        std::cout << "Interrupt statistics not available on this platform" << std::endl;
    }
}

/**
 * Demonstrate platform-specific feature detection and handling
 */
void demonstrate_platform_features(platform_metrics_collector& collector) {
    std::cout << "\n=== Platform-Specific Feature Detection ===" << std::endl;

    // Collect platform info first
    auto info = collector.get_platform_info();

    std::cout << "\nDetected Platform: " << info.name << std::endl;

    // Demonstrate feature availability checks before collection
    std::cout << "\nFeature Availability:" << std::endl;

    // Check which features are available on this platform
    auto config = collector.get_config();
    std::cout << "  Uptime Collection: " << (config.collect_uptime ? "Enabled" : "Disabled") << std::endl;
    std::cout << "  Context Switches: " << (config.collect_context_switches ? "Enabled" : "Disabled") << std::endl;
    std::cout << "  TCP States: " << (config.collect_tcp_states ? "Enabled" : "Disabled") << std::endl;
    std::cout << "  Socket Buffers: " << (config.collect_socket_buffers ? "Enabled" : "Disabled") << std::endl;
    std::cout << "  Interrupts: " << (config.collect_interrupts ? "Enabled" : "Disabled") << std::endl;

    std::cout << "\nNote: The Strategy pattern abstracts platform-specific implementations." << std::endl;
    std::cout << "      Features not supported on a platform return empty/unavailable values." << std::endl;

#if defined(__linux__)
    std::cout << "\nLinux-specific features:" << std::endl;
    std::cout << "  - Reading /proc/stat for context switches" << std::endl;
    std::cout << "  - Reading /proc/net/tcp for TCP state info" << std::endl;
    std::cout << "  - Reading /proc/uptime for system uptime" << std::endl;
#elif defined(__APPLE__)
    std::cout << "\nmacOS-specific features:" << std::endl;
    std::cout << "  - Using sysctl for system metrics" << std::endl;
    std::cout << "  - Limited TCP state information" << std::endl;
    std::cout << "  - Using kern.boottime for uptime" << std::endl;
#elif defined(_WIN32)
    std::cout << "\nWindows-specific features:" << std::endl;
    std::cout << "  - Using GetTickCount64 for uptime" << std::endl;
    std::cout << "  - Using Performance Counters for metrics" << std::endl;
    std::cout << "  - Limited context switch information" << std::endl;
#else
    std::cout << "\nUnknown platform - limited feature support" << std::endl;
#endif
}

/**
 * Demonstrate cross-platform metric normalization
 */
void demonstrate_metric_normalization(const std::vector<metric>& metrics) {
    std::cout << "\n=== Cross-Platform Metric Normalization ===" << std::endl;

    std::cout << "\nAll metrics use standardized naming conventions:" << std::endl;
    std::cout << "  platform.uptime.* - Uptime metrics" << std::endl;
    std::cout << "  platform.context_switches.* - Context switch metrics" << std::endl;
    std::cout << "  platform.tcp.* - TCP state metrics" << std::endl;
    std::cout << "  platform.socket.* - Socket buffer metrics" << std::endl;
    std::cout << "  platform.interrupts.* - Interrupt metrics" << std::endl;

    std::cout << "\nCollected Metrics (" << metrics.size() << " total):" << std::endl;

    // Group metrics by prefix for better readability
    std::map<std::string, std::vector<metric>> grouped_metrics;
    for (const auto& m : metrics) {
        auto pos = m.name.find('.', m.name.find('.') + 1); // Find second dot
        std::string prefix = (pos != std::string::npos) ? m.name.substr(0, pos) : m.name;
        grouped_metrics[prefix].push_back(m);
    }

    for (const auto& [prefix, group] : grouped_metrics) {
        std::cout << "\n  " << prefix << ".*:" << std::endl;
        for (const auto& m : group) {
            std::cout << "    " << m.name << ": " << m.value;
            if (!m.unit.empty()) {
                std::cout << " " << m.unit;
            }
            std::cout << std::endl;
        }
    }
}

int main() {
    std::cout << "=== Platform Metrics Example ===" << std::endl;

    try {
        // Step 1: Create platform_metrics_collector with configuration
        std::cout << "\n1. Creating platform_metrics_collector..." << std::endl;

        platform_metrics_config config;
        config.collect_uptime = true;
        config.collect_context_switches = true;
        config.collect_tcp_states = true;
        config.collect_socket_buffers = true;
        config.collect_interrupts = true;

        platform_metrics_collector collector(config);

        // Initialize the collector
        std::unordered_map<std::string, std::string> init_config;
        if (!collector.initialize(init_config)) {
            std::cerr << "Failed to initialize platform_metrics_collector" << std::endl;
            return 1;
        }

        std::cout << "   Initialized: " << collector.get_name() << std::endl;
        std::cout << "   Health: " << (collector.is_healthy() ? "OK" : "UNHEALTHY") << std::endl;

        // Step 2: Display platform information
        std::cout << "\n2. Retrieving platform information..." << std::endl;
        auto platform_info = collector.get_platform_info();
        display_platform_info(platform_info);

        // Step 3: Demonstrate platform-specific feature detection
        std::cout << "\n3. Demonstrating platform-specific features..." << std::endl;
        demonstrate_platform_features(collector);

        // Step 4: Collect and display metrics (3 iterations)
        std::cout << "\n4. Collecting platform metrics (3 iterations)..." << std::endl;

        for (int i = 0; i < 3; ++i) {
            std::cout << "\n--- Iteration " << (i + 1) << "/3 ---" << std::endl;

            // Collect all platform metrics
            auto metrics = collector.collect();
            std::cout << "Metrics collected: " << metrics.size() << std::endl;

            // Get structured metric data for detailed display
            auto uptime = collector.get_uptime();
            auto switches = collector.get_context_switches();
            auto tcp = collector.get_tcp_info();
            auto socket = collector.get_socket_info();
            auto interrupts = collector.get_interrupt_info();

            // Display each metric category
            display_uptime_metrics(uptime);
            display_context_switch_stats(switches);
            display_tcp_info(tcp);
            display_socket_info(socket);
            display_interrupt_info(interrupts);

            // Wait before next collection
            if (i < 2) {
                std::cout << "\nWaiting 2 seconds before next collection..." << std::endl;
                std::this_thread::sleep_for(2s);
            }
        }

        // Step 5: Demonstrate metric normalization
        std::cout << "\n5. Demonstrating cross-platform metric normalization..." << std::endl;
        auto final_metrics = collector.collect();
        demonstrate_metric_normalization(final_metrics);

        // Step 6: Display collector statistics
        std::cout << "\n6. Collector Statistics:" << std::endl;

        auto stats = collector.get_statistics();
        for (const auto& [key, value] : stats) {
            std::cout << "  " << key << ": " << value << std::endl;
        }

        // Step 7: Demonstrate dynamic configuration updates
        std::cout << "\n7. Demonstrating dynamic configuration updates..." << std::endl;

        std::cout << "   Disabling socket buffer collection..." << std::endl;
        config.collect_socket_buffers = false;
        collector.set_config(config);

        auto reduced_metrics = collector.collect();
        std::cout << "   Metrics collected after config update: " << reduced_metrics.size() << std::endl;

        std::cout << "\n=== Example completed successfully ===" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
