# Integration Guide - Monitoring System

> **Language:** **English** | [한국어](INTEGRATION.kr.md)

## Overview

This guide describes how to integrate monitoring_system with other modules in the ecosystem. Monitoring System provides comprehensive observability and reliability patterns that seamlessly integrate with common, thread, and logger systems for production-grade monitoring capabilities.

**Version:** 0.2.0.0
**Last Updated:** 2025-10-22

---

## Table of Contents

- [Quick Start](#quick-start)
- [Integration with common_system](#integration-with-common_system)
- [Integration with thread_system](#integration-with-thread_system)
- [Integration with logger_system](#integration-with-logger_system)
- [Build Configuration](#build-configuration)
- [Performance Considerations](#performance-considerations)
- [Troubleshooting](#troubleshooting)

---

## Quick Start

### Basic Integration

```cpp
#include <kcenon/monitoring/core/performance_monitor.h>
#include <kcenon/monitoring/tracing/distributed_tracer.h>
#include <kcenon/monitoring/health/health_monitor.h>

using namespace monitoring_system;

// Create comprehensive monitoring setup
performance_monitor perf_monitor("my_application");
auto& tracer = global_tracer();
health_monitor health_monitor;

// Enable performance metrics collection
perf_monitor.enable_collection(true);

// Start distributed trace
auto span_result = tracer.start_span("main_operation", "application");
if (span_result) {
    auto span = span_result.value();
    span->set_tag("operation.type", "batch_processing");

    // Your operation here

    tracer.finish_span(span);
}
```

### CMake Integration

```cmake
find_package(monitoring_system CONFIG REQUIRED)

target_link_libraries(your_target PRIVATE
    kcenon::monitoring_system
)
```

---

## Integration with common_system

Monitoring System uses common_system for core interfaces and error handling patterns.

### IMonitor Interface Integration

```cpp
#include <kcenon/common/interfaces/monitoring_interface.h>
#include <kcenon/monitoring/core/performance_monitor.h>

// Implement IMonitor interface
class MyMonitorAdapter : public common::interfaces::IMonitor {
public:
    auto configure(const std::string& config) -> common::Result<void> override {
        // Configuration logic
        return common::ok();
    }

    auto start() -> common::Result<void> override {
        monitor_.enable_collection(true);
        return common::ok();
    }

    auto stop() -> common::Result<void> override {
        monitor_.enable_collection(false);
        return common::ok();
    }

    auto collect_now() -> common::Result<void> override {
        auto snapshot = monitor_.collect();
        if (!snapshot) {
            return common::error<void>(
                common::error_codes::COLLECTION_FAILED,
                "Failed to collect metrics",
                "monitor_adapter"
            );
        }
        return common::ok();
    }

private:
    monitoring_system::performance_monitor monitor_{"my_service"};
};
```

### Result<T> Pattern Integration

```cpp
#include <kcenon/common/patterns/result.h>
#include <kcenon/monitoring/core/result_types.h>

// Use Result<T> for comprehensive error handling
common::Result<metrics_snapshot> collect_metrics() {
    auto result = monitor.collect();
    if (!result) {
        return common::error<metrics_snapshot>(
            common::error_codes::COLLECTION_FAILED,
            result.error().message,
            "metrics_collector"
        );
    }

    return common::ok(result.value());
}

// Chain operations with monadic pattern
auto result = collect_metrics()
    .and_then(validate_metrics)
    .map(transform_metrics);

if (result.is_ok()) {
    auto metrics = result.value();
    // Use metrics
} else {
    auto error = result.error();
    log_error(error.message);
}
```

### Build Configuration

```cmake
# Enable common_system integration (mandatory)
set(BUILD_WITH_COMMON_SYSTEM ON)

find_package(monitoring_system CONFIG REQUIRED)
find_package(common_system CONFIG REQUIRED)

target_link_libraries(your_target PRIVATE
    kcenon::monitoring_system
    kcenon::common_system
)
```

---

## Integration with thread_system

Monitor thread pool performance and collect threading metrics.

### Thread Pool Monitoring

```cpp
#include <kcenon/monitoring/adapters/thread_system_adapter.h>
#include <thread_system/thread_pool.h>

// Create thread pool with monitoring
auto thread_pool = thread_system::create_thread_pool(8);

// Create monitoring adapter
monitoring_system::thread_system_adapter adapter;
adapter.attach_to_pool(thread_pool);

// Thread pool metrics are now automatically collected
auto metrics = adapter.collect_metrics();
if (metrics) {
    auto data = metrics.value();
    std::cout << "Active threads: " << data.get_metric("threads.active") << "\n";
    std::cout << "Queue depth: " << data.get_metric("queue.depth") << "\n";
    std::cout << "Tasks completed: " << data.get_metric("tasks.completed") << "\n";
}
```

### Async Operation Tracing

```cpp
#include <kcenon/monitoring/tracing/distributed_tracer.h>
#include <thread_system/async_executor.h>

auto& tracer = monitoring_system::global_tracer();

// Start trace for async operation
auto span_result = tracer.start_span("async_operation", "worker_pool");
if (span_result) {
    auto span = span_result.value();

    // Execute async with trace context
    thread_pool->submit([span, &tracer]() {
        auto child_span = tracer.start_child_span(span, "background_task").value();

        // Your async operation
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        tracer.finish_span(child_span);
    });

    tracer.finish_span(span);
}
```

### Build Configuration

```cmake
# Enable thread_system integration
set(MONITORING_USE_THREAD_SYSTEM ON)

find_package(monitoring_system CONFIG REQUIRED)
find_package(thread_system CONFIG REQUIRED)

target_link_libraries(your_target PRIVATE
    kcenon::monitoring_system
    thread_system::interfaces
)
```

---

## Integration with logger_system

Combine monitoring metrics with structured logging for comprehensive observability.

### Logger Integration with DI Container

```cpp
#include <kcenon/monitoring/core/di_container.h>
#include <kcenon/common/interfaces/logger_interface.h>
#include <logger_system/logger.h>

// Register logger in DI container
auto& container = monitoring_system::global_di_container();

container.register_singleton<common::interfaces::ILogger, logger_system::logger>();

// Resolve logger in monitoring components
auto logger_result = container.resolve<common::interfaces::ILogger>();
if (logger_result) {
    auto logger = logger_result.value();
    logger->info("Monitoring system initialized", "monitoring");
}
```

### Trace Context in Logs

```cpp
#include <kcenon/monitoring/tracing/distributed_tracer.h>
#include <kcenon/monitoring/context/thread_context.h>
#include <logger_system/logger.h>

auto& tracer = monitoring_system::global_tracer();
auto logger = logger_system::get_logger();

// Start trace
auto span_result = tracer.start_span("user_request", "web_service");
if (span_result) {
    auto span = span_result.value();

    // Set trace context
    monitoring_system::thread_context::set_trace_id(span->trace_id());
    monitoring_system::thread_context::set_span_id(span->span_id());

    // Log with trace context
    logger->info("Processing request", "handler", {
        {"trace_id", span->trace_id()},
        {"span_id", span->span_id()},
        {"user_id", "12345"}
    });

    tracer.finish_span(span);
}
```

### Event Bus Integration

```cpp
#include <kcenon/monitoring/core/event_bus.h>
#include <logger_system/logger.h>

auto& event_bus = monitoring_system::global_event_bus();
auto logger = logger_system::get_logger();

// Subscribe to monitoring events
event_bus.subscribe<monitoring_system::metric_collection_event>(
    [logger](const auto& event) {
        logger->debug("Metrics collected", "monitoring", {
            {"metric_count", event.metrics.size()},
            {"timestamp", event.timestamp}
        });
    }
);

// Subscribe to health check events
event_bus.subscribe<monitoring_system::health_check_event>(
    [logger](const auto& event) {
        if (event.status == monitoring_system::health_status::unhealthy) {
            logger->error("Health check failed", "health", {
                {"check_name", event.check_name},
                {"message", event.message}
            });
        }
    }
);
```

### Build Configuration

```cmake
# Enable logger_system integration
set(BUILD_WITH_LOGGER_SYSTEM ON)

find_package(monitoring_system CONFIG REQUIRED)
find_package(logger_system CONFIG REQUIRED)

target_link_libraries(your_target PRIVATE
    kcenon::monitoring_system
    kcenon::logger_system
)
```

---

## Build Configuration

### CMake Options

```cmake
# Core options
option(MONITORING_BUILD_TESTS "Build unit tests" ON)
option(MONITORING_BUILD_INTEGRATION_TESTS "Build integration tests" ON)
option(MONITORING_BUILD_EXAMPLES "Build example programs" ON)
option(MONITORING_BUILD_BENCHMARKS "Build benchmarks" OFF)

# Integration options
option(BUILD_WITH_COMMON_SYSTEM "Enable common_system integration" ON)
option(MONITORING_USE_THREAD_SYSTEM "Enable thread_system integration" OFF)
option(BUILD_WITH_LOGGER_SYSTEM "Enable logger_system integration" OFF)

# Sanitizer options
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(ENABLE_COVERAGE "Enable coverage reporting" OFF)
```

### Full Integration Example

```cmake
cmake_minimum_required(VERSION 3.20)
project(my_application)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable all integrations
set(BUILD_WITH_COMMON_SYSTEM ON)
set(MONITORING_USE_THREAD_SYSTEM ON)
set(BUILD_WITH_LOGGER_SYSTEM ON)

# Find all required packages
find_package(monitoring_system CONFIG REQUIRED)
find_package(common_system CONFIG REQUIRED)
find_package(thread_system CONFIG REQUIRED)
find_package(logger_system CONFIG REQUIRED)

add_executable(my_app main.cpp)

target_link_libraries(my_app PRIVATE
    kcenon::monitoring_system
    kcenon::common_system
    thread_system::interfaces
    kcenon::logger_system
)
```

---

## Performance Considerations

### Monitoring Overhead

| Component | Overhead | Throughput | Use Case |
|-----------|----------|------------|----------|
| **Atomic Counters** | <10ns | 10M ops/s | High-frequency metrics |
| **Trace Spans** | <50ns | 2.5M spans/s | Request tracing |
| **Health Checks** | <2μs | 500K checks/s | System health |
| **Event Publishing** | <200ns | 5.8M events/s | Event-driven monitoring |

### Optimization Tips

1. **Use atomic counters for high-frequency metrics**
   ```cpp
   // Fast path: atomic increment
   perf_monitor.increment_counter("requests_total");

   // Slow path: complex calculations (batch these)
   perf_monitor.record_histogram("response_time", calculate_time());
   ```

2. **Enable sampling for high-throughput scenarios**
   ```cpp
   // Configure 10% sampling rate
   monitoring_system::monitoring_config config;
   config.sampling_rate = 0.1;
   tracer.configure(config);
   ```

3. **Batch health checks for efficiency**
   ```cpp
   // Avoid: Check health on every request
   if (health_monitor.check_health().status == health_status::healthy) {
       process_request();
   }

   // Better: Cache health status with TTL
   static auto last_check = std::chrono::steady_clock::now();
   static auto cached_status = health_status::unknown;

   auto now = std::chrono::steady_clock::now();
   if (now - last_check > std::chrono::seconds(5)) {
       cached_status = health_monitor.check_health().status;
       last_check = now;
   }
   ```

4. **Use scoped timers for automatic measurement**
   ```cpp
   // Scoped timer automatically records duration
   {
       auto timer = perf_monitor.start_timer("database_query");
       // Query executes
   } // Timer records on destruction
   ```

---

## Troubleshooting

### Common Issues

#### 1. Link Error: `undefined reference to monitoring_system::performance_monitor`

**Solution**: Ensure monitoring_system is properly linked
```cmake
target_link_libraries(your_target PRIVATE kcenon::monitoring_system)
```

#### 2. Missing common_system Interfaces

**Problem**: Compilation errors about missing IMonitor or Result<T>
**Solution**: Enable common_system integration
```cmake
set(BUILD_WITH_COMMON_SYSTEM ON)
find_package(common_system CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE kcenon::common_system)
```

#### 3. Thread System Adapter Not Found

**Problem**: Cannot find thread_system_adapter
**Solution**: Enable thread_system integration and link properly
```cmake
set(MONITORING_USE_THREAD_SYSTEM ON)
find_package(thread_system CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE thread_system::interfaces)
```

#### 4. Result Type Mismatch

**Problem**: Incompatible result types between systems
**Solution**: Use consistent Result<T> from common_system
```cpp
#include <kcenon/common/patterns/result.h>

// All systems use common::Result<T>
common::Result<metrics_snapshot> collect() {
    auto internal_result = monitor_.collect();
    if (!internal_result) {
        return common::error<metrics_snapshot>(
            common::error_codes::COLLECTION_FAILED,
            internal_result.error().message,
            "collector"
        );
    }
    return common::ok(internal_result.value());
}
```

#### 5. High Monitoring Overhead

**Problem**: Monitoring consuming too many resources
**Solution**: Configure sampling and batch operations
```cpp
// Enable sampling
config.sampling_rate = 0.1; // 10% sampling

// Batch metric collection
config.collection_interval = std::chrono::seconds(10);

// Disable verbose tracing in production
config.enable_distributed_tracing = false;
```

---

## Example Applications

### Complete Integration Example

See [examples/](examples/) for complete applications demonstrating:
- Performance monitoring with common_system Result<T>
- Thread pool monitoring via thread_system integration
- Distributed tracing with logger_system correlation
- Health checks with circuit breaker patterns
- Event-driven monitoring with event bus

### Running the Examples

```bash
cd monitoring_system
mkdir build && cd build
cmake .. -DMONITORING_BUILD_EXAMPLES=ON \
         -DBUILD_WITH_COMMON_SYSTEM=ON \
         -DMONITORING_USE_THREAD_SYSTEM=ON \
         -DBUILD_WITH_LOGGER_SYSTEM=ON
make
./examples/basic_monitoring_example
./examples/distributed_tracing_example
./examples/health_reliability_example
./examples/logger_di_integration_example
```

---

## Support

- **Documentation**: [docs/](docs/)
- **API Reference**: [docs/API_REFERENCE.md](docs/API_REFERENCE.md)
- **Architecture Guide**: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)
- **Issues**: [GitHub Issues](https://github.com/kcenon/monitoring_system/issues)
- **Email**: kcenon@naver.com

---

**Last Updated**: 2025-10-22
**Maintainer**: kcenon@naver.com
