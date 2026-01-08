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

#include "error_boundary.h"

#include <any>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace kcenon::monitoring {

/**
 * @brief Service priority levels
 */
enum class service_priority {
    optional = 0,
    normal = 1,
    important = 2,
    critical = 3
};

/**
 * @brief Graceful degradation metrics
 */
struct graceful_degradation_metrics {
    std::atomic<size_t> total_degradations{0};
    std::atomic<size_t> successful_degradations{0};
    std::atomic<size_t> failed_degradations{0};
    std::atomic<size_t> recovery_attempts{0};
    std::atomic<size_t> successful_recoveries{0};

    graceful_degradation_metrics() = default;

    graceful_degradation_metrics(const graceful_degradation_metrics& other)
        : total_degradations(other.total_degradations.load())
        , successful_degradations(other.successful_degradations.load())
        , failed_degradations(other.failed_degradations.load())
        , recovery_attempts(other.recovery_attempts.load())
        , successful_recoveries(other.successful_recoveries.load()) {}

    graceful_degradation_metrics& operator=(const graceful_degradation_metrics& other) {
        if (this != &other) {
            total_degradations = other.total_degradations.load();
            successful_degradations = other.successful_degradations.load();
            failed_degradations = other.failed_degradations.load();
            recovery_attempts = other.recovery_attempts.load();
            successful_recoveries = other.successful_recoveries.load();
        }
        return *this;
    }
};

/**
 * @brief Service configuration for graceful degradation
 */
struct service_config {
    std::string name;
    service_priority priority = service_priority::normal;
    double error_rate_threshold = 0.5;
    std::chrono::milliseconds health_check_interval = std::chrono::milliseconds(5000);
    bool auto_recover = true;

    bool validate() const {
        if (name.empty()) {
            return false;
        }
        if (error_rate_threshold < 0.0 || error_rate_threshold > 1.0) {
            return false;
        }
        return true;
    }
};

/**
 * @brief Degradation plan for coordinated service degradation
 */
struct degradation_plan {
    std::string name;
    std::vector<std::string> services_to_maintain;
    std::vector<std::string> services_to_disable;
    degradation_level target_level = degradation_level::minimal;

    bool validate() const {
        return !name.empty();
    }
};

/**
 * @brief Service state tracking
 */
struct service_state {
    service_config config;
    degradation_level current_level = degradation_level::normal;
    std::string last_degradation_reason;
    std::chrono::steady_clock::time_point last_state_change;
};

/**
 * @brief Graceful degradation manager
 *
 * Manages service degradation and recovery in a coordinated manner.
 */
class graceful_degradation_manager {
public:
    graceful_degradation_manager() : name_("default") {}

    explicit graceful_degradation_manager(const std::string& name) : name_(name) {}

    /**
     * @brief Register a service for management
     */
    result_void register_service(const service_config& config) {
        if (!config.validate()) {
            return make_void_error(monitoring_error_code::invalid_configuration,
                                  "Invalid service configuration");
        }

        std::lock_guard<std::mutex> lock(mutex_);
        if (services_.find(config.name) != services_.end()) {
            return make_void_error(monitoring_error_code::already_exists,
                                  "Service already registered: " + config.name);
        }

        service_state state;
        state.config = config;
        state.current_level = degradation_level::normal;
        state.last_state_change = std::chrono::steady_clock::now();
        services_[config.name] = state;

        return make_void_success();
    }

    /**
     * @brief Unregister a service
     */
    result_void unregister_service(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = services_.find(name);
        if (it == services_.end()) {
            return make_void_error(monitoring_error_code::not_found,
                                  "Service not found: " + name);
        }
        services_.erase(it);
        return make_void_success();
    }

    /**
     * @brief Degrade a specific service
     */
    result_void degrade_service(const std::string& name, degradation_level level,
                               const std::string& reason) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = services_.find(name);
        if (it == services_.end()) {
            metrics_.failed_degradations++;
            return make_void_error(monitoring_error_code::not_found,
                                  "Service not found: " + name);
        }

        metrics_.total_degradations++;
        it->second.current_level = level;
        it->second.last_degradation_reason = reason;
        it->second.last_state_change = std::chrono::steady_clock::now();
        metrics_.successful_degradations++;

        return make_void_success();
    }

    /**
     * @brief Recover a specific service to normal operation
     */
    result_void recover_service(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = services_.find(name);
        if (it == services_.end()) {
            return make_void_error(monitoring_error_code::not_found,
                                  "Service not found: " + name);
        }

        metrics_.recovery_attempts++;
        it->second.current_level = degradation_level::normal;
        it->second.last_degradation_reason.clear();
        it->second.last_state_change = std::chrono::steady_clock::now();
        metrics_.successful_recoveries++;

        return make_void_success();
    }

    /**
     * @brief Recover all services to normal operation
     */
    result_void recover_all_services() {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [name, state] : services_) {
            metrics_.recovery_attempts++;
            state.current_level = degradation_level::normal;
            state.last_degradation_reason.clear();
            state.last_state_change = std::chrono::steady_clock::now();
            metrics_.successful_recoveries++;
        }
        return make_void_success();
    }

    /**
     * @brief Get current degradation level for a service
     */
    degradation_level get_service_level(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = services_.find(name);
        if (it == services_.end()) {
            return degradation_level::normal;
        }
        return it->second.current_level;
    }

    /**
     * @brief Add a degradation plan
     */
    result_void add_degradation_plan(const degradation_plan& plan) {
        if (!plan.validate()) {
            return make_void_error(monitoring_error_code::invalid_configuration,
                                  "Invalid degradation plan");
        }

        std::lock_guard<std::mutex> lock(mutex_);
        plans_[plan.name] = plan;
        return make_void_success();
    }

    /**
     * @brief Execute a degradation plan
     */
    result_void execute_plan(const std::string& plan_name, const std::string& reason) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = plans_.find(plan_name);
        if (it == plans_.end()) {
            return make_void_error(monitoring_error_code::not_found,
                                  "Plan not found: " + plan_name);
        }

        const auto& plan = it->second;

        // Degrade services to maintain to target level
        for (const auto& service_name : plan.services_to_maintain) {
            auto service_it = services_.find(service_name);
            if (service_it != services_.end()) {
                metrics_.total_degradations++;
                service_it->second.current_level = plan.target_level;
                service_it->second.last_degradation_reason = reason;
                service_it->second.last_state_change = std::chrono::steady_clock::now();
                metrics_.successful_degradations++;
            }
        }

        // Disable (set to emergency) services to disable
        for (const auto& service_name : plan.services_to_disable) {
            auto service_it = services_.find(service_name);
            if (service_it != services_.end()) {
                metrics_.total_degradations++;
                service_it->second.current_level = degradation_level::emergency;
                service_it->second.last_degradation_reason = reason;
                service_it->second.last_state_change = std::chrono::steady_clock::now();
                metrics_.successful_degradations++;
            }
        }

        return make_void_success();
    }

    /**
     * @brief Check if the manager is healthy (more than 50% services at normal level)
     */
    result<bool> is_healthy() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (services_.empty()) {
            return make_success(true);
        }

        size_t normal_count = 0;
        for (const auto& [name, state] : services_) {
            if (state.current_level == degradation_level::normal) {
                normal_count++;
            }
        }

        double healthy_ratio = static_cast<double>(normal_count) / static_cast<double>(services_.size());
        return make_success(healthy_ratio > 0.5);
    }

    /**
     * @brief Get metrics
     */
    graceful_degradation_metrics get_metrics() const {
        return metrics_;
    }

    /**
     * @brief Get manager name
     */
    const std::string& get_name() const {
        return name_;
    }

    /**
     * @brief Get all registered service names
     */
    std::vector<std::string> get_service_names() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> names;
        names.reserve(services_.size());
        for (const auto& [name, state] : services_) {
            names.push_back(name);
        }
        return names;
    }

private:
    std::string name_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, service_state> services_;
    std::unordered_map<std::string, degradation_plan> plans_;
    mutable graceful_degradation_metrics metrics_;
};

/**
 * @brief Degradable service wrapper
 *
 * Wraps a service with degradation support.
 */
template<typename T>
class degradable_service {
public:
    using normal_operation = std::function<result<T>()>;
    using degraded_operation = std::function<result<T>(degradation_level)>;

    degradable_service(const std::string& name,
                       std::shared_ptr<graceful_degradation_manager> manager,
                       normal_operation normal_op,
                       degraded_operation degraded_op)
        : name_(name)
        , manager_(std::move(manager))
        , normal_op_(std::move(normal_op))
        , degraded_op_(std::move(degraded_op)) {}

    /**
     * @brief Execute the service operation
     */
    result<T> execute() {
        if (!manager_) {
            return normal_op_();
        }

        auto level = manager_->get_service_level(name_);
        if (level == degradation_level::normal) {
            return normal_op_();
        }

        if (degraded_op_) {
            return degraded_op_(level);
        }

        return make_error<T>(monitoring_error_code::service_degraded,
                            "Service is degraded and no fallback available");
    }

    /**
     * @brief Get service name
     */
    const std::string& get_name() const {
        return name_;
    }

private:
    std::string name_;
    std::shared_ptr<graceful_degradation_manager> manager_;
    normal_operation normal_op_;
    degraded_operation degraded_op_;
};

/**
 * @brief Error boundary registry for managing multiple boundaries
 */
class error_boundary_registry {
public:
    template<typename T>
    void register_boundary(const std::string& name, std::shared_ptr<error_boundary<T>> boundary) {
        std::lock_guard<std::mutex> lock(mutex_);
        registry_[name] = std::move(boundary);
    }

    template<typename T>
    std::shared_ptr<error_boundary<T>> get_boundary(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = registry_.find(name);
        if (it != registry_.end()) {
            try {
                return std::any_cast<std::shared_ptr<error_boundary<T>>>(it->second);
            } catch (const std::bad_any_cast&) {
                return nullptr;
            }
        }
        return nullptr;
    }

    void remove_boundary(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        registry_.erase(name);
    }

    std::vector<std::string> get_all_names() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> names;
        names.reserve(registry_.size());
        for (const auto& [name, boundary] : registry_) {
            names.push_back(name);
        }
        return names;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        registry_.clear();
    }

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::any> registry_;
};

// Factory functions

/**
 * @brief Get global error boundary registry
 */
inline error_boundary_registry& global_error_boundary_registry() {
    static error_boundary_registry instance;
    return instance;
}

/**
 * @brief Create a graceful degradation manager
 */
inline std::shared_ptr<graceful_degradation_manager> create_degradation_manager(const std::string& name) {
    return std::make_shared<graceful_degradation_manager>(name);
}

/**
 * @brief Create a service configuration
 */
inline service_config create_service_config(const std::string& name, service_priority priority) {
    service_config config;
    config.name = name;
    config.priority = priority;
    return config;
}

/**
 * @brief Create a degradation plan
 */
inline degradation_plan create_degradation_plan(const std::string& name,
                                                const std::vector<std::string>& maintain,
                                                const std::vector<std::string>& disable,
                                                degradation_level level) {
    degradation_plan plan;
    plan.name = name;
    plan.services_to_maintain = maintain;
    plan.services_to_disable = disable;
    plan.target_level = level;
    return plan;
}

/**
 * @brief Create a degradable service
 */
template<typename T>
std::shared_ptr<degradable_service<T>> create_degradable_service(
    const std::string& name,
    std::shared_ptr<graceful_degradation_manager> manager,
    typename degradable_service<T>::normal_operation normal_op,
    typename degradable_service<T>::degraded_operation degraded_op) {
    return std::make_shared<degradable_service<T>>(name, std::move(manager),
                                                    std::move(normal_op), std::move(degraded_op));
}

} // namespace kcenon::monitoring
