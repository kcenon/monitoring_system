# Storage Backend Implementation Guide

> **Version**: 1.0.0
> **Last Updated**: 2026-02-09
> **Parent Epic**: [Documentation Gap Analysis](https://github.com/kcenon/common_system/issues/325)
> **Related Issue**: [#457](https://github.com/kcenon/monitoring_system/issues/457)

## Table of Contents

1. [Overview](#1-overview)
2. [Storage Architecture](#2-storage-architecture)
3. [Backend Comparison](#3-backend-comparison)
4. [Backend Configuration](#4-backend-configuration)
5. [Retention and Cleanup](#5-retention-and-cleanup)
6. [Custom Backend Implementation](#6-custom-backend-implementation)
7. [Configuration Examples](#7-configuration-examples)

---

## 1. Overview

The monitoring_system storage subsystem provides a pluggable backend
architecture for persisting metrics snapshots. The system uses a factory pattern
to create backends from a unified configuration struct, supporting 10 backend
types across 4 categories.

### Key Characteristics

| Aspect | Description |
|--------|-------------|
| **Dual interface** | `storage_backend` (core) and `snapshot_storage_backend` (extended) |
| **Factory pattern** | `storage_backend_factory` creates backends from `storage_config` |
| **Thread safety** | All backends use `std::mutex` for concurrent access |
| **Capacity management** | Automatic eviction of oldest entries when capacity is reached |
| **Compression support** | gzip, LZ4, and Zstandard compression algorithms |
| **Configuration validation** | `storage_config::validate()` checks constraints before use |

### Source File Map

| File | Lines | Purpose |
|------|-------|---------|
| [`storage_backends.h`](../../include/kcenon/monitoring/storage/storage_backends.h) | 608 | All backend implementations, factory, and configuration |
| [`monitoring_core.h`](../../include/kcenon/monitoring/interfaces/monitoring_core.h) | 550 | Core `storage_backend` interface and `metrics_snapshot` data model |
| [`result_types.h`](../../include/kcenon/monitoring/core/result_types.h) | — | `common::Result<T>` and `common::VoidResult` types |
| [`error_codes.h`](../../include/kcenon/monitoring/core/error_codes.h) | — | `monitoring_error_code` and `error_info` |

---

## 2. Storage Architecture

### 2.1 High-Level Data Flow

```
  Collectors             Monitoring System              Storage Layer
  ──────────            ──────────────────             ──────────────
                    ┌────────────────────────┐
  metrics_snapshot  │                        │    ┌──────────────────────┐
  ─────────────────►│  monitoring_interface   │───►│  storage_backend     │
                    │                        │    │  (core interface)    │
                    └────────────────────────┘    └──────────┬───────────┘
                                                            │
                    ┌────────────────────────────────────────┘
                    │
                    ▼
         ┌──────────────────────────────────────────────────────┐
         │           snapshot_storage_backend                    │
         │           (extended interface)                        │
         │                                                      │
         │  ┌──────────┐  ┌───────────┐  ┌──────────────────┐  │
         │  │ Memory   │  │   File    │  │    Database      │  │
         │  │ Backend  │  │  Backend  │  │    Backend       │  │
         │  └──────────┘  └───────────┘  └──────────────────┘  │
         │                                                      │
         │  ┌──────────────────────────────────────────────┐   │
         │  │              Cloud Backend                    │   │
         │  │   (S3 / GCS / Azure Blob)                    │   │
         │  └──────────────────────────────────────────────┘   │
         └──────────────────────────────────────────────────────┘
```

### 2.2 Dual Interface Design

The system provides two storage interfaces at different abstraction levels:

#### Core Interface: `storage_backend`

Defined in [`monitoring_core.h:422`](../../include/kcenon/monitoring/interfaces/monitoring_core.h),
this interface is used by `monitoring_interface` for basic storage operations:

```cpp
class storage_backend {
public:
    virtual ~storage_backend() = default;
    virtual common::VoidResult store(const metrics_snapshot& snapshot) = 0;
    virtual common::Result<metrics_snapshot> retrieve(std::size_t index) const = 0;
    virtual common::Result<std::vector<metrics_snapshot>> retrieve_range(
        std::size_t start_index, std::size_t count) const = 0;
    virtual std::size_t capacity() const = 0;
    virtual std::size_t size() const = 0;
    virtual common::VoidResult clear() = 0;
    virtual common::VoidResult flush() = 0;
};
```

#### Extended Interface: `snapshot_storage_backend`

Defined in [`storage_backends.h:125`](../../include/kcenon/monitoring/storage/storage_backends.h),
this interface adds statistics and returns `common::Result<bool>`:

```cpp
class snapshot_storage_backend {
public:
    virtual ~snapshot_storage_backend() = default;
    virtual common::Result<bool> store(const metrics_snapshot& snapshot) = 0;
    virtual common::Result<metrics_snapshot> retrieve(size_t index) = 0;
    virtual common::Result<std::vector<metrics_snapshot>> retrieve_range(
        size_t start, size_t count) = 0;
    virtual size_t size() const = 0;
    virtual size_t capacity() const = 0;
    virtual common::Result<bool> flush() = 0;
    virtual common::Result<bool> clear() = 0;
    virtual std::unordered_map<std::string, size_t> get_stats() const = 0;
};
```

**Key difference**: `snapshot_storage_backend` adds `get_stats()` for runtime
introspection and uses `Result<bool>` for write operations.

### 2.3 Data Model

Storage backends persist `metrics_snapshot` objects:

```cpp
struct metrics_snapshot {
    std::vector<metric_value> metrics;
    std::chrono::system_clock::time_point capture_time;
    std::string source_id;

    void add_metric(const std::string& name, double value);
    void add_metric(const std::string& name, double value,
                   const std::unordered_map<std::string, std::string>& tags);
    std::optional<double> get_metric(const std::string& name) const;
};

struct metric_value {
    std::string name;
    double value;
    std::chrono::system_clock::time_point timestamp;
    std::unordered_map<std::string, std::string> tags;
};
```

### 2.4 Backend Type Hierarchy

```cpp
enum class storage_backend_type {
    // Memory (no persistence)
    memory_buffer,

    // File-based (single-node persistence)
    file_json,
    file_binary,
    file_csv,

    // Database (queryable persistence)
    database_sqlite,
    database_postgresql,
    database_mysql,

    // Cloud object storage (scalable persistence)
    cloud_s3,
    cloud_gcs,
    cloud_azure_blob
};
```

The factory maps these types to implementation classes:

| Type Enum | Implementation Class |
|-----------|---------------------|
| `memory_buffer` | `memory_storage_backend` |
| `file_json`, `file_binary`, `file_csv` | `file_storage_backend` |
| `database_sqlite`, `database_postgresql`, `database_mysql` | `database_storage_backend` |
| `cloud_s3`, `cloud_gcs`, `cloud_azure_blob` | `cloud_storage_backend` |

### 2.5 Compression Support

```cpp
enum class compression_algorithm {
    none,   // No compression (default)
    gzip,   // Wide compatibility, moderate ratio
    lz4,    // Fast compression/decompression
    zstd    // Best ratio with good speed
};
```

| Algorithm | Compression Ratio | Speed | Best For |
|-----------|------------------|-------|----------|
| `none` | 1:1 | Fastest | Development, low-volume |
| `gzip` | ~5:1 | Moderate | General purpose, compatibility |
| `lz4` | ~3:1 | Very fast | High-throughput, real-time |
| `zstd` | ~6:1 | Fast | Long-term storage, archival |

---

## 3. Backend Comparison

### 3.1 Selection Matrix

| Backend | Persistence | Query Speed | Scalability | Default Capacity | Use Case |
|---------|------------|-------------|-------------|------------------|----------|
| **Memory** | No | Fastest | RAM-limited | 1,000 | Development, dashboards, short-lived data |
| **File (JSON)** | Yes | Fast | Single node | 1,000 | Simple deployments, human-readable |
| **File (Binary)** | Yes | Faster | Single node | 1,000 | Space-efficient local storage |
| **File (CSV)** | Yes | Fast | Single node | 1,000 | Data export, spreadsheet analysis |
| **SQLite** | Yes | Good | Single node | 10,000 | Embedded monitoring, lightweight persistence |
| **PostgreSQL** | Yes | Good | Horizontal | 10,000 | Production analytics, complex queries |
| **MySQL** | Yes | Good | Horizontal | 10,000 | Production, existing MySQL infrastructure |
| **S3** | Yes | Slow | Unlimited | 100,000 | Long-term archival, AWS environments |
| **GCS** | Yes | Slow | Unlimited | 100,000 | GCP environments, BigQuery integration |
| **Azure Blob** | Yes | Slow | Unlimited | 100,000 | Azure environments, Data Lake integration |

### 3.2 Decision Guide

```
Need persistent storage?
├─ NO → Memory Backend
│       (fastest, no disk I/O, data lost on restart)
│
└─ YES → Need SQL queries?
         ├─ YES → Need horizontal scaling?
         │        ├─ YES → PostgreSQL or MySQL
         │        └─ NO  → SQLite (embedded, zero-config)
         │
         └─ NO → Need cross-region/unlimited scale?
                  ├─ YES → Cloud Backend (S3/GCS/Azure)
                  │        └─ Choose based on cloud provider
                  └─ NO  → File Backend
                           ├─ Need human-readable? → JSON
                           ├─ Need compact storage? → Binary
                           └─ Need data export?     → CSV
```

### 3.3 Feature Comparison

| Feature | Memory | File | Database | Cloud |
|---------|--------|------|----------|-------|
| Thread-safe | Yes | Yes | Yes | Yes |
| Zero-config | Yes | Path only | Connection | Credentials |
| Atomic writes | N/A | No | Yes | Eventual |
| Range queries | Yes | Yes | Yes (optimized) | Yes |
| Statistics | Yes | Yes | Yes (+connected) | Yes |
| Auto-eviction | Yes | Yes | Yes | Yes |
| Batch write | Implicit | Config | Config | Config |
| Compression | In-memory | On-disk | DB-native | Transfer |

---

## 4. Backend Configuration

### 4.1 Configuration Struct

All backends are configured through `storage_config`:

```cpp
struct storage_config {
    // Backend selection
    storage_backend_type type{storage_backend_type::memory_buffer};

    // File/Path settings
    std::string path;               // File path or connection string
    std::string data_directory;     // Directory for data files

    // Compression
    compression_algorithm compression{compression_algorithm::none};

    // Size limits
    size_t max_size_mb{100};        // Maximum storage size in MB
    size_t max_capacity{1000};      // Maximum number of snapshots

    // Flush behavior
    bool auto_flush{true};
    std::chrono::milliseconds flush_interval{5000};  // 5 seconds

    // Batching
    size_t batch_size{100};

    // Database settings
    std::string table_name;
    std::string host;
    uint16_t port{0};
    std::string database_name;
    std::string username;
    std::string password;

    common::Result<bool> validate() const;
};
```

### 4.2 Configuration Validation

The `validate()` method enforces:

| Rule | Error Code | Description |
|------|-----------|-------------|
| Non-memory needs path or host | `invalid_configuration` | File/DB/Cloud require a target |
| Capacity > 0 | `invalid_capacity` | Must store at least 1 snapshot |
| Batch size > 0 | `invalid_configuration` | Batch must be positive |
| Batch size <= capacity | `invalid_configuration` | Cannot batch more than capacity |

```cpp
storage_config config;
config.type = storage_backend_type::database_postgresql;
// config.host is empty, config.path is empty

auto result = config.validate();
// result.is_err() == true
// Error: "Path or host required for non-memory storage"
```

### 4.3 Memory Backend

No external configuration required. Data is stored in an in-memory `std::deque`.

```cpp
storage_config config;
config.type = storage_backend_type::memory_buffer;
config.max_capacity = 5000;  // Keep last 5000 snapshots

auto backend = storage_backend_factory::create_backend(config);
```

**Statistics returned**:
```cpp
{"stored_count": N, "capacity": max_capacity}
```

### 4.4 File Backends

Three file formats are supported, all using the same `file_storage_backend`
implementation class:

| Format | Type Enum | Extension | Use Case |
|--------|-----------|-----------|----------|
| JSON | `file_json` | `.json` | Human-readable, debugging |
| Binary | `file_binary` | `.bin` | Compact storage |
| CSV | `file_csv` | `.csv` | Spreadsheet export |

```cpp
// JSON file storage
storage_config config;
config.type = storage_backend_type::file_json;
config.path = "/var/monitoring/metrics.json";
config.max_capacity = 10000;
config.compression = compression_algorithm::gzip;
config.auto_flush = true;
config.flush_interval = std::chrono::milliseconds(10000);

auto backend = storage_backend_factory::create_backend(config);
```

**Helper function**:

```cpp
auto backend = create_file_storage(
    "/var/monitoring/metrics.json",  // File path
    storage_backend_type::file_json, // Format
    10000                            // Max capacity
);
```

**Statistics returned**:
```cpp
{"total_snapshots": N, "capacity": max_capacity}
```

### 4.5 Database Backends

Three database engines are supported via the `database_storage_backend` class:

#### SQLite

```cpp
storage_config config;
config.type = storage_backend_type::database_sqlite;
config.path = "/var/monitoring/metrics.db";
config.table_name = "metrics_snapshots";
config.max_capacity = 100000;

auto backend = storage_backend_factory::create_backend(config);
```

#### PostgreSQL

```cpp
storage_config config;
config.type = storage_backend_type::database_postgresql;
config.host = "db.internal";
config.port = 5432;
config.database_name = "monitoring";
config.username = "monitor_user";
config.password = get_from_secret_manager("db_password");
config.table_name = "metrics_snapshots";
config.max_capacity = 500000;
config.batch_size = 500;

auto backend = storage_backend_factory::create_backend(config);
```

#### MySQL

```cpp
storage_config config;
config.type = storage_backend_type::database_mysql;
config.host = "mysql.internal";
config.port = 3306;
config.database_name = "monitoring";
config.username = "monitor_user";
config.password = get_from_secret_manager("db_password");
config.table_name = "metrics_snapshots";
config.max_capacity = 500000;

auto backend = storage_backend_factory::create_backend(config);
```

**Helper function**:

```cpp
auto backend = create_database_storage(
    storage_backend_type::database_postgresql,
    "db.internal",             // Path/host
    "metrics_snapshots"        // Table name
);
// Default capacity: 10,000
```

**Statistics returned**:
```cpp
{"stored_count": N, "capacity": max_capacity, "connected": 0|1}
```

### 4.6 Cloud Backends

Three cloud object storage providers are supported via the
`cloud_storage_backend` class:

#### Amazon S3

```cpp
storage_config config;
config.type = storage_backend_type::cloud_s3;
config.path = "my-monitoring-bucket";
config.data_directory = "metrics/production/";
config.max_capacity = 1000000;
config.compression = compression_algorithm::zstd;
config.batch_size = 1000;

auto backend = storage_backend_factory::create_backend(config);
```

#### Google Cloud Storage (GCS)

```cpp
storage_config config;
config.type = storage_backend_type::cloud_gcs;
config.path = "my-monitoring-bucket";
config.data_directory = "metrics/production/";
config.max_capacity = 1000000;

auto backend = storage_backend_factory::create_backend(config);
```

#### Azure Blob Storage

```cpp
storage_config config;
config.type = storage_backend_type::cloud_azure_blob;
config.path = "monitoring-container";
config.data_directory = "metrics/production/";
config.max_capacity = 1000000;

auto backend = storage_backend_factory::create_backend(config);
```

**Helper function**:

```cpp
auto backend = create_cloud_storage(
    storage_backend_type::cloud_s3,
    "my-monitoring-bucket"     // Bucket/container name
);
// Default capacity: 100,000
```

**Statistics returned**:
```cpp
{"stored_count": N, "capacity": max_capacity}
```

---

## 5. Retention and Cleanup

### 5.1 Automatic Capacity Management

All backends implement automatic eviction using a deque-based FIFO strategy.
When `max_capacity` is reached, the oldest snapshot is removed:

```cpp
common::Result<bool> store(const metrics_snapshot& snapshot) override {
    std::lock_guard<std::mutex> lock(mutex_);

    // Evict oldest if at capacity
    if (snapshots_.size() >= config_.max_capacity) {
        snapshots_.pop_front();
    }

    snapshots_.push_back(snapshot);
    return common::ok(true);
}
```

This means:
- Storage never exceeds `max_capacity` snapshots
- Oldest data is automatically discarded
- No manual cleanup needed for capacity management

### 5.2 Capacity Sizing Guidelines

| Scenario | Collection Interval | Retention | Recommended Capacity |
|----------|-------------------|-----------|---------------------|
| Dashboard (real-time) | 1s | 15 min | 900 |
| Development | 5s | 1 hour | 720 |
| Production (standard) | 10s | 24 hours | 8,640 |
| Production (detailed) | 1s | 24 hours | 86,400 |
| Long-term analytics | 60s | 30 days | 43,200 |
| Archival | 300s | 1 year | 63,072 |

**Formula**: `capacity = (retention_seconds / collection_interval_seconds)`

### 5.3 Manual Cleanup

All backends support explicit data clearing:

```cpp
// Clear all stored data
auto result = backend->clear();
if (result.is_ok()) {
    // All snapshots removed
}

// Flush buffered data to persistent storage
auto flush_result = backend->flush();
```

### 5.4 Flush Behavior

The `auto_flush` and `flush_interval` settings control when buffered data is
written to persistent storage:

```cpp
storage_config config;
config.auto_flush = true;                              // Enable auto-flush
config.flush_interval = std::chrono::milliseconds(5000); // Every 5 seconds
```

| `auto_flush` | `flush_interval` | Behavior |
|--------------|------------------|----------|
| `true` | 5000ms | Automatic flush every 5 seconds |
| `true` | 0ms | Flush after every store operation |
| `false` | — | Manual flush only (call `flush()` explicitly) |

### 5.5 Compression and Size Management

For file and cloud backends, compression reduces storage footprint:

```cpp
storage_config config;
config.type = storage_backend_type::file_json;
config.compression = compression_algorithm::zstd;  // Best ratio
config.max_size_mb = 500;                          // 500 MB limit
```

Compression is applied per-snapshot on write and decompressed on read.

---

## 6. Custom Backend Implementation

### 6.1 Choosing the Interface

| Interface | Use When |
|-----------|----------|
| `snapshot_storage_backend` | Building a concrete backend with statistics |
| `storage_backend` | Integrating with `monitoring_interface` |
| `kv_storage_backend` | Legacy key-value use case |

### 6.2 Implementing `snapshot_storage_backend`

Here is a complete example of a Redis-backed storage backend:

```cpp
#include <kcenon/monitoring/storage/storage_backends.h>
#include <mutex>

class redis_storage_backend : public snapshot_storage_backend {
private:
    storage_config config_;
    std::deque<metrics_snapshot> cache_;  // Local cache
    mutable std::mutex mutex_;
    bool connected_{false};
    size_t total_stored_{0};
    size_t store_failures_{0};

public:
    explicit redis_storage_backend(const storage_config& config)
        : config_(config) {
        // Connect to Redis using config_.host and config_.port
        connected_ = true;  // Simplified
    }

    common::Result<bool> store(const metrics_snapshot& snapshot) override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!connected_) {
            store_failures_++;
            return common::Result<bool>::err(error_info(
                monitoring_error_code::network_error,
                "Not connected to Redis"
            ).to_common_error());
        }

        // Evict oldest if at capacity
        if (cache_.size() >= config_.max_capacity) {
            cache_.pop_front();
        }

        cache_.push_back(snapshot);
        total_stored_++;

        // In real implementation: serialize and LPUSH to Redis
        return common::ok(true);
    }

    common::Result<metrics_snapshot> retrieve(size_t index) override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (index >= cache_.size()) {
            return common::Result<metrics_snapshot>::err(error_info(
                monitoring_error_code::not_found,
                "Snapshot index out of range"
            ).to_common_error());
        }

        return common::ok(cache_[index]);
    }

    common::Result<std::vector<metrics_snapshot>> retrieve_range(
        size_t start, size_t count) override {
        std::lock_guard<std::mutex> lock(mutex_);

        std::vector<metrics_snapshot> result;
        size_t end = std::min(start + count, cache_.size());

        for (size_t i = start; i < end; ++i) {
            result.push_back(cache_[i]);
        }

        return common::ok(std::move(result));
    }

    size_t size() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return cache_.size();
    }

    size_t capacity() const override {
        return config_.max_capacity;
    }

    common::Result<bool> flush() override {
        // In real implementation: ensure all cached data is in Redis
        return common::ok(true);
    }

    common::Result<bool> clear() override {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_.clear();
        // In real implementation: DEL the Redis key
        return common::ok(true);
    }

    std::unordered_map<std::string, size_t> get_stats() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return {
            {"stored_count", cache_.size()},
            {"capacity", config_.max_capacity},
            {"total_stored", total_stored_},
            {"store_failures", store_failures_},
            {"connected", connected_ ? 1UL : 0UL}
        };
    }
};
```

### 6.3 Implementing `storage_backend` (Core Interface)

For integration with `monitoring_interface`:

```cpp
#include <kcenon/monitoring/interfaces/monitoring_core.h>

class redis_core_storage : public storage_backend {
private:
    std::deque<metrics_snapshot> data_;
    mutable std::mutex mutex_;
    std::size_t capacity_{10000};

public:
    explicit redis_core_storage(std::size_t capacity = 10000)
        : capacity_(capacity) {}

    common::VoidResult store(const metrics_snapshot& snapshot) override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (data_.size() >= capacity_) {
            data_.pop_front();
        }
        data_.push_back(snapshot);
        return common::ok();
    }

    common::Result<metrics_snapshot> retrieve(std::size_t index) const override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (index >= data_.size()) {
            error_info err(monitoring_error_code::not_found, "Index out of range");
            return common::Result<metrics_snapshot>::err(err.to_common_error());
        }
        return common::ok(data_[index]);
    }

    common::Result<std::vector<metrics_snapshot>> retrieve_range(
        std::size_t start_index, std::size_t count) const override {
        std::lock_guard<std::mutex> lock(mutex_);

        std::vector<metrics_snapshot> result;
        std::size_t end = std::min(start_index + count, data_.size());

        for (std::size_t i = start_index; i < end; ++i) {
            result.push_back(data_[i]);
        }
        return common::ok(std::move(result));
    }

    std::size_t capacity() const override { return capacity_; }
    std::size_t size() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return data_.size();
    }

    common::VoidResult clear() override {
        std::lock_guard<std::mutex> lock(mutex_);
        data_.clear();
        return common::ok();
    }

    common::VoidResult flush() override {
        return common::ok();
    }
};

// Usage with monitoring_interface
monitor->set_storage_backend(std::make_unique<redis_core_storage>(50000));
```

### 6.4 Registering with the Factory

To add a custom backend to the factory, extend the `create_backend` method
or create a wrapper:

```cpp
std::unique_ptr<snapshot_storage_backend> create_extended_backend(
    const storage_config& config) {

    // Handle custom types first
    if (config.host.find("redis://") == 0) {
        return std::make_unique<redis_storage_backend>(config);
    }

    // Fall back to built-in factory
    return storage_backend_factory::create_backend(config);
}
```

### 6.5 Legacy Key-Value Interface

For backward compatibility, a simple key-value storage interface is provided:

```cpp
class kv_storage_backend {
public:
    virtual ~kv_storage_backend() = default;
    virtual bool store(const std::string& key, const std::string& value) = 0;
    virtual std::string retrieve(const std::string& key) = 0;
    virtual bool remove(const std::string& key) = 0;
    virtual common::Result<bool> flush() { return common::ok(true); }
};
```

The built-in `kv_memory_storage_backend` provides an in-memory hash map
implementation. This interface is available for use cases that need simple
key-value semantics rather than time-series snapshot storage.

### 6.6 Best Practices

| Practice | Rationale |
|----------|-----------|
| **Thread safety** | Use `std::lock_guard<std::mutex>` in all methods |
| **FIFO eviction** | Pop from front of deque when capacity is reached |
| **Error propagation** | Return `common::Result<T>` with descriptive `error_info` |
| **Statistics** | Track `stored_count`, `capacity`, and backend-specific counters |
| **Validate config** | Call `config.validate()` in constructor |
| **Flush on shutdown** | Ensure `flush()` persists all buffered data |
| **Bounds checking** | Validate index parameters in `retrieve()` |

---

## 7. Configuration Examples

### 7.1 Production: Tiered Storage

Use multiple backends for different retention windows:

```cpp
#include <kcenon/monitoring/storage/storage_backends.h>

// Hot tier: in-memory for real-time dashboards (last 15 minutes)
storage_config hot_config;
hot_config.type = storage_backend_type::memory_buffer;
hot_config.max_capacity = 900;  // 1s interval × 900 = 15 min
auto hot_storage = storage_backend_factory::create_backend(hot_config);

// Warm tier: SQLite for recent queries (last 24 hours)
storage_config warm_config;
warm_config.type = storage_backend_type::database_sqlite;
warm_config.path = "/var/monitoring/warm_metrics.db";
warm_config.table_name = "metrics_24h";
warm_config.max_capacity = 86400;  // 1s interval × 86400 = 24 hours
warm_config.compression = compression_algorithm::lz4;
auto warm_storage = storage_backend_factory::create_backend(warm_config);

// Cold tier: S3 for long-term archival (last 1 year)
storage_config cold_config;
cold_config.type = storage_backend_type::cloud_s3;
cold_config.path = "monitoring-archive-bucket";
cold_config.data_directory = "production/metrics/";
cold_config.max_capacity = 1000000;
cold_config.compression = compression_algorithm::zstd;
cold_config.batch_size = 5000;
auto cold_storage = storage_backend_factory::create_backend(cold_config);

// Store in all tiers
hot_storage->store(snapshot);
warm_storage->store(snapshot);
cold_storage->store(snapshot);
```

### 7.2 Development: Simple File Storage

Minimal configuration for local development:

```cpp
#include <kcenon/monitoring/storage/storage_backends.h>

// JSON file for easy inspection
auto storage = create_file_storage(
    "/tmp/dev_metrics.json",
    storage_backend_type::file_json,
    1000
);

storage->store(snapshot);

// Read back
auto result = storage->retrieve(0);
if (result.is_ok()) {
    auto& s = result.value();
    // Inspect snapshot
}

// Check stats
auto stats = storage->get_stats();
// stats["total_snapshots"] == 1
// stats["capacity"] == 1000
```

### 7.3 PostgreSQL with Full Configuration

Production database backend with all options:

```cpp
storage_config config;
config.type = storage_backend_type::database_postgresql;
config.host = "pg-primary.internal";
config.port = 5432;
config.database_name = "metrics_db";
config.username = "metrics_writer";
config.password = get_secret("pg_password");
config.table_name = "metrics_snapshots";
config.max_capacity = 500000;
config.batch_size = 500;
config.max_size_mb = 10240;    // 10 GB
config.auto_flush = true;
config.flush_interval = std::chrono::milliseconds(2000);
config.compression = compression_algorithm::lz4;

// Validate before use
auto validation = config.validate();
if (validation.is_err()) {
    std::cerr << "Config error: " << validation.error().message << "\n";
    return;
}

auto backend = storage_backend_factory::create_backend(config);

// Store metrics
backend->store(snapshot);

// Query range
auto range = backend->retrieve_range(0, 100);
if (range.is_ok()) {
    for (const auto& s : range.value()) {
        // Process snapshots
    }
}

// Check connection status
auto stats = backend->get_stats();
bool connected = stats["connected"] > 0;
```

### 7.4 Cloud Storage with Compression

Azure Blob storage for compliance archival:

```cpp
storage_config config;
config.type = storage_backend_type::cloud_azure_blob;
config.path = "compliance-monitoring-container";
config.data_directory = "audit/metrics/2026/";
config.max_capacity = 500000;
config.batch_size = 2000;
config.compression = compression_algorithm::zstd;
config.auto_flush = true;
config.flush_interval = std::chrono::milliseconds(30000);  // 30s

auto backend = storage_backend_factory::create_backend(config);
backend->store(snapshot);
backend->flush();  // Ensure data is persisted
```

---

## Appendix A: Backend Quick Reference

### Factory Usage

```cpp
// Using factory
auto backend = storage_backend_factory::create_backend(config);

// Using helper functions
auto file    = create_file_storage(path, type, capacity);
auto db      = create_database_storage(type, path, table);
auto cloud   = create_cloud_storage(type, bucket);
```

### Supported Backend Types

```cpp
auto supported = storage_backend_factory::get_supported_backends();
// Returns all 10 storage_backend_type values
```

## Appendix B: Error Codes

| Error Code | Constant | Meaning |
|-----------|----------|---------|
| Configuration error | `monitoring_error_code::invalid_configuration` | Invalid config values |
| Capacity error | `monitoring_error_code::invalid_capacity` | Capacity must be > 0 |
| Not found | `monitoring_error_code::not_found` | Index out of range |
| Network error | `monitoring_error_code::network_error` | Connection failure |

## Appendix C: File Reference

All source files referenced in this guide:

| File | Path |
|------|------|
| `storage_backends.h` | `include/kcenon/monitoring/storage/storage_backends.h` |
| `monitoring_core.h` | `include/kcenon/monitoring/interfaces/monitoring_core.h` |
| `result_types.h` | `include/kcenon/monitoring/core/result_types.h` |
| `error_codes.h` | `include/kcenon/monitoring/core/error_codes.h` |

---

*This guide is part of the monitoring_system documentation suite.*
*See also: [Collector Development Guide](COLLECTOR_DEVELOPMENT.md) | [Exporter Development Guide](EXPORTER_DEVELOPMENT.md) | [Architecture](../ARCHITECTURE.md)*
