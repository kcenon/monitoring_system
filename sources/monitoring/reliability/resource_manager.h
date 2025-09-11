#pragma once

/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, monitoring_system contributors
All rights reserved.
*****************************************************************************/

/**
 * @file resource_manager.h
 * @brief Resource management system for monitoring operations
 * 
 * This file implements comprehensive resource management including:
 * - Rate limiting (token bucket, leaky bucket algorithms)
 * - Memory quota management and tracking
 * - CPU throttling based on system load
 * - Resource pools and allocation tracking
 * - Bandwidth and throughput control
 */

#include <monitoring/core/result_types.h>
#include <monitoring/core/error_codes.h>
#include <atomic>
#include <chrono>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <functional>
#include <algorithm>
#include <cmath>

namespace monitoring_system {

// Forward declarations
class resource_manager;
class rate_limiter;
class memory_quota_manager;
class cpu_throttler;

/**
 * @enum resource_type
 * @brief Types of resources that can be managed
 */
enum class resource_type {
    memory,         // Memory usage tracking and limits
    cpu,            // CPU usage monitoring and throttling
    network,        // Network bandwidth control
    disk_io,        // Disk I/O rate limiting  
    api_calls,      // API call rate limiting
    custom          // Custom resource types
};

/**
 * @enum throttling_strategy
 * @brief Strategies for handling resource limit violations
 */
enum class throttling_strategy {
    block,          // Block until resource becomes available
    reject,         // Reject request immediately
    delay,          // Introduce delay before processing
    degrade,        // Reduce quality of service
    queue           // Queue request for later processing
};

/**
 * @enum rate_limit_algorithm
 * @brief Rate limiting algorithms
 */
enum class rate_limit_algorithm {
    token_bucket,   // Token bucket algorithm
    leaky_bucket,   // Leaky bucket algorithm
    fixed_window,   // Fixed time window counter
    sliding_window  // Sliding time window counter
};

/**
 * @struct resource_quota
 * @brief Resource quota configuration
 */
struct resource_quota {
    resource_type type;
    std::size_t max_value;              // Maximum allowed value
    std::size_t warning_threshold;      // Warning threshold (percentage of max)
    std::size_t critical_threshold;     // Critical threshold (percentage of max)
    throttling_strategy strategy;       // Strategy when quota exceeded
    std::chrono::milliseconds check_interval{std::chrono::seconds(1)};
    bool enable_auto_scaling{false};    // Enable automatic quota adjustment
    double auto_scale_factor{1.2};     // Factor for auto scaling
    
    resource_quota() = default;
    
    resource_quota(resource_type t, std::size_t max_val, 
                  throttling_strategy strat = throttling_strategy::block)
        : type(t), max_value(max_val), warning_threshold(max_val * 70 / 100),
          critical_threshold(max_val * 90 / 100), strategy(strat) {}
          
    bool validate() const {
        return max_value > 0 && 
               warning_threshold <= max_value && 
               critical_threshold <= max_value &&
               warning_threshold <= critical_threshold;
    }
};

/**
 * @struct rate_limit_config
 * @brief Rate limiting configuration
 */
struct rate_limit_config {
    rate_limit_algorithm algorithm{rate_limit_algorithm::token_bucket};
    std::size_t rate_per_second{1000};     // Allowed operations per second
    std::size_t burst_capacity{100};       // Burst capacity
    std::chrono::milliseconds window_size{std::chrono::seconds(1)};
    throttling_strategy strategy{throttling_strategy::block};
    
    bool validate() const {
        return rate_per_second > 0 && burst_capacity > 0;
    }
};

/**
 * @struct cpu_throttle_config  
 * @brief CPU throttling configuration
 */
struct cpu_throttle_config {
    double max_cpu_usage{0.8};             // Maximum CPU usage (0.0-1.0)
    double warning_threshold{0.7};         // Warning threshold
    std::chrono::milliseconds check_interval{std::chrono::milliseconds(100)};
    throttling_strategy strategy{throttling_strategy::delay};
    std::chrono::milliseconds max_delay{std::chrono::milliseconds(100)};
    
    bool validate() const {
        return max_cpu_usage > 0.0 && max_cpu_usage <= 1.0 &&
               warning_threshold > 0.0 && warning_threshold <= max_cpu_usage;
    }
};

/**
 * @struct resource_metrics
 * @brief Resource usage metrics
 */
struct resource_metrics {
    std::atomic<std::size_t> current_usage{0};
    std::atomic<std::size_t> peak_usage{0};
    std::atomic<std::size_t> total_allocations{0};
    std::atomic<std::size_t> total_deallocations{0};
    std::atomic<std::size_t> quota_violations{0};
    std::atomic<std::size_t> throttled_operations{0};
    std::atomic<double> average_usage{0.0};
    std::chrono::steady_clock::time_point last_reset;
    
    resource_metrics() : last_reset(std::chrono::steady_clock::now()) {}
    
    resource_metrics(const resource_metrics& other) 
        : current_usage(other.current_usage.load())
        , peak_usage(other.peak_usage.load())
        , total_allocations(other.total_allocations.load())
        , total_deallocations(other.total_deallocations.load())
        , quota_violations(other.quota_violations.load())
        , throttled_operations(other.throttled_operations.load())
        , average_usage(other.average_usage.load())
        , last_reset(other.last_reset) {}
        
    resource_metrics& operator=(const resource_metrics& other) {
        if (this != &other) {
            current_usage = other.current_usage.load();
            peak_usage = other.peak_usage.load();
            total_allocations = other.total_allocations.load();
            total_deallocations = other.total_deallocations.load();
            quota_violations = other.quota_violations.load();
            throttled_operations = other.throttled_operations.load();
            average_usage = other.average_usage.load();
            last_reset = other.last_reset;
        }
        return *this;
    }
    
    double get_utilization_rate(std::size_t quota) const {
        return quota > 0 ? static_cast<double>(current_usage.load()) / quota : 0.0;
    }
    
    void reset() {
        current_usage = 0;
        peak_usage = 0;
        total_allocations = 0;
        total_deallocations = 0;
        quota_violations = 0;
        throttled_operations = 0;
        average_usage = 0.0;
        last_reset = std::chrono::steady_clock::now();
    }
};

/**
 * @class token_bucket_limiter
 * @brief Token bucket rate limiting implementation
 */
class token_bucket_limiter {
public:
    token_bucket_limiter(std::size_t rate, std::size_t capacity)
        : rate_(rate), capacity_(capacity), tokens_(capacity), 
          last_refill_(std::chrono::steady_clock::now()) {}
          
    bool try_acquire(std::size_t tokens = 1) {
        std::lock_guard<std::mutex> lock(mutex_);
        refill_tokens();
        
        if (tokens_ >= tokens) {
            tokens_ -= tokens;
            return true;
        }
        return false;
    }
    
    result_void acquire(std::size_t tokens = 1) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        while (true) {
            refill_tokens();
            if (tokens_ >= tokens) {
                tokens_ -= tokens;
                return result_void{};
            }
            
            // Wait for token refill
            auto wait_time = std::chrono::milliseconds(
                static_cast<long long>(1000.0 * tokens / rate_));
            condition_.wait_for(lock, wait_time);
        }
    }
    
    std::size_t available_tokens() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return tokens_;
    }

private:
    void refill_tokens() {
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_refill_);
            
        if (duration.count() > 0) {
            auto tokens_to_add = static_cast<std::size_t>(
                duration.count() * rate_ / 1000.0);
            tokens_ = std::min(capacity_, tokens_ + tokens_to_add);
            last_refill_ = now;
            condition_.notify_all();
        }
    }
    
    std::size_t rate_;          // Tokens per second
    std::size_t capacity_;      // Maximum tokens
    std::size_t tokens_;        // Current tokens
    std::chrono::steady_clock::time_point last_refill_;
    mutable std::mutex mutex_;
    std::condition_variable condition_;
};

/**
 * @class leaky_bucket_limiter  
 * @brief Leaky bucket rate limiting implementation
 */
class leaky_bucket_limiter {
public:
    leaky_bucket_limiter(std::size_t rate, std::size_t capacity)
        : rate_(rate), capacity_(capacity), queue_size_(0),
          last_leak_(std::chrono::steady_clock::now()) {}
          
    bool try_acquire(std::size_t items = 1) {
        std::lock_guard<std::mutex> lock(mutex_);
        leak_items();
        
        if (queue_size_ + items <= capacity_) {
            queue_size_ += items;
            return true;
        }
        return false;
    }
    
    result_void acquire(std::size_t items = 1) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        while (queue_size_ + items > capacity_) {
            leak_items();
            if (queue_size_ + items <= capacity_) {
                break;
            }
            
            // Wait for bucket to leak
            auto wait_time = std::chrono::milliseconds(
                static_cast<long long>(1000.0 * items / rate_));
            condition_.wait_for(lock, wait_time);
        }
        
        queue_size_ += items;
        return result_void{};
    }
    
    std::size_t queue_size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_size_;
    }

private:
    void leak_items() {
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_leak_);
            
        if (duration.count() > 0) {
            auto items_to_leak = static_cast<std::size_t>(
                duration.count() * rate_ / 1000.0);
            queue_size_ = (queue_size_ > items_to_leak) ? 
                         queue_size_ - items_to_leak : 0;
            last_leak_ = now;
            condition_.notify_all();
        }
    }
    
    std::size_t rate_;          // Items per second leak rate
    std::size_t capacity_;      // Maximum queue size
    std::size_t queue_size_;    // Current queue size
    std::chrono::steady_clock::time_point last_leak_;
    mutable std::mutex mutex_;
    std::condition_variable condition_;
};

/**
 * @class rate_limiter
 * @brief Unified rate limiting interface
 */
class rate_limiter {
public:
    rate_limiter(const std::string& name, const rate_limit_config& config)
        : name_(name), config_(config) {
        
        switch (config.algorithm) {
            case rate_limit_algorithm::token_bucket:
                token_bucket_impl_ = std::make_unique<token_bucket_limiter>(
                    config.rate_per_second, config.burst_capacity);
                break;
            case rate_limit_algorithm::leaky_bucket:
                leaky_bucket_impl_ = std::make_unique<leaky_bucket_limiter>(
                    config.rate_per_second, config.burst_capacity);
                break;
            default:
                // Default to token bucket
                token_bucket_impl_ = std::make_unique<token_bucket_limiter>(
                    config.rate_per_second, config.burst_capacity);
                break;
        }
    }
    
    template<typename F>
    auto execute(F&& operation, std::size_t cost = 1) 
        -> result<std::invoke_result_t<F>> {
        
        switch (config_.strategy) {
            case throttling_strategy::block:
                return execute_blocking(std::forward<F>(operation), cost);
            case throttling_strategy::reject:
                return execute_rejecting(std::forward<F>(operation), cost);
            default:
                return execute_blocking(std::forward<F>(operation), cost);
        }
    }
    
    bool try_acquire(std::size_t cost = 1) {
        if (token_bucket_impl_) {
            return token_bucket_impl_->try_acquire(cost);
        } else if (leaky_bucket_impl_) {
            return leaky_bucket_impl_->try_acquire(cost);
        }
        return false;
    }
    
    const resource_metrics& get_metrics() const { return metrics_; }
    const std::string& get_name() const { return name_; }

private:
    template<typename F>
    auto execute_blocking(F&& operation, std::size_t cost)
        -> result<std::invoke_result_t<F>> {
        
        // Acquire rate limit
        if (token_bucket_impl_) {
            auto acquire_result = token_bucket_impl_->acquire(cost);
            if (!acquire_result) {
                metrics_.throttled_operations++;
                return make_error<std::invoke_result_t<F>>(
                    monitoring_error_code::resource_exhausted,
                    "Rate limit acquisition failed");
            }
        } else if (leaky_bucket_impl_) {
            auto acquire_result = leaky_bucket_impl_->acquire(cost);
            if (!acquire_result) {
                metrics_.throttled_operations++;
                return make_error<std::invoke_result_t<F>>(
                    monitoring_error_code::resource_exhausted,
                    "Rate limit acquisition failed");
            }
        }
        
        try {
            if constexpr (std::is_void_v<std::invoke_result_t<F>>) {
                operation();
                metrics_.total_allocations++;
                return result_void{};
            } else {
                auto result = operation();
                metrics_.total_allocations++;
                return make_success(result);
            }
        } catch (const std::exception& e) {
            return make_error<std::invoke_result_t<F>>(
                monitoring_error_code::operation_failed,
                std::string("Operation failed: ") + e.what());
        }
    }
    
    template<typename F>
    auto execute_rejecting(F&& operation, std::size_t cost)
        -> result<std::invoke_result_t<F>> {
        
        if (!try_acquire(cost)) {
            metrics_.throttled_operations++;
            return make_error<std::invoke_result_t<F>>(
                monitoring_error_code::resource_exhausted,
                "Rate limit exceeded");
        }
        
        try {
            if constexpr (std::is_void_v<std::invoke_result_t<F>>) {
                operation();
                metrics_.total_allocations++;
                return result_void{};
            } else {
                auto result = operation();
                metrics_.total_allocations++;
                return make_success(result);
            }
        } catch (const std::exception& e) {
            return make_error<std::invoke_result_t<F>>(
                monitoring_error_code::operation_failed,
                std::string("Operation failed: ") + e.what());
        }
    }

    std::string name_;
    rate_limit_config config_;
    std::unique_ptr<token_bucket_limiter> token_bucket_impl_;
    std::unique_ptr<leaky_bucket_limiter> leaky_bucket_impl_;
    mutable resource_metrics metrics_;
};

/**
 * @class memory_quota_manager
 * @brief Memory usage tracking and quota management
 */
class memory_quota_manager {
public:
    memory_quota_manager(const std::string& name, const resource_quota& quota)
        : name_(name), quota_(quota), running_(false) {
        
        if (quota.check_interval.count() > 0) {
            running_ = true;
            monitor_thread_ = std::thread([this]() { monitor_loop(); });
        }
    }
    
    ~memory_quota_manager() {
        stop();
    }
    
    result_void allocate(std::size_t size) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (current_usage_ + size > quota_.max_value) {
            metrics_.quota_violations++;
            
            switch (quota_.strategy) {
                case throttling_strategy::reject:
                    return result_void{monitoring_error_code::resource_exhausted,
                        "Memory quota exceeded"};
                case throttling_strategy::block:
                    // Wait for memory to be freed
                    // In a real implementation, this might wait on a condition variable
                    return result_void{monitoring_error_code::resource_exhausted,
                        "Memory quota exceeded - would block"};
                default:
                    break;
            }
        }
        
        current_usage_ += size;
        metrics_.current_usage = current_usage_;
        metrics_.peak_usage = std::max(metrics_.peak_usage.load(), current_usage_);
        metrics_.total_allocations++;
        
        return result_void{};
    }
    
    void deallocate(std::size_t size) {
        std::lock_guard<std::mutex> lock(mutex_);
        current_usage_ = (current_usage_ > size) ? current_usage_ - size : 0;
        metrics_.current_usage = current_usage_;
        metrics_.total_deallocations++;
    }
    
    std::size_t current_usage() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return current_usage_;
    }
    
    double utilization_rate() const {
        return static_cast<double>(current_usage()) / quota_.max_value;
    }
    
    bool is_over_warning_threshold() const {
        return current_usage() > quota_.warning_threshold;
    }
    
    bool is_over_critical_threshold() const {
        return current_usage() > quota_.critical_threshold;
    }
    
    const resource_metrics& get_metrics() const { return metrics_; }
    const std::string& get_name() const { return name_; }
    const resource_quota& get_quota() const { return quota_; }

private:
    void stop() {
        if (running_) {
            running_ = false;
            if (monitor_thread_.joinable()) {
                monitor_thread_.join();
            }
        }
    }
    
    void monitor_loop() {
        while (running_) {
            std::this_thread::sleep_for(quota_.check_interval);
            
            std::lock_guard<std::mutex> lock(mutex_);
            
            // Update average usage
            static double alpha = 0.1; // Exponential smoothing factor
            metrics_.average_usage = alpha * current_usage_ + 
                                   (1.0 - alpha) * metrics_.average_usage.load();
            
            // Check for auto-scaling
            if (quota_.enable_auto_scaling && is_over_warning_threshold()) {
                // In a real implementation, this might trigger auto-scaling
                // For now, we just log the condition
            }
        }
    }
    
    std::string name_;
    resource_quota quota_;
    std::size_t current_usage_{0};
    mutable std::mutex mutex_;
    mutable resource_metrics metrics_;
    
    std::atomic<bool> running_;
    std::thread monitor_thread_;
};

/**
 * @class cpu_throttler
 * @brief CPU usage monitoring and throttling
 */
class cpu_throttler {
public:
    cpu_throttler(const std::string& name, const cpu_throttle_config& config)
        : name_(name), config_(config), running_(false) {
        
        if (config.check_interval.count() > 0) {
            running_ = true;
            monitor_thread_ = std::thread([this]() { monitor_loop(); });
        }
    }
    
    ~cpu_throttler() {
        stop();
    }
    
    template<typename F>
    auto execute(F&& operation) -> result<std::invoke_result_t<F>> {
        // Check current CPU usage
        double cpu_usage = get_cpu_usage();
        
        if (cpu_usage > config_.max_cpu_usage) {
            metrics_.throttled_operations++;
            
            switch (config_.strategy) {
                case throttling_strategy::reject:
                    return make_error<std::invoke_result_t<F>>(
                        monitoring_error_code::resource_exhausted,
                        "CPU usage too high");
                case throttling_strategy::delay: {
                    auto delay = calculate_delay(cpu_usage);
                    std::this_thread::sleep_for(delay);
                    break;
                }
                default:
                    break;
            }
        }
        
        try {
            if constexpr (std::is_void_v<std::invoke_result_t<F>>) {
                operation();
                metrics_.total_allocations++;
                return result_void{};
            } else {
                auto result = operation();
                metrics_.total_allocations++;
                return make_success(result);
            }
        } catch (const std::exception& e) {
            return make_error<std::invoke_result_t<F>>(
                monitoring_error_code::operation_failed,
                std::string("Operation failed: ") + e.what());
        }
    }
    
    double get_current_cpu_usage() const {
        return current_cpu_usage_.load();
    }
    
    bool is_over_threshold() const {
        return get_current_cpu_usage() > config_.max_cpu_usage;
    }
    
    const resource_metrics& get_metrics() const { return metrics_; }
    const std::string& get_name() const { return name_; }

private:
    double get_cpu_usage() {
        // Simplified CPU usage calculation
        // In a real implementation, this would read from /proc/stat or equivalent
        return current_cpu_usage_.load();
    }
    
    std::chrono::milliseconds calculate_delay(double cpu_usage) {
        double excess = cpu_usage - config_.max_cpu_usage;
        double delay_ms = std::min<double>(
            static_cast<double>(config_.max_delay.count()),
            excess * 1000.0
        );
        return std::chrono::milliseconds(static_cast<long long>(delay_ms));
    }
    
    void stop() {
        if (running_) {
            running_ = false;
            if (monitor_thread_.joinable()) {
                monitor_thread_.join();
            }
        }
    }
    
    void monitor_loop() {
        while (running_) {
            std::this_thread::sleep_for(config_.check_interval);
            
            // Simulate CPU usage monitoring
            // In a real implementation, this would read actual CPU statistics
            static double simulated_cpu = 0.1;
            simulated_cpu += (static_cast<double>(rand()) / RAND_MAX - 0.5) * 0.1;
            simulated_cpu = std::max(0.0, std::min(1.0, simulated_cpu));
            
            current_cpu_usage_ = simulated_cpu;
            
            // Update metrics
            metrics_.average_usage = 0.1 * simulated_cpu + 0.9 * metrics_.average_usage.load();
            
            if (simulated_cpu > config_.warning_threshold) {
                // CPU usage is high, might need throttling
            }
        }
    }
    
    std::string name_;
    cpu_throttle_config config_;
    std::atomic<double> current_cpu_usage_{0.0};
    mutable resource_metrics metrics_;
    
    std::atomic<bool> running_;
    std::thread monitor_thread_;
};

/**
 * @class resource_manager
 * @brief Unified resource management system
 */
class resource_manager {
public:
    resource_manager(const std::string& name) : name_(name) {}
    
    ~resource_manager() = default;
    
    // Rate limiter management
    result_void add_rate_limiter(const std::string& name, 
                               const rate_limit_config& config) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (rate_limiters_.find(name) != rate_limiters_.end()) {
            return result_void{monitoring_error_code::already_exists,
                "Rate limiter already exists"};
        }
        
        rate_limiters_[name] = std::make_unique<rate_limiter>(name, config);
        return result_void{};
    }
    
    std::shared_ptr<rate_limiter> get_rate_limiter(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = rate_limiters_.find(name);
        return (it != rate_limiters_.end()) ? 
               std::shared_ptr<rate_limiter>(std::move(it->second)) : nullptr;
    }
    
    // Memory quota management
    result_void add_memory_quota(const std::string& name, 
                               const resource_quota& quota) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (memory_quotas_.find(name) != memory_quotas_.end()) {
            return result_void{monitoring_error_code::already_exists,
                "Memory quota already exists"};
        }
        
        memory_quotas_[name] = std::make_unique<memory_quota_manager>(name, quota);
        return result_void{};
    }
    
    std::shared_ptr<memory_quota_manager> get_memory_quota(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = memory_quotas_.find(name);
        return (it != memory_quotas_.end()) ?
               std::shared_ptr<memory_quota_manager>(std::move(it->second)) : nullptr;
    }
    
    // CPU throttling management
    result_void add_cpu_throttler(const std::string& name,
                                const cpu_throttle_config& config) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (cpu_throttlers_.find(name) != cpu_throttlers_.end()) {
            return result_void{monitoring_error_code::already_exists,
                "CPU throttler already exists"};
        }
        
        cpu_throttlers_[name] = std::make_unique<cpu_throttler>(name, config);
        return result_void{};
    }
    
    std::shared_ptr<cpu_throttler> get_cpu_throttler(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cpu_throttlers_.find(name);
        return (it != cpu_throttlers_.end()) ?
               std::shared_ptr<cpu_throttler>(std::move(it->second)) : nullptr;
    }
    
    // Global resource monitoring
    std::unordered_map<std::string, resource_metrics> get_all_metrics() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::unordered_map<std::string, resource_metrics> all_metrics;
        
        for (const auto& [name, limiter] : rate_limiters_) {
            all_metrics[name + "_rate"] = limiter->get_metrics();
        }
        
        for (const auto& [name, quota] : memory_quotas_) {
            all_metrics[name + "_memory"] = quota->get_metrics();
        }
        
        for (const auto& [name, throttler] : cpu_throttlers_) {
            all_metrics[name + "_cpu"] = throttler->get_metrics();
        }
        
        return all_metrics;
    }
    
    // Health check
    result<bool> is_healthy() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Check memory quotas
        for (const auto& [name, quota] : memory_quotas_) {
            if (quota->is_over_critical_threshold()) {
                return make_success(false);
            }
        }
        
        // Check CPU throttlers
        for (const auto& [name, throttler] : cpu_throttlers_) {
            if (throttler->is_over_threshold()) {
                return make_success(false);
            }
        }
        
        return make_success(true);
    }
    
    void reset_all_metrics() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (const auto& [name, limiter] : rate_limiters_) {
            const_cast<resource_metrics&>(limiter->get_metrics()).reset();
        }
        
        for (const auto& [name, quota] : memory_quotas_) {
            const_cast<resource_metrics&>(quota->get_metrics()).reset();
        }
        
        for (const auto& [name, throttler] : cpu_throttlers_) {
            const_cast<resource_metrics&>(throttler->get_metrics()).reset();
        }
    }

private:
    std::string name_;
    mutable std::mutex mutex_;
    
    std::unordered_map<std::string, std::unique_ptr<rate_limiter>> rate_limiters_;
    std::unordered_map<std::string, std::unique_ptr<memory_quota_manager>> memory_quotas_;
    std::unordered_map<std::string, std::unique_ptr<cpu_throttler>> cpu_throttlers_;
};

// Factory functions for easier creation

/**
 * @brief Create a token bucket rate limiter
 */
inline std::unique_ptr<rate_limiter> create_token_bucket_limiter(
    const std::string& name, std::size_t rate, std::size_t capacity,
    throttling_strategy strategy = throttling_strategy::block) {
    
    rate_limit_config config;
    config.algorithm = rate_limit_algorithm::token_bucket;
    config.rate_per_second = rate;
    config.burst_capacity = capacity;
    config.strategy = strategy;
    
    return std::make_unique<rate_limiter>(name, config);
}

/**
 * @brief Create a leaky bucket rate limiter
 */
inline std::unique_ptr<rate_limiter> create_leaky_bucket_limiter(
    const std::string& name, std::size_t rate, std::size_t capacity,
    throttling_strategy strategy = throttling_strategy::block) {
    
    rate_limit_config config;
    config.algorithm = rate_limit_algorithm::leaky_bucket;
    config.rate_per_second = rate;
    config.burst_capacity = capacity;
    config.strategy = strategy;
    
    return std::make_unique<rate_limiter>(name, config);
}

/**
 * @brief Create a memory quota manager
 */
inline std::unique_ptr<memory_quota_manager> create_memory_quota_manager(
    const std::string& name, std::size_t max_bytes,
    throttling_strategy strategy = throttling_strategy::reject) {
    
    resource_quota quota(resource_type::memory, max_bytes, strategy);
    return std::make_unique<memory_quota_manager>(name, quota);
}

/**
 * @brief Create a CPU throttler
 */
inline std::unique_ptr<cpu_throttler> create_cpu_throttler(
    const std::string& name, double max_cpu_usage = 0.8,
    throttling_strategy strategy = throttling_strategy::delay) {
    
    cpu_throttle_config config;
    config.max_cpu_usage = max_cpu_usage;
    config.strategy = strategy;
    
    return std::make_unique<cpu_throttler>(name, config);
}

/**
 * @brief Create a resource manager
 */
inline std::unique_ptr<resource_manager> create_resource_manager(
    const std::string& name) {
    return std::make_unique<resource_manager>(name);
}

} // namespace monitoring_system