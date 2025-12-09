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

#if defined(__APPLE__)

#include <sys/sysctl.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>

namespace kcenon {
namespace monitoring {

namespace {

/**
 * Parse netstat -m output for mbuf statistics
 * Extracts socket buffer memory usage from mbuf pool statistics
 */
socket_buffer_metrics collect_via_netstat_m() {
    socket_buffer_metrics metrics;
    metrics.timestamp = std::chrono::system_clock::now();

    // Use popen to run netstat -m and parse the output
    std::unique_ptr<FILE, decltype(&pclose)> pipe(
        popen("netstat -m 2>/dev/null", "r"), pclose);

    if (!pipe) {
        return metrics;
    }

    metrics.metrics_available = true;

    std::array<char, 512> buffer;
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        std::string line(buffer.data());

        // Parse lines like "X/Y/Z mbufs in use" or memory statistics
        // Example: "1024/2048/4096 mbufs in use (current/cache/total)"
        if (line.find("mbufs in use") != std::string::npos) {
            // Extract the first number (current mbufs in use)
            std::istringstream iss(line);
            std::string numbers;
            iss >> numbers;
            size_t slash_pos = numbers.find('/');
            if (slash_pos != std::string::npos) {
                try {
                    uint64_t current_mbufs = std::stoull(numbers.substr(0, slash_pos));
                    // Estimate memory (each mbuf is typically 256 bytes or 2KB with cluster)
                    metrics.socket_memory_bytes += current_mbufs * 256;
                } catch (...) {
                    // Ignore parse errors
                }
            }
        }

        // Parse socket counts from "X sockets for..." lines
        if (line.find("socket") != std::string::npos) {
            std::istringstream iss(line);
            uint64_t count;
            if (iss >> count) {
                metrics.socket_count += count;
            }
        }
    }

    return metrics;
}

/**
 * Parse netstat -an output for TCP socket queue information
 * Format: Proto Recv-Q Send-Q  Local Address          Foreign Address        (state)
 */
void collect_tcp_queue_via_netstat(socket_buffer_metrics& metrics) {
    std::unique_ptr<FILE, decltype(&pclose)> pipe(
        popen("netstat -an -p tcp 2>/dev/null", "r"), pclose);

    if (!pipe) {
        return;
    }

    std::array<char, 256> buffer;
    bool header_skipped = false;

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        std::string line(buffer.data());

        // Skip header lines (usually first two lines)
        if (!header_skipped) {
            if (line.find("Recv-Q") != std::string::npos || 
                line.find("Active") != std::string::npos ||
                line.find("Proto") != std::string::npos) {
                header_skipped = true;
                continue;
            }
        }

        // Skip empty lines
        if (line.empty() || line[0] == '\n') {
            continue;
        }

        // Parse: tcp4/tcp6  recv_q  send_q  local_addr  foreign_addr  state
        std::istringstream iss(line);
        std::string proto;
        uint64_t recv_q, send_q;

        if (iss >> proto >> recv_q >> send_q) {
            if (proto.find("tcp") != std::string::npos) {
                metrics.recv_buffer_bytes += recv_q;
                metrics.send_buffer_bytes += send_q;
                metrics.tcp_socket_count++;

                if (recv_q > 0) {
                    metrics.recv_queue_full_count++;
                }
                if (send_q > 0) {
                    metrics.send_queue_full_count++;
                }
            }
        }
    }
}

/**
 * Get IPC buffer settings via sysctl
 */
void collect_sysctl_ipc_info(socket_buffer_metrics& metrics) {
    // Try to get socket buffer max sizes
    int mib[3];
    size_t len;

    // Get TCP send buffer max (kern.ipc.maxsockbuf)
    mib[0] = CTL_KERN;
    mib[1] = KERN_IPC;
    // Note: KERN_IPC_MAXSOCKBUF may not be available on all macOS versions
    // We use the numeric value directly
    
    uint64_t maxsockbuf = 0;
    len = sizeof(maxsockbuf);
    if (sysctlbyname("kern.ipc.maxsockbuf", &maxsockbuf, &len, nullptr, 0) == 0) {
        // This gives us max per-socket buffer, not total usage
        // Just note it's available but don't add to metrics
        (void)maxsockbuf;
    }
}

}  // anonymous namespace

socket_buffer_info_collector::socket_buffer_info_collector() = default;
socket_buffer_info_collector::~socket_buffer_info_collector() = default;

bool socket_buffer_info_collector::check_availability_impl() const {
    // Try running netstat to check availability
    std::unique_ptr<FILE, decltype(&pclose)> pipe(
        popen("netstat -an -p tcp 2>/dev/null | head -1", "r"), pclose);
    return pipe != nullptr;
}

socket_buffer_metrics socket_buffer_info_collector::collect_metrics_impl() {
    // Start with mbuf statistics
    socket_buffer_metrics metrics = collect_via_netstat_m();

    // Add TCP queue information
    collect_tcp_queue_via_netstat(metrics);

    // Add sysctl IPC info
    collect_sysctl_ipc_info(metrics);

    metrics.metrics_available = true;

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

#endif  // defined(__APPLE__)
