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

#include <kcenon/monitoring/collectors/uptime_collector.h>

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace kcenon {
namespace monitoring {

namespace {

/**
 * Get uptime using GetTickCount64()
 * Returns milliseconds since system boot
 */
struct windows_uptime_data {
    double uptime_seconds{0.0};
    int64_t boot_timestamp{0};
    bool valid{false};
};

windows_uptime_data read_windows_uptime() {
    windows_uptime_data result;

    // GetTickCount64 returns milliseconds since system start
    ULONGLONG uptime_ms = GetTickCount64();
    result.uptime_seconds = static_cast<double>(uptime_ms) / 1000.0;

    // Calculate boot timestamp from current time and uptime
    auto now = std::chrono::system_clock::now();
    auto now_epoch = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();
    result.boot_timestamp = static_cast<int64_t>(now_epoch - result.uptime_seconds);

    result.valid = true;
    return result;
}

}  // anonymous namespace

uptime_info_collector::uptime_info_collector() = default;
uptime_info_collector::~uptime_info_collector() = default;

bool uptime_info_collector::check_availability_impl() const {
    // GetTickCount64 is always available on Windows Vista and later
    return true;
}

bool uptime_info_collector::is_uptime_monitoring_available() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!availability_checked_) {
        available_ = check_availability_impl();
        availability_checked_ = true;
    }
    return available_;
}

uptime_metrics uptime_info_collector::collect_metrics_impl() {
    uptime_metrics metrics;
    metrics.timestamp = std::chrono::system_clock::now();

    // Read uptime via GetTickCount64
    auto uptime_data = read_windows_uptime();
    if (uptime_data.valid) {
        metrics.uptime_seconds = uptime_data.uptime_seconds;
        metrics.boot_timestamp = uptime_data.boot_timestamp;
        metrics.idle_seconds = 0.0;  // Not easily available on Windows
        metrics.metrics_available = true;
    }

    return metrics;
}

uptime_metrics uptime_info_collector::collect_metrics() {
    std::lock_guard<std::mutex> lock(mutex_);

    // Check availability directly to avoid deadlock
    if (!availability_checked_) {
        available_ = check_availability_impl();
        availability_checked_ = true;
    }

    if (!available_) {
        uptime_metrics empty;
        empty.timestamp = std::chrono::system_clock::now();
        return empty;
    }

    return collect_metrics_impl();
}

}  // namespace monitoring
}  // namespace kcenon

#endif  // defined(_WIN32)
