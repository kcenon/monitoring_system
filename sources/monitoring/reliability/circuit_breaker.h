#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <monitoring/core/result_types.h>
#include <monitoring/core/error_codes.h>

namespace monitoring_system {

enum class circuit_state {
    closed,      // Normal operation
    open,        // Circuit is open, rejecting calls
    half_open    // Testing if service has recovered
};

struct circuit_breaker_config {
    std::chrono::milliseconds timeout{std::chrono::seconds(60)};
    std::size_t failure_threshold{5};
    std::size_t success_threshold{3};
    std::chrono::milliseconds reset_timeout{std::chrono::seconds(30)};
    double failure_rate_threshold{0.5}; // 50% failure rate
    std::size_t minimum_calls{10};
    
    result_void validate() const {
        if (failure_threshold == 0) {
            return result_void::error(monitoring_error_code::invalid_configuration,
                                    "Failure threshold must be greater than 0");
        }
        if (success_threshold == 0) {
            return result_void::error(monitoring_error_code::invalid_configuration,
                                    "Success threshold must be greater than 0");
        }
        if (failure_rate_threshold < 0.0 || failure_rate_threshold > 1.0) {
            return result_void::error(monitoring_error_code::invalid_configuration,
                                    "Failure rate threshold must be between 0.0 and 1.0");
        }
        if (timeout <= std::chrono::milliseconds(0)) {
            return result_void::error(monitoring_error_code::invalid_configuration,
                                    "Timeout must be positive");
        }
        return result_void::success();
    }
};

struct circuit_breaker_metrics {
    std::atomic<std::size_t> total_calls{0};
    std::atomic<std::size_t> successful_calls{0};
    std::atomic<std::size_t> failed_calls{0};
    std::atomic<std::size_t> rejected_calls{0};
    std::atomic<std::size_t> state_transitions{0};
    std::chrono::steady_clock::time_point last_failure_time;
    std::chrono::steady_clock::time_point last_success_time;
    
    // Default constructor
    circuit_breaker_metrics() = default;
    
    // Copy constructor to handle atomic members
    circuit_breaker_metrics(const circuit_breaker_metrics& other) 
        : total_calls(other.total_calls.load())
        , successful_calls(other.successful_calls.load())
        , failed_calls(other.failed_calls.load())
        , rejected_calls(other.rejected_calls.load())
        , state_transitions(other.state_transitions.load())
        , last_failure_time(other.last_failure_time)
        , last_success_time(other.last_success_time) {}
    
    // Assignment operator to handle atomic members
    circuit_breaker_metrics& operator=(const circuit_breaker_metrics& other) {
        if (this != &other) {
            total_calls = other.total_calls.load();
            successful_calls = other.successful_calls.load();
            failed_calls = other.failed_calls.load();
            rejected_calls = other.rejected_calls.load();
            state_transitions = other.state_transitions.load();
            last_failure_time = other.last_failure_time;
            last_success_time = other.last_success_time;
        }
        return *this;
    }
    
    double get_failure_rate() const {
        auto total = total_calls.load();
        if (total == 0) return 0.0;
        return static_cast<double>(failed_calls.load()) / total;
    }
    
    double get_success_rate() const {
        auto total = total_calls.load();
        if (total == 0) return 0.0;
        return static_cast<double>(successful_calls.load()) / total;
    }
    
    void reset_window() {
        total_calls = 0;
        successful_calls = 0;
        failed_calls = 0;
        // Don't reset rejected_calls and state_transitions as they're cumulative
    }
};

template<typename T>
class circuit_breaker {
public:
    using operation_type = std::function<result<T>()>;
    using fallback_type = std::function<result<T>()>;
    
    explicit circuit_breaker(std::string name, circuit_breaker_config config = {})
        : name_(std::move(name))
        , config_(std::move(config))
        , state_(circuit_state::closed)
        , consecutive_failures_(0)
        , consecutive_successes_(0)
        , last_failure_time_(std::chrono::steady_clock::time_point::min())
    {
        if (auto validation = config_.validate(); !validation) {
            throw std::invalid_argument("Invalid circuit breaker configuration: " + 
                                      validation.get_error().message);
        }
    }
    
    // Execute operation with circuit breaker protection
    result<T> execute(operation_type operation, fallback_type fallback = nullptr) {
        if (!can_execute()) {
            metrics_.rejected_calls++;
            if (fallback) {
                return fallback();
            }
            return make_error<T>(monitoring_error_code::circuit_breaker_open,
                               "Circuit breaker is open for: " + name_);
        }
        
        auto start_time = std::chrono::steady_clock::now();
        auto call_result = operation();
        auto duration = std::chrono::steady_clock::now() - start_time;
        
        // Check for timeout
        if (duration > config_.timeout) {
            record_failure();
            return make_error<T>(monitoring_error_code::operation_timeout,
                               "Operation timed out after " + 
                               std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(duration).count()) + "ms");
        }
        
        if (call_result) {
            record_success();
            return call_result;
        } else {
            record_failure();
            if (fallback) {
                return fallback();
            }
            return call_result;
        }
    }
    
    // Get current state
    circuit_state get_state() const {
        std::lock_guard<std::mutex> lock(state_mutex_);
        return state_;
    }
    
    // Get metrics (returns reference to avoid copying atomics)
    const circuit_breaker_metrics& get_metrics() const {
        return metrics_;
    }
    
    // Get configuration
    const circuit_breaker_config& get_config() const {
        return config_;
    }
    
    // Get name
    const std::string& get_name() const {
        return name_;
    }
    
    // Reset circuit breaker to closed state
    void reset() {
        std::lock_guard<std::mutex> lock(state_mutex_);
        state_ = circuit_state::closed;
        consecutive_failures_ = 0;
        consecutive_successes_ = 0;
        metrics_.reset_window();
        metrics_.state_transitions++;
    }
    
    // Force circuit breaker to specific state (for testing)
    void force_state(circuit_state state) {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (state_ != state) {
            state_ = state;
            metrics_.state_transitions++;
        }
    }
    
private:
    bool can_execute() {
        std::lock_guard<std::mutex> lock(state_mutex_);
        
        auto now = std::chrono::steady_clock::now();
        
        switch (state_) {
            case circuit_state::closed:
                return true;
                
            case circuit_state::open:
                // Check if we should transition to half-open
                if (now - last_failure_time_ >= config_.reset_timeout) {
                    state_ = circuit_state::half_open;
                    consecutive_successes_ = 0;
                    metrics_.state_transitions++;
                    return true;
                }
                return false;
                
            case circuit_state::half_open:
                return true;
        }
        return false;
    }
    
    void record_success() {
        std::lock_guard<std::mutex> lock(state_mutex_);
        
        metrics_.total_calls++;
        metrics_.successful_calls++;
        metrics_.last_success_time = std::chrono::steady_clock::now();
        
        consecutive_failures_ = 0;
        consecutive_successes_++;
        
        if (state_ == circuit_state::half_open && 
            consecutive_successes_ >= config_.success_threshold) {
            state_ = circuit_state::closed;
            metrics_.state_transitions++;
            metrics_.reset_window(); // Reset metrics when returning to normal
        }
    }
    
    void record_failure() {
        std::lock_guard<std::mutex> lock(state_mutex_);
        
        metrics_.total_calls++;
        metrics_.failed_calls++;
        last_failure_time_ = std::chrono::steady_clock::now();
        metrics_.last_failure_time = last_failure_time_;
        
        consecutive_successes_ = 0;
        consecutive_failures_++;
        
        // Check if we should open the circuit
        bool should_open = false;
        
        if (state_ == circuit_state::closed) {
            // Check failure threshold or failure rate
            if (consecutive_failures_ >= config_.failure_threshold) {
                should_open = true;
            } else if (metrics_.total_calls >= config_.minimum_calls &&
                      metrics_.get_failure_rate() >= config_.failure_rate_threshold) {
                should_open = true;
            }
        } else if (state_ == circuit_state::half_open) {
            // Any failure in half-open state opens the circuit
            should_open = true;
        }
        
        if (should_open && state_ != circuit_state::open) {
            state_ = circuit_state::open;
            metrics_.state_transitions++;
        }
    }
    
    std::string name_;
    circuit_breaker_config config_;
    mutable std::mutex state_mutex_;
    circuit_state state_;
    std::size_t consecutive_failures_;
    std::size_t consecutive_successes_;
    std::chrono::steady_clock::time_point last_failure_time_;
    circuit_breaker_metrics metrics_;
};

// Utility function to create circuit breaker
template<typename T>
std::unique_ptr<circuit_breaker<T>> create_circuit_breaker(
    std::string name, 
    circuit_breaker_config config = {}) {
    return std::make_unique<circuit_breaker<T>>(std::move(name), std::move(config));
}

// Circuit breaker registry for managing multiple circuit breakers
class circuit_breaker_registry {
public:
    static circuit_breaker_registry& instance() {
        static circuit_breaker_registry registry;
        return registry;
    }
    
    template<typename T>
    void register_circuit_breaker(const std::string& name, 
                                 std::shared_ptr<circuit_breaker<T>> breaker) {
        std::lock_guard<std::mutex> lock(mutex_);
        circuit_breakers_[name] = breaker;
    }
    
    template<typename T>
    std::shared_ptr<circuit_breaker<T>> get_circuit_breaker(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = circuit_breakers_.find(name);
        if (it != circuit_breakers_.end()) {
            return std::static_pointer_cast<circuit_breaker<T>>(it->second);
        }
        return nullptr;
    }
    
    void remove_circuit_breaker(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        circuit_breakers_.erase(name);
    }
    
    std::vector<std::string> get_all_names() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> names;
        names.reserve(circuit_breakers_.size());
        for (const auto& pair : circuit_breakers_) {
            names.push_back(pair.first);
        }
        return names;
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        circuit_breakers_.clear();
    }
    
private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<void>> circuit_breakers_;
};

// Global circuit breaker registry access
inline circuit_breaker_registry& global_circuit_breaker_registry() {
    return circuit_breaker_registry::instance();
}

} // namespace monitoring_system