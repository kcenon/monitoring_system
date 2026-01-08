#pragma once

/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, monitoring_system contributors
All rights reserved.
*****************************************************************************/

/**
 * @file monitoring_core.h
 * @brief Core monitoring system interface definitions
 *
 * This file defines the primary interfaces for the monitoring system,
 * utilizing the Result pattern for consistent error handling.
 *
 * Note: This file was renamed from monitoring_interface.h to avoid
 * naming collision with common_system's monitoring_interface.h which
 * defines the IMonitor interface. For backward compatibility, the old
 * header path is preserved as a deprecated forwarding header.
 *
 * @see kcenon/common/interfaces/monitoring_interface.h for IMonitor interface
 */

#include "../core/result_types.h"
#include "../core/error_codes.h"
#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <functional>
#include <unordered_map>
#include <optional>
#include <exception>

namespace kcenon { namespace monitoring {

// Forward declarations
class metrics_collector;
class storage_backend;
class metrics_analyzer;
struct metrics_snapshot;
struct monitoring_config;
struct health_check_result;

/**
 * @struct metric_value
 * @brief Represents a single metric value with metadata
 */
struct metric_value {
    std::string name;
    double value;
    std::chrono::system_clock::time_point timestamp;
    std::unordered_map<std::string, std::string> tags;
    
    metric_value(const std::string& n = "", double v = 0.0)
        : name(n)
        , value(v)
        , timestamp(std::chrono::system_clock::now()) {}
};

/**
 * @struct metrics_snapshot
 * @brief Complete snapshot of metrics at a point in time
 */
struct metrics_snapshot {
    std::vector<metric_value> metrics;
    std::chrono::system_clock::time_point capture_time;
    std::string source_id;
    
    metrics_snapshot()
        : capture_time(std::chrono::system_clock::now()) {}
    
    /**
     * @brief Add a metric to the snapshot
     * @param name Metric name
     * @param value Metric value
     */
    void add_metric(const std::string& name, double value) {
        metrics.emplace_back(name, value);
    }

    /**
     * @brief Add a metric to the snapshot with tags
     * @param name Metric name
     * @param value Metric value
     * @param tags Metadata tags for the metric
     */
    void add_metric(const std::string& name, double value,
                   const std::unordered_map<std::string, std::string>& tags) {
        metric_value mv(name, value);
        mv.tags = tags;
        metrics.push_back(std::move(mv));
    }
    
    /**
     * @brief Get a specific metric value
     * @param name Metric name
     * @return Optional metric value
     */
    std::optional<double> get_metric(const std::string& name) const {
        for (const auto& m : metrics) {
            if (m.name == name) {
                return m.value;
            }
        }
        return std::nullopt;
    }
};

/**
 * @enum health_status
 * @brief System health status levels
 */
enum class health_status {
    healthy,
    degraded,
    unhealthy,
    unknown
};

/**
 * @struct health_check_result
 * @brief Result of a health check operation
 */
struct health_check_result {
    health_status status = health_status::unknown;
    std::string message;
    std::vector<std::string> issues;
    std::chrono::system_clock::time_point check_time;
    std::chrono::system_clock::time_point timestamp;
    std::chrono::milliseconds check_duration{0};
    std::unordered_map<std::string, std::string> metadata;
    std::optional<std::exception_ptr> error;
    
    health_check_result()
        : check_time(std::chrono::system_clock::now()),
          timestamp(std::chrono::system_clock::now()) {}
    
    bool is_healthy() const {
        return status == health_status::healthy;
    }
    
    bool is_operational() const {
        return status == health_status::healthy || 
               status == health_status::degraded;
    }
    
    static health_check_result healthy(const std::string& msg = "OK") {
        health_check_result result;
        result.status = health_status::healthy;
        result.message = msg;
        result.timestamp = std::chrono::system_clock::now();
        result.check_time = result.timestamp;
        return result;
    }
    
    static health_check_result unhealthy(const std::string& msg) {
        health_check_result result;
        result.status = health_status::unhealthy;
        result.message = msg;
        result.timestamp = std::chrono::system_clock::now();
        result.check_time = result.timestamp;
        return result;
    }
    
    static health_check_result degraded(const std::string& msg) {
        health_check_result result;
        result.status = health_status::degraded;
        result.message = msg;
        result.timestamp = std::chrono::system_clock::now();
        result.check_time = result.timestamp;
        return result;
    }
};

/**
 * @struct monitoring_config
 * @brief Configuration for the monitoring system
 */
struct monitoring_config {
    std::size_t history_size = 1000;
    std::chrono::milliseconds collection_interval{1000};
    bool enable_compression = false;
    bool enable_persistence = false;
    std::size_t max_collectors = 100;
    std::size_t buffer_size = 10000;
    
    /**
     * @brief Validate configuration parameters
     * @return result_void indicating success or validation error
     */
    result_void validate() const {
        if (history_size == 0) {
            return make_void_error(monitoring_error_code::invalid_capacity,
                                 "History size must be greater than 0");
        }
        if (collection_interval.count() < 10) {
            return make_void_error(monitoring_error_code::invalid_interval,
                                 "Collection interval must be at least 10ms");
        }
        if (buffer_size < history_size) {
            return make_void_error(monitoring_error_code::invalid_capacity,
                                 "Buffer size must be at least as large as history size");
        }
        return make_void_success();
    }
};

/**
 * @class monitoring_interface
 * @brief Abstract interface for monitoring operations
 *
 * This interface defines the contract for monitoring implementations,
 * using the Result pattern for all operations that may fail. It provides
 * comprehensive functionality for collector management, metrics operations,
 * health checks, storage, and analysis.
 *
 * @thread_safety Implementations MUST be thread-safe. All methods may be
 *                called from multiple threads simultaneously. The start/stop
 *                methods should handle concurrent collection safely.
 *
 * @example
 * @code
 * class my_monitor : public monitoring_interface {
 *     // Implementation
 * };
 *
 * auto monitor = std::make_unique<my_monitor>();
 *
 * // Configure
 * monitoring_config config;
 * config.collection_interval = std::chrono::seconds(5);
 * monitor->configure(config);
 *
 * // Add collectors
 * monitor->add_collector(std::make_unique<cpu_collector>());
 * monitor->add_collector(std::make_unique<memory_collector>());
 *
 * // Start monitoring
 * monitor->start();
 *
 * // Register health check
 * monitor->register_health_check("database", []() {
 *     return check_db_connection() ?
 *         health_check_result::healthy() :
 *         health_check_result::unhealthy("DB connection failed");
 * });
 *
 * // Get status
 * auto snapshot = monitor->collect_now();
 * auto health = monitor->check_health();
 * @endcode
 *
 * @see metrics_collector for collector implementation
 * @see storage_backend for storage implementation
 */
class monitoring_interface {
public:
    virtual ~monitoring_interface() = default;
    
    // Configuration
    virtual result_void configure(const monitoring_config& config) = 0;
    virtual result<monitoring_config> get_configuration() const = 0;
    
    // Collector management
    virtual result_void add_collector(std::unique_ptr<metrics_collector> collector) = 0;
    virtual result_void remove_collector(const std::string& name) = 0;
    virtual result<std::vector<std::string>> list_collectors() const = 0;
    
    // Metrics operations
    virtual result_void start() = 0;
    virtual result_void stop() = 0;
    virtual result<metrics_snapshot> collect_now() = 0;
    virtual result<metrics_snapshot> get_latest_snapshot() const = 0;
    virtual result<std::vector<metrics_snapshot>> get_history(std::size_t count) const = 0;
    
    // Health checks
    virtual result<health_check_result> check_health() const = 0;
    virtual result_void register_health_check(
        const std::string& name,
        std::function<health_check_result()> checker) = 0;
    
    // Storage management
    virtual result_void set_storage_backend(std::unique_ptr<storage_backend> storage) = 0;
    virtual result_void flush_storage() = 0;
    
    // Analysis
    virtual result_void add_analyzer(std::unique_ptr<metrics_analyzer> analyzer) = 0;
    virtual result<std::vector<std::string>> get_analysis_results() const = 0;
    
    // Status
    virtual bool is_running() const = 0;
    virtual result<std::string> get_status_summary() const = 0;
};

/**
 * @class metrics_collector
 * @brief Abstract base class for metric collectors
 *
 * Base class for components that collect specific types of metrics.
 * Collectors can be enabled/disabled and should properly initialize
 * and cleanup their resources.
 *
 * @thread_safety Implementations should be thread-safe. The collect()
 *                method may be called from the monitoring thread while
 *                other methods are called from configuration threads.
 *
 * @example
 * @code
 * class cpu_collector : public metrics_collector {
 * public:
 *     result<metrics_snapshot> collect() override {
 *         metrics_snapshot snapshot;
 *         snapshot.source_id = get_name();
 *         snapshot.add_metric("cpu_usage_percent", get_cpu_usage());
 *         snapshot.add_metric("cpu_load_1min", get_load_average());
 *         return make_success(std::move(snapshot));
 *     }
 *
 *     std::string get_name() const override { return "cpu"; }
 *     bool is_enabled() const override { return enabled_; }
 *     result_void set_enabled(bool enable) override {
 *         enabled_ = enable;
 *         return make_void_success();
 *     }
 *     result_void initialize() override { return make_void_success(); }
 *     result_void cleanup() override { return make_void_success(); }
 * private:
 *     bool enabled_ = true;
 * };
 * @endcode
 *
 * @see monitoring_interface::add_collector for registration
 */
class metrics_collector {
public:
    virtual ~metrics_collector() = default;
    
    /**
     * @brief Collect metrics
     * @return Result containing collected metrics or error
     */
    virtual result<metrics_snapshot> collect() = 0;
    
    /**
     * @brief Get collector name
     * @return Collector identifier
     */
    virtual std::string get_name() const = 0;
    
    /**
     * @brief Check if collector is enabled
     * @return true if collector is active
     */
    virtual bool is_enabled() const = 0;
    
    /**
     * @brief Enable or disable the collector
     * @param enable true to enable, false to disable
     * @return Result indicating success or error
     */
    virtual result_void set_enabled(bool enable) = 0;
    
    /**
     * @brief Initialize the collector
     * @return Result indicating success or initialization error
     */
    virtual result_void initialize() = 0;
    
    /**
     * @brief Cleanup collector resources
     * @return Result indicating success or cleanup error
     */
    virtual result_void cleanup() = 0;
};

/**
 * @class storage_backend
 * @brief Abstract interface for metrics storage
 *
 * Provides persistence capabilities for metric snapshots. Implementations
 * can store data in memory, on disk, or in external databases.
 *
 * @thread_safety Implementations MUST be thread-safe. Store and retrieve
 *                operations may be called concurrently.
 *
 * @example
 * @code
 * class memory_storage : public storage_backend {
 * public:
 *     result_void store(const metrics_snapshot& snapshot) override {
 *         std::lock_guard<std::mutex> lock(mutex_);
 *         if (data_.size() >= capacity_) {
 *             data_.erase(data_.begin());
 *         }
 *         data_.push_back(snapshot);
 *         return make_void_success();
 *     }
 *
 *     result<metrics_snapshot> retrieve(std::size_t index) const override {
 *         std::lock_guard<std::mutex> lock(mutex_);
 *         if (index >= data_.size()) {
 *             return make_error<metrics_snapshot>(
 *                 monitoring_error_code::not_found, "Index out of range");
 *         }
 *         return make_success(data_[index]);
 *     }
 *     // ... other methods
 * private:
 *     mutable std::mutex mutex_;
 *     std::vector<metrics_snapshot> data_;
 *     std::size_t capacity_ = 1000;
 * };
 * @endcode
 *
 * @see monitoring_interface::set_storage_backend for configuration
 */
class storage_backend {
public:
    virtual ~storage_backend() = default;
    
    /**
     * @brief Store a metrics snapshot
     * @param snapshot The snapshot to store
     * @return Result indicating success or storage error
     */
    virtual result_void store(const metrics_snapshot& snapshot) = 0;
    
    /**
     * @brief Retrieve a stored snapshot by index
     * @param index The snapshot index
     * @return Result containing the snapshot or error
     */
    virtual result<metrics_snapshot> retrieve(std::size_t index) const = 0;
    
    /**
     * @brief Retrieve multiple snapshots
     * @param start_index Starting index
     * @param count Number of snapshots to retrieve
     * @return Result containing snapshots or error
     */
    virtual result<std::vector<metrics_snapshot>> retrieve_range(
        std::size_t start_index, std::size_t count) const = 0;
    
    /**
     * @brief Get storage capacity
     * @return Maximum number of snapshots that can be stored
     */
    virtual std::size_t capacity() const = 0;
    
    /**
     * @brief Get current storage usage
     * @return Number of stored snapshots
     */
    virtual std::size_t size() const = 0;
    
    /**
     * @brief Clear all stored data
     * @return Result indicating success or error
     */
    virtual result_void clear() = 0;
    
    /**
     * @brief Flush any buffered data to persistent storage
     * @return Result indicating success or error
     */
    virtual result_void flush() = 0;
};

/**
 * @class metrics_analyzer
 * @brief Abstract interface for metrics analysis
 *
 * Provides analysis capabilities for metric data. Implementations can
 * perform single-snapshot analysis or trend analysis across multiple
 * snapshots to detect anomalies, patterns, or threshold violations.
 *
 * @thread_safety Implementations should be thread-safe. The analyze()
 *                method may be called concurrently with analyze_trend().
 *
 * @example
 * @code
 * class threshold_analyzer : public metrics_analyzer {
 * public:
 *     threshold_analyzer(const std::string& metric, double threshold)
 *         : metric_name_(metric), threshold_(threshold) {}
 *
 *     result<std::string> analyze(const metrics_snapshot& snapshot) override {
 *         auto value = snapshot.get_metric(metric_name_);
 *         if (value && *value > threshold_) {
 *             return make_success(fmt::format(
 *                 "ALERT: {} = {} exceeds threshold {}",
 *                 metric_name_, *value, threshold_));
 *         }
 *         return make_success("OK");
 *     }
 *
 *     result<std::string> analyze_trend(
 *         const std::vector<metrics_snapshot>& snapshots) override {
 *         // Analyze trend over time
 *         return make_success("Trend: stable");
 *     }
 *
 *     std::string get_name() const override { return "threshold_analyzer"; }
 *     result_void reset() override { return make_void_success(); }
 * private:
 *     std::string metric_name_;
 *     double threshold_;
 * };
 * @endcode
 *
 * @see monitoring_interface::add_analyzer for registration
 */
class metrics_analyzer {
public:
    virtual ~metrics_analyzer() = default;
    
    /**
     * @brief Analyze a metrics snapshot
     * @param snapshot The snapshot to analyze
     * @return Result containing analysis results or error
     */
    virtual result<std::string> analyze(const metrics_snapshot& snapshot) = 0;
    
    /**
     * @brief Analyze multiple snapshots for trends
     * @param snapshots The snapshots to analyze
     * @return Result containing trend analysis or error
     */
    virtual result<std::string> analyze_trend(
        const std::vector<metrics_snapshot>& snapshots) = 0;
    
    /**
     * @brief Get analyzer name
     * @return Analyzer identifier
     */
    virtual std::string get_name() const = 0;
    
    /**
     * @brief Reset analyzer state
     * @return Result indicating success or error
     */
    virtual result_void reset() = 0;
};

} } // namespace kcenon::monitoring