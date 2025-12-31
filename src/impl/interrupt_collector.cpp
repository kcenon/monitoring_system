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

#include <kcenon/monitoring/collectors/interrupt_collector.h>
#include <kcenon/monitoring/platform/metrics_provider.h>

namespace kcenon {
namespace monitoring {

// ============================================================================
// interrupt_info_collector implementation
// ============================================================================

interrupt_info_collector::interrupt_info_collector()
    : provider_(platform::metrics_provider::create()) {}

interrupt_info_collector::~interrupt_info_collector() = default;

bool interrupt_info_collector::is_interrupt_monitoring_available() const {
    if (!provider_) {
        return false;
    }
    auto stats = provider_->get_interrupt_stats();
    return !stats.empty();
}

interrupt_metrics interrupt_info_collector::collect_metrics() {
    interrupt_metrics result;
    result.timestamp = std::chrono::system_clock::now();

    if (!provider_) {
        return result;
    }

    auto stats = provider_->get_interrupt_stats();
    if (stats.empty()) {
        return result;
    }

    // Sum up all interrupt counts
    uint64_t total_interrupts = 0;
    for (const auto& info : stats) {
        total_interrupts += info.count;
    }

    result.interrupts_total = total_interrupts;
    result.soft_interrupts_total = 0;  // Not available from this interface
    result.metrics_available = true;
    result.soft_interrupts_available = false;

    // Calculate rates
    auto now = std::chrono::system_clock::now();
    if (has_previous_sample_) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - prev_timestamp_).count();
        if (elapsed > 0) {
            result.interrupts_per_sec = static_cast<double>(
                total_interrupts - prev_interrupts_total_) /
                (static_cast<double>(elapsed) / 1000.0);
        }
    }

    prev_interrupts_total_ = total_interrupts;
    prev_timestamp_ = now;
    has_previous_sample_ = true;

    return result;
}

// ============================================================================
// interrupt_collector implementation (common across all platforms)
// ============================================================================

interrupt_collector::interrupt_collector() : collector_(std::make_unique<interrupt_info_collector>()) {}

bool interrupt_collector::do_initialize(const config_map& config) {
    if (auto it = config.find("collect_per_cpu"); it != config.end()) {
        collect_per_cpu_ = (it->second == "true" || it->second == "1");
    }

    if (auto it = config.find("collect_soft_interrupts"); it != config.end()) {
        collect_soft_interrupts_ = (it->second == "true" || it->second == "1");
    }

    return true;
}

std::vector<metric> interrupt_collector::do_collect() {
    std::vector<metric> metrics;

    auto interrupt_data = collector_->collect_metrics();

    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        last_metrics_ = interrupt_data;
    }

    add_interrupt_metrics(metrics, interrupt_data);

    return metrics;
}

std::vector<std::string> interrupt_collector::do_get_metric_types() const {
    return {
        "interrupts_total",
        "interrupts_per_sec",
        "soft_interrupts_total",
        "soft_interrupts_per_sec"
    };
}

bool interrupt_collector::is_available() const {
    return collector_->is_interrupt_monitoring_available();
}

void interrupt_collector::do_add_statistics(stats_map& stats) const {
    stats["collect_per_cpu"] = collect_per_cpu_ ? 1.0 : 0.0;
    stats["collect_soft_interrupts"] = collect_soft_interrupts_ ? 1.0 : 0.0;
}

interrupt_metrics interrupt_collector::get_last_metrics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return last_metrics_;
}

bool interrupt_collector::is_interrupt_monitoring_available() const {
    return collector_->is_interrupt_monitoring_available();
}

void interrupt_collector::add_interrupt_metrics(std::vector<metric>& metrics, const interrupt_metrics& data) {
    if (!data.metrics_available) {
        return;
    }

    // Hardware interrupt metrics
    metrics.push_back(create_base_metric("interrupts_total", static_cast<double>(data.interrupts_total), {}, "count"));
    metrics.push_back(create_base_metric("interrupts_per_sec", data.interrupts_per_sec, {}, "count/sec"));

    // Soft interrupt metrics (when available and configured)
    if (collect_soft_interrupts_ && data.soft_interrupts_available) {
        metrics.push_back(create_base_metric("soft_interrupts_total",
                                         static_cast<double>(data.soft_interrupts_total), {}, "count"));
        metrics.push_back(create_base_metric("soft_interrupts_per_sec",
                                         data.soft_interrupts_per_sec, {}, "count/sec"));
    }

    // Per-CPU metrics (when available and configured)
    if (collect_per_cpu_ && !data.per_cpu.empty()) {
        for (const auto& cpu : data.per_cpu) {
            std::unordered_map<std::string, std::string> cpu_tags = {
                {"cpu", std::to_string(cpu.cpu_id)}
            };
            metrics.push_back(create_base_metric("interrupts_total",
                                             static_cast<double>(cpu.interrupt_count), cpu_tags, "count"));
            metrics.push_back(create_base_metric("interrupts_per_sec",
                                             cpu.interrupts_per_sec, cpu_tags, "count/sec"));
        }
    }
}

}  // namespace monitoring
}  // namespace kcenon
