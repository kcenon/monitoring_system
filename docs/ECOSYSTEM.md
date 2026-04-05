# Ecosystem Integration

**monitoring_system** provides metrics collection, tracing, alerts, and plugin support for the kcenon ecosystem.

## Dependencies
| System | Relationship |
|--------|-------------|
| common_system | Core interfaces and patterns |
| logger_system | Structured logging for metrics and alerts |

## Dependent Systems
| System | Usage |
|--------|-------|
| pacs_system | DICOM operation monitoring |

## All Systems
| System | Description | Docs |
|--------|------------|------|
| common_system | Foundation — interfaces, patterns, utilities | [Docs](https://kcenon.github.io/common_system/) |
| thread_system | Thread pool, DAG scheduling, work stealing | [Docs](https://kcenon.github.io/thread_system/) |
| logger_system | Async logging, decorators, OpenTelemetry | [Docs](https://kcenon.github.io/logger_system/) |
| container_system | Type-safe containers, SIMD serialization | [Docs](https://kcenon.github.io/container_system/) |
| **monitoring_system** | **Metrics, tracing, alerts, plugins** | **Current** |
| database_system | Multi-backend DB | [Docs](https://kcenon.github.io/database_system/) |
| network_system | TCP/UDP/WebSocket/HTTP2/QUIC/gRPC | [Docs](https://kcenon.github.io/network_system/) |
| pacs_system | DICOM medical imaging | [Docs](https://kcenon.github.io/pacs_system/) |
