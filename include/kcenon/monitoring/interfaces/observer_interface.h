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
 * @file observer_interface.h
 * @brief Observer pattern interface for monitoring system event handling
 *
 * This file defines the core observer pattern interfaces that enable
 * loose coupling between monitoring components through event-driven
 * communication.
 */

#include <chrono>
#include <memory>
#include <string>
#include <variant>
#include "metric_types_adapter.h"
#include "../core/result_types.h"

namespace kcenon { namespace monitoring {

// Forward declarations
class metric_event;
class system_event;
class state_change_event;

/**
 * @class metric_event
 * @brief Event fired when a metric is collected
 *
 * This event is published whenever a metric is collected from a source.
 * It contains the source identifier, the metric data, and a timestamp
 * for when the event was created.
 *
 * @thread_safety This class is thread-safe for read operations.
 *                Once constructed, all accessors are const and can be
 *                called from multiple threads.
 *
 * @example
 * @code
 * metric m("cpu_usage", 75.5, {{"host", "server1"}});
 * metric_event event("cpu_collector", m);
 *
 * std::cout << "Source: " << event.source() << std::endl;
 * std::cout << "Metric: " << event.data().name << std::endl;
 * @endcode
 */
class metric_event {
public:
    metric_event(const std::string& source, const metric& data)
        : source_(source), metric_data_(data), timestamp_(std::chrono::steady_clock::now()) {}

    const std::string& source() const { return source_; }
    const metric& data() const { return metric_data_; }
    std::chrono::steady_clock::time_point timestamp() const { return timestamp_; }

private:
    std::string source_;
    metric metric_data_;
    std::chrono::steady_clock::time_point timestamp_;
};

/**
 * @class system_event
 * @brief Generic system event for monitoring components
 *
 * Represents various system-level events such as component lifecycle
 * changes, errors, warnings, and configuration updates. Each event
 * has a type, source component, message, and timestamp.
 *
 * @thread_safety This class is thread-safe for read operations.
 *                Once constructed, all accessors are const.
 *
 * @example
 * @code
 * system_event startup(
 *     system_event::event_type::component_started,
 *     "performance_monitor",
 *     "Monitor initialized successfully"
 * );
 *
 * if (startup.type() == system_event::event_type::component_started) {
 *     log_info(startup.message());
 * }
 * @endcode
 */
class system_event {
public:
    enum class event_type {
        component_started,
        component_stopped,
        error_occurred,
        warning_raised,
        configuration_changed,
        threshold_exceeded
    };

    system_event(event_type type, const std::string& component, const std::string& message)
        : type_(type), component_(component), message_(message),
          timestamp_(std::chrono::steady_clock::now()) {}

    event_type type() const { return type_; }
    const std::string& component() const { return component_; }
    const std::string& message() const { return message_; }
    std::chrono::steady_clock::time_point timestamp() const { return timestamp_; }

private:
    event_type type_;
    std::string component_;
    std::string message_;
    std::chrono::steady_clock::time_point timestamp_;
};

/**
 * @class state_change_event
 * @brief Event fired when system state changes
 *
 * Represents a transition from one health state to another for a
 * specific component. Useful for tracking system health over time
 * and triggering alerts on degradation.
 *
 * @thread_safety This class is thread-safe for read operations.
 *                Once constructed, all accessors are const.
 *
 * @example
 * @code
 * state_change_event degradation(
 *     "database",
 *     state_change_event::state::healthy,
 *     state_change_event::state::degraded
 * );
 *
 * if (degradation.new_state() == state_change_event::state::degraded) {
 *     alert_ops_team(degradation.component());
 * }
 * @endcode
 */
class state_change_event {
public:
    enum class state {
        healthy,
        degraded,
        critical,
        unknown
    };

    state_change_event(const std::string& component, state old_state, state new_state)
        : component_(component), old_state_(old_state), new_state_(new_state),
          timestamp_(std::chrono::steady_clock::now()) {}

    const std::string& component() const { return component_; }
    state old_state() const { return old_state_; }
    state new_state() const { return new_state_; }
    std::chrono::steady_clock::time_point timestamp() const { return timestamp_; }

private:
    std::string component_;
    state old_state_;
    state new_state_;
    std::chrono::steady_clock::time_point timestamp_;
};

/**
 * @class interface_monitoring_observer
 * @brief Pure virtual interface for monitoring event observers
 *
 * Components implementing this interface can subscribe to monitoring
 * events and react to metrics, system events, and state changes.
 * Observers are notified synchronously when events occur.
 *
 * @thread_safety Implementations MUST be thread-safe. Observer methods
 *                may be called from multiple threads simultaneously.
 *                Avoid blocking operations in observer methods to prevent
 *                delaying event dispatch to other observers.
 *
 * @example
 * @code
 * class logging_observer : public interface_monitoring_observer {
 * public:
 *     void on_metric_collected(const metric_event& event) override {
 *         logger_.info("Metric {} = {}", event.data().name, event.data().value);
 *     }
 *
 *     void on_event_occurred(const system_event& event) override {
 *         logger_.warn("System event: {}", event.message());
 *     }
 *
 *     void on_system_state_changed(const state_change_event& event) override {
 *         logger_.error("State change: {} -> {}", event.old_state(), event.new_state());
 *     }
 * private:
 *     Logger logger_;
 * };
 * @endcode
 *
 * @see interface_observable for registering observers
 */
class interface_monitoring_observer {
public:
    virtual ~interface_monitoring_observer() = default;

    /**
     * @brief Called when a metric is collected
     * @param event The metric collection event
     */
    virtual void on_metric_collected(const metric_event& event) = 0;

    /**
     * @brief Called when a system event occurs
     * @param event The system event
     */
    virtual void on_event_occurred(const system_event& event) = 0;

    /**
     * @brief Called when system state changes
     * @param event The state change event
     */
    virtual void on_system_state_changed(const state_change_event& event) = 0;
};

/**
 * @class interface_observable
 * @brief Interface for components that can be observed
 *
 * Provides the subject side of the observer pattern. Components
 * implementing this interface can register observers and notify
 * them of metric events, system events, and state changes.
 *
 * @thread_safety Implementations MUST be thread-safe. Observer
 *                registration/unregistration and notification can
 *                occur from multiple threads simultaneously.
 *
 * @example
 * @code
 * class metric_collector : public interface_observable {
 * public:
 *     common::VoidResult register_observer(std::shared_ptr<interface_monitoring_observer> observer) override {
 *         std::lock_guard<std::mutex> lock(mutex_);
 *         observers_.push_back(observer);
 *         return common::ok();
 *     }
 *
 *     void notify_metric(const metric_event& event) override {
 *         std::lock_guard<std::mutex> lock(mutex_);
 *         for (const auto& observer : observers_) {
 *             observer->on_metric_collected(event);
 *         }
 *     }
 *     // ... implement other methods
 * private:
 *     std::mutex mutex_;
 *     std::vector<std::shared_ptr<interface_monitoring_observer>> observers_;
 * };
 * @endcode
 *
 * @see interface_monitoring_observer for observer implementation
 */
class interface_observable {
public:
    virtual ~interface_observable() = default;

    /**
     * @brief Register an observer for events
     * @param observer The observer to register
     * @return Result indicating success or failure
     */
    virtual common::VoidResult register_observer(std::shared_ptr<interface_monitoring_observer> observer) = 0;

    /**
     * @brief Unregister an observer
     * @param observer The observer to unregister
     * @return Result indicating success or failure
     */
    virtual common::VoidResult unregister_observer(std::shared_ptr<interface_monitoring_observer> observer) = 0;

    /**
     * @brief Notify all observers of a metric event
     * @param event The metric event to broadcast
     */
    virtual void notify_metric(const metric_event& event) = 0;

    /**
     * @brief Notify all observers of a system event
     * @param event The system event to broadcast
     */
    virtual void notify_event(const system_event& event) = 0;

    /**
     * @brief Notify all observers of a state change
     * @param event The state change event to broadcast
     */
    virtual void notify_state_change(const state_change_event& event) = 0;
};

} } // namespace kcenon::monitoring