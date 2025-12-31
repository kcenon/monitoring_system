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
 * @file socket_buffer_collector.h
 * @brief Socket buffer usage monitoring collector
 *
 * This file provides socket buffer (send/receive queue) usage monitoring using
 * platform-specific APIs. Tracking socket buffer fill levels helps detect network
 * bottlenecks, slow connections, and dropped packets at the socket level.
 *
 * Platform APIs:
 * - Linux: /proc/net/tcp (tx_queue, rx_queue), /proc/net/sockstat
 * - macOS: netstat -m (mbuf statistics), sysctl kern.ipc
 * - Windows: GetTcpStatistics() API (stub implementation)
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
 * @struct socket_buffer_metrics
 * @brief Aggregated socket buffer usage metrics
 */
struct socket_buffer_metrics {
    uint64_t recv_buffer_bytes{0};      ///< Total bytes in receive buffers
    uint64_t send_buffer_bytes{0};      ///< Total bytes in send buffers
    uint64_t recv_queue_full_count{0};  ///< Count of sockets with full recv queue
    uint64_t send_queue_full_count{0};  ///< Count of sockets with full send queue
    uint64_t socket_memory_bytes{0};    ///< Total socket buffer memory used
    uint64_t socket_count{0};           ///< Total number of sockets counted
    uint64_t tcp_socket_count{0};       ///< Number of TCP sockets
    uint64_t udp_socket_count{0};       ///< Number of UDP sockets
    bool metrics_available{false};      ///< Whether metrics are available
    std::chrono::system_clock::time_point timestamp;  ///< Reading timestamp
};

// Forward declaration
namespace platform {
class metrics_provider;
}  // namespace platform

/**
 * @class socket_buffer_info_collector
 * @brief Socket buffer data collector using platform abstraction layer
 *
 * This class provides socket buffer data collection using the unified
 * metrics_provider interface, eliminating platform-specific code.
 */
class socket_buffer_info_collector {
   public:
    socket_buffer_info_collector();
    ~socket_buffer_info_collector();

    // Non-copyable, non-moveable due to internal state
    socket_buffer_info_collector(const socket_buffer_info_collector&) = delete;
    socket_buffer_info_collector& operator=(const socket_buffer_info_collector&) = delete;
    socket_buffer_info_collector(socket_buffer_info_collector&&) = delete;
    socket_buffer_info_collector& operator=(socket_buffer_info_collector&&) = delete;

    /**
     * Check if socket buffer monitoring is available on this system
     * @return True if socket buffer metrics can be read
     */
    bool is_socket_buffer_monitoring_available() const;

    /**
     * Collect current socket buffer metrics
     * @return socket_buffer_metrics structure with current values
     */
    socket_buffer_metrics collect_metrics();

   private:
    std::unique_ptr<platform::metrics_provider> provider_;
};

/**
 * @class socket_buffer_collector
 * @brief Socket buffer usage monitoring collector
 *
 * Uses CRTP base class to reduce code duplication.
 */
class socket_buffer_collector : public collector_base<socket_buffer_collector> {
   public:
    static constexpr const char* collector_name = "socket_buffer_collector";

    socket_buffer_collector();
    ~socket_buffer_collector() override = default;

    socket_buffer_collector(const socket_buffer_collector&) = delete;
    socket_buffer_collector& operator=(const socket_buffer_collector&) = delete;
    socket_buffer_collector(socket_buffer_collector&&) = delete;
    socket_buffer_collector& operator=(socket_buffer_collector&&) = delete;

    // CRTP interface
    bool do_initialize(const config_map& config);
    std::vector<metric> do_collect();
    bool is_available() const;
    std::vector<std::string> do_get_metric_types() const;
    void do_add_statistics(stats_map& stats) const;

    socket_buffer_metrics get_last_metrics() const;
    bool is_socket_buffer_monitoring_available() const;

   private:
    std::unique_ptr<socket_buffer_info_collector> collector_;

    uint64_t queue_full_threshold_bytes_{65536};
    uint64_t memory_warning_threshold_bytes_{104857600};
    socket_buffer_metrics last_metrics_;

    void add_socket_buffer_metrics(std::vector<metric>& metrics,
                                   const socket_buffer_metrics& buffer_data);
};

}  // namespace monitoring
}  // namespace kcenon
