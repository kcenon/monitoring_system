#pragma once

/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, monitoring_system contributors
All rights reserved.
*****************************************************************************/

/**
 * @file lockfree_queue.h
 * @brief Lock-free queue implementation for high-performance metric collection
 * 
 * This file implements P4 task: Lock-free data structures integration
 * for optimizing metric collection and processing with minimal contention.
 */

#include "../core/result_types.h"
#include "../core/error_codes.h"
#include <atomic>
#include <memory>
#include <type_traits>
#include <chrono>
#include <thread>
#include <vector>

namespace monitoring_system {

/**
 * @struct lockfree_queue_config
 * @brief Configuration for lock-free queue
 */
struct lockfree_queue_config {
    size_t initial_capacity = 1024;           // Initial queue capacity
    size_t max_capacity = 65536;              // Maximum capacity to prevent unbounded growth
    bool allow_expansion = true;              // Allow dynamic expansion
    size_t expansion_factor = 2;              // Growth factor when expanding
    std::chrono::milliseconds retry_delay{1}; // Delay between retry attempts
    size_t max_retries = 100;                // Maximum retry attempts for operations
    
    /**
     * @brief Validate configuration
     */
    result_void validate() const {
        if (initial_capacity == 0) {
            return result_void(monitoring_error_code::invalid_configuration,
                             "Initial capacity must be positive");
        }
        
        if (max_capacity < initial_capacity) {
            return result_void(monitoring_error_code::invalid_configuration,
                             "Max capacity cannot be less than initial capacity");
        }
        
        if (expansion_factor < 2) {
            return result_void(monitoring_error_code::invalid_configuration,
                             "Expansion factor must be at least 2");
        }
        
        return result_void::success();
    }
};

/**
 * @struct lockfree_queue_stats
 * @brief Statistics for lock-free queue performance
 */
struct lockfree_queue_stats {
    std::atomic<size_t> total_pushes{0};
    std::atomic<size_t> total_pops{0};
    std::atomic<size_t> failed_pushes{0};
    std::atomic<size_t> failed_pops{0};
    std::atomic<size_t> retry_count{0};
    std::atomic<size_t> expansion_count{0};
    std::atomic<size_t> current_capacity{0};
    std::atomic<size_t> peak_size{0};
    
    std::chrono::system_clock::time_point creation_time;
    
    lockfree_queue_stats() : creation_time(std::chrono::system_clock::now()) {}
    
    /**
     * @brief Get push success rate
     */
    double get_push_success_rate() const {
        auto total = total_pushes.load();
        auto failed = failed_pushes.load();
        return total > 0 ? (1.0 - static_cast<double>(failed) / total) * 100.0 : 100.0;
    }
    
    /**
     * @brief Get pop success rate
     */
    double get_pop_success_rate() const {
        auto total = total_pops.load();
        auto failed = failed_pops.load();
        return total > 0 ? (1.0 - static_cast<double>(failed) / total) * 100.0 : 100.0;
    }
    
    /**
     * @brief Get average retries per operation
     */
    double get_avg_retries() const {
        auto operations = total_pushes.load() + total_pops.load();
        auto retries = retry_count.load();
        return operations > 0 ? static_cast<double>(retries) / operations : 0.0;
    }
};

/**
 * @class lockfree_queue_node
 * @brief Lock-free queue node with atomic next pointer
 */
template<typename T>
struct lockfree_queue_node {
    std::atomic<lockfree_queue_node*> next{nullptr};
    T data;
    
    template<typename... Args>
    lockfree_queue_node(Args&&... args) : data(std::forward<Args>(args)...) {}
};

/**
 * @class lockfree_queue
 * @brief Lock-free queue implementation using Michael & Scott algorithm
 * @tparam T The type of elements to store
 * 
 * This implementation provides a thread-safe, lock-free queue suitable
 * for high-performance metric collection scenarios.
 */
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4324) // structure was padded due to alignment specifier
#endif

template<typename T>
class lockfree_queue {
private:
    using node_type = lockfree_queue_node<T>;
    using node_ptr = std::atomic<node_type*>;
    
    alignas(64) node_ptr head_;  // Cache line aligned for performance
    alignas(64) node_ptr tail_;  // Cache line aligned for performance
    
    lockfree_queue_config config_;
    mutable lockfree_queue_stats stats_;
    std::atomic<size_t> size_{0};
    
    /**
     * @brief Allocate a new node
     */
    template<typename... Args>
    node_type* allocate_node(Args&&... args) {
        try {
            return new node_type(std::forward<Args>(args)...);
        } catch (const std::bad_alloc&) {
            return nullptr;
        }
    }
    
    /**
     * @brief Deallocate a node
     */
    void deallocate_node(node_type* node) {
        delete node;
    }
    
    /**
     * @brief Check if queue has reached capacity limits
     */
    bool is_capacity_exceeded() const {
        if (!config_.allow_expansion) {
            return size_.load(std::memory_order_relaxed) >= config_.max_capacity;
        }
        return false;
    }
    
    /**
     * @brief Update peak size statistics
     */
    void update_peak_size() const {
        auto current = size_.load(std::memory_order_relaxed);
        auto peak = stats_.peak_size.load(std::memory_order_relaxed);
        
        while (current > peak) {
            if (stats_.peak_size.compare_exchange_weak(peak, current, std::memory_order_relaxed)) {
                break;
            }
        }
    }

public:
    /**
     * @brief Constructor
     */
    explicit lockfree_queue(const lockfree_queue_config& config = {})
        : config_(config) {
        
        auto validation = config_.validate();
        if (!validation) {
            throw std::invalid_argument("Invalid lockfree queue configuration: " + 
                                      validation.get_error().message);
        }
        
        // Initialize with dummy node
        auto dummy = allocate_node();
        if (!dummy) {
            throw std::bad_alloc();
        }
        
        head_.store(dummy, std::memory_order_relaxed);
        tail_.store(dummy, std::memory_order_relaxed);
        stats_.current_capacity.store(config_.initial_capacity, std::memory_order_relaxed);
    }
    
    /**
     * @brief Destructor
     */
    ~lockfree_queue() {
        // Clean up remaining nodes
        while (auto old_head = head_.load(std::memory_order_relaxed)) {
            head_.store(old_head->next.load(std::memory_order_relaxed), std::memory_order_relaxed);
            deallocate_node(old_head);
        }
    }
    
    // Non-copyable and non-moveable
    lockfree_queue(const lockfree_queue&) = delete;
    lockfree_queue& operator=(const lockfree_queue&) = delete;
    lockfree_queue(lockfree_queue&&) = delete;
    lockfree_queue& operator=(lockfree_queue&&) = delete;
    
    /**
     * @brief Push element to queue
     */
    template<typename U = T>
    result_void push(U&& item) {
        if (is_capacity_exceeded()) {
            stats_.failed_pushes.fetch_add(1, std::memory_order_relaxed);
            return result_void(monitoring_error_code::storage_full,
                             "Queue capacity exceeded");
        }
        
        auto new_node = allocate_node(std::forward<U>(item));
        if (!new_node) {
            stats_.failed_pushes.fetch_add(1, std::memory_order_relaxed);
            return result_void(monitoring_error_code::memory_allocation_failed,
                             "Failed to allocate node");
        }
        
        size_t retries = 0;
        while (retries < config_.max_retries) {
            auto tail = tail_.load(std::memory_order_acquire);
            auto next = tail->next.load(std::memory_order_acquire);
            
            // Check if tail is still the last node
            if (tail == tail_.load(std::memory_order_acquire)) {
                if (next == nullptr) {
                    // Try to link the new node at the end of the queue
                    if (tail->next.compare_exchange_weak(next, new_node, 
                                                       std::memory_order_release,
                                                       std::memory_order_relaxed)) {
                        // Successfully linked, now update tail
                        tail_.compare_exchange_weak(tail, new_node, 
                                                  std::memory_order_release,
                                                  std::memory_order_relaxed);
                        
                        size_.fetch_add(1, std::memory_order_relaxed);
                        stats_.total_pushes.fetch_add(1, std::memory_order_relaxed);
                        stats_.retry_count.fetch_add(retries, std::memory_order_relaxed);
                        update_peak_size();
                        
                        return result_void::success();
                    }
                } else {
                    // Help advance tail
                    tail_.compare_exchange_weak(tail, next, 
                                              std::memory_order_release,
                                              std::memory_order_relaxed);
                }
            }
            
            ++retries;
            if (config_.retry_delay.count() > 0) {
                std::this_thread::sleep_for(config_.retry_delay);
            }
        }
        
        // Failed after max retries
        deallocate_node(new_node);
        stats_.failed_pushes.fetch_add(1, std::memory_order_relaxed);
        stats_.retry_count.fetch_add(retries, std::memory_order_relaxed);
        
        return result_void(monitoring_error_code::operation_timeout,
                         "Push operation exceeded max retries");
    }
    
    /**
     * @brief Pop element from queue
     */
    result<T> pop() {
        size_t retries = 0;
        
        while (retries < config_.max_retries) {
            auto head = head_.load(std::memory_order_acquire);
            auto tail = tail_.load(std::memory_order_acquire);
            auto next = head->next.load(std::memory_order_acquire);
            
            // Check consistency
            if (head == head_.load(std::memory_order_acquire)) {
                if (head == tail) {
                    if (next == nullptr) {
                        // Queue is empty
                        stats_.failed_pops.fetch_add(1, std::memory_order_relaxed);
                        return make_error<T>(monitoring_error_code::storage_empty,
                                           "Queue is empty");
                    }
                    
                    // Help advance tail
                    tail_.compare_exchange_weak(tail, next,
                                              std::memory_order_release,
                                              std::memory_order_relaxed);
                } else {
                    if (next == nullptr) {
                        // Inconsistent state, retry
                        ++retries;
                        continue;
                    }
                    
                    // Read data before advancing head
                    T data = std::move(next->data);
                    
                    // Try to advance head
                    if (head_.compare_exchange_weak(head, next,
                                                  std::memory_order_release,
                                                  std::memory_order_relaxed)) {
                        
                        deallocate_node(head);
                        size_.fetch_sub(1, std::memory_order_relaxed);
                        stats_.total_pops.fetch_add(1, std::memory_order_relaxed);
                        stats_.retry_count.fetch_add(retries, std::memory_order_relaxed);
                        
                        return make_success(std::move(data));
                    }
                }
            }
            
            ++retries;
            if (config_.retry_delay.count() > 0) {
                std::this_thread::sleep_for(config_.retry_delay);
            }
        }
        
        // Failed after max retries
        stats_.failed_pops.fetch_add(1, std::memory_order_relaxed);
        stats_.retry_count.fetch_add(retries, std::memory_order_relaxed);
        
        return make_error<T>(monitoring_error_code::operation_timeout,
                           "Pop operation exceeded max retries");
    }
    
    /**
     * @brief Check if queue is empty
     */
    bool empty() const {
        auto head = head_.load(std::memory_order_acquire);
        auto tail = tail_.load(std::memory_order_acquire);
        auto next = head->next.load(std::memory_order_acquire);
        
        return (head == tail) && (next == nullptr);
    }
    
    /**
     * @brief Get approximate queue size
     */
    size_t size() const {
        return size_.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief Get queue capacity
     */
    size_t capacity() const {
        return stats_.current_capacity.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief Get queue statistics
     */
    const lockfree_queue_stats& get_statistics() const {
        return stats_;
    }
    
    /**
     * @brief Get configuration
     */
    const lockfree_queue_config& get_config() const {
        return config_;
    }
    
    /**
     * @brief Clear queue (not thread-safe, use only when no concurrent access)
     */
    void clear() {
        T item;
        while (auto result = pop()) {
            // Discard items
        }
    }
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

/**
 * @brief Factory function to create lock-free queue
 */
template<typename T>
std::unique_ptr<lockfree_queue<T>> make_lockfree_queue(
    const lockfree_queue_config& config = {}) {
    return std::make_unique<lockfree_queue<T>>(config);
}

/**
 * @brief Create default queue configurations for common scenarios
 */
inline std::vector<lockfree_queue_config> create_default_queue_configs() {
    std::vector<lockfree_queue_config> configs;
    
    // High-throughput configuration
    lockfree_queue_config high_throughput;
    high_throughput.initial_capacity = 4096;
    high_throughput.max_capacity = 131072;  // 128K
    high_throughput.allow_expansion = true;
    high_throughput.max_retries = 1000;
    configs.push_back(high_throughput);
    
    // Low-latency configuration
    lockfree_queue_config low_latency;
    low_latency.initial_capacity = 512;
    low_latency.max_capacity = 2048;
    low_latency.allow_expansion = false;
    low_latency.max_retries = 10;
    low_latency.retry_delay = std::chrono::milliseconds(0);
    configs.push_back(low_latency);
    
    // Memory-constrained configuration
    lockfree_queue_config memory_constrained;
    memory_constrained.initial_capacity = 256;
    memory_constrained.max_capacity = 1024;
    memory_constrained.allow_expansion = false;
    memory_constrained.max_retries = 50;
    configs.push_back(memory_constrained);
    
    return configs;
}

} // namespace monitoring_system