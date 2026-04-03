---
doc_id: "MON-QUAL-002"
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
| MON-FEAT-001 | Performance Monitoring | tests/test_performance_monitoring.cpp | include/kcenon/monitoring/core/, src/core/ | Covered |
| MON-FEAT-002 | Metric Factory | tests/test_metric_factory.cpp | include/kcenon/monitoring/factory/ | Covered |
| MON-FEAT-003 | Metrics Provider (Platform) | tests/test_metrics_provider.cpp | include/kcenon/monitoring/platform/, src/platform/ | Covered |
| MON-FEAT-004 | Result Types | tests/test_result_types.cpp | include/kcenon/monitoring/core/ | Covered |
| MON-FEAT-005 | Event Bus | tests/test_event_bus.cpp | include/kcenon/monitoring/core/ | Covered |
| MON-FEAT-006 | Interfaces (Compile Check) | tests/test_interfaces_compile.cpp | include/kcenon/monitoring/interfaces/ | Covered |
| MON-FEAT-007 | Monitorable Interface | tests/test_monitorable_interface.cpp | include/kcenon/monitoring/interfaces/ | Covered |

### Collectors

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| MON-FEAT-008 | System Resource Collector | tests/test_system_resource_collector.cpp | include/kcenon/monitoring/collectors/, src/collectors/ | Covered |
| MON-FEAT-009 | Process Metrics Collector | tests/test_process_metrics_collector.cpp | include/kcenon/monitoring/collectors/ | Covered |
| MON-FEAT-010 | Platform Metrics Collector | tests/test_platform_metrics_collector.cpp | include/kcenon/monitoring/collectors/ | Covered |
| MON-FEAT-011 | Network Metrics Collector | tests/test_network_metrics_collector.cpp | include/kcenon/monitoring/collectors/ | Covered |
| MON-FEAT-012 | Container Collector | tests/test_container_collector.cpp | include/kcenon/monitoring/collectors/ | Covered |
| MON-FEAT-013 | Battery Collector | tests/test_battery_collector.cpp | include/kcenon/monitoring/collectors/ | Covered |
| MON-FEAT-014 | Temperature Collector | tests/test_temperature_collector.cpp | include/kcenon/monitoring/collectors/ | Covered |
| MON-FEAT-015 | GPU Collector | tests/test_gpu_collector.cpp | include/kcenon/monitoring/collectors/ | Covered |
| MON-FEAT-016 | Uptime Collector | tests/test_uptime_collector.cpp | include/kcenon/monitoring/collectors/ | Covered |
| MON-FEAT-017 | Power Collector | tests/test_power_collector.cpp | include/kcenon/monitoring/collectors/ | Covered |
| MON-FEAT-018 | Interrupt Collector | tests/test_interrupt_collector.cpp | include/kcenon/monitoring/collectors/ | Covered |
| MON-FEAT-019 | Security Collector | tests/test_security_collector.cpp | include/kcenon/monitoring/collectors/ | Covered |
| MON-FEAT-020 | VM Collector | tests/test_vm_collector.cpp | include/kcenon/monitoring/collectors/ | Covered |
| MON-FEAT-021 | Smart Collector | tests/test_smart_collector.cpp | include/kcenon/monitoring/collectors/ | Covered |
| MON-FEAT-022 | Lock-Free Collector | tests/test_lock_free_collector.cpp | include/kcenon/monitoring/optimization/ | Covered |
| MON-FEAT-023 | Collector Registry | tests/test_collector_registry.cpp, tests/test_collector_registry_integration.cpp | include/kcenon/monitoring/factory/ | Covered |

### Plugins

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| MON-FEAT-024 | Container Plugin | tests/test_container_plugin.cpp | include/kcenon/monitoring/plugins/container/, src/plugins/container/ | Covered |

### Distributed Tracing

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| MON-FEAT-025 | Distributed Tracer | tests/test_distributed_tracing.cpp | include/kcenon/monitoring/tracing/, src/impl/tracing/ | Covered |
| MON-FEAT-026 | Thread Context | tests/test_thread_context.cpp, tests/test_thread_context_simple.cpp | include/kcenon/monitoring/context/, src/context/ | Covered |
| MON-FEAT-027 | Trace Exporters | tests/test_trace_exporters.cpp | include/kcenon/monitoring/exporters/ | Covered |

### Health Monitoring

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| MON-FEAT-028 | Health Monitoring | tests/test_health_monitoring.cpp | include/kcenon/monitoring/health/ | Covered |

### Alert Pipeline

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| MON-FEAT-029 | Alert Types | tests/test_alert_types.cpp | include/kcenon/monitoring/alert/ | Covered |
| MON-FEAT-030 | Alert Triggers | tests/test_alert_triggers.cpp | include/kcenon/monitoring/alert/ | Covered |
| MON-FEAT-031 | Alert Manager | tests/test_alert_manager.cpp | include/kcenon/monitoring/alert/, src/alert/ | Covered |

### Reliability Patterns

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| MON-FEAT-032 | Error Boundaries | tests/test_error_boundaries.cpp | include/kcenon/monitoring/reliability/ | Covered |
| MON-FEAT-033 | Fault Tolerance | tests/test_fault_tolerance.cpp | include/kcenon/monitoring/reliability/ | Covered |

### Storage Backends

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| MON-FEAT-034 | Metric Storage | tests/test_metric_storage.cpp | include/kcenon/monitoring/storage/ | Covered |
| MON-FEAT-035 | Storage Backends | tests/test_storage_backends.cpp | include/kcenon/monitoring/storage/ | Covered |
| MON-FEAT-036 | Time Series Buffer | tests/test_time_series_buffer.cpp | include/kcenon/monitoring/storage/ | Covered |

### Exporters

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| MON-FEAT-037 | Metric Exporters (OTLP, Prometheus) | tests/test_metric_exporters.cpp | include/kcenon/monitoring/exporters/ | Covered |
| MON-FEAT-038 | OpenTelemetry Adapter | tests/test_opentelemetry_adapter.cpp | include/kcenon/monitoring/adapters/ | Covered |

### Advanced Features

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| MON-FEAT-039 | Adaptive Monitoring | tests/test_adaptive_monitoring.cpp | include/kcenon/monitoring/adaptive/ | Covered |
| MON-FEAT-040 | Stream Aggregation | tests/test_stream_aggregation.cpp | include/kcenon/monitoring/optimization/ | Covered |
| MON-FEAT-041 | Optimization (SIMD) | tests/test_optimization.cpp | include/kcenon/monitoring/optimization/ | Covered |
| MON-FEAT-042 | Buffering Strategies | tests/test_buffering_strategies.cpp | include/kcenon/monitoring/core/ | Covered |
| MON-FEAT-043 | Statistics Utils | tests/test_statistics_utils.cpp | include/kcenon/monitoring/utils/, src/utils/ | Covered |
| MON-FEAT-044 | Timer Metrics | tests/test_timer_metrics.cpp | include/kcenon/monitoring/core/ | Covered |
| MON-FEAT-045 | Hot Path Helper | tests/test_hot_path_helper.cpp | include/kcenon/monitoring/optimization/ | Covered |

### Dependency Injection

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| MON-FEAT-046 | DI Container | tests/test_di_container.cpp | include/kcenon/monitoring/di/ | Covered |
| MON-FEAT-047 | Service Registration | tests/test_service_registration.cpp | include/kcenon/monitoring/di/ | Covered |
| MON-FEAT-048 | Adapter Functionality | tests/test_adapter_functionality.cpp | include/kcenon/monitoring/adapters/ | Covered |

### Integration & Quality

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| MON-FEAT-049 | Cross-System Integration | tests/test_cross_system_integration.cpp | (cross-cutting) | Covered |
| MON-FEAT-050 | End-to-End Integration | tests/test_integration_e2e.cpp | (cross-cutting) | Covered |
| MON-FEAT-051 | Data Consistency | tests/test_data_consistency.cpp | (cross-cutting) | Covered |
| MON-FEAT-052 | Resource Management | tests/test_resource_management.cpp | (cross-cutting) | Covered |
| MON-FEAT-053 | Thread Safety | tests/thread_safety_tests.cpp | (cross-cutting) | Covered |
| MON-FEAT-054 | Stress & Performance | tests/test_stress_performance.cpp | (cross-cutting) | Covered |

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
