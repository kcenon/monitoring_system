# Monitoring System Design Document

## Executive Summary

This document outlines the comprehensive design and implementation plan for a unified monitoring system that integrates with both thread_system and logger_system. The monitoring system adopts thread_system's modern C++20 architecture patterns including the Result pattern for error handling, dependency injection, and lock-free data structures, while providing centralized metrics collection, health checks, and performance monitoring capabilities.

## System Overview

### Purpose
The monitoring_system serves as a centralized hub for collecting, processing, and exposing metrics from various components:
- Thread pool performance metrics
- Logger system metrics 
- System resource utilization
- Application health status
- Custom business metrics
- Real-time alerting and analysis

### Core Design Principles
1. **Result Pattern Error Handling**: Explicit error handling without exceptions using thread_system's Result pattern
2. **Dependency Injection**: Service container integration for loose coupling and testability
3. **Interface Segregation**: Clean abstractions with no external dependencies
4. **Pluggable Architecture**: Support for multiple monitoring backends
5. **Zero-cost Abstraction**: Minimal overhead when disabled
6. **Thread-safe Operations**: Lock-free metrics collection where possible
7. **Adaptive Sampling**: Dynamic adjustment based on system load
8. **SIMD Optimization**: Hardware-accelerated aggregations where supported

## Gap Analysis

| Area | Current State | Target State | Priority |
|------|--------------|--------------|----------|
| Error Handling | Basic bool/exceptions | Result pattern with typed errors | HIGH |
| Dependency Management | Direct instantiation | Service container with DI | HIGH |
| Interface Design | Basic interfaces | Thread_system aligned interfaces | HIGH |
| Storage Backend | Fixed ring buffer | Pluggable storage strategies | MEDIUM |
| Performance | Good but static | Adaptive with lock-free options | MEDIUM |
| Testing | Basic unit tests | Comprehensive test suite | HIGH |
| Integration | Standalone | Full thread_system integration | HIGH |

## Master Task Checklist

### 🎯 Phase 1: Core Architecture Alignment [Week 1-2]
- [x] **[A1]** Adopt thread_system's `result<T>` pattern for error handling ✅ **COMPLETED 2025-09-10**
  - Implemented result<T> and result_void classes with monadic operations
  - Added comprehensive error_info with source location tracking
  - Created helper functions and macros for error propagation
- [x] **[A2]** Define `monitoring_error_code` enum with comprehensive error categories ✅ **COMPLETED 2025-09-10**
  - Defined 30+ error codes across 7 categories
  - Added error_code_to_string and get_error_details functions
- [x] **[A3]** Integrate with thread_system's `service_container` for DI ✅ **COMPLETED 2025-09-10**
  - Implemented service_container_interface abstract interface
  - Created lightweight_container with thread-safe registration and resolution
  - Added thread_system_container_adapter for optional integration
  - Implemented service lifetimes (transient, scoped, singleton)
  - Added named service registration support
- [x] **[A4]** Implement `monitorable_interface` from thread_system ✅ **COMPLETED 2025-09-10**
  - Created monitoring_data structure for metrics and tags
  - Implemented monitorable_interface abstract class
  - Added monitorable_component base class with default implementations
  - Created monitoring_aggregator for hierarchical metric collection
  - Added comprehensive unit tests (12 tests passing)
- [x] **[A5]** Add thread_context metadata enrichment ✅ **COMPLETED 2025-09-10**
  - Implemented thread-local context storage for metadata
  - Created context_scope RAII wrapper for automatic cleanup
  - Added context_propagator for cross-thread context passing
  - Implemented context_aware_monitoring interface
  - Created context_metrics_collector base class
  - Added comprehensive unit tests (13 tests passing)

### 🏗️ Phase 2: Design Patterns Implementation [Week 3-4]
- [ ] **[D1]** Create `monitoring_builder` for configuration
- [ ] **[D2]** Implement collector factory pattern
- [ ] **[D3]** Add strategy pattern for storage backends
- [ ] **[D4]** Introduce observer pattern for real-time alerts

### ⚡ Phase 3: Performance & Optimization [Week 5-6]
- [ ] **[P1]** Integrate thread_system's lock-free queues
- [ ] **[P2]** Implement zero-copy metrics passing
- [ ] **[P3]** Add SIMD optimizations for aggregations
- [ ] **[P4]** Create memory pool for metrics objects

### 🛡️ Phase 4: Reliability & Safety [Week 7-8]
- [ ] **[R1]** Implement comprehensive error recovery
- [ ] **[R2]** Add graceful degradation under load
- [ ] **[R3]** Create health check framework
- [ ] **[R4]** Implement crash-safe metrics persistence

### 🔧 Phase 5: Build & Integration [Week 9]
- [ ] **[B1]** Add CMake package configuration
- [ ] **[B2]** Create find_package support
- [ ] **[B3]** Add sanitizer configurations
- [ ] **[B4]** Implement vcpkg manifest

### 🧪 Phase 6: Testing & Quality [Week 10]
- [ ] **[T1]** Create mock collectors for testing
- [ ] **[T2]** Add property-based testing
- [ ] **[T3]** Implement load testing framework
- [ ] **[T4]** Add fuzzing tests for parsers
- [ ] **[T5]** Create integration test suite

### 📝 Phase 7: Documentation & Maintenance [Week 11-12]
- [ ] **[M1]** Generate API documentation with Doxygen
- [ ] **[M2]** Create performance tuning guide
- [ ] **[M3]** Write migration guide from v1.0
- [ ] **[M4]** Add architecture decision records

## Architecture

### Component Dependencies

```
monitoring_system
├── interfaces/              # Abstract interfaces (no dependencies)
│   ├── monitoring_interface.h
│   ├── metrics_collector.h
│   ├── health_checker.h
│   └── monitorable_interface.h  # Thread_system interface
├── core/                    # Core implementation
│   ├── unified_monitor.h    # Main monitoring coordinator
│   ├── metrics_aggregator.h # Metrics aggregation logic
│   ├── health_manager.h     # Health check coordination
│   ├── error_codes.h        # Monitoring-specific error codes
│   └── result_types.h       # Result pattern types
├── adapters/               # System-specific adapters
│   ├── thread_system_adapter.h
│   ├── logger_system_adapter.h
│   └── system_metrics_adapter.h
├── di/                     # Dependency injection
│   ├── service_container.h
│   ├── collector_factory.h
│   └── backend_factory.h
├── backends/               # Monitoring backend implementations
│   ├── basic_backend.h     # Built-in lightweight backend
│   ├── persistent_backend.h # Crash-safe persistent storage
│   ├── prometheus_backend.h # Prometheus exporter (optional)
│   └── json_backend.h      # JSON metrics exporter
├── optimization/           # Performance optimizations
│   ├── lockfree_queue.h    # Lock-free metrics queue
│   ├── memory_pool.h       # Zero-copy memory pool
│   └── simd_aggregator.h   # SIMD-accelerated aggregations
└── utils/                  # Utility components
    ├── ring_buffer.h       # Lock-free ring buffer for metrics
    ├── time_series.h       # Time-series data storage
    └── metric_types.h      # Common metric type definitions
```

## Detailed Design

### 1. Error Handling with Result Pattern

```cpp
// Define monitoring-specific error codes
enum class monitoring_error_code {
    success = 0,
    
    // Collection errors
    collector_not_found = 1000,
    collection_failed,
    collector_initialization_failed,
    
    // Storage errors
    storage_full = 2000,
    storage_corrupted,
    compression_failed,
    
    // Configuration errors
    invalid_configuration = 3000,
    invalid_interval,
    invalid_capacity,
    
    // System errors
    system_resource_unavailable = 4000,
    permission_denied,
    
    // Integration errors
    thread_system_not_available = 5000,
    incompatible_version
};

// Convert to Result pattern
template<typename T>
using result = thread_module::result<T>;
using result_void = thread_module::result_void;

class monitoring {
public:
    auto add_collector(std::unique_ptr<metrics_collector> collector) 
        -> result_void;
    
    auto get_snapshot(std::size_t index) const 
        -> result<metrics_snapshot>;
    
    auto configure(const monitoring_config& config) 
        -> result_void;
};
```

### 2. Dependency Injection Integration

```cpp
class monitoring_service_container : public thread_module::service_container {
public:
    // Register collector factories
    template<typename CollectorType>
    void register_collector_factory(const std::string& name) {
        register_factory<metrics_collector>(
            [name]() { return std::make_shared<CollectorType>(name); },
            lifetime::transient
        );
    }
    
    // Register storage backends
    template<typename StorageType>
    void register_storage_backend(const std::string& name) {
        register_factory<storage_backend>(
            [name]() { return std::make_shared<StorageType>(); },
            lifetime::singleton
        );
    }
    
    // Register analyzers
    template<typename AnalyzerType>
    void register_analyzer() {
        register_singleton<metrics_analyzer>(
            std::make_shared<AnalyzerType>()
        );
    }
};
```

### 3. Enhanced Interface Design

```cpp
// Align with thread_system interfaces
class monitoring : public thread_module::monitorable_interface,
                  public monitoring_interface::monitoring_interface {
public:
    // From monitorable_interface
    auto get_metrics() const -> result<monitoring_data> override {
        monitoring_data data;
        data.add_metric("total_collections", total_collections_.load());
        data.add_metric("active_collectors", collectors_.size());
        data.add_metric("storage_usage_percent", get_storage_usage());
        return data;
    }
    
    auto get_status() const -> result<component_status> override {
        component_status status;
        status.healthy = is_healthy();
        status.description = get_status_description();
        return status;
    }
};

// Metrics traits for type safety
template<typename T>
struct metric_traits {
    using value_type = T;
    static constexpr const char* unit = "count";
    static constexpr bool is_counter = false;
    static constexpr bool is_gauge = true;
};
```

### 4. Builder Pattern for Configuration

```cpp
class monitoring_builder {
private:
    monitoring_config config_;
    std::vector<std::unique_ptr<metrics_collector>> collectors_;
    std::unique_ptr<storage_backend> storage_;
    std::vector<std::unique_ptr<metrics_analyzer>> analyzers_;
    
public:
    monitoring_builder& with_history_size(std::size_t size) {
        config_.history_size = size;
        return *this;
    }
    
    monitoring_builder& with_collection_interval(std::chrono::milliseconds interval) {
        config_.collection_interval = interval;
        return *this;
    }
    
    monitoring_builder& add_collector(std::unique_ptr<metrics_collector> collector) {
        collectors_.push_back(std::move(collector));
        return *this;
    }
    
    monitoring_builder& with_storage(std::unique_ptr<storage_backend> storage) {
        storage_ = std::move(storage);
        return *this;
    }
    
    monitoring_builder& enable_compression(bool enable = true) {
        config_.enable_compression = enable;
        return *this;
    }
    
    auto build() -> result<std::unique_ptr<monitoring>> {
        // Validate configuration
        if (auto validation = config_.validate(); !validation) {
            return validation.error();
        }
        
        // Create monitoring instance
        auto mon = std::make_unique<monitoring>(config_);
        
        // Add collectors
        for (auto& collector : collectors_) {
            if (auto result = mon->add_collector(std::move(collector)); !result) {
                return result.error();
            }
        }
        
        return mon;
    }
};
```

### 5. Context-Aware Monitoring

```cpp
struct enriched_metrics : monitoring_interface::metrics_snapshot {
    // Thread context
    std::optional<thread_module::thread_context> thread_ctx;
    
    // Correlation ID for distributed tracing
    std::string correlation_id;
    
    // Source information
    std::string source_pool_name;
    std::size_t source_worker_id;
    
    // Tags for filtering
    std::unordered_map<std::string, std::string> tags;
    
    // Span information for tracing
    std::optional<span_info> span;
};

class context_aware_collector : public metrics_collector {
public:
    auto collect() -> result<enriched_metrics> {
        enriched_metrics metrics;
        
        // Get thread context if available
        if (auto ctx = thread_module::thread_context::current()) {
            metrics.thread_ctx = ctx;
            metrics.source_pool_name = ctx->pool_name;
            metrics.source_worker_id = ctx->worker_id;
        }
        
        // Add correlation ID
        metrics.correlation_id = generate_correlation_id();
        
        // Collect base metrics
        collect_base_metrics(metrics);
        
        return metrics;
    }
};
```

### 6. Lock-free Metrics Collection

```cpp
template<bool UseLockFree = true>
class metrics_queue {
private:
    using queue_type = std::conditional_t<UseLockFree,
        thread_module::lockfree_queue<metrics_snapshot>,
        std::queue<metrics_snapshot>>;
    
    queue_type queue_;
    std::conditional_t<UseLockFree, 
        std::atomic<size_t>, 
        size_t> size_{0};
    
public:
    auto push(metrics_snapshot&& snapshot) -> result<bool> {
        if constexpr (UseLockFree) {
            return queue_.try_push(std::move(snapshot));
        } else {
            std::lock_guard lock(mutex_);
            queue_.push(std::move(snapshot));
            ++size_;
            return true;
        }
    }
    
    auto pop() -> result<metrics_snapshot> {
        if constexpr (UseLockFree) {
            return queue_.try_pop();
        } else {
            std::lock_guard lock(mutex_);
            if (queue_.empty()) {
                return thread_module::error{
                    monitoring_error_code::storage_empty
                };
            }
            auto snapshot = std::move(queue_.front());
            queue_.pop();
            --size_;
            return snapshot;
        }
    }
};
```

### 7. Zero-Copy Optimization

```cpp
class metrics_pool {
private:
    struct pool_entry {
        alignas(64) metrics_snapshot snapshot;
        std::atomic<bool> in_use{false};
    };
    
    std::vector<pool_entry> pool_;
    std::atomic<size_t> next_index_{0};
    
public:
    auto acquire() -> metrics_snapshot* {
        size_t start = next_index_.load();
        size_t index = start;
        
        do {
            bool expected = false;
            if (pool_[index].in_use.compare_exchange_weak(expected, true)) {
                next_index_ = (index + 1) % pool_.size();
                return &pool_[index].snapshot;
            }
            index = (index + 1) % pool_.size();
        } while (index != start);
        
        return nullptr;
    }
    
    void release(metrics_snapshot* snapshot) {
        auto index = (snapshot - &pool_[0].snapshot) / sizeof(pool_entry);
        pool_[index].in_use = false;
    }
};
```

### 8. SIMD Accelerated Aggregation

```cpp
class simd_aggregator {
public:
    // AVX2 optimized mean calculation
    double calculate_mean_avx2(const std::vector<double>& values) {
        if (values.size() < 4) {
            return scalar_mean(values);
        }
        
        __m256d sum = _mm256_setzero_pd();
        size_t i = 0;
        
        // Process 4 values at a time
        for (; i + 3 < values.size(); i += 4) {
            __m256d v = _mm256_loadu_pd(&values[i]);
            sum = _mm256_add_pd(sum, v);
        }
        
        // Sum the vector elements
        double result[4];
        _mm256_storeu_pd(result, sum);
        double total = result[0] + result[1] + result[2] + result[3];
        
        // Handle remaining elements
        for (; i < values.size(); ++i) {
            total += values[i];
        }
        
        return total / values.size();
    }
    
    // SIMD optimized percentile calculation
    double calculate_percentile_simd(std::vector<double>& values, double p);
    
    // Vectorized min/max finding
    std::pair<double, double> find_min_max_simd(const std::vector<double>& values);
};
```

### 9. Adaptive Collection Strategies

```cpp
class adaptive_collector {
private:
    struct collection_stats {
        double avg_collection_time_ms;
        double cpu_overhead_percent;
        size_t dropped_collections;
    };
    
    collection_stats stats_;
    std::chrono::milliseconds current_interval_;
    
public:
    auto adapt_interval() -> result_void {
        // Increase interval if overhead is too high
        if (stats_.cpu_overhead_percent > 5.0) {
            current_interval_ *= 1.5;
        }
        // Decrease interval if we have headroom
        else if (stats_.cpu_overhead_percent < 1.0) {
            current_interval_ *= 0.8;
        }
        
        // Clamp to reasonable bounds
        current_interval_ = std::clamp(current_interval_,
            std::chrono::milliseconds(100),
            std::chrono::milliseconds(10000));
        
        return {};
    }
    
    auto select_collectors() -> std::vector<metrics_collector*> {
        // Prioritize critical collectors under load
        if (stats_.cpu_overhead_percent > 3.0) {
            return get_critical_collectors();
        }
        return get_all_collectors();
    }
};
```

### 10. Real-time Alerting System

```cpp
class alert_rule {
public:
    virtual ~alert_rule() = default;
    virtual bool evaluate(const metrics_snapshot& metrics) const = 0;
    virtual alert_severity severity() const = 0;
    virtual std::string description() const = 0;
};

class threshold_alert : public alert_rule {
private:
    std::string metric_name_;
    double threshold_;
    comparison_op op_;
    alert_severity severity_;
    
public:
    bool evaluate(const metrics_snapshot& metrics) const override {
        auto value = metrics.get_metric(metric_name_);
        return compare(value, threshold_, op_);
    }
};

class alert_manager {
private:
    std::vector<std::unique_ptr<alert_rule>> rules_;
    std::vector<std::function<void(const alert&)>> handlers_;
    
public:
    auto add_rule(std::unique_ptr<alert_rule> rule) -> result_void;
    auto add_handler(std::function<void(const alert&)> handler) -> result_void;
    
    auto evaluate(const metrics_snapshot& metrics) -> result<std::vector<alert>> {
        std::vector<alert> triggered;
        
        for (const auto& rule : rules_) {
            if (rule->evaluate(metrics)) {
                alert a{
                    .timestamp = std::chrono::steady_clock::now(),
                    .severity = rule->severity(),
                    .description = rule->description(),
                    .metrics = metrics
                };
                triggered.push_back(a);
                notify_handlers(a);
            }
        }
        
        return triggered;
    }
};
```

### 11. Health Monitoring Framework

```cpp
class health_check {
public:
    enum class health_status {
        healthy,
        degraded,
        unhealthy
    };
    
    struct health_result {
        health_status status;
        std::string message;
        std::unordered_map<std::string, std::string> details;
    };
    
    virtual ~health_check() = default;
    virtual auto check() -> result<health_result> = 0;
    virtual std::string name() const = 0;
};

class monitoring_health : public health_check {
public:
    auto check() -> result<health_result> override {
        health_result result;
        
        // Check collector health
        auto collector_health = check_collectors();
        
        // Check storage health
        auto storage_health = check_storage();
        
        // Check performance
        auto perf_health = check_performance();
        
        // Aggregate results
        if (all_healthy({collector_health, storage_health, perf_health})) {
            result.status = health_status::healthy;
        } else if (any_unhealthy({collector_health, storage_health, perf_health})) {
            result.status = health_status::unhealthy;
        } else {
            result.status = health_status::degraded;
        }
        
        return result;
    }
};
```

### 12. Crash-Safe Persistence

```cpp
class crash_safe_storage {
private:
    // Memory-mapped file for crash safety
    struct mmap_header {
        std::uint32_t magic;
        std::uint32_t version;
        std::uint64_t write_position;
        std::uint64_t item_count;
        std::uint32_t checksum;
    };
    
    void* mmap_base_;
    size_t mmap_size_;
    mmap_header* header_;
    
public:
    auto store_crash_safe(const metrics_snapshot& snapshot) -> result_void {
        // Write to memory-mapped file
        auto offset = header_->write_position;
        
        // Serialize snapshot
        auto serialized = serialize(snapshot);
        
        // Write with checksum
        write_with_checksum(mmap_base_ + offset, serialized);
        
        // Update header atomically
        header_->write_position += serialized.size();
        header_->item_count++;
        header_->checksum = calculate_checksum();
        
        // Force sync to disk
        msync(mmap_base_, mmap_size_, MS_SYNC);
        
        return {};
    }
    
    auto recover() -> result<std::vector<metrics_snapshot>> {
        // Verify checksum
        if (!verify_checksum()) {
            return thread_module::error{
                monitoring_error_code::storage_corrupted
            };
        }
        
        // Read all valid entries
        std::vector<metrics_snapshot> recovered;
        size_t offset = sizeof(mmap_header);
        
        while (offset < header_->write_position) {
            auto snapshot = deserialize(mmap_base_ + offset);
            if (snapshot) {
                recovered.push_back(*snapshot);
            }
            offset += get_entry_size(mmap_base_ + offset);
        }
        
        return recovered;
    }
};
```

## Integration Examples

### Thread System Integration
```cpp
// In thread_pool initialization
auto monitor_adapter = std::make_unique<thread_system_monitoring_adapter>(
    unified_monitor.get());
thread_pool.set_monitoring(std::move(monitor_adapter));
```

### Logger System Integration
```cpp
// In logger initialization
auto monitor_adapter = std::make_unique<logger_system_monitoring_adapter>(
    unified_monitor.get());
logger.set_monitor(std::move(monitor_adapter));
```

### Application Usage
```cpp
// Setup DI container
auto& container = monitoring_service_container::global();
container.register_collector_factory<cpu_collector>("cpu");
container.register_collector_factory<memory_collector>("memory");
container.register_storage_backend<crash_safe_storage>("persistent");

// Build monitoring system
auto monitoring = monitoring_builder()
    .with_history_size(10000)
    .with_collection_interval(std::chrono::seconds(1))
    .add_collector(container.resolve<metrics_collector>("cpu"))
    .add_collector(container.resolve<metrics_collector>("memory"))
    .with_storage(container.resolve<storage_backend>("persistent"))
    .enable_compression(true)
    .build();

// Register health check
monitoring->register_health_check("database", []() {
    return check_database_connection();
});

// Add alert rule
auto alert_rule = std::make_unique<threshold_alert>(
    "cpu_usage", 90.0, comparison_op::greater_than, 
    alert_severity::critical);
monitoring->add_alert_rule(std::move(alert_rule));

// Get metrics with Result pattern
if (auto snapshot = monitoring->get_full_snapshot(); snapshot) {
    std::cout << "CPU usage: " << snapshot->cpu_usage << "%\n";
} else {
    std::cerr << "Failed to get metrics: " << snapshot.error().message() << "\n";
}
```

## Enhanced CMake Structure

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(monitoring_system VERSION 2.0.0 LANGUAGES CXX)

# C++20 required
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(MONITORING_USE_THREAD_SYSTEM "Link with thread_system" ON)
option(MONITORING_USE_SIMD "Enable SIMD optimizations" ON)
option(MONITORING_BUILD_PLUGINS "Build plugin system" OFF)
option(MONITORING_ENABLE_PERSISTENCE "Enable persistent storage" ON)
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)

# Find dependencies
find_package(Threads REQUIRED)
if(MONITORING_USE_THREAD_SYSTEM)
    find_package(thread_system CONFIG REQUIRED)
endif()

# Create library
add_library(monitoring_system
    sources/core/unified_monitor.cpp
    sources/core/metrics_aggregator.cpp
    sources/core/health_manager.cpp
    sources/adapters/thread_system_adapter.cpp
    sources/adapters/logger_system_adapter.cpp
    sources/backends/basic_backend.cpp
    sources/backends/crash_safe_storage.cpp
)

# Configure features
if(MONITORING_USE_SIMD)
    target_compile_definitions(monitoring_system PRIVATE USE_SIMD=1)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
        target_compile_options(monitoring_system PRIVATE -mavx2)
    endif()
endif()

# Add sanitizers
include(cmake/Sanitizers.cmake)
add_sanitizers(monitoring_system)

# Package configuration
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    MonitoringSystemConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Install
install(TARGETS monitoring_system EXPORT MonitoringSystemTargets)
install(EXPORT MonitoringSystemTargets
    FILE MonitoringSystemTargets.cmake
    NAMESPACE MonitoringSystem::
    DESTINATION lib/cmake/MonitoringSystem
)
```

## Testing Strategy

### Comprehensive Testing Framework

```cpp
// Mock collector for testing
class mock_collector : public metrics_collector {
public:
    MOCK_METHOD(result<metrics_snapshot>, collect, (), (override));
    MOCK_METHOD(std::string, name, (), (const, override));
    MOCK_METHOD(bool, is_enabled, (), (const, override));
};

// Property-based testing
TEST(MonitoringProperties, StorageInvariants) {
    rc::check("storage maintains ordering", []() {
        auto storage = create_test_storage();
        auto snapshots = *rc::gen::container<std::vector<metrics_snapshot>>(
            rc::gen::arbitrary<metrics_snapshot>());
        
        for (const auto& snapshot : snapshots) {
            RC_ASSERT(storage->store(snapshot));
        }
        
        auto retrieved = storage->retrieve_all();
        RC_ASSERT(std::is_sorted(retrieved.begin(), retrieved.end(),
            [](const auto& a, const auto& b) {
                return a.timestamp < b.timestamp;
            }));
    });
}

// Load testing
class monitoring_load_test : public ::testing::Test {
protected:
    void stress_test_concurrent_collection() {
        const int num_collectors = 100;
        const int collections_per_second = 1000;
        const auto duration = std::chrono::seconds(60);
        
        auto monitoring = create_test_monitoring();
        
        // Add collectors
        for (int i = 0; i < num_collectors; ++i) {
            monitoring->add_collector(
                std::make_unique<synthetic_collector>(i));
        }
        
        // Run load test
        auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < duration) {
            monitoring->collect_now();
            std::this_thread::sleep_for(
                std::chrono::milliseconds(1000 / collections_per_second));
        }
        
        // Verify no data loss
        auto stats = monitoring->get_stats();
        EXPECT_EQ(stats.dropped_snapshots, 0);
        EXPECT_GT(stats.total_collections, 
                 collections_per_second * duration.count() * 0.95);
    }
};

// Fuzzing
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Fuzz configuration parser
    monitoring_config config;
    if (config.parse_from_bytes(data, size)) {
        auto monitoring = monitoring_builder()
            .with_config(config)
            .build();
    }
    return 0;
}
```

## Performance Considerations

### Memory Usage
- **Metric Storage**: ~100 bytes per metric
- **Ring Buffer**: Configurable size (default 10,000 entries)
- **Memory Pool**: Pre-allocated for zero-copy operations
- **Time Series**: 8 bytes per data point
- **Total Estimate**: < 10MB for typical usage

### CPU Overhead
- **Metric Collection**: < 100ns per metric (lock-free)
- **SIMD Aggregation**: 4-8x faster than scalar
- **Adaptive Sampling**: Dynamic overhead reduction
- **Health Checks**: Configurable intervals
- **Target**: < 1% CPU overhead

### Optimization Strategies
1. **Lock-free Data Structures**: Minimize contention
2. **Cache-line Alignment**: Reduce false sharing
3. **SIMD Instructions**: Hardware acceleration
4. **Memory Pooling**: Zero allocation in hot path
5. **Adaptive Sampling**: Reduce load under pressure

## Backward Compatibility

### Migration Path
```cpp
// Step 1: Update configuration
auto old_config = load_v1_config();
auto new_config = migration_helper::migrate_config(old_config);

// Step 2: Update initialization
// Old way
auto monitoring = new monitoring_v1::monitoring(1000, 1000);

// New way
auto monitoring = monitoring_builder()
    .with_history_size(1000)
    .with_collection_interval(std::chrono::milliseconds(1000))
    .build();

// Step 3: Update error handling
// Old way (exceptions)
try {
    monitoring->add_collector(collector);
} catch (const std::exception& e) {
    // handle error
}

// New way (Result pattern)
if (auto result = monitoring->add_collector(std::move(collector)); !result) {
    // handle error with result.error()
}
```

### Compatibility Layer
```cpp
namespace monitoring_v1 {
    // Old API preserved with deprecation warnings
    class monitoring {
    public:
        [[deprecated("Use monitoring_v2 with Result pattern")]]
        void update_metrics(const metrics& m) {
            auto result = v2_monitoring_->update_metrics(m);
            if (!result) {
                throw std::runtime_error(result.error().message());
            }
        }
    };
}
```

## Success Metrics

### Quantitative KPIs
- **Error Handling**: 100% Result pattern adoption
- **Performance**: <1μs metric update latency
- **Memory**: <100MB overhead for 100k metrics
- **Throughput**: >100K metrics/second
- **Reliability**: 99.99% uptime
- **Testing**: >90% code coverage

### Qualitative Goals
- Full thread_system integration
- Plugin architecture support
- Zero-copy operations
- Real-time alerting
- SIMD optimization
- Crash-safe persistence

## Risk Mitigation

| Risk | Probability | Impact | Mitigation |
|------|------------|--------|------------|
| Performance regression | Medium | High | Continuous benchmarking, SIMD optimization |
| Breaking changes | High | High | Compatibility layer, migration tools |
| Thread_system coupling | Medium | Medium | Interface abstraction, DI container |
| Memory growth | Medium | Medium | Ring buffer limits, memory pools |
| Complexity increase | High | Medium | Phased rollout, comprehensive docs |

## Implementation Timeline

### Phase Summary (12 weeks total)
1. **Weeks 1-2**: Core architecture alignment (Result pattern, DI, interfaces)
2. **Weeks 3-4**: Design patterns (Builder, Factory, Strategy, Observer)
3. **Weeks 5-6**: Performance optimizations (Lock-free, Zero-copy, SIMD)
4. **Weeks 7-8**: Reliability features (Error recovery, Health checks, Persistence)
5. **Week 9**: Build system and integration
6. **Week 10**: Comprehensive testing
7. **Weeks 11-12**: Documentation and migration support

### Resource Allocation
- **Core Team (2-3 developers)**: Focus on architecture and core implementation
- **Performance Team (1-2 developers)**: Lock-free structures and SIMD optimization
- **QA Team (2 developers)**: Testing framework and quality assurance
- **DevOps (1 developer)**: Build system and CI/CD

## Future Enhancements

### Version 2.1
- Distributed monitoring support
- Machine learning anomaly detection
- Predictive health checks
- Custom metric queries

### Version 3.0
- Cloud provider integrations
- Grafana dashboard templates
- Metric correlation analysis
- Automated remediation hooks
- Performance profiling integration

## Documentation Strategy

### Documentation Structure
Following the proven structure from thread_system and logger_system:

```
monitoring_system/
├── docs/
│   ├── README.md                    # Documentation overview
│   ├── architecture/
│   │   ├── ARCHITECTURE.md         # System architecture
│   │   ├── INTERFACES.md           # Interface specifications
│   │   └── DESIGN_PATTERNS.md      # Design pattern usage
│   ├── api/
│   │   ├── API_REFERENCE.md        # Complete API documentation
│   │   ├── api-cpp.md              # C++ API reference
│   │   └── api-examples.md         # API usage examples
│   ├── guides/
│   │   ├── GETTING_STARTED.md      # Quick start guide
│   │   ├── USER_GUIDE.md           # Comprehensive user guide
│   │   ├── MIGRATION_GUIDE.md      # Migration from v1.x
│   │   └── INTEGRATION_GUIDE.md    # Integration with thread/logger systems
│   ├── development/
│   │   ├── CONTRIBUTING.md         # Contribution guidelines
│   │   ├── DEVELOPMENT.md          # Development setup
│   │   ├── TESTING.md              # Testing guidelines
│   │   └── QUALITY.md              # Code quality standards
│   ├── performance/
│   │   ├── BENCHMARKS.md           # Benchmark results
│   │   ├── OPTIMIZATION.md         # Performance tuning
│   │   └── PROFILING.md            # Profiling guide
│   └── ci-cd/
│       ├── CI_CD_DASHBOARD.md      # CI/CD status dashboard
│       └── DEPLOYMENT.md           # Deployment guide
├── examples/                        # Example applications
│   ├── basic_monitoring/
│   ├── advanced_features/
│   ├── integration_examples/
│   └── performance_demos/
└── CHANGELOG.md                     # Version history

```

### Documentation Standards

#### 1. API Documentation (Doxygen)
```cpp
/**
 * @file monitoring_system.h
 * @brief Core monitoring system implementation
 * @author monitoring_system contributors
 * @date 2025
 * 
 * @section intro_sec Introduction
 * This file contains the core monitoring system implementation that provides
 * unified metrics collection from thread_system and logger_system.
 * 
 * @section usage_sec Basic Usage
 * @code{.cpp}
 * // Create monitoring instance
 * auto monitoring = monitoring_builder()
 *     .with_history_size(1000)
 *     .add_collector(std::make_unique<cpu_collector>())
 *     .build();
 * 
 * // Start monitoring
 * monitoring->start();
 * @endcode
 * 
 * @section arch_sec Architecture
 * The monitoring system follows a modular architecture with:
 * - Collectors: Gather metrics from various sources
 * - Aggregators: Process and aggregate metrics
 * - Backends: Store and export metrics
 * 
 * @see thread_system::monitoring_interface
 * @see logger_module::monitoring_interface
 */

/**
 * @class unified_monitoring_interface
 * @brief Unified interface for monitoring both thread and logger systems
 * 
 * @details This interface provides a unified API for collecting metrics
 * from multiple systems while maintaining loose coupling through adapters.
 * 
 * @tparam T The metric type to collect
 * 
 * @invariant The monitoring system must be initialized before use
 * @invariant Collectors must be registered before starting collection
 * 
 * @par Thread Safety
 * All public methods are thread-safe unless otherwise noted.
 * 
 * @par Example
 * @code{.cpp}
 * class MyMonitor : public unified_monitoring_interface {
 *     auto collect_metrics() -> result<monitoring_data> override {
 *         // Implementation
 *     }
 * };
 * @endcode
 * 
 * @since Version 2.0.0
 */
```

#### 2. Markdown Documentation Guidelines
- **Clear Structure**: Use hierarchical headings (##, ###, ####)
- **Code Examples**: Include practical, runnable examples
- **Visual Aids**: Use diagrams and charts where helpful
- **Cross-references**: Link related documentation
- **Version Notes**: Track API changes across versions

#### 3. Documentation Generation
```bash
# Generate Doxygen documentation
doxygen docs/Doxyfile

# Generate API reference from source
python scripts/generate_api_docs.py

# Validate documentation links
python scripts/validate_docs.py
```

## CI/CD Pipeline

### GitHub Actions Workflows

#### 1. Multi-Platform Build Matrix
```yaml
# .github/workflows/build-matrix.yml
name: Build Matrix

on:
  push:
    branches: [main, develop]
  pull_request:
    branches: [main]
  workflow_dispatch:

jobs:
  build:
    strategy:
      matrix:
        os: [ubuntu-latest, windows-latest, macos-latest]
        compiler: [gcc, clang, msvc]
        build_type: [Debug, Release]
        exclude:
          - os: ubuntu-latest
            compiler: msvc
          - os: macos-latest
            compiler: msvc
          - os: windows-latest
            compiler: gcc
    
    runs-on: ${{ matrix.os }}
    
    steps:
      - uses: actions/checkout@v4
        with:
          submodules: recursive
      
      - name: Setup vcpkg
        uses: lukka/run-vcpkg@v11
        with:
          vcpkgGitCommitId: 'latest'
      
      - name: Configure CMake
        run: |
          cmake -B build \
            -DCMAKE_BUILD_TYPE=${{ matrix.build_type }} \
            -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake \
            -DMONITORING_BUILD_TESTS=ON \
            -DMONITORING_BUILD_BENCHMARKS=ON
      
      - name: Build
        run: cmake --build build --config ${{ matrix.build_type }}
      
      - name: Test
        run: ctest --test-dir build -C ${{ matrix.build_type }} --output-on-failure
```

#### 2. Quality Checks
```yaml
# .github/workflows/quality.yml
name: Code Quality

on: [push, pull_request]

jobs:
  static-analysis:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      
      - name: Run clang-tidy
        run: |
          cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
          run-clang-tidy -p build
      
      - name: Run cppcheck
        run: |
          cppcheck --enable=all --suppress=missingInclude \
            --error-exitcode=1 sources/
      
      - name: Check formatting
        run: |
          find sources -name "*.cpp" -o -name "*.h" | \
            xargs clang-format --dry-run --Werror
  
  sanitizers:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        sanitizer: [address, thread, undefined]
    
    steps:
      - uses: actions/checkout@v4
      
      - name: Build with sanitizer
        run: |
          cmake -B build \
            -DCMAKE_BUILD_TYPE=Debug \
            -DENABLE_${{ matrix.sanitizer }}_SANITIZER=ON
          cmake --build build
      
      - name: Run tests with sanitizer
        run: |
          export ASAN_OPTIONS=detect_leaks=1:strict_string_checks=1
          export TSAN_OPTIONS=halt_on_error=1:history_size=7
          export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
          ctest --test-dir build --output-on-failure
```

#### 3. Performance Monitoring
```yaml
# .github/workflows/performance.yml
name: Performance Monitoring

on:
  push:
    branches: [main]
  schedule:
    - cron: '0 0 * * *'  # Daily

jobs:
  benchmark:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      
      - name: Build benchmarks
        run: |
          cmake -B build -DCMAKE_BUILD_TYPE=Release \
            -DMONITORING_BUILD_BENCHMARKS=ON
          cmake --build build
      
      - name: Run benchmarks
        run: |
          ./build/benchmarks/monitoring_benchmarks \
            --benchmark_format=json \
            --benchmark_out=results.json
      
      - name: Store benchmark result
        uses: benchmark-action/github-action-benchmark@v1
        with:
          tool: 'googlecpp'
          output-file-path: results.json
          github-token: ${{ secrets.GITHUB_TOKEN }}
          auto-push: true
      
      - name: Check performance regression
        run: |
          python scripts/check_regression.py results.json
```

#### 4. Documentation Build
```yaml
# .github/workflows/documentation.yml
name: Documentation

on:
  push:
    branches: [main]
    paths:
      - 'docs/**'
      - 'sources/**/*.h'
      - 'Doxyfile'

jobs:
  doxygen:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      
      - name: Install Doxygen
        run: sudo apt-get install -y doxygen graphviz
      
      - name: Generate documentation
        run: doxygen docs/Doxyfile
      
      - name: Deploy to GitHub Pages
        uses: peaceiris/actions-gh-pages@v3
        with:
          github_token: ${{ secrets.GITHUB_TOKEN }}
          publish_dir: ./docs/html
```

### CI/CD Dashboard

#### Dashboard Structure (Following logger_system pattern)
```markdown
# Monitoring System CI/CD Dashboard

## 🚀 Build Status

### Main Branch
| Platform | Compiler | Status | Coverage | Performance |
|----------|----------|--------|----------|-------------|
| Ubuntu | GCC | ![Build](badge) | 95% | ✅ |
| Ubuntu | Clang | ![Build](badge) | 95% | ✅ |
| Windows | MSVC | ![Build](badge) | 93% | ✅ |
| macOS | Clang | ![Build](badge) | 94% | ✅ |

## 📊 Performance Metrics

### Latest Benchmark Results
| Metric | Current | Target | Trend |
|--------|---------|--------|-------|
| Collection Latency | 85ns | <100ns | 📉 |
| Throughput | 125K/s | >100K/s | 📈 |
| Memory Usage | 8.5MB | <10MB | ➡️ |
| CPU Overhead | 0.8% | <1% | ✅ |

## 🔍 Code Quality

### Static Analysis
| Tool | Issues | Status |
|------|--------|--------|
| clang-tidy | 0 | ✅ |
| cppcheck | 0 | ✅ |
| CodeQL | 0 | ✅ |

### Test Coverage
| Component | Line | Branch | Function |
|-----------|------|--------|----------|
| Core | 96% | 92% | 100% |
| Adapters | 94% | 90% | 98% |
| Backends | 92% | 88% | 95% |
```

### Continuous Deployment

#### 1. Package Generation
```cmake
# CPack configuration
include(CPack)
set(CPACK_PACKAGE_NAME "monitoring_system")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_GENERATOR "TGZ;ZIP;DEB;RPM")
set(CPACK_SOURCE_GENERATOR "TGZ;ZIP")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY 
    "Unified monitoring system for thread_system and logger_system")
```

#### 2. vcpkg Integration
```json
{
  "name": "monitoring-system",
  "version": "2.0.0",
  "description": "Unified monitoring system",
  "homepage": "https://github.com/yourusername/monitoring_system",
  "dependencies": [
    "thread-system",
    "logger-system",
    {
      "name": "benchmark",
      "features": ["tools"]
    },
    "gtest"
  ],
  "features": {
    "simd": {
      "description": "Enable SIMD optimizations"
    },
    "prometheus": {
      "description": "Prometheus backend support",
      "dependencies": ["prometheus-cpp"]
    }
  }
}
```

## Architecture Decision Records

### ADR-001: Use Result Pattern for Error Handling
**Status**: Accepted  
**Context**: Need consistent error handling without exceptions  
**Decision**: Adopt thread_system's Result pattern  
**Consequences**: Explicit error handling, better composability, learning curve for team

### ADR-002: Adopt Dependency Injection Container
**Status**: Accepted  
**Context**: Need loose coupling and testability  
**Decision**: Integrate with thread_system's service container  
**Consequences**: Better testability, runtime configuration, slight complexity increase

### ADR-003: Implement SIMD Optimizations
**Status**: Accepted  
**Context**: Need high-performance aggregations  
**Decision**: Use AVX2 instructions where available  
**Consequences**: 4-8x performance improvement, platform-specific code, fallback required

### ADR-004: Comprehensive Documentation Strategy
**Status**: Accepted  
**Context**: Need maintainable and accessible documentation  
**Decision**: Adopt thread_system's documentation structure with Doxygen and markdown  
**Consequences**: Better developer experience, maintenance overhead, automated generation

### ADR-005: Multi-Platform CI/CD Pipeline
**Status**: Accepted  
**Context**: Need reliable quality assurance across platforms  
**Decision**: Implement GitHub Actions matrix builds with comprehensive testing  
**Consequences**: Early bug detection, longer build times, resource usage

## Conclusion

This comprehensive monitoring system design integrates the best practices from both thread_system and logger_system while introducing modern C++20 patterns and optimizations. The phased implementation approach ensures systematic development with clear priorities and dependencies. By adopting the Result pattern, dependency injection, and lock-free structures, the system achieves high performance, reliability, and maintainability while remaining extensible for future enhancements.

## Implementation Progress

### Phase 1: Core Architecture Alignment (Completed)
- ✅ A1: Result Pattern Implementation
- ✅ A2: Comprehensive Error Codes
- ✅ A3: Dependency Injection Container
- ✅ A4: Monitorable Interface
- ✅ A5: Thread Context and Metadata

### Phase 2: Advanced Monitoring Features (In Progress)
- ✅ D1: Distributed Tracing Implementation
  - Implemented W3C Trace Context propagation
  - Created trace span management system
  - Added baggage propagation support
  - Implemented scoped span RAII pattern
  
- ✅ D2: Performance Monitoring Implementation
  - Created comprehensive performance profiler
  - Added system resource monitoring (CPU, memory, threads)
  - Implemented percentile-based metrics (P50, P95, P99)
  - Added throughput and error rate tracking
  - Created performance benchmarking utilities
  - Implemented scoped timing with RAII pattern
  - Added threshold-based alerting capabilities
  - Created comprehensive test suite

#### Distributed Tracing Features
- **Trace Context Propagation**: W3C Trace Context standard compliance
- **Span Management**: Hierarchical span creation and lifecycle management
- **Baggage Support**: Cross-service context propagation
- **Thread Safety**: Thread-local span management
- **RAII Pattern**: Automatic span finishing with scoped_span

### Next Steps
- Complete test compilation fixes
- Implement span exporters (Jaeger, Zipkin)
- Add OpenTelemetry compatibility layer
- Performance optimization for high-throughput scenarios

