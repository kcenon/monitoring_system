# Monitoring System

[![Ubuntu-GCC](https://github.com/kcenon/monitoring_system/actions/workflows/build-ubuntu-gcc.yaml/badge.svg)](https://github.com/kcenon/monitoring_system/actions/workflows/build-ubuntu-gcc.yaml)
[![Ubuntu-Clang](https://github.com/kcenon/monitoring_system/actions/workflows/build-ubuntu-clang.yaml/badge.svg)](https://github.com/kcenon/monitoring_system/actions/workflows/build-ubuntu-clang.yaml)
[![Windows-VS](https://github.com/kcenon/monitoring_system/actions/workflows/build-windows-vs.yaml/badge.svg)](https://github.com/kcenon/monitoring_system/actions/workflows/build-windows-vs.yaml)
[![Windows-MSYS2](https://github.com/kcenon/monitoring_system/actions/workflows/build-windows-msys2.yaml/badge.svg)](https://github.com/kcenon/monitoring_system/actions/workflows/build-windows-msys2.yaml)
[![Doxygen](https://github.com/kcenon/monitoring_system/actions/workflows/build-Doxygen.yaml/badge.svg)](https://github.com/kcenon/monitoring_system/actions/workflows/build-Doxygen.yaml)

[![License](https://img.shields.io/badge/License-BSD_3--Clause-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B20)

A comprehensive monitoring and observability platform for C++ applications featuring performance monitoring, distributed tracing, health checks, and reliability patterns. Built with modern C++20 and designed for production environments.

## ✨ Features

### 🎯 Core Capabilities
- **Performance Monitoring**: Real-time metrics collection and analysis
- **Distributed Tracing**: Request flow tracking across services
- **Health Monitoring**: Service health checks and dependency validation
- **Error Handling**: Robust result types and error boundary patterns
- **Dependency Injection**: Complete container with lifecycle management

### 🔧 Technical Highlights
- **Modern C++20**: Leverages latest language features (concepts, coroutines, std::format)
- **Cross-Platform**: Windows, Linux, and macOS support
- **Thread-Safe**: Concurrent operations with atomic counters and locks
- **Modular Design**: Plugin-based architecture with optional integrations
- **Production Ready**: 37 comprehensive tests with 100% pass rate

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                     Monitoring System                           │
├─────────────────────────────────────────────────────────────────┤
│ Core Components                                                 │
├─────────────────────┬───────────────────┬───────────────────────┤
│ Performance Monitor │ Distributed Tracer │ Health Monitor        │
│ • Metrics Collection│ • Span Management  │ • Service Checks      │
│ • Profiling Data    │ • Context Propagation│ • Dependency Tracking│
│ • Aggregation       │ • Trace Export     │ • Recovery Policies   │
├─────────────────────┼───────────────────┼───────────────────────┤
│ Storage Layer       │ Event System      │ Reliability Patterns  │
│ • Memory Backend    │ • Event Bus       │ • Circuit Breakers    │
│ • File Backend      │ • Async Processing│ • Retry Policies      │
│ • Time Series       │ • Error Events    │ • Error Boundaries    │
└─────────────────────┴───────────────────┴───────────────────────┘
```

### Component Status

| Component | Status | Description |
|-----------|---------|-------------|
| **Result Types** | ✅ Complete | Error handling and success/failure patterns |
| **DI Container** | ✅ Complete | Service registration and lifecycle management |
| **Thread Context** | ✅ Complete | Request context and metadata tracking |
| **Performance Monitor** | ⚠️ Foundation | Basic metrics collection (extensible) |
| **Distributed Tracing** | ⚠️ Foundation | Span creation and context (extensible) |
| **Health Monitoring** | ⚠️ Foundation | Health checks framework (extensible) |
| **Storage Backends** | ⚠️ Foundation | Memory and file storage (extensible) |

## 🚀 Quick Start

### Prerequisites
- **Compiler**: C++20 capable (GCC 11+, Clang 14+, MSVC 2019+)
- **Build System**: CMake 3.16+
- **Testing**: Google Test (automatically fetched)

### Building

```bash
# Clone the repository
git clone https://github.com/kcenon/monitoring_system.git
cd monitoring_system

# Configure and build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Run tests
./build/tests/monitoring_system_tests

# Run examples
./build/examples/basic_monitoring_example
./build/examples/distributed_tracing_example
./build/examples/health_reliability_example
```

### CMake Integration

```cmake
# Add as subdirectory
add_subdirectory(monitoring_system)
target_link_libraries(your_target PRIVATE monitoring_system)

# Or find package (if installed)
find_package(monitoring_system REQUIRED)
target_link_libraries(your_target PRIVATE monitoring_system::monitoring_system)
```

## 📖 Usage Examples

### Basic Performance Monitoring

```cpp
#include <kcenon/monitoring/core/performance_monitor.h>

// Create performance monitor
monitoring_system::performance_monitor monitor("my_service");

// Record operation timing
auto start = std::chrono::steady_clock::now();
// ... your operation ...
auto end = std::chrono::steady_clock::now();

auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
monitor.get_profiler().record_sample("operation_name", duration, true);

// Collect metrics
auto snapshot = monitor.collect();
if (snapshot) {
    std::cout << "CPU Usage: " << snapshot.value().get_metric("cpu_usage") << "%\n";
}
```

### Distributed Tracing

```cpp
#include <monitoring/tracing/distributed_tracer.h>

auto& tracer = monitoring_system::global_tracer();

// Start a trace
auto span_result = tracer.start_span("user_request", "web_service");
if (span_result) {
    auto span = span_result.value();
    span->set_tag("user.id", "12345");
    span->set_tag("endpoint", "/api/users");

    // Create child span for database operation
    auto db_span_result = tracer.start_child_span(span, "database_query");
    if (db_span_result) {
        auto db_span = db_span_result.value();
        db_span->set_tag("query.type", "SELECT");

        // ... database operation ...

        tracer.finish_span(db_span);
    }

    tracer.finish_span(span);
}
```

### Health Monitoring

```cpp
#include <monitoring/health/health_monitor.h>

monitoring_system::health_monitor health_monitor;

// Register health checks
health_monitor.register_check(
    std::make_unique<monitoring_system::functional_health_check>(
        "database_connection",
        monitoring_system::health_check_type::dependency,
        []() {
            // Check database connectivity
            bool connected = check_database_connection();
            return connected ?
                monitoring_system::health_check_result::healthy("Database connected") :
                monitoring_system::health_check_result::unhealthy("Database unreachable");
        }
    )
);

// Check overall health
auto health_result = health_monitor.check_health();
if (health_result.status == monitoring_system::health_status::healthy) {
    std::cout << "System is healthy\n";
}
```

### Error Handling with Result Types

```cpp
#include <kcenon/monitoring/core/result_types.h>

// Function that can fail
monitoring_system::result<std::string> fetch_user_data(int user_id) {
    if (user_id <= 0) {
        return monitoring_system::make_error<std::string>(
            monitoring_system::monitoring_error_code::invalid_argument,
            "Invalid user ID"
        );
    }

    // ... fetch logic ...
    return monitoring_system::make_success(std::string("user_data"));
}

// Usage with error handling
auto result = fetch_user_data(123);
if (result) {
    std::cout << "User data: " << result.value() << "\n";
} else {
    std::cout << "Error: " << result.get_error().message << "\n";
}

// Chain operations
auto processed = result
    .map([](const std::string& data) { return data + "_processed"; })
    .and_then([](const std::string& data) {
        return monitoring_system::make_success(data.length());
    });
```

## 🔧 Configuration

### CMake Options

```bash
# Build options
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTS=ON \
  -DBUILD_EXAMPLES=ON \
  -DBUILD_BENCHMARKS=OFF

# Integration options
cmake -B build \
  -DTHREAD_SYSTEM_INTEGRATION=ON \
  -DLOGGER_SYSTEM_INTEGRATION=ON
```

### Runtime Configuration

```cpp
// Configure monitoring
monitoring_system::monitoring_config config;
config.enable_performance_monitoring = true;
config.enable_distributed_tracing = true;
config.sampling_rate = 0.1; // 10% sampling
config.max_trace_duration = std::chrono::seconds(30);

// Apply configuration
auto monitor = monitoring_system::create_monitor(config);
```

## 🧪 Testing

```bash
# Run all tests
cmake --build build --target monitoring_system_tests
./build/tests/monitoring_system_tests

# Run specific test suites
./build/tests/monitoring_system_tests --gtest_filter="*DI*"
./build/tests/monitoring_system_tests --gtest_filter="*Performance*"

# Generate test coverage (requires gcov/lcov)
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON
cmake --build build
./build/tests/monitoring_system_tests
make coverage
```

**Current Test Coverage**: 37 tests, 100% pass rate
- Result types: 13 tests
- DI container: 9 tests
- Monitorable interface: 12 tests
- Thread context: 3 tests

## 📦 Integration

### Optional Dependencies

The monitoring system can integrate with complementary libraries:

- **[thread_system](https://github.com/kcenon/thread_system)**: Enhanced concurrent processing
- **[logger_system](https://github.com/kcenon/logger_system)**: Structured logging integration

### Ecosystem Integration

```cpp
// With thread_system integration
#ifdef THREAD_SYSTEM_INTEGRATION
#include <thread_system/thread_pool.h>
auto collector = monitoring_system::create_threaded_collector(thread_pool);
#endif

// With logger_system integration
#ifdef LOGGER_SYSTEM_INTEGRATION
#include <logger_system/logger.h>
monitoring_system::set_logger(logger_system::get_logger());
#endif
```

## 📚 Documentation

- **[API Reference](docs/API_REFERENCE.md)**: Complete API documentation
- **[Architecture Guide](docs/ARCHITECTURE_GUIDE.md)**: System design and patterns
- **[Examples](examples/)**: Comprehensive usage examples
- **[Changelog](docs/CHANGELOG.md)**: Version history and changes

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Make your changes following the coding standards
4. Add tests for new functionality
5. Ensure all tests pass (`cmake --build build && ./build/tests/monitoring_system_tests`)
6. Commit your changes (`git commit -m 'Add amazing feature'`)
7. Push to the branch (`git push origin feature/amazing-feature`)
8. Open a Pull Request

### Development Guidelines

- Follow C++ Core Guidelines
- Use modern C++20 features appropriately
- Maintain 100% test coverage for new features
- Document public APIs with Doxygen comments
- Ensure cross-platform compatibility (Windows/Linux/macOS)

## 📄 License

This project is licensed under the BSD 3-Clause License - see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- Built with modern C++20 standards
- Uses Google Test for comprehensive testing
- Designed for production monitoring environments
- Inspired by industry-standard observability practices