---
doc_id: "MON-API-002"
doc_title: "Monitoring System API Reference Index"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "monitoring_system"
category: "API"
---

# Monitoring System API Reference

> **SSOT**: This document is the index page for the monitoring system API reference, split across three sub-documents for easier navigation.

> **Language:** **English** | [한국어](API_REFERENCE.kr.md)

**Version**: 0.4.0.0
**Last Updated**: 2026-02-08

## Sub-Documents

The API reference is organized into three focused documents:

- **[API_REFERENCE_CORE.md](API_REFERENCE_CORE.md)** - Core components (result types, thread context, DI container), monitoring interfaces, performance monitor, adaptive optimizer, C++20 concepts, error codes, thread safety, best practices, migration guide, and test coverage.
- **[API_REFERENCE_COLLECTORS.md](API_REFERENCE_COLLECTORS.md)** - Class, struct, and enum references for all built-in collectors: SMART, FD, inode, TCP state, interrupt, power, GPU, socket buffer, security, container, temperature, context switch, uptime, battery, and VM collectors.
- **[API_REFERENCE_ALERTS_EXPORT.md](API_REFERENCE_ALERTS_EXPORT.md)** - Health monitoring, distributed tracer, storage backends, stream processing, reliability features (circuit breaker, retry policy, error boundaries), OpenTelemetry integration, and trace/metric exporter APIs.

## Quick Links by Topic

| I want to... | See |
|--------------|-----|
| Use `Result<T>` for error handling | [API_REFERENCE_CORE.md - Result Types](API_REFERENCE_CORE.md#result-types-fully-implemented) |
| Implement a custom metric collector | [API_REFERENCE_CORE.md - Monitoring Interfaces](API_REFERENCE_CORE.md#monitoring-interfaces) |
| Use the DI container | [API_REFERENCE_CORE.md - Dependency Injection Container](API_REFERENCE_CORE.md#dependency-injection-container-fully-implemented) |
| Profile operations with scoped timer | [API_REFERENCE_CORE.md - Performance Monitor](API_REFERENCE_CORE.md#performance-monitor) |
| Use a specific collector (GPU, TCP, etc.) | [API_REFERENCE_COLLECTORS.md](API_REFERENCE_COLLECTORS.md) |
| Start distributed traces | [API_REFERENCE_ALERTS_EXPORT.md - Distributed Tracing](API_REFERENCE_ALERTS_EXPORT.md#distributed-tracing) |
| Set up health checks | [API_REFERENCE_ALERTS_EXPORT.md - Health Monitoring](API_REFERENCE_ALERTS_EXPORT.md#health-monitoring) |
| Use circuit breaker and retry policies | [API_REFERENCE_ALERTS_EXPORT.md - Reliability Features](API_REFERENCE_ALERTS_EXPORT.md#reliability-features) |
| Pick a storage backend | [API_REFERENCE_ALERTS_EXPORT.md - Storage Backends](API_REFERENCE_ALERTS_EXPORT.md#storage-backends) |
| Export to Prometheus/Jaeger/Zipkin/OTLP | [API_REFERENCE_ALERTS_EXPORT.md - OpenTelemetry Integration](API_REFERENCE_ALERTS_EXPORT.md#opentelemetry-integration) |

## Further Reading

- [FEATURES.md](FEATURES.md) - Feature documentation index
- [Phase 4 Documentation](advanced/ARCHITECTURE_GUIDE.md) - Phase 4 implementation status and architecture decisions
- [Architecture Guide](ARCHITECTURE.md) - System design and patterns
- [Examples](../examples/) - Working code examples
- [Changelog](CHANGELOG.md) - Version history and changes

---

*Last Updated: 2026-02-08*
