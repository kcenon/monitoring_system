# Monitoring System

A modern, high-performance monitoring system for C++ applications, integrating seamlessly with thread_system and logger_system.

## Features

### Core Architecture (✅ Phase 1 Complete)
- ✅ **Result Pattern Error Handling**: Explicit error handling without exceptions
- ✅ **Comprehensive Error Codes**: Categorized error codes for all operations
- ✅ **Dependency Injection**: Service container with lifetime management
- ✅ **Monitorable Interface**: Standardized interface for component monitoring
- ✅ **Thread Context Integration**: Metadata enrichment and context propagation

### Advanced Monitoring (✅ Phase 2 Complete)
- ✅ **Distributed Tracing**: W3C Trace Context compliant distributed tracing
  - Hierarchical span management
  - Baggage propagation for cross-service context
  - Thread-local span storage
  - RAII-based automatic span lifecycle
- ✅ **Performance Monitoring**: Comprehensive performance profiling
  - Nanosecond-precision timing
  - System resource monitoring (CPU, memory, threads)
  - Percentile-based metrics (P50, P95, P99)
  - Performance benchmarking utilities
  - Threshold-based alerting
- ✅ **Adaptive Monitoring**: Dynamic resource-aware monitoring
  - Load-based sampling rate adjustment
  - System resource consideration
  - Priority-based collector management
  - Multiple adaptation strategies
- ✅ **Health Monitoring**: Service health and dependency tracking
  - Liveness, readiness, and startup checks
  - Dependency graph with cycle detection
  - Composite health checks
  - Automatic recovery mechanisms
  - Health status reporting

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

## Usage Examples

### Distributed Tracing

```cpp
#include <monitoring/tracing/distributed_tracer.h>

using namespace monitoring_system;

// Start a root span
auto& tracer = global_tracer();
auto span = tracer.start_span("handle_request", "web_service");

// Add metadata
span->tags["http.method"] = "GET";
span->tags["http.url"] = "/api/users";
span->baggage["user_id"] = "12345";

// Create child spans
auto db_span = tracer.start_child_span(*span, "database_query");
// ... perform database operation
tracer.finish_span(db_span);

// Use RAII for automatic span management
{
    TRACE_SPAN("process_data");
    // Span automatically finished when scope exits
}

// Propagate context across services
std::unordered_map<std::string, std::string> headers;
auto context = tracer.extract_context(*span);
tracer.inject_context(context, headers);
// Send headers with HTTP request
```

### Performance Monitoring

```cpp
#include <monitoring/performance/performance_monitor.h>

using namespace monitoring_system;

// Time critical operations
{
    PERF_TIMER("database_query");
    // Perform database query
    // Timer automatically records duration
}

// Manual profiling
performance_profiler profiler;
auto start = std::chrono::high_resolution_clock::now();
// ... perform operation
auto end = std::chrono::high_resolution_clock::now();
profiler.record_sample("operation", end - start, true);

// Get performance metrics
auto metrics = profiler.get_metrics("operation");
std::cout << "P95 latency: " << metrics.p95_duration.count() / 1e6 << "ms\n";
std::cout << "Throughput: " << metrics.throughput << " ops/sec\n";

// Monitor system resources
system_monitor sys_mon;
sys_mon.start_monitoring();
auto sys_metrics = sys_mon.get_current_metrics();
if (sys_metrics.value().cpu_usage_percent > 80.0) {
    // Handle high CPU usage
}

// Benchmark code
performance_benchmark bench("algorithm_comparison");
auto [v1_metrics, v2_metrics] = bench.compare(
    "version_1", []() { /* algorithm v1 */ },
    "version_2", []() { /* algorithm v2 */ }
);
```

### Health Monitoring

```cpp
#include <monitoring/health/health_monitor.h>

using namespace monitoring_system;

// Create health checks
auto db_check = health_check_builder()
    .with_name("database")
    .with_type(health_check_type::liveness)
    .with_check([]() {
        // Check database connection
        if (can_connect_to_db()) {
            return health_check_result::healthy("Connected");
        }
        return health_check_result::unhealthy("Connection failed");
    })
    .with_timeout(std::chrono::seconds(5))
    .critical(true)
    .build();

// Register with global monitor
health_monitor& monitor = global_health_monitor();
monitor.register_check("database", db_check);
monitor.register_check("cache", cache_check);
monitor.register_check("api", api_check);

// Set up dependencies
monitor.add_dependency("api", "database");  // API depends on database
monitor.add_dependency("api", "cache");     // API depends on cache

// Start monitoring
monitor.start();

// Check specific service (including dependencies)
auto result = monitor.check("api");
if (result && !result.value().is_operational()) {
    // Handle degraded/unhealthy state
    std::cerr << "API health check failed: " << result.value().message << "\n";
}

// Get overall system health
auto status = monitor.get_overall_status();
if (status == health_status::unhealthy) {
    // Trigger alerts
}

// Get detailed health report
std::string report = monitor.get_health_report();
// Returns formatted status of all services
```

### Adaptive Monitoring

```cpp
#include <monitoring/adaptive/adaptive_monitor.h>

using namespace monitoring_system;

// Configure adaptive behavior
adaptive_config config;
config.strategy = adaptation_strategy::balanced;
config.idle_threshold = 20.0;      // CPU %
config.high_threshold = 80.0;      // CPU %
config.idle_interval = std::chrono::milliseconds(100);
config.critical_interval = std::chrono::milliseconds(5000);

// Register collectors with adaptive monitoring
adaptive_monitor& monitor = global_adaptive_monitor();
monitor.register_collector("critical_metrics", critical_collector, config);
monitor.register_collector("optional_metrics", optional_collector, config);

// Set collector priorities (higher = keep active longer)
monitor.set_collector_priority("critical_metrics", 100);
monitor.set_collector_priority("optional_metrics", 10);

// Start adaptive monitoring
monitor.start();
// Monitor automatically adjusts collection based on system load

// Check adaptation statistics
auto stats = monitor.get_collector_stats("critical_metrics");
std::cout << "Load level: " << static_cast<int>(stats.value().current_load_level) << "\n";
std::cout << "Sampling rate: " << stats.value().current_sampling_rate << "\n";
std::cout << "Samples dropped: " << stats.value().samples_dropped << "\n";

// Use adaptive scope for automatic management
{
    adaptive_scope scope("temp_collector", temp_collector);
    // Collector automatically registered and unregistered
}
```

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
│       ├── tracing/          # Distributed tracing
│       │   └── distributed_tracer.h
│       ├── performance/      # Performance monitoring
│       │   └── performance_monitor.h
│       ├── adaptive/         # Adaptive monitoring
│       │   └── adaptive_monitor.h
│       ├── health/           # Health monitoring
│       │   └── health_monitor.h
│       └── adapters/          # System adapters (upcoming)
├── tests/                     # Unit tests
├── examples/                  # Example programs
├── cmake/                     # CMake modules
└── docs/                      # Documentation (upcoming)
```


## Testing

The monitoring system includes comprehensive test coverage:

```bash
# Run all tests
./tests/monitoring_system_tests

# Run specific test suites
./tests/monitoring_system_tests --gtest_filter=DistributedTracingTest.*
./tests/monitoring_system_tests --gtest_filter=PerformanceMonitoringTest.*
```

### Test Coverage
- **Core Components**: 48 tests passing
- **Distributed Tracing**: 15 tests covering span management, context propagation, and W3C compliance
- **Performance Monitoring**: 19 tests covering profiling, system metrics, and benchmarking
- **Adaptive Monitoring**: 17 tests covering load-based adaptation and sampling
- **Health Monitoring**: 22 tests covering health checks, dependencies, and recovery
- **Total**: 121 tests ensuring reliability and correctness

## Project Status

### Completed Features
- ✅ **Phase 1**: Core Architecture (100% complete)
  - Result types and error handling
  - Dependency injection container
  - Monitorable interface
  - Thread context management
  
- ✅ **Phase 2**: Advanced Monitoring (100% complete)
  - Distributed tracing with W3C Trace Context
  - Performance monitoring and profiling
  - Adaptive monitoring based on system load
  - Health monitoring framework with dependency tracking
  
### Roadmap

#### Phase 3: Performance & Optimization
- [ ] Memory-efficient metric storage
- [ ] Statistical aggregation functions
- [ ] Configurable buffering strategies
- [ ] Lock-free data structures

#### Phase 4: Integration & Export
- [ ] OpenTelemetry compatibility layer
- [ ] Span exporters (Jaeger, Zipkin, OTLP)
- [ ] Prometheus metrics exporter
- [ ] Real-time alerting system
- [ ] Monitoring dashboard integration

#### Phase 5: Advanced Features
- [ ] Distributed metrics aggregation
- [ ] Trace sampling strategies
- [ ] Anomaly detection
- [ ] Predictive monitoring

## Contributing

See [MONITORING_SYSTEM_DESIGN.md](MONITORING_SYSTEM_DESIGN.md) for the complete design document and contribution guidelines.

## License

BSD 3-Clause License - See LICENSE file for details

## Related Projects

- [thread_system](https://github.com/kcenon/thread_system) - High-performance threading framework
- [logger_system](https://github.com/kcenon/logger_system) - Asynchronous logging system