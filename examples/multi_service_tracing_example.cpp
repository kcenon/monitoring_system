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

/**
 * @file multi_service_tracing_example.cpp
 * @brief Demonstrates distributed tracing across multiple services
 *
 * This example shows how to:
 * - Simulate multi-service architecture
 * - Propagate trace context between services
 * - Correlate traces across service boundaries
 * - Create parent-child span relationships
 * - Propagate baggage for cross-cutting data
 * - Visualize trace export patterns
 */

#include <iostream>
#include <memory>
#include <string>
#include <chrono>
#include <thread>
#include <iomanip>

#include "kcenon/monitoring/tracing/distributed_tracer.h"

using namespace kcenon::monitoring;
using namespace std::chrono_literals;

// Forward declarations
void display_span(const trace_span& span, int depth);
void display_children(const std::vector<trace_span>& spans,
                     const std::string& parent_id,
                     int depth);

/**
 * @brief Simulated API Gateway service
 */
class ApiGatewayService {
private:
    std::string service_name_ = "api_gateway";

public:
    /**
     * @brief Handle incoming HTTP request
     */
    trace_span handle_request(const std::string& endpoint, const std::string& method) {
        std::cout << "\n[" << service_name_ << "] Processing " << method << " " << endpoint << std::endl;

        // Create root span for this request
        trace_span span;
        span.trace_id = generate_trace_id();
        span.span_id = generate_span_id();
        span.operation_name = "http_request";
        span.service_name = service_name_;
        span.start_time = std::chrono::system_clock::now();

        // Add HTTP-specific tags
        span.tags["http.method"] = method;
        span.tags["http.url"] = endpoint;
        span.tags["http.target"] = endpoint;
        span.tags["component"] = "http_server";

        // Add baggage for cross-cutting concerns
        span.baggage["user.id"] = "user-12345";
        span.baggage["session.id"] = "sess-67890";
        span.baggage["request.priority"] = "high";

        std::cout << "   → Created root span: " << span.span_id << std::endl;
        std::cout << "     Trace ID: " << span.trace_id << std::endl;
        std::cout << "     Baggage: user.id=" << span.baggage["user.id"] << std::endl;

        return span;
    }

    /**
     * @brief Create trace context for propagation
     */
    trace_context create_context(const trace_span& span) {
        trace_context ctx;
        ctx.trace_id = span.trace_id;
        ctx.span_id = span.span_id;
        ctx.trace_flags = "01";  // Sampled
        ctx.baggage = span.baggage;

        std::cout << "   → Context for propagation: "
                  << ctx.to_w3c_traceparent() << std::endl;

        return ctx;
    }

private:
    static std::string generate_trace_id() {
        // Generate 32-character hex string (16 bytes)
        std::string id;
        for (int i = 0; i < 32; ++i) {
            id += "0123456789abcdef"[rand() % 16];
        }
        return id;
    }

    static std::string generate_span_id() {
        // Generate 16-character hex string (8 bytes)
        std::string id;
        for (int i = 0; i < 16; ++i) {
            id += "0123456789abcdef"[rand() % 16];
        }
        return id;
    }
};

/**
 * @brief Simulated Authentication service
 */
class AuthService {
private:
    std::string service_name_ = "auth_service";

public:
    /**
     * @brief Verify user authentication
     */
    trace_span verify_token(const trace_context& parent_ctx, const std::string& /* token */) {
        std::cout << "\n[" << service_name_ << "] Verifying authentication token" << std::endl;
        std::cout << "   Received context: " << parent_ctx.to_w3c_traceparent() << std::endl;

        // Create child span
        trace_span span;
        span.trace_id = parent_ctx.trace_id;         // Same trace ID
        span.span_id = generate_span_id();           // New span ID
        span.parent_span_id = parent_ctx.span_id;    // Link to parent
        span.operation_name = "verify_token";
        span.service_name = service_name_;
        span.start_time = std::chrono::system_clock::now();

        // Inherit baggage from parent
        span.baggage = parent_ctx.baggage;

        // Add service-specific tags
        span.tags["auth.token_type"] = "bearer";
        span.tags["auth.method"] = "jwt";

        std::cout << "   → Created child span: " << span.span_id << std::endl;
        std::cout << "     Parent span: " << span.parent_span_id << std::endl;
        std::cout << "     Inherited baggage: user.id=" << span.baggage["user.id"] << std::endl;

        // Simulate authentication work
        std::this_thread::sleep_for(5ms);

        span.end_time = std::chrono::system_clock::now();
        span.calculate_duration();
        span.status = trace_span::status_code::ok;
        span.tags["auth.result"] = "success";

        std::cout << "   ✓ Authentication successful (duration: "
                  << span.duration.count() << "µs)" << std::endl;

        return span;
    }

private:
    static std::string generate_span_id() {
        std::string id;
        for (int i = 0; i < 16; ++i) {
            id += "0123456789abcdef"[rand() % 16];
        }
        return id;
    }
};

/**
 * @brief Simulated User service
 */
class UserService {
private:
    std::string service_name_ = "user_service";

public:
    /**
     * @brief Fetch user profile
     */
    trace_span get_user_profile(const trace_context& parent_ctx, const std::string& user_id) {
        std::cout << "\n[" << service_name_ << "] Fetching user profile" << std::endl;
        std::cout << "   User ID from baggage: " << parent_ctx.baggage.at("user.id") << std::endl;

        // Create child span
        trace_span span;
        span.trace_id = parent_ctx.trace_id;
        span.span_id = generate_span_id();
        span.parent_span_id = parent_ctx.span_id;
        span.operation_name = "get_user_profile";
        span.service_name = service_name_;
        span.start_time = std::chrono::system_clock::now();

        // Inherit baggage
        span.baggage = parent_ctx.baggage;

        // Add database query tags
        span.tags["db.system"] = "postgresql";
        span.tags["db.name"] = "users_db";
        span.tags["db.statement"] = "SELECT * FROM users WHERE id = ?";
        span.tags["db.user_id"] = user_id;

        std::cout << "   → Created child span: " << span.span_id << std::endl;
        std::cout << "     Parent span: " << span.parent_span_id << std::endl;

        // Simulate database query
        std::this_thread::sleep_for(15ms);

        span.end_time = std::chrono::system_clock::now();
        span.calculate_duration();
        span.status = trace_span::status_code::ok;
        span.tags["db.rows_returned"] = "1";

        std::cout << "   ✓ User profile fetched (duration: "
                  << span.duration.count() << "µs)" << std::endl;

        return span;
    }

private:
    static std::string generate_span_id() {
        std::string id;
        for (int i = 0; i < 16; ++i) {
            id += "0123456789abcdef"[rand() % 16];
        }
        return id;
    }
};

/**
 * @brief Simulated Cache service
 */
class CacheService {
private:
    std::string service_name_ = "cache_service";

public:
    /**
     * @brief Check cache for user data
     */
    trace_span cache_lookup(const trace_context& parent_ctx, const std::string& key) {
        std::cout << "\n[" << service_name_ << "] Cache lookup" << std::endl;

        // Create child span
        trace_span span;
        span.trace_id = parent_ctx.trace_id;
        span.span_id = generate_span_id();
        span.parent_span_id = parent_ctx.span_id;
        span.operation_name = "cache_get";
        span.service_name = service_name_;
        span.start_time = std::chrono::system_clock::now();

        // Inherit baggage
        span.baggage = parent_ctx.baggage;

        // Add cache-specific tags
        span.tags["cache.type"] = "redis";
        span.tags["cache.key"] = key;

        std::cout << "   → Created child span: " << span.span_id << std::endl;

        // Simulate cache miss
        std::this_thread::sleep_for(2ms);

        span.end_time = std::chrono::system_clock::now();
        span.calculate_duration();
        span.status = trace_span::status_code::ok;
        span.tags["cache.hit"] = "false";

        std::cout << "   ○ Cache miss (duration: "
                  << span.duration.count() << "µs)" << std::endl;

        return span;
    }

private:
    static std::string generate_span_id() {
        std::string id;
        for (int i = 0; i < 16; ++i) {
            id += "0123456789abcdef"[rand() % 16];
        }
        return id;
    }
};

/**
 * @brief Display trace tree structure
 */
void display_trace_tree(const std::vector<trace_span>& spans) {
    std::cout << "\n=== Trace Tree Structure ===" << std::endl;

    // Find root span
    const trace_span* root = nullptr;
    for (const auto& span : spans) {
        if (span.parent_span_id.empty()) {
            root = &span;
            break;
        }
    }

    if (!root) {
        std::cout << "No root span found" << std::endl;
        return;
    }

    std::cout << "\nTrace ID: " << root->trace_id << "\n" << std::endl;

    // Display root
    display_span(*root, 0);

    // Display children
    display_children(spans, root->span_id, 1);

    std::cout << "\n=== Total Spans: " << spans.size() << " ===" << std::endl;
    std::cout << "Total trace duration: " << root->duration.count() << "µs" << std::endl;
}

/**
 * @brief Display single span
 */
void display_span(const trace_span& span, int depth) {
    std::string indent(depth * 2, ' ');
    std::string prefix = depth == 0 ? "┌─" : "├─";

    std::cout << indent << prefix << " [" << span.service_name << "] "
              << span.operation_name << std::endl;
    std::cout << indent << "   Span ID: " << span.span_id << std::endl;
    std::cout << indent << "   Duration: " << span.duration.count() << "µs" << std::endl;

    if (!span.tags.empty()) {
        std::cout << indent << "   Tags:";
        int count = 0;
        for (const auto& [key, value] : span.tags) {
            if (count++ < 3) {  // Show first 3 tags
                std::cout << " " << key << "=" << value;
            }
        }
        if (span.tags.size() > 3) {
            std::cout << " +" << (span.tags.size() - 3) << " more";
        }
        std::cout << std::endl;
    }
}

/**
 * @brief Display children recursively
 */
void display_children(const std::vector<trace_span>& spans,
                     const std::string& parent_id,
                     int depth) {
    for (const auto& span : spans) {
        if (span.parent_span_id == parent_id) {
            display_span(span, depth);
            display_children(spans, span.span_id, depth + 1);
        }
    }
}

/**
 * @brief Simulate complete multi-service request flow
 */
void simulate_multi_service_request() {
    std::cout << "=== Multi-Service Distributed Tracing Example ===" << std::endl;

    std::vector<trace_span> collected_spans;

    // Step 1: API Gateway receives request
    std::cout << "\n--- Step 1: API Gateway ---" << std::endl;
    ApiGatewayService gateway;
    auto gateway_span = gateway.handle_request("/api/user/profile", "GET");
    auto gateway_ctx = gateway.create_context(gateway_span);

    // Step 2: Check cache (parallel to auth)
    std::cout << "\n--- Step 2: Cache Lookup ---" << std::endl;
    CacheService cache;
    auto cache_span = cache.cache_lookup(gateway_ctx, "user:user-12345");
    collected_spans.push_back(cache_span);

    // Step 3: Authenticate request
    std::cout << "\n--- Step 3: Authentication ---" << std::endl;
    AuthService auth;
    auto auth_span = auth.verify_token(gateway_ctx, "eyJhbGc...");
    collected_spans.push_back(auth_span);

    // Create context for user service call
    trace_context user_ctx;
    user_ctx.trace_id = gateway_ctx.trace_id;
    user_ctx.span_id = gateway_span.span_id;
    user_ctx.baggage = gateway_ctx.baggage;

    // Step 4: Fetch user profile (after cache miss)
    std::cout << "\n--- Step 4: User Profile Service ---" << std::endl;
    UserService user_service;
    auto user_span = user_service.get_user_profile(user_ctx, "user-12345");
    collected_spans.push_back(user_span);

    // Complete gateway span
    std::this_thread::sleep_for(5ms);
    gateway_span.end_time = std::chrono::system_clock::now();
    gateway_span.calculate_duration();
    gateway_span.status = trace_span::status_code::ok;
    gateway_span.tags["http.status_code"] = "200";
    collected_spans.insert(collected_spans.begin(), gateway_span);

    // Display complete trace
    display_trace_tree(collected_spans);

    // Show baggage propagation
    std::cout << "\n=== Baggage Propagation ===" << std::endl;
    std::cout << "Baggage items propagated across all services:" << std::endl;
    for (const auto& [key, value] : gateway_span.baggage) {
        std::cout << "   " << key << " = " << value << std::endl;
    }

    std::cout << "\n=== Example completed successfully ===" << std::endl;
}

int main() {
    std::cout << "Multi-Service Distributed Tracing Example\n" << std::endl;

    simulate_multi_service_request();

    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "\nKey Concepts Demonstrated:" << std::endl;
    std::cout << "1. Trace context propagation across services" << std::endl;
    std::cout << "2. Parent-child span relationships" << std::endl;
    std::cout << "3. Baggage propagation for cross-cutting data" << std::endl;
    std::cout << "4. W3C Trace Context standard" << std::endl;
    std::cout << "5. Service-specific span tags and metadata" << std::endl;

    return 0;
}
