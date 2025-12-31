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

#if defined(__linux__)

#include <kcenon/monitoring/platform/metrics_provider.h>

namespace kcenon {
namespace monitoring {
namespace platform {

/**
 * @class linux_metrics_provider
 * @brief Linux-specific implementation of the metrics_provider interface
 *
 * This class provides system metrics collection using Linux-specific APIs:
 * - /sys filesystem for hardware metrics
 * - /proc filesystem for process and system metrics
 * - Direct kernel interfaces where available
 */
class linux_metrics_provider : public metrics_provider {
   public:
    linux_metrics_provider();
    ~linux_metrics_provider() override = default;

    std::string get_platform_name() const override { return "linux"; }

    // Battery
    bool is_battery_available() const override;
    std::vector<battery_reading> get_battery_readings() override;

    // Temperature
    bool is_temperature_available() const override;
    std::vector<temperature_reading> get_temperature_readings() override;

    // Uptime
    uptime_info get_uptime() override;

    // Context switches
    context_switch_info get_context_switches() override;

    // File descriptors
    fd_info get_fd_stats() override;

    // Inodes
    std::vector<inode_info> get_inode_stats() override;

    // TCP states
    tcp_state_info get_tcp_states() override;

    // Socket buffers
    socket_buffer_info get_socket_buffer_stats() override;

    // Interrupts
    std::vector<interrupt_info> get_interrupt_stats() override;

    // Power
    bool is_power_available() const override;
    power_info get_power_info() override;

    // GPU
    bool is_gpu_available() const override;
    std::vector<gpu_info> get_gpu_info() override;

    // Security
    security_info get_security_info() override;

   private:
    // Cached availability flags
    mutable bool battery_checked_{false};
    mutable bool battery_available_{false};
    mutable bool temperature_checked_{false};
    mutable bool temperature_available_{false};
    mutable bool power_checked_{false};
    mutable bool power_available_{false};
    mutable bool gpu_checked_{false};
    mutable bool gpu_available_{false};
};

}  // namespace platform
}  // namespace monitoring
}  // namespace kcenon

#endif  // __linux__
