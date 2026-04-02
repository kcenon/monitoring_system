// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

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
