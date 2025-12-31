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

#include <kcenon/monitoring/collectors/inode_collector.h>
#include <kcenon/monitoring/platform/metrics_provider.h>

namespace kcenon {
namespace monitoring {

// ============================================================================
// inode_info_collector implementation
// ============================================================================

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

// ============================================================================
// inode_collector implementation (common across all platforms)
// ============================================================================

inode_collector::inode_collector() : collector_(std::make_unique<inode_info_collector>()) {}

bool inode_collector::initialize(const std::unordered_map<std::string, std::string>& config) {
    // Parse configuration
    auto it = config.find("enabled");
    if (it != config.end()) {
        enabled_ = (it->second == "true" || it->second == "1");
    }

    it = config.find("include_pseudo_fs");
    if (it != config.end()) {
        include_pseudo_fs_ = (it->second == "true" || it->second == "1");
    }

    it = config.find("warning_threshold");
    if (it != config.end()) {
        try {
            warning_threshold_ = std::stod(it->second);
        } catch (...) {
            // Keep default
        }
    }

    it = config.find("critical_threshold");
    if (it != config.end()) {
        try {
            critical_threshold_ = std::stod(it->second);
        } catch (...) {
            // Keep default
        }
    }

    return true;
}

std::vector<metric> inode_collector::collect() {
    std::vector<metric> metrics;

    if (!enabled_) {
        return metrics;
    }

    try {
        auto inode_data = collector_->collect_metrics();
        
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            last_metrics_ = inode_data;
        }

        add_inode_metrics(metrics, inode_data);
        ++collection_count_;
    } catch (...) {
        ++collection_errors_;
    }

    return metrics;
}

std::vector<std::string> inode_collector::get_metric_types() const {
    return {
        "inodes_total",
        "inodes_used",
        "inodes_free",
        "inodes_usage_percent",
        "inodes_max_usage_percent",
        "inodes_average_usage_percent",
        "inodes_filesystem_count"
    };
}

bool inode_collector::is_healthy() const {
    return enabled_ && collector_->is_inode_monitoring_available();
}

std::unordered_map<std::string, double> inode_collector::get_statistics() const {
    return {
        {"collection_count", static_cast<double>(collection_count_.load())},
        {"collection_errors", static_cast<double>(collection_errors_.load())},
        {"warning_threshold", warning_threshold_},
        {"critical_threshold", critical_threshold_},
        {"enabled", enabled_ ? 1.0 : 0.0}
    };
}

inode_metrics inode_collector::get_last_metrics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return last_metrics_;
}

bool inode_collector::is_inode_monitoring_available() const {
    return collector_->is_inode_monitoring_available();
}

metric inode_collector::create_metric(const std::string& name, double value,
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
    m.tags["collector"] = "inode_collector";
    return m;
}

void inode_collector::add_inode_metrics(std::vector<metric>& metrics, const inode_metrics& inode_data) {
    if (!inode_data.metrics_available) {
        return;
    }

    // Aggregate metrics
    metrics.push_back(create_metric("inodes_total", static_cast<double>(inode_data.total_inodes), {}, "count"));
    metrics.push_back(create_metric("inodes_used", static_cast<double>(inode_data.total_inodes_used), {}, "count"));
    metrics.push_back(create_metric("inodes_free", static_cast<double>(inode_data.total_inodes_free), {}, "count"));
    metrics.push_back(create_metric("inodes_average_usage_percent", inode_data.average_usage_percent, {}, "percent"));
    metrics.push_back(create_metric("inodes_max_usage_percent", inode_data.max_usage_percent,
                                     {{"mount_point", inode_data.max_usage_mount_point}}, "percent"));
    metrics.push_back(create_metric("inodes_filesystem_count", 
                                     static_cast<double>(inode_data.filesystems.size()), {}, "count"));

    // Per-filesystem metrics
    for (const auto& fs : inode_data.filesystems) {
        std::unordered_map<std::string, std::string> tags = {
            {"mount_point", fs.mount_point},
            {"filesystem_type", fs.filesystem_type},
            {"device", fs.device}
        };

        metrics.push_back(create_metric("inodes_usage_percent", fs.inodes_usage_percent, tags, "percent"));
        metrics.push_back(create_metric("inodes_total", static_cast<double>(fs.inodes_total), tags, "count"));
        metrics.push_back(create_metric("inodes_used", static_cast<double>(fs.inodes_used), tags, "count"));
        metrics.push_back(create_metric("inodes_free", static_cast<double>(fs.inodes_free), tags, "count"));
    }
}

}  // namespace monitoring
}  // namespace kcenon
