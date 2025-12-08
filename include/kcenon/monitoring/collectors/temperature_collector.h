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
 * @file temperature_collector.h
 * @brief Hardware temperature monitoring collector
 *
 * This file provides hardware temperature monitoring using platform-specific
 * APIs to gather thermal sensor data:
 * - Linux: /sys/class/thermal/thermal_zone* or lm-sensors
 * - macOS: IOKit SMC (System Management Controller)
 * - Windows: WMI (MSAcpi_ThermalZoneTemperature)
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
 * @enum sensor_type
 * @brief Type of temperature sensor
 */
enum class sensor_type {
    unknown,      ///< Unknown sensor type
    cpu,          ///< CPU temperature sensor
    gpu,          ///< GPU temperature sensor
    motherboard,  ///< Motherboard/chipset sensor
    storage,      ///< Storage device sensor
    ambient,      ///< Ambient/case temperature
    other         ///< Other sensor type
};

/**
 * @brief Convert sensor_type to string representation
 */
inline std::string sensor_type_to_string(sensor_type type) {
    switch (type) {
        case sensor_type::cpu:
            return "cpu";
        case sensor_type::gpu:
            return "gpu";
        case sensor_type::motherboard:
            return "motherboard";
        case sensor_type::storage:
            return "storage";
        case sensor_type::ambient:
            return "ambient";
        case sensor_type::other:
            return "other";
        default:
            return "unknown";
    }
}

/**
 * @struct temperature_sensor_info
 * @brief Information about a temperature sensor
 */
struct temperature_sensor_info {
    std::string id;         ///< Unique sensor identifier
    std::string name;       ///< Human-readable sensor name
    std::string zone_path;  ///< Platform-specific path (e.g., /sys/class/thermal/thermal_zone0)
    sensor_type type{sensor_type::unknown};  ///< Sensor type classification
};

/**
 * @struct temperature_reading
 * @brief A single temperature reading from a sensor
 */
struct temperature_reading {
    temperature_sensor_info sensor;          ///< Sensor information
    double temperature_celsius{0.0};         ///< Current temperature in Celsius
    double critical_threshold_celsius{0.0};  ///< Critical temperature threshold (if available)
    double warning_threshold_celsius{0.0};   ///< Warning threshold (if available)
    bool thresholds_available{false};        ///< Whether thresholds are available
    bool is_critical{false};                 ///< True if temperature exceeds critical threshold
    bool is_warning{false};                  ///< True if temperature exceeds warning threshold
    std::chrono::system_clock::time_point timestamp;  ///< Reading timestamp
};

/**
 * @class temperature_info_collector
 * @brief Platform-specific temperature data collector implementation
 *
 * This class handles the low-level platform-specific operations for
 * enumerating thermal zones and reading temperature values.
 */
class temperature_info_collector {
   public:
    temperature_info_collector();
    ~temperature_info_collector();

    // Non-copyable, non-moveable due to internal state
    temperature_info_collector(const temperature_info_collector&) = delete;
    temperature_info_collector& operator=(const temperature_info_collector&) = delete;
    temperature_info_collector(temperature_info_collector&&) = delete;
    temperature_info_collector& operator=(temperature_info_collector&&) = delete;

    /**
     * Check if thermal monitoring is available on this system
     * @return True if temperature sensors can be read
     */
    bool is_thermal_available() const;

    /**
     * Enumerate all available temperature sensors
     * @return Vector of sensor information
     */
    std::vector<temperature_sensor_info> enumerate_sensors();

    /**
     * Read temperature from a specific sensor
     * @param sensor Sensor information
     * @return Temperature reading
     */
    temperature_reading read_temperature(const temperature_sensor_info& sensor);

    /**
     * Read temperatures from all available sensors
     * @return Vector of temperature readings
     */
    std::vector<temperature_reading> read_all_temperatures();

   private:
    mutable std::mutex mutex_;
    mutable bool thermal_checked_{false};
    mutable bool thermal_available_{false};
    std::vector<temperature_sensor_info> cached_sensors_;

    // Platform-specific helper methods
    std::vector<temperature_sensor_info> enumerate_sensors_impl();
    temperature_reading read_temperature_impl(const temperature_sensor_info& sensor);
};

/**
 * @class temperature_collector
 * @brief Hardware temperature monitoring collector
 *
 * Collects hardware temperature data from available thermal sensors
 * with cross-platform support. Gracefully degrades when sensors are
 * not available or when read access is restricted.
 */
class temperature_collector {
   public:
    temperature_collector();
    ~temperature_collector() = default;

    // Non-copyable, non-moveable due to internal state
    temperature_collector(const temperature_collector&) = delete;
    temperature_collector& operator=(const temperature_collector&) = delete;
    temperature_collector(temperature_collector&&) = delete;
    temperature_collector& operator=(temperature_collector&&) = delete;

    /**
     * Initialize the collector with configuration
     * @param config Configuration options
     * @return true if initialization successful
     */
    bool initialize(const std::unordered_map<std::string, std::string>& config);

    /**
     * Collect temperature metrics from all sensors
     * @return Vector of collected metrics
     */
    std::vector<metric> collect();

    /**
     * Get the name of this collector
     * @return Collector name
     */
    std::string get_name() const { return "temperature_collector"; }

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
     * Get last collected temperature readings
     * @return Vector of temperature readings
     */
    std::vector<temperature_reading> get_last_readings() const;

    /**
     * Check if temperature monitoring is available
     * @return True if thermal sensors are accessible
     */
    bool is_thermal_available() const;

   private:
    std::unique_ptr<temperature_info_collector> collector_;

    // Configuration
    bool enabled_{true};
    bool collect_thresholds_{true};
    bool collect_warnings_{true};

    // Statistics
    mutable std::mutex stats_mutex_;
    std::atomic<size_t> collection_count_{0};
    std::atomic<size_t> collection_errors_{0};
    std::atomic<size_t> sensors_found_{0};
    std::vector<temperature_reading> last_readings_;

    // Helper methods
    metric create_metric(const std::string& name, double value, const temperature_reading& reading,
                         const std::string& unit = "") const;
    void add_sensor_metrics(std::vector<metric>& metrics, const temperature_reading& reading);
};

}  // namespace monitoring
}  // namespace kcenon
