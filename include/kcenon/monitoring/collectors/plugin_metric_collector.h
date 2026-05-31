// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file plugin_metric_collector.h
 * @brief Plugin interface for metric collectors with a dynamic data source.
 *
 * This header defines @ref metric_collector_plugin, the pure-virtual interface
 * that concrete metric collector plugins (for example
 * @ref system_resource_collector, @ref container_plugin and
 * @ref hardware_plugin) implement. The interface is header-only and fully
 * compiled into every consumer through its vtable, so it is safe to integrate.
 *
 * @note A previous revision of this header also declared a
 *       @c plugin_metric_collector manager class and a @c plugin_factory whose
 *       member functions were never defined in @c src/, never tested, and never
 *       registered in @c builtin_collectors.h. Those test-only sketches were
 *       removed in issue #690; only the production @ref metric_collector_plugin
 *       interface remains.
 *
 * @see metric_collector_plugin
 */

#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "../interfaces/metric_types_adapter.h"

namespace kcenon { namespace monitoring {

/**
 * Plugin interface for metric collectors
 * All metric collector plugins must implement this interface
 */
class metric_collector_plugin {
  public:
    virtual ~metric_collector_plugin() = default;

    /**
     * Initialize the plugin with configuration
     * @param config Plugin-specific configuration
     * @return true if initialization successful
     */
    virtual bool initialize(const std::unordered_map<std::string, std::string>& config) = 0;

    /**
     * Collect metrics from the data source
     * @return Collection of metrics
     */
    virtual std::vector<metric> collect() = 0;

    /**
     * Get the name of this plugin
     * @return Plugin name
     */
    virtual std::string get_name() const = 0;

    /**
     * Get supported metric types
     * @return Vector of supported metric type names
     */
    virtual std::vector<std::string> get_metric_types() const = 0;

    /**
     * Check if the plugin is healthy
     * @return true if plugin is operational
     */
    virtual bool is_healthy() const = 0;

    /**
     * Get plugin-specific statistics
     * @return Map of statistic name to value
     */
    virtual std::unordered_map<std::string, double> get_statistics() const = 0;
};

} } // namespace kcenon::monitoring
