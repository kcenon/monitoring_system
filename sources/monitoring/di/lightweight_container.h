#pragma once

/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, monitoring_system contributors
All rights reserved.
*****************************************************************************/

/**
 * @file lightweight_container.h
 * @brief Lightweight dependency injection container implementation
 * 
 * This file provides a lightweight DI container with no external dependencies,
 * suitable for use when thread_system is not available.
 */

#include "service_container_interface.h"
#include <unordered_map>
#include <mutex>
#include <memory>

namespace monitoring_system {

/**
 * @class lightweight_container
 * @brief Lightweight DI container implementation
 * 
 * This container provides basic dependency injection functionality
 * without requiring any external dependencies.
 */
class lightweight_container : public service_container_interface {
private:
    struct service_registration {
        std::function<std::any()> factory;
        service_lifetime lifetime;
        std::any cached_instance;  // For singleton/scoped instances
        
        service_registration() = default;
        service_registration(std::function<std::any()> f, service_lifetime l)
            : factory(std::move(f)), lifetime(l) {}
    };
    
    using type_key = std::type_index;
    using named_key = std::pair<std::type_index, std::string>;
    
    // Hash function for named_key
    struct named_key_hash {
        std::size_t operator()(const named_key& key) const {
            auto h1 = std::hash<std::type_index>{}(key.first);
            auto h2 = std::hash<std::string>{}(key.second);
            return h1 ^ (h2 << 1);
        }
    };
    
    // Service registrations
    std::unordered_map<type_key, service_registration> type_registrations_;
    std::unordered_map<named_key, service_registration, named_key_hash> named_registrations_;
    
    // Thread safety
    mutable std::mutex mutex_;
    
    // Parent container for scoped instances
    lightweight_container* parent_container_ = nullptr;
    
public:
    /**
     * @brief Default constructor
     */
    lightweight_container() = default;
    
    /**
     * @brief Constructor for scoped container
     * @param parent Parent container
     */
    explicit lightweight_container(lightweight_container* parent)
        : parent_container_(parent) {}
    
    /**
     * @brief Destructor
     */
    ~lightweight_container() override = default;
    
    // Disable copy
    lightweight_container(const lightweight_container&) = delete;
    lightweight_container& operator=(const lightweight_container&) = delete;
    
    // Disable move (can't move mutex)
    lightweight_container(lightweight_container&&) = delete;
    lightweight_container& operator=(lightweight_container&&) = delete;
    
    /**
     * @brief Create a scoped container
     * @return New scoped container with this as parent
     */
    std::unique_ptr<service_container_interface> create_scope() override {
        return std::make_unique<lightweight_container>(this);
    }
    
    /**
     * @brief Clear all registrations
     * @return Result indicating success
     */
    result_void clear() override {
        std::lock_guard<std::mutex> lock(mutex_);
        type_registrations_.clear();
        named_registrations_.clear();
        return result_void::success();
    }
    
protected:
    /**
     * @brief Register a factory function
     */
    result_void register_factory_impl(
        std::type_index type,
        std::function<std::any()> factory,
        service_lifetime lifetime) override {
        
        if (!factory) {
            return result_void(monitoring_error_code::invalid_configuration,
                             "Factory function cannot be null");
        }
        
        std::lock_guard<std::mutex> lock(mutex_);
        type_registrations_[type] = service_registration(factory, lifetime);
        return result_void::success();
    }
    
    /**
     * @brief Register a named factory function
     */
    result_void register_named_factory_impl(
        std::type_index type,
        const std::string& name,
        std::function<std::any()> factory,
        service_lifetime lifetime) override {
        
        if (!factory) {
            return result_void(monitoring_error_code::invalid_configuration,
                             "Factory function cannot be null");
        }
        
        if (name.empty()) {
            return result_void(monitoring_error_code::invalid_configuration,
                             "Service name cannot be empty");
        }
        
        std::lock_guard<std::mutex> lock(mutex_);
        named_key key{type, name};
        named_registrations_[key] = service_registration(factory, lifetime);
        return result_void::success();
    }
    
    /**
     * @brief Register a singleton instance
     */
    result_void register_singleton_impl(
        std::type_index type,
        std::any instance) override {
        
        if (!instance.has_value()) {
            return result_void(monitoring_error_code::invalid_configuration,
                             "Singleton instance cannot be null");
        }
        
        std::lock_guard<std::mutex> lock(mutex_);
        service_registration reg;
        reg.lifetime = service_lifetime::singleton;
        reg.cached_instance = instance;
        type_registrations_[type] = std::move(reg);
        return result_void::success();
    }
    
    /**
     * @brief Register a named singleton instance
     */
    result_void register_named_singleton_impl(
        std::type_index type,
        const std::string& name,
        std::any instance) override {
        
        if (!instance.has_value()) {
            return result_void(monitoring_error_code::invalid_configuration,
                             "Singleton instance cannot be null");
        }
        
        if (name.empty()) {
            return result_void(monitoring_error_code::invalid_configuration,
                             "Service name cannot be empty");
        }
        
        std::lock_guard<std::mutex> lock(mutex_);
        named_key key{type, name};
        service_registration reg;
        reg.lifetime = service_lifetime::singleton;
        reg.cached_instance = instance;
        named_registrations_[key] = std::move(reg);
        return result_void::success();
    }
    
    /**
     * @brief Resolve a service by type
     */
    result<std::any> resolve_impl(std::type_index type) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Check current container
        auto it = type_registrations_.find(type);
        if (it != type_registrations_.end()) {
            return resolve_registration(it->second);
        }
        
        // Check parent container for scoped resolution
        if (parent_container_) {
            return parent_container_->resolve_impl(type);
        }
        
        return make_error<std::any>(
            monitoring_error_code::collector_not_found,
            "Service type not registered"
        );
    }
    
    /**
     * @brief Resolve a named service
     */
    result<std::any> resolve_named_impl(
        std::type_index type,
        const std::string& name) override {
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        named_key key{type, name};
        auto it = named_registrations_.find(key);
        if (it != named_registrations_.end()) {
            return resolve_registration(it->second);
        }
        
        // Check parent container
        if (parent_container_) {
            return parent_container_->resolve_named_impl(type, name);
        }
        
        return make_error<std::any>(
            monitoring_error_code::collector_not_found,
            "Named service not registered: " + name
        );
    }
    
    /**
     * @brief Check if a service is registered
     */
    bool is_registered_impl(std::type_index type) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (type_registrations_.find(type) != type_registrations_.end()) {
            return true;
        }
        
        return parent_container_ ? parent_container_->is_registered_impl(type) : false;
    }
    
    /**
     * @brief Check if a named service is registered
     */
    bool is_named_registered_impl(
        std::type_index type,
        const std::string& name) const override {
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        named_key key{type, name};
        if (named_registrations_.find(key) != named_registrations_.end()) {
            return true;
        }
        
        return parent_container_ ? 
               parent_container_->is_named_registered_impl(type, name) : false;
    }
    
private:
    /**
     * @brief Resolve a service registration
     * @param reg The service registration
     * @return Resolved service instance
     */
    result<std::any> resolve_registration(service_registration& reg) {
        switch (reg.lifetime) {
            case service_lifetime::transient:
                // Always create new instance
                if (reg.factory) {
                    return make_success(reg.factory());
                }
                return make_error<std::any>(
                    monitoring_error_code::invalid_configuration,
                    "No factory for transient service"
                );
                
            case service_lifetime::scoped:
            case service_lifetime::singleton:
                // Return cached instance or create new one
                if (reg.cached_instance.has_value()) {
                    return make_success(reg.cached_instance);
                }
                if (reg.factory) {
                    reg.cached_instance = reg.factory();
                    return make_success(reg.cached_instance);
                }
                return make_error<std::any>(
                    monitoring_error_code::invalid_configuration,
                    "No factory or instance for service"
                );
                
            default:
                return make_error<std::any>(
                    monitoring_error_code::unknown_error,
                    "Unknown service lifetime"
                );
        }
    }
};

/**
 * @brief Factory function to create a lightweight container
 * @return Unique pointer to a new lightweight container
 */
inline std::unique_ptr<service_container_interface> create_lightweight_container() {
    return std::make_unique<lightweight_container>();
}

} // namespace monitoring_system