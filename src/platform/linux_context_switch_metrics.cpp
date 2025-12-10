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

#include <kcenon/monitoring/collectors/context_switch_collector.h>

#if defined(__linux__)

#include <fstream>
#include <sstream>
#include <string>

namespace kcenon {
namespace monitoring {

namespace {

/**
 * Read system-wide context switch count from /proc/stat
 * Format: ctxt 123456789
 */
std::optional<uint64_t> read_system_context_switches() {
    std::ifstream stat_file("/proc/stat");
    if (!stat_file.is_open()) {
        return std::nullopt;
    }

    std::string line;
    while (std::getline(stat_file, line)) {
        if (line.compare(0, 4, "ctxt") == 0) {
            std::istringstream iss(line);
            std::string label;
            uint64_t count;
            if (iss >> label >> count) {
                return count;
            }
        }
    }
    return std::nullopt;
}

/**
 * Read process context switches from /proc/self/status
 * Format:
 *   voluntary_ctxt_switches:    123
 *   nonvoluntary_ctxt_switches: 456
 */
std::optional<process_context_switch_info> read_process_context_switches() {
    std::ifstream status_file("/proc/self/status");
    if (!status_file.is_open()) {
        return std::nullopt;
    }

    process_context_switch_info info;
    std::string line;
    bool found_voluntary = false;
    bool found_nonvoluntary = false;

    while (std::getline(status_file, line)) {
        if (line.compare(0, 24, "voluntary_ctxt_switches:") == 0) {
            std::istringstream iss(line.substr(24));
            iss >> info.voluntary_switches;
            found_voluntary = true;
        } else if (line.compare(0, 27, "nonvoluntary_ctxt_switches:") == 0) {
            std::istringstream iss(line.substr(27));
            iss >> info.nonvoluntary_switches;
            found_nonvoluntary = true;
        }
        
        if (found_voluntary && found_nonvoluntary) {
            break;
        }
    }

    if (found_voluntary || found_nonvoluntary) {
        info.total_switches = info.voluntary_switches + info.nonvoluntary_switches;
        return info;
    }
    return std::nullopt;
}

}  // anonymous namespace

context_switch_info_collector::context_switch_info_collector() = default;
context_switch_info_collector::~context_switch_info_collector() = default;

bool context_switch_info_collector::check_availability_impl() const {
    // Check if we can read /proc/stat
    std::ifstream stat_file("/proc/stat");
    return stat_file.is_open();
}

bool context_switch_info_collector::is_context_switch_monitoring_available() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!availability_checked_) {
        available_ = check_availability_impl();
        availability_checked_ = true;
    }
    return available_;
}

double context_switch_info_collector::calculate_rate(uint64_t current_switches) {
    auto now = std::chrono::steady_clock::now();
    
    if (!has_previous_sample_) {
        last_system_switches_ = current_switches;
        last_collection_time_ = now;
        has_previous_sample_ = true;
        return 0.0;
    }
    
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_collection_time_);
    
    if (elapsed.count() <= 0) {
        return 0.0;
    }
    
    double delta = static_cast<double>(current_switches - last_system_switches_);
    double seconds = elapsed.count() / 1000.0;
    double rate = delta / seconds;
    
    last_system_switches_ = current_switches;
    last_collection_time_ = now;
    
    return rate >= 0.0 ? rate : 0.0;
}

context_switch_metrics context_switch_info_collector::collect_metrics_impl() {
    context_switch_metrics metrics;
    metrics.timestamp = std::chrono::system_clock::now();

    // Read system-wide context switches
    auto system_switches = read_system_context_switches();
    if (system_switches) {
        metrics.system_context_switches_total = *system_switches;
        metrics.context_switches_per_sec = calculate_rate(*system_switches);
        metrics.rate_available = has_previous_sample_;
        metrics.metrics_available = true;
    }

    // Read process context switches
    auto process_info = read_process_context_switches();
    if (process_info) {
        metrics.process_info = *process_info;
    }

    return metrics;
}

context_switch_metrics context_switch_info_collector::collect_metrics() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check availability directly to avoid deadlock (don't call is_context_switch_monitoring_available())
    if (!availability_checked_) {
        available_ = check_availability_impl();
        availability_checked_ = true;
    }
    
    if (!available_) {
        context_switch_metrics empty;
        empty.timestamp = std::chrono::system_clock::now();
        return empty;
    }
    
    return collect_metrics_impl();
}

}  // namespace monitoring
}  // namespace kcenon

#endif  // defined(__linux__)
