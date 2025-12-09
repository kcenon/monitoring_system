// BSD 3-Clause License
//
// Copyright (c) 2021-2025, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file monitoring_concepts.h
 * @brief C++20 concepts for monitoring system types.
 *
 * This header provides concepts for validating metric types, collectors,
 * event handlers, and observers used in the monitoring system. These concepts
 * leverage common_system's concept definitions while adding monitoring-specific
 * constraints.
 *
 * Requirements:
 * - C++20 compiler with concepts support
 * - GCC 10+, Clang 10+, MSVC 2022+
 *
 * Thread Safety:
 * - Concepts are evaluated at compile-time only.
 * - No runtime thread-safety considerations apply.
 *
 * @see kcenon/common/concepts/concepts.h for common_system concepts
 */

#pragma once

#include <concepts>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#ifdef BUILD_WITH_COMMON_SYSTEM
#include <kcenon/common/concepts/concepts.h>
#endif

namespace kcenon::monitoring {
namespace concepts {

/**
 * @concept MetricValue
 * @brief A type that can be used as a metric value.
 *
 * Metric values must be arithmetic types (integral or floating-point)
 * that can be used in monitoring calculations.
 *
 * Example usage:
 * @code
 * template<MetricValue V>
 * void record_metric(const std::string& name, V value) {
 *     metrics_.record(name, static_cast<double>(value));
 * }
 * @endcode
 */
template <typename T>
concept MetricValue = std::is_arithmetic_v<T>;

/**
 * @concept MetricType
 * @brief A type that can be used as a metric in the monitoring system.
 *
 * Metrics must be class types that are copy-constructible and
 * provide name and value access.
 *
 * Example usage:
 * @code
 * template<MetricType M>
 * void publish_metric(const M& metric) {
 *     collector_.publish(metric);
 * }
 * @endcode
 */
template <typename T>
concept MetricType =
    std::is_class_v<T> && std::is_copy_constructible_v<T> && requires(const T t) {
        { t.name } -> std::convertible_to<std::string>;
        { t.value } -> std::convertible_to<double>;
    };

/**
 * @concept MetricSourceLike
 * @brief A type that can provide metrics.
 *
 * Metric sources provide current metrics and source identification.
 *
 * Example usage:
 * @code
 * template<MetricSourceLike S>
 * void collect_from(const S& source) {
 *     auto metrics = source.get_current_metrics();
 *     // Process metrics
 * }
 * @endcode
 */
template <typename T>
concept MetricSourceLike = requires(const T t) {
    { t.get_current_metrics() };
    { t.get_source_name() } -> std::convertible_to<std::string>;
    { t.is_healthy() } -> std::convertible_to<bool>;
};

/**
 * @concept MetricCollectorLike
 * @brief A type that can collect metrics from sources.
 *
 * Metric collectors manage metric collection and observer notification.
 *
 * Example usage:
 * @code
 * template<MetricCollectorLike C>
 * void start_monitoring(C& collector) {
 *     collector.start_collection();
 * }
 * @endcode
 */
template <typename T>
concept MetricCollectorLike = requires(T t) {
    { t.collect_metrics() };
    { t.is_collecting() } -> std::convertible_to<bool>;
    { t.get_metric_types() };
};

/**
 * @concept ObserverLike
 * @brief A type that can observe metric updates.
 *
 * Observers receive notifications when metrics are updated.
 *
 * Example usage:
 * @code
 * template<ObserverLike O>
 * void register_observer(std::shared_ptr<O> observer) {
 *     observers_.push_back(observer);
 * }
 * @endcode
 */
template <typename T>
concept ObserverLike = requires(T t) {
    { t.on_metrics_updated(std::declval<std::vector<int>>()) };
};

/**
 * @concept MonitoringEventType
 * @brief A type that can be used as a monitoring event.
 *
 * Monitoring events are class types that are copy-constructible
 * and suitable for event bus communication.
 *
 * Example usage:
 * @code
 * template<MonitoringEventType E>
 * void publish_event(const E& event) {
 *     event_bus_->publish_event(event);
 * }
 * @endcode
 */
template <typename T>
concept MonitoringEventType = std::is_class_v<T> && std::is_copy_constructible_v<T>;

/**
 * @concept MonitoringEventHandler
 * @brief A callable that can handle monitoring events.
 *
 * Event handlers receive events by const reference and return void.
 *
 * Example usage:
 * @code
 * template<MonitoringEventType E, MonitoringEventHandler<E> H>
 * void subscribe(H&& handler) {
 *     event_bus_->subscribe<E>(std::forward<H>(handler));
 * }
 * @endcode
 */
template <typename H, typename E>
concept MonitoringEventHandler =
    std::invocable<H, const E&> && std::is_void_v<std::invoke_result_t<H, const E&>>;

/**
 * @concept MetricFilterPredicate
 * @brief A callable that filters metrics based on criteria.
 *
 * Metric filters receive metrics and return a boolean indicating
 * whether the metric should be processed.
 *
 * Example usage:
 * @code
 * template<MetricType M, MetricFilterPredicate<M> F>
 * auto filter_metrics(const std::vector<M>& metrics, F&& filter) {
 *     std::vector<M> result;
 *     std::copy_if(metrics.begin(), metrics.end(),
 *                  std::back_inserter(result), filter);
 *     return result;
 * }
 * @endcode
 */
template <typename F, typename M>
concept MetricFilterPredicate =
    std::invocable<F, const M&> && std::convertible_to<std::invoke_result_t<F, const M&>, bool>;

/**
 * @concept MetricTransformer
 * @brief A callable that transforms metrics.
 *
 * Metric transformers receive metrics and return transformed metrics.
 *
 * Example usage:
 * @code
 * template<MetricType M, MetricTransformer<M> T>
 * auto transform_metrics(const std::vector<M>& metrics, T&& transformer) {
 *     std::vector<decltype(transformer(metrics[0]))> result;
 *     std::transform(metrics.begin(), metrics.end(),
 *                    std::back_inserter(result), transformer);
 *     return result;
 * }
 * @endcode
 */
template <typename F, typename M>
concept MetricTransformer = std::invocable<F, const M&>;

/**
 * @concept ConfigValidatable
 * @brief A configuration type that supports validation.
 *
 * Validatable configurations provide a validate() method that checks
 * internal consistency and returns a Result indicating success or errors.
 *
 * Example usage:
 * @code
 * template<ConfigValidatable C>
 * auto apply_config(const C& config) {
 *     auto result = config.validate();
 *     if (result.is_err()) {
 *         return result;
 *     }
 *     // Apply configuration
 * }
 * @endcode
 */
template <typename T>
concept ConfigValidatable = requires(const T t) {
    { t.validate() };
};

/**
 * @concept StorageBackendLike
 * @brief A type that can store metrics data.
 *
 * Storage backends provide methods for storing and retrieving metrics.
 *
 * Example usage:
 * @code
 * template<StorageBackendLike S>
 * void persist_metrics(S& storage, const std::vector<metric>& metrics) {
 *     storage.store(metrics);
 * }
 * @endcode
 */
template <typename T>
concept StorageBackendLike = requires(T t) {
    { t.store(std::declval<std::vector<int>>()) };
    { t.is_connected() } -> std::convertible_to<bool>;
};

/**
 * @concept ExporterLike
 * @brief A type that can export metrics to external systems.
 *
 * Exporters provide methods for exporting metrics data.
 *
 * Example usage:
 * @code
 * template<ExporterLike E>
 * void export_metrics(E& exporter, const std::vector<metric>& metrics) {
 *     exporter.export_metrics(metrics);
 * }
 * @endcode
 */
template <typename T>
concept ExporterLike = requires(T t) {
    { t.export_metrics(std::declval<std::vector<int>>()) };
    { t.is_ready() } -> std::convertible_to<bool>;
};

/**
 * @concept HealthCheckable
 * @brief A type that supports health checking.
 *
 * Health-checkable types provide methods for querying health status.
 *
 * Example usage:
 * @code
 * template<HealthCheckable H>
 * bool check_health(const H& component) {
 *     return component.is_healthy();
 * }
 * @endcode
 */
template <typename T>
concept HealthCheckable = requires(const T t) {
    { t.is_healthy() } -> std::convertible_to<bool>;
};

/**
 * @concept TracingContextLike
 * @brief A type that represents a tracing context.
 *
 * Tracing contexts provide trace and span identification.
 *
 * Example usage:
 * @code
 * template<TracingContextLike C>
 * void start_span(C& context, const std::string& name) {
 *     auto trace_id = context.get_trace_id();
 *     // Create new span
 * }
 * @endcode
 */
template <typename T>
concept TracingContextLike = requires(const T t) {
    { t.get_trace_id() } -> std::convertible_to<std::string>;
    { t.get_span_id() } -> std::convertible_to<std::string>;
};

} // namespace concepts
} // namespace kcenon::monitoring
