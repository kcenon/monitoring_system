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

#include <kcenon/monitoring/collectors/network_metrics_collector.h>
#include <kcenon/monitoring/platform/metrics_provider.h>

#include <algorithm>

namespace kcenon {
namespace monitoring {

// ============================================================================
// network_info_collector implementation
// ============================================================================

network_info_collector::network_info_collector()
    : provider_(platform::metrics_provider::create()) {}

network_info_collector::~network_info_collector() = default;

bool network_info_collector::is_socket_buffer_monitoring_available() const {
    if (!provider_) {
        return false;
    }
    auto stats = provider_->get_socket_buffer_stats();
    return stats.available;
}

bool network_info_collector::is_tcp_state_monitoring_available() const {
    if (!provider_) {
        return false;
    }
    auto stats = provider_->get_tcp_states();
    return stats.available;
}

network_metrics network_info_collector::collect_metrics(const network_metrics_config& config) {
    network_metrics result;
    result.timestamp = std::chrono::system_clock::now();

    if (!provider_) {
        return result;
    }

    // Collect socket buffer metrics if enabled
    if (config.collect_socket_buffers) {
        auto buffer_stats = provider_->get_socket_buffer_stats();
        if (buffer_stats.available) {
            result.recv_buffer_bytes = buffer_stats.rx_buffer_used;
            result.send_buffer_bytes = buffer_stats.tx_buffer_used;
            result.socket_buffer_available = true;
        }
    }

    // Collect TCP state metrics if enabled
    if (config.collect_tcp_states) {
        auto tcp_stats = provider_->get_tcp_states();
        if (tcp_stats.available) {
            result.tcp_counts.established = tcp_stats.established;
            result.tcp_counts.syn_sent = tcp_stats.syn_sent;
            result.tcp_counts.syn_recv = tcp_stats.syn_recv;
            result.tcp_counts.fin_wait1 = tcp_stats.fin_wait1;
            result.tcp_counts.fin_wait2 = tcp_stats.fin_wait2;
            result.tcp_counts.time_wait = tcp_stats.time_wait;
            result.tcp_counts.close_wait = tcp_stats.close_wait;
            result.tcp_counts.last_ack = tcp_stats.last_ack;
            result.tcp_counts.listen = tcp_stats.listen;
            result.tcp_counts.closing = tcp_stats.closing;
            result.total_connections = tcp_stats.total;
            result.tcp_state_available = true;
        }
    }

    return result;
}

// ============================================================================
// network_metrics_collector implementation
// ============================================================================

network_metrics_collector::network_metrics_collector()
    : collector_(std::make_unique<network_info_collector>()) {}

bool network_metrics_collector::do_initialize(const config_map& config) {
    // Parse collect_socket_buffers
    if (auto it = config.find("collect_socket_buffers"); it != config.end()) {
        config_.collect_socket_buffers = (it->second == "true" || it->second == "1");
    }

    // Parse collect_tcp_states
    if (auto it = config.find("collect_tcp_states"); it != config.end()) {
        config_.collect_tcp_states = (it->second == "true" || it->second == "1");
    }

    // Parse time_wait_warning_threshold
    if (auto it = config.find("time_wait_warning_threshold"); it != config.end()) {
        try {
            config_.time_wait_warning_threshold = std::stoull(it->second);
        } catch (...) {
            // Use default if parsing fails
        }
    }

    // Parse close_wait_warning_threshold
    if (auto it = config.find("close_wait_warning_threshold"); it != config.end()) {
        try {
            config_.close_wait_warning_threshold = std::stoull(it->second);
        } catch (...) {
            // Use default if parsing fails
        }
    }

    // Parse queue_full_threshold_bytes
    if (auto it = config.find("queue_full_threshold_bytes"); it != config.end()) {
        try {
            config_.queue_full_threshold_bytes = std::stoull(it->second);
        } catch (...) {
            // Use default if parsing fails
        }
    }

    // Parse memory_warning_threshold_bytes
    if (auto it = config.find("memory_warning_threshold_bytes"); it != config.end()) {
        try {
            config_.memory_warning_threshold_bytes = std::stoull(it->second);
        } catch (...) {
            // Use default if parsing fails
        }
    }

    return true;
}

std::vector<metric> network_metrics_collector::do_collect() {
    std::vector<metric> metrics;

    auto data = collector_->collect_metrics(config_);

    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        last_metrics_ = data;
    }

    if (data.socket_buffer_available) {
        add_socket_buffer_metrics(metrics, data);
    }

    if (data.tcp_state_available) {
        add_tcp_state_metrics(metrics, data);
    }

    return metrics;
}

std::vector<std::string> network_metrics_collector::do_get_metric_types() const {
    std::vector<std::string> types;

    // Socket buffer metric types
    if (config_.collect_socket_buffers) {
        types.insert(types.end(), {
            "network_socket_recv_buffer_bytes",
            "network_socket_send_buffer_bytes",
            "network_socket_memory_bytes",
            "network_socket_count_total",
            "network_socket_tcp_count",
            "network_socket_udp_count"
        });
    }

    // TCP state metric types
    if (config_.collect_tcp_states) {
        types.insert(types.end(), {
            "network_tcp_connections_established",
            "network_tcp_connections_syn_sent",
            "network_tcp_connections_syn_recv",
            "network_tcp_connections_fin_wait1",
            "network_tcp_connections_fin_wait2",
            "network_tcp_connections_time_wait",
            "network_tcp_connections_close",
            "network_tcp_connections_close_wait",
            "network_tcp_connections_last_ack",
            "network_tcp_connections_listen",
            "network_tcp_connections_closing",
            "network_tcp_connections_total"
        });
    }

    return types;
}

bool network_metrics_collector::is_available() const {
    bool socket_available = config_.collect_socket_buffers &&
                            collector_->is_socket_buffer_monitoring_available();
    bool tcp_available = config_.collect_tcp_states &&
                         collector_->is_tcp_state_monitoring_available();
    return socket_available || tcp_available;
}

void network_metrics_collector::do_add_statistics(stats_map& stats) const {
    stats["socket_buffer_available"] =
        collector_->is_socket_buffer_monitoring_available() ? 1.0 : 0.0;
    stats["tcp_state_available"] =
        collector_->is_tcp_state_monitoring_available() ? 1.0 : 0.0;
    stats["collect_socket_buffers"] = config_.collect_socket_buffers ? 1.0 : 0.0;
    stats["collect_tcp_states"] = config_.collect_tcp_states ? 1.0 : 0.0;
}

network_metrics network_metrics_collector::get_last_metrics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return last_metrics_;
}

bool network_metrics_collector::is_socket_buffer_monitoring_available() const {
    return collector_->is_socket_buffer_monitoring_available();
}

bool network_metrics_collector::is_tcp_state_monitoring_available() const {
    return collector_->is_tcp_state_monitoring_available();
}

void network_metrics_collector::add_socket_buffer_metrics(
    std::vector<metric>& metrics,
    const network_metrics& data) {

    std::unordered_map<std::string, std::string> base_tags;

    // Buffer usage metrics
    metrics.push_back(create_base_metric("network_socket_recv_buffer_bytes",
        static_cast<double>(data.recv_buffer_bytes), base_tags, "bytes"));
    metrics.push_back(create_base_metric("network_socket_send_buffer_bytes",
        static_cast<double>(data.send_buffer_bytes), base_tags, "bytes"));

    // Memory usage
    metrics.push_back(create_base_metric("network_socket_memory_bytes",
        static_cast<double>(data.socket_memory_bytes), base_tags, "bytes"));

    // Socket counts
    metrics.push_back(create_base_metric("network_socket_count_total",
        static_cast<double>(data.socket_count), base_tags, "count"));
    metrics.push_back(create_base_metric("network_socket_tcp_count",
        static_cast<double>(data.tcp_socket_count), base_tags, "count"));
    metrics.push_back(create_base_metric("network_socket_udp_count",
        static_cast<double>(data.udp_socket_count), base_tags, "count"));

    // Warning indicators for high memory usage
    if (data.socket_memory_bytes >= config_.memory_warning_threshold_bytes) {
        std::unordered_map<std::string, std::string> warning_tags = base_tags;
        warning_tags["alert"] = "memory_high";
        metrics.push_back(create_base_metric("network_socket_warning",
            static_cast<double>(data.socket_memory_bytes), warning_tags, "bytes"));
    }

    // Warning for queue buildup
    uint64_t total_queued = data.recv_buffer_bytes + data.send_buffer_bytes;
    if (data.tcp_socket_count > 0 &&
        total_queued >= config_.queue_full_threshold_bytes * data.tcp_socket_count) {
        std::unordered_map<std::string, std::string> warning_tags = base_tags;
        warning_tags["alert"] = "queue_buildup";
        metrics.push_back(create_base_metric("network_socket_warning",
            static_cast<double>(total_queued), warning_tags, "bytes"));
    }
}

void network_metrics_collector::add_tcp_state_metrics(
    std::vector<metric>& metrics,
    const network_metrics& data) {

    const auto& counts = data.tcp_counts;
    std::unordered_map<std::string, std::string> base_tags;

    // Individual state metrics
    metrics.push_back(create_base_metric("network_tcp_connections_established",
        static_cast<double>(counts.established), base_tags, "connections"));
    metrics.push_back(create_base_metric("network_tcp_connections_syn_sent",
        static_cast<double>(counts.syn_sent), base_tags, "connections"));
    metrics.push_back(create_base_metric("network_tcp_connections_syn_recv",
        static_cast<double>(counts.syn_recv), base_tags, "connections"));
    metrics.push_back(create_base_metric("network_tcp_connections_fin_wait1",
        static_cast<double>(counts.fin_wait1), base_tags, "connections"));
    metrics.push_back(create_base_metric("network_tcp_connections_fin_wait2",
        static_cast<double>(counts.fin_wait2), base_tags, "connections"));
    metrics.push_back(create_base_metric("network_tcp_connections_time_wait",
        static_cast<double>(counts.time_wait), base_tags, "connections"));
    metrics.push_back(create_base_metric("network_tcp_connections_close",
        static_cast<double>(counts.close), base_tags, "connections"));
    metrics.push_back(create_base_metric("network_tcp_connections_close_wait",
        static_cast<double>(counts.close_wait), base_tags, "connections"));
    metrics.push_back(create_base_metric("network_tcp_connections_last_ack",
        static_cast<double>(counts.last_ack), base_tags, "connections"));
    metrics.push_back(create_base_metric("network_tcp_connections_listen",
        static_cast<double>(counts.listen), base_tags, "connections"));
    metrics.push_back(create_base_metric("network_tcp_connections_closing",
        static_cast<double>(counts.closing), base_tags, "connections"));

    // Total connections
    metrics.push_back(create_base_metric("network_tcp_connections_total",
        static_cast<double>(data.total_connections), base_tags, "connections"));

    // Warning indicators
    if (counts.time_wait >= config_.time_wait_warning_threshold) {
        std::unordered_map<std::string, std::string> warning_tags = base_tags;
        warning_tags["alert"] = "time_wait_high";
        metrics.push_back(create_base_metric("network_tcp_warning",
            static_cast<double>(counts.time_wait), warning_tags, "connections"));
    }

    if (counts.close_wait >= config_.close_wait_warning_threshold) {
        std::unordered_map<std::string, std::string> warning_tags = base_tags;
        warning_tags["alert"] = "close_wait_high";
        metrics.push_back(create_base_metric("network_tcp_warning",
            static_cast<double>(counts.close_wait), warning_tags, "connections"));
    }
}

}  // namespace monitoring
}  // namespace kcenon
