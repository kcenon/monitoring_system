# DI Container and C++20 Concepts Guide

> **Language:** **English** | Advanced Guide
>
> **Source Reference**: This guide documents actual implementations from the monitoring_system codebase.
> All code examples are derived from real source files.

## Table of Contents

- [Overview](#overview)
- [1. Dependency Injection Container](#1-dependency-injection-container)
  - [Architecture](#di-architecture)
  - [Service Registration](#service-registration)
  - [Service Resolution](#service-resolution)
  - [Lifecycle Management](#lifecycle-management)
  - [The Adapter Pattern](#the-adapter-pattern)
  - [Pre-Configured Instance Registration](#pre-configured-instance-registration)
  - [Accessing Underlying Monitor](#accessing-underlying-monitor)
  - [Unregistration](#unregistration)
  - [Feature Flag Guards](#feature-flag-guards)
  - [Testing with DI](#testing-with-di)
- [2. Metric Factory](#2-metric-factory)
  - [Factory Architecture](#factory-architecture)
  - [The collector_interface](#the-collector_interface)
  - [Registering Collectors](#registering-collectors)
  - [Creating Collectors](#creating-collectors)
  - [Batch Creation](#batch-creation)
  - [Static Registration Macro](#static-registration-macro)
  - [Collector Adapters](#collector-adapters)
- [3. C++20 Concepts](#3-c20-concepts)
  - [Why Concepts?](#why-concepts)
  - [Available Concepts](#available-concepts)
  - [Value and Type Concepts](#value-and-type-concepts)
  - [Component Concepts](#component-concepts)
  - [Callable Concepts](#callable-concepts)
  - [Infrastructure Concepts](#infrastructure-concepts)
  - [Using Concepts in Your Code](#using-concepts-in-your-code)
  - [Compiler Error Messages](#compiler-error-messages)
  - [Compiler Requirements](#compiler-requirements)
- [4. Adaptive Monitor](#4-adaptive-monitor)
  - [How Adaptive Monitoring Works](#how-adaptive-monitoring-works)
  - [Load Levels and Thresholds](#load-levels-and-thresholds)
  - [Configuration](#adaptive-configuration)
  - [Adaptation Strategies](#adaptation-strategies)
  - [Hysteresis and Cooldown](#hysteresis-and-cooldown)
  - [The Adaptive Collector](#the-adaptive-collector)
  - [The Adaptive Monitor Controller](#the-adaptive-monitor-controller)
  - [RAII Scope Management](#raii-scope-management)
  - [Monitoring Adaptation Statistics](#monitoring-adaptation-statistics)
- [5. Putting It All Together](#5-putting-it-all-together)
  - [Complete DI + Factory Setup](#complete-di--factory-setup)
  - [Concept-Constrained Custom Types](#concept-constrained-custom-types)
  - [Adaptive Monitoring Pipeline](#adaptive-monitoring-pipeline)
- [Related Documentation](#related-documentation)

---

## Overview

The monitoring system employs several advanced C++ patterns for flexible, type-safe service composition:

| Pattern | Purpose | Source File |
|---------|---------|------------|
| **DI Container** | Service lifecycle management via common_system | [`di/service_registration.h`](../../include/kcenon/monitoring/di/service_registration.h) |
| **Metric Factory** | Collector creation and configuration | [`factory/metric_factory.h`](../../include/kcenon/monitoring/factory/metric_factory.h) |
| **Collector Adapters** | Bridge different collector types to factory | [`factory/collector_adapters.h`](../../include/kcenon/monitoring/factory/collector_adapters.h) |
| **C++20 Concepts** | Compile-time type validation | [`concepts/monitoring_concepts.h`](../../include/kcenon/monitoring/concepts/monitoring_concepts.h) |
| **Adaptive Monitor** | Self-tuning collection behavior | [`adaptive/adaptive_monitor.h`](../../include/kcenon/monitoring/adaptive/adaptive_monitor.h) |

**Dependency graph:**

```
common_system (DI container, IMonitor interface)
     │
     ▼
monitoring_system
├── DI Registration ──────► IServiceContainer (from common_system)
│   └── performance_monitor_adapter ──► IMonitor (from common_system)
│
├── Metric Factory ──────► collector_interface (local)
│   ├── plugin_collector_adapter<T>
│   ├── crtp_collector_adapter<T>
│   └── standalone_collector_adapter<T>
│
├── C++20 Concepts ──────► Compile-time validation
│   └── Extends common_system concepts
│
└── Adaptive Monitor ────► metrics_collector (local)
    └── adaptive_collector wrapper
```

---

## 1. Dependency Injection Container

> **Source**: [`di/service_registration.h`](../../include/kcenon/monitoring/di/service_registration.h) (208 lines)
> **Adapter**: [`adapters/performance_monitor_adapter.h`](../../include/kcenon/monitoring/adapters/performance_monitor_adapter.h) (233 lines)

The DI container integrates monitoring_system with common_system's `IServiceContainer`, enabling service resolution across the entire kcenon ecosystem.

### DI Architecture

```
┌─────────────────────────────────────────────────────┐
│              common_system                           │
│  ┌─────────────────────────────────────┐             │
│  │        IServiceContainer            │             │
│  │  ┌──────────────────────────┐       │             │
│  │  │ IMonitor  ─────────────────────────► consumer  │
│  │  │ ILogger                  │       │             │
│  │  │ IEventBus                │       │             │
│  │  └──────────────────────────┘       │             │
│  └──────────────┬──────────────────────┘             │
│                 │ register_factory<IMonitor>()        │
└─────────────────┼───────────────────────────────────┘
                  │
┌─────────────────┼───────────────────────────────────┐
│                 │  monitoring_system                  │
│                 ▼                                     │
│  register_monitor_services(container, config)        │
│       │                                              │
│       ├── Creates performance_monitor                │
│       ├── Configures thresholds                      │
│       ├── Initializes system monitoring              │
│       └── Wraps in performance_monitor_adapter       │
│                │                                     │
│                ▼                                     │
│    performance_monitor_adapter : IMonitor             │
│    ├── record_metric()    → IMonitor interface       │
│    ├── get_metrics()      → IMonitor interface       │
│    ├── check_health()     → IMonitor interface       │
│    └── get_wrapped_monitor() → performance_monitor   │
└──────────────────────────────────────────────────────┘
```

### Service Registration

Register monitoring services with the common_system DI container:

```cpp
#include <kcenon/monitoring/di/service_registration.h>
using namespace kcenon::monitoring::di;

// Get the global service container
auto& container = common::di::service_container::global();

// Option 1: Register with default configuration
auto result = register_monitor_services(container);
if (result.is_err()) {
    // Handle error (e.g., IMonitor already registered)
}

// Option 2: Register with custom configuration
monitor_registration_config config;
config.monitor_name = "app_monitor";
config.cpu_threshold = 90.0;              // Alert above 90% CPU
config.memory_threshold = 85.0;           // Alert above 85% memory
config.latency_threshold = std::chrono::milliseconds{500};
config.enable_system_monitoring = true;   // Initialize system metrics
config.enable_lock_free = true;           // Lock-free collection mode
config.lifetime = common::di::service_lifetime::singleton;

auto result = register_monitor_services(container, config);
```

**Configuration options:**

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `monitor_name` | `std::string` | `"default_performance_monitor"` | Instance identifier |
| `cpu_threshold` | `double` | `80.0` | CPU alert threshold (%) |
| `memory_threshold` | `double` | `90.0` | Memory alert threshold (%) |
| `latency_threshold` | `std::chrono::milliseconds` | `1000ms` | Latency alert threshold |
| `enable_system_monitoring` | `bool` | `true` | Auto-initialize system resource monitoring |
| `enable_lock_free` | `bool` | `false` | Enable lock-free collection mode |
| `lifetime` | `service_lifetime` | `singleton` | Service lifecycle (singleton/transient/scoped) |

### Service Resolution

Once registered, any part of the application can resolve the monitor:

```cpp
// Resolve the IMonitor interface from anywhere
auto monitor_result = container.resolve<common::interfaces::IMonitor>();
if (monitor_result.is_ok()) {
    auto monitor = monitor_result.value();

    // Use the standard IMonitor interface
    monitor->record_metric("requests_count", 42.0);
    monitor->record_metric("response_time_ms", 150.0,
        {{"service", "api"}, {"endpoint", "/users"}});

    // Get metrics snapshot
    auto metrics = monitor->get_metrics();

    // Health check
    auto health = monitor->check_health();
}
```

### Lifecycle Management

The DI container supports three service lifetimes:

| Lifetime | Behavior | Use Case |
|----------|----------|----------|
| **singleton** (default) | One instance shared across all consumers | Monitoring (shared state) |
| **transient** | New instance per resolution | Stateless services |
| **scoped** | One instance per scope | Request-scoped metrics |

```cpp
// Singleton (recommended for monitors)
config.lifetime = common::di::service_lifetime::singleton;
// All resolve() calls return the same instance

// Transient (new instance each time)
config.lifetime = common::di::service_lifetime::transient;
// Each resolve() creates a new performance_monitor + adapter
```

> **Recommendation**: Use `singleton` for monitors. Creating multiple performance_monitor instances wastes resources and fragments metrics across instances.

### The Adapter Pattern

The `performance_monitor_adapter` bridges monitoring_system's internal interface (`metrics_collector`) to common_system's standard interface (`IMonitor`) using composition:

```
┌───────────────────────────────────────────────────────┐
│ performance_monitor_adapter : IMonitor                 │
│                                                       │
│  ┌──────────────────┐    ┌─────────────────────────┐  │
│  │ IMonitor API      │    │ performance_monitor      │  │
│  │                   │    │                          │  │
│  │ record_metric()  ─┼──► │ record_counter(tags)     │  │
│  │ get_metrics()    ─┼──► │ get_profiler().get_all() │  │
│  │ check_health()   ─┼──► │ is_enabled()             │  │
│  │ reset()          ─┼──► │ reset()                  │  │
│  └──────────────────┘    └─────────────────────────┘  │
│                                                       │
│  Design: Composition over inheritance                  │
│  Reason: Avoids diamond inheritance problem            │
└───────────────────────────────────────────────────────┘
```

The adapter converts between the two worlds:

- **`record_metric(name, value, tags)`**: Forwards to `monitor->record_counter()` with tag conversion
- **`get_metrics()`**: Collects profiler timing metrics (min/max/mean/p95/p99/count) + tagged metrics
- **`check_health()`**: Maps `is_enabled()` to `healthy`/`degraded`/`unhealthy`

### Pre-Configured Instance Registration

If you need full control over `performance_monitor` setup:

```cpp
// Create and configure monitor manually
auto monitor = std::make_shared<performance_monitor>("custom_monitor");
monitor->set_cpu_threshold(95.0);
monitor->set_latency_threshold(std::chrono::milliseconds{200});
monitor->initialize();

// Register the pre-configured instance
auto result = register_monitor_instance(container, monitor);
// The instance is wrapped in performance_monitor_adapter automatically
```

### Accessing Underlying Monitor

When you need advanced features not exposed through `IMonitor`:

```cpp
// Resolve as IMonitor
auto imonitor = container.resolve<common::interfaces::IMonitor>().value();

// Cast back to get the underlying performance_monitor
auto perf_monitor = get_underlying_performance_monitor(imonitor);
if (perf_monitor) {
    // Access advanced features
    auto timer = perf_monitor->time_operation("database_query");
    // ... do work ...
    // timer auto-records duration on destruction

    // Access profiler directly
    auto& profiler = perf_monitor->get_profiler();
    auto metrics = profiler.get_all_metrics();
}
```

### Unregistration

Remove monitoring services from the container:

```cpp
auto result = unregister_monitor_services(container);
// The IMonitor instance is released when all consumers drop their references
```

### Feature Flag Guards

DI integration is conditionally compiled based on common_system availability:

```cpp
// From feature_flags.h
#if KCENON_HAS_COMMON_SYSTEM
    // DI container, IMonitor, concepts from common_system are available
    #include <kcenon/common/di/service_container.h>
#endif

// Related feature flags:
// KCENON_HAS_COMMON_DI       — DI container available
// KCENON_HAS_COMMON_IMONITOR — IMonitor interface available
// KCENON_HAS_COMMON_CONCEPTS — C++20 concepts from common_system available
```

Detection is automatic via `__has_include`:

```cpp
#if __has_include(<kcenon/common/config/feature_flags.h>)
    #define KCENON_HAS_COMMON_SYSTEM 1
#else
    #define KCENON_HAS_COMMON_SYSTEM 0
#endif
```

### Testing with DI

Replace the real monitor with a mock for unit testing:

```cpp
// Test setup: register a mock monitor
class mock_monitor : public common::interfaces::IMonitor {
public:
    common::VoidResult record_metric(const std::string& name, double value) override {
        recorded_metrics.push_back({name, value});
        return common::ok();
    }
    // ... implement other IMonitor methods ...

    std::vector<std::pair<std::string, double>> recorded_metrics;
};

// In test
auto& container = common::di::service_container::global();
auto mock = std::make_shared<mock_monitor>();
container.register_instance<common::interfaces::IMonitor>(mock);

// Now any code that resolves IMonitor gets the mock
run_application_code();

// Verify
ASSERT_EQ(mock->recorded_metrics.size(), expected_count);
```

---

## 2. Metric Factory

> **Source**: [`factory/metric_factory.h`](../../include/kcenon/monitoring/factory/metric_factory.h) (346 lines)
> **Adapters**: [`factory/collector_adapters.h`](../../include/kcenon/monitoring/factory/collector_adapters.h) (192 lines)

The metric factory provides centralized creation and configuration of metric collectors through a type-erased factory pattern.

### Factory Architecture

```
┌──────────────────────────────────────────────────────────┐
│                   metric_factory (Singleton)              │
│                                                          │
│  factories_: unordered_map<string, collector_factory_fn>  │
│  ┌────────────────────┬──────────────────────────────┐   │
│  │ "cpu_collector"    │ → plugin_collector_adapter<T> │   │
│  │ "memory_collector" │ → crtp_collector_adapter<T>   │   │
│  │ "disk_collector"   │ → standalone_collector_adapter│   │
│  │ "custom_collector" │ → direct factory function     │   │
│  └────────────────────┴──────────────────────────────┘   │
│                                                          │
│  create("cpu_collector", config)                         │
│       │                                                  │
│       ├── 1. Lock mutex, find factory                    │
│       ├── 2. Call factory() → unique_ptr<collector_interface> │
│       ├── 3. Call collector->initialize(config)           │
│       └── 4. Return create_result{collector, true, ""}   │
│                                                          │
│  Thread-safe: All operations protected by mutex_          │
└──────────────────────────────────────────────────────────┘
```

### The collector_interface

All collectors must implement this type-erased interface:

```cpp
class collector_interface {
public:
    virtual ~collector_interface() = default;

    // Initialize with configuration key-value pairs
    virtual bool initialize(const config_map& config) = 0;

    // Identification
    virtual std::string get_name() const = 0;

    // Health checking
    virtual bool is_healthy() const = 0;

    // Supported metric types
    virtual std::vector<std::string> get_metric_types() const = 0;
};
```

### Registering Collectors

**Method 1: Template registration (simplest)**

```cpp
auto& factory = metric_factory::instance();

// Register by type — factory creates instances automatically
factory.register_collector<my_cpu_collector>("cpu_collector");
// Internally: factory stores []() { return make_unique<my_cpu_collector>(); }
```

**Method 2: Lambda factory (custom construction)**

```cpp
factory.register_collector("custom_collector", []() {
    auto collector = std::make_unique<custom_collector>();
    collector->set_special_option(true);  // Pre-configure before initialize()
    return collector;
});
```

**Method 3: Static registration macro**

```cpp
// In your collector's .cpp file
REGISTER_COLLECTOR(my_cpu_collector);
// Expands to a static bool that registers at program startup
```

> **Note**: `REGISTER_COLLECTOR` uses the type name as the string key. The collector `my_cpu_collector` registers as `"my_cpu_collector"`.

**Duplicate prevention:**

```cpp
bool success = factory.register_collector<my_collector>("my_collector");
// success == true (first registration)

bool duplicate = factory.register_collector<other_collector>("my_collector");
// duplicate == false (name already taken)
```

### Creating Collectors

**Method 1: Full result (recommended)**

```cpp
config_map config = {
    {"enabled", "true"},
    {"interval_ms", "1000"},
    {"detailed", "false"}
};

auto result = factory.create("cpu_collector", config);
if (result) {
    // result.collector is a unique_ptr<collector_interface>
    auto& collector = result.collector;
    // Use collector...
} else {
    // result.error_message explains what went wrong
    log_error(result.error_message);
}
```

**Possible error messages:**

| Error | Cause |
|-------|-------|
| `"Unknown collector: X"` | Name not registered |
| `"Factory returned null for: X"` | Factory function returned nullptr |
| `"Failed to create collector 'X': ..."` | Exception during construction |
| `"Initialization failed for: X"` | `initialize()` returned false |
| `"Exception during initialization of 'X': ..."` | Exception during `initialize()` |

**Method 2: Null-on-failure (for optional collectors)**

```cpp
auto collector = factory.create_or_null("optional_collector", config);
if (collector) {
    // Use collector
}
// No error details available
```

### Batch Creation

Create multiple collectors from a configuration map:

```cpp
std::unordered_map<std::string, config_map> configs = {
    {"cpu_collector", {{"enabled", "true"}}},
    {"memory_collector", {{"enabled", "true"}, {"detailed", "true"}}},
    {"disk_collector", {{"enabled", "false"}}}
};

// Creates all, skips failures silently
auto collectors = factory.create_multiple(configs);
// collectors.size() <= configs.size() (only successful ones)
```

### Static Registration Macro

The `REGISTER_COLLECTOR` macro enables automatic registration at static initialization:

```cpp
// my_collector.h
class my_collector : public collector_interface {
public:
    bool initialize(const config_map& config) override { /* ... */ }
    std::string get_name() const override { return "my_collector"; }
    bool is_healthy() const override { return true; }
    std::vector<std::string> get_metric_types() const override {
        return {"gauge", "counter"};
    }
};

// my_collector.cpp
#include "my_collector.h"
REGISTER_COLLECTOR(my_collector);
// Now available: factory.create("my_collector", config)
```

**How it works internally:**

```cpp
#define REGISTER_COLLECTOR(CollectorType)                \
    namespace {                                           \
    static const bool CollectorType##_registered = []() { \
        return metric_factory::instance()                 \
            .register_collector<CollectorType>(#CollectorType); \
    }();                                                  \
    }
```

The lambda runs during static initialization before `main()`, registering the collector type with its name as the string key.

### Collector Adapters

Three adapter templates bridge existing collector types to the `collector_interface`:

| Adapter | For Collectors That... | Registration Helper |
|---------|----------------------|---------------------|
| `plugin_collector_adapter<T>` | Implement `metric_collector_plugin` | `register_plugin_collector<T>(name)` |
| `crtp_collector_adapter<T>` | Derive from `collector_base<T>` | `register_crtp_collector<T>(name)` |
| `standalone_collector_adapter<T>` | Have their own interface | `register_standalone_collector<T>(name)` |

**Example with each adapter type:**

```cpp
// Plugin-based collector
register_plugin_collector<system_resource_collector>("system_resource_collector");

// CRTP-based collector
register_crtp_collector<network_collector>("network_collector");

// Standalone collector (e.g., vm_collector)
register_standalone_collector<vm_collector>("vm_collector");

// Now all three can be created uniformly:
auto cpu = factory.create("system_resource_collector", config);
auto net = factory.create("network_collector", config);
auto vm = factory.create("vm_collector", config);
```

**Accessing the underlying collector:**

```cpp
auto result = factory.create("system_resource_collector", config);
if (result) {
    // Downcast to get the specific adapter
    auto* adapter = dynamic_cast<plugin_collector_adapter<system_resource_collector>*>(
        result.collector.get()
    );
    if (adapter) {
        // Access the original collector
        auto* original = adapter->get_collector();
        original->specific_method();
    }
}
```

---

## 3. C++20 Concepts

> **Source**: [`concepts/monitoring_concepts.h`](../../include/kcenon/monitoring/concepts/monitoring_concepts.h) (329 lines)

The monitoring system defines 13 C++20 concepts that provide compile-time type validation for monitoring components.

### Why Concepts?

Concepts replace SFINAE and `static_assert` with readable, composable constraints:

```cpp
// Without concepts (SFINAE — cryptic error messages)
template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
void record_metric(const std::string& name, T value);

// With concepts (clear constraint)
template <MetricValue V>
void record_metric(const std::string& name, V value);

// Compiler error with concepts:
//   "constraints not satisfied: MetricValue<std::string>"
// vs SFINAE:
//   "no matching function for call to 'record_metric'"
//   (followed by pages of template instantiation errors)
```

### Available Concepts

| Concept | Constraint | Category |
|---------|-----------|----------|
| `MetricValue` | Arithmetic type | Value |
| `MetricType` | Class with `.name` and `.value` | Type |
| `MetricSourceLike` | Provides metrics + health check | Component |
| `MetricCollectorLike` | Collects metrics | Component |
| `ObserverLike` | Receives metric updates | Component |
| `MonitoringEventType` | Copy-constructible class | Event |
| `MonitoringEventHandler<H,E>` | Callable handling event E | Callable |
| `MetricFilterPredicate<F,M>` | Filters metrics by criteria | Callable |
| `MetricTransformer<F,M>` | Transforms metrics | Callable |
| `ConfigValidatable` | Has `validate()` method | Infrastructure |
| `StorageBackendLike` | Stores and connects | Infrastructure |
| `ExporterLike` | Exports metrics externally | Infrastructure |
| `HealthCheckable` | Has `is_healthy()` method | Infrastructure |
| `TracingContextLike` | Provides trace/span IDs | Infrastructure |

### Value and Type Concepts

**`MetricValue`** — Any arithmetic type:

```cpp
template <typename T>
concept MetricValue = std::is_arithmetic_v<T>;

// Satisfies: int, double, float, uint64_t, bool
// Does NOT satisfy: std::string, std::vector<int>, custom_class
```

```cpp
template <MetricValue V>
void record(const std::string& name, V value) {
    store(name, static_cast<double>(value));
}

record("count", 42);        // OK: int is arithmetic
record("rate", 3.14);       // OK: double is arithmetic
record("name", "hello");    // ERROR: string is not arithmetic
```

**`MetricType`** — A structured metric with name and value:

```cpp
template <typename T>
concept MetricType =
    std::is_class_v<T> &&
    std::is_copy_constructible_v<T> &&
    requires(const T t) {
        { t.name } -> std::convertible_to<std::string>;
        { t.value } -> std::convertible_to<double>;
    };
```

```cpp
// This satisfies MetricType
struct cpu_metric {
    std::string name;
    double value;
};

// This does NOT (missing .name field)
struct bad_metric {
    double value;
};

template <MetricType M>
void publish(const M& metric) {
    send(metric.name, metric.value);
}
```

### Component Concepts

**`MetricSourceLike`** — Something that provides metrics:

```cpp
template <typename T>
concept MetricSourceLike = requires(const T t) {
    { t.get_current_metrics() };
    { t.get_source_name() } -> std::convertible_to<std::string>;
    { t.is_healthy() } -> std::convertible_to<bool>;
};
```

```cpp
// Implementing a concept-conforming source
class my_source {
public:
    std::vector<metric> get_current_metrics() const { /* ... */ }
    std::string get_source_name() const { return "my_source"; }
    bool is_healthy() const { return connected_; }
};
static_assert(MetricSourceLike<my_source>);  // Compile-time verification
```

**`MetricCollectorLike`** — Something that collects metrics:

```cpp
template <typename T>
concept MetricCollectorLike = requires(T t) {
    { t.collect_metrics() };
    { t.is_collecting() } -> std::convertible_to<bool>;
    { t.get_metric_types() };
};
```

**`ObserverLike`** — Something that receives metric updates:

```cpp
template <typename T>
concept ObserverLike = requires(T t) {
    { t.on_metrics_updated(std::declval<std::vector<int>>()) };
};
```

### Callable Concepts

**`MonitoringEventHandler<H, E>`** — A callable that handles events:

```cpp
template <typename H, typename E>
concept MonitoringEventHandler =
    std::invocable<H, const E&> &&
    std::is_void_v<std::invoke_result_t<H, const E&>>;
```

```cpp
struct alert_event {
    std::string name;
    double value;
};

// Lambda satisfies MonitoringEventHandler<lambda, alert_event>
auto handler = [](const alert_event& e) {
    log("Alert: " + e.name);
};

template <MonitoringEventType E, MonitoringEventHandler<E> H>
void subscribe(H&& handler) {
    bus_.subscribe<E>(std::forward<H>(handler));
}

subscribe<alert_event>(handler);  // OK
subscribe<alert_event>([](const alert_event& e) -> int { return 0; });  // ERROR: must return void
```

**`MetricFilterPredicate<F, M>`** — Filters metrics:

```cpp
template <typename F, typename M>
concept MetricFilterPredicate =
    std::invocable<F, const M&> &&
    std::convertible_to<std::invoke_result_t<F, const M&>, bool>;
```

```cpp
template <MetricType M, MetricFilterPredicate<M> F>
auto filter_metrics(const std::vector<M>& metrics, F&& filter) {
    std::vector<M> result;
    std::copy_if(metrics.begin(), metrics.end(),
                 std::back_inserter(result), filter);
    return result;
}

// Usage
auto high_values = filter_metrics(metrics,
    [](const cpu_metric& m) { return m.value > 80.0; });
```

**`MetricTransformer<F, M>`** — Transforms metrics:

```cpp
template <typename F, typename M>
concept MetricTransformer = std::invocable<F, const M&>;
```

### Infrastructure Concepts

**`ConfigValidatable`** — Types that support validation:

```cpp
template <typename T>
concept ConfigValidatable = requires(const T t) {
    { t.validate() };
};

template <ConfigValidatable C>
auto apply_config(const C& config) {
    auto result = config.validate();
    // Proceed only if valid
}
```

**`StorageBackendLike`** — Storage implementations:

```cpp
template <typename T>
concept StorageBackendLike = requires(T t) {
    { t.store(std::declval<std::vector<int>>()) };
    { t.is_connected() } -> std::convertible_to<bool>;
};
```

**`ExporterLike`** — External metric exporters:

```cpp
template <typename T>
concept ExporterLike = requires(T t) {
    { t.export_metrics(std::declval<std::vector<int>>()) };
    { t.is_ready() } -> std::convertible_to<bool>;
};
```

**`HealthCheckable`** — Any component with health checking:

```cpp
template <typename T>
concept HealthCheckable = requires(const T t) {
    { t.is_healthy() } -> std::convertible_to<bool>;
};

// Works with any component that has is_healthy()
template <HealthCheckable H>
bool check_all(const std::vector<H*>& components) {
    return std::all_of(components.begin(), components.end(),
        [](const H* c) { return c->is_healthy(); });
}
```

**`TracingContextLike`** — Distributed tracing context:

```cpp
template <typename T>
concept TracingContextLike = requires(const T t) {
    { t.get_trace_id() } -> std::convertible_to<std::string>;
    { t.get_span_id() } -> std::convertible_to<std::string>;
};
```

### Using Concepts in Your Code

**Pattern 1: Function template constraint**

```cpp
template <MetricValue V>
void record(const std::string& name, V value) {
    // V is guaranteed to be arithmetic
}
```

**Pattern 2: Abbreviated function template (C++20)**

```cpp
void process(MetricCollectorLike auto& collector) {
    collector.collect_metrics();
}
```

**Pattern 3: Requires clause**

```cpp
template <typename T>
    requires MetricSourceLike<T> && HealthCheckable<T>
void monitor(const T& source) {
    if (source.is_healthy()) {
        auto metrics = source.get_current_metrics();
    }
}
```

**Pattern 4: Combining concepts**

```cpp
template <typename T>
concept MonitorableSource =
    MetricSourceLike<T> &&
    HealthCheckable<T> &&
    requires(const T t) {
        { t.get_priority() } -> std::convertible_to<int>;
    };
```

**Pattern 5: Static assertion for documentation**

```cpp
class my_collector {
    // ... implementation ...
};

// Verify at compile time that my_collector satisfies the concept
static_assert(MetricCollectorLike<my_collector>,
    "my_collector must implement collect_metrics(), is_collecting(), and get_metric_types()");
```

### Compiler Error Messages

When a concept is not satisfied, compilers provide clear diagnostics:

```
error: constraints not satisfied for 'void record(const string&, V)'
note: because 'std::string' does not satisfy 'MetricValue'
note: because 'std::is_arithmetic_v<std::string>' evaluated to false
```

Compare with pre-concept SFINAE:

```
error: no matching function for call to 'record(const char*, std::string)'
note: candidate template ignored: requirement 'std::is_arithmetic_v<std::string>' was not satisfied
```

### Compiler Requirements

| Compiler | Minimum Version | Flag |
|----------|----------------|------|
| GCC | 10+ | `-std=c++20` (or `-fconcepts` with GCC 10) |
| Clang | 10+ | `-std=c++20` |
| MSVC | 2022+ (19.30+) | `/std:c++20` |

> **Note**: The concepts are only available when `KCENON_HAS_COMMON_SYSTEM` is defined, as they may extend common_system concepts.

---

## 4. Adaptive Monitor

> **Source**: [`adaptive/adaptive_monitor.h`](../../include/kcenon/monitoring/adaptive/adaptive_monitor.h) → [`src/impl/adaptive_monitor.h`](../../src/impl/adaptive_monitor.h) (627 lines)

The adaptive monitor automatically adjusts collection intervals and sampling rates based on current system load, preventing the monitoring system from becoming a performance bottleneck under stress.

### How Adaptive Monitoring Works

```
┌─────────────────────────────────────────────────────────────┐
│                  Adaptive Monitoring Loop                     │
│                                                             │
│   System Metrics ──► Exponential ──► Load Level ──► Adjust  │
│   (CPU, Memory)      Smoothing       Detection     Behavior │
│                                                             │
│   idle(100ms, 100%)                                         │
│   ────────────────────────►                                 │
│   low(250ms, 80%)          ◄── Upscale (more detail)       │
│   ──────────────────►                                       │
│   moderate(500ms, 50%)     ◄── Default starting point       │
│   ────────────►                                             │
│   high(1000ms, 20%)        ◄── Downscale (less overhead)   │
│   ──────►                                                   │
│   critical(5000ms, 10%)    ◄── Survival mode               │
│   ──►                                                       │
│                                                             │
│   Hysteresis margin ±5%:                                    │
│   ┌────────────────────────────────────────────┐            │
│   │ load=59% → stays at "moderate"              │            │
│   │ load=66% → transitions to "high"            │            │
│   │ (60% threshold + 5% margin = 65% to cross)  │            │
│   └────────────────────────────────────────────┘            │
│                                                             │
│   Cooldown: 1000ms minimum between level changes            │
└─────────────────────────────────────────────────────────────┘
```

### Load Levels and Thresholds

| Level | CPU Threshold | Collection Interval | Sampling Rate | Behavior |
|-------|--------------|--------------------|--------------|---------|
| **idle** | < 20% | 100ms | 100% | Maximum detail |
| **low** | 20-40% | 250ms | 80% | High detail |
| **moderate** | 40-60% | 500ms | 50% | Balanced (default) |
| **high** | 60-80% | 1000ms | 20% | Reduced overhead |
| **critical** | > 80% | 5000ms | 10% | Survival mode |

The effective load also considers memory pressure:
- Memory > `memory_critical_threshold` (85%): Escalates to at least `high` level
- Memory > `memory_warning_threshold` (70%): Escalates to at least `moderate` level

### Adaptive Configuration

```cpp
#include <kcenon/monitoring/adaptive/adaptive_monitor.h>
using namespace kcenon::monitoring;

adaptive_config config;

// CPU thresholds (percentage)
config.idle_threshold = 20.0;
config.low_threshold = 40.0;
config.moderate_threshold = 60.0;
config.high_threshold = 80.0;

// Memory thresholds (percentage)
config.memory_warning_threshold = 70.0;
config.memory_critical_threshold = 85.0;

// Collection intervals per load level
config.idle_interval = std::chrono::milliseconds{100};
config.low_interval = std::chrono::milliseconds{250};
config.moderate_interval = std::chrono::milliseconds{500};
config.high_interval = std::chrono::milliseconds{1000};
config.critical_interval = std::chrono::milliseconds{5000};

// Sampling rates per load level (0.0 to 1.0)
config.idle_sampling_rate = 1.0;       // Collect everything
config.low_sampling_rate = 0.8;        // 80% of samples
config.moderate_sampling_rate = 0.5;   // Half of samples
config.high_sampling_rate = 0.2;       // 20% of samples
config.critical_sampling_rate = 0.1;   // 10% of samples

// Strategy
config.strategy = adaptation_strategy::balanced;

// Adaptation timing
config.adaptation_interval = std::chrono::seconds{10};
config.smoothing_factor = 0.7;  // Exponential smoothing weight (0-1)

// Stability controls
config.enable_hysteresis = true;
config.hysteresis_margin = 5.0;                        // ±5% margin
config.enable_cooldown = true;
config.cooldown_period = std::chrono::milliseconds{1000};  // 1 sec minimum between changes
```

### Adaptation Strategies

Three strategies control how aggressively the system responds to load:

| Strategy | Effective Load Multiplier | Effect |
|----------|--------------------------|--------|
| **conservative** | `effective_load *= 0.8` | Less sensitive to load — stays at higher detail longer |
| **balanced** | `effective_load *= 1.0` | No adjustment — uses raw load levels |
| **aggressive** | `effective_load *= 1.2` | More sensitive — reduces detail earlier |

```cpp
// Example: At 70% CPU...
// conservative: effective_load = 70 * 0.8 = 56% → "moderate" level
// balanced:     effective_load = 70 * 1.0 = 70% → "high" level
// aggressive:   effective_load = 70 * 1.2 = 84% → "critical" level
```

### Hysteresis and Cooldown

**Hysteresis** prevents oscillation at threshold boundaries:

```
Without hysteresis:
  CPU: 59% → moderate, 61% → high, 59% → moderate, 61% → high ...
  (rapid oscillation at the 60% boundary)

With hysteresis (margin = 5%):
  CPU: 59% → moderate (stays)
  CPU: 63% → moderate (63 < 60 + 5 = 65, within margin)
  CPU: 66% → high    (66 >= 65, crosses margin)
  CPU: 63% → high    (63 > 60 - 5 = 55, within margin going down)
  CPU: 54% → moderate (54 < 55, crosses margin going down)
```

**Cooldown** enforces a minimum time between level changes:

```
Change to "high" at T=0
CPU drops to 30% at T=500ms → cooldown prevents change (< 1000ms)
CPU drops to 25% at T=1200ms → change to "low" allowed (> 1000ms)
```

### The Adaptive Collector

The `adaptive_collector` wraps a `metrics_collector` with adaptive sampling:

```cpp
auto collector = std::make_shared<metrics_collector>();

adaptive_config config;
config.strategy = adaptation_strategy::balanced;

adaptive_collector adaptive(collector, config);

// Collect with adaptive sampling
auto result = adaptive.collect();
if (result.is_ok()) {
    auto& snapshot = result.value();
    // Process metrics
} else {
    // Sample was dropped due to adaptive sampling
    // error: "Sample dropped due to adaptive sampling"
}

// Feed system metrics to trigger adaptation
system_metrics sys;
sys.cpu_usage_percent = 75.0;
sys.memory_usage_percent = 60.0;
adaptive.adapt(sys);
// After adaptation: sampling rate drops to 20% (high load level)

// Check current state
auto stats = adaptive.get_stats();
auto interval = adaptive.get_current_interval();
```

**Sampling mechanism:**

```cpp
// From adaptive_monitor.h:336-343
bool should_sample() const {
    if (!enabled_) return true;  // Always collect when disabled

    // Random sampling based on current rate
    static thread_local std::mt19937 gen(std::random_device{}());
    std::uniform_real_distribution<> dis(0.0, 1.0);
    return dis(gen) < current_sampling_rate_.load();
}
```

Each sample has a `current_sampling_rate` probability of being collected. At 20% rate, approximately 1 in 5 samples is collected, and the rest are dropped.

### The Adaptive Monitor Controller

The `adaptive_monitor` class manages multiple adaptive collectors:

```cpp
// Get the global adaptive monitor
auto& monitor = global_adaptive_monitor();

// Register collectors with different configs
auto cpu_collector = std::make_shared<metrics_collector>();
auto mem_collector = std::make_shared<metrics_collector>();

monitor.register_collector("cpu_monitor", cpu_collector,
    adaptive_config{.strategy = adaptation_strategy::conservative});

monitor.register_collector("memory_monitor", mem_collector,
    adaptive_config{.strategy = adaptation_strategy::aggressive});

// Set collector priority (higher = keep active longer under load)
monitor.set_collector_priority("cpu_monitor", 10);
monitor.set_collector_priority("memory_monitor", 5);

// Start adaptive monitoring loop
monitor.start();

// Change global strategy at runtime
monitor.set_global_strategy(adaptation_strategy::conservative);

// Force an immediate adaptation cycle
monitor.force_adaptation();

// Get statistics
auto all_stats = monitor.get_all_stats();
for (const auto& [name, stats] : all_stats) {
    // stats.current_load_level, stats.current_sampling_rate, etc.
}

// Get currently active collectors
auto active = monitor.get_active_collectors();

// Stop monitoring
monitor.stop();
```

### RAII Scope Management

The `adaptive_scope` class provides automatic register/unregister:

```cpp
{
    auto collector = std::make_shared<metrics_collector>();

    // Register automatically on construction
    adaptive_scope scope("my_collector", collector,
        adaptive_config{.strategy = adaptation_strategy::balanced});

    if (scope.is_registered()) {
        // Collector is registered and adaptive monitoring is active
        // ... do work ...
    }

    // Automatically unregistered when scope goes out of scope
}
```

This is useful for temporary monitoring in specific code sections:

```cpp
void process_batch(const std::vector<Item>& items) {
    auto collector = std::make_shared<metrics_collector>();
    adaptive_scope scope("batch_processing", collector);

    for (const auto& item : items) {
        process(item);
    }
    // Collector automatically unregistered here
}
```

### Monitoring Adaptation Statistics

```cpp
auto stats = adaptive.get_stats();

// Adaptation counters
stats.total_adaptations;         // Total adaptation cycles
stats.upscale_count;             // Times detail increased (load decreased)
stats.downscale_count;           // Times detail decreased (load increased)

// Sample counters
stats.samples_collected;         // Samples actually collected
stats.samples_dropped;           // Samples dropped by adaptive sampling
// Drop rate = samples_dropped / (samples_collected + samples_dropped)

// Load metrics
stats.average_cpu_usage;         // Exponentially smoothed CPU average
stats.average_memory_usage;      // Exponentially smoothed memory average
stats.current_load_level;        // Current load level enum
stats.current_interval;          // Current collection interval
stats.current_sampling_rate;     // Current sampling probability

// Stability metrics (from hysteresis/cooldown)
stats.hysteresis_prevented_changes;  // Changes prevented by hysteresis
stats.cooldown_prevented_changes;    // Changes prevented by cooldown
stats.last_adaptation;               // Timestamp of last adaptation
stats.last_level_change;             // Timestamp of last level change
```

**Health indicators:**

| Metric | Healthy | Warning | Action |
|--------|---------|---------|--------|
| Drop rate | < 50% | > 80% | System may be under-monitored |
| Adaptations/hour | 1-10 | > 50 | Increase hysteresis margin or cooldown |
| Hysteresis preventions | > 0 | N/A | Good — stability mechanism working |
| Cooldown preventions | < adaptations × 0.1 | > adaptations × 0.5 | Cooldown too aggressive |

---

## 5. Putting It All Together

### Complete DI + Factory Setup

A complete initialization sequence combining DI container and metric factory:

```cpp
#include <kcenon/monitoring/di/service_registration.h>
#include <kcenon/monitoring/factory/metric_factory.h>
#include <kcenon/monitoring/factory/collector_adapters.h>
#include <kcenon/monitoring/concepts/monitoring_concepts.h>
#include <kcenon/monitoring/adaptive/adaptive_monitor.h>

using namespace kcenon::monitoring;

void initialize_monitoring() {
    // Step 1: Register monitoring with DI container
#if KCENON_HAS_COMMON_SYSTEM
    auto& container = common::di::service_container::global();
    di::monitor_registration_config di_config;
    di_config.monitor_name = "production_monitor";
    di_config.cpu_threshold = 85.0;
    di_config.enable_system_monitoring = true;
    di_config.lifetime = common::di::service_lifetime::singleton;

    auto result = di::register_monitor_services(container, di_config);
    // Now IMonitor is available via container.resolve<IMonitor>()
#endif

    // Step 2: Register collectors with factory
    auto& factory = metric_factory::instance();

    register_plugin_collector<system_resource_collector>("system_resources");
    register_crtp_collector<network_collector>("network");
    register_standalone_collector<vm_collector>("vm");

    // Step 3: Create collectors from configuration
    config_map cpu_config = {{"enabled", "true"}, {"interval_ms", "1000"}};
    config_map net_config = {{"enabled", "true"}, {"interfaces", "eth0,eth1"}};

    auto cpu = factory.create("system_resources", cpu_config);
    auto net = factory.create("network", net_config);

    // Step 4: Wrap with adaptive monitoring
    auto& adaptive = global_adaptive_monitor();

    adaptive_config adaptive_cfg;
    adaptive_cfg.strategy = adaptation_strategy::balanced;

    if (cpu) {
        // adaptive.register_collector("cpu", cpu_as_metrics_collector, adaptive_cfg);
    }

    adaptive.start();
}
```

### Concept-Constrained Custom Types

Building custom types that satisfy monitoring concepts:

```cpp
namespace concepts = kcenon::monitoring::concepts;

// A custom metric type that satisfies MetricType
struct latency_metric {
    std::string name;
    double value;  // in milliseconds
    std::string unit = "ms";
};
static_assert(concepts::MetricType<latency_metric>);

// A custom source that satisfies MetricSourceLike + HealthCheckable
class database_source {
public:
    std::vector<latency_metric> get_current_metrics() const {
        return {
            {"query_latency", last_query_ms_},
            {"connection_pool_usage", pool_usage_pct_}
        };
    }

    std::string get_source_name() const { return "database"; }
    bool is_healthy() const { return connected_ && last_query_ms_ < 5000; }

private:
    double last_query_ms_ = 0.0;
    double pool_usage_pct_ = 0.0;
    bool connected_ = false;
};
static_assert(concepts::MetricSourceLike<database_source>);
static_assert(concepts::HealthCheckable<database_source>);

// Concept-constrained generic function
template <concepts::MetricSourceLike Source>
void poll_and_record(const Source& source) {
    if (!source.is_healthy()) return;

    auto metrics = source.get_current_metrics();
    for (const auto& m : metrics) {
        // Process metrics from any conforming source
    }
}

// Usage
database_source db;
poll_and_record(db);  // OK — database_source satisfies MetricSourceLike
```

### Adaptive Monitoring Pipeline

Combining adaptive monitoring with concept-constrained components:

```cpp
template <concepts::MetricCollectorLike Collector>
class adaptive_pipeline {
public:
    adaptive_pipeline(Collector& collector, const adaptive_config& config)
        : collector_(collector)
        , config_(config)
        , current_rate_(config.moderate_sampling_rate) {}

    void process_cycle(double cpu_usage) {
        // Determine load level
        load_level level;
        if (cpu_usage < config_.idle_threshold) level = load_level::idle;
        else if (cpu_usage < config_.low_threshold) level = load_level::low;
        else if (cpu_usage < config_.moderate_threshold) level = load_level::moderate;
        else if (cpu_usage < config_.high_threshold) level = load_level::high;
        else level = load_level::critical;

        // Adjust sampling rate
        current_rate_ = config_.get_sampling_rate_for_load(level);

        // Collect if sampling allows
        if (should_collect()) {
            collector_.collect_metrics();
        }
    }

private:
    bool should_collect() {
        static thread_local std::mt19937 gen(std::random_device{}());
        std::uniform_real_distribution<> dis(0.0, 1.0);
        return dis(gen) < current_rate_;
    }

    Collector& collector_;
    adaptive_config config_;
    double current_rate_;
};
```

---

## Related Documentation

- [Performance Optimization Cookbook](PERFORMANCE_COOKBOOK.md) — Low-level optimization components
- [Best Practices](BEST_PRACTICES.md) — General monitoring best practices
- [Plugin Development](../plugins/PLUGIN_DEVELOPMENT.md) — Writing custom plugins
- [Collector Development](COLLECTOR_DEVELOPMENT.md) — Building custom collectors
- [Quick Start](QUICK_START.md) — Getting started with monitoring_system

---

*Last Updated: 2025-10-20*
