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

#include "kcenon/monitoring/alert/alert_manager.h"

#include <iostream>
#include <sstream>

namespace kcenon::monitoring {

// ========== alert_manager Implementation ==========

alert_manager::alert_manager()
    : config_() {}

alert_manager::alert_manager(const alert_manager_config& config)
    : config_(config) {}

alert_manager::~alert_manager() {
    if (running_.load()) {
        stop();
    }
}

result_void alert_manager::start() {
    if (running_.load()) {
        return make_void_error(monitoring_error_code::already_started,
                              "Alert manager is already running");
    }

    if (!config_.validate()) {
        return make_void_error(monitoring_error_code::invalid_configuration,
                              "Invalid alert manager configuration");
    }

    running_.store(true);
    evaluation_thread_ = std::thread(&alert_manager::evaluation_loop, this);

    return make_void_success();
}

result_void alert_manager::stop() {
    if (!running_.load()) {
        return make_void_success();
    }

    running_.store(false);

    {
        std::lock_guard<std::mutex> lock(cv_mutex_);
        cv_.notify_all();
    }

    if (evaluation_thread_.joinable()) {
        evaluation_thread_.join();
    }

    return make_void_success();
}

bool alert_manager::is_running() const {
    return running_.load();
}

result_void alert_manager::add_rule(std::shared_ptr<alert_rule> rule) {
    if (!rule) {
        return make_void_error(monitoring_error_code::invalid_argument,
                              "Rule cannot be null");
    }

    auto validation = rule->validate();
    if (!validation.is_ok()) {
        return validation;
    }

    std::lock_guard<std::mutex> lock(rules_mutex_);

    if (rules_.find(rule->name()) != rules_.end()) {
        return make_void_error(monitoring_error_code::already_exists,
                              "Rule with name '" + rule->name() + "' already exists");
    }

    rules_[rule->name()] = std::move(rule);
    return make_void_success();
}

result_void alert_manager::remove_rule(const std::string& rule_name) {
    std::lock_guard<std::mutex> lock(rules_mutex_);

    auto it = rules_.find(rule_name);
    if (it == rules_.end()) {
        return make_void_error(monitoring_error_code::not_found,
                              "Rule '" + rule_name + "' not found");
    }

    rules_.erase(it);
    return make_void_success();
}

std::shared_ptr<alert_rule> alert_manager::get_rule(const std::string& rule_name) const {
    std::lock_guard<std::mutex> lock(rules_mutex_);

    auto it = rules_.find(rule_name);
    if (it != rules_.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<std::shared_ptr<alert_rule>> alert_manager::get_rules() const {
    std::lock_guard<std::mutex> lock(rules_mutex_);

    std::vector<std::shared_ptr<alert_rule>> result;
    result.reserve(rules_.size());
    for (const auto& [name, rule] : rules_) {
        result.push_back(rule);
    }
    return result;
}

result_void alert_manager::add_rule_group(std::shared_ptr<alert_rule_group> group) {
    if (!group) {
        return make_void_error(monitoring_error_code::invalid_argument,
                              "Rule group cannot be null");
    }

    std::lock_guard<std::mutex> lock(rules_mutex_);

    for (const auto& rule : group->rules()) {
        if (rules_.find(rule->name()) != rules_.end()) {
            return make_void_error(monitoring_error_code::already_exists,
                                  "Rule with name '" + rule->name() + "' already exists");
        }
    }

    // Add all rules from the group
    for (const auto& rule : group->rules()) {
        rules_[rule->name()] = rule;
    }

    rule_groups_.push_back(std::move(group));
    return make_void_success();
}

result_void alert_manager::process_metric(const std::string& metric_name, double value) {
    std::vector<std::shared_ptr<alert_rule>> matching_rules;

    {
        std::lock_guard<std::mutex> lock(rules_mutex_);
        for (const auto& [name, rule] : rules_) {
            if (rule->is_enabled() && rule->metric_name() == metric_name) {
                matching_rules.push_back(rule);
            }
        }
    }

    for (const auto& rule : matching_rules) {
        evaluate_rule(rule, value);
        metrics_.rules_evaluated++;
    }

    return make_void_success();
}

result_void alert_manager::process_metrics(const std::unordered_map<std::string, double>& metrics) {
    for (const auto& [metric_name, value] : metrics) {
        auto result = process_metric(metric_name, value);
        if (!result.is_ok()) {
            return result;
        }
    }
    return make_void_success();
}

std::vector<alert> alert_manager::get_active_alerts() const {
    std::lock_guard<std::mutex> lock(alerts_mutex_);

    std::vector<alert> result;
    for (const auto& [fingerprint, a] : alerts_) {
        if (a.is_active()) {
            result.push_back(a);
        }
    }
    return result;
}

std::optional<alert> alert_manager::get_alert(const std::string& fingerprint) const {
    std::lock_guard<std::mutex> lock(alerts_mutex_);

    auto it = alerts_.find(fingerprint);
    if (it != alerts_.end()) {
        return it->second;
    }
    return std::nullopt;
}

result_void alert_manager::resolve_alert(const std::string& fingerprint) {
    std::lock_guard<std::mutex> lock(alerts_mutex_);

    auto it = alerts_.find(fingerprint);
    if (it == alerts_.end()) {
        return make_void_error(monitoring_error_code::not_found,
                              "Alert not found: " + fingerprint);
    }

    if (it->second.transition_to(alert_state::resolved)) {
        metrics_.alerts_resolved++;
        send_notifications(it->second);
    }

    return make_void_success();
}

result<uint64_t> alert_manager::create_silence(const alert_silence& silence) {
    std::lock_guard<std::mutex> lock(silences_mutex_);

    if (silences_.size() >= config_.max_silences) {
        return make_error<uint64_t>(monitoring_error_code::resource_exhausted,
                                   "Maximum number of silences reached");
    }

    alert_silence new_silence = silence;
    silences_[new_silence.id] = new_silence;
    return make_success(new_silence.id);
}

result_void alert_manager::delete_silence(uint64_t silence_id) {
    std::lock_guard<std::mutex> lock(silences_mutex_);

    auto it = silences_.find(silence_id);
    if (it == silences_.end()) {
        return make_void_error(monitoring_error_code::not_found,
                              "Silence not found");
    }

    silences_.erase(it);
    return make_void_success();
}

std::vector<alert_silence> alert_manager::get_silences() const {
    std::lock_guard<std::mutex> lock(silences_mutex_);

    std::vector<alert_silence> result;
    result.reserve(silences_.size());
    for (const auto& [id, silence] : silences_) {
        if (silence.is_active()) {
            result.push_back(silence);
        }
    }
    return result;
}

bool alert_manager::is_silenced(const alert& a) const {
    std::lock_guard<std::mutex> lock(silences_mutex_);

    for (const auto& [id, silence] : silences_) {
        if (silence.matches(a)) {
            return true;
        }
    }
    return false;
}

result_void alert_manager::add_notifier(std::shared_ptr<alert_notifier> notifier) {
    if (!notifier) {
        return make_void_error(monitoring_error_code::invalid_argument,
                              "Notifier cannot be null");
    }

    std::lock_guard<std::mutex> lock(notifiers_mutex_);

    // Check for duplicate names
    for (const auto& n : notifiers_) {
        if (n->name() == notifier->name()) {
            return make_void_error(monitoring_error_code::already_exists,
                                  "Notifier with name '" + notifier->name() + "' already exists");
        }
    }

    notifiers_.push_back(std::move(notifier));
    return make_void_success();
}

result_void alert_manager::remove_notifier(const std::string& notifier_name) {
    std::lock_guard<std::mutex> lock(notifiers_mutex_);

    auto it = std::remove_if(notifiers_.begin(), notifiers_.end(),
        [&notifier_name](const std::shared_ptr<alert_notifier>& n) {
            return n->name() == notifier_name;
        });

    if (it == notifiers_.end()) {
        return make_void_error(monitoring_error_code::not_found,
                              "Notifier '" + notifier_name + "' not found");
    }

    notifiers_.erase(it, notifiers_.end());
    return make_void_success();
}

std::vector<std::shared_ptr<alert_notifier>> alert_manager::get_notifiers() const {
    std::lock_guard<std::mutex> lock(notifiers_mutex_);
    return notifiers_;
}

void alert_manager::set_metric_provider(metric_provider_func provider) {
    std::lock_guard<std::mutex> lock(provider_mutex_);
    metric_provider_ = std::move(provider);
}

void alert_manager::set_event_bus(std::shared_ptr<interface_event_bus> event_bus) {
    event_bus_ = std::move(event_bus);
}

alert_manager_metrics alert_manager::get_metrics() const {
    return metrics_;
}

const alert_manager_config& alert_manager::config() const {
    return config_;
}

void alert_manager::evaluation_loop() {
    while (running_.load()) {
        auto start_time = std::chrono::steady_clock::now();

        // Get metrics from provider and evaluate rules
        metric_provider_func provider;
        {
            std::lock_guard<std::mutex> lock(provider_mutex_);
            provider = metric_provider_;
        }

        if (provider) {
            std::vector<std::shared_ptr<alert_rule>> rules_to_evaluate;
            {
                std::lock_guard<std::mutex> lock(rules_mutex_);
                for (const auto& [name, rule] : rules_) {
                    if (rule->is_enabled()) {
                        rules_to_evaluate.push_back(rule);
                    }
                }
            }

            for (const auto& rule : rules_to_evaluate) {
                auto value = provider(rule->metric_name());
                if (value.has_value()) {
                    evaluate_rule(rule, *value);
                    metrics_.rules_evaluated++;
                }
            }
        }

        // Cleanup expired silences
        cleanup_silences();

        // Cleanup old resolved alerts
        cleanup_resolved_alerts();

        // Wait for next evaluation interval
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        auto wait_time = config_.default_evaluation_interval - elapsed;

        if (wait_time > std::chrono::milliseconds::zero()) {
            std::unique_lock<std::mutex> lock(cv_mutex_);
            cv_.wait_for(lock, wait_time, [this] { return !running_.load(); });
        }
    }
}

void alert_manager::evaluate_rule(const std::shared_ptr<alert_rule>& rule, double value) {
    if (!rule || !rule->trigger()) {
        return;
    }

    bool condition_met = rule->trigger()->evaluate(value);
    alert a = rule->create_alert(value);
    std::string fingerprint = a.fingerprint();

    update_alert_state(fingerprint, condition_met, value, rule);
}

void alert_manager::update_alert_state(const std::string& fingerprint,
                                       bool condition_met,
                                       double value,
                                       const std::shared_ptr<alert_rule>& rule) {
    std::lock_guard<std::mutex> lock(alerts_mutex_);

    auto it = alerts_.find(fingerprint);
    auto now = std::chrono::steady_clock::now();

    if (condition_met) {
        if (it == alerts_.end()) {
            // Create new alert in pending state
            alert new_alert = rule->create_alert(value);
            new_alert.transition_to(alert_state::pending);
            alerts_[fingerprint] = std::move(new_alert);
            metrics_.alerts_created++;
        } else {
            alert& existing = it->second;
            existing.value = value;

            if (existing.state == alert_state::pending) {
                // Check if for_duration has elapsed
                auto pending_duration = now - existing.updated_at;
                if (pending_duration >= rule->config().for_duration) {
                    if (existing.transition_to(alert_state::firing)) {
                        if (!is_silenced(existing)) {
                            send_notifications(existing);
                        } else {
                            metrics_.alerts_suppressed++;
                        }
                    }
                }
            } else if (existing.state == alert_state::firing) {
                // Check if we should send repeat notification
                auto last_notify_it = last_notification_times_.find(fingerprint);
                if (last_notify_it != last_notification_times_.end()) {
                    auto since_last = now - last_notify_it->second;
                    if (since_last >= rule->config().repeat_interval) {
                        if (!is_silenced(existing)) {
                            send_notifications(existing);
                        }
                    }
                }
            } else if (existing.state == alert_state::resolved ||
                       existing.state == alert_state::inactive) {
                // Alert has come back
                existing.transition_to(alert_state::pending);
            }
        }
    } else {
        // Condition not met
        if (it != alerts_.end()) {
            alert& existing = it->second;

            if (existing.state == alert_state::pending) {
                existing.transition_to(alert_state::inactive);
            } else if (existing.state == alert_state::firing) {
                if (existing.transition_to(alert_state::resolved)) {
                    metrics_.alerts_resolved++;
                    if (!is_silenced(existing)) {
                        send_notifications(existing);
                    }
                }
            }
        }
    }
}

void alert_manager::send_notifications(const alert& a) {
    std::vector<std::shared_ptr<alert_notifier>> notifiers;
    {
        std::lock_guard<std::mutex> lock(notifiers_mutex_);
        notifiers = notifiers_;
    }

    for (const auto& notifier : notifiers) {
        if (notifier && notifier->is_ready()) {
            auto result = notifier->notify(a);
            if (result.is_ok()) {
                metrics_.notifications_sent++;
            } else {
                metrics_.notifications_failed++;
            }
        }
    }

    // Update last notification time
    last_notification_times_[a.fingerprint()] = std::chrono::steady_clock::now();
}

void alert_manager::cleanup_silences() {
    std::lock_guard<std::mutex> lock(silences_mutex_);

    auto it = silences_.begin();
    while (it != silences_.end()) {
        if (!it->second.is_active()) {
            it = silences_.erase(it);
        } else {
            ++it;
        }
    }
}

void alert_manager::cleanup_resolved_alerts() {
    std::lock_guard<std::mutex> lock(alerts_mutex_);

    auto it = alerts_.begin();
    while (it != alerts_.end()) {
        if (it->second.state == alert_state::resolved) {
            auto since_resolved = std::chrono::steady_clock::now() - it->second.updated_at;
            if (since_resolved > config_.resolve_timeout) {
                last_notification_times_.erase(it->first);
                it = alerts_.erase(it);
                continue;
            }
        }
        ++it;
    }
}

// ========== log_notifier Implementation ==========

result_void log_notifier::notify(const alert& a) {
    std::ostringstream oss;
    oss << "[ALERT] " << alert_state_to_string(a.state) << " - "
        << a.name << " (" << alert_severity_to_string(a.severity) << "): "
        << a.annotations.summary
        << " | Value: " << a.value;

    std::cout << oss.str() << std::endl;
    return make_void_success();
}

result_void log_notifier::notify_group(const alert_group& group) {
    std::ostringstream oss;
    oss << "[ALERT GROUP] " << group.group_key
        << " (" << group.size() << " alerts, max severity: "
        << alert_severity_to_string(group.max_severity()) << ")";

    std::cout << oss.str() << std::endl;

    for (const auto& a : group.alerts) {
        auto result = notify(a);
        if (!result.is_ok()) {
            return result;
        }
    }

    return make_void_success();
}

} // namespace kcenon::monitoring
