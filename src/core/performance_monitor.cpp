/**
 * @file performance_monitor.cpp
 * @brief Performance monitoring implementation
 */

#include <kcenon/monitoring/core/performance_monitor.h>
#include <shared_mutex>

namespace kcenon { namespace monitoring {

result<bool> performance_profiler::record_sample(
    const std::string& operation_name,
    std::chrono::nanoseconds duration,
    bool success) {

    if (!enabled_) {
        return result<bool>(true);
    }

    profile_data* profile = nullptr;

    // First, try read lock (hot path optimization)
    {
        std::shared_lock<std::shared_mutex> read_lock(profiles_mutex_);
        auto it = profiles_.find(operation_name);
        if (it != profiles_.end()) {
            profile = it->second.get();
        }
    }

    // If not found, acquire write lock to create
    if (!profile) {
        std::unique_lock<std::shared_mutex> write_lock(profiles_mutex_);
        // Double-check after acquiring write lock
        auto& profile_ptr = profiles_[operation_name];
        if (!profile_ptr) {
            profile_ptr = std::make_unique<profile_data>();
        }
        profile = profile_ptr.get();
    }

    // Now use profile (which is guaranteed to be valid)
    // Update counters
    profile->call_count.fetch_add(1, std::memory_order_relaxed);
    if (!success) {
        profile->error_count.fetch_add(1, std::memory_order_relaxed);
    }

    // Record sample
    std::lock_guard sample_lock(profile->mutex);

    // Limit samples to prevent unbounded growth
    if (profile->samples.size() >= max_samples_per_operation_) {
        // Remove oldest sample (simple ring buffer behavior)
        // Using deque::pop_front() for O(1) performance instead of vector::erase(begin()) which is O(n)
        profile->samples.pop_front();
    }

    profile->samples.push_back(duration);

    return kcenon::monitoring::result<bool>(true);
}

kcenon::monitoring::result<kcenon::monitoring::performance_metrics> kcenon::monitoring::performance_profiler::get_metrics(
    const std::string& operation_name) const {

    std::shared_lock<std::shared_mutex> lock(profiles_mutex_);

    auto it = profiles_.find(operation_name);
    if (it == profiles_.end()) {
        return kcenon::monitoring::make_error<kcenon::monitoring::performance_metrics>(
            kcenon::monitoring::monitoring_error_code::not_found,
            "Operation not found: " + operation_name
        );
    }

    const auto& profile = it->second;

    std::lock_guard sample_lock(profile->mutex);

    kcenon::monitoring::performance_metrics metrics;
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
        metrics.mean_duration = total / profile->samples.size();

        // Calculate percentiles using sorted samples
        std::vector<std::chrono::nanoseconds> sorted_samples(profile->samples.begin(), profile->samples.end());
        std::sort(sorted_samples.begin(), sorted_samples.end());

        metrics.median_duration = performance_metrics::calculate_percentile(sorted_samples, 50.0);
        metrics.p95_duration = performance_metrics::calculate_percentile(sorted_samples, 95.0);
        metrics.p99_duration = performance_metrics::calculate_percentile(sorted_samples, 99.0);

        // Use atomic call_count, don't overwrite with samples.size()
        metrics.call_count = profile->call_count.load(std::memory_order_acquire);
    }

    return result<performance_metrics>(metrics);
}

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
    // TODO: Integrate with system_resource_collector or implement platform-specific metrics
    // Platform-specific implementations should be added for:
    // - Linux: /proc filesystem, sysinfo
    // - Windows: Performance Counters, GetSystemInfo
    // - macOS: sysctl, host_statistics
    return make_error<system_metrics>(
        monitoring_error_code::system_resource_unavailable,
        "Platform-specific system metrics not implemented. "
        "Please integrate with system_resource_collector or implement for this platform.");
}

result<bool> system_monitor::start_monitoring(std::chrono::milliseconds interval) {
    if (impl_->monitoring) {
        return make_success(true);
    }

    impl_->interval = interval;
    impl_->monitoring = true;

    // Start background monitoring thread
    impl_->monitor_thread = std::thread([this]() {
        while (impl_->monitoring.load(std::memory_order_acquire)) {
            auto metrics = get_current_metrics();
            if (metrics) {
                std::lock_guard<std::mutex> lock(impl_->history_mutex);
                impl_->history.push_back(metrics.value());

                // Trim history to prevent unbounded growth (keep last hour with 1s interval)
                if (impl_->history.size() > 3600) {
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
    (void)duration; // Suppress unused parameter warning
    std::lock_guard<std::mutex> lock(impl_->history_mutex);
    return impl_->history; // Simplified stub
}

// performance_monitor additional methods
result<metrics_snapshot> performance_monitor::collect() {
    metrics_snapshot snapshot;
    snapshot.capture_time = std::chrono::system_clock::now();
    snapshot.source_id = name_;

    // Add system metrics
    auto system_metrics_result = system_monitor_.get_current_metrics();
    if (system_metrics_result) {
        auto& sys_metrics = system_metrics_result.value();

        // Convert system metrics to metric_value format
        snapshot.add_metric("cpu_usage", sys_metrics.cpu_usage_percent);
        snapshot.add_metric("memory_usage", sys_metrics.memory_usage_percent);
        snapshot.add_metric("memory_bytes", static_cast<double>(sys_metrics.memory_usage_bytes));
        snapshot.add_metric("thread_count", static_cast<double>(sys_metrics.thread_count));
    }

    // Add profiler metrics
    auto profiler_metrics = profiler_.get_all_metrics();
    for (const auto& perf_metric : profiler_metrics) {
        // Add the metric value (using mean duration as the primary metric)
        snapshot.add_metric(perf_metric.operation_name,
                          static_cast<double>(perf_metric.mean_duration.count()));
    }

    return make_success(snapshot);
}

result<bool> performance_monitor::check_thresholds() const {
    return make_success(true); // Stub implementation
}

// performance_profiler additional methods
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

#ifdef MONITORING_USING_COMMON_INTERFACES
// IMonitor interface implementation

common::VoidResult performance_monitor::record_metric(const std::string& name, double value) {
    // Record as a duration metric in nanoseconds
    auto duration = std::chrono::nanoseconds(static_cast<std::int64_t>(value));
    auto result = profiler_.record_sample(name, duration, true);

    if (!result) {
        return common::error_info(
            static_cast<int>(result.get_error().code),
            result.get_error().message
        );
    }

    return std::monostate{};
}

common::VoidResult performance_monitor::record_metric(
    const std::string& name,
    double value,
    const std::unordered_map<std::string, std::string>& tags) {

    (void)tags;  // Suppress unused parameter warning on MSVC
    // For now, record the metric without tag support
    // Future enhancement: store tags in performance_profiler
    return record_metric(name, value);
}

common::Result<common::interfaces::metrics_snapshot> performance_monitor::get_metrics() {
    // Collect metrics from our internal profiler and system monitor
    auto snapshot_result = collect();

    if (!snapshot_result) {
        return common::error_info(
            static_cast<int>(snapshot_result.get_error().code),
            snapshot_result.get_error().message
        );
    }

    // Convert kcenon::monitoring::metrics_snapshot to common::interfaces::metrics_snapshot
    const auto& internal_snapshot = snapshot_result.value();
    common::interfaces::metrics_snapshot common_snapshot;
    common_snapshot.source_id = internal_snapshot.source_id;
    common_snapshot.capture_time = internal_snapshot.capture_time;

    // Convert metrics
    for (const auto& metric : internal_snapshot.metrics) {
        common::interfaces::metric_value common_metric;
        common_metric.name = metric.name;
        common_metric.value = metric.value;
        common_metric.timestamp = metric.timestamp;
        common_metric.tags = metric.tags;
        common_snapshot.metrics.push_back(common_metric);
    }

    return common_snapshot;
}

common::Result<common::interfaces::health_check_result> performance_monitor::check_health() {
    common::interfaces::health_check_result result;
    result.timestamp = std::chrono::system_clock::now();
    auto start_time = std::chrono::high_resolution_clock::now();

    // Check if we're enabled
    if (!is_enabled()) {
        result.status = common::interfaces::health_status::unknown;
        result.message = "Performance monitor is disabled";
        auto end_time = std::chrono::high_resolution_clock::now();
        result.check_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time
        );
        return result;
    }

    // Check thresholds
    auto threshold_result = check_thresholds();

    if (threshold_result && threshold_result.value()) {
        // Thresholds exceeded
        result.status = common::interfaces::health_status::degraded;
        result.message = "Performance thresholds exceeded";
    } else {
        result.status = common::interfaces::health_status::healthy;
        result.message = "Performance monitor operational";
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    result.check_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time
    );

    return result;
}

common::VoidResult performance_monitor::reset() {
    // Clear all profiler samples
    profiler_.clear_all_samples();

    return std::monostate{};
}
#endif // MONITORING_USING_COMMON_INTERFACES

// Global instance
performance_monitor& global_performance_monitor() {
    static performance_monitor instance;
    return instance;
}

} } // namespace kcenon::monitoring