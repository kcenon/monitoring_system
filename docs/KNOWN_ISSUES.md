# Known Issues and Limitations

**Version**: 2.0.0
**Last Updated**: 2025-11-24

This document lists known issues, limitations, and STUB implementations in the monitoring system.

---

## STUB Implementations

The following components are marked as STUB and are **not production-ready**:

| Component | File | Status | Ticket |
|-----------|------|--------|--------|
| CircuitBreaker | `reliability/circuit_breaker.h` | STUB | MON-001 |
| Jaeger Exporter (HTTP transport) | `exporters/trace_exporters.h` | STUB | MON-005 |
| Zipkin Exporter (HTTP transport) | `exporters/trace_exporters.h` | STUB | MON-005 |
| OTLP Exporter (HTTP transport) | `exporters/trace_exporters.h` | STUB | MON-005 |

### CircuitBreaker

The `circuit_breaker` class is marked with `[[deprecated("STUB implementation")]]`.

**Current Limitations:**
- `get_state()` always returns `closed`
- No actual state transitions (closed → open → half_open)
- Failure threshold logic not implemented
- Not suitable for production use

**Workaround:** Use external circuit breaker libraries until MON-001 is completed.

### Trace Exporters

Jaeger, Zipkin, and OTLP exporters have `send_*` methods that are stubs:

```cpp
result_void send_thrift_batch(const std::vector<jaeger_span_data>& spans) {
    (void)spans; // Suppress unused parameter warning
    return common::ok();  // STUB - no actual transmission
}
```

**Current Limitations:**
- No HTTP/gRPC network transmission
- Spans are converted but not actually sent
- Configuration validation works but export is no-op

**Workaround:** Use OpenTelemetry Collector as sidecar and export via stdout/file.

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
| Unit Tests | 5 | 23 | 18 tests disabled |
| Thread Safety Tests | 1 | 1 | - |
| Integration Tests | - | - | Framework only |

### Disabled Tests

The following test files are commented out in `tests/CMakeLists.txt`:

- `test_distributed_tracing.cpp`
- `test_performance_monitoring.cpp`
- `test_adaptive_monitoring.cpp`
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
- `test_trace_exporters.cpp`
- `test_metric_exporters.cpp`
- `test_storage_backends.cpp`
- `test_integration_e2e.cpp`
- `test_stress_performance.cpp`

**Reason:** API alignment and missing implementations. See MON-002.

---

## CMake Configuration

### Known Issues

1. **Inconsistent Option Naming**
   - Mix of `BUILD_*`, `MONITORING_BUILD_*`, and `MONITORING_USE_*` prefixes
   - See MON-007 for cleanup plan

2. **Backward Compatibility Code**
   - Lines 34-65 in CMakeLists.txt contain legacy option handling
   - May cause confusion during configuration

---

## API Stability

| Component | Stability |
|-----------|-----------|
| Core metrics types | Stable |
| Result types | Stable |
| Tracing interfaces | Stable |
| Exporter interfaces | Beta |
| CircuitBreaker | Unstable (STUB) |

---

## Reporting Issues

Please report issues at the project repository with:
1. Version information
2. Platform details
3. Minimal reproduction steps
4. Expected vs actual behavior
