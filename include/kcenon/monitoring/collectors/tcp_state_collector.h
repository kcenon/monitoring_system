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
 * @file tcp_state_collector.h
 * @brief TCP connection state monitoring collector
 *
 * This file provides TCP connection state monitoring using platform-specific APIs.
 * Tracking TCP connection states helps detect connection leaks, capacity issues,
 * and networking problems like TIME_WAIT accumulation.
 *
 * Platform APIs:
 * - Linux: /proc/net/tcp and /proc/net/tcp6 parsing
 * - macOS: sysctlbyname("net.inet.tcp.pcblist") or lsof-style enumeration
 * - Windows: GetExtendedTcpTable() API (stub implementation)
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
 * @struct tcp_state_metrics
 * @brief Aggregated TCP connection state metrics
 */
struct tcp_state_metrics {
    tcp_state_counts ipv4_counts;                    ///< IPv4 connection counts
    tcp_state_counts ipv6_counts;                    ///< IPv6 connection counts
    tcp_state_counts combined_counts;                ///< Combined IPv4+IPv6 counts
    uint64_t total_connections{0};                   ///< Total connection count
    bool metrics_available{false};                   ///< Whether metrics are available
    std::chrono::system_clock::time_point timestamp; ///< Reading timestamp
};

// Forward declaration
namespace platform {
class metrics_provider;
}  // namespace platform

/**
 * @class tcp_state_info_collector
 * @brief TCP state data collector using platform abstraction layer
 *
 * This class provides TCP state data collection using the unified
 * metrics_provider interface, eliminating platform-specific code.
 */
class tcp_state_info_collector {
   public:
    tcp_state_info_collector();
    ~tcp_state_info_collector();

    // Non-copyable, non-moveable due to internal state
    tcp_state_info_collector(const tcp_state_info_collector&) = delete;
    tcp_state_info_collector& operator=(const tcp_state_info_collector&) = delete;
    tcp_state_info_collector(tcp_state_info_collector&&) = delete;
    tcp_state_info_collector& operator=(tcp_state_info_collector&&) = delete;

    /**
     * Check if TCP state monitoring is available on this system
     * @return True if TCP state metrics can be read
     */
    bool is_tcp_state_monitoring_available() const;

    /**
     * Collect current TCP state metrics
     * @return tcp_state_metrics structure with current values
     */
    tcp_state_metrics collect_metrics();

   private:
    std::unique_ptr<platform::metrics_provider> provider_;
};

/**
 * @class tcp_state_collector
 * @brief TCP connection state monitoring collector
 *
 * Collects TCP connection state metrics with cross-platform support.
 * Returns unavailable metrics on Windows (stub implementation).
 */
class tcp_state_collector {
   public:
    tcp_state_collector();
    ~tcp_state_collector() = default;

    // Non-copyable, non-moveable due to internal state
    tcp_state_collector(const tcp_state_collector&) = delete;
    tcp_state_collector& operator=(const tcp_state_collector&) = delete;
    tcp_state_collector(tcp_state_collector&&) = delete;
    tcp_state_collector& operator=(tcp_state_collector&&) = delete;

    /**
     * Initialize the collector with configuration
     * @param config Configuration options:
     *   - "enabled": "true"/"false" (default: true)
     *   - "time_wait_warning_threshold": count (default: 10000)
     *   - "close_wait_warning_threshold": count (default: 100)
     *   - "include_ipv6": "true"/"false" (default: true)
     * @return true if initialization successful
     */
    bool initialize(const std::unordered_map<std::string, std::string>& config);

    /**
     * Collect TCP state metrics
     * @return Vector of collected metrics
     */
    std::vector<metric> collect();

    /**
     * Get the name of this collector
     * @return Collector name
     */
    std::string get_name() const { return "tcp_state_collector"; }

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
     * Get last collected TCP state metrics
     * @return Most recent tcp_state_metrics reading
     */
    tcp_state_metrics get_last_metrics() const;

    /**
     * Check if TCP state monitoring is available
     * @return True if TCP state metrics are accessible
     */
    bool is_tcp_state_monitoring_available() const;

   private:
    std::unique_ptr<tcp_state_info_collector> collector_;

    // Configuration
    bool enabled_{true};
    bool include_ipv6_{true};
    uint64_t time_wait_warning_threshold_{10000};
    uint64_t close_wait_warning_threshold_{100};

    // Statistics
    mutable std::mutex stats_mutex_;
    std::atomic<size_t> collection_count_{0};
    std::atomic<size_t> collection_errors_{0};
    tcp_state_metrics last_metrics_;

    // Helper methods
    metric create_metric(const std::string& name, double value,
                         const std::unordered_map<std::string, std::string>& tags = {},
                         const std::string& unit = "") const;
    void add_tcp_state_metrics(std::vector<metric>& metrics,
                               const tcp_state_metrics& tcp_data);
};

}  // namespace monitoring
}  // namespace kcenon
