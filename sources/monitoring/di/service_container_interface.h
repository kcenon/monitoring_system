#pragma once

/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, monitoring_system contributors
All rights reserved.
*****************************************************************************/

/**
 * @file service_container_interface.h
 * @brief Abstract interface for dependency injection container
 * 
 * This file provides the abstract interface for service containers,
 * allowing different DI implementations without creating dependencies.
 */

#include "../core/result_types.h"
#include "../core/error_codes.h"
#include <memory>
#include <string>
#include <functional>
#include <typeindex>
#include <any>

namespace monitoring_system {

/**
 * @enum service_lifetime
 * @brief Service lifetime management options
 */
enum class service_lifetime {
    transient,  ///< New instance created each time
    scoped,     ///< Single instance per scope
    singleton   ///< Single instance for application lifetime
};

/**
 * @class service_container_interface
 * @brief Abstract interface for dependency injection container
 * 
 * This interface defines the contract for service containers,
 * providing registration and resolution of dependencies.
 */
class service_container_interface {
public:
    virtual ~service_container_interface() = default;
    
    /**
     * @brief Register a factory function for a service
     * @tparam T The service interface type
     * @param factory Factory function that creates the service
     * @param lifetime Service lifetime management
     * @return Result indicating success or error
     */
    template<typename T>
    result_void register_factory(
        std::function<std::shared_ptr<T>()> factory,
        service_lifetime lifetime = service_lifetime::transient) {
        return register_factory_impl(
            std::type_index(typeid(T)),
            [factory]() -> std::any { return factory(); },
            lifetime
        );
    }
    
    /**
     * @brief Register a factory with a name
     * @tparam T The service interface type
     * @param name Service name for named registration
     * @param factory Factory function
     * @param lifetime Service lifetime
     * @return Result indicating success or error
     */
    template<typename T>
    result_void register_factory(
        const std::string& name,
        std::function<std::shared_ptr<T>()> factory,
        service_lifetime lifetime = service_lifetime::transient) {
        return register_named_factory_impl(
            std::type_index(typeid(T)),
            name,
            [factory]() -> std::any { return factory(); },
            lifetime
        );
    }
    
    /**
     * @brief Register a singleton instance
     * @tparam T The service type
     * @param instance The singleton instance
     * @return Result indicating success or error
     */
    template<typename T>
    result_void register_singleton(std::shared_ptr<T> instance) {
        return register_singleton_impl(
            std::type_index(typeid(T)),
            instance
        );
    }
    
    /**
     * @brief Register a named singleton instance
     * @tparam T The service type
     * @param name Service name
     * @param instance The singleton instance
     * @return Result indicating success or error
     */
    template<typename T>
    result_void register_singleton(const std::string& name, 
                                  std::shared_ptr<T> instance) {
        return register_named_singleton_impl(
            std::type_index(typeid(T)),
            name,
            instance
        );
    }
    
    /**
     * @brief Resolve a service by type
     * @tparam T The service type to resolve
     * @return Result containing the service instance or error
     */
    template<typename T>
    result<std::shared_ptr<T>> resolve() {
        auto result = resolve_impl(std::type_index(typeid(T)));
        if (!result) {
            return result.get_error();
        }
        
        try {
            auto service = std::any_cast<std::shared_ptr<T>>(result.value());
            return make_success(service);
        } catch (const std::bad_any_cast&) {
            return make_error<std::shared_ptr<T>>(
                monitoring_error_code::invalid_configuration,
                "Type mismatch in service resolution"
            );
        }
    }
    
    /**
     * @brief Resolve a named service
     * @tparam T The service type
     * @param name Service name
     * @return Result containing the service instance or error
     */
    template<typename T>
    result<std::shared_ptr<T>> resolve(const std::string& name) {
        auto result = resolve_named_impl(std::type_index(typeid(T)), name);
        if (!result) {
            return result.get_error();
        }
        
        try {
            auto service = std::any_cast<std::shared_ptr<T>>(result.value());
            return make_success(service);
        } catch (const std::bad_any_cast&) {
            return make_error<std::shared_ptr<T>>(
                monitoring_error_code::invalid_configuration,
                "Type mismatch in named service resolution"
            );
        }
    }
    
    /**
     * @brief Check if a service is registered
     * @tparam T The service type
     * @return true if the service is registered
     */
    template<typename T>
    bool is_registered() const {
        return is_registered_impl(std::type_index(typeid(T)));
    }
    
    /**
     * @brief Check if a named service is registered
     * @tparam T The service type
     * @param name Service name
     * @return true if the named service is registered
     */
    template<typename T>
    bool is_registered(const std::string& name) const {
        return is_named_registered_impl(std::type_index(typeid(T)), name);
    }
    
    /**
     * @brief Create a scoped container
     * @return New scoped container instance
     */
    virtual std::unique_ptr<service_container_interface> create_scope() = 0;
    
    /**
     * @brief Clear all registrations
     * @return Result indicating success or error
     */
    virtual result_void clear() = 0;
    
protected:
    // Implementation methods to be overridden by concrete containers
    virtual result_void register_factory_impl(
        std::type_index type,
        std::function<std::any()> factory,
        service_lifetime lifetime) = 0;
    
    virtual result_void register_named_factory_impl(
        std::type_index type,
        const std::string& name,
        std::function<std::any()> factory,
        service_lifetime lifetime) = 0;
    
    virtual result_void register_singleton_impl(
        std::type_index type,
        std::any instance) = 0;
    
    virtual result_void register_named_singleton_impl(
        std::type_index type,
        const std::string& name,
        std::any instance) = 0;
    
    virtual result<std::any> resolve_impl(std::type_index type) = 0;
    
    virtual result<std::any> resolve_named_impl(
        std::type_index type,
        const std::string& name) = 0;
    
    virtual bool is_registered_impl(std::type_index type) const = 0;
    
    virtual bool is_named_registered_impl(
        std::type_index type,
        const std::string& name) const = 0;
};

/**
 * @class service_locator
 * @brief Global service locator for application-wide DI
 */
class service_locator {
private:
    static std::unique_ptr<service_container_interface> container_;
    
public:
    /**
     * @brief Set the global container
     * @param container The container to use globally
     */
    static void set_container(std::unique_ptr<service_container_interface> container) {
        container_ = std::move(container);
    }
    
    /**
     * @brief Get the global container
     * @return Pointer to the global container
     */
    static service_container_interface* get_container() {
        return container_.get();
    }
    
    /**
     * @brief Check if a global container is set
     * @return true if a container is available
     */
    static bool has_container() {
        return container_ != nullptr;
    }
    
    /**
     * @brief Reset the global container
     */
    static void reset() {
        container_.reset();
    }
};

// Static member definition
inline std::unique_ptr<service_container_interface> service_locator::container_ = nullptr;

} // namespace monitoring_system