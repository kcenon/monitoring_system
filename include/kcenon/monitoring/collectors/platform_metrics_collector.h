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
 * @file platform_metrics_collector.h
 * @brief Unified platform-agnostic metrics collector
 *
 * This file provides a unified collector for platform information and
 * platform-specific metrics using the Strategy pattern. It abstracts away
 * platform differences through the metrics_provider interface.
 *
 * Architecture:
 * - Uses metrics_provider (Strategy pattern) for platform-specific implementations
 * - Factory method (metrics_provider::create()) handles platform detection
 * - No #ifdef guards in this header - all platform logic is encapsulated
 *
 * Provides:
 * - Platform identification (name, version)
 * - Uptime metrics
 * - Context switch statistics
 * - Socket/TCP state metrics
 * - Interrupt statistics
 *
 * Part of #389 - Collector consolidation initiative
 */

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "../interfaces/metric_types_adapter.h"
#include "../plugins/collector_plugin.h"
#include "collector_base.h"

namespace kcenon {
namespace monitoring {

// =============================================================================
// Metrics structures
// =============================================================================

/**
 * @struct platform_info
 * @brief Platform identification information
 */
struct platform_info {
    std::string name;        ///< Platform name (linux, macos, windows, unknown)
    std::string version;     ///< OS version string (if available)
    std::string architecture;///< CPU architecture (if available)
    bool available{false};   ///< Whether platform info is available
};

/**
 * @struct platform_uptime
 * @brief Platform uptime information
 */
struct platform_uptime {
    int64_t uptime_seconds{0};   ///< System uptime in seconds
    int64_t idle_seconds{0};     ///< Total idle time in seconds
    int64_t boot_timestamp{0};   ///< Unix timestamp of last boot
    bool available{false};       ///< Whether uptime info is available
};

/**
 * @struct platform_context_switches
 * @brief Platform context switch statistics
 */
struct platform_context_switches {
    uint64_t total_switches{0};       ///< Total context switches
    uint64_t voluntary_switches{0};   ///< Voluntary context switches
    uint64_t involuntary_switches{0}; ///< Involuntary context switches
    double switches_per_second{0.0};  ///< Context switches per second
    bool available{false};            ///< Whether info is available
};

/**
 * @struct platform_tcp_info
 * @brief Platform TCP connection state information
 */
struct platform_tcp_info {
    uint64_t established{0};   ///< ESTABLISHED connections
    uint64_t syn_sent{0};      ///< SYN_SENT connections
    uint64_t syn_recv{0};      ///< SYN_RECV connections
    uint64_t fin_wait1{0};     ///< FIN_WAIT1 connections
    uint64_t fin_wait2{0};     ///< FIN_WAIT2 connections
    uint64_t time_wait{0};     ///< TIME_WAIT connections
    uint64_t close_wait{0};    ///< CLOSE_WAIT connections
    uint64_t listen{0};        ///< LISTEN connections
    uint64_t total{0};         ///< Total connections
    bool available{false};     ///< Whether info is available
};

/**
 * @struct platform_socket_info
 * @brief Platform socket buffer statistics
 */
struct platform_socket_info {
    uint64_t rx_buffer_size{0};  ///< Receive buffer size
    uint64_t tx_buffer_size{0};  ///< Transmit buffer size
    uint64_t rx_buffer_used{0};  ///< Receive buffer used
    uint64_t tx_buffer_used{0};  ///< Transmit buffer used
    bool available{false};       ///< Whether info is available
};

/**
 * @struct platform_interrupt_info
 * @brief Platform interrupt statistics
 */
struct platform_interrupt_info {
    uint64_t total_interrupts{0}; ///< Total interrupt count
    bool available{false};        ///< Whether info is available
};

/**
 * @struct platform_metrics_config
 * @brief Configuration for platform metrics collection
 */
struct platform_metrics_config {
    bool collect_uptime{true};            ///< Collect uptime metrics
    bool collect_context_switches{true};  ///< Collect context switch metrics
    bool collect_tcp_states{true};        ///< Collect TCP state metrics
    bool collect_socket_buffers{true};    ///< Collect socket buffer metrics
    bool collect_interrupts{true};        ///< Collect interrupt metrics
};

/**
 * @struct platform_metrics
 * @brief Combined platform-level metrics
 */
struct platform_metrics {
    platform_info info;
    platform_uptime uptime;
    platform_context_switches context_switches;
    platform_tcp_info tcp;
    platform_socket_info socket;
    platform_interrupt_info interrupts;
    std::chrono::system_clock::time_point timestamp;
};

// =============================================================================
// Forward declarations
// =============================================================================

namespace platform {
class metrics_provider;
}

// =============================================================================
// Platform info collector
// =============================================================================

/**
 * @class platform_info_collector
 * @brief Platform data collector using platform abstraction layer
 *
 * This class provides platform data collection using the unified
 * metrics_provider interface, eliminating platform-specific code.
 */
class platform_info_collector {
   public:
    platform_info_collector();
    ~platform_info_collector();

    platform_info_collector(const platform_info_collector&) = delete;
    platform_info_collector& operator=(const platform_info_collector&) = delete;
    platform_info_collector(platform_info_collector&&) = delete;
    platform_info_collector& operator=(platform_info_collector&&) = delete;

    /**
     * Check if platform monitoring is available
     * @return True if platform metrics can be collected
     */
    bool is_platform_available() const;

    /**
     * Get platform information
     * @return Platform info structure
     */
    platform_info get_platform_info() const;

    /**
     * Get platform uptime information
     * @return Uptime info structure
     */
    platform_uptime get_uptime() const;

    /**
     * Get context switch statistics
     * @return Context switch info structure
     */
    platform_context_switches get_context_switches() const;

    /**
     * Get TCP state information
     * @return TCP info structure
     */
    platform_tcp_info get_tcp_states() const;

    /**
     * Get socket buffer information
     * @return Socket info structure
     */
    platform_socket_info get_socket_buffers() const;

    /**
     * Get interrupt statistics
     * @return Interrupt info structure
     */
    platform_interrupt_info get_interrupt_stats() const;

   private:
    std::unique_ptr<platform::metrics_provider> provider_;
};

// =============================================================================
// Main collector
// =============================================================================

/**
 * @class platform_metrics_collector
 * @brief Unified platform-agnostic metrics collector
 *
 * Collects platform information and platform-specific metrics using the
 * Strategy pattern. The metrics_provider interface abstracts platform
 * differences, providing a unified API across Linux, macOS, and Windows.
 *
 * This collector provides:
 * - Platform identification (name, version, architecture)
 * - System uptime and boot time
 * - Context switch statistics
 * - TCP connection state metrics
 * - Socket buffer statistics
 * - Interrupt statistics
 *
 * Configuration options:
 * - "collect_uptime": "true"/"false" (default: true)
 * - "collect_context_switches": "true"/"false" (default: true)
 * - "collect_tcp_states": "true"/"false" (default: true)
 * - "collect_socket_buffers": "true"/"false" (default: true)
 * - "collect_interrupts": "true"/"false" (default: true)
 *
 * Implements collector_plugin interface for plugin-based architecture.
 */
class platform_metrics_collector : public collector_plugin {
   public:
    platform_metrics_collector();
    explicit platform_metrics_collector(platform_metrics_config config);
    ~platform_metrics_collector() override = default;

    platform_metrics_collector(const platform_metrics_collector&) = delete;
    platform_metrics_collector& operator=(const platform_metrics_collector&) = delete;
    platform_metrics_collector(platform_metrics_collector&&) = delete;
    platform_metrics_collector& operator=(platform_metrics_collector&&) = delete;

    // collector_plugin interface implementation
    /**
     * Get the unique name of this plugin
     * @return Plugin name
     */
    auto name() const -> std::string_view override { return "platform_metrics_collector"; }

    /**
     * Collect platform metrics
     * @return Vector of collected metrics
     */
    auto collect() -> std::vector<metric> override;

    /**
     * Get the collection interval for this plugin
     * @return Collection interval
     */
    auto interval() const -> std::chrono::milliseconds override { return collection_interval_; }

    /**
     * Check if platform monitoring is available
     * @return True if platform metrics are accessible
     */
    auto is_available() const -> bool override;

    /**
     * Check if collector is in a healthy state
     * @return True if collector is operational
     */
    bool is_healthy() const { return is_available(); }

    /**
     * Get supported metric types
     * @return Vector of supported metric type names
     */
    auto get_metric_types() const -> std::vector<std::string> override;

    /**
     * Initialize collector with configuration
     * @param config Configuration map
     * @return True on success
     */
    bool initialize(const config_map& config) override;

    /**
     * Get collector statistics
     * @return Map of statistics
     */
    auto get_statistics() const -> stats_map override;

    // Accessors
    /**
     * Get last collected platform metrics
     * @return Most recent platform_metrics reading
     */
    platform_metrics get_last_metrics() const;

    /**
     * Get platform information
     * @return Platform info structure
     */
    platform_info get_platform_info() const;

    /**
     * Get platform name
     * @return Platform name (linux, macos, windows, unknown)
     */
    std::string get_platform_name() const;

    /**
     * Check if platform monitoring is available
     * @return True if platform metrics are accessible
     */
    bool is_platform_available() const;

   private:
    std::unique_ptr<platform_info_collector> collector_;

    platform_metrics_config config_;
    platform_metrics last_metrics_;

    // Cached platform info (doesn't change during runtime)
    platform_info cached_platform_info_;
    bool platform_info_cached_{false};

    // Collection interval
    std::chrono::milliseconds collection_interval_{std::chrono::seconds(10)};
    mutable std::mutex metrics_mutex_;

    // Helper methods
    void collect_platform_info_metrics(std::vector<metric>& metrics);
    void collect_uptime_metrics(std::vector<metric>& metrics);
    void collect_context_switch_metrics(std::vector<metric>& metrics);
    void collect_tcp_metrics(std::vector<metric>& metrics);
    void collect_socket_metrics(std::vector<metric>& metrics);
    void collect_interrupt_metrics(std::vector<metric>& metrics);
};

}  // namespace monitoring
}  // namespace kcenon
