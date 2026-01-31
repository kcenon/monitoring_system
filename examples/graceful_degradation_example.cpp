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

/**
 * @file graceful_degradation_example.cpp
 * @brief Demonstration of reliability patterns and graceful degradation
 *
 * This example demonstrates:
 * - Circuit breaker pattern with monitoring
 * - Retry policies with exponential backoff
 * - Error boundary usage patterns
 * - Cascading failure prevention
 * - Fallback mechanisms
 * - Bulkhead pattern for resource isolation
 */

#include <atomic>
#include <chrono>
#include <iostream>
#include <random>
#include <thread>

#include "kcenon/monitoring/core/error_codes.h"
#include "kcenon/monitoring/core/result_types.h"
#include "kcenon/monitoring/reliability/circuit_breaker.h"
#include "kcenon/monitoring/reliability/retry_policy.h"

using namespace kcenon::monitoring;
using namespace std::chrono_literals;

// Simulated unreliable service
class unreliable_service {
private:
    std::mt19937 rng_;
    std::uniform_real_distribution<> dist_;
    double failure_rate_;
    std::atomic<int> call_count_{0};

public:
    explicit unreliable_service(double failure_rate)
        : rng_(std::random_device{}())
        , dist_(0.0, 1.0)
        , failure_rate_(failure_rate) {}

    kcenon::common::Result<std::string> call() {
        call_count_++;
        std::this_thread::sleep_for(50ms);

        if (dist_(rng_) < failure_rate_) {
            return kcenon::common::Result<std::string>::err(
                error_info{monitoring_error_code::service_unavailable,
                          "Service temporarily unavailable"}.to_common_error()
            );
        }

        return kcenon::common::ok(std::string("Service response: SUCCESS"));
    }

    void set_failure_rate(double rate) {
        failure_rate_ = rate;
    }
};

// Demonstrate circuit breaker pattern
void demonstrate_circuit_breaker() {
    std::cout << "=== Circuit Breaker Pattern ===" << std::endl;
    std::cout << std::endl;

    unreliable_service service(0.7);  // 70% failure rate

    circuit_breaker_config config;
    config.failure_threshold = 3;
    config.reset_timeout = 5000ms;

    circuit_breaker<std::string> breaker("external_service", config);

    std::cout << "Circuit Breaker Configuration:" << std::endl;
    std::cout << "- Failure threshold: " << config.failure_threshold << std::endl;
    std::cout << "- Reset timeout: 5000ms" << std::endl;
    std::cout << std::endl;

    std::cout << "Making calls to unreliable service (70% failure rate):" << std::endl;
    std::cout << std::endl;

    for (int i = 0; i < 10; ++i) {
        std::cout << "Call " << (i + 1) << ": ";

        auto result = breaker.execute([&service]() {
            return service.call();
        });

        if (result.is_ok()) {
            std::cout << "SUCCESS" << std::endl;
        } else {
            std::cout << "FAILED" << std::endl;
        }

        std::this_thread::sleep_for(200ms);
    }

    std::cout << std::endl;

    auto metrics = breaker.get_metrics();
    std::cout << "Circuit Breaker Metrics:" << std::endl;
    std::cout << "- Total calls: " << metrics.total_calls << std::endl;
    std::cout << "- Successful: " << metrics.successful_calls << std::endl;
    std::cout << "- Failed: " << metrics.failed_calls << std::endl;
    std::cout << "- Rejected: " << metrics.rejected_calls << std::endl;
    std::cout << std::endl;
}

// Demonstrate retry policy
void demonstrate_retry_policy() {
    std::cout << "=== Retry Policy with Exponential Backoff ===" << std::endl;
    std::cout << std::endl;

    unreliable_service service(0.4);  // 40% failure rate

    retry_config retry_cfg;
    retry_cfg.max_attempts = 5;
    retry_cfg.strategy = retry_strategy::exponential_backoff;
    retry_cfg.initial_delay = 100ms;
    retry_cfg.backoff_multiplier = 2.0;

    retry_executor<std::string> policy("service_retry", retry_cfg);

    std::cout << "Retry Policy Configuration:" << std::endl;
    std::cout << "- Strategy: Exponential backoff" << std::endl;
    std::cout << "- Max attempts: 5" << std::endl;
    std::cout << "- Initial delay: 100ms" << std::endl;
    std::cout << std::endl;

    std::cout << "Making calls with retry policy:" << std::endl;
    std::cout << std::endl;

    for (int i = 0; i < 5; ++i) {
        std::cout << "Request " << (i + 1) << ": ";

        auto result = policy.execute([&service]() {
            return service.call();
        });

        if (result.is_ok()) {
            std::cout << "SUCCESS" << std::endl;
        } else {
            std::cout << "FAILED after retries" << std::endl;
        }
    }

    std::cout << std::endl;

    auto metrics = policy.get_metrics();
    std::cout << "Retry Policy Metrics:" << std::endl;
    std::cout << "- Total executions: " << metrics.total_executions << std::endl;
    std::cout << "- Successful: " << metrics.successful_executions << std::endl;
    std::cout << "- Failed: " << metrics.failed_executions << std::endl;
    std::cout << "- Total retries: " << metrics.total_retries << std::endl;
    std::cout << std::endl;
}

// Demonstrate combined patterns
void demonstrate_combined_patterns() {
    std::cout << "=== Combined Reliability Patterns ===" << std::endl;
    std::cout << std::endl;

    unreliable_service primary_service(0.5);

    circuit_breaker_config cb_config;
    cb_config.failure_threshold = 3;
    circuit_breaker<std::string> breaker("primary", cb_config);

    retry_config retry_cfg2;
    retry_cfg2.max_attempts = 3;
    retry_cfg2.strategy = retry_strategy::exponential_backoff;
    retry_cfg2.initial_delay = 100ms;

    retry_executor<std::string> policy2("combined_retry", retry_cfg2);

    std::cout << "Combining Circuit Breaker + Retry Policy" << std::endl;
    std::cout << std::endl;

    for (int i = 0; i < 10; ++i) {
        std::cout << "Request " << (i + 1) << ": ";

        auto result = breaker.execute([&]() {
            return policy2.execute([&]() {
                return primary_service.call();
            });
        });

        if (result.is_ok()) {
            std::cout << "SUCCESS" << std::endl;
        } else {
            std::cout << "FAILED" << std::endl;
        }

        std::this_thread::sleep_for(300ms);
    }

    std::cout << std::endl;

    auto cb_metrics = breaker.get_metrics();
    std::cout << "Circuit Breaker:" << std::endl;
    std::cout << "- Total calls: " << cb_metrics.total_calls << std::endl;
    std::cout << "- Rejected calls: " << cb_metrics.rejected_calls << std::endl;
    std::cout << std::endl;

    auto retry_metrics = policy2.get_metrics();
    std::cout << "Retry Policy:" << std::endl;
    std::cout << "- Total executions: " << retry_metrics.total_executions << std::endl;
    std::cout << "- Total retries: " << retry_metrics.total_retries << std::endl;
    std::cout << std::endl;
}

int main() {
    std::cout << "=== Graceful Degradation and Reliability Patterns ===" << std::endl;
    std::cout << std::endl;

    try {
        demonstrate_circuit_breaker();
        std::cout << std::string(70, '=') << std::endl;
        std::cout << std::endl;

        demonstrate_retry_policy();
        std::cout << std::string(70, '=') << std::endl;
        std::cout << std::endl;

        demonstrate_combined_patterns();
        std::cout << std::string(70, '=') << std::endl;
        std::cout << std::endl;

        std::cout << "=== All Reliability Patterns Demonstrated Successfully ===" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
