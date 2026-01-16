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
 * @file performance_monitor.h
 * @brief Performance monitoring and profiling implementation
 * @date 2025
 * 
 * Provides performance monitoring capabilities including CPU, memory,
 * and custom performance metrics collection with minimal overhead.
 */

#pragma once

#include <string>
#include <memory>
#include <chrono>
#include <vector>
#include <deque>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <functional>
#include <thread>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <shared_mutex>

#include "../core/result_types.h"
#include "../core/error_codes.h"
#include "../interfaces/monitoring_core.h"
#include "../utils/statistics.h"

// Use common_system interfaces (Phase 2.3.4)
#include <kcenon/common/interfaces/monitoring_interface.h>

namespace kcenon { namespace monitoring {

/**
 * @brief Type alias for metric tags/labels
 *
 * Tags are key-value pairs that add dimensions to metrics,
 * enabling filtering and aggregation by service, endpoint, host, etc.
 */
using tag_map = std::unordered_map<std::string, std::string>;

/**
 * @enum recorded_metric_type
 * @brief Types of recorded metrics
 */
enum class recorded_metric_type {
    counter,    ///< Monotonically increasing counter
    gauge,      ///< Instantaneous value that can go up and down
    histogram   ///< Distribution of values with buckets
};

/**
 * @struct tagged_metric
 * @brief Represents a metric value with associated tags
 */
struct tagged_metric {
    std::string name;
    double value;
    recorded_metric_type type;
    tag_map tags;
    std::chrono::system_clock::time_point timestamp;

    tagged_metric(const std::string& n, double v, recorded_metric_type t,
                  const tag_map& tgs = {})
        : name(n)
        , value(v)
        , type(t)
        , tags(tgs)
        , timestamp(std::chrono::system_clock::now()) {}

    /**
     * @brief Generate unique key for aggregation based on name and sorted tags
     */
    std::string key() const {
        std::string k = name;
        // Sort tags for consistent key generation
        std::vector<std::pair<std::string, std::string>> sorted_tags(
            tags.begin(), tags.end());
        std::sort(sorted_tags.begin(), sorted_tags.end());
        for (const auto& [tag_key, tag_value] : sorted_tags) {
            k += ";" + tag_key + "=" + tag_value;
        }
        return k;
    }
};

/**
 * @brief Performance metrics for a specific operation
 */
struct performance_metrics {
    std::string operation_name;
    std::chrono::nanoseconds min_duration{std::chrono::nanoseconds::max()};
    std::chrono::nanoseconds max_duration{std::chrono::nanoseconds::zero()};
    std::chrono::nanoseconds total_duration{std::chrono::nanoseconds::zero()};
    std::chrono::nanoseconds mean_duration{std::chrono::nanoseconds::zero()};
    std::chrono::nanoseconds median_duration{std::chrono::nanoseconds::zero()};
    std::chrono::nanoseconds p95_duration{std::chrono::nanoseconds::zero()};
    std::chrono::nanoseconds p99_duration{std::chrono::nanoseconds::zero()};
    std::uint64_t call_count{0};
    std::uint64_t error_count{0};
    double throughput{0.0};  // Operations per second
    
    /**
     * @brief Calculate percentile from sorted durations
     * @deprecated Use stats::percentile() directly for new code
     */
    static std::chrono::nanoseconds calculate_percentile(
        const std::vector<std::chrono::nanoseconds>& sorted_durations,
        double percentile_value) {
        return stats::percentile(sorted_durations, percentile_value);
    }

    /**
     * @brief Update statistics with new duration samples
     * @deprecated Use stats::compute() directly for new code
     */
    void update_statistics(const std::vector<std::chrono::nanoseconds>& durations) {
        if (durations.empty()) return;

        auto computed = stats::compute(durations);
        min_duration = computed.min;
        max_duration = computed.max;
        mean_duration = computed.mean;
        median_duration = computed.median;
        p95_duration = computed.p95;
        p99_duration = computed.p99;
        total_duration = computed.total;
    }
};

/**
 * @brief System resource metrics
 */
struct system_metrics {
    double cpu_usage_percent{0.0};
    double memory_usage_percent{0.0};
    std::size_t memory_usage_bytes{0};
    std::size_t available_memory_bytes{0};
    std::uint32_t thread_count{0};
    std::uint32_t handle_count{0};
    double disk_io_read_rate{0.0};   // MB/s
    double disk_io_write_rate{0.0};  // MB/s
    double network_io_recv_rate{0.0}; // MB/s
    double network_io_send_rate{0.0}; // MB/s
    
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @brief Performance profiler for code sections
 *
 * @thread_safety Thread-safe. All public methods can be called concurrently.
 *   - Uses std::shared_mutex for read/write synchronization on profiles
 *   - Uses per-profile std::mutex for sample data protection
 *   - Uses std::atomic for counters and flags
 */
class performance_profiler {
private:
    struct profile_data {
        // Using deque instead of vector for O(1) pop_front performance
        // when removing oldest samples in ring buffer behavior
        std::deque<std::chrono::nanoseconds> samples;
        std::atomic<std::uint64_t> call_count{0};
        std::atomic<std::uint64_t> error_count{0};
        // Store time as atomic integer for thread-safe access without locks
        std::atomic<std::chrono::steady_clock::rep> last_access_time{std::chrono::steady_clock::now().time_since_epoch().count()};
        mutable std::mutex mutex;
    };

    std::unordered_map<std::string, std::unique_ptr<profile_data>> profiles_;
    mutable std::shared_mutex profiles_mutex_;
    std::atomic<bool> enabled_{true};
    std::size_t max_samples_per_operation_{10000};
    std::size_t max_profiles_{10000};  // LRU eviction threshold

    // Lock-free collection path (Sprint 3-4)
    std::atomic<bool> use_lock_free_path_{false};
    
public:
    /**
     * @brief Record a performance sample
     */
    kcenon::monitoring::result<bool> record_sample(
        const std::string& operation_name,
        std::chrono::nanoseconds duration,
        bool success = true
    );
    
    /**
     * @brief Get performance metrics for an operation
     */
    kcenon::monitoring::result<performance_metrics> get_metrics(
        const std::string& operation_name
    ) const;
    
    /**
     * @brief Get all performance metrics
     */
    std::vector<performance_metrics> get_all_metrics() const;
    
    /**
     * @brief Clear samples for an operation
     */
    kcenon::monitoring::result<bool> clear_samples(const std::string& operation_name);
    
    /**
     * @brief Clear all samples
     */
    void clear_all_samples();
    
    /**
     * @brief Enable or disable profiling
     */
    void set_enabled(bool enabled) { enabled_ = enabled; }
    
    /**
     * @brief Check if profiling is enabled
     */
    bool is_enabled() const { return enabled_; }
    
    /**
     * @brief Set maximum samples per operation
     */
    void set_max_samples(std::size_t max_samples) {
        max_samples_per_operation_ = max_samples;
    }

    /**
     * @brief Enable lock-free collection path (Sprint 3-4)
     *
     * When enabled, uses thread-local buffers for lock-free metric collection.
     * This provides significantly better performance under high concurrency.
     *
     * @param enable true to enable lock-free path, false for legacy path
     * @note Lock-free path is experimental in Sprint 3-4
     */
    void set_lock_free_mode(bool enable) {
        use_lock_free_path_ = enable;
    }

    /**
     * @brief Check if lock-free mode is enabled
     * @return true if lock-free mode is active
     */
    bool is_lock_free_mode() const {
        return use_lock_free_path_;
    }
};

/**
 * @brief Scoped performance timer
 */
class scoped_timer {
private:
    performance_profiler* profiler_;
    std::string operation_name_;
    std::chrono::high_resolution_clock::time_point start_time_;
    bool success_{true};
    bool completed_{false};
    
public:
    scoped_timer(performance_profiler* profiler, const std::string& operation_name)
        : profiler_(profiler)
        , operation_name_(operation_name)
        , start_time_(std::chrono::high_resolution_clock::now()) {}
    
    ~scoped_timer() {
        if (!completed_ && profiler_) {
            complete();
        }
    }
    
    /**
     * @brief Mark the operation as failed
     */
    void mark_failed() { success_ = false; }
    
    /**
     * @brief Manually complete the timing
     */
    void complete() {
        if (completed_) return;
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
            end_time - start_time_
        );
        
        if (profiler_) {
            profiler_->record_sample(operation_name_, duration, success_);
        }
        
        completed_ = true;
    }
    
    /**
     * @brief Get elapsed time without completing
     */
    std::chrono::nanoseconds elapsed() const {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            now - start_time_
        );
    }
};

/**
 * @brief System resource monitor
 */
class system_monitor {
private:
    struct monitor_impl;
    std::unique_ptr<monitor_impl> impl_;
    
public:
    system_monitor();
    ~system_monitor();
    
    // Disable copy
    system_monitor(const system_monitor&) = delete;
    system_monitor& operator=(const system_monitor&) = delete;
    
    // Enable move
    system_monitor(system_monitor&&) noexcept;
    system_monitor& operator=(system_monitor&&) noexcept;
    
    /**
     * @brief Get current system metrics
     */
    kcenon::monitoring::result<system_metrics> get_current_metrics() const;
    
    /**
     * @brief Start monitoring system resources
     */
    kcenon::monitoring::result<bool> start_monitoring(
        std::chrono::milliseconds interval = std::chrono::milliseconds(1000)
    );
    
    /**
     * @brief Stop monitoring
     */
    kcenon::monitoring::result<bool> stop_monitoring();
    
    /**
     * @brief Check if monitoring is active
     */
    bool is_monitoring() const;
    
    /**
     * @brief Get historical metrics
     */
    std::vector<system_metrics> get_history(
        std::chrono::seconds duration = std::chrono::seconds(60)
    ) const;
};

/**
 * @brief Performance monitor combining profiling and system monitoring
 *
 * Implements kcenon::monitoring::metrics_collector for internal monitoring.
 * For interoperability with common::interfaces::IMonitor, use performance_monitor_adapter.
 *
 * @thread_safety Thread-safe. All public methods can be called concurrently.
 *   - profiler_ is thread-safe (uses internal synchronization)
 *   - system_monitor_ is thread-safe (uses internal synchronization)
 *   - thresholds_ protected by thresholds_mutex_
 *
 * @see performance_monitor_adapter For bridging to common::interfaces::IMonitor
 */
class performance_monitor : public metrics_collector {
private:
    performance_profiler profiler_;
    system_monitor system_monitor_;
    std::string name_;
    bool enabled_{true};

    // Performance thresholds for alerting
    struct thresholds {
        double cpu_threshold{80.0};
        double memory_threshold{90.0};
        std::chrono::milliseconds latency_threshold{1000};
    } thresholds_;
    mutable std::mutex thresholds_mutex_;  // Protects thresholds_

    // Tagged metrics storage for counters, gauges, and histograms
    struct metric_data {
        double value{0.0};
        recorded_metric_type type;
        tag_map tags;
        std::chrono::system_clock::time_point last_update;
        std::vector<double> histogram_values;  // For histogram type only
        std::size_t max_histogram_samples{1000};
    };
    std::unordered_map<std::string, std::unique_ptr<metric_data>> tagged_metrics_;
    mutable std::shared_mutex metrics_mutex_;  // Protects tagged_metrics_
    
public:
    explicit performance_monitor(const std::string& name = "performance_monitor")
        : name_(name) {}
    
    // Implement metrics_collector interface
    std::string get_name() const override { return name_; }
    bool is_enabled() const override { return enabled_; }
    
    result_void set_enabled(bool enable) override {
        enabled_ = enable;
        profiler_.set_enabled(enable);
        return make_void_success();
    }

    result_void initialize() override {
        auto result = system_monitor_.start_monitoring();
        if (result.is_err()) {
            auto& err = result.error();
            return make_void_error(static_cast<monitoring_error_code>(err.code), err.message);
        }
        return make_void_success();
    }

    result_void cleanup() override {
        auto result = system_monitor_.stop_monitoring();
        if (result.is_err()) {
            auto& err = result.error();
            return make_void_error(static_cast<monitoring_error_code>(err.code), err.message);
        }
        return make_void_success();
    }
    
    kcenon::monitoring::result<metrics_snapshot> collect() override;
    
    /**
     * @brief Create a scoped timer for an operation
     */
    scoped_timer time_operation(const std::string& operation_name) {
        return scoped_timer(&profiler_, operation_name);
    }
    
    /**
     * @brief Get performance profiler
     */
    performance_profiler& get_profiler() { return profiler_; }
    const performance_profiler& get_profiler() const { return profiler_; }
    
    /**
     * @brief Get system monitor
     */
    system_monitor& get_system_monitor() { return system_monitor_; }
    const system_monitor& get_system_monitor() const { return system_monitor_; }
    
    /**
     * @brief Set performance thresholds
     * @thread_safety Thread-safe. Uses mutex for synchronization.
     */
    void set_cpu_threshold(double threshold) {
        std::lock_guard<std::mutex> lock(thresholds_mutex_);
        thresholds_.cpu_threshold = threshold;
    }

    void set_memory_threshold(double threshold) {
        std::lock_guard<std::mutex> lock(thresholds_mutex_);
        thresholds_.memory_threshold = threshold;
    }

    void set_latency_threshold(std::chrono::milliseconds threshold) {
        std::lock_guard<std::mutex> lock(thresholds_mutex_);
        thresholds_.latency_threshold = threshold;
    }

    /**
     * @brief Get current threshold values
     * @thread_safety Thread-safe. Uses mutex for synchronization.
     */
    thresholds get_thresholds() const {
        std::lock_guard<std::mutex> lock(thresholds_mutex_);
        return thresholds_;
    }
    
    /**
     * @brief Check if any thresholds are exceeded
     */
    kcenon::monitoring::result<bool> check_thresholds() const;

    // IMonitor interface implementation (Phase 2.3.4)

    /**
     * @brief Reset all performance profiler samples and system metrics
     * @note For IMonitor interface compatibility, use performance_monitor_adapter
     */
    void reset() {
        profiler_.clear_all_samples();
        clear_all_metrics();
    }

    // =========================================================================
    // Tagged Metric Recording Methods
    // =========================================================================

    /**
     * @brief Record a counter metric (monotonically increasing value)
     *
     * @param name Metric name (must not be empty)
     * @param value Value to add (should be >= 0 for counters)
     * @param tags Key-value labels for metric dimensions (default: empty)
     * @return result_void Success or error with details
     *
     * @thread_safety Thread-safe, uses shared_mutex for synchronization
     *
     * @example
     * @code
     * // Without tags
     * monitor.record_counter("requests_total", 1);
     *
     * // With tags
     * monitor.record_counter("http_requests_total", 1, {
     *     {"method", "GET"},
     *     {"endpoint", "/api/users"},
     *     {"status_code", "200"}
     * });
     * @endcode
     */
    result_void record_counter(const std::string& name, double value,
                               const tag_map& tags = {});

    /**
     * @brief Record a gauge metric (instantaneous value)
     *
     * @param name Metric name (must not be empty)
     * @param value Current value (can be positive or negative)
     * @param tags Key-value labels for metric dimensions (default: empty)
     * @return result_void Success or error with details
     *
     * @thread_safety Thread-safe, uses shared_mutex for synchronization
     *
     * @example
     * @code
     * // Without tags
     * monitor.record_gauge("temperature", 25.5);
     *
     * // With tags
     * monitor.record_gauge("active_connections", 42, {
     *     {"pool", "database"},
     *     {"host", "db-primary"}
     * });
     * @endcode
     */
    result_void record_gauge(const std::string& name, double value,
                             const tag_map& tags = {});

    /**
     * @brief Record a histogram metric (distribution of values)
     *
     * @param name Metric name (must not be empty)
     * @param value Observed value to record
     * @param tags Key-value labels for metric dimensions (default: empty)
     * @return result_void Success or error with details
     *
     * @thread_safety Thread-safe, uses shared_mutex for synchronization
     *
     * @example
     * @code
     * // Without tags
     * monitor.record_histogram("response_time_ms", 150.5);
     *
     * // With tags
     * monitor.record_histogram("request_duration_ms", 150.5, {
     *     {"service", "auth"},
     *     {"operation", "login"}
     * });
     * @endcode
     */
    result_void record_histogram(const std::string& name, double value,
                                 const tag_map& tags = {});

    /**
     * @brief Get all recorded tagged metrics
     *
     * @return Vector of tagged_metric containing all recorded metrics with their tags
     *
     * @thread_safety Thread-safe, uses shared_mutex for synchronization
     */
    std::vector<tagged_metric> get_all_tagged_metrics() const;

    /**
     * @brief Clear all recorded tagged metrics
     *
     * @thread_safety Thread-safe, uses shared_mutex for synchronization
     */
    void clear_all_metrics();

private:
    /**
     * @brief Generate a unique key from metric name and tags
     */
    static std::string make_metric_key(const std::string& name, const tag_map& tags);

    /**
     * @brief Internal method to record a metric with type and tags
     */
    result_void record_metric_internal(const std::string& name, double value,
                                       recorded_metric_type type, const tag_map& tags);
};

/**
 * @brief Global performance monitor instance
 */
performance_monitor& global_performance_monitor();

/**
 * @brief Helper macro for timing code sections
 */
#define PERF_TIMER(operation_name) \
    kcenon::monitoring::scoped_timer _perf_timer( \
        &kcenon::monitoring::global_performance_monitor().get_profiler(), \
        operation_name \
    )

#define PERF_TIMER_CUSTOM(profiler, operation_name) \
    kcenon::monitoring::scoped_timer _perf_timer(profiler, operation_name)

/**
 * @brief Performance benchmark utility
 */
class performance_benchmark {
private:
    performance_profiler profiler_;
    std::string name_;
    std::uint32_t iterations_{1000};
    std::uint32_t warmup_iterations_{100};
    
public:
    explicit performance_benchmark(const std::string& name)
        : name_(name) {}
    
    /**
     * @brief Set number of iterations
     */
    void set_iterations(std::uint32_t iterations) {
        iterations_ = iterations;
    }
    
    /**
     * @brief Set warmup iterations
     */
    void set_warmup_iterations(std::uint32_t warmup) {
        warmup_iterations_ = warmup;
    }
    
    /**
     * @brief Run a benchmark
     */
    template<typename Func>
    kcenon::monitoring::result<performance_metrics> run(
        const std::string& operation_name,
        Func&& func
    ) {
        // Warmup
        for (std::uint32_t i = 0; i < warmup_iterations_; ++i) {
            func();
        }
        
        // Actual benchmark
        for (std::uint32_t i = 0; i < iterations_; ++i) {
            auto start = std::chrono::high_resolution_clock::now();
            
            try {
                func();
            } catch (...) {
                // Record error
                auto end = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    end - start
                );
                profiler_.record_sample(operation_name, duration, false);
                continue;
            }
            
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
                end - start
            );
            profiler_.record_sample(operation_name, duration, true);
        }
        
        return profiler_.get_metrics(operation_name);
    }
    
    /**
     * @brief Compare two operations
     */
    template<typename Func1, typename Func2>
    kcenon::monitoring::result<std::pair<performance_metrics, performance_metrics>> compare(
        const std::string& operation1_name,
        Func1&& func1,
        const std::string& operation2_name,
        Func2&& func2
    ) {
        auto result1 = run(operation1_name, std::forward<Func1>(func1));
        if (result1.is_err()) {
            return result<std::pair<performance_metrics, performance_metrics>>::err(result1.error());
        }

        auto result2 = run(operation2_name, std::forward<Func2>(func2));
        if (result2.is_err()) {
            return result<std::pair<performance_metrics, performance_metrics>>::err(result2.error());
        }
        
        return std::make_pair(result1.value(), result2.value());
    }
};

// Platform-specific system metrics collection functions
#if defined(__linux__)
/**
 * @brief Get system metrics on Linux using /proc filesystem
 * @return System metrics or error
 */
result<system_metrics> get_linux_system_metrics();
#endif

#if defined(_WIN32)
/**
 * @brief Get system metrics on Windows using PDH API
 * @return System metrics or error
 */
result<system_metrics> get_windows_system_metrics();
#endif

} } // namespace kcenon::monitoring