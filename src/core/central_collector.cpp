/**
 * @file central_collector.cpp
 * @brief Central collector implementation for aggregating metrics
 */

#include <kcenon/monitoring/core/central_collector.h>
#include <algorithm>
#include <limits>
#include <mutex>

namespace kcenon { namespace monitoring {

central_collector::central_collector(size_t max_profiles)
    : max_profiles_(max_profiles) {
}

void central_collector::receive_batch(const std::vector<metric_sample>& samples) {
    if (samples.empty()) {
        return;
    }

    batches_received_.fetch_add(1, std::memory_order_relaxed);
    total_samples_.fetch_add(samples.size(), std::memory_order_relaxed);

    // Process all samples in the batch
    for (const auto& sample : samples) {
        process_sample(sample);
    }
}

void central_collector::process_sample(const metric_sample& sample) {
    profile_data* profile = nullptr;

    // Try to find existing profile with shared lock (hot path)
    {
        std::shared_lock<std::shared_mutex> read_lock(profiles_mutex_);
        auto it = profiles_.find(sample.operation_name);
        if (it != profiles_.end()) {
            profile = it->second.get();
        }
    }

    // Create new profile if needed
    if (!profile) {
        std::unique_lock<std::shared_mutex> write_lock(profiles_mutex_);
        // Double-check after acquiring write lock
        auto& profile_ptr = profiles_[sample.operation_name];
        if (!profile_ptr) {
            // Check LRU eviction
            if (profiles_.size() >= max_profiles_) {
                evict_lru();
            }
            profile_ptr = std::make_unique<profile_data>();
        }
        profile = profile_ptr.get();
    }

    // Update last access time
    profile->last_access_time.store(
        std::chrono::steady_clock::now().time_since_epoch().count(),
        std::memory_order_relaxed
    );

    // Aggregate sample into profile (with per-profile lock)
    std::lock_guard<std::mutex> lock(profile->mutex);

    auto& p = profile->profile;
    p.total_calls++;
    if (!sample.success) {
        p.error_count++;
    }

    // Update timing statistics
    auto duration_ns = sample.duration.count();
    p.total_duration_ns += duration_ns;
    p.min_duration_ns = std::min(p.min_duration_ns, duration_ns);
    p.max_duration_ns = std::max(p.max_duration_ns, duration_ns);

    // Update average
    if (p.total_calls > 0) {
        p.avg_duration_ns = p.total_duration_ns / p.total_calls;
    }
}

void central_collector::evict_lru() {
    // Find least recently used profile
    auto lru_it = profiles_.end();
    auto oldest_time = (std::numeric_limits<std::chrono::steady_clock::rep>::max)();

    for (auto it = profiles_.begin(); it != profiles_.end(); ++it) {
        if (it->second) {
            auto access_time = it->second->last_access_time.load(std::memory_order_relaxed);
            if (access_time < oldest_time) {
                oldest_time = access_time;
                lru_it = it;
            }
        }
    }

    if (lru_it != profiles_.end()) {
        profiles_.erase(lru_it);
        lru_evictions_.fetch_add(1, std::memory_order_relaxed);
    }
}

result<performance_profile> central_collector::get_profile(const std::string& operation_name) const {
    std::shared_lock<std::shared_mutex> lock(profiles_mutex_);

    auto it = profiles_.find(operation_name);
    if (it == profiles_.end()) {
        return make_error<performance_profile>(
            monitoring_error_code::metric_not_found,
            "Operation profile not found: " + operation_name
        );
    }

    // Lock the profile while copying
    std::lock_guard<std::mutex> profile_lock(it->second->mutex);
    return result<performance_profile>(it->second->profile);
}

std::unordered_map<std::string, performance_profile> central_collector::get_all_profiles() const {
    std::shared_lock<std::shared_mutex> lock(profiles_mutex_);

    std::unordered_map<std::string, performance_profile> result;
    for (const auto& [name, data] : profiles_) {
        if (data) {
            std::lock_guard<std::mutex> profile_lock(data->mutex);
            result[name] = data->profile;
        }
    }
    return result;
}

void central_collector::clear() {
    std::unique_lock<std::shared_mutex> lock(profiles_mutex_);
    profiles_.clear();
    total_samples_.store(0, std::memory_order_relaxed);
    batches_received_.store(0, std::memory_order_relaxed);
    lru_evictions_.store(0, std::memory_order_relaxed);
}

size_t central_collector::get_operation_count() const {
    std::shared_lock<std::shared_mutex> lock(profiles_mutex_);
    return profiles_.size();
}

central_collector::stats central_collector::get_stats() const {
    return stats{
        .operation_count = get_operation_count(),
        .total_samples = total_samples_.load(std::memory_order_relaxed),
        .batches_received = batches_received_.load(std::memory_order_relaxed),
        .lru_evictions = lru_evictions_.load(std::memory_order_relaxed)
    };
}

}} // namespace kcenon::monitoring
