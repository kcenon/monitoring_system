# Phase 3: Error Handling Preparation - monitoring_system

**Version**: 1.0
**Date**: 2025-10-09
**Status**: Ready for Implementation

---

## Overview

This document outlines the migration path for monitoring_system to fully adopt the centralized error handling from common_system Phase 3, replacing the current hybrid wrapper approach.

---

## Current State

### Error Handling Status

**Current Approach**:
- Custom `monitoring_system::result<T>` wrapper around `common::Result<T>`
- Custom `monitoring_error_code` enum (1000-9999 range)
- Conditional compilation based on `MONITORING_HAS_COMMON_RESULT`
- Wrapper provides monitoring-specific error codes and conversion

**Example**:
```cpp
// Current: Custom result wrapper
monitoring_system::result<performance_metrics> get_metrics(
    const std::string& operation_name
) const;

// Current: Custom error codes
enum class monitoring_error_code : std::uint32_t {
    success = 0,
    collector_not_found = 1000,
    storage_full = 2000,
    // ...
};

// Current: Wrapper implementation
#if MONITORING_HAS_COMMON_RESULT
    common::Result<T> value_;
    mutable std::optional<error_info> error_;
#else
    std::variant<T, error_info> value_;
#endif
```

---

## Migration Plan

### Phase 3.1: Error Code Migration

**Action**: Map monitoring_error_code values to common_system error codes (-300 to -399)

**Error Code Mapping**:

```cpp
// Before: monitoring_system::monitoring_error_code
collector_not_found = 1000
collection_failed = 1001
storage_full = 2000
metric_not_found = 6000

// After: common::error::codes::monitoring_system
metric_not_found = -300
invalid_metric_type = -301
metric_collection_failed = -302
storage_full = -320
storage_error = -321
event_publish_failed = -340
event_subscribe_failed = -341
invalid_event_type = -342
profiler_not_enabled = -360
profiler_error = -361
```

### Phase 3.2: Remove Wrapper Layer

**Action**: Use `common::Result<T>` directly instead of `monitoring_system::result<T>`

```cpp
// Before (Wrapper)
class result<T> {
#if MONITORING_HAS_COMMON_RESULT
    common::Result<T> value_;
    mutable std::optional<error_info> error_;
#else
    std::variant<T, error_info> value_;
#endif
};

// After (Direct)
#include <kcenon/common/patterns/result.h>
#include <kcenon/common/error/error_codes.h>

using namespace common;

// Use common::Result<T> directly
Result<performance_metrics> get_metrics(
    const std::string& operation_name
) const;
```

### Phase 3.3: Key API Migrations

#### Priority 1: Performance Profiler Operations

```cpp
// Before
monitoring_system::result<bool> record_sample(
    const std::string& operation_name,
    std::chrono::nanoseconds duration,
    bool success = true
);

monitoring_system::result<performance_metrics> get_metrics(
    const std::string& operation_name
) const;

// After
Result<void> record_sample(
    const std::string& operation_name,
    std::chrono::nanoseconds duration,
    bool success = true
);

Result<performance_metrics> get_metrics(
    const std::string& operation_name
) const;
```

**Error Codes**:
- `codes::monitoring_system::profiler_not_enabled`
- `codes::monitoring_system::profiler_error`
- `codes::monitoring_system::metric_not_found`

**Example Implementation**:
```cpp
Result<void> performance_profiler::record_sample(
    const std::string& operation_name,
    std::chrono::nanoseconds duration,
    bool success
) {
    if (!enabled_) {
        return error<std::monostate>(
            codes::monitoring_system::profiler_not_enabled,
            "Profiler is disabled",
            "performance_profiler"
        );
    }

    std::unique_lock<std::shared_mutex> lock(profiles_mutex_);

    auto it = profiles_.find(operation_name);
    if (it == profiles_.end()) {
        profiles_[operation_name] = std::make_unique<profile_data>();
        it = profiles_.find(operation_name);
    }

    try {
        std::lock_guard<std::mutex> data_lock(it->second->mutex);

        it->second->samples.push_back(duration);
        it->second->call_count.fetch_add(1);
        if (!success) {
            it->second->error_count.fetch_add(1);
        }

        // Limit sample size
        if (it->second->samples.size() > max_samples_per_operation_) {
            it->second->samples.erase(it->second->samples.begin());
        }

        return ok();
    } catch (const std::exception& e) {
        return error<std::monostate>(
            codes::monitoring_system::profiler_error,
            "Failed to record sample",
            "performance_profiler",
            e.what()
        );
    }
}

Result<performance_metrics> performance_profiler::get_metrics(
    const std::string& operation_name
) const {
    std::shared_lock<std::shared_mutex> lock(profiles_mutex_);

    auto it = profiles_.find(operation_name);
    if (it == profiles_.end()) {
        return error<performance_metrics>(
            codes::monitoring_system::metric_not_found,
            "Operation not found",
            "performance_profiler",
            operation_name
        );
    }

    performance_metrics metrics;
    metrics.operation_name = operation_name;

    std::lock_guard<std::mutex> data_lock(it->second->mutex);
    metrics.call_count = it->second->call_count.load();
    metrics.error_count = it->second->error_count.load();
    metrics.update_statistics(it->second->samples);

    return ok(std::move(metrics));
}
```

#### Priority 2: System Monitor Operations

```cpp
// Before
monitoring_system::result<system_metrics> get_current_metrics() const;
monitoring_system::result<bool> start_monitoring(
    std::chrono::milliseconds interval = std::chrono::milliseconds(1000)
);
monitoring_system::result<bool> stop_monitoring();

// After
Result<system_metrics> get_current_metrics() const;
Result<void> start_monitoring(
    std::chrono::milliseconds interval = std::chrono::milliseconds(1000)
);
Result<void> stop_monitoring();
```

**Error Codes**:
- `codes::monitoring_system::metric_collection_failed`
- `codes::common::already_exists` (for already started)
- `codes::common::not_initialized` (for not started)

**Example Implementation**:
```cpp
Result<system_metrics> system_monitor::get_current_metrics() const {
    if (!impl_) {
        return error<system_metrics>(
            codes::common::not_initialized,
            "System monitor not initialized",
            "system_monitor"
        );
    }

    try {
        system_metrics metrics = impl_->collect_current_metrics();
        return ok(std::move(metrics));
    } catch (const std::exception& e) {
        return error<system_metrics>(
            codes::monitoring_system::metric_collection_failed,
            "Failed to collect system metrics",
            "system_monitor",
            e.what()
        );
    }
}

Result<void> system_monitor::start_monitoring(
    std::chrono::milliseconds interval
) {
    if (!impl_) {
        return error<std::monostate>(
            codes::common::not_initialized,
            "System monitor not initialized",
            "system_monitor"
        );
    }

    if (impl_->is_running()) {
        return error<std::monostate>(
            codes::common::already_exists,
            "Monitoring already started",
            "system_monitor"
        );
    }

    try {
        impl_->start(interval);
        return ok();
    } catch (const std::exception& e) {
        return error<std::monostate>(
            codes::common::internal_error,
            "Failed to start monitoring",
            "system_monitor",
            e.what()
        );
    }
}
```

#### Priority 3: Event Bus Operations

```cpp
// Before
result_void publish_event_impl(std::type_index event_type, std::any event) override;
result<subscription_token> subscribe_event_impl(
    std::type_index event_type,
    std::function<void(const std::any&)> handler,
    uint64_t handler_id,
    event_priority priority
) override;

// After
Result<void> publish_event_impl(std::type_index event_type, std::any event) override;
Result<subscription_token> subscribe_event_impl(
    std::type_index event_type,
    std::function<void(const std::any&)> handler,
    uint64_t handler_id,
    event_priority priority
) override;
```

**Error Codes**:
- `codes::monitoring_system::event_publish_failed`
- `codes::monitoring_system::event_subscribe_failed`
- `codes::monitoring_system::storage_full` (queue full)

**Example Implementation**:
```cpp
Result<void> event_bus::publish_event_impl(
    std::type_index event_type,
    std::any event
) {
    if (config_.enable_back_pressure) {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (event_queue_.size() >= config_.max_queue_size) {
            total_events_dropped_.fetch_add(1);
            return error<std::monostate>(
                codes::monitoring_system::storage_full,
                "Event queue is full",
                "event_bus",
                std::to_string(event_queue_.size())
            );
        }
    }

    try {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        event_queue_.emplace(event_type, std::move(event), event_priority::normal);
        total_events_published_.fetch_add(1);
        queue_cv_.notify_one();
        return ok();
    } catch (const std::exception& e) {
        return error<std::monostate>(
            codes::monitoring_system::event_publish_failed,
            "Failed to publish event",
            "event_bus",
            e.what()
        );
    }
}

Result<subscription_token> event_bus::subscribe_event_impl(
    std::type_index event_type,
    std::function<void(const std::any&)> handler,
    uint64_t handler_id,
    event_priority priority
) {
    try {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        handlers_[event_type].emplace_back(std::move(handler), priority, handler_id);

        // Sort handlers by priority
        std::sort(handlers_[event_type].begin(), handlers_[event_type].end(),
                 [](const event_handler_wrapper& a, const event_handler_wrapper& b) {
                     return static_cast<int>(a.priority) > static_cast<int>(b.priority);
                 });

        return ok(subscription_token(event_type, handler_id));
    } catch (const std::exception& e) {
        return error<subscription_token>(
            codes::monitoring_system::event_subscribe_failed,
            "Failed to subscribe to event",
            "event_bus",
            e.what()
        );
    }
}
```

---

## Migration Checklist

### Code Changes

- [ ] Map all monitoring_error_code values to common error codes
- [ ] Replace `monitoring_system::result<T>` with `common::Result<T>`
- [ ] Replace `result_void` with `common::Result<void>` or `common::VoidResult`
- [ ] Remove wrapper layer and conditional compilation
- [ ] Migrate performance_profiler operations
- [ ] Migrate system_monitor operations
- [ ] Migrate event_bus operations
- [ ] Update all helper functions (make_success, make_error)
- [ ] Replace MONITORING_TRY macros with RETURN_IF_ERROR
- [ ] Add error context (operation names, values, etc.)

### Test Updates

- [ ] Update unit tests for common::Result<T> APIs
- [ ] Add error case tests for each error code
- [ ] Test profiler disabled scenarios
- [ ] Test system monitor initialization failures
- [ ] Test event bus queue overflow
- [ ] Test metric not found scenarios
- [ ] Verify error message quality

### Documentation

- [ ] Update API reference with common::Result<T> signatures
- [ ] Document error code mappings
- [ ] Add migration examples for existing code
- [ ] Update integration examples
- [ ] Document breaking changes

---

## Example Migrations

### Example 1: Basic Profiler Usage

```cpp
// Usage before
auto result = profiler.get_metrics("my_operation");
if (!result) {
    auto error = result.get_error();
    log_error("Failed: {}", error.message);
    return;
}
performance_metrics metrics = result.value();

// Usage after
auto result = profiler.get_metrics("my_operation");
if (result.is_err()) {
    const auto& err = result.error();
    log_error("Failed: {}", err.message);
    return;
}
performance_metrics metrics = result.value();
```

### Example 2: Monadic Event Handling

```cpp
// Chain event bus operations
auto result = event_bus.start()
    .and_then([&](auto) {
        return event_bus.subscribe<my_event>([](const my_event& evt) {
            // Handle event
        });
    })
    .and_then([&](subscription_token token) {
        return event_bus.publish(my_event{});
    })
    .or_else([](const error_info& err) {
        log_error("Event bus setup failed: {}", err.message);
        return ok(subscription_token{});
    });
```

### Example 3: System Monitor with Error Recovery

```cpp
// Start monitoring with retry
Result<void> start_monitoring_with_retry(
    system_monitor& monitor,
    int max_attempts = 3
) {
    for (int attempt = 1; attempt <= max_attempts; ++attempt) {
        auto result = monitor.start_monitoring();
        if (result.is_ok()) {
            return result;
        }

        if (attempt < max_attempts) {
            log_warn("Start attempt {} failed: {}",
                     attempt, result.error().message);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        } else {
            return result;
        }
    }
    return ok();
}
```

---

## Error Code Mapping

### Monitoring System Error Codes (-300 to -399)

```cpp
namespace common::error::codes::monitoring_system {
    // Metric errors (-300 to -319)
    constexpr int metric_not_found = -300;
    constexpr int invalid_metric_type = -301;
    constexpr int metric_collection_failed = -302;

    // Storage errors (-320 to -339)
    constexpr int storage_full = -320;
    constexpr int storage_error = -321;

    // Event errors (-340 to -359)
    constexpr int event_publish_failed = -340;
    constexpr int event_subscribe_failed = -341;
    constexpr int invalid_event_type = -342;

    // Profiler errors (-360 to -379)
    constexpr int profiler_not_enabled = -360;
    constexpr int profiler_error = -361;
}
```

### Mapping Table

| Old Code | New Code | Category |
|----------|----------|----------|
| collector_not_found (1000) | metric_not_found (-300) | Metrics |
| collection_failed (1001) | metric_collection_failed (-302) | Metrics |
| invalid_metric_type (6001) | invalid_metric_type (-301) | Metrics |
| storage_full (2000) | storage_full (-320) | Storage |
| storage_write_failed (2004) | storage_error (-321) | Storage |
| event_publish_failed (custom) | event_publish_failed (-340) | Events |
| event_subscribe_failed (custom) | event_subscribe_failed (-341) | Events |
| profiler_not_enabled (custom) | profiler_not_enabled (-360) | Profiler |

### Error Messages

| Code | Message | When to Use |
|------|---------|-------------|
| metric_not_found | "Metric not found" | Metric lookup failed |
| metric_collection_failed | "Metric collection failed" | Collection error |
| storage_full | "Metric storage full" | Queue capacity reached |
| event_publish_failed | "Event publish failed" | Publication error |
| event_subscribe_failed | "Event subscribe failed" | Subscription error |
| profiler_not_enabled | "Profiler not enabled" | Use when disabled |

---

## Testing Strategy

### Unit Tests

```cpp
TEST(MonitoringPhase3, ProfilerDisabled) {
    performance_profiler profiler;
    profiler.set_enabled(false);

    auto result = profiler.record_sample(
        "test_op",
        std::chrono::nanoseconds(1000)
    );

    ASSERT_TRUE(result.is_err());
    EXPECT_EQ(
        codes::monitoring_system::profiler_not_enabled,
        result.error().code
    );
    EXPECT_EQ("performance_profiler", result.error().module);
}

TEST(MonitoringPhase3, MetricNotFound) {
    performance_profiler profiler;
    profiler.set_enabled(true);

    auto result = profiler.get_metrics("nonexistent");

    ASSERT_TRUE(result.is_err());
    EXPECT_EQ(
        codes::monitoring_system::metric_not_found,
        result.error().code
    );
}

TEST(MonitoringPhase3, EventQueueOverflow) {
    event_bus_config cfg;
    cfg.max_queue_size = 2;
    cfg.enable_back_pressure = true;
    cfg.auto_start = true;

    event_bus bus(cfg);

    // Fill queue
    for (int i = 0; i < 2; ++i) {
        auto result = bus.publish(test_event{});
        ASSERT_TRUE(result.is_ok());
    }

    // Overflow
    auto result = bus.publish(test_event{});
    ASSERT_TRUE(result.is_err());
    EXPECT_EQ(
        codes::monitoring_system::storage_full,
        result.error().code
    );
}
```

### Integration Tests

```cpp
TEST(MonitoringPhase3, FullWorkflow) {
    performance_monitor monitor;

    // Initialize
    auto init_result = monitor.initialize();
    ASSERT_TRUE(init_result.is_ok());

    // Record samples
    {
        auto timer = monitor.time_operation("test_op");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Get metrics
    auto metrics_result = monitor.get_profiler().get_metrics("test_op");
    ASSERT_TRUE(metrics_result.is_ok());

    const auto& metrics = metrics_result.value();
    EXPECT_EQ("test_op", metrics.operation_name);
    EXPECT_EQ(1, metrics.call_count);

    // Cleanup
    auto cleanup_result = monitor.cleanup();
    ASSERT_TRUE(cleanup_result.is_ok());
}
```

---

## Performance Impact

### Expected Overhead

- **Result<T> size**: Direct use removes wrapper overhead (-24 bytes per result)
- **Success path**: ~0-1ns (same as before, inline optimization)
- **Error path**: ~2-3ns (no wrapper conversion needed)

### Current Performance (With Wrapper)

- **Event publish**: ~150ns (includes wrapper overhead)
- **Metric collection**: ~200ns (includes wrapper overhead)
- **Profiler record**: ~50ns (minimal wrapper impact)

### Expected After Migration

- **Event publish**: ~145ns (-3% due to removing wrapper)
- **Metric collection**: ~195ns (-2.5%)
- **Profiler record**: ~50ns (no change)

**Conclusion**: Slight performance improvement (~2-3%) from removing wrapper layer

---

## Implementation Timeline

### Week 1: Foundation
- Day 1-2: Map error codes and update error_codes.h
- Day 3-4: Remove wrapper layer, replace with direct common::Result<T>
- Day 5: Update core tests

### Week 2: API Migration
- Day 1-2: Migrate performance_profiler
- Day 3: Migrate system_monitor
- Day 4-5: Migrate event_bus

### Week 3: Finalization
- Day 1-2: Migrate remaining collectors and adapters
- Day 3: Integration tests and performance validation
- Day 4-5: Documentation and code review

---

## Backwards Compatibility

### Strategy: Gradual Deprecation

The migration from monitoring_system::result<T> to common::Result<T> is a breaking change. Strategy:

```cpp
// Phase 3.1: Add type aliases (transitional)
namespace monitoring_system {
    template<typename T>
    using result [[deprecated("Use common::Result<T> directly")]] = common::Result<T>;

    using result_void [[deprecated("Use common::VoidResult directly")]] = common::VoidResult;
}

// Phase 3.2: Update code to use common::Result<T>
// Phase 3.3: Remove aliases (after 6 months)
```

**Timeline**:
- Phase 3.1: Add deprecation warnings
- Phase 3.2: Update all internal code
- Phase 3.3: Remove deprecated aliases (after 6 months)

---

## Special Considerations

### IMonitor Interface Compatibility

monitoring_system implements `common::interfaces::IMonitor`, which already returns `common::VoidResult` and `common::Result<T>`. These methods are already compatible:

```cpp
// Already using common::Result<T> (Phase 2.3.4)
common::VoidResult record_metric(const std::string& name, double value) override;
common::Result<common::interfaces::metrics_snapshot> get_metrics() override;
common::Result<common::interfaces::health_check_result> check_health() override;
```

### Error Info Conversion

The current code has a conversion layer between monitoring_system::error_info and common::error_info. This should be removed:

```cpp
// Before: Conversion needed
inline common::error_info to_common_error(const error_info& error) {
    common::error_info info(static_cast<int>(error.code), error.message, "monitoring_system");
    if (error.context) {
        info.details = error.context;
    }
    return info;
}

// After: Use common::error_info directly
// No conversion needed
```

---

## References

- [common_system Error Codes](../../common_system/include/kcenon/common/error/error_codes.h)
- [Error Handling Guidelines](../../common_system/docs/ERROR_HANDLING.md)
- [Result<T> Implementation](../../common_system/include/kcenon/common/patterns/result.h)
- [IMonitor Interface](../../common_system/include/kcenon/common/interfaces/monitoring_interface.h)

---

**Document Status**: Phase 3 Preparation Complete
**Next Action**: Begin implementation or await approval
**Maintainer**: monitoring_system team
