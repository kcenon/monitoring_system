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
 * @file network_metrics_collector.h
 * @brief Unified network metrics collector for socket buffers and TCP states
 *
 * This file provides a consolidated network metrics collector that combines
 * socket buffer monitoring and TCP connection state monitoring into a single
 * collector. This reduces code duplication and provides a unified interface
 * for network-related metrics.
 *
 * Platform APIs:
 * - Linux: /proc/net/tcp, /proc/net/tcp6, /proc/net/sockstat
 * - macOS: sysctlbyname, netstat equivalents
 * - Windows: GetTcpStatistics(), GetExtendedTcpTable() (stub implementation)
 *
 * @note This collector consolidates the following deprecated collectors:
 *       - socket_buffer_collector
 *       - tcp_state_collector
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
 * @enum tcp_state
 * @brief TCP connection states as defined in RFC 793
 */
enum class tcp_state {
    ESTABLISHED = 1,   ///< Connection established
    SYN_SENT = 2,      ///< SYN sent, waiting for SYN-ACK
    SYN_RECV = 3,      ///< SYN received, SYN-ACK sent
    FIN_WAIT1 = 4,     ///< FIN sent, waiting for ACK or FIN
    FIN_WAIT2 = 5,     ///< FIN-ACK received, waiting for FIN
    TIME_WAIT = 6,     ///< Waiting for enough time to pass (2MSL)
    CLOSE = 7,         ///< Connection closed
    CLOSE_WAIT = 8,    ///< Remote side has closed, waiting for local close
    LAST_ACK = 9,      ///< FIN sent after CLOSE_WAIT, waiting for ACK
    LISTEN = 10,       ///< Listening for incoming connections
    CLOSING = 11,      ///< Both sides sent FIN simultaneously
    UNKNOWN = 0        ///< Unknown or invalid state
};

/**
 * @brief Convert tcp_state to string representation
 * @param state The TCP state to convert
 * @return String name of the state
 */
inline std::string tcp_state_to_string(tcp_state state) {
    switch (state) {
        case tcp_state::ESTABLISHED: return "ESTABLISHED";
        case tcp_state::SYN_SENT: return "SYN_SENT";
        case tcp_state::SYN_RECV: return "SYN_RECV";
        case tcp_state::FIN_WAIT1: return "FIN_WAIT1";
        case tcp_state::FIN_WAIT2: return "FIN_WAIT2";
        case tcp_state::TIME_WAIT: return "TIME_WAIT";
        case tcp_state::CLOSE: return "CLOSE";
        case tcp_state::CLOSE_WAIT: return "CLOSE_WAIT";
        case tcp_state::LAST_ACK: return "LAST_ACK";
        case tcp_state::LISTEN: return "LISTEN";
        case tcp_state::CLOSING: return "CLOSING";
        default: return "UNKNOWN";
    }
}

/**
 * @struct tcp_state_counts
 * @brief Counts of connections in each TCP state
 */
struct tcp_state_counts {
    uint64_t established{0};    ///< ESTABLISHED connections
    uint64_t syn_sent{0};       ///< SYN_SENT connections
    uint64_t syn_recv{0};       ///< SYN_RECV connections
    uint64_t fin_wait1{0};      ///< FIN_WAIT1 connections
    uint64_t fin_wait2{0};      ///< FIN_WAIT2 connections
    uint64_t time_wait{0};      ///< TIME_WAIT connections
    uint64_t close{0};          ///< CLOSE connections
    uint64_t close_wait{0};     ///< CLOSE_WAIT connections (leak indicator)
    uint64_t last_ack{0};       ///< LAST_ACK connections
    uint64_t listen{0};         ///< LISTEN sockets
    uint64_t closing{0};        ///< CLOSING connections
    uint64_t unknown{0};        ///< Unknown state connections

    /**
     * Get count for a specific state
     * @param state The TCP state
     * @return Count of connections in that state
     */
    uint64_t get_count(tcp_state state) const {
        switch (state) {
            case tcp_state::ESTABLISHED: return established;
            case tcp_state::SYN_SENT: return syn_sent;
            case tcp_state::SYN_RECV: return syn_recv;
            case tcp_state::FIN_WAIT1: return fin_wait1;
            case tcp_state::FIN_WAIT2: return fin_wait2;
            case tcp_state::TIME_WAIT: return time_wait;
            case tcp_state::CLOSE: return close;
            case tcp_state::CLOSE_WAIT: return close_wait;
            case tcp_state::LAST_ACK: return last_ack;
            case tcp_state::LISTEN: return listen;
            case tcp_state::CLOSING: return closing;
            default: return unknown;
        }
    }

    /**
     * Increment count for a specific state
     * @param state The TCP state to increment
     */
    void increment(tcp_state state) {
        switch (state) {
            case tcp_state::ESTABLISHED: ++established; break;
            case tcp_state::SYN_SENT: ++syn_sent; break;
            case tcp_state::SYN_RECV: ++syn_recv; break;
            case tcp_state::FIN_WAIT1: ++fin_wait1; break;
            case tcp_state::FIN_WAIT2: ++fin_wait2; break;
            case tcp_state::TIME_WAIT: ++time_wait; break;
            case tcp_state::CLOSE: ++close; break;
            case tcp_state::CLOSE_WAIT: ++close_wait; break;
            case tcp_state::LAST_ACK: ++last_ack; break;
            case tcp_state::LISTEN: ++listen; break;
            case tcp_state::CLOSING: ++closing; break;
            default: ++unknown; break;
        }
    }

    /**
     * Get total connection count
     * @return Total number of connections across all states
     */
    uint64_t total() const {
        return established + syn_sent + syn_recv + fin_wait1 + fin_wait2 +
               time_wait + close + close_wait + last_ack + listen + closing + unknown;
    }
};

/**
 * @struct network_metrics_config
 * @brief Configuration for network metrics collector
 */
struct network_metrics_config {
    bool collect_socket_buffers{true};    ///< Enable socket buffer collection
    bool collect_tcp_states{true};        ///< Enable TCP state collection
    uint64_t time_wait_warning_threshold{10000};    ///< TIME_WAIT warning threshold
    uint64_t close_wait_warning_threshold{100};     ///< CLOSE_WAIT warning threshold
    uint64_t queue_full_threshold_bytes{65536};     ///< Socket queue full threshold
    uint64_t memory_warning_threshold_bytes{104857600};  ///< Socket memory warning (100MB)
};

/**
 * @struct network_metrics
 * @brief Aggregated network metrics from all sources
 */
struct network_metrics {
    // Socket buffer metrics
    uint64_t recv_buffer_bytes{0};        ///< Total bytes in receive buffers
    uint64_t send_buffer_bytes{0};        ///< Total bytes in send buffers
    uint64_t socket_memory_bytes{0};      ///< Total socket buffer memory used
    uint64_t socket_count{0};             ///< Total number of sockets
    uint64_t tcp_socket_count{0};         ///< Number of TCP sockets
    uint64_t udp_socket_count{0};         ///< Number of UDP sockets
    bool socket_buffer_available{false};  ///< Socket buffer metrics availability

    // TCP state metrics
    tcp_state_counts tcp_counts;          ///< TCP state counts
    uint64_t total_connections{0};        ///< Total TCP connections
    bool tcp_state_available{false};      ///< TCP state metrics availability

    std::chrono::system_clock::time_point timestamp;  ///< Reading timestamp
};

// Forward declaration
namespace platform {
class metrics_provider;
}  // namespace platform

/**
 * @class network_info_collector
 * @brief Internal network data collector using platform abstraction layer
 *
 * This class provides network data collection using the unified
 * metrics_provider interface, eliminating platform-specific code.
 */
class network_info_collector {
   public:
    network_info_collector();
    ~network_info_collector();

    // Non-copyable, non-moveable due to internal state
    network_info_collector(const network_info_collector&) = delete;
    network_info_collector& operator=(const network_info_collector&) = delete;
    network_info_collector(network_info_collector&&) = delete;
    network_info_collector& operator=(network_info_collector&&) = delete;

    /**
     * Check if socket buffer monitoring is available on this system
     * @return True if socket buffer metrics can be read
     */
    bool is_socket_buffer_monitoring_available() const;

    /**
     * Check if TCP state monitoring is available on this system
     * @return True if TCP state metrics can be read
     */
    bool is_tcp_state_monitoring_available() const;

    /**
     * Collect all network metrics
     * @param config Configuration specifying which metrics to collect
     * @return network_metrics structure with current values
     */
    network_metrics collect_metrics(const network_metrics_config& config);

   private:
    std::unique_ptr<platform::metrics_provider> provider_;
};

/**
 * @class network_metrics_collector
 * @brief Unified network metrics collector
 *
 * Combines socket buffer and TCP state monitoring into a single collector.
 * Provides configurable collection of different metric types.
 *
 * Uses CRTP base class to reduce code duplication.
 */
class network_metrics_collector : public collector_base<network_metrics_collector> {
   public:
    static constexpr const char* collector_name = "network_metrics_collector";

    network_metrics_collector();
    ~network_metrics_collector() override = default;

    network_metrics_collector(const network_metrics_collector&) = delete;
    network_metrics_collector& operator=(const network_metrics_collector&) = delete;
    network_metrics_collector(network_metrics_collector&&) = delete;
    network_metrics_collector& operator=(network_metrics_collector&&) = delete;

    // CRTP interface implementation
    /**
     * Collector-specific initialization
     * @param config Configuration options:
     *   - "collect_socket_buffers": "true"/"false" (default: true)
     *   - "collect_tcp_states": "true"/"false" (default: true)
     *   - "time_wait_warning_threshold": count (default: 10000)
     *   - "close_wait_warning_threshold": count (default: 100)
     *   - "queue_full_threshold_bytes": bytes (default: 65536)
     *   - "memory_warning_threshold_bytes": bytes (default: 104857600)
     * @return true if initialization successful
     */
    bool do_initialize(const config_map& config);

    /**
     * Collect network metrics
     * @return Vector of collected metrics
     */
    std::vector<metric> do_collect();

    /**
     * Check if network metrics monitoring is available
     * @return True if any network metrics are accessible
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
     * Get last collected network metrics
     * @return Most recent network_metrics reading
     */
    network_metrics get_last_metrics() const;

    /**
     * Check if socket buffer monitoring is available
     * @return True if socket buffer metrics are accessible
     */
    bool is_socket_buffer_monitoring_available() const;

    /**
     * Check if TCP state monitoring is available
     * @return True if TCP state metrics are accessible
     */
    bool is_tcp_state_monitoring_available() const;

   private:
    std::unique_ptr<network_info_collector> collector_;
    network_metrics_config config_;
    network_metrics last_metrics_;

    void add_socket_buffer_metrics(std::vector<metric>& metrics,
                                   const network_metrics& data);
    void add_tcp_state_metrics(std::vector<metric>& metrics,
                               const network_metrics& data);
};

}  // namespace monitoring
}  // namespace kcenon
