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
 * @brief Health check types
 */
enum class health_check_type {
    liveness,
    readiness,
    startup
};

/**
 * @brief Health monitor configuration
 */
struct health_monitor_config {
    std::chrono::milliseconds check_interval{std::chrono::milliseconds(5000)};
    std::chrono::seconds cache_duration{std::chrono::seconds(1)};
    bool enable_auto_recovery{true};
    size_t max_consecutive_failures{3};
    std::chrono::seconds recovery_timeout{std::chrono::seconds(30)};
};

/**
 * @brief Statistics for health monitoring
 */
struct health_monitor_stats {
    size_t total_checks{0};
    size_t healthy_checks{0};
    size_t unhealthy_checks{0};
    size_t degraded_checks{0};
    size_t recovery_attempts{0};
    size_t successful_recoveries{0};
    std::chrono::system_clock::time_point last_check_time;
};

/**
 * @brief Abstract base class for health checks
 */
class health_check {
public:
    virtual ~health_check() = default;

    virtual std::string get_name() const = 0;
    virtual health_check_type get_type() const = 0;
    virtual health_check_result check() = 0;

    virtual std::chrono::milliseconds get_timeout() const {
        return std::chrono::milliseconds(1000);
    }

    virtual bool is_critical() const {
        return false;
    }
};

/**
 * @brief Functional health check implementation
 */
class functional_health_check : public health_check {
public:
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

    std::string get_name() const override { return name_; }
    health_check_type get_type() const override { return type_; }
    std::chrono::milliseconds get_timeout() const override { return timeout_; }
    bool is_critical() const override { return critical_; }

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
 * @brief Composite health check that aggregates multiple health checks
 */
class composite_health_check : public health_check {
public:
    composite_health_check(const std::string& name,
                           health_check_type type,
                           bool all_required = true)
        : name_(name)
        , type_(type)
        , all_required_(all_required) {}

    std::string get_name() const override { return name_; }
    health_check_type get_type() const override { return type_; }

    void add_check(std::shared_ptr<health_check> check) {
        std::lock_guard<std::mutex> lock(mutex_);
        checks_.push_back(std::move(check));
    }

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
 * @brief Directed acyclic graph for health check dependencies
 */
class health_dependency_graph {
public:
    result<bool> add_node(const std::string& name, std::shared_ptr<health_check> check) {
        std::lock_guard<std::shared_mutex> lock(mutex_);

        if (nodes_.find(name) != nodes_.end()) {
            return make_error<bool>(monitoring_error_code::already_exists,
                                   "Node '" + name + "' already exists");
        }

        nodes_[name] = std::move(check);
        dependencies_[name] = {};
        dependents_[name] = {};
        return make_success(true);
    }

    result<bool> add_dependency(const std::string& dependent, const std::string& dependency) {
        std::lock_guard<std::shared_mutex> lock(mutex_);

        if (nodes_.find(dependent) == nodes_.end()) {
            return make_error<bool>(monitoring_error_code::not_found,
                                   "Dependent '" + dependent + "' not found");
        }
        if (nodes_.find(dependency) == nodes_.end()) {
            return make_error<bool>(monitoring_error_code::not_found,
                                   "Dependency '" + dependency + "' not found");
        }

        if (would_create_cycle_internal(dependent, dependency)) {
            return make_error<bool>(monitoring_error_code::invalid_state,
                                   "Adding dependency would create a cycle");
        }

        dependencies_[dependent].push_back(dependency);
        dependents_[dependency].push_back(dependent);
        return make_success(true);
    }

    std::vector<std::string> get_dependencies(const std::string& name) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        auto it = dependencies_.find(name);
        if (it != dependencies_.end()) {
            return it->second;
        }
        return {};
    }

    std::vector<std::string> get_dependents(const std::string& name) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        auto it = dependents_.find(name);
        if (it != dependents_.end()) {
            return it->second;
        }
        return {};
    }

    bool would_create_cycle(const std::string& from, const std::string& to) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return would_create_cycle_internal(from, to);
    }

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
 * @brief Builder pattern for creating health checks
 */
class health_check_builder {
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
        check_func_ = std::move(func);
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
 * @brief Health monitor with dependency management and statistics
 */
class health_monitor {
public:
    health_monitor() = default;
    explicit health_monitor(const health_monitor_config& config) : config_(config) {}
    virtual ~health_monitor() { stop(); }

    result<bool> register_check(const std::string& name, std::shared_ptr<health_check> check) {
        std::lock_guard<std::shared_mutex> lock(mutex_);

        if (checks_.find(name) != checks_.end()) {
            return make_error<bool>(monitoring_error_code::already_exists,
                                   "Check '" + name + "' already registered");
        }

        checks_[name] = std::move(check);
        auto graph_result = dependency_graph_.add_node(name, checks_[name]);
        if (graph_result.is_err()) {
            checks_.erase(name);
            return common::Result<bool>::err(graph_result.error());
        }
        return make_success(true);
    }

    result<bool> unregister_check(const std::string& name) {
        std::lock_guard<std::shared_mutex> lock(mutex_);

        if (checks_.find(name) == checks_.end()) {
            return make_error<bool>(monitoring_error_code::not_found,
                                   "Check '" + name + "' not found");
        }

        checks_.erase(name);
        recovery_handlers_.erase(name);
        return make_success(true);
    }

    result<health_check_result> check(const std::string& name) {
        std::lock_guard<std::shared_mutex> lock(mutex_);

        auto it = checks_.find(name);
        if (it == checks_.end()) {
            return make_error<health_check_result>(monitoring_error_code::not_found,
                                                   "Check '" + name + "' not found");
        }

        auto result = dependency_graph_.check_with_dependencies(name);
        update_stats(result);
        cached_results_[name] = result;
        return make_success(result);
    }

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

    result<bool> add_dependency(const std::string& dependent, const std::string& dependency) {
        std::lock_guard<std::shared_mutex> lock(mutex_);
        return dependency_graph_.add_dependency(dependent, dependency);
    }

    result_void start() {
        std::lock_guard<std::mutex> lock(lifecycle_mutex_);

        if (running_.load()) {
            return make_void_success();
        }

        running_.store(true);
        monitor_thread_ = std::thread([this]() { run_monitoring_loop(); });
        return make_void_success();
    }

    result_void stop() {
        std::lock_guard<std::mutex> lock(lifecycle_mutex_);

        if (!running_.load()) {
            return make_void_success();
        }

        running_.store(false);
        cv_.notify_all();

        if (monitor_thread_.joinable()) {
            monitor_thread_.join();
        }

        return make_void_success();
    }

    bool is_running() const {
        return running_.load();
    }

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

    void register_recovery_handler(const std::string& check_name,
                                   std::function<bool()> handler) {
        std::lock_guard<std::shared_mutex> lock(mutex_);
        recovery_handlers_[check_name] = std::move(handler);
    }

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

    health_monitor_stats get_stats() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return stats_;
    }

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
