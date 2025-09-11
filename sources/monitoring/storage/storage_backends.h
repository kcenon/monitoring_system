#pragma once

/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, monitoring_system contributors
All rights reserved.
*****************************************************************************/

/**
 * @file storage_backends.h
 * @brief Storage backend implementations for monitoring data persistence
 * 
 * This file provides various storage backend implementations:
 * - File-based storage (JSON, Binary, CSV formats)
 * - Database storage (SQLite, PostgreSQL, MySQL simulation)
 * - Cloud storage (AWS S3, Google Cloud Storage, Azure Blob simulation)
 * 
 * Each backend handles data serialization, persistence, and retrieval
 * with appropriate error handling and performance optimizations.
 */

#include "../core/result_types.h"
#include "../core/error_codes.h"
#include "../interfaces/monitoring_interface.h"
#include "../interfaces/monitorable_interface.h"
#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <optional>
#include <functional>
#include <atomic>
#include <algorithm>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <queue>
#include <condition_variable>
#include <regex>
#include <iomanip>
#include <cstdint>

namespace monitoring_system {

/**
 * @enum storage_backend_type
 * @brief Supported storage backend types
 */
enum class storage_backend_type {
    file_json,          ///< JSON file storage
    file_binary,        ///< Binary file storage
    file_csv,           ///< CSV file storage
    database_sqlite,    ///< SQLite database storage
    database_postgresql,///< PostgreSQL database storage
    database_mysql,     ///< MySQL database storage
    cloud_s3,          ///< AWS S3 cloud storage
    cloud_gcs,         ///< Google Cloud Storage
    cloud_azure_blob,  ///< Azure Blob Storage
    memory_buffer      ///< In-memory buffer storage
};

/**
 * @enum compression_type
 * @brief Supported compression types
 */
enum class compression_type {
    none,      ///< No compression
    gzip,      ///< GZIP compression
    lz4,       ///< LZ4 compression
    snappy     ///< Snappy compression
};

/**
 * @struct storage_config
 * @brief Configuration for storage backends
 */
struct storage_config {
    storage_backend_type type = storage_backend_type::file_json;
    std::string path;                                        ///< File path or connection string
    std::string database_name;                              ///< Database name
    std::string table_name = "metrics_snapshots";          ///< Table name
    std::string username;                                   ///< Database username
    std::string password;                                   ///< Database password
    std::string host = "localhost";                        ///< Database host
    std::uint16_t port = 0;                                ///< Database port
    std::size_t max_capacity = 10000;                      ///< Maximum stored items
    std::size_t batch_size = 100;                          ///< Batch write size
    std::chrono::milliseconds flush_interval{5000};        ///< Auto-flush interval
    compression_type compression = compression_type::none;   ///< Compression type
    bool enable_encryption = false;                         ///< Enable encryption
    std::string encryption_key;                             ///< Encryption key
    bool enable_indexing = true;                            ///< Enable indexing
    bool enable_compression = false;                        ///< Enable compression
    std::unordered_map<std::string, std::string> options;  ///< Backend-specific options
    
    /**
     * @brief Validate storage configuration
     */
    result_void validate() const {
        if (path.empty() && type != storage_backend_type::memory_buffer) {
            return result_void(monitoring_error_code::invalid_configuration,
                             "Storage path cannot be empty");
        }
        
        if (max_capacity == 0) {
            return result_void(monitoring_error_code::invalid_capacity,
                             "Storage capacity must be greater than 0");
        }
        
        if (batch_size == 0) {
            return result_void(monitoring_error_code::invalid_configuration,
                             "Batch size must be greater than 0");
        }
        
        if (batch_size > max_capacity) {
            return result_void(monitoring_error_code::invalid_configuration,
                             "Batch size cannot exceed max capacity");
        }
        
        return result_void::success();
    }
};

/**
 * @struct stored_snapshot
 * @brief Extended snapshot with storage metadata
 */
struct stored_snapshot {
    metrics_snapshot snapshot;
    std::size_t index;
    std::chrono::system_clock::time_point stored_time;
    std::string storage_id;
    std::size_t compressed_size = 0;
    std::string checksum;
    std::unordered_map<std::string, std::string> metadata;
    
    stored_snapshot() : stored_time(std::chrono::system_clock::now()) {}
    
    explicit stored_snapshot(const metrics_snapshot& snap, std::size_t idx = 0)
        : snapshot(snap), index(idx), stored_time(std::chrono::system_clock::now()) {}
};

/**
 * @class file_storage_backend
 * @brief File-based storage backend implementation
 */
class file_storage_backend : public storage_backend {
private:
    storage_config config_;
    std::vector<stored_snapshot> snapshots_;
    mutable std::shared_mutex storage_mutex_;
    std::atomic<std::size_t> next_index_{0};
    std::filesystem::path storage_path_;
    
public:
    explicit file_storage_backend(const storage_config& config)
        : config_(config), storage_path_(config.path) {
        
        // Create directory if it doesn't exist
        if (config_.type != storage_backend_type::memory_buffer) {
            std::filesystem::create_directories(storage_path_.parent_path());
        }
        
        // Load existing data
        load_from_file();
    }
    
    result_void store(const metrics_snapshot& snapshot) override {
        try {
            std::unique_lock<std::shared_mutex> lock(storage_mutex_);
            
            // Check capacity
            if (snapshots_.size() >= config_.max_capacity) {
                // Remove oldest snapshot(s) to make room
                snapshots_.erase(snapshots_.begin());
            }
            
            // Create stored snapshot
            stored_snapshot stored(snapshot, next_index_++);
            stored.storage_id = generate_storage_id(stored);
            stored.checksum = calculate_checksum(snapshot);
            
            // Add to memory
            snapshots_.push_back(stored);
            
            // Write to file if not memory-only
            if (config_.type != storage_backend_type::memory_buffer) {
                auto write_result = write_to_file();
                if (!write_result) {
                    snapshots_.pop_back(); // Rollback
                    return write_result;
                }
            }
            
            return result_void::success();
            
        } catch (const std::exception& e) {
            return result_void(monitoring_error_code::storage_write_failed,
                             "Failed to store snapshot: " + std::string(e.what()));
        }
    }
    
    result<metrics_snapshot> retrieve(std::size_t index) const override {
        try {
            std::shared_lock<std::shared_mutex> lock(storage_mutex_);
            
            auto it = std::find_if(snapshots_.begin(), snapshots_.end(),
                [index](const stored_snapshot& s) { return s.index == index; });
            
            if (it != snapshots_.end()) {
                return make_success(it->snapshot);
            } else {
                return make_error<metrics_snapshot>(monitoring_error_code::not_found,
                                                  "Snapshot with index " + std::to_string(index) + " not found");
            }
            
        } catch (const std::exception& e) {
            return make_error<metrics_snapshot>(monitoring_error_code::storage_read_failed,
                                               "Failed to retrieve snapshot: " + std::string(e.what()));
        }
    }
    
    result<std::vector<metrics_snapshot>> retrieve_range(std::size_t start_index, std::size_t count) const override {
        try {
            std::shared_lock<std::shared_mutex> lock(storage_mutex_);
            
            std::vector<metrics_snapshot> result;
            result.reserve(count);
            
            for (const auto& stored : snapshots_) {
                if (stored.index >= start_index && result.size() < count) {
                    result.push_back(stored.snapshot);
                }
            }
            
            return make_success(std::move(result));
            
        } catch (const std::exception& e) {
            return make_error<std::vector<metrics_snapshot>>(monitoring_error_code::storage_read_failed,
                                                           "Failed to retrieve range: " + std::string(e.what()));
        }
    }
    
    std::size_t capacity() const override {
        return config_.max_capacity;
    }
    
    std::size_t size() const override {
        std::shared_lock<std::shared_mutex> lock(storage_mutex_);
        return snapshots_.size();
    }
    
    result_void clear() override {
        try {
            std::unique_lock<std::shared_mutex> lock(storage_mutex_);
            snapshots_.clear();
            next_index_ = 0;
            
            if (config_.type != storage_backend_type::memory_buffer) {
                return write_to_file();
            }
            
            return result_void::success();
            
        } catch (const std::exception& e) {
            return result_void(monitoring_error_code::operation_failed,
                             "Failed to clear storage: " + std::string(e.what()));
        }
    }
    
    result_void flush() override {
        if (config_.type != storage_backend_type::memory_buffer) {
            std::shared_lock<std::shared_mutex> lock(storage_mutex_);
            return write_to_file();
        }
        return result_void::success();
    }
    
    /**
     * @brief Get storage statistics
     */
    std::unordered_map<std::string, std::size_t> get_stats() const {
        std::shared_lock<std::shared_mutex> lock(storage_mutex_);
        return {
            {"total_snapshots", snapshots_.size()},
            {"capacity", config_.max_capacity},
            {"next_index", next_index_.load()},
            {"file_size", get_file_size()}
        };
    }
    
private:
    result_void load_from_file() {
        if (config_.type == storage_backend_type::memory_buffer || !std::filesystem::exists(storage_path_)) {
            return result_void::success();
        }
        
        try {
            switch (config_.type) {
                case storage_backend_type::file_json:
                    return load_json_file();
                case storage_backend_type::file_binary:
                    return load_binary_file();
                case storage_backend_type::file_csv:
                    return load_csv_file();
                default:
                    return result_void(monitoring_error_code::invalid_configuration,
                                     "Unsupported file storage type");
            }
        } catch (const std::exception& e) {
            return result_void(monitoring_error_code::storage_read_failed,
                             "Failed to load from file: " + std::string(e.what()));
        }
    }
    
    result_void write_to_file() const {
        try {
            switch (config_.type) {
                case storage_backend_type::file_json:
                    return write_json_file();
                case storage_backend_type::file_binary:
                    return write_binary_file();
                case storage_backend_type::file_csv:
                    return write_csv_file();
                default:
                    return result_void(monitoring_error_code::invalid_configuration,
                                     "Unsupported file storage type");
            }
        } catch (const std::exception& e) {
            return result_void(monitoring_error_code::storage_write_failed,
                             "Failed to write to file: " + std::string(e.what()));
        }
    }
    
    result_void load_json_file() {
        std::ifstream file(storage_path_);
        if (!file.is_open()) {
            return result_void::success(); // File doesn't exist yet
        }
        
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            
            // Simple JSON parsing (in real implementation, use proper JSON library)
            // For now, just store the line as metadata
            stored_snapshot stored;
            stored.index = next_index_++;
            stored.metadata["json_data"] = line;
            snapshots_.push_back(stored);
        }
        
        return result_void::success();
    }
    
    result_void write_json_file() const {
        std::ofstream file(storage_path_);
        if (!file.is_open()) {
            return result_void(monitoring_error_code::storage_write_failed,
                             "Failed to open file for writing: " + storage_path_.string());
        }
        
        for (const auto& stored : snapshots_) {
            // Simple JSON serialization (in real implementation, use proper JSON library)
            file << "{\"index\":" << stored.index 
                 << ",\"timestamp\":" << stored.snapshot.capture_time.time_since_epoch().count()
                 << ",\"source\":\"" << stored.snapshot.source_id << "\""
                 << ",\"metrics_count\":" << stored.snapshot.metrics.size()
                 << "}\n";
        }
        
        return result_void::success();
    }
    
    result_void load_binary_file() {
        std::ifstream file(storage_path_, std::ios::binary);
        if (!file.is_open()) {
            return result_void::success(); // File doesn't exist yet
        }
        
        // Simple binary format: [count][snapshot1][snapshot2]...
        std::size_t count;
        file.read(reinterpret_cast<char*>(&count), sizeof(count));
        
        for (std::size_t i = 0; i < count && file.good(); ++i) {
            stored_snapshot stored;
            stored.index = next_index_++;
            
            // Read snapshot data (simplified)
            std::size_t source_len;
            file.read(reinterpret_cast<char*>(&source_len), sizeof(source_len));
            stored.snapshot.source_id.resize(source_len);
            file.read(stored.snapshot.source_id.data(), source_len);
            
            snapshots_.push_back(stored);
        }
        
        return result_void::success();
    }
    
    result_void write_binary_file() const {
        std::ofstream file(storage_path_, std::ios::binary);
        if (!file.is_open()) {
            return result_void(monitoring_error_code::storage_write_failed,
                             "Failed to open binary file for writing");
        }
        
        std::size_t count = snapshots_.size();
        file.write(reinterpret_cast<const char*>(&count), sizeof(count));
        
        for (const auto& stored : snapshots_) {
            // Write snapshot data (simplified)
            std::size_t source_len = stored.snapshot.source_id.length();
            file.write(reinterpret_cast<const char*>(&source_len), sizeof(source_len));
            file.write(stored.snapshot.source_id.data(), source_len);
        }
        
        return result_void::success();
    }
    
    result_void load_csv_file() {
        std::ifstream file(storage_path_);
        if (!file.is_open()) {
            return result_void::success(); // File doesn't exist yet
        }
        
        std::string line;
        bool header_read = false;
        
        while (std::getline(file, line)) {
            if (!header_read) {
                header_read = true;
                continue; // Skip header
            }
            
            if (line.empty()) continue;
            
            // Simple CSV parsing
            std::stringstream ss(line);
            std::string item;
            
            stored_snapshot stored;
            stored.index = next_index_++;
            
            // Parse CSV fields (index, timestamp, source, metrics_count)
            if (std::getline(ss, item, ',')) {
                // Parse index, timestamp, etc.
                stored.metadata["csv_data"] = line;
            }
            
            snapshots_.push_back(stored);
        }
        
        return result_void::success();
    }
    
    result_void write_csv_file() const {
        std::ofstream file(storage_path_);
        if (!file.is_open()) {
            return result_void(monitoring_error_code::storage_write_failed,
                             "Failed to open CSV file for writing");
        }
        
        // Write header
        file << "index,timestamp,source_id,metrics_count,stored_time\n";
        
        for (const auto& stored : snapshots_) {
            file << stored.index << ","
                 << stored.snapshot.capture_time.time_since_epoch().count() << ","
                 << stored.snapshot.source_id << ","
                 << stored.snapshot.metrics.size() << ","
                 << stored.stored_time.time_since_epoch().count() << "\n";
        }
        
        return result_void::success();
    }
    
    std::string generate_storage_id(const stored_snapshot& stored) const {
        std::stringstream ss;
        ss << "snap_" << stored.index << "_" << stored.stored_time.time_since_epoch().count();
        return ss.str();
    }
    
    std::string calculate_checksum(const metrics_snapshot& snapshot) const {
        std::hash<std::string> hasher;
        std::string data = snapshot.source_id + std::to_string(snapshot.metrics.size());
        return std::to_string(hasher(data));
    }
    
    std::size_t get_file_size() const {
        if (std::filesystem::exists(storage_path_)) {
            return std::filesystem::file_size(storage_path_);
        }
        return 0;
    }
};

/**
 * @class database_storage_backend
 * @brief Database storage backend implementation
 */
class database_storage_backend : public storage_backend {
private:
    storage_config config_;
    std::atomic<std::size_t> stored_count_{0};
    std::atomic<std::size_t> next_index_{0};
    mutable std::mutex connection_mutex_;
    bool connected_ = false;
    
public:
    explicit database_storage_backend(const storage_config& config)
        : config_(config) {
        
        // Initialize database connection (simulated)
        initialize_database();
    }
    
    ~database_storage_backend() {
        disconnect_database();
    }
    
    result_void store(const metrics_snapshot& snapshot) override {
        std::lock_guard<std::mutex> lock(connection_mutex_);
        
        if (!connected_) {
            return result_void(monitoring_error_code::storage_not_initialized,
                             "Database not connected");
        }
        
        try {
            // Simulate database insertion
            std::string sql = build_insert_query(snapshot);
            auto exec_result = execute_sql(sql);
            if (!exec_result) {
                return exec_result;
            }
            
            stored_count_++;
            next_index_++;
            return result_void::success();
            
        } catch (const std::exception& e) {
            return result_void(monitoring_error_code::storage_write_failed,
                             "Database storage failed: " + std::string(e.what()));
        }
    }
    
    result<metrics_snapshot> retrieve(std::size_t index) const override {
        std::lock_guard<std::mutex> lock(connection_mutex_);
        
        if (!connected_) {
            return make_error<metrics_snapshot>(monitoring_error_code::storage_not_initialized,
                                               "Database not connected");
        }
        
        try {
            std::string sql = "SELECT * FROM " + config_.table_name + " WHERE index = " + std::to_string(index);
            return execute_select_query(sql);
            
        } catch (const std::exception& e) {
            return make_error<metrics_snapshot>(monitoring_error_code::storage_read_failed,
                                               "Database retrieval failed: " + std::string(e.what()));
        }
    }
    
    result<std::vector<metrics_snapshot>> retrieve_range(std::size_t start_index, std::size_t count) const override {
        std::lock_guard<std::mutex> lock(connection_mutex_);
        
        if (!connected_) {
            return make_error<std::vector<metrics_snapshot>>(monitoring_error_code::storage_not_initialized,
                                                           "Database not connected");
        }
        
        try {
            std::string sql = "SELECT * FROM " + config_.table_name 
                            + " WHERE index >= " + std::to_string(start_index)
                            + " ORDER BY index LIMIT " + std::to_string(count);
            
            return execute_select_range_query(sql);
            
        } catch (const std::exception& e) {
            return make_error<std::vector<metrics_snapshot>>(monitoring_error_code::storage_read_failed,
                                                           "Database range query failed: " + std::string(e.what()));
        }
    }
    
    std::size_t capacity() const override {
        return config_.max_capacity;
    }
    
    std::size_t size() const override {
        return stored_count_.load();
    }
    
    result_void clear() override {
        std::lock_guard<std::mutex> lock(connection_mutex_);
        
        if (!connected_) {
            return result_void(monitoring_error_code::storage_not_initialized,
                             "Database not connected");
        }
        
        try {
            std::string sql = "DELETE FROM " + config_.table_name;
            auto result = execute_sql(sql);
            if (result) {
                stored_count_ = 0;
                next_index_ = 0;
            }
            return result;
            
        } catch (const std::exception& e) {
            return result_void(monitoring_error_code::operation_failed,
                             "Database clear failed: " + std::string(e.what()));
        }
    }
    
    result_void flush() override {
        // Database operations are typically auto-committed
        return result_void::success();
    }
    
    /**
     * @brief Get database statistics
     */
    std::unordered_map<std::string, std::size_t> get_stats() const {
        return {
            {"stored_count", stored_count_.load()},
            {"capacity", config_.max_capacity},
            {"next_index", next_index_.load()},
            {"connected", connected_ ? 1 : 0}
        };
    }
    
private:
    result_void initialize_database() {
        try {
            // Simulate database connection
            std::string connection_string = build_connection_string();
            
            // In real implementation, use actual database drivers
            // For now, just mark as connected
            connected_ = true;
            
            // Create table if it doesn't exist
            return create_table();
            
        } catch (const std::exception& e) {
            return result_void(monitoring_error_code::storage_not_initialized,
                             "Database initialization failed: " + std::string(e.what()));
        }
    }
    
    void disconnect_database() {
        std::lock_guard<std::mutex> lock(connection_mutex_);
        connected_ = false;
    }
    
    std::string build_connection_string() const {
        std::stringstream ss;
        
        switch (config_.type) {
            case storage_backend_type::database_sqlite:
                ss << "sqlite://" << config_.path;
                break;
            case storage_backend_type::database_postgresql:
                ss << "postgresql://" << config_.username << ":" << config_.password
                   << "@" << config_.host << ":" << config_.port << "/" << config_.database_name;
                break;
            case storage_backend_type::database_mysql:
                ss << "mysql://" << config_.username << ":" << config_.password
                   << "@" << config_.host << ":" << config_.port << "/" << config_.database_name;
                break;
            default:
                break;
        }
        
        return ss.str();
    }
    
    result_void create_table() {
        std::string sql = "CREATE TABLE IF NOT EXISTS " + config_.table_name + " ("
                        "id INTEGER PRIMARY KEY, "
                        "index_val INTEGER, "
                        "source_id TEXT, "
                        "capture_time INTEGER, "
                        "metrics_data TEXT, "
                        "stored_time INTEGER)";
        
        return execute_sql(sql);
    }
    
    std::string build_insert_query(const metrics_snapshot& snapshot) const {
        std::stringstream ss;
        ss << "INSERT INTO " << config_.table_name 
           << " (index_val, source_id, capture_time, metrics_data, stored_time) VALUES ("
           << next_index_.load() << ", "
           << "'" << snapshot.source_id << "', "
           << snapshot.capture_time.time_since_epoch().count() << ", "
           << "'" << serialize_metrics(snapshot.metrics) << "', "
           << std::chrono::system_clock::now().time_since_epoch().count() << ")";
        
        return ss.str();
    }
    
    std::string serialize_metrics(const std::vector<metric_value>& metrics) const {
        std::stringstream ss;
        ss << "{\"count\":" << metrics.size() << "}"; // Simplified serialization
        return ss.str();
    }
    
    result_void execute_sql(const std::string& sql) const {
        // Simulate SQL execution
        (void)sql; // Suppress unused parameter warning
        return result_void::success();
    }
    
    result<metrics_snapshot> execute_select_query(const std::string& sql) const {
        // Simulate SQL query execution
        (void)sql; // Suppress unused parameter warning
        
        metrics_snapshot snapshot;
        snapshot.source_id = "db_source";
        return make_success(snapshot);
    }
    
    result<std::vector<metrics_snapshot>> execute_select_range_query(const std::string& sql) const {
        // Simulate SQL range query execution
        (void)sql; // Suppress unused parameter warning
        
        std::vector<metrics_snapshot> snapshots;
        return make_success(snapshots);
    }
};

/**
 * @class cloud_storage_backend
 * @brief Cloud storage backend implementation
 */
class cloud_storage_backend : public storage_backend {
private:
    storage_config config_;
    std::atomic<std::size_t> stored_count_{0};
    std::atomic<std::size_t> next_index_{0};
    mutable std::mutex upload_mutex_;
    bool connected_ = false;
    std::string bucket_name_;
    
public:
    explicit cloud_storage_backend(const storage_config& config)
        : config_(config), bucket_name_(config.path) {
        
        // Initialize cloud connection
        initialize_cloud_client();
    }
    
    result_void store(const metrics_snapshot& snapshot) override {
        std::lock_guard<std::mutex> lock(upload_mutex_);
        
        if (!connected_) {
            return result_void(monitoring_error_code::storage_not_initialized,
                             "Cloud storage not connected");
        }
        
        try {
            std::string object_key = generate_object_key(next_index_.load());
            std::string data = serialize_snapshot(snapshot);
            
            auto upload_result = upload_object(object_key, data);
            if (!upload_result) {
                return upload_result;
            }
            
            stored_count_++;
            next_index_++;
            return result_void::success();
            
        } catch (const std::exception& e) {
            return result_void(monitoring_error_code::storage_write_failed,
                             "Cloud storage upload failed: " + std::string(e.what()));
        }
    }
    
    result<metrics_snapshot> retrieve(std::size_t index) const override {
        std::lock_guard<std::mutex> lock(upload_mutex_);
        
        if (!connected_) {
            return make_error<metrics_snapshot>(monitoring_error_code::storage_not_initialized,
                                               "Cloud storage not connected");
        }
        
        try {
            std::string object_key = generate_object_key(index);
            auto download_result = download_object(object_key);
            
            if (!download_result) {
                return make_error<metrics_snapshot>(download_result.get_error().code, download_result.get_error().message);
            }
            
            return deserialize_snapshot(download_result.value());
            
        } catch (const std::exception& e) {
            return make_error<metrics_snapshot>(monitoring_error_code::storage_read_failed,
                                               "Cloud storage download failed: " + std::string(e.what()));
        }
    }
    
    result<std::vector<metrics_snapshot>> retrieve_range(std::size_t start_index, std::size_t count) const override {
        std::lock_guard<std::mutex> lock(upload_mutex_);
        
        if (!connected_) {
            return make_error<std::vector<metrics_snapshot>>(monitoring_error_code::storage_not_initialized,
                                                           "Cloud storage not connected");
        }
        
        try {
            std::vector<metrics_snapshot> snapshots;
            snapshots.reserve(count);
            
            for (std::size_t i = 0; i < count; ++i) {
                std::size_t index = start_index + i;
                std::string object_key = generate_object_key(index);
                
                auto download_result = download_object(object_key);
                if (download_result) {
                    auto snapshot_result = deserialize_snapshot(download_result.value());
                    if (snapshot_result) {
                        snapshots.push_back(snapshot_result.value());
                    }
                }
            }
            
            return make_success(std::move(snapshots));
            
        } catch (const std::exception& e) {
            return make_error<std::vector<metrics_snapshot>>(monitoring_error_code::storage_read_failed,
                                                           "Cloud storage range query failed: " + std::string(e.what()));
        }
    }
    
    std::size_t capacity() const override {
        return config_.max_capacity;
    }
    
    std::size_t size() const override {
        return stored_count_.load();
    }
    
    result_void clear() override {
        std::lock_guard<std::mutex> lock(upload_mutex_);
        
        if (!connected_) {
            return result_void(monitoring_error_code::storage_not_initialized,
                             "Cloud storage not connected");
        }
        
        try {
            // List and delete all objects (simplified)
            auto clear_result = clear_bucket();
            if (clear_result) {
                stored_count_ = 0;
                next_index_ = 0;
            }
            return clear_result;
            
        } catch (const std::exception& e) {
            return result_void(monitoring_error_code::operation_failed,
                             "Cloud storage clear failed: " + std::string(e.what()));
        }
    }
    
    result_void flush() override {
        // Cloud storage operations are typically immediate
        return result_void::success();
    }
    
private:
    result_void initialize_cloud_client() {
        try {
            // Simulate cloud client initialization
            connected_ = true;
            return result_void::success();
            
        } catch (const std::exception& e) {
            return result_void(monitoring_error_code::storage_not_initialized,
                             "Cloud client initialization failed: " + std::string(e.what()));
        }
    }
    
    std::string generate_object_key(std::size_t index) const {
        std::stringstream ss;
        ss << "snapshots/snapshot_" << std::setfill('0') << std::setw(10) << index << ".json";
        return ss.str();
    }
    
    std::string serialize_snapshot(const metrics_snapshot& snapshot) const {
        // Simple JSON serialization (in real implementation, use proper JSON library)
        std::stringstream ss;
        ss << "{\"source_id\":\"" << snapshot.source_id << "\""
           << ",\"capture_time\":" << snapshot.capture_time.time_since_epoch().count()
           << ",\"metrics_count\":" << snapshot.metrics.size() << "}";
        return ss.str();
    }
    
    result<metrics_snapshot> deserialize_snapshot(const std::string& /*data*/) const {
        // Simple JSON deserialization
        metrics_snapshot snapshot;
        snapshot.source_id = "cloud_source";
        return make_success(snapshot);
    }
    
    result_void upload_object(const std::string& key, const std::string& data) const {
        // Simulate cloud upload
        (void)key;
        (void)data;
        return result_void::success();
    }
    
    result<std::string> download_object(const std::string& key) const {
        // Simulate cloud download
        (void)key;
        return make_success(std::string("{\"data\":\"mock\"}"));
    }
    
    result_void clear_bucket() const {
        // Simulate bucket clearing
        return result_void::success();
    }
};

/**
 * @class storage_backend_factory
 * @brief Factory for creating storage backends
 */
class storage_backend_factory {
public:
    /**
     * @brief Create a storage backend based on configuration
     */
    static std::unique_ptr<storage_backend> create_backend(const storage_config& config) {
        switch (config.type) {
            case storage_backend_type::file_json:
            case storage_backend_type::file_binary:
            case storage_backend_type::file_csv:
            case storage_backend_type::memory_buffer:
                return std::make_unique<file_storage_backend>(config);
                
            case storage_backend_type::database_sqlite:
            case storage_backend_type::database_postgresql:
            case storage_backend_type::database_mysql:
                return std::make_unique<database_storage_backend>(config);
                
            case storage_backend_type::cloud_s3:
            case storage_backend_type::cloud_gcs:
            case storage_backend_type::cloud_azure_blob:
                return std::make_unique<cloud_storage_backend>(config);
                
            default:
                return nullptr;
        }
    }
    
    /**
     * @brief Get supported backends
     */
    static std::vector<storage_backend_type> get_supported_backends() {
        return {
            storage_backend_type::file_json,
            storage_backend_type::file_binary,
            storage_backend_type::file_csv,
            storage_backend_type::database_sqlite,
            storage_backend_type::database_postgresql,
            storage_backend_type::database_mysql,
            storage_backend_type::cloud_s3,
            storage_backend_type::cloud_gcs,
            storage_backend_type::cloud_azure_blob,
            storage_backend_type::memory_buffer
        };
    }
};

/**
 * @brief Helper function to create a file storage backend
 */
inline std::unique_ptr<file_storage_backend> create_file_storage(
    const std::string& path,
    storage_backend_type type = storage_backend_type::file_json,
    std::size_t capacity = 10000) {
    
    storage_config config;
    config.type = type;
    config.path = path;
    config.max_capacity = capacity;
    return std::make_unique<file_storage_backend>(config);
}

/**
 * @brief Helper function to create a database storage backend
 */
inline std::unique_ptr<database_storage_backend> create_database_storage(
    storage_backend_type type,
    const std::string& connection_params,
    const std::string& table_name = "metrics_snapshots") {
    
    storage_config config;
    config.type = type;
    config.path = connection_params;
    config.table_name = table_name;
    return std::make_unique<database_storage_backend>(config);
}

/**
 * @brief Helper function to create a cloud storage backend
 */
inline std::unique_ptr<cloud_storage_backend> create_cloud_storage(
    storage_backend_type type,
    const std::string& bucket_name) {
    
    storage_config config;
    config.type = type;
    config.path = bucket_name;
    return std::make_unique<cloud_storage_backend>(config);
}

} // namespace monitoring_system