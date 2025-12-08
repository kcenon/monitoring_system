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

#include <kcenon/monitoring/collectors/smart_collector.h>

#include <array>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <memory>

namespace kcenon {
namespace monitoring {

// ============================================================================
// smart_info_collector implementation
// ============================================================================

smart_info_collector::smart_info_collector() = default;
smart_info_collector::~smart_info_collector() = default;

std::string smart_info_collector::execute_command(const std::string& command) const {
    std::string result;
    std::array<char, 4096> buffer{};

#if defined(_WIN32)
    std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(command.c_str(), "r"), _pclose);
#else
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
#endif

    if (!pipe) {
        return result;
    }

    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    return result;
}

bool smart_info_collector::is_smartctl_available() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (smartctl_checked_) {
        return smartctl_available_;
    }

    // Try to run smartctl --version
    std::string output = execute_command("smartctl --version 2>/dev/null");
    smartctl_available_ = !output.empty() && output.find("smartctl") != std::string::npos;
    smartctl_checked_ = true;

    return smartctl_available_;
}

std::vector<disk_info> smart_info_collector::enumerate_disks() {
    std::vector<disk_info> disks;

    if (!is_smartctl_available()) {
        return disks;
    }

#if defined(__APPLE__)
    // macOS: Check /dev/disk* devices
    for (int i = 0; i < 10; ++i) {
        std::string device = "/dev/disk" + std::to_string(i);
        if (std::filesystem::exists(device)) {
            disk_info info;
            info.device_path = device;
            info.device_type = "ata";  // Will be auto-detected by smartctl
            info.smart_available = true;
            disks.push_back(std::move(info));
        }
    }
#elif defined(__linux__)
    // Linux: Check /dev/sd* and /dev/nvme* devices
    // First check SATA/SCSI drives
    for (char c = 'a'; c <= 'z'; ++c) {
        std::string device = "/dev/sd";
        device += c;
        if (std::filesystem::exists(device)) {
            disk_info info;
            info.device_path = device;
            info.device_type = "auto";
            info.smart_available = true;
            disks.push_back(std::move(info));
        }
    }
    // Then check NVMe drives
    for (int i = 0; i < 10; ++i) {
        std::string device = "/dev/nvme" + std::to_string(i) + "n1";
        if (std::filesystem::exists(device)) {
            disk_info info;
            info.device_path = device;
            info.device_type = "nvme";
            info.smart_available = true;
            disks.push_back(std::move(info));
        }
    }
#elif defined(_WIN32)
    // Windows: Check physical drives
    for (int i = 0; i < 10; ++i) {
        std::string device = "/dev/pd" + std::to_string(i);  // smartctl uses this notation
        disk_info info;
        info.device_path = device;
        info.device_type = "auto";
        info.smart_available = true;
        disks.push_back(std::move(info));
    }
#endif

    return disks;
}

smart_disk_metrics smart_info_collector::parse_smartctl_json(const std::string& json_output,
                                                             const disk_info& info) const {
    smart_disk_metrics metrics;
    metrics.device_path = info.device_path;
    metrics.timestamp = std::chrono::system_clock::now();

    if (json_output.empty()) {
        return metrics;
    }

    // Simple JSON parsing without external library
    // Look for specific fields in the JSON output

    auto find_string_value = [&json_output](const std::string& key) -> std::string {
        std::string search = "\"" + key + "\":";
        auto pos = json_output.find(search);
        if (pos == std::string::npos) return "";

        pos += search.length();
        // Skip whitespace
        while (pos < json_output.length() &&
               (json_output[pos] == ' ' || json_output[pos] == '\t')) {
            ++pos;
        }

        if (pos >= json_output.length()) return "";

        if (json_output[pos] == '"') {
            // String value
            ++pos;
            auto end = json_output.find('"', pos);
            if (end != std::string::npos) {
                return json_output.substr(pos, end - pos);
            }
        }
        return "";
    };

    auto find_number_value = [&json_output](const std::string& key) -> int64_t {
        std::string search = "\"" + key + "\":";
        auto pos = json_output.find(search);
        if (pos == std::string::npos) return 0;

        pos += search.length();
        // Skip whitespace
        while (pos < json_output.length() &&
               (json_output[pos] == ' ' || json_output[pos] == '\t')) {
            ++pos;
        }

        if (pos >= json_output.length()) return 0;

        std::string num_str;
        while (pos < json_output.length() &&
               (std::isdigit(json_output[pos]) || json_output[pos] == '-')) {
            num_str += json_output[pos];
            ++pos;
        }

        if (!num_str.empty()) {
            try {
                return std::stoll(num_str);
            } catch (...) {
                return 0;
            }
        }
        return 0;
    };

    auto find_bool_value = [&json_output](const std::string& key) -> bool {
        std::string search = "\"" + key + "\":";
        auto pos = json_output.find(search);
        if (pos == std::string::npos) {
            return false;
        }

        pos += search.length();
        // Skip whitespace
        while (pos < json_output.length() &&
               (json_output[pos] == ' ' || json_output[pos] == '\t')) {
            ++pos;
        }

        return (pos + 4 <= json_output.length() && json_output.substr(pos, 4) == "true");
    };

    // Parse device info
    metrics.model_name = find_string_value("model_name");
    if (metrics.model_name.empty()) {
        metrics.model_name = find_string_value("product");
    }
    metrics.serial_number = find_string_value("serial_number");
    metrics.firmware_version = find_string_value("firmware_version");

    // Parse SMART status
    metrics.smart_supported = find_bool_value("available");
    metrics.smart_enabled = find_bool_value("enabled");

    // Parse health status - look for "passed" string in smart_status
    if (json_output.find("\"passed\":true") != std::string::npos ||
        json_output.find("\"passed\": true") != std::string::npos) {
        metrics.health_ok = true;
    } else if (json_output.find("\"passed\":false") != std::string::npos ||
               json_output.find("\"passed\": false") != std::string::npos) {
        metrics.health_ok = false;
    }

    // Parse temperature - look for current temperature
    auto temp = find_number_value("current");
    if (temp > 0 && temp < 200) {  // Reasonable temperature range
        metrics.temperature_celsius = static_cast<double>(temp);
    }

    // Parse SMART attributes by looking for specific attribute IDs
    // These are common SMART attribute IDs:
    // 5 = Reallocated Sector Count
    // 9 = Power-On Hours
    // 12 = Power Cycle Count
    // 197 = Current Pending Sector Count
    // 198 = Uncorrectable Sector Count

    // Look for "power_on_time" section
    auto poh_hours = find_number_value("hours");
    if (poh_hours > 0) {
        metrics.power_on_hours = static_cast<uint64_t>(poh_hours);
    }

    auto power_cycle = find_number_value("power_cycle_count");
    if (power_cycle > 0) {
        metrics.power_cycle_count = static_cast<uint64_t>(power_cycle);
    }

    // For SMART attributes, we need to find them in the ata_smart_attributes table
    // Look for patterns like "id":5 followed by "raw":{"value":X}
    auto find_smart_attr_raw = [&json_output](int attr_id) -> uint64_t {
        std::string id_search = "\"id\":" + std::to_string(attr_id);
        auto pos = json_output.find(id_search);
        if (pos == std::string::npos) return 0;

        // Look for "raw" section after this
        auto raw_pos = json_output.find("\"raw\":", pos);
        if (raw_pos == std::string::npos || raw_pos > pos + 500) return 0;

        auto value_pos = json_output.find("\"value\":", raw_pos);
        if (value_pos == std::string::npos || value_pos > raw_pos + 100) return 0;

        value_pos += 8;  // length of "\"value\":"
        while (value_pos < json_output.length() &&
               (json_output[value_pos] == ' ' || json_output[value_pos] == '\t')) {
            ++value_pos;
        }

        std::string num_str;
        while (value_pos < json_output.length() && std::isdigit(json_output[value_pos])) {
            num_str += json_output[value_pos];
            ++value_pos;
        }

        if (!num_str.empty()) {
            try {
                return std::stoull(num_str);
            } catch (...) {
                return 0;
            }
        }
        return 0;
    };

    metrics.reallocated_sectors = find_smart_attr_raw(5);
    if (metrics.power_on_hours == 0) {
        metrics.power_on_hours = find_smart_attr_raw(9);
    }
    if (metrics.power_cycle_count == 0) {
        metrics.power_cycle_count = find_smart_attr_raw(12);
    }
    metrics.pending_sectors = find_smart_attr_raw(197);
    metrics.uncorrectable_errors = find_smart_attr_raw(198);
    metrics.read_error_rate = find_smart_attr_raw(1);
    metrics.write_error_rate = find_smart_attr_raw(7);

    return metrics;
}

smart_disk_metrics smart_info_collector::collect_smart_metrics(const disk_info& info) {
    smart_disk_metrics metrics;
    metrics.device_path = info.device_path;
    metrics.timestamp = std::chrono::system_clock::now();

    if (!is_smartctl_available()) {
        return metrics;
    }

    // Run smartctl with JSON output
    std::string command = "smartctl -a -j " + info.device_path + " 2>/dev/null";
    std::string output = execute_command(command);

    if (!output.empty()) {
        metrics = parse_smartctl_json(output, info);
    }

    return metrics;
}

// ============================================================================
// smart_collector implementation
// ============================================================================

smart_collector::smart_collector() : collector_(std::make_unique<smart_info_collector>()) {}

bool smart_collector::initialize(const std::unordered_map<std::string, std::string>& config) {
    // Parse configuration options
    auto it = config.find("enabled");
    if (it != config.end()) {
        enabled_ = (it->second == "true" || it->second == "1");
    }

    it = config.find("collect_temperature");
    if (it != config.end()) {
        collect_temperature_ = (it->second == "true" || it->second == "1");
    }

    it = config.find("collect_error_rates");
    if (it != config.end()) {
        collect_error_rates_ = (it->second == "true" || it->second == "1");
    }

    return true;
}

std::vector<metric> smart_collector::collect() {
    std::vector<metric> metrics;

    if (!enabled_ || !collector_) {
        return metrics;
    }

    try {
        // Enumerate disks
        auto disks = collector_->enumerate_disks();
        disks_found_.store(disks.size());

        std::vector<smart_disk_metrics> collected_metrics;

        for (const auto& disk : disks) {
            auto disk_metrics = collector_->collect_smart_metrics(disk);
            // Only add metrics if SMART data was actually retrieved
            if (disk_metrics.smart_supported || !disk_metrics.model_name.empty()) {
                add_disk_metrics(metrics, disk_metrics);
                collected_metrics.push_back(std::move(disk_metrics));
            }
        }

        // Store last collected metrics
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            last_metrics_ = std::move(collected_metrics);
        }

        collection_count_++;

    } catch (const std::exception&) {
        collection_errors_++;
    }

    return metrics;
}

std::vector<std::string> smart_collector::get_metric_types() const {
    return {"smart_health_ok",           "smart_temperature_celsius", "smart_reallocated_sectors",
            "smart_power_on_hours",      "smart_power_cycle_count",   "smart_pending_sectors",
            "smart_uncorrectable_errors"};
}

bool smart_collector::is_healthy() const { return enabled_ && collector_ != nullptr; }

std::unordered_map<std::string, double> smart_collector::get_statistics() const {
    return {{"collection_count", static_cast<double>(collection_count_.load())},
            {"collection_errors", static_cast<double>(collection_errors_.load())},
            {"disks_found", static_cast<double>(disks_found_.load())}};
}

std::vector<smart_disk_metrics> smart_collector::get_last_metrics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return last_metrics_;
}

bool smart_collector::is_smart_available() const {
    return collector_ && collector_->is_smartctl_available();
}

metric smart_collector::create_metric(const std::string& name, double value,
                                      const smart_disk_metrics& disk,
                                      const std::string& /* unit */) const {
    metric m;
    m.name = name;
    m.value = value;
    m.timestamp = disk.timestamp;
    m.tags["device"] = disk.device_path;

    if (!disk.model_name.empty()) {
        m.tags["model"] = disk.model_name;
    }
    if (!disk.serial_number.empty()) {
        m.tags["serial"] = disk.serial_number;
    }

    return m;
}

void smart_collector::add_disk_metrics(std::vector<metric>& metrics,
                                       const smart_disk_metrics& disk) {
    // Health status (1 = healthy, 0 = failing)
    metrics.push_back(
        create_metric("smart_health_ok", disk.health_ok ? 1.0 : 0.0, disk, "boolean"));

    // Temperature
    if (collect_temperature_ && disk.temperature_celsius > 0) {
        metrics.push_back(
            create_metric("smart_temperature_celsius", disk.temperature_celsius, disk, "celsius"));
    }

    // SMART attributes
    metrics.push_back(create_metric("smart_reallocated_sectors",
                                    static_cast<double>(disk.reallocated_sectors), disk, "count"));

    metrics.push_back(create_metric("smart_power_on_hours",
                                    static_cast<double>(disk.power_on_hours), disk, "hours"));

    metrics.push_back(create_metric("smart_power_cycle_count",
                                    static_cast<double>(disk.power_cycle_count), disk, "count"));

    metrics.push_back(create_metric("smart_pending_sectors",
                                    static_cast<double>(disk.pending_sectors), disk, "count"));

    metrics.push_back(create_metric("smart_uncorrectable_errors",
                                    static_cast<double>(disk.uncorrectable_errors), disk, "count"));

    // Error rates (optional)
    if (collect_error_rates_) {
        if (disk.read_error_rate > 0) {
            metrics.push_back(create_metric(
                "smart_read_error_rate", static_cast<double>(disk.read_error_rate), disk, "count"));
        }
        if (disk.write_error_rate > 0) {
            metrics.push_back(create_metric("smart_write_error_rate",
                                            static_cast<double>(disk.write_error_rate), disk,
                                            "count"));
        }
    }
}

}  // namespace monitoring
}  // namespace kcenon
