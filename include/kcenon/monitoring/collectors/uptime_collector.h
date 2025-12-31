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
 * @file uptime_collector.h
 * @brief System uptime monitoring collector
 *
 * This file provides system uptime monitoring using platform-specific APIs.
 * Tracks boot time, uptime duration, and system availability for SLA compliance
 * and stability analysis.
 *
 * Platform APIs:
 * - Linux: /proc/uptime or sysinfo() syscall
 * - macOS: sysctl(KERN_BOOTTIME)
 * - Windows: GetTickCount64()
 */

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "../interfaces/metric_types_adapter.h"
#include "collector_base.h"

namespace kcenon {
namespace monitoring {

/**
 * @struct uptime_metrics
 * @brief Aggregated uptime metrics for the system
 */
struct uptime_metrics {
    double uptime_seconds{0.0};         ///< Time since boot in seconds (gauge)
    int64_t boot_timestamp{0};          ///< Unix timestamp of last boot (gauge)
    double idle_seconds{0.0};           ///< Total idle time in seconds (Linux only)

    bool metrics_available{false};      ///< Whether metrics are available
    std::chrono::system_clock::time_point timestamp;  ///< Reading timestamp
};

// Forward declaration
namespace platform {
class metrics_provider;
}  // namespace platform

/**
 * @class uptime_info_collector
 * @brief Uptime data collector using platform abstraction layer
 *
 * This class provides uptime data collection using the unified
 * metrics_provider interface, eliminating platform-specific code.
 */
class uptime_info_collector {
   public:
    uptime_info_collector();
    ~uptime_info_collector();

    // Non-copyable, non-moveable due to internal state
    uptime_info_collector(const uptime_info_collector&) = delete;
    uptime_info_collector& operator=(const uptime_info_collector&) = delete;
    uptime_info_collector(uptime_info_collector&&) = delete;
    uptime_info_collector& operator=(uptime_info_collector&&) = delete;

    /**
     * Check if uptime monitoring is available on this system
     * @return True if uptime metrics can be read
     */
    bool is_uptime_monitoring_available() const;

    /**
     * Collect current uptime metrics
     * @return uptime_metrics structure with current values
     */
    uptime_metrics collect_metrics();

   private:
    std::unique_ptr<platform::metrics_provider> provider_;
};

/**
 * @class uptime_collector
 * @brief System uptime monitoring collector
 *
 * Collects system uptime metrics with cross-platform support.
 * Provides boot timestamp and uptime duration for availability tracking.
 *
 * Uses CRTP base class to reduce code duplication.
 */
class uptime_collector : public collector_base<uptime_collector> {
   public:
    /// Collector name for CRTP base class
    static constexpr const char* collector_name = "uptime_collector";

    uptime_collector();
    ~uptime_collector() override = default;

    // Non-copyable, non-moveable due to internal state
    uptime_collector(const uptime_collector&) = delete;
    uptime_collector& operator=(const uptime_collector&) = delete;
    uptime_collector(uptime_collector&&) = delete;
    uptime_collector& operator=(uptime_collector&&) = delete;

    // CRTP interface implementation
    /**
     * Collector-specific initialization
     * @param config Configuration options:
     *   - "collect_idle_time": "true"/"false" (default: true, Linux only)
     * @return true if initialization successful
     */
    bool do_initialize(const config_map& config);

    /**
     * Collect uptime metrics
     * @return Vector of collected metrics
     */
    std::vector<metric> do_collect();

    /**
     * Check if uptime monitoring is available
     * @return True if uptime metrics are accessible
     */
    bool is_available() const;

    /**
     * Get supported metric types
     * @return Vector of supported metric type names
     */
    std::vector<std::string> do_get_metric_types() const;

    /**
     * Add collector-specific statistics
     * @param stats Map to add statistics to
     */
    void do_add_statistics(stats_map& stats) const;

    /**
     * Get last collected uptime metrics
     * @return Most recent uptime_metrics reading
     */
    uptime_metrics get_last_metrics() const;

    /**
     * Check if uptime monitoring is available
     * @return True if uptime metrics are accessible
     */
    bool is_uptime_monitoring_available() const;

   private:
    std::unique_ptr<uptime_info_collector> collector_;

    // Configuration
    bool collect_idle_time_{true};

    // Last metrics cache
    uptime_metrics last_metrics_;

    // Helper methods
    void add_uptime_metrics(std::vector<metric>& metrics,
                            const uptime_metrics& uptime_data);
};

}  // namespace monitoring
}  // namespace kcenon
