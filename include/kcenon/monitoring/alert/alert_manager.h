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
 * @file alert_manager.h
 * @brief Central coordinator for alert lifecycle management
 *
 * This file defines the alert manager which coordinates rule evaluation,
 * alert state management, and notification routing.
 */

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "alert_rule.h"
#include "alert_types.h"
#include "../core/result_types.h"
#include "../interfaces/event_bus_interface.h"

namespace kcenon::monitoring {

// Forward declarations
class alert_notifier;

/**
 * @struct alert_manager_config
 * @brief Configuration for the alert manager
 */
struct alert_manager_config {
    std::chrono::milliseconds default_evaluation_interval{15000};   ///< Default eval interval
    std::chrono::milliseconds default_repeat_interval{300000};      ///< Default repeat interval
    size_t max_alerts_per_rule{100};                                ///< Max alerts per rule
    size_t max_silences{1000};                                      ///< Max active silences
    bool enable_grouping{true};                                     ///< Enable alert grouping
    std::chrono::milliseconds group_wait{30000};                    ///< Wait time before group send
    std::chrono::milliseconds group_interval{300000};               ///< Group batch interval
    std::chrono::milliseconds resolve_timeout{300000};              ///< Auto-resolve timeout

    /**
     * @brief Validate configuration
     * @return True if valid
     */
    bool validate() const {
        return default_evaluation_interval.count() > 0 &&
               default_repeat_interval.count() > 0 &&
               max_alerts_per_rule > 0 &&
               max_silences > 0;
    }
};

/**
 * @struct alert_manager_metrics
 * @brief Metrics for alert manager operations
 */
struct alert_manager_metrics {
    std::atomic<uint64_t> rules_evaluated{0};
    std::atomic<uint64_t> alerts_created{0};
    std::atomic<uint64_t> alerts_resolved{0};
    std::atomic<uint64_t> alerts_suppressed{0};
    std::atomic<uint64_t> notifications_sent{0};
    std::atomic<uint64_t> notifications_failed{0};

    alert_manager_metrics() = default;

    alert_manager_metrics(const alert_manager_metrics& other)
        : rules_evaluated(other.rules_evaluated.load())
        , alerts_created(other.alerts_created.load())
        , alerts_resolved(other.alerts_resolved.load())
        , alerts_suppressed(other.alerts_suppressed.load())
        , notifications_sent(other.notifications_sent.load())
        , notifications_failed(other.notifications_failed.load()) {}
};

/**
 * @class alert_manager
 * @brief Central coordinator for the alert pipeline
 *
 * The alert manager is responsible for:
 * - Managing alert rules and their lifecycle
 * - Evaluating rules against incoming metrics
 * - Managing alert state transitions
 * - Routing notifications to configured notifiers
 * - Handling alert silencing and grouping
 *
 * @thread_safety This class is thread-safe. All public methods can be
 *                called from multiple threads simultaneously.
 *
 * @example
 * @code
 * alert_manager_config config;
 * config.default_evaluation_interval = std::chrono::seconds(15);
 *
 * alert_manager manager(config);
 *
 * // Add a rule
 * auto rule = std::make_shared<alert_rule>("high_cpu");
 * rule->set_metric_name("cpu_usage")
 *     .set_severity(alert_severity::critical)
 *     .set_trigger(threshold_trigger::above(80.0));
 * manager.add_rule(rule);
 *
 * // Add a notifier
 * manager.add_notifier(std::make_shared<webhook_notifier>(...));
 *
 * // Start the manager
 * manager.start();
 *
 * // Process metrics
 * manager.process_metric("cpu_usage", 95.0);
 * @endcode
 */
class alert_manager {
public:
    using metric_provider_func = std::function<std::optional<double>(const std::string&)>;

    /**
     * @brief Default constructor
     */
    alert_manager();

    /**
     * @brief Construct with configuration
     * @param config Manager configuration
     */
    explicit alert_manager(const alert_manager_config& config);

    /**
     * @brief Destructor
     */
    ~alert_manager();

    // Non-copyable, non-movable
    alert_manager(const alert_manager&) = delete;
    alert_manager& operator=(const alert_manager&) = delete;
    alert_manager(alert_manager&&) = delete;
    alert_manager& operator=(alert_manager&&) = delete;

    // ========== Lifecycle Management ==========

    /**
     * @brief Start the alert manager
     * @return Result indicating success or failure
     */
    result_void start();

    /**
     * @brief Stop the alert manager
     * @return Result indicating success or failure
     */
    result_void stop();

    /**
     * @brief Check if manager is running
     * @return True if running
     */
    bool is_running() const;

    // ========== Rule Management ==========

    /**
     * @brief Add an alert rule
     * @param rule Rule to add
     * @return Result indicating success or failure
     */
    result_void add_rule(std::shared_ptr<alert_rule> rule);

    /**
     * @brief Remove an alert rule
     * @param rule_name Name of rule to remove
     * @return Result indicating success or failure
     */
    result_void remove_rule(const std::string& rule_name);

    /**
     * @brief Get a rule by name
     * @param rule_name Name of rule
     * @return Rule if found
     */
    std::shared_ptr<alert_rule> get_rule(const std::string& rule_name) const;

    /**
     * @brief Get all rules
     * @return Vector of all rules
     */
    std::vector<std::shared_ptr<alert_rule>> get_rules() const;

    /**
     * @brief Add a rule group
     * @param group Rule group to add
     * @return Result indicating success or failure
     */
    result_void add_rule_group(std::shared_ptr<alert_rule_group> group);

    // ========== Alert Operations ==========

    /**
     * @brief Process a metric value
     * @param metric_name Name of the metric
     * @param value Metric value
     * @return Result indicating success or failure
     */
    result_void process_metric(const std::string& metric_name, double value);

    /**
     * @brief Process a batch of metrics
     * @param metrics Map of metric names to values
     * @return Result indicating success or failure
     */
    result_void process_metrics(const std::unordered_map<std::string, double>& metrics);

    /**
     * @brief Get all active alerts
     * @return Vector of active alerts
     */
    std::vector<alert> get_active_alerts() const;

    /**
     * @brief Get alert by fingerprint
     * @param fingerprint Alert fingerprint
     * @return Alert if found
     */
    std::optional<alert> get_alert(const std::string& fingerprint) const;

    /**
     * @brief Resolve an alert manually
     * @param fingerprint Alert fingerprint
     * @return Result indicating success or failure
     */
    result_void resolve_alert(const std::string& fingerprint);

    // ========== Silence Management ==========

    /**
     * @brief Create a silence
     * @param silence Silence configuration
     * @return Result containing silence ID
     */
    result<uint64_t> create_silence(const alert_silence& silence);

    /**
     * @brief Delete a silence
     * @param silence_id Silence ID
     * @return Result indicating success or failure
     */
    result_void delete_silence(uint64_t silence_id);

    /**
     * @brief Get all active silences
     * @return Vector of active silences
     */
    std::vector<alert_silence> get_silences() const;

    /**
     * @brief Check if an alert is silenced
     * @param a Alert to check
     * @return True if silenced
     */
    bool is_silenced(const alert& a) const;

    // ========== Notifier Management ==========

    /**
     * @brief Add a notifier
     * @param notifier Notifier to add
     * @return Result indicating success or failure
     */
    result_void add_notifier(std::shared_ptr<alert_notifier> notifier);

    /**
     * @brief Remove a notifier
     * @param notifier_name Name of notifier
     * @return Result indicating success or failure
     */
    result_void remove_notifier(const std::string& notifier_name);

    /**
     * @brief Get all notifiers
     * @return Vector of notifiers
     */
    std::vector<std::shared_ptr<alert_notifier>> get_notifiers() const;

    // ========== Metric Provider ==========

    /**
     * @brief Set the metric provider function
     * @param provider Function that returns metric values by name
     */
    void set_metric_provider(metric_provider_func provider);

    // ========== Event Bus Integration ==========

    /**
     * @brief Set event bus for publishing alert events
     * @param event_bus Event bus instance
     */
    void set_event_bus(std::shared_ptr<interface_event_bus> event_bus);

    // ========== Metrics ==========

    /**
     * @brief Get manager metrics
     * @return Current metrics
     */
    alert_manager_metrics get_metrics() const;

    /**
     * @brief Get configuration
     * @return Current configuration
     */
    const alert_manager_config& config() const;

private:
    /**
     * @brief Main evaluation loop
     */
    void evaluation_loop();

    /**
     * @brief Evaluate a single rule
     * @param rule Rule to evaluate
     * @param value Metric value
     */
    void evaluate_rule(const std::shared_ptr<alert_rule>& rule, double value);

    /**
     * @brief Update alert state
     * @param fingerprint Alert fingerprint
     * @param condition_met Whether condition is met
     * @param value Current metric value
     */
    void update_alert_state(const std::string& fingerprint,
                           bool condition_met,
                           double value,
                           const std::shared_ptr<alert_rule>& rule);

    /**
     * @brief Send notifications for an alert
     * @param a Alert to notify about
     */
    void send_notifications(const alert& a);

    /**
     * @brief Clean up expired silences
     */
    void cleanup_silences();

    /**
     * @brief Clean up resolved alerts
     */
    void cleanup_resolved_alerts();

    // Configuration
    alert_manager_config config_;

    // Rules
    mutable std::mutex rules_mutex_;
    std::unordered_map<std::string, std::shared_ptr<alert_rule>> rules_;
    std::vector<std::shared_ptr<alert_rule_group>> rule_groups_;

    // Alerts
    mutable std::mutex alerts_mutex_;
    std::unordered_map<std::string, alert> alerts_;

    // Silences
    mutable std::mutex silences_mutex_;
    std::unordered_map<uint64_t, alert_silence> silences_;

    // Notifiers
    mutable std::mutex notifiers_mutex_;
    std::vector<std::shared_ptr<alert_notifier>> notifiers_;

    // Metric provider
    std::mutex provider_mutex_;
    metric_provider_func metric_provider_;

    // Event bus
    std::shared_ptr<interface_event_bus> event_bus_;

    // Metrics
    mutable alert_manager_metrics metrics_;

    // Evaluation thread
    std::atomic<bool> running_{false};
    std::thread evaluation_thread_;
    std::condition_variable cv_;
    std::mutex cv_mutex_;

    // Last notification times
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> last_notification_times_;
};

/**
 * @class alert_notifier
 * @brief Base class for alert notification handlers
 *
 * Notifiers receive alerts and send them to external systems
 * (webhooks, logging, email, etc.).
 */
class alert_notifier {
public:
    virtual ~alert_notifier() = default;

    /**
     * @brief Get notifier name
     * @return Notifier name
     */
    virtual std::string name() const = 0;

    /**
     * @brief Send a notification for an alert
     * @param a Alert to notify about
     * @return Result indicating success or failure
     */
    virtual result_void notify(const alert& a) = 0;

    /**
     * @brief Send a notification for an alert group
     * @param group Alert group to notify about
     * @return Result indicating success or failure
     */
    virtual result_void notify_group(const alert_group& group) = 0;

    /**
     * @brief Check if notifier is ready
     * @return True if ready to send notifications
     */
    virtual bool is_ready() const = 0;
};

/**
 * @class log_notifier
 * @brief Simple notifier that logs alerts
 *
 * Writes alert information to the logging system.
 */
class log_notifier : public alert_notifier {
public:
    /**
     * @brief Construct log notifier
     * @param notifier_name Name for this notifier
     */
    explicit log_notifier(std::string notifier_name = "log_notifier")
        : name_(std::move(notifier_name)) {}

    std::string name() const override { return name_; }

    result_void notify(const alert& a) override;

    result_void notify_group(const alert_group& group) override;

    bool is_ready() const override { return true; }

private:
    std::string name_;
};

/**
 * @class callback_notifier
 * @brief Notifier that invokes a callback function
 *
 * Allows custom notification handling via user-defined callbacks.
 */
class callback_notifier : public alert_notifier {
public:
    using callback_func = std::function<void(const alert&)>;
    using group_callback_func = std::function<void(const alert_group&)>;

    /**
     * @brief Construct callback notifier
     * @param notifier_name Name for this notifier
     * @param callback Callback function for single alerts
     * @param group_callback Callback function for alert groups (optional)
     */
    callback_notifier(std::string notifier_name,
                      callback_func callback,
                      group_callback_func group_callback = nullptr)
        : name_(std::move(notifier_name))
        , callback_(std::move(callback))
        , group_callback_(std::move(group_callback)) {}

    std::string name() const override { return name_; }

    result_void notify(const alert& a) override {
        if (callback_) {
            callback_(a);
            return make_void_success();
        }
        return make_void_error(monitoring_error_code::operation_failed,
                              "No callback configured");
    }

    result_void notify_group(const alert_group& group) override {
        if (group_callback_) {
            group_callback_(group);
            return make_void_success();
        }
        // Fall back to individual notifications
        for (const auto& a : group.alerts) {
            auto result = notify(a);
            if (!result.is_ok()) {
                return result;
            }
        }
        return make_void_success();
    }

    bool is_ready() const override { return callback_ != nullptr; }

private:
    std::string name_;
    callback_func callback_;
    group_callback_func group_callback_;
};

} // namespace kcenon::monitoring
