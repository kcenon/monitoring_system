---
doc_id: "MON-QUAL-005"
doc_title: "Feature-Test-Module Traceability Matrix"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "monitoring_system"
category: "QUAL"
---

# Traceability Matrix

> **SSOT**: This document is the single source of truth for **Monitoring System Feature-Test-Module Traceability**.

## Feature -> Test -> Module Mapping

### Core Monitoring

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| MON-FEAT-001 | Performance Monitoring | tests/core/test_performance_monitoring.cpp | include/kcenon/monitoring/core/, src/core/ | Covered |
| MON-FEAT-002 | Metric Factory | tests/plugins/test_metric_factory.cpp | include/kcenon/monitoring/factory/ | Covered |
| MON-FEAT-003 | Metrics Provider (Platform) | tests/platform/test_metrics_provider.cpp | include/kcenon/monitoring/platform/, src/platform/ | Covered |
| MON-FEAT-004 | Result Types | tests/core/test_result_types.cpp | include/kcenon/monitoring/core/ | Covered |
| MON-FEAT-005 | Event Bus | tests/core/test_event_bus.cpp | include/kcenon/monitoring/core/ | Covered |
| MON-FEAT-006 | Interfaces (Compile Check) | tests/core/test_interfaces_compile.cpp | include/kcenon/monitoring/interfaces/ | Covered |
| MON-FEAT-007 | Monitorable Interface | tests/core/test_monitorable_interface.cpp | include/kcenon/monitoring/interfaces/ | Covered |

### Collectors

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| MON-FEAT-008 | System Resource Collector | tests/collectors/test_system_resource_collector.cpp | include/kcenon/monitoring/collectors/, src/collectors/ | Covered |
| MON-FEAT-009 | Process Metrics Collector | tests/collectors/test_process_metrics_collector.cpp | include/kcenon/monitoring/collectors/ | Covered |
| MON-FEAT-010 | Platform Metrics Collector | tests/collectors/test_platform_metrics_collector.cpp | include/kcenon/monitoring/collectors/ | Covered |
| MON-FEAT-011 | Network Metrics Collector | tests/collectors/test_network_metrics_collector.cpp | include/kcenon/monitoring/collectors/ | Covered |
| MON-FEAT-012 | Container Collector | tests/collectors/test_container_collector.cpp | include/kcenon/monitoring/collectors/ | Covered |
| MON-FEAT-013 | Battery Collector | tests/collectors/test_battery_collector.cpp | include/kcenon/monitoring/collectors/ | Covered |
| MON-FEAT-014 | Temperature Collector | tests/collectors/test_temperature_collector.cpp | include/kcenon/monitoring/collectors/ | Covered |
| MON-FEAT-015 | GPU Collector | tests/collectors/test_gpu_collector.cpp | include/kcenon/monitoring/collectors/ | Covered |
| MON-FEAT-016 | Uptime Collector | tests/collectors/test_uptime_collector.cpp | include/kcenon/monitoring/collectors/ | Covered |
| MON-FEAT-017 | Power Collector | tests/collectors/test_power_collector.cpp | include/kcenon/monitoring/collectors/ | Covered |
| MON-FEAT-018 | Interrupt Collector | tests/collectors/test_interrupt_collector.cpp | include/kcenon/monitoring/collectors/ | Covered |
| MON-FEAT-019 | Security Collector | tests/collectors/test_security_collector.cpp | include/kcenon/monitoring/collectors/ | Covered |
| MON-FEAT-020 | VM Collector | tests/collectors/test_vm_collector.cpp | include/kcenon/monitoring/collectors/ | Covered |
| MON-FEAT-021 | Smart Collector | tests/collectors/test_smart_collector.cpp | include/kcenon/monitoring/collectors/ | Covered |
| MON-FEAT-022 | Lock-Free Collector | tests/core/test_lock_free_collector.cpp | include/kcenon/monitoring/optimization/ | Covered |
| MON-FEAT-023 | Collector Registry | tests/plugins/test_collector_registry.cpp, tests/plugins/test_collector_registry_integration.cpp | include/kcenon/monitoring/factory/ | Covered |

### Plugins

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| MON-FEAT-024 | Container Plugin | tests/plugins/test_container_plugin.cpp | include/kcenon/monitoring/plugins/container/, src/plugins/container/ | Covered |

### Distributed Tracing

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| MON-FEAT-025 | Distributed Tracer | tests/core/test_distributed_tracing.cpp | include/kcenon/monitoring/tracing/, src/impl/tracing/ | Covered |
| MON-FEAT-026 | Thread Context | tests/context/test_thread_context.cpp, tests/context/test_thread_context_simple.cpp | include/kcenon/monitoring/context/, src/context/ | Covered |
| MON-FEAT-027 | Trace Exporters | tests/integration/test_trace_exporters.cpp | include/kcenon/monitoring/exporters/ | Covered |

### Health Monitoring

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| MON-FEAT-028 | Health Monitoring | tests/integration/test_health_monitoring.cpp | include/kcenon/monitoring/health/ | Covered |

### Alert Pipeline

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| MON-FEAT-029 | Alert Types | tests/alert/test_alert_types.cpp | include/kcenon/monitoring/alert/ | Covered |
| MON-FEAT-030 | Alert Triggers | tests/alert/test_alert_triggers.cpp | include/kcenon/monitoring/alert/ | Covered |
| MON-FEAT-031 | Alert Manager | tests/alert/test_alert_manager.cpp | include/kcenon/monitoring/alert/, src/alert/ | Covered |

### Reliability Patterns

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| MON-FEAT-032 | Error Boundaries | tests/integration/test_error_boundaries.cpp | include/kcenon/monitoring/reliability/ | Covered |
| MON-FEAT-033 | Fault Tolerance | tests/integration/test_fault_tolerance.cpp | include/kcenon/monitoring/reliability/ | Covered |

### Storage Backends

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| MON-FEAT-034 | Metric Storage | tests/utils/test_metric_storage.cpp | include/kcenon/monitoring/storage/ | Covered |
| MON-FEAT-035 | Storage Backends | tests/integration/test_storage_backends.cpp | include/kcenon/monitoring/storage/ | Covered |
| MON-FEAT-036 | Time Series Buffer | tests/utils/test_time_series_buffer.cpp | include/kcenon/monitoring/storage/ | Covered |

### Exporters

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| MON-FEAT-037 | Metric Exporters (OTLP, Prometheus) | tests/integration/test_metric_exporters.cpp | include/kcenon/monitoring/exporters/ | Covered |
| MON-FEAT-038 | OpenTelemetry Adapter | tests/integration/test_opentelemetry_adapter.cpp | include/kcenon/monitoring/adapters/ | Covered |

### Advanced Features

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| MON-FEAT-039 | Adaptive Monitoring | tests/core/test_adaptive_monitoring.cpp | include/kcenon/monitoring/adaptive/ | Covered |
| MON-FEAT-040 | Stream Aggregation | tests/utils/test_stream_aggregation.cpp | include/kcenon/monitoring/optimization/ | Covered |
| MON-FEAT-041 | Optimization (SIMD) | tests/core/test_optimization.cpp | include/kcenon/monitoring/optimization/ | Covered |
| MON-FEAT-042 | Buffering Strategies | tests/utils/test_buffering_strategies.cpp | include/kcenon/monitoring/core/ | Covered |
| MON-FEAT-043 | Statistics Utils | tests/utils/test_statistics_utils.cpp | include/kcenon/monitoring/utils/, src/utils/ | Covered |
| MON-FEAT-044 | Timer Metrics | tests/utils/test_timer_metrics.cpp | include/kcenon/monitoring/core/ | Covered |
| MON-FEAT-045 | Hot Path Helper | tests/utils/test_hot_path_helper.cpp | include/kcenon/monitoring/optimization/ | Covered |

### Dependency Injection

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| MON-FEAT-046 | DI Container | tests/integration/test_di_container.cpp | include/kcenon/monitoring/di/ | Covered |
| MON-FEAT-047 | Service Registration | tests/integration/test_service_registration.cpp | include/kcenon/monitoring/di/ | Covered |
| MON-FEAT-048 | Adapter Functionality | tests/core/test_adapter_functionality.cpp | include/kcenon/monitoring/adapters/ | Covered |

### Integration & Quality

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| MON-FEAT-049 | Cross-System Integration | tests/core/test_cross_system_integration.cpp | (cross-cutting) | Covered |
| MON-FEAT-050 | End-to-End Integration | tests/core/test_integration_e2e.cpp | (cross-cutting) | Covered |
| MON-FEAT-051 | Data Consistency | tests/integration/test_data_consistency.cpp | (cross-cutting) | Covered |
| MON-FEAT-052 | Resource Management | tests/integration/test_resource_management.cpp | (cross-cutting) | Covered |
| MON-FEAT-053 | Thread Safety | tests/core/thread_safety_tests.cpp | (cross-cutting) | Covered |
| MON-FEAT-054 | Stress & Performance | tests/core/test_stress_performance.cpp | (cross-cutting) | Covered |

## Coverage Summary

| Category | Total Features | Covered | Partial | Uncovered |
|----------|---------------|---------|---------|-----------|
| Core Monitoring | 7 | 7 | 0 | 0 |
| Collectors | 16 | 16 | 0 | 0 |
| Plugins | 1 | 1 | 0 | 0 |
| Distributed Tracing | 3 | 3 | 0 | 0 |
| Health Monitoring | 1 | 1 | 0 | 0 |
| Alert Pipeline | 3 | 3 | 0 | 0 |
| Reliability Patterns | 2 | 2 | 0 | 0 |
| Storage Backends | 3 | 3 | 0 | 0 |
| Exporters | 2 | 2 | 0 | 0 |
| Advanced Features | 7 | 7 | 0 | 0 |
| Dependency Injection | 3 | 3 | 0 | 0 |
| Integration & Quality | 6 | 6 | 0 | 0 |
| **Total** | **54** | **54** | **0** | **0** |

## See Also

- [FEATURES.md](FEATURES.md) -- Detailed feature documentation
- [README.md](README.md) -- SSOT Documentation Registry
