#pragma once

/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, monitoring_system contributors
All rights reserved.
*****************************************************************************/

/**
 * @file ring_buffer.h
 * @brief Lock-free ring buffer for efficient metric storage
 * 
 * This file provides a high-performance, memory-efficient ring buffer
 * implementation specifically designed for metric storage with minimal
 * allocation overhead and cache-friendly access patterns.
 */

#include <kcenon/monitoring/core/result_types.h>
#include <kcenon/monitoring/core/error_codes.h>
#include <memory>
#include <atomic>
#include <vector>
#include <chrono>
#include <cstddef>
#include <type_traits>

namespace kcenon { namespace monitoring {

/**
 * @struct ring_buffer_config
 * @brief Configuration for ring buffer behavior
 */
struct ring_buffer_config {
    size_t capacity = 8192;                        // Default capacity (power of 2)
    bool overwrite_old = true;                     // Overwrite oldest data when full
    size_t batch_size = 64;                        // Batch size for bulk operations
    std::chrono::milliseconds gc_interval{1000};   // Garbage collection interval
    
    /**
     * @brief Validate ring buffer configuration
     */
    result_void validate() const {
        if (capacity == 0 || (capacity & (capacity - 1)) != 0) {
            return result_void(monitoring_error_code::invalid_configuration,
                             "Capacity must be a power of 2");
        }
        
        if (batch_size == 0 || batch_size > capacity) {
            return result_void(monitoring_error_code::invalid_configuration,
                             "Invalid batch size");
        }
        
        return result_void::success();
    }
};

/**
 * @struct ring_buffer_stats
 * @brief Statistics for ring buffer performance monitoring
 */
struct ring_buffer_stats {
    std::atomic<size_t> total_writes{0};
    std::atomic<size_t> total_reads{0};
    std::atomic<size_t> overwrites{0};
    std::atomic<size_t> failed_writes{0};
    std::atomic<size_t> failed_reads{0};
    std::atomic<size_t> contention_retries{0};
    std::chrono::system_clock::time_point creation_time;

    ring_buffer_stats() : creation_time(std::chrono::system_clock::now()) {}

    /**
     * @brief Get current utilization percentage
     */
    double get_utilization(size_t current_size, size_t capacity) const {
        return capacity > 0 ? (static_cast<double>(current_size) / capacity) * 100.0 : 0.0;
    }

    /**
     * @brief Get write success rate
     */
    double get_write_success_rate() const {
        auto total = total_writes.load();
        auto failed = failed_writes.load();
        return total > 0 ? (1.0 - static_cast<double>(failed) / total) * 100.0 : 100.0;
    }

    /**
     * @brief Get overflow rate (overwrites per total writes)
     */
    double get_overflow_rate() const {
        auto total = total_writes.load();
        auto overflows = overwrites.load();
        return total > 0 ? (static_cast<double>(overflows) / total) * 100.0 : 0.0;
    }

    /**
     * @brief Check if overflow rate is high (> 10%)
     */
    bool is_overflow_rate_high() const {
        return get_overflow_rate() > 10.0;
    }

    /**
     * @brief Get average contention (retries per write)
     */
    double get_avg_contention() const {
        auto total = total_writes.load();
        auto retries = contention_retries.load();
        return total > 0 ? static_cast<double>(retries) / total : 0.0;
    }

    /**
     * @brief Get read success rate
     */
    double get_read_success_rate() const {
        auto total = total_reads.load();
        auto failed = failed_reads.load();
        return total > 0 ? (1.0 - static_cast<double>(failed) / total) * 100.0 : 100.0;
    }
};

/**
 * @class ring_buffer
 * @brief Lock-free ring buffer with atomic operations
 * @tparam T The type of elements to store
 * 
 * This implementation uses atomic operations for thread-safety and
 * provides efficient circular buffer semantics with configurable
 * overflow behavior.
 */
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4324) // structure was padded due to alignment specifier
#endif

template<typename T>
class ring_buffer {
private:
    static_assert(std::is_move_constructible_v<T>, "T must be move constructible");
    static_assert(std::is_move_assignable_v<T>, "T must be move assignable");
    
    alignas(64) std::atomic<size_t> write_index_{0};  // Cache line aligned
    alignas(64) std::atomic<size_t> read_index_{0};   // Cache line aligned
    
    std::unique_ptr<T[]> buffer_;
    ring_buffer_config config_;
    mutable ring_buffer_stats stats_;
    
    /**
     * @brief Get the mask for efficient modulo operation
     */
    size_t get_mask() const noexcept {
        return config_.capacity - 1;
    }
    
    /**
     * @brief Check if buffer is full
     */
    bool is_full_unsafe(size_t write_idx, size_t read_idx) const noexcept {
        return ((write_idx + 1) & get_mask()) == read_idx;
    }
    
    /**
     * @brief Check if buffer is empty
     */
    bool is_empty_unsafe(size_t write_idx, size_t read_idx) const noexcept {
        return write_idx == read_idx;
    }
    
public:
    /**
     * @brief Constructor with configuration
     */
    explicit ring_buffer(const ring_buffer_config& config = {})
        : buffer_(std::make_unique<T[]>(config.capacity))
        , config_(config) {
        
        // Validate configuration
        auto validation = config_.validate();
        if (validation.is_err()) {
            throw std::invalid_argument("Invalid ring buffer configuration: " +
                                      validation.error().message);
        }
    }
    
    // Non-copyable but moveable
    ring_buffer(const ring_buffer&) = delete;
    ring_buffer& operator=(const ring_buffer&) = delete;
    ring_buffer(ring_buffer&&) = default;
    ring_buffer& operator=(ring_buffer&&) = default;
    
    /**
     * @brief Write a single element to the buffer
     * @param item Item to write
     * @return Result indicating success or failure
     */
    result_void write(T&& item) {
        stats_.total_writes.fetch_add(1, std::memory_order_relaxed);

        // Atomically claim a write slot using CAS loop to avoid ABA problem
        size_t current_write;
        size_t new_write;
        bool overflow_handled = false;
        size_t retry_count = 0;
        constexpr size_t max_retries = 100;

        do {
            current_write = write_index_.load(std::memory_order_acquire);
            size_t current_read = read_index_.load(std::memory_order_acquire);

            // Check for buffer full condition
            if (is_full_unsafe(current_write, current_read)) {
                if (config_.overwrite_old) {
                    // Advance read index to overwrite oldest data
                    // Use strong CAS in a loop to ensure it succeeds
                    size_t expected_read = current_read;
                    size_t new_read = (current_read + 1) & get_mask();

                    // Try to advance read index with strong CAS
                    // If it fails, another thread already advanced it, which is fine
                    if (read_index_.compare_exchange_strong(expected_read, new_read,
                                                           std::memory_order_acq_rel,
                                                           std::memory_order_acquire)) {
                        // Successfully advanced read index
                        if (!overflow_handled) {
                            stats_.overwrites.fetch_add(1, std::memory_order_relaxed);
                            overflow_handled = true;
                        }
                    }
                    // Continue to claim write slot even if CAS failed
                    // (another thread may have already made space)
                } else {
                    // Buffer is full and overwrite is not allowed
                    stats_.failed_writes.fetch_add(1, std::memory_order_relaxed);

                    // Provide more detailed error information
                    size_t current_size = size();
                    return result_void(monitoring_error_code::storage_full,
                                     "Ring buffer is full (size: " +
                                     std::to_string(current_size) +
                                     "/" + std::to_string(config_.capacity) +
                                     ", overwrites: " +
                                     std::to_string(stats_.overwrites.load()) + ")");
                }
            }

            new_write = (current_write + 1) & get_mask();

            // Prevent infinite loop in case of extreme contention
            if (++retry_count > max_retries) {
                stats_.failed_writes.fetch_add(1, std::memory_order_relaxed);
                return result_void(monitoring_error_code::collection_failed,
                                 "Failed to write to ring buffer after " +
                                 std::to_string(max_retries) + " retries (high contention)");
            }

            // Atomically claim the write slot
        } while (!write_index_.compare_exchange_weak(current_write, new_write,
                                                     std::memory_order_acq_rel,
                                                     std::memory_order_acquire));

        // Write the item to the claimed slot
        buffer_[current_write] = std::move(item);

        // Memory fence ensures data write completes before index update is visible
        std::atomic_thread_fence(std::memory_order_release);

        return result_void::success();
    }
    
    /**
     * @brief Write multiple elements in batch
     * @param items Vector of items to write
     * @return Number of items successfully written
     */
    size_t write_batch(std::vector<T>&& items) {
        if (items.empty()) {
            return 0;
        }

        size_t written = 0;
        size_t failed = 0;

        for (auto& item : items) {
            auto result = write(std::move(item));
            if (result.is_ok()) {
                ++written;
            } else {
                ++failed;
                // Stop on first failure if not overwriting to prevent data loss
                if (!config_.overwrite_old) {
                    break;
                }
            }
        }

        return written;
    }
    
    /**
     * @brief Read a single element from the buffer
     * @param item Reference to store the read item
     * @return Result indicating success or failure
     */
    result_void read(T& item) {
        stats_.total_reads.fetch_add(1, std::memory_order_relaxed);
        
        size_t current_read = read_index_.load(std::memory_order_acquire);
        size_t current_write = write_index_.load(std::memory_order_acquire);
        
        if (is_empty_unsafe(current_write, current_read)) {
            stats_.failed_reads.fetch_add(1, std::memory_order_relaxed);
            return result_void(monitoring_error_code::collection_failed,
                             "Ring buffer is empty");
        }
        
        // Read the item
        item = std::move(buffer_[current_read]);
        
        // Update read index
        size_t new_read = (current_read + 1) & get_mask();
        read_index_.store(new_read, std::memory_order_release);
        
        return result_void::success();
    }
    
    /**
     * @brief Read multiple elements in batch
     * @param items Vector to store read items
     * @param max_count Maximum number of items to read
     * @return Number of items actually read
     */
    size_t read_batch(std::vector<T>& items, size_t max_count = SIZE_MAX) {
        if (max_count == 0) {
            return 0;
        }
        
        size_t batch_size = std::min(max_count, config_.batch_size);
        items.reserve(items.size() + batch_size);
        
        size_t read_count = 0;
        T temp_item;
        
        while (read_count < batch_size) {
            auto result = read(temp_item);
            if (result.is_err()) {
                break; // No more items to read
            }
            
            items.emplace_back(std::move(temp_item));
            ++read_count;
        }
        
        return read_count;
    }
    
    /**
     * @brief Peek at the next item without removing it
     * @param item Reference to store the peeked item
     * @return Result indicating success or failure
     */
    result_void peek(T& item) const {
        size_t current_read = read_index_.load(std::memory_order_acquire);
        size_t current_write = write_index_.load(std::memory_order_acquire);
        
        if (is_empty_unsafe(current_write, current_read)) {
            return result_void(monitoring_error_code::collection_failed,
                             "Ring buffer is empty");
        }
        
        item = buffer_[current_read]; // Copy, don't move
        return result_void::success();
    }
    
    /**
     * @brief Get current number of elements in buffer
     */
    size_t size() const noexcept {
        size_t write_idx = write_index_.load(std::memory_order_acquire);
        size_t read_idx = read_index_.load(std::memory_order_acquire);

        // Handle wraparound correctly
        if (write_idx >= read_idx) {
            return write_idx - read_idx;
        } else {
            // Wraparound case: write has wrapped around but read hasn't
            return config_.capacity - read_idx + write_idx;
        }
    }
    
    /**
     * @brief Check if buffer is empty
     */
    bool empty() const noexcept {
        return size() == 0;
    }
    
    /**
     * @brief Check if buffer is full
     */
    bool full() const noexcept {
        size_t write_idx = write_index_.load(std::memory_order_acquire);
        size_t read_idx = read_index_.load(std::memory_order_acquire);
        return is_full_unsafe(write_idx, read_idx);
    }
    
    /**
     * @brief Get buffer capacity
     */
    size_t capacity() const noexcept {
        return config_.capacity;
    }
    
    /**
     * @brief Clear all elements in the buffer
     */
    void clear() noexcept {
        write_index_.store(0, std::memory_order_release);
        read_index_.store(0, std::memory_order_release);
    }
    
    /**
     * @brief Get buffer configuration
     */
    const ring_buffer_config& get_config() const noexcept {
        return config_;
    }
    
    /**
     * @brief Get buffer statistics
     */
    const ring_buffer_stats& get_stats() const noexcept {
        return stats_;
    }
    
    /**
     * @brief Reset statistics
     */
    void reset_stats() noexcept {
        stats_.total_writes.store(0);
        stats_.total_reads.store(0);
        stats_.overwrites.store(0);
        stats_.failed_writes.store(0);
        stats_.failed_reads.store(0);
        stats_.contention_retries.store(0);
        stats_.creation_time = std::chrono::system_clock::now();
    }

    /**
     * @brief Check if buffer is experiencing high overflow
     * @return true if overflow rate exceeds threshold
     */
    bool is_overflow_rate_high() const noexcept {
        return stats_.is_overflow_rate_high();
    }

    /**
     * @brief Get overflow rate percentage
     */
    double get_overflow_rate() const noexcept {
        return stats_.get_overflow_rate();
    }
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

/**
 * @brief Helper function to create a ring buffer with default configuration
 */
template<typename T>
std::unique_ptr<ring_buffer<T>> make_ring_buffer(size_t capacity = 8192) {
    ring_buffer_config config;
    config.capacity = capacity;
    return std::make_unique<ring_buffer<T>>(config);
}

/**
 * @brief Helper function to create a ring buffer with custom configuration
 */
template<typename T>
std::unique_ptr<ring_buffer<T>> make_ring_buffer(const ring_buffer_config& config) {
    return std::make_unique<ring_buffer<T>>(config);
}

} } // namespace kcenon::monitoring