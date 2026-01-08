#pragma once

/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, monitoring_system contributors
All rights reserved.
*****************************************************************************/

/**
 * @file udp_transport.h
 * @brief UDP transport layer for metric exporters
 *
 * This file provides UDP client abstraction for sending metric data
 * to StatsD and other UDP-based metric backends.
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

namespace kcenon { namespace monitoring {

/**
 * @struct udp_send_result
 * @brief Result of a UDP send operation
 */
struct udp_send_result {
    std::size_t bytes_sent{0};
    std::chrono::milliseconds elapsed{0};
};

/**
 * @struct udp_statistics
 * @brief Statistics for UDP transport operations
 */
struct udp_statistics {
    std::size_t packets_sent{0};
    std::size_t bytes_sent{0};
    std::size_t send_failures{0};
};

/**
 * @class udp_transport
 * @brief Abstract UDP transport interface
 *
 * Provides a common interface for UDP-based metric delivery,
 * with implementations for stub (testing), simple (basic), and
 * network_system-backed transports.
 */
class udp_transport {
public:
    virtual ~udp_transport() = default;

    /**
     * @brief Connect to a remote UDP endpoint
     * @param host Remote hostname or IP address
     * @param port Remote port number
     * @return result_void indicating success or failure
     */
    virtual result_void connect(const std::string& host, uint16_t port) = 0;

    /**
     * @brief Send data to the connected endpoint
     * @param data Data to send
     * @return result_void indicating success or failure
     */
    virtual result_void send(std::span<const uint8_t> data) = 0;

    /**
     * @brief Send string data to the connected endpoint
     * @param data String data to send
     * @return result_void indicating success or failure
     */
    result_void send(const std::string& data) {
        return send(std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(data.data()),
            data.size()));
    }

    /**
     * @brief Check if connected to an endpoint
     * @return true if connected
     */
    virtual bool is_connected() const = 0;

    /**
     * @brief Disconnect from the current endpoint
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
    virtual udp_statistics get_statistics() const = 0;

    /**
     * @brief Reset statistics
     */
    virtual void reset_statistics() = 0;
};

/**
 * @class stub_udp_transport
 * @brief Stub UDP transport for testing
 *
 * This implementation simulates UDP sends without actual network calls.
 * Useful for unit testing and when network is unavailable.
 */
class stub_udp_transport : public udp_transport {
private:
    std::string host_;
    uint16_t port_{0};
    bool connected_{false};
    bool simulate_success_{true};
    mutable std::atomic<std::size_t> packets_sent_{0};
    mutable std::atomic<std::size_t> bytes_sent_{0};
    mutable std::atomic<std::size_t> send_failures_{0};

public:
    stub_udp_transport() = default;

    // Bring base class send method into scope
    using udp_transport::send;

    /**
     * @brief Set whether to simulate success or failure
     */
    void set_simulate_success(bool success) {
        simulate_success_ = success;
    }

    result_void connect(const std::string& host, uint16_t port) override {
        if (!simulate_success_) {
            return result_void::err(error_info(
                monitoring_error_code::network_error,
                "Simulated connection failure",
                "stub_udp_transport"
            ).to_common_error());
        }
        host_ = host;
        port_ = port;
        connected_ = true;
        return common::ok();
    }

    result_void send(std::span<const uint8_t> data) override {
        if (!connected_) {
            send_failures_.fetch_add(1, std::memory_order_relaxed);
            return result_void::err(error_info(
                monitoring_error_code::network_error,
                "Not connected",
                "stub_udp_transport"
            ).to_common_error());
        }

        if (!simulate_success_) {
            send_failures_.fetch_add(1, std::memory_order_relaxed);
            return result_void::err(error_info(
                monitoring_error_code::network_error,
                "Simulated send failure",
                "stub_udp_transport"
            ).to_common_error());
        }

        packets_sent_.fetch_add(1, std::memory_order_relaxed);
        bytes_sent_.fetch_add(data.size(), std::memory_order_relaxed);
        return common::ok();
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

    udp_statistics get_statistics() const override {
        return {
            packets_sent_.load(std::memory_order_relaxed),
            bytes_sent_.load(std::memory_order_relaxed),
            send_failures_.load(std::memory_order_relaxed)
        };
    }

    void reset_statistics() override {
        packets_sent_.store(0, std::memory_order_relaxed);
        bytes_sent_.store(0, std::memory_order_relaxed);
        send_failures_.store(0, std::memory_order_relaxed);
    }

    // Test helpers
    std::string get_host() const { return host_; }
    uint16_t get_port() const { return port_; }
};

} } // namespace kcenon::monitoring

#ifdef MONITORING_HAS_COMMON_TRANSPORT_INTERFACES
#include <kcenon/common/interfaces/transport.h>

namespace kcenon { namespace monitoring {

/**
 * @class common_udp_transport
 * @brief UDP transport implementation using common_system interfaces
 *
 * This implementation provides real UDP functionality by delegating
 * to ::kcenon::common::interfaces::IUdpClient implementations.
 */
class common_udp_transport : public udp_transport {
private:
    std::shared_ptr<::kcenon::common::interfaces::IUdpClient> client_;
    mutable std::atomic<std::size_t> packets_sent_{0};
    mutable std::atomic<std::size_t> bytes_sent_{0};
    mutable std::atomic<std::size_t> send_failures_{0};

public:
    explicit common_udp_transport(std::shared_ptr<::kcenon::common::interfaces::IUdpClient> client)
        : client_(std::move(client)) {}

    result_void connect(const std::string& host, uint16_t port) override {
        if (!client_) {
            return result_void::err(error_info(
                monitoring_error_code::dependency_missing,
                "UDP client not available",
                "common_udp_transport"
            ).to_common_error());
        }

        auto result = client_->connect(host, port);
        if (result.is_err()) {
            return result_void::err(error_info(
                monitoring_error_code::network_error,
                "Connection failed: " + result.error().message,
                "common_udp_transport"
            ).to_common_error());
        }
        return common::ok();
    }

    result_void send(std::span<const uint8_t> data) override {
        if (!client_) {
            send_failures_.fetch_add(1, std::memory_order_relaxed);
            return result_void::err(error_info(
                monitoring_error_code::dependency_missing,
                "UDP client not available",
                "common_udp_transport"
            ).to_common_error());
        }

        if (!client_->is_connected()) {
            send_failures_.fetch_add(1, std::memory_order_relaxed);
            return result_void::err(error_info(
                monitoring_error_code::network_error,
                "Not connected",
                "common_udp_transport"
            ).to_common_error());
        }

        auto result = client_->send(data);
        if (result.is_err()) {
            send_failures_.fetch_add(1, std::memory_order_relaxed);
            return result_void::err(error_info(
                monitoring_error_code::network_error,
                "Send failed: " + result.error().message,
                "common_udp_transport"
            ).to_common_error());
        }

        packets_sent_.fetch_add(1, std::memory_order_relaxed);
        bytes_sent_.fetch_add(data.size(), std::memory_order_relaxed);
        return common::ok();
    }

    bool is_connected() const override {
        return client_ && client_->is_connected();
    }

    void disconnect() override {
        if (client_) {
            client_->disconnect();
        }
    }

    bool is_available() const override {
        return client_ != nullptr;
    }

    std::string name() const override {
        if (client_) {
            return "common:" + client_->get_implementation_name();
        }
        return "common:unavailable";
    }

    udp_statistics get_statistics() const override {
        return {
            packets_sent_.load(std::memory_order_relaxed),
            bytes_sent_.load(std::memory_order_relaxed),
            send_failures_.load(std::memory_order_relaxed)
        };
    }

    void reset_statistics() override {
        packets_sent_.store(0, std::memory_order_relaxed);
        bytes_sent_.store(0, std::memory_order_relaxed);
        send_failures_.store(0, std::memory_order_relaxed);
    }
};

} } // namespace kcenon::monitoring

#endif // MONITORING_HAS_COMMON_TRANSPORT_INTERFACES

#ifdef MONITORING_HAS_NETWORK_SYSTEM
#include <kcenon/network/udp/udp_client.h>

namespace kcenon { namespace monitoring {

/**
 * @class network_udp_transport
 * @brief UDP transport implementation using network_system
 *
 * This implementation provides real UDP functionality by delegating
 * to network_system::udp::udp_client.
 */
class network_udp_transport : public udp_transport {
private:
    std::unique_ptr<network_system::udp::udp_client> client_;
    std::string host_;
    uint16_t port_{0};
    bool connected_{false};
    mutable std::atomic<std::size_t> packets_sent_{0};
    mutable std::atomic<std::size_t> bytes_sent_{0};
    mutable std::atomic<std::size_t> send_failures_{0};

public:
    network_udp_transport() = default;

    result_void connect(const std::string& host, uint16_t port) override {
        try {
            client_ = std::make_unique<network_system::udp::udp_client>(host, port);
            host_ = host;
            port_ = port;
            connected_ = true;
            return common::ok();
        } catch (const std::exception& e) {
            return result_void::err(error_info(
                monitoring_error_code::network_error,
                std::string("Connection failed: ") + e.what(),
                "network_udp_transport"
            ).to_common_error());
        }
    }

    result_void send(std::span<const uint8_t> data) override {
        if (!connected_ || !client_) {
            send_failures_.fetch_add(1, std::memory_order_relaxed);
            return result_void::err(error_info(
                monitoring_error_code::network_error,
                "Not connected",
                "network_udp_transport"
            ).to_common_error());
        }

        try {
            auto result = client_->send(data);
            if (!result) {
                send_failures_.fetch_add(1, std::memory_order_relaxed);
                return result_void::err(error_info(
                    monitoring_error_code::network_error,
                    "Send failed: " + result.error().message,
                    "network_udp_transport"
                ).to_common_error());
            }

            packets_sent_.fetch_add(1, std::memory_order_relaxed);
            bytes_sent_.fetch_add(data.size(), std::memory_order_relaxed);
            return common::ok();
        } catch (const std::exception& e) {
            send_failures_.fetch_add(1, std::memory_order_relaxed);
            return result_void::err(error_info(
                monitoring_error_code::network_error,
                std::string("Send failed: ") + e.what(),
                "network_udp_transport"
            ).to_common_error());
        }
    }

    bool is_connected() const override {
        return connected_ && client_ != nullptr;
    }

    void disconnect() override {
        client_.reset();
        connected_ = false;
        host_.clear();
        port_ = 0;
    }

    bool is_available() const override {
        return true;
    }

    std::string name() const override {
        return "network_system";
    }

    udp_statistics get_statistics() const override {
        return {
            packets_sent_.load(std::memory_order_relaxed),
            bytes_sent_.load(std::memory_order_relaxed),
            send_failures_.load(std::memory_order_relaxed)
        };
    }

    void reset_statistics() override {
        packets_sent_.store(0, std::memory_order_relaxed);
        bytes_sent_.store(0, std::memory_order_relaxed);
        send_failures_.store(0, std::memory_order_relaxed);
    }
};

} } // namespace kcenon::monitoring

#endif // MONITORING_HAS_NETWORK_SYSTEM

namespace kcenon { namespace monitoring {

/**
 * @brief Create default UDP transport
 *
 * Returns a network_system-based transport if available,
 * common_system transport if available, otherwise falls back
 * to a stub implementation.
 */
inline std::unique_ptr<udp_transport> create_default_udp_transport() {
#ifdef MONITORING_HAS_NETWORK_SYSTEM
    return std::make_unique<network_udp_transport>();
#else
    return std::make_unique<stub_udp_transport>();
#endif
}

/**
 * @brief Create stub UDP transport for testing
 */
inline std::unique_ptr<stub_udp_transport> create_stub_udp_transport() {
    return std::make_unique<stub_udp_transport>();
}

#ifdef MONITORING_HAS_COMMON_TRANSPORT_INTERFACES
/**
 * @brief Create common_system-based UDP transport
 * @param client The IUdpClient implementation to use
 */
inline std::unique_ptr<common_udp_transport> create_common_udp_transport(
    std::shared_ptr<::kcenon::common::interfaces::IUdpClient> client) {
    return std::make_unique<common_udp_transport>(std::move(client));
}
#endif

#ifdef MONITORING_HAS_NETWORK_SYSTEM
/**
 * @brief Create network_system-based UDP transport
 */
inline std::unique_ptr<network_udp_transport> create_network_udp_transport() {
    return std::make_unique<network_udp_transport>();
}
#endif

} } // namespace kcenon::monitoring
