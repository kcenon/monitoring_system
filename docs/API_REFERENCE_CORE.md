---
doc_id: "MON-API-CORE-001"
doc_title: "Monitoring System API Reference - Core"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "monitoring_system"
category: "API"
---

# Monitoring System API Reference - Core

> **SSOT**: This document is the single source of truth for the **Monitoring System API - Core Components**, including result types, thread context, dependency injection, monitoring interfaces, performance monitoring, C++20 concepts, and baseline implementation details.

For collector class APIs, see [API_REFERENCE_COLLECTORS.md](API_REFERENCE_COLLECTORS.md).
For alerts, exporters, tracing, reliability, and storage APIs, see [API_REFERENCE_ALERTS_EXPORT.md](API_REFERENCE_ALERTS_EXPORT.md).

**Phase 4 - Current Implementation Status**

This document describes the **actually implemented** APIs and interfaces in the monitoring system Phase 4. Items marked with *(Stub Implementation)* provide functional interfaces but with simplified implementations as foundation for future development.

## Table of Contents

1. [Core Components](#core-components) **Implemented**
2. [Monitoring Interfaces](#monitoring-interfaces) **Implemented**
3. [Performance Monitoring](#performance-monitoring) **Implemented**
4. [C++20 Concepts](#c20-concepts)
5. [Error Codes](#error-codes)
6. [Thread Safety](#thread-safety)
7. [Performance Considerations](#performance-considerations)
8. [Best Practices](#best-practices)
9. [Migration Guide](#migration-guide)
10. [Test Coverage](#test-coverage-37-tests-passing-100-success-rate) **37 Tests Passing**
11. [Current Implementation Status](#current-implementation-status)
12. [Migration Notes for Phase 4](#migration-notes-for-phase-4)

---

## Core Components

### Result Types **Fully Implemented**
**Header:** `include/kcenon/monitoring/core/result_types.h`

> **MIGRATION NOTICE**: The monitoring-specific result types are now deprecated in favor of `common::Result<T>` from common_system. Existing code will continue to work, but new code should use the common_system types directly.

#### Migration Guide

| Deprecated | Replacement |
|------------|-------------|
| `result<T>` | `common::Result<T>` |
| `result_void` | `common::VoidResult` |
| `make_success<T>(value)` | `common::ok(value)` |
| `make_error<T>(code, msg)` | `common::make_error<T>(code, msg, module)` |
| `make_void_success()` | `common::ok()` |
| `make_void_error(code, msg)` | `common::VoidResult::err(error_info)` |
| `MONITORING_TRY(expr)` | `COMMON_RETURN_IF_ERROR(expr)` |
| `MONITORING_TRY_ASSIGN(var, expr)` | `COMMON_ASSIGN_OR_RETURN(var, expr)` |

See `result_types.h` for detailed migration examples.

#### `result<T>` *(Deprecated)*
A monadic result type for error handling without exceptions. **Fully tested with 13 passing tests.**

```cpp
template<typename T>
class result {
public:
    // Constructors
    result(const T& value);
    result(T&& value);
    result(const error_info& error);

    // Check if result contains a value
    bool has_value() const;
    bool is_ok() const;
    bool is_error() const;
    explicit operator bool() const;

    // Access the value
    T& value();
    const T& value() const;
    T value_or(const T& default_value) const;

    // Access the error
    error_info get_error() const;

    // Monadic operations
    template<typename F>
    auto map(F&& f) const;

    template<typename F>
    auto and_then(F&& f) const;
};
```

#### `result_void` *(Deprecated)*
Specialized result type for operations that don't return values.

```cpp
class result_void {
public:
    static result_void success();
    static result_void error(const error_info& error);

    bool is_ok() const;
    bool is_error() const;
    error_info get_error() const;
};
```

**Usage Example:**
```cpp
result<int> divide(int a, int b) {
    if (b == 0) {
        return result<int>(error_info{
            .code = monitoring_error_code::invalid_argument,
            .message = "Division by zero"
        });
    }
    return result<int>(a / b);
}

// Chain operations
auto result = divide(10, 2)
    .map([](int x) { return x * 2; })
    .and_then([](int x) { return divide(x, 3); });
```

### Thread Context **Fully Implemented**
**Header:** `include/kcenon/monitoring/context/thread_context.h`

#### `thread_context`
Manages thread-local context for correlation and tracing. **Fully tested with 6 passing tests.**

```cpp
class thread_context {
public:
    // Get current thread context
    static context_metadata& current();

    // Create new context with optional request ID
    static context_metadata& create(const std::string& request_id = "");

    // Clear current context
    static void clear();

    // Generate unique IDs
    static std::string generate_request_id();
    static std::string generate_correlation_id();

    // Check if context exists
    static bool has_current();
};
```

#### `context_metadata`
Thread-local context information for tracing and correlation.

```cpp
struct context_metadata {
    std::string request_id;
    std::string correlation_id;
    std::string trace_id;
    std::string span_id;
    std::chrono::system_clock::time_point created_at;
    std::unordered_map<std::string, std::string> baggage;
};
```

### Dependency Injection Container **Fully Implemented**
**Header:** `include/kcenon/monitoring/di/di_container.h`

#### `di_container`
Lightweight dependency injection container for managing service instances. **Fully tested with 9 passing tests.**

```cpp
class di_container {
public:
    // Service registration
    template<typename T>
    void register_transient(std::function<std::shared_ptr<T>()> factory);

    template<typename T>
    void register_singleton(std::function<std::shared_ptr<T>()> factory);

    template<typename T>
    void register_singleton_instance(std::shared_ptr<T> instance);

    // Named service registration
    template<typename T>
    void register_named(const std::string& name,
                       std::function<std::shared_ptr<T>()> factory);

    // Service resolution
    template<typename T>
    std::shared_ptr<T> resolve();

    template<typename T>
    std::shared_ptr<T> resolve_named(const std::string& name);

    // Utilities
    void clear();
    bool has_service(const std::string& type_name) const;
    size_t service_count() const;
};
```

**Usage Example:**
```cpp
di_container container;

// Register services
container.register_singleton<database_service>([]() {
    return std::make_shared<sqlite_database>();
});

container.register_transient<user_service>([&]() {
    auto db = container.resolve<database_service>();
    return std::make_shared<user_service>(db);
});

// Resolve services
auto user_svc = container.resolve<user_service>();
```

---

## Monitoring Interfaces

### Monitoring Interface
**Header:** `include/kcenon/monitoring/interfaces/monitoring_core.h`

> **Note:** This header was renamed from `monitoring_interface.h` to avoid naming
> collision with `common_system`'s `monitoring_interface.h` (which defines the
> `IMonitor` interface). The deprecated forwarding header has been removed.

#### `metrics_collector`
Base interface for all metric collectors.

```cpp
class metrics_collector {
public:
    virtual std::string get_name() const = 0;
    virtual bool is_enabled() const = 0;
    virtual common::VoidResult set_enabled(bool enable) = 0;
    virtual common::VoidResult initialize() = 0;
    virtual common::VoidResult cleanup() = 0;
    virtual common::Result<metrics_snapshot> collect() = 0;
};
```

#### `monitorable`
Interface for objects that can be monitored.

```cpp
class monitorable {
public:
    virtual std::string get_name() const = 0;
    virtual common::Result<metrics_snapshot> get_metrics() const = 0;
    virtual common::VoidResult reset_metrics() = 0;
    virtual common::Result<std::string> get_status() const = 0;
};
```

---

## Performance Monitoring

### Performance Monitor
**Header:** `sources/monitoring/performance/performance_monitor.h`

#### `performance_monitor`
Monitors system and application performance metrics.

```cpp
class performance_monitor : public metrics_collector {
public:
    explicit performance_monitor(const std::string& name = "performance_monitor");
    
    // Create scoped timer
    scoped_timer time_operation(const std::string& operation_name);
    
    // Get profiler
    performance_profiler& get_profiler();
    
    // Get system monitor
    system_monitor& get_system_monitor();
    
    // Set thresholds
    void set_cpu_threshold(double threshold);
    void set_memory_threshold(double threshold);
    void set_latency_threshold(std::chrono::milliseconds threshold);
};
```

#### `scoped_timer`
RAII timer for measuring operation duration.

```cpp
class scoped_timer {
public:
    scoped_timer(performance_profiler* profiler, const std::string& operation_name);
    ~scoped_timer();
    
    void mark_failed();
    void complete();
    std::chrono::nanoseconds elapsed() const;
};
```

### Adaptive Optimizer
**Header:** `sources/monitoring/performance/adaptive_optimizer.h`

#### `adaptive_optimizer`
Dynamically optimizes monitoring parameters based on system load.

```cpp
class adaptive_optimizer {
public:
    struct optimization_config {
        double cpu_threshold = 80.0;
        double memory_threshold = 90.0;
        double target_overhead = 5.0;
        std::chrono::seconds adaptation_interval{60};
    };
    
    explicit adaptive_optimizer(const optimization_config& config = {});
    
    common::Result<bool> start();
    common::Result<bool> stop();
    common::Result<optimization_decision> analyze_and_optimize();
    common::Result<bool> apply_optimization(const optimization_decision& decision);
};
```

---

## Basic Usage Examples

### Basic Monitoring Setup
```cpp
// Create monitoring instance
monitoring_builder builder;
auto monitoring = builder
    .with_history_size(1000)
    .with_collection_interval(1s)
    .add_collector(std::make_unique<performance_monitor>())
    .with_storage(std::make_unique<sqlite_storage_backend>(config))
    .enable_compression(true)
    .build();

// Start monitoring
monitoring.value()->start();
```

---

## Error Codes

Common error codes used throughout the system:

```cpp
enum class monitoring_error_code {
    success = 0,
    unknown_error,
    invalid_argument,
    out_of_range,
    not_found,
    already_exists,
    permission_denied,
    resource_exhausted,
    operation_cancelled,
    operation_timeout,
    not_implemented,
    internal_error,
    unavailable,
    data_loss,
    unauthenticated,
    circuit_breaker_open,
    retry_exhausted,
    validation_failed,
    initialization_failed,
    configuration_error
};
```

---

## Thread Safety

All public interfaces in the monitoring system are thread-safe unless explicitly documented otherwise. Internal synchronization uses:
- `std::mutex` for exclusive access
- `std::shared_mutex` for read-write locks
- `std::atomic` for lock-free operations
- Thread-local storage for context management

---

## Performance Considerations

1. **Metric Collection**: Designed for minimal overhead (<5% CPU)
2. **Storage**: Async writes with batching for efficiency
3. **Tracing**: Sampling support to reduce overhead
4. **Health Checks**: Cached results with configurable TTL
5. **Circuit Breakers**: Lock-free state transitions
6. **Stream Processing**: Zero-copy where possible

---

## Best Practices

1. **Error Handling**: Always check `common::Result<T>` return values
2. **Resource Management**: Use RAII patterns (scoped_timer, scoped_span)
3. **Configuration**: Validate configs before use
4. **Monitoring**: Start with conservative thresholds, tune based on metrics
5. **Tracing**: Use sampling in production to control overhead
6. **Health Checks**: Keep checks lightweight and fast
7. **Storage**: Choose backend based on durability vs performance needs

---

## Migration Guide

### From Direct Metrics to Collectors
```cpp
// Old way
metrics.record("latency", 150.0);

// New way
auto timer = perf_monitor.time_operation("process_request");
// ... operation ...
timer.complete();
```

### From Callbacks to Health Checks
```cpp
// Old way
register_health_callback([]() { return check_health(); });

// New way
monitor.register_check("service",
    std::make_shared<functional_health_check>(
        "service_check",
        health_check_type::liveness,
        []() { return check_health(); }
    )
);
```

---

## Version Compatibility

- C++20 required (C++17 no longer supported)
- Thread support required
- C++20 Concepts used for type-safe APIs with clear error messages
- Compatible with: GCC 13+, Clang 17+, MSVC 2022+, Apple Clang 14+

## C++20 Concepts

The monitoring system uses C++20 Concepts for compile-time type validation with clear, actionable error messages.

### Available Concepts

#### Event Bus Concepts (`interfaces/event_bus_interface.h`)
```cpp
namespace kcenon::monitoring::concepts {
    // A type that can be used as an event (class type, copy-constructible)
    template <typename T>
    concept EventType = std::is_class_v<T> && std::is_copy_constructible_v<T>;

    // A callable that handles events (invocable with const E&, returns void)
    template <typename H, typename E>
    concept EventHandler = std::invocable<H, const E&> &&
        std::is_void_v<std::invoke_result_t<H, const E&>>;

    // A callable that filters events (invocable with const E&, returns bool)
    template <typename F, typename E>
    concept EventFilter = std::invocable<F, const E&> &&
        std::convertible_to<std::invoke_result_t<F, const E&>, bool>;
}
```

#### Metric Collector Concepts (`interfaces/metric_collector_interface.h`)
```cpp
namespace kcenon::monitoring::concepts {
    // A configuration type that can validate itself
    template <typename T>
    concept Validatable = requires(const T t) {
        { t.validate() };
    };

    // A type that provides metrics
    template <typename T>
    concept MetricSourceLike = requires(const T t) {
        { t.get_current_metrics() };
        { t.get_source_name() } -> std::convertible_to<std::string>;
        { t.is_healthy() } -> std::convertible_to<bool>;
    };

    // A type that collects metrics
    template <typename T>
    concept MetricCollectorLike = requires(T t) {
        { t.collect_metrics() };
        { t.is_collecting() } -> std::convertible_to<bool>;
        { t.get_metric_types() };
    };
}
```

### Usage Examples

#### Type-Safe Event Publishing
```cpp
// Only class types that are copy-constructible can be published
struct my_event { std::string data; };
event_bus.publish_event(my_event{"hello"});  // OK

// Compile error: int is not a class type
// event_bus.publish_event(42);  // Error: concepts::EventType not satisfied
```

#### Constrained Event Handlers
```cpp
// Handler with correct signature
event_bus.subscribe_event<my_event>([](const my_event& e) {
    std::cout << e.data << std::endl;
});

// Handler must return void - compile error otherwise
// event_bus.subscribe_event<my_event>([](const my_event& e) {
//     return e.data.size();  // Error: must return void
// });
```

#### Configuration Validation
```cpp
// collection_config satisfies Validatable concept
collection_config config;
config.interval = std::chrono::seconds(1);
auto result = config.validate();  // Compile-time verified to exist
if (result.is_err()) {
    // Handle validation error
}
```

### Benefits of Concepts

1. **Clear Error Messages**: Instead of cryptic SFINAE errors, you get:
   ```
   error: constraints not satisfied for 'publish_event'
   note: concept 'EventType<int>' was not satisfied
   ```

2. **Self-Documenting APIs**: Concept names describe requirements explicitly

3. **Better IDE Support**: Accurate auto-completion and type hints

4. **Code Simplification**: No more `std::enable_if` boilerplate

---

## Test Coverage **37 Tests Passing (100% Success Rate)**

### Test Suite Status

**Phase 4 Test Results:** All core functionality is thoroughly tested with comprehensive test coverage.

| Test Suite | Tests | Status | Coverage |
|-------------|-------|--------|----------|
| **Result Types** | 13 tests | PASS | Complete error handling, Result<T> pattern, metadata operations |
| **DI Container** | 9 tests | PASS | Service registration, resolution, singleton/transient lifecycles |
| **Monitorable Interface** | 9 tests | PASS | Basic monitoring interfaces and stub implementations |
| **Thread Context** | 6 tests | PASS | Thread-safe context management and ID generation |

### Running Tests

```bash
# Navigate to build directory
cd build

# Run all tests
./tests/monitoring_system_tests

# Run specific test suites
./tests/monitoring_system_tests --gtest_filter="ResultTypesTest.*"
./tests/monitoring_system_tests --gtest_filter="DIContainerTest.*"
./tests/monitoring_system_tests --gtest_filter="MonitorableInterfaceTest.*"
./tests/monitoring_system_tests --gtest_filter="ThreadContextTest.*"

# List all available tests
./tests/monitoring_system_tests --gtest_list_tests
```

### Test Implementation Details

#### Result Types Test Coverage
- Success and error result creation
- Value access and error handling
- `value_or()` default value behavior
- Monadic operations (`map`, `and_then`)
- Result void operations
- Error code to string conversion
- Metadata operations

#### DI Container Test Coverage
- Transient service registration and resolution
- Singleton service lifecycle management
- Named service registration
- Instance registration
- Service resolution verification
- Container state management

#### Monitorable Interface Test Coverage
- Basic interface implementations
- Metrics collection stubs
- Status reporting
- Configuration management
- Monitoring lifecycle

#### Thread Context Test Coverage
- Context creation and retrieval
- Request ID and correlation ID generation
- Thread-local storage behavior
- Context clearing and state management

---

## Current Implementation Status

### Fully Implemented & Tested
- **Result Types**: Complete error handling framework
- **DI Container**: Full dependency injection functionality
- **Thread Context**: Thread-safe context management
- **Core Interfaces**: Basic monitoring abstractions

### Stub Implementations (Foundation for Future Development)
- **Performance Monitoring**: Interface complete, basic implementation
- **Distributed Tracing**: Interface complete, stub functionality
- **Storage Backends**: Interface complete, file storage stub
- **Health Monitoring**: Interface complete, basic health checks
- **Circuit Breakers**: Interface complete, basic state management

### Future Implementation Phases
- Advanced alerting system
- Real-time web dashboard
- Advanced storage backends
- OpenTelemetry integration
- Stream processing capabilities

---

## Migration Notes for Phase 4

Phase 4 focuses on **core foundation stability** rather than feature completeness. This approach provides:

1. **Solid Foundation**: All core types and patterns are fully implemented and tested
2. **Extensible Architecture**: Stub implementations provide clear interfaces for future expansion
3. **Production Ready Core**: Error handling, DI, and context management are production-quality
4. **Incremental Development**: Features can be added incrementally without breaking existing code

---

## Further Reading

- [API_REFERENCE.md](API_REFERENCE.md) - API Reference index
- [API_REFERENCE_COLLECTORS.md](API_REFERENCE_COLLECTORS.md) - Collector class APIs
- [API_REFERENCE_ALERTS_EXPORT.md](API_REFERENCE_ALERTS_EXPORT.md) - Alerts, exporters, tracing, reliability, storage APIs
- [Phase 4 Documentation](advanced/ARCHITECTURE_GUIDE.md) - Phase 4 implementation status and architecture decisions
- [Architecture Guide](ARCHITECTURE.md) - System design and patterns
- [Examples](../examples/) - Working code examples
- [Changelog](CHANGELOG.md) - Version history and changes

---

*Last Updated: 2026-02-08*
