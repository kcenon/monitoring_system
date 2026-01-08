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
#include <mutex>
#include <stdexcept>
#include <string>

#include "kcenon/monitoring/core/result_types.h"

namespace kcenon::monitoring {

/**
 * @brief Circuit breaker states
 *
 * State machine:
 *   closed ─[failures >= threshold]─> open
 *   open ─[reset_timeout elapsed]─> half_open
 *   half_open ─[success]─> closed
 *   half_open ─[failure]─> open
 */
enum class circuit_state {
    closed,     ///< Normal operation, requests are allowed
    open,       ///< Circuit is open, requests are rejected
    half_open   ///< Testing state, limited requests allowed
};

/**
 * @brief Circuit breaker configuration
 */
struct circuit_breaker_config {
    size_t failure_threshold = 5;       ///< Number of failures before opening
    std::chrono::milliseconds timeout = std::chrono::milliseconds(30000);   ///< Call timeout
    std::chrono::milliseconds reset_timeout = std::chrono::milliseconds(60000);  ///< Time before half_open
    size_t success_threshold = 3;       ///< Successes in half_open to close

    /**
     * @brief Validate configuration
     * @return true if configuration is valid
     */
    bool validate() const {
        if (failure_threshold == 0) {
            return false;
        }
        if (success_threshold == 0) {
            return false;
        }
        return true;
    }
};

/**
 * @brief Circuit breaker metrics
 */
struct circuit_breaker_metrics {
    std::atomic<size_t> total_calls{0};
    std::atomic<size_t> successful_calls{0};
    std::atomic<size_t> failed_calls{0};
    std::atomic<size_t> rejected_calls{0};
    std::atomic<size_t> state_transitions{0};

    circuit_breaker_metrics() = default;
    circuit_breaker_metrics(const circuit_breaker_metrics& other)
        : total_calls(other.total_calls.load())
        , successful_calls(other.successful_calls.load())
        , failed_calls(other.failed_calls.load())
        , rejected_calls(other.rejected_calls.load())
        , state_transitions(other.state_transitions.load()) {}

    /**
     * @brief Get success rate
     * @return Success rate between 0.0 and 1.0
     */
    double get_success_rate() const {
        auto total = total_calls.load();
        if (total == 0) {
            return 1.0;
        }
        return static_cast<double>(successful_calls.load()) / static_cast<double>(total);
    }
};

/**
 * @brief Exception thrown when circuit breaker is open
 */
class circuit_open_exception : public std::runtime_error {
public:
    explicit circuit_open_exception(const std::string& name)
        : std::runtime_error("Circuit breaker '" + name + "' is open") {}
};

/**
 * @brief Thread-safe circuit breaker implementation
 *
 * Implements the circuit breaker pattern to prevent cascading failures.
 * When a service experiences repeated failures, the circuit "opens" and
 * subsequent requests fail fast without attempting the operation.
 *
 * @tparam T The return value type of operations
 */
template<typename T = void>
class circuit_breaker {
public:
    using config = circuit_breaker_config;
    using clock = std::chrono::steady_clock;

    circuit_breaker() : name_("default"), config_() {}

    explicit circuit_breaker(const std::string& name)
        : name_(name), config_() {}

    explicit circuit_breaker(const std::string& name, const config& cfg)
        : name_(name), config_(cfg) {}

    /**
     * @brief Execute a function with circuit breaker protection and fallback
     *
     * @tparam Func The function type to execute (must return result<T>)
     * @tparam Fallback The fallback function type
     * @param func The function to execute
     * @param fallback The fallback function to execute on failure or open circuit
     * @return result<T> containing success value or error
     */
    template<typename Func, typename Fallback>
    result<T> execute(Func&& func, Fallback&& fallback) {
        metrics_.total_calls++;

        auto current_state = check_state();

        if (current_state == circuit_state::open) {
            metrics_.rejected_calls++;
            return fallback();
        }

        auto op_result = func();
        if (op_result.is_ok()) {
            on_success();
            return op_result;
        } else {
            on_failure();
            return fallback();
        }
    }

    /**
     * @brief Execute a function with circuit breaker protection
     *
     * @tparam Func The function type to execute (must return result<T>)
     * @param func The function to execute
     * @return result<T> containing success value or error
     */
    template<typename Func>
    result<T> execute(Func&& func) {
        metrics_.total_calls++;

        auto current_state = check_state();

        if (current_state == circuit_state::open) {
            metrics_.rejected_calls++;
            return make_error<T>(monitoring_error_code::circuit_breaker_open,
                               "Circuit breaker '" + name_ + "' is open");
        }

        auto op_result = func();
        if (op_result.is_ok()) {
            on_success();
            return op_result;
        } else {
            on_failure();
            return op_result;
        }
    }

    /**
     * @brief Get current circuit state
     */
    circuit_state get_state() const {
        return state_.load(std::memory_order_acquire);
    }

    /**
     * @brief Get current failure count
     */
    size_t get_failure_count() const {
        return failure_count_.load(std::memory_order_acquire);
    }

    /**
     * @brief Get circuit breaker metrics
     */
    circuit_breaker_metrics get_metrics() const {
        return metrics_;
    }

    /**
     * @brief Get circuit breaker name
     */
    const std::string& get_name() const {
        return name_;
    }

    /**
     * @brief Manually reset circuit to closed state
     */
    void reset() {
        std::lock_guard<std::mutex> lock(state_mutex_);
        transition_to(circuit_state::closed);
        failure_count_.store(0, std::memory_order_release);
        consecutive_successes_.store(0, std::memory_order_release);
    }

private:
    /**
     * @brief Check current state and handle timeout transitions
     */
    circuit_state check_state() {
        auto current = state_.load(std::memory_order_acquire);

        if (current == circuit_state::open) {
            auto now = clock::now();
            auto opened_at = last_failure_time_.load(std::memory_order_acquire);

            if (now - opened_at >= config_.reset_timeout) {
                std::lock_guard<std::mutex> lock(state_mutex_);
                if (state_.load(std::memory_order_acquire) == circuit_state::open) {
                    transition_to(circuit_state::half_open);
                    consecutive_successes_.store(0, std::memory_order_release);
                    return circuit_state::half_open;
                }
            }
        }

        return state_.load(std::memory_order_acquire);
    }

    /**
     * @brief Handle successful execution
     */
    void on_success() {
        metrics_.successful_calls++;

        auto current = state_.load(std::memory_order_acquire);

        if (current == circuit_state::half_open) {
            auto successes = consecutive_successes_.fetch_add(1, std::memory_order_acq_rel) + 1;

            if (successes >= config_.success_threshold) {
                std::lock_guard<std::mutex> lock(state_mutex_);
                if (state_.load(std::memory_order_acquire) == circuit_state::half_open) {
                    transition_to(circuit_state::closed);
                    failure_count_.store(0, std::memory_order_release);
                    consecutive_successes_.store(0, std::memory_order_release);
                }
            }
        } else if (current == circuit_state::closed) {
            failure_count_.store(0, std::memory_order_release);
        }
    }

    /**
     * @brief Handle failed execution
     */
    void on_failure() {
        metrics_.failed_calls++;
        last_failure_time_.store(clock::now(), std::memory_order_release);

        auto current = state_.load(std::memory_order_acquire);

        if (current == circuit_state::half_open) {
            std::lock_guard<std::mutex> lock(state_mutex_);
            if (state_.load(std::memory_order_acquire) == circuit_state::half_open) {
                transition_to(circuit_state::open);
                consecutive_successes_.store(0, std::memory_order_release);
            }
        } else if (current == circuit_state::closed) {
            auto failures = failure_count_.fetch_add(1, std::memory_order_acq_rel) + 1;

            if (failures >= config_.failure_threshold) {
                std::lock_guard<std::mutex> lock(state_mutex_);
                if (state_.load(std::memory_order_acquire) == circuit_state::closed &&
                    failure_count_.load(std::memory_order_acquire) >= config_.failure_threshold) {
                    transition_to(circuit_state::open);
                }
            }
        }
    }

    /**
     * @brief Transition to a new state (must be called with state_mutex_ held)
     */
    void transition_to(circuit_state new_state) {
        auto old_state = state_.load(std::memory_order_acquire);
        if (old_state != new_state) {
            state_.store(new_state, std::memory_order_release);
            metrics_.state_transitions++;
        }
    }

    std::string name_;
    config config_;

    std::atomic<size_t> failure_count_{0};
    std::atomic<size_t> consecutive_successes_{0};
    std::atomic<circuit_state> state_{circuit_state::closed};
    std::atomic<clock::time_point> last_failure_time_{clock::now()};

    mutable std::mutex state_mutex_;
    mutable circuit_breaker_metrics metrics_;
};

} // namespace kcenon::monitoring
