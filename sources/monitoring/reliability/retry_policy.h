#pragma once

#include <algorithm>
#include <chrono>
#include <functional>
#include <random>
#include <thread>
#include <vector>
#include <monitoring/core/result_types.h>
#include <monitoring/core/error_codes.h>

namespace monitoring_system {

enum class retry_strategy {
    fixed_delay,           // Fixed delay between retries
    exponential_backoff,   // Exponentially increasing delay
    linear_backoff,        // Linearly increasing delay
    fibonacci_backoff,     // Fibonacci sequence delay
    random_jitter,         // Random delay within range
    custom                 // Custom delay function
};

struct retry_config {
    retry_strategy strategy{retry_strategy::exponential_backoff};
    std::size_t max_attempts{3};
    std::chrono::milliseconds initial_delay{std::chrono::milliseconds(100)};
    std::chrono::milliseconds max_delay{std::chrono::seconds(30)};
    double backoff_multiplier{2.0};
    double jitter_factor{0.1}; // 10% jitter
    std::function<std::chrono::milliseconds(std::size_t)> custom_delay_func;
    
    // Predicate to determine if an error should trigger a retry
    std::function<bool(const error_info&)> should_retry;
    
    result_void validate() const {
        if (max_attempts == 0) {
            return result_void::error(monitoring_error_code::invalid_configuration,
                                    "Maximum attempts must be greater than 0");
        }
        if (initial_delay <= std::chrono::milliseconds(0)) {
            return result_void::error(monitoring_error_code::invalid_configuration,
                                    "Initial delay must be positive");
        }
        if (max_delay < initial_delay) {
            return result_void::error(monitoring_error_code::invalid_configuration,
                                    "Maximum delay must be greater than or equal to initial delay");
        }
        if (backoff_multiplier <= 1.0) {
            return result_void::error(monitoring_error_code::invalid_configuration,
                                    "Backoff multiplier must be greater than 1.0");
        }
        if (jitter_factor < 0.0 || jitter_factor > 1.0) {
            return result_void::error(monitoring_error_code::invalid_configuration,
                                    "Jitter factor must be between 0.0 and 1.0");
        }
        if (strategy == retry_strategy::custom && !custom_delay_func) {
            return result_void::error(monitoring_error_code::invalid_configuration,
                                    "Custom delay function must be provided for custom strategy");
        }
        return result_void::success();
    }
};

struct retry_metrics {
    std::size_t total_executions{0};
    std::size_t successful_executions{0};
    std::size_t failed_executions{0};
    std::size_t total_retries{0};
    std::chrono::milliseconds total_delay{0};
    std::chrono::steady_clock::time_point last_execution_time;
    
    double get_success_rate() const {
        if (total_executions == 0) return 0.0;
        return static_cast<double>(successful_executions) / total_executions;
    }
    
    double get_average_retries() const {
        if (total_executions == 0) return 0.0;
        return static_cast<double>(total_retries) / total_executions;
    }
    
    std::chrono::milliseconds get_average_delay() const {
        if (total_executions == 0) return std::chrono::milliseconds(0);
        return total_delay / total_executions;
    }
};

class delay_calculator {
public:
    explicit delay_calculator(const retry_config& config) : config_(config) {
        // Initialize Fibonacci sequence for fibonacci_backoff
        fibonacci_sequence_.push_back(1);
        fibonacci_sequence_.push_back(1);
    }
    
    std::chrono::milliseconds calculate_delay(std::size_t attempt) {
        std::chrono::milliseconds base_delay;
        
        switch (config_.strategy) {
            case retry_strategy::fixed_delay:
                base_delay = config_.initial_delay;
                break;
                
            case retry_strategy::exponential_backoff:
                base_delay = std::chrono::milliseconds(
                    static_cast<long long>(config_.initial_delay.count() * 
                                         std::pow(config_.backoff_multiplier, attempt - 1)));
                break;
                
            case retry_strategy::linear_backoff:
                base_delay = std::chrono::milliseconds(
                    config_.initial_delay.count() * attempt);
                break;
                
            case retry_strategy::fibonacci_backoff:
                base_delay = calculate_fibonacci_delay(attempt);
                break;
                
            case retry_strategy::random_jitter:
                base_delay = calculate_random_delay();
                break;
                
            case retry_strategy::custom:
                base_delay = config_.custom_delay_func(attempt);
                break;
        }
        
        // Apply jitter if configured
        if (config_.jitter_factor > 0.0) {
            base_delay = apply_jitter(base_delay);
        }
        
        // Ensure delay doesn't exceed maximum
        return std::min(base_delay, config_.max_delay);
    }
    
private:
    std::chrono::milliseconds calculate_fibonacci_delay(std::size_t attempt) {
        // Ensure we have enough Fibonacci numbers
        while (fibonacci_sequence_.size() < attempt) {
            auto next = fibonacci_sequence_[fibonacci_sequence_.size() - 1] + 
                       fibonacci_sequence_[fibonacci_sequence_.size() - 2];
            fibonacci_sequence_.push_back(next);
        }
        
        return std::chrono::milliseconds(
            config_.initial_delay.count() * fibonacci_sequence_[attempt - 1]);
    }
    
    std::chrono::milliseconds calculate_random_delay() {
        static thread_local std::random_device rd;
        static thread_local std::mt19937 gen(rd());
        
        std::uniform_int_distribution<long long> dist(
            config_.initial_delay.count(),
            config_.max_delay.count()
        );
        
        return std::chrono::milliseconds(dist(gen));
    }
    
    std::chrono::milliseconds apply_jitter(std::chrono::milliseconds base_delay) {
        static thread_local std::random_device rd;
        static thread_local std::mt19937 gen(rd());
        
        auto jitter_range = static_cast<long long>(base_delay.count() * config_.jitter_factor);
        std::uniform_int_distribution<long long> dist(-jitter_range, jitter_range);
        
        auto jittered_delay = base_delay.count() + dist(gen);
        return std::chrono::milliseconds(std::max(0LL, jittered_delay));
    }
    
    const retry_config& config_;
    std::vector<std::size_t> fibonacci_sequence_;
};

template<typename T>
class retry_executor {
public:
    using operation_type = std::function<result<T>()>;
    
    explicit retry_executor(std::string name, retry_config config = {})
        : name_(std::move(name))
        , config_(std::move(config))
        , delay_calculator_(config_)
    {
        if (auto validation = config_.validate(); !validation) {
            throw std::invalid_argument("Invalid retry configuration: " + 
                                      validation.get_error().message);
        }
        
        // Set default retry predicate if none provided
        if (!config_.should_retry) {
            config_.should_retry = [](const error_info& error) {
                // By default, retry on transient errors and operation failures
                return error.code == monitoring_error_code::operation_timeout ||
                       error.code == monitoring_error_code::system_resource_unavailable ||
                       error.code == monitoring_error_code::network_error ||
                       error.code == monitoring_error_code::service_unavailable ||
                       error.code == monitoring_error_code::operation_failed;
            };
        }
    }
    
    result<T> execute(operation_type operation) {
        metrics_.total_executions++;
        metrics_.last_execution_time = std::chrono::steady_clock::now();
        
        auto start_time = std::chrono::steady_clock::now();
        result<T> last_result = make_error<T>(monitoring_error_code::unknown_error, "No attempts made");
        
        for (std::size_t attempt = 1; attempt <= config_.max_attempts; ++attempt) {
            auto result = operation();
            
            if (result) {
                // Success - record metrics and return
                metrics_.successful_executions++;
                auto total_time = std::chrono::steady_clock::now() - start_time;
                metrics_.total_delay += std::chrono::duration_cast<std::chrono::milliseconds>(total_time);
                return result;
            }
            
            last_result = result;
            
            // Check if this error should trigger a retry
            if (!config_.should_retry(result.get_error())) {
                break;
            }
            
            // If this was the last attempt, don't delay
            if (attempt == config_.max_attempts) {
                break;
            }
            
            // This is a retry - increment counter
            metrics_.total_retries++;
            
            // Calculate and apply delay before retry
            auto delay = delay_calculator_.calculate_delay(attempt);
            std::this_thread::sleep_for(delay);
        }
        
        // All attempts failed
        metrics_.failed_executions++;
        auto total_time = std::chrono::steady_clock::now() - start_time;
        metrics_.total_delay += std::chrono::duration_cast<std::chrono::milliseconds>(total_time);
        
        return last_result;
    }
    
    // Execute with timeout
    result<T> execute_with_timeout(operation_type operation, 
                                  std::chrono::milliseconds timeout) {
        auto start_time = std::chrono::steady_clock::now();
        
        auto timed_operation = [&]() -> result<T> {
            auto now = std::chrono::steady_clock::now();
            if (now - start_time >= timeout) {
                return make_error<T>(monitoring_error_code::operation_timeout,
                                   "Operation timed out after " + 
                                   std::to_string(timeout.count()) + "ms");
            }
            return operation();
        };
        
        return execute(timed_operation);
    }
    
    const retry_metrics& get_metrics() const {
        return metrics_;
    }
    
    const retry_config& get_config() const {
        return config_;
    }
    
    const std::string& get_name() const {
        return name_;
    }
    
    void reset_metrics() {
        metrics_ = retry_metrics{};
    }
    
private:
    std::string name_;
    retry_config config_;
    delay_calculator delay_calculator_;
    retry_metrics metrics_;
};

// Utility functions for creating common retry configurations
inline retry_config create_exponential_backoff_config(
    std::size_t max_attempts = 3,
    std::chrono::milliseconds initial_delay = std::chrono::milliseconds(100),
    double multiplier = 2.0) {
    
    retry_config config;
    config.strategy = retry_strategy::exponential_backoff;
    config.max_attempts = max_attempts;
    config.initial_delay = initial_delay;
    config.backoff_multiplier = multiplier;
    return config;
}

inline retry_config create_fixed_delay_config(
    std::size_t max_attempts = 3,
    std::chrono::milliseconds delay = std::chrono::milliseconds(500)) {
    
    retry_config config;
    config.strategy = retry_strategy::fixed_delay;
    config.max_attempts = max_attempts;
    config.initial_delay = delay;
    return config;
}

inline retry_config create_fibonacci_backoff_config(
    std::size_t max_attempts = 5,
    std::chrono::milliseconds base_delay = std::chrono::milliseconds(100)) {
    
    retry_config config;
    config.strategy = retry_strategy::fibonacci_backoff;
    config.max_attempts = max_attempts;
    config.initial_delay = base_delay;
    return config;
}

// Factory function for retry executor
template<typename T>
std::unique_ptr<retry_executor<T>> create_retry_executor(
    std::string name,
    retry_config config = {}) {
    return std::make_unique<retry_executor<T>>(std::move(name), std::move(config));
}

// Retry executor registry
class retry_executor_registry {
public:
    static retry_executor_registry& instance() {
        static retry_executor_registry registry;
        return registry;
    }
    
    template<typename T>
    void register_executor(const std::string& name, 
                          std::shared_ptr<retry_executor<T>> executor) {
        std::lock_guard<std::mutex> lock(mutex_);
        executors_[name] = executor;
    }
    
    template<typename T>
    std::shared_ptr<retry_executor<T>> get_executor(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = executors_.find(name);
        if (it != executors_.end()) {
            return std::static_pointer_cast<retry_executor<T>>(it->second);
        }
        return nullptr;
    }
    
    void remove_executor(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        executors_.erase(name);
    }
    
    std::vector<std::string> get_all_names() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> names;
        names.reserve(executors_.size());
        for (const auto& pair : executors_) {
            names.push_back(pair.first);
        }
        return names;
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        executors_.clear();
    }
    
private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<void>> executors_;
};

// Global retry executor registry access
inline retry_executor_registry& global_retry_executor_registry() {
    return retry_executor_registry::instance();
}

} // namespace monitoring_system