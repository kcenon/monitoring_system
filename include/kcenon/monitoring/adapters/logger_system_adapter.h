#pragma once

#ifndef KCENON_MONITORING_ADAPTERS_LOGGER_SYSTEM_ADAPTER_H
#define KCENON_MONITORING_ADAPTERS_LOGGER_SYSTEM_ADAPTER_H

/*
 * BSD 3-Clause License
 * Copyright (c) 2025, monitoring_system contributors
 */

// Module description:
// Logger system adapter for monitoring_system (Phase 2.3.3).
// Uses common_system interfaces for logger integration via dependency injection.
// No direct dependency on logger_system - works with any ILogger implementation.

#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "../core/event_bus.h"
#include "../core/event_types.h"
#include "../core/result_types.h"
#include "../interfaces/metric_types_adapter.h"

// Use common_system interfaces for logger integration (Phase 2.3.3)
#include <kcenon/common/interfaces/logger_interface.h>
#include <kcenon/common/interfaces/monitoring_interface.h>

namespace kcenon { namespace monitoring {

/**
 * @brief Logger system adapter using dependency injection (Phase 2.3.3)
 *
 * This adapter uses common::interfaces::ILogger instead of concrete logger_system
 * classes, removing compile-time dependency on logger_system. Works with any
 * ILogger implementation through dependency injection.
 */
class logger_system_adapter {
public:
    /**
     * @brief Constructor with optional logger injection
     * @param bus Event bus for monitoring events
     * @param logger Optional logger instance (any ILogger implementation)
     */
    explicit logger_system_adapter(
        std::shared_ptr<event_bus> bus,
        std::shared_ptr<common::interfaces::ILogger> logger = nullptr
    ) : bus_(std::move(bus)), logger_(std::move(logger)) {}

    /**
     * @brief Check if logger is available
     */
    bool is_logger_system_available() const {
        return logger_ != nullptr;
    }

    /**
     * @brief Set or replace the logger instance
     * @param logger New logger instance
     */
    void set_logger(std::shared_ptr<common::interfaces::ILogger> logger) {
        logger_ = std::move(logger);
    }

    /**
     * @brief Get the current logger instance
     */
    std::shared_ptr<common::interfaces::ILogger> get_logger() const {
        return logger_;
    }

    /**
     * @brief Collect metrics from logger if available (Phase 2.3.3)
     */
    result<std::vector<metric>> collect_metrics() {
        std::vector<metric> out;
        if (logger_) {
            // If logger implements IMonitorable, collect its metrics
            auto monitorable = std::dynamic_pointer_cast<common::interfaces::IMonitorable>(logger_);
            if (monitorable) {
                auto metrics_result = monitorable->get_monitoring_data();
                if (metrics_result.is_ok()) {
                    const auto& snapshot = metrics_result.value();
                    // Convert to monitoring_system metric format
                    for (const auto& m : snapshot.metrics) {
                        metric converted;
                        converted.name = m.name;
                        converted.value = m.value;
                        converted.timestamp = m.timestamp;
                        out.push_back(converted);
                    }
                }
            }
        }
        return make_success(std::move(out));
    }

    /**
     * @brief Register a logger instance by name
     */
    result_void register_logger(const std::string& /*name*/) {
        // Logger is now provided via DI, not registered by name
        return make_void_success();
    }

    /**
     * @brief Get current log rate (if logger supports metrics)
     */
    double get_current_log_rate() const {
        if (logger_) {
            auto monitorable = std::dynamic_pointer_cast<common::interfaces::IMonitorable>(logger_);
            if (monitorable) {
                auto metrics_result = monitorable->get_monitoring_data();
                if (metrics_result.is_ok()) {
                    const auto& snapshot = metrics_result.value();
                    for (const auto& m : snapshot.metrics) {
                        if (m.name.find("messages_logged") != std::string::npos ||
                            m.name.find("log_rate") != std::string::npos) {
                            return m.value;
                        }
                    }
                }
            }
        }
        return 0.0;
    }

private:
    std::shared_ptr<event_bus> bus_;
    std::shared_ptr<common::interfaces::ILogger> logger_;  // Phase 2.3.3
};

} } // namespace kcenon::monitoring

#endif // KCENON_MONITORING_ADAPTERS_LOGGER_SYSTEM_ADAPTER_H

