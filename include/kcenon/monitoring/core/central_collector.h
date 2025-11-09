#pragma once

#include <kcenon/monitoring/core/thread_local_buffer.h>
#include <kcenon/monitoring/core/performance_types.h>
#include <kcenon/monitoring/core/result_types.h>
#include <kcenon/monitoring/core/error_codes.h>
#include <shared_mutex>
#include <unordered_map>
#include <memory>
#include <atomic>

namespace kcenon { namespace monitoring {

/**
 * @brief Central collector for aggregating metrics from thread-local buffers
 *
 * Receives batches of metric samples from multiple thread-local buffers and
 * aggregates them into performance profiles.
 *
 * @thread_safety Thread-safe. All methods can be called concurrently.
 *                Uses std::shared_mutex for read/write synchronization.
 */
class central_collector {
public:
    static constexpr size_t DEFAULT_MAX_PROFILES = 10000;

    /**
     * @brief Construct a central collector
     * @param max_profiles Maximum number of operation profiles to maintain
     */
    explicit central_collector(size_t max_profiles = DEFAULT_MAX_PROFILES);

    /**
     * @brief Receive a batch of samples from a thread-local buffer
     *
     * @param samples Vector of metric samples to process
     *
     * @thread_safety Thread-safe. Uses exclusive lock for write access.
     * @performance Batching reduces lock acquisition frequency.
     *              O(n) where n = samples.size()
     */
    void receive_batch(const std::vector<metric_sample>& samples);

    /**
     * @brief Get aggregated profile for an operation
     *
     * @param operation_name Name of the operation
     * @return result<performance_profile> Profile data or error
     *
     * @thread_safety Thread-safe. Uses shared lock for read access.
     */
    result<performance_profile> get_profile(const std::string& operation_name) const;

    /**
     * @brief Get all aggregated profiles
     *
     * @return Map of operation names to performance profiles
     *
     * @thread_safety Thread-safe. Uses shared lock for read access.
     */
    std::unordered_map<std::string, performance_profile> get_all_profiles() const;

    /**
     * @brief Clear all collected data
     *
     * @thread_safety Thread-safe. Uses exclusive lock.
     */
    void clear();

    /**
     * @brief Get the number of tracked operations
     * @return Number of operation profiles
     *
     * @thread_safety Thread-safe. Uses shared lock.
     */
    size_t get_operation_count() const;

    /**
     * @brief Get total number of samples received
     * @return Total sample count across all operations
     *
     * @thread_safety Thread-safe. Atomic operation.
     */
    size_t get_total_sample_count() const {
        return total_samples_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Get statistics about the collector
     *
     * @return Collector statistics
     *
     * @thread_safety Thread-safe.
     */
    struct stats {
        size_t operation_count;     ///< Number of tracked operations
        size_t total_samples;       ///< Total samples received
        size_t batches_received;    ///< Total batches received
        size_t lru_evictions;       ///< Number of LRU evictions
    };

    stats get_stats() const;

private:
    /**
     * @brief Internal structure for profile data with LRU tracking
     */
    struct profile_data {
        performance_profile profile;
        std::atomic<std::chrono::steady_clock::rep> last_access_time;
        std::mutex mutex;  // Per-profile lock for sample aggregation

        profile_data() : last_access_time(0) {}
    };

    /**
     * @brief Process a single sample into a profile
     *
     * @param sample Metric sample to process
     *
     * @note Caller must hold profiles_mutex_ (at least shared lock)
     */
    void process_sample(const metric_sample& sample);

    /**
     * @brief Evict least recently used profile
     *
     * @note Caller must hold profiles_mutex_ (exclusive lock)
     */
    void evict_lru();

    mutable std::shared_mutex profiles_mutex_;
    std::unordered_map<std::string, std::unique_ptr<profile_data>> profiles_;
    size_t max_profiles_;

    // Statistics (atomic for thread-safe updates)
    std::atomic<size_t> total_samples_{0};
    std::atomic<size_t> batches_received_{0};
    std::atomic<size_t> lru_evictions_{0};
};

}} // namespace kcenon::monitoring
