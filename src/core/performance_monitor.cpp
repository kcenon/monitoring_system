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
 * @file performance_monitor.cpp
 * @brief Performance monitoring implementation
 */

#include <kcenon/monitoring/core/performance_monitor.h>
#include <shared_mutex>
#include <deque>
#include <limits>

// Platform-specific headers for system metrics
#if defined(__APPLE__)
    #include <mach/mach.h>
    #include <mach/mach_host.h>
    #include <mach/host_info.h>
    #include <mach/processor_info.h>
    #include <mach/vm_statistics.h>
    #include <sys/sysctl.h>
    #include <algorithm> // for std::max, std::min
#elif defined(__linux__)
    #include <sys/sysinfo.h>
    #include <unistd.h>
    #include <fstream>
#elif defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
    #define NOMINMAX
    #endif
    #include <windows.h>
    #include <pdh.h>
#endif

namespace kcenon { namespace monitoring {

common::Result<bool> performance_profiler::record_sample(
    const std::string& operation_name,
    std::chrono::nanoseconds duration,
    bool success) {

    if (!enabled_) {
        return common::ok(true);
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
            // Check if we've exceeded the max profiles limit
            if (profiles_.size() >= max_profiles_) {
                // Find and evict the least recently used profile
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
                }
            }

            profile_ptr = std::make_unique<profile_data>();
        }
        profile = profile_ptr.get();
    }

    // Update last access time atomically (thread-safe without locks)
    profile->last_access_time.store(
        std::chrono::steady_clock::now().time_since_epoch().count(),
        std::memory_order_relaxed
    );

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

    return common::ok(true);
}

common::Result<performance_metrics> performance_profiler::get_metrics(
    const std::string& operation_name) const {

    std::shared_lock<std::shared_mutex> lock(profiles_mutex_);

    auto it = profiles_.find(operation_name);
    if (it == profiles_.end()) {
        error_info err(monitoring_error_code::not_found,
                      "Operation not found: " + operation_name);
        return common::Result<performance_metrics>::err(err.to_common_error());
    }

    const auto& profile = it->second;

    // Update last access time atomically (for LRU tracking)
    profile->last_access_time.store(
        std::chrono::steady_clock::now().time_since_epoch().count(),
        std::memory_order_relaxed
    );

    std::lock_guard sample_lock(profile->mutex);

    performance_metrics metrics;
    metrics.operation_name = operation_name;
    metrics.call_count = profile->call_count.load();
    metrics.error_count = profile->error_count.load();

    if (!profile->samples.empty()) {
        // Convert deque to vector for statistics computation
        std::vector<std::chrono::nanoseconds> samples_vec(
            profile->samples.begin(), profile->samples.end());

        // Use centralized statistics utilities
        auto computed = stats::compute(samples_vec);

        metrics.min_duration = computed.min;
        metrics.max_duration = computed.max;
        metrics.mean_duration = computed.mean;
        metrics.median_duration = computed.median;
        metrics.p95_duration = computed.p95;
        metrics.p99_duration = computed.p99;
        metrics.total_duration = computed.total;

        // Use atomic call_count, don't overwrite with samples.size()
        metrics.call_count = profile->call_count.load(std::memory_order_acquire);
    }

    return common::ok(metrics);
}

// system_monitor implementation
struct system_monitor::monitor_impl {
    std::atomic<bool> monitoring{false};
    std::thread monitor_thread;
    std::deque<system_metrics> history;  // Changed from vector for O(1) front removal
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

common::Result<system_metrics> system_monitor::get_current_metrics() const {
#if defined(__APPLE__)
    // macOS implementation using mach APIs
    system_metrics metrics;
    metrics.timestamp = std::chrono::system_clock::now();

    // Get CPU usage via host_processor_info
    {
        natural_t processor_count;
        processor_info_array_t cpu_info;
        mach_msg_type_number_t cpu_info_count;

        if (host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO,
                                &processor_count,
                                &cpu_info,
                                &cpu_info_count) == KERN_SUCCESS) {

            uint64_t total_ticks = 0;
            uint64_t idle_ticks = 0;

            for (natural_t i = 0; i < processor_count; ++i) {
                processor_cpu_load_info_t cpu_load =
                    &((processor_cpu_load_info_t)cpu_info)[i];

                for (int state = 0; state < CPU_STATE_MAX; ++state) {
                    total_ticks += cpu_load->cpu_ticks[state];
                }
                idle_ticks += cpu_load->cpu_ticks[CPU_STATE_IDLE];
            }

            if (total_ticks > 0) {
                double usage = 100.0 * (1.0 - (static_cast<double>(idle_ticks) / total_ticks));
                metrics.cpu_usage_percent = (std::max)(0.0, (std::min)(100.0, usage));
            }

            vm_deallocate(mach_task_self(),
                         reinterpret_cast<vm_address_t>(cpu_info),
                         cpu_info_count * sizeof(int));
        }
    }

    // Get memory usage via host_statistics64
    {
        vm_statistics64_data_t vm_stats;
        mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;

        if (host_statistics64(mach_host_self(), HOST_VM_INFO64,
                             reinterpret_cast<host_info64_t>(&vm_stats),
                             &count) == KERN_SUCCESS) {

            // Page size (typically 4096 bytes on macOS)
            vm_size_t page_size;
            host_page_size(mach_host_self(), &page_size);

            // Calculate total physical memory
            uint64_t total_memory = 0;
            {
                int mib[2] = {CTL_HW, HW_MEMSIZE};
                size_t length = sizeof(total_memory);
                sysctl(mib, 2, &total_memory, &length, nullptr, 0);
            }

            // Calculate used memory (active + wired + compressed)
            uint64_t active_pages = vm_stats.active_count;
            uint64_t wired_pages = vm_stats.wire_count;
            uint64_t compressed_pages = vm_stats.compressor_page_count;

            metrics.memory_usage_bytes =
                (active_pages + wired_pages + compressed_pages) * page_size;

            metrics.available_memory_bytes =
                vm_stats.free_count * page_size;

            if (total_memory > 0) {
                metrics.memory_usage_percent =
                    100.0 * (static_cast<double>(metrics.memory_usage_bytes) / total_memory);
            }
        }
    }

    // Get thread count via task_info
    {
        task_basic_info_64_data_t task_basic_info;
        mach_msg_type_number_t count = TASK_BASIC_INFO_64_COUNT;

        if (task_info(mach_task_self(), TASK_BASIC_INFO_64,
                     reinterpret_cast<task_info_t>(&task_basic_info),
                     &count) == KERN_SUCCESS) {
            metrics.thread_count = task_basic_info.resident_size / 1024; // Rough approximation
        }
    }

    return common::ok(metrics);

#elif defined(__linux__)
    // Linux implementation using /proc filesystem
    return get_linux_system_metrics();

#elif defined(_WIN32)
    // Windows implementation using PDH (Performance Data Helper)
    return get_windows_system_metrics();

#else
    // Unknown platform
    error_info err(monitoring_error_code::system_resource_unavailable,
                  "Platform-specific system metrics not implemented for this platform. "
                  "Supported platforms: macOS (implemented), Linux (TODO), Windows (TODO).");
    return common::Result<system_metrics>::err(err.to_common_error());
#endif
}

common::Result<bool> system_monitor::start_monitoring(std::chrono::milliseconds interval) {
    if (impl_->monitoring) {
        return common::ok(true);
    }

    impl_->interval = interval;
    impl_->monitoring = true;

    // Start background monitoring thread
    impl_->monitor_thread = std::thread([this]() {
        while (impl_->monitoring.load(std::memory_order_acquire)) {
            auto metrics = get_current_metrics();
            if (metrics.is_ok()) {
                std::lock_guard<std::mutex> lock(impl_->history_mutex);
                impl_->history.push_back(metrics.value());

                // Trim history to prevent unbounded growth (keep last hour with 1s interval)
                // Using deque for O(1) front removal instead of O(n) vector::erase
                if (impl_->history.size() > 3600) {
                    impl_->history.pop_front();
                }
            }
            std::this_thread::sleep_for(impl_->interval);
        }
    });

    return common::ok(true);
}

common::Result<bool> system_monitor::stop_monitoring() {
    if (!impl_->monitoring) {
        return common::ok(true);
    }

    impl_->monitoring = false;
    if (impl_->monitor_thread.joinable()) {
        impl_->monitor_thread.join();
    }

    return common::ok(true);
}

bool system_monitor::is_monitoring() const {
    return impl_->monitoring;
}

std::vector<system_metrics> system_monitor::get_history(std::chrono::seconds duration) const {
    (void)duration; // Suppress unused parameter warning
    std::lock_guard<std::mutex> lock(impl_->history_mutex);
    // Convert deque to vector for API compatibility
    return std::vector<system_metrics>(impl_->history.begin(), impl_->history.end());
}

// performance_monitor additional methods
common::Result<metrics_snapshot> performance_monitor::collect() {
    metrics_snapshot snapshot;
    snapshot.capture_time = std::chrono::system_clock::now();
    snapshot.source_id = name_;

    // Add system metrics
    auto system_metrics_result = system_monitor_.get_current_metrics();
    if (system_metrics_result.is_ok()) {
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

    // Add tagged metrics (counters, gauges, histograms)
    auto tagged = get_all_tagged_metrics();
    for (const auto& metric : tagged) {
        snapshot.add_metric(metric.name, metric.value, metric.tags);
    }

    return common::ok(snapshot);
}

common::Result<bool> performance_monitor::check_thresholds() const {
    return common::ok(true); // Stub implementation
}

// performance_profiler additional methods
std::vector<performance_metrics> performance_profiler::get_all_metrics() const {
    std::vector<performance_metrics> result;
    std::shared_lock<std::shared_mutex> lock(profiles_mutex_);

    for (const auto& [name, profile] : profiles_) {
        // Calculate metrics inline to avoid nested locking with get_metrics()
        // get_metrics() acquires shared_lock on profiles_mutex_ which we already hold
        performance_metrics metrics;
        metrics.operation_name = name;
        metrics.call_count = profile->call_count.load();
        metrics.error_count = profile->error_count.load();

        // Lock individual profile for sample access
        std::lock_guard sample_lock(profile->mutex);

        if (!profile->samples.empty()) {
            // Convert deque to vector for statistics computation
            std::vector<std::chrono::nanoseconds> samples_vec(
                profile->samples.begin(), profile->samples.end());

            // Use centralized statistics utilities
            auto computed = stats::compute(samples_vec);

            metrics.min_duration = computed.min;
            metrics.max_duration = computed.max;
            metrics.mean_duration = computed.mean;
            metrics.median_duration = computed.median;
            metrics.p95_duration = computed.p95;
            metrics.p99_duration = computed.p99;
            metrics.total_duration = computed.total;
        }

        result.push_back(metrics);
    }

    return result;
}

common::Result<bool> performance_profiler::clear_samples(const std::string& operation_name) {
    std::unique_lock<std::shared_mutex> lock(profiles_mutex_);

    auto it = profiles_.find(operation_name);
    if (it != profiles_.end()) {
        std::lock_guard<std::mutex> data_lock(it->second->mutex);
        it->second->samples.clear();
        it->second->call_count = 0;
        it->second->error_count = 0;
    }

    return common::ok(true);
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

// IMonitor interface implementations removed (use performance_monitor_adapter instead)

// =========================================================================
// Tagged Metric Recording Methods
// =========================================================================

std::string performance_monitor::make_metric_key(const std::string& name, const tag_map& tags) {
    std::string key = name;
    if (!tags.empty()) {
        // Sort tags for consistent key generation
        std::vector<std::pair<std::string, std::string>> sorted_tags(
            tags.begin(), tags.end());
        std::sort(sorted_tags.begin(), sorted_tags.end());
        for (const auto& [tag_key, tag_value] : sorted_tags) {
            key += ";";
            key += tag_key;
            key += "=";
            key += tag_value;
        }
    }
    return key;
}

common::VoidResult performance_monitor::record_metric_internal(
    const std::string& name, double value,
    recorded_metric_type type, const tag_map& tags) {

    if (!enabled_) {
        return common::ok();
    }

    if (name.empty()) {
        error_info err(monitoring_error_code::invalid_configuration,
                      "Metric name cannot be empty");
        return common::VoidResult::err(err.to_common_error());
    }

    const std::string key = make_metric_key(name, tags);
    metric_data* data = nullptr;

    // First, try read lock (hot path optimization)
    {
        std::shared_lock<std::shared_mutex> read_lock(metrics_mutex_);
        auto it = tagged_metrics_.find(key);
        if (it != tagged_metrics_.end()) {
            data = it->second.get();
        }
    }

    // If not found, acquire write lock to create
    if (data == nullptr) {
        std::unique_lock<std::shared_mutex> write_lock(metrics_mutex_);
        // Double-check after acquiring write lock
        auto& data_ptr = tagged_metrics_[key];
        if (!data_ptr) {
            data_ptr = std::make_unique<metric_data>();
            data_ptr->type = type;
            data_ptr->tags = tags;
        }
        data = data_ptr.get();
    }

    // Update the metric value based on type
    std::unique_lock<std::shared_mutex> write_lock(metrics_mutex_);
    data->last_update = std::chrono::system_clock::now();

    switch (type) {
        case recorded_metric_type::counter:
            data->value += value;  // Counters accumulate
            break;
        case recorded_metric_type::gauge:
            data->value = value;  // Gauges replace
            break;
        case recorded_metric_type::histogram:
            data->value = value;  // Store latest value
            // Add to histogram samples
            if (data->histogram_values.size() >= data->max_histogram_samples) {
                data->histogram_values.erase(data->histogram_values.begin());
            }
            data->histogram_values.push_back(value);
            break;
    }

    return common::ok();
}

common::VoidResult performance_monitor::record_counter(const std::string& name, double value,
                                                        const tag_map& tags) {
    return record_metric_internal(name, value, recorded_metric_type::counter, tags);
}

common::VoidResult performance_monitor::record_gauge(const std::string& name, double value,
                                                      const tag_map& tags) {
    return record_metric_internal(name, value, recorded_metric_type::gauge, tags);
}

common::VoidResult performance_monitor::record_histogram(const std::string& name, double value,
                                                          const tag_map& tags) {
    return record_metric_internal(name, value, recorded_metric_type::histogram, tags);
}

std::vector<tagged_metric> performance_monitor::get_all_tagged_metrics() const {
    std::vector<tagged_metric> result;
    std::shared_lock<std::shared_mutex> lock(metrics_mutex_);

    result.reserve(tagged_metrics_.size());
    for (const auto& [key, data] : tagged_metrics_) {
        // Extract name from key (everything before first ';' or entire key)
        std::string name = key;
        auto semicolon_pos = key.find(';');
        if (semicolon_pos != std::string::npos) {
            name = key.substr(0, semicolon_pos);
        }

        tagged_metric metric(name, data->value, data->type, data->tags);
        metric.timestamp = data->last_update;
        result.push_back(std::move(metric));
    }

    return result;
}

void performance_monitor::clear_all_metrics() {
    std::unique_lock<std::shared_mutex> lock(metrics_mutex_);
    tagged_metrics_.clear();
}

// Global instance
performance_monitor& global_performance_monitor() {
    static performance_monitor instance;
    return instance;
}

} } // namespace kcenon::monitoring