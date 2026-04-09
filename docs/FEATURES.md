---
doc_id: "MON-FEAT-002"
doc_title: "Monitoring System - Feature Documentation Index"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "monitoring_system"
category: "FEAT"
---

# Monitoring System - Feature Documentation

> **SSOT**: This document is the index page for the monitoring system feature documentation, split across three sub-documents for easier navigation.

**Version**: 0.4.0.0
**Last Updated**: 2026-02-08

## Sub-Documents

Feature documentation is organized into three focused documents:

- **[FEATURES_CORE.md](FEATURES_CORE.md)** - Core monitoring capabilities including performance monitoring, metric types, health monitoring, error handling, dependency injection, storage backends, reliability patterns, the plugin system, adaptive monitoring, and SIMD optimization.
- **[FEATURES_COLLECTORS.md](FEATURES_COLLECTORS.md)** - All built-in collector implementations: container, SMART disk, temperature, file descriptor, uptime, battery, inode, TCP state, interrupt, power, GPU, socket buffer, security event, virtualization, and context switch collectors.
- **[FEATURES_ALERTS_TRACING.md](FEATURES_ALERTS_TRACING.md)** - Alert pipeline (triggers, notifiers, routing), distributed tracing (spans, context propagation, export), and trace/metric exporters (Jaeger, Zipkin, OTLP, Prometheus, StatsD, InfluxDB, Graphite).

## See Also

- [API Reference](API_REFERENCE.md) - Complete API documentation
- [Architecture Guide](ARCHITECTURE.md) - System design and patterns
- [User Guide](guides/USER_GUIDE.md) - Usage examples and best practices
- [Benchmarks](BENCHMARKS.md) - Performance metrics and comparisons
