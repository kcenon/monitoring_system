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

#include "kcenon/monitoring/plugins/collector_registry.h"

#include <algorithm>

namespace kcenon {
namespace monitoring {

auto collector_registry::instance() -> collector_registry& {
    static collector_registry instance;
    return instance;
}

auto collector_registry::load_plugin(std::string_view path) -> bool {
    std::lock_guard<std::mutex> lock(mutex_);

    // Lazy initialization of plugin loader
    if (!plugin_loader_) {
        plugin_loader_ = std::make_unique<dynamic_plugin_loader>();
    }

    // Load the plugin from shared library
    auto plugin = plugin_loader_->load_plugin(path);
    if (!plugin) {
        return false;
    }

    // Get plugin name before moving
    auto name = std::string(plugin->name());

    // Check for duplicates
    if (plugins_.find(name) != plugins_.end()) {
        return false;
    }

    // Register the plugin
    plugins_[name] = std::move(plugin);
    initialized_[name] = false;

    return true;
}

auto collector_registry::unload_plugin(std::string_view name) -> bool {
    std::lock_guard<std::mutex> lock(mutex_);

    // Check if plugin exists
    auto it = plugins_.find(std::string(name));
    if (it == plugins_.end()) {
        return false;
    }

    // Shutdown if initialized
    if (initialized_[std::string(name)]) {
        it->second->shutdown();
        initialized_[std::string(name)] = false;
    }

    // Remove from plugins map (this destroys the plugin instance)
    plugins_.erase(it);
    initialized_.erase(std::string(name));

    // Unload the shared library
    if (plugin_loader_) {
        return plugin_loader_->unload_plugin(name);
    }

    return true;
}

auto collector_registry::get_plugin_loader_error() const -> std::string {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!plugin_loader_) {
        return "";
    }

    return plugin_loader_->get_last_error_message();
}

auto collector_registry::register_plugin(std::unique_ptr<collector_plugin> plugin)
    -> bool {
    if (!plugin) {
        return false;
    }

    // Check availability before registration
    if (!plugin->is_available()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    auto name = std::string(plugin->name());

    // Check for duplicates
    if (plugins_.find(name) != plugins_.end()) {
        return false;
    }

    plugins_[name] = std::move(plugin);
    initialized_[name] = false;

    return true;
}

auto collector_registry::unregister_plugin(std::string_view name) -> bool {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = plugins_.find(std::string(name));
    if (it == plugins_.end()) {
        return false;
    }

    // Shutdown before removal
    if (initialized_[std::string(name)]) {
        it->second->shutdown();
        initialized_[std::string(name)] = false;
    }

    plugins_.erase(it);
    initialized_.erase(std::string(name));

    return true;
}

auto collector_registry::get_plugin(std::string_view name) -> collector_plugin* {
    std::lock_guard<std::mutex> lock(mutex_);

    // Try to find existing plugin
    auto it = plugins_.find(std::string(name));
    if (it != plugins_.end()) {
        return it->second.get();
    }

    // Try to instantiate from factory
    if (instantiate_from_factory(std::string(name))) {
        it = plugins_.find(std::string(name));
        if (it != plugins_.end()) {
            return it->second.get();
        }
    }

    return nullptr;
}

auto collector_registry::get_plugins() -> std::vector<collector_plugin*> {
    std::lock_guard<std::mutex> lock(mutex_);

    // Instantiate all factory-registered plugins
    for (const auto& [name, factory] : factories_) {
        instantiate_from_factory(name);
    }

    std::vector<collector_plugin*> result;
    result.reserve(plugins_.size());

    for (const auto& [name, plugin] : plugins_) {
        result.push_back(plugin.get());
    }

    return result;
}

auto collector_registry::get_plugins_by_category(plugin_category category)
    -> std::vector<collector_plugin*> {
    std::lock_guard<std::mutex> lock(mutex_);

    // Instantiate all factory-registered plugins
    for (const auto& [name, factory] : factories_) {
        instantiate_from_factory(name);
    }

    std::vector<collector_plugin*> result;

    for (const auto& [name, plugin] : plugins_) {
        if (plugin->get_metadata().category == category) {
            result.push_back(plugin.get());
        }
    }

    return result;
}

auto collector_registry::initialize_all(const config_map& config) -> size_t {
    std::lock_guard<std::mutex> lock(mutex_);

    // Instantiate all factory-registered plugins before initialization
    for (const auto& [name, factory] : factories_) {
        instantiate_from_factory(name);
    }

    size_t success_count = 0;

    for (const auto& [name, plugin] : plugins_) {
        if (initialized_[name]) {
            ++success_count;
            continue;
        }

        if (plugin->initialize(config)) {
            initialized_[name] = true;
            ++success_count;
        }
    }

    return success_count;
}

void collector_registry::shutdown_all() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (shutdown_) {
        return;
    }

    // Shutdown in reverse order of registration
    // Collect plugin names first since unordered_map doesn't support reverse iteration
    std::vector<std::string> plugin_names;
    plugin_names.reserve(plugins_.size());
    for (const auto& [name, _] : plugins_) {
        plugin_names.push_back(name);
    }

    // Shutdown in reverse order
    for (auto it = plugin_names.rbegin(); it != plugin_names.rend(); ++it) {
        const auto& name = *it;
        const auto& plugin = plugins_[name];

        if (initialized_[name]) {
            plugin->shutdown();
            initialized_[name] = false;
        }
    }

    shutdown_ = true;
}

auto collector_registry::get_registry_stats() const -> std::map<std::string, size_t> {
    std::lock_guard<std::mutex> lock(mutex_);

    std::map<std::string, size_t> stats;

    stats["total_plugins"] = plugins_.size();

    size_t initialized_count = 0;
    size_t available_count = 0;

    std::map<plugin_category, size_t> category_counts;

    for (const auto& [name, plugin] : plugins_) {
        if (initialized_.at(name)) {
            ++initialized_count;
        }

        if (plugin->is_available()) {
            ++available_count;
        }

        auto category = plugin->get_metadata().category;
        ++category_counts[category];
    }

    stats["initialized_plugins"] = initialized_count;
    stats["available_plugins"] = available_count;

    // Category-specific counts
    const char* category_names[] = {
        "system", "hardware", "platform", "network", "process", "custom"
    };

    for (size_t i = 0; i < sizeof(category_names) / sizeof(category_names[0]); ++i) {
        auto category = static_cast<plugin_category>(i);
        auto count = category_counts[category];
        if (count > 0) {
            stats[std::string("category_") + category_names[i] + "_count"] = count;
        }
    }

    return stats;
}

auto collector_registry::has_plugin(std::string_view name) const -> bool {
    std::lock_guard<std::mutex> lock(mutex_);

    auto name_str = std::string(name);
    return plugins_.find(name_str) != plugins_.end() ||
           factories_.find(name_str) != factories_.end();
}

auto collector_registry::plugin_count() const -> size_t {
    std::lock_guard<std::mutex> lock(mutex_);
    return plugins_.size() + factories_.size();
}

void collector_registry::clear() {
    shutdown_all();

    std::lock_guard<std::mutex> lock(mutex_);

    plugins_.clear();
    factories_.clear();
    initialized_.clear();
    shutdown_ = false;
}

collector_registry::~collector_registry() {
    shutdown_all();
}

auto collector_registry::instantiate_from_factory(const std::string& name) -> bool {
    // Caller must hold mutex_

    // Check if already instantiated
    if (plugins_.find(name) != plugins_.end()) {
        return true;
    }

    // Check if factory exists
    auto factory_it = factories_.find(name);
    if (factory_it == factories_.end()) {
        return false;
    }

    // Instantiate plugin
    auto plugin = factory_it->second();
    if (!plugin) {
        return false;
    }

    // Check availability
    if (!plugin->is_available()) {
        return false;
    }

    // Register the instantiated plugin
    plugins_[name] = std::move(plugin);
    initialized_[name] = false;

    // Remove factory (no longer needed)
    factories_.erase(factory_it);

    return true;
}

} // namespace monitoring
} // namespace kcenon
