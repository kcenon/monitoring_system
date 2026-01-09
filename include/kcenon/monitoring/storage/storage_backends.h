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

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <deque>
#include <algorithm>

#include "kcenon/monitoring/core/result_types.h"
#include "kcenon/monitoring/interfaces/monitoring_core.h"

namespace kcenon::monitoring {

/**
 * @brief Storage backend types
 */
enum class storage_backend_type {
    memory_buffer,
    file_json,
    file_binary,
    file_csv,
    database_sqlite,
    database_postgresql,
    database_mysql,
    cloud_s3,
    cloud_gcs,
    cloud_azure_blob
};

/**
 * @brief Compression algorithms
 */
enum class compression_algorithm {
    none,
    gzip,
    lz4,
    zstd
};

/**
 * @brief Storage configuration
 */
struct storage_config {
    storage_backend_type type{storage_backend_type::memory_buffer};
    std::string path;
    std::string data_directory;
    compression_algorithm compression{compression_algorithm::none};
    size_t max_size_mb{100};
    bool auto_flush{true};
    std::chrono::milliseconds flush_interval{std::chrono::milliseconds(5000)};

    // Extended configuration for Phase 2
    size_t max_capacity{1000};
    size_t batch_size{100};
    std::string table_name;
    std::string host;
    uint16_t port{0};
    std::string database_name;
    std::string username;
    std::string password;

    /**
     * @brief Validate configuration
     * @return result<bool> indicating validation success or failure
     */
    result<bool> validate() const {
        // Memory buffer doesn't require path
        if (type != storage_backend_type::memory_buffer) {
            if (path.empty() && host.empty()) {
                return make_error<bool>(monitoring_error_code::invalid_configuration,
                                       "Path or host required for non-memory storage");
            }
        }

        if (max_capacity == 0) {
            return make_error<bool>(monitoring_error_code::invalid_capacity,
                                   "Capacity must be greater than 0");
        }

        if (batch_size == 0) {
            return make_error<bool>(monitoring_error_code::invalid_configuration,
                                   "Batch size must be greater than 0");
        }

        if (batch_size > max_capacity) {
            return make_error<bool>(monitoring_error_code::invalid_configuration,
                                   "Batch size cannot exceed capacity");
        }

        return make_success(true);
    }
};

/**
 * @brief Base interface for snapshot storage backends
 */
class snapshot_storage_backend {
public:
    virtual ~snapshot_storage_backend() = default;

    virtual result<bool> store(const metrics_snapshot& snapshot) = 0;
    virtual result<metrics_snapshot> retrieve(size_t index) = 0;
    virtual result<std::vector<metrics_snapshot>> retrieve_range(size_t start, size_t count) = 0;
    virtual size_t size() const = 0;
    virtual size_t capacity() const = 0;
    virtual result<bool> flush() = 0;
    virtual result<bool> clear() = 0;
    virtual std::unordered_map<std::string, size_t> get_stats() const = 0;
};

/**
 * @brief File storage backend for metrics snapshots
 */
class file_storage_backend : public snapshot_storage_backend {
public:
    file_storage_backend() : config_() {}

    explicit file_storage_backend(const storage_config& config)
        : config_(config) {}

    result<bool> store(const metrics_snapshot& snapshot) override {
        std::lock_guard<std::mutex> lock(mutex_);

        // Remove oldest if at capacity
        if (snapshots_.size() >= config_.max_capacity) {
            snapshots_.pop_front();
        }

        snapshots_.push_back(snapshot);
        return make_success(true);
    }

    result<metrics_snapshot> retrieve(size_t index) override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (index >= snapshots_.size()) {
            return make_error<metrics_snapshot>(monitoring_error_code::not_found,
                                               "Snapshot index out of range");
        }

        return make_success(snapshots_[index]);
    }

    result<std::vector<metrics_snapshot>> retrieve_range(size_t start, size_t count) override {
        std::lock_guard<std::mutex> lock(mutex_);

        std::vector<metrics_snapshot> result;
        size_t end = std::min(start + count, snapshots_.size());

        for (size_t i = start; i < end; ++i) {
            result.push_back(snapshots_[i]);
        }

        return make_success(std::move(result));
    }

    size_t size() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return snapshots_.size();
    }

    size_t capacity() const override {
        return config_.max_capacity;
    }

    result<bool> flush() override {
        // Stub implementation - actual file I/O would go here
        return make_success(true);
    }

    result<bool> clear() override {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshots_.clear();
        return make_success(true);
    }

    std::unordered_map<std::string, size_t> get_stats() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return {
            {"total_snapshots", snapshots_.size()},
            {"capacity", config_.max_capacity}
        };
    }

private:
    storage_config config_;
    std::deque<metrics_snapshot> snapshots_;
    mutable std::mutex mutex_;
};

/**
 * @brief Database storage backend (stub implementation)
 */
class database_storage_backend : public snapshot_storage_backend {
public:
    database_storage_backend() : config_() {}

    explicit database_storage_backend(const storage_config& config)
        : config_(config), connected_(true) {}

    result<bool> store(const metrics_snapshot& snapshot) override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (snapshots_.size() >= config_.max_capacity) {
            snapshots_.pop_front();
        }

        snapshots_.push_back(snapshot);
        return make_success(true);
    }

    result<metrics_snapshot> retrieve(size_t index) override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (index >= snapshots_.size()) {
            return make_error<metrics_snapshot>(monitoring_error_code::not_found,
                                               "Snapshot index out of range");
        }

        return make_success(snapshots_[index]);
    }

    result<std::vector<metrics_snapshot>> retrieve_range(size_t start, size_t count) override {
        std::lock_guard<std::mutex> lock(mutex_);

        std::vector<metrics_snapshot> result;
        size_t end = std::min(start + count, snapshots_.size());

        for (size_t i = start; i < end; ++i) {
            result.push_back(snapshots_[i]);
        }

        return make_success(std::move(result));
    }

    size_t size() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return snapshots_.size();
    }

    size_t capacity() const override {
        return config_.max_capacity;
    }

    result<bool> flush() override {
        return make_success(true);
    }

    result<bool> clear() override {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshots_.clear();
        return make_success(true);
    }

    std::unordered_map<std::string, size_t> get_stats() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return {
            {"stored_count", snapshots_.size()},
            {"capacity", config_.max_capacity},
            {"connected", connected_ ? 1UL : 0UL}
        };
    }

private:
    storage_config config_;
    std::deque<metrics_snapshot> snapshots_;
    mutable std::mutex mutex_;
    bool connected_{false};
};

/**
 * @brief Cloud storage backend (stub implementation)
 */
class cloud_storage_backend : public snapshot_storage_backend {
public:
    cloud_storage_backend() : config_() {}

    explicit cloud_storage_backend(const storage_config& config)
        : config_(config) {}

    result<bool> store(const metrics_snapshot& snapshot) override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (snapshots_.size() >= config_.max_capacity) {
            snapshots_.pop_front();
        }

        snapshots_.push_back(snapshot);
        return make_success(true);
    }

    result<metrics_snapshot> retrieve(size_t index) override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (index >= snapshots_.size()) {
            return make_error<metrics_snapshot>(monitoring_error_code::not_found,
                                               "Snapshot index out of range");
        }

        return make_success(snapshots_[index]);
    }

    result<std::vector<metrics_snapshot>> retrieve_range(size_t start, size_t count) override {
        std::lock_guard<std::mutex> lock(mutex_);

        std::vector<metrics_snapshot> result;
        size_t end = std::min(start + count, snapshots_.size());

        for (size_t i = start; i < end; ++i) {
            result.push_back(snapshots_[i]);
        }

        return make_success(std::move(result));
    }

    size_t size() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return snapshots_.size();
    }

    size_t capacity() const override {
        return config_.max_capacity;
    }

    result<bool> flush() override {
        return make_success(true);
    }

    result<bool> clear() override {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshots_.clear();
        return make_success(true);
    }

    std::unordered_map<std::string, size_t> get_stats() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return {
            {"stored_count", snapshots_.size()},
            {"capacity", config_.max_capacity}
        };
    }

private:
    storage_config config_;
    std::deque<metrics_snapshot> snapshots_;
    mutable std::mutex mutex_;
};

/**
 * @brief In-memory snapshot storage backend
 */
class memory_storage_backend : public snapshot_storage_backend {
public:
    memory_storage_backend() : config_() {}

    explicit memory_storage_backend(const storage_config& config)
        : config_(config) {}

    result<bool> store(const metrics_snapshot& snapshot) override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (snapshots_.size() >= config_.max_capacity) {
            snapshots_.pop_front();
        }

        snapshots_.push_back(snapshot);
        return make_success(true);
    }

    result<metrics_snapshot> retrieve(size_t index) override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (index >= snapshots_.size()) {
            return make_error<metrics_snapshot>(monitoring_error_code::not_found,
                                               "Snapshot index out of range");
        }

        return make_success(snapshots_[index]);
    }

    result<std::vector<metrics_snapshot>> retrieve_range(size_t start, size_t count) override {
        std::lock_guard<std::mutex> lock(mutex_);

        std::vector<metrics_snapshot> result;
        size_t end = std::min(start + count, snapshots_.size());

        for (size_t i = start; i < end; ++i) {
            result.push_back(snapshots_[i]);
        }

        return make_success(std::move(result));
    }

    size_t size() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return snapshots_.size();
    }

    size_t capacity() const override {
        return config_.max_capacity;
    }

    result<bool> flush() override {
        return make_success(true);
    }

    result<bool> clear() override {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshots_.clear();
        return make_success(true);
    }

    std::unordered_map<std::string, size_t> get_stats() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return {
            {"stored_count", snapshots_.size()},
            {"capacity", config_.max_capacity}
        };
    }

private:
    storage_config config_;
    std::deque<metrics_snapshot> snapshots_;
    mutable std::mutex mutex_;
};

/**
 * @brief Factory for creating storage backends
 */
class storage_backend_factory {
public:
    /**
     * @brief Create a storage backend based on configuration
     * @param config Storage configuration
     * @return Unique pointer to storage backend, nullptr on failure
     */
    static std::unique_ptr<snapshot_storage_backend> create_backend(const storage_config& config) {
        switch (config.type) {
            case storage_backend_type::memory_buffer:
                return std::make_unique<memory_storage_backend>(config);

            case storage_backend_type::file_json:
            case storage_backend_type::file_binary:
            case storage_backend_type::file_csv:
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
     * @brief Get list of supported backend types
     * @return Vector of supported storage backend types
     */
    static std::vector<storage_backend_type> get_supported_backends() {
        return {
            storage_backend_type::memory_buffer,
            storage_backend_type::file_json,
            storage_backend_type::file_binary,
            storage_backend_type::file_csv,
            storage_backend_type::database_sqlite,
            storage_backend_type::database_postgresql,
            storage_backend_type::database_mysql,
            storage_backend_type::cloud_s3,
            storage_backend_type::cloud_gcs,
            storage_backend_type::cloud_azure_blob
        };
    }
};

// Helper functions

/**
 * @brief Create a file storage backend
 * @param path File path
 * @param type Storage type (file_json, file_binary, file_csv)
 * @param capacity Maximum capacity
 * @return Unique pointer to file storage backend
 */
inline std::unique_ptr<snapshot_storage_backend> create_file_storage(
    const std::string& path,
    storage_backend_type type,
    size_t capacity) {

    storage_config config;
    config.type = type;
    config.path = path;
    config.max_capacity = capacity;

    return storage_backend_factory::create_backend(config);
}

/**
 * @brief Create a database storage backend
 * @param type Database type (database_sqlite, database_postgresql, database_mysql)
 * @param path Database path or connection string
 * @param table Table name
 * @return Unique pointer to database storage backend
 */
inline std::unique_ptr<snapshot_storage_backend> create_database_storage(
    storage_backend_type type,
    const std::string& path,
    const std::string& table) {

    storage_config config;
    config.type = type;
    config.path = path;
    config.table_name = table;
    config.max_capacity = 10000;

    return storage_backend_factory::create_backend(config);
}

/**
 * @brief Create a cloud storage backend
 * @param type Cloud type (cloud_s3, cloud_gcs, cloud_azure_blob)
 * @param bucket Bucket or container name
 * @return Unique pointer to cloud storage backend
 */
inline std::unique_ptr<snapshot_storage_backend> create_cloud_storage(
    storage_backend_type type,
    const std::string& bucket) {

    storage_config config;
    config.type = type;
    config.path = bucket;
    config.max_capacity = 100000;

    return storage_backend_factory::create_backend(config);
}

// Legacy key-value storage interface (for backward compatibility)

/**
 * @brief Basic key-value storage interface - stub
 */
class kv_storage_backend {
public:
    virtual ~kv_storage_backend() = default;
    virtual bool store(const std::string& key, const std::string& value) = 0;
    virtual std::string retrieve(const std::string& key) = 0;
    virtual bool remove(const std::string& key) = 0;
    virtual result<bool> flush() { return make_success(true); }
};

/**
 * @brief In-memory key-value storage backend (legacy interface)
 */
class kv_memory_storage_backend : public kv_storage_backend {
public:
    kv_memory_storage_backend() = default;

    explicit kv_memory_storage_backend(const storage_config& /* config */) {}

    bool store(const std::string& key, const std::string& value) override {
        data_[key] = value;
        return true;
    }

    std::string retrieve(const std::string& key) override {
        auto it = data_.find(key);
        return it != data_.end() ? it->second : "";
    }

    bool remove(const std::string& key) override {
        return data_.erase(key) > 0;
    }

private:
    std::unordered_map<std::string, std::string> data_;
};

} // namespace kcenon::monitoring
