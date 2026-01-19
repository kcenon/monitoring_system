# Migration Guide - Monitoring System

> **Language:** **English** | [한국어](MIGRATION.kr.md)

## Overview

This guide helps you migrate between versions of monitoring_system, including updates from earlier architecture phases and integration pattern changes.

**Migration Paths**:
- monitoring_system 1.x → 2.0 (Architecture modernization)
- Phase 2 (Manual RAII) → Phase 3 (Result<T> pattern)
- Standalone monitoring → Ecosystem integration

**Estimated Time**: 3-6 hours for typical applications
**Breaking Changes**: Yes (namespace, interfaces, error handling)

---

## Quick Migration Checklist

- [ ] Update CMakeLists.txt dependencies
- [ ] Replace header includes
- [ ] Update error handling to Result<T>
- [ ] Migrate to IMonitor interface
- [ ] Update namespace references
- [ ] Rebuild and test
- [ ] Update integration points

### Result API Upgrade Checklist

1. Search for legacy patterns `if (!result)` or `result.get_error()` and replace them with `result.is_err()` / `result.error()`.
2. Replace `result_void::error(...)` invocations with `Result<void>::err(...)`.
3. When bridging to `common_system`, return `Result<T>::err(common::error_info{...})` to preserve module names.
4. Update documentation snippets (README, tutorials) alongside code changes to prevent regressions like those highlighted in `docs/SYSTEM_REFERENCE_IMPROVEMENT_RESULTS.md`.

---

## Step-by-Step Migration

### Migration Path 1: Version 1.x to 2.0

#### Step 1: Update CMakeLists.txt

**Before** (1.x):
```cmake
find_package(monitoring_system CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE monitoring_system)
```

**After** (2.0):
```cmake
find_package(monitoring_system CONFIG REQUIRED)
find_package(common_system CONFIG REQUIRED)

target_link_libraries(your_target PRIVATE
    kcenon::monitoring_system
    kcenon::common_system
)
```

#### Step 2: Update Header Includes

**Before**:
```cpp
#include <monitoring/performance_monitor.h>
#include <monitoring/distributed_tracer.h>
#include <monitoring/health_monitor.h>
```

**After**:
```cpp
#include <kcenon/monitoring/core/performance_monitor.h>
#include <kcenon/monitoring/tracing/distributed_tracer.h>
#include <kcenon/monitoring/health/health_monitor.h>
```

#### Step 3: Update Namespace

**Before**:
```cpp
using namespace monitoring;

auto monitor = monitoring::performance_monitor("service");
```

**After**:
```cpp
using namespace monitoring_system;

auto monitor = monitoring_system::performance_monitor("service");
```

#### Step 4: Update Error Handling to Result<T>

**Before** (1.x - exceptions):
```cpp
try {
    auto snapshot = monitor.collect();
    process_metrics(snapshot);
} catch (const monitoring_exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
}
```

**After** (2.0 - Result<T>):
```cpp
auto result = monitor.collect();
if (result) {
    auto snapshot = result.value();
    process_metrics(snapshot);
} else {
    auto error = result.error();
    std::cerr << "Error: " << error.message << " (code: "
              << static_cast<int>(error.code) << ")\n";
}
```

---

## Migration Path 2: Phase 2 to Phase 3 (Result Pattern)

### Converting Exception-Based Code

#### Example 1: Metrics Collection

**Before** (Phase 2):
```cpp
void collect_metrics() {
    try {
        auto& monitor = get_monitor();
        auto snapshot = monitor.collect();

        if (snapshot.empty()) {
            throw std::runtime_error("No metrics collected");
        }

        store_metrics(snapshot);
    } catch (const std::exception& e) {
        log_error("Collection failed: " + std::string(e.what()));
    }
}
```

**After** (Phase 3):
```cpp
auto collect_metrics() -> common::Result<void> {
    auto& monitor = get_monitor();
    auto result = monitor.collect();

    if (!result) {
        return common::error<void>(
            common::error_codes::COLLECTION_FAILED,
            "Failed to collect metrics: " + result.error().message,
            "metrics_collector"
        );
    }

    auto snapshot = result.value();
    if (snapshot.empty()) {
        return common::error<void>(
            common::error_codes::INVALID_STATE,
            "No metrics collected",
            "metrics_collector"
        );
    }

    auto store_result = store_metrics(snapshot);
    if (!store_result) {
        return store_result;
    }

    return common::ok();
}
```

#### Example 2: Distributed Tracing

**Before**:
```cpp
void process_request(const std::string& request_id) {
    auto& tracer = global_tracer();

    try {
        auto span = tracer.start_span("process_request", "api");
        span->set_tag("request.id", request_id);

        // Process request
        handle_request(request_id);

        tracer.finish_span(span);
    } catch (const tracing_exception& e) {
        log_error("Tracing failed: " + std::string(e.what()));
    }
}
```

**After**:
```cpp
auto process_request(const std::string& request_id) -> common::Result<void> {
    auto& tracer = global_tracer();

    auto span_result = tracer.start_span("process_request", "api");
    if (!span_result) {
        return common::error<void>(
            common::error_codes::TRACING_FAILED,
            "Failed to start span: " + span_result.error().message,
            "request_processor"
        );
    }

    auto span = span_result.value();
    span->set_tag("request.id", request_id);

    // Process request
    auto handle_result = handle_request(request_id);
    if (!handle_result) {
        span->set_tag("error", handle_result.error().message);
        tracer.finish_span(span);
        return handle_result;
    }

    auto finish_result = tracer.finish_span(span);
    if (!finish_result) {
        return common::error<void>(
            common::error_codes::TRACING_FAILED,
            "Failed to finish span",
            "request_processor"
        );
    }

    return common::ok();
}
```

---

## Migration Path 3: Standalone to Ecosystem Integration

### Adding common_system Integration

**Before** (Standalone):
```cpp
#include <monitoring/performance_monitor.h>

class MyApplication {
public:
    void initialize() {
        monitor_.enable_collection(true);
    }

private:
    monitoring_system::performance_monitor monitor_{"my_app"};
};
```

**After** (With common_system):
```cpp
#include <kcenon/monitoring/core/performance_monitor.h>
#include <kcenon/common/interfaces/monitoring_interface.h>
#include <kcenon/common/patterns/result.h>

class MyApplication : public common::interfaces::IMonitor {
public:
    auto configure(const std::string& config) -> common::Result<void> override {
        // Parse and apply configuration
        return common::ok();
    }

    auto start() -> common::Result<void> override {
        monitor_.enable_collection(true);
        return common::ok();
    }

    auto stop() -> common::Result<void> override {
        monitor_.enable_collection(false);
        return common::ok();
    }

    auto collect_now() -> common::Result<void> override {
        auto result = monitor_.collect();
        if (!result) {
            return common::error<void>(
                common::error_codes::COLLECTION_FAILED,
                result.error().message,
                "my_app"
            );
        }
        return common::ok();
    }

private:
    monitoring_system::performance_monitor monitor_{"my_app"};
};
```

### Adding thread_system Integration

**Before**:
```cpp
// Manual thread pool monitoring
class ThreadPoolMonitor {
public:
    void record_task_completion() {
        tasks_completed_++;
    }

    size_t get_tasks_completed() const {
        return tasks_completed_;
    }

private:
    std::atomic<size_t> tasks_completed_{0};
};
```

**After**:
```cpp
#include <kcenon/monitoring/adapters/thread_system_adapter.h>
#include <thread_system/thread_pool.h>

class Application {
public:
    void initialize() {
        // Create thread pool
        thread_pool_ = thread_system::create_thread_pool(8);

        // Attach monitoring adapter
        thread_adapter_.attach_to_pool(thread_pool_);
    }

    auto get_thread_metrics() -> common::Result<metrics_snapshot> {
        return thread_adapter_.collect_metrics();
    }

private:
    std::shared_ptr<thread_system::thread_pool> thread_pool_;
    monitoring_system::thread_system_adapter thread_adapter_;
};
```

---

## Code Migration Examples

### Example 1: Health Check Migration

**Before** (1.x):
```cpp
class DatabaseHealthCheck {
public:
    bool check() {
        return db_->is_connected();
    }

    std::string status_message() {
        return check() ? "Connected" : "Disconnected";
    }

private:
    std::shared_ptr<Database> db_;
};
```

**After** (2.0):
```cpp
#include <kcenon/monitoring/health/health_check.h>

class DatabaseHealthCheck : public monitoring_system::health_check_interface {
public:
    auto check_health() -> monitoring_system::health_check_result override {
        if (db_->is_connected()) {
            return monitoring_system::health_check_result::healthy(
                "Database connected"
            );
        } else {
            return monitoring_system::health_check_result::unhealthy(
                "Database connection lost"
            );
        }
    }

    auto get_name() const -> std::string override {
        return "database_connection";
    }

    auto get_type() const -> monitoring_system::health_check_type override {
        return monitoring_system::health_check_type::dependency;
    }

private:
    std::shared_ptr<Database> db_;
};
```

### Example 2: Circuit Breaker Migration

**Before** (Manual implementation):
```cpp
class ServiceClient {
public:
    std::optional<Response> call_service() {
        if (failure_count_ >= 5) {
            return std::nullopt; // Circuit open
        }

        try {
            auto response = make_request();
            failure_count_ = 0;
            return response;
        } catch (...) {
            failure_count_++;
            return std::nullopt;
        }
    }

private:
    int failure_count_{0};
};
```

**After** (Using circuit_breaker):
```cpp
#include <kcenon/monitoring/health/circuit_breaker.h>

class ServiceClient {
public:
    ServiceClient()
        : breaker_("external_service",
                  monitoring_system::circuit_breaker_config{
                      .failure_threshold = 5,
                      .timeout = std::chrono::seconds(30),
                      .half_open_max_calls = 3
                  }) {}

    auto call_service() -> common::Result<Response> {
        return breaker_.execute([this]() -> common::Result<Response> {
            try {
                auto response = make_request();
                return common::ok(response);
            } catch (const std::exception& e) {
                return common::error<Response>(
                    common::error_codes::EXTERNAL_SERVICE_ERROR,
                    e.what(),
                    "service_client"
                );
            }
        });
    }

private:
    monitoring_system::circuit_breaker breaker_;
};
```

---

## API Changes Reference

### Renamed Types and Functions

| Old API (1.x) | New API (2.0) |
|---------------|---------------|
| `monitoring::performance_monitor` | `monitoring_system::performance_monitor` |
| `monitoring::tracer` | `monitoring_system::distributed_tracer` |
| `monitor.collect()` throws | `monitor.collect()` returns `Result<T>` |
| `tracer.start_span()` throws | `tracer.start_span()` returns `Result<T>` |
| `health_check::check()` returns `bool` | `health_check::check_health()` returns `health_check_result` |
| Manual error handling | `common::Result<T>` pattern |

### New Interfaces

2.0 introduces these new interfaces from common_system:

- `common::interfaces::IMonitor` - Standard monitoring interface
- `common::interfaces::ILogger` - Logger integration
- `common::Result<T>` - Result pattern for error handling
- `common::error_codes` - Centralized error code registry

---

## Testing Your Migration

### 1. Compile Test

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DBUILD_WITH_COMMON_SYSTEM=ON
make
```

Expected: Clean build with no errors

### 2. Unit Tests

```bash
# Run all tests
ctest

# Or manually
./build/tests/monitoring_system_tests
```

Expected: All 37 tests pass (100% pass rate)

### 3. Integration Tests

```bash
# Test with ecosystem integration
cmake .. -DMONITORING_BUILD_INTEGRATION_TESTS=ON \
         -DMONITORING_USE_THREAD_SYSTEM=ON \
         -DBUILD_WITH_LOGGER_SYSTEM=ON
make
./build/integration_tests/ecosystem_integration_test
```

Expected: All integration tests pass

### 4. Performance Regression Tests

```bash
# Run benchmarks to ensure no performance degradation
cmake .. -DMONITORING_BUILD_BENCHMARKS=ON
make
./build/benchmarks/performance_benchmark
```

Expected performance (baseline):
- Metrics collection: 10M ops/s
- Trace span creation: 2.5M spans/s
- Health checks: 500K checks/s
- Event publishing: 5.8M events/s

---

## Common Migration Issues

### Issue 1: Undefined Reference to IMonitor

**Problem**: Linker errors about missing IMonitor interface
**Solution**: Link common_system
```cmake
find_package(common_system CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE kcenon::common_system)
```

### Issue 2: Result Type Ambiguity

**Problem**: Compilation errors with multiple Result<T> definitions
**Solution**: Use explicit namespace or common_system Result
```cpp
// Avoid: using namespace monitoring_system; using namespace common;

// Use: Explicit common::Result<T>
auto collect() -> common::Result<metrics_snapshot> {
    // Implementation
}
```

### Issue 3: CMake Cache Issues

**Problem**: CMake still finds old version
**Solution**: Clear CMake cache
```bash
rm -rf build
mkdir build && cd build
cmake ..
```

### Issue 4: Binary Incompatibility

**Problem**: Old metrics data doesn't load
**Solution**: Metrics format changed in 2.0 - use migration tool
```bash
# Download migration tool
curl -O https://raw.githubusercontent.com/kcenon/monitoring_system/main/scripts/migrate_metrics.sh
chmod +x migrate_metrics.sh

# Migrate old metrics data
./migrate_metrics.sh /path/to/old/metrics.db /path/to/new/metrics.db
```

### Issue 5: Performance Regression

**Problem**: Monitoring overhead increased after migration
**Solution**: Enable performance optimizations
```cmake
set(CMAKE_BUILD_TYPE Release)
set(ENABLE_SIMD ON)
```

```cpp
// Configure optimal settings
monitoring_system::monitoring_config config;
config.sampling_rate = 0.1; // Enable sampling
config.collection_interval = std::chrono::seconds(10); // Batch collection
```

---

## Performance Comparison

After migration, you should see similar or better performance:

| Metric | 1.x | 2.0 | Change |
|--------|-----|-----|--------|
| Metrics collection | 8.5M/s | 10M/s | +18% |
| Trace span creation | 2.2M/s | 2.5M/s | +14% |
| Health checks | 450K/s | 500K/s | +11% |
| Event publishing | 5.5M/s | 5.8M/s | +5% |
| Memory baseline | 6MB | <5MB | -17% |

---

## Rollback Plan

If migration fails, you can rollback:

```bash
# Revert changes
git checkout -- .

# Or restore from backup
cp -r backup_before_migration/* .

# Rebuild with old version
mkdir build && cd build
cmake -DUSE_OLD_MONITORING=ON ..
make
```

**Note**: We maintain 1.x branch for critical bug fixes during migration period.

---

## Migration Timeline Recommendations

### Small Projects (<10K LOC)
- **Week 1**: Update dependencies and headers
- **Week 2**: Migrate to Result<T> pattern
- **Week 3**: Testing and validation

### Medium Projects (10K-50K LOC)
- **Week 1-2**: Update build system and dependencies
- **Week 3-4**: Migrate core components to Result<T>
- **Week 5-6**: Update integration points
- **Week 7-8**: Comprehensive testing

### Large Projects (>50K LOC)
- **Month 1**: Planning and dependency updates
- **Month 2-3**: Incremental migration of modules
- **Month 4**: Integration testing and performance validation
- **Month 5**: Production deployment with gradual rollout

---

## Support

Need help with migration?

- **Migration Guide**: This document
- **Migration Issues**: [GitHub Issues](https://github.com/kcenon/monitoring_system/issues)
- **Migration Questions**: [GitHub Discussions](https://github.com/kcenon/monitoring_system/discussions)
- **Email Support**: kcenon@naver.com

---

**Last Updated**: 2025-10-22
**Migration Tool Version**: 2.0.0
