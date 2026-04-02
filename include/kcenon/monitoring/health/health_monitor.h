// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

/**
 * @file health_monitor.h
 * @brief Health monitoring with dependency graphs, auto-recovery, and statistics.
 *
 * @details Provides a comprehensive health monitoring framework including:
 * - Pluggable health checks (functional, composite, dependency-aware)
 * - DAG-based dependency graph for ordered checking
 * - Automatic periodic monitoring with configurable intervals
 * - Auto-recovery handlers for unhealthy components
 * - Builder pattern for convenient health check creation
 *
 * ### Thread Safety
 * health_monitor and health_dependency_graph use std::shared_mutex for
 * concurrent read access and exclusive write access. The monitoring loop
 * runs on a dedicated thread.
 *
 * @code
 * health_monitor monitor({
 *     .check_interval = std::chrono::milliseconds(5000),
 *     .enable_auto_recovery = true,
 *     .max_consecutive_failures = 3
 * });
 *
 * auto db_check = health_check_builder()
 *     .with_name("database")
 *     .with_type(health_check_type::readiness)
 *     .with_check([]() { return check_db_connection(); })
 *     .critical(true)
 *     .build();
 *
 * monitor.register_check("database", db_check);
 * monitor.start();
 * @endcode
 *
 * @author kcenon
 * @since 1.0.0
 * @see thread_context For request-scoped context during health checks
 */

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <kcenon/monitoring/interfaces/monitoring_core.h>

namespace kcenon::monitoring {

/**
 * @brief Types of health checks following Kubernetes probe conventions.
 *
 * @see https://kubernetes.io/docs/tasks/configure-pod-container/configure-liveness-readiness-startup-probes/
 */
enum class health_check_type {
    liveness,   ///< Indicates whether the process is alive and should be restarted if failing
    readiness,  ///< Indicates whether the service is ready to accept traffic
    startup     ///< Indicates whether the application has finished initializing
};

/**
 * @brief Configuration for the health_monitor.
 */
struct health_monitor_config {
    std::chrono::milliseconds check_interval{std::chrono::milliseconds(5000)}; ///< Interval between automatic health check cycles
    std::chrono::seconds cache_duration{std::chrono::seconds(1)};             ///< Duration to cache health check results
    bool enable_auto_recovery{true};                                          ///< Whether to invoke recovery handlers on failure
    size_t max_consecutive_failures{3};                                        ///< Failures before triggering recovery
    std::chrono::seconds recovery_timeout{std::chrono::seconds(30)};          ///< Maximum time allowed for a recovery attempt
};

/**
 * @brief Accumulated statistics for health monitoring operations.
 */
struct health_monitor_stats {
    size_t total_checks{0};            ///< Total number of health checks performed
    size_t healthy_checks{0};          ///< Number of checks that returned healthy
    size_t unhealthy_checks{0};        ///< Number of checks that returned unhealthy
    size_t degraded_checks{0};         ///< Number of checks that returned degraded
    size_t recovery_attempts{0};       ///< Number of auto-recovery attempts made
    size_t successful_recoveries{0};   ///< Number of successful recovery attempts
    std::chrono::system_clock::time_point last_check_time; ///< Timestamp of the last check cycle
};

/**
 * @brief Abstract base class for health checks.
 *
 * @details Subclass this to implement custom health checks. Override check()
 * to perform the actual health verification, and optionally override
 * get_timeout() and is_critical() to customize behavior.
 *
 * @see functional_health_check For a lambda-based implementation
 * @see composite_health_check For aggregating multiple checks
 * @see health_check_builder For convenient construction
 */
class health_check {
public:
    virtual ~health_check() = default;

    /**
     * @brief Get the human-readable name of this health check.
     * @return The health check name
     */
    virtual std::string get_name() const = 0;

    /**
     * @brief Get the type of this health check (liveness, readiness, or startup).
     * @return The health check type
     */
    virtual health_check_type get_type() const = 0;

    /**
     * @brief Execute the health check and return the result.
     * @return A health_check_result indicating the current health status
     */
    virtual health_check_result check() = 0;

    /**
     * @brief Get the maximum time allowed for this check to complete.
     * @return Timeout duration (default: 1000ms)
     */
    virtual std::chrono::milliseconds get_timeout() const {
        return std::chrono::milliseconds(1000);
    }

    /**
     * @brief Whether this check is critical for overall system health.
     * @return true if failure of this check should mark the system as unhealthy (default: false)
     */
    virtual bool is_critical() const {
        return false;
    }
};

/**
 * @brief Health check implementation backed by a std::function.
 *
 * @details Wraps a callable as a health_check, allowing health checks to be
 * defined inline without subclassing. Typically created via health_check_builder.
 *
 * @see health_check_builder For the preferred way to create instances
 */
class functional_health_check : public health_check {
public:
    /**
     * @brief Construct a functional health check.
     * @param name Human-readable name for this check
     * @param type The check type (liveness, readiness, or startup)
     * @param check_func Callable that performs the health check
     * @param timeout Maximum duration for the check (default: 1000ms)
     * @param critical Whether this check is critical for overall health (default: false)
     */
    functional_health_check(const std::string& name,
                            health_check_type type,
                            std::function<health_check_result()> check_func,
                            std::chrono::milliseconds timeout = std::chrono::milliseconds(1000),
                            bool critical = false)
        : name_(name)
        , type_(type)
        , check_func_(std::move(check_func))
        , timeout_(timeout)
        , critical_(critical) {}

    /// @copydoc health_check::get_name()
    std::string get_name() const override { return name_; }
    /// @copydoc health_check::get_type()
    health_check_type get_type() const override { return type_; }
    /// @copydoc health_check::get_timeout()
    std::chrono::milliseconds get_timeout() const override { return timeout_; }
    /// @copydoc health_check::is_critical()
    bool is_critical() const override { return critical_; }

    /**
     * @brief Execute the stored check function.
     * @return The health check result, or healthy("No check function") if no callable is set
     */
    health_check_result check() override {
        if (check_func_) {
            return check_func_();
        }
        return health_check_result::healthy("No check function");
    }

private:
    std::string name_;
    health_check_type type_;
    std::function<health_check_result()> check_func_;
    std::chrono::milliseconds timeout_;
    bool critical_;
};

/**
 * @brief Composite health check that aggregates multiple sub-checks.
 *
 * @details Evaluates a collection of child health checks and returns an
 * aggregate result. Supports two aggregation modes:
 * - **all_required** (AND): All checks must pass for healthy status
 * - **any_required** (OR): At least one check must pass for healthy status
 *
 * ### Thread Safety
 * Thread-safe. Uses a mutex to protect the checks_ collection.
 *
 * @see health_check For the base interface
 */
class composite_health_check : public health_check {
public:
    /**
     * @brief Construct a composite health check.
     * @param name Human-readable name for this composite check
     * @param type The check type (liveness, readiness, or startup)
     * @param all_required If true (default), all sub-checks must pass (AND mode);
     *        if false, at least one must pass (OR mode)
     */
    composite_health_check(const std::string& name,
                           health_check_type type,
                           bool all_required = true)
        : name_(name)
        , type_(type)
        , all_required_(all_required) {}

    /// @copydoc health_check::get_name()
    std::string get_name() const override { return name_; }
    /// @copydoc health_check::get_type()
    health_check_type get_type() const override { return type_; }

    /**
     * @brief Add a child health check to this composite.
     * @param check The health check to add
     */
    void add_check(std::shared_ptr<health_check> check) {
        std::lock_guard<std::mutex> lock(mutex_);
        checks_.push_back(std::move(check));
    }

    /**
     * @brief Execute all child checks and return the aggregate result.
     * @return Aggregate health_check_result based on the aggregation mode
     */
    health_check_result check() override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (checks_.empty()) {
            return health_check_result::healthy("No checks configured");
        }

        std::vector<health_check_result> results;
        results.reserve(checks_.size());

        for (const auto& chk : checks_) {
            results.push_back(chk->check());
        }

        if (all_required_) {
            return check_all_required(results);
        } else {
            return check_any_required(results);
        }
    }

private:
    health_check_result check_all_required(const std::vector<health_check_result>& results) {
        bool has_unhealthy = false;
        bool has_degraded = false;
        std::string message;

        for (const auto& result : results) {
            if (result.status == health_status::unhealthy) {
                has_unhealthy = true;
                message += result.message + "; ";
            } else if (result.status == health_status::degraded) {
                has_degraded = true;
                message += result.message + "; ";
            }
        }

        if (has_unhealthy) {
            return health_check_result::unhealthy(message.empty() ? "One or more checks failed" : message);
        }
        if (has_degraded) {
            return health_check_result::degraded(message.empty() ? "One or more checks degraded" : message);
        }
        return health_check_result::healthy("All checks passed");
    }

    health_check_result check_any_required(const std::vector<health_check_result>& results) {
        bool any_healthy = false;
        std::string message;

        for (const auto& result : results) {
            if (result.status == health_status::healthy) {
                any_healthy = true;
                break;
            }
            message += result.message + "; ";
        }

        if (any_healthy) {
            return health_check_result::healthy("At least one check passed");
        }
        return health_check_result::unhealthy(message.empty() ? "All checks failed" : message);
    }

    std::string name_;
    health_check_type type_;
    bool all_required_;
    std::vector<std::shared_ptr<health_check>> checks_;
    mutable std::mutex mutex_;
};

/**
 * @brief Directed acyclic graph for health check dependencies.
 *
 * @details Models dependency relationships between health checks so that a
 * check's dependencies are verified before the check itself. Prevents
 * circular dependencies and supports topological ordering and impact analysis.
 *
 * ### Thread Safety
 * All public methods are thread-safe using std::shared_mutex
 * (shared lock for reads, exclusive lock for writes).
 *
 * @see health_monitor For the primary consumer of this graph
 */
class health_dependency_graph {
public:
    /**
     * @brief Add a health check node to the graph.
     * @param name Unique name for this node
     * @param check The health check implementation
     * @return Ok(true) on success, or error if the name already exists
     */
    common::Result<bool> add_node(const std::string& name, std::shared_ptr<health_check> check) {
        std::lock_guard<std::shared_mutex> lock(mutex_);

        if (nodes_.find(name) != nodes_.end()) {
            return common::Result<bool>::err(error_info(monitoring_error_code::already_exists, "Node '" + name + "' already exists").to_common_error());
        }

        nodes_[name] = std::move(check);
        dependencies_[name] = {};
        dependents_[name] = {};
        return common::ok(true);
    }

    /**
     * @brief Add a dependency edge: dependent depends on dependency.
     * @param dependent Name of the node that depends on another
     * @param dependency Name of the node being depended upon
     * @return Ok(true) on success, or error if nodes are not found or a cycle would be created
     */
    common::Result<bool> add_dependency(const std::string& dependent, const std::string& dependency) {
        std::lock_guard<std::shared_mutex> lock(mutex_);

        if (nodes_.find(dependent) == nodes_.end()) {
            return common::Result<bool>::err(error_info(monitoring_error_code::not_found, "Dependent '" + dependent + "' not found").to_common_error());
        }
        if (nodes_.find(dependency) == nodes_.end()) {
            return common::Result<bool>::err(error_info(monitoring_error_code::not_found, "Dependency '" + dependency + "' not found").to_common_error());
        }

        if (would_create_cycle_internal(dependent, dependency)) {
            return common::Result<bool>::err(error_info(monitoring_error_code::invalid_state, "Adding dependency would create a cycle").to_common_error());
        }

        dependencies_[dependent].push_back(dependency);
        dependents_[dependency].push_back(dependent);
        return common::ok(true);
    }

    /**
     * @brief Get the direct dependencies of a node.
     * @param name Node name to query
     * @return List of dependency names, or empty vector if node not found
     */
    std::vector<std::string> get_dependencies(const std::string& name) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        auto it = dependencies_.find(name);
        if (it != dependencies_.end()) {
            return it->second;
        }
        return {};
    }

    /**
     * @brief Get the nodes that directly depend on the given node.
     * @param name Node name to query
     * @return List of dependent names, or empty vector if node not found
     */
    std::vector<std::string> get_dependents(const std::string& name) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        auto it = dependents_.find(name);
        if (it != dependents_.end()) {
            return it->second;
        }
        return {};
    }

    /**
     * @brief Check whether adding an edge from -> to would create a cycle.
     * @param from Source node name
     * @param to Target node name
     * @return true if the edge would create a cycle
     */
    bool would_create_cycle(const std::string& from, const std::string& to) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return would_create_cycle_internal(from, to);
    }

    /**
     * @brief Compute a topological ordering of all nodes.
     * @return Node names in dependency order (dependencies before dependents)
     */
    std::vector<std::string> topological_sort() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        std::unordered_map<std::string, int> in_degree;
        for (const auto& [name, _] : nodes_) {
            in_degree[name] = 0;
        }

        for (const auto& [name, deps] : dependencies_) {
            in_degree[name] = static_cast<int>(deps.size());
        }

        std::queue<std::string> queue;
        for (const auto& [name, degree] : in_degree) {
            if (degree == 0) {
                queue.push(name);
            }
        }

        std::vector<std::string> result;
        result.reserve(nodes_.size());

        while (!queue.empty()) {
            std::string current = queue.front();
            queue.pop();
            result.push_back(current);

            auto it = dependents_.find(current);
            if (it != dependents_.end()) {
                for (const auto& dep : it->second) {
                    if (--in_degree[dep] == 0) {
                        queue.push(dep);
                    }
                }
            }
        }

        return result;
    }

    /**
     * @brief Execute a health check after verifying all its dependencies are healthy.
     * @param name Node name to check
     * @return The check result; unhealthy/degraded if any dependency fails
     */
    health_check_result check_with_dependencies(const std::string& name) {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        auto it = nodes_.find(name);
        if (it == nodes_.end()) {
            return health_check_result::unhealthy("Node '" + name + "' not found");
        }

        auto deps_it = dependencies_.find(name);
        if (deps_it != dependencies_.end()) {
            for (const auto& dep_name : deps_it->second) {
                auto dep_it = nodes_.find(dep_name);
                if (dep_it != nodes_.end()) {
                    auto dep_result = dep_it->second->check();
                    if (dep_result.status == health_status::unhealthy) {
                        return health_check_result::unhealthy(
                            "Dependency '" + dep_name + "' is unhealthy: " + dep_result.message);
                    }
                    if (dep_result.status == health_status::degraded) {
                        return health_check_result::degraded(
                            "Dependency '" + dep_name + "' is degraded: " + dep_result.message);
                    }
                }
            }
        }

        return it->second->check();
    }

    /**
     * @brief Compute all nodes that would be impacted if the given node fails.
     * @param name Node name to analyze
     * @return List of all transitively dependent node names
     */
    std::vector<std::string> get_failure_impact(const std::string& name) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        std::vector<std::string> impacted;
        std::unordered_set<std::string> visited;
        std::queue<std::string> to_visit;

        auto it = dependents_.find(name);
        if (it != dependents_.end()) {
            for (const auto& dep : it->second) {
                to_visit.push(dep);
            }
        }

        while (!to_visit.empty()) {
            std::string current = to_visit.front();
            to_visit.pop();

            if (visited.find(current) != visited.end()) {
                continue;
            }
            visited.insert(current);
            impacted.push_back(current);

            auto dep_it = dependents_.find(current);
            if (dep_it != dependents_.end()) {
                for (const auto& dep : dep_it->second) {
                    if (visited.find(dep) == visited.end()) {
                        to_visit.push(dep);
                    }
                }
            }
        }

        return impacted;
    }

private:
    bool would_create_cycle_internal(const std::string& from, const std::string& to) const {
        if (from == to) {
            return true;
        }

        std::unordered_set<std::string> visited;
        std::queue<std::string> to_visit;
        to_visit.push(to);

        while (!to_visit.empty()) {
            std::string current = to_visit.front();
            to_visit.pop();

            if (current == from) {
                return true;
            }

            if (visited.find(current) != visited.end()) {
                continue;
            }
            visited.insert(current);

            auto it = dependencies_.find(current);
            if (it != dependencies_.end()) {
                for (const auto& dep : it->second) {
                    if (visited.find(dep) == visited.end()) {
                        to_visit.push(dep);
                    }
                }
            }
        }

        return false;
    }

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<health_check>> nodes_;
    std::unordered_map<std::string, std::vector<std::string>> dependencies_;
    std::unordered_map<std::string, std::vector<std::string>> dependents_;
};

/**
 * @brief Fluent builder for creating functional_health_check instances.
 *
 * @code
 * auto check = health_check_builder()
 *     .with_name("redis")
 *     .with_type(health_check_type::readiness)
 *     .with_check([]() {
 *         return ping_redis()
 *             ? health_check_result::healthy("Redis OK")
 *             : health_check_result::unhealthy("Redis unreachable");
 *     })
 *     .with_timeout(std::chrono::milliseconds(500))
 *     .critical(true)
 *     .build();
 * @endcode
 *
 * @see functional_health_check For the type created by build()
 */
class health_check_builder {
public:
    /**
     * @brief Set the health check name.
     * @param name Human-readable name
     * @return Reference to this builder for chaining
     */
    health_check_builder& with_name(const std::string& name) {
        name_ = name;
        return *this;
    }

    /**
     * @brief Set the health check type.
     * @param type The check type (liveness, readiness, or startup)
     * @return Reference to this builder for chaining
     */
    health_check_builder& with_type(health_check_type type) {
        type_ = type;
        return *this;
    }

    /**
     * @brief Set the callable that performs the health check.
     * @param func A callable returning health_check_result
     * @return Reference to this builder for chaining
     */
    health_check_builder& with_check(std::function<health_check_result()> func) {
        check_func_ = std::move(func);
        return *this;
    }

    /**
     * @brief Set the maximum duration allowed for the check.
     * @param timeout Timeout duration
     * @return Reference to this builder for chaining
     */
    health_check_builder& with_timeout(std::chrono::milliseconds timeout) {
        timeout_ = timeout;
        return *this;
    }

    /**
     * @brief Mark this check as critical for overall system health.
     * @param is_critical true if system should be marked unhealthy when this check fails
     * @return Reference to this builder for chaining
     */
    health_check_builder& critical(bool is_critical) {
        critical_ = is_critical;
        return *this;
    }

    /**
     * @brief Build and return the configured functional_health_check.
     * @return Shared pointer to the constructed health check
     */
    std::shared_ptr<functional_health_check> build() {
        return std::make_shared<functional_health_check>(
            name_, type_, check_func_, timeout_, critical_);
    }

private:
    std::string name_;
    health_check_type type_{health_check_type::liveness};
    std::function<health_check_result()> check_func_;
    std::chrono::milliseconds timeout_{std::chrono::milliseconds(1000)};
    bool critical_{false};
};

/**
 * @brief Health monitor with dependency management, auto-recovery, and statistics.
 *
 * @details Manages a collection of named health checks, runs them periodically
 * on a background thread, maintains cached results, and optionally invokes
 * recovery handlers when checks fail.
 *
 * ### Lifecycle
 * 1. Create monitor with optional configuration
 * 2. Register health checks and optional recovery handlers
 * 3. Call start() to begin periodic monitoring
 * 4. Query status via get_overall_status(), check(), or get_health_report()
 * 5. Call stop() or let the destructor handle cleanup
 *
 * ### Thread Safety
 * All public methods are thread-safe. Internal state is protected by
 * std::shared_mutex (data) and std::mutex (lifecycle).
 *
 * @see health_check For the check interface
 * @see health_check_builder For creating checks
 * @see health_dependency_graph For dependency management
 */
class health_monitor {
public:
    /** @brief Default constructor with default configuration. */
    health_monitor() = default;

    /**
     * @brief Construct with custom configuration.
     * @param config Health monitor configuration settings
     */
    explicit health_monitor(const health_monitor_config& config) : config_(config) {}

    /** @brief Destructor. Stops the monitoring loop if running. */
    virtual ~health_monitor() { stop(); }

    /**
     * @brief Register a named health check.
     * @param name Unique name for this check
     * @param check The health check implementation
     * @return Ok(true) on success, or error if the name already exists
     */
    common::Result<bool> register_check(const std::string& name, std::shared_ptr<health_check> check) {
        std::lock_guard<std::shared_mutex> lock(mutex_);

        if (checks_.find(name) != checks_.end()) {
            return common::Result<bool>::err(error_info(monitoring_error_code::already_exists, "Check '" + name + "' already registered").to_common_error());
        }

        checks_[name] = std::move(check);
        auto graph_result = dependency_graph_.add_node(name, checks_[name]);
        if (graph_result.is_err()) {
            checks_.erase(name);
            return common::Result<bool>::err(graph_result.error());
        }
        return common::ok(true);
    }

    /**
     * @brief Remove a previously registered health check.
     * @param name Name of the check to remove
     * @return Ok(true) on success, or error if the check was not found
     */
    common::Result<bool> unregister_check(const std::string& name) {
        std::lock_guard<std::shared_mutex> lock(mutex_);

        if (checks_.find(name) == checks_.end()) {
            error_info err(monitoring_error_code::not_found,
                          "Check '" + name + "' not found");
            return common::Result<bool>::err(err.to_common_error());
        }

        checks_.erase(name);
        recovery_handlers_.erase(name);
        return common::ok(true);
    }

    /**
     * @brief Execute a single named health check (with dependency verification).
     * @param name Name of the check to execute
     * @return Ok(result) on success, or error if the check was not found
     */
    common::Result<health_check_result> check(const std::string& name) {
        std::lock_guard<std::shared_mutex> lock(mutex_);

        auto it = checks_.find(name);
        if (it == checks_.end()) {
            error_info err(monitoring_error_code::not_found,
                          "Check '" + name + "' not found");
            return common::Result<health_check_result>::err(err.to_common_error());
        }

        auto result = dependency_graph_.check_with_dependencies(name);
        update_stats(result);
        cached_results_[name] = result;
        return common::ok(result);
    }

    /**
     * @brief Execute all registered health checks.
     * @return Map of check name to result for every registered check
     */
    std::unordered_map<std::string, health_check_result> check_all() {
        std::lock_guard<std::shared_mutex> lock(mutex_);

        std::unordered_map<std::string, health_check_result> results;
        for (const auto& [name, check] : checks_) {
            auto result = check->check();
            results[name] = result;
            cached_results_[name] = result;
            update_stats(result);
        }
        return results;
    }

    /**
     * @brief Add a dependency between two registered health checks.
     * @param dependent Name of the check that depends on another
     * @param dependency Name of the check being depended upon
     * @return Ok(true) on success, or error if checks not found or cycle detected
     */
    common::Result<bool> add_dependency(const std::string& dependent, const std::string& dependency) {
        std::lock_guard<std::shared_mutex> lock(mutex_);
        return dependency_graph_.add_dependency(dependent, dependency);
    }

    /**
     * @brief Start the periodic health monitoring background thread.
     * @return Ok on success; no-op if already running
     */
    common::VoidResult start() {
        std::lock_guard<std::mutex> lock(lifecycle_mutex_);

        if (running_.load()) {
            return common::ok();
        }

        running_.store(true);
        monitor_thread_ = std::thread([this]() { run_monitoring_loop(); });
        return common::ok();
    }

    /**
     * @brief Stop the periodic health monitoring background thread.
     * @return Ok on success; no-op if not running. Blocks until the thread joins.
     */
    common::VoidResult stop() {
        std::lock_guard<std::mutex> lock(lifecycle_mutex_);

        if (!running_.load()) {
            return common::ok();
        }

        running_.store(false);
        cv_.notify_all();

        if (monitor_thread_.joinable()) {
            monitor_thread_.join();
        }

        return common::ok();
    }

    /**
     * @brief Check whether the monitoring background thread is running.
     * @return true if the monitoring loop is active
     */
    bool is_running() const {
        return running_.load();
    }

    /**
     * @brief Manually refresh all health checks and trigger recovery if needed.
     *
     * @details Runs every registered check, updates cached results and statistics,
     * and invokes recovery handlers for unhealthy checks when auto-recovery is enabled.
     */
    void refresh() {
        std::lock_guard<std::shared_mutex> lock(mutex_);

        for (const auto& [name, check] : checks_) {
            auto result = check->check();
            cached_results_[name] = result;
            update_stats(result);

            if (result.status == health_status::unhealthy) {
                auto it = recovery_handlers_.find(name);
                if (it != recovery_handlers_.end() && config_.enable_auto_recovery) {
                    stats_.recovery_attempts++;
                    if (it->second()) {
                        stats_.successful_recoveries++;
                    }
                }
            }
        }

        stats_.last_check_time = std::chrono::system_clock::now();
    }

    /**
     * @brief Register a recovery handler for a named health check.
     * @param check_name Name of the health check this handler is for
     * @param handler Callable that attempts recovery; returns true on success
     */
    void register_recovery_handler(const std::string& check_name,
                                   std::function<bool()> handler) {
        std::lock_guard<std::shared_mutex> lock(mutex_);
        recovery_handlers_[check_name] = std::move(handler);
    }

    /**
     * @brief Get the aggregate health status across all cached results.
     * @return healthy if all checks pass, degraded if any are degraded,
     *         unhealthy if any are unhealthy, unknown if no results exist
     */
    health_status get_overall_status() {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        if (checks_.empty()) {
            return cached_results_.empty() ? health_status::healthy : health_status::unknown;
        }

        bool has_unhealthy = false;
        bool has_degraded = false;

        for (const auto& [name, result] : cached_results_) {
            if (result.status == health_status::unhealthy) {
                has_unhealthy = true;
            } else if (result.status == health_status::degraded) {
                has_degraded = true;
            }
        }

        if (has_unhealthy) return health_status::unhealthy;
        if (has_degraded) return health_status::degraded;
        return health_status::healthy;
    }

    /**
     * @brief Get accumulated health monitoring statistics.
     * @return Copy of the current statistics
     */
    health_monitor_stats get_stats() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return stats_;
    }

    /**
     * @brief Generate a human-readable health report.
     * @return Multi-line string summarizing the status of all cached checks
     */
    std::string get_health_report() {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        std::string report = "Health Report:\n";

        if (cached_results_.empty()) {
            report += "  No health checks have been performed yet.\n";
            return report;
        }

        for (const auto& [name, result] : cached_results_) {
            report += "  " + name + ": ";
            switch (result.status) {
                case health_status::healthy:
                    report += "HEALTHY";
                    break;
                case health_status::degraded:
                    report += "DEGRADED";
                    break;
                case health_status::unhealthy:
                    report += "UNHEALTHY";
                    break;
                default:
                    report += "UNKNOWN";
                    break;
            }
            report += " - " + result.message + "\n";
        }

        return report;
    }

    /**
     * @brief Quick self-check of the health monitor itself.
     * @return Always returns healthy with "Health monitor operational" message
     */
    health_check_result check_health() const {
        health_check_result result;
        result.status = health_status::healthy;
        result.message = "Health monitor operational";
        result.timestamp = std::chrono::system_clock::now();
        return result;
    }

private:
    void run_monitoring_loop() {
        while (running_.load()) {
            refresh();

            std::unique_lock<std::mutex> lock(cv_mutex_);
            cv_.wait_for(lock, config_.check_interval, [this]() {
                return !running_.load();
            });
        }
    }

    void update_stats(const health_check_result& result) {
        stats_.total_checks++;
        switch (result.status) {
            case health_status::healthy:
                stats_.healthy_checks++;
                break;
            case health_status::unhealthy:
                stats_.unhealthy_checks++;
                break;
            case health_status::degraded:
                stats_.degraded_checks++;
                break;
            default:
                break;
        }
    }

    health_monitor_config config_;
    health_monitor_stats stats_;
    health_dependency_graph dependency_graph_;

    mutable std::shared_mutex mutex_;
    std::mutex lifecycle_mutex_;
    std::mutex cv_mutex_;
    std::condition_variable cv_;

    std::unordered_map<std::string, std::shared_ptr<health_check>> checks_;
    std::unordered_map<std::string, std::function<bool()>> recovery_handlers_;
    std::unordered_map<std::string, health_check_result> cached_results_;

    std::atomic<bool> running_{false};
    std::thread monitor_thread_;
};

/**
 * @brief Get the global health monitor singleton instance
 * @return Reference to the global health monitor
 */
inline health_monitor& global_health_monitor() {
    static health_monitor instance;
    return instance;
}

} // namespace kcenon::monitoring
