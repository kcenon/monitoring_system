[![CI](https://github.com/kcenon/monitoring_system/actions/workflows/ci.yml/badge.svg)](https://github.com/kcenon/monitoring_system/actions/workflows/ci.yml)
[![Code Coverage](https://github.com/kcenon/monitoring_system/actions/workflows/coverage.yml/badge.svg)](https://github.com/kcenon/monitoring_system/actions/workflows/coverage.yml)
[![Static Analysis](https://github.com/kcenon/monitoring_system/actions/workflows/static-analysis.yml/badge.svg)](https://github.com/kcenon/monitoring_system/actions/workflows/static-analysis.yml)
[![Documentation](https://github.com/kcenon/monitoring_system/actions/workflows/build-Doxygen.yaml/badge.svg)](https://github.com/kcenon/monitoring_system/actions/workflows/build-Doxygen.yaml)

# Monitoring System Project

> **Language:** **English** | [한국어](README_KO.md)

## Overview

Modern C++20 observability platform with comprehensive monitoring, distributed tracing, and reliability capabilities for high-performance applications. Built with a modular, interface-based architecture for seamless ecosystem integration.

**Key Value Proposition**:
- **Performance Excellence**: 10M+ metric operations/sec, <50ns context propagation
- **Reliable Design**: Thread-safe design, comprehensive error handling, circuit breakers
- **Developer Productivity**: Intuitive API, rich telemetry, modular components
- **Enterprise-Ready**: Distributed tracing, health monitoring, reliability patterns

**Latest Status**: ✅ All CI/CD pipelines green, 37/37 tests passing (100% pass rate)

---

## Requirements

| Dependency | Version | Required | Description |
|------------|---------|----------|-------------|
| C++20 Compiler | GCC 11+ / Clang 14+ / MSVC 2022+ / Apple Clang 14+ | Yes | C++20 features required |
| CMake | 3.20+ | Yes | Build system |
| [common_system](https://github.com/kcenon/common_system) | latest | Yes | Common interfaces (IMonitor, Result<T>) |
| [thread_system](https://github.com/kcenon/thread_system) | latest | Yes | Thread pool and async operations |
| [logger_system](https://github.com/kcenon/logger_system) | latest | Optional | Logging capabilities |

### Dependency Flow

```
monitoring_system
├── common_system (required)
├── thread_system (required)
│   └── common_system
└── logger_system (optional)
    └── common_system
```

### Building with Dependencies

```bash
# Clone all dependencies
git clone https://github.com/kcenon/common_system.git
git clone https://github.com/kcenon/thread_system.git
git clone https://github.com/kcenon/logger_system.git
git clone https://github.com/kcenon/monitoring_system.git

# Build monitoring_system
cd monitoring_system
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

📖 **[Quick Start Guide →](docs/guides/QUICK_START.md)** | **[빠른 시작 가이드 →](docs/guides/QUICK_START_KO.md)**

---

## 🔗 Ecosystem Integration

Part of a modular C++ ecosystem with clean interface boundaries:

**Dependencies**:
- **[common_system](https://github.com/kcenon/common_system)**: Core interfaces (IMonitor, ILogger, Result<T>)
- **[thread_system](https://github.com/kcenon/thread_system)**: Threading primitives (required)
- **[logger_system](https://github.com/kcenon/logger_system)**: Logging capabilities (optional)

**Integration Pattern**:
```
common_system (interfaces) ← monitoring_system implements IMonitor
                          ↖ optional: inject ILogger at runtime
```

**Benefits**: Interface-only dependencies, independent compilation, runtime DI, clean separation

📖 [Complete Ecosystem Integration Guide →](../ECOSYSTEM_INTEGRATION.md)

---

## Quick Start

### Installation

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
```

### Basic Usage

```cpp
#include <kcenon/monitoring/core/performance_monitor.h>
#include <kcenon/monitoring/tracing/distributed_tracer.h>
#include <kcenon/monitoring/health/health_monitor.h>

using namespace monitoring_system;

int main() {
    // 1. Create monitoring components
    performance_monitor monitor("my_service");
    auto& tracer = global_tracer();
    health_monitor health;

    // 2. Enable metrics collection
    monitor.enable_collection(true);

    // 3. Start distributed trace
    auto span_result = tracer.start_span("main_operation", "service");
    if (!span_result) {
        std::cerr << "Failed to start trace: " << span_result.error().message << "\n";
        return -1;
    }
    auto span = span_result.value();
    span->set_tag("operation.type", "batch_processing");

    // 4. Monitor operations
    auto timer = monitor.start_timer("processing");
    for (int i = 0; i < 1000; ++i) {
        monitor.increment_counter("items_processed");
        // ... your processing logic ...
    }

    // 5. Collect metrics
    auto snapshot = monitor.collect();
    if (snapshot) {
        std::cout << "CPU: " << snapshot.value().get_metric("cpu_usage") << "%\n";
        std::cout << "Processed: " << snapshot.value().get_metric("items_processed") << "\n";
    }

    // 6. Finish trace
    tracer.finish_span(span);
    tracer.export_traces();

    return 0;
}
```

📖 [Complete User Guide →](docs/guides/USER_GUIDE.md)

---

## Core Features

- **Performance Monitoring**: Real-time metrics (counters, gauges, histograms) - 10M+ ops/sec
- **Distributed Tracing**: Request flow tracking across services - 2.5M spans/sec
- **Health Monitoring**: Service health checks and dependency validation - 500K checks/sec
- **Error Handling**: Robust Result<T> pattern for type-safe error management
- **Dependency Injection**: Complete DI container with lifecycle management
- **Circuit Breakers**: Automatic failure detection and recovery
- **Storage Backends**: Memory, file, and time-series storage options
- **Thread-Safe**: Concurrent operations with atomic counters and locks

📚 [Detailed Features →](docs/FEATURES.md)

---

## Performance Highlights

*Benchmarked on Apple M1 (8-core) @ 3.2GHz, 16GB RAM, macOS Sonoma*

| Operation | Throughput | Latency (P95) | Memory |
|-----------|------------|---------------|--------|
| **Counter Operations** | 10.5M ops/sec | 120 ns | <1MB |
| **Span Creation** | 2.5M spans/sec | 580 ns | 384 bytes/span |
| **Health Checks** | 520K checks/sec | 2.85 μs | <3MB |
| **Context Propagation** | 15M ops/sec | <50 ns | Thread-local |

**Platform**: Apple M1 @ 3.2GHz

### Industry Comparison

| Solution | Counter Ops/sec | Memory | Features |
|----------|-----------------|--------|----------|
| **Monitoring System** | 10.5M | <5MB | Full observability |
| Prometheus Client | 2.5M | 15MB | Metrics only |
| OpenTelemetry | 1.8M | 25MB | Complex API |

⚡ [Full Benchmarks →](docs/BENCHMARKS.md)

---

## Collector Factory

The monitoring system includes a unified **MetricFactory** for standardized collector creation and configuration:

```cpp
#include <kcenon/monitoring/factory/builtin_collectors.h>

// Register all built-in collectors (call once at startup)
kcenon::monitoring::register_builtin_collectors();

// Create collectors via factory
auto& factory = kcenon::monitoring::metric_factory::instance();
auto result = factory.create("system_resource_collector", {{"enabled", "true"}});

if (result) {
    auto& collector = result.collector;
    // Use collector...
}

// Register custom collectors
factory.register_collector("my_collector", []() {
    return std::make_unique<my_custom_collector>();
});
```

**Features**:
- Type-safe configuration parsing via `config_parser`:
  - Basic types: `get<bool>`, `get<int>`, `get<double>`, `get<std::string>`
  - Validation: `get_clamped`, `get_enum`, `get_matching`, `get_validated`
  - Advanced: `get_duration` (with ms/s/m/h suffixes), `get_list` (comma-separated)
- Thread-safe singleton factory pattern
- Support for plugin, CRTP, and standalone collector types
- Consistent error handling and initialization

📖 [Factory API Reference →](docs/02-API_REFERENCE.md#metric-factory)

---

## Architecture Overview

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
└─────────────────────┴───────────────────┴───────────────────────┘
```

**Key Characteristics**:
- **Interface-Driven Design**: Clean separation via abstract interfaces
- **Modular Components**: Pluggable storage, tracers, and health checkers
- **Zero Circular Dependencies**: Interface-only dependencies via common_system
- **Production Grade**: 100% test pass rate, <10% overhead

🏗️ [Architecture Guide →](docs/01-ARCHITECTURE.md)

---

## Documentation

### Getting Started
- 📖 [User Guide](docs/guides/USER_GUIDE.md) - Comprehensive usage guide
- 🚀 [Quick Start Examples](examples/) - Working code examples
- 🔧 [Integration Guide](docs/guides/INTEGRATION.md) - Ecosystem integration

### Core Documentation
- 📘 [API Reference](docs/02-API_REFERENCE.md) - Complete API documentation
- 📚 [Features](docs/FEATURES.md) - Detailed feature descriptions
- ⚡ [Benchmarks](docs/BENCHMARKS.md) - Performance metrics and comparisons
- 🏗️ [Architecture](docs/01-ARCHITECTURE.md) - System design and patterns
- 📦 [Project Structure](docs/PROJECT_STRUCTURE.md) - File organization

### Advanced Topics
- ✅ [Best Practices](docs/guides/BEST_PRACTICES.md) - Usage recommendations
- 🔍 [Troubleshooting](docs/guides/TROUBLESHOOTING.md) - Common issues and solutions
- 📋 [FAQ](docs/guides/FAQ.md) - Frequently asked questions
- 🔄 [Migration Guide](docs/guides/MIGRATION_GUIDE.md) - Version migration

### Development
- 🤝 [Contributing](docs/contributing/CONTRIBUTING.md) - Contribution guidelines
- 🏆 [Production Quality](docs/PRODUCTION_QUALITY.md) - CI/CD and quality metrics
- 📊 [Performance Baselines](docs/performance/BASELINE.md) - Regression thresholds

---

## CMake Integration

### Add as Subdirectory

```cmake
# Add monitoring system
add_subdirectory(monitoring_system)
target_link_libraries(your_target PRIVATE monitoring_system)

# Optional: Add ecosystem integration
add_subdirectory(thread_system)
add_subdirectory(logger_system)

target_link_libraries(your_target PRIVATE
    monitoring_system
    thread_system::interfaces
    logger_system
)
```

### Using FetchContent

```cmake
include(FetchContent)

FetchContent_Declare(
    monitoring_system
    GIT_REPOSITORY https://github.com/kcenon/monitoring_system.git
    GIT_TAG main
)

FetchContent_MakeAvailable(monitoring_system)

target_link_libraries(your_target PRIVATE monitoring_system)
```

### Build Options

```bash
# Build with tests and examples
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTS=ON \
  -DBUILD_EXAMPLES=ON \
  -DBUILD_BENCHMARKS=OFF

# Enable ecosystem integration
cmake -B build \
  -DBUILD_WITH_COMMON_SYSTEM=ON \
  -DTHREAD_SYSTEM_INTEGRATION=ON \
  -DLOGGER_SYSTEM_INTEGRATION=ON
```

---

## Configuration

### Runtime Configuration

```cpp
// Configure monitoring
monitoring_config config;
config.enable_performance_monitoring = true;
config.enable_distributed_tracing = true;
config.sampling_rate = 0.1;  // 10% sampling
config.max_trace_duration = std::chrono::seconds(30);

// Configure storage
auto storage = std::make_unique<memory_storage>(memory_storage_config{
    .max_entries = 10000,
    .retention_period = std::chrono::hours(1)
});

// Create monitor with config
auto monitor = create_monitor(config, std::move(storage));
```

---

## Testing

```bash
# Run all tests
cmake --build build --target monitoring_system_tests
./build/tests/monitoring_system_tests

# Run specific test suites
./build/tests/monitoring_system_tests --gtest_filter="*DI*"
./build/tests/monitoring_system_tests --gtest_filter="*Performance*"

# Generate coverage report
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON
cmake --build build
./build/tests/monitoring_system_tests
make coverage
```

**Test Status**: ✅ 37/37 tests passing (100% pass rate)

**Test Coverage**:
- Line Coverage: 87.3%
- Function Coverage: 92.1%
- Branch Coverage: 78.5%

---

## Production Quality

### Quality Grades

| Aspect | Grade | Status |
|--------|-------|--------|
| **Thread Safety** | A- | ✅ TSan clean, 0 data races |
| **Resource Management** | A | ✅ ASan clean, 0 leaks |
| **Error Handling** | A- | ✅ Result<T> pattern, 95% complete |
| **Test Coverage** | A | ✅ 37/37 tests, 100% pass rate |
| **CI/CD** | A | ✅ Multi-platform, all green |

### CI/CD Validation

**Platforms Tested**:
- Linux (Ubuntu 22.04): GCC 11+, Clang 14+
- macOS (macOS 12+): Apple Clang 14+
- Windows (Server 2022): MSVC 2022+

**Sanitizers**:
- ✅ AddressSanitizer: 0 leaks, 0 errors
- ✅ ThreadSanitizer: 0 data races
- ✅ UndefinedBehaviorSanitizer: 0 issues

**Static Analysis**:
- ✅ clang-tidy: 0 warnings
- ✅ cppcheck: 0 warnings
- ✅ cpplint: 0 issues

🏆 [Production Quality Metrics →](docs/PRODUCTION_QUALITY.md)

---

## Real-World Use Cases

**Ideal Applications**:
- **Microservices**: Distributed tracing and service health monitoring
- **High-Frequency Trading**: Ultra-low latency performance monitoring
- **Real-Time Systems**: Continuous health checks and circuit breaker protection
- **Web Applications**: Request tracing and bottleneck identification
- **IoT Platforms**: Resource usage monitoring and reliability patterns

---

## Ecosystem Integration

This monitoring system integrates seamlessly with other KCENON systems:

```cpp
// With thread_system integration
#include <thread_system/thread_pool.h>
auto collector = create_threaded_collector(thread_pool);

// With logger_system integration
#include <logger_system/logger.h>
monitoring_system::set_logger(logger_system::get_logger());
```

🌐 [Ecosystem Integration Guide →](../ECOSYSTEM_INTEGRATION.md)

---

## Support & Contributing

### Get Help
- 💬 [GitHub Discussions](https://github.com/kcenon/monitoring_system/discussions) - Ask questions
- 🐛 [Issue Tracker](https://github.com/kcenon/monitoring_system/issues) - Report bugs
- 📧 Email: kcenon@naver.com

### Contribute
We welcome contributions! See our [Contributing Guide](docs/contributing/CONTRIBUTING.md) for details.

**Quick Start**:
1. Fork the repository
2. Create your feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add some amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

---

## License

This project is licensed under the BSD 3-Clause License - see the [LICENSE](LICENSE) file for details.

---

## Acknowledgments

- Inspired by modern observability platforms and best practices
- Built with C++20 features (GCC 11+, Clang 14+, MSVC 2022+) for maximum performance and safety
- Maintained by kcenon@naver.com

---

<p align="center">
  Made with ❤️ by 🍀☀🌕🌥 🌊
</p>
