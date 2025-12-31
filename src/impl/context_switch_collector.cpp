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

#include <kcenon/monitoring/collectors/context_switch_collector.h>
#include <kcenon/monitoring/platform/metrics_provider.h>

namespace kcenon {
namespace monitoring {

// ============================================================================
// context_switch_info_collector implementation
// ============================================================================

context_switch_info_collector::context_switch_info_collector()
    : provider_(platform::metrics_provider::create()) {}

context_switch_info_collector::~context_switch_info_collector() = default;

bool context_switch_info_collector::is_context_switch_monitoring_available() const {
    if (!provider_) {
        return false;
    }
    auto cs = provider_->get_context_switches();
    return cs.available;
}

double context_switch_info_collector::calculate_rate(uint64_t current_switches) {
    auto now = std::chrono::steady_clock::now();

    if (!has_previous_sample_) {
        last_system_switches_ = current_switches;
        last_collection_time_ = now;
        has_previous_sample_ = true;
        return 0.0;
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_collection_time_).count();
    if (elapsed <= 0) {
        return 0.0;
    }

    double rate = static_cast<double>(current_switches - last_system_switches_) /
                  (static_cast<double>(elapsed) / 1000.0);

    last_system_switches_ = current_switches;
    last_collection_time_ = now;

    return rate;
}

context_switch_metrics context_switch_info_collector::collect_metrics() {
    context_switch_metrics result;
    result.timestamp = std::chrono::system_clock::now();

    if (!provider_) {
        return result;
    }

    auto cs = provider_->get_context_switches();
    if (!cs.available) {
        return result;
    }

    result.system_context_switches_total = cs.total_switches;
    result.context_switches_per_sec = calculate_rate(cs.total_switches);
    result.process_info.voluntary_switches = cs.voluntary_switches;
    result.process_info.nonvoluntary_switches = cs.involuntary_switches;
    result.process_info.total_switches = cs.voluntary_switches + cs.involuntary_switches;
    result.metrics_available = true;
    result.rate_available = has_previous_sample_;

    return result;
}

// ============================================================================
// context_switch_collector implementation
// ============================================================================

context_switch_collector::context_switch_collector()
    : collector_(std::make_unique<context_switch_info_collector>()) {}

bool context_switch_collector::do_initialize(const config_map& config) {
    if (auto it = config.find("collect_process_metrics"); it != config.end()) {
        collect_process_metrics_ = (it->second == "true" || it->second == "1");
    }

    if (auto it = config.find("rate_warning_threshold"); it != config.end()) {
        try {
            rate_warning_threshold_ = std::stod(it->second);
        } catch (...) {
            // Keep default
        }
    }

    return true;
}

std::vector<std::string> context_switch_collector::do_get_metric_types() const {
    return {
        "context_switches_total",
        "context_switches_per_sec",
        "voluntary_context_switches",
        "nonvoluntary_context_switches",
        "process_context_switches_total"
    };
}

bool context_switch_collector::is_available() const {
    return collector_->is_context_switch_monitoring_available();
}

bool context_switch_collector::is_context_switch_monitoring_available() const {
    return collector_->is_context_switch_monitoring_available();
}

void context_switch_collector::do_add_statistics(stats_map& stats) const {
    stats["rate_warning_threshold"] = rate_warning_threshold_;
    stats["collect_process_metrics"] = collect_process_metrics_ ? 1.0 : 0.0;
}

context_switch_metrics context_switch_collector::get_last_metrics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return last_metrics_;
}

void context_switch_collector::add_context_switch_metrics(
    std::vector<metric>& metrics,
    const context_switch_metrics& cs_data) {

    if (!cs_data.metrics_available) {
        return;
    }

    // System-wide metrics
    metrics.push_back(create_base_metric(
        "context_switches_total",
        static_cast<double>(cs_data.system_context_switches_total),
        {{"type", "system"}},
        "count"
    ));

    if (cs_data.rate_available) {
        metrics.push_back(create_base_metric(
            "context_switches_per_sec",
            cs_data.context_switches_per_sec,
            {{"type", "system"}},
            "switches/s"
        ));
    }

    // Process-level metrics
    if (collect_process_metrics_) {
        metrics.push_back(create_base_metric(
            "voluntary_context_switches",
            static_cast<double>(cs_data.process_info.voluntary_switches),
            {{"type", "process"}},
            "count"
        ));

        metrics.push_back(create_base_metric(
            "nonvoluntary_context_switches",
            static_cast<double>(cs_data.process_info.nonvoluntary_switches),
            {{"type", "process"}},
            "count"
        ));

        metrics.push_back(create_base_metric(
            "process_context_switches_total",
            static_cast<double>(cs_data.process_info.total_switches),
            {{"type", "process"}},
            "count"
        ));
    }
}

std::vector<metric> context_switch_collector::do_collect() {
    std::vector<metric> metrics;

    auto cs_data = collector_->collect_metrics();

    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        last_metrics_ = cs_data;
    }

    add_context_switch_metrics(metrics, cs_data);

    return metrics;
}

}  // namespace monitoring
}  // namespace kcenon
