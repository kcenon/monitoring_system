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

#if defined(__linux__)

#include <fstream>
#include <sstream>
#include <string>

namespace kcenon {
namespace monitoring {

namespace {

/**
 * Read system uptime and idle time from /proc/uptime
 * Format: uptime_seconds idle_seconds
 * Example: 12345.67 9876.54
 */
struct proc_uptime_data {
    double uptime_seconds{0.0};
    double idle_seconds{0.0};
    bool valid{false};
};

proc_uptime_data read_proc_uptime() {
    proc_uptime_data result;

    std::ifstream uptime_file("/proc/uptime");
    if (!uptime_file.is_open()) {
        return result;
    }

    std::string line;
    if (std::getline(uptime_file, line)) {
        std::istringstream iss(line);
        if (iss >> result.uptime_seconds >> result.idle_seconds) {
            result.valid = true;
        }
    }

    return result;
}

/**
 * Calculate boot timestamp from uptime
 */
int64_t calculate_boot_timestamp(double uptime_seconds) {
    auto now = std::chrono::system_clock::now();
    auto now_epoch = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();
    return static_cast<int64_t>(now_epoch - uptime_seconds);
}

}  // anonymous namespace

uptime_info_collector::uptime_info_collector() = default;
uptime_info_collector::~uptime_info_collector() = default;

bool uptime_info_collector::check_availability_impl() const {
    // Check if we can read /proc/uptime
    std::ifstream uptime_file("/proc/uptime");
    return uptime_file.is_open();
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

    // Read /proc/uptime
    auto proc_data = read_proc_uptime();
    if (proc_data.valid) {
        metrics.uptime_seconds = proc_data.uptime_seconds;
        metrics.idle_seconds = proc_data.idle_seconds;
        metrics.boot_timestamp = calculate_boot_timestamp(proc_data.uptime_seconds);
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

#endif  // defined(__linux__)
