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
    result_void publish_event(const E& event) {
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
    result<subscription_token> subscribe_event(std::function<void(const E&)> handler,
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
    result<subscription_token> subscribe_event(H&& handler,
                                               event_priority priority = event_priority::normal) {
        return subscribe_event(std::function<void(const E&)>(std::forward<H>(handler)), priority);
    }

    /**
     * @brief Unsubscribe from events using subscription token
     * @param token The subscription token
     * @return Result indicating success or failure
     */
    virtual result_void unsubscribe_event(const subscription_token& token) = 0;

    /**
     * @brief Clear all subscriptions for a specific event type
     * @tparam E The event type to clear subscriptions for (must satisfy concepts::EventType)
     * @return Result indicating success or failure
     */
    template <concepts::EventType E>
    result_void clear_subscriptions() {
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
    virtual result_void start() = 0;

    /**
     * @brief Stop the event bus
     * @return Result indicating success or failure
     */
    virtual result_void stop() = 0;

    /**
     * @brief Get pending event count
     * @return Number of events waiting to be processed
     */
    virtual size_t get_pending_event_count() const = 0;

    /**
     * @brief Process all pending events synchronously
     * @return Result indicating success or failure
     */
    virtual result_void process_pending_events() = 0;

protected:
    // Implementation methods to be overridden by concrete classes
    virtual result_void publish_event_impl(
        std::type_index event_type,
        std::any event) = 0;

    virtual result<subscription_token> subscribe_event_impl(
        std::type_index event_type,
        std::function<void(const std::any&)> handler,
        uint64_t handler_id,
        event_priority priority) = 0;

    virtual result_void clear_subscriptions_impl(
        std::type_index event_type) = 0;

    virtual size_t get_subscriber_count_impl(
        std::type_index event_type) const = 0;
};

/**
 * @class interface_event_publisher
 * @brief Interface for components that publish events
 */
class interface_event_publisher {
public:
    virtual ~interface_event_publisher() = default;

    /**
     * @brief Set the event bus for publishing
     * @param bus The event bus to use
     * @return Result indicating success or failure
     */
    virtual result_void set_event_bus(std::shared_ptr<interface_event_bus> bus) = 0;

    /**
     * @brief Get the current event bus
     * @return Shared pointer to the event bus
     */
    virtual std::shared_ptr<interface_event_bus> get_event_bus() const = 0;
};

/**
 * @class interface_event_subscriber
 * @brief Interface for components that subscribe to events
 */
class interface_event_subscriber {
public:
    virtual ~interface_event_subscriber() = default;

    /**
     * @brief Subscribe to required events
     * @param bus The event bus to subscribe to
     * @return Result indicating success or failure
     */
    virtual result_void subscribe_to_events(std::shared_ptr<interface_event_bus> bus) = 0;

    /**
     * @brief Unsubscribe from all events
     * @return Result indicating success or failure
     */
    virtual result_void unsubscribe_from_events() = 0;

    /**
     * @brief Get subscription tokens
     * @return Vector of active subscription tokens
     */
    virtual std::vector<subscription_token> get_subscriptions() const = 0;
};

} } // namespace kcenon::monitoring