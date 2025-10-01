#pragma once

#ifndef KCENON_MONITORING_ADAPTERS_LOGGER_SYSTEM_ADAPTER_H
#define KCENON_MONITORING_ADAPTERS_LOGGER_SYSTEM_ADAPTER_H

/*
 * BSD 3-Clause License
 * Copyright (c) 2025, monitoring_system contributors
 */

// Module description:
// Logger system adapter for monitoring_system. Provides a graceful fallback
// implementation when logger_system is not present so unit tests and examples
// can compile and run without optional dependencies.

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

// Use common_system interfaces for logger integration
#ifdef BUILD_WITH_COMMON_SYSTEM
#  include <kcenon/common/interfaces/logger_interface.h>
#  include <kcenon/common/interfaces/monitoring_interface.h>
#endif

namespace monitoring_system {

/**
 * @brief Logger system adapter using dependency injection
 *
 * This adapter uses common::interfaces::ILogger instead of concrete logger_system
 * classes, removing compile-time dependency on logger_system.
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
#ifdef BUILD_WITH_COMMON_SYSTEM
        std::shared_ptr<common::interfaces::ILogger> logger = nullptr
#else
        std::nullptr_t logger = nullptr
#endif
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
#ifdef BUILD_WITH_COMMON_SYSTEM
    void set_logger(std::shared_ptr<common::interfaces::ILogger> logger) {
        logger_ = std::move(logger);
    }

    /**
     * @brief Get the current logger instance
     */
    std::shared_ptr<common::interfaces::ILogger> get_logger() const {
        return logger_;
    }
#endif

    /**
     * @brief Collect metrics from logger if available
     */
    result<std::vector<metric>> collect_metrics() {
        std::vector<metric> out;
#ifdef BUILD_WITH_COMMON_SYSTEM
        if (logger_) {
            // If logger implements IMonitorable, collect its metrics
            auto monitorable = std::dynamic_pointer_cast<common::interfaces::IMonitorable>(logger_);
            if (monitorable) {
                auto metrics_result = monitorable->get_monitoring_data();
                if (std::holds_alternative<common::interfaces::metrics_snapshot>(metrics_result)) {
                    const auto& snapshot = std::get<common::interfaces::metrics_snapshot>(metrics_result);
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
#endif
        return make_success(std::move(out));
    }

    /**
     * @brief Register a logger instance by name
     */
    result_void register_logger(const std::string& /*name*/) {
        // Logger is now provided via DI, not registered by name
        return result_void::success();
    }

    /**
     * @brief Get current log rate (if logger supports metrics)
     */
    double get_current_log_rate() const {
#ifdef BUILD_WITH_COMMON_SYSTEM
        if (logger_) {
            auto monitorable = std::dynamic_pointer_cast<common::interfaces::IMonitorable>(logger_);
            if (monitorable) {
                auto metrics_result = monitorable->get_monitoring_data();
                if (std::holds_alternative<common::interfaces::metrics_snapshot>(metrics_result)) {
                    const auto& snapshot = std::get<common::interfaces::metrics_snapshot>(metrics_result);
                    for (const auto& m : snapshot.metrics) {
                        if (m.name.find("messages_logged") != std::string::npos ||
                            m.name.find("log_rate") != std::string::npos) {
                            return m.value;
                        }
                    }
                }
            }
        }
#endif
        return 0.0;
    }

private:
    std::shared_ptr<event_bus> bus_;
#ifdef BUILD_WITH_COMMON_SYSTEM
    std::shared_ptr<common::interfaces::ILogger> logger_;
#else
    std::nullptr_t logger_ = nullptr;
#endif
};

} // namespace monitoring_system

#endif // KCENON_MONITORING_ADAPTERS_LOGGER_SYSTEM_ADAPTER_H

