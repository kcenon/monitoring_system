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
 * @file plugin_loader.h
 * @brief Dynamic plugin loading from shared libraries
 *
 * This file defines the plugin_loader class which handles loading collector
 * plugins from shared libraries (.so/.dylib/.dll) at runtime.
 *
 * Features:
 * - Cross-platform support (Linux, macOS, Windows)
 * - Thread-safe operations
 * - API version compatibility checking
 * - Automatic resource cleanup
 * - Error reporting with detailed messages
 *
 * Usage:
 * @code
 * auto loader = std::make_unique<dynamic_plugin_loader>();
 *
 * // Load a plugin
 * auto plugin = loader->load_plugin("/path/to/libmy_plugin.so");
 * if (plugin) {
 *     // Use the plugin...
 * }
 *
 * // Unload the plugin
 * loader->unload_plugin("my_plugin");
 * @endcode
 */

#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

#include "collector_plugin.h"
#include "plugin_api.h"

namespace kcenon {
namespace monitoring {

/**
 * @enum plugin_load_error
 * @brief Error codes for plugin loading operations
 */
enum class plugin_load_error {
    none = 0,
    file_not_found,
    library_load_failed,
    symbol_not_found,
    incompatible_api_version,
    create_function_failed,
    plugin_unavailable,
    already_loaded,
    not_loaded,
    invalid_metadata
};

/**
 * @brief Convert plugin_load_error to string
 */
auto to_string(plugin_load_error error) -> std::string;

/**
 * @class plugin_loader
 * @brief Abstract interface for plugin loading
 *
 * This interface defines the contract for loading and unloading plugins.
 * Concrete implementations provide platform-specific loading mechanisms.
 */
class plugin_loader {
public:
    virtual ~plugin_loader() = default;

    /**
     * @brief Load a plugin from a shared library
     * @param path Path to the plugin library (.so/.dylib/.dll)
     * @return Unique pointer to the loaded plugin, or nullptr on failure
     *
     * The loader will:
     * 1. Load the shared library
     * 2. Resolve required symbols (create_plugin, etc.)
     * 3. Verify API version compatibility
     * 4. Create the plugin instance
     * 5. Track the plugin for cleanup
     *
     * On failure, check get_last_error() for details.
     */
    virtual auto load_plugin(std::string_view path)
        -> std::unique_ptr<collector_plugin> = 0;

    /**
     * @brief Unload a previously loaded plugin
     * @param plugin_name Name of the plugin to unload
     * @return True if unloaded successfully
     *
     * This will:
     * 1. Destroy the plugin instance
     * 2. Unload the shared library
     * 3. Clean up tracking data
     *
     * The plugin instance must have been destroyed before calling this.
     */
    virtual auto unload_plugin(std::string_view plugin_name) -> bool = 0;

    /**
     * @brief Get the last error that occurred
     * @return Error code
     */
    virtual auto get_last_error() const -> plugin_load_error = 0;

    /**
     * @brief Get detailed error message for the last error
     * @return Error message string
     */
    virtual auto get_last_error_message() const -> std::string = 0;

    /**
     * @brief Check if a plugin is currently loaded
     * @param plugin_name Name of the plugin
     * @return True if the plugin is loaded
     */
    virtual auto is_plugin_loaded(std::string_view plugin_name) const -> bool = 0;

    /**
     * @brief Get list of loaded plugin names
     * @return Vector of plugin names
     */
    virtual auto get_loaded_plugins() const -> std::vector<std::string> = 0;
};

/**
 * @class dynamic_plugin_loader
 * @brief Concrete implementation of plugin_loader using OS dynamic loading APIs
 *
 * This class uses platform-specific APIs:
 * - Linux/macOS: dlopen, dlsym, dlclose
 * - Windows: LoadLibrary, GetProcAddress, FreeLibrary
 *
 * Thread Safety:
 * - All operations are thread-safe
 * - Uses internal mutex for synchronization
 */
class dynamic_plugin_loader : public plugin_loader {
public:
    dynamic_plugin_loader();
    ~dynamic_plugin_loader() override;

    // Disable copy
    dynamic_plugin_loader(const dynamic_plugin_loader&) = delete;
    dynamic_plugin_loader& operator=(const dynamic_plugin_loader&) = delete;

    // Enable move
    dynamic_plugin_loader(dynamic_plugin_loader&&) noexcept;
    dynamic_plugin_loader& operator=(dynamic_plugin_loader&&) noexcept;

    auto load_plugin(std::string_view path)
        -> std::unique_ptr<collector_plugin> override;

    auto unload_plugin(std::string_view plugin_name) -> bool override;

    auto get_last_error() const -> plugin_load_error override;
    auto get_last_error_message() const -> std::string override;
    auto is_plugin_loaded(std::string_view plugin_name) const -> bool override;
    auto get_loaded_plugins() const -> std::vector<std::string> override;

private:
    /**
     * @struct library_handle
     * @brief Opaque handle to a loaded library
     *
     * Platform-specific:
     * - Unix: void* (from dlopen)
     * - Windows: HMODULE (from LoadLibrary)
     */
    struct library_handle;

    /**
     * @struct plugin_entry
     * @brief Information about a loaded plugin
     */
    struct plugin_entry {
        std::string name;
        std::string path;
        std::unique_ptr<library_handle> handle;
        destroy_plugin_fn destroy_fn;
        plugin_api_metadata metadata;
    };

    /**
     * @brief Load a library file
     * @param path Library file path
     * @return Library handle or nullptr
     */
    auto load_library(std::string_view path) -> std::unique_ptr<library_handle>;

    /**
     * @brief Unload a library
     * @param handle Library handle to unload
     */
    void unload_library(std::unique_ptr<library_handle> handle);

    /**
     * @brief Resolve a symbol from a library
     * @tparam T Function pointer type
     * @param handle Library handle
     * @param symbol_name Symbol name to resolve
     * @return Function pointer or nullptr
     */
    template <typename T>
    auto resolve_symbol(library_handle* handle, const char* symbol_name) -> T;

    /**
     * @brief Verify plugin API version compatibility
     * @param metadata Plugin API metadata
     * @return True if compatible
     */
    auto verify_api_version(const plugin_api_metadata& metadata) const -> bool;

    /**
     * @brief Set error state
     * @param error Error code
     * @param message Detailed error message
     */
    void set_error(plugin_load_error error, std::string message);

    // Loaded plugins tracking
    std::unordered_map<std::string, plugin_entry> loaded_plugins_;

    // Error state
    plugin_load_error last_error_{plugin_load_error::none};
    std::string last_error_message_;

    // Thread safety
    mutable std::mutex mutex_;
};

} // namespace monitoring
} // namespace kcenon
