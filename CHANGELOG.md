# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.0.0] - 2026-04-16

### Added

- Result-based static factory methods (`create()`) for `ring_buffer`, `metric_storage`, `time_series_buffer`, and `performance_monitor_adapter` to eliminate `throw` in public API construction paths ([#667](https://github.com/kcenon/monitoring_system/issues/667))
- Add reusable GitHub Actions workflow for automated vcpkg registry synchronization ([#607](https://github.com/kcenon/monitoring_system/issues/607))

### Changed

- Bump version to 1.0.0 — public API freeze ([#667](https://github.com/kcenon/monitoring_system/issues/667))
- CMake export target `monitoring_system::monitoring_system` is now the stable public target name
- `make_monitor_adapter()` now returns `Result<shared_ptr<IMonitor>>` instead of `shared_ptr<IMonitor>` (breaking change from v0.x)
- Consolidate 8 bidirectional adapter files into 3 umbrella headers with backward-compatible includes ([#599](https://github.com/kcenon/monitoring_system/issues/599))

### Deprecated

- Throwing constructors in `ring_buffer`, `metric_storage`, `time_series_buffer`, and `performance_monitor_adapter` — use `create()` static factory methods instead

## [0.1.0] - 2026-03-11

### Added
- Pluggable metric collector architecture with factory pattern
- System resource collectors (CPU, memory, disk, network)
- Distributed tracing with OpenTelemetry-compatible context propagation
- Alert system with configurable thresholds and circuit breakers
- Custom Doxygen ALIASES (@thread_safety, @performance)
- Ecosystem integration adapters (thread_system, logger_system, container_system)
- C++20 concepts for collector validation
- gRPC transport for OTLP trace export
- Plugin system (built-in and dynamic shared library plugins)
- Dependabot and OSV-Scanner vulnerability monitoring (#501)
- SBOM generation and CVE scanning workflows (#500)
- IEC 62304 SOUP compliance documentation (#492)
- vcpkg overlay ports for ecosystem dependencies

### Infrastructure
- GitHub Actions CI/CD with sanitizer testing
- Doxygen documentation workflow
- vcpkg manifest with optional features (grpc, logging)
- Cross-platform support (Linux, macOS, Windows)

[Unreleased]: https://github.com/kcenon/monitoring_system/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/kcenon/monitoring_system/compare/v0.1.0...v1.0.0
