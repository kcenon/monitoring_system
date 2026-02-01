# Plugin API Reference

Complete reference for the collector plugin API.

## Table of Contents

- [collector_plugin Interface](#collector_plugin-interface)
- [collector_registry API](#collector_registry-api)
- [plugin_metadata Structure](#plugin_metadata-structure)
- [Dynamic Plugin API](#dynamic-plugin-api)
- [Helper Functions](#helper-functions)

## collector_plugin Interface

### Overview

The `collector_plugin` interface is the core abstraction for all metric collectors. All plugins must inherit from this class and implement the pure virtual methods.

```cpp
class collector_plugin {
public:
    virtual ~collector_plugin() = default;

    // Required methods (pure virtual)
    virtual auto name() const -> std::string_view = 0;
    virtual auto collect() -> std::vector<metric> = 0;
    virtual auto interval() const -> std::chrono::milliseconds = 0;
    virtual auto is_available() const -> bool = 0;
    virtual auto get_metric_types() const -> std::vector<std::string> = 0;

    // Optional methods (with default implementations)
    virtual auto get_metadata() const -> plugin_metadata;
    virtual auto initialize(const config_map& config) -> bool;
    virtual void shutdown();
    virtual auto get_statistics() const -> stats_map;
};
```

### Required Methods

#### name()

Returns the unique name identifier for this plugin.

**Signature**:
```cpp
virtual auto name() const -> std::string_view = 0;
```

**Return Value**:
- A string view representing the plugin name
- Must be unique within the registry
- Recommended format: lowercase with underscores (e.g., "cpu_collector")

**Example**:
```cpp
auto name() const -> std::string_view override {
    return "my_collector";
}
```

**Notes**:
- The name is used for plugin lookup and discovery
- Added as "collector" tag to all metrics
- Should not change across plugin versions

---

#### collect()

Collects current metrics from this plugin.

**Signature**:
```cpp
virtual auto collect() -> std::vector<metric> = 0;
```

**Return Value**:
- Vector of collected metrics
- Empty vector if collection fails or no metrics available

**Performance Guidelines**:
- Should return quickly (< 100ms recommended)
- Avoid blocking I/O when possible
- Use async operations with timeout for slow data sources

**Thread Safety**:
- May be called concurrently from multiple threads
- Implementations must be thread-safe or document restrictions

**Error Handling**:
- Should not throw exceptions
- Return empty vector on error
- Optionally emit error metrics

**Example**:
```cpp
auto collect() -> std::vector<metric> override {
    std::vector<metric> metrics;

    try {
        metric cpu_metric;
        cpu_metric.name = "cpu_usage_percent";
        cpu_metric.value = get_cpu_usage();
        cpu_metric.timestamp = std::chrono::system_clock::now();
        cpu_metric.tags["unit"] = "percent";

        metrics.push_back(cpu_metric);

    } catch (const std::exception& e) {
        // Log error, return empty vector
        std::cerr << "Collection failed: " << e.what() << "\n";
    }

    return metrics;
}
```

---

#### interval()

Returns the collection interval for this plugin.

**Signature**:
```cpp
virtual auto interval() const -> std::chrono::milliseconds = 0;
```

**Return Value**:
- Collection interval in milliseconds
- Determines how often `collect()` is called

**Typical Values**:
| Metric Type | Interval | Rationale |
|-------------|----------|-----------|
| CPU, Memory | 1-5 seconds | Fast-changing, real-time monitoring |
| Disk, Network | 10-60 seconds | Moderate update frequency |
| SMART data, Logs | 5-15 minutes | Slow-changing, expensive to collect |

**Example**:
```cpp
auto interval() const -> std::chrono::milliseconds override {
    return std::chrono::seconds(5);  // Collect every 5 seconds
}
```

**Notes**:
- Registry uses this for scheduling collection tasks
- Can be overridden per-instance via configuration
- Balance between freshness and overhead

---

#### is_available()

Checks if this plugin is available on the current system.

**Signature**:
```cpp
virtual auto is_available() const -> bool = 0;
```

**Return Value**:
- `true` if plugin can collect metrics
- `false` if platform incompatible or resources unavailable

**Availability Checks May Include**:
- Platform compatibility (Linux-only, Windows-only)
- Hardware presence (GPU, sensors)
- Permission checks (root required)
- Resource availability (proc filesystem, WMI)
- Runtime dependencies (libraries, drivers)

**Example**:
```cpp
auto is_available() const -> bool override {
#ifdef __linux__
    // Check for /proc filesystem
    return std::filesystem::exists("/proc/stat");
#else
    return false;  // Not available on non-Linux
#endif
}
```

**Notes**:
- Called before registration
- Unavailable plugins are typically not registered
- Should be fast (< 1ms)
- Should be side-effect free

---

#### get_metric_types()

Returns the list of metric types this plugin produces.

**Signature**:
```cpp
virtual auto get_metric_types() const -> std::vector<std::string> = 0;
```

**Return Value**:
- Vector of metric type names (e.g., "cpu_usage", "memory_free")
- Used for filtering and documentation

**Example**:
```cpp
auto get_metric_types() const -> std::vector<std::string> override {
    return {
        "cpu_usage_percent",
        "cpu_user_time",
        "cpu_system_time"
    };
}
```

---

### Optional Methods

#### get_metadata()

Returns metadata describing this plugin.

**Signature**:
```cpp
virtual auto get_metadata() const -> plugin_metadata;
```

**Default Implementation**:
```cpp
virtual auto get_metadata() const -> plugin_metadata {
    return plugin_metadata{
        .name = name(),
        .description = "",
        .category = plugin_category::custom,
        .version = "1.0.0",
        .dependencies = {},
        .requires_platform_support = false
    };
}
```

**Example**:
```cpp
auto get_metadata() const -> plugin_metadata override {
    return plugin_metadata{
        .name = "cpu_collector",
        .description = "Collects CPU usage statistics",
        .category = plugin_category::hardware,
        .version = "2.1.0",
        .dependencies = {},
        .requires_platform_support = true
    };
}
```

---

#### initialize()

Initializes the plugin with configuration.

**Signature**:
```cpp
virtual auto initialize(const config_map& config) -> bool;
```

**Parameters**:
- `config`: Configuration key-value pairs

**Return Value**:
- `true` if initialization succeeded
- `false` if initialization failed

**Default Implementation**:
```cpp
virtual auto initialize(const config_map& /* config */) -> bool {
    return true;  // Always succeeds
}
```

**Example**:
```cpp
auto initialize(const config_map& config) -> bool override {
    // Read configuration
    if (config.contains("sample_rate")) {
        sample_rate_ = std::stoi(config.at("sample_rate"));
    }

    // Perform one-time setup
    if (!connect_to_sensor()) {
        return false;
    }

    initialized_ = true;
    return true;
}
```

**Notes**:
- Called once after plugin registration
- Failure does not prevent registration
- Failed plugins may fail during collection

---

#### shutdown()

Shuts down the plugin and releases resources.

**Signature**:
```cpp
virtual void shutdown();
```

**Default Implementation**:
```cpp
virtual void shutdown() {}  // No-op
```

**Example**:
```cpp
void shutdown() override {
    // Close connections
    if (connection_) {
        connection_->close();
        connection_.reset();
    }

    // Clear caches
    cached_data_.clear();

    // Reset state
    initialized_ = false;
}
```

**Notes**:
- Called before plugin destruction
- Safe to call multiple times
- Should not throw exceptions

---

#### get_statistics()

Returns plugin-specific statistics.

**Signature**:
```cpp
virtual auto get_statistics() const -> stats_map;
```

**Default Implementation**:
```cpp
virtual auto get_statistics() const -> stats_map {
    return {};  // No statistics
}
```

**Example**:
```cpp
auto get_statistics() const -> stats_map override {
    stats_map stats;
    stats["total_collections"] = std::to_string(collection_count_);
    stats["total_errors"] = std::to_string(error_count_);
    stats["avg_duration_ms"] = std::to_string(avg_duration_ms_);
    stats["last_collection_time"] = format_timestamp(last_collection_);
    return stats;
}
```

**Common Statistics**:
- Collection count
- Error count
- Average/min/max collection duration
- Last collection timestamp
- Cache hit rate

---

## collector_registry API

### Overview

The `collector_registry` manages the lifecycle of all collector plugins. It uses a singleton pattern to provide a global instance.

### Static Methods

#### instance()

Returns the singleton registry instance.

**Signature**:
```cpp
static auto instance() -> collector_registry&;
```

**Example**:
```cpp
auto& registry = collector_registry::instance();
```

---

### Plugin Registration

#### register_plugin()

Registers a plugin instance.

**Signature**:
```cpp
auto register_plugin(std::unique_ptr<collector_plugin> plugin) -> bool;
```

**Parameters**:
- `plugin`: Unique pointer to the plugin (registry takes ownership)

**Return Value**:
- `true` if registration succeeded
- `false` if plugin name already exists or plugin is unavailable

**Example**:
```cpp
auto plugin = std::make_unique<my_plugin>();
if (!registry.register_plugin(std::move(plugin))) {
    std::cerr << "Registration failed\n";
}
```

**Notes**:
- Registry takes ownership of the plugin
- Calls `is_available()` before registration
- Unavailable plugins are not registered

---

#### unregister_plugin()

Unregisters a plugin by name.

**Signature**:
```cpp
auto unregister_plugin(std::string_view name) -> bool;
```

**Parameters**:
- `name`: Plugin name to remove

**Return Value**:
- `true` if plugin was found and removed
- `false` if plugin not found

**Example**:
```cpp
if (registry.unregister_plugin("my_plugin")) {
    std::cout << "Plugin removed\n";
}
```

**Notes**:
- Calls `shutdown()` on the plugin before removal
- Plugin is destroyed after unregistration

---

#### register_factory()

Registers a factory function for lazy instantiation.

**Signature**:
```cpp
template <typename T>
void register_factory(std::string_view name);
```

**Template Parameter**:
- `T`: Plugin type (must derive from `collector_plugin`)

**Parameters**:
- `name`: Plugin name (used for lookup)

**Example**:
```cpp
// Register factory
registry.register_factory<my_plugin>("my_plugin");

// Plugin is instantiated on first access
auto* plugin = registry.get_plugin("my_plugin");
```

**Notes**:
- Defers plugin construction until first access
- Useful for expensive-to-construct plugins
- Thread-safe instantiation

---

### Plugin Discovery

#### get_plugin()

Retrieves a plugin by name.

**Signature**:
```cpp
auto get_plugin(std::string_view name) -> collector_plugin*;
```

**Parameters**:
- `name`: Plugin name

**Return Value**:
- Pointer to plugin if found
- `nullptr` if not found

**Example**:
```cpp
auto* plugin = registry.get_plugin("cpu_collector");
if (plugin) {
    auto metrics = plugin->collect();
}
```

**Notes**:
- Triggers factory instantiation if not yet created
- Thread-safe access

---

#### get_plugins()

Returns all registered plugins.

**Signature**:
```cpp
auto get_plugins() -> std::vector<collector_plugin*>;
```

**Return Value**:
- Vector of pointers to all plugins

**Example**:
```cpp
auto plugins = registry.get_plugins();
for (auto* plugin : plugins) {
    std::cout << plugin->name() << "\n";
}
```

---

#### get_plugins_by_category()

Returns plugins in a specific category.

**Signature**:
```cpp
auto get_plugins_by_category(plugin_category category)
    -> std::vector<collector_plugin*>;
```

**Parameters**:
- `category`: Plugin category filter

**Return Value**:
- Vector of pointers to matching plugins

**Example**:
```cpp
auto hw_plugins = registry.get_plugins_by_category(
    plugin_category::hardware
);
```

---

#### has_plugin()

Checks if a plugin is registered.

**Signature**:
```cpp
auto has_plugin(std::string_view name) const -> bool;
```

**Parameters**:
- `name`: Plugin name

**Return Value**:
- `true` if plugin exists (instantiated or factory-registered)
- `false` if not found

**Example**:
```cpp
if (registry.has_plugin("my_plugin")) {
    std::cout << "Plugin found\n";
}
```

---

#### plugin_count()

Returns the number of registered plugins.

**Signature**:
```cpp
auto plugin_count() const -> size_t;
```

**Return Value**:
- Count of plugins (both instantiated and factory-registered)

**Example**:
```cpp
std::cout << "Total plugins: " << registry.plugin_count() << "\n";
```

---

### Lifecycle Management

#### initialize_all()

Initializes all registered plugins.

**Signature**:
```cpp
auto initialize_all(const config_map& config = {}) -> size_t;
```

**Parameters**:
- `config`: Optional configuration map

**Return Value**:
- Number of successfully initialized plugins

**Example**:
```cpp
config_map config;
config["global_option"] = "value";

size_t initialized = registry.initialize_all(config);
std::cout << "Initialized: " << initialized << " plugins\n";
```

**Notes**:
- Calls `initialize()` on each plugin
- Plugins that fail remain registered
- Safe to call multiple times

---

#### shutdown_all()

Shuts down all registered plugins.

**Signature**:
```cpp
void shutdown_all();
```

**Example**:
```cpp
registry.shutdown_all();
```

**Notes**:
- Calls `shutdown()` in reverse registration order
- Safe to call multiple times

---

### Dynamic Plugin Loading

#### load_plugin()

Loads a plugin from a shared library.

**Signature**:
```cpp
auto load_plugin(std::string_view path) -> bool;
```

**Parameters**:
- `path`: Path to shared library (.so/.dylib/.dll)

**Return Value**:
- `true` if plugin loaded and registered successfully
- `false` if loading failed

**Example**:
```cpp
if (!registry.load_plugin("./libmy_plugin.so")) {
    std::cerr << "Load failed: "
              << registry.get_plugin_loader_error() << "\n";
}
```

---

#### unload_plugin()

Unloads a dynamically loaded plugin.

**Signature**:
```cpp
auto unload_plugin(std::string_view name) -> bool;
```

**Parameters**:
- `name`: Plugin name

**Return Value**:
- `true` if plugin was unloaded successfully
- `false` if plugin not found or not dynamically loaded

**Example**:
```cpp
registry.unload_plugin("my_plugin");
```

---

#### get_plugin_loader_error()

Returns the last error from plugin loader.

**Signature**:
```cpp
auto get_plugin_loader_error() const -> std::string;
```

**Return Value**:
- Error message string
- Empty string if no error

**Example**:
```cpp
if (!registry.load_plugin(path)) {
    std::cout << "Error: " << registry.get_plugin_loader_error() << "\n";
}
```

---

### Statistics

#### get_registry_stats()

Returns registry statistics.

**Signature**:
```cpp
auto get_registry_stats() const -> std::map<std::string, size_t>;
```

**Return Value**:
- Map of statistic name to value

**Available Statistics**:
| Statistic | Description |
|-----------|-------------|
| `total_plugins` | Total number of registered plugins |
| `initialized_plugins` | Number of initialized plugins |
| `available_plugins` | Number of available plugins |
| `category_system_count` | Count of system plugins |
| `category_hardware_count` | Count of hardware plugins |
| `category_platform_count` | Count of platform plugins |
| `category_network_count` | Count of network plugins |
| `category_process_count` | Count of process plugins |
| `category_custom_count` | Count of custom plugins |

**Example**:
```cpp
auto stats = registry.get_registry_stats();
for (const auto& [name, value] : stats) {
    std::cout << name << ": " << value << "\n";
}
```

---

### Utility Methods

#### clear()

Clears all plugins (for testing).

**Signature**:
```cpp
void clear();
```

**Example**:
```cpp
// In test teardown
registry.clear();
```

**Notes**:
- Calls `shutdown_all()` first
- Removes all plugins
- Use with caution in production code

---

## plugin_metadata Structure

### Overview

Metadata describing a collector plugin.

```cpp
struct plugin_metadata {
    std::string_view name;
    std::string_view description;
    plugin_category category;
    std::string_view version;
    std::vector<std::string_view> dependencies;
    bool requires_platform_support{false};
};
```

### Fields

| Field | Type | Description |
|-------|------|-------------|
| `name` | `std::string_view` | Unique plugin identifier |
| `description` | `std::string_view` | Human-readable description |
| `category` | `plugin_category` | Plugin category (system/hardware/etc.) |
| `version` | `std::string_view` | Plugin version (semver recommended) |
| `dependencies` | `std::vector<std::string_view>` | Required dependencies |
| `requires_platform_support` | `bool` | Platform-specific plugin flag |

---

## Dynamic Plugin API

### Overview

C ABI for dynamically loaded plugins.

### Required Exports

All dynamically loaded plugins must export these functions:

```cpp
extern "C" {
    PLUGIN_EXPORT collector_plugin* create_plugin();
    PLUGIN_EXPORT void destroy_plugin(collector_plugin* plugin);
    PLUGIN_EXPORT const plugin_api_metadata* get_plugin_info();
}
```

### IMPLEMENT_PLUGIN Macro

Simplifies plugin export implementation:

```cpp
IMPLEMENT_PLUGIN(
    PluginClass,      // Plugin class name
    "plugin_name",    // Plugin name
    "1.0.0",         // Version
    "Description",   // Description
    "Author Name",   // Author
    "category"       // Category
)
```

**Example**:
```cpp
#include "kcenon/monitoring/plugins/plugin_api.h"

class my_dynamic_plugin : public collector_plugin {
    // Implementation...
};

IMPLEMENT_PLUGIN(
    my_dynamic_plugin,
    "my_dynamic_plugin",
    "1.0.0",
    "My dynamically loaded plugin",
    "Your Name",
    "custom"
)
```

---

## Helper Functions

### Metric Creation

```cpp
// Helper to create a metric
inline auto create_metric(
    std::string_view name,
    double value,
    std::string_view unit = ""
) -> metric {
    metric m;
    m.name = std::string(name);
    m.value = value;
    m.timestamp = std::chrono::system_clock::now();
    if (!unit.empty()) {
        m.tags["unit"] = std::string(unit);
    }
    return m;
}
```

### Platform Detection

```cpp
// Check if running on Linux
inline auto is_linux() -> bool {
#ifdef __linux__
    return true;
#else
    return false;
#endif
}

// Check if running on Windows
inline auto is_windows() -> bool {
#ifdef _WIN32
    return true;
#else
    return false;
#endif
}

// Check if running on macOS
inline auto is_macos() -> bool {
#ifdef __APPLE__
    return true;
#else
    return false;
#endif
}
```

---

## See Also

- [Plugin Development Guide](plugin_development_guide.md) - Step-by-step guide
- [Dynamic Plugin Example](../examples/plugin_example/README.md) - Complete example
