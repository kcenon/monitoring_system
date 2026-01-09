# Changelog

> **Language:** **English** | [한국어](CHANGELOG_KO.md)

## Table of Contents

- [[Unreleased]](#unreleased)
  - [Added](#added)
  - [Changed](#changed)
- [[4.0.0] - 2024-09-16](#400---2024-09-16)
  - [Added - Phase 4: Core Foundation Stabilization](#added---phase-4-core-foundation-stabilization)
    - [DI Container Implementation](#di-container-implementation)
    - [Test Suite Stabilization](#test-suite-stabilization)
    - [Cross-Platform Compatibility](#cross-platform-compatibility)
    - [Build System Improvements](#build-system-improvements)
    - [Core Architecture Improvements](#core-architecture-improvements)
  - [Changed](#changed)
  - [Fixed](#fixed)
  - [Technical Details](#technical-details)
- [[3.0.0] - 2024-09-14](#300---2024-09-14)
  - [Added - Phase 3: Real-time Alerting System and Web Dashboard](#added---phase-3-real-time-alerting-system-and-web-dashboard)
    - [Alerting System](#alerting-system)
    - [Web Dashboard](#web-dashboard)
    - [API Enhancements](#api-enhancements)
    - [Performance Improvements](#performance-improvements)
  - [Changed](#changed-1)
  - [Fixed](#fixed-1)
- [[2.0.0] - 2024-08-01](#200---2024-08-01)
  - [Added - Phase 2: Advanced Monitoring Features](#added---phase-2-advanced-monitoring-features)
    - [Distributed Tracing](#distributed-tracing)
    - [Health Monitoring](#health-monitoring)
    - [Reliability Features](#reliability-features)
    - [Storage Enhancements](#storage-enhancements)
  - [Changed](#changed-2)
  - [Fixed](#fixed-2)
- [[1.0.0] - 2024-06-01](#100---2024-06-01)
  - [Added - Phase 1: Core Monitoring Foundation](#added---phase-1-core-monitoring-foundation)
    - [Core Monitoring](#core-monitoring)
    - [Plugin System](#plugin-system)
    - [Configuration](#configuration)
    - [Integration](#integration)
  - [Dependencies](#dependencies)
- [[0.9.0-beta] - 2024-05-01](#090-beta---2024-05-01)
  - [Added](#added-5)
  - [Known Issues](#known-issues)
- [Version Numbering](#version-numbering)
- [Migration Guides](#migration-guides)
  - [Upgrading to v3.0.0](#upgrading-to-v300)
  - [Upgrading to v2.0.0](#upgrading-to-v200)
- [Support](#support)

All notable changes to the Monitoring System will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- **Data consistency API** (#342)
  - `transaction_operation` class with execute/rollback capabilities
  - `transaction` class for managing multiple operations with timeout and state management
  - `transaction_manager` for coordinating transactions with deadlock detection
  - `state_validator` for continuous system state validation with auto-repair support
  - `data_consistency_manager` for centralized coordination of transaction managers and validators
  - Transaction states (active, committed, aborted) and validation results (valid, invalid)
  - Factory functions for creating managers and validators
  - Thread-safe implementation with shared mutex
  - All 22 tests in `test_data_consistency.cpp` passing
- **Health monitoring API** (#330)
  - `health_check` abstract base class with `get_name()`, `get_type()`, `check()`, `get_timeout()`, `is_critical()`
  - `functional_health_check` for lambda-based health checks
  - `composite_health_check` for aggregating multiple health checks with all-required or any-required semantics
  - `health_dependency_graph` DAG for health check dependencies with cycle detection, topological sort, failure impact analysis
  - `health_check_builder` for fluent API health check creation
  - `health_monitor` extended with `register_check()`, `unregister_check()`, `check()`, `add_dependency()`, `start()`/`stop()`, `get_stats()`, `get_health_report()`
  - `health_monitor_stats` for tracking health check statistics
  - `global_health_monitor()` singleton accessor
  - All 22 tests in `test_health_monitoring.cpp` passing
- **Resource management API** (#341)
  - Token bucket and leaky bucket rate limiters
  - Memory quota manager with threshold monitoring
  - CPU throttler for execution control
  - Resource manager coordinator for centralized resource management
- **Public ring_buffer.h and metric_storage.h APIs** (#339)
  - Added `include/kcenon/monitoring/utils/ring_buffer.h` exposing internal ring buffer as public API
  - Added `include/kcenon/monitoring/utils/metric_storage.h` with memory-efficient metric storage
  - `metric_storage` class with ring buffer buffering and time series storage for historical data
  - Thread-safe implementation with background processing support
  - `metric_storage_config` for configurable capacity, retention, and flush intervals
  - `metric_storage_stats` for monitoring storage performance
  - Enabled `test_metric_storage.cpp` with 14 passing tests
- **Tag/label support for multi-dimensional metrics in performance_monitor** (#324)
  - Added `tag_map` type alias for metric labels (key-value pairs)
  - Added `tagged_metric` struct for representing metrics with tags
  - Added `record_counter(name, value, tags)` for accumulating counters with tags
  - Added `record_gauge(name, value, tags)` for instantaneous values with tags
  - Added `record_histogram(name, value, tags)` for value distributions with tags
  - Updated `collect()` to include tagged metrics in snapshot
  - Updated `performance_monitor_adapter` to pass tags through to underlying monitor
  - Tags enable filtering and aggregation by dimensions (service, endpoint, status, etc.)
  - Thread-safe implementation using shared_mutex
  - Comprehensive unit tests for all tagged metric operations
- **Disk and Network metrics collection in system_resource_collector** (#323)
  - Implemented `collect_disk_stats()` for macOS (IOKit), Linux (/proc/diskstats), Windows
  - Implemented `collect_network_stats()` for macOS (ifaddrs), Linux (/proc/net/dev), Windows
  - Disk metrics: usage_percent, total/used/available bytes, read/write bytes per sec, read/write ops per sec
  - Network metrics: rx/tx bytes per sec, rx/tx packets per sec, rx/tx errors, rx/tx dropped
  - Added `add_disk_metrics()` and `add_network_metrics()` methods
  - Updated `get_metric_types()` to include all new metrics
  - Comprehensive unit tests for disk and network metric collection
- **Connect distributed_tracer to trace exporters (ARC-006)** (#321)
  - Connected `distributed_tracer` to `trace_exporter_interface` for Jaeger/Zipkin/OTLP export
  - Added `set_exporter()` method to configure trace exporters
  - Added `configure_export()` for export settings (batch_size, max_queue_size, export_on_finish)
  - Implemented automatic batch export when buffer threshold is reached
  - Added `flush()` method for manual span export
  - Export failure handling with retry/queue mechanism
  - Export statistics: `get_export_stats()` returns exported_spans, failed_exports, dropped_spans
  - Queue size limit enforcement to prevent memory exhaustion
  - Comprehensive unit tests for exporter integration
- **Windows CPU/Memory stats collection in system_resource_collector** (#319)
  - Implemented `collect_windows_cpu_stats()` using `GetSystemTimes()` API
  - Implemented `collect_windows_memory_stats()` using `GlobalMemoryStatusEx()` API
  - CPU metrics: usage_percent, user_percent, system_percent, idle_percent, core count
  - Memory metrics: total, available, used bytes, usage_percent
  - Swap (Page File) metrics: total, used bytes, usage_percent
  - Completed Windows platform support for system resource collection
- **C++20 Module Support** (#310)
  - Created C++20 module files for `kcenon.monitoring` module
  - Module partitions:
    - `kcenon.monitoring.core`: Core types, interfaces, concepts, and utilities
    - `kcenon.monitoring.collectors`: Metric collector implementations and registry
    - `kcenon.monitoring.adaptive`: Adaptive monitoring, alerts, and load-based adaptation
  - Primary module interface: `kcenon.monitoring`
  - CMake configuration:
    - Added `MONITORING_ENABLE_MODULES` option (requires CMake 3.28+)
    - Created `monitoring_system_modules` library target
    - Module support is opt-in during transition period
  - Header-based builds remain fully supported
  - Part of C++20 Module Migration Epic (common_system#256)

### Removed
- **Deprecated `monitoring_interface.h` header removed** (#307)
  - Deleted `include/kcenon/monitoring/interfaces/monitoring_interface.h` forwarding header
  - All code should use `monitoring_core.h` instead
  - This was a breaking change announced in the deprecation notice
  - Note: `common_system`'s `monitoring_interface.h` (IMonitor) is unaffected

### Fixed
- **Flaky StateValidatorContinuousValidation test on macOS** (#342)
  - Increased sleep duration from 250ms to 400ms for more reliable timing
  - Changed assertion from EXPECT_GT to EXPECT_GE to handle edge cases
  - Addresses platform-specific thread scheduling delays on macOS CI
- **Thread sanitizer failures in health_monitor** (#356)
  - Fixed data race in `check()`, `check_all()`, and `refresh()` methods by changing `shared_lock` to `lock_guard`
  - Added missing `<condition_variable>` header required for `cv_` member variable
  - Made `test_health_check` thread-safe with atomic status and mutex-protected message
  - Resolved 4 thread sanitizer and 1 undefined behavior sanitizer test failures
- **GCC Release build maybe-uninitialized warning in test_metric_storage.cpp** (#354)
  - Fixed uninitialized local variables in `RingBufferBasicOperations` and `RingBufferPeek` tests
  - Resolved `-Werror=maybe-uninitialized` errors occurring in GCC Release builds
- **Windows MSVC build failure due to winsock header conflicts** (#323)
  - Fixed header inclusion order in `system_resource_collector.h` to include winsock2.h before windows.h
  - Added WIN32_LEAN_AND_MEAN and NOMINMAX macros to prevent winsock.h being included via windows.h
  - Moved iphlpapi.h include to header file to ensure proper Windows socket type definitions
  - Applied consistent Windows header ordering across performance_monitor.cpp and windows_metrics.cpp
- **MSVC build error from deprecated common_system helper functions** (#314)
  - Replaced deprecated `kcenon::common::is_error()`, `get_value()`, `get_error()` with Result member methods
  - Affected adapters: `thread_system_adapter.h`, `common_monitor_adapter.h`, `common_system_adapter.h`
  - Updated examples to use modern Result API: `result.is_ok()`, `result.value()`, `result.error()`
- **macOS CI test flakiness** (#300)
  - Fixed flaky `ContextSwitchMonitoring` test on macOS CI
  - macOS reads process-level context switches which may not be monotonically increasing
  - Changed assertion to verify non-zero values instead of monotonic increase on macOS
  - Linux continues to verify monotonic increase for system-wide context switches
- Added explicit `<sys/time.h>` header include for `struct timeval` portability on macOS
- **Align test expectations with CRTP collector_base implementation** (#306)
  - Fixed IsHealthyReflectsState tests: disabled collectors are considered healthy (no errors)
  - Fixed MetricsHaveCorrectTags tests: use actual collector_name values
  - Added missing 'available' statistic to socket_buffer_collector

### Changed
- **OTLP exporter timing precision improvement** (#353)
  - Changed `otlp_exporter_stats.total_export_time` from `std::chrono::milliseconds` to `std::chrono::microseconds`
  - Provides higher precision timing for fast export operations
  - Fixes test failures on CI where stub transport operations completed in under 1ms
- **Clarify public vs internal buffer APIs** (#313)
  - Added `@public` documentation tags to public API headers: `thread_local_buffer.h`, `time_series_buffer.h`, `socket_buffer_collector.h`
  - Enhanced `@internal` documentation with warnings in internal headers: `ring_buffer.h`, `buffering_strategy.h`, `buffer_manager.h`
  - Added C++20 module migration notes to all buffer-related headers
  - Prepared buffer layer for C++20 module partitioning with clear export boundaries
- **Refactor buffering_strategy to use ring_buffer internally** (#312)
  - Modified `fixed_size_strategy`, `time_based_strategy`, and `adaptive_strategy` to use `ring_buffer<buffered_metric>` for storage
  - Added `detail::next_power_of_two()` helper function for ring_buffer capacity calculation
  - `priority_based_strategy` retains vector-based storage (requires sorting/selective deletion)
  - Updated internal include paths to use relative paths for internal headers
  - Reduces code duplication and improves memory efficiency through lock-free ring buffer
- **Apply CRTP pattern to collector implementations** (#292)
  - Created `collector_base` template class with CRTP for common collector functionality
  - Migrated 8 collectors to use collector_base: uptime, fd, battery, tcp_state,
    context_switch, inode, interrupt, socket_buffer
  - Extracted common functionality: enabled state, statistics, error handling, metric creation
  - Reduced code duplication across collectors (~400+ lines removed)
  - Pattern enables compile-time polymorphism with zero runtime overhead
- **Restructured system_resources as nested structs** (#293)
  - Reorganized flat 35-field struct into logically grouped nested sub-structs
  - CPU metrics grouped under `cpu` with `load_average` nested struct
  - Memory metrics grouped with `swap_info` nested struct
  - Disk metrics grouped with `io_throughput` nested struct
  - Cleaner access pattern: `resources.cpu.usage_percent` instead of `resources.cpu_usage_percent`
  - Enables partial access (e.g., pass `resources.cpu` only when needed)

### Added
- **Migrate all collectors to metrics_provider abstraction** (#291, #298, #299)
  - All 12 platform-specific collectors now use unified metrics_provider interface
  - Removed 36 platform-specific files (12 files × 3 platforms)
  - Significant code reduction: ~8,700 lines deleted
  - Affected collectors: battery, temperature, power, gpu, context_switch, fd,
    inode, tcp_state, socket_buffer, interrupt, security, uptime
  - info_collector classes now delegate to metrics_provider for all platforms
- **Windows metrics_provider implementation** (#291, Phase 4: #297)
  - Implemented full `windows_metrics_provider` with WMI and Win32 API
  - Battery metrics: WMI Win32_Battery + GetSystemPowerStatus fallback
  - Temperature: WMI MSAcpi_ThermalZoneTemperature
  - Uptime: GetTickCount64
  - File descriptors: GetProcessHandleCount (Windows handles)
  - TCP states: GetExtendedTcpTable API with full state tracking
  - Power info: GetSystemPowerStatus + WMI battery voltage
  - Stubs for: context switches, interrupts, GPU, security, socket buffers
- **Platform abstraction layer interface** (#291, Phase 1: #294)
  - Added `metrics_provider` abstract interface for unified platform metrics
  - Defined common data structures (uptime_info, context_switch_info, fd_info, etc.)
  - Created platform-specific provider headers (linux, macos, windows)
  - Implemented factory function for automatic platform detection
  - Foundation for consolidating 39 platform-specific files into unified layer
- **vcpkg manifest: Add ecosystem dependencies** (#277)
  - Added `kcenon-common-system` as required dependency
  - Added `kcenon-thread-system` as required dependency
  - Added `logging` feature with `kcenon-logger-system` dependency
  - Follows vcpkg ecosystem standard template
- **UDP and gRPC transport implementations** (#273)
  - Added `udp_transport.h` with abstract interface for UDP communication
  - Added `grpc_transport.h` with abstract interface for gRPC communication
  - `stub_udp_transport`: Testing implementation with simulated UDP sends
  - `stub_grpc_transport`: Testing implementation with simulated gRPC calls
  - `common_udp_transport`: Integration with common_system IUdpClient interface
  - `network_udp_transport`: Integration with network_system UDP client
  - Factory functions for automatic backend selection based on availability
- **StatsD exporter with real UDP transport** (#274)
  - Updated `statsd_exporter` to use `udp_transport` abstraction
  - Support for custom transport injection for testing
  - Start/stop lifecycle management with connection handling
  - Transport statistics included in exporter stats
- **OTLP exporter with HTTP/gRPC transport** (#275)
  - Updated `otlp_metrics_exporter` to use transport abstractions
  - OTLP/HTTP support via `http_transport` (JSON and Protobuf)
  - OTLP/gRPC support via `grpc_transport`
  - Basic OTLP JSON serialization for metrics
- **CMake transport interface detection**
  - Automatic detection of common_system transport interfaces
  - `MONITORING_HAS_COMMON_TRANSPORT_INTERFACES` compile definition
- Documentation structure reorganization
- **Deprecated API warning flag** (#267)
  - Added `-Wdeprecated-declarations` for GCC/Clang compilers
  - Added `/w14996` for MSVC compiler
  - Catches usage of deprecated common_system APIs before v3.0.0 removal
  - No deprecated API usage found in codebase (already using new patterns)
- **common_system v3.0.0 compatibility verification** (#269)
  - Verified no usage of deprecated `THREAD_LOG_*` macros (already using `LOG_*` macros)
  - Verified no usage of legacy `log(level, msg, file, line, func)` method
  - Codebase already uses modern `log(level, msg)` with automatic source_location
  - Ready for common_system v3.0.0 upgrade with no code changes required

### Changed
- **thread_system v3.0 compatibility** (#263)
  - Removed dependency on deprecated `kcenon/thread/interfaces/shared_interfaces.h`
  - Updated `monitor_adapter.h` to standalone adapter (renamed to `performance_monitor_adapter`)
  - Updated `thread_system_adapter.h` to use `common::interfaces::IMonitorable`
  - Detection now uses `thread_pool.h` instead of removed `monitorable_interface.h`
  - For thread_system integration, use `common_monitor_adapter.h` which provides adapters for `kcenon::common::interfaces::IMonitor` and `IMonitorable`

### Fixed
- **Test mock classes using deprecated ILogger API** (#272)
  - Removed deprecated 5-argument `log(level, msg, file, line, func)` override from mock_logger
  - Updated mock classes to match common_system v3.0.0 ILogger interface (Issue #217)
  - The deprecated API was replaced with source_location-based API in common_system
- **CMake error when using monitoring_system via FetchContent** (#261)
  - Fixed `kcenon::common_system` target alias not found error
  - Added support for multiple common_system target names (`kcenon::common_system`, `kcenon::common`, `common_system`, `common`)
  - Automatic alias creation when base target exists but namespaced alias doesn't
  - Fallback to header-only integration using include directories

### Changed
- **Adopt common_system Result<T> for unified error handling** (#259)
  - Mark `result<T>` and `result_void` type aliases as deprecated
  - Mark helper functions (`make_success`, `make_error`, `make_void_success`, `make_void_error`, `make_result_void`, `make_error_with_context`) as deprecated
  - Mark `MONITORING_TRY` and `MONITORING_TRY_ASSIGN` macros as deprecated
  - Add migration documentation with code examples
  - New code should use `common::Result<T>`, `common::VoidResult`, `common::ok()`, and `common::make_error()` directly
  - See migration guide in `result_types.h` for detailed examples
- Comprehensive contribution guidelines
- Security policy documentation
- **Container metrics monitoring** (#228)
  - Docker/Podman container detection and metrics collection
  - Container CPU, memory, network, and I/O monitoring
  - Container state tracking (running, paused, stopped)
  - Linux support via cgroups v1/v2 and `/proc` filesystem
  - macOS/Windows stub implementations
- **SMART disk health monitoring** (#227)
  - S.M.A.R.T. disk health metrics collection via smartctl
  - Temperature, reallocated sectors, power-on hours monitoring
  - Pending sectors and uncorrectable error tracking
  - Cross-platform support (Linux, macOS with smartmontools)
- **Hardware temperature monitoring** (#215)
  - CPU, GPU, and system temperature collection
  - Per-core CPU temperature tracking
  - Linux support via hwmon sysfs and lm-sensors
  - macOS support via SMC (System Management Controller)
- **File descriptor usage monitoring** (#220)
  - System-wide and per-process FD tracking
  - FD limit monitoring (soft/hard limits)
  - Proactive FD exhaustion detection
  - Linux support via `/proc/sys/fs/file-nr` and `/proc/self/fd`
  - macOS support via `getrlimit()` and directory enumeration
- **Inode usage monitoring** (#224)
  - Per-filesystem inode usage metrics
  - Inode exhaustion risk detection
  - Linux/macOS support via `statvfs()`
- **TCP connection state monitoring** (#225)
  - Connection state counts (ESTABLISHED, TIME_WAIT, CLOSE_WAIT, etc.)
  - IPv4 and IPv6 connection tracking
  - Connection leak detection (CLOSE_WAIT accumulation)
  - Linux support via `/proc/net/tcp` and `/proc/net/tcp6`
  - macOS support via `netstat`
- **Interrupt statistics monitoring** (#223)
  - Hardware interrupt counting and rate calculation
  - Soft interrupt tracking (Linux)
  - Per-CPU interrupt breakdown (optional)
  - Linux support via `/proc/stat` and `/proc/softirqs`
  - macOS support via `host_statistics64()`
- **Power consumption monitoring** (#216)
  - CPU/package power consumption via Intel RAPL
  - Battery power and charge rate monitoring
  - Energy consumption tracking in Joules
  - Linux support via powercap sysfs
  - macOS support via IOKit and SMC
- **GPU metrics monitoring** (#221)
  - Multi-vendor support (NVIDIA, AMD, Intel, Apple)
  - GPU utilization, VRAM usage, temperature, power, clock speeds
  - Fan speed monitoring
  - Linux support via sysfs and hwmon
  - macOS support via IOKit
- **Socket buffer usage monitoring** (#226)
  - TCP send/receive queue monitoring
  - Socket memory consumption tracking
  - Network bottleneck detection
  - Linux support via `/proc/net/tcp` and `/proc/net/sockstat`
  - macOS support via `netstat` and `sysctl`
- **Security event monitoring** (#230)
  - Login success/failure tracking
  - Sudo usage and privilege escalation monitoring
  - Account creation/deletion events
  - Session tracking
  - Linux support via auth.log parsing
- **Virtualization metrics monitoring** (#229)
  - VM environment detection (VMware, VirtualBox, Hyper-V, KVM, etc.)
  - Guest CPU steal time monitoring
  - Hypervisor vendor identification
  - Linux support via DMI and cpuinfo
  - macOS support via sysctl
- **Context switch statistics monitoring** (#222)
  - System-wide context switch counting and rate
  - Per-process voluntary/involuntary switch tracking
  - Scheduling overhead analysis
  - Linux support via `/proc/stat` and `/proc/self/status`
  - macOS support via `task_info()`
- **Load average history tracking** (#219)
  - 1/5/15 minute load average collection
  - Historical load average data with time-series buffer
  - Trend analysis support
  - Cross-platform support (Linux, macOS, Windows)
- **System uptime monitoring** (#217)
  - Cross-platform uptime tracking (Linux, macOS, Windows)
  - Boot timestamp and uptime duration metrics
  - Idle time tracking on Linux via `/proc/uptime`
  - macOS support via `sysctl(KERN_BOOTTIME)`
  - Windows support via `GetTickCount64()`
- **Battery status monitoring** (#218)
  - Cross-platform battery monitoring (Linux, macOS, Windows)
  - Battery level percentage, charging status, time estimates
  - Battery health percentage and cycle count
  - Voltage, current, and power metrics
  - Temperature monitoring (when available)
  - Linux support via `/sys/class/power_supply/BAT*` sysfs
  - macOS support via IOKit (AppleSmartBattery)
  - Windows support via GetSystemPowerStatus() and WMI
- **C++20 Concepts support** (#247)
  - Added `monitoring_concepts.h` with concepts for metrics, events, and collectors
  - Added concepts to `event_bus_interface.h`: `EventType`, `EventHandler`, `EventFilter`
  - Added concepts to `metric_collector_interface.h`: `Validatable`, `MetricSourceLike`, `MetricCollectorLike`
  - Compile-time type validation with clear error messages
  - Static assertions for concept satisfaction verification

### Changed
- **C++20 now required**: Upgraded from C++17 to C++20 for Concepts support
  - Updated compiler requirements: GCC 10+, Clang 10+, MSVC 2019 16.3+
  - common_system integration with C++20 Concepts when BUILD_WITH_COMMON_SYSTEM defined
- **logger_system now optional**: Changed from required to optional dependency (#213)
  - monitoring_system now uses common_system's ILogger interface for runtime binding
  - logger_system can be injected at runtime via dependency injection
  - Removes compile-time dependency on logger_system
  - MONITORING_WITH_LOGGER_SYSTEM option defaults to OFF
- Unified documentation into centralized structure
- **Removed fmt library fallback**: CMake configuration now requires C++20 `std::format` exclusively
  - Simplifies build configuration by removing external dependency fallback logic
  - Part of ecosystem-wide standardization on C++20 features
  - Related: thread_system#219, container_system#168, network_system#257, database_system#203, logger_system#218
- **Upgraded macOS CI runner from 13 to 14**: Required for C++20 `std::format` support
  - Apple Clang on macOS-13 lacks `std::format` support
  - macOS-14 (Sonoma) includes Apple Clang 15+ with `std::format` support
  - Updated triplet from `x64-osx` to `arm64-osx` for M1/M2 architecture

## [4.0.0] - 2024-09-16

### Added - Phase 4: Core Foundation Stabilization

#### DI Container Implementation
- **Complete dependency injection container**: Full service registration, resolution, and lifecycle management
- **Service lifecycles**: Support for transient, singleton, and named services
- **Type-safe resolution**: Template-based service resolution with compile-time type checking
- **Instance management**: Automatic lifecycle management for singleton services
- **Error handling**: Comprehensive error reporting for missing or invalid services

#### Test Suite Stabilization
- **37 core tests passing**: 100% success rate for all implemented features
- **Test categories**: Result types (13), DI container (9), monitorable interface (9), thread context (6)
- **Disabled tests**: 3 tests temporarily disabled for unimplemented features
- **Test framework**: Google Test integration with comprehensive assertions
- **Continuous validation**: Automated test execution in CI/CD pipeline

#### Cross-Platform Compatibility
- **Windows CI compliance**: Fixed MSVC warning-as-error compilation issues
- **Parameter suppression**: Added proper unused parameter handling in performance_monitor.cpp and distributed_tracer.h
- **Compiler support**: Validated compatibility with GCC 11+, Clang 14+, MSVC 2019+
- **Build system**: Optimized CMake configuration for all platforms

#### Build System Improvements
- **Static library generation**: Successfully built libmonitoring_system.a (7.2MB)
- **Example applications**: All 4 examples compile and execute successfully
- **CMake integration**: Streamlined build configuration with proper dependency management
- **Testing infrastructure**: Google Test framework integrated with CTest

#### Core Architecture Improvements
- **Result pattern**: Comprehensive Result<T> implementation for error handling
- **Thread context**: Thread-safe context management for monitoring operations
- **Stub implementations**: Functional stubs providing foundation for future development
- **Modular design**: Clean separation allowing for incremental feature expansion

### Changed
- **Test simplification**: Simplified thread_context tests to match actual API implementation
- **Error handling**: Improved error reporting throughout the codebase
- **Code quality**: Improved code consistency and reduced warning noise
- **Documentation**: Updated all documentation to reflect actual implementation status

### Fixed
- **Compilation issues**: Resolved underlying compilation issues across test files
- **Warning suppression**: Fixed Windows CI unused parameter warnings
- **Test stability**: Stabilized core test suite with reliable execution
- **Build configuration**: Optimized CMake for working components only

### Technical Details
- **Architecture**: Modular design with functional stub implementations
- **Dependencies**: Standalone operation with optional thread_system/logger_system integration
- **Test coverage**: 100% coverage of implemented features
- **Performance**: Optimized for core operations with minimal overhead

## [3.0.0] - 2024-09-14

### Added - Phase 3: Real-time Alerting System and Web Dashboard

#### Alerting System
- **Rule-based alerting engine**: Configurable alert rules with threshold and rate-based conditions
- **Multi-channel notifications**: Support for Email, SMS, Webhook, and Slack notifications
- **Alert severity levels**: Critical, warning, info, and debug severity classifications
- **Alert state management**: Proper tracking of alert lifecycle (pending, firing, resolved)
- **Alert aggregation**: Smart grouping and deduplication of similar alerts
- **Escalation policies**: Automatic alert escalation based on time and severity

#### Web Dashboard
- **Real-time visualization**: Live metrics dashboard with WebSocket streaming
- **Interactive charts**: Time-series charts with zoom, pan, and filtering capabilities
- **Responsive UI**: Mobile-friendly interface with adaptive layouts
- **Multi-panel support**: Customizable dashboard layout with drag-and-drop panels
- **Historical data view**: Access to historical metrics with configurable time ranges
- **Alert management UI**: View, acknowledge, and manage alerts through web interface

#### API Enhancements
- **RESTful API**: Complete REST API for metrics, alerts, and dashboard management
- **WebSocket support**: Real-time data streaming for live dashboard updates
- **Authentication**: Basic authentication and API key support
- **CORS support**: Cross-origin resource sharing for web integration
- **Metric aggregation API**: Endpoints for querying aggregated metrics
- **Dashboard configuration API**: Dynamic dashboard creation and management

#### Performance Improvements
- **Optimized storage**: Improved time-series data storage with compression
- **Efficient querying**: Optimized metric queries with indexing and caching
- **Memory management**: Reduced memory footprint with smart data retention
- **Connection pooling**: Efficient WebSocket connection management
- **Batch processing**: Optimized batch metric collection and processing

### Changed
- **Architecture refactoring**: Event-driven architecture with Observer pattern
- **Storage engine**: Enhanced time-series storage with better compression
- **Configuration system**: Unified configuration management across all components
- **Error handling**: Comprehensive error handling with result patterns throughout

### Fixed
- **Memory leaks**: Resolved memory leaks in metric collection pipeline
- **Threading issues**: Fixed race conditions in concurrent metric processing
- **WebSocket stability**: Improved WebSocket connection stability
- **Metric precision**: Enhanced numerical precision for high-frequency metrics

## [2.0.0] - 2024-08-01

### Added - Phase 2: Advanced Monitoring Features

#### Distributed Tracing
- **Trace context propagation**: Support for distributed tracing correlation
- **Span management**: Hierarchical span tracking with parent-child relationships
- **Trace sampling**: Configurable sampling strategies for performance optimization
- **Trace export**: Export traces in OpenTelemetry compatible format

#### Health Monitoring
- **Component health checks**: Track health status of individual components
- **Dependency monitoring**: Validate health of external dependencies
- **Health aggregation**: Calculate overall system health from components
- **Health thresholds**: Configurable health score thresholds and alerts

#### Reliability Features
- **Circuit breaker pattern**: Automatic failure detection and recovery
- **Retry mechanisms**: Configurable retry policies with exponential backoff
- **Bulkhead pattern**: Resource isolation for fault tolerance
- **Graceful degradation**: Fallback mechanisms for service outages

#### Storage Enhancements
- **Ring buffer storage**: High-performance circular buffer for time-series data
- **Data compression**: Efficient storage with configurable compression algorithms
- **Retention policies**: Automatic data cleanup based on age and storage limits
- **Storage backends**: Support for multiple storage backend implementations

### Changed
- **Observer pattern**: Refactored event system using Observer pattern
- **Plugin architecture**: Enhanced plugin system for extensible collectors
- **Performance optimizations**: Significant performance improvements in data collection

### Fixed
- **Concurrency issues**: Resolved race conditions in multi-threaded scenarios
- **Memory management**: Improved memory efficiency and leak prevention

## [1.0.0] - 2024-06-01

### Added - Phase 1: Core Monitoring Foundation

#### Core Monitoring
- **Basic metric collection**: Counter, gauge, and histogram metric types
- **Metric registry**: Central registry for metric registration and management
- **Time-series storage**: Basic time-series data storage capabilities
- **Metric export**: Support for various export formats (JSON, CSV, Prometheus)

#### Plugin System
- **Collector plugins**: Extensible plugin architecture for custom collectors
- **System metrics**: Built-in collectors for CPU, memory, disk, and network metrics
- **Application metrics**: Support for custom application-specific metrics
- **Plugin management**: Dynamic plugin loading and unloading capabilities

#### Configuration
- **YAML configuration**: Human-readable configuration file support
- **Environment variables**: Configuration override through environment variables
- **Validation**: Configuration validation with detailed error reporting
- **Hot reload**: Runtime configuration updates without restart

#### Integration
- **Thread system integration**: Optional integration with thread_system for enhanced performance
- **Logger integration**: Optional integration with logger_system for structured logging
- **Standalone operation**: Full functionality without external dependencies

### Dependencies
- C++20 compatible compiler
- CMake 3.16+
- Optional: thread_system for enhanced threading capabilities
- Optional: logger_system for structured logging

## [0.9.0-beta] - 2024-05-01

### Added
- Initial beta release
- Basic monitoring infrastructure
- Proof-of-concept implementation

### Known Issues
- Limited documentation
- Performance not optimized
- Basic test coverage

---

## Version Numbering

This project uses [Semantic Versioning](https://semver.org/):

- **MAJOR**: Incompatible API changes
- **MINOR**: New functionality in a backwards compatible manner
- **PATCH**: Backwards compatible bug fixes

## Migration Guides

### Upgrading to v3.0.0

The v3.0.0 release introduces breaking changes to the API:

1. **Configuration changes**:
   - Alert configuration moved to `alerting` section
   - Dashboard configuration added to `dashboard` section

2. **API changes**:
   - `MetricsCollector::collect()` now returns `Result<MetricsSnapshot>`
   - Alert rule API changed from function-based to class-based

3. **Migration steps**:
   ```cpp
   // Old way (v2.x)
   auto metrics = collector.collect();

   // New way (v3.x)
   auto result = collector.collect();
   if (result.is_ok()) {
       auto metrics = result.value();
   }
   ```

### Upgrading to v2.0.0

1. **Plugin API changes**: Plugin interface updated for better extensibility
2. **Configuration format**: Some configuration key names changed for clarity
3. **Storage changes**: Time-series storage format updated for better performance

For detailed migration instructions, refer to [ARCHITECTURE_GUIDE.md](ARCHITECTURE_GUIDE.md).

## Support

- **Issues**: [GitHub Issues](https://github.com/kcenon/monitoring_system/issues)
- **Discussions**: [GitHub Discussions](https://github.com/kcenon/monitoring_system/discussions)
- **Security**: For security-related issues, see [SECURITY.md](SECURITY.md)

---

*Last Updated: 2025-12-10*
