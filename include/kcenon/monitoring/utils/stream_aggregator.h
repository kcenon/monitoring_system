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

#pragma once

/**
 * @file stream_aggregator.h
 * @brief Streaming statistical aggregation for real-time metrics
 *
 * This file provides streaming aggregation capabilities including:
 * - online_statistics: Welford's algorithm for streaming statistics
 * - quantile_estimator: P² algorithm for streaming quantile estimation
 * - moving_window_aggregator: Time-windowed value collection
 * - stream_aggregator: Full-featured streaming aggregation
 */

#include "../core/result_types.h"
#include "../core/error_codes.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <deque>
#include <map>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <vector>

namespace kcenon::monitoring {

/**
 * @struct streaming_statistics
 * @brief Statistical summary from streaming computation
 */
struct streaming_statistics {
    size_t count = 0;
    double mean = 0.0;
    double variance = 0.0;
    double std_deviation = 0.0;
    double min_value = 0.0;
    double max_value = 0.0;
    double sum = 0.0;
    size_t outlier_count = 0;
    std::vector<double> outliers;
    std::map<double, double> percentiles;
};

/**
 * @class online_statistics
 * @brief Welford's algorithm for computing streaming statistics
 *
 * This class computes running mean, variance, and standard deviation
 * using Welford's numerically stable online algorithm.
 */
class online_statistics {
public:
    online_statistics() = default;

    /**
     * @brief Add a value to the statistics
     * @param value The value to add
     */
    void add_value(double value) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        count_++;
        sum_ += value;

        double delta = value - mean_;
        mean_ += delta / static_cast<double>(count_);
        double delta2 = value - mean_;
        m2_ += delta * delta2;

        if (count_ == 1) {
            min_value_ = value;
            max_value_ = value;
        } else {
            min_value_ = std::min(min_value_, value);
            max_value_ = std::max(max_value_, value);
        }
    }

    /**
     * @brief Get sample count
     */
    size_t count() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return count_;
    }

    /**
     * @brief Get running mean
     */
    double mean() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return mean_;
    }

    /**
     * @brief Get running variance (sample variance)
     */
    double variance() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        if (count_ < 2) {
            return 0.0;
        }
        return m2_ / static_cast<double>(count_ - 1);
    }

    /**
     * @brief Get running standard deviation
     */
    double stddev() const {
        return std::sqrt(variance());
    }

    /**
     * @brief Get minimum value
     */
    double min() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return min_value_;
    }

    /**
     * @brief Get maximum value
     */
    double max() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return max_value_;
    }

    /**
     * @brief Get sum of all values
     */
    double sum() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return sum_;
    }

    /**
     * @brief Get full statistics
     */
    streaming_statistics get_statistics() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        streaming_statistics stats;
        stats.count = count_;
        stats.mean = mean_;
        stats.sum = sum_;
        stats.min_value = min_value_;
        stats.max_value = max_value_;

        if (count_ >= 2) {
            stats.variance = m2_ / static_cast<double>(count_ - 1);
            stats.std_deviation = std::sqrt(stats.variance);
        }

        return stats;
    }

    /**
     * @brief Reset statistics
     */
    void reset() {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        count_ = 0;
        mean_ = 0.0;
        m2_ = 0.0;
        sum_ = 0.0;
        min_value_ = 0.0;
        max_value_ = 0.0;
    }

private:
    mutable std::shared_mutex mutex_;
    size_t count_ = 0;
    double mean_ = 0.0;
    double m2_ = 0.0;
    double sum_ = 0.0;
    double min_value_ = 0.0;
    double max_value_ = 0.0;
};

/**
 * @class quantile_estimator
 * @brief P² algorithm for streaming quantile estimation
 *
 * Implements the P² algorithm for estimating quantiles without
 * storing all observations. Uses piecewise-parabolic interpolation.
 */
class quantile_estimator {
public:
    /**
     * @brief Constructor
     * @param p The quantile to estimate (0.0 to 1.0)
     */
    explicit quantile_estimator(double p) : p_(p) {
        init_markers();
    }

    // Enable move construction and assignment
    quantile_estimator(quantile_estimator&& other) noexcept
        : p_(other.p_)
        , count_(other.count_) {
        std::copy(std::begin(other.q_), std::end(other.q_), std::begin(q_));
        std::copy(std::begin(other.n_), std::end(other.n_), std::begin(n_));
        std::copy(std::begin(other.n_prime_), std::end(other.n_prime_), std::begin(n_prime_));
        std::copy(std::begin(other.dn_), std::end(other.dn_), std::begin(dn_));
    }

    quantile_estimator& operator=(quantile_estimator&& other) noexcept {
        if (this != &other) {
            p_ = other.p_;
            count_ = other.count_;
            std::copy(std::begin(other.q_), std::end(other.q_), std::begin(q_));
            std::copy(std::begin(other.n_), std::end(other.n_), std::begin(n_));
            std::copy(std::begin(other.n_prime_), std::end(other.n_prime_), std::begin(n_prime_));
            std::copy(std::begin(other.dn_), std::end(other.dn_), std::begin(dn_));
        }
        return *this;
    }

    // Disable copy
    quantile_estimator(const quantile_estimator&) = delete;
    quantile_estimator& operator=(const quantile_estimator&) = delete;

private:
    void init_markers() {
        // Initialize marker positions
        n_[0] = 1;
        n_[1] = 2;
        n_[2] = 3;
        n_[3] = 4;
        n_[4] = 5;

        // Initialize desired marker positions
        n_prime_[0] = 1;
        n_prime_[1] = 1 + 2 * p_;
        n_prime_[2] = 1 + 4 * p_;
        n_prime_[3] = 3 + 2 * p_;
        n_prime_[4] = 5;

        // Increments
        dn_[0] = 0;
        dn_[1] = p_ / 2;
        dn_[2] = p_;
        dn_[3] = (1 + p_) / 2;
        dn_[4] = 1;
    }

public:

    /**
     * @brief Add an observation
     * @param x The observation value
     */
    void add_observation(double x) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        count_++;

        if (count_ <= 5) {
            q_[count_ - 1] = x;
            if (count_ == 5) {
                std::sort(q_, q_ + 5);
            }
            return;
        }

        // Find cell k
        int k;
        if (x < q_[0]) {
            q_[0] = x;
            k = 0;
        } else if (x >= q_[4]) {
            q_[4] = x;
            k = 3;
        } else {
            for (k = 0; k < 4; ++k) {
                if (x < q_[k + 1]) {
                    break;
                }
            }
        }

        // Increment positions
        for (int i = k + 1; i < 5; ++i) {
            n_[i]++;
        }

        // Update desired positions
        for (int i = 0; i < 5; ++i) {
            n_prime_[i] += dn_[i];
        }

        // Adjust marker heights
        for (int i = 1; i < 4; ++i) {
            double d = n_prime_[i] - n_[i];
            if ((d >= 1 && n_[i + 1] - n_[i] > 1) ||
                (d <= -1 && n_[i - 1] - n_[i] < -1)) {
                int sign = (d >= 0) ? 1 : -1;
                double q_new = parabolic(i, sign);

                if (q_[i - 1] < q_new && q_new < q_[i + 1]) {
                    q_[i] = q_new;
                } else {
                    q_[i] = linear(i, sign);
                }
                n_[i] += sign;
            }
        }
    }

    /**
     * @brief Get the estimated quantile
     * @return The estimated quantile value
     */
    double get_quantile() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        if (count_ < 5) {
            if (count_ == 0) {
                return 0.0;
            }
            // For small samples, use simple interpolation
            std::vector<double> sorted(q_, q_ + count_);
            std::sort(sorted.begin(), sorted.end());
            size_t idx = static_cast<size_t>(p_ * (count_ - 1));
            return sorted[idx];
        }
        return q_[2];  // Middle marker for p-quantile
    }

    /**
     * @brief Get observation count
     */
    size_t count() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return count_;
    }

    /**
     * @brief Reset the estimator
     */
    void reset() {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        count_ = 0;
        n_[0] = 1; n_[1] = 2; n_[2] = 3; n_[3] = 4; n_[4] = 5;
        n_prime_[0] = 1;
        n_prime_[1] = 1 + 2 * p_;
        n_prime_[2] = 1 + 4 * p_;
        n_prime_[3] = 3 + 2 * p_;
        n_prime_[4] = 5;
    }

private:
    double parabolic(int i, int sign) const {
        double qi = q_[i];
        double qim1 = q_[i - 1];
        double qip1 = q_[i + 1];
        int ni = n_[i];
        int nim1 = n_[i - 1];
        int nip1 = n_[i + 1];

        double term1 = static_cast<double>(sign) / (nip1 - nim1);
        double term2 = (ni - nim1 + sign) * (qip1 - qi) / (nip1 - ni);
        double term3 = (nip1 - ni - sign) * (qi - qim1) / (ni - nim1);

        return qi + term1 * (term2 + term3);
    }

    double linear(int i, int sign) const {
        int idx = (sign < 0) ? i - 1 : i + 1;
        return q_[i] + static_cast<double>(sign) * (q_[idx] - q_[i]) /
               (n_[idx] - n_[i]);
    }

    mutable std::shared_mutex mutex_;
    double p_;
    size_t count_ = 0;
    double q_[5] = {0};
    int n_[5] = {0};
    double n_prime_[5] = {0};
    double dn_[5] = {0};
};

/**
 * @class moving_window_aggregator
 * @brief Time-windowed value collection
 *
 * Maintains a sliding window of values with automatic expiration.
 *
 * @tparam T The value type
 */
template<typename T>
class moving_window_aggregator {
public:
    using time_point = std::chrono::system_clock::time_point;
    using duration = std::chrono::system_clock::duration;

    /**
     * @brief Constructor
     * @param window_duration Duration of the sliding window
     * @param max_size Maximum number of elements
     */
    moving_window_aggregator(std::chrono::milliseconds window_duration,
                              size_t max_size)
        : window_duration_(window_duration)
        , max_size_(max_size) {}

    /**
     * @brief Add a value with timestamp
     * @param value The value to add
     * @param timestamp The timestamp
     */
    void add_value(const T& value, time_point timestamp) {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        // Remove expired entries
        expire_old_entries(timestamp);

        // Add new entry
        if (entries_.size() >= max_size_) {
            entries_.pop_front();
        }
        entries_.push_back({value, timestamp});
    }

    /**
     * @brief Get current size
     */
    size_t size() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return entries_.size();
    }

    /**
     * @brief Check if empty
     */
    bool empty() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return entries_.empty();
    }

    /**
     * @brief Get all values in the window
     */
    std::vector<T> get_values() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        std::vector<T> result;
        result.reserve(entries_.size());
        for (const auto& entry : entries_) {
            result.push_back(entry.value);
        }
        return result;
    }

    /**
     * @brief Clear all entries
     */
    void clear() {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        entries_.clear();
    }

private:
    struct entry {
        T value;
        time_point timestamp;
    };

    void expire_old_entries(time_point current) {
        auto cutoff = current - window_duration_;
        while (!entries_.empty() && entries_.front().timestamp < cutoff) {
            entries_.pop_front();
        }
    }

    mutable std::shared_mutex mutex_;
    std::chrono::milliseconds window_duration_;
    size_t max_size_;
    std::deque<entry> entries_;
};

/**
 * @struct stream_aggregator_config
 * @brief Configuration for stream aggregator
 */
struct stream_aggregator_config {
    size_t window_size = 10000;
    std::chrono::milliseconds window_duration{60000};
    bool enable_outlier_detection = true;
    double outlier_threshold = 3.0;  // Standard deviations
    std::vector<double> percentiles_to_track = {0.5, 0.9, 0.95, 0.99};

    /**
     * @brief Validate configuration
     */
    common::VoidResult validate() const {
        if (window_size == 0) {
            return common::VoidResult::err(error_info(monitoring_error_code::invalid_configuration,
                                   "Window size must be positive").to_common_error());
        }
        if (window_duration.count() <= 0) {
            return common::VoidResult::err(error_info(monitoring_error_code::invalid_configuration,
                                   "Window duration must be positive").to_common_error());
        }
        if (outlier_threshold <= 0) {
            return common::VoidResult::err(error_info(monitoring_error_code::invalid_configuration,
                                   "Outlier threshold must be positive").to_common_error());
        }
        return common::ok();
    }
};

/**
 * @class stream_aggregator
 * @brief Full-featured streaming aggregation
 *
 * Combines online statistics, quantile estimation, and outlier detection
 * for comprehensive streaming metric aggregation.
 */
class stream_aggregator {
public:
    /**
     * @brief Default constructor
     */
    stream_aggregator() : stream_aggregator(stream_aggregator_config{}) {}

    /**
     * @brief Constructor with configuration
     * @param config The configuration
     */
    explicit stream_aggregator(const stream_aggregator_config& config)
        : config_(config) {
        // Initialize percentile estimators
        for (double p : config_.percentiles_to_track) {
            percentile_estimators_.emplace(p, quantile_estimator(p));
        }
    }

    /**
     * @brief Add an observation
     * @param value The observation value
     * @return Result indicating success or failure
     */
    common::VoidResult add_observation(double value) {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        // Check for outlier
        if (config_.enable_outlier_detection && stats_.count() > 10) {
            double z_score = std::abs(value - stats_.mean()) /
                            (stats_.stddev() + 1e-10);
            if (z_score > config_.outlier_threshold) {
                outlier_count_++;
                outliers_.push_back(value);
                if (outliers_.size() > 100) {
                    outliers_.erase(outliers_.begin());
                }
            }
        }

        stats_.add_value(value);

        // Update percentile estimators
        for (auto& [p, estimator] : percentile_estimators_) {
            estimator.add_observation(value);
        }

        return common::ok();
    }

    /**
     * @brief Get full statistics
     */
    streaming_statistics get_statistics() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto stats = stats_.get_statistics();
        stats.outlier_count = outlier_count_;
        stats.outliers = outliers_;

        // Add percentiles
        for (const auto& [p, estimator] : percentile_estimators_) {
            stats.percentiles[p] = estimator.get_quantile();
        }

        return stats;
    }

    /**
     * @brief Get specific percentile
     * @param p The percentile (0.0 to 1.0)
     * @return Optional containing the percentile value
     */
    std::optional<double> get_percentile(double p) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = percentile_estimators_.find(p);
        if (it != percentile_estimators_.end()) {
            return it->second.get_quantile();
        }
        return std::nullopt;
    }

    /**
     * @brief Get observation count
     */
    size_t count() const {
        return stats_.count();
    }

    /**
     * @brief Get mean
     */
    double mean() const {
        return stats_.mean();
    }

    /**
     * @brief Get variance
     */
    double variance() const {
        return stats_.variance();
    }

    /**
     * @brief Get standard deviation
     */
    double stddev() const {
        return stats_.stddev();
    }

    /**
     * @brief Reset the aggregator
     */
    void reset() {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        stats_.reset();
        outlier_count_ = 0;
        outliers_.clear();
        for (auto& [p, estimator] : percentile_estimators_) {
            estimator.reset();
        }
    }

private:
    mutable std::shared_mutex mutex_;
    stream_aggregator_config config_;
    online_statistics stats_;
    std::map<double, quantile_estimator> percentile_estimators_;
    size_t outlier_count_ = 0;
    std::vector<double> outliers_;
};

/**
 * @brief Calculate Pearson correlation coefficient
 * @param x First data series
 * @param y Second data series
 * @return Correlation coefficient (-1 to 1), 0 if sizes differ
 */
inline double pearson_correlation(const std::vector<double>& x,
                                   const std::vector<double>& y) {
    if (x.size() != y.size() || x.size() < 2) {
        return 0.0;
    }

    size_t n = x.size();
    double sum_x = 0, sum_y = 0, sum_xy = 0;
    double sum_x2 = 0, sum_y2 = 0;

    for (size_t i = 0; i < n; ++i) {
        sum_x += x[i];
        sum_y += y[i];
        sum_xy += x[i] * y[i];
        sum_x2 += x[i] * x[i];
        sum_y2 += y[i] * y[i];
    }

    double num = n * sum_xy - sum_x * sum_y;
    double den = std::sqrt((n * sum_x2 - sum_x * sum_x) *
                           (n * sum_y2 - sum_y * sum_y));

    if (den < 1e-10) {
        return 0.0;
    }

    return num / den;
}

} // namespace kcenon::monitoring
