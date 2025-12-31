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
#include <kcenon/monitoring/platform/metrics_provider.h>

#include <algorithm>

namespace kcenon {
namespace monitoring {

// ============================================================================
// tcp_state_info_collector implementation
// ============================================================================

tcp_state_info_collector::tcp_state_info_collector()
    : provider_(platform::metrics_provider::create()) {}

tcp_state_info_collector::~tcp_state_info_collector() = default;

bool tcp_state_info_collector::is_tcp_state_monitoring_available() const {
    if (!provider_) {
        return false;
    }
    auto stats = provider_->get_tcp_states();
    return stats.available;
}

tcp_state_metrics tcp_state_info_collector::collect_metrics() {
    tcp_state_metrics result;
    result.timestamp = std::chrono::system_clock::now();

    if (!provider_) {
        return result;
    }

    auto stats = provider_->get_tcp_states();
    if (!stats.available) {
        return result;
    }

    result.combined_counts.established = stats.established;
    result.combined_counts.syn_sent = stats.syn_sent;
    result.combined_counts.syn_recv = stats.syn_recv;
    result.combined_counts.fin_wait1 = stats.fin_wait1;
    result.combined_counts.fin_wait2 = stats.fin_wait2;
    result.combined_counts.time_wait = stats.time_wait;
    result.combined_counts.close_wait = stats.close_wait;
    result.combined_counts.last_ack = stats.last_ack;
    result.combined_counts.listen = stats.listen;
    result.combined_counts.closing = stats.closing;
    result.total_connections = stats.total;
    result.metrics_available = true;

    return result;
}

// ============================================================================
// tcp_state_collector implementation
// ============================================================================

tcp_state_collector::tcp_state_collector()
    : collector_(std::make_unique<tcp_state_info_collector>()) {}

bool tcp_state_collector::do_initialize(const config_map& config) {
    auto it = config.find("include_ipv6");
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

std::vector<metric> tcp_state_collector::do_collect() {
    std::vector<metric> metrics;

    auto tcp_data = collector_->collect_metrics();

    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        last_metrics_ = tcp_data;
    }

    if (tcp_data.metrics_available) {
        add_tcp_state_metrics(metrics, tcp_data);
    }

    return metrics;
}

std::vector<std::string> tcp_state_collector::do_get_metric_types() const {
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

bool tcp_state_collector::is_available() const {
    return collector_->is_tcp_state_monitoring_available();
}

void tcp_state_collector::do_add_statistics(stats_map& stats) const {
    stats["available"] = collector_->is_tcp_state_monitoring_available() ? 1.0 : 0.0;
}

tcp_state_metrics tcp_state_collector::get_last_metrics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return last_metrics_;
}

bool tcp_state_collector::is_tcp_state_monitoring_available() const {
    return collector_->is_tcp_state_monitoring_available();
}

void tcp_state_collector::add_tcp_state_metrics(
    std::vector<metric>& metrics,
    const tcp_state_metrics& tcp_data) {

    // Add metrics for each TCP state
    const auto& counts = tcp_data.combined_counts;

    std::unordered_map<std::string, std::string> base_tags;

    // Individual state metrics
    metrics.push_back(create_base_metric("tcp_connections_established",
        static_cast<double>(counts.established), base_tags, "connections"));
    metrics.push_back(create_base_metric("tcp_connections_syn_sent",
        static_cast<double>(counts.syn_sent), base_tags, "connections"));
    metrics.push_back(create_base_metric("tcp_connections_syn_recv",
        static_cast<double>(counts.syn_recv), base_tags, "connections"));
    metrics.push_back(create_base_metric("tcp_connections_fin_wait1",
        static_cast<double>(counts.fin_wait1), base_tags, "connections"));
    metrics.push_back(create_base_metric("tcp_connections_fin_wait2",
        static_cast<double>(counts.fin_wait2), base_tags, "connections"));
    metrics.push_back(create_base_metric("tcp_connections_time_wait",
        static_cast<double>(counts.time_wait), base_tags, "connections"));
    metrics.push_back(create_base_metric("tcp_connections_close",
        static_cast<double>(counts.close), base_tags, "connections"));
    metrics.push_back(create_base_metric("tcp_connections_close_wait",
        static_cast<double>(counts.close_wait), base_tags, "connections"));
    metrics.push_back(create_base_metric("tcp_connections_last_ack",
        static_cast<double>(counts.last_ack), base_tags, "connections"));
    metrics.push_back(create_base_metric("tcp_connections_listen",
        static_cast<double>(counts.listen), base_tags, "connections"));
    metrics.push_back(create_base_metric("tcp_connections_closing",
        static_cast<double>(counts.closing), base_tags, "connections"));

    // Total connections
    metrics.push_back(create_base_metric("tcp_connections_total",
        static_cast<double>(tcp_data.total_connections), base_tags, "connections"));

    // IPv4-specific metrics if available
    if (tcp_data.ipv4_counts.total() > 0) {
        std::unordered_map<std::string, std::string> ipv4_tags = base_tags;
        ipv4_tags["ip_version"] = "4";
        metrics.push_back(create_base_metric("tcp_connections_ipv4_total",
            static_cast<double>(tcp_data.ipv4_counts.total()), ipv4_tags, "connections"));
    }

    // IPv6-specific metrics if available and configured
    if (include_ipv6_ && tcp_data.ipv6_counts.total() > 0) {
        std::unordered_map<std::string, std::string> ipv6_tags = base_tags;
        ipv6_tags["ip_version"] = "6";
        metrics.push_back(create_base_metric("tcp_connections_ipv6_total",
            static_cast<double>(tcp_data.ipv6_counts.total()), ipv6_tags, "connections"));
    }

    // Warning indicators
    if (counts.time_wait >= time_wait_warning_threshold_) {
        std::unordered_map<std::string, std::string> warning_tags = base_tags;
        warning_tags["alert"] = "time_wait_high";
        metrics.push_back(create_base_metric("tcp_connections_warning",
            static_cast<double>(counts.time_wait), warning_tags, "connections"));
    }

    if (counts.close_wait >= close_wait_warning_threshold_) {
        std::unordered_map<std::string, std::string> warning_tags = base_tags;
        warning_tags["alert"] = "close_wait_high";
        metrics.push_back(create_base_metric("tcp_connections_warning",
            static_cast<double>(counts.close_wait), warning_tags, "connections"));
    }
}

}  // namespace monitoring
}  // namespace kcenon
