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
 * @file alert_types.h
 * @brief Core alert data structures for the monitoring system
 *
 * This file defines the fundamental alert types, states, and data structures
 * used throughout the alert pipeline. Alerts represent conditions that require
 * attention or notification.
 */

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace kcenon::monitoring {

/**
 * @enum alert_severity
 * @brief Severity levels for alerts
 *
 * Defines the urgency and importance of alerts, affecting routing
 * and notification behavior.
 */
enum class alert_severity : uint8_t {
    info = 0,       ///< Informational, no action required
    warning,        ///< Warning condition, may require attention
    critical,       ///< Critical condition, immediate attention required
    emergency       ///< Emergency condition, system-wide impact
};

/**
 * @brief Convert alert severity to string
 * @param severity The severity level
 * @return String representation of severity
 */
constexpr const char* alert_severity_to_string(alert_severity severity) noexcept {
    switch (severity) {
        case alert_severity::info:      return "info";
        case alert_severity::warning:   return "warning";
        case alert_severity::critical:  return "critical";
        case alert_severity::emergency: return "emergency";
        default:                        return "unknown";
    }
}

/**
 * @enum alert_state
 * @brief State machine states for alert lifecycle
 *
 * State machine:
 *   inactive ─[condition met]─> pending
 *   pending ─[for_duration elapsed]─> firing
 *   firing ─[condition cleared]─> resolved
 *   resolved ─[condition met]─> pending
 *   any state ─[silenced]─> suppressed
 *   suppressed ─[silence expired]─> previous state
 */
enum class alert_state : uint8_t {
    inactive = 0,   ///< Alert condition not met
    pending,        ///< Condition met, waiting for duration threshold
    firing,         ///< Alert is active and notifications sent
    resolved,       ///< Alert condition cleared
    suppressed      ///< Alert is silenced
};

/**
 * @brief Convert alert state to string
 * @param state The alert state
 * @return String representation of state
 */
constexpr const char* alert_state_to_string(alert_state state) noexcept {
    switch (state) {
        case alert_state::inactive:   return "inactive";
        case alert_state::pending:    return "pending";
        case alert_state::firing:     return "firing";
        case alert_state::resolved:   return "resolved";
        case alert_state::suppressed: return "suppressed";
        default:                      return "unknown";
    }
}

/**
 * @struct alert_labels
 * @brief Key-value labels for alert identification and routing
 *
 * Labels are used for alert grouping, deduplication, and routing
 * to appropriate notification channels.
 */
struct alert_labels {
    std::unordered_map<std::string, std::string> labels;

    alert_labels() = default;

    explicit alert_labels(std::unordered_map<std::string, std::string> lbl)
        : labels(std::move(lbl)) {}

    /**
     * @brief Add or update a label
     * @param key Label key
     * @param value Label value
     */
    void set(const std::string& key, const std::string& value) {
        labels[key] = value;
    }

    /**
     * @brief Get a label value
     * @param key Label key
     * @return Label value or empty string if not found
     */
    std::string get(const std::string& key) const {
        auto it = labels.find(key);
        return it != labels.end() ? it->second : "";
    }

    /**
     * @brief Check if a label exists
     * @param key Label key
     * @return True if label exists
     */
    bool has(const std::string& key) const {
        return labels.find(key) != labels.end();
    }

    /**
     * @brief Generate a fingerprint for deduplication
     * @return Hash string based on sorted labels
     */
    std::string fingerprint() const {
        std::string result;
        std::vector<std::pair<std::string, std::string>> sorted_labels(
            labels.begin(), labels.end());
        std::sort(sorted_labels.begin(), sorted_labels.end());
        for (const auto& [key, value] : sorted_labels) {
            result += key + "=" + value + ",";
        }
        return result;
    }

    bool operator==(const alert_labels& other) const {
        return labels == other.labels;
    }
};

/**
 * @struct alert_annotations
 * @brief Additional metadata for alert context
 *
 * Annotations provide human-readable information about the alert
 * but are not used for routing or deduplication.
 */
struct alert_annotations {
    std::string summary;                                        ///< Brief description
    std::string description;                                    ///< Detailed description
    std::optional<std::string> runbook_url;                     ///< Link to runbook
    std::unordered_map<std::string, std::string> custom;        ///< Custom annotations

    alert_annotations() = default;

    alert_annotations(std::string sum, std::string desc)
        : summary(std::move(sum)), description(std::move(desc)) {}
};

/**
 * @struct alert
 * @brief Core alert data structure
 *
 * Represents a single alert instance with its current state, metadata,
 * and lifecycle timestamps.
 *
 * @thread_safety This structure is not thread-safe. External synchronization
 *                is required when accessed from multiple threads.
 */
struct alert {
    std::string name;                                           ///< Alert name/identifier
    alert_labels labels;                                        ///< Identifying labels
    alert_annotations annotations;                              ///< Descriptive annotations
    alert_severity severity = alert_severity::warning;          ///< Alert severity level
    alert_state state = alert_state::inactive;                  ///< Current state
    double value = 0.0;                                         ///< Current metric value

    std::chrono::steady_clock::time_point created_at;           ///< When alert was created
    std::chrono::steady_clock::time_point updated_at;           ///< Last state change
    std::optional<std::chrono::steady_clock::time_point> started_at;  ///< When firing started
    std::optional<std::chrono::steady_clock::time_point> resolved_at; ///< When resolved

    uint64_t id = 0;                                            ///< Unique alert ID
    std::string rule_name;                                      ///< Name of triggering rule
    std::string group_key;                                      ///< Grouping key for dedup

    /**
     * @brief Default constructor
     */
    alert()
        : created_at(std::chrono::steady_clock::now())
        , updated_at(created_at)
        , id(generate_id()) {}

    /**
     * @brief Construct with name and labels
     * @param alert_name Alert name
     * @param alert_labels Alert labels
     */
    alert(std::string alert_name, alert_labels lbl)
        : name(std::move(alert_name))
        , labels(std::move(lbl))
        , created_at(std::chrono::steady_clock::now())
        , updated_at(created_at)
        , id(generate_id()) {}

    /**
     * @brief Get alert fingerprint for deduplication
     * @return Unique fingerprint based on name and labels
     */
    std::string fingerprint() const {
        return name + "{" + labels.fingerprint() + "}";
    }

    /**
     * @brief Check if alert is currently active (pending or firing)
     * @return True if alert is active
     */
    bool is_active() const {
        return state == alert_state::pending || state == alert_state::firing;
    }

    /**
     * @brief Get duration in current state
     * @return Duration since last state change
     */
    std::chrono::steady_clock::duration state_duration() const {
        return std::chrono::steady_clock::now() - updated_at;
    }

    /**
     * @brief Get firing duration (if currently firing)
     * @return Duration since firing started, or zero if not firing
     */
    std::chrono::steady_clock::duration firing_duration() const {
        if (state == alert_state::firing && started_at.has_value()) {
            return std::chrono::steady_clock::now() - *started_at;
        }
        return std::chrono::steady_clock::duration::zero();
    }

    /**
     * @brief Transition to a new state
     * @param new_state The new state
     * @return True if transition was valid
     */
    bool transition_to(alert_state new_state) {
        if (!is_valid_transition(state, new_state)) {
            return false;
        }

        auto now = std::chrono::steady_clock::now();
        state = new_state;
        updated_at = now;

        if (new_state == alert_state::firing && !started_at.has_value()) {
            started_at = now;
        } else if (new_state == alert_state::resolved) {
            resolved_at = now;
        }

        return true;
    }

private:
    static uint64_t generate_id() {
        static std::atomic<uint64_t> counter{0};
        return counter.fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * @brief Check if state transition is valid
     */
    static bool is_valid_transition(alert_state from, alert_state to) {
        // All transitions are valid from suppressed (when silence expires)
        if (from == alert_state::suppressed) {
            return true;
        }

        // Transition to suppressed is always valid
        if (to == alert_state::suppressed) {
            return true;
        }

        switch (from) {
            case alert_state::inactive:
                return to == alert_state::pending;
            case alert_state::pending:
                return to == alert_state::firing || to == alert_state::inactive;
            case alert_state::firing:
                return to == alert_state::resolved;
            case alert_state::resolved:
                return to == alert_state::pending || to == alert_state::inactive;
            default:
                return false;
        }
    }
};

/**
 * @struct alert_group
 * @brief Group of related alerts for batch notification
 *
 * Alerts with the same group key are combined into an alert group
 * to reduce notification noise.
 */
struct alert_group {
    std::string group_key;                                      ///< Common grouping key
    std::vector<alert> alerts;                                  ///< Alerts in this group
    std::chrono::steady_clock::time_point created_at;           ///< Group creation time
    std::chrono::steady_clock::time_point updated_at;           ///< Last modification time
    alert_labels common_labels;                                 ///< Labels shared by all alerts

    alert_group()
        : created_at(std::chrono::steady_clock::now())
        , updated_at(created_at) {}

    explicit alert_group(std::string key)
        : group_key(std::move(key))
        , created_at(std::chrono::steady_clock::now())
        , updated_at(created_at) {}

    /**
     * @brief Add an alert to the group
     * @param a Alert to add
     */
    void add_alert(alert a) {
        alerts.push_back(std::move(a));
        updated_at = std::chrono::steady_clock::now();
    }

    /**
     * @brief Get count of alerts in the group
     * @return Number of alerts
     */
    size_t size() const {
        return alerts.size();
    }

    /**
     * @brief Check if group is empty
     * @return True if no alerts in group
     */
    bool empty() const {
        return alerts.empty();
    }

    /**
     * @brief Get highest severity in the group
     * @return Maximum severity level
     */
    alert_severity max_severity() const {
        alert_severity max_sev = alert_severity::info;
        for (const auto& a : alerts) {
            if (static_cast<uint8_t>(a.severity) > static_cast<uint8_t>(max_sev)) {
                max_sev = a.severity;
            }
        }
        return max_sev;
    }
};

/**
 * @struct alert_silence
 * @brief Silence configuration to suppress alerts
 *
 * Silences prevent matching alerts from sending notifications
 * for a specified duration.
 */
struct alert_silence {
    uint64_t id = 0;                                            ///< Silence ID
    std::string comment;                                        ///< Reason for silence
    std::string created_by;                                     ///< Creator identifier
    alert_labels matchers;                                      ///< Labels to match
    std::chrono::steady_clock::time_point starts_at;            ///< Silence start time
    std::chrono::steady_clock::time_point ends_at;              ///< Silence end time

    alert_silence()
        : id(generate_id())
        , starts_at(std::chrono::steady_clock::now())
        , ends_at(starts_at + std::chrono::hours(1)) {}

    /**
     * @brief Check if silence is currently active
     * @return True if within active period
     */
    bool is_active() const {
        auto now = std::chrono::steady_clock::now();
        return now >= starts_at && now < ends_at;
    }

    /**
     * @brief Check if an alert matches this silence
     * @param a The alert to check
     * @return True if alert should be silenced
     */
    bool matches(const alert& a) const {
        if (!is_active()) {
            return false;
        }

        // All matcher labels must be present in alert labels
        for (const auto& [key, value] : matchers.labels) {
            if (a.labels.get(key) != value) {
                return false;
            }
        }
        return true;
    }

private:
    static uint64_t generate_id() {
        static std::atomic<uint64_t> counter{0};
        return counter.fetch_add(1, std::memory_order_relaxed);
    }
};

} // namespace kcenon::monitoring
