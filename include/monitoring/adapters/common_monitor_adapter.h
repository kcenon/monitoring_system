#pragma once

/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, monitoring_system contributors
All rights reserved.
*****************************************************************************/

#include <memory>
#include <string>
#include <chrono>
#include <unordered_map>
#include <vector>

// Check if common_system is available
#ifdef BUILD_WITH_COMMON_SYSTEM
#include <kcenon/common/interfaces/monitoring_interface.h>
#include <kcenon/common/patterns/result.h>
#endif

#include "../monitor.h"
#include "../metrics_collector.h"
#include "../health_monitor.h"

namespace monitoring {
namespace adapters {

#ifdef BUILD_WITH_COMMON_SYSTEM

/**
 * @brief Convert health status between monitoring_system and common_system
 */
inline ::common::interfaces::health_status to_common_health_status(
    health_status status) {
    switch (status) {
        case health_status::healthy:
            return ::common::interfaces::health_status::healthy;
        case health_status::degraded:
            return ::common::interfaces::health_status::degraded;
        case health_status::unhealthy:
            return ::common::interfaces::health_status::unhealthy;
        case health_status::unknown:
        default:
            return ::common::interfaces::health_status::unknown;
    }
}

inline health_status from_common_health_status(
    ::common::interfaces::health_status status) {
    switch (status) {
        case ::common::interfaces::health_status::healthy:
            return health_status::healthy;
        case ::common::interfaces::health_status::degraded:
            return health_status::degraded;
        case ::common::interfaces::health_status::unhealthy:
            return health_status::unhealthy;
        case ::common::interfaces::health_status::unknown:
        default:
            return health_status::unknown;
    }
}

/**
 * @brief Adapter to expose monitor as common::interfaces::IMonitor
 */
class monitor_adapter : public ::common::interfaces::IMonitor {
public:
    /**
     * @brief Construct adapter with monitoring components
     */
    explicit monitor_adapter(
        std::shared_ptr<monitor> mon,
        std::shared_ptr<metrics_collector> metrics = nullptr,
        std::shared_ptr<health_monitor> health = nullptr)
        : monitor_(mon)
        , metrics_collector_(metrics)
        , health_monitor_(health) {}

    ~monitor_adapter() override = default;

    /**
     * @brief Record a metric value
     */
    ::common::VoidResult record_metric(const std::string& name, double value) override {
        if (!monitor_) {
            return ::common::error_info(1, "Monitor not initialized", "monitoring_system");
        }

        try {
            monitor_->record_metric(name, value);
            return ::common::VoidResult(std::monostate{});
        } catch (const std::exception& e) {
            return ::common::error_info(2, e.what(), "monitoring_system");
        }
    }

    /**
     * @brief Record a metric with tags
     */
    ::common::VoidResult record_metric(
        const std::string& name,
        double value,
        const std::unordered_map<std::string, std::string>& tags) override {
        if (!monitor_) {
            return ::common::error_info(1, "Monitor not initialized", "monitoring_system");
        }

        try {
            // Convert tags to monitoring_system format
            monitor_->record_metric_with_tags(name, value, tags);
            return ::common::VoidResult(std::monostate{});
        } catch (const std::exception& e) {
            return ::common::error_info(2, e.what(), "monitoring_system");
        }
    }

    /**
     * @brief Get current metrics snapshot
     */
    ::common::Result<::common::interfaces::metrics_snapshot> get_metrics() override {
        if (!monitor_) {
            return ::common::error_info(1, "Monitor not initialized", "monitoring_system");
        }

        try {
            ::common::interfaces::metrics_snapshot snapshot;

            if (metrics_collector_) {
                auto metrics = metrics_collector_->collect_all();
                for (const auto& [name, value] : metrics) {
                    snapshot.add_metric(name, value);
                }
            } else {
                auto metrics = monitor_->get_all_metrics();
                for (const auto& metric : metrics) {
                    snapshot.add_metric(metric.name, metric.value);
                }
            }

            snapshot.source_id = "monitoring_system";
            return snapshot;
        } catch (const std::exception& e) {
            return ::common::error_info(2, e.what(), "monitoring_system");
        }
    }

    /**
     * @brief Perform health check
     */
    ::common::Result<::common::interfaces::health_check_result> check_health() override {
        if (!monitor_) {
            return ::common::error_info(1, "Monitor not initialized", "monitoring_system");
        }

        try {
            ::common::interfaces::health_check_result result;

            if (health_monitor_) {
                auto health = health_monitor_->check_health();
                result.status = to_common_health_status(health.status);
                result.message = health.message;
                result.check_duration = health.duration;
                result.metadata = health.metadata;
            } else {
                // Simple health check based on monitor state
                result.status = ::common::interfaces::health_status::healthy;
                result.message = "Monitor is operational";
            }

            return result;
        } catch (const std::exception& e) {
            return ::common::error_info(2, e.what(), "monitoring_system");
        }
    }

    /**
     * @brief Reset all metrics
     */
    ::common::VoidResult reset() override {
        if (!monitor_) {
            return ::common::error_info(1, "Monitor not initialized", "monitoring_system");
        }

        try {
            monitor_->reset();
            if (metrics_collector_) {
                metrics_collector_->reset();
            }
            return ::common::VoidResult(std::monostate{});
        } catch (const std::exception& e) {
            return ::common::error_info(2, e.what(), "monitoring_system");
        }
    }

private:
    std::shared_ptr<monitor> monitor_;
    std::shared_ptr<metrics_collector> metrics_collector_;
    std::shared_ptr<health_monitor> health_monitor_;
};

/**
 * @brief Adapter to use common::interfaces::IMonitor in monitoring_system
 */
class monitor_from_common_adapter {
public:
    /**
     * @brief Construct adapter with common monitor
     */
    explicit monitor_from_common_adapter(
        std::shared_ptr<::common::interfaces::IMonitor> common_monitor)
        : common_monitor_(common_monitor) {}

    /**
     * @brief Record a metric
     */
    void record_metric(const std::string& name, double value) {
        if (!common_monitor_) {
            return;
        }

        auto result = common_monitor_->record_metric(name, value);
        // Silently ignore errors for now
    }

    /**
     * @brief Record a metric with tags
     */
    void record_metric_with_tags(
        const std::string& name,
        double value,
        const std::unordered_map<std::string, std::string>& tags) {
        if (!common_monitor_) {
            return;
        }

        auto result = common_monitor_->record_metric(name, value, tags);
        // Silently ignore errors for now
    }

    /**
     * @brief Get metrics snapshot
     */
    std::vector<metric_data> get_all_metrics() {
        std::vector<metric_data> metrics;
        if (!common_monitor_) {
            return metrics;
        }

        auto result = common_monitor_->get_metrics();
        if (!::common::is_error(result)) {
            const auto& snapshot = ::common::get_value(result);
            for (const auto& metric : snapshot.metrics) {
                metric_data data;
                data.name = metric.name;
                data.value = metric.value;
                data.timestamp = metric.timestamp;
                data.tags = metric.tags;
                metrics.push_back(data);
            }
        }

        return metrics;
    }

    /**
     * @brief Check health
     */
    health_check_result check_health() {
        health_check_result health_result;
        if (!common_monitor_) {
            health_result.status = health_status::unknown;
            health_result.message = "Monitor not initialized";
            return health_result;
        }

        auto result = common_monitor_->check_health();
        if (!::common::is_error(result)) {
            const auto& check = ::common::get_value(result);
            health_result.status = from_common_health_status(check.status);
            health_result.message = check.message;
            health_result.duration = check.check_duration;
            health_result.metadata = check.metadata;
        } else {
            health_result.status = health_status::unknown;
            health_result.message = "Health check failed";
        }

        return health_result;
    }

    /**
     * @brief Reset metrics
     */
    void reset() {
        if (common_monitor_) {
            common_monitor_->reset();
        }
    }

private:
    std::shared_ptr<::common::interfaces::IMonitor> common_monitor_;
};

/**
 * @brief Adapter for IMonitorable interface
 */
class monitorable_adapter : public ::common::interfaces::IMonitorable {
public:
    /**
     * @brief Construct adapter with monitoring components
     */
    explicit monitorable_adapter(
        const std::string& component_name,
        std::shared_ptr<monitor> mon,
        std::shared_ptr<health_monitor> health = nullptr)
        : component_name_(component_name)
        , monitor_(mon)
        , health_monitor_(health) {}

    ~monitorable_adapter() override = default;

    /**
     * @brief Get monitoring data
     */
    ::common::Result<::common::interfaces::metrics_snapshot> get_monitoring_data() override {
        if (!monitor_) {
            return ::common::error_info(1, "Monitor not initialized", "monitoring_system");
        }

        try {
            ::common::interfaces::metrics_snapshot snapshot;
            snapshot.source_id = component_name_;

            auto metrics = monitor_->get_all_metrics();
            for (const auto& metric : metrics) {
                snapshot.add_metric(metric.name, metric.value);
            }

            return snapshot;
        } catch (const std::exception& e) {
            return ::common::error_info(2, e.what(), "monitoring_system");
        }
    }

    /**
     * @brief Check health status
     */
    ::common::Result<::common::interfaces::health_check_result> health_check() override {
        try {
            ::common::interfaces::health_check_result result;

            if (health_monitor_) {
                auto health = health_monitor_->check_health();
                result.status = to_common_health_status(health.status);
                result.message = health.message;
                result.check_duration = health.duration;
                result.metadata = health.metadata;
            } else {
                result.status = ::common::interfaces::health_status::healthy;
                result.message = component_name_ + " is operational";
            }

            return result;
        } catch (const std::exception& e) {
            return ::common::error_info(2, e.what(), "monitoring_system");
        }
    }

    /**
     * @brief Get component name
     */
    std::string get_component_name() const override {
        return component_name_;
    }

private:
    std::string component_name_;
    std::shared_ptr<monitor> monitor_;
    std::shared_ptr<health_monitor> health_monitor_;
};

/**
 * @brief Factory for creating common_system compatible monitors
 */
class common_monitor_factory {
public:
    /**
     * @brief Create a common_system IMonitor from monitoring_system components
     */
    static std::shared_ptr<::common::interfaces::IMonitor> create_from_monitor(
        std::shared_ptr<monitor> mon,
        std::shared_ptr<metrics_collector> metrics = nullptr,
        std::shared_ptr<health_monitor> health = nullptr) {
        return std::make_shared<monitor_adapter>(mon, metrics, health);
    }

    /**
     * @brief Create a monitoring_system wrapper from common IMonitor
     */
    static std::unique_ptr<monitor_from_common_adapter> create_from_common(
        std::shared_ptr<::common::interfaces::IMonitor> common_monitor) {
        return std::make_unique<monitor_from_common_adapter>(common_monitor);
    }

    /**
     * @brief Create a common_system IMonitorable from monitoring_system components
     */
    static std::shared_ptr<::common::interfaces::IMonitorable> create_monitorable(
        const std::string& name,
        std::shared_ptr<monitor> mon,
        std::shared_ptr<health_monitor> health = nullptr) {
        return std::make_shared<monitorable_adapter>(name, mon, health);
    }
};

#endif // BUILD_WITH_COMMON_SYSTEM

} // namespace adapters
} // namespace monitoring