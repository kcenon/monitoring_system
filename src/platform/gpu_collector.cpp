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

#include <kcenon/monitoring/collectors/gpu_collector.h>

namespace kcenon {
namespace monitoring {

// gpu_collector implementation (platform-independent)

gpu_collector::gpu_collector() : collector_(std::make_unique<gpu_info_collector>()) {}

bool gpu_collector::initialize(const std::unordered_map<std::string, std::string>& config) {
    // Parse enabled option
    auto enabled_it = config.find("enabled");
    if (enabled_it != config.end()) {
        enabled_ = (enabled_it->second == "true" || enabled_it->second == "1");
    }

    // Parse collect_utilization option
    auto util_it = config.find("collect_utilization");
    if (util_it != config.end()) {
        collect_utilization_ = (util_it->second == "true" || util_it->second == "1");
    }

    // Parse collect_memory option
    auto memory_it = config.find("collect_memory");
    if (memory_it != config.end()) {
        collect_memory_ = (memory_it->second == "true" || memory_it->second == "1");
    }

    // Parse collect_temperature option
    auto temp_it = config.find("collect_temperature");
    if (temp_it != config.end()) {
        collect_temperature_ = (temp_it->second == "true" || temp_it->second == "1");
    }

    // Parse collect_power option
    auto power_it = config.find("collect_power");
    if (power_it != config.end()) {
        collect_power_ = (power_it->second == "true" || power_it->second == "1");
    }

    // Parse collect_clock option
    auto clock_it = config.find("collect_clock");
    if (clock_it != config.end()) {
        collect_clock_ = (clock_it->second == "true" || clock_it->second == "1");
    }

    // Parse collect_fan option
    auto fan_it = config.find("collect_fan");
    if (fan_it != config.end()) {
        collect_fan_ = (fan_it->second == "true" || fan_it->second == "1");
    }

    return true;
}

std::vector<metric> gpu_collector::collect() {
    std::vector<metric> metrics;

    ++collection_count_;

    if (!enabled_) {
        return metrics;
    }

    try {
        auto readings = collector_->read_all_gpu_metrics();

        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            last_readings_ = readings;
            gpus_found_ = readings.size();
        }

        for (const auto& reading : readings) {
            add_gpu_metrics(metrics, reading);
        }
    } catch (...) {
        ++collection_errors_;
    }

    return metrics;
}

std::vector<std::string> gpu_collector::get_metric_types() const {
    return {"gpu_utilization_percent",
            "gpu_memory_used_bytes",
            "gpu_memory_total_bytes",
            "gpu_memory_usage_percent",
            "gpu_temperature_celsius",
            "gpu_power_watts",
            "gpu_power_limit_watts",
            "gpu_clock_mhz",
            "gpu_memory_clock_mhz",
            "gpu_fan_speed_percent"};
}

bool gpu_collector::is_healthy() const { return enabled_; }

std::unordered_map<std::string, double> gpu_collector::get_statistics() const {
    std::unordered_map<std::string, double> stats;
    stats["collection_count"] = static_cast<double>(collection_count_.load());
    stats["collection_errors"] = static_cast<double>(collection_errors_.load());
    stats["gpus_found"] = static_cast<double>(gpus_found_.load());
    return stats;
}

std::vector<gpu_reading> gpu_collector::get_last_readings() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return last_readings_;
}

bool gpu_collector::is_gpu_available() const { return collector_->is_gpu_available(); }

metric gpu_collector::create_metric(const std::string& name, double value,
                                    const gpu_reading& reading, const std::string& unit) const {
    std::unordered_map<std::string, std::string> tags;
    tags["gpu_id"] = reading.device.id;
    tags["gpu_name"] = reading.device.name;
    tags["gpu_vendor"] = gpu_vendor_to_string(reading.device.vendor);
    tags["gpu_type"] = gpu_type_to_string(reading.device.type);
    tags["gpu_index"] = std::to_string(reading.device.device_index);
    if (!unit.empty()) {
        tags["unit"] = unit;
    }

    return metric(name, value, tags, metric_type::gauge);
}

void gpu_collector::add_gpu_metrics(std::vector<metric>& metrics, const gpu_reading& reading) {
    // Utilization metrics
    if (collect_utilization_ && reading.utilization_available) {
        metrics.push_back(
            create_metric("gpu_utilization_percent", reading.utilization_percent, reading, "percent"));
    }

    // Memory metrics
    if (collect_memory_ && reading.memory_available) {
        metrics.push_back(
            create_metric("gpu_memory_used_bytes", static_cast<double>(reading.memory_used_bytes),
                          reading, "bytes"));
        metrics.push_back(
            create_metric("gpu_memory_total_bytes", static_cast<double>(reading.memory_total_bytes),
                          reading, "bytes"));

        // Calculate memory usage percentage
        if (reading.memory_total_bytes > 0) {
            double memory_usage_percent = (static_cast<double>(reading.memory_used_bytes) /
                                           static_cast<double>(reading.memory_total_bytes)) *
                                          100.0;
            metrics.push_back(
                create_metric("gpu_memory_usage_percent", memory_usage_percent, reading, "percent"));
        }
    }

    // Temperature metrics
    if (collect_temperature_ && reading.temperature_available) {
        metrics.push_back(
            create_metric("gpu_temperature_celsius", reading.temperature_celsius, reading, "celsius"));
    }

    // Power metrics
    if (collect_power_ && reading.power_available) {
        metrics.push_back(create_metric("gpu_power_watts", reading.power_watts, reading, "watts"));
        if (reading.power_limit_watts > 0.0) {
            metrics.push_back(
                create_metric("gpu_power_limit_watts", reading.power_limit_watts, reading, "watts"));
        }
    }

    // Clock metrics
    if (collect_clock_ && reading.clock_available) {
        metrics.push_back(create_metric("gpu_clock_mhz", reading.clock_mhz, reading, "mhz"));
        if (reading.memory_clock_mhz > 0.0) {
            metrics.push_back(
                create_metric("gpu_memory_clock_mhz", reading.memory_clock_mhz, reading, "mhz"));
        }
    }

    // Fan metrics
    if (collect_fan_ && reading.fan_available) {
        metrics.push_back(
            create_metric("gpu_fan_speed_percent", reading.fan_speed_percent, reading, "percent"));
    }
}

}  // namespace monitoring
}  // namespace kcenon
