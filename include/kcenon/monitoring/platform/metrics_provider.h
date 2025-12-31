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

/**
 * @file metrics_provider.h
 * @brief Platform abstraction layer for system metrics collection
 *
 * This file provides a unified interface for collecting system metrics
 * across different platforms (Linux, macOS, Windows). Each platform
 * implements the metrics_provider interface with platform-specific code.
 *
 * Usage:
 * @code
 *   auto provider = metrics_provider::create();
 *   auto battery_readings = provider->get_battery_readings();
 *   auto temp_readings = provider->get_temperature_readings();
 * @endcode
 */

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// Forward declarations for existing types
#include "../collectors/battery_collector.h"
#include "../collectors/temperature_collector.h"

namespace kcenon {
namespace monitoring {
namespace platform {

/**
 * @struct uptime_info
 * @brief System uptime information
 */
struct uptime_info {
    int64_t uptime_seconds{0};          ///< System uptime in seconds
    int64_t idle_seconds{0};            ///< Total idle time in seconds
    std::chrono::system_clock::time_point boot_time;  ///< System boot time
    bool available{false};              ///< Whether uptime info is available
};

/**
 * @struct context_switch_info
 * @brief Context switch statistics
 */
struct context_switch_info {
    uint64_t total_switches{0};         ///< Total context switches
    uint64_t voluntary_switches{0};     ///< Voluntary context switches
    uint64_t involuntary_switches{0};   ///< Involuntary context switches
    double switches_per_second{0.0};    ///< Context switches per second
    bool available{false};              ///< Whether info is available
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @struct fd_info
 * @brief File descriptor statistics
 */
struct fd_info {
    uint64_t open_fds{0};               ///< Currently open file descriptors
    uint64_t max_fds{0};                ///< Maximum file descriptors allowed
    double usage_percent{0.0};          ///< FD usage percentage
    bool available{false};              ///< Whether info is available
};

/**
 * @struct inode_info
 * @brief Inode statistics
 */
struct inode_info {
    uint64_t total_inodes{0};           ///< Total inodes
    uint64_t used_inodes{0};            ///< Used inodes
    uint64_t free_inodes{0};            ///< Free inodes
    double usage_percent{0.0};          ///< Inode usage percentage
    std::string filesystem;             ///< Filesystem path
    bool available{false};              ///< Whether info is available
};

/**
 * @struct tcp_state_info
 * @brief TCP connection state statistics
 */
struct tcp_state_info {
    uint64_t established{0};            ///< ESTABLISHED connections
    uint64_t syn_sent{0};               ///< SYN_SENT connections
    uint64_t syn_recv{0};               ///< SYN_RECV connections
    uint64_t fin_wait1{0};              ///< FIN_WAIT1 connections
    uint64_t fin_wait2{0};              ///< FIN_WAIT2 connections
    uint64_t time_wait{0};              ///< TIME_WAIT connections
    uint64_t close_wait{0};             ///< CLOSE_WAIT connections
    uint64_t last_ack{0};               ///< LAST_ACK connections
    uint64_t listen{0};                 ///< LISTEN connections
    uint64_t closing{0};                ///< CLOSING connections
    uint64_t total{0};                  ///< Total connections
    bool available{false};              ///< Whether info is available
};

/**
 * @struct socket_buffer_info
 * @brief Socket buffer statistics
 */
struct socket_buffer_info {
    uint64_t rx_buffer_size{0};         ///< Receive buffer size
    uint64_t tx_buffer_size{0};         ///< Transmit buffer size
    uint64_t rx_buffer_used{0};         ///< Receive buffer used
    uint64_t tx_buffer_used{0};         ///< Transmit buffer used
    bool available{false};              ///< Whether info is available
};

/**
 * @struct interrupt_info
 * @brief Interrupt statistics
 */
struct interrupt_info {
    std::string name;                   ///< Interrupt name/description
    uint64_t count{0};                  ///< Interrupt count
    uint64_t irq_number{0};             ///< IRQ number
    bool available{false};              ///< Whether info is available
};

/**
 * @struct power_info
 * @brief Power consumption information
 */
struct power_info {
    double power_watts{0.0};            ///< Current power consumption in watts
    double voltage_volts{0.0};          ///< Voltage in volts
    double current_amps{0.0};           ///< Current in amps
    std::string source;                 ///< Power source (AC, battery, etc.)
    bool available{false};              ///< Whether info is available
};

/**
 * @struct gpu_info
 * @brief GPU information and metrics
 */
struct gpu_info {
    std::string name;                   ///< GPU name
    std::string vendor;                 ///< GPU vendor
    double usage_percent{0.0};          ///< GPU usage percentage
    double memory_used_mb{0.0};         ///< GPU memory used in MB
    double memory_total_mb{0.0};        ///< GPU memory total in MB
    double temperature_celsius{0.0};    ///< GPU temperature in Celsius
    double power_watts{0.0};            ///< GPU power consumption in watts
    bool available{false};              ///< Whether info is available
};

/**
 * @struct security_info
 * @brief Security-related metrics
 */
struct security_info {
    bool firewall_enabled{false};       ///< Whether firewall is enabled
    uint64_t failed_login_attempts{0};  ///< Failed login attempts
    uint64_t active_sessions{0};        ///< Active user sessions
    std::string security_level;         ///< Security level description
    bool available{false};              ///< Whether info is available
};

/**
 * @class metrics_provider
 * @brief Abstract interface for platform-specific metrics collection
 *
 * This class defines the interface that all platform-specific
 * implementations must follow. Use the static create() method
 * to get a platform-appropriate implementation.
 */
class metrics_provider {
   public:
    virtual ~metrics_provider() = default;

    // Non-copyable, non-moveable
    metrics_provider(const metrics_provider&) = delete;
    metrics_provider& operator=(const metrics_provider&) = delete;
    metrics_provider(metrics_provider&&) = delete;
    metrics_provider& operator=(metrics_provider&&) = delete;

    /**
     * @brief Create a platform-specific metrics provider
     * @return Unique pointer to the appropriate platform implementation
     */
    static std::unique_ptr<metrics_provider> create();

    /**
     * @brief Get the platform name
     * @return Platform identifier (e.g., "linux", "macos", "windows")
     */
    virtual std::string get_platform_name() const = 0;

    // =========================================================================
    // Battery Metrics
    // =========================================================================

    /**
     * @brief Check if battery monitoring is available
     * @return True if battery metrics can be collected
     */
    virtual bool is_battery_available() const = 0;

    /**
     * @brief Get battery readings from all available batteries
     * @return Vector of battery readings
     */
    virtual std::vector<battery_reading> get_battery_readings() = 0;

    // =========================================================================
    // Temperature Metrics
    // =========================================================================

    /**
     * @brief Check if temperature monitoring is available
     * @return True if temperature metrics can be collected
     */
    virtual bool is_temperature_available() const = 0;

    /**
     * @brief Get temperature readings from all available sensors
     * @return Vector of temperature readings
     */
    virtual std::vector<temperature_reading> get_temperature_readings() = 0;

    // =========================================================================
    // Uptime Metrics
    // =========================================================================

    /**
     * @brief Get system uptime information
     * @return Uptime info structure
     */
    virtual uptime_info get_uptime() = 0;

    // =========================================================================
    // Context Switch Metrics
    // =========================================================================

    /**
     * @brief Get context switch statistics
     * @return Context switch info structure
     */
    virtual context_switch_info get_context_switches() = 0;

    // =========================================================================
    // File Descriptor Metrics
    // =========================================================================

    /**
     * @brief Get file descriptor statistics
     * @return File descriptor info structure
     */
    virtual fd_info get_fd_stats() = 0;

    // =========================================================================
    // Inode Metrics
    // =========================================================================

    /**
     * @brief Get inode statistics for all filesystems
     * @return Vector of inode info structures
     */
    virtual std::vector<inode_info> get_inode_stats() = 0;

    // =========================================================================
    // TCP State Metrics
    // =========================================================================

    /**
     * @brief Get TCP connection state statistics
     * @return TCP state info structure
     */
    virtual tcp_state_info get_tcp_states() = 0;

    // =========================================================================
    // Socket Buffer Metrics
    // =========================================================================

    /**
     * @brief Get socket buffer statistics
     * @return Socket buffer info structure
     */
    virtual socket_buffer_info get_socket_buffer_stats() = 0;

    // =========================================================================
    // Interrupt Metrics
    // =========================================================================

    /**
     * @brief Get interrupt statistics
     * @return Vector of interrupt info structures
     */
    virtual std::vector<interrupt_info> get_interrupt_stats() = 0;

    // =========================================================================
    // Power Metrics
    // =========================================================================

    /**
     * @brief Check if power monitoring is available
     * @return True if power metrics can be collected
     */
    virtual bool is_power_available() const = 0;

    /**
     * @brief Get power consumption information
     * @return Power info structure
     */
    virtual power_info get_power_info() = 0;

    // =========================================================================
    // GPU Metrics
    // =========================================================================

    /**
     * @brief Check if GPU monitoring is available
     * @return True if GPU metrics can be collected
     */
    virtual bool is_gpu_available() const = 0;

    /**
     * @brief Get GPU information and metrics
     * @return Vector of GPU info structures
     */
    virtual std::vector<gpu_info> get_gpu_info() = 0;

    // =========================================================================
    // Security Metrics
    // =========================================================================

    /**
     * @brief Get security-related metrics
     * @return Security info structure
     */
    virtual security_info get_security_info() = 0;

   protected:
    metrics_provider() = default;
};

}  // namespace platform
}  // namespace monitoring
}  // namespace kcenon
