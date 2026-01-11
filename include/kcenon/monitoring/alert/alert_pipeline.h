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
 * @file alert_pipeline.h
 * @brief Alert processing pipeline components
 *
 * This file provides the alert pipeline infrastructure for processing,
 * grouping, deduplicating, and routing alerts through various stages.
 */

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "alert_types.h"
#include "../core/result_types.h"

namespace kcenon::monitoring {

/**
 * @struct alert_aggregator_config
 * @brief Configuration for alert aggregation
 */
struct alert_aggregator_config {
    std::chrono::milliseconds group_wait{30000};        ///< Initial wait before sending
    std::chrono::milliseconds group_interval{300000};   ///< Interval between group sends
    std::chrono::milliseconds resolve_timeout{300000};  ///< Time before removing resolved
    std::vector<std::string> group_by_labels;           ///< Labels to group by

    /**
     * @brief Validate configuration
     */
    bool validate() const {
        return group_wait.count() > 0 &&
               group_interval.count() > 0 &&
               resolve_timeout.count() > 0;
    }
};

/**
 * @class alert_aggregator
 * @brief Groups and deduplicates alerts
 *
 * The aggregator collects alerts over time and groups them based on
 * configured labels. This reduces notification noise by batching
 * related alerts together.
 *
 * @thread_safety This class is thread-safe.
 *
 * @example
 * @code
 * alert_aggregator_config config;
 * config.group_by_labels = {"service", "environment"};
 * config.group_wait = std::chrono::seconds(30);
 *
 * alert_aggregator aggregator(config);
 *
 * // Add alerts
 * aggregator.add_alert(cpu_alert);
 * aggregator.add_alert(memory_alert);
 *
 * // Get ready groups
 * auto groups = aggregator.get_ready_groups();
 * for (auto& group : groups) {
 *     notify_group(group);
 *     aggregator.mark_sent(group.group_key);
 * }
 * @endcode
 */
class alert_aggregator {
public:
    /**
     * @brief Construct with configuration
     */
    explicit alert_aggregator(const alert_aggregator_config& config)
        : config_(config) {}

    /**
     * @brief Add an alert for aggregation
     * @param a Alert to aggregate
     * @return Group key the alert was added to
     */
    std::string add_alert(const alert& a) {
        std::lock_guard<std::mutex> lock(mutex_);

        std::string group_key = compute_group_key(a);
        auto now = std::chrono::steady_clock::now();

        auto it = groups_.find(group_key);
        if (it == groups_.end()) {
            alert_group new_group(group_key);
            new_group.common_labels = extract_common_labels(a);
            new_group.add_alert(a);
            groups_[group_key] = std::move(new_group);
            first_seen_[group_key] = now;
        } else {
            // Check for duplicate
            bool is_duplicate = false;
            for (auto& existing : it->second.alerts) {
                if (existing.fingerprint() == a.fingerprint()) {
                    existing = a;  // Update existing alert
                    is_duplicate = true;
                    break;
                }
            }
            if (!is_duplicate) {
                it->second.add_alert(a);
            }
        }

        return group_key;
    }

    /**
     * @brief Get groups ready for notification
     * @return Vector of ready groups
     */
    std::vector<alert_group> get_ready_groups() {
        std::lock_guard<std::mutex> lock(mutex_);

        std::vector<alert_group> ready;
        auto now = std::chrono::steady_clock::now();

        for (auto& [key, group] : groups_) {
            if (group.empty()) {
                continue;
            }

            // Check if first wait has elapsed
            auto first_seen = first_seen_[key];
            if (now - first_seen < config_.group_wait) {
                continue;
            }

            // Check if enough time since last send
            auto last_sent_it = last_sent_.find(key);
            if (last_sent_it != last_sent_.end()) {
                if (now - last_sent_it->second < config_.group_interval) {
                    continue;
                }
            }

            ready.push_back(group);
        }

        return ready;
    }

    /**
     * @brief Mark a group as sent
     * @param group_key Group key to mark
     */
    void mark_sent(const std::string& group_key) {
        std::lock_guard<std::mutex> lock(mutex_);
        last_sent_[group_key] = std::chrono::steady_clock::now();
    }

    /**
     * @brief Remove resolved alerts and clean up old groups
     */
    void cleanup() {
        std::lock_guard<std::mutex> lock(mutex_);

        auto now = std::chrono::steady_clock::now();

        for (auto it = groups_.begin(); it != groups_.end(); ) {
            auto& group = it->second;

            // Remove resolved alerts that have timed out
            auto alert_it = group.alerts.begin();
            while (alert_it != group.alerts.end()) {
                if (alert_it->state == alert_state::resolved) {
                    auto resolved_time = alert_it->resolved_at.value_or(now);
                    if (now - resolved_time > config_.resolve_timeout) {
                        alert_it = group.alerts.erase(alert_it);
                        continue;
                    }
                }
                ++alert_it;
            }

            // Remove empty groups
            if (group.empty()) {
                first_seen_.erase(it->first);
                last_sent_.erase(it->first);
                it = groups_.erase(it);
            } else {
                ++it;
            }
        }
    }

    /**
     * @brief Get current group count
     */
    size_t group_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return groups_.size();
    }

    /**
     * @brief Get total alert count across all groups
     */
    size_t total_alert_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t count = 0;
        for (const auto& [key, group] : groups_) {
            count += group.size();
        }
        return count;
    }

private:
    std::string compute_group_key(const alert& a) const {
        if (config_.group_by_labels.empty()) {
            return a.rule_name;
        }

        std::string key;
        for (const auto& label : config_.group_by_labels) {
            key += a.labels.get(label) + ":";
        }
        return key;
    }

    alert_labels extract_common_labels(const alert& a) const {
        alert_labels common;
        for (const auto& label : config_.group_by_labels) {
            std::string val = a.labels.get(label);
            if (!val.empty()) {
                common.set(label, val);
            }
        }
        return common;
    }

    alert_aggregator_config config_;
    mutable std::mutex mutex_;

    std::unordered_map<std::string, alert_group> groups_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> first_seen_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> last_sent_;
};

/**
 * @struct inhibition_rule
 * @brief Rule for inhibiting alerts based on other alerts
 *
 * When a source alert is firing, target alerts matching the
 * specified labels are inhibited (silenced).
 */
struct inhibition_rule {
    std::string name;
    alert_labels source_match;      ///< Labels that source alert must have
    alert_labels target_match;      ///< Labels that target alert must have
    std::vector<std::string> equal; ///< Labels that must be equal on both

    /**
     * @brief Check if source alert matches this rule
     */
    bool matches_source(const alert& a) const {
        for (const auto& [key, value] : source_match.labels) {
            if (a.labels.get(key) != value) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Check if target alert should be inhibited by source
     */
    bool should_inhibit(const alert& source, const alert& target) const {
        // Source must match
        if (!matches_source(source)) {
            return false;
        }

        // Target must match target_match labels
        for (const auto& [key, value] : target_match.labels) {
            if (target.labels.get(key) != value) {
                return false;
            }
        }

        // Equal labels must match between source and target
        for (const auto& label : equal) {
            if (source.labels.get(label) != target.labels.get(label)) {
                return false;
            }
        }

        return true;
    }
};

/**
 * @class alert_inhibitor
 * @brief Manages alert inhibition rules
 *
 * Alert inhibition prevents certain alerts from triggering when
 * related, more important alerts are already firing.
 *
 * @example
 * @code
 * inhibition_rule rule;
 * rule.name = "critical_inhibits_warning";
 * rule.source_match.set("severity", "critical");
 * rule.target_match.set("severity", "warning");
 * rule.equal = {"service"};
 *
 * alert_inhibitor inhibitor;
 * inhibitor.add_rule(rule);
 *
 * // When critical_alert is firing, warning_alert is inhibited
 * if (inhibitor.is_inhibited(warning_alert, {critical_alert})) {
 *     // Don't notify for warning_alert
 * }
 * @endcode
 */
class alert_inhibitor {
public:
    /**
     * @brief Add an inhibition rule
     */
    void add_rule(const inhibition_rule& rule) {
        std::lock_guard<std::mutex> lock(mutex_);
        rules_.push_back(rule);
    }

    /**
     * @brief Remove an inhibition rule by name
     */
    void remove_rule(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        rules_.erase(
            std::remove_if(rules_.begin(), rules_.end(),
                [&name](const inhibition_rule& r) { return r.name == name; }),
            rules_.end()
        );
    }

    /**
     * @brief Check if an alert is inhibited by any active alerts
     * @param target Alert to check
     * @param active_alerts Currently firing alerts
     * @return True if alert should be inhibited
     */
    bool is_inhibited(const alert& target,
                      const std::vector<alert>& active_alerts) const {
        std::lock_guard<std::mutex> lock(mutex_);

        for (const auto& rule : rules_) {
            for (const auto& source : active_alerts) {
                // Source must be firing
                if (source.state != alert_state::firing) {
                    continue;
                }

                // Don't inhibit self
                if (source.fingerprint() == target.fingerprint()) {
                    continue;
                }

                if (rule.should_inhibit(source, target)) {
                    return true;
                }
            }
        }

        return false;
    }

    /**
     * @brief Get all rules
     */
    std::vector<inhibition_rule> get_rules() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return rules_;
    }

private:
    mutable std::mutex mutex_;
    std::vector<inhibition_rule> rules_;
};

/**
 * @class cooldown_tracker
 * @brief Tracks cooldown periods for alert notifications
 *
 * Prevents notification spam by enforcing minimum intervals
 * between notifications for the same alert.
 */
class cooldown_tracker {
public:
    /**
     * @brief Set default cooldown period
     */
    explicit cooldown_tracker(std::chrono::milliseconds default_cooldown)
        : default_cooldown_(default_cooldown) {}

    /**
     * @brief Check if alert is in cooldown
     * @param fingerprint Alert fingerprint
     * @return True if in cooldown
     */
    bool is_in_cooldown(const std::string& fingerprint) const {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = last_notification_.find(fingerprint);
        if (it == last_notification_.end()) {
            return false;
        }

        auto now = std::chrono::steady_clock::now();
        auto cooldown = get_cooldown_for(fingerprint);
        return (now - it->second) < cooldown;
    }

    /**
     * @brief Record notification time
     * @param fingerprint Alert fingerprint
     */
    void record_notification(const std::string& fingerprint) {
        std::lock_guard<std::mutex> lock(mutex_);
        last_notification_[fingerprint] = std::chrono::steady_clock::now();
    }

    /**
     * @brief Set custom cooldown for specific alert
     * @param fingerprint Alert fingerprint
     * @param cooldown Custom cooldown duration
     */
    void set_cooldown(const std::string& fingerprint,
                      std::chrono::milliseconds cooldown) {
        std::lock_guard<std::mutex> lock(mutex_);
        custom_cooldowns_[fingerprint] = cooldown;
    }

    /**
     * @brief Get time remaining in cooldown
     * @param fingerprint Alert fingerprint
     * @return Remaining cooldown time, or zero if not in cooldown
     */
    std::chrono::milliseconds remaining_cooldown(const std::string& fingerprint) const {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = last_notification_.find(fingerprint);
        if (it == last_notification_.end()) {
            return std::chrono::milliseconds::zero();
        }

        auto now = std::chrono::steady_clock::now();
        auto cooldown = get_cooldown_for(fingerprint);
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - it->second);

        if (elapsed >= cooldown) {
            return std::chrono::milliseconds::zero();
        }
        return cooldown - elapsed;
    }

    /**
     * @brief Clear cooldown state for an alert
     */
    void clear_cooldown(const std::string& fingerprint) {
        std::lock_guard<std::mutex> lock(mutex_);
        last_notification_.erase(fingerprint);
    }

    /**
     * @brief Clear all cooldown state
     */
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        last_notification_.clear();
    }

private:
    std::chrono::milliseconds get_cooldown_for(const std::string& fingerprint) const {
        auto it = custom_cooldowns_.find(fingerprint);
        if (it != custom_cooldowns_.end()) {
            return it->second;
        }
        return default_cooldown_;
    }

    std::chrono::milliseconds default_cooldown_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> last_notification_;
    std::unordered_map<std::string, std::chrono::milliseconds> custom_cooldowns_;
};

/**
 * @class alert_deduplicator
 * @brief Deduplicates alerts based on fingerprint
 *
 * Maintains a cache of recently seen alerts to prevent
 * duplicate notifications for the same alert condition.
 */
class alert_deduplicator {
public:
    /**
     * @brief Construct with cache duration
     * @param cache_duration How long to remember alerts
     */
    explicit alert_deduplicator(std::chrono::milliseconds cache_duration)
        : cache_duration_(cache_duration) {}

    /**
     * @brief Check if alert is a duplicate
     * @param a Alert to check
     * @return True if duplicate
     */
    bool is_duplicate(const alert& a) {
        std::lock_guard<std::mutex> lock(mutex_);

        cleanup_expired();

        auto fingerprint = a.fingerprint();
        auto it = seen_.find(fingerprint);

        if (it == seen_.end()) {
            seen_[fingerprint] = std::chrono::steady_clock::now();
            return false;
        }

        // Same state is duplicate
        auto state_it = last_state_.find(fingerprint);
        if (state_it != last_state_.end() && state_it->second == a.state) {
            return true;
        }

        // State changed - not duplicate
        last_state_[fingerprint] = a.state;
        return false;
    }

    /**
     * @brief Mark alert as seen
     */
    void mark_seen(const alert& a) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto fingerprint = a.fingerprint();
        seen_[fingerprint] = std::chrono::steady_clock::now();
        last_state_[fingerprint] = a.state;
    }

    /**
     * @brief Clear deduplication cache
     */
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        seen_.clear();
        last_state_.clear();
    }

private:
    void cleanup_expired() {
        auto now = std::chrono::steady_clock::now();
        for (auto it = seen_.begin(); it != seen_.end(); ) {
            if (now - it->second > cache_duration_) {
                last_state_.erase(it->first);
                it = seen_.erase(it);
            } else {
                ++it;
            }
        }
    }

    std::chrono::milliseconds cache_duration_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> seen_;
    std::unordered_map<std::string, alert_state> last_state_;
};

/**
 * @class pipeline_stage
 * @brief Base class for pipeline processing stages
 */
class pipeline_stage {
public:
    virtual ~pipeline_stage() = default;

    /**
     * @brief Process an alert through this stage
     * @param a Alert to process
     * @return True if alert should continue, false to stop pipeline
     */
    virtual bool process(alert& a) = 0;

    /**
     * @brief Get stage name
     */
    virtual std::string name() const = 0;
};

/**
 * @class alert_pipeline
 * @brief Configurable alert processing pipeline
 *
 * Allows building custom alert processing workflows by chaining
 * multiple processing stages together.
 */
class alert_pipeline {
public:
    /**
     * @brief Add a processing stage
     */
    void add_stage(std::shared_ptr<pipeline_stage> stage) {
        stages_.push_back(std::move(stage));
    }

    /**
     * @brief Process an alert through all stages
     * @param a Alert to process
     * @return True if alert passed all stages
     */
    bool process(alert& a) {
        for (const auto& stage : stages_) {
            if (!stage->process(a)) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Get stage names
     */
    std::vector<std::string> stage_names() const {
        std::vector<std::string> names;
        names.reserve(stages_.size());
        for (const auto& stage : stages_) {
            names.push_back(stage->name());
        }
        return names;
    }

private:
    std::vector<std::shared_ptr<pipeline_stage>> stages_;
};

} // namespace kcenon::monitoring
