#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <set>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../utils/metric_types.h"
#include "timeseries_engine.h"

namespace monitoring_system {

/**
 * Partition strategy for metrics
 */
enum class partition_strategy {
    by_metric_name,  // One partition per metric
    by_time,         // Partition by time ranges
    by_tag,          // Partition by specific tag value
    by_hash,         // Hash-based partitioning
    hybrid           // Combination of strategies
};

/**
 * Retention policy for metrics
 */
struct retention_policy {
    std::string name;
    std::chrono::hours retention_period;
    std::optional<std::string> metric_pattern;  // Regex pattern for metrics
    std::optional<std::unordered_map<std::string, std::string>> tag_filter;
    size_t max_points{0};  // 0 means unlimited
    bool downsample_on_age{false};
    std::chrono::hours downsample_after{24};
    std::chrono::milliseconds downsample_interval{60000};  // 1 minute
};

/**
 * Database configuration
 */
struct database_config {
    // Storage configuration
    std::filesystem::path data_directory{"./metrics_db"};
    storage_config storage_config;

    // Partitioning configuration
    partition_strategy partition_strategy{partition_strategy::by_metric_name};
    size_t max_partitions{100};
    size_t partition_size_mb{1024};  // 1GB per partition

    // Retention configuration
    std::vector<retention_policy> retention_policies;
    std::chrono::hours default_retention{24 * 30};  // 30 days

    // Performance configuration
    size_t write_batch_size{1000};
    std::chrono::milliseconds write_batch_timeout{100};
    size_t max_concurrent_queries{100};
    size_t query_cache_size_mb{256};

    // Background tasks
    std::chrono::minutes compaction_interval{30};
    std::chrono::hours retention_check_interval{1};
    size_t background_workers{2};
};

/**
 * Metric partition management
 */
class metric_partition {
  public:
    metric_partition(const std::string& id, const std::filesystem::path& base_path,
                     const storage_config& config);
    ~metric_partition();

    /**
     * Write metrics to this partition
     * @param metrics Metrics to write
     * @return Number of successfully written metrics
     */
    size_t write(const std::vector<metric>& metrics);

    /**
     * Query metrics from this partition
     * @param query_func Query function
     * @return Query results
     */
    std::vector<time_series> query(
        std::function<std::vector<time_series>(timeseries_engine&)> query_func);

    /**
     * Get partition statistics
     * @return Partition statistics
     */
    struct partition_stats {
        std::string partition_id;
        size_t total_metrics{0};
        size_t total_points{0};
        size_t size_bytes{0};
        std::chrono::steady_clock::time_point oldest_point;
        std::chrono::steady_clock::time_point newest_point;
        bool is_active{true};
    };
    partition_stats get_stats() const;

    /**
     * Check if partition needs rollover
     * @param max_size Maximum size in bytes
     * @param max_age Maximum age
     * @return true if rollover needed
     */
    bool needs_rollover(size_t max_size, std::chrono::hours max_age) const;

    /**
     * Mark partition as read-only
     */
    void set_readonly();

    /**
     * Compact partition data
     * @return true if successful
     */
    bool compact();

    /**
     * Apply retention policy
     * @param policy Retention policy to apply
     * @return Number of deleted points
     */
    size_t apply_retention(const retention_policy& policy);

  private:
    std::string partition_id_;
    std::filesystem::path partition_path_;
    std::unique_ptr<timeseries_engine> engine_;
    std::atomic<bool> readonly_{false};
    mutable std::shared_mutex mutex_;
    partition_stats stats_;
    std::chrono::steady_clock::time_point created_at_;

    void update_stats(const std::vector<metric>& metrics);
};

/**
 * High-performance metric database with partitioning and retention
 */
class metric_database {
  public:
    explicit metric_database(const database_config& config = {});
    ~metric_database();

    /**
     * Write a single metric
     * @param metric Metric to write
     * @return true if successful
     */
    bool write(const metric& m);

    /**
     * Write multiple metrics in batch
     * @param metrics Metrics to write
     * @return Number of successfully written metrics
     */
    size_t write_batch(const std::vector<metric>& metrics);

    /**
     * Query metrics
     * @param metric_name Metric name or pattern
     * @param start Start time
     * @param end End time
     * @param tags Optional tag filters
     * @return Query results
     */
    std::vector<time_series> query(
        const std::string& metric_name,
        std::chrono::steady_clock::time_point start,
        std::chrono::steady_clock::time_point end,
        const std::unordered_map<std::string, std::string>& tags = {});

    /**
     * Aggregate query
     * @param metric_name Metric name or pattern
     * @param start Start time
     * @param end End time
     * @param interval Aggregation interval
     * @param function Aggregation function
     * @param tags Optional tag filters
     * @return Aggregated results
     */
    std::vector<time_series> aggregate_query(
        const std::string& metric_name,
        std::chrono::steady_clock::time_point start,
        std::chrono::steady_clock::time_point end,
        std::chrono::milliseconds interval,
        const std::string& function = "avg",
        const std::unordered_map<std::string, std::string>& tags = {});

    /**
     * Get list of all metrics
     * @return Vector of metric names
     */
    std::vector<std::string> list_metrics() const;

    /**
     * Get metric metadata
     * @param metric_name Metric name
     * @return Metric metadata
     */
    struct metric_metadata {
        std::string name;
        std::set<std::string> tag_keys;
        size_t total_points{0};
        std::chrono::steady_clock::time_point first_seen;
        std::chrono::steady_clock::time_point last_seen;
        std::vector<std::string> partitions;
    };
    std::optional<metric_metadata> get_metric_metadata(const std::string& metric_name) const;

    /**
     * Add retention policy
     * @param policy Retention policy to add
     */
    void add_retention_policy(const retention_policy& policy);

    /**
     * Remove retention policy
     * @param policy_name Name of policy to remove
     * @return true if removed
     */
    bool remove_retention_policy(const std::string& policy_name);

    /**
     * Force retention policy application
     * @return Total number of deleted points
     */
    size_t apply_retention_policies();

    /**
     * Get database statistics
     * @return Database statistics
     */
    struct database_stats {
        size_t total_metrics{0};
        size_t total_points{0};
        size_t total_partitions{0};
        size_t active_partitions{0};
        size_t total_size_bytes{0};
        double write_throughput{0.0};  // metrics per second
        double query_throughput{0.0};  // queries per second
        size_t cache_hit_rate{0};      // percentage
    };
    database_stats get_stats() const;

    /**
     * Optimize database by running compaction
     * @return true if successful
     */
    bool optimize();

    /**
     * Create database backup
     * @param backup_path Path for backup
     * @return true if successful
     */
    bool backup(const std::filesystem::path& backup_path);

    /**
     * Restore from backup
     * @param backup_path Path to backup
     * @return true if successful
     */
    bool restore(const std::filesystem::path& backup_path);

    /**
     * Flush all pending writes
     * @return true if successful
     */
    bool flush();

    /**
     * Close database gracefully
     */
    void close();

  private:
    // Configuration
    database_config config_;

    // Partitions
    mutable std::shared_mutex partitions_mutex_;
    std::map<std::string, std::unique_ptr<metric_partition>> partitions_;
    std::unique_ptr<metric_partition> active_partition_;

    // Metric index
    struct metric_index_entry {
        std::string metric_name;
        std::set<std::string> partitions;
        std::set<std::string> tag_keys;
        size_t total_points{0};
        std::chrono::steady_clock::time_point first_seen;
        std::chrono::steady_clock::time_point last_seen;
    };
    mutable std::shared_mutex index_mutex_;
    std::unordered_map<std::string, metric_index_entry> metric_index_;

    // Write buffer
    struct write_buffer_entry {
        metric data;
        std::chrono::steady_clock::time_point received_at;
    };
    std::mutex write_buffer_mutex_;
    std::vector<write_buffer_entry> write_buffer_;
    std::condition_variable write_buffer_cv_;

    // Query cache
    struct query_cache_key {
        std::string query_hash;
        bool operator==(const query_cache_key& other) const {
            return query_hash == other.query_hash;
        }
    };
    struct query_cache_key_hash {
        size_t operator()(const query_cache_key& key) const {
            return std::hash<std::string>{}(key.query_hash);
        }
    };
    struct query_cache_value {
        std::vector<time_series> results;
        std::chrono::steady_clock::time_point cached_at;
    };
    mutable std::mutex cache_mutex_;
    mutable std::unordered_map<query_cache_key, query_cache_value, query_cache_key_hash> query_cache_;

    // Background threads
    std::vector<std::thread> background_workers_;
    std::thread write_worker_;
    std::thread compaction_worker_;
    std::thread retention_worker_;
    std::atomic<bool> shutdown_{false};

    // Statistics
    mutable std::mutex stats_mutex_;
    database_stats stats_;
    std::chrono::steady_clock::time_point start_time_;
    std::atomic<size_t> total_writes_{0};
    std::atomic<size_t> total_queries_{0};
    std::atomic<size_t> cache_hits_{0};

    // Internal methods
    std::string get_partition_id(const metric& m) const;
    metric_partition* get_or_create_partition(const std::string& partition_id);
    void rotate_active_partition();
    void write_worker_thread();
    void compaction_worker_thread();
    void retention_worker_thread();
    void update_metric_index(const metric& m, const std::string& partition_id);
    std::string compute_query_hash(const std::string& metric_name,
                                    std::chrono::steady_clock::time_point start,
                                    std::chrono::steady_clock::time_point end,
                                    const std::unordered_map<std::string, std::string>& tags) const;
    void cleanup_cache();
    std::vector<metric_partition*> get_partitions_for_query(
        const std::string& metric_name,
        std::chrono::steady_clock::time_point start,
        std::chrono::steady_clock::time_point end) const;
};

/**
 * Database connection pool for concurrent access
 */
class database_connection_pool {
  public:
    database_connection_pool(const database_config& config, size_t pool_size = 10);
    ~database_connection_pool();

    /**
     * Get a database connection from the pool
     * @return Database connection
     */
    class connection {
      public:
        connection(metric_database* db, database_connection_pool* pool);
        ~connection();

        metric_database* operator->() { return db_; }
        metric_database& operator*() { return *db_; }

      private:
        metric_database* db_;
        database_connection_pool* pool_;
    };

    connection get_connection();

    /**
     * Get pool statistics
     * @return Pool statistics
     */
    struct pool_stats {
        size_t total_connections{0};
        size_t active_connections{0};
        size_t idle_connections{0};
        size_t total_requests{0};
        size_t wait_time_ms{0};
    };
    pool_stats get_stats() const;

  private:
    database_config config_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<std::unique_ptr<metric_database>> available_;
    std::set<metric_database*> in_use_;
    size_t max_connections_;
    pool_stats stats_;

    void return_connection(metric_database* db);
    friend class connection;
};

/**
 * Distributed database coordinator for sharding
 */
class distributed_database {
  public:
    struct shard_config {
        std::string shard_id;
        std::string host;
        uint16_t port;
        database_config db_config;
        std::function<size_t(const metric&)> shard_key_func;
    };

    explicit distributed_database(const std::vector<shard_config>& shards);
    ~distributed_database();

    /**
     * Write metric to appropriate shard
     * @param m Metric to write
     * @return true if successful
     */
    bool write(const metric& m);

    /**
     * Query across all shards
     * @param metric_name Metric name
     * @param start Start time
     * @param end End time
     * @param tags Tag filters
     * @return Merged results from all shards
     */
    std::vector<time_series> query(
        const std::string& metric_name,
        std::chrono::steady_clock::time_point start,
        std::chrono::steady_clock::time_point end,
        const std::unordered_map<std::string, std::string>& tags = {});

    /**
     * Get shard statistics
     * @return Map of shard ID to statistics
     */
    std::unordered_map<std::string, database_stats> get_shard_stats() const;

  private:
    struct shard {
        shard_config config;
        std::unique_ptr<metric_database> database;
        std::atomic<bool> is_healthy{true};
    };

    std::vector<std::unique_ptr<shard>> shards_;
    std::hash<std::string> hasher_;

    size_t get_shard_index(const metric& m) const;
    void health_check_worker();
    std::thread health_check_thread_;
    std::atomic<bool> shutdown_{false};
};

}  // namespace monitoring_system