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

#pragma once

#include <kcenon/monitoring/platform/metrics_provider.h>

namespace kcenon {
namespace monitoring {
namespace platform {

/**
 * @class null_metrics_provider
 * @brief Null object implementation of the metrics_provider interface
 *
 * This class provides a safe fallback implementation for unsupported platforms.
 * All methods return empty/default values with available=false, ensuring the
 * system doesn't crash on unsupported platforms while clearly indicating that
 * metrics are not available.
 *
 * Implements the Null Object pattern to avoid null pointer checks throughout
 * the codebase.
 */
class null_metrics_provider : public metrics_provider {
   public:
    null_metrics_provider() = default;
    ~null_metrics_provider() override = default;

    std::string get_platform_name() const override { return "unknown"; }

    // Battery - not available
    bool is_battery_available() const override { return false; }
    std::vector<battery_reading> get_battery_readings() override { return {}; }

    // Temperature - not available
    bool is_temperature_available() const override { return false; }
    std::vector<temperature_reading> get_temperature_readings() override { return {}; }

    // Uptime - return empty
    uptime_info get_uptime() override {
        uptime_info info;
        info.available = false;
        return info;
    }

    // Context switches - return empty
    context_switch_info get_context_switches() override {
        context_switch_info info;
        info.available = false;
        return info;
    }

    // File descriptors - return empty
    fd_info get_fd_stats() override {
        fd_info info;
        info.available = false;
        return info;
    }

    // Inodes - return empty
    std::vector<inode_info> get_inode_stats() override { return {}; }

    // TCP states - return empty
    tcp_state_info get_tcp_states() override {
        tcp_state_info info;
        info.available = false;
        return info;
    }

    // Socket buffers - return empty
    socket_buffer_info get_socket_buffer_stats() override {
        socket_buffer_info info;
        info.available = false;
        return info;
    }

    // Interrupts - return empty
    std::vector<interrupt_info> get_interrupt_stats() override { return {}; }

    // Power - not available
    bool is_power_available() const override { return false; }
    power_info get_power_info() override {
        power_info info;
        info.available = false;
        return info;
    }

    // GPU - not available
    bool is_gpu_available() const override { return false; }
    std::vector<gpu_info> get_gpu_info() override { return {}; }

    // Security - return empty
    security_info get_security_info() override {
        security_info info;
        info.available = false;
        return info;
    }
};

}  // namespace platform
}  // namespace monitoring
}  // namespace kcenon
