# Dynamic Plugin Loading Example

This example demonstrates how to create and use dynamically loaded collector plugins.

## Overview

The monitoring system supports loading collector plugins from shared libraries at runtime. This allows:

- Third-party plugins without recompilation
- Plugin distribution as separate libraries
- On-demand plugin loading
- Plugin updates without rebuilding the main application

## Files

- `example_plugin.cpp` - Sample plugin implementation
- `plugin_loader_example.cpp` - Example program that loads and uses the plugin
- `CMakeLists.txt` - Build configuration for the plugin

## Building

### Build the Plugin Library

```bash
cd examples/plugin_example
mkdir build
cd build
cmake ..
make
```

This creates:
- Linux: `libexample_plugin.so`
- macOS: `libexample_plugin.dylib`
- Windows: `example_plugin.dll`

### Build the Example Program

From the project root:

```bash
mkdir build
cd build
cmake ..
make
```

## Running

```bash
# Linux
./plugin_loader_example ./libexample_plugin.so

# macOS
./plugin_loader_example ./libexample_plugin.dylib

# Windows
plugin_loader_example.exe example_plugin.dll
```

## Expected Output

```
=== Dynamic Plugin Loading Example ===

Loading plugin from: ./libexample_plugin.so
Plugin loaded successfully

Plugin Metadata:
  Name: example_plugin
  Description: Example dynamically loaded collector plugin
  Version: 1.0.0
  Available: yes

Initializing plugin...
Plugin initialized

Collecting metrics (5 iterations)...

Iteration 1:
  example.cpu_usage: 45.23 % [plugin=example, type=cpu]
  example.memory_usage: 512.67 MB [plugin=example, type=memory]
  example.request_count: 1 requests [plugin=example, type=counter]

Iteration 2:
  example.cpu_usage: 38.91 % [plugin=example, type=cpu]
  example.memory_usage: 487.32 MB [plugin=example, type=memory]
  example.request_count: 2 requests [plugin=example, type=counter]

...

Shutting down plugin...
Unloading plugin...
Plugin unloaded successfully

=== Example Complete ===
```

## Creating Your Own Plugin

### 1. Implement the collector_plugin Interface

```cpp
#include "kcenon/monitoring/plugins/collector_plugin.h"
#include "kcenon/monitoring/plugins/plugin_api.h"

class my_plugin : public kcenon::monitoring::collector_plugin {
public:
    auto name() const -> std::string_view override {
        return "my_plugin";
    }

    auto initialize(const config_map& config) -> bool override {
        // Initialize your plugin
        return true;
    }

    auto shutdown() -> void override {
        // Clean up resources
    }

    auto collect() -> std::vector<metric_data> override {
        // Collect and return metrics
        std::vector<metric_data> metrics;
        // ... add metrics ...
        return metrics;
    }

    auto is_available() const -> bool override {
        // Check if plugin is available on this platform
        return true;
    }

    auto get_metadata() const -> plugin_metadata_t override {
        return plugin_metadata_t{
            plugin_category::custom,
            plugin_type::collector,
            "My Plugin",
            "1.0.0",
            "Description"
        };
    }
};
```

### 2. Export the Plugin

Use the `IMPLEMENT_PLUGIN` macro at the end of your source file:

```cpp
IMPLEMENT_PLUGIN(
    my_plugin,              // Plugin class name
    "my_plugin",           // Plugin name (must match name() method)
    "1.0.0",              // Version
    "My custom plugin",   // Description
    "Your Name",          // Author
    "custom"              // Category
)
```

### 3. Build as Shared Library

Create a `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.20)
project(my_plugin VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(my_plugin SHARED my_plugin.cpp)

target_include_directories(my_plugin PRIVATE
    ${MONITORING_SYSTEM_INCLUDE_DIR}
)

set_target_properties(my_plugin PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)
```

Build:

```bash
mkdir build
cd build
cmake ..
make
```

### 4. Load and Use

```cpp
#include "kcenon/monitoring/plugins/collector_registry.h"

auto& registry = kcenon::monitoring::collector_registry::instance();

// Load plugin
if (!registry.load_plugin("/path/to/libmy_plugin.so")) {
    std::cerr << "Load failed: " << registry.get_plugin_loader_error() << "\n";
    return;
}

// Get and use plugin
auto* plugin = registry.get_plugin("my_plugin");
if (plugin) {
    plugin->initialize({});
    auto metrics = plugin->collect();
    // ... use metrics ...
}

// Unload when done
registry.unload_plugin("my_plugin");
```

## Plugin API Version Compatibility

Plugins must be compiled with the same `PLUGIN_API_VERSION` as the monitoring system. If the versions don't match, the plugin will fail to load with an error message:

```
Incompatible API version: plugin=2, expected=1
```

To check the API version:

```cpp
#include "kcenon/monitoring/plugins/plugin_api.h"
std::cout << "API Version: " << PLUGIN_API_VERSION << "\n";
```

## Platform-Specific Notes

### Linux

- Use `.so` extension
- Compile with `-fPIC` flag
- Link with `-ldl` for dynamic loading

### macOS

- Use `.dylib` extension
- Compile with `-fPIC` flag
- May need to set `DYLD_LIBRARY_PATH` environment variable

### Windows

- Use `.dll` extension
- Compile with `/DEXPORT_PLUGIN` or set `WINDOWS_EXPORT_ALL_SYMBOLS`
- Put DLL in same directory as executable or in PATH

## Error Handling

Check for errors after loading:

```cpp
if (!registry.load_plugin(path)) {
    std::string error = registry.get_plugin_loader_error();
    // Handle error...
}
```

Common errors:

- `file_not_found` - Plugin file doesn't exist
- `library_load_failed` - Failed to load shared library (missing dependencies)
- `symbol_not_found` - Required export function not found
- `incompatible_api_version` - Plugin compiled with different API version
- `plugin_unavailable` - Plugin's `is_available()` returned false
- `already_loaded` - Plugin with same name already loaded

## Security Considerations

- Validate plugin paths before loading
- Only load plugins from trusted sources
- Check API version compatibility
- Consider implementing plugin signing/verification
- Be aware that plugins run with full application permissions

## Performance

- Plugin loading incurs one-time cost for library loading
- Subsequent metric collection has same performance as built-in collectors
- Unloading plugins releases all associated resources
