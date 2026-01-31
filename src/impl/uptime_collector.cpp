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

#include <kcenon/monitoring/collectors/uptime_collector.h>
#include <kcenon/monitoring/platform/metrics_provider.h>

namespace kcenon {
namespace monitoring {

// ============================================================================
// uptime_info_collector implementation
// ============================================================================

uptime_info_collector::uptime_info_collector()
    : provider_(platform::metrics_provider::create()) {}

uptime_info_collector::~uptime_info_collector() = default;

bool uptime_info_collector::is_uptime_monitoring_available() const {
    if (!provider_) {
        return false;
    }
    auto uptime = provider_->get_uptime();
    return uptime.available;
}

uptime_metrics uptime_info_collector::collect_metrics() {
    uptime_metrics result;
    result.timestamp = std::chrono::system_clock::now();

    if (!provider_) {
        return result;
    }

    auto uptime = provider_->get_uptime();
    if (!uptime.available) {
        return result;
    }

    result.uptime_seconds = static_cast<double>(uptime.uptime_seconds);
    result.idle_seconds = static_cast<double>(uptime.idle_seconds);
    result.boot_timestamp = std::chrono::system_clock::to_time_t(uptime.boot_time);
    result.metrics_available = true;

    return result;
}

// ============================================================================
// uptime_collector implementation
// ============================================================================

uptime_collector::uptime_collector()
    : collector_(std::make_unique<uptime_info_collector>()) {}

auto uptime_collector::initialize(const config_map& config) -> bool {
    if (auto it = config.find("enabled"); it != config.end()) {
        enabled_ = (it->second == "true" || it->second == "1");
    }

    if (auto it = config.find("collect_idle_time"); it != config.end()) {
        collect_idle_time_ = (it->second == "true" || it->second == "1");
    }

    return true;
}

auto uptime_collector::get_metric_types() const -> std::vector<std::string> {
    return {
        "system_uptime_seconds",
        "system_boot_timestamp",
        "system_idle_seconds"
    };
}

auto uptime_collector::is_available() const -> bool {
    return collector_->is_uptime_monitoring_available();
}

bool uptime_collector::is_uptime_monitoring_available() const {
    return collector_->is_uptime_monitoring_available();
}

auto uptime_collector::get_statistics() const -> stats_map {
    stats_map stats;
    stats["enabled"] = enabled_ ? 1.0 : 0.0;
    stats["collect_idle_time"] = collect_idle_time_ ? 1.0 : 0.0;
    stats["collection_count"] = static_cast<double>(collection_count_.load());
    stats["collection_errors"] = static_cast<double>(collection_errors_.load());
    return stats;
}

uptime_metrics uptime_collector::get_last_metrics() const {
    return last_metrics_;
}

void uptime_collector::add_uptime_metrics(
    std::vector<metric>& metrics,
    const uptime_metrics& uptime_data) {

    if (!uptime_data.metrics_available) {
        return;
    }

    // System uptime in seconds
    metric m1;
    m1.name = "system_uptime_seconds";
    m1.value = uptime_data.uptime_seconds;
    m1.timestamp = std::chrono::system_clock::now();
    m1.tags["collector"] = "uptime";
    metrics.push_back(m1);

    // Boot timestamp
    metric m2;
    m2.name = "system_boot_timestamp";
    m2.value = static_cast<double>(uptime_data.boot_timestamp);
    m2.timestamp = std::chrono::system_clock::now();
    m2.tags["collector"] = "uptime";
    metrics.push_back(m2);

    // Idle time (Linux only, when enabled)
    if (collect_idle_time_ && uptime_data.idle_seconds > 0.0) {
        metric m3;
        m3.name = "system_idle_seconds";
        m3.value = uptime_data.idle_seconds;
        m3.timestamp = std::chrono::system_clock::now();
        m3.tags["collector"] = "uptime";
        metrics.push_back(m3);
    }
}

auto uptime_collector::collect() -> std::vector<metric> {
    std::vector<metric> metrics;

    if (!enabled_) {
        return metrics;
    }

    try {
        auto uptime_data = collector_->collect_metrics();
        last_metrics_ = uptime_data;

        add_uptime_metrics(metrics, uptime_data);
        ++collection_count_;
    } catch (...) {
        ++collection_errors_;
    }

    return metrics;
}

}  // namespace monitoring
}  // namespace kcenon
