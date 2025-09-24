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

// Optional: integrate with logger_system when available
#ifdef BUILD_WITH_LOGGER_SYSTEM
#  include <kcenon/logger/core/logger.h>
#endif

namespace monitoring_system {

class logger_system_adapter {
public:
    explicit logger_system_adapter(std::shared_ptr<event_bus> bus)
        : bus_(std::move(bus)) {}

    bool is_logger_system_available() const {
#ifdef BUILD_WITH_LOGGER_SYSTEM
        return true;
#else
        return false;
#endif
    }

    // One‑shot collection. Empty when logger_system is not integrated.
    result<std::vector<metric>> collect_metrics() {
        std::vector<metric> out;
#ifdef BUILD_WITH_LOGGER_SYSTEM
        // If needed, convert logger metrics to monitoring metrics here.
#endif
        return make_success(std::move(out));
    }

    // Register a logger instance by name (no‑op in fallback mode)
    result_void register_logger(const std::string& /*name*/) {
        return result_void::success();
    }

    // Convenience method for examples/tests; returns 0.0 when unavailable
    double get_current_log_rate() const { return 0.0; }

private:
    std::shared_ptr<event_bus> bus_;
};

} // namespace monitoring_system

#endif // KCENON_MONITORING_ADAPTERS_LOGGER_SYSTEM_ADAPTER_H

