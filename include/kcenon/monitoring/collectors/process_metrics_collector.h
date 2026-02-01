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
 * @file process_metrics_collector.h
 * @brief Unified process-level metrics collector
 *
 * This file consolidates file descriptor, inode, and context switch monitoring
 * into a single collector for comprehensive process-level monitoring.
 *
 * Consolidates:
 * - fd_collector: File descriptor usage monitoring
 * - inode_collector: Filesystem inode monitoring
 * - context_switch_collector: Context switch statistics
 *
 * Platform APIs:
 * - Linux: /proc filesystem, statvfs(), getrlimit()
 * - macOS: statvfs(), getmntinfo(), host_statistics()
 * - Windows: GetProcessHandleCount() (partial support)
 */

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "../interfaces/metric_types_adapter.h"
#include "../plugins/collector_plugin.h"

namespace kcenon {
namespace monitoring {

// =============================================================================
// Metrics structures
// =============================================================================

/**
 * @struct fd_metrics
 * @brief File descriptor usage metrics
 */
struct fd_metrics {
    uint64_t fd_used_system{0};
    uint64_t fd_max_system{0};
    uint64_t fd_used_process{0};
    uint64_t fd_soft_limit{0};
    uint64_t fd_hard_limit{0};
    double fd_usage_percent{0.0};
    bool system_metrics_available{false};
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @struct filesystem_inode_info
 * @brief Inode usage information for a single filesystem
 */
struct filesystem_inode_info {
    std::string mount_point;
    std::string filesystem_type;
    std::string device;
    uint64_t inodes_total{0};
    uint64_t inodes_used{0};
    uint64_t inodes_free{0};
    double inodes_usage_percent{0.0};
};

/**
 * @struct inode_metrics
 * @brief Aggregated inode usage metrics for all filesystems
 */
struct inode_metrics {
    std::vector<filesystem_inode_info> filesystems;
    uint64_t total_inodes{0};
    uint64_t total_inodes_used{0};
    uint64_t total_inodes_free{0};
    double average_usage_percent{0.0};
    double max_usage_percent{0.0};
    std::string max_usage_mount_point;
    bool metrics_available{false};
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @struct process_context_switch_info
 * @brief Context switch information for the current process
 */
struct process_context_switch_info {
    uint64_t voluntary_switches{0};
    uint64_t nonvoluntary_switches{0};
    uint64_t total_switches{0};
};

/**
 * @struct context_switch_metrics
 * @brief Aggregated context switch metrics for system and process
 */
struct context_switch_metrics {
    uint64_t system_context_switches_total{0};
    double context_switches_per_sec{0.0};
    process_context_switch_info process_info;
    bool metrics_available{false};
    bool rate_available{false};
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @struct process_metrics_config
 * @brief Configuration for selective metric collection
 */
struct process_metrics_config {
    bool collect_fd{true};
    bool collect_inodes{true};
    bool collect_context_switches{true};
    bool include_pseudo_fs{false};
    double fd_warning_threshold{80.0};
    double fd_critical_threshold{95.0};
    double inode_warning_threshold{80.0};
    double inode_critical_threshold{95.0};
    double context_switch_rate_warning{100000.0};
};

/**
 * @struct process_metrics
 * @brief Combined process-level metrics
 */
struct process_metrics {
    fd_metrics fd;
    inode_metrics inodes;
    context_switch_metrics context_switches;
    std::chrono::system_clock::time_point timestamp;
};

// =============================================================================
// Forward declarations
// =============================================================================

namespace platform {
class metrics_provider;
}

// =============================================================================
// Info collectors (internal implementation)
// =============================================================================

/**
 * @class fd_info_collector
 * @brief File descriptor data collector using platform abstraction layer
 */
class fd_info_collector {
   public:
    fd_info_collector();
    ~fd_info_collector();

    fd_info_collector(const fd_info_collector&) = delete;
    fd_info_collector& operator=(const fd_info_collector&) = delete;
    fd_info_collector(fd_info_collector&&) = delete;
    fd_info_collector& operator=(fd_info_collector&&) = delete;

    bool is_fd_monitoring_available() const;
    fd_metrics collect_metrics();

   private:
    std::unique_ptr<platform::metrics_provider> provider_;
};

/**
 * @class inode_info_collector
 * @brief Inode data collector using platform abstraction layer
 */
class inode_info_collector {
   public:
    inode_info_collector();
    ~inode_info_collector();

    inode_info_collector(const inode_info_collector&) = delete;
    inode_info_collector& operator=(const inode_info_collector&) = delete;
    inode_info_collector(inode_info_collector&&) = delete;
    inode_info_collector& operator=(inode_info_collector&&) = delete;

    bool is_inode_monitoring_available() const;
    inode_metrics collect_metrics();

   private:
    std::unique_ptr<platform::metrics_provider> provider_;
};

/**
 * @class context_switch_info_collector
 * @brief Context switch data collector using platform abstraction layer
 */
class context_switch_info_collector {
   public:
    context_switch_info_collector();
    ~context_switch_info_collector();

    context_switch_info_collector(const context_switch_info_collector&) = delete;
    context_switch_info_collector& operator=(const context_switch_info_collector&) = delete;
    context_switch_info_collector(context_switch_info_collector&&) = delete;
    context_switch_info_collector& operator=(context_switch_info_collector&&) = delete;

    bool is_context_switch_monitoring_available() const;
    context_switch_metrics collect_metrics();

   private:
    std::unique_ptr<platform::metrics_provider> provider_;
    uint64_t last_system_switches_{0};
    std::chrono::steady_clock::time_point last_collection_time_;
    bool has_previous_sample_{false};
    double calculate_rate(uint64_t current_switches);
};

// =============================================================================
// Main collector
// =============================================================================

/**
 * @class process_metrics_collector
 * @brief Unified process-level metrics collector
 *
 * Consolidates file descriptor, inode, and context switch monitoring into
 * a single collector for comprehensive process health monitoring.
 *
 * Configuration options:
 * - "collect_fd": "true"/"false" - Enable FD collection (default: true)
 * - "collect_inodes": "true"/"false" - Enable inode collection (default: true)
 * - "collect_context_switches": "true"/"false" - Enable context switch collection (default: true)
 * - "include_pseudo_fs": "true"/"false" - Include pseudo filesystems (default: false)
 * - "fd_warning_threshold": percentage (default: 80.0)
 * - "fd_critical_threshold": percentage (default: 95.0)
 * - "inode_warning_threshold": percentage (default: 80.0)
 * - "inode_critical_threshold": percentage (default: 95.0)
 * - "context_switch_rate_warning": rate (default: 100000.0)
 */
class process_metrics_collector : public collector_plugin {
   public:
    process_metrics_collector();
    explicit process_metrics_collector(process_metrics_config config);
    ~process_metrics_collector() override = default;

    process_metrics_collector(const process_metrics_collector&) = delete;
    process_metrics_collector& operator=(const process_metrics_collector&) = delete;
    process_metrics_collector(process_metrics_collector&&) = delete;
    process_metrics_collector& operator=(process_metrics_collector&&) = delete;

    // collector_plugin interface implementation
    auto name() const -> std::string_view override { return "process_metrics_collector"; }
    auto collect() -> std::vector<metric> override;
    auto interval() const -> std::chrono::milliseconds override { return collection_interval_; }
    auto is_available() const -> bool override;
    /**
     * Check if collector is in a healthy state
     * @return True if collector is operational
     */
    bool is_healthy() const { return is_available(); }
    auto get_metric_types() const -> std::vector<std::string> override;

    // Configuration
    bool initialize(const config_map& config) override;

    /**
     * Get collector statistics
     * @return Map of statistics
     */
    auto get_statistics() const -> stats_map override;

    // Accessors
    process_metrics get_last_metrics() const;
    fd_metrics get_last_fd_metrics() const;
    inode_metrics get_last_inode_metrics() const;
    context_switch_metrics get_last_context_switch_metrics() const;

    // Availability checks
    bool is_fd_monitoring_available() const;
    bool is_inode_monitoring_available() const;
    bool is_context_switch_monitoring_available() const;

   private:
    std::unique_ptr<fd_info_collector> fd_collector_;
    std::unique_ptr<inode_info_collector> inode_collector_;
    std::unique_ptr<context_switch_info_collector> cs_collector_;

    process_metrics_config config_;
    process_metrics last_metrics_;
    std::chrono::milliseconds collection_interval_{std::chrono::seconds(5)};
    mutable std::mutex metrics_mutex_;

    void collect_fd_metrics(std::vector<metric>& metrics);
    void collect_inode_metrics(std::vector<metric>& metrics);
    void collect_context_switch_metrics(std::vector<metric>& metrics);

    void add_fd_metrics(std::vector<metric>& metrics, const fd_metrics& fd_data);
    void add_inode_metrics(std::vector<metric>& metrics, const inode_metrics& inode_data);
    void add_context_switch_metrics(std::vector<metric>& metrics,
                                    const context_switch_metrics& cs_data);
};

}  // namespace monitoring
}  // namespace kcenon
