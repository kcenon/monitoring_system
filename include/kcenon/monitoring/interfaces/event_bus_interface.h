// BSD 3-Clause License
//
// Copyright (c) 2021-2025, kcenon
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
 * @file event_bus_interface.h
 * @brief Event bus interface for decoupled component communication
 *
 * This file defines the event bus pattern interfaces that enable
 * publish-subscribe communication between monitoring components
 * without direct dependencies.
 *
 * C++20 Concepts are used to provide compile-time validation with
 * clear error messages for event types and handlers.
 */

#include <any>
#include <atomic>
#include <chrono>
#include <concepts>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <vector>
#include "../core/result_types.h"
#include "../config/feature_flags.h"

#if KCENON_HAS_COMMON_SYSTEM
#include <kcenon/common/concepts/event.h>
#endif

namespace kcenon { namespace monitoring {

namespace concepts {

/**
 * @concept EventType
 * @brief A type that can be used as an event in the event bus.
 *
 * Events must be class types that are copy-constructible to ensure
 * they can be safely passed to multiple handlers.
 *
 * This concept mirrors common_system's EventType concept for consistency.
 */
template <typename T>
concept EventType = std::is_class_v<T> && std::is_copy_constructible_v<T>;

/**
 * @concept EventHandler
 * @brief A callable that can handle events of a specific type.
 *
 * Event handlers receive events by const reference and return void.
 * They should not throw exceptions to avoid disrupting event dispatch.
 */
template <typename H, typename E>
concept EventHandler =
    std::invocable<H, const E&> && std::is_void_v<std::invoke_result_t<H, const E&>>;

/**
 * @concept EventFilter
 * @brief A callable that filters events based on criteria.
 *
 * Event filters receive events by const reference and return a boolean
 * indicating whether the event should be processed.
 */
template <typename F, typename E>
concept EventFilter =
    std::invocable<F, const E&> && std::convertible_to<std::invoke_result_t<F, const E&>, bool>;

} // namespace concepts

/**
 * @class event_base
 * @brief Base class for all events in the system
 *
 * Provides common functionality for all events including automatic
 * timestamp generation and unique ID assignment. All custom events
 * should inherit from this class.
 *
 * @thread_safety This class is thread-safe. The ID generation uses
 *                atomic operations, and the timestamp is captured
 *                at construction time.
 *
 * @example
 * @code
 * class custom_event : public event_base {
 * public:
 *     custom_event(const std::string& data) : data_(data) {}
 *
 *     std::string get_type_name() const override {
 *         return "custom_event";
 *     }
 *
 *     const std::string& data() const { return data_; }
 * private:
 *     std::string data_;
 * };
 * @endcode
 */
class event_base {
public:
    virtual ~event_base() = default;

    /**
     * @brief Get the event type name
     * @return String representation of the event type
     */
    virtual std::string get_type_name() const = 0;

    /**
     * @brief Get timestamp when event was created
     * @return Event creation timestamp
     */
    std::chrono::steady_clock::time_point get_timestamp() const {
        return timestamp_;
    }

    /**
     * @brief Get unique event ID
     * @return Event ID
     */
    uint64_t get_id() const {
        return id_;
    }

protected:
    event_base()
        : timestamp_(std::chrono::steady_clock::now()),
          id_(generate_id()) {}

private:
    static uint64_t generate_id() {
        static std::atomic<uint64_t> counter{0};
        return counter.fetch_add(1, std::memory_order_relaxed);
    }

    std::chrono::steady_clock::time_point timestamp_;
    uint64_t id_;
};

/**
 * @class event_priority
 * @brief Priority levels for event processing
 */
enum class event_priority {
    low,
    normal,
    high,
    critical
};

/**
 * @class event_handler
 * @brief Type-safe event handler wrapper
 */
template<typename EventType>
class event_handler {
public:
    using handler_func = std::function<void(const EventType&)>;

    event_handler(handler_func handler, event_priority priority = event_priority::normal)
        : handler_(handler), priority_(priority), id_(generate_id()) {}

    void operator()(const EventType& event) const {
        if (handler_) {
            handler_(event);
        }
    }

    event_priority get_priority() const { return priority_; }
    uint64_t get_id() const { return id_; }

private:
    static uint64_t generate_id() {
        static std::atomic<uint64_t> counter{0};
        return counter.fetch_add(1, std::memory_order_relaxed);
    }

    handler_func handler_;
    event_priority priority_;
    uint64_t id_;
};

/**
 * @class subscription_token
 * @brief Token for managing event subscriptions
 */
class subscription_token {
public:
    subscription_token(std::type_index event_type, uint64_t handler_id)
        : event_type_(event_type), handler_id_(handler_id) {}

    std::type_index get_event_type() const { return event_type_; }
    uint64_t get_handler_id() const { return handler_id_; }

private:
    std::type_index event_type_;
    uint64_t handler_id_;
};

/**
 * @class interface_event_bus
 * @brief Pure virtual interface for event bus implementation
 *
 * The event bus provides a centralized communication mechanism
 * for loosely coupled components using publish-subscribe pattern.
 * It supports typed events, priority-based handler execution,
 * and subscription management via tokens.
 *
 * @thread_safety Implementations MUST be thread-safe. Publishing and
 *                subscribing can occur from multiple threads simultaneously.
 *                Handler execution order is determined by priority, and
 *                handlers should not block for extended periods.
 *
 * @example
 * @code
 * // Define an event type
 * struct metric_alert_event {
 *     std::string metric_name;
 *     double threshold;
 *     double current_value;
 * };
 *
 * // Subscribe to events
 * auto bus = create_event_bus();
 * auto token = bus->subscribe_event<metric_alert_event>(
 *     [](const metric_alert_event& event) {
 *         std::cout << "Alert: " << event.metric_name
 *                   << " exceeded threshold!" << std::endl;
 *     },
 *     event_priority::high
 * );
 *
 * // Publish an event
 * metric_alert_event alert{"cpu_usage", 80.0, 95.5};
 * bus->publish_event(alert);
 *
 * // Cleanup
 * bus->unsubscribe_event(token.value());
 * @endcode
 *
 * @see subscription_token for subscription management
 * @see event_priority for handler ordering
 */
class interface_event_bus {
public:
    virtual ~interface_event_bus() = default;

    /**
     * @brief Publish an event to all subscribers
     * @tparam E The type of event to publish (must satisfy concepts::EventType)
     * @param event The event to publish
     * @return Result indicating success or failure
     *
     * The event type must be a class type and copy-constructible.
     * Using C++20 Concepts provides clear compile-time error messages
     * when these requirements are not met.
     */
    template <concepts::EventType E>
    common::VoidResult publish_event(const E& event) {
        return publish_event_impl(std::type_index(typeid(E)), std::make_any<E>(event));
    }

    /**
     * @brief Subscribe to events of a specific type
     * @tparam E The type of event to subscribe to (must satisfy concepts::EventType)
     * @param handler The handler function for the event
     * @param priority Priority for handler execution
     * @return Subscription token for managing the subscription
     *
     * The event type must be a class type and copy-constructible.
     * Using C++20 Concepts provides clear compile-time error messages
     * when these requirements are not met.
     */
    template <concepts::EventType E>
    common::Result<subscription_token> subscribe_event(std::function<void(const E&)> handler,
                                               event_priority priority = event_priority::normal) {
        auto wrapped_handler = event_handler<E>(handler, priority);
        return subscribe_event_impl(
            std::type_index(typeid(E)),
            [wrapped_handler](const std::any& event) { wrapped_handler(std::any_cast<E>(event)); },
            wrapped_handler.get_id(), priority);
    }

    /**
     * @brief Subscribe to events with a callable handler
     * @tparam E The type of event to subscribe to (must satisfy concepts::EventType)
     * @tparam H The handler type (must satisfy concepts::EventHandler<E>)
     * @param handler The handler callable
     * @param priority Priority for handler execution
     * @return Subscription token for managing the subscription
     *
     * This overload accepts any callable that satisfies the EventHandler concept,
     * providing more flexibility than the std::function overload.
     */
    template <concepts::EventType E, concepts::EventHandler<E> H>
    common::Result<subscription_token> subscribe_event(H&& handler,
                                               event_priority priority = event_priority::normal) {
        return subscribe_event(std::function<void(const E&)>(std::forward<H>(handler)), priority);
    }

    /**
     * @brief Unsubscribe from events using subscription token
     * @param token The subscription token
     * @return Result indicating success or failure
     */
    virtual common::VoidResult unsubscribe_event(const subscription_token& token) = 0;

    /**
     * @brief Clear all subscriptions for a specific event type
     * @tparam E The event type to clear subscriptions for (must satisfy concepts::EventType)
     * @return Result indicating success or failure
     */
    template <concepts::EventType E>
    common::VoidResult clear_subscriptions() {
        return clear_subscriptions_impl(std::type_index(typeid(E)));
    }

    /**
     * @brief Get the number of subscribers for an event type
     * @tparam E The event type to check (must satisfy concepts::EventType)
     * @return Number of subscribers
     */
    template <concepts::EventType E>
    size_t get_subscriber_count() const {
        return get_subscriber_count_impl(std::type_index(typeid(E)));
    }

    /**
     * @brief Check if event bus is active
     * @return True if active, false otherwise
     */
    virtual bool is_active() const = 0;

    /**
     * @brief Start the event bus
     * @return Result indicating success or failure
     */
    virtual common::VoidResult start() = 0;

    /**
     * @brief Stop the event bus
     * @return Result indicating success or failure
     */
    virtual common::VoidResult stop() = 0;

    /**
     * @brief Get pending event count
     * @return Number of events waiting to be processed
     */
    virtual size_t get_pending_event_count() const = 0;

    /**
     * @brief Process all pending events synchronously
     * @return Result indicating success or failure
     */
    virtual common::VoidResult process_pending_events() = 0;

protected:
    // Implementation methods to be overridden by concrete classes
    virtual common::VoidResult publish_event_impl(
        std::type_index event_type,
        std::any event) = 0;

    virtual common::Result<subscription_token> subscribe_event_impl(
        std::type_index event_type,
        std::function<void(const std::any&)> handler,
        uint64_t handler_id,
        event_priority priority) = 0;

    virtual common::VoidResult clear_subscriptions_impl(
        std::type_index event_type) = 0;

    virtual size_t get_subscriber_count_impl(
        std::type_index event_type) const = 0;
};

/**
 * @class interface_event_publisher
 * @brief Interface for components that publish events
 *
 * Components implementing this interface can publish events to an event bus.
 * This provides a standardized way to decouple event producers from consumers.
 *
 * @thread_safety Implementations should ensure thread-safe access to the
 *                event bus reference.
 *
 * @example
 * @code
 * class metric_alerter : public interface_event_publisher {
 * public:
 *     common::VoidResult set_event_bus(std::shared_ptr<interface_event_bus> bus) override {
 *         bus_ = bus;
 *         return common::ok();
 *     }
 *
 *     std::shared_ptr<interface_event_bus> get_event_bus() const override {
 *         return bus_;
 *     }
 *
 *     void check_thresholds() {
 *         if (auto bus = bus_) {
 *             bus->publish_event(threshold_exceeded_event{});
 *         }
 *     }
 * private:
 *     std::shared_ptr<interface_event_bus> bus_;
 * };
 * @endcode
 *
 * @see interface_event_bus for the event bus interface
 */
class interface_event_publisher {
public:
    virtual ~interface_event_publisher() = default;

    /**
     * @brief Set the event bus for publishing
     * @param bus The event bus to use
     * @return Result indicating success or failure
     */
    virtual common::VoidResult set_event_bus(std::shared_ptr<interface_event_bus> bus) = 0;

    /**
     * @brief Get the current event bus
     * @return Shared pointer to the event bus
     */
    virtual std::shared_ptr<interface_event_bus> get_event_bus() const = 0;
};

/**
 * @class interface_event_subscriber
 * @brief Interface for components that subscribe to events
 *
 * Components implementing this interface can subscribe to events from
 * an event bus. This provides lifecycle management for subscriptions
 * and ensures proper cleanup when components are destroyed.
 *
 * @thread_safety Implementations should ensure thread-safe subscription
 *                management, especially during subscribe/unsubscribe operations.
 *
 * @example
 * @code
 * class metric_logger : public interface_event_subscriber {
 * public:
 *     common::VoidResult subscribe_to_events(std::shared_ptr<interface_event_bus> bus) override {
 *         auto token = bus->subscribe_event<metric_event>(
 *             [this](const metric_event& e) { log_metric(e); }
 *         );
 *         if (token.is_ok()) {
 *             tokens_.push_back(token.value());
 *             bus_ = bus;
 *         }
 *         return common::ok();
 *     }
 *
 *     common::VoidResult unsubscribe_from_events() override {
 *         if (auto bus = bus_) {
 *             for (const auto& token : tokens_) {
 *                 bus->unsubscribe_event(token);
 *             }
 *             tokens_.clear();
 *         }
 *         return common::ok();
 *     }
 *
 *     std::vector<subscription_token> get_subscriptions() const override {
 *         return tokens_;
 *     }
 * private:
 *     std::weak_ptr<interface_event_bus> bus_;
 *     std::vector<subscription_token> tokens_;
 * };
 * @endcode
 *
 * @see interface_event_bus for event bus operations
 * @see subscription_token for subscription management
 */
class interface_event_subscriber {
public:
    virtual ~interface_event_subscriber() = default;

    /**
     * @brief Subscribe to required events
     * @param bus The event bus to subscribe to
     * @return Result indicating success or failure
     */
    virtual common::VoidResult subscribe_to_events(std::shared_ptr<interface_event_bus> bus) = 0;

    /**
     * @brief Unsubscribe from all events
     * @return Result indicating success or failure
     */
    virtual common::VoidResult unsubscribe_from_events() = 0;

    /**
     * @brief Get subscription tokens
     * @return Vector of active subscription tokens
     */
    virtual std::vector<subscription_token> get_subscriptions() const = 0;
};

} } // namespace kcenon::monitoring