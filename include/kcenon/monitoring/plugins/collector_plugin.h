// BSD 3-Clause License
//
// Copyright (c) 2021-2025, kcenon
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once

/**
 * @file collector_plugin.h
 * @brief Plugin interface for metric collectors
 *
 * This file defines the plugin architecture for dynamically loadable
 * metric collectors. It provides a common interface that all collectors
 * must implement, enabling runtime registration, discovery, and lifecycle
 * management.
 *
 * Design Goals:
 * - Fewest Elements: Single interface for all collector types
 * - Reveals Intention: Clear contract for plugin behavior
 * - Separation of Concerns: Collection logic separate from registration
 * - Extensibility: Support for both built-in and dynamic plugins
 *
 * Usage:
 * @code
 * class my_collector_plugin : public collector_plugin {
 * public:
 *     auto name() const -> std::string_view override {
 *         return "my_collector";
 *     }
 *
 *     auto collect() -> std::vector<metric> override {
 *         // Collect and return metrics
 *         return metrics;
 *     }
 *
 *     auto interval() const -> std::chrono::milliseconds override {
 *         return std::chrono::seconds(5);
 *     }
 *
 *     auto is_available() const -> bool override {
 *         // Check platform/resource availability
 *         return true;
 *     }
 * };
 *
 * // Register with registry
 * registry.register_plugin(std::make_unique<my_collector_plugin>());
 * @endcode
 */

#include <chrono>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "../interfaces/metric_types_adapter.h"
#include "../collectors/collector_base.h"

namespace kcenon {
namespace monitoring {

/**
 * @enum plugin_category
 * @brief Categorization of collector plugins
 *
 * Categories help organize plugins by their data source type,
 * enabling selective loading and filtering.
 */
enum class plugin_category {
    system,      ///< System integration (threads, loggers, containers)
    hardware,    ///< Hardware sensors (GPU, temperature, battery, power)
    platform,    ///< Platform-specific (VM, uptime, interrupts)
    network,     ///< Network metrics (connectivity, bandwidth)
    process,     ///< Process-level metrics (resources, performance)
    custom       ///< User-defined plugins
};

/**
 * @struct plugin_metadata
 * @brief Metadata describing a collector plugin
 *
 * Provides discoverability and capability information about plugins
 * without requiring instantiation.
 */
struct plugin_metadata {
    std::string_view name;                    ///< Unique plugin identifier
    std::string_view description;             ///< Human-readable description
    plugin_category category;                 ///< Plugin category
    std::string_view version;                 ///< Plugin version (semver)
    std::vector<std::string_view> dependencies; ///< Required dependencies
    bool requires_platform_support{false};    ///< Platform-specific plugin
};

/**
 * @class collector_plugin
 * @brief Pure virtual interface for metric collector plugins
 *
 * This interface defines the contract that all metric collector plugins
 * must implement. It supports both built-in collectors and dynamically
 * loaded plugins.
 *
 * Thread Safety:
 * - collect() may be called concurrently from multiple threads
 * - Implementations MUST be thread-safe or document restrictions
 * - is_available() should be lock-free if possible
 *
 * Lifecycle:
 * 1. Construction (via factory or direct instantiation)
 * 2. is_available() check before registration
 * 3. Periodic collect() calls based on interval()
 * 4. Destruction on unregistration or shutdown
 *
 * @example
 * @code
 * // Implement a plugin for CPU temperature
 * class cpu_temp_plugin : public collector_plugin {
 * public:
 *     auto name() const -> std::string_view override {
 *         return "cpu_temperature";
 *     }
 *
 *     auto collect() -> std::vector<metric> override {
 *         std::vector<metric> metrics;
 *         double temp = read_cpu_temperature();
 *         metrics.push_back({"cpu_temp_celsius", temp, {}, metric_type::gauge});
 *         return metrics;
 *     }
 *
 *     auto interval() const -> std::chrono::milliseconds override {
 *         return std::chrono::seconds(1);
 *     }
 *
 *     auto is_available() const -> bool override {
 *         return has_thermal_sensors();
 *     }
 * };
 * @endcode
 */
class collector_plugin {
public:
    virtual ~collector_plugin() = default;

    /**
     * @brief Get the unique name of this plugin
     * @return Plugin name (must be unique within registry)
     *
     * The name is used for:
     * - Plugin lookup and discovery
     * - Configuration mapping
     * - Metric tagging (added as "collector" tag)
     */
    virtual auto name() const -> std::string_view = 0;

    /**
     * @brief Collect current metrics from this plugin
     * @return Vector of collected metrics
     *
     * This method is called periodically based on interval().
     * Implementations should:
     * - Return quickly (< 100ms recommended)
     * - Handle errors gracefully (return empty vector on failure)
     * - Be thread-safe if concurrent collection is enabled
     * - Avoid blocking I/O when possible
     */
    virtual auto collect() -> std::vector<metric> = 0;

    /**
     * @brief Get the collection interval for this plugin
     * @return Collection interval in milliseconds
     *
     * The registry uses this value to schedule collection tasks.
     * Typical values:
     * - Fast metrics (CPU, memory): 1-5 seconds
     * - Slow metrics (disk, network): 10-60 seconds
     * - Very slow metrics (SMART data): 5-15 minutes
     */
    virtual auto interval() const -> std::chrono::milliseconds = 0;

    /**
     * @brief Check if this plugin is available on the current system
     * @return True if plugin can collect metrics, false otherwise
     *
     * Availability checks may include:
     * - Platform compatibility (Linux-only, Windows-only)
     * - Hardware presence (GPU, sensors)
     * - Permission checks (root required)
     * - Resource availability (proc filesystem, WMI)
     *
     * The registry may skip unavailable plugins during registration.
     */
    virtual auto is_available() const -> bool = 0;

    /**
     * @brief Get plugin metadata
     * @return Metadata describing this plugin
     *
     * Default implementation returns minimal metadata.
     * Override to provide rich plugin information.
     */
    virtual auto get_metadata() const -> plugin_metadata {
        return plugin_metadata{
            .name = name(),
            .description = "",
            .category = plugin_category::custom,
            .version = "1.0.0",
            .dependencies = {},
            .requires_platform_support = false
        };
    }

    /**
     * @brief Initialize plugin with configuration
     * @param config Configuration key-value pairs
     * @return True if initialization succeeded
     *
     * Called once after plugin registration.
     * Optional: Default implementation always succeeds.
     */
    virtual auto initialize(const config_map& /* config */) -> bool {
        return true;
    }

    /**
     * @brief Shutdown plugin and release resources
     *
     * Called before plugin destruction.
     * Optional: Default implementation is no-op.
     */
    virtual void shutdown() {}

    /**
     * @brief Get plugin statistics
     * @return Map of statistic name to value
     *
     * Optional: Override to provide plugin-specific statistics.
     */
    virtual auto get_statistics() const -> stats_map {
        return {};
    }

    /**
     * @brief Get supported metric types
     * @return Vector of metric type names this plugin produces
     *
     * Used for filtering and documentation.
     */
    virtual auto get_metric_types() const -> std::vector<std::string> = 0;
};

/**
 * @brief Type alias for plugin factory function
 *
 * Factories enable lazy instantiation and dynamic loading.
 * Signature: unique_ptr<collector_plugin>(void)
 */
using plugin_factory_fn = std::unique_ptr<collector_plugin> (*)();

} // namespace monitoring
} // namespace kcenon
