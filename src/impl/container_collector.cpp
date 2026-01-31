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

#include <kcenon/monitoring/collectors/container_collector.h>

#include <algorithm>

namespace kcenon {
namespace monitoring {

container_collector::container_collector()
    : collector_(std::make_unique<container_info_collector>()) {}

bool container_collector::initialize(const config_map& config) {
    // Parse configuration options
    auto it = config.find("enabled");
    if (it != config.end()) {
        enabled_ = (it->second == "true" || it->second == "1");
    }

    it = config.find("collect_network");
    if (it != config.end()) {
        collect_network_metrics_ = (it->second == "true" || it->second == "1");
    }

    it = config.find("collect_blkio");
    if (it != config.end()) {
        collect_blkio_metrics_ = (it->second == "true" || it->second == "1");
    }

    return true;
}

std::vector<metric> container_collector::collect() {
    std::vector<metric> metrics;

    if (!enabled_ || !collector_) {
        return metrics;
    }

    try {
        // Enumerate containers
        auto containers = collector_->enumerate_containers();
        containers_found_.store(containers.size());

        std::vector<container_metrics> collected_metrics;

        for (const auto& container : containers) {
            auto container_metrics = collector_->collect_container_metrics(container);
            add_container_metrics(metrics, container_metrics);
            collected_metrics.push_back(std::move(container_metrics));
        }

        // Store last collected metrics
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            last_metrics_ = std::move(collected_metrics);
        }

        collection_count_++;

    } catch (const std::exception&) {
        collection_errors_++;
    }

    return metrics;
}

std::vector<std::string> container_collector::get_metric_types() const {
    return {"container_cpu_usage_percent",  "container_memory_usage_bytes",
            "container_memory_limit_bytes", "container_memory_usage_percent",
            "container_network_rx_bytes",   "container_network_tx_bytes",
            "container_blkio_read_bytes",   "container_blkio_write_bytes",
            "container_pids_current"};
}

bool container_collector::is_available() const {
#if defined(__linux__)
    return collector_ && collector_->detect_cgroup_version() != cgroup_version::none;
#else
    return false;  // Container metrics only available on Linux
#endif
}

bool container_collector::is_healthy() const { return enabled_ && collector_ != nullptr; }

stats_map container_collector::get_statistics() const {
    return {{"collection_count", static_cast<double>(collection_count_.load())},
            {"collection_errors", static_cast<double>(collection_errors_.load())},
            {"containers_found", static_cast<double>(containers_found_.load())}};
}

std::vector<container_metrics> container_collector::get_last_metrics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return last_metrics_;
}

bool container_collector::is_container_environment() const {
    return collector_ && collector_->is_containerized();
}

metric container_collector::create_metric(const std::string& name, double value,
                                          const container_metrics& container,
                                          const std::string& /* unit */) const {
    metric m;
    m.name = name;
    m.value = value;
    m.timestamp = container.timestamp;
    m.tags["container_id"] = container.container_id;

    if (!container.container_name.empty()) {
        m.tags["container_name"] = container.container_name;
    }
    if (!container.image_name.empty()) {
        m.tags["image_name"] = container.image_name;
    }

    return m;
}

void container_collector::add_container_metrics(std::vector<metric>& metrics,
                                                const container_metrics& container) {
    // CPU metrics
    metrics.push_back(create_metric("container_cpu_usage_percent", container.cpu_usage_percent,
                                    container, "percent"));

    // Memory metrics
    metrics.push_back(create_metric("container_memory_usage_bytes",
                                    static_cast<double>(container.memory_usage_bytes), container,
                                    "bytes"));
    metrics.push_back(create_metric("container_memory_limit_bytes",
                                    static_cast<double>(container.memory_limit_bytes), container,
                                    "bytes"));
    metrics.push_back(create_metric("container_memory_usage_percent",
                                    container.memory_usage_percent, container, "percent"));

    // Network metrics
    if (collect_network_metrics_) {
        metrics.push_back(create_metric("container_network_rx_bytes",
                                        static_cast<double>(container.network_rx_bytes), container,
                                        "bytes"));
        metrics.push_back(create_metric("container_network_tx_bytes",
                                        static_cast<double>(container.network_tx_bytes), container,
                                        "bytes"));
    }

    // Block I/O metrics
    if (collect_blkio_metrics_) {
        metrics.push_back(create_metric("container_blkio_read_bytes",
                                        static_cast<double>(container.blkio_read_bytes), container,
                                        "bytes"));
        metrics.push_back(create_metric("container_blkio_write_bytes",
                                        static_cast<double>(container.blkio_write_bytes), container,
                                        "bytes"));
    }

    // Process metrics
    metrics.push_back(create_metric(
        "container_pids_current", static_cast<double>(container.pids_current), container, "count"));
}

}  // namespace monitoring
}  // namespace kcenon
