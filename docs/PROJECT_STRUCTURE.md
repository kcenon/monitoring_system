# Monitoring System - Project Structure

**Version**: 0.2.0
**Last Updated**: 2025-12-10

---

## Table of Contents

- [Overview](#overview)
- [Directory Structure](#directory-structure)
- [Core Modules](#core-modules)
- [Collector Modules](#collector-modules)
- [File Descriptions](#file-descriptions)
- [Build Artifacts](#build-artifacts)
- [Module Dependencies](#module-dependencies)

---

## Overview

The monitoring system follows a modular, interface-based architecture with clear separation of concerns. This document provides a comprehensive guide to the project structure and file organization.

---

## Directory Structure

```
monitoring_system/
├── include/kcenon/monitoring/   # Public headers (API surface)
│   ├── adapters/                # System integration adapters
│   │   ├── common_monitor_adapter.h
│   │   ├── common_system_adapter.h
│   │   ├── logger_system_adapter.h
│   │   ├── monitor_adapter.h
│   │   ├── performance_monitor_adapter.h
│   │   └── thread_system_adapter.h
│   ├── adaptive/                # Adaptive monitoring
│   │   └── adaptive_monitor.h
│   ├── collectors/              # Metric collectors (19 collectors)
│   │   ├── battery_collector.h       # Battery status monitoring
│   │   ├── container_collector.h     # Docker/container metrics
│   │   ├── context_switch_collector.h # Context switch stats
│   │   ├── fd_collector.h            # File descriptor monitoring
│   │   ├── gpu_collector.h           # GPU metrics
│   │   ├── inode_collector.h         # Inode usage monitoring
│   │   ├── interrupt_collector.h     # Interrupt statistics
│   │   ├── logger_system_collector.h # Logger system integration
│   │   ├── plugin_metric_collector.h # Plugin-based collectors
│   │   ├── power_collector.h         # Power consumption
│   │   ├── security_collector.h      # Security events
│   │   ├── smart_collector.h         # SMART disk health
│   │   ├── socket_buffer_collector.h # Socket buffer usage
│   │   ├── system_resource_collector.h # System resources
│   │   ├── tcp_state_collector.h     # TCP connection states
│   │   ├── temperature_collector.h   # Hardware temperature
│   │   ├── thread_system_collector.h # Thread system integration
│   │   ├── uptime_collector.h        # System uptime
│   │   └── vm_collector.h            # Virtualization metrics
│   ├── concepts/                # C++20 Concepts
│   │   └── monitoring_concepts.h     # Type constraints
│   ├── context/                 # Context management
│   │   └── thread_context.h          # Thread-local context
│   ├── core/                    # Core monitoring components
│   │   ├── central_collector.h       # Central metric collection
│   │   ├── error_codes.h             # Error code definitions
│   │   ├── event_bus.h               # Event publishing
│   │   ├── event_types.h             # Event type definitions
│   │   ├── performance_monitor.h     # Performance metrics
│   │   ├── performance_types.h       # Performance type definitions
│   │   ├── result_types.h            # Result<T> pattern
│   │   ├── safe_event_dispatcher.h   # Thread-safe events
│   │   └── thread_local_buffer.h     # Thread-local buffering
│   ├── di/                      # Dependency injection
│   │   └── service_registration.h
│   ├── exporters/               # Data exporters
│   │   ├── http_transport.h          # HTTP transport
│   │   ├── metric_exporters.h        # Metric export formats
│   │   ├── opentelemetry_adapter.h   # OpenTelemetry integration
│   │   └── trace_exporters.h         # Trace exporters
│   ├── health/                  # Health monitoring
│   │   └── health_monitor.h          # Health check framework
│   ├── interfaces/              # Abstract interfaces
│   │   ├── event_bus_interface.h     # Event bus abstraction
│   │   ├── metric_collector_interface.h # Collector abstraction
│   │   ├── metric_types_adapter.h    # Metric type adapters
│   │   ├── monitorable_interface.h   # Monitoring abstraction
│   │   ├── monitoring_core.h         # Core monitoring interface
│   │   └── observer_interface.h      # Observer pattern
│   ├── reliability/             # Reliability patterns
│   │   ├── circuit_breaker.h         # Circuit breaker pattern
│   │   ├── error_boundary.h          # Error isolation
│   │   ├── fault_tolerance_manager.h # Fault tolerance
│   │   └── retry_policy.h            # Retry mechanisms
│   ├── storage/                 # Storage backends
│   │   └── storage_backends.h        # Storage interface
│   ├── tracing/                 # Distributed tracing
│   │   ├── distributed_tracer.h      # Trace management
│   │   └── trace_context.h           # Context propagation
│   ├── utils/                   # Utility components
│   │   ├── metric_types.h            # Metric type definitions
│   │   ├── time_series.h             # Time series utilities
│   │   └── time_series_buffer.h      # Time series buffer
│   ├── compatibility.h          # Backward compatibility
│   └── forward.h                # Forward declarations
├── src/                         # Implementation files
│   ├── collectors/              # Collector implementations
│   │   ├── system_resource_collector.cpp
│   │   └── vm_collector.cpp
│   ├── context/                 # Context implementations
│   │   └── thread_context.cpp
│   ├── core/                    # Core implementations
│   │   ├── central_collector.cpp
│   │   ├── performance_monitor.cpp
│   │   └── thread_local_buffer.cpp
│   ├── impl/                    # Feature implementations
│   │   ├── adaptive_monitor.cpp
│   │   ├── battery_collector.cpp
│   │   ├── container_collector.cpp
│   │   ├── context_switch_collector.cpp
│   │   ├── inode_collector.cpp
│   │   ├── interrupt_collector.cpp
│   │   ├── socket_buffer_collector.cpp
│   │   ├── tcp_state_collector.cpp
│   │   ├── tracing/
│   │   │   └── distributed_tracer.cpp
│   │   └── uptime_collector.cpp
│   ├── platform/                # Platform-specific code
│   │   ├── cgroup_metrics.cpp        # cgroups support
│   │   ├── docker_metrics.cpp        # Docker integration
│   │   ├── fd_collector.cpp
│   │   ├── gpu_collector.cpp
│   │   ├── linux_*.cpp               # Linux implementations
│   │   ├── macos_*.cpp               # macOS implementations
│   │   ├── power_collector.cpp
│   │   ├── security_collector.cpp
│   │   ├── smart_metrics.cpp
│   │   ├── temperature_collector.cpp
│   │   └── windows_*.cpp             # Windows implementations
│   └── utils/                   # Utility implementations
├── tests/                       # Test suites (48 test files)
│   ├── test_adapter_functionality.cpp
│   ├── test_adaptive_monitoring.cpp
│   ├── test_battery_collector.cpp
│   ├── test_buffering_strategies.cpp
│   ├── test_container_collector.cpp
│   ├── test_context_switch_collector.cpp
│   ├── test_cross_system_integration.cpp
│   ├── test_data_consistency.cpp
│   ├── test_di_container.cpp
│   ├── test_distributed_tracing.cpp
│   ├── test_error_boundaries.cpp
│   ├── test_event_bus.cpp
│   ├── test_fault_tolerance.cpp
│   ├── test_fd_collector.cpp
│   ├── test_gpu_collector.cpp
│   ├── test_health_monitoring.cpp
│   ├── test_inode_collector.cpp
│   ├── test_integration_e2e.cpp
│   ├── test_interfaces_compile.cpp
│   ├── test_interrupt_collector.cpp
│   ├── test_lock_free_collector.cpp
│   ├── test_metric_exporters.cpp
│   ├── test_metric_storage.cpp
│   ├── test_monitorable_interface.cpp
│   ├── test_opentelemetry_adapter.cpp
│   ├── test_optimization.cpp
│   ├── test_performance_monitoring.cpp
│   ├── test_power_collector.cpp
│   ├── test_resource_management.cpp
│   ├── test_result_types.cpp
│   ├── test_security_collector.cpp
│   ├── test_service_registration.cpp
│   ├── test_smart_collector.cpp
│   ├── test_socket_buffer_collector.cpp
│   ├── test_storage_backends.cpp
│   ├── test_stream_aggregation.cpp
│   ├── test_stress_performance.cpp
│   ├── test_system_resource_collector.cpp
│   ├── test_tcp_state_collector.cpp
│   ├── test_temperature_collector.cpp
│   ├── test_thread_context.cpp
│   ├── test_thread_context_simple.cpp
│   ├── test_time_series_buffer.cpp
│   ├── test_timer_metrics.cpp
│   ├── test_trace_exporters.cpp
│   ├── test_uptime_collector.cpp
│   ├── test_vm_collector.cpp
│   └── thread_safety_tests.cpp
├── examples/                    # Example applications
│   ├── basic_monitoring_example.cpp
│   ├── bidirectional_di_example.cpp
│   ├── custom_metric_types_example.cpp
│   ├── distributed_tracing_example.cpp
│   ├── event_bus_example.cpp
│   ├── facade_adapter_poc.cpp
│   ├── health_reliability_example.cpp
│   ├── logger_di_integration_example.cpp
│   ├── monitor_factory_pattern_example.cpp
│   ├── plugin_collector_example.cpp
│   ├── result_pattern_example.cpp
│   ├── storage_example.cpp
│   └── CMakeLists.txt
├── benchmarks/                  # Performance benchmarks
│   ├── adaptive_monitor_bench.cpp
│   ├── collector_overhead_bench.cpp
│   ├── event_bus_bench.cpp
│   ├── main_bench.cpp
│   ├── metric_collection_bench.cpp
│   ├── BASELINE.md
│   ├── CMakeLists.txt
│   └── README.md
├── integration_tests/           # Integration tests
│   └── README.md
├── docs/                        # Documentation
│   ├── advanced/                # Advanced topics
│   ├── contributing/            # Contribution guidelines
│   ├── guides/                  # User guides
│   ├── integration/             # Integration guides
│   ├── performance/             # Performance documentation
│   ├── API_REFERENCE.md         # API documentation
│   ├── API_REFERENCE_KO.md      # API documentation (Korean)
│   ├── ARCHITECTURE.md          # Architecture overview
│   ├── ARCHITECTURE_KO.md
│   ├── BENCHMARKS.md            # Benchmark results
│   ├── BENCHMARKS_KO.md
│   ├── CHANGELOG.md             # Version history
│   ├── CHANGELOG_KO.md
│   ├── FEATURES.md              # Feature documentation
│   ├── FEATURES_KO.md
│   ├── KNOWN_ISSUES.md          # Known issues
│   ├── PRODUCTION_QUALITY.md    # Production quality metrics
│   ├── PRODUCTION_QUALITY_KO.md
│   ├── PROJECT_STRUCTURE.md     # This file
│   ├── PROJECT_STRUCTURE_KO.md
│   └── README.md                # Documentation index
├── cmake/                       # CMake modules
│   ├── monitoring_system-config.cmake.in
│   ├── MonitoringCompatibility.cmake
│   └── MonitoringLegacyOptions.cmake
├── .github/                     # GitHub configuration
│   ├── workflows/               # CI/CD workflows
│   └── ISSUE_TEMPLATE/          # Issue templates
├── CMakeLists.txt               # Root build configuration
├── vcpkg.json                   # Dependency manifest
├── .clang-format                # Code formatting rules
├── .clang-tidy                  # Static analysis rules
├── .gitignore                   # Git ignore patterns
├── LICENSE                      # BSD 3-Clause license
├── README.md                    # Main documentation
└── README_KO.md                 # Korean documentation
```

---

## Core Modules

### Core Module (`include/kcenon/monitoring/core/`)

**Purpose**: Fundamental monitoring capabilities and infrastructure

**Key Components**:

| File | Purpose | Key Classes/Functions | Dependencies |
|------|---------|----------------------|--------------|
| `performance_monitor.h` | Performance metrics collection | `performance_monitor`, `metrics_snapshot` | result_types.h |
| `result_types.h` | Error handling types | `result<T>`, `monitoring_error` | None |
| `central_collector.h` | Central metric collection | `central_collector` | interfaces |
| `thread_local_buffer.h` | Thread-local buffering | `thread_local_buffer` | None |
| `event_bus.h` | Event publishing system | `event_bus` | event_types.h |

### Interfaces Module (`include/kcenon/monitoring/interfaces/`)

**Purpose**: Abstract interfaces for extensibility and testability

**Key Interfaces**:

| File | Purpose | Key Methods | Implementations |
|------|---------|-------------|-----------------|
| `monitorable_interface.h` | Monitoring capability | `configure()`, `start()`, `stop()`, `collect_now()` | performance_monitor |
| `metric_collector_interface.h` | Collector abstraction | `collect()`, `get_name()`, `is_healthy()` | All collectors |
| `event_bus_interface.h` | Event publishing | `publish()`, `subscribe()` | event_bus |
| `observer_interface.h` | Observer pattern | `on_update()` | Various observers |

### Concepts Module (`include/kcenon/monitoring/concepts/`)

**Purpose**: C++20 Concepts for compile-time type validation

**Key Concepts**:

| Concept | Purpose | Requirements |
|---------|---------|--------------|
| `EventType` | Event type constraint | Class type, copy-constructible |
| `EventHandler` | Event handler constraint | Invocable with const E&, returns void |
| `EventFilter` | Event filter constraint | Invocable with const E&, returns bool |
| `Validatable` | Configuration validation | Has `validate()` method |
| `MetricSourceLike` | Metric source | Has `get_current_metrics()`, `get_source_name()`, `is_healthy()` |
| `MetricCollectorLike` | Metric collector | Has `collect_metrics()`, `is_collecting()`, `get_metric_types()` |

---

## Collector Modules

### System Collectors (`include/kcenon/monitoring/collectors/`)

**Purpose**: Platform-specific metric collection

| Collector | Purpose | Platform Support |
|-----------|---------|------------------|
| `battery_collector` | Battery status monitoring | Linux, macOS, Windows |
| `container_collector` | Docker/container metrics | Linux (cgroups v1/v2) |
| `context_switch_collector` | Context switch statistics | Linux, macOS |
| `fd_collector` | File descriptor monitoring | Linux, macOS |
| `gpu_collector` | GPU metrics (NVIDIA, AMD, Intel, Apple) | Linux, macOS |
| `inode_collector` | Inode usage monitoring | Linux, macOS |
| `interrupt_collector` | Interrupt statistics | Linux, macOS |
| `power_collector` | Power consumption (RAPL) | Linux, macOS |
| `security_collector` | Security event monitoring | Linux |
| `smart_collector` | SMART disk health | Linux, macOS (smartmontools) |
| `socket_buffer_collector` | Socket buffer usage | Linux, macOS |
| `system_resource_collector` | CPU, memory, disk, network | Linux, macOS, Windows |
| `tcp_state_collector` | TCP connection states | Linux, macOS |
| `temperature_collector` | Hardware temperature | Linux, macOS |
| `uptime_collector` | System uptime | Linux, macOS, Windows |
| `vm_collector` | Virtualization metrics | Linux, macOS |

---

## Build Artifacts

### Build Directory Structure

```
build/
├── lib/                         # Libraries
│   └── libmonitoring_system.a   # Static library
├── bin/                         # Executables
│   ├── basic_monitoring_example
│   ├── distributed_tracing_example
│   └── health_reliability_example
├── tests/                       # Test executables
│   └── monitoring_system_tests  # All tests
├── benchmarks/                  # Benchmark executables
│   └── monitoring_benchmarks
└── docs/                        # Generated documentation
    └── html/                    # Doxygen HTML output
```

### CMake Targets

| Target | Type | Output | Purpose |
|--------|------|--------|---------|
| `monitoring_system` | Library | `libmonitoring_system.a` | Main library |
| `monitoring_system_tests` | Executable | `monitoring_system_tests` | Unit tests |
| `monitoring_benchmarks` | Executable | `monitoring_benchmarks` | Performance tests |
| `*_example` | Executable | Example binaries | Example apps |

---

## Module Dependencies

### Internal Dependencies

```
┌─────────────────────────────────────────────────────────────┐
│                     monitoring_system                       │
└─────────────────────────────────────────────────────────────┘
                           │
           ┌───────────────┼───────────────┐
           │               │               │
           ▼               ▼               ▼
    ┌──────────┐    ┌──────────┐    ┌──────────┐
    │   core   │    │collectors│    │  health  │
    └────┬─────┘    └────┬─────┘    └────┬─────┘
         │               │               │
         │          ┌────┴────┐          │
         │          │         │          │
         ▼          ▼         ▼          ▼
    ┌────────────────────────────────────────┐
    │           interfaces/concepts          │
    └────────────────────────────────────────┘
                     │
                     ▼
            ┌─────────────────┐
            │   adapters      │
            └─────────────────┘
```

### External Dependencies

| Dependency | Version | Purpose | Required |
|------------|---------|---------|----------|
| **common_system** | Latest | Core interfaces (IMonitor, ILogger, Result<T>) | Yes |
| **thread_system** | Latest | Threading primitives | Yes |
| **logger_system** | Latest | Logging capabilities | No (optional) |
| **Google Test** | 1.12+ | Unit testing framework | No (test only) |
| **Google Benchmark** | 1.7+ | Performance benchmarking | No (benchmark only) |

---

## Test Organization

### Unit Tests (`tests/`)

| Category | Test Files | Purpose |
|----------|------------|---------|
| Core | `test_result_types`, `test_di_container`, `test_event_bus` | Core functionality |
| Collectors | `test_*_collector` (15 files) | Collector validation |
| Integration | `test_cross_system_integration`, `test_integration_e2e` | System integration |
| Reliability | `test_fault_tolerance`, `test_error_boundaries` | Reliability patterns |
| Performance | `test_stress_performance`, `test_optimization` | Performance validation |
| Thread Safety | `thread_safety_tests` | Concurrency testing |

**Total**: 48 test files

---

## See Also

- [Architecture Guide](ARCHITECTURE.md) - System design and patterns
- [API Reference](API_REFERENCE.md) - Complete API documentation
- [Features](FEATURES.md) - Detailed feature documentation
- [Benchmarks](BENCHMARKS.md) - Performance metrics
