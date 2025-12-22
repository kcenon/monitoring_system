// BSD 3-Clause License
// Copyright (c) 2025, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file service_registration.h
 * @brief Service container registration for monitoring_system services.
 *
 * This header provides functions to register monitoring_system services
 * with the unified service container from common_system.
 *
 * @see TICKET-103 for integration requirements.
 */

#pragma once

#include <memory>
#include "../config/feature_flags.h"

#if KCENON_HAS_COMMON_SYSTEM

#include <kcenon/common/di/service_container.h>
#include <kcenon/common/interfaces/monitoring_interface.h>

#include "../core/performance_monitor.h"
#include "../adapters/performance_monitor_adapter.h"

namespace kcenon::monitoring::di {

/**
 * @brief Default configuration for monitor service registration
 */
struct monitor_registration_config {
    /// Name for the performance monitor instance
    std::string monitor_name = "default_performance_monitor";

    /// CPU usage threshold percentage for alerts
    double cpu_threshold = 80.0;

    /// Memory usage threshold percentage for alerts
    double memory_threshold = 90.0;

    /// Latency threshold for performance alerts
    std::chrono::milliseconds latency_threshold{1000};

    /// Enable system resource monitoring
    bool enable_system_monitoring = true;

    /// Enable lock-free collection mode
    bool enable_lock_free = false;

    /// Service lifetime (typically singleton for monitors)
    common::di::service_lifetime lifetime = common::di::service_lifetime::singleton;
};

/**
 * @brief Register monitoring services with the service container.
 *
 * Registers IMonitor implementation using monitoring_system's performance_monitor
 * with a sensible default configuration. The monitor is registered as a singleton
 * by default.
 *
 * @param container The service container to register with
 * @param config Optional configuration for the monitor
 * @return VoidResult indicating success or registration error
 *
 * @code
 * auto& container = common::di::service_container::global();
 *
 * // Register with default configuration
 * auto result = register_monitor_services(container);
 *
 * // Or with custom configuration
 * monitor_registration_config config;
 * config.monitor_name = "app_monitor";
 * config.cpu_threshold = 90.0;
 * config.enable_system_monitoring = true;
 * auto result = register_monitor_services(container, config);
 *
 * // Then resolve monitor anywhere in the application
 * auto monitor = container.resolve<common::interfaces::IMonitor>().value();
 * monitor->record_metric("requests_count", 42.0);
 * @endcode
 */
inline common::VoidResult register_monitor_services(
    common::di::IServiceContainer& container,
    const monitor_registration_config& config = {}) {

    // Check if already registered
    if (container.is_registered<common::interfaces::IMonitor>()) {
        return common::make_error<std::monostate>(
            common::di::di_error_codes::already_registered,
            "IMonitor is already registered",
            "monitoring_system::di"
        );
    }

    // Register monitor factory
    return container.register_factory<common::interfaces::IMonitor>(
        [config](common::di::IServiceContainer&) -> std::shared_ptr<common::interfaces::IMonitor> {
            // Create performance_monitor with configuration
            auto monitor = std::make_shared<performance_monitor>(config.monitor_name);

            // Apply thresholds
            monitor->set_cpu_threshold(config.cpu_threshold);
            monitor->set_memory_threshold(config.memory_threshold);
            monitor->set_latency_threshold(config.latency_threshold);

            // Configure lock-free mode if requested
            monitor->get_profiler().set_lock_free_mode(config.enable_lock_free);

            // Initialize system monitoring if enabled
            if (config.enable_system_monitoring) {
                auto init_result = monitor->initialize();
                if (init_result.is_err()) {
                    // Log warning but continue - system monitoring is optional
                    // In production, consider using a logger here
                }
            }

            // Create adapter that implements IMonitor
            return std::make_shared<performance_monitor_adapter>(std::move(monitor));
        },
        config.lifetime
    );
}

/**
 * @brief Register a pre-configured performance_monitor instance.
 *
 * Use this when you have already created a performance_monitor instance and want
 * to register it with the container.
 *
 * @param container The service container to register with
 * @param monitor The performance_monitor instance to register
 * @return VoidResult indicating success or registration error
 *
 * @code
 * // Create monitor manually with custom configuration
 * auto monitor = std::make_shared<performance_monitor>("custom_monitor");
 * monitor->set_cpu_threshold(95.0);
 * monitor->initialize();
 *
 * // Register the instance
 * register_monitor_instance(container, monitor);
 * @endcode
 */
inline common::VoidResult register_monitor_instance(
    common::di::IServiceContainer& container,
    std::shared_ptr<performance_monitor> monitor) {

    if (!monitor) {
        return common::make_error<std::monostate>(
            common::error_codes::INVALID_ARGUMENT,
            "Cannot register null monitor instance",
            "monitoring_system::di"
        );
    }

    auto adapter = std::make_shared<performance_monitor_adapter>(std::move(monitor));

    return container.register_instance<common::interfaces::IMonitor>(adapter);
}

/**
 * @brief Unregister monitor services from the container.
 *
 * @param container The service container to unregister from
 * @return VoidResult indicating success or error
 */
inline common::VoidResult unregister_monitor_services(
    common::di::IServiceContainer& container) {

    return container.unregister<common::interfaces::IMonitor>();
}

/**
 * @brief Get the performance_monitor from an IMonitor resolved from the container.
 *
 * This utility function allows accessing the underlying performance_monitor
 * when needed for advanced operations like accessing the profiler directly.
 *
 * @param monitor The IMonitor instance (should be a performance_monitor_adapter)
 * @return Shared pointer to the underlying performance_monitor, or nullptr if not a performance_monitor_adapter
 *
 * @code
 * auto imonitor = container.resolve<common::interfaces::IMonitor>().value();
 * auto perf_monitor = get_underlying_performance_monitor(imonitor);
 * if (perf_monitor) {
 *     auto timer = perf_monitor->time_operation("my_operation");
 *     // ... do work ...
 * }
 * @endcode
 */
inline std::shared_ptr<performance_monitor> get_underlying_performance_monitor(
    const std::shared_ptr<common::interfaces::IMonitor>& monitor) {

    auto adapter = std::dynamic_pointer_cast<performance_monitor_adapter>(monitor);
    if (adapter) {
        return adapter->get_wrapped_monitor();
    }
    return nullptr;
}

} // namespace kcenon::monitoring::di

#endif // KCENON_HAS_COMMON_SYSTEM
