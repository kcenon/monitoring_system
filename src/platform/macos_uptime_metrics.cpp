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

#if defined(__APPLE__)

#include <sys/sysctl.h>
#include <sys/time.h>

namespace kcenon {
namespace monitoring {

namespace {

/**
 * Get boot time using sysctl(KERN_BOOTTIME)
 * Returns boot time as timeval structure
 */
struct boot_time_data {
    int64_t boot_timestamp{0};
    double uptime_seconds{0.0};
    bool valid{false};
};

boot_time_data read_boot_time() {
    boot_time_data result;

    int mib[2] = {CTL_KERN, KERN_BOOTTIME};
    struct timeval boottime;
    size_t size = sizeof(boottime);

    if (sysctl(mib, 2, &boottime, &size, nullptr, 0) != 0) {
        return result;
    }

    result.boot_timestamp = static_cast<int64_t>(boottime.tv_sec);

    // Calculate uptime from boot time
    auto now = std::chrono::system_clock::now();
    auto now_epoch = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();
    result.uptime_seconds = static_cast<double>(now_epoch - result.boot_timestamp);

    // Add microseconds precision
    result.uptime_seconds += static_cast<double>(boottime.tv_usec) / 1000000.0;

    result.valid = true;
    return result;
}

}  // anonymous namespace

uptime_info_collector::uptime_info_collector() = default;
uptime_info_collector::~uptime_info_collector() = default;

bool uptime_info_collector::check_availability_impl() const {
    // Check if we can get boot time via sysctl
    int mib[2] = {CTL_KERN, KERN_BOOTTIME};
    struct timeval boottime;
    size_t size = sizeof(boottime);

    return sysctl(mib, 2, &boottime, &size, nullptr, 0) == 0;
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

    // Read boot time via sysctl
    auto boot_data = read_boot_time();
    if (boot_data.valid) {
        metrics.uptime_seconds = boot_data.uptime_seconds;
        metrics.boot_timestamp = boot_data.boot_timestamp;
        metrics.idle_seconds = 0.0;  // Not available on macOS
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

#endif  // defined(__APPLE__)
