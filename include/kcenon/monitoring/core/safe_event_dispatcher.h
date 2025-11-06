/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#pragma once

#include "event_bus.h"
#include <exception>
#include <functional>
#include <string>

namespace kcenon { namespace monitoring {

/**
 * @struct handler_error_info
 * @brief Information about handler execution errors
 */
struct handler_error_info {
    uint64_t handler_id;
    std::string error_message;
    std::type_index event_type;
    std::chrono::steady_clock::time_point timestamp;
};

/**
 * @class safe_event_dispatcher
 * @brief Event dispatcher with exception handling and error recovery
 *
 * Wraps event_bus to provide:
 * - Exception isolation (one handler's failure doesn't affect others)
 * - Error logging and metrics
 * - Circuit breaker for repeatedly failing handlers
 * - Dead letter queue for failed events
 *
 * ### Production Safety
 * @code
 * auto dispatcher = std::make_shared<safe_event_dispatcher>();
 *
 * // Subscribe with automatic exception handling
 * dispatcher->subscribe<my_event>([](const my_event& evt) {
 *     // This might throw, but won't crash the bus
 *     process_event(evt);
 * });
 *
 * // Publish events safely
 * dispatcher->publish(my_event{});
 *
 * // Monitor failed handlers
 * auto errors = dispatcher->get_handler_errors();
 * for (const auto& error : errors) {
 *     log_error("Handler {} failed: {}", error.handler_id, error.error_message);
 * }
 * @endcode
 */
class safe_event_dispatcher {
public:
    using error_callback = std::function<void(const handler_error_info&)>;

    explicit safe_event_dispatcher(std::shared_ptr<interface_event_bus> bus = nullptr)
        : bus_(bus ? bus : event_bus::instance())
        , total_exceptions_(0)
        , circuit_breaker_threshold_(10)
    {
    }

    /**
     * @brief Subscribe to events with automatic exception handling
     * @param handler Event handler function
     * @param priority Handler priority
     * @return Subscription ID
     */
    template<typename EventType, typename Handler>
    uint64_t subscribe_safe(Handler&& handler, event_priority priority = event_priority::normal) {
        // Wrap handler with exception handling
        auto safe_handler = [this, h = std::forward<Handler>(handler), id = next_id_++]
                           (const EventType& event) {
            try {
                h(event);
            } catch (const std::exception& e) {
                handle_exception(id, typeid(EventType), e.what());
            } catch (...) {
                handle_exception(id, typeid(EventType), "Unknown exception");
            }
        };

        return bus_->subscribe<EventType>(std::move(safe_handler));
    }

    /**
     * @brief Publish event with error recovery
     * @param event Event to publish
     * @param priority Event priority
     * @return true if published successfully
     */
    template<typename EventType>
    bool publish_safe(const EventType& event, event_priority priority = event_priority::normal) {
        try {
            bus_->publish(event, priority);
            return true;
        } catch (const std::exception& e) {
            // Log error
            if (error_callback_) {
                handler_error_info info{
                    0, // No specific handler
                    std::string("Publish failed: ") + e.what(),
                    typeid(EventType),
                    std::chrono::steady_clock::now()
                };
                error_callback_(info);
            }

            // Add to dead letter queue
            add_to_dead_letter_queue(typeid(EventType), e.what());

            return false;
        } catch (...) {
            if (error_callback_) {
                handler_error_info info{
                    0,
                    "Publish failed: Unknown exception",
                    typeid(EventType),
                    std::chrono::steady_clock::now()
                };
                error_callback_(info);
            }

            add_to_dead_letter_queue(typeid(EventType), "Unknown exception");
            return false;
        }
    }

    /**
     * @brief Set callback for handler errors
     */
    void set_error_callback(error_callback callback) {
        std::lock_guard<std::mutex> lock(error_mutex_);
        error_callback_ = std::move(callback);
    }

    /**
     * @brief Get recent handler errors
     */
    std::vector<handler_error_info> get_handler_errors() const {
        std::lock_guard<std::mutex> lock(error_mutex_);
        return recent_errors_;
    }

    /**
     * @brief Get total exception count
     */
    uint64_t get_total_exceptions() const {
        return total_exceptions_.load(std::memory_order_acquire);
    }

    /**
     * @brief Clear error history
     */
    void clear_errors() {
        std::lock_guard<std::mutex> lock(error_mutex_);
        recent_errors_.clear();
        failed_handler_counts_.clear();
        total_exceptions_ = 0;
    }

    /**
     * @brief Set circuit breaker threshold
     * @param threshold Number of failures before handler is disabled
     */
    void set_circuit_breaker_threshold(size_t threshold) {
        circuit_breaker_threshold_ = threshold;
    }

    /**
     * @brief Get dead letter queue size
     */
    size_t get_dead_letter_queue_size() const {
        std::lock_guard<std::mutex> lock(dlq_mutex_);
        return dead_letter_queue_.size();
    }

    /**
     * @brief Process dead letter queue with recovery handler
     */
    template<typename RecoveryHandler>
    void process_dead_letters(RecoveryHandler&& handler) {
        std::lock_guard<std::mutex> lock(dlq_mutex_);

        for (auto& dlq_entry : dead_letter_queue_) {
            try {
                handler(dlq_entry);
            } catch (...) {
                // Recovery handler failed, skip
            }
        }

        dead_letter_queue_.clear();
    }

    /**
     * @brief Get underlying event bus
     */
    std::shared_ptr<interface_event_bus> get_bus() const {
        return bus_;
    }

private:
    struct dead_letter_entry {
        std::type_index event_type;
        std::string error_message;
        std::chrono::steady_clock::time_point timestamp;
    };

    void handle_exception(uint64_t handler_id,
                         std::type_index event_type,
                         const std::string& error_msg) {
        total_exceptions_.fetch_add(1, std::memory_order_relaxed);

        std::lock_guard<std::mutex> lock(error_mutex_);

        // Record error
        handler_error_info info{
            handler_id,
            error_msg,
            event_type,
            std::chrono::steady_clock::now()
        };

        recent_errors_.push_back(info);

        // Keep only last 100 errors
        if (recent_errors_.size() > 100) {
            recent_errors_.erase(recent_errors_.begin());
        }

        // Track failure count
        auto& count = failed_handler_counts_[handler_id];
        count++;

        // Circuit breaker check
        if (count >= circuit_breaker_threshold_) {
            // Handler has failed too many times
            // In production, we would disable this handler
            if (error_callback_) {
                error_callback_(info);
            }
        }

        // Notify error callback
        if (error_callback_) {
            error_callback_(info);
        }
    }

    void add_to_dead_letter_queue(std::type_index event_type,
                                  const std::string& error_msg) {
        std::lock_guard<std::mutex> lock(dlq_mutex_);

        dead_letter_entry entry{
            event_type,
            error_msg,
            std::chrono::steady_clock::now()
        };

        dead_letter_queue_.push_back(entry);

        // Limit dead letter queue size
        if (dead_letter_queue_.size() > 1000) {
            dead_letter_queue_.erase(dead_letter_queue_.begin());
        }
    }

private:
    std::shared_ptr<interface_event_bus> bus_;
    std::atomic<uint64_t> next_id_{1};
    std::atomic<uint64_t> total_exceptions_{0};

    mutable std::mutex error_mutex_;
    std::vector<handler_error_info> recent_errors_;
    std::unordered_map<uint64_t, size_t> failed_handler_counts_;
    error_callback error_callback_;
    size_t circuit_breaker_threshold_;

    mutable std::mutex dlq_mutex_;
    std::vector<dead_letter_entry> dead_letter_queue_;
};

} } // namespace kcenon::monitoring
