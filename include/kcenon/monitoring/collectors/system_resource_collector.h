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

#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <numeric>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#ifdef __APPLE__
    #include <mach/mach.h>
    #include <mach/mach_host.h>
    #include <mach/processor_info.h>
    #include <mach/vm_map.h>
    #include <sys/sysctl.h>
    #include <sys/types.h>
#elif __linux__
    #include <sys/sysinfo.h>
    #include <unistd.h>
#elif _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
    #define NOMINMAX
    #endif
    // winsock2.h must be included before windows.h to avoid redefinition errors
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #include <iphlpapi.h>
    #include <psapi.h>
    #pragma comment(lib, "ws2_32.lib")
    #pragma comment(lib, "iphlpapi.lib")
#endif

#include "plugin_metric_collector.h"
#include "../utils/time_series_buffer.h"

namespace kcenon { namespace monitoring {

/**
 * Configuration for system metrics collection
 *
 * Allows selective enabling/disabling of metric categories with
 * configurable collection intervals for fine-grained control.
 */
struct system_metrics_config {
    bool collect_cpu = true;
    bool collect_memory = true;
    bool collect_disk = true;
    bool collect_network = true;
    bool collect_process = true;
    bool enable_load_history = false;
    size_t load_history_max_samples = 1000;
    std::chrono::milliseconds interval = std::chrono::seconds(10);
};

/**
 * System resource information structure with nested logical groupings
 *
 * This structure organizes system metrics into logical sub-structs for:
 * - Cleaner access patterns: resources.cpu.usage_percent vs resources.cpu_usage_percent
 * - Easier extension: add fields to relevant sub-struct
 * - Partial access: pass resources.cpu only when needed
 */
struct system_resources {
    /**
     * CPU-related metrics
     */
    struct cpu_metrics {
        double usage_percent{0.0};
        double user_percent{0.0};
        double system_percent{0.0};
        double idle_percent{0.0};
        size_t count{0};

        /**
         * System load average
         */
        struct load_average {
            double one_min{0.0};
            double five_min{0.0};
            double fifteen_min{0.0};
        } load;
    } cpu;

    /**
     * Memory-related metrics
     */
    struct memory_metrics {
        size_t total_bytes{0};
        size_t available_bytes{0};
        size_t used_bytes{0};
        double usage_percent{0.0};

        /**
         * Swap memory info
         */
        struct swap_info {
            size_t total_bytes{0};
            size_t used_bytes{0};
            double usage_percent{0.0};
        } swap;
    } memory;

    /**
     * Disk-related metrics
     */
    struct disk_metrics {
        size_t total_bytes{0};
        size_t used_bytes{0};
        size_t available_bytes{0};
        double usage_percent{0.0};

        /**
         * Disk I/O throughput
         */
        struct io_throughput {
            size_t read_bytes_per_sec{0};
            size_t write_bytes_per_sec{0};
            size_t read_ops_per_sec{0};
            size_t write_ops_per_sec{0};
        } io;
    } disk;

    /**
     * Network-related metrics
     */
    struct network_metrics {
        size_t rx_bytes_per_sec{0};
        size_t tx_bytes_per_sec{0};
        size_t rx_packets_per_sec{0};
        size_t tx_packets_per_sec{0};
        size_t rx_errors{0};
        size_t tx_errors{0};
        size_t rx_dropped{0};
        size_t tx_dropped{0};
    } network;

    /**
     * Process-related metrics
     */
    struct process_metrics {
        size_t count{0};
        size_t thread_count{0};
        size_t handle_count{0};
        size_t open_file_descriptors{0};
    } process;

    /**
     * Context switch metrics
     */
    struct context_switch_metrics {
        uint64_t total{0};
        uint64_t per_sec{0};
        uint64_t voluntary{0};
        uint64_t nonvoluntary{0};
    } context_switches;
};

/**
 * Platform-specific system resource collector implementation
 */
class system_info_collector {
  public:
    system_info_collector();
    ~system_info_collector();

    /**
     * Collect current system resources
     * @return System resource information
     */
    system_resources collect();

    /**
     * Get system uptime in seconds
     * @return Uptime in seconds
     */
    std::chrono::seconds get_uptime() const;

    /**
     * Get system hostname
     * @return Hostname string
     */
    std::string get_hostname() const;

    /**
     * Get operating system information
     * @return OS information string
     */
    std::string get_os_info() const;

  private:
    // CPU tracking for usage calculation
    struct cpu_stats {
        uint64_t user{0};
        uint64_t nice{0};
        uint64_t system{0};
        uint64_t idle{0};
        uint64_t iowait{0};
        uint64_t irq{0};
        uint64_t softirq{0};
        uint64_t steal{0};
    };

    mutable std::mutex stats_mutex_;
    cpu_stats last_cpu_stats_;
    std::chrono::steady_clock::time_point last_collection_time_;

    // Network tracking
    struct network_stats {
        uint64_t rx_bytes{0};
        uint64_t tx_bytes{0};
        uint64_t rx_packets{0};
        uint64_t tx_packets{0};
        uint64_t rx_errors{0};
        uint64_t tx_errors{0};
        uint64_t rx_dropped{0};
        uint64_t tx_dropped{0};
    };
    network_stats last_network_stats_;

    // Disk I/O tracking
    struct disk_stats {
        uint64_t read_bytes{0};
        uint64_t write_bytes{0};
        uint64_t read_ops{0};
        uint64_t write_ops{0};
    };
    disk_stats last_disk_stats_;

    // Context switch tracking
    uint64_t last_context_switches_total_{0};

    // Platform-specific collection methods
    void collect_cpu_stats(system_resources& resources);
    void collect_memory_stats(system_resources& resources);
    void collect_disk_stats(system_resources& resources);
    void collect_network_stats(system_resources& resources);
    void collect_process_stats(system_resources& resources);

#ifdef __APPLE__
    void collect_macos_cpu_stats(system_resources& resources);
    void collect_macos_memory_stats(system_resources& resources);
#elif __linux__
    void collect_linux_cpu_stats(system_resources& resources);
    void collect_linux_memory_stats(system_resources& resources);
    cpu_stats parse_proc_stat();
#elif _WIN32
    void collect_windows_cpu_stats(system_resources& resources);
    void collect_windows_memory_stats(system_resources& resources);
#endif
};

/**
 * System resource collector plugin implementation
 *
 * Collects system-level metrics with consistent naming convention:
 * - system.cpu.* - CPU metrics
 * - system.memory.* - Memory metrics
 * - system.disk.* - Disk metrics
 * - system.network.* - Network metrics
 * - system.process.* - Process metrics
 * - system.context_switches.* - Context switch metrics
 */
class system_resource_collector : public metric_collector_plugin {
  public:
    system_resource_collector();

    /**
     * Construct with configuration
     * @param config Collection configuration
     */
    explicit system_resource_collector(const system_metrics_config& config);

    ~system_resource_collector() override = default;

    // metric_collector_plugin implementation
    bool initialize(const std::unordered_map<std::string, std::string>& config) override;
    std::vector<metric> collect() override;
    std::string get_name() const override { return "system_resource_collector"; }
    std::vector<std::string> get_metric_types() const override;
    bool is_healthy() const override;
    std::unordered_map<std::string, double> get_statistics() const override;

    /**
     * Get current configuration
     * @return Current metrics configuration
     */
    system_metrics_config get_config() const;

    /**
     * Update configuration
     * @param config New configuration to apply
     */
    void set_config(const system_metrics_config& config);

    /**
     * Set collection filters
     * @param enable_cpu Enable CPU metrics collection
     * @param enable_memory Enable memory metrics collection
     * @param enable_disk Enable disk metrics collection
     * @param enable_network Enable network metrics collection
     * @deprecated Use set_config() with system_metrics_config instead
     */
    [[deprecated("Use set_config() with system_metrics_config instead")]]
    void set_collection_filters(bool enable_cpu = true, bool enable_memory = true,
                                 bool enable_disk = true, bool enable_network = true);

    /**
     * Get last collected resources
     * @return Last system resources snapshot
     */
    system_resources get_last_resources() const;

    /**
     * Get load average history for trend analysis
     * @param duration How far back to look
     * @return Vector of load average samples within the duration
     */
    template <typename Duration>
    std::vector<load_average_sample> get_load_history(Duration duration) const {
        if (load_history_) {
            return load_history_->get_samples(duration);
        }
        return {};
    }

    /**
     * Get load average statistics for a duration
     * @param duration How far back to look
     * @return Statistics for load averages within the duration
     */
    template <typename Duration>
    load_average_statistics get_load_statistics(Duration duration) const {
        if (load_history_) {
            return load_history_->get_statistics(duration);
        }
        return {};
    }

    /**
     * Get all load average history
     * @return Vector of all load average samples
     */
    std::vector<load_average_sample> get_all_load_history() const;

    /**
     * Get load average statistics for all history
     * @return Statistics for all load average samples
     */
    load_average_statistics get_all_load_statistics() const;

    /**
     * Configure load history buffer size
     * @param max_samples Maximum number of samples to keep (default: 1000)
     */
    void configure_load_history(size_t max_samples);

    /**
     * Check if load history tracking is enabled
     * @return true if load history tracking is enabled
     */
    bool is_load_history_enabled() const;

  private:
    // Collector implementation
    std::unique_ptr<system_info_collector> collector_;

    // Configuration
    bool collect_cpu_metrics_{true};
    bool collect_memory_metrics_{true};
    bool collect_disk_metrics_{true};
    bool collect_network_metrics_{true};
    bool collect_process_metrics_{true};

    // Statistics
    mutable std::mutex stats_mutex_;
    std::atomic<size_t> collection_count_{0};
    std::atomic<size_t> collection_errors_{0};
    std::atomic<int64_t> init_time_ns_{0};  // Store as nanoseconds for atomic access
    std::shared_ptr<system_resources> last_resources_;  // Use shared_ptr to avoid large struct copy

    // Load average history tracking
    std::unique_ptr<load_average_history> load_history_;
    bool enable_load_history_{true};

    // Helper methods
    metric create_metric(const std::string& name, double value, const std::string& unit = "",
                         const std::unordered_map<std::string, std::string>& labels = {}) const;
    void add_cpu_metrics(std::vector<metric>& metrics, const system_resources& resources);
    void add_memory_metrics(std::vector<metric>& metrics, const system_resources& resources);
    void add_disk_metrics(std::vector<metric>& metrics, const system_resources& resources);
    void add_network_metrics(std::vector<metric>& metrics, const system_resources& resources);
    void add_process_metrics(std::vector<metric>& metrics, const system_resources& resources);
};

/**
 * Resource threshold monitor
 * Monitors system resources against configured thresholds
 */
class resource_threshold_monitor {
  public:
    struct thresholds {
        double cpu_usage_warn{75.0};
        double cpu_usage_critical{90.0};
        double memory_usage_warn{80.0};
        double memory_usage_critical{95.0};
        double disk_usage_warn{85.0};
        double disk_usage_critical{95.0};
        double swap_usage_warn{50.0};
        double swap_usage_critical{80.0};
    };

    struct alert {
        enum class severity { info, warning, critical };

        std::string resource;
        severity level;
        double current_value;
        double threshold;
        std::string message;
        std::chrono::steady_clock::time_point timestamp;
    };

    explicit resource_threshold_monitor(const thresholds& config);

    /**
     * Check resources against thresholds
     * @param resources System resources to check
     * @return Vector of triggered alerts
     */
    std::vector<alert> check_thresholds(const system_resources& resources);

    /**
     * Update threshold configuration
     * @param config New threshold values
     */
    void update_thresholds(const thresholds& config);

    /**
     * Get current threshold configuration
     * @return Current thresholds
     */
    thresholds get_thresholds() const;

    /**
     * Get alert history
     * @param max_count Maximum number of alerts to return
     * @return Vector of recent alerts
     */
    std::vector<alert> get_alert_history(size_t max_count = 100) const;

    /**
     * Clear alert history
     */
    void clear_history();

  private:
    mutable std::mutex config_mutex_;
    thresholds config_;

    mutable std::mutex history_mutex_;
    std::vector<alert> alert_history_;
    const size_t max_history_size_{1000};

    void check_cpu_usage(std::vector<alert>& alerts, const system_resources& resources);
    void check_memory_usage(std::vector<alert>& alerts, const system_resources& resources);
    void check_disk_usage(std::vector<alert>& alerts, const system_resources& resources);
    void check_swap_usage(std::vector<alert>& alerts, const system_resources& resources);
    void add_alert(std::vector<alert>& alerts, const std::string& resource,
                   alert::severity level, double value, double threshold, const std::string& message);
};

} } // namespace kcenon::monitoring