#pragma once

/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, monitoring_system contributors
All rights reserved.
*****************************************************************************/

/**
 * @file metric_types.h
 * @brief Common metric type definitions for efficient storage
 * 
 * This file defines standardized metric types optimized for memory efficiency
 * and cache-friendly access patterns in the monitoring system.
 */

#include "../core/result_types.h"
#include "../core/error_codes.h"
#include <string>
#include <chrono>
#include <unordered_map>
#include <variant>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace kcenon { namespace monitoring {

/**
 * @enum metric_type
 * @brief Types of metrics supported by the system
 */
enum class metric_type : uint8_t {
    counter = 0,        // Monotonically increasing value
    gauge,              // Instantaneous value
    histogram,          // Distribution of values
    summary,            // Summary statistics
    timer,              // Duration measurements
    set                 // Unique value counting
};

/**
 * @brief Convert metric type to string
 */
constexpr const char* metric_type_to_string(metric_type type) noexcept {
    switch (type) {
        case metric_type::counter:   return "counter";
        case metric_type::gauge:     return "gauge";
        case metric_type::histogram: return "histogram";
        case metric_type::summary:   return "summary";
        case metric_type::timer:     return "timer";
        case metric_type::set:       return "set";
        default:                     return "unknown";
    }
}

/**
 * @struct metric_metadata
 * @brief Compact metadata for metrics
 */
struct metric_metadata {
    uint32_t name_hash;                              // Hashed metric name for fast lookup
    metric_type type;                                // Type of metric
    uint8_t tag_count;                              // Number of tags (max 255)
    uint16_t reserved;                              // Reserved for future use
    
    metric_metadata() noexcept 
        : name_hash(0), type(metric_type::gauge), tag_count(0), reserved(0) {}
    
    metric_metadata(uint32_t hash, metric_type mt, uint8_t tags = 0) noexcept
        : name_hash(hash), type(mt), tag_count(tags), reserved(0) {}
};

/**
 * @struct compact_metric_value
 * @brief Memory-efficient metric value storage
 */
struct compact_metric_value {
    using value_type = std::variant<
        double,           // Numeric value
        int64_t,          // Integer value
        std::string       // String value (for sets)
    >;
    
    metric_metadata metadata;
    value_type value;
    uint64_t timestamp_us;  // Microseconds since epoch for precision
    
    compact_metric_value() noexcept 
        : value(0.0), timestamp_us(0) {}
    
    compact_metric_value(const metric_metadata& meta, double val) noexcept
        : metadata(meta), value(val) {
        timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
    
    compact_metric_value(const metric_metadata& meta, int64_t val) noexcept
        : metadata(meta), value(val) {
        timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
    
    compact_metric_value(const metric_metadata& meta, std::string val) noexcept
        : metadata(meta), value(std::move(val)) {
        timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
    
    /**
     * @brief Get value as double
     */
    double as_double() const {
        if (std::holds_alternative<double>(value)) {
            return std::get<double>(value);
        } else if (std::holds_alternative<int64_t>(value)) {
            return static_cast<double>(std::get<int64_t>(value));
        }
        return 0.0;
    }
    
    /**
     * @brief Get value as integer
     */
    int64_t as_int64() const {
        if (std::holds_alternative<int64_t>(value)) {
            return std::get<int64_t>(value);
        } else if (std::holds_alternative<double>(value)) {
            return static_cast<int64_t>(std::get<double>(value));
        }
        return 0;
    }
    
    /**
     * @brief Get value as string
     */
    std::string as_string() const {
        if (std::holds_alternative<std::string>(value)) {
            return std::get<std::string>(value);
        } else if (std::holds_alternative<double>(value)) {
            return std::to_string(std::get<double>(value));
        } else if (std::holds_alternative<int64_t>(value)) {
            return std::to_string(std::get<int64_t>(value));
        }
        return "";
    }
    
    /**
     * @brief Get timestamp as time_point
     */
    std::chrono::system_clock::time_point get_timestamp() const {
        return std::chrono::system_clock::time_point(
            std::chrono::microseconds(timestamp_us));
    }
    
    /**
     * @brief Check if metric is numeric
     */
    bool is_numeric() const noexcept {
        return std::holds_alternative<double>(value) || 
               std::holds_alternative<int64_t>(value);
    }
    
    /**
     * @brief Get memory footprint in bytes
     */
    size_t memory_footprint() const noexcept {
        size_t base_size = sizeof(metric_metadata) + sizeof(timestamp_us) + sizeof(value_type);
        if (std::holds_alternative<std::string>(value)) {
            base_size += std::get<std::string>(value).capacity();
        }
        return base_size;
    }
};

/**
 * @struct metric_batch
 * @brief Batch of metrics for efficient processing
 */
struct metric_batch {
    std::vector<compact_metric_value> metrics;
    std::chrono::system_clock::time_point batch_timestamp;
    size_t batch_id;
    
    metric_batch() : batch_timestamp(std::chrono::system_clock::now()), batch_id(0) {}
    
    explicit metric_batch(size_t id) 
        : batch_timestamp(std::chrono::system_clock::now()), batch_id(id) {}
    
    /**
     * @brief Add metric to batch
     */
    void add_metric(compact_metric_value&& metric) {
        metrics.emplace_back(std::move(metric));
    }
    
    /**
     * @brief Get batch size in bytes
     */
    size_t memory_footprint() const noexcept {
        size_t total = sizeof(metric_batch);
        for (const auto& metric : metrics) {
            total += metric.memory_footprint();
        }
        return total;
    }
    
    /**
     * @brief Reserve space for metrics
     */
    void reserve(size_t count) {
        metrics.reserve(count);
    }
    
    /**
     * @brief Clear all metrics
     */
    void clear() {
        metrics.clear();
        batch_timestamp = std::chrono::system_clock::now();
    }
    
    /**
     * @brief Check if batch is empty
     */
    bool empty() const noexcept {
        return metrics.empty();
    }
    
    /**
     * @brief Get number of metrics in batch
     */
    size_t size() const noexcept {
        return metrics.size();
    }
};

/**
 * @struct histogram_bucket
 * @brief Bucket for histogram metrics
 */
struct histogram_bucket {
    double upper_bound;
    uint64_t count;
    
    histogram_bucket(double bound = 0.0, uint64_t cnt = 0) noexcept
        : upper_bound(bound), count(cnt) {}
    
    bool operator<(const histogram_bucket& other) const noexcept {
        return upper_bound < other.upper_bound;
    }
};

/**
 * @struct histogram_data
 * @brief Histogram data with buckets
 */
struct histogram_data {
    std::vector<histogram_bucket> buckets;
    uint64_t total_count = 0;
    double sum = 0.0;
    
    /**
     * @brief Add value to histogram
     */
    void add_sample(double value) {
        sum += value;
        total_count++;
        
        for (auto& bucket : buckets) {
            if (value <= bucket.upper_bound) {
                bucket.count++;
            }
        }
    }
    
    /**
     * @brief Get mean value
     */
    double mean() const noexcept {
        return total_count > 0 ? sum / total_count : 0.0;
    }
    
    /**
     * @brief Initialize standard buckets
     */
    void init_standard_buckets() {
        buckets = {
            {0.005, 0}, {0.01, 0}, {0.025, 0}, {0.05, 0}, {0.075, 0},
            {0.1, 0}, {0.25, 0}, {0.5, 0}, {0.75, 0}, {1.0, 0},
            {2.5, 0}, {5.0, 0}, {7.5, 0}, {10.0, 0}, 
            {std::numeric_limits<double>::infinity(), 0}
        };
    }
};

/**
 * @struct summary_data
 * @brief Summary statistics for metrics
 */
struct summary_data {
    uint64_t count = 0;
    double sum = 0.0;
    double min_value = std::numeric_limits<double>::max();
    double max_value = std::numeric_limits<double>::lowest();
    
    /**
     * @brief Add sample to summary
     */
    void add_sample(double value) {
        count++;
        sum += value;
        min_value = std::min(min_value, value);
        max_value = std::max(max_value, value);
    }
    
    /**
     * @brief Get mean value
     */
    double mean() const noexcept {
        return count > 0 ? sum / count : 0.0;
    }
    
    /**
     * @brief Reset summary
     */
    void reset() {
        count = 0;
        sum = 0.0;
        min_value = std::numeric_limits<double>::max();
        max_value = std::numeric_limits<double>::lowest();
    }
};

/**
 * @struct timer_data
 * @brief Timer data with percentile calculations
 *
 * Stores duration samples and provides percentile calculations.
 * Uses a reservoir sampling approach for memory efficiency.
 */
struct timer_data {
    static constexpr size_t DEFAULT_RESERVOIR_SIZE = 1024;

    std::vector<double> samples;
    size_t max_samples;
    uint64_t total_count = 0;
    double sum = 0.0;
    double min_value = std::numeric_limits<double>::max();
    double max_value = std::numeric_limits<double>::lowest();
    mutable bool sorted = false;

    /**
     * @brief Construct timer with default reservoir size
     */
    timer_data() : max_samples(DEFAULT_RESERVOIR_SIZE) {
        samples.reserve(max_samples);
    }

    /**
     * @brief Construct timer with custom reservoir size
     */
    explicit timer_data(size_t reservoir_size)
        : max_samples(reservoir_size) {
        samples.reserve(max_samples);
    }

    /**
     * @brief Record a duration sample (in milliseconds)
     */
    void record(double duration_ms) {
        total_count++;
        sum += duration_ms;
        min_value = std::min(min_value, duration_ms);
        max_value = std::max(max_value, duration_ms);
        sorted = false;

        if (samples.size() < max_samples) {
            samples.push_back(duration_ms);
        } else {
            // Reservoir sampling for memory efficiency
            size_t idx = static_cast<size_t>(rand()) % total_count;
            if (idx < max_samples) {
                samples[idx] = duration_ms;
            }
        }
    }

    /**
     * @brief Record a duration using chrono duration
     */
    template<typename Rep, typename Period>
    void record(std::chrono::duration<Rep, Period> duration) {
        auto ms = std::chrono::duration_cast<std::chrono::microseconds>(duration).count() / 1000.0;
        record(ms);
    }

    /**
     * @brief Get percentile value (0-100)
     * @param percentile The percentile to calculate (e.g., 50 for median, 99 for p99)
     * @return The value at the given percentile
     */
    double get_percentile(double percentile) const {
        if (samples.empty()) return 0.0;
        if (percentile <= 0) return min_value;
        if (percentile >= 100) return max_value;

        ensure_sorted();

        double rank = (percentile / 100.0) * (samples.size() - 1);
        size_t lower_idx = static_cast<size_t>(rank);
        size_t upper_idx = lower_idx + 1;
        double fraction = rank - lower_idx;

        if (upper_idx >= samples.size()) {
            return samples[lower_idx];
        }

        // Linear interpolation between adjacent values
        return samples[lower_idx] + fraction * (samples[upper_idx] - samples[lower_idx]);
    }

    /**
     * @brief Get median (p50)
     */
    double median() const {
        return get_percentile(50.0);
    }

    /**
     * @brief Get p90 value
     */
    double p90() const {
        return get_percentile(90.0);
    }

    /**
     * @brief Get p95 value
     */
    double p95() const {
        return get_percentile(95.0);
    }

    /**
     * @brief Get p99 value
     */
    double p99() const {
        return get_percentile(99.0);
    }

    /**
     * @brief Get p999 value (99.9th percentile)
     */
    double p999() const {
        return get_percentile(99.9);
    }

    /**
     * @brief Get mean value
     */
    double mean() const noexcept {
        return total_count > 0 ? sum / total_count : 0.0;
    }

    /**
     * @brief Get sample count
     */
    uint64_t count() const noexcept {
        return total_count;
    }

    /**
     * @brief Get minimum recorded value
     */
    double min() const noexcept {
        return total_count > 0 ? min_value : 0.0;
    }

    /**
     * @brief Get maximum recorded value
     */
    double max() const noexcept {
        return total_count > 0 ? max_value : 0.0;
    }

    /**
     * @brief Get standard deviation
     */
    double stddev() const {
        if (samples.size() < 2) return 0.0;

        double avg = mean();
        double variance = 0.0;
        for (double sample : samples) {
            double diff = sample - avg;
            variance += diff * diff;
        }
        variance /= samples.size();
        return std::sqrt(variance);
    }

    /**
     * @brief Reset all data
     */
    void reset() {
        samples.clear();
        samples.reserve(max_samples);
        total_count = 0;
        sum = 0.0;
        min_value = std::numeric_limits<double>::max();
        max_value = std::numeric_limits<double>::lowest();
        sorted = false;
    }

    /**
     * @brief Get snapshot of current statistics
     */
    struct snapshot {
        uint64_t count;
        double mean;
        double min;
        double max;
        double stddev;
        double p50;
        double p90;
        double p95;
        double p99;
        double p999;
    };

    snapshot get_snapshot() const {
        return snapshot{
            total_count,
            mean(),
            min(),
            max(),
            stddev(),
            median(),
            p90(),
            p95(),
            p99(),
            p999()
        };
    }

private:
    void ensure_sorted() const {
        if (!sorted && !samples.empty()) {
            auto& mutable_samples = const_cast<std::vector<double>&>(samples);
            std::sort(mutable_samples.begin(), mutable_samples.end());
            const_cast<bool&>(sorted) = true;
        }
    }
};

/**
 * @brief RAII timer scope for automatic duration recording
 */
class scoped_timer {
public:
    explicit scoped_timer(timer_data& timer)
        : timer_(timer), start_(std::chrono::steady_clock::now()) {}

    ~scoped_timer() {
        auto end = std::chrono::steady_clock::now();
        timer_.record(end - start_);
    }

    // Non-copyable, non-movable
    scoped_timer(const scoped_timer&) = delete;
    scoped_timer& operator=(const scoped_timer&) = delete;
    scoped_timer(scoped_timer&&) = delete;
    scoped_timer& operator=(scoped_timer&&) = delete;

private:
    timer_data& timer_;
    std::chrono::steady_clock::time_point start_;
};

/**
 * @brief Hash function for metric names
 */
inline uint32_t hash_metric_name(const std::string& name) noexcept {
    // Simple FNV-1a hash for fast metric name hashing
    uint32_t hash = 2166136261U;
    for (char c : name) {
        hash ^= static_cast<uint32_t>(c);
        hash *= 16777619U;
    }
    return hash;
}

/**
 * @brief Create metric metadata from name and type
 */
inline metric_metadata create_metric_metadata(const std::string& name, 
                                             metric_type type,
                                             size_t tag_count = 0) {
    return metric_metadata(
        hash_metric_name(name), 
        type, 
        static_cast<uint8_t>(std::min(tag_count, size_t(255)))
    );
}

} } // namespace kcenon::monitoring