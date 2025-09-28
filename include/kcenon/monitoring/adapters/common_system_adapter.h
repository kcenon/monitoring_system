#pragma once

/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, monitoring_system contributors
All rights reserved.
*****************************************************************************/

#include <memory>
#include <string>
#include <vector>
#include <chrono>

// Check if common_system is available
#ifdef USE_COMMON_SYSTEM
#include <kcenon/common/interfaces/monitoring_interface.h>
#include <kcenon/common/patterns/result.h>
#endif

#include "../interfaces/monitoring_interface.h"
#include "../interfaces/monitorable_interface.h"
#include "../core/result_types.h"

namespace monitoring_system {
namespace adapters {

#ifdef USE_COMMON_SYSTEM

/**
 * @brief Adapter to expose monitoring_system as common::interfaces::IMonitor
 *
 * This adapter allows monitoring_system's monitor to be used through
 * the standard common_system monitoring interface.
 */
class common_system_monitor_adapter : public ::common::interfaces::IMonitor {
public:
    /**
     * @brief Construct adapter with monitoring_system monitor
     * @param monitor Shared pointer to monitoring_system monitor
     */
    explicit common_system_monitor_adapter(
        std::shared_ptr<monitoring_interface> monitor)
        : monitor_(monitor) {}

    ~common_system_monitor_adapter() override = default;

    /**
     * @brief Record a metric value
     */
    ::common::VoidResult record_metric(
        const std::string& name,
        double value) override {
        if (!monitor_) {
            return ::common::VoidResult(
                ::common::error_info(1, "Monitor not initialized", "monitoring_system"));
        }

        // Create metric value and add to current collection
        metric_value metric(name, value);

        // monitoring_system doesn't have direct record_metric,
        // store locally and include in next collect
        pending_metrics_.push_back(metric);

        return ::common::VoidResult(std::monostate{});
    }

    /**
     * @brief Record a metric with tags
     */
    ::common::VoidResult record_metric(
        const std::string& name,
        double value,
        const std::unordered_map<std::string, std::string>& tags) override {
        if (!monitor_) {
            return ::common::VoidResult(
                ::common::error_info(1, "Monitor not initialized", "monitoring_system"));
        }

        metric_value metric(name, value);
        metric.tags = tags;
        pending_metrics_.push_back(metric);

        return ::common::VoidResult(std::monostate{});
    }

    /**
     * @brief Get current metrics snapshot
     */
    ::common::Result<::common::interfaces::metrics_snapshot> get_metrics() override {
        if (!monitor_) {
            return ::common::error_info(1, "Monitor not initialized", "monitoring_system");
        }

        auto result = monitor_->collect_now();
        if (!result) {
            return ::common::error_info(2, "Failed to collect metrics", "monitoring_system");
        }

        // Convert monitoring_system snapshot to common snapshot
        ::common::interfaces::metrics_snapshot snapshot;
        snapshot.capture_time = result.value().capture_time;
        snapshot.source_id = result.value().source_id;

        // Add collected metrics
        for (const auto& m : result.value().metrics) {
            snapshot.metrics.emplace_back(m.name, m.value);
        }

        // Add pending metrics
        for (const auto& m : pending_metrics_) {
            snapshot.metrics.push_back(m);
        }
        pending_metrics_.clear();

        return snapshot;
    }

    /**
     * @brief Perform health check
     */
    ::common::Result<::common::interfaces::health_check_result> check_health() override {
        if (!monitor_) {
            return ::common::error_info(1, "Monitor not initialized", "monitoring_system");
        }

        auto result = monitor_->check_health();
        if (!result) {
            return ::common::error_info(2, "Health check failed", "monitoring_system");
        }

        // Convert monitoring_system health to common health
        ::common::interfaces::health_check_result health;

        switch(result.value().status) {
            case health_status::healthy:
                health.status = ::common::interfaces::health_status::healthy;
                break;
            case health_status::degraded:
                health.status = ::common::interfaces::health_status::degraded;
                break;
            case health_status::unhealthy:
                health.status = ::common::interfaces::health_status::unhealthy;
                break;
            default:
                health.status = ::common::interfaces::health_status::unknown;
        }

        health.message = result.value().message;
        health.timestamp = result.value().timestamp;
        health.check_duration = result.value().check_duration;
        health.metadata = result.value().metadata;

        return health;
    }

    /**
     * @brief Reset all metrics
     */
    ::common::VoidResult reset() override {
        pending_metrics_.clear();
        // monitoring_system doesn't have direct reset
        return ::common::VoidResult(std::monostate{});
    }

private:
    std::shared_ptr<monitoring_interface> monitor_;
    std::vector<metric_value> pending_metrics_;
};

/**
 * @brief Adapter to use common::interfaces::IMonitor in monitoring_system
 *
 * This adapter allows a common_system monitor to be used as
 * a monitoring_system metric collector.
 */
class monitor_from_common_adapter : public metrics_collector {
public:
    /**
     * @brief Construct adapter with common monitor
     * @param monitor Shared pointer to common monitor
     * @param name Collector name
     */
    explicit monitor_from_common_adapter(
        std::shared_ptr<::common::interfaces::IMonitor> common_monitor,
        const std::string& name = "common_adapter")
        : common_monitor_(common_monitor), name_(name) {}

    ~monitor_from_common_adapter() override = default;

    /**
     * @brief Collect metrics
     */
    result<metrics_snapshot> collect() override {
        if (!common_monitor_) {
            return result<metrics_snapshot>(
                monitoring_error_code::collection_failed,
                "Common monitor not initialized");
        }

        auto common_result = common_monitor_->get_metrics();
        if (::common::is_error(common_result)) {
            return result<metrics_snapshot>(
                monitoring_error_code::collection_failed,
                ::common::get_error(common_result).message);
        }

        const auto& common_snapshot = ::common::get_value(common_result);

        // Convert common snapshot to monitoring_system snapshot
        metrics_snapshot snapshot;
        snapshot.capture_time = common_snapshot.capture_time;
        snapshot.source_id = common_snapshot.source_id;

        for (const auto& m : common_snapshot.metrics) {
            snapshot.add_metric(m.name, m.value);
        }

        return result<metrics_snapshot>(snapshot);
    }

    /**
     * @brief Get collector name
     */
    std::string get_name() const override {
        return name_;
    }

    /**
     * @brief Check if collector is enabled
     */
    bool is_enabled() const override {
        return enabled_;
    }

    /**
     * @brief Enable or disable the collector
     */
    result_void set_enabled(bool enable) override {
        enabled_ = enable;
        return result_void::success();
    }

    /**
     * @brief Initialize the collector
     */
    result_void initialize() override {
        return result_void::success();
    }

    /**
     * @brief Cleanup collector resources
     */
    result_void cleanup() override {
        if (common_monitor_) {
            common_monitor_->reset();
        }
        return result_void::success();
    }

private:
    std::shared_ptr<::common::interfaces::IMonitor> common_monitor_;
    std::string name_;
    bool enabled_ = true;
};

/**
 * @brief Adapter to expose monitorable as common::interfaces::IMonitorable
 */
class common_system_monitorable_adapter : public ::common::interfaces::IMonitorable {
public:
    /**
     * @brief Construct adapter with monitoring_system monitorable
     * @param monitorable Shared pointer to monitorable
     * @param name Component name
     */
    explicit common_system_monitorable_adapter(
        std::shared_ptr<monitorable_interface> monitorable,
        const std::string& name = "monitoring_component")
        : monitorable_(monitorable), component_name_(name) {}

    ~common_system_monitorable_adapter() override = default;

    /**
     * @brief Get monitoring data
     */
    ::common::Result<::common::interfaces::metrics_snapshot> get_monitoring_data() override {
        // monitoring_system monitorable doesn't have get_monitoring_data
        // Return empty snapshot
        ::common::interfaces::metrics_snapshot snapshot;
        snapshot.source_id = component_name_;
        return snapshot;
    }

    /**
     * @brief Check health status
     */
    ::common::Result<::common::interfaces::health_check_result> health_check() override {
        // monitoring_system monitorable doesn't have health_check
        // Return healthy status
        ::common::interfaces::health_check_result result;
        result.status = ::common::interfaces::health_status::healthy;
        result.message = "Component operational";
        return result;
    }

    /**
     * @brief Get component name for monitoring
     */
    std::string get_component_name() const override {
        return component_name_;
    }

private:
    std::shared_ptr<monitorable_interface> monitorable_;
    std::string component_name_;
};

/**
 * @brief Factory for creating common_system compatible monitors
 */
class common_monitor_factory {
public:
    /**
     * @brief Create a common_system IMonitor from monitoring_system monitor
     */
    static std::shared_ptr<::common::interfaces::IMonitor> create_common_monitor(
        std::shared_ptr<monitoring_interface> monitor) {
        return std::make_shared<common_system_monitor_adapter>(monitor);
    }

    /**
     * @brief Create a monitoring_system collector that wraps common IMonitor
     */
    static std::shared_ptr<metrics_collector> create_from_common(
        std::shared_ptr<::common::interfaces::IMonitor> common_monitor,
        const std::string& name = "common_adapter") {
        return std::make_shared<monitor_from_common_adapter>(common_monitor, name);
    }

    /**
     * @brief Create a common_system IMonitorable from monitoring_system monitorable
     */
    static std::shared_ptr<::common::interfaces::IMonitorable> create_common_monitorable(
        std::shared_ptr<monitorable_interface> monitorable,
        const std::string& name = "monitoring_component") {
        return std::make_shared<common_system_monitorable_adapter>(monitorable, name);
    }
};

#endif // USE_COMMON_SYSTEM

} // namespace adapters
} // namespace monitoring_system