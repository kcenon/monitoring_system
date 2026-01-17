#pragma once

/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, monitoring_system contributors
All rights reserved.
*****************************************************************************/

/**
 * @file metric_storage.h
 * @brief Memory-efficient metric storage with ring buffer backend
 *
 * This file provides metric storage implementation that uses ring buffers
 * for efficient incoming metric buffering and time series for historical data.
 */

#include "../core/result_types.h"
#include "../core/error_codes.h"
#include "metric_types.h"
#include "time_series.h"
#include "ring_buffer.h"
#include <string>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <atomic>
#include <vector>
#include <thread>

namespace kcenon { namespace monitoring {

/**
 * @struct metric_storage_config
 * @brief Configuration for metric storage
 */
struct metric_storage_config {
    size_t ring_buffer_capacity = 8192;       // Ring buffer capacity (power of 2)
    size_t max_metrics = 10000;               // Maximum number of unique metric series
    bool enable_background_processing = true;  // Enable background flushing
    std::chrono::milliseconds flush_interval{1000}; // Background flush interval
    size_t time_series_max_points = 3600;     // Max points per time series
    std::chrono::seconds retention_period{3600}; // Data retention period

    /**
     * @brief Validate configuration
     */
    common::VoidResult validate() const {
        if (ring_buffer_capacity == 0 || (ring_buffer_capacity & (ring_buffer_capacity - 1)) != 0) {
            return common::VoidResult::err(error_info(monitoring_error_code::invalid_configuration,
                             "Ring buffer capacity must be a power of 2").to_common_error());
        }

        if (max_metrics == 0) {
            return common::VoidResult::err(error_info(monitoring_error_code::invalid_configuration,
                             "Max metrics must be positive").to_common_error());
        }

        if (time_series_max_points == 0) {
            return common::VoidResult::err(error_info(monitoring_error_code::invalid_configuration,
                             "Time series max points must be positive").to_common_error());
        }

        if (retention_period.count() <= 0) {
            return common::VoidResult::err(error_info(monitoring_error_code::invalid_configuration,
                             "Retention period must be positive").to_common_error());
        }

        return common::ok();
    }
};

/**
 * @struct metric_storage_stats
 * @brief Statistics for metric storage performance
 */
struct metric_storage_stats {
    std::atomic<size_t> total_metrics_stored{0};
    std::atomic<size_t> total_metrics_dropped{0};
    std::atomic<size_t> active_metric_series{0};
    std::atomic<size_t> flush_count{0};
    std::atomic<size_t> failed_flushes{0};
    std::chrono::system_clock::time_point creation_time;

    metric_storage_stats() : creation_time(std::chrono::system_clock::now()) {}
};

/**
 * @class metric_storage
 * @brief Thread-safe metric storage with ring buffer buffering
 *
 * Provides efficient metric storage using ring buffers for incoming data
 * and time series for historical queries. Supports background processing
 * for automatic flushing.
 */
class metric_storage {
private:
    mutable std::shared_mutex mutex_;
    metric_storage_config config_;
    mutable metric_storage_stats stats_;

    // Ring buffer for incoming metrics
    std::unique_ptr<ring_buffer<compact_metric_value>> incoming_buffer_;

    // Time series storage for each metric
    std::unordered_map<std::string, std::unique_ptr<time_series>> time_series_map_;

    // Metric name to hash mapping for fast lookup
    std::unordered_map<uint32_t, std::string> hash_to_name_;

    // Background processing
    std::atomic<bool> running_{false};
    std::thread background_thread_;

    /**
     * @brief Background processing loop
     */
    void background_processor() {
        while (running_.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(config_.flush_interval);
            if (running_.load(std::memory_order_acquire)) {
                flush();
            }
        }
    }

    /**
     * @brief Get or create time series for a metric
     */
    time_series* get_or_create_series(const std::string& name) {
        auto it = time_series_map_.find(name);
        if (it != time_series_map_.end()) {
            return it->second.get();
        }

        // Check if we're at capacity
        if (time_series_map_.size() >= config_.max_metrics) {
            return nullptr;
        }

        // Create new time series
        time_series_config ts_config;
        ts_config.max_points = config_.time_series_max_points;
        ts_config.retention_period = config_.retention_period;

        auto result = time_series::create(name, ts_config);
        if (result.is_err()) {
            return nullptr;
        }

        auto* ptr = result.value().get();
        time_series_map_[name] = std::move(result.value());
        stats_.active_metric_series.fetch_add(1, std::memory_order_relaxed);

        // Store hash mapping
        hash_to_name_[hash_metric_name(name)] = name;

        return ptr;
    }

public:
    /**
     * @brief Constructor with configuration
     */
    explicit metric_storage(const metric_storage_config& config = {})
        : config_(config) {

        auto validation = config_.validate();
        if (validation.is_err()) {
            throw std::invalid_argument("Invalid metric storage configuration: " +
                                       validation.error().message);
        }

        // Initialize ring buffer
        ring_buffer_config rb_config;
        rb_config.capacity = config_.ring_buffer_capacity;
        rb_config.overwrite_old = true;
        rb_config.batch_size = (std::min)(rb_config.capacity / 2, size_t(64));
        if (rb_config.batch_size == 0) rb_config.batch_size = 1;
        incoming_buffer_ = std::make_unique<ring_buffer<compact_metric_value>>(rb_config);

        // Start background processing if enabled
        if (config_.enable_background_processing) {
            running_.store(true, std::memory_order_release);
            background_thread_ = std::thread(&metric_storage::background_processor, this);
        }
    }

    /**
     * @brief Destructor
     */
    ~metric_storage() {
        if (running_.load(std::memory_order_acquire)) {
            running_.store(false, std::memory_order_release);
            if (background_thread_.joinable()) {
                background_thread_.join();
            }
        }
    }

    // Non-copyable
    metric_storage(const metric_storage&) = delete;
    metric_storage& operator=(const metric_storage&) = delete;

    /**
     * @brief Store a single metric value
     * @param name Metric name
     * @param value Metric value
     * @param type Metric type (default: gauge)
     * @return Result indicating success or failure
     */
    common::VoidResult store_metric(const std::string& name, double value,
                            metric_type type = metric_type::gauge) {
        auto metadata = create_metric_metadata(name, type);
        compact_metric_value metric(metadata, value);

        auto result = incoming_buffer_->write(std::move(metric));
        if (result.is_ok()) {
            stats_.total_metrics_stored.fetch_add(1, std::memory_order_relaxed);

            // Store name mapping
            std::unique_lock<std::shared_mutex> lock(mutex_);
            if (hash_to_name_.find(metadata.name_hash) == hash_to_name_.end()) {
                hash_to_name_[metadata.name_hash] = name;
            }
        } else {
            stats_.total_metrics_dropped.fetch_add(1, std::memory_order_relaxed);
        }

        return result;
    }


    /**
     * @brief Store a batch of metrics
     * @param batch Metric batch to store
     * @return Number of metrics successfully stored
     */
    size_t store_metrics_batch(const metric_batch& batch) {
        size_t stored = 0;

        for (const auto& metric : batch.metrics) {
            compact_metric_value copy = metric;
            auto result = incoming_buffer_->write(std::move(copy));
            if (result.is_ok()) {
                stored++;
                stats_.total_metrics_stored.fetch_add(1, std::memory_order_relaxed);
            } else {
                stats_.total_metrics_dropped.fetch_add(1, std::memory_order_relaxed);
            }
        }

        return stored;
    }

    /**
     * @brief Flush buffered metrics to time series
     */
    void flush() {
        std::vector<compact_metric_value> flushed_metrics;
        flushed_metrics.reserve(config_.ring_buffer_capacity);

        // Read all available metrics from buffer
        incoming_buffer_->read_batch(flushed_metrics, config_.ring_buffer_capacity);

        if (flushed_metrics.empty()) {
            return;
        }

        std::unique_lock<std::shared_mutex> lock(mutex_);

        for (auto& metric : flushed_metrics) {
            // Find metric name from hash
            auto name_it = hash_to_name_.find(metric.metadata.name_hash);
            if (name_it == hash_to_name_.end()) {
                continue;
            }

            const std::string& name = name_it->second;

            // Get or create time series
            auto* series = get_or_create_series(name);
            if (series == nullptr) {
                stats_.failed_flushes.fetch_add(1, std::memory_order_relaxed);
                continue;
            }

            // Add data point to time series
            series->add_point(metric.as_double(), metric.get_timestamp());
        }

        stats_.flush_count.fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * @brief Get the latest value for a metric
     * @param name Metric name
     * @return Optional containing the latest value if available
     */
    common::Result<double> get_latest_value(const std::string& name) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        auto it = time_series_map_.find(name);
        if (it == time_series_map_.end()) {
            return common::Result<double>::err(error_info(monitoring_error_code::collection_failed,
                                      "Metric not found: " + name, "monitoring_system").to_common_error());
        }

        return it->second->get_latest_value();
    }

    /**
     * @brief Get all metric names
     * @return Vector of metric names
     */
    std::vector<std::string> get_metric_names() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        std::vector<std::string> names;
        names.reserve(time_series_map_.size());

        for (const auto& pair : time_series_map_) {
            names.push_back(pair.first);
        }

        return names;
    }

    /**
     * @brief Query metric data
     * @param name Metric name
     * @param query Query parameters
     * @return Aggregation result
     */
    common::Result<aggregation_result> query_metric(const std::string& name,
                                            const time_series_query& query) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        auto it = time_series_map_.find(name);
        if (it == time_series_map_.end()) {
            return common::Result<aggregation_result>::err(error_info(monitoring_error_code::collection_failed,
                                                 "Metric not found: " + name, "monitoring_system").to_common_error());
        }

        return it->second->query(query);
    }

    /**
     * @brief Get storage statistics
     */
    const metric_storage_stats& get_stats() const noexcept {
        return stats_;
    }

    /**
     * @brief Get configuration
     */
    const metric_storage_config& get_config() const noexcept {
        return config_;
    }

    /**
     * @brief Clear all stored metrics
     */
    void clear() {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        incoming_buffer_->clear();
        time_series_map_.clear();
        hash_to_name_.clear();
        stats_.active_metric_series.store(0, std::memory_order_relaxed);
    }

    /**
     * @brief Get number of active metric series
     */
    size_t series_count() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return time_series_map_.size();
    }

    /**
     * @brief Get memory footprint estimate
     */
    size_t memory_footprint() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        size_t total = sizeof(metric_storage);
        total += config_.ring_buffer_capacity * sizeof(compact_metric_value);

        for (const auto& pair : time_series_map_) {
            total += pair.first.capacity();
            total += pair.second->memory_footprint();
        }

        return total;
    }
};

} } // namespace kcenon::monitoring
