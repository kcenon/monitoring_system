#pragma once

/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, monitoring_system contributors
All rights reserved.
*****************************************************************************/

/**
 * @file grpc_transport.h
 * @brief gRPC transport layer for OTLP exporters
 *
 * This file provides gRPC client abstraction for sending telemetry data
 * to OpenTelemetry Protocol (OTLP) backends via gRPC.
 *
 * @note gRPC implementation requires external gRPC library.
 * When gRPC is not available, HTTP-based OTLP should be used instead.
 */

#include "../core/result_types.h"
#include "../core/error_codes.h"
#include <string>
#include <vector>
#include <cstdint>
#include <chrono>
#include <memory>
#include <atomic>
#include <span>
#include <functional>

namespace kcenon { namespace monitoring {

/**
 * @struct grpc_request
 * @brief gRPC request configuration
 */
struct grpc_request {
    std::string service;
    std::string method;
    std::vector<uint8_t> body;
    std::chrono::milliseconds timeout{30000};
    std::unordered_map<std::string, std::string> metadata;
};

/**
 * @struct grpc_response
 * @brief gRPC response data
 */
struct grpc_response {
    int status_code{0};
    std::string status_message;
    std::vector<uint8_t> body;
    std::chrono::milliseconds elapsed{0};
};

/**
 * @struct grpc_statistics
 * @brief Statistics for gRPC transport operations
 */
struct grpc_statistics {
    std::size_t requests_sent{0};
    std::size_t bytes_sent{0};
    std::size_t send_failures{0};
};

/**
 * @class grpc_transport
 * @brief Abstract gRPC transport interface
 *
 * Provides a common interface for gRPC-based communication,
 * with implementations for stub (testing) and real gRPC backends.
 */
class grpc_transport {
public:
    virtual ~grpc_transport() = default;

    /**
     * @brief Connect to a gRPC server
     * @param host Server hostname or IP address
     * @param port Server port number
     * @return result_void indicating success or failure
     */
    virtual result_void connect(const std::string& host, uint16_t port) = 0;

    /**
     * @brief Send a gRPC request
     * @param request The gRPC request to send
     * @return Result containing response or error
     */
    virtual result<grpc_response> send(const grpc_request& request) = 0;

    /**
     * @brief Check if connected to the server
     * @return true if connected
     */
    virtual bool is_connected() const = 0;

    /**
     * @brief Disconnect from the server
     */
    virtual void disconnect() = 0;

    /**
     * @brief Check if transport is available
     * @return true if transport can send data
     */
    virtual bool is_available() const = 0;

    /**
     * @brief Get transport name
     * @return Transport implementation identifier
     */
    virtual std::string name() const = 0;

    /**
     * @brief Get transport statistics
     * @return Current statistics
     */
    virtual grpc_statistics get_statistics() const = 0;

    /**
     * @brief Reset statistics
     */
    virtual void reset_statistics() = 0;
};

/**
 * @class stub_grpc_transport
 * @brief Stub gRPC transport for testing
 *
 * This implementation simulates gRPC calls without actual network operations.
 * Useful for unit testing and when gRPC library is unavailable.
 */
class stub_grpc_transport : public grpc_transport {
private:
    std::string host_;
    uint16_t port_{0};
    bool connected_{false};
    bool simulate_success_{true};
    std::function<grpc_response(const grpc_request&)> response_handler_;
    mutable std::atomic<std::size_t> requests_sent_{0};
    mutable std::atomic<std::size_t> bytes_sent_{0};
    mutable std::atomic<std::size_t> send_failures_{0};

public:
    stub_grpc_transport() = default;

    /**
     * @brief Set whether to simulate success or failure
     */
    void set_simulate_success(bool success) {
        simulate_success_ = success;
    }

    /**
     * @brief Set custom response handler for testing
     */
    void set_response_handler(std::function<grpc_response(const grpc_request&)> handler) {
        response_handler_ = std::move(handler);
    }

    result_void connect(const std::string& host, uint16_t port) override {
        if (!simulate_success_) {
            return result_void::err(error_info(
                monitoring_error_code::network_error,
                "Simulated connection failure",
                "stub_grpc_transport"
            ).to_common_error());
        }
        host_ = host;
        port_ = port;
        connected_ = true;
        return common::ok();
    }

    result<grpc_response> send(const grpc_request& request) override {
        if (!connected_) {
            send_failures_.fetch_add(1, std::memory_order_relaxed);
            return make_error<grpc_response>(
                monitoring_error_code::network_error,
                "Not connected");
        }

        if (response_handler_) {
            auto response = response_handler_(request);
            requests_sent_.fetch_add(1, std::memory_order_relaxed);
            bytes_sent_.fetch_add(request.body.size(), std::memory_order_relaxed);
            return make_success(response);
        }

        if (!simulate_success_) {
            send_failures_.fetch_add(1, std::memory_order_relaxed);
            return make_error<grpc_response>(
                monitoring_error_code::network_error,
                "Simulated send failure");
        }

        grpc_response response;
        response.status_code = 0; // OK in gRPC
        response.status_message = "OK";
        response.elapsed = std::chrono::milliseconds(10);

        requests_sent_.fetch_add(1, std::memory_order_relaxed);
        bytes_sent_.fetch_add(request.body.size(), std::memory_order_relaxed);

        return make_success(response);
    }

    bool is_connected() const override {
        return connected_;
    }

    void disconnect() override {
        connected_ = false;
        host_.clear();
        port_ = 0;
    }

    bool is_available() const override {
        return true;
    }

    std::string name() const override {
        return "stub";
    }

    grpc_statistics get_statistics() const override {
        return {
            requests_sent_.load(std::memory_order_relaxed),
            bytes_sent_.load(std::memory_order_relaxed),
            send_failures_.load(std::memory_order_relaxed)
        };
    }

    void reset_statistics() override {
        requests_sent_.store(0, std::memory_order_relaxed);
        bytes_sent_.store(0, std::memory_order_relaxed);
        send_failures_.store(0, std::memory_order_relaxed);
    }

    // Test helpers
    std::string get_host() const { return host_; }
    uint16_t get_port() const { return port_; }
};

#ifdef MONITORING_HAS_GRPC

/**
 * @class network_grpc_transport
 * @brief gRPC transport implementation using actual gRPC library
 *
 * This implementation provides real gRPC functionality when the
 * gRPC library is available.
 *
 * @note Requires linking against gRPC library.
 */
class network_grpc_transport : public grpc_transport {
    // Implementation would require gRPC library
    // This is a placeholder for future implementation
};

#endif // MONITORING_HAS_GRPC

/**
 * @brief Create default gRPC transport
 *
 * Returns a real gRPC transport if available,
 * otherwise falls back to a stub implementation.
 */
inline std::unique_ptr<grpc_transport> create_default_grpc_transport() {
#ifdef MONITORING_HAS_GRPC
    return std::make_unique<network_grpc_transport>();
#else
    return std::make_unique<stub_grpc_transport>();
#endif
}

/**
 * @brief Create stub gRPC transport for testing
 */
inline std::unique_ptr<stub_grpc_transport> create_stub_grpc_transport() {
    return std::make_unique<stub_grpc_transport>();
}

} } // namespace kcenon::monitoring
