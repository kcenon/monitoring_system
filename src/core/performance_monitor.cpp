/**
 * @file performance_monitor.cpp
 * @brief Performance monitoring implementation
 */

#include <kcenon/monitoring/core/performance_monitor.h>
#include <shared_mutex>

namespace kcenon::monitoring {

using namespace monitoring_system;

monitoring_system::result<bool> monitoring_system::performance_profiler::record_sample(
    const std::string& operation_name,
    std::chrono::nanoseconds duration,
    bool success) {

    if (!enabled_) {
        return monitoring_system::result<bool>(true);
    }

    std::unique_lock<std::shared_mutex> lock(profiles_mutex_);

    auto& profile = profiles_[operation_name];
    if (!profile) {
        profile = std::make_unique<profile_data>();
    }

    lock.unlock();

    // Update counters
    profile->call_count.fetch_add(1);
    if (!success) {
        profile->error_count.fetch_add(1);
    }

    // Record sample
    std::lock_guard sample_lock(profile->mutex);

    // Limit samples to prevent unbounded growth
    if (profile->samples.size() >= max_samples_per_operation_) {
        // Remove oldest sample (simple ring buffer behavior)
        profile->samples.erase(profile->samples.begin());
    }

    profile->samples.push_back(duration);

    return monitoring_system::result<bool>(true);
}

monitoring_system::result<monitoring_system::performance_metrics> monitoring_system::performance_profiler::get_metrics(
    const std::string& operation_name) const {

    std::shared_lock<std::shared_mutex> lock(profiles_mutex_);

    auto it = profiles_.find(operation_name);
    if (it == profiles_.end()) {
        return monitoring_system::make_error<monitoring_system::performance_metrics>(
            monitoring_system::monitoring_error_code::not_found,
            "Operation not found: " + operation_name
        );
    }

    const auto& profile = it->second;

    std::lock_guard sample_lock(profile->mutex);

    monitoring_system::performance_metrics metrics;
    metrics.operation_name = operation_name;
    metrics.call_count = profile->call_count.load();
    metrics.error_count = profile->error_count.load();

    if (!profile->samples.empty()) {
        auto total = std::chrono::nanoseconds::zero();
        auto min_sample = profile->samples[0];
        auto max_sample = profile->samples[0];

        for (const auto& sample : profile->samples) {
            total += sample;
            if (sample < min_sample) min_sample = sample;
            if (sample > max_sample) max_sample = sample;
        }

        metrics.min_duration = min_sample;
        metrics.max_duration = max_sample;
        metrics.avg_duration = total / profile->samples.size();
        metrics.sample_count = profile->samples.size();
    }

    return monitoring_system::result<monitoring_system::performance_metrics>(metrics);
}

// Global instance
monitoring_system::performance_monitor& global_performance_monitor() {
    static monitoring_system::performance_monitor instance;
    return instance;
}

} // namespace kcenon::monitoring