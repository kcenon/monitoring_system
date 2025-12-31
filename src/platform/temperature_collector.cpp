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
#include <kcenon/monitoring/platform/metrics_provider.h>

namespace kcenon {
namespace monitoring {

// ============================================================================
// temperature_info_collector implementation
// ============================================================================

temperature_info_collector::temperature_info_collector()
    : provider_(platform::metrics_provider::create()) {}

temperature_info_collector::~temperature_info_collector() = default;

bool temperature_info_collector::is_thermal_available() const {
    return provider_ && provider_->is_temperature_available();
}

std::vector<temperature_sensor_info> temperature_info_collector::enumerate_sensors() {
    if (!provider_) {
        return {};
    }

    auto readings = provider_->get_temperature_readings();
    std::vector<temperature_sensor_info> result;
    result.reserve(readings.size());

    for (const auto& reading : readings) {
        result.push_back(reading.sensor);
    }

    return result;
}

std::vector<temperature_reading> temperature_info_collector::read_all_temperatures() {
    if (!provider_) {
        return {};
    }
    return provider_->get_temperature_readings();
}

// ============================================================================
// temperature_collector implementation (platform-independent)
// ============================================================================

temperature_collector::temperature_collector()
    : collector_(std::make_unique<temperature_info_collector>()) {}

bool temperature_collector::initialize(const std::unordered_map<std::string, std::string>& config) {
    // Parse enabled option
    auto enabled_it = config.find("enabled");
    if (enabled_it != config.end()) {
        enabled_ = (enabled_it->second == "true" || enabled_it->second == "1");
    }

    // Parse collect_thresholds option
    auto thresholds_it = config.find("collect_thresholds");
    if (thresholds_it != config.end()) {
        collect_thresholds_ = (thresholds_it->second == "true" || thresholds_it->second == "1");
    }

    // Parse collect_warnings option
    auto warnings_it = config.find("collect_warnings");
    if (warnings_it != config.end()) {
        collect_warnings_ = (warnings_it->second == "true" || warnings_it->second == "1");
    }

    return true;
}

std::vector<metric> temperature_collector::collect() {
    std::vector<metric> metrics;

    ++collection_count_;

    if (!enabled_) {
        return metrics;
    }

    try {
        auto readings = collector_->read_all_temperatures();

        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            last_readings_ = readings;
            sensors_found_ = readings.size();
        }

        for (const auto& reading : readings) {
            add_sensor_metrics(metrics, reading);
        }
    } catch (...) {
        ++collection_errors_;
    }

    return metrics;
}

std::vector<std::string> temperature_collector::get_metric_types() const {
    return {"temperature_celsius", "temperature_critical_threshold",
            "temperature_warning_threshold", "temperature_is_critical", "temperature_is_warning"};
}

bool temperature_collector::is_healthy() const { return enabled_; }

std::unordered_map<std::string, double> temperature_collector::get_statistics() const {
    std::unordered_map<std::string, double> stats;
    stats["collection_count"] = static_cast<double>(collection_count_.load());
    stats["collection_errors"] = static_cast<double>(collection_errors_.load());
    stats["sensors_found"] = static_cast<double>(sensors_found_.load());
    return stats;
}

std::vector<temperature_reading> temperature_collector::get_last_readings() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return last_readings_;
}

bool temperature_collector::is_thermal_available() const {
    return collector_->is_thermal_available();
}

metric temperature_collector::create_metric(const std::string& name, double value,
                                            const temperature_reading& reading,
                                            const std::string& unit) const {
    std::unordered_map<std::string, std::string> tags;
    tags["sensor_id"] = reading.sensor.id;
    tags["sensor_name"] = reading.sensor.name;
    tags["sensor_type"] = sensor_type_to_string(reading.sensor.type);
    if (!unit.empty()) {
        tags["unit"] = unit;
    }

    return metric(name, value, tags, metric_type::gauge);
}

void temperature_collector::add_sensor_metrics(std::vector<metric>& metrics,
                                               const temperature_reading& reading) {
    // Always add the current temperature
    metrics.push_back(
        create_metric("temperature_celsius", reading.temperature_celsius, reading, "celsius"));

    // Add thresholds if available and configured
    if (collect_thresholds_ && reading.thresholds_available) {
        if (reading.critical_threshold_celsius > 0.0) {
            metrics.push_back(create_metric("temperature_critical_threshold",
                                            reading.critical_threshold_celsius, reading,
                                            "celsius"));
        }
        if (reading.warning_threshold_celsius > 0.0) {
            metrics.push_back(create_metric("temperature_warning_threshold",
                                            reading.warning_threshold_celsius, reading, "celsius"));
        }
    }

    // Add warning/critical status if configured
    if (collect_warnings_) {
        metrics.push_back(
            create_metric("temperature_is_critical", reading.is_critical ? 1.0 : 0.0, reading));
        metrics.push_back(
            create_metric("temperature_is_warning", reading.is_warning ? 1.0 : 0.0, reading));
    }
}

}  // namespace monitoring
}  // namespace kcenon
