# Monitoring System

[![CI](https://github.com/kcenon/monitoring_system/actions/workflows/ci.yml/badge.svg)](https://github.com/kcenon/monitoring_system/actions/workflows/ci.yml)
[![License](https://img.shields.io/badge/License-BSD_3--Clause-blue.svg)](https://opensource.org/licenses/BSD-3-Clause)

A real-time performance monitoring system for C++20 applications with low-overhead metrics collection.

## Features

- **Low Overhead**: Minimal impact on application performance
- **Real-time Metrics**: System, thread pool, and worker metrics
- **Ring Buffer Storage**: Efficient circular buffer for historical data
- **Thread-safe**: Lock-free operations where possible
- **Extensible**: Easy to add custom metrics
- **Integration Ready**: Works seamlessly with Thread System

## Integration with Thread System

This monitoring system implements the monitoring interface from [Thread System](https://github.com/kcenon/thread_system):

```cpp
#include <monitoring_system/monitoring.h>
#include <thread_system/interfaces/service_container.h>

// Create and configure monitoring
auto monitor = std::make_shared<monitoring_module::monitoring>();
monitor->start();

// Register in service container
thread_module::service_container::global()
    .register_singleton<monitoring_interface::monitoring_interface>(monitor);

// Thread system components will now report metrics automatically
```

## Quick Start

### Basic Usage

```cpp
#include <monitoring_system/monitoring.h>

int main() {
    // Create monitoring instance
    auto monitor = std::make_shared<monitoring_module::monitoring>();
    
    // Start monitoring
    monitor->start();
    
    // Update metrics
    monitoring_interface::system_metrics sys_metrics;
    sys_metrics.cpu_usage_percent = 45;
    sys_metrics.memory_usage_bytes = 1024 * 1024 * 512; // 512MB
    sys_metrics.active_threads = 8;
    monitor->update_system_metrics(sys_metrics);
    
    // Get current snapshot
    auto snapshot = monitor->get_current_snapshot();
    
    // Get historical data
    auto history = monitor->get_recent_snapshots(10);
    
    return 0;
}
```

### Custom Metrics Collection

```cpp
class custom_collector : public monitoring_module::metrics_collector {
public:
    void collect(monitoring_interface::metrics_snapshot& snapshot) override {
        // Collect custom metrics
        snapshot.system.cpu_usage_percent = get_cpu_usage();
        snapshot.system.memory_usage_bytes = get_memory_usage();
    }
};

// Register custom collector
monitor->add_collector(std::make_unique<custom_collector>());
```

## Metrics Types

### System Metrics
- CPU usage percentage
- Memory usage in bytes
- Active thread count
- Total memory allocations

### Thread Pool Metrics
- Jobs completed
- Jobs pending
- Total execution time
- Average latency
- Worker thread count
- Idle thread count

### Worker Metrics
- Jobs processed per worker
- Processing time per worker
- Idle time
- Context switches

## Building

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

### Build Options

- `BUILD_TESTS`: Build unit tests (default: ON)
- `BUILD_BENCHMARKS`: Build performance benchmarks (default: OFF)
- `BUILD_SAMPLES`: Build example programs (default: ON)

## Installation

```bash
cmake --build . --target install
```

## CMake Integration

```cmake
find_package(MonitoringSystem REQUIRED)
target_link_libraries(your_target PRIVATE MonitoringSystem::monitoring)
```

## Performance

The monitoring system is designed for minimal overhead:
- Lock-free ring buffer for metrics storage
- Atomic operations for counter updates
- Configurable collection intervals
- Zero-allocation steady state operation

## License

BSD 3-Clause License - see LICENSE file for details.