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
 * @file power_collector.h
 * @brief Power consumption monitoring collector
 *
 * This file provides power consumption monitoring using platform-specific
 * APIs to gather power and energy data:
 * - Linux: RAPL (Running Average Power Limit) via /sys/class/powercap/intel-rapl/
 *          and /sys/class/power_supply/ for battery info
 * - macOS: IOKit SMC (System Management Controller)
 * - Windows: WMI (Win32_Battery) for battery metrics
 */

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "../interfaces/metric_types_adapter.h"
#include "../plugins/collector_plugin.h"

namespace kcenon {
namespace monitoring {

/**
 * @enum power_source_type
 * @brief Type of power source
 */
enum class power_source_type {
    unknown,   ///< Unknown power source type
    battery,   ///< Battery power source
    ac,        ///< AC adapter / mains power
    usb,       ///< USB power delivery
    wireless,  ///< Wireless charging
    cpu,       ///< CPU power domain (RAPL)
    gpu,       ///< GPU power domain
    memory,    ///< Memory/DRAM power domain (RAPL)
    package,   ///< Processor package power domain (RAPL)
    platform,  ///< Platform/system power domain
    other      ///< Other power source type
};

/**
 * @brief Convert power_source_type to string representation
 */
inline std::string power_source_type_to_string(power_source_type type) {
    switch (type) {
        case power_source_type::battery:
            return "battery";
        case power_source_type::ac:
            return "ac";
        case power_source_type::usb:
            return "usb";
        case power_source_type::wireless:
            return "wireless";
        case power_source_type::cpu:
            return "cpu";
        case power_source_type::gpu:
            return "gpu";
        case power_source_type::memory:
            return "memory";
        case power_source_type::package:
            return "package";
        case power_source_type::platform:
            return "platform";
        case power_source_type::other:
            return "other";
        default:
            return "unknown";
    }
}

/**
 * @struct power_source_info
 * @brief Information about a power source
 */
struct power_source_info {
    std::string id;    ///< Unique source identifier
    std::string name;  ///< Human-readable source name
    std::string path;  ///< Platform-specific path (e.g., /sys/class/power_supply/BAT0)
    power_source_type type{power_source_type::unknown};  ///< Power source type classification
};

/**
 * @struct power_reading
 * @brief A single power reading from a source
 */
struct power_reading {
    power_source_info source;  ///< Power source information

    // Power metrics
    double power_watts{0.0};        ///< Current power consumption in Watts
    double energy_joules{0.0};      ///< Cumulative energy consumed in Joules
    double power_limit_watts{0.0};  ///< Power limit/TDP in Watts (if available)

    // Voltage metrics
    double voltage_volts{0.0};  ///< Current voltage in Volts

    // Battery-specific metrics
    double battery_percent{0.0};  ///< Battery charge percentage (0-100)
    double battery_capacity_wh{0.0};  ///< Battery capacity in Watt-hours
    double battery_charge_rate{0.0};  ///< Charging/discharging rate in Watts
    bool is_charging{false};          ///< True if battery is charging
    bool is_discharging{false};       ///< True if battery is discharging
    bool is_full{false};              ///< True if battery is fully charged

    // Availability flags
    bool power_available{false};   ///< Whether power metrics are available
    bool battery_available{false}; ///< Whether battery metrics are available
    bool limits_available{false};  ///< Whether power limit info is available

    std::chrono::system_clock::time_point timestamp;  ///< Reading timestamp
};

// Forward declaration
namespace platform {
class metrics_provider;
}  // namespace platform

/**
 * @class power_info_collector
 * @brief Power data collector using platform abstraction layer
 *
 * This class provides power data collection using the unified
 * metrics_provider interface, eliminating platform-specific code.
 */
class power_info_collector {
   public:
    power_info_collector();
    ~power_info_collector();

    // Non-copyable, non-moveable due to internal state
    power_info_collector(const power_info_collector&) = delete;
    power_info_collector& operator=(const power_info_collector&) = delete;
    power_info_collector(power_info_collector&&) = delete;
    power_info_collector& operator=(power_info_collector&&) = delete;

    /**
     * Check if power monitoring is available on this system
     * @return True if power sources can be read
     */
    bool is_power_available() const;

    /**
     * Enumerate all available power sources
     * @return Vector of power source information
     */
    std::vector<power_source_info> enumerate_sources();

    /**
     * Read power from all available sources
     * @return Vector of power readings
     */
    std::vector<power_reading> read_all_power();

   private:
    std::unique_ptr<platform::metrics_provider> provider_;
};

/**
 * @class power_collector
 * @brief Power consumption monitoring collector implementing collector_plugin interface
 *
 * Collects power consumption data from available power sources
 * with cross-platform support. Gracefully degrades when power metrics
 * are not available or when read access is restricted.
 */
class power_collector : public collector_plugin {
   public:
    power_collector();
    ~power_collector() override = default;

    // Non-copyable, non-moveable due to internal state
    power_collector(const power_collector&) = delete;
    power_collector& operator=(const power_collector&) = delete;
    power_collector(power_collector&&) = delete;
    power_collector& operator=(power_collector&&) = delete;

    // collector_plugin implementation
    auto name() const -> std::string_view override { return "power"; }
    auto collect() -> std::vector<metric> override;
    auto interval() const -> std::chrono::milliseconds override { return std::chrono::seconds(10); }
    auto is_available() const -> bool override;
    auto get_metric_types() const -> std::vector<std::string> override;

    auto get_metadata() const -> plugin_metadata override {
        return plugin_metadata{
            .name = name(),
            .description = "Power consumption metrics from various sources",
            .category = plugin_category::hardware,
            .version = "1.0.0",
            .dependencies = {},
            .requires_platform_support = true
        };
    }

    auto initialize(const config_map& config) -> bool override;
    void shutdown() override {}
    auto get_statistics() const -> stats_map override;

    // Legacy compatibility (deprecated)
    bool is_healthy() const;

    /**
     * Get last collected power readings
     * @return Vector of power readings
     */
    std::vector<power_reading> get_last_readings() const;

    /**
     * Check if power monitoring is available
     * @return True if power sources are accessible
     */
    bool is_power_available() const;

   private:
    std::unique_ptr<power_info_collector> collector_;

    // Configuration
    bool enabled_{true};
    bool collect_battery_{true};
    bool collect_rapl_{true};

    // Statistics
    mutable std::mutex stats_mutex_;
    std::atomic<size_t> collection_count_{0};
    std::atomic<size_t> collection_errors_{0};
    std::atomic<size_t> sources_found_{0};
    std::vector<power_reading> last_readings_;

    // Helper methods
    metric create_metric(const std::string& name, double value, const power_reading& reading,
                         const std::string& unit = "") const;
    void add_source_metrics(std::vector<metric>& metrics, const power_reading& reading);
};

}  // namespace monitoring
}  // namespace kcenon
