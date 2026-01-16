// BSD 3-Clause License
//
// Copyright (c) 2021-2025, monitoring_system contributors
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
 * @file statistics.h
 * @brief Generic statistics utilities for percentile calculations
 * @date 2025
 *
 * Provides reusable statistics calculation functions that can work with
 * any numeric type, including std::chrono::nanoseconds and double.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <type_traits>
#include <vector>

namespace kcenon {
namespace monitoring {
namespace stats {

/**
 * @struct statistics
 * @brief Statistical summary for a collection of values
 *
 * @tparam T The value type (must support arithmetic operations)
 */
template <typename T>
struct statistics {
    T min;
    T max;
    T mean;
    T median;
    T p95;
    T p99;
    T total;
    size_t count;
};

namespace detail {

/**
 * @brief Type trait to detect std::chrono::duration types
 */
template <typename T>
struct is_chrono_duration : std::false_type {};

template <typename Rep, typename Period>
struct is_chrono_duration<std::chrono::duration<Rep, Period>> : std::true_type {};

template <typename T>
inline constexpr bool is_chrono_duration_v = is_chrono_duration<T>::value;

/**
 * @brief Get zero value for a type
 */
template <typename T>
constexpr T zero_value() {
    if constexpr (is_chrono_duration_v<T>) {
        return T::zero();
    } else {
        return T{0};
    }
}

/**
 * @brief Get maximum value for a type
 */
template <typename T>
constexpr T max_value() {
    if constexpr (is_chrono_duration_v<T>) {
        return T::max();
    } else {
        return std::numeric_limits<T>::max();
    }
}

/**
 * @brief Get minimum (lowest) value for a type
 */
template <typename T>
constexpr T min_value() {
    if constexpr (is_chrono_duration_v<T>) {
        return T::min();
    } else {
        return std::numeric_limits<T>::lowest();
    }
}

/**
 * @brief Divide value by count
 */
template <typename T>
T divide(const T& value, size_t count) {
    if (count == 0) {
        return zero_value<T>();
    }
    if constexpr (is_chrono_duration_v<T>) {
        return value / static_cast<typename T::rep>(count);
    } else {
        return value / static_cast<T>(count);
    }
}

}  // namespace detail

/**
 * @brief Calculate percentile from sorted values
 *
 * @tparam T Value type
 * @param sorted_values Vector of sorted values
 * @param percentile Percentile to calculate (0-100)
 * @return The value at the given percentile
 *
 * @note The input must be sorted in ascending order
 *
 * @example
 * @code
 * std::vector<double> values = {1.0, 2.0, 3.0, 4.0, 5.0};
 * double p50 = percentile(values, 50.0);  // Returns 3.0
 * double p95 = percentile(values, 95.0);  // Returns approximately 4.8
 * @endcode
 */
template <typename T>
T percentile(const std::vector<T>& sorted_values, double percentile_value) {
    if (sorted_values.empty()) {
        return detail::zero_value<T>();
    }

    if (percentile_value <= 0.0) {
        return sorted_values.front();
    }

    if (percentile_value >= 100.0) {
        return sorted_values.back();
    }

    double rank = (percentile_value / 100.0) * (sorted_values.size() - 1);
    size_t lower_idx = static_cast<size_t>(rank);
    size_t upper_idx = lower_idx + 1;

    if (upper_idx >= sorted_values.size()) {
        return sorted_values[lower_idx];
    }

    // For chrono types, use simple index-based selection
    // For numeric types, use linear interpolation
    if constexpr (detail::is_chrono_duration_v<T>) {
        // Use nearest-rank method for duration types
        return sorted_values[static_cast<size_t>(std::round(rank))];
    } else {
        double fraction = rank - static_cast<double>(lower_idx);
        return sorted_values[lower_idx] +
               static_cast<T>(fraction * (sorted_values[upper_idx] - sorted_values[lower_idx]));
    }
}

/**
 * @brief Compute statistics from sorted values
 *
 * This is more efficient when you already have sorted data.
 *
 * @tparam T Value type
 * @param sorted_values Vector of sorted values (ascending order)
 * @return statistics<T> containing all computed statistics
 */
template <typename T>
statistics<T> compute_sorted(const std::vector<T>& sorted_values) {
    statistics<T> result{};

    if (sorted_values.empty()) {
        result.min = detail::zero_value<T>();
        result.max = detail::zero_value<T>();
        result.mean = detail::zero_value<T>();
        result.median = detail::zero_value<T>();
        result.p95 = detail::zero_value<T>();
        result.p99 = detail::zero_value<T>();
        result.total = detail::zero_value<T>();
        result.count = 0;
        return result;
    }

    result.count = sorted_values.size();
    result.min = sorted_values.front();
    result.max = sorted_values.back();

    // Calculate total
    result.total = std::accumulate(sorted_values.begin(), sorted_values.end(),
                                   detail::zero_value<T>());

    // Calculate mean
    result.mean = detail::divide(result.total, result.count);

    // Calculate percentiles
    result.median = percentile(sorted_values, 50.0);
    result.p95 = percentile(sorted_values, 95.0);
    result.p99 = percentile(sorted_values, 99.0);

    return result;
}

/**
 * @brief Compute statistics from unsorted values
 *
 * This function will sort a copy of the input values.
 *
 * @tparam T Value type
 * @param values Vector of values (will be copied and sorted)
 * @return statistics<T> containing all computed statistics
 *
 * @example
 * @code
 * std::vector<std::chrono::nanoseconds> durations = {
 *     std::chrono::nanoseconds(100),
 *     std::chrono::nanoseconds(200),
 *     std::chrono::nanoseconds(300)
 * };
 * auto stats = compute(durations);
 * // stats.mean == 200ns, stats.median == 200ns
 * @endcode
 */
template <typename T>
statistics<T> compute(const std::vector<T>& values) {
    if (values.empty()) {
        return compute_sorted<T>({});
    }

    std::vector<T> sorted = values;
    std::sort(sorted.begin(), sorted.end());
    return compute_sorted(sorted);
}

/**
 * @brief Compute statistics in place (modifies input)
 *
 * This is more efficient when you don't need to preserve the original order.
 *
 * @tparam T Value type
 * @param values Vector of values (will be sorted in place)
 * @return statistics<T> containing all computed statistics
 */
template <typename T>
statistics<T> compute_inplace(std::vector<T>& values) {
    if (values.empty()) {
        return compute_sorted<T>({});
    }

    std::sort(values.begin(), values.end());
    return compute_sorted(values);
}

}  // namespace stats
}  // namespace monitoring
}  // namespace kcenon
