// BSD 3-Clause License
//
// Copyright (c) 2021-2025, kcenon
// All rights reserved.
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

/**
 * @file adaptive.cppm
 * @brief Adaptive partition for kcenon.monitoring module
 *
 * This module partition provides adaptive monitoring capabilities that
 * automatically adjust collection behavior based on system load.
 *
 * Contents:
 * - Adaptation strategies and load levels
 * - Adaptive configuration
 * - Adaptive collector wrapper
 * - Adaptive monitoring controller
 * - Alert rule engine (internal)
 * - Notification management (internal)
 */
module;

// Standard library includes (global module fragment)
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

export module kcenon.monitoring.adaptive;

export import kcenon.monitoring.core;
export import kcenon.monitoring.collectors;

// ============================================================================
// Adaptation Types
// ============================================================================

export namespace kcenon::monitoring {

/**
 * @enum adaptation_strategy
 * @brief Strategy for adapting monitoring behavior
 */
enum class adaptation_strategy {
    conservative,  ///< Prefer system stability over monitoring detail
    balanced,      ///< Balance between monitoring and performance
    aggressive     ///< Prefer monitoring detail over system resources
};

/**
 * @enum load_level
 * @brief System load classification levels
 */
enum class load_level {
    idle,      ///< < 20% CPU usage
    low,       ///< 20-40% CPU usage
    moderate,  ///< 40-60% CPU usage
    high,      ///< 60-80% CPU usage
    critical   ///< > 80% CPU usage
};

/**
 * @brief Convert load level to string
 * @param level The load level
 * @return String representation
 */
constexpr std::string_view load_level_to_string(load_level level) noexcept {
    switch (level) {
        case load_level::idle: return "idle";
        case load_level::low: return "low";
        case load_level::moderate: return "moderate";
        case load_level::high: return "high";
        case load_level::critical: return "critical";
        default: return "unknown";
    }
}

/**
 * @brief Convert adaptation strategy to string
 * @param strategy The strategy
 * @return String representation
 */
constexpr std::string_view strategy_to_string(adaptation_strategy strategy) noexcept {
    switch (strategy) {
        case adaptation_strategy::conservative: return "conservative";
        case adaptation_strategy::balanced: return "balanced";
        case adaptation_strategy::aggressive: return "aggressive";
        default: return "unknown";
    }
}

// ============================================================================
// Adaptive Configuration
// ============================================================================

/**
 * @struct adaptive_config
 * @brief Configuration parameters for adaptive monitoring
 */
struct adaptive_config {
    // CPU load thresholds (percentage)
    double idle_threshold{20.0};
    double low_threshold{40.0};
    double moderate_threshold{60.0};
    double high_threshold{80.0};

    // Memory thresholds (percentage)
    double memory_warning_threshold{70.0};
    double memory_critical_threshold{85.0};

    // Collection intervals by load level
    std::chrono::milliseconds idle_interval{100};
    std::chrono::milliseconds low_interval{250};
    std::chrono::milliseconds moderate_interval{500};
    std::chrono::milliseconds high_interval{1000};
    std::chrono::milliseconds critical_interval{5000};

    // Sampling rates by load level (0.0 to 1.0)
    double idle_sampling_rate{1.0};
    double low_sampling_rate{0.8};
    double moderate_sampling_rate{0.5};
    double high_sampling_rate{0.2};
    double critical_sampling_rate{0.1};

    // Adaptation parameters
    adaptation_strategy strategy{adaptation_strategy::balanced};
    std::chrono::seconds adaptation_interval{10};
    double smoothing_factor{0.7};  ///< Exponential smoothing for load average

    // Threshold tuning (hysteresis and cooldown)
    double hysteresis_margin{5.0};  ///< Percentage margin to prevent oscillation
    std::chrono::milliseconds cooldown_period{1000};  ///< Minimum time between level changes
    bool enable_hysteresis{true};
    bool enable_cooldown{true};

    /**
     * @brief Get collection interval for a load level
     * @param level The load level
     * @return Collection interval
     */
    std::chrono::milliseconds get_interval_for_load(load_level level) const {
        switch (level) {
            case load_level::idle: return idle_interval;
            case load_level::low: return low_interval;
            case load_level::moderate: return moderate_interval;
            case load_level::high: return high_interval;
            case load_level::critical: return critical_interval;
        }
        return moderate_interval;
    }

    /**
     * @brief Get sampling rate for a load level
     * @param level The load level
     * @return Sampling rate (0.0 to 1.0)
     */
    double get_sampling_rate_for_load(load_level level) const {
        switch (level) {
            case load_level::idle: return idle_sampling_rate;
            case load_level::low: return low_sampling_rate;
            case load_level::moderate: return moderate_sampling_rate;
            case load_level::high: return high_sampling_rate;
            case load_level::critical: return critical_sampling_rate;
        }
        return moderate_sampling_rate;
    }
};

// ============================================================================
// Adaptation Statistics
// ============================================================================

/**
 * @struct adaptation_stats
 * @brief Statistics about adaptation behavior
 */
struct adaptation_stats {
    std::uint64_t total_adaptations{0};
    std::uint64_t upscale_count{0};    ///< Times sampling increased
    std::uint64_t downscale_count{0};  ///< Times sampling decreased
    std::uint64_t samples_dropped{0};
    std::uint64_t samples_collected{0};

    double average_cpu_usage{0.0};
    double average_memory_usage{0.0};

    load_level current_load_level{load_level::moderate};
    std::chrono::milliseconds current_interval{500};
    double current_sampling_rate{1.0};

    std::chrono::system_clock::time_point last_adaptation;
    std::chrono::system_clock::time_point last_level_change;

    // Threshold tuning statistics
    std::uint64_t hysteresis_prevented_changes{0};
    std::uint64_t cooldown_prevented_changes{0};

    /**
     * @brief Get the effective sample rate
     * @return Ratio of collected to total samples
     */
    double get_effective_sample_rate() const {
        auto total = samples_collected + samples_dropped;
        if (total == 0) return 1.0;
        return static_cast<double>(samples_collected) / static_cast<double>(total);
    }
};

// ============================================================================
// System Metrics for Adaptation
// ============================================================================

/**
 * @struct system_load_metrics
 * @brief System metrics used for adaptation decisions
 */
struct system_load_metrics {
    double cpu_usage_percent{0.0};
    double memory_usage_percent{0.0};
    double io_wait_percent{0.0};
    double network_utilization_percent{0.0};
    std::chrono::system_clock::time_point timestamp;

    system_load_metrics()
        : timestamp(std::chrono::system_clock::now()) {}

    system_load_metrics(double cpu, double memory)
        : cpu_usage_percent(cpu)
        , memory_usage_percent(memory)
        , timestamp(std::chrono::system_clock::now()) {}
};

// ============================================================================
// Adaptive Collector
// ============================================================================

/**
 * @class adaptive_collector_wrapper
 * @brief Wrapper that adds adaptive behavior to a collector
 *
 * This class wraps an existing collector and applies adaptive sampling
 * based on system load. It tracks statistics and adjusts collection
 * parameters dynamically.
 *
 * @thread_safety Thread-safe. All public methods can be called concurrently.
 */
class adaptive_collector_wrapper {
public:
    /**
     * @brief Construct an adaptive wrapper
     * @param collector The underlying collector
     * @param config Adaptive configuration
     */
    explicit adaptive_collector_wrapper(
        std::shared_ptr<interface_collector> collector,
        const adaptive_config& config = {})
        : collector_(std::move(collector))
        , config_(config)
        , enabled_(true)
        , current_sampling_rate_(1.0) {
        stats_.current_interval = config_.moderate_interval;
        stats_.last_adaptation = std::chrono::system_clock::now();
        stats_.last_level_change = stats_.last_adaptation;
    }

    /**
     * @brief Collect metrics with adaptive sampling
     * @return Collected metrics (empty if sample was dropped)
     */
    std::vector<metric_sample> collect() {
        if (!should_sample()) {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            ++stats_.samples_dropped;
            return {};
        }

        auto metrics = collector_->collect();
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            ++stats_.samples_collected;
        }
        return metrics;
    }

    /**
     * @brief Adapt collection behavior based on system load
     * @param sys_metrics Current system load metrics
     */
    void adapt(const system_load_metrics& sys_metrics) {
        // Copy config under lock
        adaptive_config local_config;
        {
            std::lock_guard<std::mutex> lock(config_mutex_);
            local_config = config_;
        }

        std::lock_guard<std::mutex> lock(stats_mutex_);

        bool is_first = (stats_.total_adaptations == 0);

        // Update averages
        if (is_first) {
            stats_.average_cpu_usage = sys_metrics.cpu_usage_percent;
            stats_.average_memory_usage = sys_metrics.memory_usage_percent;
        } else {
            stats_.average_cpu_usage =
                local_config.smoothing_factor * sys_metrics.cpu_usage_percent +
                (1.0 - local_config.smoothing_factor) * stats_.average_cpu_usage;
            stats_.average_memory_usage =
                local_config.smoothing_factor * sys_metrics.memory_usage_percent +
                (1.0 - local_config.smoothing_factor) * stats_.average_memory_usage;
        }

        // Determine new load level
        auto new_level = calculate_load_level(
            stats_.average_cpu_usage,
            stats_.average_memory_usage,
            stats_.current_load_level,
            local_config);

        if (new_level != stats_.current_load_level) {
            auto now = std::chrono::system_clock::now();

            // Check cooldown
            if (local_config.enable_cooldown && !is_first) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - stats_.last_level_change);
                if (elapsed < local_config.cooldown_period) {
                    ++stats_.cooldown_prevented_changes;
                    return;
                }
            }

            // Apply change
            if (new_level > stats_.current_load_level) {
                ++stats_.downscale_count;
            } else {
                ++stats_.upscale_count;
            }

            stats_.current_load_level = new_level;
            stats_.current_interval = local_config.get_interval_for_load(new_level);
            stats_.current_sampling_rate = local_config.get_sampling_rate_for_load(new_level);
            current_sampling_rate_.store(stats_.current_sampling_rate);
            ++stats_.total_adaptations;
            stats_.last_adaptation = now;
            stats_.last_level_change = now;
        }
    }

    /**
     * @brief Get adaptation statistics
     */
    adaptation_stats get_stats() const {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        return stats_;
    }

    /**
     * @brief Get current collection interval
     */
    std::chrono::milliseconds get_current_interval() const {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        return stats_.current_interval;
    }

    /**
     * @brief Get current load level
     */
    load_level get_current_load_level() const {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        return stats_.current_load_level;
    }

    /**
     * @brief Set adaptive configuration
     */
    void set_config(const adaptive_config& config) {
        std::lock_guard<std::mutex> lock(config_mutex_);
        config_ = config;
    }

    /**
     * @brief Get adaptive configuration
     */
    adaptive_config get_config() const {
        std::lock_guard<std::mutex> lock(config_mutex_);
        return config_;
    }

    /**
     * @brief Enable/disable adaptive behavior
     */
    void set_enabled(bool enabled) { enabled_ = enabled; }

    /**
     * @brief Check if adaptive behavior is enabled
     */
    bool is_enabled() const { return enabled_; }

    /**
     * @brief Get the wrapped collector name
     */
    std::string name() const {
        return collector_ ? collector_->name() : "";
    }

private:
    bool should_sample() const {
        if (!enabled_) return true;

        thread_local std::mt19937 gen(std::random_device{}());
        std::uniform_real_distribution<> dis(0.0, 1.0);
        return dis(gen) < current_sampling_rate_.load();
    }

    static load_level calculate_load_level(
        double cpu_usage,
        double memory_usage,
        load_level current_level,
        const adaptive_config& cfg) {

        // Calculate effective load
        double effective_load = cpu_usage;

        if (memory_usage > cfg.memory_critical_threshold) {
            effective_load = std::max(effective_load, cfg.high_threshold + 1.0);
        } else if (memory_usage > cfg.memory_warning_threshold) {
            effective_load = std::max(effective_load, cfg.moderate_threshold + 1.0);
        }

        // Apply strategy
        switch (cfg.strategy) {
            case adaptation_strategy::conservative:
                effective_load *= 0.8;
                break;
            case adaptation_strategy::aggressive:
                effective_load *= 1.2;
                break;
            default:
                break;
        }

        // Determine raw level
        load_level raw_level;
        if (effective_load >= cfg.high_threshold) {
            raw_level = load_level::critical;
        } else if (effective_load >= cfg.moderate_threshold) {
            raw_level = load_level::high;
        } else if (effective_load >= cfg.low_threshold) {
            raw_level = load_level::moderate;
        } else if (effective_load >= cfg.idle_threshold) {
            raw_level = load_level::low;
        } else {
            raw_level = load_level::idle;
        }

        // Apply hysteresis
        if (!cfg.enable_hysteresis || raw_level == current_level) {
            return raw_level;
        }

        double current_threshold = get_threshold_for_level(current_level, cfg);

        if (raw_level > current_level) {
            double next_threshold = get_threshold_for_level(
                static_cast<load_level>(static_cast<int>(current_level) + 1), cfg);
            if (effective_load < next_threshold + cfg.hysteresis_margin) {
                return current_level;
            }
        } else {
            if (effective_load > current_threshold - cfg.hysteresis_margin) {
                return current_level;
            }
        }

        return raw_level;
    }

    static double get_threshold_for_level(load_level level, const adaptive_config& cfg) {
        switch (level) {
            case load_level::idle: return 0.0;
            case load_level::low: return cfg.idle_threshold;
            case load_level::moderate: return cfg.low_threshold;
            case load_level::high: return cfg.moderate_threshold;
            case load_level::critical: return cfg.high_threshold;
        }
        return cfg.moderate_threshold;
    }

    std::shared_ptr<interface_collector> collector_;
    adaptive_config config_;
    mutable std::mutex config_mutex_;
    adaptation_stats stats_;
    mutable std::mutex stats_mutex_;
    std::atomic<bool> enabled_;
    std::atomic<double> current_sampling_rate_;
};

// ============================================================================
// Alert Types
// ============================================================================

/**
 * @enum alert_severity
 * @brief Severity levels for alerts
 */
enum class alert_severity {
    info,
    warning,
    error,
    critical
};

/**
 * @brief Convert alert severity to string
 */
constexpr std::string_view severity_to_string(alert_severity severity) noexcept {
    switch (severity) {
        case alert_severity::info: return "info";
        case alert_severity::warning: return "warning";
        case alert_severity::error: return "error";
        case alert_severity::critical: return "critical";
        default: return "unknown";
    }
}

/**
 * @struct alert
 * @brief An alert generated by the monitoring system
 */
struct alert {
    std::string id;
    std::string name;
    std::string message;
    alert_severity severity;
    std::string source;
    std::chrono::system_clock::time_point timestamp;
    std::optional<std::chrono::system_clock::time_point> resolved_at;
    std::unordered_map<std::string, std::string> labels;
    std::optional<double> value;
    std::optional<double> threshold;

    alert()
        : severity(alert_severity::info)
        , timestamp(std::chrono::system_clock::now()) {}

    alert(std::string n, std::string msg, alert_severity sev)
        : name(std::move(n))
        , message(std::move(msg))
        , severity(sev)
        , timestamp(std::chrono::system_clock::now()) {}

    bool is_resolved() const { return resolved_at.has_value(); }
};

// ============================================================================
// Alert Rule Types
// ============================================================================

/**
 * @enum comparison_operator
 * @brief Comparison operators for alert rules
 */
enum class comparison_operator {
    greater_than,
    greater_or_equal,
    less_than,
    less_or_equal,
    equal,
    not_equal
};

/**
 * @struct alert_rule
 * @brief Definition of an alert rule
 */
struct alert_rule {
    std::string id;
    std::string name;
    std::string metric_name;
    comparison_operator op{comparison_operator::greater_than};
    double threshold{0.0};
    alert_severity severity{alert_severity::warning};
    std::chrono::seconds duration{0};  ///< How long condition must be true
    std::string message_template;
    bool enabled{true};

    alert_rule() = default;

    alert_rule(std::string n, std::string metric, comparison_operator cmp,
               double thresh, alert_severity sev = alert_severity::warning)
        : name(std::move(n))
        , metric_name(std::move(metric))
        , op(cmp)
        , threshold(thresh)
        , severity(sev) {}

    /**
     * @brief Evaluate if the rule matches a value
     * @param value The value to check
     * @return true if the rule condition is met
     */
    bool evaluate(double value) const {
        switch (op) {
            case comparison_operator::greater_than: return value > threshold;
            case comparison_operator::greater_or_equal: return value >= threshold;
            case comparison_operator::less_than: return value < threshold;
            case comparison_operator::less_or_equal: return value <= threshold;
            case comparison_operator::equal: return std::abs(value - threshold) < 1e-9;
            case comparison_operator::not_equal: return std::abs(value - threshold) >= 1e-9;
        }
        return false;
    }
};

// ============================================================================
// Alert Manager
// ============================================================================

/**
 * @class alert_manager
 * @brief Manages alert rules and active alerts
 *
 * @thread_safety Thread-safe.
 */
class alert_manager {
public:
    using alert_callback = std::function<void(const alert&)>;

    /**
     * @brief Add an alert rule
     * @param rule The rule to add
     * @return true if added successfully
     */
    bool add_rule(const alert_rule& rule) {
        std::lock_guard<std::shared_mutex> lock(mutex_);
        if (rule.id.empty()) {
            return false;
        }
        rules_[rule.id] = rule;
        return true;
    }

    /**
     * @brief Remove an alert rule
     * @param rule_id The rule ID to remove
     * @return true if removed
     */
    bool remove_rule(const std::string& rule_id) {
        std::lock_guard<std::shared_mutex> lock(mutex_);
        return rules_.erase(rule_id) > 0;
    }

    /**
     * @brief Get a rule by ID
     * @param rule_id The rule ID
     * @return Optional rule
     */
    std::optional<alert_rule> get_rule(const std::string& rule_id) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = rules_.find(rule_id);
        if (it != rules_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    /**
     * @brief Get all rules
     * @return Vector of rules
     */
    std::vector<alert_rule> get_all_rules() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        std::vector<alert_rule> result;
        result.reserve(rules_.size());
        for (const auto& [id, rule] : rules_) {
            result.push_back(rule);
        }
        return result;
    }

    /**
     * @brief Process metrics and generate alerts
     * @param metrics The metrics to process
     * @return Generated alerts
     */
    std::vector<alert> process_metrics(const std::vector<metric_sample>& metrics) {
        std::vector<alert> alerts;

        std::shared_lock<std::shared_mutex> lock(mutex_);
        for (const auto& metric : metrics) {
            for (const auto& [id, rule] : rules_) {
                if (!rule.enabled || rule.metric_name != metric.name) {
                    continue;
                }

                // Extract numeric value
                double value = 0.0;
                if (auto* v = std::get_if<double>(&metric.value)) {
                    value = *v;
                } else if (auto* v = std::get_if<int64_t>(&metric.value)) {
                    value = static_cast<double>(*v);
                } else if (auto* v = std::get_if<uint64_t>(&metric.value)) {
                    value = static_cast<double>(*v);
                } else {
                    continue;  // Can't evaluate string values
                }

                if (rule.evaluate(value)) {
                    alert a(rule.name,
                           rule.message_template.empty() ?
                               rule.name + " threshold exceeded" : rule.message_template,
                           rule.severity);
                    a.source = metric.name;
                    a.value = value;
                    a.threshold = rule.threshold;
                    a.labels = metric.labels;
                    alerts.push_back(std::move(a));
                }
            }
        }

        // Notify callbacks
        for (const auto& a : alerts) {
            for (const auto& callback : callbacks_) {
                callback(a);
            }
        }

        return alerts;
    }

    /**
     * @brief Register an alert callback
     * @param callback The callback function
     */
    void on_alert(alert_callback callback) {
        std::lock_guard<std::shared_mutex> lock(mutex_);
        callbacks_.push_back(std::move(callback));
    }

    /**
     * @brief Clear all callbacks
     */
    void clear_callbacks() {
        std::lock_guard<std::shared_mutex> lock(mutex_);
        callbacks_.clear();
    }

    /**
     * @brief Get count of active rules
     */
    std::size_t rule_count() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return rules_.size();
    }

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, alert_rule> rules_;
    std::vector<alert_callback> callbacks_;
};

// ============================================================================
// Adaptive Monitor Controller
// ============================================================================

/**
 * @class adaptive_monitor_controller
 * @brief Controller for adaptive monitoring across multiple collectors
 *
 * This controller manages multiple adaptive collectors and coordinates
 * their adaptation based on global system load.
 *
 * @thread_safety Thread-safe.
 */
class adaptive_monitor_controller {
public:
    using wrapper_ptr = std::shared_ptr<adaptive_collector_wrapper>;

    adaptive_monitor_controller() = default;

    /**
     * @brief Register a collector for adaptive monitoring
     * @param name Unique name for the collector
     * @param collector The collector to wrap
     * @param config Adaptive configuration
     * @return true if registered successfully
     */
    bool register_collector(
        const std::string& name,
        std::shared_ptr<interface_collector> collector,
        const adaptive_config& config = {}) {

        if (!collector) return false;

        auto wrapper = std::make_shared<adaptive_collector_wrapper>(
            std::move(collector), config);

        std::lock_guard<std::shared_mutex> lock(mutex_);
        if (collectors_.find(name) != collectors_.end()) {
            return false;  // Already registered
        }
        collectors_[name] = std::move(wrapper);
        return true;
    }

    /**
     * @brief Unregister a collector
     * @param name The collector name
     * @return true if unregistered
     */
    bool unregister_collector(const std::string& name) {
        std::lock_guard<std::shared_mutex> lock(mutex_);
        return collectors_.erase(name) > 0;
    }

    /**
     * @brief Get a collector wrapper by name
     * @param name The collector name
     * @return Wrapper pointer or nullptr
     */
    wrapper_ptr get_collector(const std::string& name) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = collectors_.find(name);
        if (it != collectors_.end()) {
            return it->second;
        }
        return nullptr;
    }

    /**
     * @brief Adapt all registered collectors
     * @param sys_metrics Current system load
     */
    void adapt_all(const system_load_metrics& sys_metrics) {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        for (const auto& [name, wrapper] : collectors_) {
            wrapper->adapt(sys_metrics);
        }
    }

    /**
     * @brief Collect from all registered collectors
     * @return All collected metrics
     */
    std::vector<metric_sample> collect_all() {
        std::vector<metric_sample> all_metrics;

        std::shared_lock<std::shared_mutex> lock(mutex_);
        for (const auto& [name, wrapper] : collectors_) {
            auto metrics = wrapper->collect();
            all_metrics.insert(all_metrics.end(),
                               std::make_move_iterator(metrics.begin()),
                               std::make_move_iterator(metrics.end()));
        }

        return all_metrics;
    }

    /**
     * @brief Get statistics for a collector
     * @param name The collector name
     * @return Optional statistics
     */
    std::optional<adaptation_stats> get_stats(const std::string& name) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = collectors_.find(name);
        if (it != collectors_.end()) {
            return it->second->get_stats();
        }
        return std::nullopt;
    }

    /**
     * @brief Get all collector statistics
     * @return Map of name to statistics
     */
    std::unordered_map<std::string, adaptation_stats> get_all_stats() const {
        std::unordered_map<std::string, adaptation_stats> result;

        std::shared_lock<std::shared_mutex> lock(mutex_);
        for (const auto& [name, wrapper] : collectors_) {
            result[name] = wrapper->get_stats();
        }

        return result;
    }

    /**
     * @brief Set global adaptation strategy
     * @param strategy The strategy to apply
     */
    void set_global_strategy(adaptation_strategy strategy) {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        for (const auto& [name, wrapper] : collectors_) {
            auto config = wrapper->get_config();
            config.strategy = strategy;
            wrapper->set_config(config);
        }
    }

    /**
     * @brief Get names of all registered collectors
     */
    std::vector<std::string> get_collector_names() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        std::vector<std::string> names;
        names.reserve(collectors_.size());
        for (const auto& [name, _] : collectors_) {
            names.push_back(name);
        }
        return names;
    }

    /**
     * @brief Get count of registered collectors
     */
    std::size_t collector_count() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return collectors_.size();
    }

    /**
     * @brief Clear all registered collectors
     */
    void clear() {
        std::lock_guard<std::shared_mutex> lock(mutex_);
        collectors_.clear();
    }

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, wrapper_ptr> collectors_;
};

// ============================================================================
// Global Adaptive Monitor
// ============================================================================

/**
 * @brief Get the global adaptive monitor controller instance
 * @return Reference to the global controller
 */
inline adaptive_monitor_controller& global_adaptive_controller() {
    static adaptive_monitor_controller controller;
    return controller;
}

/**
 * @brief Get the global alert manager instance
 * @return Reference to the global alert manager
 */
inline alert_manager& global_alert_manager() {
    static alert_manager manager;
    return manager;
}

} // namespace kcenon::monitoring
