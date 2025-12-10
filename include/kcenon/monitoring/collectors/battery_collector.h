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
 * @file battery_collector.h
 * @brief Battery status monitoring collector
 *
 * This file provides battery status monitoring using platform-specific APIs
 * to gather battery level, charging status, and health information.
 *
 * Platform APIs:
 * - Linux: /sys/class/power_supply/BAT0/ sysfs files
 * - macOS: IOKit (IOPSCopyPowerSourcesInfo)
 * - Windows: GetSystemPowerStatus() or WMI Win32_Battery
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

namespace kcenon {
namespace monitoring {

/**
 * @enum battery_status
 * @brief Current battery charging status
 */
enum class battery_status {
    unknown,      ///< Unknown status
    charging,     ///< Battery is charging
    discharging,  ///< Battery is discharging
    not_charging, ///< Battery is not charging (plugged in but not charging)
    full          ///< Battery is fully charged
};

/**
 * @brief Convert battery_status to string representation
 */
inline std::string battery_status_to_string(battery_status status) {
    switch (status) {
        case battery_status::charging:
            return "charging";
        case battery_status::discharging:
            return "discharging";
        case battery_status::not_charging:
            return "not_charging";
        case battery_status::full:
            return "full";
        default:
            return "unknown";
    }
}

/**
 * @struct battery_info
 * @brief Information about a battery source
 */
struct battery_info {
    std::string id;          ///< Unique battery identifier (e.g., "BAT0")
    std::string name;        ///< Human-readable battery name
    std::string path;        ///< Platform-specific path (e.g., /sys/class/power_supply/BAT0)
    std::string manufacturer;///< Battery manufacturer
    std::string model;       ///< Battery model name
    std::string serial;      ///< Battery serial number
    std::string technology;  ///< Battery technology (e.g., Li-ion, Li-poly)
};

/**
 * @struct battery_reading
 * @brief A single battery reading
 */
struct battery_reading {
    battery_info info;  ///< Battery information

    // Basic metrics
    double level_percent{0.0};         ///< Current charge percentage (0-100)
    battery_status status{battery_status::unknown}; ///< Current charging status
    bool is_charging{false};           ///< True if battery is charging
    bool ac_connected{false};          ///< True if AC power is connected

    // Time estimates
    int64_t time_to_empty_seconds{-1}; ///< Estimated time to empty (-1 if unavailable)
    int64_t time_to_full_seconds{-1};  ///< Estimated time to full (-1 if unavailable)

    // Capacity metrics
    double design_capacity_wh{0.0};    ///< Original design capacity in Wh
    double full_charge_capacity_wh{0.0}; ///< Current full charge capacity in Wh
    double current_capacity_wh{0.0};   ///< Current energy stored in Wh
    double health_percent{0.0};        ///< Battery health (full_charge/design * 100)

    // Electrical metrics
    double voltage_volts{0.0};         ///< Current voltage in Volts
    double current_amps{0.0};          ///< Current in Amps (positive=charging)
    double power_watts{0.0};           ///< Current power in Watts

    // Thermal
    double temperature_celsius{0.0};   ///< Battery temperature in Celsius
    bool temperature_available{false}; ///< Whether temperature is available

    // Cycle count
    int64_t cycle_count{-1};           ///< Battery charge cycles (-1 if unavailable)

    // Availability flags
    bool battery_present{false};       ///< Whether battery is present
    bool metrics_available{false};     ///< Whether metrics are available

    std::chrono::system_clock::time_point timestamp;  ///< Reading timestamp
};

/**
 * @class battery_info_collector
 * @brief Platform-specific battery data collector implementation
 *
 * This class handles the low-level platform-specific operations for
 * enumerating batteries and reading battery status.
 */
class battery_info_collector {
   public:
    battery_info_collector();
    ~battery_info_collector();

    // Non-copyable, non-moveable due to internal state
    battery_info_collector(const battery_info_collector&) = delete;
    battery_info_collector& operator=(const battery_info_collector&) = delete;
    battery_info_collector(battery_info_collector&&) = delete;
    battery_info_collector& operator=(battery_info_collector&&) = delete;

    /**
     * Check if battery monitoring is available on this system
     * @return True if batteries can be read
     */
    bool is_battery_available() const;

    /**
     * Enumerate all available batteries
     * @return Vector of battery information
     */
    std::vector<battery_info> enumerate_batteries();

    /**
     * Read status from a specific battery
     * @param battery Battery information
     * @return Battery reading
     */
    battery_reading read_battery(const battery_info& battery);

    /**
     * Read status from all available batteries
     * @return Vector of battery readings
     */
    std::vector<battery_reading> read_all_batteries();

   private:
    mutable std::mutex mutex_;
    mutable bool battery_checked_{false};
    mutable bool battery_available_{false};
    std::vector<battery_info> cached_batteries_;

    // Platform-specific helper methods
    std::vector<battery_info> enumerate_batteries_impl();
    battery_reading read_battery_impl(const battery_info& battery);
};

/**
 * @class battery_collector
 * @brief Battery status monitoring collector
 *
 * Collects battery status metrics from available batteries
 * with cross-platform support. Returns empty/default metrics when
 * no battery is present.
 */
class battery_collector {
   public:
    battery_collector();
    ~battery_collector() = default;

    // Non-copyable, non-moveable due to internal state
    battery_collector(const battery_collector&) = delete;
    battery_collector& operator=(const battery_collector&) = delete;
    battery_collector(battery_collector&&) = delete;
    battery_collector& operator=(battery_collector&&) = delete;

    /**
     * Initialize the collector with configuration
     * @param config Configuration options:
     *   - "enabled": "true"/"false" (default: true)
     *   - "collect_health": "true"/"false" (default: true)
     *   - "collect_thermal": "true"/"false" (default: true)
     * @return true if initialization successful
     */
    bool initialize(const std::unordered_map<std::string, std::string>& config);

    /**
     * Collect battery metrics from all batteries
     * @return Vector of collected metrics
     */
    std::vector<metric> collect();

    /**
     * Get the name of this collector
     * @return Collector name
     */
    std::string get_name() const { return "battery_collector"; }

    /**
     * Get supported metric types
     * @return Vector of supported metric type names
     */
    std::vector<std::string> get_metric_types() const;

    /**
     * Check if the collector is healthy
     * @return true if collector is operational
     */
    bool is_healthy() const;

    /**
     * Get collector statistics
     * @return Map of statistic name to value
     */
    std::unordered_map<std::string, double> get_statistics() const;

    /**
     * Get last collected battery readings
     * @return Vector of battery readings
     */
    std::vector<battery_reading> get_last_readings() const;

    /**
     * Check if battery monitoring is available
     * @return True if batteries are accessible
     */
    bool is_battery_available() const;

   private:
    std::unique_ptr<battery_info_collector> collector_;

    // Configuration
    bool enabled_{true};
    bool collect_health_{true};
    bool collect_thermal_{true};

    // Statistics
    mutable std::mutex stats_mutex_;
    std::atomic<size_t> collection_count_{0};
    std::atomic<size_t> collection_errors_{0};
    std::atomic<size_t> batteries_found_{0};
    std::vector<battery_reading> last_readings_;

    // Helper methods
    metric create_metric(const std::string& name, double value,
                         const battery_reading& reading,
                         const std::string& unit = "") const;
    void add_battery_metrics(std::vector<metric>& metrics,
                             const battery_reading& reading);
};

}  // namespace monitoring
}  // namespace kcenon
