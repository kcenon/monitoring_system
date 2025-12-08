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

#if defined(_WIN32)

namespace kcenon {
namespace monitoring {

// Windows interrupt monitoring requires Performance Counters (PDH API).
// This is a stub implementation that returns unavailable metrics.
// Future implementation would use:
// - PDH_OpenQuery/PDH_AddCounter for \Processor(*)\Interrupts/sec
// - GetSystemInfo for per-CPU breakdown

interrupt_info_collector::interrupt_info_collector() = default;
interrupt_info_collector::~interrupt_info_collector() = default;

bool interrupt_info_collector::check_availability_impl() const {
    // Interrupt monitoring not implemented on Windows yet
    return false;
}

interrupt_metrics interrupt_info_collector::collect_metrics_impl() {
    interrupt_metrics metrics;
    metrics.timestamp = std::chrono::system_clock::now();
    metrics.metrics_available = false;
    metrics.soft_interrupts_available = false;
    return metrics;
}

bool interrupt_info_collector::is_interrupt_monitoring_available() const {
    return false;
}

interrupt_metrics interrupt_info_collector::collect_metrics() {
    std::lock_guard<std::mutex> lock(mutex_);
    return collect_metrics_impl();
}

}  // namespace monitoring
}  // namespace kcenon

#endif  // defined(_WIN32)
