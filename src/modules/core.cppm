// BSD 3-Clause License
//
// Copyright (c) 2021-2025, kcenon
// All rights reserved.
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
 * @file core.cppm
 * @brief Core partition for kcenon.monitoring module
 *
 * This module partition provides core monitoring interfaces, types, and
 * fundamental utilities for the monitoring system.
 *
 * Contents:
 * - Error codes and result types
 * - Core monitoring interfaces
 * - Event bus and dispatching
 * - Central collector and thread-local buffer
 * - Performance monitor
 * - Utility types (metrics, time series)
 * - C++20 concepts
 */
module;

// Standard library includes (global module fragment)
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

export module kcenon.monitoring.core;

// Module imports for dependent modules
// Note: These imports require the dependent modules to be built first
// import kcenon.common;  // Required - Tier 0 (uncomment when available)
// import kcenon.thread;  // Required - Tier 1 (uncomment when available)

// ============================================================================
// Core Types
// ============================================================================

export namespace kcenon::monitoring {

/**
 * @enum monitoring_error_code
 * @brief Error codes for monitoring operations
 */
enum class monitoring_error_code {
    success = 0,
    invalid_configuration,
    collector_not_found,
    metric_not_found,
    storage_error,
    export_error,
    timeout,
    resource_exhausted,
    invalid_state,
    not_initialized,
    already_initialized,
    internal_error,
    invalid_argument,
    unsupported_operation,
    permission_denied,
    network_error,
    serialization_error
};

/**
 * @brief Convert error code to string representation
 * @param code The error code to convert
 * @return String representation of the error code
 */
constexpr std::string_view error_code_to_string(monitoring_error_code code) noexcept {
    switch (code) {
        case monitoring_error_code::success: return "success";
        case monitoring_error_code::invalid_configuration: return "invalid_configuration";
        case monitoring_error_code::collector_not_found: return "collector_not_found";
        case monitoring_error_code::metric_not_found: return "metric_not_found";
        case monitoring_error_code::storage_error: return "storage_error";
        case monitoring_error_code::export_error: return "export_error";
        case monitoring_error_code::timeout: return "timeout";
        case monitoring_error_code::resource_exhausted: return "resource_exhausted";
        case monitoring_error_code::invalid_state: return "invalid_state";
        case monitoring_error_code::not_initialized: return "not_initialized";
        case monitoring_error_code::already_initialized: return "already_initialized";
        case monitoring_error_code::internal_error: return "internal_error";
        case monitoring_error_code::invalid_argument: return "invalid_argument";
        case monitoring_error_code::unsupported_operation: return "unsupported_operation";
        case monitoring_error_code::permission_denied: return "permission_denied";
        case monitoring_error_code::network_error: return "network_error";
        case monitoring_error_code::serialization_error: return "serialization_error";
        default: return "unknown_error";
    }
}

// ============================================================================
// Event Types
// ============================================================================

/**
 * @enum event_priority
 * @brief Priority levels for events
 */
enum class event_priority : int {
    critical = 0,
    high = 1,
    normal = 2,
    low = 3,
    background = 4
};

/**
 * @enum event_type
 * @brief Types of monitoring events
 */
enum class event_type {
    metric_collected,
    threshold_exceeded,
    health_check,
    resource_alert,
    custom
};

/**
 * @struct event_data
 * @brief Base structure for event data
 */
struct event_data {
    event_type type;
    event_priority priority;
    std::chrono::system_clock::time_point timestamp;
    std::string source;
    std::string message;
    std::optional<std::string> details;

    event_data()
        : type(event_type::custom)
        , priority(event_priority::normal)
        , timestamp(std::chrono::system_clock::now())
        , source()
        , message()
        , details(std::nullopt) {}

    event_data(event_type t, event_priority p, std::string src, std::string msg)
        : type(t)
        , priority(p)
        , timestamp(std::chrono::system_clock::now())
        , source(std::move(src))
        , message(std::move(msg))
        , details(std::nullopt) {}
};

// ============================================================================
// Metric Types
// ============================================================================

/**
 * @enum metric_type
 * @brief Types of metrics
 */
enum class metric_type {
    counter,
    gauge,
    histogram,
    summary,
    timer
};

/**
 * @brief Metric value variant type
 */
using metric_value = std::variant<
    int64_t,
    uint64_t,
    double,
    std::string
>;

/**
 * @struct metric_sample
 * @brief A single metric sample with timestamp
 */
struct metric_sample {
    std::string name;
    metric_value value;
    std::chrono::system_clock::time_point timestamp;
    std::unordered_map<std::string, std::string> labels;

    metric_sample()
        : name()
        , value(0.0)
        , timestamp(std::chrono::system_clock::now())
        , labels() {}

    metric_sample(std::string n, metric_value v)
        : name(std::move(n))
        , value(std::move(v))
        , timestamp(std::chrono::system_clock::now())
        , labels() {}

    metric_sample(std::string n, metric_value v,
                  std::chrono::system_clock::time_point ts)
        : name(std::move(n))
        , value(std::move(v))
        , timestamp(ts)
        , labels() {}
};

/**
 * @struct metric_descriptor
 * @brief Describes a metric's metadata
 */
struct metric_descriptor {
    std::string name;
    std::string description;
    std::string unit;
    metric_type type;

    metric_descriptor()
        : name()
        , description()
        , unit()
        , type(metric_type::gauge) {}

    metric_descriptor(std::string n, std::string desc,
                      std::string u, metric_type t)
        : name(std::move(n))
        , description(std::move(desc))
        , unit(std::move(u))
        , type(t) {}
};

// ============================================================================
// Performance Types
// ============================================================================

/**
 * @struct performance_sample
 * @brief A performance measurement sample
 */
struct performance_sample {
    std::string operation_name;
    std::chrono::nanoseconds duration;
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point end_time;
    bool success;
    std::optional<std::string> error_message;

    performance_sample()
        : operation_name()
        , duration(0)
        , start_time()
        , end_time()
        , success(true)
        , error_message(std::nullopt) {}
};

/**
 * @struct performance_statistics
 * @brief Aggregated performance statistics
 */
struct performance_statistics {
    std::string operation_name;
    size_t sample_count;
    std::chrono::nanoseconds min_duration;
    std::chrono::nanoseconds max_duration;
    std::chrono::nanoseconds avg_duration;
    std::chrono::nanoseconds p50_duration;
    std::chrono::nanoseconds p95_duration;
    std::chrono::nanoseconds p99_duration;
    size_t success_count;
    size_t failure_count;

    performance_statistics()
        : operation_name()
        , sample_count(0)
        , min_duration(0)
        , max_duration(0)
        , avg_duration(0)
        , p50_duration(0)
        , p95_duration(0)
        , p99_duration(0)
        , success_count(0)
        , failure_count(0) {}
};

// ============================================================================
// C++20 Concepts
// ============================================================================

/**
 * @concept MetricLike
 * @brief Concept for types that behave like metrics
 */
template<typename T>
concept MetricLike = requires(T t) {
    { t.name() } -> std::convertible_to<std::string_view>;
    { t.value() } -> std::convertible_to<metric_value>;
    { t.type() } -> std::convertible_to<metric_type>;
};

/**
 * @concept CollectorLike
 * @brief Concept for types that can collect metrics
 */
template<typename T>
concept CollectorLike = requires(T t) {
    { t.collect() } -> std::same_as<std::vector<metric_sample>>;
    { t.name() } -> std::convertible_to<std::string_view>;
    { t.enabled() } -> std::convertible_to<bool>;
    { t.enable() } -> std::same_as<void>;
    { t.disable() } -> std::same_as<void>;
};

/**
 * @concept EventHandlerLike
 * @brief Concept for types that can handle events
 */
template<typename T>
concept EventHandlerLike = requires(T t, const event_data& e) {
    { t.handle(e) } -> std::same_as<void>;
    { t.can_handle(e.type) } -> std::convertible_to<bool>;
};

/**
 * @concept StorageBackendLike
 * @brief Concept for types that can store metrics
 */
template<typename T>
concept StorageBackendLike = requires(T t, const metric_sample& sample) {
    { t.store(sample) } -> std::same_as<bool>;
    { t.flush() } -> std::same_as<void>;
    { t.capacity() } -> std::convertible_to<size_t>;
};

// ============================================================================
// Time Series Buffer
// ============================================================================

/**
 * @class time_series_buffer
 * @brief A fixed-size ring buffer for time series data
 * @tparam T The type of data to store
 */
template<typename T>
class time_series_buffer {
public:
    using value_type = T;
    using size_type = std::size_t;

    /**
     * @brief Construct a time series buffer
     * @param capacity Maximum number of elements
     */
    explicit time_series_buffer(size_type capacity)
        : buffer_(capacity)
        , capacity_(capacity)
        , head_(0)
        , tail_(0)
        , count_(0) {}

    /**
     * @brief Add a value to the buffer
     * @param value The value to add
     */
    void push(const T& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        buffer_[tail_] = value;
        tail_ = (tail_ + 1) % capacity_;
        if (count_ < capacity_) {
            ++count_;
        } else {
            head_ = (head_ + 1) % capacity_;
        }
    }

    /**
     * @brief Add a value to the buffer (move semantics)
     * @param value The value to add
     */
    void push(T&& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        buffer_[tail_] = std::move(value);
        tail_ = (tail_ + 1) % capacity_;
        if (count_ < capacity_) {
            ++count_;
        } else {
            head_ = (head_ + 1) % capacity_;
        }
    }

    /**
     * @brief Get all values in the buffer
     * @return Vector of all values in order
     */
    std::vector<T> get_all() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<T> result;
        result.reserve(count_);
        for (size_type i = 0; i < count_; ++i) {
            result.push_back(buffer_[(head_ + i) % capacity_]);
        }
        return result;
    }

    /**
     * @brief Get the most recent n values
     * @param n Number of values to get
     * @return Vector of the most recent values
     */
    std::vector<T> get_recent(size_type n) const {
        std::lock_guard<std::mutex> lock(mutex_);
        n = std::min(n, count_);
        std::vector<T> result;
        result.reserve(n);
        size_type start = (tail_ + capacity_ - n) % capacity_;
        for (size_type i = 0; i < n; ++i) {
            result.push_back(buffer_[(start + i) % capacity_]);
        }
        return result;
    }

    /**
     * @brief Clear the buffer
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        head_ = 0;
        tail_ = 0;
        count_ = 0;
    }

    /**
     * @brief Get the current number of elements
     * @return Current element count
     */
    size_type size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_;
    }

    /**
     * @brief Get the buffer capacity
     * @return Maximum capacity
     */
    size_type capacity() const noexcept {
        return capacity_;
    }

    /**
     * @brief Check if the buffer is empty
     * @return true if empty
     */
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_ == 0;
    }

    /**
     * @brief Check if the buffer is full
     * @return true if full
     */
    bool full() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_ == capacity_;
    }

private:
    std::vector<T> buffer_;
    size_type capacity_;
    size_type head_;
    size_type tail_;
    size_type count_;
    mutable std::mutex mutex_;
};

// ============================================================================
// Thread-Local Buffer
// ============================================================================

/**
 * @class thread_local_buffer
 * @brief Lock-free thread-local buffer for metric collection
 * @tparam T The type of data to buffer
 *
 * This class provides a thread-local buffer that allows lock-free
 * metric collection from individual threads, which can then be
 * aggregated by a central collector.
 */
template<typename T>
class thread_local_buffer {
public:
    using value_type = T;
    using size_type = std::size_t;

    /**
     * @brief Construct a thread-local buffer
     * @param local_capacity Capacity of each thread-local buffer
     */
    explicit thread_local_buffer(size_type local_capacity = 1024)
        : local_capacity_(local_capacity) {}

    /**
     * @brief Add a value to the current thread's buffer
     * @param value The value to add
     * @return true if added successfully, false if buffer is full
     */
    bool push(const T& value) {
        auto& local = get_local_buffer();
        if (local.size() >= local_capacity_) {
            return false;
        }
        local.push_back(value);
        return true;
    }

    /**
     * @brief Add a value to the current thread's buffer (move semantics)
     * @param value The value to add
     * @return true if added successfully, false if buffer is full
     */
    bool push(T&& value) {
        auto& local = get_local_buffer();
        if (local.size() >= local_capacity_) {
            return false;
        }
        local.push_back(std::move(value));
        return true;
    }

    /**
     * @brief Drain the current thread's buffer
     * @return Vector of all buffered values
     */
    std::vector<T> drain() {
        auto& local = get_local_buffer();
        std::vector<T> result = std::move(local);
        local.clear();
        return result;
    }

    /**
     * @brief Get the current thread's buffer size
     * @return Number of buffered elements
     */
    size_type size() const {
        return get_local_buffer().size();
    }

    /**
     * @brief Check if the current thread's buffer is empty
     * @return true if empty
     */
    bool empty() const {
        return get_local_buffer().empty();
    }

    /**
     * @brief Get the local buffer capacity
     * @return Local buffer capacity
     */
    size_type local_capacity() const noexcept {
        return local_capacity_;
    }

private:
    std::vector<T>& get_local_buffer() {
        thread_local std::vector<T> buffer;
        buffer.reserve(local_capacity_);
        return buffer;
    }

    const std::vector<T>& get_local_buffer() const {
        thread_local std::vector<T> buffer;
        return buffer;
    }

    size_type local_capacity_;
};

} // namespace kcenon::monitoring
