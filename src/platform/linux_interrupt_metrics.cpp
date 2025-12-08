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

#include <kcenon/monitoring/collectors/interrupt_collector.h>

#if defined(__linux__)

#include <fstream>
#include <sstream>

namespace kcenon {
namespace monitoring {

namespace {

/**
 * Parse /proc/stat to get total interrupt count
 * The 'intr' line contains: intr <total> <irq0> <irq1> ...
 */
uint64_t get_total_interrupts_from_stat() {
    std::ifstream stat_file("/proc/stat");
    if (!stat_file.is_open()) {
        return 0;
    }

    std::string line;
    while (std::getline(stat_file, line)) {
        if (line.substr(0, 4) == "intr") {
            std::istringstream iss(line);
            std::string label;
            uint64_t total;
            if (iss >> label >> total) {
                return total;
            }
        }
    }
    return 0;
}

/**
 * Parse /proc/softirqs to get total soft interrupt count
 * Format:
 *                    CPU0       CPU1       ...
 *          HI:          0          0       ...
 *       TIMER:    1234567    2345678       ...
 * We sum all values across all CPUs and all types
 */
uint64_t get_total_soft_interrupts() {
    std::ifstream softirq_file("/proc/softirqs");
    if (!softirq_file.is_open()) {
        return 0;
    }

    uint64_t total = 0;
    std::string line;
    
    // Skip header line
    std::getline(softirq_file, line);
    
    while (std::getline(softirq_file, line)) {
        std::istringstream iss(line);
        std::string irq_type;
        iss >> irq_type;  // Skip the IRQ type name
        
        uint64_t count;
        while (iss >> count) {
            total += count;
        }
    }
    
    return total;
}

}  // anonymous namespace

interrupt_info_collector::interrupt_info_collector() = default;
interrupt_info_collector::~interrupt_info_collector() = default;

bool interrupt_info_collector::check_availability_impl() const {
    // Check if we can read /proc/stat
    std::ifstream stat_file("/proc/stat");
    if (!stat_file.is_open()) {
        return false;
    }

    // Look for the 'intr' line
    std::string line;
    while (std::getline(stat_file, line)) {
        if (line.substr(0, 4) == "intr") {
            return true;
        }
    }
    return false;
}

interrupt_metrics interrupt_info_collector::collect_metrics_impl() {
    interrupt_metrics metrics;
    metrics.timestamp = std::chrono::system_clock::now();
    metrics.metrics_available = true;

    // Get current interrupt counts
    metrics.interrupts_total = get_total_interrupts_from_stat();
    metrics.soft_interrupts_total = get_total_soft_interrupts();
    metrics.soft_interrupts_available = (metrics.soft_interrupts_total > 0);

    // Calculate rates if we have a previous sample
    if (has_previous_sample_) {
        auto time_delta = std::chrono::duration<double>(
            metrics.timestamp - prev_timestamp_).count();
        
        if (time_delta > 0.0) {
            // Hardware interrupt rate
            if (metrics.interrupts_total >= prev_interrupts_total_) {
                metrics.interrupts_per_sec = static_cast<double>(
                    metrics.interrupts_total - prev_interrupts_total_) / time_delta;
            }

            // Soft interrupt rate
            if (metrics.soft_interrupts_total >= prev_soft_interrupts_total_) {
                metrics.soft_interrupts_per_sec = static_cast<double>(
                    metrics.soft_interrupts_total - prev_soft_interrupts_total_) / time_delta;
            }
        }
    }

    // Store current values for next rate calculation
    prev_interrupts_total_ = metrics.interrupts_total;
    prev_soft_interrupts_total_ = metrics.soft_interrupts_total;
    prev_timestamp_ = metrics.timestamp;
    has_previous_sample_ = true;

    return metrics;
}

bool interrupt_info_collector::is_interrupt_monitoring_available() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!availability_checked_) {
        available_ = check_availability_impl();
        availability_checked_ = true;
    }
    return available_;
}

interrupt_metrics interrupt_info_collector::collect_metrics() {
    std::lock_guard<std::mutex> lock(mutex_);
    return collect_metrics_impl();
}

}  // namespace monitoring
}  // namespace kcenon

#endif  // defined(__linux__)
