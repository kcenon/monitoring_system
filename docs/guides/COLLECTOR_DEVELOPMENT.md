# Collector Development Guide

> **Language:** **English** | [한국어](COLLECTOR_DEVELOPMENT.kr.md)

**Version**: 0.4.0.0
**Last Updated**: 2026-02-09

## Purpose

This guide covers everything needed to develop custom metric collectors for the monitoring_system. It explains the CRTP base class pattern, the plugin interface, platform-specific implementations, thread safety requirements, factory registration, and testing strategies.

## Table of Contents

1. [Collector Architecture](#1-collector-architecture)
2. [Implementing a Custom Collector](#2-implementing-a-custom-collector)
3. [Platform-Specific Collectors](#3-platform-specific-collectors)
4. [Thread Safety Requirements](#4-thread-safety-requirements)
5. [Factory Registration](#5-factory-registration)
6. [Testing Custom Collectors](#6-testing-custom-collectors)
7. [Reference: Built-in Collector Catalog](#7-reference-built-in-collector-catalog)

---

## 1. Collector Architecture

### Overview

The monitoring_system provides **two primary interfaces** for building collectors, plus a factory system for lifecycle management:

```
┌─────────────────────────────────────────────────────────────┐
│                     User / Application                       │
│                                                             │
│   auto factory = metric_factory::instance();                │
│   auto result = factory.create("my_collector", config);     │
└──────────────────────────┬──────────────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────────┐
│                    metric_factory                             │
│                    (singleton, thread-safe)                   │
│                                                             │
│   register_collector(name, factory_fn)                      │
│   create(name, config) → create_result                      │
│   create_or_null(name, config) → unique_ptr                 │
└──────────────────────────┬──────────────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────────┐
│                  collector_interface                          │
│                  (abstract base, type-erased)                 │
│                                                             │
│   initialize(config)    → bool                              │
│   get_name()            → string                            │
│   is_healthy()          → bool                              │
│   get_metric_types()    → vector<string>                    │
└──────────────────────────┬──────────────────────────────────┘
                           │
          ┌────────────────┼────────────────┐
          ▼                ▼                ▼
┌──────────────┐  ┌──────────────┐  ┌──────────────────┐
│ crtp_        │  │ plugin_      │  │ standalone_      │
│ collector_   │  │ collector_   │  │ collector_       │
│ adapter<T>   │  │ adapter<T>   │  │ adapter<T>       │
│              │  │              │  │                  │
│ Wraps:       │  │ Wraps:       │  │ Wraps:           │
│ collector_   │  │ metric_      │  │ custom classes   │
│ base<T>      │  │ collector_   │  │ with similar     │
│              │  │ plugin       │  │ interfaces       │
└──────┬───────┘  └──────┬───────┘  └────────┬─────────┘
       │                 │                   │
       ▼                 ▼                   ▼
  ┌──────────┐    ┌──────────────┐    ┌──────────────┐
  │collector_│    │metric_       │    │  Standalone   │
  │base<T>   │    │collector_    │    │  collector    │
  │(CRTP)    │    │plugin        │    │  classes      │
  └──────────┘    └──────────────┘    └──────────────┘
```

### Choosing an Interface

| Criteria | `collector_base<T>` (CRTP) | `collector_plugin` | `metric_collector_plugin` |
|----------|---------------------------|-------------------|--------------------------|
| **Dispatch** | Static (compile-time) | Virtual | Virtual |
| **Performance** | Zero overhead (inlined) | Virtual call (~2-5 ns) | Virtual call (~2-5 ns) |
| **Built-in features** | Statistics, health, error handling | Metadata, lifecycle | Basic lifecycle |
| **Registration** | Via `crtp_collector_adapter<T>` | Via `plugin_collector_adapter<T>` | Direct registration |
| **Best for** | High-frequency, core collectors | Optional/dynamic plugins | Legacy plugin integration |

**Recommendation**: Use `collector_base<T>` for new collectors. It provides the most built-in functionality with zero runtime overhead.

### `collector_base<T>` CRTP Pattern

The CRTP (Curiously Recurring Template Pattern) base class eliminates virtual dispatch overhead while providing common functionality:

```
collector_base<Derived>
├── initialize(config)        ─── parses "enabled", delegates to do_initialize()
├── collect()                 ─── checks enabled_, calls do_collect(), tracks stats
├── get_name()                ─── returns Derived::collector_name
├── get_metric_types()        ─── delegates to do_get_metric_types()
├── is_healthy()              ─── checks enabled_, delegates to is_available()
├── get_statistics()          ─── returns common + do_add_statistics()
├── is_enabled() / get_collection_count() / get_collection_errors()
└── create_base_metric(name, value, tags)  ─── helper to create metric with collector tag
```

**How CRTP dispatch works:**

```cpp
// In collector_base<Derived>:
Derived& derived() { return static_cast<Derived&>(*this); }

// The compiler resolves derived().do_collect() at compile time,
// inlining the call directly — zero virtual dispatch overhead.
std::vector<metric> collect() {
    if (!enabled_) return {};
    try {
        auto metrics = derived().do_collect();  // Static dispatch, inlined
        ++collection_count_;
        return metrics;
    } catch (...) {
        ++collection_errors_;
        return {};
    }
}
```

### `collector_plugin` Interface

The newer plugin interface supports runtime discovery and lifecycle management:

```
collector_plugin
├── name()              → string_view        [required]
├── collect()           → vector<metric>     [required]
├── interval()          → milliseconds       [required]
├── is_available()      → bool               [required]
├── get_metric_types()  → vector<string>     [required]
├── get_metadata()      → plugin_metadata    [optional, default provided]
├── initialize(config)  → bool               [optional, default: true]
├── shutdown()          → void               [optional, default: no-op]
└── get_statistics()    → stats_map          [optional, default: empty]
```

### Collection Lifecycle

```
Construction
     │
     ▼
is_available() ──false──► Skip (not registered)
     │ true
     ▼
initialize(config)
     │
     ▼
 ┌──────────────────┐
 │  Active Loop     │
 │                  │
 │  collect()       │◄──── Called every interval()
 │       │          │
 │       ▼          │
 │  Return metrics  │
 │  or empty vector │
 └──────────────────┘
     │
     ▼
shutdown() / ~destructor
```

---

## 2. Implementing a Custom Collector

### Step 1: Define the Collector Class (CRTP Approach)

Create a new header in `include/kcenon/monitoring/collectors/`:

```cpp
// my_custom_collector.h
#pragma once

#include "collector_base.h"

namespace kcenon {
namespace monitoring {

class my_custom_collector : public collector_base<my_custom_collector> {
public:
    // Required: static name used by collector_base::get_name()
    static constexpr const char* collector_name = "my_custom_collector";

    // Required: collector-specific initialization
    bool do_initialize(const config_map& config) {
        if (auto it = config.find("target_host"); it != config.end()) {
            target_host_ = it->second;
        }
        return !target_host_.empty();
    }

    // Required: collect metrics from your data source
    std::vector<metric> do_collect() {
        std::vector<metric> metrics;

        double latency = measure_latency();
        metrics.push_back(
            create_base_metric("my_custom.latency_ms", latency,
                               {{"host", target_host_}})
        );

        double throughput = measure_throughput();
        metrics.push_back(
            create_base_metric("my_custom.throughput_ops", throughput,
                               {{"host", target_host_}})
        );

        return metrics;
    }

    // Required: platform/resource availability check
    bool is_available() const {
        return !target_host_.empty();
    }

    // Required: declare metric types this collector produces
    std::vector<std::string> do_get_metric_types() const {
        return {"my_custom.latency_ms", "my_custom.throughput_ops"};
    }

    // Optional: add collector-specific statistics
    void do_add_statistics(stats_map& stats) const {
        stats["target_host_configured"] = target_host_.empty() ? 0.0 : 1.0;
    }

private:
    std::string target_host_;

    double measure_latency() { /* implementation */ return 0.0; }
    double measure_throughput() { /* implementation */ return 0.0; }
};

}  // namespace monitoring
}  // namespace kcenon
```

### Step 2: Required Methods Summary

| Method | Signature | Purpose |
|--------|-----------|---------|
| `collector_name` | `static constexpr const char*` | Unique identifier for the collector |
| `do_initialize` | `bool(const config_map&)` | Parse collector-specific configuration |
| `do_collect` | `std::vector<metric>()` | Collect and return metrics |
| `is_available` | `bool() const` | Check if data source is accessible |
| `do_get_metric_types` | `std::vector<std::string>() const` | List metric types produced |
| `do_add_statistics` | `void(stats_map&) const` | Add custom statistics (optional) |

### Step 3: Use `create_base_metric` for Consistent Tagging

The base class provides `create_base_metric()` which automatically adds:
- `timestamp`: `std::chrono::system_clock::now()`
- `tags["collector"]`: Set to `Derived::collector_name`

```cpp
// Creates a metric with name, value, optional tags, and auto-applied collector tag
metric m = create_base_metric(
    "my_custom.latency_ms",   // metric name
    42.5,                      // metric value
    {{"host", "server-1"}},    // additional tags
    "ms"                       // unit (documentation only)
);
// Result: m.tags = {"collector": "my_custom_collector", "host": "server-1"}
```

### Step 4: Configuration Convention

Collectors receive configuration as `config_map` (`unordered_map<string, string>`). The base class handles the `"enabled"` key automatically.

```cpp
// Standard configuration keys
config_map config = {
    {"enabled", "true"},           // Handled by collector_base
    {"target_host", "server-1"},   // Collector-specific
    {"interval_ms", "5000"},       // Collector-specific
    {"timeout_ms", "1000"},        // Collector-specific
};
```

### Alternative: Plugin Interface Approach

For collectors that need runtime discovery and lifecycle management:

```cpp
class my_plugin_collector : public collector_plugin {
public:
    auto name() const -> std::string_view override {
        return "my_plugin_collector";
    }

    auto collect() -> std::vector<metric> override {
        std::vector<metric> metrics;
        // Collect metrics...
        return metrics;
    }

    auto interval() const -> std::chrono::milliseconds override {
        return std::chrono::seconds(5);
    }

    auto is_available() const -> bool override {
        return true;  // Check platform/resource availability
    }

    auto get_metric_types() const -> std::vector<std::string> override {
        return {"my_plugin.metric_1", "my_plugin.metric_2"};
    }

    auto get_metadata() const -> plugin_metadata override {
        return plugin_metadata{
            .name = "my_plugin_collector",
            .description = "Custom plugin collector example",
            .category = plugin_category::custom,
            .version = "1.0.0",
            .dependencies = {},
            .requires_platform_support = false
        };
    }
};
```

### Hybrid Approach: CRTP + Plugin

Some collectors use both interfaces. The CRTP base provides common functionality, while the plugin interface enables runtime registration:

```cpp
class my_hybrid_collector : public collector_base<my_hybrid_collector>,
                            public collector_plugin {
public:
    static constexpr const char* collector_name = "my_hybrid";

    // Plugin interface delegates to CRTP base
    auto name() const -> std::string_view override { return collector_name; }
    auto collect() -> std::vector<metric> override {
        return collector_base<my_hybrid_collector>::collect();
    }
    auto interval() const -> std::chrono::milliseconds override {
        return std::chrono::seconds(5);
    }
    auto get_metric_types() const -> std::vector<std::string> override {
        return do_get_metric_types();
    }

    // CRTP required methods
    bool do_initialize(const config_map& config) { return true; }
    std::vector<metric> do_collect() { return {}; }
    bool is_available() const override { return true; }
    std::vector<std::string> do_get_metric_types() const { return {"my_hybrid.metric"}; }
    void do_add_statistics(stats_map& stats) const {}
};
```

---

## 3. Platform-Specific Collectors

### Two Approaches

The monitoring_system uses two patterns for platform-specific code:

| Approach | Used by | Trade-off |
|----------|---------|-----------|
| **`#ifdef` guards** | `system_resource_collector` | Simpler, direct platform API access |
| **Strategy pattern** | `platform_metrics_collector` | Cleaner separation, testable with mocks |

### Approach 1: Direct `#ifdef` Guards

Used by `system_resource_collector` for direct system API calls:

```cpp
class my_platform_collector : public collector_base<my_platform_collector> {
public:
    static constexpr const char* collector_name = "my_platform_collector";

    std::vector<metric> do_collect() {
        std::vector<metric> metrics;
        collect_platform_metrics(metrics);
        return metrics;
    }

    bool is_available() const {
#if defined(__linux__) || defined(__APPLE__)
        return true;
#else
        return false;  // Not supported on this platform
#endif
    }

private:
    void collect_platform_metrics(std::vector<metric>& metrics) {
#ifdef __linux__
        collect_linux_metrics(metrics);
#elif defined(__APPLE__)
        collect_macos_metrics(metrics);
#elif defined(_WIN32)
        collect_windows_metrics(metrics);
#endif
    }

#ifdef __linux__
    void collect_linux_metrics(std::vector<metric>& metrics) {
        // Read from /proc, /sys filesystem
        // Example: /proc/stat, /proc/meminfo, /sys/class/thermal
    }
#endif

#ifdef __APPLE__
    void collect_macos_metrics(std::vector<metric>& metrics) {
        // Use Mach kernel APIs, sysctl
        // Example: mach_host_info, vm_statistics
    }
#endif

#ifdef _WIN32
    void collect_windows_metrics(std::vector<metric>& metrics) {
        // Use Windows API, WMI, PDH
        // Example: GetSystemInfo, GlobalMemoryStatusEx
    }
#endif

    // CRTP required methods
    std::vector<std::string> do_get_metric_types() const { return {}; }
    bool do_initialize(const config_map&) { return true; }
    void do_add_statistics(stats_map&) const {}
};
```

### Approach 2: Strategy Pattern via `metrics_provider`

Used by `platform_metrics_collector` for cleaner abstraction:

```cpp
// Forward declaration of the platform abstraction
namespace platform {
class metrics_provider;  // Created via factory: metrics_provider::create()
}

class my_strategy_collector : public collector_plugin {
public:
    my_strategy_collector()
        : provider_(platform::metrics_provider::create()) {}

    auto collect() -> std::vector<metric> override {
        std::vector<metric> metrics;
        if (provider_) {
            auto data = provider_->get_metrics();
            // Convert platform data to metrics...
        }
        return metrics;
    }

    auto is_available() const -> bool override {
        return provider_ != nullptr;
    }

private:
    std::unique_ptr<platform::metrics_provider> provider_;
};
```

**When to use which:**

| Condition | Use `#ifdef` | Use Strategy |
|-----------|--------------|-------------|
| Direct system calls (syscall, /proc) | Yes | |
| Need mock in unit tests | | Yes |
| Simple platform check | Yes | |
| Complex platform API with state | | Yes |
| Header-only collector | Yes | |

### Platform Detection Macros

| Macro | Platform | Common APIs |
|-------|----------|------------|
| `__linux__` | Linux | `/proc/*`, `/sys/*`, `sysinfo()` |
| `__APPLE__` | macOS | `mach/mach.h`, `sysctl()`, IOKit |
| `_WIN32` | Windows | `windows.h`, WMI, PDH, `psapi.h` |

### Graceful Fallback on Unsupported Platforms

**Always return `false` from `is_available()` on unsupported platforms** instead of throwing or crashing:

```cpp
bool is_available() const {
#ifdef __linux__
    return std::filesystem::exists("/proc/stat");
#elif defined(__APPLE__)
    return true;  // sysctl always available on macOS
#else
    return false;  // Unsupported platform — collector is skipped
#endif
}
```

The factory and plugin_metric_collector automatically skip collectors where `is_available()` returns false.

### Platform Include Guards

Follow the existing pattern from `system_resource_collector.h`:

```cpp
#ifdef __APPLE__
    #include <mach/mach.h>
    #include <sys/sysctl.h>
#elif __linux__
    #include <sys/sysinfo.h>
    #include <unistd.h>
#elif _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
    #define NOMINMAX
    #endif
    #include <winsock2.h>    // Must be before windows.h
    #include <windows.h>
    #include <psapi.h>
#endif
```

**Important**: On Windows, `winsock2.h` **must** be included before `windows.h` to avoid redefinition errors.

---

## 4. Thread Safety Requirements

### What Collectors Must Guarantee

| Method | Thread Safety Requirement |
|--------|--------------------------|
| `collect()` / `do_collect()` | Must be safe for concurrent calls |
| `initialize()` / `do_initialize()` | Called once, before any `collect()` |
| `is_available()` / `is_healthy()` | Must be lock-free if possible |
| `get_statistics()` | Protected by `collector_base`'s `stats_mutex_` |
| `get_name()` / `get_metric_types()` | Read-only, inherently safe |

### Built-in Thread Safety from `collector_base<T>`

The CRTP base class provides:

```cpp
// Atomic counters — lock-free, safe from any thread
std::atomic<size_t> collection_count_{0};
std::atomic<size_t> collection_errors_{0};

// Statistics mutex — protects get_statistics()
mutable std::mutex stats_mutex_;

// Enabled flag — set during initialize(), read during collect()
bool enabled_{true};
```

### Protecting Collector-Specific State

If your collector has mutable state accessed during `do_collect()`, protect it:

```cpp
class my_collector : public collector_base<my_collector> {
public:
    std::vector<metric> do_collect() {
        std::lock_guard<std::mutex> lock(data_mutex_);
        // Safe to access mutable state here
        return metrics;
    }

    void update_target(const std::string& host) {
        std::lock_guard<std::mutex> lock(data_mutex_);
        target_host_ = host;
    }

private:
    mutable std::mutex data_mutex_;
    std::string target_host_;
};
```

### Atomic Operations for Metric Updates

For counters and gauges that change frequently, prefer atomics over mutexes:

```cpp
class high_freq_collector : public collector_base<high_freq_collector> {
public:
    std::vector<metric> do_collect() {
        std::vector<metric> metrics;
        metrics.push_back(create_base_metric(
            "requests_total",
            static_cast<double>(request_count_.load(std::memory_order_relaxed))
        ));
        return metrics;
    }

    // Called from application hot path — must be fast
    void record_request() {
        request_count_.fetch_add(1, std::memory_order_relaxed);
    }

private:
    std::atomic<uint64_t> request_count_{0};
};
```

### Thread-Local Buffer Integration

For very high-frequency recording, collectors can integrate with `thread_local_buffer`:

```
Application thread            Central collector
     │                              │
     ▼                              │
thread_local_buffer::record()       │
(~5-10 ns, no lock)                │
     │                              │
     ▼ (when buffer full, 256)      │
thread_local_buffer::flush() ───────▼
                            central_collector::receive_batch()
                            (shared_mutex write lock)
```

### Safe Shutdown

Collectors should handle concurrent shutdown gracefully:

```cpp
class my_collector : public collector_base<my_collector> {
public:
    bool do_initialize(const config_map& config) {
        running_.store(true, std::memory_order_release);
        return true;
    }

    std::vector<metric> do_collect() {
        if (!running_.load(std::memory_order_acquire)) {
            return {};
        }
        // ... collect metrics
        return metrics;
    }

    void shutdown() {
        running_.store(false, std::memory_order_release);
        // Wait for any in-flight collect() calls to complete
    }

private:
    std::atomic<bool> running_{false};
};
```

---

## 5. Factory Registration

### Three Registration Methods

#### Method 1: Helper Functions (Recommended)

Use the type-specific helper functions from `collector_adapters.h`:

```cpp
#include <kcenon/monitoring/factory/collector_adapters.h>

// For CRTP-based collectors
bool ok = register_crtp_collector<my_custom_collector>("my_custom_collector");

// For plugin-based collectors
bool ok = register_plugin_collector<my_plugin>("my_plugin");

// For standalone collectors
bool ok = register_standalone_collector<my_standalone>("my_standalone");
```

These helper functions create the appropriate adapter wrapper and register with `metric_factory`.

#### Method 2: Direct Factory Registration

Register a factory lambda directly:

```cpp
#include <kcenon/monitoring/factory/metric_factory.h>

auto& factory = metric_factory::instance();

// Register with factory function
factory.register_collector("my_collector", []() {
    return std::make_unique<crtp_collector_adapter<my_custom_collector>>();
});

// Or using the template overload
factory.register_collector<crtp_collector_adapter<my_custom_collector>>("my_collector");
```

#### Method 3: `REGISTER_COLLECTOR` Macro (Static Initialization)

For automatic registration at program startup:

```cpp
#include <kcenon/monitoring/factory/metric_factory.h>

// This registers the collector before main() runs
REGISTER_COLLECTOR(my_collector_adapter);
```

**Warning**: Static initialization order is undefined across translation units. Use this only for self-contained collectors.

### Creating Collector Instances

```cpp
auto& factory = metric_factory::instance();

// Method 1: Full result with error info
config_map config = {{"enabled", "true"}, {"target_host", "server-1"}};
auto result = factory.create("my_custom_collector", config);
if (result) {
    auto& collector = result.collector;
    // Use collector...
} else {
    // result.error_message contains the reason
}

// Method 2: Simple null-or-value
auto collector = factory.create_or_null("my_custom_collector", config);
if (collector) {
    // Use collector...
}

// Method 3: Batch creation
std::unordered_map<std::string, config_map> configs = {
    {"system_resource_collector", {{"enabled", "true"}}},
    {"my_custom_collector", {{"target_host", "server-1"}}},
};
auto collectors = factory.create_multiple(configs);
```

### Plugin System Registration

For plugin-based collectors, register with `plugin_metric_collector`:

```cpp
#include <kcenon/monitoring/collectors/plugin_metric_collector.h>

plugin_collector_config pconfig;
pconfig.collection_interval = std::chrono::seconds(5);
pconfig.worker_threads = 2;

plugin_metric_collector collector(pconfig);

// Register plugins
auto my_plugin = std::make_unique<my_plugin_collector>();
collector.register_plugin(std::move(my_plugin));

// Start collection
collector.start();
```

### Configuration-Driven Collector Selection

Use `plugin_factory` for type-based creation:

```cpp
#include <kcenon/monitoring/collectors/plugin_metric_collector.h>

// Register custom factory
plugin_factory::register_factory("my_type", [](const auto& config) {
    auto plugin = std::make_unique<my_plugin_collector>();
    plugin->initialize(config);
    return plugin;
});

// Create by type name (useful for config-file-driven setup)
auto plugin = plugin_factory::create("my_type", {{"key", "value"}});
```

---

## 6. Testing Custom Collectors

### Unit Testing Pattern

Test the collector lifecycle and metric output:

```cpp
#include <gtest/gtest.h>
#include "my_custom_collector.h"

class MyCustomCollectorTest : public ::testing::Test {
protected:
    my_custom_collector collector_;
    config_map config_ = {
        {"enabled", "true"},
        {"target_host", "test-server"},
    };
};

TEST_F(MyCustomCollectorTest, InitializationSucceeds) {
    EXPECT_TRUE(collector_.initialize(config_));
    EXPECT_TRUE(collector_.is_enabled());
}

TEST_F(MyCustomCollectorTest, InitializationFailsWithoutHost) {
    config_map empty_config = {{"enabled", "true"}};
    EXPECT_FALSE(collector_.initialize(empty_config));
}

TEST_F(MyCustomCollectorTest, CollectsMetrics) {
    collector_.initialize(config_);
    auto metrics = collector_.collect();
    EXPECT_FALSE(metrics.empty());

    // Verify metric naming convention
    for (const auto& m : metrics) {
        EXPECT_TRUE(m.name.starts_with("my_custom."));
        EXPECT_EQ(m.tags.at("collector"), "my_custom_collector");
    }
}

TEST_F(MyCustomCollectorTest, ReturnsEmptyWhenDisabled) {
    config_map disabled = {{"enabled", "false"}, {"target_host", "test"}};
    collector_.initialize(disabled);
    auto metrics = collector_.collect();
    EXPECT_TRUE(metrics.empty());
}

TEST_F(MyCustomCollectorTest, TracksStatistics) {
    collector_.initialize(config_);
    collector_.collect();
    collector_.collect();

    EXPECT_EQ(collector_.get_collection_count(), 2u);
    EXPECT_EQ(collector_.get_collection_errors(), 0u);
}

TEST_F(MyCustomCollectorTest, IsHealthyWhenAvailable) {
    collector_.initialize(config_);
    EXPECT_TRUE(collector_.is_healthy());
}

TEST_F(MyCustomCollectorTest, ReportsMetricTypes) {
    auto types = collector_.get_metric_types();
    EXPECT_FALSE(types.empty());
    EXPECT_NE(std::find(types.begin(), types.end(), "my_custom.latency_ms"),
              types.end());
}
```

### Testing Factory Registration

```cpp
TEST(FactoryTest, RegisterAndCreate) {
    auto& factory = metric_factory::instance();

    bool registered = register_crtp_collector<my_custom_collector>("test_collector");
    EXPECT_TRUE(registered);

    config_map config = {{"enabled", "true"}, {"target_host", "test"}};
    auto result = factory.create("test_collector", config);
    EXPECT_TRUE(result.success);
    EXPECT_NE(result.collector, nullptr);
    EXPECT_EQ(result.collector->get_name(), "my_custom_collector");

    // Cleanup
    factory.unregister_collector("test_collector");
}

TEST(FactoryTest, DuplicateRegistrationFails) {
    auto& factory = metric_factory::instance();
    factory.register_collector("dup_test", []() {
        return std::make_unique<crtp_collector_adapter<my_custom_collector>>();
    });

    bool duplicate = factory.register_collector("dup_test", []() {
        return std::make_unique<crtp_collector_adapter<my_custom_collector>>();
    });
    EXPECT_FALSE(duplicate);

    factory.unregister_collector("dup_test");
}
```

### Testing Platform Availability

```cpp
TEST(PlatformTest, IsAvailableReturnsCorrectly) {
    my_platform_collector collector;

#ifdef __linux__
    EXPECT_TRUE(collector.is_available());
#elif defined(__APPLE__)
    EXPECT_TRUE(collector.is_available());
#else
    EXPECT_FALSE(collector.is_available());
#endif
}

TEST(PlatformTest, CollectReturnsEmptyOnUnsupportedPlatform) {
    my_platform_collector collector;
    if (!collector.is_available()) {
        collector.initialize({{"enabled", "true"}});
        auto metrics = collector.collect();
        EXPECT_TRUE(metrics.empty());
    }
}
```

### Performance Benchmarking

Verify that collection stays within performance budgets:

```cpp
#include <chrono>

TEST(PerformanceTest, CollectCompletesWithin100ms) {
    my_custom_collector collector;
    collector.initialize({{"enabled", "true"}, {"target_host", "localhost"}});

    auto start = std::chrono::steady_clock::now();
    auto metrics = collector.collect();
    auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), 100)
        << "collect() should complete within 100ms";
}

TEST(PerformanceTest, StatisticsAreLockFree) {
    my_custom_collector collector;
    collector.initialize({{"enabled", "true"}, {"target_host", "localhost"}});

    // Run 1000 collections and verify no contention issues
    for (int i = 0; i < 1000; ++i) {
        collector.collect();
    }
    EXPECT_EQ(collector.get_collection_count(), 1000u);
}
```

### Thread Safety Testing

```cpp
#include <thread>
#include <vector>

TEST(ThreadSafetyTest, ConcurrentCollect) {
    my_custom_collector collector;
    collector.initialize({{"enabled", "true"}, {"target_host", "localhost"}});

    constexpr int num_threads = 8;
    constexpr int iterations = 100;
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < iterations; ++j) {
                auto metrics = collector.collect();
                // No crashes, no data races
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(collector.get_collection_count(), num_threads * iterations);
}
```

---

## 7. Reference: Built-in Collector Catalog

### Core Collectors (6)

Always included in the build. These collectors cover fundamental system metrics.

| Collector | File | Metrics | Platform |
|-----------|------|---------|----------|
| `system_resource_collector` | `collectors/system_resource_collector.h` | CPU usage, memory, disk, network, process stats | Linux, macOS, Windows |
| `network_metrics_collector` | `collectors/network_metrics_collector.h` | Socket buffer sizes, TCP state counts | Linux, macOS |
| `process_metrics_collector` | `collectors/process_metrics_collector.h` | FD count, inodes, context switches | Linux, macOS |
| `platform_metrics_collector` | `collectors/platform_metrics_collector.h` | Platform info, uptime, TCP states (Strategy pattern) | All |
| `thread_system_collector` | `collectors/thread_system_collector.h` | Thread pool utilization, queue depth | All |
| `logger_system_collector` | `collectors/logger_system_collector.h` | Logger throughput, buffer usage | All |

### Utility Collectors (5)

Included in the core build, provide supplementary metrics:

| Collector | File | Metrics | Platform |
|-----------|------|---------|----------|
| `uptime_collector` | `collectors/uptime_collector.h` | System uptime | All |
| `vm_collector` | `collectors/vm_collector.h` | Virtual memory statistics | Linux |
| `interrupt_collector` | `collectors/interrupt_collector.h` | Hardware interrupt counts | Linux |
| `security_collector` | `collectors/security_collector.h` | Security events | All |
| `plugin_metric_collector` | `collectors/plugin_metric_collector.h` | Plugin management and aggregation | All |

### Hardware Plugin Collectors (4)

Optional. Enabled via CMake flag: `MONITORING_BUILD_HARDWARE_PLUGIN=ON`

| Collector | File | Metrics | Platform |
|-----------|------|---------|----------|
| `battery_collector` | `collectors/battery_collector.h` | Battery level, charging, health, cycles | Linux (sysfs), macOS (IOKit) |
| `power_collector` | `collectors/power_collector.h` | Power consumption (watts), RAPL domains | Linux (RAPL), macOS |
| `temperature_collector` | `collectors/temperature_collector.h` | CPU/GPU/motherboard temperatures | Linux (thermal_zone), macOS |
| `gpu_collector` | `collectors/gpu_collector.h` | GPU utilization, VRAM, temperature, clocks | Linux (NVML/ROCm) |

### Container Plugin Collectors (2)

Optional. Enabled via CMake flag: `MONITORING_BUILD_CONTAINER_PLUGIN=ON`

| Collector | File | Metrics | Platform |
|-----------|------|---------|----------|
| `container_collector` | `collectors/container_collector.h` | Docker/K8s container metrics, cgroups v1/v2 | Linux |
| `smart_collector` | `collectors/smart_collector.h` | SMART disk health attributes | Linux (`/dev/sd*`) |

### Metric Naming Convention

All built-in collectors follow the `<domain>.<metric_name>` convention:

```
system.cpu.usage_percent
system.memory.used_bytes
system.disk.io.read_bytes_per_sec
system.network.rx_bytes_per_sec
platform.uptime_seconds
platform.context_switches.per_second
hardware.battery.level_percent
hardware.gpu.utilization_percent
container.cpu.usage_percent
```

Custom collectors should follow this pattern: `<your_domain>.<metric_name>`.

---

## See Also

- [ARCHITECTURE.md](../ARCHITECTURE.md) — System architecture overview
- [plugin_architecture.md](../plugin_architecture.md) — Plugin system design
- [QUICK_START.md](QUICK_START.md) — Quick start tutorial
- [BEST_PRACTICES.md](BEST_PRACTICES.md) — Best practices guide
- `include/kcenon/monitoring/collectors/collector_base.h` — CRTP base class source
- `include/kcenon/monitoring/plugins/collector_plugin.h` — Plugin interface source
- `include/kcenon/monitoring/factory/metric_factory.h` — Factory system source

---

*Last Updated: 2026-02-09*
