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

#include <kcenon/monitoring/collectors/socket_buffer_collector.h>

#if defined(__linux__)

#include <fstream>
#include <sstream>
#include <string>

namespace kcenon {
namespace monitoring {

namespace {

/**
 * Parse /proc/net/tcp or /proc/net/tcp6 to collect buffer queue sizes
 * Format: sl local_address rem_address st tx_queue:rx_queue ...
 * tx_queue and rx_queue are in columns 5 and 6 (hex:hex format)
 */
void parse_proc_net_tcp(const std::string& path, socket_buffer_metrics& metrics) {
    std::ifstream file(path);
    
    if (!file.is_open()) {
        return;
    }

    std::string line;
    // Skip header line
    if (!std::getline(file, line)) {
        return;
    }

    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string sl, local_addr, rem_addr, st_hex, queues;
        
        // Parse: sl local_address rem_address st tx_queue:rx_queue ...
        if (!(iss >> sl >> local_addr >> rem_addr >> st_hex >> queues)) {
            continue;
        }

        // Parse tx_queue:rx_queue (both hex)
        size_t colon_pos = queues.find(':');
        if (colon_pos == std::string::npos) {
            continue;
        }

        try {
            std::string tx_hex = queues.substr(0, colon_pos);
            std::string rx_hex = queues.substr(colon_pos + 1);
            
            uint64_t tx_queue = std::stoull(tx_hex, nullptr, 16);
            uint64_t rx_queue = std::stoull(rx_hex, nullptr, 16);

            metrics.send_buffer_bytes += tx_queue;
            metrics.recv_buffer_bytes += rx_queue;
            metrics.tcp_socket_count++;

            // Count queues that appear full (non-empty)
            if (tx_queue > 0) {
                metrics.send_queue_full_count++;
            }
            if (rx_queue > 0) {
                metrics.recv_queue_full_count++;
            }
        } catch (...) {
            // Skip malformed lines
            continue;
        }
    }
}

/**
 * Parse /proc/net/sockstat for socket memory statistics
 * Format: TCP: inuse X orphan X tw X alloc X mem Y
 * The "mem" value is in pages (typically 4KB each)
 */
void parse_proc_net_sockstat(socket_buffer_metrics& metrics) {
    std::ifstream file("/proc/net/sockstat");
    
    if (!file.is_open()) {
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.substr(0, 4) == "TCP:") {
            // Parse TCP line for socket memory
            std::istringstream iss(line);
            std::string token;
            while (iss >> token) {
                if (token == "mem") {
                    uint64_t mem_pages;
                    if (iss >> mem_pages) {
                        // Convert pages to bytes (assume 4KB pages)
                        metrics.socket_memory_bytes = mem_pages * 4096;
                    }
                } else if (token == "inuse") {
                    uint64_t inuse;
                    if (iss >> inuse) {
                        metrics.socket_count += inuse;
                    }
                }
            }
        } else if (line.substr(0, 4) == "UDP:") {
            // Parse UDP line for socket count
            std::istringstream iss(line);
            std::string token;
            while (iss >> token) {
                if (token == "inuse") {
                    uint64_t inuse;
                    if (iss >> inuse) {
                        metrics.udp_socket_count = inuse;
                        metrics.socket_count += inuse;
                    }
                    break;
                }
            }
        }
    }
}

}  // anonymous namespace

socket_buffer_info_collector::socket_buffer_info_collector() = default;
socket_buffer_info_collector::~socket_buffer_info_collector() = default;

bool socket_buffer_info_collector::check_availability_impl() const {
    // Check if we can read /proc/net/tcp
    std::ifstream file("/proc/net/tcp");
    return file.is_open();
}

socket_buffer_metrics socket_buffer_info_collector::collect_metrics_impl() {
    socket_buffer_metrics metrics;
    metrics.timestamp = std::chrono::system_clock::now();
    metrics.metrics_available = true;

    // Collect TCP socket buffer data from /proc/net/tcp and /proc/net/tcp6
    parse_proc_net_tcp("/proc/net/tcp", metrics);
    parse_proc_net_tcp("/proc/net/tcp6", metrics);

    // Collect socket memory statistics from /proc/net/sockstat
    parse_proc_net_sockstat(metrics);

    return metrics;
}

bool socket_buffer_info_collector::is_socket_buffer_monitoring_available() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!availability_checked_) {
        available_ = check_availability_impl();
        availability_checked_ = true;
    }
    return available_;
}

socket_buffer_metrics socket_buffer_info_collector::collect_metrics() {
    std::lock_guard<std::mutex> lock(mutex_);
    return collect_metrics_impl();
}

}  // namespace monitoring
}  // namespace kcenon

#endif  // defined(__linux__)
