# Monitoring System

A modern, high-performance monitoring system for C++ applications, integrating seamlessly with thread_system and logger_system.

## Features

### Phase 1: Core Architecture (✅ Complete)
- ✅ **Result Pattern Error Handling**: Explicit error handling without exceptions
- ✅ **Comprehensive Error Codes**: Categorized error codes for all operations
- ✅ **Dependency Injection**: Service container with lifetime management
- ✅ **Monitorable Interface**: Standardized interface for component monitoring
- ✅ **Thread Context Integration**: Metadata enrichment and context propagation

## Building

### Requirements
- C++20 compatible compiler (GCC 10+, Clang 12+, MSVC 2019+)
- CMake 3.20+
- Optional: GTest for unit tests

### Build Instructions

```bash
# Clone the repository
git clone https://github.com/yourusername/monitoring_system.git
cd monitoring_system

# Create build directory
mkdir build && cd build

# Configure
cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DMONITORING_BUILD_TESTS=ON \
         -DMONITORING_BUILD_EXAMPLES=ON

# Build
make -j$(nproc)

# Run tests
./tests/monitoring_system_tests

# Run example
./examples/result_pattern_example
```

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `MONITORING_BUILD_TESTS` | ON | Build unit tests |
| `MONITORING_BUILD_EXAMPLES` | ON | Build example programs |
| `MONITORING_BUILD_BENCHMARKS` | OFF | Build performance benchmarks |
| `MONITORING_USE_THREAD_SYSTEM` | OFF | Enable thread_system integration |
| `MONITORING_USE_LOGGER_SYSTEM` | OFF | Enable logger_system integration |
| `ENABLE_ASAN` | OFF | Enable AddressSanitizer |
| `ENABLE_TSAN` | OFF | Enable ThreadSanitizer |
| `ENABLE_UBSAN` | OFF | Enable UndefinedBehaviorSanitizer |

## Usage

### Thread Context

Thread-local context enables request tracing and correlation:

```cpp
#include <monitoring/context/thread_context.h>

using namespace monitoring_system;

// Set context for current thread
thread_context::create("request-123");
thread_context::current()->correlation_id = "corr-456";
thread_context::current()->user_id = "user-789";
thread_context::current()->add_tag("environment", "production");

// Use RAII scope for automatic cleanup
{
    context_scope scope("scoped-request");
    // Context automatically cleared when scope exits
}

// Propagate context across threads
context_propagator propagator = context_propagator::from_current();

std::thread worker([propagator]() {
    propagator.apply();  // Apply parent thread's context
    // Worker has same context as parent
});

// Context-aware monitoring
class my_collector : public context_metrics_collector {
public:
    result<metrics_snapshot> collect() override {
        auto snapshot = create_snapshot_with_context();
        // Snapshot automatically includes thread context
        return make_success(std::move(snapshot));
    }
};
```

### Monitorable Interface

Components can expose their metrics using the monitorable interface:

```cpp
#include <monitoring/interfaces/monitorable_interface.h>

using namespace monitoring_system;

// Implement monitorable interface
class my_service : public monitorable_component {
public:
    my_service() : monitorable_component("my_service") {}
    
    result<monitoring_data> get_monitoring_data() const override {
        monitoring_data data(get_monitoring_id());
        
        // Add metrics
        data.add_metric("request_count", request_count_);
        data.add_metric("avg_latency_ms", avg_latency_);
        
        // Add tags
        data.add_tag("version", "1.0.0");
        data.add_tag("status", is_healthy() ? "healthy" : "degraded");
        
        return make_success(std::move(data));
    }
};

// Aggregate metrics from multiple components
monitoring_aggregator aggregator("system_aggregator");
aggregator.add_component(std::make_shared<my_service>());
aggregator.add_component(std::make_shared<database_service>());

auto result = aggregator.collect_all();
if (result) {
    auto data = result.value();
    // Process aggregated metrics
}
```

### Dependency Injection

The monitoring system provides a flexible dependency injection container:

```cpp
#include <monitoring/di/service_container_interface.h>
#include <monitoring/di/lightweight_container.h>

using namespace monitoring_system;

// Create a container
auto container = create_lightweight_container();

// Register services
container->register_factory<IMetricsCollector>(
    []() { return std::make_shared<DefaultMetricsCollector>(); },
    service_lifetime::singleton
);

// Resolve services
auto collector_result = container->resolve<IMetricsCollector>();
if (collector_result) {
    auto collector = collector_result.value();
    collector->collect();
}

// Named registrations
container->register_factory<IStorage>(
    "memory",
    []() { return std::make_shared<MemoryStorage>(); },
    service_lifetime::transient
);

auto storage = container->resolve<IStorage>("memory");
```

### Result Pattern

The monitoring system uses a Result pattern for error handling:

```cpp
#include <monitoring/core/result_types.h>
#include <monitoring/core/error_codes.h>

using namespace monitoring_system;

// Function that may fail
result<double> divide(double a, double b) {
    if (b == 0) {
        return make_error<double>(
            monitoring_error_code::invalid_configuration,
            "Division by zero"
        );
    }
    return make_success<double>(a / b);
}

// Usage
auto result = divide(10.0, 2.0);
if (result) {
    std::cout << "Result: " << result.value() << std::endl;
} else {
    std::cerr << "Error: " << result.get_error().message << std::endl;
}

// Monadic operations
auto processed = divide(100.0, 4.0)
    .map([](double x) { return x * 2; })
    .and_then([](double x) {
        return make_success<std::string>(std::to_string(x));
    });
```

### Metrics Collection

```cpp
#include <monitoring/interfaces/monitoring_interface.h>

// Create a metrics snapshot
metrics_snapshot snapshot;
snapshot.add_metric("cpu_usage", 65.5);
snapshot.add_metric("memory_usage", 4096.0);

// Retrieve metrics
if (auto cpu = snapshot.get_metric("cpu_usage")) {
    std::cout << "CPU: " << cpu.value() << "%" << std::endl;
}
```

### Configuration

```cpp
monitoring_config config;
config.history_size = 1000;
config.collection_interval = std::chrono::milliseconds(100);
config.buffer_size = 5000;

// Validate configuration
auto result = config.validate();
if (!result) {
    std::cerr << "Config error: " << result.get_error().message << std::endl;
}
```

## Project Structure

```
monitoring_system/
├── sources/
│   └── monitoring/
│       ├── core/              # Core types and error handling
│       │   ├── error_codes.h  # Error code definitions
│       │   └── result_types.h # Result pattern implementation
│       ├── interfaces/        # Abstract interfaces
│       │   ├── monitoring_interface.h
│       │   └── monitorable_interface.h
│       ├── context/           # Thread context
│       │   └── thread_context.h
│       ├── di/                # Dependency injection
│       │   ├── service_container_interface.h
│       │   ├── lightweight_container.h
│       │   └── thread_system_container_adapter.h
│       └── adapters/          # System adapters (upcoming)
├── tests/                     # Unit tests
├── examples/                  # Example programs
├── cmake/                     # CMake modules
└── docs/                      # Documentation (upcoming)
```

## Implementation Status

### Phase 1: Core Architecture Alignment (Week 1-2) ✅ COMPLETE
- [x] A1: Adopt thread_system's result<T> pattern
- [x] A2: Define monitoring_error_code enum (extended)
- [x] A3: Integrate with service_container
- [x] A4: Implement monitorable_interface
- [x] A5: Add thread_context metadata

### Phase 2: Design Patterns (Week 3-4)
- [ ] D1: Create monitoring_builder
- [ ] D2: Implement collector factory
- [ ] D3: Add storage strategy pattern
- [ ] D4: Observer pattern for alerts

### Phase 3: Performance (Week 5-6)
- [ ] P1: Lock-free queue integration
- [ ] P2: Zero-copy metrics
- [ ] P3: SIMD optimizations
- [ ] P4: Memory pool

## Testing

Run all tests:
```bash
cd build
ctest --output-on-failure
```

Run specific test:
```bash
./tests/monitoring_system_tests --gtest_filter=ResultTypesTest.*
./tests/monitoring_system_tests --gtest_filter=DIContainerTest.*
./tests/monitoring_system_tests --gtest_filter=MonitorableInterfaceTest.*
./tests/monitoring_system_tests --gtest_filter=ThreadContextTest.*
```

**Note**: 48 tests currently passing (2 tests temporarily disabled for further investigation)

## Contributing

See [MONITORING_SYSTEM_DESIGN.md](MONITORING_SYSTEM_DESIGN.md) for the complete design document and contribution guidelines.

## License

BSD 3-Clause License - See LICENSE file for details

## Related Projects

- [thread_system](https://github.com/kcenon/thread_system) - High-performance threading framework
- [logger_system](https://github.com/kcenon/logger_system) - Asynchronous logging system