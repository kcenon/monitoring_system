// BSD 3-Clause License
//
// Copyright (c) 2021-2025, kcenon
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
 * @file alert_triggers.h
 * @brief Alert trigger implementations for various condition types
 *
 * This file provides concrete trigger implementations including threshold-based,
 * rate-of-change, anomaly detection, and composite triggers.
 */

#include <algorithm>
#include <cmath>
#include <deque>
#include <memory>
#include <mutex>
#include <numeric>
#include <string>
#include <vector>

#include "alert_rule.h"

namespace kcenon::monitoring {

/**
 * @enum comparison_operator
 * @brief Comparison operators for threshold triggers
 */
enum class comparison_operator {
    greater_than,       ///< value > threshold
    greater_or_equal,   ///< value >= threshold
    less_than,          ///< value < threshold
    less_or_equal,      ///< value <= threshold
    equal,              ///< value == threshold (with epsilon)
    not_equal           ///< value != threshold (with epsilon)
};

/**
 * @brief Convert comparison operator to string
 */
constexpr const char* comparison_operator_to_string(comparison_operator op) noexcept {
    switch (op) {
        case comparison_operator::greater_than:     return ">";
        case comparison_operator::greater_or_equal: return ">=";
        case comparison_operator::less_than:        return "<";
        case comparison_operator::less_or_equal:    return "<=";
        case comparison_operator::equal:            return "==";
        case comparison_operator::not_equal:        return "!=";
        default:                                    return "?";
    }
}

/**
 * @class threshold_trigger
 * @brief Trigger based on comparing value against a threshold
 *
 * The most common trigger type, comparing metric values against
 * configured thresholds using various comparison operators.
 *
 * @example
 * @code
 * // Alert when CPU > 80%
 * auto trigger = threshold_trigger::above(80.0);
 *
 * // Alert when memory < 10%
 * auto trigger = threshold_trigger::below(10.0);
 *
 * // Alert when error rate >= 5%
 * auto trigger = std::make_shared<threshold_trigger>(
 *     5.0, comparison_operator::greater_or_equal);
 * @endcode
 */
class threshold_trigger : public alert_trigger {
public:
    /**
     * @brief Construct a threshold trigger
     * @param threshold The threshold value
     * @param op Comparison operator
     * @param epsilon Epsilon for floating-point comparison (default 1e-9)
     */
    explicit threshold_trigger(double threshold,
                               comparison_operator op = comparison_operator::greater_than,
                               double epsilon = 1e-9)
        : threshold_(threshold)
        , operator_(op)
        , epsilon_(epsilon) {}

    bool evaluate(double value) const override {
        switch (operator_) {
            case comparison_operator::greater_than:
                return value > threshold_;
            case comparison_operator::greater_or_equal:
                return value >= threshold_ - epsilon_;
            case comparison_operator::less_than:
                return value < threshold_;
            case comparison_operator::less_or_equal:
                return value <= threshold_ + epsilon_;
            case comparison_operator::equal:
                return std::abs(value - threshold_) <= epsilon_;
            case comparison_operator::not_equal:
                return std::abs(value - threshold_) > epsilon_;
            default:
                return false;
        }
    }

    std::string type_name() const override {
        return "threshold";
    }

    std::string description() const override {
        return "value " + std::string(comparison_operator_to_string(operator_)) +
               " " + std::to_string(threshold_);
    }

    /**
     * @brief Get the threshold value
     */
    double threshold() const { return threshold_; }

    /**
     * @brief Get the comparison operator
     */
    comparison_operator op() const { return operator_; }

    // Factory methods for common cases
    /**
     * @brief Create trigger for value > threshold
     */
    static std::shared_ptr<threshold_trigger> above(double threshold) {
        return std::make_shared<threshold_trigger>(threshold, comparison_operator::greater_than);
    }

    /**
     * @brief Create trigger for value >= threshold
     */
    static std::shared_ptr<threshold_trigger> above_or_equal(double threshold) {
        return std::make_shared<threshold_trigger>(threshold, comparison_operator::greater_or_equal);
    }

    /**
     * @brief Create trigger for value < threshold
     */
    static std::shared_ptr<threshold_trigger> below(double threshold) {
        return std::make_shared<threshold_trigger>(threshold, comparison_operator::less_than);
    }

    /**
     * @brief Create trigger for value <= threshold
     */
    static std::shared_ptr<threshold_trigger> below_or_equal(double threshold) {
        return std::make_shared<threshold_trigger>(threshold, comparison_operator::less_or_equal);
    }

    /**
     * @brief Create trigger for value within range (inclusive)
     */
    static std::shared_ptr<class range_trigger> in_range(double min_val, double max_val);

    /**
     * @brief Create trigger for value outside range (exclusive)
     */
    static std::shared_ptr<class range_trigger> out_of_range(double min_val, double max_val);

private:
    double threshold_;
    comparison_operator operator_;
    double epsilon_;
};

/**
 * @class range_trigger
 * @brief Trigger based on value being within or outside a range
 */
class range_trigger : public alert_trigger {
public:
    /**
     * @brief Construct a range trigger
     * @param min_value Minimum value of range
     * @param max_value Maximum value of range
     * @param inside_range True to trigger when inside, false when outside
     */
    range_trigger(double min_value, double max_value, bool inside_range)
        : min_value_(min_value)
        , max_value_(max_value)
        , inside_range_(inside_range) {}

    bool evaluate(double value) const override {
        bool in_range = (value >= min_value_ && value <= max_value_);
        return inside_range_ ? in_range : !in_range;
    }

    std::string type_name() const override {
        return "range";
    }

    std::string description() const override {
        if (inside_range_) {
            return "value in [" + std::to_string(min_value_) + ", " +
                   std::to_string(max_value_) + "]";
        }
        return "value outside [" + std::to_string(min_value_) + ", " +
               std::to_string(max_value_) + "]";
    }

private:
    double min_value_;
    double max_value_;
    bool inside_range_;
};

inline std::shared_ptr<range_trigger> threshold_trigger::in_range(double min_val, double max_val) {
    return std::make_shared<range_trigger>(min_val, max_val, true);
}

inline std::shared_ptr<range_trigger> threshold_trigger::out_of_range(double min_val, double max_val) {
    return std::make_shared<range_trigger>(min_val, max_val, false);
}

/**
 * @class rate_of_change_trigger
 * @brief Trigger based on rate of change of values
 *
 * Monitors how quickly a metric value is changing and triggers
 * when the rate exceeds a threshold. Useful for detecting rapid
 * increases or decreases in metrics.
 *
 * @example
 * @code
 * // Alert when CPU increases by more than 20% per minute
 * auto trigger = std::make_shared<rate_of_change_trigger>(
 *     20.0,  // threshold
 *     std::chrono::minutes(1),  // time window
 *     rate_direction::increasing
 * );
 * @endcode
 */
class rate_of_change_trigger : public alert_trigger {
public:
    /**
     * @enum rate_direction
     * @brief Direction of rate change to monitor
     */
    enum class rate_direction {
        increasing,     ///< Positive rate of change
        decreasing,     ///< Negative rate of change
        either          ///< Absolute rate of change
    };

    /**
     * @brief Construct a rate of change trigger
     * @param rate_threshold Rate threshold per time window
     * @param window Time window for rate calculation
     * @param direction Direction to monitor
     * @param min_samples Minimum samples before triggering
     */
    rate_of_change_trigger(double rate_threshold,
                           std::chrono::milliseconds window,
                           rate_direction direction = rate_direction::either,
                           size_t min_samples = 2)
        : rate_threshold_(rate_threshold)
        , window_(window)
        , direction_(direction)
        , min_samples_(min_samples) {}

    bool evaluate(double value) const override {
        auto now = std::chrono::steady_clock::now();

        std::lock_guard<std::mutex> lock(mutex_);

        // Add new sample
        samples_.push_back({value, now});

        // Remove old samples outside window
        auto cutoff = now - window_;
        while (!samples_.empty() && samples_.front().timestamp < cutoff) {
            samples_.pop_front();
        }

        // Need minimum samples to calculate rate
        if (samples_.size() < min_samples_) {
            return false;
        }

        // Calculate rate of change
        double rate = calculate_rate();

        switch (direction_) {
            case rate_direction::increasing:
                return rate > rate_threshold_;
            case rate_direction::decreasing:
                return rate < -rate_threshold_;
            case rate_direction::either:
                return std::abs(rate) > rate_threshold_;
            default:
                return false;
        }
    }

    std::string type_name() const override {
        return "rate_of_change";
    }

    std::string description() const override {
        std::string dir_str;
        switch (direction_) {
            case rate_direction::increasing: dir_str = "increase"; break;
            case rate_direction::decreasing: dir_str = "decrease"; break;
            case rate_direction::either: dir_str = "change"; break;
        }
        return dir_str + " rate > " + std::to_string(rate_threshold_) +
               " per " + std::to_string(window_.count()) + "ms";
    }

    /**
     * @brief Clear accumulated samples
     */
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        samples_.clear();
    }

private:
    struct sample {
        double value;
        std::chrono::steady_clock::time_point timestamp;
    };

    double calculate_rate() const {
        if (samples_.size() < 2) {
            return 0.0;
        }

        // Use linear regression for smoother rate calculation
        double sum_x = 0.0, sum_y = 0.0, sum_xy = 0.0, sum_xx = 0.0;
        auto base_time = samples_.front().timestamp;

        for (const auto& s : samples_) {
            double x = std::chrono::duration<double, std::milli>(
                s.timestamp - base_time).count();
            double y = s.value;
            sum_x += x;
            sum_y += y;
            sum_xy += x * y;
            sum_xx += x * x;
        }

        double n = static_cast<double>(samples_.size());
        double denominator = n * sum_xx - sum_x * sum_x;

        if (std::abs(denominator) < 1e-10) {
            return 0.0;
        }

        // Slope is rate of change per millisecond
        double slope = (n * sum_xy - sum_x * sum_y) / denominator;

        // Convert to rate per window
        return slope * static_cast<double>(window_.count());
    }

    double rate_threshold_;
    std::chrono::milliseconds window_;
    rate_direction direction_;
    size_t min_samples_;

    mutable std::mutex mutex_;
    mutable std::deque<sample> samples_;
};

/**
 * @class anomaly_trigger
 * @brief Trigger based on statistical anomaly detection
 *
 * Uses statistical methods to detect values that deviate significantly
 * from normal behavior. Maintains a sliding window of historical values
 * and triggers when the current value exceeds a configurable number of
 * standard deviations from the mean.
 *
 * @example
 * @code
 * // Alert when value is more than 3 standard deviations from mean
 * auto trigger = std::make_shared<anomaly_trigger>(
 *     3.0,   // sensitivity (number of std devs)
 *     100    // window size for historical data
 * );
 * @endcode
 */
class anomaly_trigger : public alert_trigger {
public:
    /**
     * @brief Construct an anomaly trigger
     * @param sensitivity Number of standard deviations for anomaly
     * @param window_size Number of samples for baseline calculation
     * @param min_samples Minimum samples before detection starts
     */
    explicit anomaly_trigger(double sensitivity = 3.0,
                             size_t window_size = 100,
                             size_t min_samples = 10)
        : sensitivity_(sensitivity)
        , window_size_(window_size)
        , min_samples_(min_samples) {}

    bool evaluate(double value) const override {
        std::lock_guard<std::mutex> lock(mutex_);

        // Add to history
        if (history_.size() >= window_size_) {
            history_.pop_front();
        }
        history_.push_back(value);

        // Need minimum samples for statistics
        if (history_.size() < min_samples_) {
            return false;
        }

        // Calculate statistics
        double mean_val = mean();
        double stddev = standard_deviation(mean_val);

        // Avoid division by zero or very small stddev
        if (stddev < 1e-10) {
            return false;
        }

        // Calculate z-score
        double z_score = std::abs(value - mean_val) / stddev;

        return z_score > sensitivity_;
    }

    std::string type_name() const override {
        return "anomaly";
    }

    std::string description() const override {
        return "value > " + std::to_string(sensitivity_) + " std devs from mean";
    }

    /**
     * @brief Get current mean of historical values
     */
    double current_mean() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return mean();
    }

    /**
     * @brief Get current standard deviation
     */
    double current_stddev() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return standard_deviation(mean());
    }

    /**
     * @brief Clear historical data
     */
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        history_.clear();
    }

private:
    double mean() const {
        if (history_.empty()) {
            return 0.0;
        }
        double sum = std::accumulate(history_.begin(), history_.end(), 0.0);
        return sum / static_cast<double>(history_.size());
    }

    double standard_deviation(double mean_val) const {
        if (history_.size() < 2) {
            return 0.0;
        }
        double sq_sum = 0.0;
        for (double val : history_) {
            double diff = val - mean_val;
            sq_sum += diff * diff;
        }
        return std::sqrt(sq_sum / static_cast<double>(history_.size() - 1));
    }

    double sensitivity_;
    size_t window_size_;
    size_t min_samples_;

    mutable std::mutex mutex_;
    mutable std::deque<double> history_;
};

/**
 * @enum composite_operation
 * @brief Logical operations for combining triggers
 */
enum class composite_operation {
    AND,    ///< All triggers must fire
    OR,     ///< Any trigger fires
    XOR,    ///< Exactly one trigger fires
    NOT     ///< Invert single trigger (uses first trigger only)
};

/**
 * @class composite_trigger
 * @brief Combines multiple triggers with logical operations
 *
 * Allows building complex alert conditions by combining simpler
 * triggers using AND, OR, XOR, and NOT operations.
 *
 * @example
 * @code
 * // Alert when CPU > 80% AND memory > 90%
 * auto cpu_trigger = threshold_trigger::above(80.0);
 * auto mem_trigger = threshold_trigger::above(90.0);
 * auto composite = std::make_shared<composite_trigger>(
 *     composite_operation::AND,
 *     std::vector<std::shared_ptr<alert_trigger>>{cpu_trigger, mem_trigger}
 * );
 *
 * // For evaluation, use evaluate_multi() with multiple values
 * @endcode
 */
class composite_trigger : public alert_trigger {
public:
    /**
     * @brief Construct a composite trigger
     * @param op Logical operation
     * @param triggers Child triggers
     */
    composite_trigger(composite_operation op,
                      std::vector<std::shared_ptr<alert_trigger>> triggers)
        : operation_(op)
        , triggers_(std::move(triggers)) {}

    /**
     * @brief Evaluate with a single value (applies to all triggers)
     */
    bool evaluate(double value) const override {
        std::vector<double> values(triggers_.size(), value);
        return evaluate_multi(values);
    }

    /**
     * @brief Evaluate with multiple values (one per trigger)
     * @param values Values for each trigger in order
     * @return True if composite condition is met
     */
    bool evaluate_multi(const std::vector<double>& values) const {
        if (triggers_.empty()) {
            return false;
        }

        std::vector<bool> results;
        results.reserve(triggers_.size());

        for (size_t i = 0; i < triggers_.size(); ++i) {
            double val = (i < values.size()) ? values[i] : values.back();
            results.push_back(triggers_[i]->evaluate(val));
        }

        switch (operation_) {
            case composite_operation::AND:
                return std::all_of(results.begin(), results.end(),
                                  [](bool b) { return b; });
            case composite_operation::OR:
                return std::any_of(results.begin(), results.end(),
                                  [](bool b) { return b; });
            case composite_operation::XOR: {
                size_t count = std::count(results.begin(), results.end(), true);
                return count == 1;
            }
            case composite_operation::NOT:
                return !results.front();
            default:
                return false;
        }
    }

    std::string type_name() const override {
        return "composite";
    }

    std::string description() const override {
        std::string op_str;
        switch (operation_) {
            case composite_operation::AND: op_str = " AND "; break;
            case composite_operation::OR:  op_str = " OR ";  break;
            case composite_operation::XOR: op_str = " XOR "; break;
            case composite_operation::NOT: return "NOT (" + triggers_.front()->description() + ")";
        }

        std::string result = "(";
        for (size_t i = 0; i < triggers_.size(); ++i) {
            if (i > 0) {
                result += op_str;
            }
            result += triggers_[i]->description();
        }
        result += ")";
        return result;
    }

    /**
     * @brief Get child triggers
     */
    const std::vector<std::shared_ptr<alert_trigger>>& triggers() const {
        return triggers_;
    }

    // Factory methods
    /**
     * @brief Create AND composite
     */
    static std::shared_ptr<composite_trigger> all_of(
            std::vector<std::shared_ptr<alert_trigger>> triggers) {
        return std::make_shared<composite_trigger>(composite_operation::AND,
                                                   std::move(triggers));
    }

    /**
     * @brief Create OR composite
     */
    static std::shared_ptr<composite_trigger> any_of(
            std::vector<std::shared_ptr<alert_trigger>> triggers) {
        return std::make_shared<composite_trigger>(composite_operation::OR,
                                                   std::move(triggers));
    }

    /**
     * @brief Create NOT composite
     */
    static std::shared_ptr<composite_trigger> invert(
            std::shared_ptr<alert_trigger> trigger) {
        return std::make_shared<composite_trigger>(composite_operation::NOT,
                                                   std::vector<std::shared_ptr<alert_trigger>>{std::move(trigger)});
    }

private:
    composite_operation operation_;
    std::vector<std::shared_ptr<alert_trigger>> triggers_;
};

/**
 * @class absent_trigger
 * @brief Trigger when no data is received for a period
 *
 * Useful for detecting when a metric stops being reported,
 * indicating a potential issue with the monitored service.
 */
class absent_trigger : public alert_trigger {
public:
    /**
     * @brief Construct an absent trigger
     * @param absent_duration Duration without data before triggering
     */
    explicit absent_trigger(std::chrono::milliseconds absent_duration)
        : absent_duration_(absent_duration) {}

    bool evaluate(double /*value*/) const override {
        auto now = std::chrono::steady_clock::now();

        std::lock_guard<std::mutex> lock(mutex_);

        auto previous = last_seen_;
        last_seen_ = now;

        // First evaluation - not absent yet
        if (previous == std::chrono::steady_clock::time_point{}) {
            return false;
        }

        // Check if the gap since previous value exceeds threshold
        return (now - previous) > absent_duration_;
    }

    std::string type_name() const override {
        return "absent";
    }

    std::string description() const override {
        return "no data for " + std::to_string(absent_duration_.count()) + "ms";
    }

    /**
     * @brief Reset last seen timestamp
     */
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        last_seen_ = std::chrono::steady_clock::time_point{};
    }

private:
    std::chrono::milliseconds absent_duration_;
    mutable std::mutex mutex_;
    mutable std::chrono::steady_clock::time_point last_seen_{};
};

/**
 * @class delta_trigger
 * @brief Trigger based on change from previous value
 *
 * Fires when the difference between current and previous value
 * exceeds a threshold.
 */
class delta_trigger : public alert_trigger {
public:
    /**
     * @brief Construct a delta trigger
     * @param delta_threshold Minimum change to trigger
     * @param absolute Use absolute difference (true) or signed (false)
     */
    explicit delta_trigger(double delta_threshold, bool absolute = true)
        : delta_threshold_(delta_threshold)
        , absolute_(absolute) {}

    bool evaluate(double value) const override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!has_previous_) {
            previous_value_ = value;
            has_previous_ = true;
            return false;
        }

        double delta = value - previous_value_;
        previous_value_ = value;

        if (absolute_) {
            return std::abs(delta) > delta_threshold_;
        }
        return delta > delta_threshold_;
    }

    std::string type_name() const override {
        return "delta";
    }

    std::string description() const override {
        if (absolute_) {
            return "|delta| > " + std::to_string(delta_threshold_);
        }
        return "delta > " + std::to_string(delta_threshold_);
    }

    /**
     * @brief Reset previous value
     */
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        has_previous_ = false;
    }

private:
    double delta_threshold_;
    bool absolute_;
    mutable std::mutex mutex_;
    mutable double previous_value_ = 0.0;
    mutable bool has_previous_ = false;
};

} // namespace kcenon::monitoring
