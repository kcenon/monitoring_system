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
 * @file fd_collector.h
 * @brief File descriptor usage monitoring collector
 *
 * This file provides file descriptor (FD) usage monitoring using platform-specific
 * APIs to gather FD utilization data. FD exhaustion is a common failure mode in
 * server applications, and monitoring helps detect leaks early.
 *
 * Platform APIs:
 * - Linux: /proc/sys/fs/file-nr, /proc/self/fd/, /proc/self/limits
 * - macOS: getrlimit(RLIMIT_NOFILE), /dev/fd/ directory
 * - Windows: GetProcessHandleCount() (handles instead of FDs)
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
 * @struct fd_metrics
 * @brief File descriptor usage metrics
 */
struct fd_metrics {
    uint64_t fd_used_system{0};                       ///< Total system FDs in use (Linux only)
    uint64_t fd_max_system{0};                        ///< System FD limit (Linux only)
    uint64_t fd_used_process{0};                      ///< Current process FD count
    uint64_t fd_soft_limit{0};                        ///< Process FD soft limit
    uint64_t fd_hard_limit{0};                        ///< Process FD hard limit
    double fd_usage_percent{0.0};                     ///< Percentage of soft limit used
    bool system_metrics_available{false};             ///< Whether system-wide metrics are available
    std::chrono::system_clock::time_point timestamp;  ///< Reading timestamp
};

// Forward declaration
namespace platform {
class metrics_provider;
}  // namespace platform

/**
 * @class fd_info_collector
 * @brief File descriptor data collector using platform abstraction layer
 *
 * This class provides FD data collection using the unified
 * metrics_provider interface, eliminating platform-specific code.
 */
class fd_info_collector {
   public:
    fd_info_collector();
    ~fd_info_collector();

    // Non-copyable, non-moveable due to internal state
    fd_info_collector(const fd_info_collector&) = delete;
    fd_info_collector& operator=(const fd_info_collector&) = delete;
    fd_info_collector(fd_info_collector&&) = delete;
    fd_info_collector& operator=(fd_info_collector&&) = delete;

    /**
     * Check if FD monitoring is available on this system
     * @return True if FD metrics can be read
     */
    bool is_fd_monitoring_available() const;

    /**
     * Collect current FD metrics
     * @return fd_metrics structure with current values
     */
    fd_metrics collect_metrics();

   private:
    std::unique_ptr<platform::metrics_provider> provider_;
};

/**
 * @class fd_collector
 * @brief File descriptor usage monitoring collector
 *
 * Collects file descriptor usage metrics with cross-platform support.
 * Gracefully degrades when certain metrics are not available on the
 * current platform (e.g., system-wide FD count on macOS/Windows).
 */
class fd_collector {
   public:
    fd_collector();
    ~fd_collector() = default;

    // Non-copyable, non-moveable due to internal state
    fd_collector(const fd_collector&) = delete;
    fd_collector& operator=(const fd_collector&) = delete;
    fd_collector(fd_collector&&) = delete;
    fd_collector& operator=(fd_collector&&) = delete;

    /**
     * Initialize the collector with configuration
     * @param config Configuration options:
     *   - "enabled": "true"/"false" (default: true)
     *   - "warning_threshold": percentage (default: 80.0)
     *   - "critical_threshold": percentage (default: 95.0)
     * @return true if initialization successful
     */
    bool initialize(const std::unordered_map<std::string, std::string>& config);

    /**
     * Collect FD usage metrics
     * @return Vector of collected metrics
     */
    std::vector<metric> collect();

    /**
     * Get the name of this collector
     * @return Collector name
     */
    std::string get_name() const { return "fd_collector"; }

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
     * Get last collected FD metrics
     * @return Most recent fd_metrics reading
     */
    fd_metrics get_last_metrics() const;

    /**
     * Check if FD monitoring is available
     * @return True if FD metrics are accessible
     */
    bool is_fd_monitoring_available() const;

   private:
    std::unique_ptr<fd_info_collector> collector_;

    // Configuration
    bool enabled_{true};
    double warning_threshold_{80.0};
    double critical_threshold_{95.0};

    // Statistics
    mutable std::mutex stats_mutex_;
    std::atomic<size_t> collection_count_{0};
    std::atomic<size_t> collection_errors_{0};
    fd_metrics last_metrics_;

    // Helper methods
    metric create_metric(const std::string& name, double value,
                         const std::unordered_map<std::string, std::string>& tags = {},
                         const std::string& unit = "") const;
    void add_fd_metrics(std::vector<metric>& metrics, const fd_metrics& fd_data);
};

}  // namespace monitoring
}  // namespace kcenon
