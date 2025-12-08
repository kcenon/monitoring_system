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

#include <kcenon/monitoring/collectors/tcp_state_collector.h>

#if defined(__linux__)

#include <fstream>
#include <sstream>
#include <string>

namespace kcenon {
namespace monitoring {

namespace {

/**
 * Map Linux kernel TCP state values to tcp_state enum
 * These values come from include/net/tcp_states.h in the Linux kernel
 */
tcp_state linux_state_to_tcp_state(int state) {
    switch (state) {
        case 1: return tcp_state::ESTABLISHED;
        case 2: return tcp_state::SYN_SENT;
        case 3: return tcp_state::SYN_RECV;
        case 4: return tcp_state::FIN_WAIT1;
        case 5: return tcp_state::FIN_WAIT2;
        case 6: return tcp_state::TIME_WAIT;
        case 7: return tcp_state::CLOSE;
        case 8: return tcp_state::CLOSE_WAIT;
        case 9: return tcp_state::LAST_ACK;
        case 10: return tcp_state::LISTEN;
        case 11: return tcp_state::CLOSING;
        default: return tcp_state::UNKNOWN;
    }
}

/**
 * Parse /proc/net/tcp or /proc/net/tcp6 to count connections by state
 * Format: sl local_address rem_address st tx_queue rx_queue ...
 * The 'st' field is the hex-encoded state value
 */
tcp_state_counts parse_proc_net_tcp(const std::string& path) {
    tcp_state_counts counts;
    std::ifstream file(path);
    
    if (!file.is_open()) {
        return counts;
    }

    std::string line;
    // Skip header line
    if (!std::getline(file, line)) {
        return counts;
    }

    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string sl, local_addr, rem_addr, st_hex;
        
        // Parse: sl local_address rem_address st ...
        if (!(iss >> sl >> local_addr >> rem_addr >> st_hex)) {
            continue;
        }

        // Parse hex state value
        try {
            int state = std::stoi(st_hex, nullptr, 16);
            tcp_state tcp_st = linux_state_to_tcp_state(state);
            counts.increment(tcp_st);
        } catch (...) {
            // Skip malformed lines
            continue;
        }
    }

    return counts;
}

}  // anonymous namespace

tcp_state_info_collector::tcp_state_info_collector() = default;
tcp_state_info_collector::~tcp_state_info_collector() = default;

bool tcp_state_info_collector::check_availability_impl() const {
    // Check if we can read /proc/net/tcp
    std::ifstream file("/proc/net/tcp");
    return file.is_open();
}

tcp_state_metrics tcp_state_info_collector::collect_metrics_impl() {
    tcp_state_metrics metrics;
    metrics.timestamp = std::chrono::system_clock::now();
    metrics.metrics_available = true;

    // Collect IPv4 TCP connections
    metrics.ipv4_counts = parse_proc_net_tcp("/proc/net/tcp");

    // Collect IPv6 TCP connections
    metrics.ipv6_counts = parse_proc_net_tcp("/proc/net/tcp6");

    // Combine counts
    metrics.combined_counts.established = metrics.ipv4_counts.established + metrics.ipv6_counts.established;
    metrics.combined_counts.syn_sent = metrics.ipv4_counts.syn_sent + metrics.ipv6_counts.syn_sent;
    metrics.combined_counts.syn_recv = metrics.ipv4_counts.syn_recv + metrics.ipv6_counts.syn_recv;
    metrics.combined_counts.fin_wait1 = metrics.ipv4_counts.fin_wait1 + metrics.ipv6_counts.fin_wait1;
    metrics.combined_counts.fin_wait2 = metrics.ipv4_counts.fin_wait2 + metrics.ipv6_counts.fin_wait2;
    metrics.combined_counts.time_wait = metrics.ipv4_counts.time_wait + metrics.ipv6_counts.time_wait;
    metrics.combined_counts.close = metrics.ipv4_counts.close + metrics.ipv6_counts.close;
    metrics.combined_counts.close_wait = metrics.ipv4_counts.close_wait + metrics.ipv6_counts.close_wait;
    metrics.combined_counts.last_ack = metrics.ipv4_counts.last_ack + metrics.ipv6_counts.last_ack;
    metrics.combined_counts.listen = metrics.ipv4_counts.listen + metrics.ipv6_counts.listen;
    metrics.combined_counts.closing = metrics.ipv4_counts.closing + metrics.ipv6_counts.closing;
    metrics.combined_counts.unknown = metrics.ipv4_counts.unknown + metrics.ipv6_counts.unknown;

    metrics.total_connections = metrics.combined_counts.total();

    return metrics;
}

bool tcp_state_info_collector::is_tcp_state_monitoring_available() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!availability_checked_) {
        available_ = check_availability_impl();
        availability_checked_ = true;
    }
    return available_;
}

tcp_state_metrics tcp_state_info_collector::collect_metrics() {
    std::lock_guard<std::mutex> lock(mutex_);
    return collect_metrics_impl();
}

}  // namespace monitoring
}  // namespace kcenon

#endif  // defined(__linux__)
