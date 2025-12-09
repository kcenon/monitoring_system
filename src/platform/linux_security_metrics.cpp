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

/**
 * @file linux_security_metrics.cpp
 * @brief Linux implementation of security event monitoring
 *
 * Parses /var/log/auth.log or /var/log/secure for authentication events.
 */

#include <kcenon/monitoring/collectors/security_collector.h>

#ifdef __linux__

#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <sys/stat.h>

namespace kcenon {
namespace monitoring {

namespace {

// Log file paths to check
const std::vector<std::string> AUTH_LOG_PATHS = {
    "/var/log/auth.log",   // Debian/Ubuntu
    "/var/log/secure"      // RHEL/CentOS/Fedora
};

// Regex patterns for parsing auth log entries
// SSH successful login: "Accepted password for <user> from <ip>"
// SSH failed login: "Failed password for <user> from <ip>"
// sudo usage: "sudo: <user> : TTY=<tty> ; PWD=<pwd> ; USER=<target> ; COMMAND=<cmd>"
// Session opened: "pam_unix(sshd:session): session opened for user <user>"
// Session closed: "pam_unix(sshd:session): session closed for user <user>"

const std::regex ACCEPTED_PASSWORD_REGEX(
    R"(Accepted\s+(?:password|publickey)\s+for\s+(\S+)\s+from\s+(\S+))");
const std::regex FAILED_PASSWORD_REGEX(
    R"(Failed\s+password\s+for\s+(?:invalid\s+user\s+)?(\S+)\s+from\s+(\S+))");
const std::regex SUDO_REGEX(
    R"(sudo:\s+(\S+)\s+:.*COMMAND=)");
const std::regex SESSION_OPENED_REGEX(
    R"(session\s+opened\s+for\s+user\s+(\S+))");
const std::regex SESSION_CLOSED_REGEX(
    R"(session\s+closed\s+for\s+user\s+(\S+))");
const std::regex USERADD_REGEX(
    R"(useradd.*new\s+user:\s+name=(\S+))");
const std::regex USERDEL_REGEX(
    R"(userdel.*delete\s+user\s+'(\S+)')");

std::string find_auth_log() {
    for (const auto& path : AUTH_LOG_PATHS) {
        struct stat st;
        if (stat(path.c_str(), &st) == 0) {
            return path;
        }
    }
    return "";
}

}  // namespace

security_info_collector::security_info_collector()
    : last_collection_time_(std::chrono::system_clock::now()) {
}

security_info_collector::~security_info_collector() = default;

bool security_info_collector::check_availability_impl() const {
    std::string log_path = find_auth_log();
    if (log_path.empty()) {
        return false;
    }
    
    // Check if we can read the file
    std::ifstream file(log_path);
    return file.good();
}

bool security_info_collector::is_security_monitoring_available() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!availability_checked_) {
        available_ = check_availability_impl();
        availability_checked_ = true;
    }
    return available_;
}

void security_info_collector::set_max_recent_events(size_t max_events) {
    std::lock_guard<std::mutex> lock(mutex_);
    max_recent_events_ = max_events;
}

void security_info_collector::set_mask_pii(bool mask_pii) {
    std::lock_guard<std::mutex> lock(mutex_);
    mask_pii_ = mask_pii;
}

std::string security_info_collector::mask_username(const std::string& username) const {
    if (!mask_pii_ || username.empty()) {
        return username;
    }
    
    // Mask all but first and last character
    if (username.length() <= 2) {
        return std::string(username.length(), '*');
    }
    
    std::string masked = username;
    for (size_t i = 1; i < masked.length() - 1; ++i) {
        masked[i] = '*';
    }
    return masked;
}

security_metrics security_info_collector::collect_metrics_impl() {
    security_metrics metrics;
    metrics.timestamp = std::chrono::system_clock::now();
    
    std::string log_path = find_auth_log();
    if (log_path.empty()) {
        metrics.metrics_available = false;
        return metrics;
    }
    
    std::ifstream file(log_path);
    if (!file.good()) {
        metrics.metrics_available = false;
        return metrics;
    }
    
    // Calculate time elapsed since last collection for rate calculation
    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - last_collection_time_).count();
    if (elapsed == 0) elapsed = 1;  // Avoid division by zero
    
    security_event_counts new_counts;
    std::vector<security_event> events;
    std::string line;
    std::smatch match;
    
    // Read new entries from log
    // Note: For production, we would track file position and handle rotation
    // For now, we parse the entire file each time (simple but not efficient)
    while (std::getline(file, line)) {
        security_event event;
        event.timestamp = now;  // Approximate; would parse timestamp in production
        
        if (std::regex_search(line, match, ACCEPTED_PASSWORD_REGEX)) {
            event.type = security_event_type::login_success;
            event.username = mask_username(match[1].str());
            event.source = match[2].str();
            event.success = true;
            new_counts.increment(security_event_type::login_success);
        } else if (std::regex_search(line, match, FAILED_PASSWORD_REGEX)) {
            event.type = security_event_type::login_failure;
            event.username = mask_username(match[1].str());
            event.source = match[2].str();
            event.success = false;
            new_counts.increment(security_event_type::login_failure);
        } else if (std::regex_search(line, match, SUDO_REGEX)) {
            event.type = security_event_type::sudo_usage;
            event.username = mask_username(match[1].str());
            event.success = true;
            new_counts.increment(security_event_type::sudo_usage);
        } else if (std::regex_search(line, match, SESSION_OPENED_REGEX)) {
            event.type = security_event_type::session_start;
            event.username = mask_username(match[1].str());
            event.success = true;
        } else if (std::regex_search(line, match, SESSION_CLOSED_REGEX)) {
            event.type = security_event_type::session_end;
            event.username = mask_username(match[1].str());
            event.success = true;
        } else if (std::regex_search(line, match, USERADD_REGEX)) {
            event.type = security_event_type::account_created;
            event.username = mask_username(match[1].str());
            event.success = true;
            new_counts.increment(security_event_type::account_created);
        } else if (std::regex_search(line, match, USERDEL_REGEX)) {
            event.type = security_event_type::account_deleted;
            event.username = mask_username(match[1].str());
            event.success = true;
            new_counts.increment(security_event_type::account_deleted);
        } else {
            continue;  // Skip lines that don't match any pattern
        }
        
        event.message = line.substr(0, 200);  // Truncate message
        
        // Keep only recent events
        if (events.size() < max_recent_events_) {
            events.push_back(event);
        }
    }
    
    // Update metrics
    metrics.event_counts = new_counts;
    metrics.recent_events = std::move(events);
    metrics.events_per_second = static_cast<double>(new_counts.total()) / elapsed;
    metrics.metrics_available = true;
    
    // Update state for next collection
    last_collection_time_ = now;
    cumulative_counts_ = new_counts;
    
    return metrics;
}

security_metrics security_info_collector::collect_metrics() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check availability without calling is_security_monitoring_available 
    // to avoid deadlock (we already hold the lock)
    if (!availability_checked_) {
        available_ = check_availability_impl();
        availability_checked_ = true;
    }
    
    if (!available_) {
        security_metrics metrics;
        metrics.metrics_available = false;
        metrics.timestamp = std::chrono::system_clock::now();
        return metrics;
    }
    return collect_metrics_impl();
}

}  // namespace monitoring
}  // namespace kcenon

#endif  // __linux__
