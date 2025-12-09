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

/**
 * @file security_collector.cpp
 * @brief Shared implementation of security_collector class
 */

#include <kcenon/monitoring/collectors/security_collector.h>

namespace kcenon {
namespace monitoring {

security_collector::security_collector()
    : collector_(std::make_unique<security_info_collector>()) {
}

bool security_collector::initialize(
    const std::unordered_map<std::string, std::string>& config) {
    
    // Parse configuration options
    if (auto it = config.find("enabled"); it != config.end()) {
        enabled_ = (it->second == "true" || it->second == "1");
    }
    
    if (auto it = config.find("mask_pii"); it != config.end()) {
        mask_pii_ = (it->second == "true" || it->second == "1");
        collector_->set_mask_pii(mask_pii_);
    }
    
    if (auto it = config.find("max_recent_events"); it != config.end()) {
        try {
            max_recent_events_ = std::stoull(it->second);
            collector_->set_max_recent_events(max_recent_events_);
        } catch (...) {
            // Keep default value
        }
    }
    
    if (auto it = config.find("login_failure_rate_limit"); it != config.end()) {
        try {
            login_failure_rate_limit_ = std::stod(it->second);
        } catch (...) {
            // Keep default value
        }
    }
    
    return true;
}

std::vector<std::string> security_collector::get_metric_types() const {
    return {
        "security_login_success_total",
        "security_login_failure_total",
        "security_logout_total",
        "security_sudo_usage_total",
        "security_permission_change_total",
        "security_account_created_total",
        "security_account_deleted_total",
        "security_events_total",
        "security_events_per_second",
        "security_active_sessions"
    };
}

bool security_collector::is_healthy() const {
    if (!enabled_) {
        return true;  // Disabled is not unhealthy
    }
    
    // Check error rate
    size_t count = collection_count_.load();
    size_t errors = collection_errors_.load();
    
    if (count == 0) {
        return true;
    }
    
    double error_rate = static_cast<double>(errors) / static_cast<double>(count);
    return error_rate < 0.5;  // Less than 50% errors
}

std::unordered_map<std::string, double> security_collector::get_statistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    return {
        {"collection_count", static_cast<double>(collection_count_.load())},
        {"collection_errors", static_cast<double>(collection_errors_.load())},
        {"enabled", enabled_ ? 1.0 : 0.0},
        {"available", collector_->is_security_monitoring_available() ? 1.0 : 0.0},
        {"mask_pii", mask_pii_ ? 1.0 : 0.0},
        {"max_recent_events", static_cast<double>(max_recent_events_)}
    };
}

security_metrics security_collector::get_last_metrics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return last_metrics_;
}

bool security_collector::is_security_monitoring_available() const {
    return collector_->is_security_monitoring_available();
}

metric security_collector::create_metric(
    const std::string& name, double value,
    const std::unordered_map<std::string, std::string>& tags,
    const std::string& unit) const {
    
    metric m;
    m.name = name;
    m.value = value;
    m.type = metric_type::gauge;
    m.timestamp = std::chrono::system_clock::now();
    m.tags = tags;
    if (!unit.empty()) {
        m.tags["unit"] = unit;
    }
    m.tags["collector"] = "security";
    return m;
}

void security_collector::add_security_metrics(
    std::vector<metric>& metrics,
    const security_metrics& security_data) {
    
    if (!security_data.metrics_available) {
        return;
    }
    
    const auto& counts = security_data.event_counts;
    
    // Event counters
    metrics.push_back(create_metric(
        "security_login_success_total",
        static_cast<double>(counts.login_success),
        {{"event_type", "login_success"}},
        "count"));
    
    metrics.push_back(create_metric(
        "security_login_failure_total",
        static_cast<double>(counts.login_failure),
        {{"event_type", "login_failure"}},
        "count"));
    
    metrics.push_back(create_metric(
        "security_logout_total",
        static_cast<double>(counts.logout),
        {{"event_type", "logout"}},
        "count"));
    
    metrics.push_back(create_metric(
        "security_sudo_usage_total",
        static_cast<double>(counts.sudo_usage),
        {{"event_type", "sudo_usage"}},
        "count"));
    
    metrics.push_back(create_metric(
        "security_permission_change_total",
        static_cast<double>(counts.permission_change),
        {{"event_type", "permission_change"}},
        "count"));
    
    metrics.push_back(create_metric(
        "security_account_created_total",
        static_cast<double>(counts.account_created),
        {{"event_type", "account_created"}},
        "count"));
    
    metrics.push_back(create_metric(
        "security_account_deleted_total",
        static_cast<double>(counts.account_deleted),
        {{"event_type", "account_deleted"}},
        "count"));
    
    // Total and rate metrics
    metrics.push_back(create_metric(
        "security_events_total",
        static_cast<double>(counts.total()),
        {},
        "count"));
    
    metrics.push_back(create_metric(
        "security_events_per_second",
        security_data.events_per_second,
        {},
        "events/s"));
    
    // Active sessions gauge
    metrics.push_back(create_metric(
        "security_active_sessions",
        static_cast<double>(security_data.active_sessions),
        {},
        "sessions"));
}

std::vector<metric> security_collector::collect() {
    std::vector<metric> metrics;
    
    if (!enabled_) {
        return metrics;
    }
    
    ++collection_count_;
    
    try {
        auto security_data = collector_->collect_metrics();
        
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            last_metrics_ = security_data;
        }
        
        add_security_metrics(metrics, security_data);
        
    } catch (...) {
        ++collection_errors_;
    }
    
    return metrics;
}

}  // namespace monitoring
}  // namespace kcenon
