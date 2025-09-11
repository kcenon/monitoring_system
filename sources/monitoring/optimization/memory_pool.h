#pragma once

/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, monitoring_system contributors
All rights reserved.
*****************************************************************************/

/**
 * @file memory_pool.h
 * @brief Zero-copy memory pool for efficient memory management
 * 
 * This file implements P4 task: Zero-copy memory pool optimizations
 * for reducing allocation overhead in high-frequency metric operations.
 */

#include "../core/result_types.h"
#include "../core/error_codes.h"
#include <atomic>
#include <memory>
#include <vector>
#include <mutex>
#include <chrono>
#include <cstddef>
#include <type_traits>

namespace monitoring_system {

/**
 * @struct memory_pool_config
 * @brief Configuration for memory pool
 */
struct memory_pool_config {
    size_t initial_blocks = 1024;            // Initial number of blocks
    size_t max_blocks = 65536;               // Maximum number of blocks
    size_t block_size = 64;                  // Size of each block in bytes
    bool allow_expansion = true;             // Allow pool expansion
    size_t expansion_factor = 2;             // Growth factor when expanding
    bool use_thread_local_cache = true;      // Use thread-local caching
    size_t thread_cache_size = 64;           // Thread-local cache size
    std::chrono::milliseconds gc_interval{5000}; // Garbage collection interval
    
    /**
     * @brief Validate configuration
     */
    result_void validate() const {
        if (initial_blocks == 0) {
            return result_void(monitoring_error_code::invalid_configuration,
                             "Initial blocks must be positive");
        }
        
        if (max_blocks < initial_blocks) {
            return result_void(monitoring_error_code::invalid_configuration,
                             "Max blocks cannot be less than initial blocks");
        }
        
        if (block_size == 0 || block_size % 8 != 0) {
            return result_void(monitoring_error_code::invalid_configuration,
                             "Block size must be positive and 8-byte aligned");
        }
        
        if (expansion_factor < 2) {
            return result_void(monitoring_error_code::invalid_configuration,
                             "Expansion factor must be at least 2");
        }
        
        return result_void::success();
    }
};

/**
 * @struct memory_pool_stats
 * @brief Statistics for memory pool performance
 */
struct memory_pool_stats {
    std::atomic<size_t> total_allocations{0};
    std::atomic<size_t> total_deallocations{0};
    std::atomic<size_t> failed_allocations{0};
    std::atomic<size_t> cache_hits{0};
    std::atomic<size_t> cache_misses{0};
    std::atomic<size_t> expansions{0};
    std::atomic<size_t> gc_cycles{0};
    std::atomic<size_t> current_blocks{0};
    std::atomic<size_t> free_blocks{0};
    std::atomic<size_t> peak_usage{0};
    
    std::chrono::system_clock::time_point creation_time;
    
    memory_pool_stats() : creation_time(std::chrono::system_clock::now()) {}
    
    /**
     * @brief Get allocation success rate
     */
    double get_allocation_success_rate() const {
        auto total = total_allocations.load();
        auto failed = failed_allocations.load();
        return total > 0 ? (1.0 - static_cast<double>(failed) / total) * 100.0 : 100.0;
    }
    
    /**
     * @brief Get cache hit rate
     */
    double get_cache_hit_rate() const {
        auto hits = cache_hits.load();
        auto misses = cache_misses.load();
        auto total = hits + misses;
        return total > 0 ? (static_cast<double>(hits) / total) * 100.0 : 0.0;
    }
    
    /**
     * @brief Get memory utilization
     */
    double get_utilization() const {
        auto total = current_blocks.load();
        auto free = free_blocks.load();
        return total > 0 ? (1.0 - static_cast<double>(free) / total) * 100.0 : 0.0;
    }
};

/**
 * @struct memory_block
 * @brief Memory block with metadata
 */
struct memory_block {
    std::atomic<memory_block*> next{nullptr};
    bool is_free{true};
    size_t size{0};
    std::chrono::system_clock::time_point last_used;
    
    memory_block() : last_used(std::chrono::system_clock::now()) {}
    
    /**
     * @brief Get data pointer (aligned)
     */
    void* data() {
        return reinterpret_cast<char*>(this) + sizeof(memory_block);
    }
    
    /**
     * @brief Get const data pointer
     */
    const void* data() const {
        return reinterpret_cast<const char*>(this) + sizeof(memory_block);
    }
};

/**
 * @class thread_local_cache
 * @brief Thread-local memory cache for faster allocations
 */
class thread_local_cache {
private:
    std::vector<memory_block*> free_blocks_;
    size_t max_size_;
    
public:
    explicit thread_local_cache(size_t max_size) : max_size_(max_size) {
        free_blocks_.reserve(max_size);
    }
    
    ~thread_local_cache() {
        // Note: Blocks are owned by the main pool, so we don't delete them here
        free_blocks_.clear();
    }
    
    /**
     * @brief Get block from cache
     */
    memory_block* get_block() {
        if (free_blocks_.empty()) {
            return nullptr;
        }
        
        auto block = free_blocks_.back();
        free_blocks_.pop_back();
        return block;
    }
    
    /**
     * @brief Return block to cache
     */
    bool return_block(memory_block* block) {
        if (free_blocks_.size() >= max_size_) {
            return false;  // Cache full
        }
        
        block->is_free = true;
        block->last_used = std::chrono::system_clock::now();
        free_blocks_.push_back(block);
        return true;
    }
    
    /**
     * @brief Check if cache is full
     */
    bool is_full() const {
        return free_blocks_.size() >= max_size_;
    }
    
    /**
     * @brief Get cache size
     */
    size_t size() const {
        return free_blocks_.size();
    }
    
    /**
     * @brief Clear cache
     */
    void clear() {
        free_blocks_.clear();
    }
};

/**
 * @class memory_pool
 * @brief Zero-copy memory pool with thread-local caching
 */
class memory_pool {
private:
    memory_pool_config config_;
    mutable memory_pool_stats stats_;
    
    // Memory management
    std::vector<std::unique_ptr<char[]>> memory_chunks_;
    std::atomic<memory_block*> free_list_head_{nullptr};
    
    // Thread-local caching
    thread_local static std::unique_ptr<thread_local_cache> tl_cache_;
    
    // Synchronization
    mutable std::mutex expansion_mutex_;
    
    /**
     * @brief Allocate new memory chunk
     */
    result_void allocate_chunk(size_t num_blocks) {
        size_t chunk_size = num_blocks * (sizeof(memory_block) + config_.block_size);
        
        try {
            auto chunk = std::make_unique<char[]>(chunk_size);
            char* ptr = chunk.get();
            
            // Initialize blocks in the chunk
            for (size_t i = 0; i < num_blocks; ++i) {
                auto block = reinterpret_cast<memory_block*>(ptr);
                new(block) memory_block();
                block->size = config_.block_size;
                
                // Link to free list
                auto old_head = free_list_head_.load(std::memory_order_relaxed);
                do {
                    block->next.store(old_head, std::memory_order_relaxed);
                } while (!free_list_head_.compare_exchange_weak(old_head, block,
                                                               std::memory_order_release,
                                                               std::memory_order_relaxed));
                
                ptr += sizeof(memory_block) + config_.block_size;
            }
            
            memory_chunks_.push_back(std::move(chunk));
            stats_.current_blocks.fetch_add(num_blocks, std::memory_order_relaxed);
            stats_.free_blocks.fetch_add(num_blocks, std::memory_order_relaxed);
            
            return result_void::success();
            
        } catch (const std::bad_alloc&) {
            return result_void(monitoring_error_code::memory_allocation_failed,
                             "Failed to allocate memory chunk");
        }
    }
    
    /**
     * @brief Get thread-local cache
     */
    thread_local_cache* get_thread_cache() {
        if (!config_.use_thread_local_cache) {
            return nullptr;
        }
        
        if (!tl_cache_) {
            tl_cache_ = std::make_unique<thread_local_cache>(config_.thread_cache_size);
        }
        
        return tl_cache_.get();
    }
    
    /**
     * @brief Allocate block from free list
     */
    memory_block* allocate_from_free_list() {
        auto head = free_list_head_.load(std::memory_order_acquire);
        
        while (head != nullptr) {
            auto next = head->next.load(std::memory_order_relaxed);
            
            if (free_list_head_.compare_exchange_weak(head, next,
                                                     std::memory_order_release,
                                                     std::memory_order_relaxed)) {
                head->is_free = false;
                head->last_used = std::chrono::system_clock::now();
                stats_.free_blocks.fetch_sub(1, std::memory_order_relaxed);
                return head;
            }
        }
        
        return nullptr;
    }
    
    /**
     * @brief Return block to free list
     */
    void return_to_free_list(memory_block* block) {
        block->is_free = true;
        block->last_used = std::chrono::system_clock::now();
        
        auto old_head = free_list_head_.load(std::memory_order_relaxed);
        do {
            block->next.store(old_head, std::memory_order_relaxed);
        } while (!free_list_head_.compare_exchange_weak(old_head, block,
                                                       std::memory_order_release,
                                                       std::memory_order_relaxed));
        
        stats_.free_blocks.fetch_add(1, std::memory_order_relaxed);
    }
    
    /**
     * @brief Expand pool if allowed
     */
    result_void expand_pool() {
        if (!config_.allow_expansion) {
            return result_void(monitoring_error_code::storage_full,
                             "Pool expansion not allowed");
        }
        
        std::lock_guard<std::mutex> lock(expansion_mutex_);
        
        auto current_blocks = stats_.current_blocks.load(std::memory_order_relaxed);
        if (current_blocks >= config_.max_blocks) {
            return result_void(monitoring_error_code::storage_full,
                             "Pool has reached maximum capacity");
        }
        
        size_t new_blocks = std::min(current_blocks * (config_.expansion_factor - 1),
                                   config_.max_blocks - current_blocks);
        
        auto result = allocate_chunk(new_blocks);
        if (result) {
            stats_.expansions.fetch_add(1, std::memory_order_relaxed);
        }
        
        return result;
    }

public:
    /**
     * @brief Constructor
     */
    explicit memory_pool(const memory_pool_config& config = {})
        : config_(config) {
        
        auto validation = config_.validate();
        if (!validation) {
            throw std::invalid_argument("Invalid memory pool configuration: " + 
                                      validation.get_error().message);
        }
        
        // Allocate initial blocks
        auto result = allocate_chunk(config_.initial_blocks);
        if (!result) {
            throw std::runtime_error("Failed to initialize memory pool: " + 
                                   result.get_error().message);
        }
    }
    
    /**
     * @brief Destructor
     */
    ~memory_pool() {
        // Memory chunks will be automatically freed by unique_ptr
    }
    
    // Non-copyable and non-moveable
    memory_pool(const memory_pool&) = delete;
    memory_pool& operator=(const memory_pool&) = delete;
    memory_pool(memory_pool&&) = delete;
    memory_pool& operator=(memory_pool&&) = delete;
    
    /**
     * @brief Allocate memory block
     */
    result<void*> allocate() {
        memory_block* block = nullptr;
        
        // Try thread-local cache first
        if (auto cache = get_thread_cache()) {
            block = cache->get_block();
            if (block) {
                stats_.cache_hits.fetch_add(1, std::memory_order_relaxed);
            } else {
                stats_.cache_misses.fetch_add(1, std::memory_order_relaxed);
            }
        }
        
        // Try free list if cache miss
        if (!block) {
            block = allocate_from_free_list();
        }
        
        // Expand pool if necessary
        if (!block) {
            auto expand_result = expand_pool();
            if (expand_result) {
                block = allocate_from_free_list();
            }
        }
        
        if (!block) {
            stats_.failed_allocations.fetch_add(1, std::memory_order_relaxed);
            return make_error<void*>(monitoring_error_code::memory_allocation_failed,
                                   "No free blocks available");
        }
        
        stats_.total_allocations.fetch_add(1, std::memory_order_relaxed);
        
        // Update peak usage
        auto used_blocks = stats_.current_blocks.load() - stats_.free_blocks.load();
        auto peak = stats_.peak_usage.load(std::memory_order_relaxed);
        while (used_blocks > peak) {
            if (stats_.peak_usage.compare_exchange_weak(peak, used_blocks, std::memory_order_relaxed)) {
                break;
            }
        }
        
        return make_success(block->data());
    }
    
    /**
     * @brief Deallocate memory block
     */
    result_void deallocate(void* ptr) {
        if (!ptr) {
            return result_void(monitoring_error_code::invalid_configuration,
                             "Cannot deallocate null pointer");
        }
        
        // Get block header from data pointer
        auto block = reinterpret_cast<memory_block*>(
            reinterpret_cast<char*>(ptr) - sizeof(memory_block));
        
        if (block->is_free) {
            return result_void(monitoring_error_code::invalid_configuration,
                             "Double free detected");
        }
        
        // Try to return to thread-local cache first
        if (auto cache = get_thread_cache()) {
            if (cache->return_block(block)) {
                stats_.total_deallocations.fetch_add(1, std::memory_order_relaxed);
                return result_void::success();
            }
        }
        
        // Return to global free list
        return_to_free_list(block);
        stats_.total_deallocations.fetch_add(1, std::memory_order_relaxed);
        
        return result_void::success();
    }
    
    /**
     * @brief Allocate typed object
     */
    template<typename T, typename... Args>
    result<T*> allocate_object(Args&&... args) {
        static_assert(sizeof(T) <= config_.block_size, 
                     "Object size exceeds block size");
        
        auto ptr_result = allocate();
        if (!ptr_result) {
            return make_error<T*>(ptr_result.get_error().code, ptr_result.get_error().message);
        }
        
        try {
            T* obj = new(ptr_result.value()) T(std::forward<Args>(args)...);
            return make_success(obj);
        } catch (const std::exception& e) {
            deallocate(ptr_result.value());
            return make_error<T*>(monitoring_error_code::memory_allocation_failed,
                                "Object construction failed: " + std::string(e.what()));
        }
    }
    
    /**
     * @brief Deallocate typed object
     */
    template<typename T>
    result_void deallocate_object(T* obj) {
        if (!obj) {
            return result_void(monitoring_error_code::invalid_configuration,
                             "Cannot deallocate null object");
        }
        
        obj->~T();
        return deallocate(obj);
    }
    
    /**
     * @brief Get pool statistics
     */
    const memory_pool_stats& get_statistics() const {
        return stats_;
    }
    
    /**
     * @brief Get configuration
     */
    const memory_pool_config& get_config() const {
        return config_;
    }
    
    /**
     * @brief Get available blocks
     */
    size_t available_blocks() const {
        return stats_.free_blocks.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief Get total blocks
     */
    size_t total_blocks() const {
        return stats_.current_blocks.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief Perform garbage collection (optional optimization)
     */
    void garbage_collect() {
        // This is a placeholder for more advanced GC if needed
        // Currently, the pool manages memory automatically
        stats_.gc_cycles.fetch_add(1, std::memory_order_relaxed);
    }
    
    /**
     * @brief Reset thread-local cache
     */
    void reset_thread_cache() {
        if (auto cache = get_thread_cache()) {
            cache->clear();
        }
    }
};

// Thread-local cache instance
thread_local std::unique_ptr<thread_local_cache> memory_pool::tl_cache_;

/**
 * @brief Factory function to create memory pool
 */
inline std::unique_ptr<memory_pool> make_memory_pool(
    const memory_pool_config& config = {}) {
    return std::make_unique<memory_pool>(config);
}

/**
 * @brief Create default memory pool configurations
 */
inline std::vector<memory_pool_config> create_default_pool_configs() {
    std::vector<memory_pool_config> configs;
    
    // High-performance configuration
    memory_pool_config high_performance;
    high_performance.initial_blocks = 4096;
    high_performance.max_blocks = 131072;  // 128K blocks
    high_performance.block_size = 128;     // 128 bytes per block
    high_performance.use_thread_local_cache = true;
    high_performance.thread_cache_size = 128;
    configs.push_back(high_performance);
    
    // Memory-efficient configuration
    memory_pool_config memory_efficient;
    memory_efficient.initial_blocks = 512;
    memory_efficient.max_blocks = 4096;
    memory_efficient.block_size = 64;
    memory_efficient.use_thread_local_cache = false;
    configs.push_back(memory_efficient);
    
    // Large object configuration
    memory_pool_config large_objects;
    large_objects.initial_blocks = 256;
    large_objects.max_blocks = 2048;
    large_objects.block_size = 512;  // 512 bytes per block
    large_objects.use_thread_local_cache = true;
    large_objects.thread_cache_size = 32;
    configs.push_back(large_objects);
    
    return configs;
}

/**
 * @class pool_allocator
 * @brief STL-compatible allocator using memory pool
 */
template<typename T>
class pool_allocator {
private:
    memory_pool* pool_;
    
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    
    template<typename U>
    struct rebind {
        using other = pool_allocator<U>;
    };
    
    explicit pool_allocator(memory_pool* pool) : pool_(pool) {}
    
    template<typename U>
    pool_allocator(const pool_allocator<U>& other) : pool_(other.pool_) {}
    
    pointer allocate(size_type n) {
        if (n > 1 || sizeof(T) > pool_->get_config().block_size) {
            throw std::bad_alloc();
        }
        
        auto result = pool_->template allocate_object<T>();
        if (!result) {
            throw std::bad_alloc();
        }
        
        return result.value();
    }
    
    void deallocate(pointer p, size_type) {
        pool_->deallocate_object(p);
    }
    
    template<typename U, typename... Args>
    void construct(U* p, Args&&... args) {
        new(p) U(std::forward<Args>(args)...);
    }
    
    template<typename U>
    void destroy(U* p) {
        p->~U();
    }
    
    template<typename U>
    bool operator==(const pool_allocator<U>& other) const {
        return pool_ == other.pool_;
    }
    
    template<typename U>
    bool operator!=(const pool_allocator<U>& other) const {
        return !(*this == other);
    }
};

} // namespace monitoring_system