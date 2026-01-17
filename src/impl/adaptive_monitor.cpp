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

/**
 * @file adaptive_monitor.cpp
 * @brief Adaptive monitoring implementation
 */

#include "adaptive_monitor.h"
#include <random>
#include <algorithm>
#include <shared_mutex>

namespace kcenon { namespace monitoring {

// Adaptive Monitor Implementation
struct adaptive_monitor::monitor_impl {
    struct collector_info {
        std::shared_ptr<adaptive_collector> collector;
        int priority{0};
        std::chrono::system_clock::time_point last_collection;
    };
    
    std::unordered_map<std::string, collector_info> collectors;
    mutable std::shared_mutex collectors_mutex;
    
    std::unique_ptr<kcenon::monitoring::system_monitor> sys_monitor;
    std::atomic<bool> running{false};
    std::thread adaptation_thread;
    
    adaptation_strategy global_strategy{adaptation_strategy::balanced};
    std::chrono::seconds adaptation_interval{5};
    
    ~monitor_impl() {
        stop();
    }
    
    void stop() {
        if (running.exchange(false)) {
            if (adaptation_thread.joinable()) {
                adaptation_thread.join();
            }
        }
    }
    
    void adaptation_loop() {
        while (running) {
            // Get current system metrics
            if (sys_monitor) {
                auto metrics_result = sys_monitor->get_current_metrics();
                if (metrics_result.is_ok()) {
                    const auto& sys_metrics = metrics_result.value();
                    
                    // Adapt all collectors
                    std::shared_lock lock(collectors_mutex);
                    for (auto& [name, info] : collectors) {
                        if (info.collector) {
                            info.collector->adapt(sys_metrics);
                        }
                    }
                }
            }
            
            // Sleep for adaptation interval
            std::this_thread::sleep_for(adaptation_interval);
        }
    }
    
    std::vector<std::string> get_collectors_by_priority() const {
        std::vector<std::pair<std::string, int>> collector_priorities;
        
        std::shared_lock lock(collectors_mutex);
        for (const auto& [name, info] : collectors) {
            collector_priorities.emplace_back(name, info.priority);
        }
        
        // Sort by priority (higher first)
        std::sort(collector_priorities.begin(), collector_priorities.end(),
                 [](const auto& a, const auto& b) {
                     return a.second > b.second;
                 });
        
        std::vector<std::string> result;
        for (const auto& [name, priority] : collector_priorities) {
            result.push_back(name);
        }
        
        return result;
    }
};

adaptive_monitor::adaptive_monitor()
    : impl_(std::make_unique<monitor_impl>()) {
    impl_->sys_monitor = std::make_unique<kcenon::monitoring::system_monitor>();
}

adaptive_monitor::~adaptive_monitor() = default;

adaptive_monitor::adaptive_monitor(adaptive_monitor&&) noexcept = default;
adaptive_monitor& adaptive_monitor::operator=(adaptive_monitor&&) noexcept = default;

common::Result<bool> adaptive_monitor::register_collector(
    const std::string& name,
    std::shared_ptr<metrics_collector> collector,
    const adaptive_config& config) {
    
    if (!collector) {
        return common::Result<bool>::err(error_info(kcenon::monitoring::monitoring_error_code::invalid_argument, "Collector cannot be null").to_common_error());
    }
    
    std::unique_lock lock(impl_->collectors_mutex);
    
    if (impl_->collectors.find(name) != impl_->collectors.end()) {
        error_info err(kcenon::monitoring::monitoring_error_code::already_exists,
                      "Collector already registered: " + name);
        return common::Result<bool>::err(err.to_common_error());
    }
    
    monitor_impl::collector_info info;
    info.collector = std::make_shared<adaptive_collector>(collector, config);
    info.priority = 0;
    info.last_collection = std::chrono::system_clock::now();
    
    impl_->collectors[name] = info;

    return common::ok(true);
}

common::Result<bool> adaptive_monitor::unregister_collector(
    const std::string& name) {
    
    std::unique_lock lock(impl_->collectors_mutex);
    
    auto it = impl_->collectors.find(name);
    if (it == impl_->collectors.end()) {
        error_info err(kcenon::monitoring::monitoring_error_code::not_found,
                      "Collector not found: " + name);
        return common::Result<bool>::err(err.to_common_error());
    }

    impl_->collectors.erase(it);

    return common::ok(true);
}

common::Result<bool> adaptive_monitor::start() {
    if (!impl_) {
        return common::Result<bool>::err(error_info(kcenon::monitoring::monitoring_error_code::invalid_state, "Adaptive monitor not initialized").to_common_error());
    }
    
    if (impl_->running.exchange(true)) {
        // Already running
        return common::ok(true);
    }

    // Start system monitoring
    auto sys_result = impl_->sys_monitor->start_monitoring();
    if (sys_result.is_err()) {
        impl_->running = false;
        return common::Result<bool>::err(sys_result.error());
    }
    
    // Start adaptation thread
    impl_->adaptation_thread = std::thread(
        &monitor_impl::adaptation_loop, impl_.get()
    );

    return common::ok(true);
}

common::Result<bool> adaptive_monitor::stop() {
    if (!impl_) {
        return common::Result<bool>::err(error_info(kcenon::monitoring::monitoring_error_code::invalid_state, "Adaptive monitor not initialized").to_common_error());
    }
    
    impl_->stop();

    if (impl_->sys_monitor) {
        impl_->sys_monitor->stop_monitoring();
    }

    return common::ok(true);
}

bool adaptive_monitor::is_running() const {
    return impl_ && impl_->running.load();
}

common::Result<adaptation_stats> adaptive_monitor::get_collector_stats(
    const std::string& name) const {
    
    std::shared_lock lock(impl_->collectors_mutex);
    
    auto it = impl_->collectors.find(name);
    if (it == impl_->collectors.end()) {
        error_info err(kcenon::monitoring::monitoring_error_code::not_found,
                      "Collector not found: " + name);
        return common::Result<adaptation_stats>::err(err.to_common_error());
    }
    
    if (!it->second.collector) {
        return common::Result<adaptation_stats>::err(error_info(kcenon::monitoring::monitoring_error_code::invalid_state, "Collector is null").to_common_error());
    }
    
    return it->second.collector->get_stats();
}

std::unordered_map<std::string, adaptation_stats> adaptive_monitor::get_all_stats() const {
    std::unordered_map<std::string, adaptation_stats> all_stats;
    
    std::shared_lock lock(impl_->collectors_mutex);
    
    for (const auto& [name, info] : impl_->collectors) {
        if (info.collector) {
            all_stats[name] = info.collector->get_stats();
        }
    }
    
    return all_stats;
}

void adaptive_monitor::set_global_strategy(adaptation_strategy strategy) {
    impl_->global_strategy = strategy;
    
    // Update all collectors
    std::shared_lock lock(impl_->collectors_mutex);
    for (auto& [name, info] : impl_->collectors) {
        if (info.collector) {
            auto config = info.collector->get_config();
            config.strategy = strategy;
            info.collector->set_config(config);
        }
    }
}

common::Result<bool> adaptive_monitor::force_adaptation() {
    if (!impl_->sys_monitor) {
        return common::Result<bool>::err(error_info(kcenon::monitoring::monitoring_error_code::invalid_state, "System monitor not initialized").to_common_error());
    }
    
    auto metrics_result = impl_->sys_monitor->get_current_metrics();
    if (metrics_result.is_err()) {
        return common::Result<bool>::err(metrics_result.error());
    }
    
    const auto& sys_metrics = metrics_result.value();
    
    std::shared_lock lock(impl_->collectors_mutex);
    for (auto& [name, info] : impl_->collectors) {
        if (info.collector) {
            info.collector->adapt(sys_metrics);
        }
    }

    return common::ok(true);
}

std::vector<std::string> adaptive_monitor::get_active_collectors() const {
    if (!impl_) {
        return {};
    }
    
    std::vector<std::string> active;
    
    std::shared_lock lock(impl_->collectors_mutex);
    
    // Get collectors sorted by priority
    auto sorted_collectors = impl_->get_collectors_by_priority();
    
    // Determine how many collectors to keep active based on load
    double avg_cpu = 0.0;
    size_t active_count = sorted_collectors.size();
    
    // Calculate average CPU usage across all collectors
    for (const auto& [name, info] : impl_->collectors) {
        if (info.collector) {
            auto stats = info.collector->get_stats();
            avg_cpu += stats.average_cpu_usage;
        }
    }
    
    if (!impl_->collectors.empty()) {
        avg_cpu /= impl_->collectors.size();
    }
    
    // Determine how many collectors to keep active
    if (avg_cpu > 80.0) {
        active_count = std::max(size_t(1), static_cast<size_t>(sorted_collectors.size() * 0.2));
    } else if (avg_cpu > 60.0) {
        active_count = std::max(size_t(2), static_cast<size_t>(sorted_collectors.size() * 0.5));
    } else if (avg_cpu > 40.0) {
        active_count = std::max(size_t(3), static_cast<size_t>(sorted_collectors.size() * 0.75));
    }
    
    // Return top priority collectors up to active_count
    for (size_t i = 0; i < std::min(active_count, sorted_collectors.size()); ++i) {
        active.push_back(sorted_collectors[i]);
    }
    
    return active;
}

common::Result<bool> adaptive_monitor::set_collector_priority(
    const std::string& name,
    int priority) {
    
    std::unique_lock lock(impl_->collectors_mutex);
    
    auto it = impl_->collectors.find(name);
    if (it == impl_->collectors.end()) {
        error_info err(kcenon::monitoring::monitoring_error_code::not_found,
                      "Collector not found: " + name);
        return common::Result<bool>::err(err.to_common_error());
    }

    it->second.priority = priority;

    return common::ok(true);
}

// Global instance
adaptive_monitor& global_adaptive_monitor() {
    static adaptive_monitor instance;
    return instance;
}

} } // namespace kcenon::monitoring