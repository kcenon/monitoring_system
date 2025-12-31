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
 * @file interrupt_collector.h
 * @brief Hardware and software interrupt statistics monitoring collector
 *
 * This file provides interrupt statistics monitoring using platform-specific APIs.
 * Interrupt monitoring is essential for diagnosing hardware-related performance issues,
 * detecting interrupt storms, and analyzing IRQ balancing problems.
 *
 * Platform APIs:
 * - Linux: /proc/stat (intr line), /proc/softirqs for soft interrupt breakdown
 * - macOS: host_statistics() for basic interrupt counts
 * - Windows: Not implemented (future: Performance counters)
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
 * @struct cpu_interrupt_info
 * @brief Per-CPU interrupt statistics
 */
struct cpu_interrupt_info {
    uint32_t cpu_id{0};              ///< CPU identifier
    uint64_t interrupt_count{0};     ///< Total interrupts on this CPU
    double interrupts_per_sec{0.0};  ///< Interrupt rate on this CPU
};

/**
 * @struct interrupt_metrics
 * @brief Aggregated interrupt statistics for the system
 */
struct interrupt_metrics {
    uint64_t interrupts_total{0};          ///< Total hardware interrupt count
    double interrupts_per_sec{0.0};        ///< Hardware interrupt rate (gauge)
    uint64_t soft_interrupts_total{0};     ///< Total soft interrupts (Linux only)
    double soft_interrupts_per_sec{0.0};   ///< Soft interrupt rate (Linux only)
    std::vector<cpu_interrupt_info> per_cpu;  ///< Per-CPU breakdown (optional)
    bool metrics_available{false};         ///< Whether interrupt metrics are available
    bool soft_interrupts_available{false}; ///< Whether soft interrupt metrics are available
    std::chrono::system_clock::time_point timestamp;  ///< Reading timestamp
};

// Forward declaration
namespace platform {
class metrics_provider;
}  // namespace platform

/**
 * @class interrupt_info_collector
 * @brief Interrupt data collector using platform abstraction layer
 *
 * This class provides interrupt data collection using the unified
 * metrics_provider interface, eliminating platform-specific code.
 */
class interrupt_info_collector {
   public:
    interrupt_info_collector();
    ~interrupt_info_collector();

    // Non-copyable, non-moveable due to internal state
    interrupt_info_collector(const interrupt_info_collector&) = delete;
    interrupt_info_collector& operator=(const interrupt_info_collector&) = delete;
    interrupt_info_collector(interrupt_info_collector&&) = delete;
    interrupt_info_collector& operator=(interrupt_info_collector&&) = delete;

    /**
     * Check if interrupt monitoring is available on this system
     * @return True if interrupt metrics can be read
     */
    bool is_interrupt_monitoring_available() const;

    /**
     * Collect current interrupt metrics
     * @return interrupt_metrics structure with current values
     */
    interrupt_metrics collect_metrics();

   private:
    std::unique_ptr<platform::metrics_provider> provider_;

    // Previous sample for rate calculation
    uint64_t prev_interrupts_total_{0};
    uint64_t prev_soft_interrupts_total_{0};
    std::chrono::system_clock::time_point prev_timestamp_;
    bool has_previous_sample_{false};
};

/**
 * @class interrupt_collector
 * @brief Hardware and software interrupt statistics monitoring collector
 *
 * Collects interrupt statistics with cross-platform support.
 * Provides interrupt counts and rates for diagnosing hardware-related
 * performance issues and interrupt storms.
 */
class interrupt_collector {
   public:
    interrupt_collector();
    ~interrupt_collector() = default;

    // Non-copyable, non-moveable due to internal state
    interrupt_collector(const interrupt_collector&) = delete;
    interrupt_collector& operator=(const interrupt_collector&) = delete;
    interrupt_collector(interrupt_collector&&) = delete;
    interrupt_collector& operator=(interrupt_collector&&) = delete;

    /**
     * Initialize the collector with configuration
     * @param config Configuration options:
     *   - "enabled": "true"/"false" (default: true)
     *   - "collect_per_cpu": "true"/"false" (default: false)
     *   - "collect_soft_interrupts": "true"/"false" (default: true)
     * @return true if initialization successful
     */
    bool initialize(const std::unordered_map<std::string, std::string>& config);

    /**
     * Collect interrupt statistics metrics
     * @return Vector of collected metrics
     */
    std::vector<metric> collect();

    /**
     * Get the name of this collector
     * @return Collector name
     */
    std::string get_name() const { return "interrupt_collector"; }

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
     * Get last collected interrupt metrics
     * @return Most recent interrupt_metrics reading
     */
    interrupt_metrics get_last_metrics() const;

    /**
     * Check if interrupt monitoring is available
     * @return True if interrupt metrics are accessible
     */
    bool is_interrupt_monitoring_available() const;

   private:
    std::unique_ptr<interrupt_info_collector> collector_;

    // Configuration
    bool enabled_{true};
    bool collect_per_cpu_{false};
    bool collect_soft_interrupts_{true};

    // Statistics
    mutable std::mutex stats_mutex_;
    std::atomic<size_t> collection_count_{0};
    std::atomic<size_t> collection_errors_{0};
    interrupt_metrics last_metrics_;

    // Helper methods
    metric create_metric(const std::string& name, double value,
                         const std::unordered_map<std::string, std::string>& tags = {},
                         const std::string& unit = "") const;
    void add_interrupt_metrics(std::vector<metric>& metrics, const interrupt_metrics& data);
};

}  // namespace monitoring
}  // namespace kcenon
