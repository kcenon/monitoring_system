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

namespace kcenon {
namespace monitoring {

// power_collector implementation (platform-independent)

power_collector::power_collector()
    : collector_(std::make_unique<power_info_collector>()) {}

bool power_collector::initialize(const std::unordered_map<std::string, std::string>& config) {
    // Parse enabled option
    auto enabled_it = config.find("enabled");
    if (enabled_it != config.end()) {
        enabled_ = (enabled_it->second == "true" || enabled_it->second == "1");
    }

    // Parse collect_battery option
    auto battery_it = config.find("collect_battery");
    if (battery_it != config.end()) {
        collect_battery_ = (battery_it->second == "true" || battery_it->second == "1");
    }

    // Parse collect_rapl option
    auto rapl_it = config.find("collect_rapl");
    if (rapl_it != config.end()) {
        collect_rapl_ = (rapl_it->second == "true" || rapl_it->second == "1");
    }

    return true;
}

std::vector<metric> power_collector::collect() {
    std::vector<metric> metrics;

    ++collection_count_;

    if (!enabled_) {
        return metrics;
    }

    try {
        auto readings = collector_->read_all_power();

        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            last_readings_ = readings;
            sources_found_ = readings.size();
        }

        for (const auto& reading : readings) {
            // Filter based on configuration
            if (!collect_battery_ && 
                (reading.source.type == power_source_type::battery ||
                 reading.source.type == power_source_type::ac)) {
                continue;
            }
            if (!collect_rapl_ &&
                (reading.source.type == power_source_type::cpu ||
                 reading.source.type == power_source_type::package ||
                 reading.source.type == power_source_type::memory)) {
                continue;
            }

            add_source_metrics(metrics, reading);
        }
    } catch (...) {
        ++collection_errors_;
    }

    return metrics;
}

std::vector<std::string> power_collector::get_metric_types() const {
    return {"power_consumption_watts", "energy_consumed_joules", "power_limit_watts",
            "voltage_volts", "battery_percent", "battery_capacity_wh",
            "battery_charge_rate", "battery_is_charging", "battery_is_discharging"};
}

bool power_collector::is_healthy() const { return enabled_; }

std::unordered_map<std::string, double> power_collector::get_statistics() const {
    std::unordered_map<std::string, double> stats;
    stats["collection_count"] = static_cast<double>(collection_count_.load());
    stats["collection_errors"] = static_cast<double>(collection_errors_.load());
    stats["sources_found"] = static_cast<double>(sources_found_.load());
    return stats;
}

std::vector<power_reading> power_collector::get_last_readings() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return last_readings_;
}

bool power_collector::is_power_available() const {
    return collector_->is_power_available();
}

metric power_collector::create_metric(const std::string& name, double value,
                                       const power_reading& reading,
                                       const std::string& unit) const {
    std::unordered_map<std::string, std::string> tags;
    tags["source_id"] = reading.source.id;
    tags["source_name"] = reading.source.name;
    tags["source_type"] = power_source_type_to_string(reading.source.type);
    if (!unit.empty()) {
        tags["unit"] = unit;
    }

    return metric(name, value, tags, metric_type::gauge);
}

void power_collector::add_source_metrics(std::vector<metric>& metrics,
                                          const power_reading& reading) {
    // Add power consumption metric if available
    if (reading.power_available && reading.power_watts > 0.0) {
        metrics.push_back(
            create_metric("power_consumption_watts", reading.power_watts, reading, "watts"));
    }

    // Add energy consumed metric if available
    if (reading.power_available && reading.energy_joules > 0.0) {
        metrics.push_back(
            create_metric("energy_consumed_joules", reading.energy_joules, reading, "joules"));
    }

    // Add power limit if available
    if (reading.limits_available && reading.power_limit_watts > 0.0) {
        metrics.push_back(
            create_metric("power_limit_watts", reading.power_limit_watts, reading, "watts"));
    }

    // Add voltage if available
    if (reading.voltage_volts > 0.0) {
        metrics.push_back(
            create_metric("voltage_volts", reading.voltage_volts, reading, "volts"));
    }

    // Add battery metrics if available
    if (reading.battery_available) {
        metrics.push_back(
            create_metric("battery_percent", reading.battery_percent, reading, "percent"));

        if (reading.battery_capacity_wh > 0.0) {
            metrics.push_back(
                create_metric("battery_capacity_wh", reading.battery_capacity_wh, reading, "watt_hours"));
        }

        if (reading.battery_charge_rate != 0.0) {
            metrics.push_back(
                create_metric("battery_charge_rate", reading.battery_charge_rate, reading, "watts"));
        }

        metrics.push_back(
            create_metric("battery_is_charging", reading.is_charging ? 1.0 : 0.0, reading));
        metrics.push_back(
            create_metric("battery_is_discharging", reading.is_discharging ? 1.0 : 0.0, reading));
    }
}

}  // namespace monitoring
}  // namespace kcenon
