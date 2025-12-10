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

#include <kcenon/monitoring/collectors/battery_collector.h>

#if defined(__linux__)

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace kcenon {
namespace monitoring {

namespace {

// Battery sysfs path
constexpr const char* POWER_SUPPLY_PATH = "/sys/class/power_supply";

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
 * Parse numeric value from string
 */
double parse_double(const std::string& value_str) {
    try {
        return std::stod(value_str);
    } catch (...) {
        return 0.0;
    }
}

/**
 * Parse int64 value from string
 */
int64_t parse_int64(const std::string& value_str) {
    try {
        return std::stoll(value_str);
    } catch (...) {
        return 0;
    }
}

/**
 * Parse battery status from string
 */
battery_status parse_battery_status(const std::string& status_str) {
    std::string lower_status = status_str;
    std::transform(lower_status.begin(), lower_status.end(), lower_status.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (lower_status.find("charging") != std::string::npos &&
        lower_status.find("discharging") == std::string::npos &&
        lower_status.find("not") == std::string::npos) {
        return battery_status::charging;
    }
    if (lower_status.find("discharging") != std::string::npos) {
        return battery_status::discharging;
    }
    if (lower_status.find("not charging") != std::string::npos) {
        return battery_status::not_charging;
    }
    if (lower_status.find("full") != std::string::npos) {
        return battery_status::full;
    }
    return battery_status::unknown;
}

/**
 * Check if a power supply is a battery
 */
bool is_battery_type(const std::filesystem::path& supply_path) {
    std::filesystem::path type_path = supply_path / "type";
    std::string type_str = read_file_contents(type_path);
    if (type_str.empty()) {
        return false;
    }
    std::string lower_type = type_str;
    std::transform(lower_type.begin(), lower_type.end(), lower_type.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return lower_type == "battery";
}

/**
 * Check if AC power is connected
 */
bool is_ac_connected() {
    std::filesystem::path power_path(POWER_SUPPLY_PATH);
    if (!std::filesystem::exists(power_path)) {
        return false;
    }

    try {
        for (const auto& entry : std::filesystem::directory_iterator(power_path)) {
            if (!entry.is_directory()) {
                continue;
            }

            std::filesystem::path type_path = entry.path() / "type";
            std::string type_str = read_file_contents(type_path);
            std::string lower_type = type_str;
            std::transform(lower_type.begin(), lower_type.end(), lower_type.begin(),
                           [](unsigned char c) { return std::tolower(c); });

            if (lower_type == "mains" || lower_type == "ac") {
                std::filesystem::path online_path = entry.path() / "online";
                std::string online_str = read_file_contents(online_path);
                if (!online_str.empty()) {
                    return parse_int64(online_str) == 1;
                }
            }
        }
    } catch (const std::filesystem::filesystem_error&) {
        // Ignore filesystem errors
    }

    return false;
}

}  // anonymous namespace

// battery_info_collector implementation for Linux

battery_info_collector::battery_info_collector() = default;
battery_info_collector::~battery_info_collector() = default;

bool battery_info_collector::is_battery_available() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (battery_checked_) {
        return battery_available_;
    }

    battery_checked_ = true;

    // Check if power_supply path exists and has battery
    std::filesystem::path power_path(POWER_SUPPLY_PATH);
    if (!std::filesystem::exists(power_path) || !std::filesystem::is_directory(power_path)) {
        battery_available_ = false;
        return false;
    }

    try {
        for (const auto& entry : std::filesystem::directory_iterator(power_path)) {
            if (entry.is_directory() && is_battery_type(entry.path())) {
                battery_available_ = true;
                return true;
            }
        }
    } catch (const std::filesystem::filesystem_error&) {
        // Ignore filesystem errors
    }

    battery_available_ = false;
    return false;
}

std::vector<battery_info> battery_info_collector::enumerate_batteries() {
    std::lock_guard<std::mutex> lock(mutex_);
    return enumerate_batteries_impl();
}

std::vector<battery_info> battery_info_collector::enumerate_batteries_impl() {
    std::vector<battery_info> batteries;

    std::filesystem::path power_path(POWER_SUPPLY_PATH);
    if (!std::filesystem::exists(power_path)) {
        cached_batteries_ = batteries;
        return batteries;
    }

    try {
        for (const auto& entry : std::filesystem::directory_iterator(power_path)) {
            if (!entry.is_directory()) {
                continue;
            }

            if (!is_battery_type(entry.path())) {
                continue;
            }

            std::string name = entry.path().filename().string();

            battery_info info;
            info.id = name;
            info.name = name;
            info.path = entry.path().string();

            // Read manufacturer
            std::filesystem::path mfr_path = entry.path() / "manufacturer";
            info.manufacturer = read_file_contents(mfr_path);

            // Read model name
            std::filesystem::path model_path = entry.path() / "model_name";
            info.model = read_file_contents(model_path);

            // Read serial number
            std::filesystem::path serial_path = entry.path() / "serial_number";
            info.serial = read_file_contents(serial_path);

            // Read technology
            std::filesystem::path tech_path = entry.path() / "technology";
            info.technology = read_file_contents(tech_path);

            batteries.push_back(info);
        }
    } catch (const std::filesystem::filesystem_error&) {
        // Ignore filesystem errors
    }

    cached_batteries_ = batteries;
    return batteries;
}

battery_reading battery_info_collector::read_battery(const battery_info& battery) {
    std::lock_guard<std::mutex> lock(mutex_);
    return read_battery_impl(battery);
}

battery_reading battery_info_collector::read_battery_impl(const battery_info& battery) {
    battery_reading reading;
    reading.info = battery;
    reading.timestamp = std::chrono::system_clock::now();

    std::filesystem::path battery_path(battery.path);
    if (!std::filesystem::exists(battery_path)) {
        return reading;
    }

    reading.battery_present = true;
    reading.metrics_available = true;

    // Read capacity percentage
    std::filesystem::path capacity_path = battery_path / "capacity";
    std::string capacity_str = read_file_contents(capacity_path);
    if (!capacity_str.empty()) {
        reading.level_percent = parse_double(capacity_str);
    }

    // Read status
    std::filesystem::path status_path = battery_path / "status";
    std::string status_str = read_file_contents(status_path);
    if (!status_str.empty()) {
        reading.status = parse_battery_status(status_str);
        reading.is_charging = (reading.status == battery_status::charging);
    }

    // Read AC connection status
    reading.ac_connected = is_ac_connected();

    // Read voltage (in microvolts)
    std::filesystem::path voltage_path = battery_path / "voltage_now";
    std::string voltage_str = read_file_contents(voltage_path);
    if (!voltage_str.empty()) {
        int64_t voltage_uv = parse_int64(voltage_str);
        reading.voltage_volts = static_cast<double>(voltage_uv) / 1000000.0;
    }

    // Read current (in microamps)
    std::filesystem::path current_path = battery_path / "current_now";
    std::string current_str = read_file_contents(current_path);
    if (!current_str.empty()) {
        int64_t current_ua = parse_int64(current_str);
        reading.current_amps = static_cast<double>(current_ua) / 1000000.0;

        // Calculate power
        if (reading.voltage_volts > 0.0) {
            reading.power_watts = reading.voltage_volts * reading.current_amps;
        }
    }

    // Read power directly if available
    std::filesystem::path power_path = battery_path / "power_now";
    std::string power_str = read_file_contents(power_path);
    if (!power_str.empty()) {
        int64_t power_uw = parse_int64(power_str);
        reading.power_watts = static_cast<double>(power_uw) / 1000000.0;
    }

    // Read energy now (in microWh)
    std::filesystem::path energy_now_path = battery_path / "energy_now";
    std::string energy_now_str = read_file_contents(energy_now_path);
    if (!energy_now_str.empty()) {
        int64_t energy_uwh = parse_int64(energy_now_str);
        reading.current_capacity_wh = static_cast<double>(energy_uwh) / 1000000.0;
    }

    // Read energy full (in microWh)
    std::filesystem::path energy_full_path = battery_path / "energy_full";
    std::string energy_full_str = read_file_contents(energy_full_path);
    if (!energy_full_str.empty()) {
        int64_t energy_uwh = parse_int64(energy_full_str);
        reading.full_charge_capacity_wh = static_cast<double>(energy_uwh) / 1000000.0;
    }

    // Read energy full design (in microWh)
    std::filesystem::path energy_design_path = battery_path / "energy_full_design";
    std::string energy_design_str = read_file_contents(energy_design_path);
    if (!energy_design_str.empty()) {
        int64_t energy_uwh = parse_int64(energy_design_str);
        reading.design_capacity_wh = static_cast<double>(energy_uwh) / 1000000.0;
    }

    // Calculate health percentage
    if (reading.design_capacity_wh > 0.0) {
        reading.health_percent = (reading.full_charge_capacity_wh / reading.design_capacity_wh) * 100.0;
    }

    // Read charge now/full (alternative to energy_now/full, in microAh)
    if (reading.current_capacity_wh == 0.0) {
        std::filesystem::path charge_now_path = battery_path / "charge_now";
        std::string charge_now_str = read_file_contents(charge_now_path);
        if (!charge_now_str.empty() && reading.voltage_volts > 0.0) {
            int64_t charge_uah = parse_int64(charge_now_str);
            double charge_ah = static_cast<double>(charge_uah) / 1000000.0;
            reading.current_capacity_wh = charge_ah * reading.voltage_volts;
        }
    }

    if (reading.full_charge_capacity_wh == 0.0) {
        std::filesystem::path charge_full_path = battery_path / "charge_full";
        std::string charge_full_str = read_file_contents(charge_full_path);
        if (!charge_full_str.empty() && reading.voltage_volts > 0.0) {
            int64_t charge_uah = parse_int64(charge_full_str);
            double charge_ah = static_cast<double>(charge_uah) / 1000000.0;
            reading.full_charge_capacity_wh = charge_ah * reading.voltage_volts;
        }
    }

    if (reading.design_capacity_wh == 0.0) {
        std::filesystem::path charge_design_path = battery_path / "charge_full_design";
        std::string charge_design_str = read_file_contents(charge_design_path);
        if (!charge_design_str.empty() && reading.voltage_volts > 0.0) {
            int64_t charge_uah = parse_int64(charge_design_str);
            double charge_ah = static_cast<double>(charge_uah) / 1000000.0;
            reading.design_capacity_wh = charge_ah * reading.voltage_volts;
        }
    }

    // Recalculate health if using charge values
    if (reading.health_percent == 0.0 && reading.design_capacity_wh > 0.0) {
        reading.health_percent = (reading.full_charge_capacity_wh / reading.design_capacity_wh) * 100.0;
    }

    // Read cycle count
    std::filesystem::path cycle_path = battery_path / "cycle_count";
    std::string cycle_str = read_file_contents(cycle_path);
    if (!cycle_str.empty()) {
        reading.cycle_count = parse_int64(cycle_str);
    }

    // Read temperature (in 0.1 degrees Celsius)
    std::filesystem::path temp_path = battery_path / "temp";
    std::string temp_str = read_file_contents(temp_path);
    if (!temp_str.empty()) {
        int64_t temp_deci = parse_int64(temp_str);
        reading.temperature_celsius = static_cast<double>(temp_deci) / 10.0;
        reading.temperature_available = true;
    }

    // Read time to empty (in seconds)
    std::filesystem::path time_empty_path = battery_path / "time_to_empty_now";
    std::string time_empty_str = read_file_contents(time_empty_path);
    if (!time_empty_str.empty()) {
        reading.time_to_empty_seconds = parse_int64(time_empty_str);
    } else if (reading.status == battery_status::discharging &&
               reading.power_watts > 0.0 && reading.current_capacity_wh > 0.0) {
        // Estimate time to empty
        reading.time_to_empty_seconds = static_cast<int64_t>(
            (reading.current_capacity_wh / reading.power_watts) * 3600.0);
    }

    // Read time to full (in seconds)
    std::filesystem::path time_full_path = battery_path / "time_to_full_now";
    std::string time_full_str = read_file_contents(time_full_path);
    if (!time_full_str.empty()) {
        reading.time_to_full_seconds = parse_int64(time_full_str);
    } else if (reading.status == battery_status::charging &&
               reading.power_watts > 0.0 &&
               reading.full_charge_capacity_wh > reading.current_capacity_wh) {
        // Estimate time to full
        double remaining_wh = reading.full_charge_capacity_wh - reading.current_capacity_wh;
        reading.time_to_full_seconds = static_cast<int64_t>(
            (remaining_wh / reading.power_watts) * 3600.0);
    }

    return reading;
}

std::vector<battery_reading> battery_info_collector::read_all_batteries() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<battery_reading> readings;

    // Refresh battery list if empty
    if (cached_batteries_.empty()) {
        enumerate_batteries_impl();
    }

    for (const auto& battery : cached_batteries_) {
        readings.push_back(read_battery_impl(battery));
    }

    return readings;
}

}  // namespace monitoring
}  // namespace kcenon

#endif  // defined(__linux__)
