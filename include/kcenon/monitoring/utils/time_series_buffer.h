#pragma once

/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, monitoring_system contributors
All rights reserved.
*****************************************************************************/

/**
 * @file time_series_buffer.h
 * @brief Generic time-series buffer for metric history tracking
 *
 * This file provides a thread-safe, memory-bounded ring buffer for storing
 * time-series data with configurable retention and statistics calculation.
 */

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#endif

#include "../core/result_types.h"
#include "../core/error_codes.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <mutex>
#include <vector>

namespace kcenon { namespace monitoring {

/**
 * @struct time_series_buffer_config
 * @brief Configuration for time series buffer
 */
struct time_series_buffer_config {
    size_t max_samples = 1000;

    result_void validate() const {
        if (max_samples == 0) {
            return make_result_void(monitoring_error_code::invalid_configuration,
                                    "Max samples must be positive");
        }
        return make_void_success();
    }
};

/**
 * @struct time_series_sample
 * @brief Single sample in time series with timestamp
 */
template <typename T>
struct time_series_sample {
    std::chrono::system_clock::time_point timestamp;
    T value;

    time_series_sample() noexcept : value{} {}

    time_series_sample(std::chrono::system_clock::time_point ts, T val) noexcept
        : timestamp(ts), value(val) {}
};

/**
 * @struct time_series_statistics
 * @brief Statistics calculated from time series data
 */
struct time_series_statistics {
    double min_value = (std::numeric_limits<double>::max)();
    double max_value = (std::numeric_limits<double>::lowest)();
    double avg = 0.0;
    double stddev = 0.0;
    double p95 = 0.0;
    double p99 = 0.0;
    size_t sample_count = 0;
    std::chrono::system_clock::time_point oldest_timestamp;
    std::chrono::system_clock::time_point newest_timestamp;
};

/**
 * @namespace detail
 * @brief Internal implementation details - not part of public API
 * @internal
 */
namespace detail {

/**
 * @brief Calculate percentile from sorted values
 * @param sorted_values Pre-sorted vector of values
 * @param percentile Percentile to calculate (0-100)
 * @return Calculated percentile value
 * @internal
 */
inline double calculate_percentile(const std::vector<double>& sorted_values,
                                   double percentile) {
    if (sorted_values.empty()) {
        return 0.0;
    }
    if (sorted_values.size() == 1) {
        return sorted_values[0];
    }

    double rank = (percentile / 100.0) * (sorted_values.size() - 1);
    size_t lower_idx = static_cast<size_t>(rank);
    size_t upper_idx = lower_idx + 1;
    double fraction = rank - lower_idx;

    if (upper_idx >= sorted_values.size()) {
        return sorted_values[lower_idx];
    }

    return sorted_values[lower_idx] +
           fraction * (sorted_values[upper_idx] - sorted_values[lower_idx]);
}

/**
 * @brief Calculate ring buffer actual index from logical index
 * @param logical_index Logical index (0 to count-1)
 * @param head Current head position
 * @param count Current element count
 * @param capacity Buffer capacity
 * @return Actual index in buffer
 * @internal
 */
inline size_t ring_buffer_index(size_t logical_index, size_t head,
                                size_t count, size_t capacity) noexcept {
    if (count < capacity) {
        return logical_index;
    }
    return (head + logical_index) % capacity;
}

/**
 * @brief Calculate basic statistics from a vector of double values
 * @param values Vector of values to analyze
 * @param oldest_timestamp Timestamp of oldest sample
 * @param newest_timestamp Timestamp of newest sample
 * @return Calculated statistics
 * @internal
 */
inline time_series_statistics calculate_basic_statistics(
    const std::vector<double>& values,
    std::chrono::system_clock::time_point oldest_timestamp,
    std::chrono::system_clock::time_point newest_timestamp) {

    time_series_statistics stats;
    stats.sample_count = values.size();

    if (values.empty()) {
        stats.min_value = 0.0;
        stats.max_value = 0.0;
        return stats;
    }

    stats.oldest_timestamp = oldest_timestamp;
    stats.newest_timestamp = newest_timestamp;

    double sum = 0.0;
    for (double val : values) {
        sum += val;
        stats.min_value = (std::min)(stats.min_value, val);
        stats.max_value = (std::max)(stats.max_value, val);
    }
    stats.avg = sum / values.size();

    double variance = 0.0;
    for (double val : values) {
        double diff = val - stats.avg;
        variance += diff * diff;
    }
    stats.stddev = std::sqrt(variance / values.size());

    std::vector<double> sorted_values = values;
    std::sort(sorted_values.begin(), sorted_values.end());
    stats.p95 = calculate_percentile(sorted_values, 95.0);
    stats.p99 = calculate_percentile(sorted_values, 99.0);

    return stats;
}

}  // namespace detail

/**
 * @class time_series_buffer
 * @brief Thread-safe ring buffer for time-series data with statistics
 * @tparam T The type of values to store (must be numeric)
 */
template <typename T>
class time_series_buffer {
    static_assert(std::is_arithmetic_v<T>, "T must be an arithmetic type");

  private:
    mutable std::mutex mutex_;
    std::vector<time_series_sample<T>> buffer_;
    size_t head_ = 0;
    size_t count_ = 0;
    time_series_buffer_config config_;

    size_t get_actual_index(size_t logical_index) const noexcept {
        return detail::ring_buffer_index(logical_index, head_, count_, config_.max_samples);
    }

  public:
    explicit time_series_buffer(const time_series_buffer_config& config = {}) : config_(config) {
        auto validation = config_.validate();
        if (validation.is_err()) {
            throw std::invalid_argument("Invalid time_series_buffer configuration: " +
                                        validation.error().message);
        }
        buffer_.resize(config_.max_samples);
    }

    time_series_buffer(const time_series_buffer&) = delete;
    time_series_buffer& operator=(const time_series_buffer&) = delete;
    time_series_buffer(time_series_buffer&&) = delete;
    time_series_buffer& operator=(time_series_buffer&&) = delete;

    /**
     * @brief Add a sample to the buffer
     * @param value The value to add
     * @param timestamp Optional timestamp (defaults to now)
     */
    void add_sample(T value,
                    std::chrono::system_clock::time_point timestamp =
                        std::chrono::system_clock::now()) {
        std::lock_guard<std::mutex> lock(mutex_);

        buffer_[head_] = time_series_sample<T>(timestamp, value);
        head_ = (head_ + 1) % config_.max_samples;

        if (count_ < config_.max_samples) {
            ++count_;
        }
    }

    /**
     * @brief Get samples within a duration from now
     * @param duration How far back to look
     * @return Vector of samples within the duration
     */
    template <typename Duration>
    std::vector<time_series_sample<T>> get_samples(Duration duration) const {
        auto cutoff = std::chrono::system_clock::now() - duration;
        return get_samples_since(cutoff);
    }

    /**
     * @brief Get samples since a specific timestamp
     * @param since The timestamp to start from
     * @return Vector of samples since the timestamp
     */
    std::vector<time_series_sample<T>> get_samples_since(
        std::chrono::system_clock::time_point since) const {
        std::lock_guard<std::mutex> lock(mutex_);

        std::vector<time_series_sample<T>> result;
        result.reserve(count_);

        for (size_t i = 0; i < count_; ++i) {
            const auto& sample = buffer_[get_actual_index(i)];
            if (sample.timestamp >= since) {
                result.push_back(sample);
            }
        }

        std::sort(result.begin(), result.end(),
                  [](const time_series_sample<T>& a, const time_series_sample<T>& b) {
                      return a.timestamp < b.timestamp;
                  });

        return result;
    }

    /**
     * @brief Get all samples in chronological order
     * @return Vector of all samples
     */
    std::vector<time_series_sample<T>> get_all_samples() const {
        std::lock_guard<std::mutex> lock(mutex_);

        std::vector<time_series_sample<T>> result;
        result.reserve(count_);

        for (size_t i = 0; i < count_; ++i) {
            result.push_back(buffer_[get_actual_index(i)]);
        }

        std::sort(result.begin(), result.end(),
                  [](const time_series_sample<T>& a, const time_series_sample<T>& b) {
                      return a.timestamp < b.timestamp;
                  });

        return result;
    }

    /**
     * @brief Get statistics for samples within a duration
     * @param duration How far back to look
     * @return Statistics for the samples
     */
    template <typename Duration>
    time_series_statistics get_statistics(Duration duration) const {
        auto samples = get_samples(duration);
        return calculate_statistics(samples);
    }

    /**
     * @brief Get statistics for all samples
     * @return Statistics for all samples
     */
    time_series_statistics get_statistics() const {
        auto samples = get_all_samples();
        return calculate_statistics(samples);
    }

    /**
     * @brief Get the latest sample value
     * @return Result containing the latest value or error
     */
    result<T> get_latest() const {
        std::lock_guard<std::mutex> lock(mutex_);

        if (count_ == 0) {
            return make_error<T>(monitoring_error_code::collection_failed, "No samples available");
        }

        size_t latest_idx = (head_ == 0) ? config_.max_samples - 1 : head_ - 1;
        return make_success(buffer_[latest_idx].value);
    }

    /**
     * @brief Get the latest sample with timestamp
     * @return Result containing the latest sample or error
     */
    result<time_series_sample<T>> get_latest_sample() const {
        std::lock_guard<std::mutex> lock(mutex_);

        if (count_ == 0) {
            return make_error<time_series_sample<T>>(monitoring_error_code::collection_failed,
                                                     "No samples available");
        }

        size_t latest_idx = (head_ == 0) ? config_.max_samples - 1 : head_ - 1;
        return make_success(buffer_[latest_idx]);
    }

    /**
     * @brief Get current number of samples
     */
    size_t size() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_;
    }

    /**
     * @brief Check if buffer is empty
     */
    bool empty() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_ == 0;
    }

    /**
     * @brief Get buffer capacity
     */
    size_t capacity() const noexcept { return config_.max_samples; }

    /**
     * @brief Clear all samples
     */
    void clear() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        head_ = 0;
        count_ = 0;
    }

    /**
     * @brief Get memory footprint in bytes
     */
    size_t memory_footprint() const noexcept {
        return sizeof(time_series_buffer<T>) +
               config_.max_samples * sizeof(time_series_sample<T>);
    }

  private:
    static time_series_statistics calculate_statistics(
        const std::vector<time_series_sample<T>>& samples) {
        if (samples.empty()) {
            time_series_statistics stats;
            stats.sample_count = 0;
            stats.min_value = 0.0;
            stats.max_value = 0.0;
            return stats;
        }

        std::vector<double> values;
        values.reserve(samples.size());
        for (const auto& sample : samples) {
            values.push_back(static_cast<double>(sample.value));
        }

        return detail::calculate_basic_statistics(
            values,
            samples.front().timestamp,
            samples.back().timestamp);
    }
};

/**
 * @struct load_average_sample
 * @brief Sample containing all three load averages
 */
struct load_average_sample {
    std::chrono::system_clock::time_point timestamp;
    double load_1m;
    double load_5m;
    double load_15m;

    load_average_sample() noexcept : load_1m(0.0), load_5m(0.0), load_15m(0.0) {}

    load_average_sample(std::chrono::system_clock::time_point ts, double l1, double l5,
                        double l15) noexcept
        : timestamp(ts), load_1m(l1), load_5m(l5), load_15m(l15) {}
};

/**
 * @struct load_average_statistics
 * @brief Statistics for load average history
 */
struct load_average_statistics {
    time_series_statistics load_1m_stats;
    time_series_statistics load_5m_stats;
    time_series_statistics load_15m_stats;
};

/**
 * @class load_average_history
 * @brief Specialized buffer for tracking load average history
 */
class load_average_history {
  private:
    mutable std::mutex mutex_;
    std::vector<load_average_sample> buffer_;
    size_t head_ = 0;
    size_t count_ = 0;
    size_t max_samples_;

    size_t get_actual_index(size_t logical_index) const noexcept {
        return detail::ring_buffer_index(logical_index, head_, count_, max_samples_);
    }

  public:
    explicit load_average_history(size_t max_samples = 1000) : max_samples_(max_samples) {
        if (max_samples_ == 0) {
            throw std::invalid_argument("Max samples must be positive");
        }
        buffer_.resize(max_samples_);
    }

    load_average_history(const load_average_history&) = delete;
    load_average_history& operator=(const load_average_history&) = delete;
    load_average_history(load_average_history&&) = delete;
    load_average_history& operator=(load_average_history&&) = delete;

    /**
     * @brief Add a load average sample
     * @param load_1m 1-minute load average
     * @param load_5m 5-minute load average
     * @param load_15m 15-minute load average
     * @param timestamp Optional timestamp (defaults to now)
     */
    void add_sample(double load_1m, double load_5m, double load_15m,
                    std::chrono::system_clock::time_point timestamp =
                        std::chrono::system_clock::now()) {
        std::lock_guard<std::mutex> lock(mutex_);

        buffer_[head_] = load_average_sample(timestamp, load_1m, load_5m, load_15m);
        head_ = (head_ + 1) % max_samples_;

        if (count_ < max_samples_) {
            ++count_;
        }
    }

    /**
     * @brief Get samples within a duration from now
     * @param duration How far back to look
     * @return Vector of samples within the duration
     */
    template <typename Duration>
    std::vector<load_average_sample> get_samples(Duration duration) const {
        auto cutoff = std::chrono::system_clock::now() - duration;
        return get_samples_since(cutoff);
    }

    /**
     * @brief Get samples since a specific timestamp
     * @param since The timestamp to start from
     * @return Vector of samples since the timestamp
     */
    std::vector<load_average_sample> get_samples_since(
        std::chrono::system_clock::time_point since) const {
        std::lock_guard<std::mutex> lock(mutex_);

        std::vector<load_average_sample> result;
        result.reserve(count_);

        for (size_t i = 0; i < count_; ++i) {
            const auto& sample = buffer_[get_actual_index(i)];
            if (sample.timestamp >= since) {
                result.push_back(sample);
            }
        }

        std::sort(result.begin(), result.end(),
                  [](const load_average_sample& a, const load_average_sample& b) {
                      return a.timestamp < b.timestamp;
                  });

        return result;
    }

    /**
     * @brief Get all samples in chronological order
     * @return Vector of all samples
     */
    std::vector<load_average_sample> get_all_samples() const {
        std::lock_guard<std::mutex> lock(mutex_);

        std::vector<load_average_sample> result;
        result.reserve(count_);

        for (size_t i = 0; i < count_; ++i) {
            result.push_back(buffer_[get_actual_index(i)]);
        }

        std::sort(result.begin(), result.end(),
                  [](const load_average_sample& a, const load_average_sample& b) {
                      return a.timestamp < b.timestamp;
                  });

        return result;
    }

    /**
     * @brief Get statistics for samples within a duration
     * @param duration How far back to look
     * @return Statistics for the samples
     */
    template <typename Duration>
    load_average_statistics get_statistics(Duration duration) const {
        auto samples = get_samples(duration);
        return calculate_statistics(samples);
    }

    /**
     * @brief Get statistics for all samples
     * @return Statistics for all samples
     */
    load_average_statistics get_statistics() const {
        auto samples = get_all_samples();
        return calculate_statistics(samples);
    }

    /**
     * @brief Get the latest sample
     * @return Result containing the latest sample or error
     */
    result<load_average_sample> get_latest() const {
        std::lock_guard<std::mutex> lock(mutex_);

        if (count_ == 0) {
            return make_error<load_average_sample>(monitoring_error_code::collection_failed,
                                                   "No samples available");
        }

        size_t latest_idx = (head_ == 0) ? max_samples_ - 1 : head_ - 1;
        return make_success(buffer_[latest_idx]);
    }

    /**
     * @brief Get current number of samples
     */
    size_t size() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_;
    }

    /**
     * @brief Check if buffer is empty
     */
    bool empty() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_ == 0;
    }

    /**
     * @brief Get buffer capacity
     */
    size_t capacity() const noexcept { return max_samples_; }

    /**
     * @brief Clear all samples
     */
    void clear() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        head_ = 0;
        count_ = 0;
    }

    /**
     * @brief Get memory footprint in bytes
     */
    size_t memory_footprint() const noexcept {
        return sizeof(load_average_history) + max_samples_ * sizeof(load_average_sample);
    }

  private:
    static load_average_statistics calculate_statistics(
        const std::vector<load_average_sample>& samples) {
        load_average_statistics stats;

        if (samples.empty()) {
            stats.load_1m_stats.sample_count = 0;
            stats.load_1m_stats.min_value = 0.0;
            stats.load_1m_stats.max_value = 0.0;
            stats.load_5m_stats = stats.load_1m_stats;
            stats.load_15m_stats = stats.load_1m_stats;
            return stats;
        }

        std::vector<double> values_1m, values_5m, values_15m;
        values_1m.reserve(samples.size());
        values_5m.reserve(samples.size());
        values_15m.reserve(samples.size());

        for (const auto& sample : samples) {
            values_1m.push_back(sample.load_1m);
            values_5m.push_back(sample.load_5m);
            values_15m.push_back(sample.load_15m);
        }

        auto oldest = samples.front().timestamp;
        auto newest = samples.back().timestamp;

        stats.load_1m_stats = detail::calculate_basic_statistics(values_1m, oldest, newest);
        stats.load_5m_stats = detail::calculate_basic_statistics(values_5m, oldest, newest);
        stats.load_15m_stats = detail::calculate_basic_statistics(values_15m, oldest, newest);

        return stats;
    }
};

}}  // namespace kcenon::monitoring
