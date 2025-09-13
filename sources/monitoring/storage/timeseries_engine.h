#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <set>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../interfaces/metric_types_adapter.h"
#include "../utils/metric_types.h"

namespace monitoring_system {

/**
 * Time series data point
 */
struct time_point {
    std::chrono::steady_clock::time_point timestamp;
    double value;
    std::unordered_map<std::string, std::string> tags;

    bool operator<(const time_point& other) const {
        return timestamp < other.timestamp;
    }
};

/**
 * Time series data structure
 */
struct time_series {
    std::string metric_name;
    std::vector<time_point> points;
    std::unordered_map<std::string, std::string> metadata;

    // Statistics
    double min_value{std::numeric_limits<double>::max()};
    double max_value{std::numeric_limits<double>::lowest()};
    double sum_value{0.0};
    size_t count{0};

    void update_stats(double value) {
        min_value = std::min(min_value, value);
        max_value = std::max(max_value, value);
        sum_value += value;
        count++;
    }

    double average() const {
        return count > 0 ? sum_value / count : 0.0;
    }
};

/**
 * Compression algorithm enumeration
 */
enum class compression_algorithm {
    none,
    snappy,
    lz4,
    zstd,
    gzip
};

/**
 * Storage configuration
 */
struct storage_config {
    // Storage paths
    std::filesystem::path data_directory{"./tsdb_data"};
    std::filesystem::path wal_directory{"./tsdb_wal"};

    // LSM-Tree configuration
    size_t memtable_size_mb{64};
    size_t max_memtables{3};
    size_t level0_file_num_compaction_trigger{4};
    size_t max_background_compactions{2};

    // Compression settings
    compression_algorithm compression{compression_algorithm::lz4};
    size_t compression_block_size{4096};

    // Cache settings
    size_t block_cache_size_mb{128};
    size_t index_cache_size_mb{32};

    // Write settings
    bool sync_writes{false};
    size_t write_buffer_size{1024 * 1024};  // 1MB

    // Retention settings
    std::chrono::hours default_retention{24 * 30};  // 30 days
};

/**
 * LSM-Tree based storage engine for time series data
 */
class lsm_tree_storage {
  public:
    explicit lsm_tree_storage(const storage_config& config);
    ~lsm_tree_storage();

    /**
     * Write a batch of time points
     * @param series_id Series identifier
     * @param points Time points to write
     * @return true if successful
     */
    bool write_batch(const std::string& series_id, const std::vector<time_point>& points);

    /**
     * Read time points in a time range
     * @param series_id Series identifier
     * @param start Start time
     * @param end End time
     * @return Vector of time points
     */
    std::vector<time_point> read_range(
        const std::string& series_id,
        std::chrono::steady_clock::time_point start,
        std::chrono::steady_clock::time_point end) const;

    /**
     * Compact storage files
     * @param level Level to compact
     * @return true if successful
     */
    bool compact(size_t level);

    /**
     * Get storage statistics
     * @return Storage statistics
     */
    struct storage_stats {
        size_t total_bytes{0};
        size_t compressed_bytes{0};
        size_t num_series{0};
        size_t num_points{0};
        size_t num_files{0};
        double compression_ratio{0.0};
    };
    storage_stats get_stats() const;

  private:
    // Memtable structure
    struct memtable {
        std::map<std::string, std::map<std::chrono::steady_clock::time_point, double>> data;
        std::atomic<size_t> size_bytes{0};
        std::chrono::steady_clock::time_point created_at;
        mutable std::shared_mutex mutex;

        bool is_full(size_t max_size) const {
            return size_bytes >= max_size;
        }
    };

    // SSTable structure
    struct sstable {
        std::filesystem::path file_path;
        size_t level{0};
        std::string min_key;
        std::string max_key;
        std::chrono::steady_clock::time_point min_timestamp;
        std::chrono::steady_clock::time_point max_timestamp;
        size_t file_size{0};
        size_t num_entries{0};
    };

    storage_config config_;

    // Memtables
    std::unique_ptr<memtable> active_memtable_;
    std::deque<std::unique_ptr<memtable>> immutable_memtables_;
    mutable std::shared_mutex memtable_mutex_;

    // SSTables
    std::vector<std::vector<sstable>> levels_;  // levels_[0] is Level 0
    mutable std::shared_mutex sstable_mutex_;

    // Write-Ahead Log
    std::unique_ptr<std::ofstream> wal_writer_;
    std::mutex wal_mutex_;

    // Background threads
    std::vector<std::thread> compaction_threads_;
    std::atomic<bool> shutdown_{false};

    // Statistics
    mutable std::mutex stats_mutex_;
    storage_stats stats_;

    // Internal methods
    void flush_memtable(std::unique_ptr<memtable> mt);
    void background_compaction_worker();
    std::unique_ptr<sstable> write_sstable(const memtable& mt, size_t level);
    std::vector<time_point> read_sstable(const sstable& sst, const std::string& series_id,
                                          std::chrono::steady_clock::time_point start,
                                          std::chrono::steady_clock::time_point end) const;
    void merge_sstables(const std::vector<sstable>& tables, size_t target_level);
    std::vector<uint8_t> compress_data(const std::vector<uint8_t>& data) const;
    std::vector<uint8_t> decompress_data(const std::vector<uint8_t>& compressed) const;
};

/**
 * Time series storage engine with indexing and caching
 */
class timeseries_engine {
  public:
    explicit timeseries_engine(const storage_config& config = {});
    ~timeseries_engine();

    /**
     * Write a metric value
     * @param metric_name Metric name
     * @param value Metric value
     * @param timestamp Timestamp (defaults to now)
     * @param tags Optional tags
     * @return true if successful
     */
    bool write(const std::string& metric_name, double value,
               std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now(),
               const std::unordered_map<std::string, std::string>& tags = {});

    /**
     * Write multiple metrics in a batch
     * @param metrics Vector of metrics to write
     * @return Number of successfully written metrics
     */
    size_t write_batch(const std::vector<metric>& metrics);

    /**
     * Query time series data
     * @param metric_name Metric name or pattern
     * @param start Start time
     * @param end End time
     * @param tag_filter Optional tag filters
     * @return Time series data
     */
    std::vector<time_series> query(
        const std::string& metric_name,
        std::chrono::steady_clock::time_point start,
        std::chrono::steady_clock::time_point end,
        const std::unordered_map<std::string, std::string>& tag_filter = {}) const;

    /**
     * Aggregate query with downsampling
     * @param metric_name Metric name or pattern
     * @param start Start time
     * @param end End time
     * @param interval Aggregation interval
     * @param aggregation Aggregation function (avg, sum, min, max, count)
     * @return Aggregated time series
     */
    time_series aggregate_query(
        const std::string& metric_name,
        std::chrono::steady_clock::time_point start,
        std::chrono::steady_clock::time_point end,
        std::chrono::milliseconds interval,
        const std::string& aggregation = "avg") const;

    /**
     * Delete old data based on retention policy
     * @param before Delete data before this timestamp
     * @return Number of deleted points
     */
    size_t delete_before(std::chrono::steady_clock::time_point before);

    /**
     * Get list of all metric names
     * @return Vector of metric names
     */
    std::vector<std::string> list_metrics() const;

    /**
     * Get all unique tag keys for a metric
     * @param metric_name Metric name
     * @return Vector of tag keys
     */
    std::vector<std::string> get_tag_keys(const std::string& metric_name) const;

    /**
     * Get all unique tag values for a tag key
     * @param metric_name Metric name
     * @param tag_key Tag key
     * @return Vector of tag values
     */
    std::vector<std::string> get_tag_values(const std::string& metric_name,
                                             const std::string& tag_key) const;

    /**
     * Optimize storage by running compaction
     * @return true if successful
     */
    bool optimize();

    /**
     * Get engine statistics
     * @return Engine statistics
     */
    struct engine_stats {
        size_t total_metrics{0};
        size_t total_points{0};
        size_t total_series{0};
        size_t storage_bytes{0};
        size_t memory_bytes{0};
        double compression_ratio{0.0};
        double write_throughput{0.0};  // points per second
        double read_throughput{0.0};   // points per second
    };
    engine_stats get_stats() const;

    /**
     * Flush all pending writes to disk
     * @return true if successful
     */
    bool flush();

    /**
     * Create a snapshot of the database
     * @param snapshot_path Path to save snapshot
     * @return true if successful
     */
    bool create_snapshot(const std::filesystem::path& snapshot_path);

    /**
     * Restore from a snapshot
     * @param snapshot_path Path to snapshot
     * @return true if successful
     */
    bool restore_snapshot(const std::filesystem::path& snapshot_path);

  private:
    // Storage backend
    std::unique_ptr<lsm_tree_storage> storage_;

    // Indexing structures
    struct series_index {
        std::string metric_name;
        std::unordered_map<std::string, std::string> tags;
        std::chrono::steady_clock::time_point first_timestamp;
        std::chrono::steady_clock::time_point last_timestamp;
        size_t point_count{0};
    };

    mutable std::shared_mutex index_mutex_;
    std::unordered_map<std::string, series_index> series_indices_;
    std::unordered_map<std::string, std::set<std::string>> metric_to_series_;
    std::unordered_map<std::string, std::unordered_map<std::string, std::set<std::string>>> tag_index_;

    // Write buffer
    struct write_buffer {
        std::vector<std::pair<std::string, time_point>> pending_writes;
        std::mutex mutex;
        std::condition_variable cv;
        size_t size_bytes{0};
    };
    write_buffer write_buffer_;
    std::thread flush_thread_;
    std::atomic<bool> shutdown_{false};

    // Cache
    struct query_cache_entry {
        std::vector<time_series> data;
        std::chrono::steady_clock::time_point cached_at;
    };
    mutable std::mutex cache_mutex_;
    mutable std::unordered_map<std::string, query_cache_entry> query_cache_;
    const size_t max_cache_entries_{1000};
    const std::chrono::seconds cache_ttl_{60};

    // Statistics
    mutable std::mutex stats_mutex_;
    engine_stats stats_;
    std::chrono::steady_clock::time_point start_time_;
    std::atomic<size_t> total_writes_{0};
    std::atomic<size_t> total_reads_{0};

    // Configuration
    storage_config config_;

    // Internal methods
    std::string create_series_id(const std::string& metric_name,
                                  const std::unordered_map<std::string, std::string>& tags) const;
    void update_indices(const std::string& series_id, const std::string& metric_name,
                        const std::unordered_map<std::string, std::string>& tags,
                        std::chrono::steady_clock::time_point timestamp);
    void flush_worker();
    void cleanup_cache();
    bool matches_filter(const series_index& index,
                        const std::unordered_map<std::string, std::string>& filter) const;
    time_series apply_aggregation(const time_series& ts, std::chrono::milliseconds interval,
                                   const std::string& aggregation) const;
};

/**
 * Query builder for time series queries
 */
class query_builder {
  public:
    query_builder& select(const std::string& metric) {
        metric_name_ = metric;
        return *this;
    }

    query_builder& where(const std::string& tag_key, const std::string& tag_value) {
        tag_filters_[tag_key] = tag_value;
        return *this;
    }

    query_builder& from(std::chrono::steady_clock::time_point start) {
        start_time_ = start;
        return *this;
    }

    query_builder& to(std::chrono::steady_clock::time_point end) {
        end_time_ = end;
        return *this;
    }

    query_builder& group_by(std::chrono::milliseconds interval) {
        group_interval_ = interval;
        return *this;
    }

    query_builder& aggregate(const std::string& func) {
        aggregation_func_ = func;
        return *this;
    }

    std::vector<time_series> execute(const timeseries_engine& engine) const;

  private:
    std::string metric_name_;
    std::unordered_map<std::string, std::string> tag_filters_;
    std::chrono::steady_clock::time_point start_time_{std::chrono::steady_clock::now() - std::chrono::hours(1)};
    std::chrono::steady_clock::time_point end_time_{std::chrono::steady_clock::now()};
    std::optional<std::chrono::milliseconds> group_interval_;
    std::string aggregation_func_{"avg"};
};

}  // namespace monitoring_system