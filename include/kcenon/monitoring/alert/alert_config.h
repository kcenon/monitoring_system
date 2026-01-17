// BSD 3-Clause License
//
// Copyright (c) 2021-2025, kcenon
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

/**
 * @file alert_config.h
 * @brief Alert configuration parsing and templating
 *
 * This file provides configuration parsing for alert rules and
 * message templating support with variable substitution.
 */

#include <chrono>
#include <functional>
#include <iomanip>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "alert_rule.h"
#include "alert_triggers.h"
#include "alert_types.h"
#include "../core/result_types.h"

namespace kcenon::monitoring {

/**
 * @class alert_template
 * @brief Template engine for alert messages
 *
 * Supports variable substitution in alert summaries and descriptions
 * using ${variable} syntax. Variables can include alert properties,
 * labels, and annotations.
 *
 * Built-in variables:
 * - ${name} - Alert name
 * - ${state} - Alert state (pending, firing, resolved)
 * - ${severity} - Alert severity
 * - ${value} - Current metric value
 * - ${threshold} - Trigger threshold (if applicable)
 * - ${labels.X} - Label value for key X
 * - ${annotations.X} - Annotation value for key X
 *
 * @example
 * @code
 * alert_template tmpl("CPU usage is ${value}% (threshold: ${threshold}%)");
 * tmpl.set("threshold", "80");
 *
 * std::string message = tmpl.render(alert);
 * // Output: "CPU usage is 95% (threshold: 80%)"
 * @endcode
 */
class alert_template {
public:
    /**
     * @brief Construct with template string
     */
    explicit alert_template(std::string template_str)
        : template_str_(std::move(template_str)) {}

    /**
     * @brief Set a custom variable value
     */
    void set(const std::string& key, const std::string& value) {
        custom_vars_[key] = value;
    }

    /**
     * @brief Render template with alert data
     * @param a Alert providing context
     * @return Rendered string
     */
    std::string render(const alert& a) const {
        std::string result = template_str_;

        // Build variable map
        std::unordered_map<std::string, std::string> vars;

        // Alert properties
        vars["name"] = a.name;
        vars["state"] = alert_state_to_string(a.state);
        vars["severity"] = alert_severity_to_string(a.severity);
        vars["value"] = format_value(a.value);
        vars["fingerprint"] = a.fingerprint();
        vars["rule_name"] = a.rule_name;
        vars["group_key"] = a.group_key;

        // Labels
        for (const auto& [key, value] : a.labels.labels) {
            vars["labels." + key] = value;
        }

        // Annotations
        vars["annotations.summary"] = a.annotations.summary;
        vars["annotations.description"] = a.annotations.description;
        if (a.annotations.runbook_url) {
            vars["annotations.runbook_url"] = *a.annotations.runbook_url;
        }
        for (const auto& [key, value] : a.annotations.custom) {
            vars["annotations." + key] = value;
        }

        // Custom variables
        for (const auto& [key, value] : custom_vars_) {
            vars[key] = value;
        }

        // Substitute variables
        result = substitute_variables(result, vars);

        return result;
    }

    /**
     * @brief Get template string
     */
    const std::string& template_string() const { return template_str_; }

    /**
     * @brief Validate template syntax
     * @return Result with validation errors if any
     */
    common::VoidResult validate() const {
        // Check for unclosed variable references
        std::regex pattern(R"(\$\{[^}]*$)");
        if (std::regex_search(template_str_, pattern)) {
            return common::VoidResult::err(error_info(monitoring_error_code::validation_failed, "Unclosed variable reference in template").to_common_error());
        }
        return common::ok();
    }

private:
    static std::string format_value(double value) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << value;
        return oss.str();
    }

    static std::string substitute_variables(
            const std::string& input,
            const std::unordered_map<std::string, std::string>& vars) {
        std::string result = input;
        std::regex pattern(R"(\$\{([^}]+)\})");

        std::string::const_iterator start = result.cbegin();
        std::smatch match;
        std::string output;

        while (std::regex_search(start, result.cend(), match, pattern)) {
            output += std::string(start, match[0].first);

            std::string var_name = match[1].str();
            auto it = vars.find(var_name);
            if (it != vars.end()) {
                output += it->second;
            } else {
                output += match[0].str();  // Keep original if not found
            }

            start = match[0].second;
        }
        output += std::string(start, result.cend());

        return output;
    }

    std::string template_str_;
    std::unordered_map<std::string, std::string> custom_vars_;
};

/**
 * @struct rule_definition
 * @brief Structured definition for alert rule configuration
 *
 * This structure can be serialized to/from YAML or JSON for
 * configuration file support.
 */
struct rule_definition {
    std::string name;
    std::string group;
    std::string metric_name;
    std::string severity;           // "info", "warning", "critical", "emergency"
    bool enabled = true;

    // Trigger configuration
    struct trigger_config {
        std::string type;           // "threshold", "rate", "anomaly", "absent"
        std::string operator_str;   // ">", ">=", "<", "<=", "==", "!="
        double threshold = 0.0;
        double rate_threshold = 0.0;
        double sensitivity = 3.0;
        int window_seconds = 60;
        int absent_seconds = 300;
    } trigger;

    // Timing configuration
    int evaluation_interval_seconds = 15;
    int for_duration_seconds = 0;
    int repeat_interval_seconds = 300;

    // Labels and annotations
    std::unordered_map<std::string, std::string> labels;
    std::string summary;
    std::string description;
    std::string runbook_url;
};

/**
 * @class rule_builder
 * @brief Builds alert_rule from rule_definition
 *
 * Provides validation and construction of alert rules from
 * configuration definitions.
 */
class rule_builder {
public:
    /**
     * @brief Build alert_rule from definition
     * @param def Rule definition
     * @return Result containing built rule or error
     */
    static common::Result<std::shared_ptr<alert_rule>> build(const rule_definition& def) {
        // Validate required fields
        if (def.name.empty()) {
            return make_error<std::shared_ptr<alert_rule>>(
                monitoring_error_code::invalid_argument,
                "Rule name is required");
        }
        if (def.metric_name.empty()) {
            return make_error<std::shared_ptr<alert_rule>>(
                monitoring_error_code::invalid_argument,
                "Metric name is required");
        }

        auto rule = std::make_shared<alert_rule>(def.name);

        // Set group
        if (!def.group.empty()) {
            rule->set_group(def.group);
        }

        // Set metric name
        rule->set_metric_name(def.metric_name);

        // Set severity
        auto severity = parse_severity(def.severity);
        if (!severity.is_ok()) {
            return make_error<std::shared_ptr<alert_rule>>(
                monitoring_error_code::invalid_argument,
                severity.error().message);
        }
        rule->set_severity(severity.value());

        // Set enabled
        rule->set_enabled(def.enabled);

        // Build trigger
        auto trigger = build_trigger(def.trigger);
        if (!trigger.is_ok()) {
            return make_error<std::shared_ptr<alert_rule>>(
                monitoring_error_code::invalid_argument,
                trigger.error().message);
        }
        rule->set_trigger(trigger.value());

        // Set timing
        rule->set_evaluation_interval(
            std::chrono::seconds(def.evaluation_interval_seconds));
        rule->set_for_duration(
            std::chrono::seconds(def.for_duration_seconds));
        rule->set_repeat_interval(
            std::chrono::seconds(def.repeat_interval_seconds));

        // Set labels
        for (const auto& [key, value] : def.labels) {
            rule->add_label(key, value);
        }

        // Set annotations
        if (!def.summary.empty()) {
            rule->set_summary(def.summary);
        }
        if (!def.description.empty()) {
            rule->set_description(def.description);
        }
        if (!def.runbook_url.empty()) {
            rule->set_runbook_url(def.runbook_url);
        }

        return common::ok(std::move(rule));
    }

private:
    static common::Result<alert_severity> parse_severity(const std::string& str) {
        if (str.empty() || str == "warning") {
            return common::ok(alert_severity::warning);
        }
        if (str == "info") {
            return common::ok(alert_severity::info);
        }
        if (str == "critical") {
            return common::ok(alert_severity::critical);
        }
        if (str == "emergency") {
            return common::ok(alert_severity::emergency);
        }
        return make_error<alert_severity>(
            monitoring_error_code::invalid_argument,
            "Unknown severity: " + str);
    }

    static common::Result<std::shared_ptr<alert_trigger>> build_trigger(
            const rule_definition::trigger_config& cfg) {
        if (cfg.type.empty() || cfg.type == "threshold") {
            auto op = parse_operator(cfg.operator_str);
            if (!op.is_ok()) {
                return make_error<std::shared_ptr<alert_trigger>>(
                    monitoring_error_code::invalid_argument,
                    op.error().message);
            }
            return make_success(std::shared_ptr<alert_trigger>(
                std::make_shared<threshold_trigger>(cfg.threshold, op.value())));
        }

        if (cfg.type == "rate") {
            return make_success(std::shared_ptr<alert_trigger>(
                std::make_shared<rate_of_change_trigger>(
                    cfg.rate_threshold,
                    std::chrono::seconds(cfg.window_seconds))));
        }

        if (cfg.type == "anomaly") {
            return make_success(std::shared_ptr<alert_trigger>(
                std::make_shared<anomaly_trigger>(
                    cfg.sensitivity,
                    static_cast<size_t>(cfg.window_seconds))));
        }

        if (cfg.type == "absent") {
            return make_success(std::shared_ptr<alert_trigger>(
                std::make_shared<absent_trigger>(
                    std::chrono::seconds(cfg.absent_seconds))));
        }

        return make_error<std::shared_ptr<alert_trigger>>(
            monitoring_error_code::invalid_argument,
            "Unknown trigger type: " + cfg.type);
    }

    static common::Result<comparison_operator> parse_operator(const std::string& str) {
        if (str.empty() || str == ">") {
            return common::ok(comparison_operator::greater_than);
        }
        if (str == ">=") {
            return common::ok(comparison_operator::greater_or_equal);
        }
        if (str == "<") {
            return common::ok(comparison_operator::less_than);
        }
        if (str == "<=") {
            return common::ok(comparison_operator::less_or_equal);
        }
        if (str == "==" || str == "=") {
            return common::ok(comparison_operator::equal);
        }
        if (str == "!=" || str == "<>") {
            return common::ok(comparison_operator::not_equal);
        }
        return make_error<comparison_operator>(
            monitoring_error_code::invalid_argument,
            "Unknown operator: " + str);
    }
};

/**
 * @class rule_registry
 * @brief Dynamic registry for alert rules with hot-reload support
 *
 * Manages a collection of alert rules and supports runtime updates
 * without service interruption.
 */
class rule_registry {
public:
    using rule_change_callback = std::function<void(
        const std::string& rule_name,
        const std::shared_ptr<alert_rule>& rule,
        bool is_removal)>;

    /**
     * @brief Register a rule
     * @param rule Rule to register
     * @return Result indicating success or failure
     */
    common::VoidResult register_rule(std::shared_ptr<alert_rule> rule) {
        if (!rule) {
            return common::VoidResult::err(error_info(monitoring_error_code::invalid_argument, "Rule cannot be null").to_common_error());
        }

        std::lock_guard<std::mutex> lock(mutex_);

        std::string name = rule->name();
        rules_[name] = rule;

        // Notify listeners
        for (const auto& callback : change_callbacks_) {
            callback(name, rule, false);
        }

        return common::ok();
    }

    /**
     * @brief Unregister a rule
     * @param name Rule name
     * @return Result indicating success or failure
     */
    common::VoidResult unregister_rule(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = rules_.find(name);
        if (it == rules_.end()) {
            return make_void_error(monitoring_error_code::not_found,
                                   "Rule not found: " + name);
        }

        auto rule = it->second;
        rules_.erase(it);

        // Notify listeners
        for (const auto& callback : change_callbacks_) {
            callback(name, rule, true);
        }

        return common::ok();
    }

    /**
     * @brief Get a rule by name
     */
    std::shared_ptr<alert_rule> get_rule(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = rules_.find(name);
        return it != rules_.end() ? it->second : nullptr;
    }

    /**
     * @brief Get all registered rules
     */
    std::vector<std::shared_ptr<alert_rule>> get_all_rules() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::shared_ptr<alert_rule>> result;
        result.reserve(rules_.size());
        for (const auto& [name, rule] : rules_) {
            result.push_back(rule);
        }
        return result;
    }

    /**
     * @brief Get rules in a group
     */
    std::vector<std::shared_ptr<alert_rule>> get_rules_by_group(
            const std::string& group) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::shared_ptr<alert_rule>> result;
        for (const auto& [name, rule] : rules_) {
            if (rule->group() == group) {
                result.push_back(rule);
            }
        }
        return result;
    }

    /**
     * @brief Get rule count
     */
    size_t rule_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return rules_.size();
    }

    /**
     * @brief Register callback for rule changes
     */
    void on_rule_change(rule_change_callback callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        change_callbacks_.push_back(std::move(callback));
    }

    /**
     * @brief Load rules from definitions
     * @param definitions Vector of rule definitions
     * @return Result with count of successfully loaded rules
     */
    common::Result<size_t> load_definitions(const std::vector<rule_definition>& definitions) {
        size_t loaded = 0;
        std::vector<std::string> errors;

        for (const auto& def : definitions) {
            auto rule_result = rule_builder::build(def);
            if (rule_result.is_ok()) {
                auto reg_result = register_rule(rule_result.value());
                if (reg_result.is_ok()) {
                    ++loaded;
                } else {
                    errors.push_back(def.name + ": " + reg_result.error().message);
                }
            } else {
                errors.push_back(def.name + ": " + rule_result.error().message);
            }
        }

        if (!errors.empty() && loaded == 0) {
            return make_error<size_t>(
                monitoring_error_code::configuration_parse_error,
                "Failed to load any rules: " + errors.front());
        }

        return common::ok(loaded);
    }

    /**
     * @brief Clear all rules
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);

        for (const auto& [name, rule] : rules_) {
            for (const auto& callback : change_callbacks_) {
                callback(name, rule, true);
            }
        }

        rules_.clear();
    }

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<alert_rule>> rules_;
    std::vector<rule_change_callback> change_callbacks_;
};

/**
 * @brief Configuration schema documentation for YAML/JSON format
 *
 * Example YAML configuration:
 * @code
 * rules:
 *   - name: high_cpu
 *     group: system
 *     metric_name: cpu_usage
 *     severity: critical
 *     trigger:
 *       type: threshold
 *       operator: ">"
 *       threshold: 80
 *     evaluation_interval_seconds: 15
 *     for_duration_seconds: 60
 *     labels:
 *       team: infrastructure
 *       environment: production
 *     summary: "High CPU usage detected"
 *     description: "CPU usage is ${value}% on ${labels.host}"
 *     runbook_url: "https://runbooks.example.com/high-cpu"
 *
 *   - name: memory_anomaly
 *     group: system
 *     metric_name: memory_usage
 *     severity: warning
 *     trigger:
 *       type: anomaly
 *       sensitivity: 3.0
 *       window_seconds: 300
 *     summary: "Unusual memory usage pattern"
 *
 *   - name: service_down
 *     group: availability
 *     metric_name: health_check
 *     severity: emergency
 *     trigger:
 *       type: absent
 *       absent_seconds: 300
 *     summary: "Service health check missing"
 * @endcode
 */

} // namespace kcenon::monitoring
