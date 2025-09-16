/**
 * @file performance_monitor.cpp
 * @brief Performance monitor implementation
 */

#include "../../include/kcenon/monitoring/core/performance_monitor.h"
#include <iostream>

namespace monitoring_system {

// system_monitor implementation
struct system_monitor::monitor_impl {
    std::atomic<bool> monitoring{false};
    std::thread monitor_thread;
    std::vector<system_metrics> history;
    mutable std::mutex history_mutex;
    std::chrono::milliseconds interval{1000};

    ~monitor_impl() {
        if (monitoring) {
            monitoring = false;
            if (monitor_thread.joinable()) {
                monitor_thread.join();
            }
        }
    }
};

system_monitor::system_monitor() : impl_(std::make_unique<monitor_impl>()) {}

system_monitor::~system_monitor() = default;

system_monitor::system_monitor(system_monitor&&) noexcept = default;
system_monitor& system_monitor::operator=(system_monitor&&) noexcept = default;

result<system_metrics> system_monitor::get_current_metrics() const {
    system_metrics metrics;
    metrics.timestamp = std::chrono::system_clock::now();

    // Stub implementation - return dummy values
    metrics.cpu_usage_percent = 10.0;
    metrics.memory_usage_percent = 25.0;
    metrics.memory_usage_bytes = 1024 * 1024 * 100; // 100MB
    metrics.available_memory_bytes = 1024 * 1024 * 500; // 500MB
    metrics.thread_count = 10;
    metrics.handle_count = 50;

    return make_success(metrics);
}

result<bool> system_monitor::start_monitoring(std::chrono::milliseconds interval) {
    if (impl_->monitoring) {
        return make_success(true);
    }

    impl_->interval = interval;
    impl_->monitoring = true;

    // Start monitoring thread (stub implementation)
    impl_->monitor_thread = std::thread([this]() {
        while (impl_->monitoring) {
            auto metrics_result = get_current_metrics();
            if (metrics_result) {
                std::lock_guard<std::mutex> lock(impl_->history_mutex);
                impl_->history.push_back(metrics_result.value());

                // Keep only last 100 entries
                if (impl_->history.size() > 100) {
                    impl_->history.erase(impl_->history.begin());
                }
            }

            std::this_thread::sleep_for(impl_->interval);
        }
    });

    return make_success(true);
}

result<bool> system_monitor::stop_monitoring() {
    if (!impl_->monitoring) {
        return make_success(true);
    }

    impl_->monitoring = false;
    if (impl_->monitor_thread.joinable()) {
        impl_->monitor_thread.join();
    }

    return make_success(true);
}

bool system_monitor::is_monitoring() const {
    return impl_->monitoring;
}

std::vector<system_metrics> system_monitor::get_history(std::chrono::seconds duration) const {
    std::lock_guard<std::mutex> lock(impl_->history_mutex);

    auto cutoff = std::chrono::system_clock::now() - duration;
    std::vector<system_metrics> result;

    for (const auto& metrics : impl_->history) {
        if (metrics.timestamp >= cutoff) {
            result.push_back(metrics);
        }
    }

    return result;
}

// performance_profiler implementation
result<bool> performance_profiler::record_sample(
    const std::string& operation_name,
    std::chrono::nanoseconds duration,
    bool success) {

    if (!enabled_) {
        return make_success(true);
    }

    std::unique_lock<std::shared_mutex> lock(profiles_mutex_);

    auto& profile = profiles_[operation_name];
    if (!profile) {
        profile = std::make_unique<profile_data>();
    }

    lock.unlock();

    std::lock_guard<std::mutex> data_lock(profile->mutex);

    // Keep sample count under limit
    if (profile->samples.size() >= max_samples_per_operation_) {
        profile->samples.erase(profile->samples.begin());
    }

    profile->samples.push_back(duration);
    profile->call_count++;

    if (!success) {
        profile->error_count++;
    }

    return make_success(true);
}

result<performance_metrics> performance_profiler::get_metrics(
    const std::string& operation_name) const {

    std::shared_lock<std::shared_mutex> lock(profiles_mutex_);

    auto it = profiles_.find(operation_name);
    if (it == profiles_.end()) {
        return make_error<performance_metrics>(
            monitoring_error_code::metric_not_found,
            "Operation not found: " + operation_name
        );
    }

    std::lock_guard<std::mutex> data_lock(it->second->mutex);

    performance_metrics metrics;
    metrics.operation_name = operation_name;
    metrics.call_count = it->second->call_count;
    metrics.error_count = it->second->error_count;

    if (!it->second->samples.empty()) {
        metrics.update_statistics(it->second->samples);

        // Calculate throughput (operations per second)
        if (metrics.total_duration.count() > 0) {
            metrics.throughput = static_cast<double>(metrics.call_count) /
                               (static_cast<double>(metrics.total_duration.count()) / 1e9);
        }
    }

    return make_success(metrics);
}

std::vector<performance_metrics> performance_profiler::get_all_metrics() const {
    std::vector<performance_metrics> result;

    std::shared_lock<std::shared_mutex> lock(profiles_mutex_);

    for (const auto& [name, profile] : profiles_) {
        auto metrics_result = get_metrics(name);
        if (metrics_result) {
            result.push_back(metrics_result.value());
        }
    }

    return result;
}

result<bool> performance_profiler::clear_samples(const std::string& operation_name) {
    std::unique_lock<std::shared_mutex> lock(profiles_mutex_);

    auto it = profiles_.find(operation_name);
    if (it != profiles_.end()) {
        std::lock_guard<std::mutex> data_lock(it->second->mutex);
        it->second->samples.clear();
        it->second->call_count = 0;
        it->second->error_count = 0;
    }

    return make_success(true);
}

void performance_profiler::clear_all_samples() {
    std::unique_lock<std::shared_mutex> lock(profiles_mutex_);

    for (auto& [name, profile] : profiles_) {
        std::lock_guard<std::mutex> data_lock(profile->mutex);
        profile->samples.clear();
        profile->call_count = 0;
        profile->error_count = 0;
    }
}

// performance_monitor implementation
result<metrics_snapshot> performance_monitor::collect() {
    metrics_snapshot snapshot;
    snapshot.timestamp = std::chrono::system_clock::now();
    snapshot.source = name_;

    // Add system metrics
    auto system_metrics_result = system_monitor_.get_current_metrics();
    if (system_metrics_result) {
        auto& sys_metrics = system_metrics_result.value();

        // Convert system metrics to metric_value format
        snapshot.metrics["cpu_usage"] = metric_value{sys_metrics.cpu_usage_percent, "percent"};
        snapshot.metrics["memory_usage"] = metric_value{sys_metrics.memory_usage_percent, "percent"};
        snapshot.metrics["memory_bytes"] = metric_value{static_cast<double>(sys_metrics.memory_usage_bytes), "bytes"};
        snapshot.metrics["thread_count"] = metric_value{static_cast<double>(sys_metrics.thread_count), "count"};
    }

    // Add performance metrics
    auto perf_metrics = profiler_.get_all_metrics();
    for (const auto& metrics : perf_metrics) {
        std::string prefix = "perf_" + metrics.operation_name + "_";

        snapshot.metrics[prefix + "call_count"] = metric_value{
            static_cast<double>(metrics.call_count), "count"
        };
        snapshot.metrics[prefix + "error_count"] = metric_value{
            static_cast<double>(metrics.error_count), "count"
        };
        snapshot.metrics[prefix + "mean_duration_ms"] = metric_value{
            static_cast<double>(metrics.mean_duration.count()) / 1e6, "ms"
        };
        snapshot.metrics[prefix + "throughput"] = metric_value{
            metrics.throughput, "ops/sec"
        };
    }

    return make_success(snapshot);
}

result<bool> performance_monitor::check_thresholds() const {
    auto metrics_result = system_monitor_.get_current_metrics();
    if (!metrics_result) {
        return make_error<bool>(
            metrics_result.get_error().code,
            "Failed to get system metrics: " + metrics_result.get_error().message
        );
    }

    const auto& metrics = metrics_result.value();

    // Check CPU threshold
    if (metrics.cpu_usage_percent > thresholds_.cpu_threshold) {
        return make_error<bool>(
            monitoring_error_code::resource_exhausted,
            "CPU usage " + std::to_string(metrics.cpu_usage_percent) +
            "% exceeds threshold " + std::to_string(thresholds_.cpu_threshold) + "%"
        );
    }

    // Check memory threshold
    if (metrics.memory_usage_percent > thresholds_.memory_threshold) {
        return make_error<bool>(
            monitoring_error_code::resource_exhausted,
            "Memory usage " + std::to_string(metrics.memory_usage_percent) +
            "% exceeds threshold " + std::to_string(thresholds_.memory_threshold) + "%"
        );
    }

    return make_success(true);
}

// Global performance monitor
performance_monitor& global_performance_monitor() {
    static performance_monitor instance("global_performance_monitor");
    return instance;
}

} // namespace monitoring_system