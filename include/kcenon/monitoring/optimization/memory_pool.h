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
#include <memory>
#include <mutex>
#include <new>
#include <vector>

#include "kcenon/monitoring/core/result_types.h"

namespace kcenon::monitoring {

/**
 * @brief Configuration for memory pool
 */
struct memory_pool_config {
    size_t initial_blocks = 256;       ///< Initial number of blocks
    size_t max_blocks = 4096;          ///< Maximum number of blocks (0 = unlimited)
    size_t block_size = 64;            ///< Size of each block in bytes
    size_t alignment = 8;              ///< Memory alignment (must be power of 2)
    bool use_thread_local_cache = false; ///< Use thread-local caching

    /**
     * @brief Validate configuration
     * @return true if configuration is valid
     */
    bool validate() const {
        if (initial_blocks == 0) {
            return false;
        }
        if (max_blocks != 0 && max_blocks < initial_blocks) {
            return false;
        }
        if (block_size == 0) {
            return false;
        }
        // Block size must be 8-byte aligned
        if (block_size % 8 != 0) {
            return false;
        }
        // Alignment must be power of 2
        if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
            return false;
        }
        return true;
    }
};

/**
 * @brief Statistics for memory pool operations
 */
struct memory_pool_statistics {
    std::atomic<size_t> total_allocations{0};
    std::atomic<size_t> total_deallocations{0};
    std::atomic<size_t> allocation_failures{0};
    std::atomic<size_t> peak_usage{0};

    memory_pool_statistics() = default;
    memory_pool_statistics(const memory_pool_statistics& other)
        : total_allocations(other.total_allocations.load())
        , total_deallocations(other.total_deallocations.load())
        , allocation_failures(other.allocation_failures.load())
        , peak_usage(other.peak_usage.load()) {}

    memory_pool_statistics& operator=(const memory_pool_statistics& other) {
        if (this != &other) {
            total_allocations.store(other.total_allocations.load());
            total_deallocations.store(other.total_deallocations.load());
            allocation_failures.store(other.allocation_failures.load());
            peak_usage.store(other.peak_usage.load());
        }
        return *this;
    }

    memory_pool_statistics(memory_pool_statistics&& other) noexcept
        : total_allocations(other.total_allocations.load())
        , total_deallocations(other.total_deallocations.load())
        , allocation_failures(other.allocation_failures.load())
        , peak_usage(other.peak_usage.load()) {}

    memory_pool_statistics& operator=(memory_pool_statistics&& other) noexcept {
        if (this != &other) {
            total_allocations.store(other.total_allocations.load());
            total_deallocations.store(other.total_deallocations.load());
            allocation_failures.store(other.allocation_failures.load());
            peak_usage.store(other.peak_usage.load());
        }
        return *this;
    }

    /**
     * @brief Get allocation success rate
     * @return Success rate between 0.0 and 100.0
     */
    double get_allocation_success_rate() const {
        auto total = total_allocations.load() + allocation_failures.load();
        if (total == 0) {
            return 100.0;
        }
        return (static_cast<double>(total_allocations.load()) / static_cast<double>(total)) * 100.0;
    }

    /**
     * @brief Reset all statistics
     */
    void reset() {
        total_allocations.store(0);
        total_deallocations.store(0);
        allocation_failures.store(0);
        peak_usage.store(0);
    }
};

/**
 * @brief Thread-safe fixed-size block memory allocator
 *
 * This pool pre-allocates memory blocks of fixed size for efficient
 * allocation/deallocation without heap fragmentation.
 */
class memory_pool {
public:
    /**
     * @brief Default constructor with default configuration
     */
    memory_pool() : memory_pool(memory_pool_config{}) {}

    /**
     * @brief Construct with configuration
     * @param config Pool configuration
     */
    explicit memory_pool(const memory_pool_config& config)
        : config_(config)
        , block_size_(config.block_size)
        , total_blocks_(config.initial_blocks) {
        initialize_pool();
    }

    ~memory_pool() {
        for (auto* chunk : memory_chunks_) {
            std::free(chunk);
        }
    }

    // Disable copy
    memory_pool(const memory_pool&) = delete;
    memory_pool& operator=(const memory_pool&) = delete;

    // Enable move
    memory_pool(memory_pool&& other) noexcept
        : config_(other.config_)
        , block_size_(other.block_size_)
        , total_blocks_(other.total_blocks_)
        , free_blocks_(std::move(other.free_blocks_))
        , memory_chunks_(std::move(other.memory_chunks_))
        , stats_(std::move(other.stats_)) {
        other.total_blocks_ = 0;
    }

    memory_pool& operator=(memory_pool&& other) noexcept {
        if (this != &other) {
            for (auto* chunk : memory_chunks_) {
                std::free(chunk);
            }
            config_ = other.config_;
            block_size_ = other.block_size_;
            total_blocks_ = other.total_blocks_;
            free_blocks_ = std::move(other.free_blocks_);
            memory_chunks_ = std::move(other.memory_chunks_);
            stats_ = std::move(other.stats_);
            other.total_blocks_ = 0;
        }
        return *this;
    }

    /**
     * @brief Allocate a memory block
     * @return result<void*> containing pointer to allocated block
     */
    result<void*> allocate() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (free_blocks_.empty()) {
            // Try to grow the pool
            if (!grow_pool()) {
                stats_.allocation_failures++;
                return make_error<void*>(monitoring_error_code::resource_unavailable,
                                         "Memory pool exhausted");
            }
        }

        void* block = free_blocks_.back();
        free_blocks_.pop_back();

        stats_.total_allocations++;
        update_peak_usage();

        return common::ok(block);
    }

    /**
     * @brief Deallocate a memory block
     * @param ptr Pointer to the block to deallocate
     * @return result_void indicating success or error
     */
    result_void deallocate(void* ptr) {
        if (ptr == nullptr) {
            return make_void_error(monitoring_error_code::invalid_argument,
                                   "Cannot deallocate null pointer");
        }

        std::lock_guard<std::mutex> lock(mutex_);

        // Verify the pointer belongs to this pool
        if (!is_owned_block(ptr)) {
            return make_void_error(monitoring_error_code::invalid_argument,
                                   "Pointer does not belong to this pool");
        }

        free_blocks_.push_back(ptr);
        stats_.total_deallocations++;

        return make_void_success();
    }

    /**
     * @brief Allocate and construct an object
     * @tparam T The object type
     * @tparam Args Constructor argument types
     * @param args Constructor arguments
     * @return result<T*> containing pointer to constructed object
     */
    template<typename T, typename... Args>
    result<T*> allocate_object(Args&&... args) {
        if (sizeof(T) > block_size_) {
            return make_error<T*>(monitoring_error_code::invalid_argument,
                                  "Object size exceeds block size");
        }

        auto result = allocate();
        if (result.is_err()) {
            return make_error<T*>(monitoring_error_code::resource_unavailable,
                                  "Failed to allocate memory for object");
        }

        void* ptr = result.value();
        T* obj = new (ptr) T(std::forward<Args>(args)...);

        return common::ok(obj);
    }

    /**
     * @brief Destroy and deallocate an object
     * @tparam T The object type
     * @param obj Pointer to the object
     * @return result_void indicating success or error
     */
    template<typename T>
    result_void deallocate_object(T* obj) {
        if (obj == nullptr) {
            return make_void_error(monitoring_error_code::invalid_argument,
                                   "Cannot deallocate null object");
        }

        obj->~T();
        return deallocate(static_cast<void*>(obj));
    }

    /**
     * @brief Get number of available blocks
     * @return Number of free blocks
     */
    size_t available_blocks() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return free_blocks_.size();
    }

    /**
     * @brief Get total number of blocks
     * @return Total block count
     */
    size_t total_blocks() const {
        return total_blocks_;
    }

    /**
     * @brief Get block size
     * @return Size of each block in bytes
     */
    size_t block_size() const {
        return block_size_;
    }

    /**
     * @brief Get pool statistics
     * @return Reference to statistics
     */
    const memory_pool_statistics& get_statistics() const {
        return stats_;
    }

    /**
     * @brief Reset statistics
     */
    void reset_statistics() {
        stats_.reset();
    }

private:
    void initialize_pool() {
        size_t chunk_size = total_blocks_ * block_size_;
        void* chunk = std::aligned_alloc(config_.alignment, chunk_size);

        if (chunk == nullptr) {
            throw std::bad_alloc();
        }

        memory_chunks_.push_back(chunk);

        // Initialize free block list
        free_blocks_.reserve(total_blocks_);
        char* ptr = static_cast<char*>(chunk);
        for (size_t i = 0; i < total_blocks_; ++i) {
            free_blocks_.push_back(ptr + i * block_size_);
        }
    }

    bool grow_pool() {
        if (config_.max_blocks > 0 && total_blocks_ >= config_.max_blocks) {
            return false;
        }

        size_t new_blocks = std::min(total_blocks_, config_.max_blocks - total_blocks_);
        if (new_blocks == 0) {
            new_blocks = total_blocks_;  // Double the size
        }

        size_t chunk_size = new_blocks * block_size_;
        void* chunk = std::aligned_alloc(config_.alignment, chunk_size);

        if (chunk == nullptr) {
            return false;
        }

        memory_chunks_.push_back(chunk);

        char* ptr = static_cast<char*>(chunk);
        for (size_t i = 0; i < new_blocks; ++i) {
            free_blocks_.push_back(ptr + i * block_size_);
        }

        total_blocks_ += new_blocks;
        return true;
    }

    bool is_owned_block(void* ptr) const {
        for (size_t i = 0; i < memory_chunks_.size(); ++i) {
            char* chunk_start = static_cast<char*>(memory_chunks_[i]);
            size_t chunk_blocks = (i == 0) ? config_.initial_blocks :
                                  ((total_blocks_ - config_.initial_blocks) / (memory_chunks_.size() - 1));
            char* chunk_end = chunk_start + chunk_blocks * block_size_;

            if (ptr >= chunk_start && ptr < chunk_end) {
                // Verify alignment
                size_t offset = static_cast<char*>(ptr) - chunk_start;
                if (offset % block_size_ == 0) {
                    return true;
                }
            }
        }
        return false;
    }

    void update_peak_usage() {
        size_t current_usage = total_blocks_ - free_blocks_.size();
        size_t peak = stats_.peak_usage.load();
        while (current_usage > peak) {
            if (stats_.peak_usage.compare_exchange_weak(peak, current_usage)) {
                break;
            }
        }
    }

    memory_pool_config config_;
    size_t block_size_;
    size_t total_blocks_;
    std::vector<void*> free_blocks_;
    std::vector<void*> memory_chunks_;
    mutable std::mutex mutex_;
    mutable memory_pool_statistics stats_;
};

/**
 * @brief Create a memory pool with default configuration
 * @return Unique pointer to the pool
 */
inline std::unique_ptr<memory_pool> make_memory_pool() {
    return std::make_unique<memory_pool>();
}

/**
 * @brief Create a memory pool with configuration
 * @param config Pool configuration
 * @return Unique pointer to the pool
 */
inline std::unique_ptr<memory_pool> make_memory_pool(const memory_pool_config& config) {
    return std::make_unique<memory_pool>(config);
}

/**
 * @brief Create default pool configurations for different use cases
 * @return Vector of configurations
 */
inline std::vector<memory_pool_config> create_default_pool_configs() {
    return {
        // Small objects pool
        {.initial_blocks = 512, .max_blocks = 2048, .block_size = 32, .alignment = 8},
        // Medium objects pool
        {.initial_blocks = 256, .max_blocks = 1024, .block_size = 128, .alignment = 16},
        // Large objects pool
        {.initial_blocks = 64, .max_blocks = 256, .block_size = 512, .alignment = 32},
        // Thread-local cache enabled pool
        {.initial_blocks = 256, .max_blocks = 1024, .block_size = 64, .alignment = 8, .use_thread_local_cache = true}
    };
}

} // namespace kcenon::monitoring
