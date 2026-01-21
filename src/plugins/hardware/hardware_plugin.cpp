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

#include "kcenon/monitoring/plugins/hardware/hardware_plugin.h"

#include "kcenon/monitoring/collectors/battery_collector.h"
#include "kcenon/monitoring/collectors/gpu_collector.h"
#include "kcenon/monitoring/collectors/power_collector.h"
#include "kcenon/monitoring/collectors/temperature_collector.h"

namespace kcenon {
namespace monitoring {
namespace plugins {

std::unique_ptr<hardware_plugin> hardware_plugin::create(const hardware_plugin_config& config) {
    return std::unique_ptr<hardware_plugin>(new hardware_plugin(config));
}

hardware_plugin::hardware_plugin(const hardware_plugin_config& config) : config_(config) {
    initialize_collectors();
}

hardware_plugin::~hardware_plugin() = default;

void hardware_plugin::initialize_collectors() {
    if (config_.enable_battery) {
        battery_collector_ = std::make_unique<battery_collector>();
    }

    if (config_.enable_power) {
        power_collector_ = std::make_unique<power_collector>();
    }

    if (config_.enable_temperature) {
        temperature_collector_ = std::make_unique<temperature_collector>();
    }

    if (config_.enable_gpu) {
        gpu_collector_ = std::make_unique<gpu_collector>();
    }
}

bool hardware_plugin::initialize(const std::unordered_map<std::string, std::string>& config) {
    // Parse configuration from string map
    if (auto it = config.find("enable_battery"); it != config.end()) {
        config_.enable_battery = (it->second == "true" || it->second == "1");
    }
    if (auto it = config.find("enable_power"); it != config.end()) {
        config_.enable_power = (it->second == "true" || it->second == "1");
    }
    if (auto it = config.find("enable_temperature"); it != config.end()) {
        config_.enable_temperature = (it->second == "true" || it->second == "1");
    }
    if (auto it = config.find("enable_gpu"); it != config.end()) {
        config_.enable_gpu = (it->second == "true" || it->second == "1");
    }

    // Battery-specific options
    if (auto it = config.find("battery_collect_health"); it != config.end()) {
        config_.battery_collect_health = (it->second == "true" || it->second == "1");
    }
    if (auto it = config.find("battery_collect_thermal"); it != config.end()) {
        config_.battery_collect_thermal = (it->second == "true" || it->second == "1");
    }

    // Power-specific options
    if (auto it = config.find("power_collect_battery"); it != config.end()) {
        config_.power_collect_battery = (it->second == "true" || it->second == "1");
    }
    if (auto it = config.find("power_collect_rapl"); it != config.end()) {
        config_.power_collect_rapl = (it->second == "true" || it->second == "1");
    }

    // Temperature-specific options
    if (auto it = config.find("temperature_collect_thresholds"); it != config.end()) {
        config_.temperature_collect_thresholds = (it->second == "true" || it->second == "1");
    }
    if (auto it = config.find("temperature_collect_warnings"); it != config.end()) {
        config_.temperature_collect_warnings = (it->second == "true" || it->second == "1");
    }

    // GPU-specific options
    if (auto it = config.find("gpu_collect_utilization"); it != config.end()) {
        config_.gpu_collect_utilization = (it->second == "true" || it->second == "1");
    }
    if (auto it = config.find("gpu_collect_memory"); it != config.end()) {
        config_.gpu_collect_memory = (it->second == "true" || it->second == "1");
    }
    if (auto it = config.find("gpu_collect_temperature"); it != config.end()) {
        config_.gpu_collect_temperature = (it->second == "true" || it->second == "1");
    }
    if (auto it = config.find("gpu_collect_power"); it != config.end()) {
        config_.gpu_collect_power = (it->second == "true" || it->second == "1");
    }
    if (auto it = config.find("gpu_collect_clock"); it != config.end()) {
        config_.gpu_collect_clock = (it->second == "true" || it->second == "1");
    }
    if (auto it = config.find("gpu_collect_fan"); it != config.end()) {
        config_.gpu_collect_fan = (it->second == "true" || it->second == "1");
    }

    // Re-initialize collectors with new configuration
    battery_collector_.reset();
    power_collector_.reset();
    temperature_collector_.reset();
    gpu_collector_.reset();
    initialize_collectors();

    // Initialize each enabled collector
    bool all_success = true;

    if (battery_collector_) {
        std::unordered_map<std::string, std::string> battery_config;
        battery_config["collect_health"] = config_.battery_collect_health ? "true" : "false";
        battery_config["collect_thermal"] = config_.battery_collect_thermal ? "true" : "false";
        all_success &= battery_collector_->initialize(battery_config);
    }

    if (power_collector_) {
        std::unordered_map<std::string, std::string> power_config;
        power_config["collect_battery"] = config_.power_collect_battery ? "true" : "false";
        power_config["collect_rapl"] = config_.power_collect_rapl ? "true" : "false";
        all_success &= power_collector_->initialize(power_config);
    }

    if (temperature_collector_) {
        std::unordered_map<std::string, std::string> temp_config;
        temp_config["collect_thresholds"] = config_.temperature_collect_thresholds ? "true" : "false";
        temp_config["collect_warnings"] = config_.temperature_collect_warnings ? "true" : "false";
        all_success &= temperature_collector_->initialize(temp_config);
    }

    if (gpu_collector_) {
        std::unordered_map<std::string, std::string> gpu_config;
        gpu_config["collect_utilization"] = config_.gpu_collect_utilization ? "true" : "false";
        gpu_config["collect_memory"] = config_.gpu_collect_memory ? "true" : "false";
        gpu_config["collect_temperature"] = config_.gpu_collect_temperature ? "true" : "false";
        gpu_config["collect_power"] = config_.gpu_collect_power ? "true" : "false";
        gpu_config["collect_clock"] = config_.gpu_collect_clock ? "true" : "false";
        gpu_config["collect_fan"] = config_.gpu_collect_fan ? "true" : "false";
        all_success &= gpu_collector_->initialize(gpu_config);
    }

    initialized_ = all_success;
    return all_success;
}

std::vector<metric> hardware_plugin::collect() {
    std::vector<metric> all_metrics;
    ++total_collections_;

    try {
        if (battery_collector_ && is_battery_available()) {
            auto metrics = battery_collector_->collect();
            all_metrics.insert(all_metrics.end(), metrics.begin(), metrics.end());
        }

        if (power_collector_ && is_power_available()) {
            auto metrics = power_collector_->collect();
            all_metrics.insert(all_metrics.end(), metrics.begin(), metrics.end());
        }

        if (temperature_collector_ && is_temperature_available()) {
            auto metrics = temperature_collector_->collect();
            all_metrics.insert(all_metrics.end(), metrics.begin(), metrics.end());
        }

        if (gpu_collector_ && is_gpu_available()) {
            auto metrics = gpu_collector_->collect();
            all_metrics.insert(all_metrics.end(), metrics.begin(), metrics.end());
        }
    } catch (...) {
        ++collection_errors_;
    }

    return all_metrics;
}

std::string hardware_plugin::get_name() const {
    return "hardware_plugin";
}

std::vector<std::string> hardware_plugin::get_metric_types() const {
    std::vector<std::string> types;

    if (battery_collector_) {
        types.push_back("hardware.battery.level_percent");
        types.push_back("hardware.battery.charging");
        types.push_back("hardware.battery.health_percent");
        types.push_back("hardware.battery.capacity_wh");
        types.push_back("hardware.battery.cycle_count");
        types.push_back("hardware.battery.voltage_v");
        types.push_back("hardware.battery.current_a");
        types.push_back("hardware.battery.power_w");
        types.push_back("hardware.battery.temperature_celsius");
        types.push_back("hardware.battery.time_to_empty_sec");
        types.push_back("hardware.battery.time_to_full_sec");
    }

    if (power_collector_) {
        types.push_back("hardware.power.consumption_watts");
        types.push_back("hardware.power.voltage_volts");
        types.push_back("hardware.power.energy_joules");
        types.push_back("hardware.power.limit_watts");
    }

    if (temperature_collector_) {
        types.push_back("hardware.temperature.cpu_celsius");
        types.push_back("hardware.temperature.gpu_celsius");
        types.push_back("hardware.temperature.ambient_celsius");
        types.push_back("hardware.temperature.critical_threshold_celsius");
        types.push_back("hardware.temperature.warning_threshold_celsius");
    }

    if (gpu_collector_) {
        types.push_back("hardware.gpu.utilization_percent");
        types.push_back("hardware.gpu.memory_used_bytes");
        types.push_back("hardware.gpu.memory_total_bytes");
        types.push_back("hardware.gpu.temperature_celsius");
        types.push_back("hardware.gpu.power_watts");
        types.push_back("hardware.gpu.power_limit_watts");
        types.push_back("hardware.gpu.clock_mhz");
        types.push_back("hardware.gpu.memory_clock_mhz");
        types.push_back("hardware.gpu.fan_speed_percent");
    }

    return types;
}

bool hardware_plugin::is_healthy() const {
    // Plugin is healthy if at least one collector is available and working
    bool has_available_collector = is_battery_available() || is_power_available() ||
                                   is_temperature_available() || is_gpu_available();

    // Also check if we haven't had too many errors
    double error_rate = 0.0;
    if (total_collections_ > 0) {
        error_rate =
            static_cast<double>(collection_errors_.load()) / static_cast<double>(total_collections_.load());
    }

    return has_available_collector && error_rate < 0.5;
}

std::unordered_map<std::string, double> hardware_plugin::get_statistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    std::unordered_map<std::string, double> stats;

    stats["total_collections"] = static_cast<double>(total_collections_.load());
    stats["collection_errors"] = static_cast<double>(collection_errors_.load());
    stats["battery_available"] = is_battery_available() ? 1.0 : 0.0;
    stats["power_available"] = is_power_available() ? 1.0 : 0.0;
    stats["temperature_available"] = is_temperature_available() ? 1.0 : 0.0;
    stats["gpu_available"] = is_gpu_available() ? 1.0 : 0.0;
    stats["battery_enabled"] = config_.enable_battery ? 1.0 : 0.0;
    stats["power_enabled"] = config_.enable_power ? 1.0 : 0.0;
    stats["temperature_enabled"] = config_.enable_temperature ? 1.0 : 0.0;
    stats["gpu_enabled"] = config_.enable_gpu ? 1.0 : 0.0;

    return stats;
}

bool hardware_plugin::is_battery_available() const {
    if (!battery_collector_) {
        return false;
    }
    return battery_collector_->is_battery_available();
}

bool hardware_plugin::is_power_available() const {
    if (!power_collector_) {
        return false;
    }
    return power_collector_->is_power_available();
}

bool hardware_plugin::is_temperature_available() const {
    if (!temperature_collector_) {
        return false;
    }
    return temperature_collector_->is_thermal_available();
}

bool hardware_plugin::is_gpu_available() const {
    if (!gpu_collector_) {
        return false;
    }
    return gpu_collector_->is_gpu_available();
}

hardware_plugin_config hardware_plugin::get_config() const {
    return config_;
}

}  // namespace plugins
}  // namespace monitoring
}  // namespace kcenon
