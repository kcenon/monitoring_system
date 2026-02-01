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

auto network_metrics_collector::initialize(const config_map& config) -> bool {
    // Parse enabled
    if (auto it = config.find("enabled"); it != config.end()) {
        enabled_ = (it->second == "true" || it->second == "1");
    }

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

auto network_metrics_collector::collect() -> std::vector<metric> {
    std::vector<metric> metrics;

    if (!enabled_) {
        return metrics;
    }

    auto data = collector_->collect_metrics(config_);

    bool has_data = data.socket_buffer_available || data.tcp_state_available;

    if (has_data) {
        last_metrics_ = data;

        if (data.socket_buffer_available) {
            add_socket_buffer_metrics(metrics, data);
        }

        if (data.tcp_state_available) {
            add_tcp_state_metrics(metrics, data);
        }

        collection_count_++;
    } else {
        collection_errors_++;
    }

    return metrics;
}

auto network_metrics_collector::get_metric_types() const -> std::vector<std::string> {
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

auto network_metrics_collector::is_available() const -> bool {
    bool socket_available = config_.collect_socket_buffers &&
                            collector_->is_socket_buffer_monitoring_available();
    bool tcp_available = config_.collect_tcp_states &&
                         collector_->is_tcp_state_monitoring_available();
    return socket_available || tcp_available;
}

auto network_metrics_collector::get_statistics() const -> stats_map {
    stats_map stats;
    stats["enabled"] = enabled_ ? 1.0 : 0.0;
    stats["socket_buffer_available"] =
        collector_->is_socket_buffer_monitoring_available() ? 1.0 : 0.0;
    stats["tcp_state_available"] =
        collector_->is_tcp_state_monitoring_available() ? 1.0 : 0.0;
    stats["collect_socket_buffers"] = config_.collect_socket_buffers ? 1.0 : 0.0;
    stats["collect_tcp_states"] = config_.collect_tcp_states ? 1.0 : 0.0;
    stats["collection_count"] = static_cast<double>(collection_count_.load());
    stats["collection_errors"] = static_cast<double>(collection_errors_.load());
    return stats;
}

network_metrics network_metrics_collector::get_last_metrics() const {
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

    auto now = std::chrono::system_clock::now();

    // Buffer usage metrics
    metric m1;
    m1.name = "network_socket_recv_buffer_bytes";
    m1.value = static_cast<double>(data.recv_buffer_bytes);
    m1.timestamp = now;
    m1.tags["collector"] = "network_metrics_collector";
    metrics.push_back(m1);

    metric m2;
    m2.name = "network_socket_send_buffer_bytes";
    m2.value = static_cast<double>(data.send_buffer_bytes);
    m2.timestamp = now;
    m2.tags["collector"] = "network_metrics_collector";
    metrics.push_back(m2);

    // Memory usage
    metric m3;
    m3.name = "network_socket_memory_bytes";
    m3.value = static_cast<double>(data.socket_memory_bytes);
    m3.timestamp = now;
    m3.tags["collector"] = "network_metrics_collector";
    metrics.push_back(m3);

    // Socket counts
    metric m4;
    m4.name = "network_socket_count_total";
    m4.value = static_cast<double>(data.socket_count);
    m4.timestamp = now;
    m4.tags["collector"] = "network_metrics_collector";
    metrics.push_back(m4);

    metric m5;
    m5.name = "network_socket_tcp_count";
    m5.value = static_cast<double>(data.tcp_socket_count);
    m5.timestamp = now;
    m5.tags["collector"] = "network_metrics_collector";
    metrics.push_back(m5);

    metric m6;
    m6.name = "network_socket_udp_count";
    m6.value = static_cast<double>(data.udp_socket_count);
    m6.timestamp = now;
    m6.tags["collector"] = "network_metrics_collector";
    metrics.push_back(m6);

    // Warning indicators for high memory usage
    if (data.socket_memory_bytes >= config_.memory_warning_threshold_bytes) {
        metric w1;
        w1.name = "network_socket_warning";
        w1.value = static_cast<double>(data.socket_memory_bytes);
        w1.timestamp = now;
        w1.tags["collector"] = "network_metrics_collector";
        w1.tags["alert"] = "memory_high";
        metrics.push_back(w1);
    }

    // Warning for queue buildup
    uint64_t total_queued = data.recv_buffer_bytes + data.send_buffer_bytes;
    if (data.tcp_socket_count > 0 &&
        total_queued >= config_.queue_full_threshold_bytes * data.tcp_socket_count) {
        metric w2;
        w2.name = "network_socket_warning";
        w2.value = static_cast<double>(total_queued);
        w2.timestamp = now;
        w2.tags["collector"] = "network_metrics_collector";
        w2.tags["alert"] = "queue_buildup";
        metrics.push_back(w2);
    }
}

void network_metrics_collector::add_tcp_state_metrics(
    std::vector<metric>& metrics,
    const network_metrics& data) {

    const auto& counts = data.tcp_counts;
    auto now = std::chrono::system_clock::now();

    // Helper lambda to create TCP state metrics
    auto create_tcp_metric = [&](const std::string& name, double value) {
        metric m;
        m.name = name;
        m.value = value;
        m.timestamp = now;
        m.tags["collector"] = "network_metrics_collector";
        return m;
    };

    // Individual state metrics
    metrics.push_back(create_tcp_metric("network_tcp_connections_established",
        static_cast<double>(counts.established)));
    metrics.push_back(create_tcp_metric("network_tcp_connections_syn_sent",
        static_cast<double>(counts.syn_sent)));
    metrics.push_back(create_tcp_metric("network_tcp_connections_syn_recv",
        static_cast<double>(counts.syn_recv)));
    metrics.push_back(create_tcp_metric("network_tcp_connections_fin_wait1",
        static_cast<double>(counts.fin_wait1)));
    metrics.push_back(create_tcp_metric("network_tcp_connections_fin_wait2",
        static_cast<double>(counts.fin_wait2)));
    metrics.push_back(create_tcp_metric("network_tcp_connections_time_wait",
        static_cast<double>(counts.time_wait)));
    metrics.push_back(create_tcp_metric("network_tcp_connections_close",
        static_cast<double>(counts.close)));
    metrics.push_back(create_tcp_metric("network_tcp_connections_close_wait",
        static_cast<double>(counts.close_wait)));
    metrics.push_back(create_tcp_metric("network_tcp_connections_last_ack",
        static_cast<double>(counts.last_ack)));
    metrics.push_back(create_tcp_metric("network_tcp_connections_listen",
        static_cast<double>(counts.listen)));
    metrics.push_back(create_tcp_metric("network_tcp_connections_closing",
        static_cast<double>(counts.closing)));

    // Total connections
    metrics.push_back(create_tcp_metric("network_tcp_connections_total",
        static_cast<double>(data.total_connections)));

    // Warning indicators
    if (counts.time_wait >= config_.time_wait_warning_threshold) {
        metric w1;
        w1.name = "network_tcp_warning";
        w1.value = static_cast<double>(counts.time_wait);
        w1.timestamp = now;
        w1.tags["collector"] = "network_metrics_collector";
        w1.tags["alert"] = "time_wait_high";
        metrics.push_back(w1);
    }

    if (counts.close_wait >= config_.close_wait_warning_threshold) {
        metric w2;
        w2.name = "network_tcp_warning";
        w2.value = static_cast<double>(counts.close_wait);
        w2.timestamp = now;
        w2.tags["collector"] = "network_metrics_collector";
        w2.tags["alert"] = "close_wait_high";
        metrics.push_back(w2);
    }
}

}  // namespace monitoring
}  // namespace kcenon
