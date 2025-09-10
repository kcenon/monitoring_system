#pragma once

/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, monitoring_system contributors
All rights reserved.
*****************************************************************************/

/**
 * @file thread_context.h
 * @brief Thread-local context for monitoring metadata
 * 
 * This file provides thread context support for enriching monitoring data
 * with contextual information like request IDs, correlation IDs, and custom
 * metadata, following patterns from thread_system.
 */

#include "../core/result_types.h"
#include "../core/error_codes.h"
#include "../interfaces/monitoring_interface.h"
#include "../interfaces/monitorable_interface.h"
#include <string>
#include <unordered_map>
#include <memory>
#include <thread>
#include <chrono>
#include <optional>
#include <mutex>
#include <atomic>
#include <cstdio>

namespace monitoring_system {

/**
 * @struct context_metadata
 * @brief Metadata that can be attached to thread contexts
 */
struct context_metadata {
    std::string request_id;
    std::string correlation_id;
    std::string user_id;
    std::string session_id;
    std::string trace_id;
    std::string span_id;
    std::unordered_map<std::string, std::string> custom_tags;
    std::chrono::system_clock::time_point created_at;
    
    /**
     * @brief Default constructor
     */
    context_metadata()
        : created_at(std::chrono::system_clock::now()) {}
    
    /**
     * @brief Constructor with request ID
     * @param req_id Request identifier
     */
    explicit context_metadata(const std::string& req_id)
        : request_id(req_id)
        , created_at(std::chrono::system_clock::now()) {}
    
    /**
     * @brief Add a custom tag
     * @param key Tag key
     * @param value Tag value
     */
    void add_tag(const std::string& key, const std::string& value) {
        custom_tags[key] = value;
    }
    
    /**
     * @brief Get a custom tag
     * @param key Tag key
     * @return Optional containing the value if found
     */
    std::optional<std::string> get_tag(const std::string& key) const {
        auto it = custom_tags.find(key);
        if (it != custom_tags.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    /**
     * @brief Clear all metadata
     */
    void clear() {
        request_id.clear();
        correlation_id.clear();
        user_id.clear();
        session_id.clear();
        trace_id.clear();
        span_id.clear();
        custom_tags.clear();
    }
    
    /**
     * @brief Check if metadata is empty
     * @return true if all fields are empty
     */
    bool empty() const {
        return request_id.empty() && 
               correlation_id.empty() && 
               user_id.empty() &&
               session_id.empty() &&
               trace_id.empty() &&
               span_id.empty() &&
               custom_tags.empty();
    }
    
    /**
     * @brief Merge another metadata into this one
     * @param other Metadata to merge
     * @param overwrite If true, overwrite existing values
     */
    void merge(const context_metadata& other, bool overwrite = false) {
        if (overwrite || request_id.empty()) request_id = other.request_id;
        if (overwrite || correlation_id.empty()) correlation_id = other.correlation_id;
        if (overwrite || user_id.empty()) user_id = other.user_id;
        if (overwrite || session_id.empty()) session_id = other.session_id;
        if (overwrite || trace_id.empty()) trace_id = other.trace_id;
        if (overwrite || span_id.empty()) span_id = other.span_id;
        
        for (const auto& [key, value] : other.custom_tags) {
            if (overwrite || custom_tags.find(key) == custom_tags.end()) {
                custom_tags[key] = value;
            }
        }
    }
};

/**
 * @class thread_context
 * @brief Thread-local storage for monitoring context
 * 
 * This class manages thread-specific context that can be used to
 * enrich monitoring data with contextual information.
 */
class thread_context {
private:
    static thread_local std::unique_ptr<context_metadata> current_context_;
    static std::atomic<uint64_t> context_counter_;
    
public:
    /**
     * @brief Get the current thread's context
     * @return Pointer to context metadata or nullptr if not set
     */
    static context_metadata* current() {
        return current_context_.get();
    }
    
    /**
     * @brief Set the current thread's context
     * @param metadata Context metadata to set
     */
    static void set_current(std::unique_ptr<context_metadata> metadata) {
        current_context_ = std::move(metadata);
    }
    
    /**
     * @brief Create and set a new context for the current thread
     * @param request_id Optional request ID
     * @return Reference to the created context
     */
    static context_metadata& create(const std::string& request_id = "") {
        auto metadata = std::make_unique<context_metadata>(request_id);
        if (request_id.empty()) {
            // Generate a unique request ID if not provided
            metadata->request_id = generate_request_id();
        }
        current_context_ = std::move(metadata);
        return *current_context_;
    }
    
    /**
     * @brief Clear the current thread's context
     */
    static void clear() {
        current_context_.reset();
    }
    
    /**
     * @brief Check if current thread has a context
     * @return true if context is set
     */
    static bool has_context() {
        return current_context_ != nullptr;
    }
    
    /**
     * @brief Generate a unique request ID
     * @return Generated request ID
     */
    static std::string generate_request_id() {
        auto tid = std::this_thread::get_id();
        auto counter = context_counter_.fetch_add(1);
        auto timestamp = std::chrono::system_clock::now().time_since_epoch().count();
        
        // Create a unique ID from thread ID, counter, and timestamp
        std::hash<std::thread::id> hasher;
        auto tid_hash = hasher(tid);
        
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "%zx-%llu-%llx", 
                tid_hash, counter, static_cast<unsigned long long>(timestamp));
        return std::string(buffer);
    }
    
    /**
     * @brief Generate a unique correlation ID
     * @return Generated correlation ID
     */
    static std::string generate_correlation_id() {
        return "corr-" + generate_request_id();
    }
    
    /**
     * @brief Copy context from another thread (for propagation)
     * @param source Source context to copy
     * @return Result indicating success
     */
    static result_void copy_from(const context_metadata& source) {
        try {
            current_context_ = std::make_unique<context_metadata>(source);
            return result_void::success();
        } catch (const std::exception& e) {
            return result_void(monitoring_error_code::out_of_memory,
                             std::string("Failed to copy context: ") + e.what());
        }
    }
};

// Static member declarations are in the class definition above
// Definitions are in thread_context.cpp

/**
 * @class context_scope
 * @brief RAII wrapper for managing thread context lifecycle
 * 
 * This class ensures that thread context is properly cleaned up
 * when leaving a scope.
 */
class context_scope {
private:
    std::unique_ptr<context_metadata> previous_context_;
    bool should_restore_;
    
public:
    /**
     * @brief Constructor that sets new context
     * @param metadata Context to set
     * @param preserve_previous If true, restore previous context on destruction
     */
    explicit context_scope(std::unique_ptr<context_metadata> metadata, 
                          bool preserve_previous = false)
        : should_restore_(preserve_previous) {
        
        if (should_restore_ && thread_context::has_context()) {
            // Save current context
            previous_context_ = std::make_unique<context_metadata>(*thread_context::current());
        }
        
        thread_context::set_current(std::move(metadata));
    }
    
    /**
     * @brief Constructor that creates new context with request ID
     * @param request_id Request identifier
     */
    explicit context_scope(const std::string& request_id)
        : should_restore_(false) {
        thread_context::create(request_id);
    }
    
    /**
     * @brief Destructor - restores or clears context
     */
    ~context_scope() {
        if (should_restore_ && previous_context_) {
            thread_context::set_current(std::move(previous_context_));
        } else {
            thread_context::clear();
        }
    }
    
    // Disable copy
    context_scope(const context_scope&) = delete;
    context_scope& operator=(const context_scope&) = delete;
    
    // Enable move
    context_scope(context_scope&&) = default;
    context_scope& operator=(context_scope&&) = default;
};

/**
 * @class context_propagator
 * @brief Utility for propagating context across thread boundaries
 */
class context_propagator {
private:
    std::shared_ptr<context_metadata> captured_context_;
    
public:
    /**
     * @brief Default constructor
     */
    context_propagator() = default;
    
    /**
     * @brief Capture current thread's context
     * @return Result indicating success
     */
    result_void capture() {
        if (!thread_context::has_context()) {
            return result_void(monitoring_error_code::configuration_not_found,
                             "No context to capture");
        }
        
        try {
            captured_context_ = std::make_shared<context_metadata>(*thread_context::current());
            return result_void::success();
        } catch (const std::exception& e) {
            return result_void(monitoring_error_code::out_of_memory,
                             std::string("Failed to capture context: ") + e.what());
        }
    }
    
    /**
     * @brief Apply captured context to current thread
     * @return Result indicating success
     */
    result_void apply() const {
        if (!captured_context_) {
            return result_void(monitoring_error_code::configuration_not_found,
                             "No captured context to apply");
        }
        
        return thread_context::copy_from(*captured_context_);
    }
    
    /**
     * @brief Get the captured context
     * @return Shared pointer to captured context or nullptr
     */
    std::shared_ptr<context_metadata> get_captured() const {
        return captured_context_;
    }
    
    /**
     * @brief Check if context has been captured
     * @return true if context is captured
     */
    bool has_captured() const {
        return captured_context_ != nullptr;
    }
    
    /**
     * @brief Clear captured context
     */
    void clear() {
        captured_context_.reset();
    }
    
    /**
     * @brief Create a propagator with current context
     * @return Propagator with captured context or empty on error
     */
    static context_propagator from_current() {
        context_propagator prop;
        prop.capture(); // Ignore error, will have empty propagator
        return prop;
    }
};

/**
 * @class context_aware_monitoring
 * @brief Interface for monitoring components that use thread context
 */
class context_aware_monitoring {
public:
    virtual ~context_aware_monitoring() = default;
    
    /**
     * @brief Enrich monitoring data with thread context
     * @param data Monitoring data to enrich
     * @param context Thread context to use (or current if nullptr)
     * @return Result indicating success
     */
    virtual result_void enrich_with_context(
        monitoring_data& data,
        const context_metadata* context = nullptr) const {
        
        const context_metadata* ctx = context;
        if (!ctx) {
            ctx = thread_context::current();
        }
        
        if (!ctx) {
            // No context available, not an error
            return result_void::success();
        }
        
        // Add context metadata as tags
        if (!ctx->request_id.empty()) {
            data.add_tag("request_id", ctx->request_id);
        }
        if (!ctx->correlation_id.empty()) {
            data.add_tag("correlation_id", ctx->correlation_id);
        }
        if (!ctx->user_id.empty()) {
            data.add_tag("user_id", ctx->user_id);
        }
        if (!ctx->session_id.empty()) {
            data.add_tag("session_id", ctx->session_id);
        }
        if (!ctx->trace_id.empty()) {
            data.add_tag("trace_id", ctx->trace_id);
        }
        if (!ctx->span_id.empty()) {
            data.add_tag("span_id", ctx->span_id);
        }
        
        // Add custom tags with prefix
        for (const auto& [key, value] : ctx->custom_tags) {
            data.add_tag("ctx." + key, value);
        }
        
        return result_void::success();
    }
    
    /**
     * @brief Check if context-aware monitoring is enabled
     * @return true if enabled
     */
    virtual bool is_context_aware_enabled() const {
        return true;
    }
};

/**
 * @class context_metrics_collector
 * @brief Collector that automatically includes thread context in metrics
 */
class context_metrics_collector : public metrics_collector,
                                 public context_aware_monitoring {
protected:
    std::string collector_name_;
    bool enabled_ = true;
    bool context_aware_ = true;
    
public:
    /**
     * @brief Constructor
     * @param name Collector name
     */
    explicit context_metrics_collector(const std::string& name)
        : collector_name_(name) {}
    
    /**
     * @brief Get collector name
     * @return Collector identifier
     */
    std::string get_name() const override {
        return collector_name_;
    }
    
    /**
     * @brief Check if collector is enabled
     * @return true if enabled
     */
    bool is_enabled() const override {
        return enabled_;
    }
    
    /**
     * @brief Enable or disable the collector
     * @param enable true to enable
     * @return Result indicating success
     */
    result_void set_enabled(bool enable) override {
        enabled_ = enable;
        return result_void::success();
    }
    
    /**
     * @brief Initialize the collector
     * @return Result indicating success
     */
    result_void initialize() override {
        return result_void::success();
    }
    
    /**
     * @brief Cleanup collector resources
     * @return Result indicating success
     */
    result_void cleanup() override {
        return result_void::success();
    }
    
    /**
     * @brief Check if context-aware collection is enabled
     * @return true if enabled
     */
    bool is_context_aware_enabled() const override {
        return context_aware_;
    }
    
    /**
     * @brief Enable or disable context-aware collection
     * @param enable true to enable
     */
    void set_context_aware(bool enable) {
        context_aware_ = enable;
    }
    
protected:
    /**
     * @brief Helper to create snapshot with context
     * @param source_id Source identifier
     * @return Metrics snapshot with context
     */
    metrics_snapshot create_snapshot_with_context(const std::string& source_id = "") {
        metrics_snapshot snapshot;
        snapshot.source_id = source_id.empty() ? collector_name_ : source_id;
        
        // Add context if available and enabled
        if (context_aware_ && thread_context::has_context()) {
            auto* ctx = thread_context::current();
            if (ctx) {
                // Add context metadata to snapshot
                monitoring_data temp_data;
                enrich_with_context(temp_data, ctx);
                
                // Copy tags to snapshot metrics as tags
                for (const auto& [key, value] : temp_data.get_tags()) {
                    // Add as metric tag (simplified approach)
                    // In real implementation, metrics would have individual tags
                }
            }
        }
        
        return snapshot;
    }
};

} // namespace monitoring_system