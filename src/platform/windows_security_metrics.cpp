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
 * @file windows_security_metrics.cpp
 * @brief Windows stub implementation of security event monitoring
 *
 * Returns unavailable metrics on Windows.
 * Future implementation would use Windows Event Log API.
 */

#include <kcenon/monitoring/collectors/security_collector.h>

#ifdef _WIN32

namespace kcenon {
namespace monitoring {

security_info_collector::security_info_collector()
    : last_collection_time_(std::chrono::system_clock::now()) {
}

security_info_collector::~security_info_collector() = default;

bool security_info_collector::check_availability_impl() const {
    // Windows implementation not yet available
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
    metrics.metrics_available = false;
    metrics.timestamp = std::chrono::system_clock::now();
    return metrics;
}

security_metrics security_info_collector::collect_metrics() {
    std::lock_guard<std::mutex> lock(mutex_);
    return collect_metrics_impl();
}

}  // namespace monitoring
}  // namespace kcenon

#endif  // _WIN32
