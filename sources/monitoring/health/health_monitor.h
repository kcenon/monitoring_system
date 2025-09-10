/**
 * @file health_monitor.h
 * @brief Health monitoring framework for service health checks and dependency tracking
 * @date 2025
 * 
 * Provides comprehensive health monitoring capabilities including
 * liveness checks, readiness checks, and dependency health tracking.
 */

#pragma once

#include <string>
#include <memory>
#include <chrono>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <atomic>
#include <functional>
#include <optional>
#include <thread>
#include <future>
#include <shared_mutex>

#include "../core/result_types.h"
#include "../core/error_codes.h"
#include "../interfaces/monitoring_interface.h"

namespace monitoring_system {

// health_status is defined in monitoring_interface.h

/**
 * @brief Health check types
 */
enum class health_check_type {
    liveness,     // Is the service alive?
    readiness,    // Is the service ready to accept requests?
    startup       // Has the service started successfully?
};

// health_check_result is defined in monitoring_interface.h with additional methods

/**
 * @brief Abstract health check interface
 */
class health_check {
public:
    virtual ~health_check() = default;
    
    /**
     * @brief Get the name of this health check
     */
    virtual std::string get_name() const = 0;
    
    /**
     * @brief Get the type of this health check
     */
    virtual health_check_type get_type() const = 0;
    
    /**
     * @brief Perform the health check
     */
    virtual health_check_result check() = 0;
    
    /**
     * @brief Get the timeout for this health check
     */
    virtual std::chrono::milliseconds get_timeout() const {
        return std::chrono::milliseconds(5000);
    }
    
    /**
     * @brief Check if this health check is critical
     */
    virtual bool is_critical() const {
        return true;
    }
};

/**
 * @brief Simple functional health check
 */
class functional_health_check : public health_check {
private:
    std::string name_;
    health_check_type type_;
    std::function<health_check_result()> check_func_;
    std::chrono::milliseconds timeout_;
    bool critical_;
    
public:
    functional_health_check(
        const std::string& name,
        health_check_type type,
        std::function<health_check_result()> func,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(5000),
        bool critical = true
    ) : name_(name), type_(type), check_func_(func), 
        timeout_(timeout), critical_(critical) {}
    
    std::string get_name() const override { return name_; }
    health_check_type get_type() const override { return type_; }
    std::chrono::milliseconds get_timeout() const override { return timeout_; }
    bool is_critical() const override { return critical_; }
    
    health_check_result check() override {
        if (check_func_) {
            return check_func_();
        }
        return health_check_result::unhealthy("Check function not set");
    }
};

/**
 * @brief Composite health check that aggregates multiple checks
 */
class composite_health_check : public health_check {
private:
    std::string name_;
    health_check_type type_;
    std::vector<std::shared_ptr<health_check>> checks_;
    bool require_all_;
    
public:
    composite_health_check(
        const std::string& name,
        health_check_type type,
        bool require_all = true
    ) : name_(name), type_(type), require_all_(require_all) {}
    
    /**
     * @brief Add a health check to the composite
     */
    void add_check(std::shared_ptr<health_check> check) {
        checks_.push_back(check);
    }
    
    std::string get_name() const override { return name_; }
    health_check_type get_type() const override { return type_; }
    
    health_check_result check() override {
        if (checks_.empty()) {
            return health_check_result::healthy("No checks configured");
        }
        
        std::vector<health_check_result> results;
        bool any_healthy = false;
        bool any_unhealthy = false;
        bool any_degraded = false;
        
        for (const auto& check : checks_) {
            auto result = check->check();
            
            if (result.status == health_status::healthy) {
                any_healthy = true;
            } else if (result.status == health_status::unhealthy) {
                any_unhealthy = true;
                if (check->is_critical() && require_all_) {
                    // Critical check failed
                    return result;
                }
            } else if (result.status == health_status::degraded) {
                any_degraded = true;
            }
            
            results.push_back(result);
        }
        
        // Determine overall status
        if (require_all_) {
            if (any_unhealthy) {
                return health_check_result::unhealthy("Some checks failed");
            } else if (any_degraded) {
                return health_check_result::degraded("Some checks degraded");
            } else if (any_healthy) {
                return health_check_result::healthy("All checks passed");
            }
        } else {
            if (any_healthy) {
                return health_check_result::healthy("At least one check passed");
            } else if (any_degraded) {
                return health_check_result::degraded("No healthy checks");
            } else {
                return health_check_result::unhealthy("All checks failed");
            }
        }
        
        return health_check_result::unhealthy("Unknown status");
    }
};

/**
 * @brief Health dependency graph
 */
class health_dependency_graph {
private:
    struct node {
        std::string name;
        std::shared_ptr<health_check> check;
        std::unordered_set<std::string> dependencies;
        std::unordered_set<std::string> dependents;
        health_check_result last_result;
        std::chrono::system_clock::time_point last_check;
    };
    
    std::unordered_map<std::string, node> nodes_;
    mutable std::mutex graph_mutex_;
    
public:
    /**
     * @brief Add a health check node
     */
    monitoring_system::result<bool> add_node(
        const std::string& name,
        std::shared_ptr<health_check> check
    );
    
    /**
     * @brief Add a dependency relationship
     */
    monitoring_system::result<bool> add_dependency(
        const std::string& dependent,
        const std::string& dependency
    );
    
    /**
     * @brief Remove a dependency relationship
     */
    monitoring_system::result<bool> remove_dependency(
        const std::string& dependent,
        const std::string& dependency
    );
    
    /**
     * @brief Get all dependencies of a node
     */
    std::vector<std::string> get_dependencies(const std::string& name) const;
    
    /**
     * @brief Get all dependents of a node
     */
    std::vector<std::string> get_dependents(const std::string& name) const;
    
    /**
     * @brief Check if adding a dependency would create a cycle
     */
    bool would_create_cycle(
        const std::string& dependent,
        const std::string& dependency
    ) const;
    
    /**
     * @brief Get topologically sorted nodes
     */
    std::vector<std::string> topological_sort() const;
    
    /**
     * @brief Check health including dependencies
     */
    health_check_result check_with_dependencies(const std::string& name);
    
    /**
     * @brief Get the impact of a node failure
     */
    std::vector<std::string> get_failure_impact(const std::string& name) const;
};

/**
 * @brief Health monitoring configuration
 */
struct health_monitor_config {
    std::chrono::seconds check_interval{30};
    std::chrono::seconds cache_duration{10};
    std::uint32_t max_parallel_checks{10};
    bool enable_auto_recovery{false};
    std::uint32_t max_recovery_attempts{3};
    std::chrono::seconds recovery_delay{60};
};

/**
 * @brief Health monitoring statistics
 */
struct health_stats {
    std::uint64_t total_checks{0};
    std::uint64_t healthy_checks{0};
    std::uint64_t degraded_checks{0};
    std::uint64_t unhealthy_checks{0};
    std::uint64_t timeout_count{0};
    std::uint64_t recovery_attempts{0};
    std::uint64_t successful_recoveries{0};
    std::chrono::milliseconds average_check_duration{0};
    std::chrono::system_clock::time_point last_check_time;
};

/**
 * @brief Health monitor controller
 */
class health_monitor {
private:
    struct monitor_impl;
    std::unique_ptr<monitor_impl> impl_;
    
public:
    health_monitor(const health_monitor_config& config = {});
    ~health_monitor();
    
    // Disable copy
    health_monitor(const health_monitor&) = delete;
    health_monitor& operator=(const health_monitor&) = delete;
    
    // Enable move
    health_monitor(health_monitor&&) noexcept;
    health_monitor& operator=(health_monitor&&) noexcept;
    
    /**
     * @brief Register a health check
     */
    monitoring_system::result<bool> register_check(
        const std::string& name,
        std::shared_ptr<health_check> check
    );
    
    /**
     * @brief Unregister a health check
     */
    monitoring_system::result<bool> unregister_check(const std::string& name);
    
    /**
     * @brief Add a dependency between checks
     */
    monitoring_system::result<bool> add_dependency(
        const std::string& dependent,
        const std::string& dependency
    );
    
    /**
     * @brief Start monitoring
     */
    monitoring_system::result<bool> start();
    
    /**
     * @brief Stop monitoring
     */
    monitoring_system::result<bool> stop();
    
    /**
     * @brief Check if monitoring is active
     */
    bool is_running() const;
    
    /**
     * @brief Perform a specific health check
     */
    monitoring_system::result<health_check_result> check(const std::string& name);
    
    /**
     * @brief Perform all health checks
     */
    std::unordered_map<std::string, health_check_result> check_all();
    
    /**
     * @brief Get overall health status
     */
    health_status get_overall_status() const;
    
    /**
     * @brief Get health statistics
     */
    health_stats get_stats() const;
    
    /**
     * @brief Register a recovery handler
     */
    void register_recovery_handler(
        const std::string& check_name,
        std::function<bool()> handler
    );
    
    /**
     * @brief Get health report
     */
    std::string get_health_report() const;
    
    /**
     * @brief Force a health check refresh
     */
    monitoring_system::result<bool> refresh();
};

/**
 * @brief Global health monitor instance
 */
health_monitor& global_health_monitor();

/**
 * @brief Health check builder
 */
class health_check_builder {
private:
    std::string name_;
    health_check_type type_{health_check_type::liveness};
    std::function<health_check_result()> check_func_;
    std::chrono::milliseconds timeout_{5000};
    bool critical_{true};
    
public:
    health_check_builder& with_name(const std::string& name) {
        name_ = name;
        return *this;
    }
    
    health_check_builder& with_type(health_check_type type) {
        type_ = type;
        return *this;
    }
    
    health_check_builder& with_check(std::function<health_check_result()> func) {
        check_func_ = func;
        return *this;
    }
    
    health_check_builder& with_timeout(std::chrono::milliseconds timeout) {
        timeout_ = timeout;
        return *this;
    }
    
    health_check_builder& critical(bool is_critical) {
        critical_ = is_critical;
        return *this;
    }
    
    std::shared_ptr<health_check> build() {
        return std::make_shared<functional_health_check>(
            name_, type_, check_func_, timeout_, critical_
        );
    }
};

} // namespace monitoring_system