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

#include "kcenon/monitoring/plugins/plugin_loader.h"

#include <cstring>
#include <sstream>

// Platform-specific includes
#if defined(_WIN32) || defined(_WIN64)
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

namespace kcenon {
namespace monitoring {

// Platform-specific library handle implementation
struct dynamic_plugin_loader::library_handle {
#if defined(_WIN32) || defined(_WIN64)
    HMODULE handle{nullptr};
#else
    void* handle{nullptr};
#endif

    library_handle() = default;
    ~library_handle() = default;

    library_handle(const library_handle&) = delete;
    library_handle& operator=(const library_handle&) = delete;
    library_handle(library_handle&&) = default;
    library_handle& operator=(library_handle&&) = default;
};

// Error code to string conversion
auto to_string(plugin_load_error error) -> std::string {
    switch (error) {
        case plugin_load_error::none:
            return "No error";
        case plugin_load_error::file_not_found:
            return "Plugin file not found";
        case plugin_load_error::library_load_failed:
            return "Failed to load library";
        case plugin_load_error::symbol_not_found:
            return "Required symbol not found in library";
        case plugin_load_error::incompatible_api_version:
            return "Incompatible plugin API version";
        case plugin_load_error::create_function_failed:
            return "Plugin creation function failed";
        case plugin_load_error::plugin_unavailable:
            return "Plugin is not available on this platform";
        case plugin_load_error::already_loaded:
            return "Plugin is already loaded";
        case plugin_load_error::not_loaded:
            return "Plugin is not loaded";
        case plugin_load_error::invalid_metadata:
            return "Invalid plugin metadata";
        default:
            return "Unknown error";
    }
}

// dynamic_plugin_loader implementation

dynamic_plugin_loader::dynamic_plugin_loader() = default;

dynamic_plugin_loader::~dynamic_plugin_loader() {
    std::lock_guard<std::mutex> lock(mutex_);

    // Unload all plugins
    for (auto& [name, entry] : loaded_plugins_) {
        if (entry.destroy_fn && entry.handle) {
            // Note: Plugins should already be destroyed by the registry
            // We're just cleaning up the library handles here
        }
        unload_library(std::move(entry.handle));
    }
    loaded_plugins_.clear();
}

dynamic_plugin_loader::dynamic_plugin_loader(dynamic_plugin_loader&& other) noexcept
    : loaded_plugins_(std::move(other.loaded_plugins_)),
      last_error_(other.last_error_),
      last_error_message_(std::move(other.last_error_message_)) {
}

dynamic_plugin_loader& dynamic_plugin_loader::operator=(
    dynamic_plugin_loader&& other) noexcept {
    if (this != &other) {
        std::lock_guard<std::mutex> lock1(mutex_);
        std::lock_guard<std::mutex> lock2(other.mutex_);

        loaded_plugins_ = std::move(other.loaded_plugins_);
        last_error_ = other.last_error_;
        last_error_message_ = std::move(other.last_error_message_);
    }
    return *this;
}

auto dynamic_plugin_loader::load_plugin(std::string_view path)
    -> std::unique_ptr<collector_plugin> {
    std::lock_guard<std::mutex> lock(mutex_);

    // Load the library
    auto lib_handle = load_library(path);
    if (!lib_handle) {
        return nullptr;
    }

    // Get plugin info first to retrieve name and verify API version
    auto get_info_fn = resolve_symbol<get_plugin_info_fn>(
        lib_handle.get(), GET_PLUGIN_INFO_FN_NAME);
    if (!get_info_fn) {
        set_error(plugin_load_error::symbol_not_found,
                  "Symbol not found: " + std::string(GET_PLUGIN_INFO_FN_NAME));
        unload_library(std::move(lib_handle));
        return nullptr;
    }

    const plugin_api_metadata* metadata = get_info_fn();
    if (!metadata) {
        set_error(plugin_load_error::invalid_metadata,
                  "get_plugin_info() returned nullptr");
        unload_library(std::move(lib_handle));
        return nullptr;
    }

    // Verify API version compatibility
    if (!verify_api_version(*metadata)) {
        std::ostringstream oss;
        oss << "Incompatible API version: plugin=" << metadata->api_version
            << ", expected=" << PLUGIN_API_VERSION;
        set_error(plugin_load_error::incompatible_api_version, oss.str());
        unload_library(std::move(lib_handle));
        return nullptr;
    }

    // Check if plugin with this name is already loaded
    std::string plugin_name(metadata->name);
    if (loaded_plugins_.find(plugin_name) != loaded_plugins_.end()) {
        set_error(plugin_load_error::already_loaded,
                  "Plugin already loaded: " + plugin_name);
        unload_library(std::move(lib_handle));
        return nullptr;
    }

    // Resolve create function
    auto create_fn = resolve_symbol<create_plugin_fn>(
        lib_handle.get(), CREATE_PLUGIN_FN_NAME);
    if (!create_fn) {
        set_error(plugin_load_error::symbol_not_found,
                  "Symbol not found: " + std::string(CREATE_PLUGIN_FN_NAME));
        unload_library(std::move(lib_handle));
        return nullptr;
    }

    // Resolve destroy function
    auto destroy_fn = resolve_symbol<destroy_plugin_fn>(
        lib_handle.get(), DESTROY_PLUGIN_FN_NAME);
    if (!destroy_fn) {
        set_error(plugin_load_error::symbol_not_found,
                  "Symbol not found: " + std::string(DESTROY_PLUGIN_FN_NAME));
        unload_library(std::move(lib_handle));
        return nullptr;
    }

    // Create plugin instance
    collector_plugin* raw_plugin = create_fn();
    if (!raw_plugin) {
        set_error(plugin_load_error::create_function_failed,
                  "create_plugin() returned nullptr");
        unload_library(std::move(lib_handle));
        return nullptr;
    }

    // Check if plugin is available on this platform
    if (!raw_plugin->is_available()) {
        destroy_fn(raw_plugin);
        set_error(plugin_load_error::plugin_unavailable,
                  "Plugin is not available on this platform: " + plugin_name);
        unload_library(std::move(lib_handle));
        return nullptr;
    }

    // Store plugin entry for tracking
    plugin_entry entry;
    entry.name = plugin_name;
    entry.path = std::string(path);
    entry.handle = std::move(lib_handle);
    entry.destroy_fn = destroy_fn;
    entry.metadata = *metadata;

    loaded_plugins_[plugin_name] = std::move(entry);

    // Clear error state
    last_error_ = plugin_load_error::none;
    last_error_message_.clear();

    return std::unique_ptr<collector_plugin>(raw_plugin);
}

auto dynamic_plugin_loader::unload_plugin(std::string_view plugin_name) -> bool {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = loaded_plugins_.find(std::string(plugin_name));
    if (it == loaded_plugins_.end()) {
        set_error(plugin_load_error::not_loaded,
                  "Plugin not loaded: " + std::string(plugin_name));
        return false;
    }

    // Note: Plugin instance should be destroyed before calling this
    // We only unload the library here
    unload_library(std::move(it->second.handle));
    loaded_plugins_.erase(it);

    last_error_ = plugin_load_error::none;
    last_error_message_.clear();

    return true;
}

auto dynamic_plugin_loader::get_last_error() const -> plugin_load_error {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_error_;
}

auto dynamic_plugin_loader::get_last_error_message() const -> std::string {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_error_message_;
}

auto dynamic_plugin_loader::is_plugin_loaded(std::string_view plugin_name) const
    -> bool {
    std::lock_guard<std::mutex> lock(mutex_);
    return loaded_plugins_.find(std::string(plugin_name)) != loaded_plugins_.end();
}

auto dynamic_plugin_loader::get_loaded_plugins() const
    -> std::vector<std::string> {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> result;
    result.reserve(loaded_plugins_.size());
    for (const auto& [name, entry] : loaded_plugins_) {
        result.push_back(name);
    }
    return result;
}

// Private methods

auto dynamic_plugin_loader::load_library(std::string_view path)
    -> std::unique_ptr<library_handle> {
    auto handle = std::make_unique<library_handle>();

#if defined(_WIN32) || defined(_WIN64)
    // Windows: Use LoadLibraryA for narrow strings
    handle->handle = LoadLibraryA(std::string(path).c_str());
    if (!handle->handle) {
        DWORD error = GetLastError();
        std::ostringstream oss;
        oss << "LoadLibrary failed for '" << path << "': error code " << error;
        set_error(plugin_load_error::library_load_failed, oss.str());
        return nullptr;
    }
#else
    // Unix: Use dlopen with RTLD_LAZY | RTLD_LOCAL
    handle->handle = dlopen(std::string(path).c_str(), RTLD_LAZY | RTLD_LOCAL);
    if (!handle->handle) {
        const char* error = dlerror();
        std::ostringstream oss;
        oss << "dlopen failed for '" << path << "': "
            << (error ? error : "unknown error");
        set_error(plugin_load_error::library_load_failed, oss.str());
        return nullptr;
    }
#endif

    return handle;
}

void dynamic_plugin_loader::unload_library(std::unique_ptr<library_handle> handle) {
    if (!handle || !handle->handle) {
        return;
    }

#if defined(_WIN32) || defined(_WIN64)
    FreeLibrary(handle->handle);
#else
    dlclose(handle->handle);
#endif
}

template <typename T>
auto dynamic_plugin_loader::resolve_symbol(library_handle* handle,
                                             const char* symbol_name) -> T {
    if (!handle || !handle->handle) {
        return nullptr;
    }

#if defined(_WIN32) || defined(_WIN64)
    // Windows: Use GetProcAddress
    FARPROC proc = GetProcAddress(handle->handle, symbol_name);
    return reinterpret_cast<T>(proc);
#else
    // Unix: Use dlsym
    // Clear any previous error
    dlerror();
    void* symbol = dlsym(handle->handle, symbol_name);
    // Check for error (nullptr might be a valid symbol value)
    const char* error = dlerror();
    if (error) {
        return nullptr;
    }
    return reinterpret_cast<T>(symbol);
#endif
}

auto dynamic_plugin_loader::verify_api_version(
    const plugin_api_metadata& metadata) const -> bool {
    return metadata.api_version == PLUGIN_API_VERSION;
}

void dynamic_plugin_loader::set_error(plugin_load_error error,
                                       std::string message) {
    last_error_ = error;
    last_error_message_ = std::move(message);
}

} // namespace monitoring
} // namespace kcenon
