// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

#if defined(_WIN32)

#include <kcenon/monitoring/platform/metrics_provider.h>

namespace kcenon {
namespace monitoring {
namespace platform {

/**
 * @class windows_metrics_provider
 * @brief Windows-specific implementation of the metrics_provider interface
 *
 * This class provides system metrics collection using Windows-specific APIs:
 * - WMI for hardware and system metrics
 * - Performance Counters
 * - Win32 API
 */
class windows_metrics_provider : public metrics_provider {
   public:
    windows_metrics_provider();
    ~windows_metrics_provider() override = default;

    std::string get_platform_name() const override { return "windows"; }

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

#endif  // _WIN32
