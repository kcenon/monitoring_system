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
 * @file alert_rule.h
 * @brief Alert rule configuration and evaluation
 *
 * This file defines alert rules that specify conditions for triggering alerts,
 * including threshold configurations, evaluation intervals, and notification targets.
 */

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "alert_types.h"
#include "../core/result_types.h"

namespace kcenon::monitoring {

// Forward declarations
class alert_trigger;

/**
 * @struct alert_rule_config
 * @brief Configuration for an alert rule
 *
 * Defines the timing and behavior parameters for alert evaluation.
 */
struct alert_rule_config {
    std::chrono::milliseconds evaluation_interval{15000};       ///< How often to evaluate
    std::chrono::milliseconds for_duration{0};                  ///< Duration before firing
    std::chrono::milliseconds repeat_interval{300000};          ///< Notification repeat interval
    bool keep_firing_for{false};                                ///< Keep firing after resolve
    std::chrono::milliseconds keep_firing_duration{300000};     ///< Duration to keep firing

    /**
     * @brief Validate configuration
     * @return True if configuration is valid
     */
    bool validate() const {
        if (evaluation_interval.count() <= 0) {
            return false;
        }
        if (repeat_interval.count() <= 0) {
            return false;
        }
        return true;
    }
};

/**
 * @class alert_rule
 * @brief Defines conditions and behavior for alert triggering
 *
 * An alert rule encapsulates the logic for when alerts should be triggered,
 * how they should be labeled and annotated, and where notifications should
 * be routed.
 *
 * @thread_safety This class is thread-safe for read operations after
 *                construction. Trigger evaluation may require external
 *                synchronization depending on trigger implementation.
 *
 * @example
 * @code
 * alert_rule rule("high_cpu");
 * rule.set_severity(alert_severity::critical)
 *     .set_summary("CPU usage is high")
 *     .set_description("CPU usage exceeded ${threshold}%")
 *     .add_label("team", "infrastructure")
 *     .set_for_duration(std::chrono::minutes(5));
 *
 * rule.set_trigger(threshold_trigger::above(80.0));
 * @endcode
 */
class alert_rule {
public:
    /**
     * @brief Construct an alert rule with a name
     * @param name Unique rule name
     */
    explicit alert_rule(std::string name)
        : name_(std::move(name))
        , enabled_(true) {}

    /**
     * @brief Get rule name
     * @return Rule name
     */
    const std::string& name() const { return name_; }

    /**
     * @brief Get rule group
     * @return Rule group name
     */
    const std::string& group() const { return group_; }

    /**
     * @brief Set rule group
     * @param group_name Group name
     * @return Reference to this for chaining
     */
    alert_rule& set_group(std::string group_name) {
        group_ = std::move(group_name);
        return *this;
    }

    /**
     * @brief Get alert severity
     * @return Severity level
     */
    alert_severity severity() const { return severity_; }

    /**
     * @brief Set alert severity
     * @param sev Severity level
     * @return Reference to this for chaining
     */
    alert_rule& set_severity(alert_severity sev) {
        severity_ = sev;
        return *this;
    }

    /**
     * @brief Get labels
     * @return Alert labels
     */
    const alert_labels& labels() const { return labels_; }

    /**
     * @brief Add a label
     * @param key Label key
     * @param value Label value
     * @return Reference to this for chaining
     */
    alert_rule& add_label(const std::string& key, const std::string& value) {
        labels_.set(key, value);
        return *this;
    }

    /**
     * @brief Get annotations
     * @return Alert annotations
     */
    const alert_annotations& annotations() const { return annotations_; }

    /**
     * @brief Set alert summary
     * @param summary Summary text
     * @return Reference to this for chaining
     */
    alert_rule& set_summary(std::string summary) {
        annotations_.summary = std::move(summary);
        return *this;
    }

    /**
     * @brief Set alert description
     * @param description Description text
     * @return Reference to this for chaining
     */
    alert_rule& set_description(std::string description) {
        annotations_.description = std::move(description);
        return *this;
    }

    /**
     * @brief Set runbook URL
     * @param url Runbook URL
     * @return Reference to this for chaining
     */
    alert_rule& set_runbook_url(std::string url) {
        annotations_.runbook_url = std::move(url);
        return *this;
    }

    /**
     * @brief Get configuration
     * @return Rule configuration
     */
    const alert_rule_config& config() const { return config_; }

    /**
     * @brief Set evaluation interval
     * @param interval Evaluation interval
     * @return Reference to this for chaining
     */
    alert_rule& set_evaluation_interval(std::chrono::milliseconds interval) {
        config_.evaluation_interval = interval;
        return *this;
    }

    /**
     * @brief Set for duration (pending time before firing)
     * @param duration Duration before firing
     * @return Reference to this for chaining
     */
    alert_rule& set_for_duration(std::chrono::milliseconds duration) {
        config_.for_duration = duration;
        return *this;
    }

    /**
     * @brief Set notification repeat interval
     * @param interval Repeat interval
     * @return Reference to this for chaining
     */
    alert_rule& set_repeat_interval(std::chrono::milliseconds interval) {
        config_.repeat_interval = interval;
        return *this;
    }

    /**
     * @brief Check if rule is enabled
     * @return True if enabled
     */
    bool is_enabled() const { return enabled_; }

    /**
     * @brief Enable or disable rule
     * @param enabled Enable state
     * @return Reference to this for chaining
     */
    alert_rule& set_enabled(bool enabled) {
        enabled_ = enabled;
        return *this;
    }

    /**
     * @brief Set the trigger for this rule
     * @param trigger Trigger implementation
     * @return Reference to this for chaining
     */
    alert_rule& set_trigger(std::shared_ptr<alert_trigger> trigger) {
        trigger_ = std::move(trigger);
        return *this;
    }

    /**
     * @brief Get the trigger
     * @return Trigger implementation
     */
    std::shared_ptr<alert_trigger> trigger() const { return trigger_; }

    /**
     * @brief Create an alert from this rule
     * @param value Current metric value
     * @return Created alert
     */
    alert create_alert(double value) const {
        alert a(name_, labels_);
        a.annotations = annotations_;
        a.severity = severity_;
        a.value = value;
        a.rule_name = name_;
        a.group_key = group_.empty() ? name_ : group_;
        return a;
    }

    /**
     * @brief Validate rule configuration
     * @return Result indicating if configuration is valid
     */
    result_void validate() const {
        if (name_.empty()) {
            return make_void_error(monitoring_error_code::invalid_argument,
                                   "Rule name cannot be empty");
        }
        if (!config_.validate()) {
            return make_void_error(monitoring_error_code::invalid_configuration,
                                   "Rule configuration is invalid");
        }
        if (!trigger_) {
            return make_void_error(monitoring_error_code::invalid_argument,
                                   "Rule must have a trigger");
        }
        return make_void_success();
    }

    /**
     * @brief Get metric name to monitor
     * @return Metric name
     */
    const std::string& metric_name() const { return metric_name_; }

    /**
     * @brief Set metric name to monitor
     * @param name Metric name
     * @return Reference to this for chaining
     */
    alert_rule& set_metric_name(std::string name) {
        metric_name_ = std::move(name);
        return *this;
    }

private:
    std::string name_;
    std::string group_;
    std::string metric_name_;
    alert_severity severity_ = alert_severity::warning;
    alert_labels labels_;
    alert_annotations annotations_;
    alert_rule_config config_;
    bool enabled_;
    std::shared_ptr<alert_trigger> trigger_;
};

/**
 * @class alert_trigger
 * @brief Base class for alert trigger conditions
 *
 * Triggers define the conditions that cause an alert to fire.
 * Different trigger types (threshold, rate of change, anomaly, etc.)
 * inherit from this class.
 */
class alert_trigger {
public:
    virtual ~alert_trigger() = default;

    /**
     * @brief Evaluate the trigger condition
     * @param value Current metric value
     * @return True if trigger condition is met
     */
    virtual bool evaluate(double value) const = 0;

    /**
     * @brief Get trigger type name
     * @return Type name string
     */
    virtual std::string type_name() const = 0;

    /**
     * @brief Get human-readable description
     * @return Description string
     */
    virtual std::string description() const = 0;
};

/**
 * @class alert_rule_group
 * @brief A group of related alert rules
 *
 * Rule groups allow organizing rules and applying common settings
 * to multiple rules.
 */
class alert_rule_group {
public:
    /**
     * @brief Construct a rule group
     * @param name Group name
     */
    explicit alert_rule_group(std::string name)
        : name_(std::move(name)) {}

    /**
     * @brief Get group name
     * @return Group name
     */
    const std::string& name() const { return name_; }

    /**
     * @brief Add a rule to the group
     * @param rule Rule to add
     */
    void add_rule(std::shared_ptr<alert_rule> rule) {
        if (rule) {
            rule->set_group(name_);
            rules_.push_back(std::move(rule));
        }
    }

    /**
     * @brief Get all rules in the group
     * @return Vector of rules
     */
    const std::vector<std::shared_ptr<alert_rule>>& rules() const {
        return rules_;
    }

    /**
     * @brief Get number of rules
     * @return Rule count
     */
    size_t size() const { return rules_.size(); }

    /**
     * @brief Check if group is empty
     * @return True if no rules
     */
    bool empty() const { return rules_.empty(); }

    /**
     * @brief Set common evaluation interval for all rules
     * @param interval Evaluation interval
     */
    void set_common_interval(std::chrono::milliseconds interval) {
        common_interval_ = interval;
        for (auto& rule : rules_) {
            rule->set_evaluation_interval(interval);
        }
    }

    /**
     * @brief Get common evaluation interval
     * @return Evaluation interval if set
     */
    std::optional<std::chrono::milliseconds> common_interval() const {
        return common_interval_;
    }

private:
    std::string name_;
    std::vector<std::shared_ptr<alert_rule>> rules_;
    std::optional<std::chrono::milliseconds> common_interval_;
};

} // namespace kcenon::monitoring
