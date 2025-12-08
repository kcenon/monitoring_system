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

#include <kcenon/monitoring/collectors/tcp_state_collector.h>

#include <algorithm>

namespace kcenon {
namespace monitoring {

tcp_state_collector::tcp_state_collector()
    : collector_(std::make_unique<tcp_state_info_collector>()) {}

bool tcp_state_collector::initialize(
    const std::unordered_map<std::string, std::string>& config) {
    // Parse configuration options
    auto it = config.find("enabled");
    if (it != config.end()) {
        enabled_ = (it->second == "true" || it->second == "1");
    }

    it = config.find("include_ipv6");
    if (it != config.end()) {
        include_ipv6_ = (it->second == "true" || it->second == "1");
    }

    it = config.find("time_wait_warning_threshold");
    if (it != config.end()) {
        try {
            time_wait_warning_threshold_ = std::stoull(it->second);
        } catch (...) {
            // Use default if parsing fails
        }
    }

    it = config.find("close_wait_warning_threshold");
    if (it != config.end()) {
        try {
            close_wait_warning_threshold_ = std::stoull(it->second);
        } catch (...) {
            // Use default if parsing fails
        }
    }

    return true;
}

std::vector<metric> tcp_state_collector::collect() {
    std::vector<metric> metrics;

    if (!enabled_) {
        return metrics;
    }

    try {
        auto tcp_data = collector_->collect_metrics();

        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            last_metrics_ = tcp_data;
        }

        if (tcp_data.metrics_available) {
            add_tcp_state_metrics(metrics, tcp_data);
            ++collection_count_;
        } else {
            ++collection_errors_;
        }
    } catch (...) {
        ++collection_errors_;
    }

    return metrics;
}

std::vector<std::string> tcp_state_collector::get_metric_types() const {
    return {
        "tcp_connections_established",
        "tcp_connections_syn_sent",
        "tcp_connections_syn_recv",
        "tcp_connections_fin_wait1",
        "tcp_connections_fin_wait2",
        "tcp_connections_time_wait",
        "tcp_connections_close",
        "tcp_connections_close_wait",
        "tcp_connections_last_ack",
        "tcp_connections_listen",
        "tcp_connections_closing",
        "tcp_connections_total"
    };
}

bool tcp_state_collector::is_healthy() const {
    if (!enabled_) {
        return true;  // Disabled is healthy
    }

    // Healthy if we can collect metrics and haven't had too many errors
    size_t total = collection_count_.load() + collection_errors_.load();
    if (total == 0) {
        return collector_->is_tcp_state_monitoring_available();
    }

    double error_rate = static_cast<double>(collection_errors_.load()) / total;
    return error_rate < 0.5;  // Less than 50% error rate
}

std::unordered_map<std::string, double> tcp_state_collector::get_statistics() const {
    std::unordered_map<std::string, double> stats;
    stats["collection_count"] = static_cast<double>(collection_count_.load());
    stats["collection_errors"] = static_cast<double>(collection_errors_.load());
    stats["enabled"] = enabled_ ? 1.0 : 0.0;
    stats["available"] = collector_->is_tcp_state_monitoring_available() ? 1.0 : 0.0;
    return stats;
}

tcp_state_metrics tcp_state_collector::get_last_metrics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return last_metrics_;
}

bool tcp_state_collector::is_tcp_state_monitoring_available() const {
    return collector_->is_tcp_state_monitoring_available();
}

metric tcp_state_collector::create_metric(
    const std::string& name, double value,
    const std::unordered_map<std::string, std::string>& tags,
    const std::string& /* unit */) const {
    metric m;
    m.name = name;
    m.value = value;
    m.tags = tags;
    m.timestamp = std::chrono::system_clock::now();
    return m;
}

void tcp_state_collector::add_tcp_state_metrics(
    std::vector<metric>& metrics,
    const tcp_state_metrics& tcp_data) {
    
    // Add metrics for each TCP state
    const auto& counts = tcp_data.combined_counts;
    
    std::unordered_map<std::string, std::string> base_tags = {
        {"collector", "tcp_state"}
    };

    // Individual state metrics
    metrics.push_back(create_metric("tcp_connections_established",
        static_cast<double>(counts.established), base_tags, "connections"));
    metrics.push_back(create_metric("tcp_connections_syn_sent",
        static_cast<double>(counts.syn_sent), base_tags, "connections"));
    metrics.push_back(create_metric("tcp_connections_syn_recv",
        static_cast<double>(counts.syn_recv), base_tags, "connections"));
    metrics.push_back(create_metric("tcp_connections_fin_wait1",
        static_cast<double>(counts.fin_wait1), base_tags, "connections"));
    metrics.push_back(create_metric("tcp_connections_fin_wait2",
        static_cast<double>(counts.fin_wait2), base_tags, "connections"));
    metrics.push_back(create_metric("tcp_connections_time_wait",
        static_cast<double>(counts.time_wait), base_tags, "connections"));
    metrics.push_back(create_metric("tcp_connections_close",
        static_cast<double>(counts.close), base_tags, "connections"));
    metrics.push_back(create_metric("tcp_connections_close_wait",
        static_cast<double>(counts.close_wait), base_tags, "connections"));
    metrics.push_back(create_metric("tcp_connections_last_ack",
        static_cast<double>(counts.last_ack), base_tags, "connections"));
    metrics.push_back(create_metric("tcp_connections_listen",
        static_cast<double>(counts.listen), base_tags, "connections"));
    metrics.push_back(create_metric("tcp_connections_closing",
        static_cast<double>(counts.closing), base_tags, "connections"));

    // Total connections
    metrics.push_back(create_metric("tcp_connections_total",
        static_cast<double>(tcp_data.total_connections), base_tags, "connections"));

    // IPv4-specific metrics if available
    if (tcp_data.ipv4_counts.total() > 0) {
        std::unordered_map<std::string, std::string> ipv4_tags = base_tags;
        ipv4_tags["ip_version"] = "4";
        metrics.push_back(create_metric("tcp_connections_ipv4_total",
            static_cast<double>(tcp_data.ipv4_counts.total()), ipv4_tags, "connections"));
    }

    // IPv6-specific metrics if available and configured
    if (include_ipv6_ && tcp_data.ipv6_counts.total() > 0) {
        std::unordered_map<std::string, std::string> ipv6_tags = base_tags;
        ipv6_tags["ip_version"] = "6";
        metrics.push_back(create_metric("tcp_connections_ipv6_total",
            static_cast<double>(tcp_data.ipv6_counts.total()), ipv6_tags, "connections"));
    }

    // Warning indicators
    if (counts.time_wait >= time_wait_warning_threshold_) {
        std::unordered_map<std::string, std::string> warning_tags = base_tags;
        warning_tags["alert"] = "time_wait_high";
        metrics.push_back(create_metric("tcp_connections_warning",
            static_cast<double>(counts.time_wait), warning_tags, "connections"));
    }

    if (counts.close_wait >= close_wait_warning_threshold_) {
        std::unordered_map<std::string, std::string> warning_tags = base_tags;
        warning_tags["alert"] = "close_wait_high";
        metrics.push_back(create_metric("tcp_connections_warning",
            static_cast<double>(counts.close_wait), warning_tags, "connections"));
    }
}

}  // namespace monitoring
}  // namespace kcenon
