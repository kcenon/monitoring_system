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

#include "windows_metrics_provider.h"

#if defined(_WIN32)

namespace kcenon {
namespace monitoring {
namespace platform {

windows_metrics_provider::windows_metrics_provider() = default;

bool windows_metrics_provider::is_battery_available() const {
    // TODO: Implement using GetSystemPowerStatus
    return false;
}

std::vector<battery_reading> windows_metrics_provider::get_battery_readings() {
    // TODO: Implement using GetSystemPowerStatus or WMI Win32_Battery
    return {};
}

bool windows_metrics_provider::is_temperature_available() const {
    // TODO: Implement using WMI MSAcpi_ThermalZoneTemperature
    return false;
}

std::vector<temperature_reading> windows_metrics_provider::get_temperature_readings() {
    // TODO: Implement using WMI
    return {};
}

uptime_info windows_metrics_provider::get_uptime() {
    uptime_info info;
    // TODO: Implement using GetTickCount64
    return info;
}

context_switch_info windows_metrics_provider::get_context_switches() {
    context_switch_info info;
    // TODO: Implement using Performance Counters
    return info;
}

fd_info windows_metrics_provider::get_fd_stats() {
    fd_info info;
    // TODO: Implement using handle counting
    return info;
}

std::vector<inode_info> windows_metrics_provider::get_inode_stats() {
    // Not applicable on Windows - NTFS uses MFT instead of inodes
    return {};
}

tcp_state_info windows_metrics_provider::get_tcp_states() {
    tcp_state_info info;
    // TODO: Implement using GetExtendedTcpTable
    return info;
}

socket_buffer_info windows_metrics_provider::get_socket_buffer_stats() {
    socket_buffer_info info;
    // TODO: Implement
    return info;
}

std::vector<interrupt_info> windows_metrics_provider::get_interrupt_stats() {
    // TODO: Implement using Performance Counters
    return {};
}

bool windows_metrics_provider::is_power_available() const {
    // TODO: Implement
    return false;
}

power_info windows_metrics_provider::get_power_info() {
    power_info info;
    // TODO: Implement using GetSystemPowerStatus
    return info;
}

bool windows_metrics_provider::is_gpu_available() const {
    // TODO: Implement using DXGI or WMI
    return false;
}

std::vector<gpu_info> windows_metrics_provider::get_gpu_info() {
    // TODO: Implement using DXGI or WMI Win32_VideoController
    return {};
}

security_info windows_metrics_provider::get_security_info() {
    security_info info;
    // TODO: Implement using Windows Security Center API
    return info;
}

}  // namespace platform
}  // namespace monitoring
}  // namespace kcenon

#endif  // _WIN32
