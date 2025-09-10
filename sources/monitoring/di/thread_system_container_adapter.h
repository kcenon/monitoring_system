#pragma once

/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, monitoring_system contributors
All rights reserved.
*****************************************************************************/

/**
 * @file thread_system_container_adapter.h
 * @brief Adapter for thread_system's service container
 * 
 * This file provides an adapter that allows monitoring_system to use
 * thread_system's service container when available.
 */

#include "service_container_interface.h"
#include "lightweight_container.h"

#ifdef MONITORING_HAS_THREAD_SYSTEM
#include <thread_system/service_container.h>
#endif

namespace monitoring_system {

#ifdef MONITORING_HAS_THREAD_SYSTEM

/**
 * @class thread_system_container_adapter
 * @brief Adapter for thread_system's service container
 * 
 * This adapter wraps thread_system's service container to provide
 * monitoring_system's service_container_interface.
 */
class thread_system_container_adapter : public service_container_interface {
private:
    thread_module::service_container* thread_container_;
    bool owns_container_;
    
public:
    /**
     * @brief Constructor with existing container
     * @param container Pointer to thread_system's container
     * @param take_ownership Whether to take ownership of the container
     */
    explicit thread_system_container_adapter(
        thread_module::service_container* container,
        bool take_ownership = false)
        : thread_container_(container)
        , owns_container_(take_ownership) {
        
        if (!container) {
            throw std::invalid_argument("Thread container cannot be null");
        }
    }
    
    /**
     * @brief Create adapter with new container
     */
    thread_system_container_adapter()
        : thread_container_(new thread_module::service_container())
        , owns_container_(true) {}
    
    /**
     * @brief Destructor
     */
    ~thread_system_container_adapter() override {
        if (owns_container_ && thread_container_) {
            delete thread_container_;
        }
    }
    
    // Disable copy
    thread_system_container_adapter(const thread_system_container_adapter&) = delete;
    thread_system_container_adapter& operator=(const thread_system_container_adapter&) = delete;
    
    /**
     * @brief Create a scoped container
     * @return New scoped container
     */
    std::unique_ptr<service_container_interface> create_scope() override {
        // Thread system's container may have its own scoping mechanism
        // For now, return a lightweight container as scope
        return std::make_unique<lightweight_container>();
    }
    
    /**
     * @brief Clear all registrations
     * @return Result indicating success
     */
    result_void clear() override {
        // Thread system container may not support clearing
        return result_void(monitoring_error_code::operation_not_supported,
                          "Clear operation not supported by thread_system container");
    }
    
protected:
    /**
     * @brief Register a factory function
     */
    result_void register_factory_impl(
        std::type_index type,
        std::function<std::any()> factory,
        service_lifetime lifetime) override {
        
        try {
            // Convert lifetime to thread_system equivalent
            auto ts_lifetime = convert_lifetime(lifetime);
            
            // Register with thread_system container
            thread_container_->register_factory(
                type,
                [factory]() { return factory(); },
                ts_lifetime
            );
            
            return result_void::success();
        } catch (const std::exception& e) {
            return result_void(monitoring_error_code::adapter_initialization_failed,
                             std::string("Failed to register factory: ") + e.what());
        }
    }
    
    /**
     * @brief Register a named factory function
     */
    result_void register_named_factory_impl(
        std::type_index type,
        const std::string& name,
        std::function<std::any()> factory,
        service_lifetime lifetime) override {
        
        try {
            auto ts_lifetime = convert_lifetime(lifetime);
            
            // Thread system may support named registration differently
            thread_container_->register_named_factory(
                type,
                name,
                [factory]() { return factory(); },
                ts_lifetime
            );
            
            return result_void::success();
        } catch (const std::exception& e) {
            return result_void(monitoring_error_code::adapter_initialization_failed,
                             std::string("Failed to register named factory: ") + e.what());
        }
    }
    
    /**
     * @brief Register a singleton instance
     */
    result_void register_singleton_impl(
        std::type_index type,
        std::any instance) override {
        
        try {
            thread_container_->register_singleton(type, instance);
            return result_void::success();
        } catch (const std::exception& e) {
            return result_void(monitoring_error_code::adapter_initialization_failed,
                             std::string("Failed to register singleton: ") + e.what());
        }
    }
    
    /**
     * @brief Register a named singleton instance
     */
    result_void register_named_singleton_impl(
        std::type_index type,
        const std::string& name,
        std::any instance) override {
        
        try {
            thread_container_->register_named_singleton(type, name, instance);
            return result_void::success();
        } catch (const std::exception& e) {
            return result_void(monitoring_error_code::adapter_initialization_failed,
                             std::string("Failed to register named singleton: ") + e.what());
        }
    }
    
    /**
     * @brief Resolve a service by type
     */
    result<std::any> resolve_impl(std::type_index type) override {
        try {
            auto service = thread_container_->resolve(type);
            return make_success(service);
        } catch (const std::exception& e) {
            return make_error<std::any>(
                monitoring_error_code::collector_not_found,
                std::string("Failed to resolve service: ") + e.what()
            );
        }
    }
    
    /**
     * @brief Resolve a named service
     */
    result<std::any> resolve_named_impl(
        std::type_index type,
        const std::string& name) override {
        
        try {
            auto service = thread_container_->resolve_named(type, name);
            return make_success(service);
        } catch (const std::exception& e) {
            return make_error<std::any>(
                monitoring_error_code::collector_not_found,
                std::string("Failed to resolve named service: ") + e.what()
            );
        }
    }
    
    /**
     * @brief Check if a service is registered
     */
    bool is_registered_impl(std::type_index type) const override {
        return thread_container_->is_registered(type);
    }
    
    /**
     * @brief Check if a named service is registered
     */
    bool is_named_registered_impl(
        std::type_index type,
        const std::string& name) const override {
        
        return thread_container_->is_named_registered(type, name);
    }
    
private:
    /**
     * @brief Convert lifetime enum to thread_system equivalent
     */
    thread_module::service_lifetime convert_lifetime(service_lifetime lifetime) {
        switch (lifetime) {
            case service_lifetime::transient:
                return thread_module::service_lifetime::transient;
            case service_lifetime::scoped:
                return thread_module::service_lifetime::scoped;
            case service_lifetime::singleton:
                return thread_module::service_lifetime::singleton;
            default:
                throw std::invalid_argument("Unknown service lifetime");
        }
    }
};

/**
 * @brief Factory function to create a thread_system container adapter
 * @param container Optional existing thread_system container
 * @return Unique pointer to the adapter
 */
inline std::unique_ptr<service_container_interface> 
create_thread_system_adapter(thread_module::service_container* container = nullptr) {
    if (container) {
        return std::make_unique<thread_system_container_adapter>(container, false);
    }
    return std::make_unique<thread_system_container_adapter>();
}

#else // !MONITORING_HAS_THREAD_SYSTEM

/**
 * @brief Stub for when thread_system is not available
 */
inline std::unique_ptr<service_container_interface> 
create_thread_system_adapter(void* = nullptr) {
    // Fall back to lightweight container when thread_system is not available
    return create_lightweight_container();
}

#endif // MONITORING_HAS_THREAD_SYSTEM

} // namespace monitoring_system