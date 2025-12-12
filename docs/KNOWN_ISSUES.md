# Known Issues and Limitations

**Version**: 0.2.0.0
**Last Updated**: 2025-11-26

This document lists known issues, limitations, and STUB implementations in the monitoring system.

---

## STUB Implementations

The following components are marked as STUB and are **not fully implemented**:

| Component | File | Status | Ticket |
|-----------|------|--------|--------|
| ~~CircuitBreaker~~ | `reliability/circuit_breaker.h` | **Implemented** | MON-001 |
| ~~Jaeger Exporter (HTTP transport)~~ | `exporters/trace_exporters.h` | **Implemented** | MON-005 |
| ~~Zipkin Exporter (HTTP transport)~~ | `exporters/trace_exporters.h` | **Implemented** | MON-005 |
| OTLP Exporter (gRPC transport) | `exporters/trace_exporters.h` | STUB | MON-005 |

### ~~CircuitBreaker~~ (Resolved)

The `circuit_breaker` class has been fully implemented with:
- Complete state machine (closed → open → half_open → closed)
- Failure threshold tracking
- Reset timeout for half_open transitions
- Success threshold for closing from half_open
- Thread-safe operations with proper atomics and mutex
- Comprehensive metrics tracking

### ~~Trace Exporters~~ (Partially Resolved)

**Resolved (MON-005):**
- Jaeger and Zipkin exporters now support real HTTP transmission when built with `MONITORING_WITH_NETWORK_SYSTEM=ON`
- Uses `network_system::core::http_client` for actual network communication
- Supports Thrift JSON format for Jaeger and JSON v2 format for Zipkin

**Still STUB:**
- OTLP gRPC transport (requires gRPC library integration)
- Protobuf serialization for Jaeger and Zipkin (requires protobuf library)

**Usage:**
```cmake
# Enable real HTTP transport
cmake -DMONITORING_WITH_NETWORK_SYSTEM=ON ..
```

**Workaround for OTLP:** Use OpenTelemetry Collector as sidecar and export via Jaeger/Zipkin HTTP.

---

## Platform Support

| Platform | Metrics Collection | Status |
|----------|-------------------|--------|
| macOS | Full | Supported |
| Linux | Full | Supported |
| Windows | Full | Supported |

All platforms have complete implementations for:
- CPU usage (via platform-specific APIs)
- Memory usage
- Thread count

---

## Test Coverage

| Category | Active | Total | Notes |
|----------|--------|-------|-------|
| Unit Tests | 11 | 24 | 13 tests disabled |
| Thread Safety Tests | 1 | 1 | - |
| Integration Tests | - | - | Framework only |

### Active Tests

The following test files are enabled and passing:
- `test_result_types.cpp`
- `test_di_container.cpp`
- `test_monitorable_interface.cpp`
- `test_thread_context_simple.cpp`
- `test_lock_free_collector.cpp`
- `test_distributed_tracing.cpp`
- `test_performance_monitoring.cpp`
- `test_event_bus.cpp`
- `test_trace_exporters.cpp`
- `test_adapter_functionality.cpp`
- `test_adaptive_monitoring.cpp` (124 tests total)

### Disabled Tests

The following test files are commented out in `tests/CMakeLists.txt`:

- `test_health_monitoring.cpp`
- `test_metric_storage.cpp`
- `test_stream_aggregation.cpp`
- `test_buffering_strategies.cpp`
- `test_optimization.cpp`
- `test_fault_tolerance.cpp`
- `test_error_boundaries.cpp`
- `test_resource_management.cpp`
- `test_data_consistency.cpp`
- `test_opentelemetry_adapter.cpp`
- `test_metric_exporters.cpp`
- `test_storage_backends.cpp`
- `test_integration_e2e.cpp`

**Reason:** API alignment and missing implementations. See MON-002.

---

## CMake Configuration

### ~~MON-007: Option Naming~~ (Resolved)

All CMake options are now consistently prefixed with `MONITORING_`:
- `MONITORING_BUILD_TESTS`
- `MONITORING_BUILD_INTEGRATION_TESTS`
- `MONITORING_BUILD_EXAMPLES`
- `MONITORING_BUILD_BENCHMARKS`
- `MONITORING_WITH_COMMON_SYSTEM`
- `MONITORING_WITH_THREAD_SYSTEM`
- `MONITORING_WITH_LOGGER_SYSTEM`
- `MONITORING_ENABLE_ASAN`
- `MONITORING_ENABLE_TSAN`
- `MONITORING_ENABLE_UBSAN`
- `MONITORING_ENABLE_COVERAGE`

**Backward Compatibility:** Legacy option names (e.g., `BUILD_TESTS`) are still supported via `cmake/MonitoringLegacyOptions.cmake` but will emit deprecation warnings. This compatibility layer can be removed in a future major version

---

## API Stability

| Component | Stability |
|-----------|-----------|
| Core metrics types | Stable |
| Result types | Stable |
| Tracing interfaces | Stable |
| Exporter interfaces | Beta |
| CircuitBreaker | Stable |

---

## Reporting Issues

Please report issues at the project repository with:
1. Version information
2. Platform details
3. Minimal reproduction steps
4. Expected vs actual behavior
