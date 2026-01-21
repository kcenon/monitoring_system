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

#include <kcenon/monitoring/collectors/platform_metrics_collector.h>
#include <kcenon/monitoring/platform/metrics_provider.h>

namespace kcenon {
namespace monitoring {

// ============================================================================
// platform_info_collector implementation
// ============================================================================

platform_info_collector::platform_info_collector()
    : provider_(platform::metrics_provider::create()) {}

platform_info_collector::~platform_info_collector() = default;

bool platform_info_collector::is_platform_available() const {
    return provider_ != nullptr;
}

platform_info platform_info_collector::get_platform_info() const {
    platform_info result;

    if (!provider_) {
        result.name = "unknown";
        return result;
    }

    result.name = provider_->get_platform_name();
    result.available = true;

    return result;
}

platform_uptime platform_info_collector::get_uptime() const {
    platform_uptime result;

    if (!provider_) {
        return result;
    }

    auto uptime = provider_->get_uptime();
    if (!uptime.available) {
        return result;
    }

    result.uptime_seconds = uptime.uptime_seconds;
    result.idle_seconds = uptime.idle_seconds;
    result.boot_timestamp = std::chrono::system_clock::to_time_t(uptime.boot_time);
    result.available = true;

    return result;
}

platform_context_switches platform_info_collector::get_context_switches() const {
    platform_context_switches result;

    if (!provider_) {
        return result;
    }

    auto ctx = provider_->get_context_switches();
    if (!ctx.available) {
        return result;
    }

    result.total_switches = ctx.total_switches;
    result.voluntary_switches = ctx.voluntary_switches;
    result.involuntary_switches = ctx.involuntary_switches;
    result.switches_per_second = ctx.switches_per_second;
    result.available = true;

    return result;
}

platform_tcp_info platform_info_collector::get_tcp_states() const {
    platform_tcp_info result;

    if (!provider_) {
        return result;
    }

    auto tcp = provider_->get_tcp_states();
    if (!tcp.available) {
        return result;
    }

    result.established = tcp.established;
    result.syn_sent = tcp.syn_sent;
    result.syn_recv = tcp.syn_recv;
    result.fin_wait1 = tcp.fin_wait1;
    result.fin_wait2 = tcp.fin_wait2;
    result.time_wait = tcp.time_wait;
    result.close_wait = tcp.close_wait;
    result.listen = tcp.listen;
    result.total = tcp.total;
    result.available = true;

    return result;
}

platform_socket_info platform_info_collector::get_socket_buffers() const {
    platform_socket_info result;

    if (!provider_) {
        return result;
    }

    auto socket = provider_->get_socket_buffer_stats();
    if (!socket.available) {
        return result;
    }

    result.rx_buffer_size = socket.rx_buffer_size;
    result.tx_buffer_size = socket.tx_buffer_size;
    result.rx_buffer_used = socket.rx_buffer_used;
    result.tx_buffer_used = socket.tx_buffer_used;
    result.available = true;

    return result;
}

platform_interrupt_info platform_info_collector::get_interrupt_stats() const {
    platform_interrupt_info result;

    if (!provider_) {
        return result;
    }

    auto interrupts = provider_->get_interrupt_stats();
    for (const auto& irq : interrupts) {
        if (irq.available && irq.name == "total_interrupts") {
            result.total_interrupts = irq.count;
            result.available = true;
            break;
        }
    }

    return result;
}

// ============================================================================
// platform_metrics_collector implementation
// ============================================================================

platform_metrics_collector::platform_metrics_collector()
    : collector_(std::make_unique<platform_info_collector>()) {}

platform_metrics_collector::platform_metrics_collector(platform_metrics_config config)
    : collector_(std::make_unique<platform_info_collector>()),
      config_(config) {}

bool platform_metrics_collector::do_initialize(const config_map& config) {
    if (auto it = config.find("collect_uptime"); it != config.end()) {
        config_.collect_uptime = (it->second == "true" || it->second == "1");
    }
    if (auto it = config.find("collect_context_switches"); it != config.end()) {
        config_.collect_context_switches = (it->second == "true" || it->second == "1");
    }
    if (auto it = config.find("collect_tcp_states"); it != config.end()) {
        config_.collect_tcp_states = (it->second == "true" || it->second == "1");
    }
    if (auto it = config.find("collect_socket_buffers"); it != config.end()) {
        config_.collect_socket_buffers = (it->second == "true" || it->second == "1");
    }
    if (auto it = config.find("collect_interrupts"); it != config.end()) {
        config_.collect_interrupts = (it->second == "true" || it->second == "1");
    }

    return true;
}

std::vector<std::string> platform_metrics_collector::do_get_metric_types() const {
    std::vector<std::string> types;
    types.push_back("platform_info");

    if (config_.collect_uptime) {
        types.push_back("platform_uptime_seconds");
        types.push_back("platform_boot_timestamp");
    }
    if (config_.collect_context_switches) {
        types.push_back("platform_context_switches_total");
    }
    if (config_.collect_tcp_states) {
        types.push_back("platform_tcp_established");
        types.push_back("platform_tcp_time_wait");
        types.push_back("platform_tcp_close_wait");
    }
    if (config_.collect_socket_buffers) {
        types.push_back("platform_socket_rx_buffer_used");
        types.push_back("platform_socket_tx_buffer_used");
    }
    if (config_.collect_interrupts) {
        types.push_back("platform_interrupts_total");
    }

    return types;
}

bool platform_metrics_collector::is_available() const {
    return collector_->is_platform_available();
}

bool platform_metrics_collector::is_platform_available() const {
    return collector_->is_platform_available();
}

void platform_metrics_collector::do_add_statistics(stats_map& stats) const {
    stats["collect_uptime"] = config_.collect_uptime ? 1.0 : 0.0;
    stats["collect_context_switches"] = config_.collect_context_switches ? 1.0 : 0.0;
    stats["collect_tcp_states"] = config_.collect_tcp_states ? 1.0 : 0.0;
    stats["collect_socket_buffers"] = config_.collect_socket_buffers ? 1.0 : 0.0;
    stats["collect_interrupts"] = config_.collect_interrupts ? 1.0 : 0.0;
}

platform_metrics platform_metrics_collector::get_last_metrics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return last_metrics_;
}

platform_info platform_metrics_collector::get_platform_info() const {
    if (!platform_info_cached_) {
        return collector_->get_platform_info();
    }
    return cached_platform_info_;
}

std::string platform_metrics_collector::get_platform_name() const {
    if (platform_info_cached_) {
        return cached_platform_info_.name;
    }
    auto info = collector_->get_platform_info();
    return info.name;
}

void platform_metrics_collector::collect_platform_info_metrics(std::vector<metric>& metrics) {
    if (!platform_info_cached_) {
        cached_platform_info_ = collector_->get_platform_info();
        platform_info_cached_ = true;
    }

    if (cached_platform_info_.available) {
        metrics.push_back(create_base_metric(
            "platform_info",
            1.0,
            {{"platform", cached_platform_info_.name}},
            "info"
        ));
    }
}

void platform_metrics_collector::collect_uptime_metrics(std::vector<metric>& metrics) {
    if (!config_.collect_uptime) {
        return;
    }

    auto uptime = collector_->get_uptime();
    last_metrics_.uptime = uptime;

    if (!uptime.available) {
        return;
    }

    metrics.push_back(create_base_metric(
        "platform_uptime_seconds",
        static_cast<double>(uptime.uptime_seconds),
        {},
        "seconds"
    ));

    metrics.push_back(create_base_metric(
        "platform_boot_timestamp",
        static_cast<double>(uptime.boot_timestamp),
        {},
        "timestamp"
    ));

    if (uptime.idle_seconds > 0) {
        metrics.push_back(create_base_metric(
            "platform_idle_seconds",
            static_cast<double>(uptime.idle_seconds),
            {},
            "seconds"
        ));
    }
}

void platform_metrics_collector::collect_context_switch_metrics(std::vector<metric>& metrics) {
    if (!config_.collect_context_switches) {
        return;
    }

    auto ctx = collector_->get_context_switches();
    last_metrics_.context_switches = ctx;

    if (!ctx.available) {
        return;
    }

    metrics.push_back(create_base_metric(
        "platform_context_switches_total",
        static_cast<double>(ctx.total_switches),
        {},
        "count"
    ));

    if (ctx.voluntary_switches > 0) {
        metrics.push_back(create_base_metric(
            "platform_context_switches_voluntary",
            static_cast<double>(ctx.voluntary_switches),
            {},
            "count"
        ));
    }

    if (ctx.involuntary_switches > 0) {
        metrics.push_back(create_base_metric(
            "platform_context_switches_involuntary",
            static_cast<double>(ctx.involuntary_switches),
            {},
            "count"
        ));
    }

    if (ctx.switches_per_second > 0.0) {
        metrics.push_back(create_base_metric(
            "platform_context_switches_per_second",
            ctx.switches_per_second,
            {},
            "rate"
        ));
    }
}

void platform_metrics_collector::collect_tcp_metrics(std::vector<metric>& metrics) {
    if (!config_.collect_tcp_states) {
        return;
    }

    auto tcp = collector_->get_tcp_states();
    last_metrics_.tcp = tcp;

    if (!tcp.available) {
        return;
    }

    metrics.push_back(create_base_metric(
        "platform_tcp_established",
        static_cast<double>(tcp.established),
        {},
        "connections"
    ));

    metrics.push_back(create_base_metric(
        "platform_tcp_time_wait",
        static_cast<double>(tcp.time_wait),
        {},
        "connections"
    ));

    metrics.push_back(create_base_metric(
        "platform_tcp_close_wait",
        static_cast<double>(tcp.close_wait),
        {},
        "connections"
    ));

    metrics.push_back(create_base_metric(
        "platform_tcp_listen",
        static_cast<double>(tcp.listen),
        {},
        "connections"
    ));

    metrics.push_back(create_base_metric(
        "platform_tcp_total",
        static_cast<double>(tcp.total),
        {},
        "connections"
    ));
}

void platform_metrics_collector::collect_socket_metrics(std::vector<metric>& metrics) {
    if (!config_.collect_socket_buffers) {
        return;
    }

    auto socket = collector_->get_socket_buffers();
    last_metrics_.socket = socket;

    if (!socket.available) {
        return;
    }

    metrics.push_back(create_base_metric(
        "platform_socket_rx_buffer_size",
        static_cast<double>(socket.rx_buffer_size),
        {},
        "bytes"
    ));

    metrics.push_back(create_base_metric(
        "platform_socket_tx_buffer_size",
        static_cast<double>(socket.tx_buffer_size),
        {},
        "bytes"
    ));

    metrics.push_back(create_base_metric(
        "platform_socket_rx_buffer_used",
        static_cast<double>(socket.rx_buffer_used),
        {},
        "bytes"
    ));

    metrics.push_back(create_base_metric(
        "platform_socket_tx_buffer_used",
        static_cast<double>(socket.tx_buffer_used),
        {},
        "bytes"
    ));
}

void platform_metrics_collector::collect_interrupt_metrics(std::vector<metric>& metrics) {
    if (!config_.collect_interrupts) {
        return;
    }

    auto interrupts = collector_->get_interrupt_stats();
    last_metrics_.interrupts = interrupts;

    if (!interrupts.available) {
        return;
    }

    metrics.push_back(create_base_metric(
        "platform_interrupts_total",
        static_cast<double>(interrupts.total_interrupts),
        {},
        "count"
    ));
}

std::vector<metric> platform_metrics_collector::do_collect() {
    std::vector<metric> metrics;

    last_metrics_.timestamp = std::chrono::system_clock::now();

    collect_platform_info_metrics(metrics);
    collect_uptime_metrics(metrics);
    collect_context_switch_metrics(metrics);
    collect_tcp_metrics(metrics);
    collect_socket_metrics(metrics);
    collect_interrupt_metrics(metrics);

    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        last_metrics_.info = cached_platform_info_;
    }

    return metrics;
}

}  // namespace monitoring
}  // namespace kcenon
