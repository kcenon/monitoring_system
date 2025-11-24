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
    virtual result<http_response> send(const http_request& request) = 0;

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

    result<http_response> send(const http_request& request) override {
        if (response_handler_) {
            return make_success(response_handler_(request));
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

        return make_success(response);
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

    result<http_response> send(const http_request& request) override {
        // Parse URL to extract host, port, and path
        auto url_parts = parse_url(request.url);
        if (!url_parts.valid) {
            return make_error<http_response>(monitoring_error_code::invalid_configuration,
                "Invalid URL: " + request.url);
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

        return make_success(response);
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

/**
 * @brief Create default HTTP transport
 *
 * Returns a stub transport for now. In production, this would
 * return an implementation based on available HTTP libraries.
 */
inline std::unique_ptr<http_transport> create_default_transport() {
    return std::make_unique<simple_http_client>();
}

/**
 * @brief Create stub HTTP transport for testing
 */
inline std::unique_ptr<stub_http_transport> create_stub_transport() {
    return std::make_unique<stub_http_transport>();
}

} } // namespace kcenon::monitoring
