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

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

#include "kcenon/monitoring/core/result_types.h"

namespace kcenon::monitoring {

/**
 * @brief Degradation levels for error boundary
 */
enum class degradation_level {
    normal = 0,
    limited = 1,
    minimal = 2,
    emergency = 3
};

/**
 * @brief Error boundary policies
 */
enum class error_boundary_policy {
    fail_fast,
    isolate,
    degrade,
    fallback
};

/**
 * @brief Error boundary metrics with atomic counters
 */
struct error_boundary_metrics {
    std::atomic<size_t> total_operations{0};
    std::atomic<size_t> successful_operations{0};
    std::atomic<size_t> failed_operations{0};
    std::atomic<size_t> recovered_operations{0};
    std::atomic<size_t> recovery_attempts{0};

    error_boundary_metrics() = default;

    error_boundary_metrics(const error_boundary_metrics& other)
        : total_operations(other.total_operations.load())
        , successful_operations(other.successful_operations.load())
        , failed_operations(other.failed_operations.load())
        , recovered_operations(other.recovered_operations.load())
        , recovery_attempts(other.recovery_attempts.load()) {}

    error_boundary_metrics& operator=(const error_boundary_metrics& other) {
        if (this != &other) {
            total_operations = other.total_operations.load();
            successful_operations = other.successful_operations.load();
            failed_operations = other.failed_operations.load();
            recovered_operations = other.recovered_operations.load();
            recovery_attempts = other.recovery_attempts.load();
        }
        return *this;
    }

    double get_success_rate() const {
        size_t total = total_operations.load();
        if (total == 0) {
            return 1.0;
        }
        return static_cast<double>(successful_operations.load()) / static_cast<double>(total);
    }
};

/**
 * @brief Error boundary configuration
 */
struct error_boundary_config {
    std::string name;
    size_t error_threshold = 5;
    std::chrono::seconds error_window = std::chrono::seconds(60);
    bool enable_fallback_logging = true;
    degradation_level max_degradation = degradation_level::emergency;
    error_boundary_policy policy = error_boundary_policy::fail_fast;
    bool enable_automatic_recovery = false;
    std::chrono::milliseconds recovery_timeout = std::chrono::milliseconds(5000);

    bool validate() const {
        if (name.empty()) {
            return false;
        }
        if (error_threshold == 0) {
            return false;
        }
        return true;
    }
};

/**
 * @brief Base interface for fallback strategies
 */
template<typename T>
class fallback_strategy_interface {
public:
    virtual ~fallback_strategy_interface() = default;
    virtual result<T> get_fallback(const error_info& err, degradation_level level) = 0;
};

/**
 * @brief Default value fallback strategy
 */
template<typename T>
class default_value_strategy : public fallback_strategy_interface<T> {
public:
    explicit default_value_strategy(T default_val) : default_value_(std::move(default_val)) {}

    result<T> get_fallback(const error_info& /*err*/, degradation_level /*level*/) override {
        return make_success(default_value_);
    }

private:
    T default_value_;
};

/**
 * @brief Cached value fallback strategy
 */
template<typename T>
class cached_value_strategy : public fallback_strategy_interface<T> {
public:
    explicit cached_value_strategy(std::chrono::seconds ttl = std::chrono::seconds(60))
        : ttl_(ttl), has_value_(false) {}

    void update_cache(const T& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        cached_value_ = value;
        cache_time_ = std::chrono::steady_clock::now();
        has_value_ = true;
    }

    result<T> get_fallback(const error_info& /*err*/, degradation_level /*level*/) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!has_value_) {
            return make_error<T>(monitoring_error_code::operation_failed, "No cached value available");
        }

        auto now = std::chrono::steady_clock::now();
        auto age = std::chrono::duration_cast<std::chrono::seconds>(now - cache_time_);
        if (age > ttl_) {
            return make_error<T>(monitoring_error_code::operation_failed, "Cached value expired");
        }

        return make_success(cached_value_);
    }

private:
    std::chrono::seconds ttl_;
    T cached_value_;
    std::chrono::steady_clock::time_point cache_time_;
    bool has_value_;
    mutable std::mutex mutex_;
};

/**
 * @brief Alternative service fallback strategy
 */
template<typename T>
class alternative_service_strategy : public fallback_strategy_interface<T> {
public:
    using alternative_func = std::function<result<T>()>;

    explicit alternative_service_strategy(alternative_func func) : alternative_func_(std::move(func)) {}

    result<T> get_fallback(const error_info& /*err*/, degradation_level /*level*/) override {
        if (alternative_func_) {
            return alternative_func_();
        }
        return make_error<T>(monitoring_error_code::operation_failed, "No alternative service available");
    }

private:
    alternative_func alternative_func_;
};

/**
 * @brief Error boundary implementation for resilient operations
 */
template<typename T>
class error_boundary {
public:
    using config = error_boundary_config;

    error_boundary() : name_("default"), config_() {}

    explicit error_boundary(const std::string& name) : name_(name), config_() {
        config_.name = name;
    }

    explicit error_boundary(const std::string& name, const config& cfg)
        : name_(name), config_(cfg) {
        config_.name = name;
    }

    /**
     * @brief Execute a function within the error boundary
     */
    template<typename Func>
    auto execute(Func&& func) -> result<T> {
        metrics_.total_operations++;

        try {
            auto op_result = func();

            if (op_result.is_ok()) {
                metrics_.successful_operations++;
                handle_success();
                return op_result;
            }

            metrics_.failed_operations++;
            return handle_failure(op_result.error());

        } catch (const std::exception& e) {
            metrics_.failed_operations++;
            error_info err(monitoring_error_code::operation_failed, e.what());
            return handle_failure(err.to_common_error());
        } catch (...) {
            metrics_.failed_operations++;
            error_info err(monitoring_error_code::operation_failed, "Unknown exception");
            return handle_failure(err.to_common_error());
        }
    }

    /**
     * @brief Execute with custom fallback function
     */
    template<typename Func, typename FallbackFunc>
    auto execute(Func&& func, FallbackFunc&& fallback) -> result<T> {
        metrics_.total_operations++;

        try {
            auto op_result = func();

            if (op_result.is_ok()) {
                metrics_.successful_operations++;
                handle_success();
                return op_result;
            }

            metrics_.failed_operations++;
            error_info err = error_info::from_common_error(op_result.error());
            return fallback(err, current_degradation_level_);

        } catch (const std::exception& e) {
            metrics_.failed_operations++;
            error_info err(monitoring_error_code::operation_failed, e.what());
            return fallback(err, current_degradation_level_);
        }
    }

    /**
     * @brief Set error handler callback
     */
    void set_error_handler(std::function<void(const error_info&, degradation_level)> handler) {
        error_handler_ = std::move(handler);
    }

    /**
     * @brief Set fallback strategy
     */
    void set_fallback_strategy(std::shared_ptr<fallback_strategy_interface<T>> strategy) {
        fallback_strategy_ = std::move(strategy);
    }

    /**
     * @brief Get current degradation level
     */
    degradation_level get_degradation_level() const {
        return current_degradation_level_;
    }

    /**
     * @brief Force degradation to a specific level
     */
    void force_degradation(degradation_level level) {
        if (level <= config_.max_degradation) {
            current_degradation_level_ = level;
        }
    }

    /**
     * @brief Check if the boundary is healthy
     */
    result<bool> is_healthy() const {
        bool healthy = (current_degradation_level_ == degradation_level::normal);
        return make_success(healthy);
    }

    /**
     * @brief Get metrics
     */
    error_boundary_metrics get_metrics() const {
        return metrics_;
    }

    /**
     * @brief Get boundary name
     */
    const std::string& get_name() const {
        return name_;
    }

private:
    result<T> handle_failure(const common::error_info& err) {
        consecutive_failures_++;

        // Check if we should degrade
        if (config_.policy == error_boundary_policy::degrade &&
            consecutive_failures_ >= config_.error_threshold) {
            upgrade_degradation();
        }

        // Call error handler if set
        if (error_handler_) {
            error_info monitoring_err = error_info::from_common_error(err);
            error_handler_(monitoring_err, current_degradation_level_);
        }

        // Apply policy
        switch (config_.policy) {
            case error_boundary_policy::fail_fast:
                return result<T>::err(err);

            case error_boundary_policy::isolate:
                return make_error<T>(monitoring_error_code::service_degraded,
                                    "Service isolated due to error");

            case error_boundary_policy::fallback:
                if (fallback_strategy_) {
                    error_info monitoring_err = error_info::from_common_error(err);
                    return fallback_strategy_->get_fallback(monitoring_err, current_degradation_level_);
                }
                return result<T>::err(err);

            case error_boundary_policy::degrade:
            default:
                return result<T>::err(err);
        }
    }

    void handle_success() {
        consecutive_failures_ = 0;

        // Check for recovery
        if (config_.enable_automatic_recovery &&
            current_degradation_level_ != degradation_level::normal) {
            metrics_.recovery_attempts++;
            downgrade_degradation();
        }
    }

    void upgrade_degradation() {
        auto current = static_cast<int>(current_degradation_level_);
        auto max_level = static_cast<int>(config_.max_degradation);
        if (current < max_level) {
            current_degradation_level_ = static_cast<degradation_level>(current + 1);
        }
    }

    void downgrade_degradation() {
        auto current = static_cast<int>(current_degradation_level_);
        if (current > 0) {
            current_degradation_level_ = static_cast<degradation_level>(current - 1);
        }
    }

    std::string name_;
    config config_;
    std::function<void(const error_info&, degradation_level)> error_handler_;
    std::shared_ptr<fallback_strategy_interface<T>> fallback_strategy_;
    mutable error_boundary_metrics metrics_;
    degradation_level current_degradation_level_ = degradation_level::normal;
    size_t consecutive_failures_ = 0;
};

} // namespace kcenon::monitoring
