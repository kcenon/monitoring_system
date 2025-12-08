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

#include <kcenon/monitoring/collectors/fd_collector.h>

#if defined(_WIN32) || defined(_WIN64)

#include <windows.h>

namespace kcenon {
namespace monitoring {

namespace {

/**
 * Get process handle count using Windows API
 * Note: Windows uses handles instead of file descriptors
 */
struct handle_info {
    DWORD handle_count{0};
    bool valid{false};
};

handle_info get_handle_count() {
    handle_info info;
    HANDLE process = GetCurrentProcess();
    if (GetProcessHandleCount(process, &info.handle_count)) {
        info.valid = true;
    }
    return info;
}

}  // anonymous namespace

fd_info_collector::fd_info_collector() = default;
fd_info_collector::~fd_info_collector() = default;

bool fd_info_collector::check_availability_impl() const {
    // Windows always supports GetProcessHandleCount
    DWORD count;
    return GetProcessHandleCount(GetCurrentProcess(), &count) != 0;
}

fd_metrics fd_info_collector::collect_metrics_impl() {
    fd_metrics metrics;
    metrics.timestamp = std::chrono::system_clock::now();

    // System-wide handle metrics not easily available on Windows
    metrics.system_metrics_available = false;
    metrics.fd_used_system = 0;
    metrics.fd_max_system = 0;

    // Process handle count (Windows equivalent of FD count)
    auto handle_data = get_handle_count();
    if (handle_data.valid) {
        metrics.fd_used_process = static_cast<uint64_t>(handle_data.handle_count);
    }

    // Windows doesn't have the same soft/hard limit concept as POSIX
    // Use a reasonable default for process handle limit
    // The default per-process handle limit is 16,777,216 on modern Windows
    metrics.fd_soft_limit = 16777216;
    metrics.fd_hard_limit = 16777216;

    // Calculate usage percentage
    if (metrics.fd_soft_limit > 0) {
        metrics.fd_usage_percent = 100.0 * static_cast<double>(metrics.fd_used_process) /
                                   static_cast<double>(metrics.fd_soft_limit);
    }

    return metrics;
}

}  // namespace monitoring
}  // namespace kcenon

#endif  // defined(_WIN32) || defined(_WIN64)
