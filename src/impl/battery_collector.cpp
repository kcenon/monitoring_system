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

namespace kcenon {
namespace monitoring {

// ============================================================================
// battery_collector implementation
// ============================================================================

battery_collector::battery_collector()
    : collector_(std::make_unique<battery_info_collector>()) {}

bool battery_collector::initialize(
    const std::unordered_map<std::string, std::string>& config) {

    // Parse configuration
    if (auto it = config.find("enabled"); it != config.end()) {
        enabled_ = (it->second == "true" || it->second == "1");
    }

    if (auto it = config.find("collect_health"); it != config.end()) {
        collect_health_ = (it->second == "true" || it->second == "1");
    }

    if (auto it = config.find("collect_thermal"); it != config.end()) {
        collect_thermal_ = (it->second == "true" || it->second == "1");
    }

    // Count batteries
    auto batteries = collector_->enumerate_batteries();
    batteries_found_.store(batteries.size());

    return true;
}

std::vector<std::string> battery_collector::get_metric_types() const {
    return {
        "battery_level_percent",
        "battery_charging",
        "battery_time_to_empty_seconds",
        "battery_time_to_full_seconds",
        "battery_health_percent",
        "battery_voltage_volts",
        "battery_power_watts",
        "battery_cycle_count",
        "battery_temperature_celsius"
    };
}

bool battery_collector::is_healthy() const {
    return enabled_ && collector_->is_battery_available();
}

bool battery_collector::is_battery_available() const {
    return collector_->is_battery_available();
}

std::unordered_map<std::string, double> battery_collector::get_statistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return {
        {"enabled", enabled_ ? 1.0 : 0.0},
        {"collection_count", static_cast<double>(collection_count_.load())},
        {"collection_errors", static_cast<double>(collection_errors_.load())},
        {"batteries_found", static_cast<double>(batteries_found_.load())},
        {"collect_health", collect_health_ ? 1.0 : 0.0},
        {"collect_thermal", collect_thermal_ ? 1.0 : 0.0}
    };
}

std::vector<battery_reading> battery_collector::get_last_readings() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return last_readings_;
}

metric battery_collector::create_metric(
    const std::string& name,
    double value,
    const battery_reading& reading,
    const std::string& /* unit */) const {

    metric m;
    m.name = name;
    m.value = value;
    m.timestamp = std::chrono::system_clock::now();
    m.tags["collector"] = "battery_collector";
    m.tags["battery_id"] = reading.info.id;

    if (!reading.info.name.empty()) {
        m.tags["battery_name"] = reading.info.name;
    }
    if (!reading.info.manufacturer.empty()) {
        m.tags["manufacturer"] = reading.info.manufacturer;
    }

    return m;
}

void battery_collector::add_battery_metrics(
    std::vector<metric>& metrics,
    const battery_reading& reading) {

    if (!reading.metrics_available) {
        return;
    }

    // Battery level percentage (gauge)
    metrics.push_back(create_metric(
        "battery_level_percent",
        reading.level_percent,
        reading,
        "percent"
    ));

    // Battery charging status (1 = charging, 0 = not charging)
    metrics.push_back(create_metric(
        "battery_charging",
        reading.is_charging ? 1.0 : 0.0,
        reading,
        "boolean"
    ));

    // Add status tag to a separate metric
    {
        metric status_metric = create_metric(
            "battery_status",
            static_cast<double>(static_cast<int>(reading.status)),
            reading,
            "enum"
        );
        status_metric.tags["status"] = battery_status_to_string(reading.status);
        metrics.push_back(status_metric);
    }

    // AC connected status
    metrics.push_back(create_metric(
        "battery_ac_connected",
        reading.ac_connected ? 1.0 : 0.0,
        reading,
        "boolean"
    ));

    // Time to empty (only if discharging and available)
    if (reading.time_to_empty_seconds > 0) {
        metrics.push_back(create_metric(
            "battery_time_to_empty_seconds",
            static_cast<double>(reading.time_to_empty_seconds),
            reading,
            "seconds"
        ));
    }

    // Time to full (only if charging and available)
    if (reading.time_to_full_seconds > 0) {
        metrics.push_back(create_metric(
            "battery_time_to_full_seconds",
            static_cast<double>(reading.time_to_full_seconds),
            reading,
            "seconds"
        ));
    }

    // Voltage
    if (reading.voltage_volts > 0.0) {
        metrics.push_back(create_metric(
            "battery_voltage_volts",
            reading.voltage_volts,
            reading,
            "volts"
        ));
    }

    // Power (current draw/charge rate)
    if (reading.power_watts > 0.0) {
        metrics.push_back(create_metric(
            "battery_power_watts",
            reading.power_watts,
            reading,
            "watts"
        ));
    }

    // Health metrics (when enabled)
    if (collect_health_) {
        // Battery health percentage
        if (reading.health_percent > 0.0) {
            metrics.push_back(create_metric(
                "battery_health_percent",
                reading.health_percent,
                reading,
                "percent"
            ));
        }

        // Design capacity
        if (reading.design_capacity_wh > 0.0) {
            metrics.push_back(create_metric(
                "battery_design_capacity_wh",
                reading.design_capacity_wh,
                reading,
                "watt_hours"
            ));
        }

        // Full charge capacity
        if (reading.full_charge_capacity_wh > 0.0) {
            metrics.push_back(create_metric(
                "battery_full_charge_capacity_wh",
                reading.full_charge_capacity_wh,
                reading,
                "watt_hours"
            ));
        }

        // Cycle count
        if (reading.cycle_count >= 0) {
            metrics.push_back(create_metric(
                "battery_cycle_count",
                static_cast<double>(reading.cycle_count),
                reading,
                "count"
            ));
        }
    }

    // Thermal metrics (when enabled and available)
    if (collect_thermal_ && reading.temperature_available) {
        metrics.push_back(create_metric(
            "battery_temperature_celsius",
            reading.temperature_celsius,
            reading,
            "celsius"
        ));
    }
}

std::vector<metric> battery_collector::collect() {
    std::vector<metric> metrics;

    if (!enabled_) {
        return metrics;
    }

    try {
        auto readings = collector_->read_all_batteries();

        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            last_readings_ = readings;
        }

        batteries_found_.store(readings.size());

        for (const auto& reading : readings) {
            add_battery_metrics(metrics, reading);
        }

        ++collection_count_;

    } catch (...) {
        ++collection_errors_;
    }

    return metrics;
}

}  // namespace monitoring
}  // namespace kcenon
