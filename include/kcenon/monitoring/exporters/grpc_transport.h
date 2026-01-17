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
     * @return common::VoidResult indicating success or failure
     */
    virtual common::VoidResult connect(const std::string& host, uint16_t port) = 0;

    /**
     * @brief Send a gRPC request
     * @param request The gRPC request to send
     * @return Result containing response or error
     */
    virtual common::Result<grpc_response> send(const grpc_request& request) = 0;

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

    common::VoidResult connect(const std::string& host, uint16_t port) override {
        if (!simulate_success_) {
            return common::VoidResult::err(error_info(
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

    common::Result<grpc_response> send(const grpc_request& request) override {
        if (!connected_) {
            send_failures_.fetch_add(1, std::memory_order_relaxed);
            return common::Result<grpc_response>::err(error_info(monitoring_error_code::network_error, "Not connected").to_common_error());
        }

        if (response_handler_) {
            auto response = response_handler_(request);
            requests_sent_.fetch_add(1, std::memory_order_relaxed);
            bytes_sent_.fetch_add(request.body.size(), std::memory_order_relaxed);
            return common::ok(response);
        }

        if (!simulate_success_) {
            send_failures_.fetch_add(1, std::memory_order_relaxed);
            return common::Result<grpc_response>::err(error_info(monitoring_error_code::network_error, "Simulated send failure").to_common_error());
        }

        grpc_response response;
        response.status_code = 0; // OK in gRPC
        response.status_message = "OK";
        response.elapsed = std::chrono::milliseconds(10);

        requests_sent_.fetch_add(1, std::memory_order_relaxed);
        bytes_sent_.fetch_add(request.body.size(), std::memory_order_relaxed);

        return common::ok(response);
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

#include <grpcpp/grpcpp.h>
#include <mutex>
#include <unordered_map>

/**
 * @struct grpc_channel_config
 * @brief Configuration for gRPC channel
 */
struct grpc_channel_config {
    std::string target;                              ///< Target address (host:port)
    bool use_tls = false;                            ///< Whether to use TLS
    std::string root_certificates;                   ///< Root CA certificates (PEM)
    std::string private_key;                         ///< Client private key (PEM)
    std::string certificate_chain;                   ///< Client certificate chain (PEM)
    std::chrono::milliseconds connect_timeout{5000}; ///< Connection timeout
    std::chrono::milliseconds keepalive_time{10000}; ///< Keepalive ping interval
    bool enable_retry = true;                        ///< Enable automatic retry
};

/**
 * @class grpc_channel_manager
 * @brief Manages gRPC channel connections with pooling
 *
 * Provides connection pooling and reuse for gRPC channels,
 * supporting both secure (TLS) and insecure connections.
 */
class grpc_channel_manager {
private:
    std::unordered_map<std::string, std::shared_ptr<grpc::Channel>> channels_;
    std::mutex mutex_;
    grpc_channel_config default_config_;

public:
    explicit grpc_channel_manager(const grpc_channel_config& config = {})
        : default_config_(config) {}

    /**
     * @brief Get or create a channel for the given target
     * @param target Target address (host:port)
     * @return Shared pointer to gRPC channel
     */
    std::shared_ptr<grpc::Channel> get_channel(const std::string& target) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = channels_.find(target);
        if (it != channels_.end()) {
            // Check if channel is still usable
            auto state = it->second->GetState(false);
            if (state != GRPC_CHANNEL_SHUTDOWN) {
                return it->second;
            }
            // Remove dead channel
            channels_.erase(it);
        }

        // Create new channel
        auto channel = create_channel(target);
        channels_[target] = channel;
        return channel;
    }

    /**
     * @brief Get channel with custom configuration
     * @param target Target address
     * @param config Channel configuration
     * @return Shared pointer to gRPC channel
     */
    std::shared_ptr<grpc::Channel> get_channel(
        const std::string& target,
        const grpc_channel_config& config) {

        std::lock_guard<std::mutex> lock(mutex_);

        // For custom config, create a unique key
        std::string key = target + (config.use_tls ? "_tls" : "_insecure");

        auto it = channels_.find(key);
        if (it != channels_.end()) {
            auto state = it->second->GetState(false);
            if (state != GRPC_CHANNEL_SHUTDOWN) {
                return it->second;
            }
            channels_.erase(it);
        }

        auto channel = create_channel(target, config);
        channels_[key] = channel;
        return channel;
    }

    /**
     * @brief Shutdown all channels
     */
    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        channels_.clear();
    }

    /**
     * @brief Get number of active channels
     * @return Number of channels in pool
     */
    std::size_t channel_count() const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex_));
        return channels_.size();
    }

private:
    std::shared_ptr<grpc::Channel> create_channel(const std::string& target) {
        return create_channel(target, default_config_);
    }

    std::shared_ptr<grpc::Channel> create_channel(
        const std::string& target,
        const grpc_channel_config& config) {

        grpc::ChannelArguments args;

        // Set keepalive parameters
        args.SetInt(GRPC_ARG_KEEPALIVE_TIME_MS,
            static_cast<int>(config.keepalive_time.count()));
        args.SetInt(GRPC_ARG_KEEPALIVE_TIMEOUT_MS, 5000);
        args.SetInt(GRPC_ARG_KEEPALIVE_PERMIT_WITHOUT_CALLS, 1);

        // Set connection timeout
        args.SetInt(GRPC_ARG_INITIAL_RECONNECT_BACKOFF_MS,
            static_cast<int>(config.connect_timeout.count()));

        if (config.use_tls) {
            grpc::SslCredentialsOptions ssl_opts;
            if (!config.root_certificates.empty()) {
                ssl_opts.pem_root_certs = config.root_certificates;
            }
            if (!config.private_key.empty()) {
                ssl_opts.pem_private_key = config.private_key;
            }
            if (!config.certificate_chain.empty()) {
                ssl_opts.pem_cert_chain = config.certificate_chain;
            }
            auto creds = grpc::SslCredentials(ssl_opts);
            return grpc::CreateCustomChannel(target, creds, args);
        } else {
            return grpc::CreateCustomChannel(
                target, grpc::InsecureChannelCredentials(), args);
        }
    }
};

/**
 * @class network_grpc_transport
 * @brief gRPC transport implementation using actual gRPC library
 *
 * This implementation provides real gRPC functionality when the
 * gRPC library is available. Uses generic gRPC calls for flexibility.
 *
 * @note Requires linking against gRPC library.
 */
class network_grpc_transport : public grpc_transport {
private:
    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<grpc::GenericStub> stub_;
    std::string host_;
    uint16_t port_{0};
    grpc_channel_config config_;
    mutable std::atomic<std::size_t> requests_sent_{0};
    mutable std::atomic<std::size_t> bytes_sent_{0};
    mutable std::atomic<std::size_t> send_failures_{0};
    std::mutex connect_mutex_;

public:
    explicit network_grpc_transport(const grpc_channel_config& config = {})
        : config_(config) {}

    common::VoidResult connect(const std::string& host, uint16_t port) override {
        std::lock_guard<std::mutex> lock(connect_mutex_);

        host_ = host;
        port_ = port;
        std::string target = host + ":" + std::to_string(port);
        config_.target = target;

        try {
            grpc_channel_manager manager(config_);
            channel_ = manager.get_channel(target, config_);
            stub_ = std::make_unique<grpc::GenericStub>(channel_);

            // Wait for channel to be ready (with timeout)
            auto deadline = std::chrono::system_clock::now() + config_.connect_timeout;
            if (!channel_->WaitForConnected(deadline)) {
                return common::VoidResult::err(error_info(
                    monitoring_error_code::network_error,
                    "Connection timeout to " + target,
                    "network_grpc_transport"
                ).to_common_error());
            }

            return common::ok();
        } catch (const std::exception& e) {
            return common::VoidResult::err(error_info(
                monitoring_error_code::network_error,
                "Failed to connect: " + std::string(e.what()),
                "network_grpc_transport"
            ).to_common_error());
        }
    }

    common::Result<grpc_response> send(const grpc_request& request) override {
        if (!is_connected()) {
            send_failures_.fetch_add(1, std::memory_order_relaxed);
            return common::Result<grpc_response>::err(error_info(monitoring_error_code::network_error, "Not connected to server").to_common_error());
        }

        try {
            grpc::ClientContext context;

            // Set timeout
            auto deadline = std::chrono::system_clock::now() + request.timeout;
            context.set_deadline(deadline);

            // Set metadata
            for (const auto& [key, value] : request.metadata) {
                context.AddMetadata(key, value);
            }

            // Prepare the method path
            std::string method_path = "/" + request.service + "/" + request.method;

            // Create request slice
            grpc::Slice request_slice(request.body.data(), request.body.size());
            grpc::ByteBuffer request_buffer(&request_slice, 1);

            // Prepare response buffer
            grpc::ByteBuffer response_buffer;

            // Make the call
            auto start_time = std::chrono::steady_clock::now();
            grpc::Status status = stub_->UnaryCall(
                &context, method_path, request_buffer, &response_buffer);
            auto end_time = std::chrono::steady_clock::now();

            grpc_response response;
            response.status_code = static_cast<int>(status.error_code());
            response.status_message = status.error_message();
            response.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                end_time - start_time);

            // Extract response body
            if (response_buffer.Length() > 0) {
                std::vector<grpc::Slice> slices;
                response_buffer.Dump(&slices);
                for (const auto& slice : slices) {
                    const uint8_t* data = slice.begin();
                    response.body.insert(response.body.end(), data, data + slice.size());
                }
            }

            if (status.ok()) {
                requests_sent_.fetch_add(1, std::memory_order_relaxed);
                bytes_sent_.fetch_add(request.body.size(), std::memory_order_relaxed);
                return common::ok(response);
            } else {
                send_failures_.fetch_add(1, std::memory_order_relaxed);
                return make_error<grpc_response>(
                    monitoring_error_code::network_error,
                    "gRPC call failed: " + status.error_message());
            }
        } catch (const std::exception& e) {
            send_failures_.fetch_add(1, std::memory_order_relaxed);
            return make_error<grpc_response>(
                monitoring_error_code::network_error,
                "gRPC exception: " + std::string(e.what()));
        }
    }

    bool is_connected() const override {
        if (!channel_) return false;
        auto state = channel_->GetState(false);
        return state == GRPC_CHANNEL_READY ||
               state == GRPC_CHANNEL_IDLE ||
               state == GRPC_CHANNEL_CONNECTING;
    }

    void disconnect() override {
        std::lock_guard<std::mutex> lock(connect_mutex_);
        stub_.reset();
        channel_.reset();
        host_.clear();
        port_ = 0;
    }

    bool is_available() const override {
        return true;  // gRPC library is available when this class is compiled
    }

    std::string name() const override {
        return "grpc";
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

    // Accessors for testing
    std::string get_host() const { return host_; }
    uint16_t get_port() const { return port_; }
    grpc_connectivity_state get_channel_state() const {
        return channel_ ? channel_->GetState(false) : GRPC_CHANNEL_SHUTDOWN;
    }
};

/**
 * @brief Check gRPC channel health
 * @param target Target address (host:port)
 * @param config Channel configuration
 * @param timeout Health check timeout
 * @return true if channel is healthy
 */
inline bool grpc_health_check(
    const std::string& target,
    const grpc_channel_config& config = {},
    std::chrono::milliseconds timeout = std::chrono::seconds(5)) {

    grpc_channel_manager manager(config);
    auto channel = manager.get_channel(target, config);

    auto deadline = std::chrono::system_clock::now() + timeout;
    return channel->WaitForConnected(deadline);
}

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
