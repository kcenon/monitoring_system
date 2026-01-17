#pragma once

/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, monitoring_system contributors
All rights reserved.
*****************************************************************************/

/**
 * @file monitorable_interface.h
 * @brief Interface for components that expose monitoring metrics
 * 
 * This file provides the monitorable interface pattern from thread_system,
 * allowing any component to expose its internal metrics in a standardized way.
 */

#include "../core/result_types.h"
#include "../core/error_codes.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <memory>
#include <any>
#include <optional>
#include <algorithm>

namespace kcenon { namespace monitoring {

/**
 * @struct monitoring_data
 * @brief Container for monitoring metrics from a component
 *
 * This structure holds key-value pairs of metrics that a component
 * exposes for monitoring purposes. It supports both numeric metrics
 * and string tags for additional metadata.
 *
 * @thread_safety This class is NOT inherently thread-safe. When accessed
 *                from multiple threads, external synchronization is required.
 *                Consider using `mutable` with mutex for thread-safe access
 *                in derived classes.
 *
 * @example
 * @code
 * monitoring_data data("my_service");
 * data.add_metric("requests_per_second", 150.5);
 * data.add_metric("active_connections", 42.0);
 * data.add_tag("version", "1.2.3");
 * data.add_tag("region", "us-east-1");
 *
 * // Retrieve metrics
 * if (auto rps = data.get_metric("requests_per_second")) {
 *     std::cout << "RPS: " << *rps << std::endl;
 * }
 *
 * // Merge with another data source
 * monitoring_data other_data("cache");
 * other_data.add_metric("hit_rate", 0.95);
 * data.merge(other_data, "cache");  // Prefixed as "cache.hit_rate"
 * @endcode
 *
 * @see monitorable_interface for the interface that produces this data
 */
struct monitoring_data {
    using metric_map = std::unordered_map<std::string, double>;
    using tag_map = std::unordered_map<std::string, std::string>;
    
private:
    metric_map metrics_;
    tag_map tags_;
    std::chrono::system_clock::time_point timestamp_;
    std::string component_name_;
    
public:
    /**
     * @brief Default constructor
     */
    monitoring_data()
        : timestamp_(std::chrono::system_clock::now()) {}
    
    /**
     * @brief Constructor with component name
     * @param name Name of the component being monitored
     */
    explicit monitoring_data(const std::string& name)
        : timestamp_(std::chrono::system_clock::now())
        , component_name_(name) {}
    
    /**
     * @brief Add a numeric metric
     * @param key Metric name
     * @param value Metric value
     */
    void add_metric(const std::string& key, double value) {
        metrics_[key] = value;
    }
    
    /**
     * @brief Add a tag (string metadata)
     * @param key Tag name
     * @param value Tag value
     */
    void add_tag(const std::string& key, const std::string& value) {
        tags_[key] = value;
    }
    
    /**
     * @brief Get a metric value
     * @param key Metric name
     * @return Optional containing the value if found
     */
    std::optional<double> get_metric(const std::string& key) const {
        auto it = metrics_.find(key);
        if (it != metrics_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    /**
     * @brief Get a tag value
     * @param key Tag name
     * @return Optional containing the value if found
     */
    std::optional<std::string> get_tag(const std::string& key) const {
        auto it = tags_.find(key);
        if (it != tags_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    /**
     * @brief Get all metrics
     * @return Map of all metrics
     */
    const metric_map& get_metrics() const {
        return metrics_;
    }
    
    /**
     * @brief Get all tags
     * @return Map of all tags
     */
    const tag_map& get_tags() const {
        return tags_;
    }
    
    /**
     * @brief Get the timestamp
     * @return Time when the data was collected
     */
    std::chrono::system_clock::time_point get_timestamp() const {
        return timestamp_;
    }
    
    /**
     * @brief Get the component name
     * @return Name of the monitored component
     */
    const std::string& get_component_name() const {
        return component_name_;
    }
    
    /**
     * @brief Set the component name
     * @param name Component name
     */
    void set_component_name(const std::string& name) {
        component_name_ = name;
    }
    
    /**
     * @brief Clear all metrics and tags
     */
    void clear() {
        metrics_.clear();
        tags_.clear();
    }
    
    /**
     * @brief Check if data is empty
     * @return true if no metrics or tags are present
     */
    bool empty() const {
        return metrics_.empty() && tags_.empty();
    }
    
    /**
     * @brief Get the number of metrics
     * @return Count of metrics
     */
    std::size_t metric_count() const {
        return metrics_.size();
    }
    
    /**
     * @brief Get the number of tags
     * @return Count of tags
     */
    std::size_t tag_count() const {
        return tags_.size();
    }
    
    /**
     * @brief Merge another monitoring_data into this one
     * @param other Data to merge
     * @param prefix Optional prefix for merged metrics
     */
    void merge(const monitoring_data& other, const std::string& prefix = "") {
        for (const auto& [key, value] : other.metrics_) {
            if (prefix.empty()) {
                metrics_[key] = value;
            } else {
                metrics_[prefix + "." + key] = value;
            }
        }
        
        for (const auto& [key, value] : other.tags_) {
            if (prefix.empty()) {
                tags_[key] = value;
            } else {
                tags_[prefix + "." + key] = value;
            }
        }
    }
};

/**
 * @class monitorable_interface
 * @brief Interface for components that can be monitored
 *
 * This interface allows components to expose their internal state
 * and metrics for monitoring purposes. It follows the pattern from
 * thread_system for consistent monitoring across components.
 *
 * @thread_safety Implementations should be thread-safe for the
 *                get_monitoring_data() method as it may be called
 *                from monitoring threads while the component is active.
 *
 * @example
 * @code
 * class database_connection : public monitorable_interface {
 * public:
 *     common::Result<monitoring_data> get_monitoring_data() const override {
 *         monitoring_data data(get_monitoring_id());
 *         data.add_metric("active_queries", active_queries_.load());
 *         data.add_metric("connection_pool_size", pool_size_);
 *         data.add_tag("database", database_name_);
 *         return common::ok(std::move(data));
 *     }
 *
 *     std::string get_monitoring_id() const override {
 *         return "db_connection_" + database_name_;
 *     }
 * private:
 *     std::atomic<int> active_queries_{0};
 *     size_t pool_size_;
 *     std::string database_name_;
 * };
 * @endcode
 *
 * @see monitoring_data for the returned data structure
 * @see monitorable_component for a convenient base implementation
 */
class monitorable_interface {
public:
    virtual ~monitorable_interface() = default;
    
    /**
     * @brief Get current monitoring data from the component
     * @return Result containing monitoring data or error
     */
    virtual common::Result<monitoring_data> get_monitoring_data() const = 0;
    
    /**
     * @brief Get the component's monitoring identifier
     * @return Unique identifier for this component
     */
    virtual std::string get_monitoring_id() const = 0;
    
    /**
     * @brief Check if monitoring is enabled for this component
     * @return true if monitoring is active
     */
    virtual bool is_monitoring_enabled() const {
        return true;
    }
    
    /**
     * @brief Enable or disable monitoring
     * @param enable true to enable, false to disable
     * @return Result indicating success or error
     */
    virtual common::VoidResult set_monitoring_enabled(bool enable) {
        // Default implementation - can be overridden
        (void)enable; // Suppress unused parameter warning
        return common::ok();
    }
    
    /**
     * @brief Reset monitoring counters and state
     * @return Result indicating success or error
     */
    virtual common::VoidResult reset_monitoring() {
        // Default implementation - can be overridden
        return common::ok();
    }
};

/**
 * @class monitorable_component
 * @brief Base class providing default monitorable implementation
 *
 * This class provides a convenient base for components that want
 * to implement the monitorable interface with common functionality.
 * It handles ID management, enable/disable state, and cached data.
 *
 * @thread_safety The base implementation uses mutable cached_data_
 *                which is NOT thread-safe. Derived classes should
 *                add synchronization if needed for concurrent access.
 *
 * @example
 * @code
 * class web_server : public monitorable_component {
 * public:
 *     web_server() : monitorable_component("web_server") {}
 *
 *     common::Result<monitoring_data> get_monitoring_data() const override {
 *         update_metric("requests_total", requests_.load());
 *         update_metric("avg_response_time_ms", avg_response_time_.load());
 *         update_tag("status", is_running_ ? "running" : "stopped");
 *         return common::ok(cached_data_);
 *     }
 * private:
 *     std::atomic<uint64_t> requests_{0};
 *     std::atomic<double> avg_response_time_{0.0};
 *     bool is_running_ = false;
 * };
 * @endcode
 *
 * @see monitorable_interface for the interface contract
 */
class monitorable_component : public monitorable_interface {
protected:
    std::string monitoring_id_;
    bool monitoring_enabled_ = true;
    mutable monitoring_data cached_data_;
    
public:
    /**
     * @brief Constructor
     * @param id Monitoring identifier for this component
     */
    explicit monitorable_component(const std::string& id)
        : monitoring_id_(id)
        , cached_data_(id) {}
    
    /**
     * @brief Get monitoring identifier
     * @return Component's monitoring ID
     */
    std::string get_monitoring_id() const override {
        return monitoring_id_;
    }
    
    /**
     * @brief Check if monitoring is enabled
     * @return true if monitoring is active
     */
    bool is_monitoring_enabled() const override {
        return monitoring_enabled_;
    }
    
    /**
     * @brief Enable or disable monitoring
     * @param enable true to enable, false to disable
     * @return Result indicating success
     */
    common::VoidResult set_monitoring_enabled(bool enable) override {
        monitoring_enabled_ = enable;
        return common::ok();
    }
    
    /**
     * @brief Reset monitoring data
     * @return Result indicating success
     */
    common::VoidResult reset_monitoring() override {
        cached_data_.clear();
        cached_data_.set_component_name(monitoring_id_);
        return common::ok();
    }
    
protected:
    /**
     * @brief Helper to update a metric
     * @param key Metric name
     * @param value Metric value
     */
    void update_metric(const std::string& key, double value) const {
        cached_data_.add_metric(key, value);
    }
    
    /**
     * @brief Helper to update a tag
     * @param key Tag name
     * @param value Tag value
     */
    void update_tag(const std::string& key, const std::string& value) const {
        cached_data_.add_tag(key, value);
    }
};

/**
 * @class monitoring_aggregator
 * @brief Utility class to aggregate metrics from multiple monitorable components
 *
 * Collects and merges monitoring data from multiple components into a
 * single aggregated view. Useful for creating dashboards or exporting
 * combined metrics from a subsystem.
 *
 * @thread_safety This class is NOT thread-safe. External synchronization
 *                is required when modifying the component list from
 *                multiple threads.
 *
 * @example
 * @code
 * monitoring_aggregator aggregator("my_service");
 *
 * // Add components to monitor
 * aggregator.add_component(std::make_shared<database_connection>());
 * aggregator.add_component(std::make_shared<cache_service>());
 * aggregator.add_component(std::make_shared<http_server>());
 *
 * // Collect all metrics
 * auto result = aggregator.collect_all();
 * if (result.is_ok()) {
 *     const auto& data = result.value();
 *     for (const auto& [name, value] : data.get_metrics()) {
 *         export_metric(name, value);
 *     }
 * }
 *
 * // Check specific component
 * auto db = aggregator.get_component("db_connection_main");
 * if (db && db->is_monitoring_enabled()) {
 *     // Process DB metrics specifically
 * }
 * @endcode
 *
 * @see monitorable_interface for the interface components must implement
 */
class monitoring_aggregator {
private:
    std::vector<std::shared_ptr<monitorable_interface>> components_;
    std::string aggregator_id_;
    
public:
    /**
     * @brief Constructor
     * @param id Identifier for this aggregator
     */
    explicit monitoring_aggregator(const std::string& id = "aggregator")
        : aggregator_id_(id) {}
    
    /**
     * @brief Add a component to monitor
     * @param component Component to add
     */
    void add_component(std::shared_ptr<monitorable_interface> component) {
        if (component) {
            components_.push_back(component);
        }
    }
    
    /**
     * @brief Remove a component by ID
     * @param id Component ID to remove
     * @return true if component was found and removed
     */
    bool remove_component(const std::string& id) {
        auto it = std::remove_if(components_.begin(), components_.end(),
            [&id](const auto& comp) {
                return comp->get_monitoring_id() == id;
            });
        
        if (it != components_.end()) {
            components_.erase(it, components_.end());
            return true;
        }
        return false;
    }
    
    /**
     * @brief Collect data from all components
     * @return Aggregated monitoring data
     */
    common::Result<monitoring_data> collect_all() const {
        monitoring_data aggregated(aggregator_id_);
        
        for (const auto& component : components_) {
            if (!component->is_monitoring_enabled()) {
                continue;
            }
            
            auto result = component->get_monitoring_data();
            if (result.is_ok()) {
                aggregated.merge(result.value(), component->get_monitoring_id());
            } else {
                // Log error but continue with other components
                aggregated.add_tag(
                    component->get_monitoring_id() + ".error",
                    result.error().message
                );
            }
        }
        
        // Add aggregator metadata
        aggregated.add_metric("aggregator.component_count", 
                            static_cast<double>(components_.size()));
        aggregated.add_metric("aggregator.total_metrics", 
                            static_cast<double>(aggregated.metric_count()));
        
        return common::ok(std::move(aggregated));
    }
    
    /**
     * @brief Get a specific component by ID
     * @param id Component ID
     * @return Shared pointer to component or nullptr
     */
    std::shared_ptr<monitorable_interface> get_component(const std::string& id) const {
        auto it = std::find_if(components_.begin(), components_.end(),
            [&id](const auto& comp) {
                return comp->get_monitoring_id() == id;
            });
        
        return (it != components_.end()) ? *it : nullptr;
    }
    
    /**
     * @brief Get all component IDs
     * @return Vector of component identifiers
     */
    std::vector<std::string> get_component_ids() const {
        std::vector<std::string> ids;
        ids.reserve(components_.size());
        
        for (const auto& component : components_) {
            ids.push_back(component->get_monitoring_id());
        }
        
        return ids;
    }
    
    /**
     * @brief Clear all components
     */
    void clear() {
        components_.clear();
    }
    
    /**
     * @brief Get the number of registered components
     * @return Component count
     */
    std::size_t size() const {
        return components_.size();
    }
};

} } // namespace kcenon::monitoring