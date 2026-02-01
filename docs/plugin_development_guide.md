# Plugin Development Guide

## Table of Contents

- [Quick Start](#quick-start)
- [Architecture Overview](#architecture-overview)
- [Creating Your First Plugin](#creating-your-first-plugin)
- [Advanced Features](#advanced-features)
- [Testing Your Plugin](#testing-your-plugin)
- [Troubleshooting](#troubleshooting)

## Quick Start

Create a collector plugin in under 5 minutes:

```cpp
#include "kcenon/monitoring/plugins/collector_plugin.h"

using namespace kcenon::monitoring;

class my_first_plugin : public collector_plugin {
public:
    auto name() const -> std::string_view override {
        return "my_first_plugin";
    }

    auto collect() -> std::vector<metric> override {
        std::vector<metric> metrics;

        metric m;
        m.name = "hello_metric";
        m.value = 42.0;
        m.timestamp = std::chrono::system_clock::now();

        metrics.push_back(m);
        return metrics;
    }

    auto interval() const -> std::chrono::milliseconds override {
        return std::chrono::seconds(5);
    }

    auto is_available() const -> bool override {
        return true;
    }

    auto get_metric_types() const -> std::vector<std::string> override {
        return {"hello_metric"};
    }
};

// Register with registry
auto& registry = collector_registry::instance();
registry.register_plugin(std::make_unique<my_first_plugin>());
```

That's it! Your plugin will collect metrics every 5 seconds.

## Architecture Overview

### Plugin Lifecycle

```
                ┌─────────────────┐
                │  Construction   │
                └────────┬────────┘
                         │
                         v
                ┌─────────────────┐
                │ is_available()  │◄── Platform check
                └────────┬────────┘
                         │ yes
                         v
                ┌─────────────────┐
                │ Registration    │◄── Added to registry
                └────────┬────────┘
                         │
                         v
                ┌─────────────────┐
                │ initialize()    │◄── One-time setup
                └────────┬────────┘
                         │
                         v
                ┌─────────────────┐
          ┌────►│   collect()     │◄── Periodic calls
          │     └────────┬────────┘
          │              │
          └──────────────┘
                         │
                         v
                ┌─────────────────┐
                │   shutdown()    │◄── Cleanup
                └────────┬────────┘
                         │
                         v
                ┌─────────────────┐
                │  Destruction    │
                └─────────────────┘
```

### Key Components

#### collector_plugin Interface

The core interface that all collectors implement. It defines:

- **name()**: Unique identifier for the plugin
- **collect()**: Metric collection logic
- **interval()**: How often to collect metrics
- **is_available()**: Platform/resource availability check

See [Plugin API Reference](plugin_api_reference.md) for complete details.

#### collector_registry

The central registry that manages all plugins. It provides:

- Plugin registration and lifecycle management
- Plugin discovery by name or category
- Factory-based lazy instantiation
- Thread-safe operations

#### Plugin Categories

Plugins are organized into categories:

| Category | Description | Examples |
|----------|-------------|----------|
| `system` | System integration | Thread pools, loggers, containers |
| `hardware` | Hardware sensors | GPU, temperature, battery |
| `platform` | Platform-specific | VM detection, uptime, interrupts |
| `network` | Network metrics | Connectivity, bandwidth |
| `process` | Process-level | CPU, memory, I/O |
| `custom` | User-defined | Your custom collectors |

## Creating Your First Plugin

### Step 1: Implement the Interface

Create a class that inherits from `collector_plugin`:

```cpp
#include "kcenon/monitoring/plugins/collector_plugin.h"
#include "kcenon/monitoring/interfaces/metric_types_adapter.h"

using namespace kcenon::monitoring;

class cpu_usage_plugin : public collector_plugin {
public:
    auto name() const -> std::string_view override {
        return "cpu_usage";
    }

    auto collect() -> std::vector<metric> override {
        std::vector<metric> metrics;

        // Collect CPU usage
        double usage = get_cpu_usage();

        metric cpu_metric;
        cpu_metric.name = "cpu_usage_percent";
        cpu_metric.value = usage;
        cpu_metric.timestamp = std::chrono::system_clock::now();
        cpu_metric.tags["unit"] = "percent";
        cpu_metric.tags["collector"] = std::string(name());

        metrics.push_back(cpu_metric);
        return metrics;
    }

    auto interval() const -> std::chrono::milliseconds override {
        return std::chrono::seconds(1);  // Collect every second
    }

    auto is_available() const -> bool override {
#ifdef __linux__
        return true;  // Available on Linux
#else
        return false; // Not available on other platforms
#endif
    }

    auto get_metric_types() const -> std::vector<std::string> override {
        return {"cpu_usage_percent"};
    }

private:
    double get_cpu_usage() {
        // Your CPU usage collection logic here
        return 45.2;  // Example value
    }
};
```

### Step 2: Define Metadata

Override `get_metadata()` to provide rich plugin information:

```cpp
auto get_metadata() const -> plugin_metadata override {
    return plugin_metadata{
        .name = "cpu_usage",
        .description = "Collects CPU usage percentage",
        .category = plugin_category::hardware,
        .version = "1.0.0",
        .dependencies = {},
        .requires_platform_support = true  // Linux only
    };
}
```

### Step 3: Register with Registry

#### Option A: Direct Registration

```cpp
auto& registry = collector_registry::instance();
auto plugin = std::make_unique<cpu_usage_plugin>();

if (!registry.register_plugin(std::move(plugin))) {
    std::cerr << "Failed to register plugin\n";
}
```

#### Option B: Factory Registration (Lazy Loading)

```cpp
auto& registry = collector_registry::instance();
registry.register_factory<cpu_usage_plugin>("cpu_usage");

// Plugin is instantiated only when first accessed
auto* plugin = registry.get_plugin("cpu_usage");
```

### Step 4: Initialize and Use

```cpp
// Initialize all plugins
registry.initialize_all();

// Get your plugin
auto* plugin = registry.get_plugin("cpu_usage");
if (plugin) {
    // Collect metrics manually
    auto metrics = plugin->collect();
    for (const auto& m : metrics) {
        std::cout << m.name << ": " << m.value << "\n";
    }
}

// Cleanup on shutdown
registry.shutdown_all();
```

## Advanced Features

### Configuration Handling

Plugins can accept configuration through the `initialize()` method:

```cpp
class configurable_plugin : public collector_plugin {
public:
    auto initialize(const config_map& config) -> bool override {
        // Read configuration
        if (config.contains("sample_interval")) {
            sample_interval_ = std::chrono::seconds(
                std::stoi(config.at("sample_interval"))
            );
        }

        if (config.contains("metric_prefix")) {
            metric_prefix_ = config.at("metric_prefix");
        }

        return true;
    }

    auto collect() -> std::vector<metric> override {
        std::vector<metric> metrics;

        metric m;
        m.name = metric_prefix_ + ".value";
        // ... rest of implementation

        return metrics;
    }

private:
    std::chrono::seconds sample_interval_{5};
    std::string metric_prefix_{"custom"};
};

// Initialize with configuration
config_map config;
config["sample_interval"] = "10";
config["metric_prefix"] = "myapp";
registry.initialize_all(config);
```

### Platform-Specific Collectors

Use preprocessor directives and runtime checks:

```cpp
class platform_specific_plugin : public collector_plugin {
public:
    auto is_available() const -> bool override {
#if defined(__linux__)
        // Check for /proc filesystem
        return std::filesystem::exists("/proc/stat");
#elif defined(_WIN32)
        // Check for Windows Performance Counters
        return check_pdh_available();
#elif defined(__APPLE__)
        // Check for macOS sysctl
        return check_sysctl_available();
#else
        return false;
#endif
    }

    auto collect() -> std::vector<metric> override {
#if defined(__linux__)
        return collect_linux();
#elif defined(_WIN32)
        return collect_windows();
#elif defined(__APPLE__)
        return collect_macos();
#endif
    }

private:
    auto collect_linux() -> std::vector<metric> { /* ... */ }
    auto collect_windows() -> std::vector<metric> { /* ... */ }
    auto collect_macos() -> std::vector<metric> { /* ... */ }
};
```

### Error Handling Best Practices

```cpp
auto collect() -> std::vector<metric> override {
    std::vector<metric> metrics;

    try {
        // Collection logic that might fail
        auto data = read_sensor_data();

        metric m;
        m.name = "sensor_value";
        m.value = data;
        metrics.push_back(m);

    } catch (const std::exception& e) {
        // Log error but don't crash
        std::cerr << "Collection failed: " << e.what() << "\n";

        // Optionally emit error metric
        metric error_metric;
        error_metric.name = "collection_errors";
        error_metric.value = 1.0;
        metrics.push_back(error_metric);
    }

    return metrics;  // Return empty vector on failure
}
```

### Performance Optimization

#### 1. Cache Expensive Operations

```cpp
class optimized_plugin : public collector_plugin {
public:
    auto collect() -> std::vector<metric> override {
        std::vector<metric> metrics;

        // Cache results that don't change often
        if (!metadata_cached_) {
            cache_system_metadata();
            metadata_cached_ = true;
        }

        // Use cached data
        metrics.push_back(create_metric_with_cache());

        return metrics;
    }

private:
    bool metadata_cached_{false};
    std::string hostname_;

    void cache_system_metadata() {
        hostname_ = get_hostname();
    }
};
```

#### 2. Avoid Blocking I/O

```cpp
auto collect() -> std::vector<metric> override {
    std::vector<metric> metrics;

    // Use non-blocking I/O or timeout
    auto future = std::async(std::launch::async, [this]() {
        return read_slow_sensor();
    });

    // Wait with timeout
    if (future.wait_for(std::chrono::milliseconds(100))
        == std::future_status::ready) {
        metrics.push_back(create_metric(future.get()));
    }

    return metrics;
}
```

#### 3. Minimize Allocations

```cpp
class efficient_plugin : public collector_plugin {
public:
    auto collect() -> std::vector<metric> override {
        // Pre-allocate if size is known
        metrics_buffer_.clear();
        metrics_buffer_.reserve(expected_metric_count_);

        // Fill buffer
        collect_into_buffer(metrics_buffer_);

        return metrics_buffer_;
    }

private:
    std::vector<metric> metrics_buffer_;
    const size_t expected_metric_count_{10};
};
```

### Thread Safety

If your plugin will be accessed from multiple threads:

```cpp
class thread_safe_plugin : public collector_plugin {
public:
    auto collect() -> std::vector<metric> override {
        std::lock_guard<std::mutex> lock(mutex_);

        // Thread-safe collection
        std::vector<metric> metrics;
        metrics.push_back(create_metric());

        return metrics;
    }

private:
    mutable std::mutex mutex_;
};
```

### Dynamic Plugin Loading

For plugins loaded from shared libraries, see the [Dynamic Plugin Example](../examples/plugin_example/README.md).

Key requirements:
- Implement the C ABI using `IMPLEMENT_PLUGIN` macro
- Ensure API version compatibility
- Build as shared library (.so/.dylib/.dll)

```cpp
#include "kcenon/monitoring/plugins/plugin_api.h"

class dynamic_plugin : public collector_plugin {
    // Implementation...
};

// Export plugin
IMPLEMENT_PLUGIN(
    dynamic_plugin,
    "dynamic_plugin",
    "1.0.0",
    "Dynamically loaded plugin",
    "Your Name",
    "custom"
)
```

## Testing Your Plugin

### Unit Testing

Test individual plugin methods:

```cpp
#include <gtest/gtest.h>
#include "my_plugin.h"

TEST(MyPluginTest, BasicCollection) {
    my_plugin plugin;

    // Test availability
    EXPECT_TRUE(plugin.is_available());

    // Test collection
    auto metrics = plugin.collect();
    EXPECT_FALSE(metrics.empty());

    // Verify metric values
    EXPECT_EQ(metrics[0].name, "expected_name");
    EXPECT_GT(metrics[0].value, 0.0);
}

TEST(MyPluginTest, Initialization) {
    my_plugin plugin;

    config_map config;
    config["key"] = "value";

    EXPECT_TRUE(plugin.initialize(config));
}

TEST(MyPluginTest, Shutdown) {
    my_plugin plugin;

    plugin.initialize({});
    plugin.shutdown();

    // Verify cleanup
    EXPECT_TRUE(plugin.is_clean());
}
```

### Integration Testing

Test plugin within the registry:

```cpp
TEST(PluginIntegrationTest, RegistryLifecycle) {
    auto& registry = collector_registry::instance();

    // Register
    auto plugin = std::make_unique<my_plugin>();
    EXPECT_TRUE(registry.register_plugin(std::move(plugin)));

    // Retrieve
    auto* retrieved = registry.get_plugin("my_plugin");
    ASSERT_NE(retrieved, nullptr);

    // Initialize
    EXPECT_GT(registry.initialize_all(), 0);

    // Collect
    auto metrics = retrieved->collect();
    EXPECT_FALSE(metrics.empty());

    // Shutdown
    registry.shutdown_all();

    // Cleanup
    registry.clear();
}
```

### Mock Data Generation

Create realistic test data:

```cpp
class test_plugin : public collector_plugin {
public:
    // Allow injecting test data
    void set_test_value(double value) {
        test_value_ = value;
    }

    auto collect() -> std::vector<metric> override {
        std::vector<metric> metrics;

        metric m;
        m.name = "test_metric";
        m.value = test_value_;
        metrics.push_back(m);

        return metrics;
    }

private:
    double test_value_{0.0};
};

TEST(TestPluginTest, MockData) {
    test_plugin plugin;

    plugin.set_test_value(123.45);
    auto metrics = plugin.collect();

    EXPECT_DOUBLE_EQ(metrics[0].value, 123.45);
}
```

### Performance Testing

Verify collection performance:

```cpp
TEST(PluginPerformanceTest, CollectionSpeed) {
    my_plugin plugin;

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < 1000; ++i) {
        plugin.collect();
    }

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start
    );

    // Should complete 1000 collections in under 1 second
    EXPECT_LT(duration.count(), 1000);
}
```

## Troubleshooting

### Common Errors

#### 1. Plugin Not Registered

**Symptom**: `get_plugin()` returns `nullptr`

**Causes**:
- Plugin name mismatch between `name()` and `register_plugin()`
- `is_available()` returned `false`
- Registration failed due to duplicate name

**Solution**:
```cpp
// Check if plugin is registered
if (registry.has_plugin("my_plugin")) {
    std::cout << "Plugin registered\n";
} else {
    std::cout << "Plugin not found\n";
}

// Check availability
my_plugin test;
if (!test.is_available()) {
    std::cout << "Plugin not available on this platform\n";
}
```

#### 2. Empty Metrics

**Symptom**: `collect()` returns empty vector

**Causes**:
- Plugin not initialized
- Data source unavailable
- Exception caught and swallowed

**Solution**:
```cpp
auto collect() -> std::vector<metric> override {
    std::vector<metric> metrics;

    if (!initialized_) {
        std::cerr << "Plugin not initialized\n";
        return metrics;
    }

    try {
        // Collection logic
    } catch (const std::exception& e) {
        std::cerr << "Collection error: " << e.what() << "\n";
    }

    return metrics;
}
```

#### 3. Metric Collection Too Slow

**Symptom**: Collection takes longer than interval

**Causes**:
- Blocking I/O operations
- Expensive computations
- Lock contention

**Solution**:
- Use async operations with timeout
- Cache expensive results
- Profile and optimize hot paths
- Consider increasing interval

```cpp
auto interval() const -> std::chrono::milliseconds override {
    // Increase interval if collection is slow
    return std::chrono::seconds(10);  // Instead of 1 second
}
```

#### 4. Memory Leaks

**Symptom**: Increasing memory usage over time

**Causes**:
- Failing to clean up in `shutdown()`
- Accumulating data in member variables
- Circular references

**Solution**:
```cpp
void shutdown() override {
    // Explicitly clear all cached data
    cached_data_.clear();

    // Release resources
    connection_.reset();

    initialized_ = false;
}
```

#### 5. Thread Safety Issues

**Symptom**: Crashes or data corruption under concurrent load

**Causes**:
- Shared mutable state without synchronization
- Non-atomic updates to counters

**Solution**:
```cpp
class thread_safe_plugin : public collector_plugin {
public:
    auto collect() -> std::vector<metric> override {
        std::lock_guard<std::mutex> lock(mutex_);
        // Thread-safe implementation
    }

private:
    mutable std::mutex mutex_;
    std::atomic<uint64_t> counter_{0};
};
```

### Debugging Tips

#### Enable Verbose Logging

```cpp
auto collect() -> std::vector<metric> override {
    std::cerr << "[" << name() << "] Starting collection\n";

    std::vector<metric> metrics;

    // Collection logic
    std::cerr << "[" << name() << "] Collected "
              << metrics.size() << " metrics\n";

    return metrics;
}
```

#### Use Statistics API

```cpp
auto get_statistics() const -> stats_map override {
    stats_map stats;
    stats["collection_count"] = collection_count_;
    stats["error_count"] = error_count_;
    stats["last_collection_ms"] = last_duration_.count();
    return stats;
}
```

#### Validate Metric Data

```cpp
auto collect() -> std::vector<metric> override {
    std::vector<metric> metrics;

    // ... collect metrics ...

    // Validate before returning
    for (const auto& m : metrics) {
        assert(!m.name.empty() && "Metric name cannot be empty");
        assert(std::isfinite(m.value) && "Metric value must be finite");
    }

    return metrics;
}
```

## See Also

- [Plugin API Reference](plugin_api_reference.md) - Complete API documentation
- [Dynamic Plugin Example](../examples/plugin_example/README.md) - Shared library plugin example
- [Migration Guide](migration_guide.md) - Migrating existing collectors
