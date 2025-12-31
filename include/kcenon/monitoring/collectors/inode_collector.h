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
 * @file inode_collector.h
 * @brief Filesystem inode usage monitoring collector
 *
 * This file provides inode usage monitoring using platform-specific APIs.
 * Inode exhaustion is a common failure mode on Unix systems - a filesystem
 * can have free disk space but still fail with "No space left on device"
 * when inodes are exhausted.
 *
 * Platform APIs:
 * - Linux: statvfs() syscall, /proc/mounts for filesystem enumeration
 * - macOS: statvfs() syscall, getmntinfo() for filesystem enumeration
 * - Windows: Not applicable (NTFS uses MFT, not traditional inodes)
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

namespace kcenon {
namespace monitoring {

/**
 * @struct filesystem_inode_info
 * @brief Inode usage information for a single filesystem
 */
struct filesystem_inode_info {
    std::string mount_point;       ///< Filesystem mount point (e.g., "/", "/home")
    std::string filesystem_type;   ///< Filesystem type (e.g., "ext4", "apfs")
    std::string device;            ///< Device path (e.g., "/dev/sda1")
    uint64_t inodes_total{0};      ///< Total inodes on filesystem
    uint64_t inodes_used{0};       ///< Used inodes
    uint64_t inodes_free{0};       ///< Free inodes
    double inodes_usage_percent{0.0};  ///< Percentage of inodes used
};

/**
 * @struct inode_metrics
 * @brief Aggregated inode usage metrics for all filesystems
 */
struct inode_metrics {
    std::vector<filesystem_inode_info> filesystems;  ///< Per-filesystem inode info
    uint64_t total_inodes{0};                        ///< Sum of all filesystem inodes
    uint64_t total_inodes_used{0};                   ///< Sum of all used inodes
    uint64_t total_inodes_free{0};                   ///< Sum of all free inodes
    double average_usage_percent{0.0};               ///< Average usage across filesystems
    double max_usage_percent{0.0};                   ///< Maximum usage among filesystems
    std::string max_usage_mount_point;               ///< Mount point with highest usage
    bool metrics_available{false};                   ///< Whether inode metrics are available
    std::chrono::system_clock::time_point timestamp; ///< Reading timestamp
};

// Forward declaration
namespace platform {
class metrics_provider;
}  // namespace platform

/**
 * @class inode_info_collector
 * @brief Inode data collector using platform abstraction layer
 *
 * This class provides inode data collection using the unified
 * metrics_provider interface, eliminating platform-specific code.
 */
class inode_info_collector {
   public:
    inode_info_collector();
    ~inode_info_collector();

    // Non-copyable, non-moveable due to internal state
    inode_info_collector(const inode_info_collector&) = delete;
    inode_info_collector& operator=(const inode_info_collector&) = delete;
    inode_info_collector(inode_info_collector&&) = delete;
    inode_info_collector& operator=(inode_info_collector&&) = delete;

    /**
     * Check if inode monitoring is available on this system
     * @return True if inode metrics can be read
     */
    bool is_inode_monitoring_available() const;

    /**
     * Collect current inode metrics from all filesystems
     * @return inode_metrics structure with current values
     */
    inode_metrics collect_metrics();

   private:
    std::unique_ptr<platform::metrics_provider> provider_;
};

/**
 * @class inode_collector
 * @brief Filesystem inode usage monitoring collector
 *
 * Collects inode usage metrics with cross-platform support.
 * Returns empty/unavailable metrics on Windows since NTFS
 * uses MFT instead of traditional inodes.
 */
class inode_collector {
   public:
    inode_collector();
    ~inode_collector() = default;

    // Non-copyable, non-moveable due to internal state
    inode_collector(const inode_collector&) = delete;
    inode_collector& operator=(const inode_collector&) = delete;
    inode_collector(inode_collector&&) = delete;
    inode_collector& operator=(inode_collector&&) = delete;

    /**
     * Initialize the collector with configuration
     * @param config Configuration options:
     *   - "enabled": "true"/"false" (default: true)
     *   - "warning_threshold": percentage (default: 80.0)
     *   - "critical_threshold": percentage (default: 95.0)
     *   - "include_pseudo_fs": "true"/"false" (default: false)
     * @return true if initialization successful
     */
    bool initialize(const std::unordered_map<std::string, std::string>& config);

    /**
     * Collect inode usage metrics
     * @return Vector of collected metrics
     */
    std::vector<metric> collect();

    /**
     * Get the name of this collector
     * @return Collector name
     */
    std::string get_name() const { return "inode_collector"; }

    /**
     * Get supported metric types
     * @return Vector of supported metric type names
     */
    std::vector<std::string> get_metric_types() const;

    /**
     * Check if the collector is healthy
     * @return true if collector is operational
     */
    bool is_healthy() const;

    /**
     * Get collector statistics
     * @return Map of statistic name to value
     */
    std::unordered_map<std::string, double> get_statistics() const;

    /**
     * Get last collected inode metrics
     * @return Most recent inode_metrics reading
     */
    inode_metrics get_last_metrics() const;

    /**
     * Check if inode monitoring is available
     * @return True if inode metrics are accessible
     */
    bool is_inode_monitoring_available() const;

   private:
    std::unique_ptr<inode_info_collector> collector_;

    // Configuration
    bool enabled_{true};
    bool include_pseudo_fs_{false};
    double warning_threshold_{80.0};
    double critical_threshold_{95.0};

    // Statistics
    mutable std::mutex stats_mutex_;
    std::atomic<size_t> collection_count_{0};
    std::atomic<size_t> collection_errors_{0};
    inode_metrics last_metrics_;

    // Helper methods
    metric create_metric(const std::string& name, double value,
                         const std::unordered_map<std::string, std::string>& tags = {},
                         const std::string& unit = "") const;
    void add_inode_metrics(std::vector<metric>& metrics, const inode_metrics& inode_data);
};

}  // namespace monitoring
}  // namespace kcenon
