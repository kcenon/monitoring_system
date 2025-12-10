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

#if defined(__APPLE__)

#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/task_info.h>
#include <sys/sysctl.h>

namespace kcenon {
namespace monitoring {

namespace {

/**
 * Read process context switches using task_info
 */
std::optional<process_context_switch_info> read_process_context_switches() {
    struct task_events_info events_info;
    mach_msg_type_number_t count = TASK_EVENTS_INFO_COUNT;
    
    kern_return_t kr = task_info(mach_task_self(),
                                  TASK_EVENTS_INFO,
                                  reinterpret_cast<task_info_t>(&events_info),
                                  &count);
    
    if (kr != KERN_SUCCESS) {
        return std::nullopt;
    }
    
    process_context_switch_info info;
    // csw = context switches (voluntary + involuntary combined on macOS)
    info.voluntary_switches = events_info.csw;
    info.nonvoluntary_switches = 0;  // Not separately available on macOS
    info.total_switches = events_info.csw;
    
    return info;
}

}  // anonymous namespace

context_switch_info_collector::context_switch_info_collector() = default;
context_switch_info_collector::~context_switch_info_collector() = default;

bool context_switch_info_collector::check_availability_impl() const {
    // Check if we can get task info
    struct task_events_info events_info;
    mach_msg_type_number_t count = TASK_EVENTS_INFO_COUNT;
    
    kern_return_t kr = task_info(mach_task_self(),
                                  TASK_EVENTS_INFO,
                                  reinterpret_cast<task_info_t>(&events_info),
                                  &count);
    
    return kr == KERN_SUCCESS;
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

    // Read process context switches (this is what's available on macOS)
    auto process_info = read_process_context_switches();
    if (process_info) {
        metrics.process_info = *process_info;
        metrics.metrics_available = true;
        
        // Use process switches for rate calculation on macOS
        metrics.system_context_switches_total = process_info->total_switches;
        metrics.context_switches_per_sec = calculate_rate(process_info->total_switches);
        metrics.rate_available = has_previous_sample_;
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

#endif  // defined(__APPLE__)
