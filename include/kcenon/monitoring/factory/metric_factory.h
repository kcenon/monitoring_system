// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
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
 * @file metric_factory.h
 * @brief Unified factory for metric collector instantiation
 *
 * This file provides a factory pattern implementation for creating and
 * configuring metric collectors. It centralizes collector creation,
 * reduces configuration duplication, and ensures consistent initialization.
 *
 * Usage:
 * @code
 * auto& factory = metric_factory::instance();
 *
 * // Create a collector with configuration
 * config_map config = {{"enabled", "true"}};
 * auto collector = factory.create("system_resource_collector", config);
 *
 * // Register a custom collector
 * factory.register_collector("my_collector", []() {
 *     return std::make_unique<my_collector>();
 * });
 * @endcode
 */

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "../utils/config_parser.h"

namespace kcenon::monitoring {

/**
 * @brief Forward declarations for collector types
 */
class metric_collector_plugin;

/**
 * @brief Base interface for type-erased collectors
 *
 * This provides a common interface that all collectors can implement,
 * allowing the factory to work with different collector types uniformly.
 */
class collector_interface {
   public:
    collector_interface() = default;
    virtual ~collector_interface() = default;

    // Non-copyable, non-moveable
    collector_interface(const collector_interface&) = delete;
    collector_interface& operator=(const collector_interface&) = delete;
    collector_interface(collector_interface&&) = delete;
    collector_interface& operator=(collector_interface&&) = delete;

    /**
     * Initialize the collector with configuration
     * @param config Configuration options
     * @return true if initialization successful
     */
    virtual bool initialize(const config_map& config) = 0;

    /**
     * Get the name of this collector
     * @return Collector name
     */
    [[nodiscard]] virtual std::string get_name() const = 0;

    /**
     * Check if the collector is healthy
     * @return true if collector is operational
     */
    [[nodiscard]] virtual bool is_healthy() const = 0;

    /**
     * Get supported metric types
     * @return Vector of supported metric type names
     */
    [[nodiscard]] virtual std::vector<std::string> get_metric_types() const = 0;
};

/**
 * @brief Result of collector creation
 */
struct create_result {
    std::unique_ptr<collector_interface> collector;
    bool success{false};
    std::string error_message;

    explicit operator bool() const { return success; }
};

/**
 * @brief Factory function type for creating collectors
 */
using collector_factory_fn = std::function<std::unique_ptr<collector_interface>()>;

/**
 * @class metric_factory
 * @brief Unified factory for metric collector instantiation
 *
 * This singleton factory provides centralized creation and configuration
 * of metric collectors. Features include:
 * - Type-safe collector registration
 * - Centralized configuration validation
 * - Consistent error handling
 * - Support for custom collectors
 *
 * Thread-safe for all operations.
 */
class metric_factory {
   public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the global factory instance
     */
    static metric_factory& instance() {
        static metric_factory instance;
        return instance;
    }

    ~metric_factory() = default;

    // Non-copyable, non-moveable
    metric_factory(const metric_factory&) = delete;
    metric_factory& operator=(const metric_factory&) = delete;
    metric_factory(metric_factory&&) = delete;
    metric_factory& operator=(metric_factory&&) = delete;

    /**
     * @brief Register a collector factory function
     * @param name The collector name (identifier)
     * @param factory Factory function that creates the collector
     * @return true if registration successful, false if name already exists
     */
    bool register_collector(const std::string& name, collector_factory_fn factory) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (factories_.find(name) != factories_.end()) {
            return false;
        }
        factories_[name] = std::move(factory);
        return true;
    }

    /**
     * @brief Register a collector type using template
     * @tparam T The collector type (must derive from collector_interface)
     * @param name The collector name (identifier)
     * @return true if registration successful
     */
    template <typename T>
    bool register_collector(const std::string& name) {
        return register_collector(name, []() {
            return std::make_unique<T>();
        });
    }

    /**
     * @brief Unregister a collector
     * @param name The collector name to unregister
     * @return true if collector was registered and removed
     */
    bool unregister_collector(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        return factories_.erase(name) > 0;
    }

    /**
     * @brief Check if a collector is registered
     * @param name The collector name to check
     * @return true if collector is registered
     */
    bool is_registered(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return factories_.find(name) != factories_.end();
    }

    /**
     * @brief Get list of registered collector names
     * @return Vector of registered collector names
     */
    std::vector<std::string> get_registered_collectors() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> names;
        names.reserve(factories_.size());
        for (const auto& [name, _] : factories_) {
            names.push_back(name);
        }
        return names;
    }

    /**
     * @brief Create a collector instance
     * @param name The collector name
     * @param config Configuration options
     * @return create_result containing the collector or error information
     */
    create_result create(const std::string& name, const config_map& config = {}) {
        create_result result;

        // Find factory
        collector_factory_fn factory;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = factories_.find(name);
            if (it == factories_.end()) {
                result.error_message = "Unknown collector: " + name;
                return result;
            }
            factory = it->second;
        }

        // Create collector
        try {
            result.collector = factory();
            if (!result.collector) {
                result.error_message = "Factory returned null for: " + name;
                return result;
            }
        } catch (const std::exception& e) {
            result.error_message = "Failed to create collector '" + name + "': " + e.what();
            return result;
        } catch (...) {
            result.error_message = "Unknown error creating collector: " + name;
            return result;
        }

        // Initialize collector
        try {
            if (!result.collector->initialize(config)) {
                result.error_message = "Initialization failed for: " + name;
                result.collector.reset();
                return result;
            }
        } catch (const std::exception& e) {
            result.error_message =
                "Exception during initialization of '" + name + "': " + e.what();
            result.collector.reset();
            return result;
        } catch (...) {
            result.error_message = "Unknown error initializing collector: " + name;
            result.collector.reset();
            return result;
        }

        result.success = true;
        return result;
    }

    /**
     * @brief Create a collector and return raw pointer (for compatibility)
     * @param name The collector name
     * @param config Configuration options
     * @return Unique pointer to collector, or nullptr on failure
     */
    std::unique_ptr<collector_interface> create_or_null(const std::string& name,
                                                         const config_map& config = {}) {
        auto result = create(name, config);
        return std::move(result.collector);
    }

    /**
     * @brief Create multiple collectors from configuration
     * @param configs Map of collector name to configuration
     * @return Vector of created collectors (only successfully created ones)
     */
    std::vector<std::unique_ptr<collector_interface>> create_multiple(
        const std::unordered_map<std::string, config_map>& configs) {
        std::vector<std::unique_ptr<collector_interface>> collectors;
        collectors.reserve(configs.size());

        for (const auto& [name, config] : configs) {
            auto result = create(name, config);
            if (result) {
                collectors.push_back(std::move(result.collector));
            }
        }

        return collectors;
    }

    /**
     * @brief Clear all registered collectors
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        factories_.clear();
    }

   private:
    metric_factory() = default;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, collector_factory_fn> factories_;
};

/**
 * @brief Helper macro for registering collectors at static initialization
 *
 * Usage:
 * @code
 * REGISTER_COLLECTOR(my_collector);
 * @endcode
 */
#define REGISTER_COLLECTOR(CollectorType)                                   \
    namespace {                                                              \
    static const bool CollectorType##_registered = []() {                   \
        return kcenon::monitoring::metric_factory::instance()               \
            .register_collector<CollectorType>(#CollectorType);             \
    }();                                                                     \
    }

}  // namespace kcenon::monitoring
