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

/**
 * @file container_collector.h
 * @brief Container metrics collector for Docker/cgroup monitoring
 *
 * This file provides container-level metrics collection supporting:
 * - Linux cgroups v1/v2
 * - Docker API (optional)
 */

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "../interfaces/metric_types_adapter.h"
#include "../plugins/collector_plugin.h"

namespace kcenon {
namespace monitoring {

/**
 * Container metrics structure containing per-container resource usage
 */
struct container_metrics {
    // Container identification
    std::string container_id;    ///< Short container ID
    std::string container_name;  ///< Container name (Docker only)
    std::string image_name;      ///< Image name (Docker only)

    // CPU metrics
    double cpu_usage_percent{0.0};  ///< CPU utilization percentage
    uint64_t cpu_usage_ns{0};       ///< Total CPU time in nanoseconds

    // Memory metrics
    uint64_t memory_usage_bytes{0};    ///< Current memory usage in bytes
    uint64_t memory_limit_bytes{0};    ///< Memory limit in bytes
    double memory_usage_percent{0.0};  ///< Memory usage percentage

    // Network metrics (counters)
    uint64_t network_rx_bytes{0};  ///< Total bytes received
    uint64_t network_tx_bytes{0};  ///< Total bytes transmitted

    // Block I/O metrics (counters)
    uint64_t blkio_read_bytes{0};   ///< Total bytes read from disk
    uint64_t blkio_write_bytes{0};  ///< Total bytes written to disk

    // Process metrics
    uint64_t pids_current{0};  ///< Current number of processes
    uint64_t pids_limit{0};    ///< Process limit (0 = unlimited)

    // Timestamp
    std::chrono::system_clock::time_point timestamp;
};

/**
 * Container info collected from cgroups or Docker API
 */
struct container_info {
    std::string container_id;
    std::string container_name;
    std::string image_name;
    std::string cgroup_path;  ///< Path to cgroup directory
    bool is_running{false};
};

/**
 * Cgroup version detection
 */
enum class cgroup_version : uint8_t {
    none = 0,  ///< Not in a cgroup or not Linux
    v1 = 1,    ///< Legacy cgroups v1
    v2 = 2     ///< Unified cgroups v2 hierarchy
};

/**
 * Platform-specific container info collector implementation
 */
class container_info_collector {
   public:
    container_info_collector();
    ~container_info_collector();

    // Non-copyable, non-moveable due to internal state
    container_info_collector(const container_info_collector&) = delete;
    container_info_collector& operator=(const container_info_collector&) = delete;
    container_info_collector(container_info_collector&&) = delete;
    container_info_collector& operator=(container_info_collector&&) = delete;

    /**
     * Detect which cgroup version is available
     * @return Detected cgroup version
     */
    cgroup_version detect_cgroup_version() const;

    /**
     * Enumerate all containers
     * @return Vector of container info
     */
    std::vector<container_info> enumerate_containers();

    /**
     * Collect metrics for a specific container
     * @param info Container info
     * @return Container metrics
     */
    container_metrics collect_container_metrics(const container_info& info);

    /**
     * Check if running inside a container
     * @return True if running inside a container
     */
    bool is_containerized() const;

   private:
    mutable std::mutex mutex_;
    cgroup_version cached_version_{cgroup_version::none};
    bool version_detected_{false};

    // Previous CPU stats for calculating usage percentage
    struct cpu_stats {
        uint64_t usage_ns{0};
        std::chrono::steady_clock::time_point timestamp;
    };
    std::unordered_map<std::string, cpu_stats> prev_cpu_stats_;

    // Platform-specific methods
#if defined(__linux__)
    cgroup_version detect_cgroup_version_linux() const;
    std::vector<container_info> enumerate_containers_cgroup_v2();
    std::vector<container_info> enumerate_containers_cgroup_v1();
    container_metrics collect_metrics_cgroup_v2(const container_info& info);
    container_metrics collect_metrics_cgroup_v1(const container_info& info);

    // Helper methods for cgroup parsing
    uint64_t read_cgroup_value(const std::string& path, const std::string& key = "") const;
    std::unordered_map<std::string, uint64_t> read_cgroup_stat(const std::string& path) const;
#endif
};

/**
 * Container metrics collector implementing the collector_plugin interface
 * Collects metrics from Docker containers and cgroups
 */
class container_collector : public collector_plugin {
   public:
    container_collector();
    ~container_collector() override = default;

    // Non-copyable, non-moveable due to internal state
    container_collector(const container_collector&) = delete;
    container_collector& operator=(const container_collector&) = delete;
    container_collector(container_collector&&) = delete;
    container_collector& operator=(container_collector&&) = delete;

    // collector_plugin implementation
    auto name() const -> std::string_view override { return "container"; }
    auto collect() -> std::vector<metric> override;
    auto interval() const -> std::chrono::milliseconds override { return std::chrono::seconds(10); }
    auto is_available() const -> bool override;
    auto get_metric_types() const -> std::vector<std::string> override;

    auto get_metadata() const -> plugin_metadata override {
        return plugin_metadata{
            .name = name(),
            .description = "Container metrics from Docker and cgroups",
            .category = plugin_category::system,
            .version = "1.0.0",
            .dependencies = {},
            .requires_platform_support = true  // Linux-specific
        };
    }

    auto initialize(const config_map& config) -> bool override;
    void shutdown() override {}
    auto get_statistics() const -> stats_map override;

    // Legacy compatibility (deprecated)
    bool is_healthy() const;

    /**
     * Get last collected container metrics
     * @return Vector of container metrics
     */
    std::vector<container_metrics> get_last_metrics() const;

    /**
     * Check if container metrics are available
     * @return True if running in a containerized environment
     */
    bool is_container_environment() const;

   private:
    std::unique_ptr<container_info_collector> collector_;

    // Configuration
    bool enabled_{true};
    bool collect_network_metrics_{true};
    bool collect_blkio_metrics_{true};

    // Statistics
    mutable std::mutex stats_mutex_;
    std::atomic<size_t> collection_count_{0};
    std::atomic<size_t> collection_errors_{0};
    std::atomic<size_t> containers_found_{0};
    std::vector<container_metrics> last_metrics_;

    // Helper methods
    metric create_metric(const std::string& name, double value, const container_metrics& container,
                         const std::string& unit = "") const;
    void add_container_metrics(std::vector<metric>& metrics, const container_metrics& container);
};

}  // namespace monitoring
}  // namespace kcenon
