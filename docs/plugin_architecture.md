# Collector Plugin Architecture

## Overview

The collector plugin architecture provides a unified interface for all metric collectors in the monitoring system. It enables:

- **Dynamic registration**: Plugins can be registered at runtime
- **Lazy instantiation**: Plugins are created only when needed
- **Platform abstraction**: Unavailable plugins are automatically skipped
- **Extensibility**: Easy to add new collectors without modifying core code

## Architecture

### Core Components

```
collector_plugin (interface)
    ├── name() - Unique identifier
    ├── collect() - Metric collection
    ├── interval() - Collection frequency
    ├── is_available() - Platform compatibility check
    └── get_metadata() - Plugin information
```

### Plugin Categories

| Category | Description | Examples |
|----------|-------------|----------|
| `system` | System integration | thread_collector, logger_collector, container_collector |
| `hardware` | Hardware sensors | gpu_collector, temperature_collector, battery_collector |
| `platform` | Platform-specific | vm_collector, uptime_collector, interrupt_collector |
| `network` | Network metrics | network_metrics_collector |
| `process` | Process-level | process_metrics_collector, resource_collector |
| `custom` | User-defined | Custom plugins |

## Plugin Interface

### Required Methods

```cpp
class my_plugin : public collector_plugin {
public:
    // Unique plugin identifier
    auto name() const -> std::string_view override {
        return "my_plugin";
    }

    // Collect current metrics
    auto collect() -> std::vector<metric> override {
        std::vector<metric> metrics;
        // Collect metrics here
        return metrics;
    }

    // Collection interval
    auto interval() const -> std::chrono::milliseconds override {
        return std::chrono::seconds(5);
    }

    // Platform availability check
    auto is_available() const -> bool override {
        return check_platform_support();
    }

    // Metric types produced
    auto get_metric_types() const -> std::vector<std::string> override {
        return {"metric_type_1", "metric_type_2"};
    }
};
```

### Optional Methods

```cpp
class my_plugin : public collector_plugin {
public:
    // Plugin metadata
    auto get_metadata() const -> plugin_metadata override {
        return plugin_metadata{
            .name = "my_plugin",
            .description = "Example plugin",
            .category = plugin_category::custom,
            .version = "1.0.0",
            .dependencies = {},
            .requires_platform_support = false
        };
    }

    // Initialize with configuration
    auto initialize(const config_map& config) -> bool override {
        // Parse configuration
        return true;
    }

    // Cleanup on shutdown
    void shutdown() override {
        // Release resources
    }

    // Plugin statistics
    auto get_statistics() const -> stats_map override {
        return {{"collections", 42.0}};
    }
};
```

## Creating a Plugin

### Example: CPU Temperature Plugin

```cpp
#include <kcenon/monitoring/plugins/collector_plugin.h>

class cpu_temperature_plugin : public collector_plugin {
public:
    cpu_temperature_plugin() = default;

    auto name() const -> std::string_view override {
        return "cpu_temperature";
    }

    auto collect() -> std::vector<metric> override {
        std::vector<metric> metrics;

        double temp = read_cpu_temperature();
        metrics.push_back({
            "cpu_temp_celsius",
            temp,
            {{"sensor", "package"}},
            metric_type::gauge
        });

        return metrics;
    }

    auto interval() const -> std::chrono::milliseconds override {
        return std::chrono::seconds(1);
    }

    auto is_available() const -> bool override {
#ifdef __linux__
        return std::filesystem::exists("/sys/class/thermal");
#else
        return false;
#endif
    }

    auto get_metric_types() const -> std::vector<std::string> override {
        return {"cpu_temp_celsius"};
    }

    auto get_metadata() const -> plugin_metadata override {
        return plugin_metadata{
            .name = name(),
            .description = "CPU temperature monitoring",
            .category = plugin_category::hardware,
            .version = "1.0.0",
            .dependencies = {},
            .requires_platform_support = true
        };
    }

private:
    double read_cpu_temperature() {
        // Platform-specific implementation
        return 45.0;
    }
};
```

## Using Collector Base

For convenience, plugins can inherit from `collector_base<T>` CRTP class:

```cpp
class my_plugin : public collector_base<my_plugin>,
                  public collector_plugin {
public:
    static constexpr const char* collector_name = "my_plugin";

    auto name() const -> std::string_view override {
        return collector_name;
    }

    auto collect() -> std::vector<metric> override {
        // Use collector_base::collect() which calls do_collect()
        return collector_base<my_plugin>::collect();
    }

    // CRTP methods
    bool do_initialize(const config_map& config) {
        // Initialization logic
        return true;
    }

    std::vector<metric> do_collect() {
        // Collection logic
        return {};
    }

    bool is_available() const override {
        return true;
    }

    std::vector<std::string> do_get_metric_types() const {
        return {"my_metric"};
    }

    void do_add_statistics(stats_map& stats) const {
        // Add custom statistics
    }
};
```

## Best Practices

### Thread Safety

- `collect()` may be called concurrently from multiple threads
- Use locks or lock-free data structures
- Avoid global mutable state

### Performance

- Keep `collect()` execution time < 100ms
- Return empty vector on errors instead of throwing
- Use `is_available()` to skip expensive checks in `collect()`

### Error Handling

```cpp
auto collect() -> std::vector<metric> override {
    std::vector<metric> metrics;

    try {
        // Collection logic
        auto value = risky_operation();
        metrics.push_back({"my_metric", value, {}});
    } catch (const std::exception& e) {
        // Log error, return empty vector
        return {};
    }

    return metrics;
}
```

### Platform Compatibility

```cpp
auto is_available() const -> bool override {
#if defined(__linux__)
    return std::filesystem::exists("/proc/stat");
#elif defined(_WIN32)
    return check_wmi_available();
#elif defined(__APPLE__)
    return check_sysctl_available();
#else
    return false;
#endif
}
```

## Migration from Existing Collectors

To convert an existing collector to a plugin:

1. **Implement collector_plugin interface**
2. **Keep existing CRTP base** (optional, for compatibility)
3. **Add plugin methods**:
   - `name()` → return `collector_name`
   - `collect()` → delegate to base `collect()`
   - `interval()` → return collection frequency
   - `is_available()` → delegate to base method
   - `get_metric_types()` → delegate to base method

Example:

```cpp
// Before: Only CRTP base
class uptime_collector : public collector_base<uptime_collector> {
    // ...
};

// After: CRTP base + plugin interface
class uptime_collector : public collector_base<uptime_collector>,
                         public collector_plugin {
public:
    // Add plugin interface methods
    auto name() const -> std::string_view override {
        return collector_name;
    }

    auto collect() -> std::vector<metric> override {
        return collector_base<uptime_collector>::collect();
    }

    auto interval() const -> std::chrono::milliseconds override {
        return std::chrono::seconds(5);
    }

    auto get_metric_types() const -> std::vector<std::string> override {
        return do_get_metric_types();
    }

    // Existing CRTP methods unchanged
};
```

## Future Enhancements

- **Dynamic loading**: Load plugins from shared libraries (`.so`/`.dll`)
- **Plugin registry**: Centralized registration and discovery
- **Factory pattern**: Lazy instantiation via factory functions
- **Configuration schema**: JSON/YAML configuration validation

## See Also

- [collector_base.h](../include/kcenon/monitoring/collectors/collector_base.h) - CRTP base class
- [metric_collector_interface.h](../include/kcenon/monitoring/interfaces/metric_collector_interface.h) - Metric collection interface
- [Issue #423](https://github.com/kcenon/monitoring_system/issues/423) - Plugin architecture epic
