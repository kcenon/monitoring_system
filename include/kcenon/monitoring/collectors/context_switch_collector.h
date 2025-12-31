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
 * @file context_switch_collector.h
 * @brief Context switch statistics monitoring collector
 *
 * This file provides context switch monitoring using platform-specific APIs.
 * Excessive context switching indicates CPU contention or poor thread pool
 * sizing, and monitoring enables scheduling analysis and performance tuning.
 *
 * Platform APIs:
 * - Linux: /proc/stat (ctxt field), /proc/self/status (voluntary/nonvoluntary)
 * - macOS: host_statistics() with HOST_CPU_LOAD_INFO
 * - Windows: Performance counters (stub implementation)
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
 * @struct process_context_switch_info
 * @brief Context switch information for the current process
 */
struct process_context_switch_info {
    uint64_t voluntary_switches{0};      ///< Voluntary context switches (I/O wait, sleep)
    uint64_t nonvoluntary_switches{0};   ///< Involuntary context switches (preemption)
    uint64_t total_switches{0};          ///< Total process context switches
};

/**
 * @struct context_switch_metrics
 * @brief Aggregated context switch metrics for system and process
 */
struct context_switch_metrics {
    // System-wide metrics
    uint64_t system_context_switches_total{0};  ///< Total system context switches (counter)
    double context_switches_per_sec{0.0};       ///< Context switch rate (gauge)
    
    // Process-level metrics
    process_context_switch_info process_info;   ///< Current process context switch info
    
    // Metadata
    bool metrics_available{false};              ///< Whether metrics are available
    bool rate_available{false};                 ///< Whether rate calculation is available
    std::chrono::system_clock::time_point timestamp;  ///< Reading timestamp
};

// Forward declaration
namespace platform {
class metrics_provider;
}  // namespace platform

/**
 * @class context_switch_info_collector
 * @brief Context switch data collector using platform abstraction layer
 *
 * This class provides context switch data collection using the unified
 * metrics_provider interface, eliminating platform-specific code.
 */
class context_switch_info_collector {
   public:
    context_switch_info_collector();
    ~context_switch_info_collector();

    // Non-copyable, non-moveable due to internal state
    context_switch_info_collector(const context_switch_info_collector&) = delete;
    context_switch_info_collector& operator=(const context_switch_info_collector&) = delete;
    context_switch_info_collector(context_switch_info_collector&&) = delete;
    context_switch_info_collector& operator=(context_switch_info_collector&&) = delete;

    /**
     * Check if context switch monitoring is available on this system
     * @return True if context switch metrics can be read
     */
    bool is_context_switch_monitoring_available() const;

    /**
     * Collect current context switch metrics
     * @return context_switch_metrics structure with current values
     */
    context_switch_metrics collect_metrics();

   private:
    std::unique_ptr<platform::metrics_provider> provider_;

    // For rate calculation
    uint64_t last_system_switches_{0};
    std::chrono::steady_clock::time_point last_collection_time_;
    bool has_previous_sample_{false};

    // Rate calculation
    double calculate_rate(uint64_t current_switches);
};

/**
 * @class context_switch_collector
 * @brief Context switch statistics monitoring collector
 *
 * Collects context switch metrics with cross-platform support.
 * Returns empty/unavailable metrics on Windows (stub implementation).
 */
class context_switch_collector {
   public:
    context_switch_collector();
    ~context_switch_collector() = default;

    // Non-copyable, non-moveable due to internal state
    context_switch_collector(const context_switch_collector&) = delete;
    context_switch_collector& operator=(const context_switch_collector&) = delete;
    context_switch_collector(context_switch_collector&&) = delete;
    context_switch_collector& operator=(context_switch_collector&&) = delete;

    /**
     * Initialize the collector with configuration
     * @param config Configuration options:
     *   - "enabled": "true"/"false" (default: true)
     *   - "collect_process_metrics": "true"/"false" (default: true)
     *   - "rate_warning_threshold": switches/sec (default: 100000)
     * @return true if initialization successful
     */
    bool initialize(const std::unordered_map<std::string, std::string>& config);

    /**
     * Collect context switch metrics
     * @return Vector of collected metrics
     */
    std::vector<metric> collect();

    /**
     * Get the name of this collector
     * @return Collector name
     */
    std::string get_name() const { return "context_switch_collector"; }

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
     * Get last collected context switch metrics
     * @return Most recent context_switch_metrics reading
     */
    context_switch_metrics get_last_metrics() const;

    /**
     * Check if context switch monitoring is available
     * @return True if context switch metrics are accessible
     */
    bool is_context_switch_monitoring_available() const;

   private:
    std::unique_ptr<context_switch_info_collector> collector_;

    // Configuration
    bool enabled_{true};
    bool collect_process_metrics_{true};
    double rate_warning_threshold_{100000.0};

    // Statistics
    mutable std::mutex stats_mutex_;
    std::atomic<size_t> collection_count_{0};
    std::atomic<size_t> collection_errors_{0};
    context_switch_metrics last_metrics_;

    // Helper methods
    metric create_metric(const std::string& name, double value,
                         const std::unordered_map<std::string, std::string>& tags = {},
                         const std::string& unit = "") const;
    void add_context_switch_metrics(std::vector<metric>& metrics, 
                                    const context_switch_metrics& cs_data);
};

}  // namespace monitoring
}  // namespace kcenon
