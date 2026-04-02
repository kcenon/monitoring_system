// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

/**
 * @file collector_base.h
 * @brief CRTP base class for metric collectors
 *
 * This file provides a CRTP (Curiously Recurring Template Pattern) base class
 * that extracts common collector functionality to reduce code duplication.
 * All metric collectors share common patterns for initialization, collection,
 * statistics tracking, and health monitoring.
 *
 * Usage:
 * @code
 * class my_collector : public collector_base<my_collector> {
 * public:
 *     static constexpr const char* collector_name = "my_collector";
 *
 *     bool do_initialize(const config_map& config) {
 *         // Collector-specific initialization
 *         return true;
 *     }
 *
 *     std::vector<metric> do_collect() {
 *         // Collector-specific metric collection
 *         return metrics;
 *     }
 *
 *     bool is_available() const {
 *         // Check if this collector can operate
 *         return true;
 *     }
 *
 *     std::vector<std::string> do_get_metric_types() const {
 *         return {"metric_type_1", "metric_type_2"};
 *     }
 *
 *     void do_add_statistics(stats_map& stats) const {
 *         // Add collector-specific statistics
 *     }
 * };
 * @endcode
 */

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "../interfaces/metric_types_adapter.h"

namespace kcenon {
namespace monitoring {

/**
 * @brief Type alias for configuration map
 */
using config_map = std::unordered_map<std::string, std::string>;

/**
 * @brief Type alias for statistics map
 */
using stats_map = std::unordered_map<std::string, double>;

/**
 * @class collector_base
 * @brief CRTP base class for metric collectors
 *
 * This template class implements common functionality shared by all collectors:
 * - Configuration parsing (enabled state)
 * - Collection with error handling and statistics
 * - Health monitoring
 * - Statistics tracking (collection count, error count)
 *
 * @tparam Derived The derived collector class (CRTP pattern)
 */
template <typename Derived>
class collector_base {
   public:
    collector_base() = default;
    virtual ~collector_base() = default;

    // Non-copyable, non-moveable
    collector_base(const collector_base&) = delete;
    collector_base& operator=(const collector_base&) = delete;
    collector_base(collector_base&&) = delete;
    collector_base& operator=(collector_base&&) = delete;

    /**
     * @brief Initialize the collector with configuration
     * @param config Configuration options (common: "enabled")
     * @return true if initialization successful
     */
    bool initialize(const config_map& config) {
        // Parse common configuration
        if (auto it = config.find("enabled"); it != config.end()) {
            enabled_ = (it->second == "true" || it->second == "1");
        }

        // Delegate to derived class for specific initialization
        return derived().do_initialize(config);
    }

    /**
     * @brief Collect metrics from the data source
     * @return Collection of metrics
     */
    std::vector<metric> collect() {
        if (!enabled_) {
            return {};
        }

        try {
            auto metrics = derived().do_collect();
            ++collection_count_;
            return metrics;
        } catch (...) {
            ++collection_errors_;
            return {};
        }
    }

    /**
     * @brief Get the name of this collector
     * @return Collector name from Derived::collector_name
     */
    std::string get_name() const {
        return Derived::collector_name;
    }

    /**
     * @brief Get supported metric types
     * @return Vector of supported metric type names
     */
    std::vector<std::string> get_metric_types() const {
        return derived().do_get_metric_types();
    }

    /**
     * @brief Check if the collector is healthy
     * @return true if collector is operational
     */
    bool is_healthy() const {
        if (!enabled_) {
            return true;  // Disabled collectors are considered healthy
        }
        return derived().is_available();
    }

    /**
     * @brief Get collector statistics
     * @return Map of statistic name to value
     */
    stats_map get_statistics() const {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_map stats;

        // Common statistics
        stats["enabled"] = enabled_ ? 1.0 : 0.0;
        stats["collection_count"] = static_cast<double>(collection_count_.load());
        stats["collection_errors"] = static_cast<double>(collection_errors_.load());

        // Let derived class add specific statistics
        derived().do_add_statistics(stats);

        return stats;
    }

    /**
     * @brief Check if collector is enabled
     * @return true if enabled
     */
    bool is_enabled() const { return enabled_; }

    /**
     * @brief Get collection count
     * @return Number of successful collections
     */
    size_t get_collection_count() const { return collection_count_.load(); }

    /**
     * @brief Get error count
     * @return Number of failed collections
     */
    size_t get_collection_errors() const { return collection_errors_.load(); }

   protected:
    /**
     * @brief Create a metric with common tags
     * @param name Metric name
     * @param value Metric value
     * @param tags Additional tags
     * @param unit Optional unit string (not used, for documentation)
     * @return Created metric
     */
    metric create_base_metric(const std::string& name, double value,
                              const std::unordered_map<std::string, std::string>& tags = {},
                              const std::string& /* unit */ = "") const {
        metric m;
        m.name = name;
        m.value = value;
        m.timestamp = std::chrono::system_clock::now();
        m.tags = tags;
        m.tags["collector"] = Derived::collector_name;
        return m;
    }

    // Configuration
    bool enabled_{true};

    // Statistics
    mutable std::mutex stats_mutex_;
    std::atomic<size_t> collection_count_{0};
    std::atomic<size_t> collection_errors_{0};

   private:
    /**
     * @brief Get reference to derived class (CRTP helper)
     */
    Derived& derived() { return static_cast<Derived&>(*this); }
    const Derived& derived() const { return static_cast<const Derived&>(*this); }
};

}  // namespace monitoring
}  // namespace kcenon
