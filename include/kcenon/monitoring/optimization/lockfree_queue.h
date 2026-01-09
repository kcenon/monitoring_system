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

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "kcenon/monitoring/core/result_types.h"

// Disable MSVC warning C4324: structure was padded due to alignment specifier
// This is intentional for cache line optimization in lock-free data structures
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4324)
#endif

namespace kcenon::monitoring {

/**
 * @brief Configuration for lock-free queue
 */
struct lockfree_queue_config {
    size_t initial_capacity = 1024;    ///< Initial capacity of the queue
    size_t max_capacity = 65536;       ///< Maximum capacity (0 = unlimited)
    bool allow_overwrite = false;      ///< Allow overwriting oldest elements when full

    /**
     * @brief Validate configuration
     * @return true if configuration is valid
     */
    bool validate() const {
        if (initial_capacity == 0) {
            return false;
        }
        if (max_capacity != 0 && max_capacity < initial_capacity) {
            return false;
        }
        return true;
    }
};

/**
 * @brief Statistics for lock-free queue operations
 */
struct lockfree_queue_statistics {
    std::atomic<size_t> push_attempts{0};
    std::atomic<size_t> push_successes{0};
    std::atomic<size_t> push_failures{0};
    std::atomic<size_t> pop_attempts{0};
    std::atomic<size_t> pop_successes{0};
    std::atomic<size_t> pop_failures{0};

    lockfree_queue_statistics() = default;
    lockfree_queue_statistics(const lockfree_queue_statistics& other)
        : push_attempts(other.push_attempts.load())
        , push_successes(other.push_successes.load())
        , push_failures(other.push_failures.load())
        , pop_attempts(other.pop_attempts.load())
        , pop_successes(other.pop_successes.load())
        , pop_failures(other.pop_failures.load()) {}

    /**
     * @brief Get push success rate
     * @return Success rate between 0.0 and 100.0
     */
    double get_push_success_rate() const {
        auto attempts = push_attempts.load();
        if (attempts == 0) {
            return 100.0;
        }
        return (static_cast<double>(push_successes.load()) / static_cast<double>(attempts)) * 100.0;
    }

    /**
     * @brief Get pop success rate
     * @return Success rate between 0.0 and 100.0
     */
    double get_pop_success_rate() const {
        auto attempts = pop_attempts.load();
        if (attempts == 0) {
            return 100.0;
        }
        return (static_cast<double>(pop_successes.load()) / static_cast<double>(attempts)) * 100.0;
    }

    /**
     * @brief Reset all statistics
     */
    void reset() {
        push_attempts.store(0);
        push_successes.store(0);
        push_failures.store(0);
        pop_attempts.store(0);
        pop_successes.store(0);
        pop_failures.store(0);
    }
};

/**
 * @brief Thread-safe lock-free MPMC (Multiple Producer Multiple Consumer) queue
 *
 * This implementation uses a bounded ring buffer with atomic operations
 * for thread-safe access without locks.
 *
 * @tparam T The type of elements stored in the queue
 */
template<typename T>
class lockfree_queue {
public:
    /**
     * @brief Default constructor with default configuration
     */
    lockfree_queue() : lockfree_queue(lockfree_queue_config{}) {}

    /**
     * @brief Construct with configuration
     * @param config Queue configuration
     */
    explicit lockfree_queue(const lockfree_queue_config& config)
        : config_(config)
        , capacity_(config.initial_capacity)
        , buffer_(config.initial_capacity)
        , head_(0)
        , tail_(0)
        , size_(0) {
        // Initialize each slot's sequence to its index
        for (size_t i = 0; i < capacity_; ++i) {
            buffer_[i].sequence.store(i, std::memory_order_relaxed);
        }
    }

    // Disable copy
    lockfree_queue(const lockfree_queue&) = delete;
    lockfree_queue& operator=(const lockfree_queue&) = delete;

    // Enable move
    lockfree_queue(lockfree_queue&& other) noexcept
        : config_(std::move(other.config_))
        , capacity_(other.capacity_)
        , buffer_(std::move(other.buffer_))
        , head_(other.head_.load())
        , tail_(other.tail_.load())
        , size_(other.size_.load())
        , stats_(std::move(other.stats_)) {}

    lockfree_queue& operator=(lockfree_queue&& other) noexcept {
        if (this != &other) {
            config_ = std::move(other.config_);
            capacity_ = other.capacity_;
            buffer_ = std::move(other.buffer_);
            head_.store(other.head_.load());
            tail_.store(other.tail_.load());
            size_.store(other.size_.load());
            stats_ = std::move(other.stats_);
        }
        return *this;
    }

    /**
     * @brief Push an element to the queue
     * @param value The value to push
     * @return result<bool> containing true on success, false if queue is full
     */
    result<bool> push(const T& value) {
        return push_impl(value);
    }

    /**
     * @brief Push an element to the queue (move version)
     * @param value The value to push
     * @return result<bool> containing true on success, false if queue is full
     */
    result<bool> push(T&& value) {
        return push_impl(std::move(value));
    }

    /**
     * @brief Pop an element from the queue
     * @return result<T> containing the value on success, error if queue is empty
     */
    result<T> pop() {
        stats_.pop_attempts++;

        size_t current_head = head_.load(std::memory_order_relaxed);

        while (true) {
            size_t index = current_head % capacity_;
            auto& slot = buffer_[index];
            size_t seq = slot.sequence.load(std::memory_order_acquire);
            intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(current_head + 1);

            if (diff == 0) {
                // Slot is ready to be read
                if (head_.compare_exchange_weak(current_head, current_head + 1,
                                                 std::memory_order_relaxed)) {
                    T value = std::move(slot.data);
                    slot.sequence.store(current_head + capacity_, std::memory_order_release);
                    size_.fetch_sub(1, std::memory_order_relaxed);
                    stats_.pop_successes++;
                    return common::ok(std::move(value));
                }
            } else if (diff < 0) {
                // Queue is empty
                stats_.pop_failures++;
                return make_error<T>(monitoring_error_code::resource_unavailable,
                                     "Queue is empty");
            } else {
                // Another thread is modifying, retry
                current_head = head_.load(std::memory_order_relaxed);
            }
        }
    }

    /**
     * @brief Check if the queue is empty
     * @return true if empty
     */
    bool empty() const {
        return size_.load(std::memory_order_relaxed) == 0;
    }

    /**
     * @brief Get current queue size
     * @return Number of elements in the queue
     */
    size_t size() const {
        return size_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Get queue capacity
     * @return Current capacity of the queue
     */
    size_t capacity() const {
        return capacity_;
    }

    /**
     * @brief Get queue statistics
     * @return Reference to statistics
     */
    const lockfree_queue_statistics& get_statistics() const {
        return stats_;
    }

    /**
     * @brief Reset statistics
     */
    void reset_statistics() {
        stats_.reset();
    }

private:
    struct alignas(64) slot {
        std::atomic<size_t> sequence;
        T data;
    };

    template<typename U>
    result<bool> push_impl(U&& value) {
        stats_.push_attempts++;

        size_t current_tail = tail_.load(std::memory_order_relaxed);

        while (true) {
            size_t index = current_tail % capacity_;
            auto& slot = buffer_[index];
            size_t seq = slot.sequence.load(std::memory_order_acquire);
            intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(current_tail);

            if (diff == 0) {
                // Slot is available for writing
                if (tail_.compare_exchange_weak(current_tail, current_tail + 1,
                                                 std::memory_order_relaxed)) {
                    slot.data = std::forward<U>(value);
                    slot.sequence.store(current_tail + 1, std::memory_order_release);
                    size_.fetch_add(1, std::memory_order_relaxed);
                    stats_.push_successes++;
                    return common::ok(true);
                }
            } else if (diff < 0) {
                // Queue is full
                stats_.push_failures++;
                return common::ok(false);
            } else {
                // Another thread is modifying, retry
                current_tail = tail_.load(std::memory_order_relaxed);
            }
        }
    }

    lockfree_queue_config config_;
    size_t capacity_;
    std::vector<slot> buffer_;
    alignas(64) std::atomic<size_t> head_;
    alignas(64) std::atomic<size_t> tail_;
    alignas(64) std::atomic<size_t> size_;
    mutable lockfree_queue_statistics stats_;
};

/**
 * @brief Create a lock-free queue with default configuration
 * @tparam T The element type
 * @return Unique pointer to the queue
 */
template<typename T>
std::unique_ptr<lockfree_queue<T>> make_lockfree_queue() {
    return std::make_unique<lockfree_queue<T>>();
}

/**
 * @brief Create a lock-free queue with configuration
 * @tparam T The element type
 * @param config Queue configuration
 * @return Unique pointer to the queue
 */
template<typename T>
std::unique_ptr<lockfree_queue<T>> make_lockfree_queue(const lockfree_queue_config& config) {
    return std::make_unique<lockfree_queue<T>>(config);
}

/**
 * @brief Create default queue configurations for different use cases
 * @return Vector of configurations
 */
inline std::vector<lockfree_queue_config> create_default_queue_configs() {
    return {
        // Small queue for low-throughput scenarios
        {.initial_capacity = 64, .max_capacity = 256, .allow_overwrite = false},
        // Medium queue for general use
        {.initial_capacity = 1024, .max_capacity = 4096, .allow_overwrite = false},
        // Large queue for high-throughput scenarios
        {.initial_capacity = 4096, .max_capacity = 65536, .allow_overwrite = false},
        // Overwrite queue for streaming data
        {.initial_capacity = 1024, .max_capacity = 1024, .allow_overwrite = true}
    };
}

} // namespace kcenon::monitoring

#ifdef _MSC_VER
#pragma warning(pop)
#endif
