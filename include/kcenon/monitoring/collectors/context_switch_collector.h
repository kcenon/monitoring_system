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
 *
 * Uses CRTP base class to reduce code duplication.
 */
class context_switch_collector : public collector_base<context_switch_collector> {
   public:
    static constexpr const char* collector_name = "context_switch_collector";

    context_switch_collector();
    ~context_switch_collector() override = default;

    context_switch_collector(const context_switch_collector&) = delete;
    context_switch_collector& operator=(const context_switch_collector&) = delete;
    context_switch_collector(context_switch_collector&&) = delete;
    context_switch_collector& operator=(context_switch_collector&&) = delete;

    // CRTP interface
    bool do_initialize(const config_map& config);
    std::vector<metric> do_collect();
    bool is_available() const;
    std::vector<std::string> do_get_metric_types() const;
    void do_add_statistics(stats_map& stats) const;

    context_switch_metrics get_last_metrics() const;
    bool is_context_switch_monitoring_available() const;

   private:
    std::unique_ptr<context_switch_info_collector> collector_;

    bool collect_process_metrics_{true};
    double rate_warning_threshold_{100000.0};
    context_switch_metrics last_metrics_;

    void add_context_switch_metrics(std::vector<metric>& metrics,
                                    const context_switch_metrics& cs_data);
};

}  // namespace monitoring
}  // namespace kcenon
