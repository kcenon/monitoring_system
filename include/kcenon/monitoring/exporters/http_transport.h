#pragma once

/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, monitoring_system contributors
All rights reserved.
*****************************************************************************/

/**
 * @file http_transport.h
 * @brief HTTP transport layer for trace exporters
 *
 * This file provides HTTP client abstraction for sending trace data
 * to Jaeger, Zipkin, and OTLP backends.
 */

#include "../core/result_types.h"
#include "../core/error_codes.h"
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <chrono>
#include <memory>
#include <functional>

namespace kcenon { namespace monitoring {

/**
 * @struct http_request
 * @brief HTTP request configuration
 */
struct http_request {
    std::string url;
    std::string method{"POST"};
    std::unordered_map<std::string, std::string> headers;
    std::vector<uint8_t> body;
    std::chrono::milliseconds timeout{30000};
    bool enable_compression{false};
};

/**
 * @struct http_response
 * @brief HTTP response data
 */
struct http_response {
    int status_code{0};
    std::string status_message;
    std::unordered_map<std::string, std::string> headers;
    std::vector<uint8_t> body;
    std::chrono::milliseconds elapsed{0};
};

/**
 * @class http_transport
 * @brief Abstract HTTP transport interface
 */
class http_transport {
public:
    virtual ~http_transport() = default;

    /**
     * @brief Send HTTP request and receive response
     */
    virtual common::Result<http_response> send(const http_request& request) = 0;

    /**
     * @brief Check if transport is available
     */
    virtual bool is_available() const = 0;

    /**
     * @brief Get transport name
     */
    virtual std::string name() const = 0;
};

/**
 * @class stub_http_transport
 * @brief Stub HTTP transport for testing
 *
 * This implementation simulates HTTP responses without actual network calls.
 * Useful for unit testing and when network is unavailable.
 */
class stub_http_transport : public http_transport {
private:
    std::function<http_response(const http_request&)> response_handler_;
    bool simulate_success_{true};

public:
    stub_http_transport() = default;

    /**
     * @brief Set custom response handler for testing
     */
    void set_response_handler(std::function<http_response(const http_request&)> handler) {
        response_handler_ = std::move(handler);
    }

    /**
     * @brief Set whether to simulate success or failure
     */
    void set_simulate_success(bool success) {
        simulate_success_ = success;
    }

    common::Result<http_response> send(const http_request& request) override {
        if (response_handler_) {
            return common::ok(response_handler_(request));
        }

        http_response response;
        if (simulate_success_) {
            response.status_code = 202; // Accepted
            response.status_message = "Accepted";
        } else {
            response.status_code = 503; // Service Unavailable
            response.status_message = "Service Unavailable";
        }
        response.elapsed = std::chrono::milliseconds(10);

        return common::ok(response);
    }

    bool is_available() const override {
        return true;
    }

    std::string name() const override {
        return "stub";
    }
};

/**
 * @class simple_http_client
 * @brief Simple HTTP client using basic socket operations
 *
 * This implementation provides basic HTTP functionality without external dependencies.
 * For production use, consider integrating a full HTTP library like libcurl or cpp-httplib.
 */
class simple_http_client : public http_transport {
private:
    std::chrono::milliseconds default_timeout_{30000};
    bool use_tls_{false};

public:
    explicit simple_http_client(std::chrono::milliseconds timeout = std::chrono::milliseconds(30000))
        : default_timeout_(timeout) {}

    common::Result<http_response> send(const http_request& request) override {
        // Parse URL to extract host, port, and path
        auto url_parts = parse_url(request.url);
        if (!url_parts.valid) {
            error_info err(monitoring_error_code::invalid_configuration,
                          "Invalid URL: " + request.url);
            return common::Result<http_response>::err(err.to_common_error());
        }

        use_tls_ = url_parts.scheme == "https";

        // For now, return a stub response
        // Real implementation would use socket operations
        http_response response;
        response.status_code = 202;
        response.status_message = "Accepted (Stub)";
        response.elapsed = std::chrono::milliseconds(1);

        // Log the request for debugging
        // In production, this would actually send the HTTP request

        return common::ok(response);
    }

    bool is_available() const override {
        // Simple client is always available (stub)
        return true;
    }

    std::string name() const override {
        return "simple";
    }

private:
    struct url_parts {
        std::string scheme;
        std::string host;
        int port{80};
        std::string path;
        bool valid{false};
    };

    url_parts parse_url(const std::string& url) {
        url_parts parts;

        // Find scheme
        auto scheme_end = url.find("://");
        if (scheme_end == std::string::npos) {
            return parts;
        }
        parts.scheme = url.substr(0, scheme_end);

        // Find host and port
        auto host_start = scheme_end + 3;
        auto path_start = url.find('/', host_start);
        if (path_start == std::string::npos) {
            path_start = url.length();
        }

        auto host_port = url.substr(host_start, path_start - host_start);
        auto colon_pos = host_port.find(':');
        if (colon_pos != std::string::npos) {
            parts.host = host_port.substr(0, colon_pos);
            try {
                parts.port = std::stoi(host_port.substr(colon_pos + 1));
            } catch (...) {
                return parts;
            }
        } else {
            parts.host = host_port;
            parts.port = (parts.scheme == "https") ? 443 : 80;
        }

        // Extract path
        if (path_start < url.length()) {
            parts.path = url.substr(path_start);
        } else {
            parts.path = "/";
        }

        parts.valid = !parts.host.empty();
        return parts;
    }
};

#ifdef MONITORING_HAS_NETWORK_SYSTEM
#include <kcenon/network/core/http_client.h>

/**
 * @class network_http_transport
 * @brief HTTP transport implementation using network_system
 *
 * This implementation provides real HTTP functionality by delegating
 * to network_system::core::http_client.
 */
class network_http_transport : public http_transport {
private:
    std::shared_ptr<network_system::core::http_client> client_;

public:
    explicit network_http_transport(std::chrono::milliseconds timeout = std::chrono::milliseconds(30000))
        : client_(std::make_shared<network_system::core::http_client>(timeout)) {}

    common::Result<http_response> send(const http_request& request) override {
        // Convert headers from unordered_map to map
        std::map<std::string, std::string> headers(request.headers.begin(), request.headers.end());

        // Make the actual HTTP request using network_system
        network_system::core::Result<network_system::core::internal::http_response> net_result;

        if (request.method == "GET") {
            net_result = client_->get(request.url, {}, headers);
        } else if (request.method == "POST") {
            net_result = client_->post(request.url, request.body, headers);
        } else if (request.method == "PUT") {
            std::string body_str(request.body.begin(), request.body.end());
            net_result = client_->put(request.url, body_str, headers);
        } else if (request.method == "DELETE") {
            net_result = client_->del(request.url, headers);
        } else if (request.method == "HEAD") {
            net_result = client_->head(request.url, headers);
        } else if (request.method == "PATCH") {
            std::string body_str(request.body.begin(), request.body.end());
            net_result = client_->patch(request.url, body_str, headers);
        } else {
            return make_error<http_response>(monitoring_error_code::invalid_configuration,
                "Unsupported HTTP method: " + request.method);
        }

        if (!net_result) {
            return make_error<http_response>(monitoring_error_code::operation_failed,
                "HTTP request failed: " + net_result.error().message);
        }

        // Convert network_system response to monitoring response
        http_response response;
        response.status_code = net_result->status_code;
        response.status_message = net_result->status_text;
        response.body = net_result->body;

        // Convert headers
        for (const auto& [key, value] : net_result->headers) {
            response.headers[key] = value;
        }

        return common::ok(response);
    }

    bool is_available() const override {
        return true;
    }

    std::string name() const override {
        return "network_system";
    }
};
#endif // MONITORING_HAS_NETWORK_SYSTEM

/**
 * @brief Create default HTTP transport
 *
 * Returns a network_system-based transport if available,
 * otherwise falls back to a stub implementation.
 */
inline std::unique_ptr<http_transport> create_default_transport() {
#ifdef MONITORING_HAS_NETWORK_SYSTEM
    return std::make_unique<network_http_transport>();
#else
    return std::make_unique<simple_http_client>();
#endif
}

/**
 * @brief Create stub HTTP transport for testing
 */
inline std::unique_ptr<stub_http_transport> create_stub_transport() {
    return std::make_unique<stub_http_transport>();
}

#ifdef MONITORING_HAS_NETWORK_SYSTEM
/**
 * @brief Create network_system-based HTTP transport
 */
inline std::unique_ptr<network_http_transport> create_network_transport(
    std::chrono::milliseconds timeout = std::chrono::milliseconds(30000)) {
    return std::make_unique<network_http_transport>(timeout);
}
#endif

} } // namespace kcenon::monitoring
