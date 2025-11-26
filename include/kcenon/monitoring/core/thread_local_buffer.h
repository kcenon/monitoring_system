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

#include <vector>
#include <string>
#include <chrono>
#include <memory>
#include <functional>

namespace kcenon { namespace monitoring {

// Forward declaration
class central_collector;

/**
 * @brief Sample data structure for metric recording
 */
struct metric_sample {
    std::string operation_name;
    std::chrono::nanoseconds duration;
    bool success;
    std::chrono::steady_clock::time_point timestamp;

    metric_sample() = default;
    metric_sample(const std::string& name,
                  std::chrono::nanoseconds dur,
                  bool succ)
        : operation_name(name)
        , duration(dur)
        , success(succ)
        , timestamp(std::chrono::steady_clock::now())
    {}
};

/**
 * @brief Thread-local buffer for lock-free metric collection
 *
 * Each thread maintains its own buffer for recording metrics without locks.
 * When the buffer fills up, it flushes to a central collector.
 *
 * Optimized with pre-allocated ring buffer to eliminate runtime allocations.
 *
 * @thread_safety NOT thread-safe across threads (thread-local use only).
 *                Thread-safe within a single thread (no concurrent access).
 */
class thread_local_buffer {
public:
    static constexpr size_t DEFAULT_CAPACITY = 256;

    /**
     * @brief Construct a thread-local buffer
     * @param capacity Maximum number of samples before flush
     * @param collector Central collector to receive flushed samples
     */
    explicit thread_local_buffer(size_t capacity = DEFAULT_CAPACITY,
                                 std::shared_ptr<central_collector> collector = nullptr);

    /**
     * @brief Destructor - flushes any remaining samples
     */
    ~thread_local_buffer();

    // Non-copyable, non-movable (thread-local resource)
    thread_local_buffer(const thread_local_buffer&) = delete;
    thread_local_buffer& operator=(const thread_local_buffer&) = delete;
    thread_local_buffer(thread_local_buffer&&) = delete;
    thread_local_buffer& operator=(thread_local_buffer&&) = delete;

    /**
     * @brief Record a metric sample (lock-free)
     *
     * @param sample Metric sample to record
     * @return true if recorded, false if buffer is full (caller should flush)
     *
     * @thread_safety Thread-safe (single-threaded access guaranteed by TLS)
     * @performance O(1) - direct array write, ~5-10 ns (no allocation)
     */
    bool record(const metric_sample& sample);

    /**
     * @brief Record a metric sample with automatic flush on overflow
     *
     * @param sample Metric sample to record
     * @return true if recorded successfully (with or without flush)
     *
     * @thread_safety Thread-safe (single-threaded access guaranteed by TLS)
     * @note Automatically flushes and retries if buffer is full
     */
    bool record_auto_flush(const metric_sample& sample);

    /**
     * @brief Flush buffered samples to central collector
     *
     * @return Number of samples flushed
     *
     * @thread_safety Thread-safe (single-threaded access guaranteed by TLS)
     * @note Acquires lock in central_collector during flush
     */
    size_t flush();

    /**
     * @brief Get current number of buffered samples
     * @return Number of samples waiting to be flushed
     */
    size_t size() const { return write_index_; }

    /**
     * @brief Check if buffer is full
     * @return true if at capacity, false otherwise
     */
    bool is_full() const { return write_index_ >= capacity_; }

    /**
     * @brief Get buffer capacity
     * @return Maximum number of samples before flush
     */
    size_t capacity() const { return capacity_; }

    /**
     * @brief Set the central collector
     * @param collector Central collector to receive flushed samples
     */
    void set_collector(std::shared_ptr<central_collector> collector) {
        collector_ = collector;
    }

    /**
     * @brief Get statistics about buffer operations
     */
    struct stats {
        size_t total_records{0};    ///< Total records written
        size_t total_flushes{0};    ///< Total flush operations
        size_t auto_flushes{0};     ///< Flushes triggered by auto_flush
    };

    /**
     * @brief Get buffer statistics
     * @return Statistics about buffer operations
     */
    const stats& get_stats() const { return stats_; }

private:
    std::vector<metric_sample> buffer_;  // Pre-allocated ring buffer
    size_t capacity_;
    size_t write_index_{0};  // Single-writer, no atomic needed
    std::shared_ptr<central_collector> collector_;
    stats stats_;
};

}} // namespace kcenon::monitoring
