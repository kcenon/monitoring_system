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

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <string>
#include <thread>

#include "kcenon/monitoring/core/result_types.h"

namespace kcenon::monitoring {

/**
 * @brief Retry strategies
 */
enum class retry_strategy {
    fixed_delay,
    exponential_backoff,
    linear_backoff,
    fibonacci_backoff
};

/**
 * @brief Retry metrics
 */
struct retry_metrics {
    size_t total_executions = 0;
    size_t successful_executions = 0;
    size_t failed_executions = 0;
    size_t total_retries = 0;

    void reset() {
        total_executions = 0;
        successful_executions = 0;
        failed_executions = 0;
        total_retries = 0;
    }
};

/**
 * @brief Retry configuration
 */
struct retry_config {
    size_t max_attempts = 3;
    retry_strategy strategy = retry_strategy::exponential_backoff;
    std::chrono::milliseconds initial_delay = std::chrono::milliseconds(1000);
    std::chrono::milliseconds max_delay = std::chrono::milliseconds(30000);
    double backoff_multiplier = 2.0;
    std::function<bool(const error_info&)> should_retry = nullptr;

    /**
     * @brief Validate configuration
     * @return true if configuration is valid
     */
    bool validate() const {
        if (max_attempts == 0) {
            return false;
        }
        if (backoff_multiplier < 1.0) {
            return false;
        }
        return true;
    }
};

/**
 * @brief Factory function for exponential backoff config
 * @param max_attempts Maximum retry attempts
 * @param initial_delay Initial delay between retries
 * @return retry_config configured for exponential backoff
 */
inline retry_config create_exponential_backoff_config(
    size_t max_attempts = 3,
    std::chrono::milliseconds initial_delay = std::chrono::milliseconds(1000)) {
    retry_config config;
    config.max_attempts = max_attempts;
    config.strategy = retry_strategy::exponential_backoff;
    config.initial_delay = initial_delay;
    config.backoff_multiplier = 2.0;
    return config;
}

/**
 * @brief Factory function for fixed delay config
 * @param max_attempts Maximum retry attempts
 * @param delay Fixed delay between retries
 * @return retry_config configured for fixed delay
 */
inline retry_config create_fixed_delay_config(
    size_t max_attempts = 3,
    std::chrono::milliseconds delay = std::chrono::milliseconds(1000)) {
    retry_config config;
    config.max_attempts = max_attempts;
    config.strategy = retry_strategy::fixed_delay;
    config.initial_delay = delay;
    return config;
}

/**
 * @brief Factory function for Fibonacci backoff config
 * @param max_attempts Maximum retry attempts
 * @param initial_delay Base delay for Fibonacci sequence
 * @return retry_config configured for Fibonacci backoff
 */
inline retry_config create_fibonacci_backoff_config(
    size_t max_attempts = 3,
    std::chrono::milliseconds initial_delay = std::chrono::milliseconds(1000)) {
    retry_config config;
    config.max_attempts = max_attempts;
    config.strategy = retry_strategy::fibonacci_backoff;
    config.initial_delay = initial_delay;
    return config;
}

/**
 * @brief Retry executor template class
 *
 * Executes operations with configurable retry logic.
 *
 * @tparam T The return value type of operations
 */
template<typename T>
class retry_executor {
public:
    using config = retry_config;

    retry_executor() : name_("default"), config_() {}

    explicit retry_executor(const std::string& name)
        : name_(name), config_() {}

    explicit retry_executor(const std::string& name, const config& cfg)
        : name_(name), config_(cfg) {}

    /**
     * @brief Execute a function with retry logic
     *
     * @tparam Func The function type to execute (must return common::Result<T>)
     * @param func The function to execute
     * @return common::Result<T> containing success value or error from last attempt
     */
    template<typename Func>
    common::Result<T> execute(Func&& func) {
        metrics_.total_executions++;

        common::Result<T> last_result = common::Result<T>::err(error_info(monitoring_error_code::operation_failed, "No attempts made").to_common_error());

        for (size_t attempt = 0; attempt < config_.max_attempts; ++attempt) {
            if (attempt > 0) {
                metrics_.total_retries++;
                auto delay = calculate_delay(attempt);
                std::this_thread::sleep_for(delay);
            }

            last_result = func();

            if (last_result.is_ok()) {
                metrics_.successful_executions++;
                return last_result;
            }

            if (config_.should_retry) {
                auto err_info = error_info::from_common_error(last_result.error());
                if (!config_.should_retry(err_info)) {
                    metrics_.failed_executions++;
                    return last_result;
                }
            }
        }

        metrics_.failed_executions++;
        return last_result;
    }

    /**
     * @brief Get retry metrics
     */
    retry_metrics get_metrics() const {
        return metrics_;
    }

    /**
     * @brief Reset metrics
     */
    void reset_metrics() {
        metrics_.reset();
    }

    /**
     * @brief Get executor name
     */
    const std::string& get_name() const {
        return name_;
    }

private:
    /**
     * @brief Calculate delay for the current attempt
     * @param attempt Current attempt number (0-indexed)
     * @return Delay duration
     */
    std::chrono::milliseconds calculate_delay(size_t attempt) const {
        std::chrono::milliseconds delay;

        switch (config_.strategy) {
            case retry_strategy::fixed_delay:
                delay = config_.initial_delay;
                break;

            case retry_strategy::exponential_backoff: {
                auto multiplier = std::pow(config_.backoff_multiplier, static_cast<double>(attempt - 1));
                auto delay_ms = static_cast<long long>(config_.initial_delay.count() * multiplier);
                delay = std::chrono::milliseconds(delay_ms);
                break;
            }

            case retry_strategy::linear_backoff:
                delay = config_.initial_delay * attempt;
                break;

            case retry_strategy::fibonacci_backoff: {
                delay = config_.initial_delay * fibonacci(attempt);
                break;
            }

            default:
                delay = config_.initial_delay;
                break;
        }

        return std::min(delay, config_.max_delay);
    }

    /**
     * @brief Calculate Fibonacci number
     * @param n Index in Fibonacci sequence
     * @return Fibonacci number at index n
     */
    static size_t fibonacci(size_t n) {
        if (n <= 1) return 1;

        size_t prev = 1;
        size_t curr = 1;

        for (size_t i = 2; i <= n; ++i) {
            size_t next = prev + curr;
            prev = curr;
            curr = next;
        }

        return curr;
    }

    std::string name_;
    config config_;
    mutable retry_metrics metrics_;
};

/**
 * @brief Basic retry policy implementation (backward compatibility)
 */
class retry_policy {
public:
    using config = retry_config;

    retry_policy() : config_() {}
    explicit retry_policy(const config& cfg) : config_(cfg) {}

    template<typename Func>
    auto execute(Func&& func) -> decltype(func()) {
        return func();
    }

private:
    config config_;
};

} // namespace kcenon::monitoring
