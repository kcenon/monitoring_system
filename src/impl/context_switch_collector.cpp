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

namespace kcenon {
namespace monitoring {

// ============================================================================
// context_switch_collector implementation
// ============================================================================

context_switch_collector::context_switch_collector()
    : collector_(std::make_unique<context_switch_info_collector>()) {}

bool context_switch_collector::initialize(
    const std::unordered_map<std::string, std::string>& config) {
    
    // Parse configuration
    if (auto it = config.find("enabled"); it != config.end()) {
        enabled_ = (it->second == "true" || it->second == "1");
    }
    
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

std::vector<std::string> context_switch_collector::get_metric_types() const {
    return {
        "context_switches_total",
        "context_switches_per_sec",
        "voluntary_context_switches",
        "nonvoluntary_context_switches",
        "process_context_switches_total"
    };
}

bool context_switch_collector::is_healthy() const {
    return enabled_ && collector_->is_context_switch_monitoring_available();
}

bool context_switch_collector::is_context_switch_monitoring_available() const {
    return collector_->is_context_switch_monitoring_available();
}

std::unordered_map<std::string, double> context_switch_collector::get_statistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return {
        {"enabled", enabled_ ? 1.0 : 0.0},
        {"collection_count", static_cast<double>(collection_count_.load())},
        {"collection_errors", static_cast<double>(collection_errors_.load())},
        {"rate_warning_threshold", rate_warning_threshold_},
        {"collect_process_metrics", collect_process_metrics_ ? 1.0 : 0.0}
    };
}

context_switch_metrics context_switch_collector::get_last_metrics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return last_metrics_;
}

metric context_switch_collector::create_metric(
    const std::string& name, 
    double value,
    const std::unordered_map<std::string, std::string>& tags,
    const std::string& /* unit */) const {
    
    metric m;
    m.name = name;
    m.value = value;
    m.timestamp = std::chrono::system_clock::now();
    m.tags = tags;
    m.tags["collector"] = "context_switch_collector";
    return m;
}

void context_switch_collector::add_context_switch_metrics(
    std::vector<metric>& metrics,
    const context_switch_metrics& cs_data) {
    
    if (!cs_data.metrics_available) {
        return;
    }
    
    // System-wide metrics
    metrics.push_back(create_metric(
        "context_switches_total",
        static_cast<double>(cs_data.system_context_switches_total),
        {{"type", "system"}},
        "count"
    ));
    
    if (cs_data.rate_available) {
        metrics.push_back(create_metric(
            "context_switches_per_sec",
            cs_data.context_switches_per_sec,
            {{"type", "system"}},
            "switches/s"
        ));
    }
    
    // Process-level metrics
    if (collect_process_metrics_) {
        metrics.push_back(create_metric(
            "voluntary_context_switches",
            static_cast<double>(cs_data.process_info.voluntary_switches),
            {{"type", "process"}},
            "count"
        ));
        
        metrics.push_back(create_metric(
            "nonvoluntary_context_switches",
            static_cast<double>(cs_data.process_info.nonvoluntary_switches),
            {{"type", "process"}},
            "count"
        ));
        
        metrics.push_back(create_metric(
            "process_context_switches_total",
            static_cast<double>(cs_data.process_info.total_switches),
            {{"type", "process"}},
            "count"
        ));
    }
}

std::vector<metric> context_switch_collector::collect() {
    std::vector<metric> metrics;
    
    if (!enabled_) {
        return metrics;
    }
    
    try {
        auto cs_data = collector_->collect_metrics();
        
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            last_metrics_ = cs_data;
        }
        
        add_context_switch_metrics(metrics, cs_data);
        ++collection_count_;
        
    } catch (...) {
        ++collection_errors_;
    }
    
    return metrics;
}

}  // namespace monitoring
}  // namespace kcenon
