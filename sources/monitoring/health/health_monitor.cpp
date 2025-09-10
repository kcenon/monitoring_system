/**
 * @file health_monitor.cpp
 * @brief Health monitoring implementation
 */

#include "health_monitor.h"
#include <sstream>
#include <algorithm>
#include <queue>
#include <shared_mutex>

namespace monitoring_system {

// Health Dependency Graph Implementation
monitoring_system::result<bool> health_dependency_graph::add_node(
    const std::string& name,
    std::shared_ptr<health_check> check) {
    
    if (!check) {
        return monitoring_system::make_error<bool>(
            monitoring_error_code::invalid_argument,
            "Health check cannot be null"
        );
    }
    
    std::unique_lock lock(graph_mutex_);
    
    if (nodes_.find(name) != nodes_.end()) {
        return monitoring_system::make_error<bool>(
            monitoring_error_code::already_exists,
            "Node already exists: " + name
        );
    }
    
    node n;
    n.name = name;
    n.check = check;
    n.last_check = std::chrono::system_clock::time_point{};
    nodes_[name] = n;
    
    return monitoring_system::result<bool>(true);
}

monitoring_system::result<bool> health_dependency_graph::add_dependency(
    const std::string& dependent,
    const std::string& dependency) {
    
    std::unique_lock lock(graph_mutex_);
    
    // Check if both nodes exist
    if (nodes_.find(dependent) == nodes_.end()) {
        return monitoring_system::make_error<bool>(
            monitoring_error_code::not_found,
            "Dependent node not found: " + dependent
        );
    }
    
    if (nodes_.find(dependency) == nodes_.end()) {
        return monitoring_system::make_error<bool>(
            monitoring_error_code::not_found,
            "Dependency node not found: " + dependency
        );
    }
    
    // Check for cycle
    if (would_create_cycle(dependent, dependency)) {
        return monitoring_system::make_error<bool>(
            monitoring_error_code::invalid_state,
            "Adding dependency would create a cycle"
        );
    }
    
    // Add the dependency
    nodes_[dependent].dependencies.insert(dependency);
    nodes_[dependency].dependents.insert(dependent);
    
    return monitoring_system::result<bool>(true);
}

monitoring_system::result<bool> health_dependency_graph::remove_dependency(
    const std::string& dependent,
    const std::string& dependency) {
    
    std::unique_lock lock(graph_mutex_);
    
    auto dep_it = nodes_.find(dependent);
    if (dep_it == nodes_.end()) {
        return monitoring_system::make_error<bool>(
            monitoring_error_code::not_found,
            "Dependent node not found: " + dependent
        );
    }
    
    auto dependency_it = nodes_.find(dependency);
    if (dependency_it == nodes_.end()) {
        return monitoring_system::make_error<bool>(
            monitoring_error_code::not_found,
            "Dependency node not found: " + dependency
        );
    }
    
    dep_it->second.dependencies.erase(dependency);
    dependency_it->second.dependents.erase(dependent);
    
    return monitoring_system::result<bool>(true);
}

std::vector<std::string> health_dependency_graph::get_dependencies(
    const std::string& name) const {
    
    std::lock_guard lock(graph_mutex_);
    
    auto it = nodes_.find(name);
    if (it == nodes_.end()) {
        return {};
    }
    
    return std::vector<std::string>(
        it->second.dependencies.begin(),
        it->second.dependencies.end()
    );
}

std::vector<std::string> health_dependency_graph::get_dependents(
    const std::string& name) const {
    
    std::lock_guard lock(graph_mutex_);
    
    auto it = nodes_.find(name);
    if (it == nodes_.end()) {
        return {};
    }
    
    return std::vector<std::string>(
        it->second.dependents.begin(),
        it->second.dependents.end()
    );
}

bool health_dependency_graph::would_create_cycle(
    const std::string& dependent,
    const std::string& dependency) const {
    
    // DFS to check if dependency can reach dependent
    std::unordered_set<std::string> visited;
    std::queue<std::string> to_visit;
    
    to_visit.push(dependency);
    
    while (!to_visit.empty()) {
        std::string current = to_visit.front();
        to_visit.pop();
        
        if (current == dependent) {
            return true;  // Cycle detected
        }
        
        if (visited.find(current) != visited.end()) {
            continue;
        }
        
        visited.insert(current);
        
        auto it = nodes_.find(current);
        if (it != nodes_.end()) {
            for (const auto& dep : it->second.dependencies) {
                to_visit.push(dep);
            }
        }
    }
    
    return false;
}

std::vector<std::string> health_dependency_graph::topological_sort() const {
    std::lock_guard lock(graph_mutex_);
    
    std::vector<std::string> result;
    std::unordered_map<std::string, size_t> in_degree;
    
    // Calculate in-degrees
    for (const auto& [name, node] : nodes_) {
        in_degree[name] = node.dependencies.size();
    }
    
    // Find nodes with no dependencies
    std::queue<std::string> zero_in_degree;
    for (const auto& [name, degree] : in_degree) {
        if (degree == 0) {
            zero_in_degree.push(name);
        }
    }
    
    // Process nodes
    while (!zero_in_degree.empty()) {
        std::string current = zero_in_degree.front();
        zero_in_degree.pop();
        result.push_back(current);
        
        // Reduce in-degree for dependents
        auto it = nodes_.find(current);
        if (it != nodes_.end()) {
            for (const auto& dependent : it->second.dependents) {
                in_degree[dependent]--;
                if (in_degree[dependent] == 0) {
                    zero_in_degree.push(dependent);
                }
            }
        }
    }
    
    return result;
}

health_check_result health_dependency_graph::check_with_dependencies(
    const std::string& name) {
    
    std::unique_lock lock(graph_mutex_);
    
    auto it = nodes_.find(name);
    if (it == nodes_.end()) {
        return health_check_result::unhealthy("Node not found: " + name);
    }
    
    // Check dependencies first
    for (const auto& dep_name : it->second.dependencies) {
        auto dep_it = nodes_.find(dep_name);
        if (dep_it != nodes_.end()) {
            // Check dependency
            auto dep_result = dep_it->second.check->check();
            dep_it->second.last_result = dep_result;
            dep_it->second.last_check = std::chrono::system_clock::now();
            
            if (!dep_result.is_operational()) {
                return health_check_result::unhealthy(
                    "Dependency " + dep_name + " is not operational"
                );
            }
        }
    }
    
    // Check the node itself
    auto result = it->second.check->check();
    it->second.last_result = result;
    it->second.last_check = std::chrono::system_clock::now();
    
    return result;
}

std::vector<std::string> health_dependency_graph::get_failure_impact(
    const std::string& name) const {
    
    std::lock_guard lock(graph_mutex_);
    
    std::vector<std::string> impacted;
    std::unordered_set<std::string> visited;
    std::queue<std::string> to_visit;
    
    to_visit.push(name);
    
    while (!to_visit.empty()) {
        std::string current = to_visit.front();
        to_visit.pop();
        
        if (visited.find(current) != visited.end()) {
            continue;
        }
        
        visited.insert(current);
        
        if (current != name) {
            impacted.push_back(current);
        }
        
        auto it = nodes_.find(current);
        if (it != nodes_.end()) {
            for (const auto& dependent : it->second.dependents) {
                to_visit.push(dependent);
            }
        }
    }
    
    return impacted;
}

// Health Monitor Implementation
struct health_monitor::monitor_impl {
    health_monitor_config config;
    std::unordered_map<std::string, std::shared_ptr<health_check>> checks;
    std::unordered_map<std::string, std::function<bool()>> recovery_handlers;
    std::unordered_map<std::string, health_check_result> cached_results;
    std::unordered_map<std::string, std::uint32_t> recovery_attempts;
    
    health_dependency_graph dependency_graph;
    health_stats stats;
    
    mutable std::shared_mutex checks_mutex;
    std::atomic<bool> running{false};
    std::thread monitor_thread;
    
    ~monitor_impl() {
        stop();
    }
    
    void stop() {
        if (running.exchange(false)) {
            if (monitor_thread.joinable()) {
                monitor_thread.join();
            }
        }
    }
    
    void monitoring_loop() {
        auto last_check = std::chrono::steady_clock::now();
        while (running) {
            // Use shorter sleep for responsiveness
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            if (!running) break;
            
            auto now = std::chrono::steady_clock::now();
            if (now - last_check >= config.check_interval) {
                perform_all_checks();
                last_check = now;
            }
        }
    }
    
    void perform_all_checks() {
        std::shared_lock lock(checks_mutex);
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Get topologically sorted checks
        auto sorted_checks = dependency_graph.topological_sort();
        
        // Perform checks in order
        for (const auto& name : sorted_checks) {
            auto check_it = checks.find(name);
            if (check_it != checks.end()) {
                auto result = perform_single_check(name, check_it->second);
                
                // Handle recovery if needed
                if (!result.is_operational() && config.enable_auto_recovery) {
                    attempt_recovery(name);
                }
            }
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time
        );
        
        // Update statistics
        stats.last_check_time = std::chrono::system_clock::now();
        if (stats.total_checks > 0) {
            stats.average_check_duration = 
                (stats.average_check_duration * (stats.total_checks - 1) + duration) / 
                stats.total_checks;
        } else {
            stats.average_check_duration = duration;
        }
    }
    
    health_check_result perform_single_check(
        const std::string& name,
        std::shared_ptr<health_check> check) {
        
        auto start = std::chrono::high_resolution_clock::now();
        
        // Use async with timeout
        auto future = std::async(std::launch::async, [check]() {
            return check->check();
        });
        
        health_check_result result;
        
        if (future.wait_for(check->get_timeout()) == std::future_status::ready) {
            result = future.get();
        } else {
            result = health_check_result::unhealthy("Health check timed out");
            stats.timeout_count++;
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        result.check_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end - start
        );
        
        // Update statistics
        stats.total_checks++;
        switch (result.status) {
            case health_status::healthy:
                stats.healthy_checks++;
                break;
            case health_status::degraded:
                stats.degraded_checks++;
                break;
            case health_status::unhealthy:
                stats.unhealthy_checks++;
                break;
            default:
                break;
        }
        
        // Cache the result
        cached_results[name] = result;
        
        return result;
    }
    
    bool attempt_recovery(const std::string& name) {
        auto handler_it = recovery_handlers.find(name);
        if (handler_it == recovery_handlers.end()) {
            return false;
        }
        
        auto& attempts = recovery_attempts[name];
        if (attempts >= config.max_recovery_attempts) {
            return false;
        }
        
        attempts++;
        stats.recovery_attempts++;
        
        // Wait before attempting recovery
        std::this_thread::sleep_for(config.recovery_delay);
        
        bool success = handler_it->second();
        if (success) {
            stats.successful_recoveries++;
            recovery_attempts[name] = 0;  // Reset attempts on success
        }
        
        return success;
    }
};

health_monitor::health_monitor(const health_monitor_config& config)
    : impl_(std::make_unique<monitor_impl>()) {
    impl_->config = config;
}

health_monitor::~health_monitor() = default;

health_monitor::health_monitor(health_monitor&&) noexcept = default;
health_monitor& health_monitor::operator=(health_monitor&&) noexcept = default;

monitoring_system::result<bool> health_monitor::register_check(
    const std::string& name,
    std::shared_ptr<health_check> check) {
    
    if (!check) {
        return monitoring_system::make_error<bool>(
            monitoring_error_code::invalid_argument,
            "Health check cannot be null"
        );
    }
    
    std::unique_lock lock(impl_->checks_mutex);
    
    if (impl_->checks.find(name) != impl_->checks.end()) {
        return monitoring_system::make_error<bool>(
            monitoring_error_code::already_exists,
            "Health check already registered: " + name
        );
    }
    
    impl_->checks[name] = check;
    
    // Add to dependency graph
    return impl_->dependency_graph.add_node(name, check);
}

monitoring_system::result<bool> health_monitor::unregister_check(
    const std::string& name) {
    
    std::unique_lock lock(impl_->checks_mutex);
    
    auto it = impl_->checks.find(name);
    if (it == impl_->checks.end()) {
        return monitoring_system::make_error<bool>(
            monitoring_error_code::not_found,
            "Health check not found: " + name
        );
    }
    
    impl_->checks.erase(it);
    impl_->cached_results.erase(name);
    impl_->recovery_handlers.erase(name);
    impl_->recovery_attempts.erase(name);
    
    return monitoring_system::result<bool>(true);
}

monitoring_system::result<bool> health_monitor::add_dependency(
    const std::string& dependent,
    const std::string& dependency) {
    
    return impl_->dependency_graph.add_dependency(dependent, dependency);
}

monitoring_system::result<bool> health_monitor::start() {
    if (impl_->running.exchange(true)) {
        // Already running
        return monitoring_system::result<bool>(true);
    }
    
    impl_->monitor_thread = std::thread(
        &monitor_impl::monitoring_loop, impl_.get()
    );
    
    return monitoring_system::result<bool>(true);
}

monitoring_system::result<bool> health_monitor::stop() {
    impl_->stop();
    return monitoring_system::result<bool>(true);
}

bool health_monitor::is_running() const {
    return impl_->running.load();
}

monitoring_system::result<health_check_result> health_monitor::check(
    const std::string& name) {
    
    std::shared_lock lock(impl_->checks_mutex);
    
    auto it = impl_->checks.find(name);
    if (it == impl_->checks.end()) {
        return monitoring_system::make_error<health_check_result>(
            monitoring_error_code::not_found,
            "Health check not found: " + name
        );
    }
    
    // Check if we have a recent cached result
    auto cached_it = impl_->cached_results.find(name);
    if (cached_it != impl_->cached_results.end()) {
        auto age = std::chrono::system_clock::now() - cached_it->second.timestamp;
        if (age < impl_->config.cache_duration) {
            return cached_it->second;
        }
    }
    
    // Perform the check with dependencies
    auto result = impl_->dependency_graph.check_with_dependencies(name);
    impl_->cached_results[name] = result;
    
    return result;
}

std::unordered_map<std::string, health_check_result> health_monitor::check_all() {
    std::unordered_map<std::string, health_check_result> results;
    
    std::shared_lock lock(impl_->checks_mutex);
    
    for (const auto& [name, check] : impl_->checks) {
        auto result = impl_->perform_single_check(name, check);
        results[name] = result;
    }
    
    return results;
}

health_status health_monitor::get_overall_status() const {
    std::shared_lock lock(impl_->checks_mutex);
    
    bool any_unhealthy = false;
    bool any_degraded = false;
    bool any_healthy = false;
    
    for (const auto& [name, result] : impl_->cached_results) {
        switch (result.status) {
            case health_status::healthy:
                any_healthy = true;
                break;
            case health_status::degraded:
                any_degraded = true;
                break;
            case health_status::unhealthy:
                any_unhealthy = true;
                break;
            default:
                break;
        }
    }
    
    if (any_unhealthy) {
        return health_status::unhealthy;
    } else if (any_degraded) {
        return health_status::degraded;
    } else if (any_healthy) {
        return health_status::healthy;
    } else {
        return health_status::unknown;
    }
}

health_stats health_monitor::get_stats() const {
    return impl_->stats;
}

void health_monitor::register_recovery_handler(
    const std::string& check_name,
    std::function<bool()> handler) {
    
    std::unique_lock lock(impl_->checks_mutex);
    impl_->recovery_handlers[check_name] = handler;
}

std::string health_monitor::get_health_report() const {
    std::shared_lock lock(impl_->checks_mutex);
    
    std::ostringstream report;
    report << "Health Report\n";
    report << "=============\n\n";
    
    report << "Overall Status: ";
    switch (get_overall_status()) {
        case health_status::healthy:
            report << "HEALTHY\n";
            break;
        case health_status::degraded:
            report << "DEGRADED\n";
            break;
        case health_status::unhealthy:
            report << "UNHEALTHY\n";
            break;
        default:
            report << "UNKNOWN\n";
    }
    
    report << "\nIndividual Checks:\n";
    for (const auto& [name, result] : impl_->cached_results) {
        report << "  - " << name << ": ";
        switch (result.status) {
            case health_status::healthy:
                report << "✓ HEALTHY";
                break;
            case health_status::degraded:
                report << "⚠ DEGRADED";
                break;
            case health_status::unhealthy:
                report << "✗ UNHEALTHY";
                break;
            default:
                report << "? UNKNOWN";
        }
        report << " (" << result.message << ")\n";
    }
    
    report << "\nStatistics:\n";
    report << "  Total Checks: " << impl_->stats.total_checks << "\n";
    report << "  Healthy: " << impl_->stats.healthy_checks << "\n";
    report << "  Degraded: " << impl_->stats.degraded_checks << "\n";
    report << "  Unhealthy: " << impl_->stats.unhealthy_checks << "\n";
    report << "  Timeouts: " << impl_->stats.timeout_count << "\n";
    
    if (impl_->config.enable_auto_recovery) {
        report << "\nRecovery:\n";
        report << "  Attempts: " << impl_->stats.recovery_attempts << "\n";
        report << "  Successful: " << impl_->stats.successful_recoveries << "\n";
    }
    
    return report.str();
}

monitoring_system::result<bool> health_monitor::refresh() {
    impl_->perform_all_checks();
    return monitoring_system::result<bool>(true);
}

// Global instance
health_monitor& global_health_monitor() {
    static health_monitor instance;
    return instance;
}

} // namespace monitoring_system