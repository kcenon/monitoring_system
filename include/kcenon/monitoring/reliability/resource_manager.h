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
#include <unordered_map>

#include "kcenon/monitoring/core/result_types.h"

namespace kcenon::monitoring {

// ============================================================================
// Enums
// ============================================================================

/**
 * @brief Strategy for handling resource exhaustion
 */
enum class throttling_strategy {
    reject,  ///< Reject requests immediately when limit exceeded
    delay    ///< Delay requests until resources are available
};

/**
 * @brief Type of resource being managed
 */
enum class resource_type {
    memory,  ///< Memory resource
    cpu      ///< CPU resource
};

// ============================================================================
// Metrics Types
// ============================================================================

/**
 * @brief Metrics for resource usage tracking
 */
struct resource_metrics {
    std::atomic<size_t> current_usage{0};
    std::atomic<size_t> total_allocations{0};
    std::atomic<size_t> peak_usage{0};
    std::atomic<size_t> rejected_operations{0};
    std::atomic<size_t> delayed_operations{0};

    resource_metrics() = default;
    resource_metrics(const resource_metrics& other)
        : current_usage(other.current_usage.load())
        , total_allocations(other.total_allocations.load())
        , peak_usage(other.peak_usage.load())
        , rejected_operations(other.rejected_operations.load())
        , delayed_operations(other.delayed_operations.load()) {}

    resource_metrics& operator=(const resource_metrics& other) {
        if (this != &other) {
            current_usage.store(other.current_usage.load());
            total_allocations.store(other.total_allocations.load());
            peak_usage.store(other.peak_usage.load());
            rejected_operations.store(other.rejected_operations.load());
            delayed_operations.store(other.delayed_operations.load());
        }
        return *this;
    }
};

// ============================================================================
// Configuration Types
// ============================================================================

/**
 * @brief Configuration for rate limiting
 */
struct rate_limit_config {
    double rate_per_second = 100.0;   ///< Rate of token refill per second
    size_t burst_capacity = 10;       ///< Maximum burst capacity
    throttling_strategy strategy = throttling_strategy::reject;

    /**
     * @brief Validate configuration
     * @return true if configuration is valid
     */
    bool validate() const {
        return rate_per_second > 0 && burst_capacity > 0;
    }
};

/**
 * @brief Configuration for resource quotas
 */
struct resource_quota {
    resource_type type = resource_type::memory;
    size_t max_value = 0;              ///< Maximum allowed resource usage
    size_t warning_threshold = 0;      ///< Warning level threshold
    size_t critical_threshold = 0;     ///< Critical level threshold
    throttling_strategy strategy = throttling_strategy::reject;

    resource_quota() = default;
    resource_quota(resource_type t, size_t max, throttling_strategy s = throttling_strategy::reject)
        : type(t), max_value(max), warning_threshold(max * 70 / 100),
          critical_threshold(max * 90 / 100), strategy(s) {}

    /**
     * @brief Validate configuration
     * @return true if configuration is valid
     */
    bool validate() const {
        if (max_value == 0) {
            return false;
        }
        if (warning_threshold > max_value) {
            return false;
        }
        if (critical_threshold > max_value) {
            return false;
        }
        if (warning_threshold > critical_threshold) {
            return false;
        }
        return true;
    }
};

/**
 * @brief Configuration for CPU throttling
 */
struct cpu_throttle_config {
    double max_cpu_usage = 0.8;        ///< Maximum CPU usage (0.0 to 1.0)
    double warning_threshold = 0.7;    ///< Warning threshold (0.0 to 1.0)
    throttling_strategy strategy = throttling_strategy::reject;
    std::chrono::milliseconds check_interval = std::chrono::milliseconds(100);

    /**
     * @brief Validate configuration
     * @return true if configuration is valid
     */
    bool validate() const {
        if (max_cpu_usage <= 0.0 || max_cpu_usage > 1.0) {
            return false;
        }
        if (warning_threshold <= 0.0 || warning_threshold > 1.0) {
            return false;
        }
        if (warning_threshold > max_cpu_usage) {
            return false;
        }
        return true;
    }
};

// ============================================================================
// Rate Limiter Interface
// ============================================================================

/**
 * @brief Base interface for rate limiters
 */
class rate_limiter {
public:
    virtual ~rate_limiter() = default;

    /**
     * @brief Try to acquire tokens
     * @param count Number of tokens to acquire
     * @return true if tokens were acquired
     */
    virtual bool try_acquire(size_t count = 1) = 0;

    /**
     * @brief Execute a function with rate limiting
     * @tparam Func Function type that returns common::Result<T>
     * @param func Function to execute
     * @return Result of the function or error if rate limited
     */
    template<typename Func>
    auto execute(Func&& func) -> decltype(func()) {
        using result_type = decltype(func());
        using value_type = typename result_type::value_type;

        if (!try_acquire(1)) {
            return make_error<value_type>(monitoring_error_code::resource_exhausted,
                                          "Rate limit exceeded for '" + get_name() + "'");
        }
        return func();
    }

    /**
     * @brief Get the name of this rate limiter
     */
    virtual const std::string& get_name() const = 0;
};

// ============================================================================
// Token Bucket Rate Limiter
// ============================================================================

/**
 * @brief Token bucket rate limiter implementation
 *
 * Uses the token bucket algorithm for rate limiting.
 * Tokens are added at a fixed rate and consumed by requests.
 */
class token_bucket_limiter : public rate_limiter {
public:
    using clock = std::chrono::steady_clock;

    token_bucket_limiter(const std::string& name, double rate, size_t capacity,
                         throttling_strategy /*strategy*/ = throttling_strategy::reject)
        : name_(name)
        , rate_(rate)
        , capacity_(capacity)
        , tokens_(static_cast<double>(capacity))
        , last_refill_(clock::now()) {}

    bool try_acquire(size_t count = 1) override {
        std::lock_guard<std::mutex> lock(mutex_);
        refill();

        if (tokens_ >= static_cast<double>(count)) {
            tokens_ -= static_cast<double>(count);
            return true;
        }
        return false;
    }

    const std::string& get_name() const override {
        return name_;
    }

private:
    void refill() {
        auto now = clock::now();
        auto elapsed = std::chrono::duration<double>(now - last_refill_).count();
        tokens_ = std::min(static_cast<double>(capacity_), tokens_ + elapsed * rate_);
        last_refill_ = now;
    }

    std::string name_;
    double rate_;
    size_t capacity_;
    double tokens_;
    clock::time_point last_refill_;
    mutable std::mutex mutex_;
};

// ============================================================================
// Leaky Bucket Rate Limiter
// ============================================================================

/**
 * @brief Leaky bucket rate limiter implementation
 *
 * Uses the leaky bucket algorithm for rate limiting.
 * Requests fill the bucket and it leaks at a constant rate.
 */
class leaky_bucket_limiter : public rate_limiter {
public:
    using clock = std::chrono::steady_clock;

    leaky_bucket_limiter(const std::string& name, double rate, size_t capacity)
        : name_(name)
        , rate_(rate)
        , capacity_(capacity)
        , water_(0.0)
        , last_leak_(clock::now()) {}

    bool try_acquire(size_t count = 1) override {
        std::lock_guard<std::mutex> lock(mutex_);
        leak();

        if (water_ + static_cast<double>(count) <= static_cast<double>(capacity_)) {
            water_ += static_cast<double>(count);
            return true;
        }
        return false;
    }

    const std::string& get_name() const override {
        return name_;
    }

private:
    void leak() {
        auto now = clock::now();
        auto elapsed = std::chrono::duration<double>(now - last_leak_).count();
        water_ = std::max(0.0, water_ - elapsed * rate_);
        last_leak_ = now;
    }

    std::string name_;
    double rate_;
    size_t capacity_;
    double water_;
    clock::time_point last_leak_;
    mutable std::mutex mutex_;
};

// ============================================================================
// Memory Quota Manager
// ============================================================================

/**
 * @brief Manages memory quota with tracking and throttling
 */
class memory_quota_manager {
public:
    memory_quota_manager(const std::string& name, const resource_quota& quota)
        : name_(name), quota_(quota) {}

    memory_quota_manager(const std::string& name, size_t max_bytes,
                         throttling_strategy strategy = throttling_strategy::reject)
        : name_(name), quota_(resource_type::memory, max_bytes, strategy) {}

    /**
     * @brief Allocate memory from the quota
     * @param bytes Number of bytes to allocate
     * @return Success or error if quota exceeded
     */
    common::Result<bool> allocate(size_t bytes) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (metrics_.current_usage.load() + bytes > quota_.max_value) {
            metrics_.rejected_operations++;
            return make_error<bool>(monitoring_error_code::resource_exhausted,
                                   "Memory quota exceeded for '" + name_ + "'");
        }

        metrics_.current_usage += bytes;
        metrics_.total_allocations++;

        // Update peak usage
        size_t current = metrics_.current_usage.load();
        size_t peak = metrics_.peak_usage.load();
        while (current > peak && !metrics_.peak_usage.compare_exchange_weak(peak, current)) {
            // Retry until successful
        }

        return common::ok(true);
    }

    /**
     * @brief Deallocate memory back to the quota
     * @param bytes Number of bytes to deallocate
     */
    void deallocate(size_t bytes) {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t current = metrics_.current_usage.load();
        metrics_.current_usage.store(current >= bytes ? current - bytes : 0);
    }

    /**
     * @brief Get current memory usage
     */
    size_t current_usage() const {
        return metrics_.current_usage.load();
    }

    /**
     * @brief Check if usage is over warning threshold
     */
    bool is_over_warning_threshold() const {
        return metrics_.current_usage.load() >= quota_.warning_threshold;
    }

    /**
     * @brief Check if usage is over critical threshold
     */
    bool is_over_critical_threshold() const {
        return metrics_.current_usage.load() >= quota_.critical_threshold;
    }

    /**
     * @brief Get current metrics
     */
    resource_metrics get_metrics() const {
        return metrics_;
    }

    /**
     * @brief Get the name of this manager
     */
    const std::string& get_name() const {
        return name_;
    }

private:
    std::string name_;
    resource_quota quota_;
    mutable resource_metrics metrics_;
    mutable std::mutex mutex_;
};

// ============================================================================
// CPU Throttler
// ============================================================================

/**
 * @brief Throttles operations based on CPU usage
 */
class cpu_throttler {
public:
    cpu_throttler(const std::string& name, const cpu_throttle_config& config)
        : name_(name), config_(config) {}

    /**
     * @brief Execute a function with CPU throttling
     * @tparam Func Function type that returns common::Result<T>
     * @param func Function to execute
     * @return Result of the function or error if throttled
     */
    template<typename Func>
    auto execute(Func&& func) -> decltype(func()) {
        // For simplicity, we just execute the function and track metrics
        // Real CPU monitoring would require platform-specific code
        metrics_.total_allocations++;

        return std::forward<Func>(func)();
    }

    /**
     * @brief Get current metrics
     */
    resource_metrics get_metrics() const {
        return metrics_;
    }

    /**
     * @brief Get the name of this throttler
     */
    const std::string& get_name() const {
        return name_;
    }

private:
    std::string name_;
    cpu_throttle_config config_;
    mutable resource_metrics metrics_;
};

// ============================================================================
// Resource Manager
// ============================================================================

/**
 * @brief Coordinates multiple resource management components
 */
class resource_manager {
public:
    explicit resource_manager(const std::string& name) : name_(name) {}

    /**
     * @brief Add a rate limiter
     * @param name Name for the limiter
     * @param config Rate limit configuration
     * @return Success or error if name already exists
     */
    common::Result<bool> add_rate_limiter(const std::string& name, const rate_limit_config& config) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (rate_limiters_.find(name) != rate_limiters_.end()) {
            return make_error<bool>(monitoring_error_code::already_exists,
                                   "Rate limiter '" + name + "' already exists");
        }

        rate_limiters_[name] = std::make_unique<token_bucket_limiter>(
            name, config.rate_per_second, config.burst_capacity, config.strategy);
        return common::ok(true);
    }

    /**
     * @brief Get a rate limiter by name
     * @param name Name of the limiter
     * @return Pointer to the limiter or nullptr if not found
     */
    rate_limiter* get_rate_limiter(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = rate_limiters_.find(name);
        return it != rate_limiters_.end() ? it->second.get() : nullptr;
    }

    /**
     * @brief Add a memory quota manager
     * @param name Name for the manager
     * @param quota Resource quota configuration
     * @return Success or error if name already exists
     */
    common::Result<bool> add_memory_quota(const std::string& name, const resource_quota& quota) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (memory_quotas_.find(name) != memory_quotas_.end()) {
            return make_error<bool>(monitoring_error_code::already_exists,
                                   "Memory quota '" + name + "' already exists");
        }

        memory_quotas_[name] = std::make_unique<memory_quota_manager>(name, quota);
        return common::ok(true);
    }

    /**
     * @brief Get a memory quota manager by name
     * @param name Name of the manager
     * @return Pointer to the manager or nullptr if not found
     */
    memory_quota_manager* get_memory_quota(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = memory_quotas_.find(name);
        return it != memory_quotas_.end() ? it->second.get() : nullptr;
    }

    /**
     * @brief Add a CPU throttler
     * @param name Name for the throttler
     * @param config CPU throttle configuration
     * @return Success or error if name already exists
     */
    common::Result<bool> add_cpu_throttler(const std::string& name, const cpu_throttle_config& config) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (cpu_throttlers_.find(name) != cpu_throttlers_.end()) {
            return make_error<bool>(monitoring_error_code::already_exists,
                                   "CPU throttler '" + name + "' already exists");
        }

        cpu_throttlers_[name] = std::make_unique<cpu_throttler>(name, config);
        return common::ok(true);
    }

    /**
     * @brief Get a CPU throttler by name
     * @param name Name of the throttler
     * @return Pointer to the throttler or nullptr if not found
     */
    cpu_throttler* get_cpu_throttler(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cpu_throttlers_.find(name);
        return it != cpu_throttlers_.end() ? it->second.get() : nullptr;
    }

    /**
     * @brief Check if all resources are healthy
     * @return Success with health status
     */
    common::Result<bool> is_healthy() const {
        std::lock_guard<std::mutex> lock(mutex_);

        for (const auto& [name, manager] : memory_quotas_) {
            if (manager->is_over_critical_threshold()) {
                return common::ok(false);
            }
        }

        return common::ok(true);
    }

    /**
     * @brief Get metrics for all managed resources
     * @return Map of resource name to metrics
     */
    std::unordered_map<std::string, resource_metrics> get_all_metrics() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::unordered_map<std::string, resource_metrics> all_metrics;

        for (const auto& [name, limiter] : rate_limiters_) {
            all_metrics["rate_" + name] = resource_metrics{};
        }

        for (const auto& [name, manager] : memory_quotas_) {
            all_metrics["memory_" + name] = manager->get_metrics();
        }

        for (const auto& [name, throttler] : cpu_throttlers_) {
            all_metrics["cpu_" + name] = throttler->get_metrics();
        }

        return all_metrics;
    }

private:
    std::string name_;
    std::unordered_map<std::string, std::unique_ptr<rate_limiter>> rate_limiters_;
    std::unordered_map<std::string, std::unique_ptr<memory_quota_manager>> memory_quotas_;
    std::unordered_map<std::string, std::unique_ptr<cpu_throttler>> cpu_throttlers_;
    mutable std::mutex mutex_;
};

// ============================================================================
// Factory Functions
// ============================================================================

/**
 * @brief Create a token bucket rate limiter
 */
inline std::unique_ptr<token_bucket_limiter> create_token_bucket_limiter(
    const std::string& name, double rate, size_t capacity,
    throttling_strategy strategy = throttling_strategy::reject) {
    return std::make_unique<token_bucket_limiter>(name, rate, capacity, strategy);
}

/**
 * @brief Create a leaky bucket rate limiter
 */
inline std::unique_ptr<leaky_bucket_limiter> create_leaky_bucket_limiter(
    const std::string& name, double rate, size_t capacity) {
    return std::make_unique<leaky_bucket_limiter>(name, rate, capacity);
}

/**
 * @brief Create a memory quota manager
 */
inline std::unique_ptr<memory_quota_manager> create_memory_quota_manager(
    const std::string& name, size_t max_bytes,
    throttling_strategy strategy = throttling_strategy::reject) {
    return std::make_unique<memory_quota_manager>(name, max_bytes, strategy);
}

/**
 * @brief Create a resource manager
 */
inline std::unique_ptr<resource_manager> create_resource_manager(const std::string& name) {
    return std::make_unique<resource_manager>(name);
}

} // namespace kcenon::monitoring
