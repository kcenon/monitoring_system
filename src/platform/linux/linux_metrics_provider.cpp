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

#include "linux_metrics_provider.h"

#if defined(__linux__)

#include <sys/statvfs.h>
#include <sys/stat.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace kcenon {
namespace monitoring {
namespace platform {

namespace {

// =========================================================================
// Common Helper Functions
// =========================================================================

constexpr const char* POWER_SUPPLY_PATH = "/sys/class/power_supply";
constexpr const char* THERMAL_BASE_PATH = "/sys/class/thermal";
constexpr const char* DRM_PATH = "/sys/class/drm";

std::string read_file_contents(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return "";
    }
    std::string content;
    std::getline(file, content);
    return content;
}

double parse_double(const std::string& value_str) {
    try {
        return std::stod(value_str);
    } catch (...) {
        return 0.0;
    }
}

int64_t parse_int64(const std::string& value_str) {
    try {
        return std::stoll(value_str);
    } catch (...) {
        return 0;
    }
}

// =========================================================================
// Battery Helper Functions
// =========================================================================

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

bool check_ac_connected() {
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

// =========================================================================
// Temperature Helper Functions
// =========================================================================

double parse_temperature(const std::string& value_str) {
    try {
        int64_t millidegrees = std::stoll(value_str);
        return static_cast<double>(millidegrees) / 1000.0;
    } catch (...) {
        return 0.0;
    }
}

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

// =========================================================================
// Inode Helper Functions
// =========================================================================

const std::unordered_set<std::string> PSEUDO_FILESYSTEMS = {
    "proc",     "sysfs",    "devtmpfs", "devpts",  "tmpfs",
    "securityfs", "cgroup",  "cgroup2",  "pstore",  "debugfs",
    "hugetlbfs", "mqueue",   "fusectl",  "configfs", "binfmt_misc",
    "autofs",   "rpc_pipefs", "nfsd",    "tracefs", "overlay"
};

bool should_skip_filesystem(const std::string& fs_type) {
    return PSEUDO_FILESYSTEMS.find(fs_type) != PSEUDO_FILESYSTEMS.end();
}

struct mount_entry {
    std::string device;
    std::string mount_point;
    std::string fs_type;
};

std::vector<mount_entry> get_mount_entries() {
    std::vector<mount_entry> entries;
    std::ifstream mounts_file("/proc/mounts");

    if (!mounts_file.is_open()) {
        return entries;
    }

    std::string line;
    while (std::getline(mounts_file, line)) {
        std::istringstream iss(line);
        mount_entry entry;
        std::string options, dump, pass;

        if (iss >> entry.device >> entry.mount_point >> entry.fs_type >> options >> dump >> pass) {
            entries.push_back(std::move(entry));
        }
    }

    return entries;
}

}  // anonymous namespace

// =========================================================================
// linux_metrics_provider Implementation
// =========================================================================

linux_metrics_provider::linux_metrics_provider() = default;

// =========================================================================
// Battery
// =========================================================================

bool linux_metrics_provider::is_battery_available() const {
    if (!battery_checked_) {
        std::filesystem::path power_path(POWER_SUPPLY_PATH);
        if (!std::filesystem::exists(power_path) || !std::filesystem::is_directory(power_path)) {
            battery_available_ = false;
            battery_checked_ = true;
            return false;
        }

        try {
            for (const auto& entry : std::filesystem::directory_iterator(power_path)) {
                if (entry.is_directory() && is_battery_type(entry.path())) {
                    battery_available_ = true;
                    battery_checked_ = true;
                    return true;
                }
            }
        } catch (const std::filesystem::filesystem_error&) {
            // Ignore filesystem errors
        }

        battery_available_ = false;
        battery_checked_ = true;
    }
    return battery_available_;
}

std::vector<battery_reading> linux_metrics_provider::get_battery_readings() {
    std::vector<battery_reading> readings;

    std::filesystem::path power_path(POWER_SUPPLY_PATH);
    if (!std::filesystem::exists(power_path)) {
        return readings;
    }

    try {
        for (const auto& entry : std::filesystem::directory_iterator(power_path)) {
            if (!entry.is_directory() || !is_battery_type(entry.path())) {
                continue;
            }

            battery_reading reading;
            reading.timestamp = std::chrono::system_clock::now();
            reading.battery_present = true;
            reading.metrics_available = true;

            std::string name = entry.path().filename().string();
            reading.info.id = name;
            reading.info.name = name;
            reading.info.path = entry.path().string();
            reading.info.manufacturer = read_file_contents(entry.path() / "manufacturer");
            reading.info.model = read_file_contents(entry.path() / "model_name");
            reading.info.serial = read_file_contents(entry.path() / "serial_number");
            reading.info.technology = read_file_contents(entry.path() / "technology");

            // Capacity percentage
            std::string capacity_str = read_file_contents(entry.path() / "capacity");
            if (!capacity_str.empty()) {
                reading.level_percent = parse_double(capacity_str);
            }

            // Status
            std::string status_str = read_file_contents(entry.path() / "status");
            if (!status_str.empty()) {
                reading.status = parse_battery_status(status_str);
                reading.is_charging = (reading.status == battery_status::charging);
            }

            // AC connection
            reading.ac_connected = check_ac_connected();

            // Voltage (microvolts)
            std::string voltage_str = read_file_contents(entry.path() / "voltage_now");
            if (!voltage_str.empty()) {
                int64_t voltage_uv = parse_int64(voltage_str);
                reading.voltage_volts = static_cast<double>(voltage_uv) / 1000000.0;
            }

            // Current (microamps)
            std::string current_str = read_file_contents(entry.path() / "current_now");
            if (!current_str.empty()) {
                int64_t current_ua = parse_int64(current_str);
                reading.current_amps = static_cast<double>(current_ua) / 1000000.0;
                if (reading.voltage_volts > 0.0) {
                    reading.power_watts = reading.voltage_volts * reading.current_amps;
                }
            }

            // Power (microwatts)
            std::string power_str = read_file_contents(entry.path() / "power_now");
            if (!power_str.empty()) {
                int64_t power_uw = parse_int64(power_str);
                reading.power_watts = static_cast<double>(power_uw) / 1000000.0;
            }

            // Energy now (microWh)
            std::string energy_now_str = read_file_contents(entry.path() / "energy_now");
            if (!energy_now_str.empty()) {
                int64_t energy_uwh = parse_int64(energy_now_str);
                reading.current_capacity_wh = static_cast<double>(energy_uwh) / 1000000.0;
            }

            // Energy full (microWh)
            std::string energy_full_str = read_file_contents(entry.path() / "energy_full");
            if (!energy_full_str.empty()) {
                int64_t energy_uwh = parse_int64(energy_full_str);
                reading.full_charge_capacity_wh = static_cast<double>(energy_uwh) / 1000000.0;
            }

            // Energy full design (microWh)
            std::string energy_design_str = read_file_contents(entry.path() / "energy_full_design");
            if (!energy_design_str.empty()) {
                int64_t energy_uwh = parse_int64(energy_design_str);
                reading.design_capacity_wh = static_cast<double>(energy_uwh) / 1000000.0;
            }

            // Health percentage
            if (reading.design_capacity_wh > 0.0) {
                reading.health_percent = (reading.full_charge_capacity_wh / reading.design_capacity_wh) * 100.0;
            }

            // Cycle count
            std::string cycle_str = read_file_contents(entry.path() / "cycle_count");
            if (!cycle_str.empty()) {
                reading.cycle_count = parse_int64(cycle_str);
            }

            // Temperature (0.1 degrees Celsius)
            std::string temp_str = read_file_contents(entry.path() / "temp");
            if (!temp_str.empty()) {
                int64_t temp_deci = parse_int64(temp_str);
                reading.temperature_celsius = static_cast<double>(temp_deci) / 10.0;
                reading.temperature_available = true;
            }

            readings.push_back(reading);
        }
    } catch (const std::filesystem::filesystem_error&) {
        // Ignore filesystem errors
    }

    return readings;
}

// =========================================================================
// Temperature
// =========================================================================

bool linux_metrics_provider::is_temperature_available() const {
    if (!temperature_checked_) {
        std::filesystem::path thermal_path(THERMAL_BASE_PATH);
        if (!std::filesystem::exists(thermal_path) || !std::filesystem::is_directory(thermal_path)) {
            temperature_available_ = false;
            temperature_checked_ = true;
            return false;
        }

        try {
            for (const auto& entry : std::filesystem::directory_iterator(thermal_path)) {
                if (entry.is_directory() &&
                    entry.path().filename().string().find("thermal_zone") == 0) {
                    temperature_available_ = true;
                    temperature_checked_ = true;
                    return true;
                }
            }
        } catch (...) {
            // Ignore filesystem errors
        }

        temperature_available_ = false;
        temperature_checked_ = true;
    }
    return temperature_available_;
}

std::vector<temperature_reading> linux_metrics_provider::get_temperature_readings() {
    std::vector<temperature_reading> readings;

    std::filesystem::path thermal_path(THERMAL_BASE_PATH);
    if (!std::filesystem::exists(thermal_path)) {
        return readings;
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

            std::filesystem::path temp_path = entry.path() / "temp";
            if (!std::filesystem::exists(temp_path)) {
                continue;
            }

            std::string zone_type = read_file_contents(entry.path() / "type");
            if (zone_type.empty()) {
                zone_type = dir_name;
            }

            temperature_reading reading;
            reading.timestamp = std::chrono::system_clock::now();

            reading.sensor.id = dir_name;
            reading.sensor.name = zone_type;
            reading.sensor.zone_path = entry.path().string();
            reading.sensor.type = classify_sensor(zone_type);

            std::string temp_str = read_file_contents(temp_path);
            if (!temp_str.empty()) {
                reading.temperature_celsius = parse_temperature(temp_str);
            }

            // Read trip points for thresholds
            for (int i = 0; i < 5; ++i) {
                std::filesystem::path trip_type_path = entry.path() / ("trip_point_" + std::to_string(i) + "_type");
                std::filesystem::path trip_temp_path = entry.path() / ("trip_point_" + std::to_string(i) + "_temp");

                std::string trip_type = read_file_contents(trip_type_path);
                std::string trip_temp_str = read_file_contents(trip_temp_path);

                if (!trip_temp_str.empty()) {
                    double trip_temp = parse_temperature(trip_temp_str);
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

            readings.push_back(reading);
        }
    } catch (const std::filesystem::filesystem_error&) {
        // Ignore filesystem errors
    }

    return readings;
}

// =========================================================================
// Uptime
// =========================================================================

uptime_info linux_metrics_provider::get_uptime() {
    uptime_info info;

    std::ifstream uptime_file("/proc/uptime");
    if (!uptime_file.is_open()) {
        return info;
    }

    double uptime_seconds = 0.0;
    double idle_seconds = 0.0;

    if (uptime_file >> uptime_seconds >> idle_seconds) {
        info.uptime_seconds = static_cast<int64_t>(uptime_seconds);
        info.idle_seconds = static_cast<int64_t>(idle_seconds);

        auto now = std::chrono::system_clock::now();
        auto now_epoch = std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch()).count();
        info.boot_time = std::chrono::system_clock::from_time_t(
            static_cast<time_t>(now_epoch - uptime_seconds));
        info.available = true;
    }

    return info;
}

// =========================================================================
// Context Switches
// =========================================================================

context_switch_info linux_metrics_provider::get_context_switches() {
    context_switch_info info;
    info.timestamp = std::chrono::system_clock::now();

    std::ifstream stat_file("/proc/stat");
    if (!stat_file.is_open()) {
        return info;
    }

    std::string line;
    while (std::getline(stat_file, line)) {
        if (line.compare(0, 4, "ctxt") == 0) {
            std::istringstream iss(line);
            std::string label;
            uint64_t count;
            if (iss >> label >> count) {
                info.total_switches = count;
                info.available = true;
            }
            break;
        }
    }

    // Read voluntary/involuntary from /proc/self/status
    std::ifstream status_file("/proc/self/status");
    if (status_file.is_open()) {
        while (std::getline(status_file, line)) {
            if (line.compare(0, 24, "voluntary_ctxt_switches:") == 0) {
                std::istringstream iss(line.substr(24));
                iss >> info.voluntary_switches;
            } else if (line.compare(0, 27, "nonvoluntary_ctxt_switches:") == 0) {
                std::istringstream iss(line.substr(27));
                iss >> info.involuntary_switches;
            }
        }
    }

    return info;
}

// =========================================================================
// File Descriptors
// =========================================================================

fd_info linux_metrics_provider::get_fd_stats() {
    fd_info info;

    // Read system-wide FD info from /proc/sys/fs/file-nr
    std::ifstream file_nr("/proc/sys/fs/file-nr");
    if (file_nr.is_open()) {
        uint64_t allocated = 0, free = 0, maximum = 0;
        if (file_nr >> allocated >> free >> maximum) {
            info.open_fds = allocated - free;
            info.max_fds = maximum;
            if (maximum > 0) {
                info.usage_percent = 100.0 * static_cast<double>(info.open_fds) /
                                     static_cast<double>(maximum);
            }
            info.available = true;
        }
    }

    return info;
}

// =========================================================================
// Inodes
// =========================================================================

std::vector<inode_info> linux_metrics_provider::get_inode_stats() {
    std::vector<inode_info> result;

    auto mounts = get_mount_entries();

    for (const auto& mount : mounts) {
        if (should_skip_filesystem(mount.fs_type)) {
            continue;
        }

        struct statvfs stat;
        if (statvfs(mount.mount_point.c_str(), &stat) != 0) {
            continue;
        }

        if (stat.f_files == 0) {
            continue;
        }

        inode_info info;
        info.filesystem = mount.mount_point;
        info.total_inodes = stat.f_files;
        info.free_inodes = stat.f_ffree;
        info.used_inodes = info.total_inodes - info.free_inodes;
        if (info.total_inodes > 0) {
            info.usage_percent = 100.0 * static_cast<double>(info.used_inodes) /
                                 static_cast<double>(info.total_inodes);
        }
        info.available = true;

        result.push_back(info);
    }

    return result;
}

// =========================================================================
// TCP States
// =========================================================================

tcp_state_info linux_metrics_provider::get_tcp_states() {
    tcp_state_info info;
    info.available = true;

    auto parse_proc_net_tcp = [&info](const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            return;
        }

        std::string line;
        // Skip header line
        std::getline(file, line);

        while (std::getline(file, line)) {
            std::istringstream iss(line);
            std::string sl, local_addr, rem_addr, st_hex;

            if (!(iss >> sl >> local_addr >> rem_addr >> st_hex)) {
                continue;
            }

            try {
                int state = std::stoi(st_hex, nullptr, 16);
                switch (state) {
                    case 1: info.established++; break;
                    case 2: info.syn_sent++; break;
                    case 3: info.syn_recv++; break;
                    case 4: info.fin_wait1++; break;
                    case 5: info.fin_wait2++; break;
                    case 6: info.time_wait++; break;
                    case 8: info.close_wait++; break;
                    case 9: info.last_ack++; break;
                    case 10: info.listen++; break;
                    case 11: info.closing++; break;
                    default: break;
                }
                info.total++;
            } catch (...) {
                continue;
            }
        }
    };

    parse_proc_net_tcp("/proc/net/tcp");
    parse_proc_net_tcp("/proc/net/tcp6");

    return info;
}

// =========================================================================
// Socket Buffers
// =========================================================================

socket_buffer_info linux_metrics_provider::get_socket_buffer_stats() {
    socket_buffer_info info;

    auto parse_proc_net_tcp = [&info](const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            return;
        }

        std::string line;
        // Skip header line
        std::getline(file, line);

        while (std::getline(file, line)) {
            std::istringstream iss(line);
            std::string sl, local_addr, rem_addr, st_hex, queues;

            if (!(iss >> sl >> local_addr >> rem_addr >> st_hex >> queues)) {
                continue;
            }

            size_t colon_pos = queues.find(':');
            if (colon_pos == std::string::npos) {
                continue;
            }

            try {
                std::string tx_hex = queues.substr(0, colon_pos);
                std::string rx_hex = queues.substr(colon_pos + 1);

                uint64_t tx_queue = std::stoull(tx_hex, nullptr, 16);
                uint64_t rx_queue = std::stoull(rx_hex, nullptr, 16);

                info.tx_buffer_used += tx_queue;
                info.rx_buffer_used += rx_queue;
            } catch (...) {
                continue;
            }
        }
    };

    parse_proc_net_tcp("/proc/net/tcp");
    parse_proc_net_tcp("/proc/net/tcp6");
    info.available = true;

    return info;
}

// =========================================================================
// Interrupts
// =========================================================================

std::vector<interrupt_info> linux_metrics_provider::get_interrupt_stats() {
    std::vector<interrupt_info> result;

    std::ifstream stat_file("/proc/stat");
    if (!stat_file.is_open()) {
        return result;
    }

    std::string line;
    while (std::getline(stat_file, line)) {
        if (line.substr(0, 4) == "intr") {
            std::istringstream iss(line);
            std::string label;
            uint64_t total;
            if (iss >> label >> total) {
                interrupt_info info;
                info.name = "total_interrupts";
                info.count = total;
                info.available = true;
                result.push_back(info);
            }
            break;
        }
    }

    // Read soft interrupts total
    std::ifstream softirq_file("/proc/softirqs");
    if (softirq_file.is_open()) {
        uint64_t soft_total = 0;
        // Skip header
        std::getline(softirq_file, line);

        while (std::getline(softirq_file, line)) {
            std::istringstream iss(line);
            std::string irq_type;
            iss >> irq_type;

            uint64_t count;
            while (iss >> count) {
                soft_total += count;
            }
        }

        if (soft_total > 0) {
            interrupt_info info;
            info.name = "soft_interrupts";
            info.count = soft_total;
            info.available = true;
            result.push_back(info);
        }
    }

    return result;
}

// =========================================================================
// Power
// =========================================================================

bool linux_metrics_provider::is_power_available() const {
    if (!power_checked_) {
        std::filesystem::path power_path(POWER_SUPPLY_PATH);
        if (std::filesystem::exists(power_path) && std::filesystem::is_directory(power_path)) {
            try {
                for (const auto& entry : std::filesystem::directory_iterator(power_path)) {
                    if (entry.is_directory()) {
                        power_available_ = true;
                        power_checked_ = true;
                        return true;
                    }
                }
            } catch (...) {
                // Ignore filesystem errors
            }
        }

        // Check RAPL
        std::filesystem::path rapl_path("/sys/class/powercap/intel-rapl");
        if (std::filesystem::exists(rapl_path) && std::filesystem::is_directory(rapl_path)) {
            power_available_ = true;
            power_checked_ = true;
            return true;
        }

        power_available_ = false;
        power_checked_ = true;
    }
    return power_available_;
}

power_info linux_metrics_provider::get_power_info() {
    power_info info;

    std::filesystem::path power_path(POWER_SUPPLY_PATH);
    if (!std::filesystem::exists(power_path)) {
        return info;
    }

    try {
        for (const auto& entry : std::filesystem::directory_iterator(power_path)) {
            if (!entry.is_directory()) {
                continue;
            }

            std::string type_str = read_file_contents(entry.path() / "type");
            std::string lower_type = type_str;
            std::transform(lower_type.begin(), lower_type.end(), lower_type.begin(),
                           [](unsigned char c) { return std::tolower(c); });

            if (lower_type == "mains" || lower_type == "ac") {
                std::string online_str = read_file_contents(entry.path() / "online");
                if (!online_str.empty() && parse_int64(online_str) == 1) {
                    info.source = "ac";
                    info.available = true;
                }
            } else if (lower_type == "battery") {
                // Read power from battery
                std::string power_str = read_file_contents(entry.path() / "power_now");
                if (!power_str.empty()) {
                    int64_t power_uw = parse_int64(power_str);
                    info.power_watts = static_cast<double>(power_uw) / 1000000.0;
                    info.available = true;
                }

                std::string voltage_str = read_file_contents(entry.path() / "voltage_now");
                if (!voltage_str.empty()) {
                    int64_t voltage_uv = parse_int64(voltage_str);
                    info.voltage_volts = static_cast<double>(voltage_uv) / 1000000.0;
                }

                std::string current_str = read_file_contents(entry.path() / "current_now");
                if (!current_str.empty()) {
                    int64_t current_ua = parse_int64(current_str);
                    info.current_amps = static_cast<double>(current_ua) / 1000000.0;
                }

                if (info.source.empty()) {
                    info.source = "battery";
                }
            }
        }
    } catch (const std::filesystem::filesystem_error&) {
        // Ignore filesystem errors
    }

    return info;
}

// =========================================================================
// GPU
// =========================================================================

bool linux_metrics_provider::is_gpu_available() const {
    if (!gpu_checked_) {
        if (!std::filesystem::exists(DRM_PATH)) {
            gpu_available_ = false;
            gpu_checked_ = true;
            return false;
        }

        try {
            for (const auto& entry : std::filesystem::directory_iterator(DRM_PATH)) {
                std::string name = entry.path().filename().string();
                if (name.find("card") == 0 && name.find('-') == std::string::npos) {
                    std::string device_path = entry.path().string() + "/device";
                    if (std::filesystem::exists(device_path + "/vendor")) {
                        gpu_available_ = true;
                        gpu_checked_ = true;
                        return true;
                    }
                }
            }
        } catch (...) {
            // Directory enumeration failed
        }

        gpu_available_ = false;
        gpu_checked_ = true;
    }
    return gpu_available_;
}

std::vector<gpu_info> linux_metrics_provider::get_gpu_info() {
    std::vector<gpu_info> result;

    if (!std::filesystem::exists(DRM_PATH)) {
        return result;
    }

    try {
        for (const auto& entry : std::filesystem::directory_iterator(DRM_PATH)) {
            std::string name = entry.path().filename().string();
            if (name.find("card") != 0 || name.find('-') != std::string::npos) {
                continue;
            }

            std::string device_path = entry.path().string() + "/device";
            std::string vendor_path = device_path + "/vendor";

            if (!std::filesystem::exists(vendor_path)) {
                continue;
            }

            gpu_info info;
            info.name = name;
            info.available = true;

            // Read vendor
            std::string vendor_str = read_file_contents(vendor_path);
            if (!vendor_str.empty()) {
                try {
                    uint16_t vendor_id = static_cast<uint16_t>(std::stoul(vendor_str, nullptr, 16));
                    switch (vendor_id) {
                        case 0x10de: info.vendor = "NVIDIA"; break;
                        case 0x1002: info.vendor = "AMD"; break;
                        case 0x8086: info.vendor = "Intel"; break;
                        default: info.vendor = "Unknown"; break;
                    }
                } catch (...) {
                    info.vendor = "Unknown";
                }
            }

            // Try to read AMD-specific metrics
            std::string gpu_busy = read_file_contents(device_path + "/gpu_busy_percent");
            if (!gpu_busy.empty()) {
                info.usage_percent = parse_double(gpu_busy);
            }

            std::string mem_used = read_file_contents(device_path + "/mem_info_vram_used");
            if (!mem_used.empty()) {
                info.memory_used_mb = static_cast<double>(parse_int64(mem_used)) / (1024.0 * 1024.0);
            }

            std::string mem_total = read_file_contents(device_path + "/mem_info_vram_total");
            if (!mem_total.empty()) {
                info.memory_total_mb = static_cast<double>(parse_int64(mem_total)) / (1024.0 * 1024.0);
            }

            // Try hwmon for temperature and power
            std::string hwmon_base = device_path + "/hwmon";
            if (std::filesystem::exists(hwmon_base)) {
                for (const auto& hwmon_entry : std::filesystem::directory_iterator(hwmon_base)) {
                    if (hwmon_entry.is_directory()) {
                        std::string hwmon_path = hwmon_entry.path().string();

                        std::string temp_str = read_file_contents(hwmon_path + "/temp1_input");
                        if (!temp_str.empty()) {
                            info.temperature_celsius = static_cast<double>(parse_int64(temp_str)) / 1000.0;
                        }

                        std::string power_str = read_file_contents(hwmon_path + "/power1_average");
                        if (power_str.empty()) {
                            power_str = read_file_contents(hwmon_path + "/power1_input");
                        }
                        if (!power_str.empty()) {
                            info.power_watts = static_cast<double>(parse_int64(power_str)) / 1000000.0;
                        }

                        break;
                    }
                }
            }

            result.push_back(info);
        }
    } catch (...) {
        // Directory enumeration failed
    }

    return result;
}

// =========================================================================
// Security
// =========================================================================

security_info linux_metrics_provider::get_security_info() {
    security_info info;

    // Check firewall status (iptables)
    std::ifstream iptables("/proc/net/ip_tables_names");
    info.firewall_enabled = iptables.is_open();

    // Count active sessions from /proc/*/loginuid
    std::unordered_set<std::string> active_users;
    std::filesystem::path proc_path("/proc");
    try {
        for (const auto& entry : std::filesystem::directory_iterator(proc_path)) {
            if (!entry.is_directory()) {
                continue;
            }

            std::string name = entry.path().filename().string();
            // Check if directory name is numeric (PID)
            if (name.empty() || !std::all_of(name.begin(), name.end(), ::isdigit)) {
                continue;
            }

            std::string loginuid = read_file_contents(entry.path() / "loginuid");
            if (!loginuid.empty() && loginuid != "4294967295") {  // -1 as unsigned
                active_users.insert(loginuid);
            }
        }
    } catch (...) {
        // Ignore errors
    }

    info.active_sessions = active_users.size();
    info.available = true;

    return info;
}

}  // namespace platform
}  // namespace monitoring
}  // namespace kcenon

#endif  // __linux__
