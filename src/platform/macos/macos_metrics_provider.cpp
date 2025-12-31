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

#include "macos_metrics_provider.h"

#if defined(__APPLE__)

#include <mach/mach.h>
#include <sys/sysctl.h>
#include <sys/types.h>

namespace kcenon {
namespace monitoring {
namespace platform {

macos_metrics_provider::macos_metrics_provider() = default;

// =========================================================================
// Battery
// =========================================================================

bool macos_metrics_provider::is_battery_available() const {
    if (!battery_checked_) {
        // TODO: Implement using IOKit
        battery_available_ = false;
        battery_checked_ = true;
    }
    return battery_available_;
}

std::vector<battery_reading> macos_metrics_provider::get_battery_readings() {
    // TODO: Migrate from macos_battery_metrics.cpp
    return {};
}

// =========================================================================
// Temperature
// =========================================================================

bool macos_metrics_provider::is_temperature_available() const {
    if (!temperature_checked_) {
        // TODO: Implement using SMC
        temperature_available_ = false;
        temperature_checked_ = true;
    }
    return temperature_available_;
}

std::vector<temperature_reading> macos_metrics_provider::get_temperature_readings() {
    // TODO: Migrate from macos_temperature.cpp
    return {};
}

// =========================================================================
// Uptime
// =========================================================================

uptime_info macos_metrics_provider::get_uptime() {
    uptime_info info;

    struct timeval boottime;
    size_t len = sizeof(boottime);
    int mib[2] = {CTL_KERN, KERN_BOOTTIME};

    if (sysctl(mib, 2, &boottime, &len, nullptr, 0) == 0) {
        auto boot_time = std::chrono::system_clock::from_time_t(boottime.tv_sec);
        auto now = std::chrono::system_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - boot_time);

        info.uptime_seconds = uptime.count();
        info.boot_time = boot_time;
        info.available = true;
    }

    return info;
}

// =========================================================================
// Context Switches
// =========================================================================

context_switch_info macos_metrics_provider::get_context_switches() {
    context_switch_info info;
    // TODO: Migrate from macos_context_switch_metrics.cpp
    return info;
}

// =========================================================================
// File Descriptors
// =========================================================================

fd_info macos_metrics_provider::get_fd_stats() {
    fd_info info;
    // TODO: Migrate from macos_fd_metrics.cpp
    return info;
}

// =========================================================================
// Inodes
// =========================================================================

std::vector<inode_info> macos_metrics_provider::get_inode_stats() {
    // TODO: Migrate from macos_inode_metrics.cpp
    return {};
}

// =========================================================================
// TCP States
// =========================================================================

tcp_state_info macos_metrics_provider::get_tcp_states() {
    tcp_state_info info;
    // TODO: Migrate from macos_tcp_state_metrics.cpp
    return info;
}

// =========================================================================
// Socket Buffers
// =========================================================================

socket_buffer_info macos_metrics_provider::get_socket_buffer_stats() {
    socket_buffer_info info;
    // TODO: Migrate from macos_socket_buffer_metrics.cpp
    return info;
}

// =========================================================================
// Interrupts
// =========================================================================

std::vector<interrupt_info> macos_metrics_provider::get_interrupt_stats() {
    // TODO: Migrate from macos_interrupt_metrics.cpp
    return {};
}

// =========================================================================
// Power
// =========================================================================

bool macos_metrics_provider::is_power_available() const {
    if (!power_checked_) {
        // TODO: Implement using IOKit
        power_available_ = false;
        power_checked_ = true;
    }
    return power_available_;
}

power_info macos_metrics_provider::get_power_info() {
    power_info info;
    // TODO: Migrate from macos_power.cpp
    return info;
}

// =========================================================================
// GPU
// =========================================================================

bool macos_metrics_provider::is_gpu_available() const {
    if (!gpu_checked_) {
        // TODO: Implement using Metal/IOKit
        gpu_available_ = false;
        gpu_checked_ = true;
    }
    return gpu_available_;
}

std::vector<gpu_info> macos_metrics_provider::get_gpu_info() {
    // TODO: Migrate from macos_gpu_metrics.cpp
    return {};
}

// =========================================================================
// Security
// =========================================================================

security_info macos_metrics_provider::get_security_info() {
    security_info info;
    // TODO: Migrate from macos_security_metrics.cpp
    return info;
}

}  // namespace platform
}  // namespace monitoring
}  // namespace kcenon

#endif  // __APPLE__
