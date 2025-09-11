#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <chrono>
#include <monitoring/reliability/error_boundary.h>
#include <monitoring/core/result_types.h>
#include <monitoring/core/error_codes.h>

namespace monitoring_system {

enum class service_priority {
    critical,       // Must always be available
    important,      // Should be available, can degrade
    normal,         // Can be disabled under load
    optional        // First to be disabled
};

enum class degradation_trigger {
    error_rate,     // Based on error rate thresholds
    resource_usage, // Based on resource consumption
    manual,         // Manually triggered
    external_signal // From external monitoring
};

struct service_config {
    std::string name;
    service_priority priority{service_priority::normal};
    double error_rate_threshold{0.3}; // 30% error rate triggers degradation
    std::chrono::milliseconds degradation_cooldown{std::chrono::minutes(1)};
    bool enable_automatic_recovery{true};
    std::chrono::milliseconds recovery_check_interval{std::chrono::seconds(30)};
    
    result_void validate() const {
        if (name.empty()) {
            return result_void::error(monitoring_error_code::invalid_configuration,
                                    "Service name cannot be empty");
        }
        if (error_rate_threshold < 0.0 || error_rate_threshold > 1.0) {
            return result_void::error(monitoring_error_code::invalid_configuration,
                                    "Error rate threshold must be between 0.0 and 1.0");
        }
        return result_void::success();
    }
};

struct degradation_plan {
    std::string name;
    std::vector<std::string> services_to_degrade; // Ordered by degradation sequence
    std::vector<std::string> services_to_disable; // Services to completely disable
    degradation_level target_level;
    std::string description;
    
    result_void validate() const {
        if (name.empty()) {
            return result_void::error(monitoring_error_code::invalid_configuration,
                                    "Degradation plan name cannot be empty");
        }
        return result_void::success();
    }
};

struct graceful_degradation_metrics {
    std::atomic<std::size_t> total_degradations{0};
    std::atomic<std::size_t> successful_degradations{0};
    std::atomic<std::size_t> failed_degradations{0};
    std::atomic<std::size_t> recovery_attempts{0};
    std::atomic<std::size_t> successful_recoveries{0};
    std::atomic<std::size_t> services_currently_degraded{0};
    std::atomic<std::size_t> services_currently_disabled{0};
    std::chrono::steady_clock::time_point last_degradation_time;
    std::chrono::steady_clock::time_point last_recovery_time;
    
    // Copy constructor to handle atomic members
    graceful_degradation_metrics() = default;
    
    graceful_degradation_metrics(const graceful_degradation_metrics& other) 
        : total_degradations(other.total_degradations.load())
        , successful_degradations(other.successful_degradations.load())
        , failed_degradations(other.failed_degradations.load())
        , recovery_attempts(other.recovery_attempts.load())
        , successful_recoveries(other.successful_recoveries.load())
        , services_currently_degraded(other.services_currently_degraded.load())
        , services_currently_disabled(other.services_currently_disabled.load())
        , last_degradation_time(other.last_degradation_time)
        , last_recovery_time(other.last_recovery_time) {}
    
    graceful_degradation_metrics& operator=(const graceful_degradation_metrics& other) {
        if (this != &other) {
            total_degradations = other.total_degradations.load();
            successful_degradations = other.successful_degradations.load();
            failed_degradations = other.failed_degradations.load();
            recovery_attempts = other.recovery_attempts.load();
            successful_recoveries = other.successful_recoveries.load();
            services_currently_degraded = other.services_currently_degraded.load();
            services_currently_disabled = other.services_currently_disabled.load();
            last_degradation_time = other.last_degradation_time;
            last_recovery_time = other.last_recovery_time;
        }
        return *this;
    }
    
    double get_degradation_success_rate() const {
        auto total = total_degradations.load();
        if (total == 0) return 1.0;
        return static_cast<double>(successful_degradations.load()) / total;
    }
    
    double get_recovery_success_rate() const {
        auto attempts = recovery_attempts.load();
        if (attempts == 0) return 1.0;
        return static_cast<double>(successful_recoveries.load()) / attempts;
    }
};

class graceful_degradation_manager {
public:
    using degradation_callback = std::function<void(const std::string&, degradation_level, degradation_level)>;
    using health_checker = std::function<result<bool>(const std::string&)>;
    
    explicit graceful_degradation_manager(std::string name = "default")
        : name_(std::move(name))
        , global_degradation_level_(degradation_level::normal) {}
    
    // Service management
    result_void register_service(const service_config& config) {
        if (auto validation = config.validate(); !validation) {
            return validation;
        }
        
        std::lock_guard<std::mutex> lock(services_mutex_);
        services_[config.name] = config;
        service_states_[config.name] = degradation_level::normal;
        
        return result_void::success();
    }
    
    result_void unregister_service(const std::string& service_name) {
        std::lock_guard<std::mutex> lock(services_mutex_);
        services_.erase(service_name);
        service_states_.erase(service_name);
        
        return result_void::success();
    }
    
    // Degradation plan management
    result_void add_degradation_plan(const degradation_plan& plan) {
        if (auto validation = plan.validate(); !validation) {
            return validation;
        }
        
        std::lock_guard<std::mutex> lock(plans_mutex_);
        degradation_plans_[plan.name] = plan;
        
        return result_void::success();
    }
    
    result_void remove_degradation_plan(const std::string& plan_name) {
        std::lock_guard<std::mutex> lock(plans_mutex_);
        degradation_plans_.erase(plan_name);
        
        return result_void::success();
    }
    
    // Execute degradation
    result_void degrade_service(const std::string& service_name, 
                               degradation_level target_level,
                               const std::string& reason = "") {
        std::lock_guard<std::mutex> lock(services_mutex_);
        
        auto service_it = services_.find(service_name);
        if (service_it == services_.end()) {
            return result_void::error(monitoring_error_code::not_found,
                                    "Service not found: " + service_name);
        }
        
        auto current_level = service_states_[service_name];
        if (current_level == target_level) {
            return result_void::success(); // Already at target level
        }
        
        // Execute degradation
        service_states_[service_name] = target_level;
        
        // Update metrics
        metrics_.total_degradations++;
        metrics_.successful_degradations++;
        metrics_.last_degradation_time = std::chrono::steady_clock::now();
        
        if (target_level > current_level) {
            metrics_.services_currently_degraded++;
        }
        
        // Notify callback
        if (degradation_callback_) {
            try {
                degradation_callback_(service_name, current_level, target_level);
            } catch (...) {
                // Don't let callback exceptions escape
            }
        }
        
        return result_void::success();
    }
    
    // Execute degradation plan
    result_void execute_plan(const std::string& plan_name, const std::string& reason = "") {
        degradation_plan plan;
        {
            std::lock_guard<std::mutex> lock(plans_mutex_);
            auto it = degradation_plans_.find(plan_name);
            if (it == degradation_plans_.end()) {
                return result_void::error(monitoring_error_code::not_found,
                                        "Degradation plan not found: " + plan_name);
            }
            plan = it->second;
        }
        
        std::size_t successful_degradations = 0;
        
        // Degrade services according to plan
        for (const auto& service_name : plan.services_to_degrade) {
            auto result = degrade_service(service_name, plan.target_level, 
                                        "Plan: " + plan_name + " - " + reason);
            if (result) {
                successful_degradations++;
            }
        }
        
        // Disable services according to plan
        for (const auto& service_name : plan.services_to_disable) {
            auto result = degrade_service(service_name, degradation_level::emergency, 
                                        "Plan: " + plan_name + " (disabled) - " + reason);
            if (result) {
                successful_degradations++;
                metrics_.services_currently_disabled++;
            }
        }
        
        // Update global degradation level
        {
            std::lock_guard<std::mutex> lock(services_mutex_);
            global_degradation_level_ = std::max(global_degradation_level_, plan.target_level);
        }
        
        return result_void::success();
    }
    
    // Automatic degradation based on error rates
    result_void check_and_degrade() {
        std::lock_guard<std::mutex> lock(services_mutex_);
        
        for (auto& [service_name, config] : services_) {
            if (health_checker_) {
                auto health_result = health_checker_(service_name);
                if (health_result && !health_result.value()) {
                    // Service is unhealthy - calculate error rate
                    // This is a simplified implementation - in practice, you'd track actual metrics
                    auto current_level = service_states_[service_name];
                    if (current_level == degradation_level::normal) {
                        // Degrade based on priority
                        degradation_level target_level;
                        switch (config.priority) {
                            case service_priority::critical:
                                target_level = degradation_level::limited;
                                break;
                            case service_priority::important:
                                target_level = degradation_level::minimal;
                                break;
                            case service_priority::normal:
                            case service_priority::optional:
                                target_level = degradation_level::emergency;
                                break;
                        }
                        
                        degrade_service(service_name, target_level, "Automatic degradation");
                    }
                }
            }
        }
        
        return result_void::success();
    }
    
    // Recovery operations
    result_void recover_service(const std::string& service_name) {
        std::lock_guard<std::mutex> lock(services_mutex_);
        
        auto service_it = services_.find(service_name);
        if (service_it == services_.end()) {
            return result_void::error(monitoring_error_code::not_found,
                                    "Service not found: " + service_name);
        }
        
        auto current_level = service_states_[service_name];
        if (current_level == degradation_level::normal) {
            return result_void::success(); // Already at normal level
        }
        
        // Attempt recovery
        metrics_.recovery_attempts++;
        
        if (health_checker_) {
            auto health_result = health_checker_(service_name);
            if (!health_result || !health_result.value()) {
                // Service still not healthy
                return result_void::error(monitoring_error_code::operation_failed,
                                        "Service health check failed during recovery");
            }
        }
        
        // Recover service
        service_states_[service_name] = degradation_level::normal;
        metrics_.successful_recoveries++;
        metrics_.last_recovery_time = std::chrono::steady_clock::now();
        
        if (current_level > degradation_level::normal) {
            if (metrics_.services_currently_degraded > 0) {
                metrics_.services_currently_degraded--;
            }
        }
        
        if (current_level == degradation_level::emergency) {
            if (metrics_.services_currently_disabled > 0) {
                metrics_.services_currently_disabled--;
            }
        }
        
        // Notify callback
        if (degradation_callback_) {
            try {
                degradation_callback_(service_name, current_level, degradation_level::normal);
            } catch (...) {
                // Don't let callback exceptions escape
            }
        }
        
        return result_void::success();
    }
    
    result_void recover_all_services() {
        std::vector<std::string> service_names;
        {
            std::lock_guard<std::mutex> lock(services_mutex_);
            for (const auto& [name, config] : services_) {
                service_names.push_back(name);
            }
        }
        
        for (const auto& service_name : service_names) {
            recover_service(service_name);
        }
        
        // Reset global degradation level
        {
            std::lock_guard<std::mutex> lock(services_mutex_);
            global_degradation_level_ = degradation_level::normal;
        }
        
        return result_void::success();
    }
    
    // Query operations
    degradation_level get_service_level(const std::string& service_name) const {
        std::lock_guard<std::mutex> lock(services_mutex_);
        auto it = service_states_.find(service_name);
        if (it != service_states_.end()) {
            return it->second;
        }
        return degradation_level::normal; // Default if not found
    }
    
    degradation_level get_global_level() const {
        std::lock_guard<std::mutex> lock(services_mutex_);
        return global_degradation_level_;
    }
    
    std::vector<std::string> get_degraded_services() const {
        std::lock_guard<std::mutex> lock(services_mutex_);
        std::vector<std::string> degraded;
        for (const auto& [name, level] : service_states_) {
            if (level != degradation_level::normal) {
                degraded.push_back(name);
            }
        }
        return degraded;
    }
    
    // Configuration
    void set_degradation_callback(degradation_callback callback) {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        degradation_callback_ = callback;
    }
    
    void set_health_checker(health_checker checker) {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        health_checker_ = checker;
    }
    
    // Metrics
    const graceful_degradation_metrics& get_metrics() const {
        return metrics_;
    }
    
    // Health check
    result<bool> is_healthy() const {
        std::lock_guard<std::mutex> lock(services_mutex_);
        
        // Consider unhealthy if too many services are degraded
        auto total_services = services_.size();
        auto degraded_services = metrics_.services_currently_degraded.load();
        
        if (total_services > 0) {
            double degradation_ratio = static_cast<double>(degraded_services) / total_services;
            if (degradation_ratio > 0.5) { // More than 50% of services degraded
                return make_success(false);
            }
        }
        
        // Check global degradation level
        if (global_degradation_level_ >= degradation_level::emergency) {
            return make_success(false);
        }
        
        return make_success(true);
    }
    
private:
    std::string name_;
    mutable std::mutex services_mutex_;
    mutable std::mutex plans_mutex_;
    mutable std::mutex callback_mutex_;
    
    std::unordered_map<std::string, service_config> services_;
    std::unordered_map<std::string, degradation_level> service_states_;
    std::unordered_map<std::string, degradation_plan> degradation_plans_;
    
    degradation_level global_degradation_level_;
    graceful_degradation_metrics metrics_;
    
    degradation_callback degradation_callback_;
    health_checker health_checker_;
};

// Service degradation wrapper
template<typename T>
class degradable_service {
public:
    using operation_type = std::function<result<T>()>;
    using degraded_operation_type = std::function<result<T>(degradation_level)>;
    
    degradable_service(std::string name, 
                      std::shared_ptr<graceful_degradation_manager> manager,
                      operation_type normal_op,
                      degraded_operation_type degraded_op = nullptr)
        : name_(std::move(name))
        , manager_(manager)
        , normal_operation_(normal_op)
        , degraded_operation_(degraded_op) {}
    
    result<T> execute() {
        auto current_level = manager_->get_service_level(name_);
        
        switch (current_level) {
            case degradation_level::normal:
                if (normal_operation_) {
                    return normal_operation_();
                }
                break;
                
            case degradation_level::limited:
            case degradation_level::minimal:
            case degradation_level::emergency:
                if (degraded_operation_) {
                    return degraded_operation_(current_level);
                }
                break;
        }
        
        return make_error<T>(monitoring_error_code::service_unavailable,
                           "Service unavailable at degradation level: " + 
                           std::to_string(static_cast<int>(current_level)));
    }
    
    const std::string& get_name() const { return name_; }
    degradation_level get_current_level() const { 
        return manager_->get_service_level(name_); 
    }
    
private:
    std::string name_;
    std::shared_ptr<graceful_degradation_manager> manager_;
    operation_type normal_operation_;
    degraded_operation_type degraded_operation_;
};

// Utility functions
inline std::unique_ptr<graceful_degradation_manager> create_degradation_manager(
    std::string name = "default") {
    return std::make_unique<graceful_degradation_manager>(std::move(name));
}

inline service_config create_service_config(
    std::string name,
    service_priority priority = service_priority::normal,
    double error_rate_threshold = 0.3) {
    
    service_config config;
    config.name = std::move(name);
    config.priority = priority;
    config.error_rate_threshold = error_rate_threshold;
    return config;
}

inline degradation_plan create_degradation_plan(
    std::string name,
    std::vector<std::string> services_to_degrade = {},
    std::vector<std::string> services_to_disable = {},
    degradation_level target_level = degradation_level::minimal) {
    
    degradation_plan plan;
    plan.name = std::move(name);
    plan.services_to_degrade = std::move(services_to_degrade);
    plan.services_to_disable = std::move(services_to_disable);
    plan.target_level = target_level;
    return plan;
}

template<typename T>
std::unique_ptr<degradable_service<T>> create_degradable_service(
    std::string name,
    std::shared_ptr<graceful_degradation_manager> manager,
    std::function<result<T>()> normal_op,
    std::function<result<T>(degradation_level)> degraded_op = nullptr) {
    
    return std::make_unique<degradable_service<T>>(
        std::move(name), manager, normal_op, degraded_op);
}

} // namespace monitoring_system