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
 * @file macos_security_metrics.cpp
 * @brief macOS implementation of security event monitoring
 *
 * Uses the unified logging system to query security events.
 */

#include <kcenon/monitoring/collectors/security_collector.h>

#ifdef __APPLE__

#include <array>
#include <cstdio>
#include <memory>
#include <regex>
#include <sstream>
#include <string>

namespace kcenon {
namespace monitoring {

namespace {

// Execute a command and capture output
std::string exec_command(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        return "";
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

// Check if 'log' command is available
bool is_log_command_available() {
    std::string result = exec_command("which log 2>/dev/null");
    return !result.empty();
}

// Regex patterns for macOS log output
const std::regex LOGIN_SUCCESS_REGEX(
    R"(loginwindow.*Login.*succeeded|sshd.*Accepted)");
const std::regex LOGIN_FAILURE_REGEX(
    R"(loginwindow.*Login.*failed|sshd.*Failed)");
const std::regex SUDO_REGEX(
    R"(sudo.*:.*COMMAND)");
const std::regex AUTH_REGEX(
    R"(authorizationhost|securityd|Authorization)");

}  // namespace

security_info_collector::security_info_collector()
    : last_collection_time_(std::chrono::system_clock::now()) {
}

security_info_collector::~security_info_collector() = default;

bool security_info_collector::check_availability_impl() const {
    // macOS log show command can be very slow and block tests
    // Return false for now to use stub implementation
    // TODO: Implement async log querying or use alternative method
    return false;
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
    
    // Calculate time elapsed since last collection
    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - last_collection_time_).count();
    if (elapsed == 0) elapsed = 1;
    
    // Query unified logging for recent security events
    // Using --last 10s for a short time window to avoid long queries
    // Adding timeout to prevent command from hanging
    std::string cmd = 
        "timeout 2 log show --last 10s --predicate "
        "'process == \"sshd\" OR process == \"sudo\"' "
        "--style compact 2>/dev/null | head -50";
    
    std::string output = exec_command(cmd.c_str());
    
    security_event_counts new_counts;
    std::vector<security_event> events;
    std::istringstream stream(output);
    std::string line;
    std::smatch match;
    
    while (std::getline(stream, line)) {
        security_event event;
        event.timestamp = now;
        
        if (std::regex_search(line, LOGIN_SUCCESS_REGEX)) {
            event.type = security_event_type::login_success;
            event.success = true;
            new_counts.increment(security_event_type::login_success);
        } else if (std::regex_search(line, LOGIN_FAILURE_REGEX)) {
            event.type = security_event_type::login_failure;
            event.success = false;
            new_counts.increment(security_event_type::login_failure);
        } else if (std::regex_search(line, SUDO_REGEX)) {
            event.type = security_event_type::sudo_usage;
            event.success = true;
            new_counts.increment(security_event_type::sudo_usage);
        } else if (std::regex_search(line, AUTH_REGEX)) {
            // Generic authorization event - count but don't categorize
            continue;
        } else {
            continue;
        }
        
        event.message = line.substr(0, 200);
        
        if (events.size() < max_recent_events_) {
            events.push_back(event);
        }
    }
    
    metrics.event_counts = new_counts;
    metrics.recent_events = std::move(events);
    metrics.events_per_second = static_cast<double>(new_counts.total()) / elapsed;
    metrics.metrics_available = true;
    
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

#endif  // __APPLE__
