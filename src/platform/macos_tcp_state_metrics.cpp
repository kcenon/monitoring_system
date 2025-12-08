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

#if defined(__APPLE__)

#include <sys/sysctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>

#include <cstring>
#include <vector>
#include <array>
#include <cstdio>
#include <memory>

namespace kcenon {
namespace monitoring {

namespace {

/**
 * Map macOS TCP state values to tcp_state enum
 * These values come from netinet/tcp_fsm.h
 */
tcp_state macos_state_to_tcp_state(int state) {
    // macOS uses different state values from Linux
    // These are defined in netinet/tcp_fsm.h
    switch (state) {
        case TCPS_CLOSED:        return tcp_state::CLOSE;
        case TCPS_LISTEN:        return tcp_state::LISTEN;
        case TCPS_SYN_SENT:      return tcp_state::SYN_SENT;
        case TCPS_SYN_RECEIVED:  return tcp_state::SYN_RECV;
        case TCPS_ESTABLISHED:   return tcp_state::ESTABLISHED;
        case TCPS_CLOSE_WAIT:    return tcp_state::CLOSE_WAIT;
        case TCPS_FIN_WAIT_1:    return tcp_state::FIN_WAIT1;
        case TCPS_CLOSING:       return tcp_state::CLOSING;
        case TCPS_LAST_ACK:      return tcp_state::LAST_ACK;
        case TCPS_FIN_WAIT_2:    return tcp_state::FIN_WAIT2;
        case TCPS_TIME_WAIT:     return tcp_state::TIME_WAIT;
        default:                 return tcp_state::UNKNOWN;
    }
}

/**
 * Parse netstat output to count TCP connection states
 * This is a portable approach that works across macOS versions
 */
tcp_state_counts collect_tcp_states_via_netstat() {
    tcp_state_counts counts;

    // Use popen to run netstat and parse the output
    std::unique_ptr<FILE, decltype(&pclose)> pipe(
        popen("netstat -an -p tcp 2>/dev/null", "r"), pclose);

    if (!pipe) {
        return counts;
    }

    std::array<char, 256> buffer;
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        std::string line(buffer.data());

        // Parse netstat output - look for state in the last column
        // Format: Proto Recv-Q Send-Q  Local Address          Foreign Address        (state)
        
        // Find the state by looking for known TCP states
        if (line.find("ESTABLISHED") != std::string::npos) {
            counts.increment(tcp_state::ESTABLISHED);
        } else if (line.find("SYN_SENT") != std::string::npos) {
            counts.increment(tcp_state::SYN_SENT);
        } else if (line.find("SYN_RCVD") != std::string::npos || 
                   line.find("SYN_RECEIVED") != std::string::npos) {
            counts.increment(tcp_state::SYN_RECV);
        } else if (line.find("FIN_WAIT_1") != std::string::npos) {
            counts.increment(tcp_state::FIN_WAIT1);
        } else if (line.find("FIN_WAIT_2") != std::string::npos) {
            counts.increment(tcp_state::FIN_WAIT2);
        } else if (line.find("TIME_WAIT") != std::string::npos) {
            counts.increment(tcp_state::TIME_WAIT);
        } else if (line.find("CLOSED") != std::string::npos) {
            counts.increment(tcp_state::CLOSE);
        } else if (line.find("CLOSE_WAIT") != std::string::npos) {
            counts.increment(tcp_state::CLOSE_WAIT);
        } else if (line.find("LAST_ACK") != std::string::npos) {
            counts.increment(tcp_state::LAST_ACK);
        } else if (line.find("LISTEN") != std::string::npos) {
            counts.increment(tcp_state::LISTEN);
        } else if (line.find("CLOSING") != std::string::npos) {
            counts.increment(tcp_state::CLOSING);
        }
    }

    return counts;
}

}  // anonymous namespace

tcp_state_info_collector::tcp_state_info_collector() = default;
tcp_state_info_collector::~tcp_state_info_collector() = default;

bool tcp_state_info_collector::check_availability_impl() const {
    // Try running netstat to check availability
    std::unique_ptr<FILE, decltype(&pclose)> pipe(
        popen("netstat -an -p tcp 2>/dev/null | head -1", "r"), pclose);
    return pipe != nullptr;
}

tcp_state_metrics tcp_state_info_collector::collect_metrics_impl() {
    tcp_state_metrics metrics;
    metrics.timestamp = std::chrono::system_clock::now();
    metrics.metrics_available = true;

    // Collect TCP states via netstat parsing
    metrics.combined_counts = collect_tcp_states_via_netstat();
    
    // On macOS with netstat, we get combined counts
    metrics.ipv4_counts = metrics.combined_counts;
    metrics.ipv6_counts = tcp_state_counts{};  // Not separated

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

#endif  // defined(__APPLE__)

