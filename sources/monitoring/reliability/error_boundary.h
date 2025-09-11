#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <chrono>
#include <exception>
#include <optional>
#include <unordered_map>
#include <monitoring/core/result_types.h>
#include <monitoring/core/error_codes.h>

namespace monitoring_system {

enum class degradation_level {
    normal,         // Full functionality available
    limited,        // Some features disabled but core functions work
    minimal,        // Only essential functions available
    emergency       // Only critical safety functions
};

enum class error_boundary_policy {
    fail_fast,      // Propagate errors immediately
    isolate,        // Contain errors within boundary
    degrade,        // Gracefully degrade functionality
    fallback        // Use alternative implementation
};

struct error_boundary_config {
    std::string name;
    error_boundary_policy policy{error_boundary_policy::degrade};
    std::chrono::milliseconds recovery_timeout{std::chrono::minutes(5)};
    std::size_t error_threshold{3};
    std::chrono::milliseconds error_window{std::chrono::minutes(1)};
    degradation_level max_degradation{degradation_level::minimal};
    bool enable_automatic_recovery{true};
    bool enable_fallback_logging{true};
    
    result_void validate() const {
        if (name.empty()) {
            return result_void::error(monitoring_error_code::invalid_configuration,
                                    "Error boundary name cannot be empty");
        }
        if (error_threshold == 0) {
            return result_void::error(monitoring_error_code::invalid_configuration,
                                    "Error threshold must be greater than 0");
        }
        if (recovery_timeout <= std::chrono::milliseconds(0)) {
            return result_void::error(monitoring_error_code::invalid_configuration,
                                    "Recovery timeout must be positive");
        }
        return result_void::success();
    }
};

struct error_boundary_metrics {
    std::atomic<std::size_t> total_operations{0};
    std::atomic<std::size_t> successful_operations{0};
    std::atomic<std::size_t> failed_operations{0};
    std::atomic<std::size_t> degraded_operations{0};
    std::atomic<std::size_t> fallback_operations{0};
    std::atomic<std::size_t> recovery_attempts{0};
    std::atomic<std::size_t> successful_recoveries{0};
    std::chrono::steady_clock::time_point creation_time{std::chrono::steady_clock::now()};
    std::chrono::steady_clock::time_point last_error_time;
    std::chrono::steady_clock::time_point last_recovery_time;
    monitoring_error_code last_error_code{monitoring_error_code::success};
    
    // Copy constructor to handle atomic members
    error_boundary_metrics() = default;
    
    error_boundary_metrics(const error_boundary_metrics& other) 
        : total_operations(other.total_operations.load())
        , successful_operations(other.successful_operations.load())
        , failed_operations(other.failed_operations.load())
        , degraded_operations(other.degraded_operations.load())
        , fallback_operations(other.fallback_operations.load())
        , recovery_attempts(other.recovery_attempts.load())
        , successful_recoveries(other.successful_recoveries.load())
        , creation_time(other.creation_time)
        , last_error_time(other.last_error_time)
        , last_recovery_time(other.last_recovery_time)
        , last_error_code(other.last_error_code) {}
    
    error_boundary_metrics& operator=(const error_boundary_metrics& other) {
        if (this != &other) {
            total_operations = other.total_operations.load();
            successful_operations = other.successful_operations.load();
            failed_operations = other.failed_operations.load();
            degraded_operations = other.degraded_operations.load();
            fallback_operations = other.fallback_operations.load();
            recovery_attempts = other.recovery_attempts.load();
            successful_recoveries = other.successful_recoveries.load();
            creation_time = other.creation_time;
            last_error_time = other.last_error_time;
            last_recovery_time = other.last_recovery_time;
            last_error_code = other.last_error_code;
        }
        return *this;
    }
    
    double get_success_rate() const {
        auto total = total_operations.load();
        if (total == 0) return 1.0;
        return static_cast<double>(successful_operations.load()) / total;
    }
    
    double get_degradation_rate() const {
        auto total = total_operations.load();
        if (total == 0) return 0.0;
        return static_cast<double>(degraded_operations.load()) / total;
    }
    
    double get_recovery_rate() const {
        auto attempts = recovery_attempts.load();
        if (attempts == 0) return 1.0;
        return static_cast<double>(successful_recoveries.load()) / attempts;
    }
};

// Forward declaration for fallback strategies
template<typename T>
class fallback_strategy;

template<typename T>
class error_boundary {
public:
    using operation_type = std::function<result<T>()>;
    using fallback_type = std::function<result<T>(const error_info&, degradation_level)>;
    using error_handler_type = std::function<void(const error_info&, degradation_level)>;
    
    explicit error_boundary(std::string name, error_boundary_config config = {})
        : config_(std::move(config))
        , current_level_(degradation_level::normal)
        , consecutive_errors_(0)
        , last_error_window_start_(std::chrono::steady_clock::now())
    {
        config_.name = std::move(name);
        if (auto validation = config_.validate(); !validation) {
            throw std::invalid_argument("Invalid error boundary configuration: " + 
                                      validation.get_error().message);
        }
    }
    
    // Execute operation within error boundary
    result<T> execute(operation_type operation, fallback_type fallback = nullptr) {
        metrics_.total_operations++;
        
        // Check if we should attempt recovery
        if (should_attempt_recovery()) {
            attempt_recovery();
        }
        
        try {
            // Execute the operation based on current degradation level
            auto result = execute_with_degradation(operation, fallback);
            
            if (result) {
                record_success();
                return result;
            } else {
                return handle_operation_error(result.get_error(), fallback);
            }
        } catch (const std::exception& ex) {
            error_info error(monitoring_error_code::operation_failed, 
                           std::string("Exception caught: ") + ex.what());
            return handle_operation_error(error, fallback);
        } catch (...) {
            error_info error(monitoring_error_code::operation_failed, 
                           "Unknown exception caught");
            return handle_operation_error(error, fallback);
        }
    }
    
    // Set custom fallback strategy
    void set_fallback_strategy(std::shared_ptr<fallback_strategy<T>> strategy) {
        std::lock_guard<std::mutex> lock(state_mutex_);
        fallback_strategy_ = strategy;
    }
    
    // Set error handler
    void set_error_handler(error_handler_type handler) {
        std::lock_guard<std::mutex> lock(state_mutex_);
        error_handler_ = handler;
    }
    
    // Get current degradation level
    degradation_level get_degradation_level() const {
        std::lock_guard<std::mutex> lock(state_mutex_);
        return current_level_;
    }
    
    // Get metrics
    const error_boundary_metrics& get_metrics() const {
        return metrics_;
    }
    
    // Get configuration
    const error_boundary_config& get_config() const {
        return config_;
    }
    
    // Manual degradation control
    void force_degradation(degradation_level level) {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (level <= config_.max_degradation) {
            current_level_ = level;
        }
    }
    
    // Reset to normal operation
    void reset() {
        std::lock_guard<std::mutex> lock(state_mutex_);
        current_level_ = degradation_level::normal;
        consecutive_errors_ = 0;
        last_error_window_start_ = std::chrono::steady_clock::now();
    }
    
    // Health check
    result<bool> is_healthy() const {
        std::lock_guard<std::mutex> lock(state_mutex_);
        
        // Consider unhealthy if severely degraded
        if (current_level_ >= degradation_level::emergency) {
            return make_success(false);
        }
        
        // Check recent error rate
        auto success_rate = metrics_.get_success_rate();
        if (success_rate < 0.5) { // Less than 50% success rate
            return make_success(false);
        }
        
        return make_success(true);
    }
    
private:
    result<T> execute_with_degradation(operation_type operation, fallback_type fallback) {
        std::lock_guard<std::mutex> lock(state_mutex_);
        
        switch (current_level_) {
            case degradation_level::normal:
                return operation();
                
            case degradation_level::limited:
                // Try operation with reduced functionality
                try {
                    return operation();
                } catch (...) {
                    // If normal operation fails, try fallback
                    if (fallback) {
                        metrics_.degraded_operations++;
                        error_info degradation_error(monitoring_error_code::service_degraded,
                                                    "Operating in limited mode");
                        return fallback(degradation_error, current_level_);
                    }
                    throw;
                }
                
            case degradation_level::minimal:
                // Only use fallback in minimal mode
                if (fallback) {
                    metrics_.degraded_operations++;
                    error_info degradation_error(monitoring_error_code::service_degraded,
                                                "Operating in minimal mode");
                    return fallback(degradation_error, current_level_);
                } else if (fallback_strategy_) {
                    metrics_.fallback_operations++;
                    return fallback_strategy_->execute(current_level_);
                }
                break;
                
            case degradation_level::emergency:
                // Emergency mode - only critical operations
                if (fallback_strategy_) {
                    metrics_.fallback_operations++;
                    return fallback_strategy_->execute(current_level_);
                }
                return make_error<T>(monitoring_error_code::service_unavailable,
                                   "Service operating in emergency mode");
        }
        
        return make_error<T>(monitoring_error_code::service_unavailable,
                           "No fallback available for current degradation level");
    }
    
    result<T> handle_operation_error(const error_info& error, fallback_type fallback) {
        record_error(error);
        
        // Notify error handler if configured
        if (error_handler_) {
            try {
                error_handler_(error, current_level_);
            } catch (...) {
                // Don't let error handler exceptions escape
            }
        }
        
        // Apply policy-based error handling
        switch (config_.policy) {
            case error_boundary_policy::fail_fast:
                return result<T>(error);
                
            case error_boundary_policy::isolate:
                // Isolate error but don't propagate
                metrics_.failed_operations++;
                return make_error<T>(monitoring_error_code::service_degraded,
                                   "Error isolated by boundary: " + config_.name);
                
            case error_boundary_policy::degrade:
                // Try degraded operation
                degrade_service();
                if (fallback) {
                    metrics_.degraded_operations++;
                    return fallback(error, current_level_);
                }
                return result<T>(error);
                
            case error_boundary_policy::fallback:
                // Use fallback strategy
                if (fallback_strategy_) {
                    metrics_.fallback_operations++;
                    return fallback_strategy_->execute(current_level_);
                } else if (fallback) {
                    metrics_.fallback_operations++;
                    return fallback(error, current_level_);
                }
                return result<T>(error);
        }
        
        return result<T>(error);
    }
    
    void record_success() {
        std::lock_guard<std::mutex> lock(state_mutex_);
        metrics_.successful_operations++;
        
        // Reset consecutive errors on success
        consecutive_errors_ = 0;
        
        // Consider upgrading service level if we've been degraded
        if (current_level_ != degradation_level::normal && 
            should_attempt_recovery()) {
            attempt_recovery();
        }
    }
    
    void record_error(const error_info& error) {
        std::lock_guard<std::mutex> lock(state_mutex_);
        metrics_.failed_operations++;
        metrics_.last_error_time = std::chrono::steady_clock::now();
        metrics_.last_error_code = error.code;
        
        // Track consecutive errors in current window
        auto now = std::chrono::steady_clock::now();
        if (now - last_error_window_start_ > config_.error_window) {
            // Start new error window
            consecutive_errors_ = 1;
            last_error_window_start_ = now;
        } else {
            consecutive_errors_++;
        }
        
        // Check if we should degrade service
        if (consecutive_errors_ >= config_.error_threshold) {
            degrade_service();
        }
    }
    
    void degrade_service() {
        if (current_level_ < config_.max_degradation) {
            switch (current_level_) {
                case degradation_level::normal:
                    current_level_ = degradation_level::limited;
                    break;
                case degradation_level::limited:
                    current_level_ = degradation_level::minimal;
                    break;
                case degradation_level::minimal:
                    current_level_ = degradation_level::emergency;
                    break;
                case degradation_level::emergency:
                    // Already at maximum degradation
                    break;
            }
        }
    }
    
    bool should_attempt_recovery() const {
        if (!config_.enable_automatic_recovery || 
            current_level_ == degradation_level::normal) {
            return false;
        }
        
        auto now = std::chrono::steady_clock::now();
        return (now - metrics_.last_recovery_time) >= config_.recovery_timeout;
    }
    
    void attempt_recovery() {
        metrics_.recovery_attempts++;
        metrics_.last_recovery_time = std::chrono::steady_clock::now();
        
        // Try to upgrade service level
        if (current_level_ > degradation_level::normal) {
            switch (current_level_) {
                case degradation_level::emergency:
                    current_level_ = degradation_level::minimal;
                    break;
                case degradation_level::minimal:
                    current_level_ = degradation_level::limited;
                    break;
                case degradation_level::limited:
                    current_level_ = degradation_level::normal;
                    break;
                case degradation_level::normal:
                    // Already at normal level
                    break;
            }
            
            // Reset error count on recovery attempt
            consecutive_errors_ = 0;
            metrics_.successful_recoveries++;
        }
    }
    
    error_boundary_config config_;
    mutable std::mutex state_mutex_;
    degradation_level current_level_;
    std::size_t consecutive_errors_;
    std::chrono::steady_clock::time_point last_error_window_start_;
    error_boundary_metrics metrics_;
    
    std::shared_ptr<fallback_strategy<T>> fallback_strategy_;
    error_handler_type error_handler_;
};

// Fallback strategy interface
template<typename T>
class fallback_strategy {
public:
    virtual ~fallback_strategy() = default;
    virtual result<T> execute(degradation_level level) = 0;
    virtual std::string get_name() const = 0;
};

// Concrete fallback strategies
template<typename T>
class default_value_strategy : public fallback_strategy<T> {
public:
    explicit default_value_strategy(T default_value)
        : default_value_(std::move(default_value)) {}
    
    result<T> execute(degradation_level) override {
        return make_success(default_value_);
    }
    
    std::string get_name() const override {
        return "default_value";
    }
    
private:
    T default_value_;
};

template<typename T>
class cached_value_strategy : public fallback_strategy<T> {
public:
    explicit cached_value_strategy(std::chrono::milliseconds cache_ttl = std::chrono::minutes(5))
        : cache_ttl_(cache_ttl) {}
    
    result<T> execute(degradation_level) override {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        
        if (cached_value_.has_value()) {
            auto now = std::chrono::steady_clock::now();
            if (now - cache_timestamp_ <= cache_ttl_) {
                return make_success(cached_value_.value());
            }
        }
        
        return make_error<T>(monitoring_error_code::service_unavailable,
                           "No cached value available");
    }
    
    void update_cache(const T& value) {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        cached_value_ = value;
        cache_timestamp_ = std::chrono::steady_clock::now();
    }
    
    std::string get_name() const override {
        return "cached_value";
    }
    
private:
    std::chrono::milliseconds cache_ttl_;
    std::optional<T> cached_value_;
    std::chrono::steady_clock::time_point cache_timestamp_;
    mutable std::mutex cache_mutex_;
};

template<typename T>
class alternative_service_strategy : public fallback_strategy<T> {
public:
    using alternative_operation = std::function<result<T>()>;
    
    explicit alternative_service_strategy(alternative_operation alt_op)
        : alternative_operation_(std::move(alt_op)) {}
    
    result<T> execute(degradation_level) override {
        if (alternative_operation_) {
            return alternative_operation_();
        }
        return make_error<T>(monitoring_error_code::service_unavailable,
                           "No alternative service available");
    }
    
    std::string get_name() const override {
        return "alternative_service";
    }
    
private:
    alternative_operation alternative_operation_;
};

// Error boundary registry for managing multiple boundaries
class error_boundary_registry {
public:
    static error_boundary_registry& instance() {
        static error_boundary_registry registry;
        return registry;
    }
    
    template<typename T>
    void register_boundary(const std::string& name, 
                          std::shared_ptr<error_boundary<T>> boundary) {
        std::lock_guard<std::mutex> lock(mutex_);
        boundaries_[name] = boundary;
    }
    
    template<typename T>
    std::shared_ptr<error_boundary<T>> get_boundary(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = boundaries_.find(name);
        if (it != boundaries_.end()) {
            return std::static_pointer_cast<error_boundary<T>>(it->second);
        }
        return nullptr;
    }
    
    void remove_boundary(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        boundaries_.erase(name);
    }
    
    std::vector<std::string> get_all_names() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> names;
        names.reserve(boundaries_.size());
        for (const auto& pair : boundaries_) {
            names.push_back(pair.first);
        }
        return names;
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        boundaries_.clear();
    }
    
private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<void>> boundaries_;
};

// Global error boundary registry access
inline error_boundary_registry& global_error_boundary_registry() {
    return error_boundary_registry::instance();
}

// Utility functions
template<typename T>
std::unique_ptr<error_boundary<T>> create_error_boundary(
    std::string name,
    error_boundary_config config = {}) {
    return std::make_unique<error_boundary<T>>(std::move(name), std::move(config));
}

template<typename T>
std::unique_ptr<error_boundary<T>> create_degradable_boundary(
    std::string name,
    degradation_level max_degradation = degradation_level::minimal) {
    
    error_boundary_config config;
    config.policy = error_boundary_policy::degrade;
    config.max_degradation = max_degradation;
    config.enable_automatic_recovery = true;
    
    return create_error_boundary<T>(std::move(name), std::move(config));
}

template<typename T>
std::unique_ptr<error_boundary<T>> create_fallback_boundary(
    std::string name,
    std::shared_ptr<fallback_strategy<T>> strategy) {
    
    error_boundary_config config;
    config.policy = error_boundary_policy::fallback;
    
    auto boundary = create_error_boundary<T>(std::move(name), std::move(config));
    boundary->set_fallback_strategy(strategy);
    
    return boundary;
}

} // namespace monitoring_system