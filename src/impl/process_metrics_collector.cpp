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

#include <kcenon/monitoring/collectors/process_metrics_collector.h>
#include <kcenon/monitoring/platform/metrics_provider.h>

#include <algorithm>
#include <cstdlib>

namespace kcenon {
namespace monitoring {

// =============================================================================
// fd_info_collector implementation
// =============================================================================

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

// =============================================================================
// inode_info_collector implementation
// =============================================================================

inode_info_collector::inode_info_collector()
    : provider_(platform::metrics_provider::create()) {}

inode_info_collector::~inode_info_collector() = default;

bool inode_info_collector::is_inode_monitoring_available() const {
    if (!provider_) {
        return false;
    }
    auto stats = provider_->get_inode_stats();
    return !stats.empty() && stats[0].available;
}

inode_metrics inode_info_collector::collect_metrics() {
    inode_metrics result;
    result.timestamp = std::chrono::system_clock::now();

    if (!provider_) {
        return result;
    }

    auto stats = provider_->get_inode_stats();
    if (stats.empty()) {
        return result;
    }

    for (const auto& stat : stats) {
        if (!stat.available) {
            continue;
        }

        filesystem_inode_info fs_info;
        fs_info.mount_point = stat.filesystem;
        fs_info.inodes_total = stat.total_inodes;
        fs_info.inodes_used = stat.used_inodes;
        fs_info.inodes_free = stat.free_inodes;
        fs_info.inodes_usage_percent = stat.usage_percent;
        result.filesystems.push_back(fs_info);

        result.total_inodes += stat.total_inodes;
        result.total_inodes_used += stat.used_inodes;
        result.total_inodes_free += stat.free_inodes;

        if (stat.usage_percent > result.max_usage_percent) {
            result.max_usage_percent = stat.usage_percent;
            result.max_usage_mount_point = stat.filesystem;
        }
    }

    if (!result.filesystems.empty()) {
        result.average_usage_percent = result.total_inodes > 0 ?
            (static_cast<double>(result.total_inodes_used) / result.total_inodes * 100.0) : 0.0;
        result.metrics_available = true;
    }

    return result;
}

// =============================================================================
// context_switch_info_collector implementation
// =============================================================================

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

// =============================================================================
// process_metrics_collector implementation
// =============================================================================

process_metrics_collector::process_metrics_collector()
    : process_metrics_collector(process_metrics_config{}) {}

process_metrics_collector::process_metrics_collector(process_metrics_config config)
    : fd_collector_(std::make_unique<fd_info_collector>()),
      inode_collector_(std::make_unique<inode_info_collector>()),
      cs_collector_(std::make_unique<context_switch_info_collector>()),
      config_(std::move(config)) {}

bool process_metrics_collector::do_initialize(const config_map& config) {
    auto parse_bool = [](const std::string& value) {
        return value == "true" || value == "1";
    };

    auto parse_double = [](const std::string& value, double default_val) {
        try {
            return std::stod(value);
        } catch (...) {
            return default_val;
        }
    };

    if (auto it = config.find("collect_fd"); it != config.end()) {
        config_.collect_fd = parse_bool(it->second);
    }
    if (auto it = config.find("collect_inodes"); it != config.end()) {
        config_.collect_inodes = parse_bool(it->second);
    }
    if (auto it = config.find("collect_context_switches"); it != config.end()) {
        config_.collect_context_switches = parse_bool(it->second);
    }
    if (auto it = config.find("include_pseudo_fs"); it != config.end()) {
        config_.include_pseudo_fs = parse_bool(it->second);
    }
    if (auto it = config.find("fd_warning_threshold"); it != config.end()) {
        config_.fd_warning_threshold = parse_double(it->second, 80.0);
    }
    if (auto it = config.find("fd_critical_threshold"); it != config.end()) {
        config_.fd_critical_threshold = parse_double(it->second, 95.0);
    }
    if (auto it = config.find("inode_warning_threshold"); it != config.end()) {
        config_.inode_warning_threshold = parse_double(it->second, 80.0);
    }
    if (auto it = config.find("inode_critical_threshold"); it != config.end()) {
        config_.inode_critical_threshold = parse_double(it->second, 95.0);
    }
    if (auto it = config.find("context_switch_rate_warning"); it != config.end()) {
        config_.context_switch_rate_warning = parse_double(it->second, 100000.0);
    }

    return true;
}

std::vector<std::string> process_metrics_collector::do_get_metric_types() const {
    std::vector<std::string> types;

    if (config_.collect_fd) {
        types.insert(types.end(), {
            "process.fd.open_count",
            "process.fd.soft_limit",
            "process.fd.hard_limit",
            "process.fd.usage_percent",
            "process.fd.threshold_state"
        });
    }

    if (config_.collect_inodes) {
        types.insert(types.end(), {
            "process.fs.inodes_total",
            "process.fs.inodes_used",
            "process.fs.inodes_free",
            "process.fs.inodes_usage_percent",
            "process.fs.inodes_max_usage_percent",
            "process.fs.inodes_average_usage_percent",
            "process.fs.filesystem_count"
        });
    }

    if (config_.collect_context_switches) {
        types.insert(types.end(), {
            "process.context_switches.total",
            "process.context_switches.per_sec",
            "process.context_switches.voluntary",
            "process.context_switches.involuntary",
            "process.context_switches.process_total"
        });
    }

    return types;
}

bool process_metrics_collector::is_available() const {
    if (config_.collect_fd && fd_collector_ && fd_collector_->is_fd_monitoring_available()) {
        return true;
    }
    if (config_.collect_inodes && inode_collector_ && inode_collector_->is_inode_monitoring_available()) {
        return true;
    }
    if (config_.collect_context_switches && cs_collector_ && cs_collector_->is_context_switch_monitoring_available()) {
        return true;
    }
    return false;
}

void process_metrics_collector::do_add_statistics(stats_map& stats) const {
    stats["collect_fd"] = config_.collect_fd ? 1.0 : 0.0;
    stats["collect_inodes"] = config_.collect_inodes ? 1.0 : 0.0;
    stats["collect_context_switches"] = config_.collect_context_switches ? 1.0 : 0.0;
    stats["include_pseudo_fs"] = config_.include_pseudo_fs ? 1.0 : 0.0;
    stats["fd_warning_threshold"] = config_.fd_warning_threshold;
    stats["fd_critical_threshold"] = config_.fd_critical_threshold;
    stats["inode_warning_threshold"] = config_.inode_warning_threshold;
    stats["inode_critical_threshold"] = config_.inode_critical_threshold;
    stats["context_switch_rate_warning"] = config_.context_switch_rate_warning;
}

process_metrics process_metrics_collector::get_last_metrics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return last_metrics_;
}

fd_metrics process_metrics_collector::get_last_fd_metrics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return last_metrics_.fd;
}

inode_metrics process_metrics_collector::get_last_inode_metrics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return last_metrics_.inodes;
}

context_switch_metrics process_metrics_collector::get_last_context_switch_metrics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return last_metrics_.context_switches;
}

bool process_metrics_collector::is_fd_monitoring_available() const {
    return fd_collector_ && fd_collector_->is_fd_monitoring_available();
}

bool process_metrics_collector::is_inode_monitoring_available() const {
    return inode_collector_ && inode_collector_->is_inode_monitoring_available();
}

bool process_metrics_collector::is_context_switch_monitoring_available() const {
    return cs_collector_ && cs_collector_->is_context_switch_monitoring_available();
}

void process_metrics_collector::add_fd_metrics(std::vector<metric>& metrics, const fd_metrics& fd_data) {
    std::unordered_map<std::string, std::string> tags;

    metrics.push_back(create_base_metric("process.fd.open_count",
                                         static_cast<double>(fd_data.fd_used_process), tags, "count"));
    metrics.push_back(create_base_metric("process.fd.soft_limit",
                                         static_cast<double>(fd_data.fd_soft_limit), tags, "count"));
    metrics.push_back(create_base_metric("process.fd.hard_limit",
                                         static_cast<double>(fd_data.fd_hard_limit), tags, "count"));
    metrics.push_back(create_base_metric("process.fd.usage_percent",
                                         fd_data.fd_usage_percent, tags, "percent"));

    if (fd_data.system_metrics_available) {
        tags["scope"] = "system";
        metrics.push_back(create_base_metric("process.fd.system_used",
                                             static_cast<double>(fd_data.fd_used_system), tags, "count"));
        metrics.push_back(create_base_metric("process.fd.system_max",
                                             static_cast<double>(fd_data.fd_max_system), tags, "count"));
    }

    std::unordered_map<std::string, std::string> threshold_tags;
    if (fd_data.fd_usage_percent >= config_.fd_critical_threshold) {
        threshold_tags["state"] = "critical";
        metrics.push_back(create_base_metric("process.fd.threshold_state", 2.0, threshold_tags));
    } else if (fd_data.fd_usage_percent >= config_.fd_warning_threshold) {
        threshold_tags["state"] = "warning";
        metrics.push_back(create_base_metric("process.fd.threshold_state", 1.0, threshold_tags));
    } else {
        threshold_tags["state"] = "normal";
        metrics.push_back(create_base_metric("process.fd.threshold_state", 0.0, threshold_tags));
    }
}

void process_metrics_collector::add_inode_metrics(std::vector<metric>& metrics, const inode_metrics& inode_data) {
    if (!inode_data.metrics_available) {
        return;
    }

    metrics.push_back(create_base_metric("process.fs.inodes_total",
                                         static_cast<double>(inode_data.total_inodes), {}, "count"));
    metrics.push_back(create_base_metric("process.fs.inodes_used",
                                         static_cast<double>(inode_data.total_inodes_used), {}, "count"));
    metrics.push_back(create_base_metric("process.fs.inodes_free",
                                         static_cast<double>(inode_data.total_inodes_free), {}, "count"));
    metrics.push_back(create_base_metric("process.fs.inodes_average_usage_percent",
                                         inode_data.average_usage_percent, {}, "percent"));
    metrics.push_back(create_base_metric("process.fs.inodes_max_usage_percent",
                                         inode_data.max_usage_percent,
                                         {{"mount_point", inode_data.max_usage_mount_point}}, "percent"));
    metrics.push_back(create_base_metric("process.fs.filesystem_count",
                                         static_cast<double>(inode_data.filesystems.size()), {}, "count"));

    for (const auto& fs : inode_data.filesystems) {
        std::unordered_map<std::string, std::string> tags = {
            {"mount_point", fs.mount_point},
            {"filesystem_type", fs.filesystem_type},
            {"device", fs.device}
        };

        metrics.push_back(create_base_metric("process.fs.inodes_usage_percent",
                                             fs.inodes_usage_percent, tags, "percent"));
        metrics.push_back(create_base_metric("process.fs.inodes_total",
                                             static_cast<double>(fs.inodes_total), tags, "count"));
        metrics.push_back(create_base_metric("process.fs.inodes_used",
                                             static_cast<double>(fs.inodes_used), tags, "count"));
        metrics.push_back(create_base_metric("process.fs.inodes_free",
                                             static_cast<double>(fs.inodes_free), tags, "count"));
    }
}

void process_metrics_collector::add_context_switch_metrics(
    std::vector<metric>& metrics,
    const context_switch_metrics& cs_data) {

    if (!cs_data.metrics_available) {
        return;
    }

    metrics.push_back(create_base_metric(
        "process.context_switches.total",
        static_cast<double>(cs_data.system_context_switches_total),
        {{"type", "system"}},
        "count"
    ));

    if (cs_data.rate_available) {
        metrics.push_back(create_base_metric(
            "process.context_switches.per_sec",
            cs_data.context_switches_per_sec,
            {{"type", "system"}},
            "switches/s"
        ));
    }

    metrics.push_back(create_base_metric(
        "process.context_switches.voluntary",
        static_cast<double>(cs_data.process_info.voluntary_switches),
        {{"type", "process"}},
        "count"
    ));

    metrics.push_back(create_base_metric(
        "process.context_switches.involuntary",
        static_cast<double>(cs_data.process_info.nonvoluntary_switches),
        {{"type", "process"}},
        "count"
    ));

    metrics.push_back(create_base_metric(
        "process.context_switches.process_total",
        static_cast<double>(cs_data.process_info.total_switches),
        {{"type", "process"}},
        "count"
    ));
}

void process_metrics_collector::collect_fd_metrics(std::vector<metric>& metrics) {
    if (!config_.collect_fd || !fd_collector_) {
        return;
    }

    auto fd_data = fd_collector_->collect_metrics();
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        last_metrics_.fd = fd_data;
    }
    add_fd_metrics(metrics, fd_data);
}

void process_metrics_collector::collect_inode_metrics(std::vector<metric>& metrics) {
    if (!config_.collect_inodes || !inode_collector_) {
        return;
    }

    auto inode_data = inode_collector_->collect_metrics();
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        last_metrics_.inodes = inode_data;
    }
    add_inode_metrics(metrics, inode_data);
}

void process_metrics_collector::collect_context_switch_metrics(std::vector<metric>& metrics) {
    if (!config_.collect_context_switches || !cs_collector_) {
        return;
    }

    auto cs_data = cs_collector_->collect_metrics();
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        last_metrics_.context_switches = cs_data;
    }
    add_context_switch_metrics(metrics, cs_data);
}

std::vector<metric> process_metrics_collector::do_collect() {
    std::vector<metric> metrics;

    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        last_metrics_.timestamp = std::chrono::system_clock::now();
    }

    collect_fd_metrics(metrics);
    collect_inode_metrics(metrics);
    collect_context_switch_metrics(metrics);

    return metrics;
}

}  // namespace monitoring
}  // namespace kcenon
