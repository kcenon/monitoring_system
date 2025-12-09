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

/**
 * @file security_collector.h
 * @brief Security event monitoring collector
 *
 * This file provides security event monitoring for audit and compliance.
 * Tracking security events helps detect security incidents, audit access patterns,
 * and maintain compliance with security policies.
 *
 * Platform APIs:
 * - Linux: /var/log/auth.log or /var/log/secure parsing
 * - macOS: Unified logging with security subsystem predicates
 * - Windows: Stub implementation (future: Windows Event Log API)
 */

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "../interfaces/metric_types_adapter.h"

namespace kcenon {
namespace monitoring {

/**
 * @enum security_event_type
 * @brief Types of security events tracked
 */
enum class security_event_type {
    login_success = 1,     ///< Successful login attempt
    login_failure = 2,     ///< Failed login attempt
    logout = 3,            ///< User logout
    sudo_usage = 4,        ///< Privilege escalation (sudo)
    permission_change = 5, ///< Permission/ACL change
    account_created = 6,   ///< New account creation
    account_deleted = 7,   ///< Account deletion
    account_modified = 8,  ///< Account modification
    session_start = 9,     ///< Session started
    session_end = 10,      ///< Session ended
    unknown = 0            ///< Unknown event type
};

/**
 * @brief Convert security_event_type to string representation
 * @param type The event type to convert
 * @return String name of the event type
 */
inline std::string security_event_type_to_string(security_event_type type) {
    switch (type) {
        case security_event_type::login_success: return "LOGIN_SUCCESS";
        case security_event_type::login_failure: return "LOGIN_FAILURE";
        case security_event_type::logout: return "LOGOUT";
        case security_event_type::sudo_usage: return "SUDO_USAGE";
        case security_event_type::permission_change: return "PERMISSION_CHANGE";
        case security_event_type::account_created: return "ACCOUNT_CREATED";
        case security_event_type::account_deleted: return "ACCOUNT_DELETED";
        case security_event_type::account_modified: return "ACCOUNT_MODIFIED";
        case security_event_type::session_start: return "SESSION_START";
        case security_event_type::session_end: return "SESSION_END";
        default: return "UNKNOWN";
    }
}

/**
 * @struct security_event
 * @brief Individual security event information
 */
struct security_event {
    security_event_type type{security_event_type::unknown}; ///< Event type
    std::string username;           ///< Username involved (may be masked for privacy)
    std::string source;             ///< Source IP/terminal
    std::string message;            ///< Event message/details
    bool success{false};            ///< Whether the action succeeded
    std::chrono::system_clock::time_point timestamp; ///< Event timestamp
};

/**
 * @struct security_event_counts
 * @brief Counts of security events by type
 */
struct security_event_counts {
    uint64_t login_success{0};      ///< Successful login count
    uint64_t login_failure{0};      ///< Failed login count
    uint64_t logout{0};             ///< Logout count
    uint64_t sudo_usage{0};         ///< Sudo/privilege escalation count
    uint64_t permission_change{0};  ///< Permission change count
    uint64_t account_created{0};    ///< Account creation count
    uint64_t account_deleted{0};    ///< Account deletion count
    uint64_t account_modified{0};   ///< Account modification count
    uint64_t unknown{0};            ///< Unknown event count

    /**
     * Get count for a specific event type
     * @param type The event type
     * @return Count of events of that type
     */
    uint64_t get_count(security_event_type type) const {
        switch (type) {
            case security_event_type::login_success: return login_success;
            case security_event_type::login_failure: return login_failure;
            case security_event_type::logout: return logout;
            case security_event_type::sudo_usage: return sudo_usage;
            case security_event_type::permission_change: return permission_change;
            case security_event_type::account_created: return account_created;
            case security_event_type::account_deleted: return account_deleted;
            case security_event_type::account_modified: return account_modified;
            default: return unknown;
        }
    }

    /**
     * Increment count for a specific event type
     * @param type The event type to increment
     */
    void increment(security_event_type type) {
        switch (type) {
            case security_event_type::login_success: ++login_success; break;
            case security_event_type::login_failure: ++login_failure; break;
            case security_event_type::logout: ++logout; break;
            case security_event_type::sudo_usage: ++sudo_usage; break;
            case security_event_type::permission_change: ++permission_change; break;
            case security_event_type::account_created: ++account_created; break;
            case security_event_type::account_deleted: ++account_deleted; break;
            case security_event_type::account_modified: ++account_modified; break;
            default: ++unknown; break;
        }
    }

    /**
     * Get total event count
     * @return Total number of events across all types
     */
    uint64_t total() const {
        return login_success + login_failure + logout + sudo_usage +
               permission_change + account_created + account_deleted +
               account_modified + unknown;
    }
};

/**
 * @struct security_metrics
 * @brief Aggregated security event metrics
 */
struct security_metrics {
    security_event_counts event_counts;              ///< Event counts by type
    uint64_t active_sessions{0};                     ///< Current active login sessions
    std::vector<security_event> recent_events;       ///< Recent security events (limited)
    double events_per_second{0.0};                   ///< Event rate
    bool metrics_available{false};                   ///< Whether metrics are available
    std::chrono::system_clock::time_point timestamp; ///< Reading timestamp
};

/**
 * @class security_info_collector
 * @brief Platform-specific security event data collector implementation
 *
 * This class handles the low-level platform-specific operations for
 * reading security events from system logs.
 */
class security_info_collector {
   public:
    security_info_collector();
    ~security_info_collector();

    // Non-copyable, non-moveable due to internal state
    security_info_collector(const security_info_collector&) = delete;
    security_info_collector& operator=(const security_info_collector&) = delete;
    security_info_collector(security_info_collector&&) = delete;
    security_info_collector& operator=(security_info_collector&&) = delete;

    /**
     * Check if security event monitoring is available on this system
     * @return True if security events can be read
     */
    bool is_security_monitoring_available() const;

    /**
     * Collect current security event metrics
     * @return security_metrics structure with current values
     */
    security_metrics collect_metrics();

    /**
     * Set maximum number of recent events to track
     * @param max_events Maximum events to keep in memory
     */
    void set_max_recent_events(size_t max_events);

    /**
     * Enable or disable PII masking for usernames
     * @param mask_pii True to mask usernames
     */
    void set_mask_pii(bool mask_pii);

   private:
    mutable std::mutex mutex_;
    mutable bool availability_checked_{false};
    mutable bool available_{false};
    size_t max_recent_events_{100};
    bool mask_pii_{false};
    
    // Track last read position for log parsing
    std::streamoff last_log_position_{0};
    std::chrono::system_clock::time_point last_collection_time_;
    security_event_counts cumulative_counts_;

    // Platform-specific helper methods
    security_metrics collect_metrics_impl();
    bool check_availability_impl() const;
    std::string mask_username(const std::string& username) const;
};

/**
 * @class security_collector
 * @brief Security event monitoring collector
 *
 * Collects security event metrics with cross-platform support.
 * Returns unavailable metrics on Windows (stub implementation).
 */
class security_collector {
   public:
    security_collector();
    ~security_collector() = default;

    // Non-copyable, non-moveable due to internal state
    security_collector(const security_collector&) = delete;
    security_collector& operator=(const security_collector&) = delete;
    security_collector(security_collector&&) = delete;
    security_collector& operator=(security_collector&&) = delete;

    /**
     * Initialize the collector with configuration
     * @param config Configuration options:
     *   - "enabled": "true"/"false" (default: true)
     *   - "mask_pii": "true"/"false" (default: false)
     *   - "max_recent_events": count (default: 100)
     *   - "login_failure_rate_limit": events/sec (default: 1000)
     * @return true if initialization successful
     */
    bool initialize(const std::unordered_map<std::string, std::string>& config);

    /**
     * Collect security event metrics
     * @return Vector of collected metrics
     */
    std::vector<metric> collect();

    /**
     * Get the name of this collector
     * @return Collector name
     */
    std::string get_name() const { return "security_collector"; }

    /**
     * Get supported metric types
     * @return Vector of supported metric type names
     */
    std::vector<std::string> get_metric_types() const;

    /**
     * Check if the collector is healthy
     * @return true if collector is operational
     */
    bool is_healthy() const;

    /**
     * Get collector statistics
     * @return Map of statistic name to value
     */
    std::unordered_map<std::string, double> get_statistics() const;

    /**
     * Get last collected security metrics
     * @return Most recent security_metrics reading
     */
    security_metrics get_last_metrics() const;

    /**
     * Check if security event monitoring is available
     * @return True if security events are accessible
     */
    bool is_security_monitoring_available() const;

   private:
    std::unique_ptr<security_info_collector> collector_;

    // Configuration
    bool enabled_{true};
    bool mask_pii_{false};
    size_t max_recent_events_{100};
    double login_failure_rate_limit_{1000.0};

    // Statistics
    mutable std::mutex stats_mutex_;
    std::atomic<size_t> collection_count_{0};
    std::atomic<size_t> collection_errors_{0};
    security_metrics last_metrics_;

    // Helper methods
    metric create_metric(const std::string& name, double value,
                         const std::unordered_map<std::string, std::string>& tags = {},
                         const std::string& unit = "") const;
    void add_security_metrics(std::vector<metric>& metrics,
                              const security_metrics& security_data);
};

}  // namespace monitoring
}  // namespace kcenon
