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
 * @file thread_context.h
 * @brief Thread-local context management for request tracking and distributed tracing.
 *
 * @details Provides thread-local storage for request metadata (request IDs,
 * correlation IDs, trace/span IDs) enabling distributed tracing and
 * per-request diagnostics across the monitoring system.
 *
 * Two APIs are provided:
 * - thread_context: Primary API using pointer-based thread-local storage
 * - thread_context_manager: Legacy API using std::optional-based storage
 *
 * ### Thread Safety
 * Each thread has its own independent context via thread_local storage.
 * No cross-thread synchronization is needed for context operations.
 *
 * @code
 * // Set up context for an incoming request
 * auto& ctx = thread_context::create("req-12345");
 * ctx.correlation_id = "corr-67890";
 * ctx.add_tag("service", "monitoring");
 *
 * // Later, retrieve the context
 * if (auto* ctx = thread_context::current()) {
 *     log("Processing request: " + ctx->request_id);
 * }
 *
 * // Clean up when done
 * thread_context::clear();
 * @endcode
 *
 * @author kcenon
 * @since 1.0.0
 * @see distributed_tracer For span-level tracing integration
 * @see performance_monitor For metrics correlated with request contexts
 */

#include <string>
#include <chrono>
#include <optional>
#include <unordered_map>
#include <memory>

namespace kcenon { namespace monitoring {

/**
 * @brief Context metadata for thread-specific information.
 *
 * @details Lightweight metadata struct for attaching request-scoped
 * information to the current thread. Useful for correlating log
 * entries and metrics with specific requests.
 */
struct context_metadata {
    std::string request_id;       ///< Unique identifier for the current request
    std::string correlation_id;   ///< Correlation ID for tracing across services
    std::string user_id;          ///< User identifier associated with the request
    std::unordered_map<std::string, std::string> tags; ///< Arbitrary key-value tags

    /**
     * @brief Construct context metadata with an optional request ID.
     * @param req_id Request identifier (defaults to empty string)
     */
    explicit context_metadata(std::string req_id = "")
        : request_id(std::move(req_id)) {}

    /**
     * @brief Check if all metadata fields are empty.
     * @return true if no metadata has been set, false otherwise
     */
    bool empty() const {
        return request_id.empty() && correlation_id.empty() && user_id.empty() && tags.empty();
    }

    /**
     * @brief Set a custom tag on this context.
     * @param key Tag name
     * @param value Tag value
     */
    void set_tag(const std::string& key, const std::string& value) {
        tags[key] = value;
    }

    /**
     * @brief Retrieve a tag value by key.
     * @param key Tag name to look up
     * @return The tag value, or empty string if not found
     */
    std::string get_tag(const std::string& key) const {
        auto it = tags.find(key);
        return it != tags.end() ? it->second : "";
    }
};

/**
 * @brief Enhanced thread context data for comprehensive request and trace tracking.
 *
 * @details Extends basic context metadata with distributed tracing fields
 * (span_id, trace_id, parent_span_id) and timing information. Each instance
 * records its creation time for elapsed duration calculations.
 *
 * @see thread_context For managing thread-local instances of this struct
 */
struct thread_context_data {
    std::string request_id;       ///< Unique identifier for the current request
    std::string correlation_id;   ///< Correlation ID for cross-service tracing
    std::string user_id;          ///< User identifier associated with the request
    std::string span_id;          ///< Current span ID for distributed tracing
    std::string trace_id;         ///< Trace ID linking all spans in a trace
    std::chrono::steady_clock::time_point start_time; ///< Context creation timestamp
    std::optional<std::string> parent_span_id; ///< Parent span ID (if nested)
    std::unordered_map<std::string, std::string> tags; ///< Arbitrary key-value tags

    /**
     * @brief Default constructor. Records the current time as start_time.
     */
    thread_context_data() : start_time(std::chrono::steady_clock::now()) {}

    /**
     * @brief Construct with a request ID. Records the current time as start_time.
     * @param req_id Request identifier
     */
    explicit thread_context_data(std::string req_id)
        : request_id(std::move(req_id))
        , start_time(std::chrono::steady_clock::now()) {}

    /**
     * @brief Add or update a custom tag.
     * @param key Tag name
     * @param value Tag value
     */
    void add_tag(const std::string& key, const std::string& value) {
        tags[key] = value;
    }

    /**
     * @brief Retrieve a tag value by key.
     * @param key Tag name to look up
     * @return The tag value, or empty string if not found
     */
    std::string get_tag(const std::string& key) const {
        auto it = tags.find(key);
        return it != tags.end() ? it->second : "";
    }
};

/**
 * @brief Thread-local context management for request tracking.
 *
 * @details Manages a thread_context_data instance in thread-local storage.
 * Each thread has its own independent context, so no locking is required.
 * Use create() to initialize a context at the start of request processing,
 * and clear() to release it when done.
 *
 * @code
 * // At request entry point
 * auto& ctx = thread_context::create();
 * ctx.correlation_id = thread_context::generate_correlation_id();
 *
 * // In downstream code
 * if (thread_context::has_context()) {
 *     auto* ctx = thread_context::current();
 *     // use ctx->request_id, ctx->tags, etc.
 * }
 *
 * // At request exit
 * thread_context::clear();
 * @endcode
 *
 * @see thread_context_data For the stored context data structure
 * @see thread_context_manager For the legacy compatibility API
 */
class thread_context {
public:
    /**
     * @brief Create a new thread-local context, replacing any existing one.
     * @param request_id Optional request identifier. If empty, one can be
     *        assigned later or generated via generate_request_id().
     * @return Reference to the newly created thread-local context data
     */
    static thread_context_data& create(const std::string& request_id = "");

    /**
     * @brief Get the current thread-local context.
     * @return Pointer to the current context, or nullptr if no context exists
     */
    static thread_context_data* current();

    /**
     * @brief Check whether a context exists on the current thread.
     * @return true if a context has been created and not yet cleared
     */
    static bool has_context();

    /**
     * @brief Clear and destroy the current thread-local context.
     */
    static void clear();

    /**
     * @brief Generate a unique request ID.
     * @return A new UUID-style request identifier string
     */
    static std::string generate_request_id();

    /**
     * @brief Generate a unique correlation ID.
     * @return A new UUID-style correlation identifier string
     */
    static std::string generate_correlation_id();

    /**
     * @brief Copy context data from another source into the current thread.
     * @param source The context data to copy from
     * @return true if the copy succeeded, false on failure
     */
    static bool copy_from(const thread_context_data& source);

private:
    static thread_local std::unique_ptr<thread_context_data> current_context_;
};

/**
 * @brief Thread-local context storage (legacy compatibility).
 *
 * @details Provides an alternative API using std::optional-based storage.
 * Prefer thread_context for new code; this class exists for backward
 * compatibility with older consumers.
 *
 * @deprecated Prefer thread_context instead. This class may be removed in v1.0.0.
 * @see thread_context For the primary context management API
 */
class thread_context_manager {
public:
    /**
     * @brief Store a copy of the given context in thread-local storage.
     * @param context The context data to store
     */
    static void set_context(const thread_context_data& context);

    /**
     * @brief Retrieve the current thread-local context, if any.
     * @return The context data, or std::nullopt if no context is set
     */
    static std::optional<thread_context_data> get_context();

    /**
     * @brief Clear the current thread-local context.
     */
    static void clear_context();

    /**
     * @brief Generate a unique request ID.
     * @return A new UUID-style request identifier string
     */
    static std::string generate_request_id();

    /**
     * @brief Generate a unique correlation ID.
     * @return A new UUID-style correlation identifier string
     */
    static std::string generate_correlation_id();

private:
    static thread_local std::optional<thread_context_data> current_context_;
};

} } // namespace kcenon::monitoring