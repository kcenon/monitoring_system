#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <monitoring/reliability/circuit_breaker.h>
#include <monitoring/reliability/retry_policy.h>
#include <monitoring/core/result_types.h>

namespace monitoring_system {

struct fault_tolerance_config {
    circuit_breaker_config circuit_config;
    retry_config retry_config;
    bool enable_circuit_breaker{true};
    bool enable_retry{true};
    bool circuit_breaker_first{true}; // If true, circuit breaker wraps retry; otherwise retry wraps circuit breaker
    
    result_void validate() const {
        if (enable_circuit_breaker) {
            if (auto validation = circuit_config.validate(); !validation) {
                return validation;
            }
        }
        if (enable_retry) {
            if (auto validation = retry_config.validate(); !validation) {
                return validation;
            }
        }
        if (!enable_circuit_breaker && !enable_retry) {
            return result_void::error(monitoring_error_code::invalid_configuration,
                                    "At least one fault tolerance mechanism must be enabled");
        }
        return result_void::success();
    }
};

struct fault_tolerance_metrics {
    circuit_breaker_metrics circuit_metrics;
    retry_metrics retry_metrics;
    std::size_t total_operations{0};
    std::size_t successful_operations{0};
    std::size_t failed_operations{0};
    std::chrono::steady_clock::time_point start_time{std::chrono::steady_clock::now()};
    
    // Default constructor
    fault_tolerance_metrics() = default;
    
    // Copy constructor to handle atomic members
    fault_tolerance_metrics(const fault_tolerance_metrics& other) 
        : retry_metrics(other.retry_metrics)
        , total_operations(other.total_operations)
        , successful_operations(other.successful_operations) 
        , failed_operations(other.failed_operations)
        , start_time(other.start_time) {
        // Manually copy atomic values from circuit_metrics
        circuit_metrics.total_calls = other.circuit_metrics.total_calls.load();
        circuit_metrics.successful_calls = other.circuit_metrics.successful_calls.load();
        circuit_metrics.failed_calls = other.circuit_metrics.failed_calls.load();
        circuit_metrics.rejected_calls = other.circuit_metrics.rejected_calls.load();
        circuit_metrics.state_transitions = other.circuit_metrics.state_transitions.load();
        circuit_metrics.last_failure_time = other.circuit_metrics.last_failure_time;
        circuit_metrics.last_success_time = other.circuit_metrics.last_success_time;
    }
    
    // Assignment operator
    fault_tolerance_metrics& operator=(const fault_tolerance_metrics& other) {
        if (this != &other) {
            retry_metrics = other.retry_metrics;
            total_operations = other.total_operations;
            successful_operations = other.successful_operations;
            failed_operations = other.failed_operations;
            start_time = other.start_time;
            
            // Manually copy atomic values from circuit_metrics
            circuit_metrics.total_calls = other.circuit_metrics.total_calls.load();
            circuit_metrics.successful_calls = other.circuit_metrics.successful_calls.load();
            circuit_metrics.failed_calls = other.circuit_metrics.failed_calls.load();
            circuit_metrics.rejected_calls = other.circuit_metrics.rejected_calls.load();
            circuit_metrics.state_transitions = other.circuit_metrics.state_transitions.load();
            circuit_metrics.last_failure_time = other.circuit_metrics.last_failure_time;
            circuit_metrics.last_success_time = other.circuit_metrics.last_success_time;
        }
        return *this;
    }
    
    double get_overall_success_rate() const {
        if (total_operations == 0) return 0.0;
        return static_cast<double>(successful_operations) / total_operations;
    }
    
    std::chrono::seconds get_uptime() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::seconds>(now - start_time);
    }
};

template<typename T>
class fault_tolerance_manager {
public:
    using operation_type = std::function<result<T>()>;
    using fallback_type = std::function<result<T>()>;
    
    explicit fault_tolerance_manager(std::string name, fault_tolerance_config config = {})
        : name_(std::move(name))
        , config_(std::move(config))
    {
        if (auto validation = config_.validate(); !validation) {
            throw std::invalid_argument("Invalid fault tolerance configuration: " + 
                                      validation.get_error().message);
        }
        
        if (config_.enable_circuit_breaker) {
            circuit_breaker_ = std::make_unique<circuit_breaker<T>>(
                name_ + "_circuit_breaker", config_.circuit_config);
        }
        
        if (config_.enable_retry) {
            retry_executor_ = std::make_unique<retry_executor<T>>(
                name_ + "_retry_executor", config_.retry_config);
        }
    }
    
    // Execute operation with fault tolerance
    result<T> execute(operation_type operation, fallback_type fallback = nullptr) {
        metrics_.total_operations++;
        
        result<T> final_result = make_error<T>(monitoring_error_code::unknown_error, "No operation executed");
        
        if (config_.circuit_breaker_first) {
            // Circuit breaker wraps retry
            final_result = execute_with_circuit_breaker_first(operation, fallback);
        } else {
            // Retry wraps circuit breaker
            final_result = execute_with_retry_first(operation, fallback);
        }
        
        if (final_result) {
            metrics_.successful_operations++;
        } else {
            metrics_.failed_operations++;
        }
        
        return final_result;
    }
    
    // Execute with timeout
    result<T> execute_with_timeout(operation_type operation, 
                                  std::chrono::milliseconds timeout,
                                  fallback_type fallback = nullptr) {
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
        
        return execute(timed_operation, fallback);
    }
    
    // Get comprehensive metrics
    fault_tolerance_metrics get_metrics() const {
        fault_tolerance_metrics combined_metrics = metrics_;
        
        if (circuit_breaker_) {
            combined_metrics.circuit_metrics = circuit_breaker_->get_metrics();
        }
        
        if (retry_executor_) {
            combined_metrics.retry_metrics = retry_executor_->get_metrics();
        }
        
        return combined_metrics;
    }
    
    // Get individual components
    circuit_breaker<T>* get_circuit_breaker() const {
        return circuit_breaker_.get();
    }
    
    retry_executor<T>* get_retry_executor() const {
        return retry_executor_.get();
    }
    
    // Reset all metrics
    void reset_metrics() {
        metrics_ = fault_tolerance_metrics{};
        if (circuit_breaker_) {
            circuit_breaker_->reset();
        }
        if (retry_executor_) {
            retry_executor_->reset_metrics();
        }
    }
    
    // Get configuration
    const fault_tolerance_config& get_config() const {
        return config_;
    }
    
    // Get name
    const std::string& get_name() const {
        return name_;
    }
    
    // Health check
    result<bool> is_healthy() const {
        if (circuit_breaker_) {
            auto state = circuit_breaker_->get_state();
            if (state == circuit_state::open) {
                return make_success(false);
            }
        }
        
        auto metrics = get_metrics();
        
        // Consider unhealthy if success rate is too low
        if (metrics.total_operations > 10 && 
            metrics.get_overall_success_rate() < 0.5) {
            return make_success(false);
        }
        
        return make_success(true);
    }
    
private:
    result<T> execute_with_circuit_breaker_first(operation_type operation, fallback_type fallback) {
        if (config_.enable_circuit_breaker && config_.enable_retry) {
            // Circuit breaker wraps retry
            auto retry_operation = [this, operation]() -> result<T> {
                return retry_executor_->execute(operation);
            };
            return circuit_breaker_->execute(retry_operation, fallback);
        } else if (config_.enable_circuit_breaker) {
            // Only circuit breaker
            return circuit_breaker_->execute(operation, fallback);
        } else if (config_.enable_retry) {
            // Only retry
            return retry_executor_->execute(operation);
        } else {
            // Neither enabled (should not happen due to validation)
            return operation();
        }
    }
    
    result<T> execute_with_retry_first(operation_type operation, fallback_type fallback) {
        if (config_.enable_retry && config_.enable_circuit_breaker) {
            // Retry wraps circuit breaker
            auto circuit_operation = [this, operation, fallback]() -> result<T> {
                return circuit_breaker_->execute(operation, fallback);
            };
            return retry_executor_->execute(circuit_operation);
        } else if (config_.enable_retry) {
            // Only retry
            return retry_executor_->execute(operation);
        } else if (config_.enable_circuit_breaker) {
            // Only circuit breaker
            return circuit_breaker_->execute(operation, fallback);
        } else {
            // Neither enabled (should not happen due to validation)
            return operation();
        }
    }
    
    std::string name_;
    fault_tolerance_config config_;
    std::unique_ptr<circuit_breaker<T>> circuit_breaker_;
    std::unique_ptr<retry_executor<T>> retry_executor_;
    fault_tolerance_metrics metrics_;
};

// Factory functions for common configurations
template<typename T>
std::unique_ptr<fault_tolerance_manager<T>> create_fault_tolerance_manager(
    std::string name,
    fault_tolerance_config config = {}) {
    return std::make_unique<fault_tolerance_manager<T>>(std::move(name), std::move(config));
}

template<typename T>
std::unique_ptr<fault_tolerance_manager<T>> create_resilient_manager(
    std::string name,
    std::size_t max_retries = 3,
    std::size_t failure_threshold = 5,
    std::chrono::milliseconds circuit_timeout = std::chrono::seconds(60)) {
    
    fault_tolerance_config config;
    config.enable_circuit_breaker = true;
    config.enable_retry = true;
    config.circuit_breaker_first = true;
    
    config.circuit_config.failure_threshold = failure_threshold;
    config.circuit_config.timeout = circuit_timeout;
    
    config.retry_config = create_exponential_backoff_config(max_retries);
    
    return create_fault_tolerance_manager<T>(std::move(name), std::move(config));
}

template<typename T>
std::unique_ptr<fault_tolerance_manager<T>> create_fast_fail_manager(
    std::string name,
    std::size_t failure_threshold = 3,
    std::chrono::milliseconds circuit_timeout = std::chrono::seconds(30)) {
    
    fault_tolerance_config config;
    config.enable_circuit_breaker = true;
    config.enable_retry = false;
    
    config.circuit_config.failure_threshold = failure_threshold;
    config.circuit_config.timeout = circuit_timeout;
    
    return create_fault_tolerance_manager<T>(std::move(name), std::move(config));
}

// Fault tolerance manager registry
class fault_tolerance_registry {
public:
    static fault_tolerance_registry& instance() {
        static fault_tolerance_registry registry;
        return registry;
    }
    
    template<typename T>
    void register_manager(const std::string& name, 
                         std::shared_ptr<fault_tolerance_manager<T>> manager) {
        std::lock_guard<std::mutex> lock(mutex_);
        managers_[name] = manager;
    }
    
    template<typename T>
    std::shared_ptr<fault_tolerance_manager<T>> get_manager(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = managers_.find(name);
        if (it != managers_.end()) {
            return std::static_pointer_cast<fault_tolerance_manager<T>>(it->second);
        }
        return nullptr;
    }
    
    void remove_manager(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        managers_.erase(name);
    }
    
    std::vector<std::string> get_all_names() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> names;
        names.reserve(managers_.size());
        for (const auto& pair : managers_) {
            names.push_back(pair.first);
        }
        return names;
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        managers_.clear();
    }
    
private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<void>> managers_;
};

// Global fault tolerance registry access
inline fault_tolerance_registry& global_fault_tolerance_registry() {
    return fault_tolerance_registry::instance();
}

} // namespace monitoring_system