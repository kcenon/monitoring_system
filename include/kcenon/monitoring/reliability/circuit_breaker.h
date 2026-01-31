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

#include <kcenon/common/resilience/circuit_breaker.h>
#include <kcenon/common/resilience/circuit_state.h>
#include <kcenon/common/resilience/circuit_breaker_config.h>

#include "kcenon/monitoring/core/result_types.h"

#include <memory>

namespace kcenon::monitoring {

// Re-export common_system circuit_state enum for backward compatibility
using circuit_state = common::resilience::circuit_state;

// Legacy enum values for backward compatibility with lowercase names
namespace {
    constexpr auto closed = circuit_state::CLOSED;
    constexpr auto open = circuit_state::OPEN;
    constexpr auto half_open = circuit_state::HALF_OPEN;
}

// Legacy config structure - deprecated in favor of common_system's circuit_breaker_config
struct [[deprecated("Use kcenon::common::resilience::circuit_breaker_config instead")]] circuit_breaker_config {
    size_t failure_threshold = 5;
    std::chrono::milliseconds timeout = std::chrono::milliseconds(30000);
    std::chrono::milliseconds reset_timeout = std::chrono::milliseconds(60000);
    size_t success_threshold = 3;

    bool validate() const {
        return failure_threshold > 0 && success_threshold > 0;
    }

    // Convert to common_system config
    common::resilience::circuit_breaker_config to_common() const {
        common::resilience::circuit_breaker_config config;
        config.failure_threshold = failure_threshold;
        config.timeout = reset_timeout;  // OPEN -> HALF_OPEN transition time
        config.success_threshold = success_threshold;
        config.failure_window = std::chrono::seconds(60);  // Default window
        config.half_open_max_requests = success_threshold + 1;  // Allow enough requests for testing
        return config;
    }
};

// Legacy metrics structure - deprecated
struct [[deprecated("Use circuit_breaker::get_stats() instead")]] circuit_breaker_metrics {
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

    double get_success_rate() const {
        auto total = total_calls.load();
        if (total == 0) {
            return 1.0;
        }
        return static_cast<double>(successful_calls.load()) / static_cast<double>(total);
    }
};

// Legacy exception - deprecated
class [[deprecated("Circuit breaker now returns Result<T> instead of throwing")]] circuit_open_exception : public std::runtime_error {
public:
    explicit circuit_open_exception(const std::string& name)
        : std::runtime_error("Circuit breaker '" + name + "' is open") {}
};

/**
 * @brief Adapter class for common_system circuit_breaker with monitoring_system interface
 *
 * This class wraps common_system's circuit_breaker and provides the execute() interface
 * that monitoring_system components expect. It translates between the allow_request/record_*
 * pattern and the functional execute() pattern.
 *
 * @tparam T The return value type of operations
 *
 * @deprecated This adapter is temporary. Migrate to common::resilience::circuit_breaker directly.
 */
template<typename T = void>
class [[deprecated("Migrate to kcenon::common::resilience::circuit_breaker directly")]] circuit_breaker {
public:
    using config = circuit_breaker_config;
    using clock = std::chrono::steady_clock;

    circuit_breaker()
        : name_("default")
        , config_()
        , impl_(std::make_unique<common::resilience::circuit_breaker>(config_.to_common())) {}

    explicit circuit_breaker(const std::string& name)
        : name_(name)
        , config_()
        , impl_(std::make_unique<common::resilience::circuit_breaker>(config_.to_common())) {}

    explicit circuit_breaker(const std::string& name, const config& cfg)
        : name_(name)
        , config_(cfg)
        , impl_(std::make_unique<common::resilience::circuit_breaker>(config_.to_common())) {}

    /**
     * @brief Execute a function with circuit breaker protection and fallback
     *
     * @tparam Func The function type to execute (must return common::Result<T>)
     * @tparam Fallback The fallback function type
     * @param func The function to execute
     * @param fallback The fallback function to execute on failure or open circuit
     * @return common::Result<T> containing success value or error
     */
    template<typename Func, typename Fallback>
    common::Result<T> execute(Func&& func, Fallback&& fallback) {
        metrics_.total_calls++;

        if (!impl_->allow_request()) {
            metrics_.rejected_calls++;
            return fallback();
        }

        auto op_result = func();
        if (op_result.is_ok()) {
            impl_->record_success();
            metrics_.successful_calls++;
            return op_result;
        } else {
            impl_->record_failure();
            metrics_.failed_calls++;
            return fallback();
        }
    }

    /**
     * @brief Execute a function with circuit breaker protection
     *
     * @tparam Func The function type to execute (must return common::Result<T>)
     * @param func The function to execute
     * @return common::Result<T> containing success value or error
     */
    template<typename Func>
    common::Result<T> execute(Func&& func) {
        metrics_.total_calls++;

        if (!impl_->allow_request()) {
            metrics_.rejected_calls++;
            return common::make_error<T>(static_cast<int>(monitoring_error_code::circuit_breaker_open),
                               "Circuit breaker '" + name_ + "' is open");
        }

        auto op_result = func();
        if (op_result.is_ok()) {
            impl_->record_success();
            metrics_.successful_calls++;
            return op_result;
        } else {
            impl_->record_failure();
            metrics_.failed_calls++;
            return op_result;
        }
    }

    /**
     * @brief Get current circuit state
     */
    circuit_state get_state() const {
        return impl_->get_state();
    }

    /**
     * @brief Get current failure count (approximation from stats)
     */
    size_t get_failure_count() const {
        auto stats = impl_->get_stats();
        auto it = stats.find("failure_count");
        if (it != stats.end() && std::holds_alternative<std::int64_t>(it->second)) {
            return static_cast<size_t>(std::get<std::int64_t>(it->second));
        }
        return 0;
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
        // common_system circuit_breaker doesn't expose reset()
        // Recreate the instance to reset state
        impl_ = std::make_unique<common::resilience::circuit_breaker>(config_.to_common());

        // Reset metrics
        metrics_.total_calls = 0;
        metrics_.successful_calls = 0;
        metrics_.failed_calls = 0;
        metrics_.rejected_calls = 0;
        metrics_.state_transitions = 0;
    }

private:
    std::string name_;
    config config_;
    std::unique_ptr<common::resilience::circuit_breaker> impl_;
    mutable circuit_breaker_metrics metrics_;
};

} // namespace kcenon::monitoring
