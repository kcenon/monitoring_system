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

#include <kcenon/monitoring/collectors/temperature_collector.h>

#if defined(__linux__)

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace kcenon {
namespace monitoring {

namespace {

// Thermal zone base path
constexpr const char* THERMAL_BASE_PATH = "/sys/class/thermal";

/**
 * Read a single-line value from a file
 */
std::string read_file_contents(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return "";
    }
    std::string content;
    std::getline(file, content);
    return content;
}

/**
 * Parse temperature value from millidegrees Celsius to Celsius
 */
double parse_temperature(const std::string& value_str) {
    try {
        int64_t millidegrees = std::stoll(value_str);
        return static_cast<double>(millidegrees) / 1000.0;
    } catch (...) {
        return 0.0;
    }
}

/**
 * Classify sensor type based on zone type string
 */
sensor_type classify_sensor(const std::string& type_str) {
    std::string lower_type = type_str;
    std::transform(lower_type.begin(), lower_type.end(), lower_type.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (lower_type.find("cpu") != std::string::npos ||
        lower_type.find("x86_pkg") != std::string::npos ||
        lower_type.find("coretemp") != std::string::npos ||
        lower_type.find("pkg-temp") != std::string::npos) {
        return sensor_type::cpu;
    }
    if (lower_type.find("gpu") != std::string::npos ||
        lower_type.find("nouveau") != std::string::npos ||
        lower_type.find("radeon") != std::string::npos ||
        lower_type.find("amdgpu") != std::string::npos) {
        return sensor_type::gpu;
    }
    if (lower_type.find("acpi") != std::string::npos ||
        lower_type.find("pch") != std::string::npos) {
        return sensor_type::motherboard;
    }
    if (lower_type.find("nvme") != std::string::npos ||
        lower_type.find("sata") != std::string::npos ||
        lower_type.find("storage") != std::string::npos) {
        return sensor_type::storage;
    }
    if (lower_type.find("ambient") != std::string::npos ||
        lower_type.find("case") != std::string::npos) {
        return sensor_type::ambient;
    }
    return sensor_type::unknown;
}

/**
 * Read trip point temperature (threshold)
 */
double read_trip_point(const std::filesystem::path& zone_path, int trip_index) {
    std::filesystem::path trip_path =
        zone_path / ("trip_point_" + std::to_string(trip_index) + "_temp");
    std::string value = read_file_contents(trip_path);
    if (!value.empty()) {
        return parse_temperature(value);
    }
    return 0.0;
}

/**
 * Read trip point type
 */
std::string read_trip_type(const std::filesystem::path& zone_path, int trip_index) {
    std::filesystem::path trip_path =
        zone_path / ("trip_point_" + std::to_string(trip_index) + "_type");
    return read_file_contents(trip_path);
}

}  // anonymous namespace

// temperature_info_collector implementation for Linux

temperature_info_collector::temperature_info_collector() = default;
temperature_info_collector::~temperature_info_collector() = default;

bool temperature_info_collector::is_thermal_available() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (thermal_checked_) {
        return thermal_available_;
    }

    thermal_checked_ = true;

    // Check if thermal base path exists
    std::filesystem::path thermal_path(THERMAL_BASE_PATH);
    if (!std::filesystem::exists(thermal_path) || !std::filesystem::is_directory(thermal_path)) {
        thermal_available_ = false;
        return false;
    }

    // Check if any thermal_zone directories exist
    try {
        for (const auto& entry : std::filesystem::directory_iterator(thermal_path)) {
            if (entry.is_directory() &&
                entry.path().filename().string().find("thermal_zone") == 0) {
                thermal_available_ = true;
                return true;
            }
        }
    } catch (...) {
        // Ignore filesystem errors
    }

    thermal_available_ = false;
    return false;
}

std::vector<temperature_sensor_info> temperature_info_collector::enumerate_sensors() {
    std::lock_guard<std::mutex> lock(mutex_);
    return enumerate_sensors_impl();
}

std::vector<temperature_sensor_info> temperature_info_collector::enumerate_sensors_impl() {
    std::vector<temperature_sensor_info> sensors;

    std::filesystem::path thermal_path(THERMAL_BASE_PATH);
    if (!std::filesystem::exists(thermal_path)) {
        return sensors;
    }

    try {
        for (const auto& entry : std::filesystem::directory_iterator(thermal_path)) {
            if (!entry.is_directory()) {
                continue;
            }

            std::string dir_name = entry.path().filename().string();
            if (dir_name.find("thermal_zone") != 0) {
                continue;
            }

            // Read zone type
            std::filesystem::path type_path = entry.path() / "type";
            std::string zone_type = read_file_contents(type_path);
            if (zone_type.empty()) {
                zone_type = dir_name;
            }

            // Check if temperature file is readable
            std::filesystem::path temp_path = entry.path() / "temp";
            if (!std::filesystem::exists(temp_path)) {
                continue;
            }

            temperature_sensor_info info;
            info.id = dir_name;
            info.name = zone_type;
            info.zone_path = entry.path().string();
            info.type = classify_sensor(zone_type);

            sensors.push_back(info);
        }
    } catch (const std::filesystem::filesystem_error&) {
        // Ignore filesystem errors, return what we have
    }

    cached_sensors_ = sensors;
    return sensors;
}

temperature_reading temperature_info_collector::read_temperature(
    const temperature_sensor_info& sensor) {
    std::lock_guard<std::mutex> lock(mutex_);
    return read_temperature_impl(sensor);
}

temperature_reading temperature_info_collector::read_temperature_impl(
    const temperature_sensor_info& sensor) {
    temperature_reading reading;
    reading.sensor = sensor;
    reading.timestamp = std::chrono::system_clock::now();

    std::filesystem::path zone_path(sensor.zone_path);

    // Read current temperature
    std::filesystem::path temp_path = zone_path / "temp";
    std::string temp_str = read_file_contents(temp_path);
    if (!temp_str.empty()) {
        reading.temperature_celsius = parse_temperature(temp_str);
    }

    // Try to read trip points for thresholds
    // Typically: trip_point_0 = critical, trip_point_1 = hot (warning)
    for (int i = 0; i < 5; ++i) {
        std::string trip_type = read_trip_type(zone_path, i);
        double trip_temp = read_trip_point(zone_path, i);

        if (trip_temp > 0.0) {
            reading.thresholds_available = true;

            if (trip_type == "critical" || trip_type == "hot") {
                if (reading.critical_threshold_celsius == 0.0 ||
                    trip_temp < reading.critical_threshold_celsius) {
                    reading.critical_threshold_celsius = trip_temp;
                }
            } else if (trip_type == "passive" || trip_type == "active") {
                if (reading.warning_threshold_celsius == 0.0 ||
                    trip_temp < reading.warning_threshold_celsius) {
                    reading.warning_threshold_celsius = trip_temp;
                }
            }
        }
    }

    // Check threshold status
    if (reading.thresholds_available) {
        if (reading.critical_threshold_celsius > 0.0 &&
            reading.temperature_celsius >= reading.critical_threshold_celsius) {
            reading.is_critical = true;
        }
        if (reading.warning_threshold_celsius > 0.0 &&
            reading.temperature_celsius >= reading.warning_threshold_celsius) {
            reading.is_warning = true;
        }
    }

    return reading;
}

std::vector<temperature_reading> temperature_info_collector::read_all_temperatures() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<temperature_reading> readings;

    // Refresh sensor list if empty
    if (cached_sensors_.empty()) {
        enumerate_sensors_impl();
    }

    for (const auto& sensor : cached_sensors_) {
        readings.push_back(read_temperature_impl(sensor));
    }

    return readings;
}

}  // namespace monitoring
}  // namespace kcenon

#endif  // defined(__linux__)
