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
     * @performance O(1) - simple array write, ~5-10 ns
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

private:
    std::vector<metric_sample> buffer_;
    size_t capacity_;
    size_t write_index_{0};  // Single-writer, no atomic needed
    std::shared_ptr<central_collector> collector_;
};

}} // namespace kcenon::monitoring
