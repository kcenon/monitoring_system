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
 * @file collector_registry.h
 * @brief Registry for managing collector plugin lifecycle
 *
 * This file defines the central registry that manages all collector plugins.
 * It provides:
 * - Plugin registration and unregistration
 * - Plugin discovery by name or category
 * - Lifecycle management (initialization, shutdown)
 * - Factory-based lazy instantiation
 * - Thread-safe operations
 *
 * The registry uses a singleton pattern to ensure single global instance.
 *
 * Usage:
 * @code
 * // Register a plugin instance
 * auto& registry = collector_registry::instance();
 * registry.register_plugin(std::make_unique<my_collector_plugin>());
 *
 * // Register a factory for lazy loading
 * registry.register_factory<my_collector_plugin>("my_collector");
 *
 * // Initialize all registered plugins
 * registry.initialize_all();
 *
 * // Get a specific plugin
 * if (auto* plugin = registry.get_plugin("my_collector")) {
 *     auto metrics = plugin->collect();
 * }
 *
 * // Get plugins by category
 * auto hw_plugins = registry.get_plugins_by_category(plugin_category::hardware);
 *
 * // Shutdown all plugins
 * registry.shutdown_all();
 * @endcode
 */

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "collector_plugin.h"
#include "plugin_loader.h"

namespace kcenon {
namespace monitoring {

/**
 * @class collector_registry
 * @brief Thread-safe registry for managing collector plugins
 *
 * This class manages the lifecycle of all collector plugins in the system.
 * It supports both eager registration (with plugin instances) and lazy
 * registration (with factory functions).
 *
 * Thread Safety:
 * - All public methods are thread-safe
 * - Uses internal mutex for synchronization
 * - Safe to call from multiple threads concurrently
 *
 * Lifecycle:
 * 1. Construction (via instance())
 * 2. Plugin registration (register_plugin, register_factory)
 * 3. Initialization (initialize_all)
 * 4. Collection (periodic calls to each plugin's collect())
 * 5. Shutdown (shutdown_all)
 * 6. Destruction (automatic on program exit)
 */
class collector_registry {
public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the global registry instance
     */
    static auto instance() -> collector_registry&;

    /**
     * @brief Register a plugin instance
     * @param plugin Unique pointer to the plugin
     * @return True if registration succeeded, false if plugin already exists
     *
     * The registry takes ownership of the plugin.
     * If a plugin with the same name already exists, registration fails.
     * Unavailable plugins (is_available() returns false) are not registered.
     */
    auto register_plugin(std::unique_ptr<collector_plugin> plugin) -> bool;

    /**
     * @brief Unregister a plugin by name
     * @param name Plugin name
     * @return True if plugin was found and removed
     *
     * Calls shutdown() on the plugin before removal.
     */
    auto unregister_plugin(std::string_view name) -> bool;

    /**
     * @brief Register a factory function for lazy instantiation
     * @tparam T Plugin type (must derive from collector_plugin)
     * @param name Plugin name (used for lookup)
     *
     * The factory function will be called when the plugin is first accessed.
     * This allows deferring plugin construction until needed.
     */
    template <typename T>
    void register_factory(std::string_view name) {
        static_assert(std::is_base_of_v<collector_plugin, T>,
                      "T must derive from collector_plugin");

        std::lock_guard<std::mutex> lock(mutex_);
        factories_[std::string(name)] = []() -> std::unique_ptr<collector_plugin> {
            return std::make_unique<T>();
        };
    }

    /**
     * @brief Get a plugin by name
     * @param name Plugin name
     * @return Pointer to plugin, or nullptr if not found
     *
     * If the plugin was registered via factory and not yet instantiated,
     * this will trigger instantiation.
     */
    auto get_plugin(std::string_view name) -> collector_plugin*;

    /**
     * @brief Get all registered plugins
     * @return Vector of plugin pointers
     *
     * Triggers instantiation of any factory-registered plugins.
     */
    auto get_plugins() -> std::vector<collector_plugin*>;

    /**
     * @brief Get plugins in a specific category
     * @param category Plugin category filter
     * @return Vector of matching plugin pointers
     *
     * Triggers instantiation of any factory-registered plugins.
     */
    auto get_plugins_by_category(plugin_category category)
        -> std::vector<collector_plugin*>;

    /**
     * @brief Initialize all registered plugins
     * @param config Configuration map (optional)
     * @return Number of successfully initialized plugins
     *
     * Calls initialize() on each plugin.
     * Plugins that fail initialization remain registered but may fail collection.
     */
    auto initialize_all(const config_map& config = {}) -> size_t;

    /**
     * @brief Shutdown all registered plugins
     *
     * Calls shutdown() on each plugin in reverse registration order.
     * Safe to call multiple times.
     */
    void shutdown_all();

    /**
     * @brief Get registry statistics
     * @return Map of statistic name to value
     *
     * Available statistics:
     * - "total_plugins": Total number of registered plugins
     * - "initialized_plugins": Number of initialized plugins
     * - "available_plugins": Number of available plugins
     * - "category_<name>_count": Count per category
     */
    auto get_registry_stats() const -> std::map<std::string, size_t>;

    /**
     * @brief Check if a plugin is registered
     * @param name Plugin name
     * @return True if plugin exists (either instantiated or factory-registered)
     */
    auto has_plugin(std::string_view name) const -> bool;

    /**
     * @brief Get the number of registered plugins
     * @return Count of plugins (both instantiated and factory-registered)
     */
    auto plugin_count() const -> size_t;

    /**
     * @brief Load a plugin from a shared library
     * @param path Path to the shared library (.so/.dylib/.dll)
     * @return True if plugin loaded and registered successfully
     *
     * This method:
     * 1. Loads the shared library using dynamic_plugin_loader
     * 2. Creates the plugin instance
     * 3. Registers it in the registry
     *
     * If loading fails, check the loader's error message via get_plugin_loader_error().
     */
    auto load_plugin(std::string_view path) -> bool;

    /**
     * @brief Unload a dynamically loaded plugin
     * @param name Plugin name
     * @return True if plugin was unloaded successfully
     *
     * This method:
     * 1. Unregisters the plugin from registry
     * 2. Destroys the plugin instance
     * 3. Unloads the shared library
     *
     * Only plugins loaded via load_plugin() can be unloaded this way.
     */
    auto unload_plugin(std::string_view name) -> bool;

    /**
     * @brief Get the last error from plugin loader
     * @return Error message string (empty if no error)
     */
    auto get_plugin_loader_error() const -> std::string;

    /**
     * @brief Clear all plugins (for testing)
     *
     * Calls shutdown_all() and removes all plugins.
     * Use with caution in production code.
     */
    void clear();

    // Non-copyable, non-moveable
    collector_registry(const collector_registry&) = delete;
    collector_registry& operator=(const collector_registry&) = delete;
    collector_registry(collector_registry&&) = delete;
    collector_registry& operator=(collector_registry&&) = delete;

private:
    collector_registry() = default;
    ~collector_registry();

    /**
     * @brief Instantiate a plugin from factory if needed
     * @param name Plugin name
     * @return True if plugin was instantiated or already exists
     */
    auto instantiate_from_factory(const std::string& name) -> bool;

    // Plugin storage
    std::unordered_map<std::string, std::unique_ptr<collector_plugin>> plugins_;

    // Factory storage for lazy instantiation
    std::unordered_map<std::string, plugin_factory_fn> factories_;

    // Initialization tracking
    std::unordered_map<std::string, bool> initialized_;

    // Thread safety
    mutable std::mutex mutex_;

    // Shutdown flag
    bool shutdown_{false};

    // Dynamic plugin loader
    std::unique_ptr<dynamic_plugin_loader> plugin_loader_;
};

} // namespace monitoring
} // namespace kcenon
