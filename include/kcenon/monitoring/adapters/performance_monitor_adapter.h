#pragma once

/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, monitoring_system contributors
All rights reserved.
*****************************************************************************/

/**
 * @file performance_monitor_adapter.h
 * @brief Adapter to bridge performance_monitor to common::interfaces::IMonitor
 *
 * This adapter resolves the multiple inheritance issue by using composition
 * instead of inheritance. The performance_monitor class focuses on its core
 * metrics collection functionality, while this adapter provides the IMonitor
 * interface for interoperability with common_system.
 */

#include "../core/performance_monitor.h"
#include <kcenon/common/interfaces/monitoring_interface.h>
#include <memory>
#include <string>

namespace kcenon { namespace monitoring {

/**
 * @class performance_monitor_adapter
 * @brief Adapter that wraps performance_monitor to implement IMonitor interface
 *
 * This adapter follows the Adapter design pattern to eliminate multiple inheritance.
 * It provides a clean separation between the monitoring system's internal interface
 * (metrics_collector) and the common system's standard interface (IMonitor).
 *
 * @thread_safety Thread-safe if the wrapped performance_monitor is thread-safe
 *
 * Example usage:
 * @code
 * auto monitor = std::make_shared<performance_monitor>("my_monitor");
 * auto adapter = std::make_shared<performance_monitor_adapter>(monitor);
 *
 * // Use through IMonitor interface
 * adapter->record_metric("requests_count", 42.0);
 * auto snapshot = adapter->get_metrics();
 * @endcode
 */
class performance_monitor_adapter : public common::interfaces::IMonitor {
public:
    /**
     * @brief Construct adapter with existing performance_monitor
     * @param monitor Shared pointer to the performance_monitor to wrap
     * @throws std::invalid_argument if monitor is nullptr
     */
    explicit performance_monitor_adapter(std::shared_ptr<performance_monitor> monitor)
        : monitor_(std::move(monitor)) {
        if (!monitor_) {
            throw std::invalid_argument("performance_monitor cannot be null");
        }
    }

    /**
     * @brief Record a simple metric value
     * @param name Metric name
     * @param value Metric value
     * @return VoidResult indicating success or error
     * @override
     *
     * @note performance_monitor is designed for operation timing, not arbitrary metrics.
     * This method is a no-op for compatibility with IMonitor interface.
     * Use start_operation() / scoped_timer for actual performance monitoring.
     */
    common::VoidResult record_metric(const std::string& name, double value) override {
        // performance_monitor is specifically designed for timing operations,
        // not recording arbitrary metric values. For IMonitor compatibility,
        // we accept the call but don't store it.
        (void)name;   // Suppress unused parameter warning
        (void)value;
        return common::ok();
    }

    /**
     * @brief Record a metric with additional tags
     * @param name Metric name
     * @param value Metric value
     * @param tags Metadata tags for the metric
     * @return VoidResult indicating success or error
     * @override
     */
    common::VoidResult record_metric(
        const std::string& name,
        double value,
        const std::unordered_map<std::string, std::string>& tags) override {
        // Note: performance_monitor doesn't currently support tags in record_value
        // For now, we record the metric without tags
        // TODO: Extend performance_monitor to support tags if needed
        (void)tags; // Suppress unused parameter warning
        return record_metric(name, value);
    }

    /**
     * @brief Get current metrics snapshot
     * @return Result containing metrics snapshot compatible with common::interfaces
     * @override
     */
    common::Result<common::interfaces::metrics_snapshot> get_metrics() override {
        try {
            // Get all recorded metrics from the performance_monitor
            common::interfaces::metrics_snapshot snapshot;
            snapshot.source_id = monitor_->get_name();
            snapshot.capture_time = std::chrono::system_clock::now();

            // Get performance metrics from the monitor
            const auto& perf_metrics = monitor_->get_all_metrics();

            // Convert each performance_metrics (operation timing) into multiple metric_value entries
            for (const auto& perf : perf_metrics) {
                // Add various duration metrics as gauges
                snapshot.add_metric(perf.operation_name + "_min_ns",
                                  static_cast<double>(perf.min_duration.count()));
                snapshot.add_metric(perf.operation_name + "_max_ns",
                                  static_cast<double>(perf.max_duration.count()));
                snapshot.add_metric(perf.operation_name + "_mean_ns",
                                  static_cast<double>(perf.mean_duration.count()));
                snapshot.add_metric(perf.operation_name + "_median_ns",
                                  static_cast<double>(perf.median_duration.count()));
                snapshot.add_metric(perf.operation_name + "_p95_ns",
                                  static_cast<double>(perf.p95_duration.count()));
                snapshot.add_metric(perf.operation_name + "_p99_ns",
                                  static_cast<double>(perf.p99_duration.count()));

                // Add count metrics as counters (conceptually)
                snapshot.add_metric(perf.operation_name + "_call_count",
                                  static_cast<double>(perf.call_count));
                snapshot.add_metric(perf.operation_name + "_error_count",
                                  static_cast<double>(perf.error_count));
            }

            return common::ok(std::move(snapshot));
        } catch (const std::exception& e) {
            return common::Result<common::interfaces::metrics_snapshot>(
                common::error_info{3, std::string("Exception in get_metrics: ") + e.what(), "performance_monitor_adapter"}
            );
        }
    }

    /**
     * @brief Perform health check on the performance_monitor
     * @return Result containing health check result
     * @override
     */
    common::Result<common::interfaces::health_check_result> check_health() override {
        try {
            common::interfaces::health_check_result result;
            result.timestamp = std::chrono::system_clock::now();

            if (monitor_->is_enabled()) {
                result.status = common::interfaces::health_status::healthy;
                result.message = "Performance monitor is operational";
            } else {
                result.status = common::interfaces::health_status::degraded;
                result.message = "Performance monitor is disabled";
            }

            return common::ok(std::move(result));
        } catch (const std::exception& e) {
            common::interfaces::health_check_result result;
            result.status = common::interfaces::health_status::unhealthy;
            result.message = std::string("Health check failed: ") + e.what();
            result.timestamp = std::chrono::system_clock::now();
            return common::ok(std::move(result));
        }
    }

    /**
     * @brief Reset all metrics in the wrapped performance_monitor
     * @return VoidResult indicating success or error
     * @override
     */
    common::VoidResult reset() override {
        try {
            monitor_->reset();
            return common::ok();
        } catch (const std::exception& e) {
            return common::VoidResult(
                common::error_info{4, std::string("Failed to reset monitor: ") + e.what(), "performance_monitor_adapter"}
            );
        }
    }

    /**
     * @brief Get the wrapped performance_monitor
     * @return Shared pointer to the wrapped monitor
     */
    std::shared_ptr<performance_monitor> get_wrapped_monitor() const {
        return monitor_;
    }

private:
    std::shared_ptr<performance_monitor> monitor_;
};

/**
 * @brief Factory function to create performance_monitor_adapter
 * @param monitor The performance_monitor to wrap
 * @return Shared pointer to the adapter implementing IMonitor
 */
inline std::shared_ptr<common::interfaces::IMonitor> make_monitor_adapter(
    std::shared_ptr<performance_monitor> monitor) {
    return std::make_shared<performance_monitor_adapter>(std::move(monitor));
}

} } // namespace kcenon::monitoring
