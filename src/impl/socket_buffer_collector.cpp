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

#include <kcenon/monitoring/collectors/socket_buffer_collector.h>

#include <algorithm>

namespace kcenon {
namespace monitoring {

socket_buffer_collector::socket_buffer_collector()
    : collector_(std::make_unique<socket_buffer_info_collector>()) {}

bool socket_buffer_collector::initialize(
    const std::unordered_map<std::string, std::string>& config) {
    // Parse configuration options
    auto it = config.find("enabled");
    if (it != config.end()) {
        enabled_ = (it->second == "true" || it->second == "1");
    }

    it = config.find("queue_full_threshold_bytes");
    if (it != config.end()) {
        try {
            queue_full_threshold_bytes_ = std::stoull(it->second);
        } catch (...) {
            // Use default if parsing fails
        }
    }

    it = config.find("memory_warning_threshold_bytes");
    if (it != config.end()) {
        try {
            memory_warning_threshold_bytes_ = std::stoull(it->second);
        } catch (...) {
            // Use default if parsing fails
        }
    }

    return true;
}

std::vector<metric> socket_buffer_collector::collect() {
    std::vector<metric> metrics;

    if (!enabled_) {
        return metrics;
    }

    try {
        auto buffer_data = collector_->collect_metrics();

        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            last_metrics_ = buffer_data;
        }

        if (buffer_data.metrics_available) {
            add_socket_buffer_metrics(metrics, buffer_data);
            ++collection_count_;
        } else {
            ++collection_errors_;
        }
    } catch (...) {
        ++collection_errors_;
    }

    return metrics;
}

std::vector<std::string> socket_buffer_collector::get_metric_types() const {
    return {
        "socket_recv_buffer_bytes",
        "socket_send_buffer_bytes",
        "socket_recv_queue_full_count",
        "socket_send_queue_full_count",
        "socket_memory_bytes",
        "socket_count_total",
        "socket_tcp_count",
        "socket_udp_count"
    };
}

bool socket_buffer_collector::is_healthy() const {
    if (!enabled_) {
        return true;  // Disabled is healthy
    }

    // Healthy if we can collect metrics and haven't had too many errors
    size_t total = collection_count_.load() + collection_errors_.load();
    if (total == 0) {
        return collector_->is_socket_buffer_monitoring_available();
    }

    double error_rate = static_cast<double>(collection_errors_.load()) / total;
    return error_rate < 0.5;  // Less than 50% error rate
}

std::unordered_map<std::string, double> socket_buffer_collector::get_statistics() const {
    std::unordered_map<std::string, double> stats;
    stats["collection_count"] = static_cast<double>(collection_count_.load());
    stats["collection_errors"] = static_cast<double>(collection_errors_.load());
    stats["enabled"] = enabled_ ? 1.0 : 0.0;
    stats["available"] = collector_->is_socket_buffer_monitoring_available() ? 1.0 : 0.0;
    return stats;
}

socket_buffer_metrics socket_buffer_collector::get_last_metrics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return last_metrics_;
}

bool socket_buffer_collector::is_socket_buffer_monitoring_available() const {
    return collector_->is_socket_buffer_monitoring_available();
}

metric socket_buffer_collector::create_metric(
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

void socket_buffer_collector::add_socket_buffer_metrics(
    std::vector<metric>& metrics,
    const socket_buffer_metrics& buffer_data) {
    
    std::unordered_map<std::string, std::string> base_tags = {
        {"collector", "socket_buffer"}
    };

    // Buffer usage metrics
    metrics.push_back(create_metric("socket_recv_buffer_bytes",
        static_cast<double>(buffer_data.recv_buffer_bytes), base_tags, "bytes"));
    metrics.push_back(create_metric("socket_send_buffer_bytes",
        static_cast<double>(buffer_data.send_buffer_bytes), base_tags, "bytes"));

    // Queue full counts
    metrics.push_back(create_metric("socket_recv_queue_full_count",
        static_cast<double>(buffer_data.recv_queue_full_count), base_tags, "count"));
    metrics.push_back(create_metric("socket_send_queue_full_count",
        static_cast<double>(buffer_data.send_queue_full_count), base_tags, "count"));

    // Memory usage
    metrics.push_back(create_metric("socket_memory_bytes",
        static_cast<double>(buffer_data.socket_memory_bytes), base_tags, "bytes"));

    // Socket counts
    metrics.push_back(create_metric("socket_count_total",
        static_cast<double>(buffer_data.socket_count), base_tags, "count"));
    metrics.push_back(create_metric("socket_tcp_count",
        static_cast<double>(buffer_data.tcp_socket_count), base_tags, "count"));
    metrics.push_back(create_metric("socket_udp_count",
        static_cast<double>(buffer_data.udp_socket_count), base_tags, "count"));

    // Warning indicators for high memory usage
    if (buffer_data.socket_memory_bytes >= memory_warning_threshold_bytes_) {
        std::unordered_map<std::string, std::string> warning_tags = base_tags;
        warning_tags["alert"] = "memory_high";
        metrics.push_back(create_metric("socket_buffer_warning",
            static_cast<double>(buffer_data.socket_memory_bytes), warning_tags, "bytes"));
    }

    // Warning for queue buildup
    uint64_t total_queued = buffer_data.recv_buffer_bytes + buffer_data.send_buffer_bytes;
    if (total_queued >= queue_full_threshold_bytes_ * buffer_data.tcp_socket_count) {
        std::unordered_map<std::string, std::string> warning_tags = base_tags;
        warning_tags["alert"] = "queue_buildup";
        metrics.push_back(create_metric("socket_buffer_warning",
            static_cast<double>(total_queued), warning_tags, "bytes"));
    }
}

}  // namespace monitoring
}  // namespace kcenon
