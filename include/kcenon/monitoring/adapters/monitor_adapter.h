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

#include <kcenon/monitoring/core/performance_monitor.h>
#include <memory>
#include <chrono>

// For thread_system v3.0+ integration, use common_monitor_adapter.h which provides
// adapters for kcenon::common::interfaces::IMonitor and IMonitorable

namespace kcenon::monitoring::adapters {

/**
 * @brief Standalone adapter for performance_monitor
 * @note For thread_system integration, use common_monitor_adapter.h which provides
 *       adapters for kcenon::common::interfaces::IMonitor and IMonitorable
 *       (the unified interfaces used by thread_system v3.0+)
 */
class performance_monitor_adapter {
public:
    /**
     * @brief Constructor with performance monitor instance
     * @param monitor Performance monitor instance
     */
    explicit performance_monitor_adapter(std::shared_ptr<kcenon::monitoring::performance_monitor> monitor)
        : monitor_(std::move(monitor)) {
    }

    /**
     * @brief Default constructor - creates a default monitor
     */
    performance_monitor_adapter() {
        // Create default monitor configuration
        kcenon::monitoring::performance_monitor::config config;
        config.enable_cpu_monitoring = true;
        config.enable_memory_monitoring = true;
        config.sampling_interval_ms = 1000;
        monitor_ = std::make_shared<kcenon::monitoring::performance_monitor>(config);
    }

    /**
     * @brief Initialize the adapter
     * @return true if initialization succeeded
     */
    bool initialize() {
        if (monitor_) {
            monitor_->start();
            is_running_ = true;
            return true;
        }
        return false;
    }

    /**
     * @brief Shutdown the adapter
     */
    void shutdown() {
        if (monitor_) {
            monitor_->stop();
        }
        is_running_ = false;
    }

    /**
     * @brief Check if the adapter is running
     * @return true if running
     */
    bool is_running() const {
        return is_running_ && monitor_ != nullptr;
    }

    /**
     * @brief Get the adapter name
     * @return "MonitorAdapter"
     */
    std::string name() const {
        return "MonitorAdapter";
    }

    /**
     * @brief Get the underlying performance monitor
     * @return Performance monitor instance
     */
    std::shared_ptr<kcenon::monitoring::performance_monitor> get_monitor() const {
        return monitor_;
    }

    /**
     * @brief Set monitoring configuration
     * @param config Monitor configuration
     */
    void set_config(const kcenon::monitoring::performance_monitor::config& config) {
        if (monitor_) {
            // Stop, reconfigure, and restart if running
            bool was_running = is_running_;
            if (was_running) {
                monitor_->stop();
            }

            monitor_ = std::make_shared<kcenon::monitoring::performance_monitor>(config);

            if (was_running) {
                monitor_->start();
            }
        }
    }

    /**
     * @brief Enable or disable metrics collection
     * @param enabled true to enable, false to disable
     */
    void set_metrics_enabled(bool enabled) {
        metrics_enabled_ = enabled;
        if (monitor_) {
            if (enabled) {
                monitor_->start();
            } else {
                monitor_->stop();
            }
        }
    }

private:
    std::shared_ptr<kcenon::monitoring::performance_monitor> monitor_;
    bool metrics_enabled_{true};
    bool is_running_{false};
};

// Legacy alias for backward compatibility
using monitor_adapter = performance_monitor_adapter;

} // namespace kcenon::monitoring::adapters