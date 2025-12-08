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

#include <kcenon/monitoring/collectors/power_collector.h>

#if defined(__linux__)

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace kcenon {
namespace monitoring {

namespace {

// Power source paths
constexpr const char* POWER_SUPPLY_PATH = "/sys/class/power_supply";
constexpr const char* RAPL_PATH = "/sys/class/powercap/intel-rapl";

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
 * Classify power source type based on type string
 */
power_source_type classify_power_source(const std::string& type_str) {
    std::string lower_type = type_str;
    std::transform(lower_type.begin(), lower_type.end(), lower_type.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (lower_type == "battery") {
        return power_source_type::battery;
    }
    if (lower_type == "mains" || lower_type == "ac") {
        return power_source_type::ac;
    }
    if (lower_type == "usb" || lower_type == "usb_pd") {
        return power_source_type::usb;
    }
    if (lower_type == "wireless") {
        return power_source_type::wireless;
    }
    return power_source_type::unknown;
}

/**
 * Classify RAPL domain type based on name
 */
power_source_type classify_rapl_domain(const std::string& name) {
    std::string lower_name = name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (lower_name.find("package") != std::string::npos ||
        lower_name.find("pkg") != std::string::npos) {
        return power_source_type::package;
    }
    if (lower_name.find("core") != std::string::npos ||
        lower_name.find("cpu") != std::string::npos) {
        return power_source_type::cpu;
    }
    if (lower_name.find("dram") != std::string::npos ||
        lower_name.find("memory") != std::string::npos) {
        return power_source_type::memory;
    }
    if (lower_name.find("gpu") != std::string::npos ||
        lower_name.find("uncore") != std::string::npos) {
        return power_source_type::gpu;
    }
    if (lower_name.find("psys") != std::string::npos ||
        lower_name.find("platform") != std::string::npos) {
        return power_source_type::platform;
    }
    return power_source_type::other;
}

}  // anonymous namespace

// power_info_collector implementation for Linux

power_info_collector::power_info_collector()
    : last_reading_time_(std::chrono::steady_clock::now()) {}

power_info_collector::~power_info_collector() = default;

bool power_info_collector::is_power_available() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (power_checked_) {
        return power_available_;
    }

    power_checked_ = true;

    // Check if power_supply path exists
    std::filesystem::path power_path(POWER_SUPPLY_PATH);
    if (std::filesystem::exists(power_path) && std::filesystem::is_directory(power_path)) {
        try {
            for (const auto& entry : std::filesystem::directory_iterator(power_path)) {
                if (entry.is_directory()) {
                    power_available_ = true;
                    return true;
                }
            }
        } catch (...) {
            // Ignore filesystem errors
        }
    }

    // Check if RAPL path exists
    std::filesystem::path rapl_path(RAPL_PATH);
    if (std::filesystem::exists(rapl_path) && std::filesystem::is_directory(rapl_path)) {
        power_available_ = true;
        return true;
    }

    power_available_ = false;
    return false;
}

std::vector<power_source_info> power_info_collector::enumerate_sources() {
    std::lock_guard<std::mutex> lock(mutex_);
    return enumerate_sources_impl();
}

std::vector<power_source_info> power_info_collector::enumerate_sources_impl() {
    std::vector<power_source_info> sources;

    // Enumerate power_supply sources
    std::filesystem::path power_path(POWER_SUPPLY_PATH);
    if (std::filesystem::exists(power_path)) {
        try {
            for (const auto& entry : std::filesystem::directory_iterator(power_path)) {
                if (!entry.is_directory()) {
                    continue;
                }

                std::string name = entry.path().filename().string();
                
                // Read type
                std::filesystem::path type_path = entry.path() / "type";
                std::string type_str = read_file_contents(type_path);
                if (type_str.empty()) {
                    continue;
                }

                power_source_info info;
                info.id = "power_supply_" + name;
                info.name = name;
                info.path = entry.path().string();
                info.type = classify_power_source(type_str);

                sources.push_back(info);
            }
        } catch (const std::filesystem::filesystem_error&) {
            // Ignore filesystem errors
        }
    }

    // Enumerate RAPL domains
    std::filesystem::path rapl_path(RAPL_PATH);
    if (std::filesystem::exists(rapl_path)) {
        try {
            for (const auto& entry : std::filesystem::directory_iterator(rapl_path)) {
                if (!entry.is_directory()) {
                    continue;
                }

                std::string dir_name = entry.path().filename().string();
                if (dir_name.find("intel-rapl:") != 0) {
                    continue;
                }

                // Read domain name
                std::filesystem::path name_path = entry.path() / "name";
                std::string domain_name = read_file_contents(name_path);
                if (domain_name.empty()) {
                    domain_name = dir_name;
                }

                // Check if energy_uj file exists (indicator of readable RAPL)
                std::filesystem::path energy_path = entry.path() / "energy_uj";
                if (!std::filesystem::exists(energy_path)) {
                    continue;
                }

                power_source_info info;
                info.id = "rapl_" + dir_name;
                info.name = domain_name;
                info.path = entry.path().string();
                info.type = classify_rapl_domain(domain_name);

                sources.push_back(info);

                // Also enumerate subdomains (e.g., intel-rapl:0:0, intel-rapl:0:1)
                for (const auto& sub_entry : std::filesystem::directory_iterator(entry.path())) {
                    if (!sub_entry.is_directory()) {
                        continue;
                    }

                    std::string sub_dir_name = sub_entry.path().filename().string();
                    if (sub_dir_name.find("intel-rapl:") != 0) {
                        continue;
                    }

                    std::filesystem::path sub_name_path = sub_entry.path() / "name";
                    std::string sub_domain_name = read_file_contents(sub_name_path);
                    if (sub_domain_name.empty()) {
                        sub_domain_name = sub_dir_name;
                    }

                    std::filesystem::path sub_energy_path = sub_entry.path() / "energy_uj";
                    if (!std::filesystem::exists(sub_energy_path)) {
                        continue;
                    }

                    power_source_info sub_info;
                    sub_info.id = "rapl_" + sub_dir_name;
                    sub_info.name = sub_domain_name;
                    sub_info.path = sub_entry.path().string();
                    sub_info.type = classify_rapl_domain(sub_domain_name);

                    sources.push_back(sub_info);
                }
            }
        } catch (const std::filesystem::filesystem_error&) {
            // Ignore filesystem errors
        }
    }

    cached_sources_ = sources;
    return sources;
}

power_reading power_info_collector::read_power(const power_source_info& source) {
    std::lock_guard<std::mutex> lock(mutex_);
    return read_power_impl(source);
}

power_reading power_info_collector::read_power_impl(const power_source_info& source) {
    power_reading reading;
    reading.source = source;
    reading.timestamp = std::chrono::system_clock::now();

    std::filesystem::path source_path(source.path);

    // Handle RAPL sources
    if (source.id.find("rapl_") == 0) {
        // Read energy in microjoules
        std::filesystem::path energy_path = source_path / "energy_uj";
        std::string energy_str = read_file_contents(energy_path);
        if (!energy_str.empty()) {
            int64_t energy_uj = parse_int64(energy_str);
            reading.energy_joules = static_cast<double>(energy_uj) / 1000000.0;  // Convert to Joules
            reading.power_available = true;

            // Calculate power from energy delta
            auto now = std::chrono::steady_clock::now();
            auto it = last_energy_readings_.find(source.id);
            if (it != last_energy_readings_.end()) {
                double delta_energy = reading.energy_joules - it->second;
                auto delta_time = std::chrono::duration<double>(now - last_reading_time_).count();
                if (delta_time > 0.0 && delta_energy >= 0.0) {
                    reading.power_watts = delta_energy / delta_time;
                }
            }
            last_energy_readings_[source.id] = reading.energy_joules;
        }

        // Read power constraint (TDP)
        std::filesystem::path constraint_path = source_path / "constraint_0_power_limit_uw";
        std::string constraint_str = read_file_contents(constraint_path);
        if (!constraint_str.empty()) {
            int64_t power_limit_uw = parse_int64(constraint_str);
            reading.power_limit_watts = static_cast<double>(power_limit_uw) / 1000000.0;  // Convert to Watts
            reading.limits_available = true;
        }

        return reading;
    }

    // Handle power_supply sources (battery, AC)
    if (source.type == power_source_type::battery) {
        reading.battery_available = true;

        // Read capacity percentage
        std::filesystem::path capacity_path = source_path / "capacity";
        std::string capacity_str = read_file_contents(capacity_path);
        if (!capacity_str.empty()) {
            reading.battery_percent = parse_double(capacity_str);
        }

        // Read status (Charging, Discharging, Full, Not charging)
        std::filesystem::path status_path = source_path / "status";
        std::string status_str = read_file_contents(status_path);
        if (!status_str.empty()) {
            std::string lower_status = status_str;
            std::transform(lower_status.begin(), lower_status.end(), lower_status.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            reading.is_charging = (lower_status.find("charging") != std::string::npos && 
                                   lower_status.find("discharging") == std::string::npos);
            reading.is_discharging = (lower_status.find("discharging") != std::string::npos);
            reading.is_full = (lower_status.find("full") != std::string::npos);
        }

        // Read voltage (in microvolts)
        std::filesystem::path voltage_path = source_path / "voltage_now";
        std::string voltage_str = read_file_contents(voltage_path);
        if (!voltage_str.empty()) {
            int64_t voltage_uv = parse_int64(voltage_str);
            reading.voltage_volts = static_cast<double>(voltage_uv) / 1000000.0;
        }

        // Read current (in microamps) for power calculation
        std::filesystem::path current_path = source_path / "current_now";
        std::string current_str = read_file_contents(current_path);
        if (!current_str.empty() && reading.voltage_volts > 0.0) {
            int64_t current_ua = parse_int64(current_str);
            double current_a = static_cast<double>(current_ua) / 1000000.0;
            reading.power_watts = reading.voltage_volts * current_a;
            reading.battery_charge_rate = reading.is_discharging ? -reading.power_watts : reading.power_watts;
            reading.power_available = true;
        }

        // Read power directly if available
        std::filesystem::path power_path = source_path / "power_now";
        std::string power_str = read_file_contents(power_path);
        if (!power_str.empty()) {
            int64_t power_uw = parse_int64(power_str);
            reading.power_watts = static_cast<double>(power_uw) / 1000000.0;
            reading.battery_charge_rate = reading.is_discharging ? -reading.power_watts : reading.power_watts;
            reading.power_available = true;
        }

        // Read energy full (capacity in microWh)
        std::filesystem::path energy_full_path = source_path / "energy_full";
        std::string energy_full_str = read_file_contents(energy_full_path);
        if (!energy_full_str.empty()) {
            int64_t energy_uwh = parse_int64(energy_full_str);
            reading.battery_capacity_wh = static_cast<double>(energy_uwh) / 1000000.0;
        }
    } else if (source.type == power_source_type::ac) {
        // Read online status
        std::filesystem::path online_path = source_path / "online";
        std::string online_str = read_file_contents(online_path);
        if (!online_str.empty()) {
            int online = static_cast<int>(parse_double(online_str));
            reading.power_available = (online == 1);
        }
    }

    return reading;
}

std::vector<power_reading> power_info_collector::read_all_power() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<power_reading> readings;

    // Refresh source list if empty
    if (cached_sources_.empty()) {
        enumerate_sources_impl();
    }

    auto now = std::chrono::steady_clock::now();
    for (const auto& source : cached_sources_) {
        readings.push_back(read_power_impl(source));
    }
    last_reading_time_ = now;

    return readings;
}

}  // namespace monitoring
}  // namespace kcenon

#endif  // defined(__linux__)
