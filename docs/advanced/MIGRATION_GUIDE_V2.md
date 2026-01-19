# Migration Guide: Interface-Based Architecture

> **Language:** **English** | [한국어](MIGRATION_GUIDE_V2.kr.md)

**Target Audience**: Developers using logger_system and/or monitoring_system
**Estimated Migration Time**: 1-2 hours for typical projects
**Breaking Changes**: Minimal (interface-based)

---

## Table of Contents

1. [Overview](#overview)
2. [Quick Migration Path](#quick-migration-path)
3. [Detailed Changes](#detailed-changes)
4. [Common Scenarios](#common-scenarios)
5. [Troubleshooting](#troubleshooting)
6. [FAQ](#faq)

---

## Overview

### What Changed?

The new architecture eliminates circular dependencies between logger_system and monitoring_system by:

1. **Standardizing interfaces** in `common_system`
2. **Removing concrete dependencies** between systems
3. **Introducing dependency injection** for runtime integration

### Why Migrate?

✅ **No circular dependencies** - Clean compile-time dependency graph
✅ **Better testability** - Easy to mock and test components
✅ **Flexibility** - Mix and match any ILogger/IMonitor implementations
✅ **Future-proof** - Extensible without modifying existing code

---

## Quick Migration Path

### For logger_system Users

**Before**:
```cpp
#include <kcenon/logger/core/logger.h>

auto logger = logger_builder().build();
```

**After** - Same code works!:
```cpp
#include <kcenon/logger/core/logger.h>

auto logger = logger_builder().build();

// Optional: Add monitoring
auto monitor = std::make_shared<performance_monitor>();
logger->set_monitor(monitor);
```

**Impact**: ✅ **Zero breaking changes** for basic logger usage

---

### For monitoring_system Users

**Before**:
```cpp
#include <kcenon/monitoring/monitoring.h>

auto monitor = std::make_shared<performance_monitor>();
monitor->record_metric("test", 42.0);
```

**After** - Same code works!:
```cpp
#include <kcenon/monitoring/monitoring.h>

auto monitor = std::make_shared<performance_monitor>();
monitor->record_metric("test", 42.0);

// Optional: Add logging
auto logger = /* any ILogger implementation */;
// Configure adapter to use logger
```

**Impact**: ✅ **Zero breaking changes** for basic monitoring usage

---

### For Users of Both Systems

**Before** - This caused circular dependency!:
```cpp
// DON'T DO THIS - circular dependency!
#include <kcenon/logger/core/logger.h>
#include <kcenon/monitoring/core/performance_monitor.h>

auto logger = logger_builder().build();
auto monitor = std::make_shared<performance_monitor>();
// Hard-coded concrete dependencies
```

**After** - Clean dependency injection:
```cpp
#include <kcenon/logger/core/logger_builder.h>
#include <kcenon/monitoring/core/performance_monitor.h>

// Create both systems
auto logger_result = logger_builder().build();
auto logger = std::move(logger_result.value());

auto monitor = std::make_shared<performance_monitor>();

// Inject dependencies (NO circular dependency!)
logger->set_monitor(monitor);  // Logger uses IMonitor interface
// Adapter can use ILogger interface

// Both systems work together seamlessly
logger->log(log_level::info, "System started");
monitor->record_metric("startup_time", 125.5);
```

**Impact**: ✅ **Cleaner code** with explicit dependency injection

---

## Detailed Changes

### 1. Interface Locations Changed

#### logger_system

**Old**:
```cpp
#include <kcenon/logger/core/monitoring/monitoring_interface.h>
using kcenon::logger::monitoring::monitoring_interface;
```

**New**:
```cpp
#include <kcenon/common/interfaces/monitoring_interface.h>
using common::interfaces::IMonitor;
```

**Migration Action**:
```bash
# Automated migration script
sed -i 's|kcenon/logger/core/monitoring/monitoring_interface.h|kcenon/common/interfaces/monitoring_interface.h|g' *.cpp *.h
sed -i 's|kcenon::logger::monitoring::monitoring_interface|common::interfaces::IMonitor|g' *.cpp *.h
```

---

#### monitoring_system

**Old**:
```cpp
// Direct dependency on logger_system
#include <kcenon/logger/core/logger.h>
auto logger = std::make_shared<kcenon::logger::logger>();
```

**New**:
```cpp
// Use interface instead
#include <kcenon/common/interfaces/logger_interface.h>
using common::interfaces::ILogger;

// Any ILogger implementation works
std::shared_ptr<ILogger> logger = /* inject via DI */;
```

---

### 2. Monitoring Interface Updates

#### Before
```cpp
class my_monitor : public monitoring_interface {
    monitoring_data get_monitoring_data() const override;
    bool is_healthy() const override;
    health_status get_health_status() const override;
};
```

#### After
```cpp
class my_monitor : public common::interfaces::IMonitor {
    common::VoidResult record_metric(const std::string& name, double value) override;
    common::Result<metrics_snapshot> get_metrics() override;
    common::Result<health_check_result> check_health() override;
    common::VoidResult reset() override;
};
```

**Key Differences**:
- Return types use `Result<T>` pattern for better error handling
- Method names standardized across all systems
- Richer health check results with metadata

---

### 3. Logger Integration via DI

#### Before (v1.x) - Compile-time dependency
```cpp
// This created circular dependency
#include <kcenon/logger/core/logger.h>  // Concrete class!

void setup_monitoring() {
    auto logger = std::make_shared<kcenon::logger::logger>();
    // Hard-coded dependency
}
```

#### After (v2.0) - Runtime dependency injection
```cpp
// Use interface, no concrete dependency
#include <kcenon/common/interfaces/logger_interface.h>

void setup_monitoring(std::shared_ptr<common::interfaces::ILogger> logger) {
    // Logger injected, works with ANY ILogger implementation
    auto adapter = std::make_shared<logger_system_adapter>(bus, logger);
}
```

---

## Common Scenarios

### Scenario 1: Standalone Logger (No Monitoring)

**Code**:
```cpp
#include <kcenon/logger/core/logger_builder.h>

auto logger_result = logger_builder()
    .with_async(true)
    .with_min_level(log_level::info)
    .build();

if (!logger_result) {
    // Handle error
    return;
}

auto logger = std::move(logger_result.value());
logger->log(log_level::info, "Application started");
```

**Migration**: ✅ No changes needed

---

### Scenario 2: Standalone Monitor (No Logger)

**Code**:
```cpp
#include <kcenon/monitoring/core/performance_monitor.h>

auto monitor = std::make_shared<performance_monitor>();
monitor->record_metric("cpu_usage", 45.5);

auto health = monitor->check_health();
if (std::holds_alternative<health_check_result>(health)) {
    auto& result = std::get<health_check_result>(health);
    // Process health result
}
```

**Migration**: ✅ No changes needed

---

### Scenario 3: Logger with Monitoring

**Before**:
```cpp
auto logger = logger_builder()
    .with_monitoring_enabled(true)  // Old API
    .build();
```

**After**:
```cpp
auto monitor = std::make_shared<performance_monitor>();
auto logger = logger_builder()
    .with_monitoring(monitor)  // New: explicit DI
    .build()
    .value();
```

**Why Better?**:
- Explicit dependency
- Can use ANY IMonitor implementation
- Easier to test with mocks

---

### Scenario 4: Monitor with Logging

**Before**:
```cpp
// Monitoring system had hard-coded logger_system dependency
auto monitor = create_monitor_with_logging();  // How?
```

**After**:
```cpp
// Create logger (any ILogger implementation)
auto logger = logger_builder().build().value();

// Create monitor
auto monitor = std::make_shared<performance_monitor>();

// Use adapter for integration
auto bus = std::make_shared<event_bus>();
auto adapter = std::make_shared<logger_system_adapter>(bus, logger);

// Adapter collects logger metrics
auto metrics = adapter->collect_metrics();
```

**Why Better?**:
- No compile-time dependency on logger_system
- Works with ANY ILogger (not just logger_system's logger)
- Testable with mock loggers

---

### Scenario 5: Bidirectional Integration

**After** - The sweet spot!:
```cpp
// Create both systems
auto logger = logger_builder().build().value();
auto monitor = std::make_shared<performance_monitor>();

// Bidirectional injection (NO CIRCULAR DEPENDENCY!)
logger->set_monitor(monitor);  // Logger reports to monitor

auto bus = std::make_shared<event_bus>();
auto adapter = std::make_shared<logger_system_adapter>(bus, logger);
// Adapter can collect logger metrics

// Use both systems
for (int i = 0; i < 100; ++i) {
    logger->log(log_level::info, "Request " + std::to_string(i));
    monitor->record_metric("requests", i + 1);
}

// Check combined health
auto logger_health = logger->health_check();
auto monitor_health = monitor->check_health();
```

---

## Troubleshooting

### Issue 1: Compile Error - "monitoring_interface not found"

**Error**:
```
error: 'monitoring_interface' does not name a type
```

**Cause**: Old include path

**Fix**:
```cpp
// Remove old include
// #include <kcenon/logger/core/monitoring/monitoring_interface.h>

// Add new include
#include <kcenon/common/interfaces/monitoring_interface.h>
using common::interfaces::IMonitor;
```

---

### Issue 2: Linker Error - "undefined reference to logger"

**Error**:
```
undefined reference to `kcenon::logger::logger::logger()'
```

**Cause**: Trying to use concrete logger class in monitoring_system

**Fix**:
```cpp
// Don't do this in monitoring_system
// #include <kcenon/logger/core/logger.h>
// auto logger = std::make_shared<kcenon::logger::logger>();

// Do this instead - use interface + DI
#include <kcenon/common/interfaces/logger_interface.h>
void setup(std::shared_ptr<common::interfaces::ILogger> logger) {
    // Logger injected from outside
}
```

---

### Issue 3: Type Mismatch - "cannot convert Result to result"

**Error**:
```
cannot convert 'common::Result<T>' to 'monitoring_system::result<T>'
```

**Cause**: Mixed use of old and new result types

**Fix**: Use `common::Result<T>` consistently:
```cpp
#include <kcenon/common/patterns/result.h>

common::Result<metrics_snapshot> get_metrics() {
    // Implementation
}
```

---

### Issue 4: Deprecation Warnings

**Warning**:
```
warning: 'monitoring_interface' is deprecated
```

**Cause**: Using legacy type aliases

**Fix**:
```cpp
// Old (deprecated but works)
using monitoring_interface = common::interfaces::IMonitor;

// New (preferred)
using common::interfaces::IMonitor;
```

---

## FAQ

### Q1: Do I need to migrate immediately?

**A**: No. Existing code continues to work with deprecation warnings. However, migration is recommended for:
- New projects
- Code that needs testing improvements
- Systems requiring flexibility

---

### Q2: Can I mix old and new code?

**A**: Yes, but not recommended. The transition headers provide compatibility:
```cpp
// This still works (deprecated)
#include <kcenon/logger/core/monitoring/monitoring_interface_transition.h>
```

---

### Q3: What if I don't need both logger and monitor?

**A**: Perfect! That's a key benefit. Each system is now truly independent:
```cpp
// Just logger
auto logger = logger_builder().build().value();

// Just monitor
auto monitor = std::make_shared<performance_monitor>();
```

---

### Q4: How do I test with the new design?

**A**: Much easier! Create mock implementations:
```cpp
class mock_logger : public common::interfaces::ILogger {
    // Minimal implementation for testing
};

class mock_monitor : public common::interfaces::IMonitor {
    // Minimal implementation for testing
};

// Use mocks in tests
auto mock_log = std::make_shared<mock_logger>();
system_under_test->set_logger(mock_log);
```

---

### Q5: What about performance?

**A**: Negligible impact:
- Interface calls have same cost as virtual functions (already used previously)
- DI happens once at startup
- Runtime performance: < 5% overhead
- Benchmark results: [PHASE3_VERIFICATION_REPORT.md](../../PHASE3_VERIFICATION_REPORT.md)

---

### Q6: Can I gradually migrate?

**A**: Yes! Recommended approach:
1. Start with new code using v2.0 interfaces
2. Leave existing code unchanged (works with deprecation warnings)
3. Gradually update modules as you touch them
4. Run automated migration script when ready

---

### Q7: Where can I find examples?

**A**: Check these files:
- `examples/bidirectional_di_example.cpp` - Full DI demonstration
- `tests/test_cross_system_integration.cpp` - Integration tests
- `tests/test_adapter_functionality.cpp` - Adapter tests

---

## Automated Migration Script

```bash
#!/bin/bash
# migrate_to_interfaces.sh - Automated migration helper

echo "Migrating codebase to interface-based architecture..."

# 1. Update include paths
find . -type f \( -name "*.cpp" -o -name "*.h" \) -exec sed -i \
    's|kcenon/logger/core/monitoring/monitoring_interface\.h|kcenon/common/interfaces/monitoring_interface.h|g' {} \;

# 2. Update namespace
find . -type f \( -name "*.cpp" -o -name "*.h" \) -exec sed -i \
    's|kcenon::logger::monitoring::monitoring_interface|common::interfaces::IMonitor|g' {} \;

find . -type f \( -name "*.cpp" -o -name "*.h" \) -exec sed -i \
    's|kcenon::logger::monitoring::health_status|common::interfaces::health_status|g' {} \;

# 3. Update logger_system types
find . -type f \( -name "*.cpp" -o -name "*.h" \) -exec sed -i \
    's|logger_system::result|common::Result|g' {} \;

# 4. Update monitoring_system types
find . -type f \( -name "*.cpp" -o -name "*.h" \) -exec sed -i \
    's|monitoring_system::result_void|common::VoidResult|g' {} \;

echo "✓ Migration complete!"
echo ""
echo "Next steps:"
echo "1. Review changes: git diff"
echo "2. Build project: cmake --build build"
echo "3. Run tests: ctest --test-dir build"
echo "4. Fix any remaining issues manually"
```

---

## Summary

✅ **Key Takeaways**:
1. Basic usage remains the same
2. Advanced usage gains flexibility via DI
3. No circular dependencies
4. Better testability
5. Future-proof architecture

📚 **Additional Resources**:
- [Phase 3 Verification Report](../../PHASE3_VERIFICATION_REPORT.md)
- [Bidirectional DI Example](../examples/bidirectional_di_example.cpp)
- [API Documentation](https://docs.example.com)

🆘 **Need Help?**:
- GitHub Issues: https://github.com/kcenon/monitoring_system/issues
- GitHub Issues: https://github.com/kcenon/logger_system/issues

---

**Last Updated**: 2025-10-02
