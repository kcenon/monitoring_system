# Phase 4: Dependency Injection Pattern Migration Guide

**Document Version**: 1.0
**Date**: 2025-10-02
**Status**: Active

## Overview

Phase 4 introduces Dependency Injection (DI) patterns to monitoring_system, enabling loose coupling with logger_system through common_system interfaces. This eliminates circular dependencies and provides flexible integration options.

---

## What Changed in Phase 4

### Key Improvements

1. **Logger Injection via DI**
   - Loggers are now injected through `ILogger` interface
   - No compile-time dependency on specific logger implementations
   - Support for monitor-only operation (logger optional)

2. **Interface-Based Architecture**
   - Monitor only depends on `ILogger` and `IMonitor` interfaces
   - Any logger implementation can be injected
   - Runtime logger swapping supported

3. **IMonitor and IMonitorProvider Implementation**
   - `performance_monitor` implements `IMonitor` interface
   - Supports `IMonitorProvider` for factory patterns
   - Enables monitor aggregation and composition

4. **No Circular Dependencies**
   - Removed `logger_system_adapter` concrete dependencies
   - Uses interface-based communication only
   - Clean separation of concerns

---

## Migration Examples

### Example 1: Basic Logger Injection

**Before (Phase 3):**
```cpp
#include <monitoring/core/performance_monitor.h>

// Monitor created without logger integration
auto monitor = std::make_shared<performance_monitor>();
```

**After (Phase 4 - with DI):**
```cpp
#include <monitoring/core/performance_monitor.h>
#include <kcenon/common/interfaces/logger_interface.h>

// Create or obtain a logger (any ILogger implementation)
std::shared_ptr<ILogger> logger = /* ... */;

// Inject logger into monitor via DI
auto monitor = std::make_shared<performance_monitor>();
monitor->set_logger(logger);

// Monitor now logs events automatically
monitor->record_metric("requests", 100.0);  // Logged to injected logger
```

### Example 2: Monitor Without Logger (Optional)

**New in Phase 4:**
```cpp
// Monitor works perfectly without logger
auto monitor = std::make_shared<performance_monitor>();

// All operations work without logger
monitor->record_metric("metric1", 42.0);
auto metrics = monitor->get_metrics();
auto health = monitor->check_health();

// Logger is truly optional
```

### Example 3: Runtime Logger Management

**New in Phase 4:**
```cpp
auto monitor = std::make_shared<performance_monitor>();

// Phase 1: No logger
monitor->record_metric("phase1", 10.0);

// Phase 2: Inject logger at runtime
std::shared_ptr<ILogger> logger = create_logger();
monitor->set_logger(logger);
monitor->record_metric("phase2", 20.0);  // Now logged

// Phase 3: Swap to different logger
monitor->set_logger(different_logger);
monitor->record_metric("phase3", 30.0);  // Logged to new logger
```

### Example 4: Using IMonitor Interface

**New in Phase 4:**
```cpp
#include <kcenon/common/interfaces/monitoring_interface.h>

// performance_monitor implements IMonitor
std::shared_ptr<IMonitor> monitor = std::make_shared<performance_monitor>();

// Use through interface
monitor->record_metric("test", 1.0);

auto metrics = monitor->get_metrics();
if (metrics) {
    std::cout << "Metrics: " << metrics.value().metrics.size() << std::endl;
}

auto health = monitor->check_health();
if (health) {
    std::cout << "Status: " << to_string(health.value().status) << std::endl;
}
```

---

## Integration with logger_system

### Pattern 1: Logger Provided by logger_system

```cpp
// In your application initialization

// Step 1: Create logger from logger_system
auto logger = logger_system::create_logger();

// Step 2: Create monitor from monitoring_system
auto monitor = std::make_shared<performance_monitor>();

// Step 3: Inject logger into monitor (DI)
monitor->set_logger(logger);

// Step 4: Monitor automatically logs events
monitor->record_metric("app_metric", 50.0);  // Event logged via injected logger
```

### Pattern 2: Bidirectional Integration

```cpp
// Create both systems
auto logger = logger_system::create_logger();
auto monitor = monitoring_system::create_monitor();

// Setup bidirectional relationship via DI
logger->set_monitor(monitor);   // Logger -> Monitor (metrics)
monitor->set_logger(logger);    // Monitor -> Logger (events)

// Both cooperate without circular dependencies
logger->log(log_level::info, "Event");  // Sends metrics to monitor
monitor->record_metric("metric", 1.0);  // Logs event via logger
```

### Pattern 3: Factory-Based Integration

```cpp
// Create factory with shared logger
auto factory = monitor_factory::instance();
auto shared_logger = /* ... */;

factory->set_shared_logger(shared_logger);

// All monitors from factory use shared logger
auto monitor1 = factory->create_monitor("service_a");
auto monitor2 = factory->create_monitor("service_b");

// Both automatically use shared logger
monitor1->record_metric("metric_a", 10.0);  // Logged
monitor2->record_metric("metric_b", 20.0);  // Logged
```

---

## Factory and Provider Patterns

### Using Monitor Factory

```cpp
#include <monitoring/factories/monitor_factory.h>

// Get singleton factory instance
auto factory = monitor_factory::instance();

// Get default monitor
auto monitor = factory->get_monitor();

// Create named monitors
auto web_monitor = factory->create_monitor("web_server");
auto db_monitor = factory->create_monitor("database");

// List all monitors
auto names = factory->list_monitors();
for (const auto& name : names) {
    std::cout << "Monitor: " << name << std::endl;
}
```

### Using IMonitorProvider Interface

```cpp
// Use factory through provider interface
std::shared_ptr<IMonitorProvider> provider = get_provider();

// Get monitor via provider
auto monitor = provider->get_monitor();

// Create named monitor via provider
auto named = provider->create_monitor("my_monitor");
```

### Aggregating Monitors

```cpp
class aggregating_monitor : public IMonitor {
    std::vector<std::shared_ptr<IMonitor>> monitors_;

public:
    void add_monitor(std::shared_ptr<IMonitor> monitor) {
        monitors_.push_back(monitor);
    }

    VoidResult record_metric(const std::string& name, double value) override {
        // Broadcast to all monitors
        for (auto& m : monitors_) {
            m->record_metric(name, value);
        }
        return VoidResult::success();
    }

    // ... other methods
};

// Usage
auto aggregator = std::make_shared<aggregating_monitor>();
aggregator->add_monitor(monitor1);
aggregator->add_monitor(monitor2);

// Single call broadcasts to all
aggregator->record_metric("broadcast", 100.0);
```

---

## Best Practices

### 1. Depend on Interfaces, Not Implementations

**Good:**
```cpp
void setup_monitoring(std::shared_ptr<ILogger> logger) {
    auto monitor = std::make_shared<performance_monitor>();
    monitor->set_logger(logger);  // Interface-based
}
```

**Avoid:**
```cpp
// Don't depend on concrete types
void setup_monitoring(std::shared_ptr<concrete_logger> logger) {
    // Creates tight coupling
}
```

### 2. Make Logger Optional

**Good:**
```cpp
auto monitor = std::make_shared<performance_monitor>();

// Add logger only if available
if (auto logger = get_optional_logger()) {
    monitor->set_logger(logger);
}

// Monitor works either way
monitor->record_metric("metric", 1.0);
```

### 3. Use Provider Patterns

**Good:**
```cpp
// Get monitor from provider
if (auto provider = get_monitor_provider()) {
    auto monitor = provider->get_monitor();
    use_monitor(monitor);
}
```

### 4. Check Logger Capability at Runtime

**Good:**
```cpp
auto monitor = get_monitor();

if (monitor->get_logger()) {
    std::cout << "Monitor has logger attached" << std::endl;
} else {
    std::cout << "Monitor operating without logger" << std::endl;
}
```

---

## Removed Components

### logger_system_adapter (Removed in Phase 4)

**Old Code (Phase 3):**
```cpp
#include <monitoring/adapters/logger_system_adapter.h>
#include <kcenon/logger/core/logger.h>  // Concrete dependency!

auto adapter = std::make_shared<logger_system_adapter>(bus, logger);
```

**New Code (Phase 4):**
```cpp
// Use direct logger injection instead
#include <kcenon/common/interfaces/logger_interface.h>

auto monitor = std::make_shared<performance_monitor>();
monitor->set_logger(logger);  // Interface-based, no adapter needed
```

**Migration Steps:**
1. Remove `#include <monitoring/adapters/logger_system_adapter.h>`
2. Remove adapter creation code
3. Use `set_logger()` for direct injection
4. Update to use `ILogger` interface instead of concrete logger types

---

## Testing DI Integration

### Unit Test Example

```cpp
#include <gtest/gtest.h>
#include <monitoring/core/performance_monitor.h>

class MockLogger : public ILogger {
public:
    int log_count = 0;

    VoidResult log(log_level, const std::string&) override {
        log_count++;
        return VoidResult::success();
    }

    bool is_enabled(log_level) const override { return true; }
};

TEST(MonitorDITest, LoggerInjection) {
    auto mock_logger = std::make_shared<MockLogger>();
    auto monitor = std::make_shared<performance_monitor>();

    monitor->set_logger(mock_logger);

    EXPECT_EQ(monitor->get_logger(), mock_logger);

    // Verify logger receives events
    monitor->record_metric("test", 1.0);

    EXPECT_GT(mock_logger->log_count, 0);
}

TEST(MonitorDITest, OptionalLogger) {
    auto monitor = std::make_shared<performance_monitor>();

    EXPECT_EQ(monitor->get_logger(), nullptr);

    // Should not crash
    EXPECT_NO_THROW(monitor->record_metric("test", 1.0));
}

TEST(MonitorDITest, LoggerSwapping) {
    auto logger1 = std::make_shared<MockLogger>();
    auto logger2 = std::make_shared<MockLogger>();

    auto monitor = std::make_shared<performance_monitor>();

    monitor->set_logger(logger1);
    monitor->set_logger(logger2);

    EXPECT_EQ(monitor->get_logger(), logger2);
}
```

---

## Performance Considerations

### Logger Injection Overhead

- **Logger check**: ~1-2 ns (pointer null check)
- **Event logging**: Depends on logger implementation
- **No logger**: Zero overhead when logger is nullptr

### Recommendations

1. **For High-Performance Scenarios**:
   ```cpp
   // No logger for maximum performance
   auto monitor = std::make_shared<performance_monitor>();
   ```

2. **For Production Monitoring**:
   ```cpp
   // Use async logger for minimal overhead
   auto monitor = std::make_shared<performance_monitor>();
   monitor->set_logger(async_logger);
   ```

3. **For Development/Debug**:
   ```cpp
   // Use sync logger for immediate feedback
   auto monitor = std::make_shared<performance_monitor>();
   monitor->set_logger(sync_logger);
   ```

---

## Troubleshooting

### Issue: Logger Not Receiving Events

**Solution:**
```cpp
// Verify logger is set
if (monitor->get_logger() == nullptr) {
    monitor->set_logger(your_logger);
}

// Check logger is enabled for the event level
if (!logger->is_enabled(log_level::debug)) {
    // Increase logger level or use higher event level
}
```

### Issue: Circular Dependency Errors

**Solution:**
```cpp
// Use forward declarations and interfaces only
#include <kcenon/common/interfaces/logger_interface.h>      // Interface
#include <kcenon/common/interfaces/monitoring_interface.h>  // Interface

// DO NOT include concrete implementations in headers
// #include <kcenon/logger/core/logger.h>         // Avoid
// #include <monitoring/core/performance_monitor.h>  // Avoid in shared headers
```

### Issue: Monitor Not Implementing IMonitor

**Solution:**
```cpp
// Ensure you're using performance_monitor or compatible type
auto monitor = std::make_shared<performance_monitor>();  // Implements IMonitor

// Verify interface
std::shared_ptr<IMonitor> imonitor = monitor;
if (imonitor) {
    std::cout << "Monitor implements IMonitor ✓" << std::endl;
}
```

---

## API Reference

### Logger Management API

```cpp
void performance_monitor::set_logger(std::shared_ptr<ILogger> logger);
std::shared_ptr<ILogger> performance_monitor::get_logger() const;
```
- Inject or replace logger at runtime
- Logger can be nullptr for logger-less operation

### IMonitor Interface

```cpp
VoidResult record_metric(const std::string& name, double value) override;
VoidResult record_metric(const std::string& name, double value,
    const std::unordered_map<std::string, std::string>& tags) override;
Result<metrics_snapshot> get_metrics() override;
Result<health_check_result> check_health() override;
VoidResult reset() override;
```
- Implemented by `performance_monitor`
- Standard monitoring interface

### IMonitorProvider Interface

```cpp
std::shared_ptr<IMonitor> get_monitor() override;
std::shared_ptr<IMonitor> create_monitor(const std::string& name) override;
```
- Implemented by `monitor_factory`
- Provides monitor instances

---

## Examples

See the following example programs:
- `examples/logger_di_integration_example.cpp` - Logger DI patterns
- `examples/monitor_factory_pattern_example.cpp` - Factory and provider patterns

Build examples:
```bash
cd monitoring_system
mkdir build && cd build
cmake .. -DMONITORING_BUILD_EXAMPLES=ON -DBUILD_WITH_COMMON_SYSTEM=ON
cmake --build .
./logger_di_integration_example
./monitor_factory_pattern_example
```

---

## Summary

Phase 4 introduces:
- ✅ Logger injection via DI
- ✅ Runtime logger swapping
- ✅ Optional logger support
- ✅ IMonitor interface implementation
- ✅ IMonitorProvider for factories
- ✅ Zero circular dependencies
- ✅ Interface-based loose coupling
- ✅ Removed `logger_system_adapter`

**Key Benefits:**
- No compile-time dependencies on logger_system
- Work with any ILogger implementation
- Flexible runtime configuration
- Clean separation of concerns
- Easy testing with mock loggers

**Next Steps:**
- Review example programs
- Update your monitor creation code to use DI
- Remove old adapter code
- Test with your logger implementations
- Refer to this guide for migration patterns

---

**Document Version**: 1.0
**Last Updated**: 2025-10-02
**Next Review**: Phase 5 Start
