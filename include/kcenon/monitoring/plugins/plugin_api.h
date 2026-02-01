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
 * @file plugin_api.h
 * @brief C API for dynamically loaded collector plugins
 *
 * This file defines the C ABI interface that all dynamically loaded plugins
 * must implement. Using a C interface ensures compatibility across different
 * C++ compilers and standard library versions.
 *
 * Plugin Requirements:
 * - Must export create_plugin() function
 * - Must export destroy_plugin() function
 * - Must export get_plugin_info() function
 * - All exported functions must use C linkage (extern "C")
 * - Must be compiled with the same plugin API version
 *
 * Example Plugin Implementation:
 * @code
 * #include "plugin_api.h"
 *
 * class my_plugin : public collector_plugin {
 *     // Implementation...
 * };
 *
 * extern "C" {
 *     PLUGIN_EXPORT collector_plugin* create_plugin() {
 *         return new my_plugin();
 *     }
 *
 *     PLUGIN_EXPORT void destroy_plugin(collector_plugin* plugin) {
 *         delete plugin;
 *     }
 *
 *     PLUGIN_EXPORT const plugin_metadata* get_plugin_info() {
 *         static plugin_metadata metadata = {
 *             PLUGIN_API_VERSION,
 *             "my_plugin",
 *             "1.0.0",
 *             "My Plugin Description"
 *         };
 *         return &metadata;
 *     }
 * }
 * @endcode
 */

#ifdef __cplusplus
extern "C" {
#endif

// Plugin API version (increment on breaking changes)
#define PLUGIN_API_VERSION 1

// Platform-specific export macro
#if defined(_WIN32) || defined(_WIN64)
    #define PLUGIN_EXPORT __declspec(dllexport)
#elif defined(__GNUC__) || defined(__clang__)
    #define PLUGIN_EXPORT __attribute__((visibility("default")))
#else
    #define PLUGIN_EXPORT
#endif

// Forward declaration for collector_plugin
// Actual definition is in collector_plugin.h (C++ header)
namespace kcenon { namespace monitoring { class collector_plugin; } }

/**
 * @struct plugin_api_metadata
 * @brief Plugin API metadata information
 *
 * This structure contains basic information about the plugin for the C API layer.
 * It must be returned by get_plugin_info() function.
 */
typedef struct plugin_api_metadata {
    /** Plugin API version this plugin was compiled against */
    int api_version;

    /** Plugin name (unique identifier) */
    const char* name;

    /** Plugin version string (semantic versioning recommended) */
    const char* version;

    /** Plugin description */
    const char* description;

    /** Plugin author (optional, can be NULL) */
    const char* author;

    /** Plugin category (optional, can be NULL) */
    const char* category;
} plugin_api_metadata;

/**
 * @brief Create a plugin instance
 * @return Pointer to newly created plugin instance
 *
 * This function must be exported by all plugins.
 * The returned pointer is managed by the plugin loader.
 * Do NOT delete the returned pointer directly - use destroy_plugin() instead.
 *
 * @note This function should never return NULL. If initialization fails,
 *       the plugin should return a valid instance that reports errors
 *       through is_available() or initialize() methods.
 */
typedef kcenon::monitoring::collector_plugin* (*create_plugin_fn)(void);

/**
 * @brief Destroy a plugin instance
 * @param plugin Plugin instance to destroy (created by create_plugin)
 *
 * This function must be exported by all plugins.
 * It is responsible for cleaning up all resources allocated by the plugin.
 * After this call, the plugin pointer becomes invalid.
 *
 * @note This function must handle NULL pointers gracefully (no-op).
 */
typedef void (*destroy_plugin_fn)(kcenon::monitoring::collector_plugin* plugin);

/**
 * @brief Get plugin API metadata
 * @return Pointer to plugin API metadata structure
 *
 * This function must be exported by all plugins.
 * The returned pointer must remain valid for the lifetime of the plugin.
 * The metadata is used to verify API version compatibility.
 *
 * @note The returned pointer should point to static storage.
 */
typedef const plugin_api_metadata* (*get_plugin_info_fn)(void);

/**
 * Plugin function names (for dlsym/GetProcAddress lookup)
 */
#define CREATE_PLUGIN_FN_NAME "create_plugin"
#define DESTROY_PLUGIN_FN_NAME "destroy_plugin"
#define GET_PLUGIN_INFO_FN_NAME "get_plugin_info"

/**
 * Helper macro for plugin implementation
 * Use this in your plugin source file to implement the required functions.
 *
 * Example:
 * @code
 * IMPLEMENT_PLUGIN(
 *     my_plugin,           // Plugin class name
 *     "my_plugin",         // Plugin name
 *     "1.0.0",            // Version
 *     "My Plugin",        // Description
 *     "Author Name",      // Author
 *     "hardware"          // Category
 * )
 * @endcode
 */
#ifdef __cplusplus
#define IMPLEMENT_PLUGIN(PluginClass, Name, Version, Description, Author, Category) \
    extern "C" { \
        PLUGIN_EXPORT struct collector_plugin* create_plugin() { \
            return new PluginClass(); \
        } \
        \
        PLUGIN_EXPORT void destroy_plugin(struct collector_plugin* plugin) { \
            delete plugin; \
        } \
        \
        PLUGIN_EXPORT const plugin_api_metadata* get_plugin_info() { \
            static plugin_api_metadata metadata = { \
                PLUGIN_API_VERSION, \
                Name, \
                Version, \
                Description, \
                Author, \
                Category \
            }; \
            return &metadata; \
        } \
    }
#endif

#ifdef __cplusplus
}
#endif
