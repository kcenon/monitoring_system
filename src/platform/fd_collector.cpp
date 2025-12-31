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

#include <kcenon/monitoring/collectors/fd_collector.h>
#include <kcenon/monitoring/platform/metrics_provider.h>

#include <algorithm>
#include <cstdlib>

namespace kcenon {
namespace monitoring {

// ============================================================================
// fd_info_collector implementation
// ============================================================================

fd_info_collector::fd_info_collector()
    : provider_(platform::metrics_provider::create()) {}

fd_info_collector::~fd_info_collector() = default;

bool fd_info_collector::is_fd_monitoring_available() const {
    if (!provider_) {
        return false;
    }
    auto stats = provider_->get_fd_stats();
    return stats.available;
}

fd_metrics fd_info_collector::collect_metrics() {
    fd_metrics result;
    result.timestamp = std::chrono::system_clock::now();

    if (!provider_) {
        return result;
    }

    auto stats = provider_->get_fd_stats();
    if (!stats.available) {
        return result;
    }

    result.fd_used_process = stats.open_fds;
    result.fd_soft_limit = stats.max_fds;
    result.fd_hard_limit = stats.max_fds;
    result.fd_usage_percent = stats.usage_percent;
    result.system_metrics_available = false;

    return result;
}

// ============================================================================
// fd_collector - Main collector implementation
// ============================================================================

fd_collector::fd_collector() : collector_(std::make_unique<fd_info_collector>()) {}

bool fd_collector::do_initialize(const config_map& config) {
    auto warning_it = config.find("warning_threshold");
    if (warning_it != config.end()) {
        try {
            warning_threshold_ = std::stod(warning_it->second);
        } catch (...) {
            // Keep default
        }
    }

    auto critical_it = config.find("critical_threshold");
    if (critical_it != config.end()) {
        try {
            critical_threshold_ = std::stod(critical_it->second);
        } catch (...) {
            // Keep default
        }
    }

    return true;
}

std::vector<std::string> fd_collector::do_get_metric_types() const {
    return {"fd_used_system", "fd_max_system", "fd_used_process",
            "fd_soft_limit",  "fd_hard_limit", "fd_usage_percent"};
}

bool fd_collector::is_available() const {
    return collector_ && collector_->is_fd_monitoring_available();
}

void fd_collector::do_add_statistics(stats_map& stats) const {
    stats["warning_threshold"] = warning_threshold_;
    stats["critical_threshold"] = critical_threshold_;
}

fd_metrics fd_collector::get_last_metrics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return last_metrics_;
}

bool fd_collector::is_fd_monitoring_available() const {
    return collector_ && collector_->is_fd_monitoring_available();
}

void fd_collector::add_fd_metrics(std::vector<metric>& metrics, const fd_metrics& fd_data) {
    std::unordered_map<std::string, std::string> tags;

    // Process-level metrics (always available)
    metrics.push_back(create_base_metric("fd_used_process",
                                         static_cast<double>(fd_data.fd_used_process), tags, "count"));
    metrics.push_back(create_base_metric("fd_soft_limit",
                                         static_cast<double>(fd_data.fd_soft_limit), tags, "count"));
    metrics.push_back(create_base_metric("fd_hard_limit",
                                         static_cast<double>(fd_data.fd_hard_limit), tags, "count"));
    metrics.push_back(create_base_metric("fd_usage_percent", fd_data.fd_usage_percent, tags, "percent"));

    // System-level metrics (Linux only)
    if (fd_data.system_metrics_available) {
        tags["scope"] = "system";
        metrics.push_back(create_base_metric("fd_used_system",
                                             static_cast<double>(fd_data.fd_used_system), tags, "count"));
        metrics.push_back(create_base_metric("fd_max_system",
                                             static_cast<double>(fd_data.fd_max_system), tags, "count"));
    }

    // Threshold state metrics
    std::unordered_map<std::string, std::string> threshold_tags;
    if (fd_data.fd_usage_percent >= critical_threshold_) {
        threshold_tags["state"] = "critical";
        metrics.push_back(create_base_metric("fd_threshold_state", 2.0, threshold_tags));
    } else if (fd_data.fd_usage_percent >= warning_threshold_) {
        threshold_tags["state"] = "warning";
        metrics.push_back(create_base_metric("fd_threshold_state", 1.0, threshold_tags));
    } else {
        threshold_tags["state"] = "normal";
        metrics.push_back(create_base_metric("fd_threshold_state", 0.0, threshold_tags));
    }
}

std::vector<metric> fd_collector::do_collect() {
    std::vector<metric> metrics;

    if (!collector_) {
        return metrics;
    }

    fd_metrics fd_data = collector_->collect_metrics();

    // Store last metrics
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        last_metrics_ = fd_data;
    }

    add_fd_metrics(metrics, fd_data);

    return metrics;
}

}  // namespace monitoring
}  // namespace kcenon
