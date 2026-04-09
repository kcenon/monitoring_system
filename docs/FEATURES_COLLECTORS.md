---
doc_id: "MON-FEAT-COLL-001"
doc_title: "Monitoring System - Collector Features"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "monitoring_system"
category: "FEAT"
---

# Monitoring System - Collector Features

> **SSOT**: This document is the single source of truth for **Monitoring System - Collector Features** (all 16+ collector implementations).

**Version**: 0.4.0.0
**Last Updated**: 2026-02-08

## Overview

This document describes each of the built-in metric collectors provided by the monitoring system. Each section covers the metrics collected, basic usage, availability checks, configuration options, and platform implementation details.

For core capabilities and performance monitoring, see [FEATURES_CORE.md](FEATURES_CORE.md).
For alert pipeline, distributed tracing, and exporters, see [FEATURES_ALERTS_TRACING.md](FEATURES_ALERTS_TRACING.md).

---

## Table of Contents

- [Container Metrics Monitoring](#container-metrics-monitoring)
- [SMART Disk Health Monitoring](#smart-disk-health-monitoring)
- [Hardware Temperature Monitoring](#hardware-temperature-monitoring)
- [File Descriptor Usage Monitoring](#file-descriptor-usage-monitoring)
- [System Uptime Monitoring](#system-uptime-monitoring)
- [Battery Status Monitoring](#battery-status-monitoring)
- [Inode Usage Monitoring](#inode-usage-monitoring)
- [TCP Connection State Monitoring](#tcp-connection-state-monitoring)
- [Interrupt Statistics Monitoring](#interrupt-statistics-monitoring)
- [Power Consumption Monitoring](#power-consumption-monitoring)
- [GPU Metrics Monitoring](#gpu-metrics-monitoring)
- [Socket Buffer Usage Monitoring](#socket-buffer-usage-monitoring)
- [Security Event Monitoring](#security-event-monitoring)
- [Virtualization Monitoring](#virtualization-monitoring)
- [Context Switch Statistics Monitoring](#context-switch-statistics-monitoring)

---

## Container Metrics Monitoring

### Overview

The container collector provides per-container resource usage metrics for containerized environments. It supports Linux cgroups v1/v2 with graceful degradation outside containers.

**Metrics Collected**:

| Metric | Description | Unit |
|--------|-------------|------|
| **CPU Usage** | Container CPU utilization | Percent |
| **Memory Usage** | Current memory consumption | Bytes |
| **Memory Limit** | Container memory limit | Bytes |
| **Network RX** | Bytes received | Bytes |
| **Network TX** | Bytes transmitted | Bytes |
| **Block I/O Read** | Bytes read from disk | Bytes |
| **Block I/O Write** | Bytes written to disk | Bytes |
| **Process Count** | Current number of processes | Count |

### Basic Usage

```cpp
#include <kcenon/monitoring/collectors/container_collector.h>

using namespace kcenon::monitoring;

// Create and initialize collector
container_collector collector;
std::unordered_map<std::string, std::string> config = {
    {"enabled", "true"},
    {"collect_network", "true"},
    {"collect_blkio", "true"}
};
collector.initialize(config);

// Collect metrics from all containers
auto metrics = collector.collect();

for (const auto& metric : metrics) {
    std::cout << metric.name << ": ";
    if (std::holds_alternative<double>(metric.value)) {
        std::cout << std::get<double>(metric.value);
    }
    std::cout << std::endl;
}

// Get raw container metrics
auto container_metrics = collector.get_last_metrics();
for (const auto& cm : container_metrics) {
    std::cout << "Container: " << cm.container_id << std::endl;
    std::cout << "  CPU: " << cm.cpu_usage_percent << "%" << std::endl;
    std::cout << "  Memory: " << cm.memory_usage_bytes << " bytes" << std::endl;
}
```

### Cgroup Detection

The collector automatically detects the cgroup version:

```cpp
container_info_collector info_collector;

// Detect cgroup version
auto version = info_collector.detect_cgroup_version();
switch (version) {
    case cgroup_version::v1:
        std::cout << "Using cgroups v1" << std::endl;
        break;
    case cgroup_version::v2:
        std::cout << "Using cgroups v2" << std::endl;
        break;
    case cgroup_version::none:
        std::cout << "No cgroups available" << std::endl;
        break;
}

// Check if running inside a container
if (info_collector.is_containerized()) {
    std::cout << "Running inside a container" << std::endl;
}
```

### Configuration Options

| Option | Default | Description |
|--------|---------|-------------|
| `enabled` | true | Enable/disable metric collection |
| `collect_network` | true | Collect network metrics |
| `collect_blkio` | true | Collect block I/O metrics |

---

## SMART Disk Health Monitoring

### Overview

The SMART collector provides S.M.A.R.T. (Self-Monitoring, Analysis and Reporting Technology) disk health monitoring for predictive failure detection and proactive maintenance. It uses `smartctl` from smartmontools for cross-platform support.

**Metrics Collected**:

| Metric | Description | Unit |
|--------|-------------|------|
| **Health Status** | Overall drive health (PASSED/FAILED) | Boolean |
| **Temperature** | Current drive temperature | Celsius |
| **Reallocated Sectors** | Count of reallocated sectors | Count |
| **Power-On Hours** | Total hours of operation | Hours |
| **Power Cycle Count** | Number of power cycles | Count |
| **Pending Sectors** | Sectors pending reallocation | Count |
| **Uncorrectable Errors** | Uncorrectable error count | Count |

### Basic Usage

```cpp
#include <kcenon/monitoring/collectors/smart_collector.h>

using namespace kcenon::monitoring;

// Create and initialize collector
smart_collector collector;
std::unordered_map<std::string, std::string> config = {
    {"enabled", "true"},
    {"collect_temperature", "true"},
    {"collect_error_rates", "true"}
};
collector.initialize(config);

// Collect SMART metrics from all disks
auto metrics = collector.collect();

for (const auto& metric : metrics) {
    std::cout << metric.name << ": " << metric.value << std::endl;
}

// Get raw SMART metrics
auto smart_metrics = collector.get_last_metrics();
for (const auto& sm : smart_metrics) {
    std::cout << "Disk: " << sm.device_path << std::endl;
    std::cout << "  Model: " << sm.model_name << std::endl;
    std::cout << "  Health: " << (sm.health_ok ? "PASSED" : "FAILED") << std::endl;
    std::cout << "  Temperature: " << sm.temperature_celsius << "°C" << std::endl;
    std::cout << "  Power-On Hours: " << sm.power_on_hours << std::endl;
}
```

### SMART Availability Check

```cpp
smart_collector collector;
collector.initialize({});

// Check if SMART monitoring is available
if (collector.is_smart_available()) {
    std::cout << "smartctl is available" << std::endl;
    auto metrics = collector.collect();
} else {
    std::cout << "smartctl not found - install smartmontools" << std::endl;
}
```

### Configuration Options

| Option | Default | Description |
|--------|---------|-------------|
| `enabled` | true | Enable/disable SMART collection |
| `collect_temperature` | true | Collect temperature metrics |
| `collect_error_rates` | true | Collect read/write error rates |

### Platform Requirements

| Platform | Requirement |
|----------|-------------|
| **Linux** | `smartmontools` package (`apt install smartmontools`) |
| **macOS** | `smartmontools` via Homebrew (`brew install smartmontools`) |
| **Windows** | smartmontools Windows installer |

> [!NOTE]
> The collector gracefully degrades when `smartctl` is not available or a disk doesn't support SMART. No errors are returned; the collector simply returns empty metrics.

---

## Hardware Temperature Monitoring

### Overview

The temperature collector provides hardware temperature monitoring for thermal sensor data from CPU, GPU, and other system components. It supports cross-platform implementations with graceful degradation when sensors are unavailable.

**Metrics Collected**:

| Metric | Description | Unit |
|--------|-------------|------|
| **Temperature** | Current sensor temperature | Celsius |
| **Critical Threshold** | Temperature threshold for critical state | Celsius |
| **Warning Threshold** | Temperature threshold for warning state | Celsius |
| **Is Critical** | Whether temperature exceeds critical threshold | Boolean |
| **Is Warning** | Whether temperature exceeds warning threshold | Boolean |

### Basic Usage

```cpp
#include <kcenon/monitoring/collectors/temperature_collector.h>

using namespace kcenon::monitoring;

// Create and initialize collector
temperature_collector collector;
std::unordered_map<std::string, std::string> config = {
    {"enabled", "true"},
    {"collect_thresholds", "true"},
    {"collect_warnings", "true"}
};
collector.initialize(config);

// Collect temperature metrics from all sensors
auto metrics = collector.collect();

for (const auto& metric : metrics) {
    std::cout << metric.name << ": ";
    if (std::holds_alternative<double>(metric.value)) {
        std::cout << std::get<double>(metric.value);
    }
    std::cout << std::endl;
}

// Get raw temperature readings
auto readings = collector.get_last_readings();
for (const auto& reading : readings) {
    std::cout << "Sensor: " << reading.sensor.name << std::endl;
    std::cout << "  Type: " << sensor_type_to_string(reading.sensor.type) << std::endl;
    std::cout << "  Temperature: " << reading.temperature_celsius << "°C" << std::endl;
    if (reading.thresholds_available) {
        std::cout << "  Warning: " << reading.warning_threshold_celsius << "°C" << std::endl;
        std::cout << "  Critical: " << reading.critical_threshold_celsius << "°C" << std::endl;
    }
}
```

### Thermal Availability Check

```cpp
temperature_collector collector;
collector.initialize({});

// Check if thermal monitoring is available
if (collector.is_thermal_available()) {
    std::cout << "Thermal sensors accessible" << std::endl;
    auto metrics = collector.collect();
} else {
    std::cout << "No thermal sensors found" << std::endl;
}
```

### Configuration Options

| Option | Default | Description |
|--------|---------|-------------|
| `enabled` | true | Enable/disable temperature collection |
| `collect_thresholds` | true | Collect critical/warning thresholds |
| `collect_warnings` | true | Collect warning/critical status flags |

### Platform Implementation

| Platform | Method | Data Source |
|----------|--------|-------------|
| **Linux** | sysfs | `/sys/class/thermal/thermal_zone*/temp` |
| **macOS** | IOKit SMC | System Management Controller keys |
| **Windows** | WMI | `MSAcpi_ThermalZoneTemperature` class |

> [!NOTE]
> The collector gracefully degrades when thermal sensors are not available or when access is restricted. No errors are returned; the collector simply returns empty metrics.

---

## File Descriptor Usage Monitoring

### Overview

The file descriptor (FD) collector provides detailed FD usage monitoring at both system and process levels. FD exhaustion is a common failure mode in server applications ("too many open files"), and monitoring enables proactive leak detection, capacity planning, and alerting.

**Metrics Collected**:

| Metric | Description | Unit |
|--------|-------------|------|
| **fd_used_system** | Total system FDs in use (Linux only) | Count |
| **fd_max_system** | System FD limit (Linux only) | Count |
| **fd_used_process** | Current process FD count | Count |
| **fd_soft_limit** | Process FD soft limit | Count |
| **fd_hard_limit** | Process FD hard limit | Count |
| **fd_usage_percent** | Percentage of soft limit used | Percent |

### Basic Usage

```cpp
#include <kcenon/monitoring/collectors/fd_collector.h>

using namespace kcenon::monitoring;

// Create and initialize collector
fd_collector collector;
std::unordered_map<std::string, std::string> config = {
    {"enabled", "true"},
    {"warning_threshold", "80.0"},
    {"critical_threshold", "95.0"}
};
collector.initialize(config);

// Collect FD usage metrics
auto metrics = collector.collect();

for (const auto& metric : metrics) {
    std::cout << metric.name << ": ";
    if (std::holds_alternative<double>(metric.value)) {
        std::cout << std::get<double>(metric.value);
    }
    std::cout << std::endl;
}

// Get raw FD metrics
auto fd_data = collector.get_last_metrics();
std::cout << "Process FDs: " << fd_data.fd_used_process << std::endl;
std::cout << "Soft limit: " << fd_data.fd_soft_limit << std::endl;
std::cout << "Usage: " << fd_data.fd_usage_percent << "%" << std::endl;
```

### FD Monitoring Availability Check

```cpp
fd_collector collector;
collector.initialize({});

// Check if FD monitoring is available
if (collector.is_fd_monitoring_available()) {
    std::cout << "FD monitoring is available" << std::endl;
    auto metrics = collector.collect();
} else {
    std::cout << "FD monitoring not available on this platform" << std::endl;
}
```

### Configuration Options

| Option | Default | Description |
|--------|---------|-------------|
| `enabled` | true | Enable/disable FD collection |
| `warning_threshold` | 80.0 | Warning threshold (percentage of soft limit) |
| `critical_threshold` | 95.0 | Critical threshold (percentage of soft limit) |

### Platform Implementation

| Platform | Process FDs | Process Limits | System FDs |
|----------|-------------|----------------|------------|
| **Linux** | `/proc/self/fd/` enumeration | `/proc/self/limits` | `/proc/sys/fs/file-nr` |
| **macOS** | `/dev/fd/` enumeration | `getrlimit(RLIMIT_NOFILE)` | Not available |
| **Windows** | `GetProcessHandleCount()` | Default limits | Not available |

> [!NOTE]
> System-wide FD metrics are only available on Linux. On macOS and Windows, the collector gracefully degrades and returns only process-level metrics.

---

## System Uptime Monitoring

### Overview

The uptime collector provides system uptime monitoring to track boot time, uptime duration, and system availability. This is essential for availability monitoring, SLA compliance tracking, and stability analysis. Unexpected reboots or low uptime can indicate system instability.

**Metrics Collected**:

| Metric | Description | Unit |
|--------|-------------|------|
| **system_uptime_seconds** | Time since system boot | Seconds |
| **system_boot_timestamp** | Unix timestamp of last boot | Timestamp |
| **system_idle_seconds** | Total system idle time (Linux only) | Seconds |

### Basic Usage

```cpp
#include <kcenon/monitoring/collectors/uptime_collector.h>

using namespace kcenon::monitoring;

// Create and initialize collector
uptime_collector collector;
std::unordered_map<std::string, std::string> config = {
    {"enabled", "true"},
    {"collect_idle_time", "true"}
};
collector.initialize(config);

// Collect uptime metrics
auto metrics = collector.collect();

for (const auto& metric : metrics) {
    std::cout << metric.name << ": ";
    if (std::holds_alternative<double>(metric.value)) {
        std::cout << std::get<double>(metric.value);
    }
    std::cout << std::endl;
}

// Get raw uptime metrics
auto uptime_data = collector.get_last_metrics();
std::cout << "Uptime: " << uptime_data.uptime_seconds << " seconds" << std::endl;
std::cout << "Boot time: " << uptime_data.boot_timestamp << std::endl;
std::cout << "Idle time: " << uptime_data.idle_seconds << " seconds" << std::endl;
```

### Uptime Monitoring Availability Check

```cpp
uptime_collector collector;
collector.initialize({});

// Check if uptime monitoring is available
if (collector.is_uptime_monitoring_available()) {
    std::cout << "Uptime monitoring is available" << std::endl;
    auto metrics = collector.collect();
} else {
    std::cout << "Uptime monitoring not available on this platform" << std::endl;
}
```

### Configuration Options

| Option | Default | Description |
|--------|---------|-------------|
| `enabled` | true | Enable/disable uptime collection |
| `collect_idle_time` | true | Collect idle time metrics (Linux only) |

### Platform Implementation

| Platform | Method | Data Source |
|----------|--------|-------------|
| **Linux** | `/proc/uptime` | Uptime and idle time in seconds |
| **macOS** | `sysctl(KERN_BOOTTIME)` | Boot time as timeval structure |
| **Windows** | `GetTickCount64()` | Milliseconds since system start |

> [!NOTE]
> System uptime monitoring is available on all major platforms (Linux, macOS, Windows). Idle time metrics are only available on Linux. The collector gracefully provides uptime and boot timestamp on all platforms.

---

## Battery Status Monitoring

### Overview

The battery collector provides battery status monitoring for portable devices (laptops, mobile). It collects battery level, charging status, health information, time estimates, and electrical metrics. This enables power-aware scheduling, battery health tracking, and UPS-like behaviors.

**Metrics Collected**:

| Metric | Description | Unit |
|--------|-------------|------|
| **battery_level_percent** | Current charge percentage | Percent (0-100) |
| **battery_charging** | Charging status (1=charging, 0=not) | Boolean |
| **battery_status** | Current status enum value | Enum |
| **battery_ac_connected** | AC power connection status | Boolean |
| **battery_time_to_empty_seconds** | Estimated time to empty | Seconds |
| **battery_time_to_full_seconds** | Estimated time to full charge | Seconds |
| **battery_voltage_volts** | Current voltage | Volts |
| **battery_power_watts** | Current power draw/charge rate | Watts |
| **battery_health_percent** | Battery health (full_charge/design) | Percent |
| **battery_design_capacity_wh** | Original design capacity | Watt-hours |
| **battery_full_charge_capacity_wh** | Current full charge capacity | Watt-hours |
| **battery_cycle_count** | Number of charge cycles | Count |
| **battery_temperature_celsius** | Battery temperature | Celsius |

### Basic Usage

```cpp
#include <kcenon/monitoring/collectors/battery_collector.h>

using namespace kcenon::monitoring;

// Create and initialize collector
battery_collector collector;
std::unordered_map<std::string, std::string> config = {
    {"enabled", "true"},
    {"collect_health", "true"},
    {"collect_thermal", "true"}
};
collector.initialize(config);

// Collect battery metrics
auto metrics = collector.collect();

for (const auto& metric : metrics) {
    std::cout << metric.name << ": ";
    if (std::holds_alternative<double>(metric.value)) {
        std::cout << std::get<double>(metric.value);
    }
    std::cout << std::endl;
}

// Get raw battery readings
auto readings = collector.get_last_readings();
for (const auto& reading : readings) {
    std::cout << "Battery: " << reading.info.name << std::endl;
    std::cout << "  Level: " << reading.level_percent << "%" << std::endl;
    std::cout << "  Status: " << battery_status_to_string(reading.status) << std::endl;
    std::cout << "  Health: " << reading.health_percent << "%" << std::endl;
}
```

### Battery Availability Check

```cpp
battery_collector collector;
collector.initialize({});

// Check if battery monitoring is available
if (collector.is_battery_available()) {
    std::cout << "Battery monitoring is available" << std::endl;
    auto metrics = collector.collect();
} else {
    std::cout << "No battery detected on this system" << std::endl;
}
```

### Configuration Options

| Option | Default | Description |
|--------|---------|-------------|
| `enabled` | true | Enable/disable battery collection |
| `collect_health` | true | Collect health metrics (capacity, cycle count) |
| `collect_thermal` | true | Collect temperature metrics |

### Platform Implementation

| Platform | Method | Data Source |
|----------|--------|-------------|
| **Linux** | `/sys/class/power_supply/BAT*` | Sysfs battery attributes |
| **macOS** | IOKit (AppleSmartBattery) | IORegistry battery properties |
| **Windows** | GetSystemPowerStatus() + WMI | Win32_Battery class |

> [!NOTE]
> Battery status monitoring gracefully returns empty metrics when no battery is present. Health and temperature metrics availability varies by platform and hardware.

---

## Inode Usage Monitoring

### Overview

The inode collector provides filesystem inode usage monitoring to detect inode exhaustion before it causes "No space left on device" errors. This is critical for systems with many small files where disk space may be available but inodes exhausted.

**Metrics Collected**:

| Metric | Description | Unit |
|--------|-------------|------|
| **inodes_total** | Total inodes per filesystem | Count |
| **inodes_used** | Used inodes | Count |
| **inodes_free** | Free inodes | Count |
| **inodes_usage_percent** | Percentage of inodes used | Percent |
| **inodes_max_usage_percent** | Maximum usage across filesystems | Percent |
| **inodes_average_usage_percent** | Average usage across filesystems | Percent |

### Basic Usage

```cpp
#include <kcenon/monitoring/collectors/inode_collector.h>

using namespace kcenon::monitoring;

// Create and initialize collector
inode_collector collector;
std::unordered_map<std::string, std::string> config = {
    {"enabled", "true"},
    {"warning_threshold", "80.0"},
    {"critical_threshold", "95.0"}
};
collector.initialize(config);

// Collect inode usage metrics
auto metrics = collector.collect();

for (const auto& metric : metrics) {
    std::cout << metric.name << ": ";
    if (std::holds_alternative<double>(metric.value)) {
        std::cout << std::get<double>(metric.value);
    }
    std::cout << std::endl;
}

// Get raw inode metrics
auto inode_data = collector.get_last_metrics();
std::cout << "Total inodes: " << inode_data.total_inodes << std::endl;
std::cout << "Used inodes: " << inode_data.total_inodes_used << std::endl;
std::cout << "Max usage: " << inode_data.max_usage_percent << "% on "
          << inode_data.max_usage_mount_point << std::endl;

// Per-filesystem breakdown
for (const auto& fs : inode_data.filesystems) {
    std::cout << fs.mount_point << " (" << fs.filesystem_type << "): "
              << fs.inodes_usage_percent << "%" << std::endl;
}
```

### Configuration Options

| Option | Default | Description |
|--------|---------|-------------|
| `enabled` | true | Enable/disable inode collection |
| `warning_threshold` | 80.0 | Warning threshold (percentage) |
| `critical_threshold` | 95.0 | Critical threshold (percentage) |
| `include_pseudo_fs` | false | Include pseudo-filesystems (tmpfs, etc.) |

### Platform Implementation

| Platform | API Used | Filesystem Enumeration |
|----------|----------|------------------------|
| **Linux** | `statvfs()` | `/proc/mounts` parsing |
| **macOS** | `statvfs()` | `getmntinfo()` |
| **Windows** | Not applicable | NTFS uses MFT, not inodes |

> [!NOTE]
> Inode monitoring is only available on Unix-like systems. On Windows, the collector returns unavailable metrics since NTFS uses a Master File Table (MFT) instead of traditional inodes.

---

## TCP Connection State Monitoring

### Overview

The TCP state collector provides monitoring of TCP connection states to detect connection leaks, TIME_WAIT accumulation, and capacity issues. This is critical for high-traffic services where connection exhaustion can cause service failures.

**Metrics Collected**:

| Metric | Description | Unit |
|--------|-------------|------|
| **tcp_connections_established** | Active established connections | Count |
| **tcp_connections_time_wait** | Connections in TIME_WAIT | Count |
| **tcp_connections_close_wait** | Connections in CLOSE_WAIT (leak indicator) | Count |
| **tcp_connections_listen** | Listening sockets | Count |
| **tcp_connections_total** | Total connections across all states | Count |
| **tcp_connections_ipv4_total** | Total IPv4 connections | Count |
| **tcp_connections_ipv6_total** | Total IPv6 connections | Count |

### Basic Usage

```cpp
#include <kcenon/monitoring/collectors/tcp_state_collector.h>

using namespace kcenon::monitoring;

// Create and initialize collector
tcp_state_collector collector;
std::unordered_map<std::string, std::string> config = {
    {"enabled", "true"},
    {"include_ipv6", "true"},
    {"time_wait_warning_threshold", "10000"},
    {"close_wait_warning_threshold", "100"}
};
collector.initialize(config);

// Collect TCP state metrics
auto metrics = collector.collect();

for (const auto& metric : metrics) {
    std::cout << metric.name << ": ";
    if (std::holds_alternative<double>(metric.value)) {
        std::cout << std::get<double>(metric.value);
    }
    std::cout << std::endl;
}

// Get raw TCP state metrics
auto tcp_data = collector.get_last_metrics();
std::cout << "Total connections: " << tcp_data.total_connections << std::endl;
std::cout << "ESTABLISHED: " << tcp_data.combined_counts.established << std::endl;
std::cout << "TIME_WAIT: " << tcp_data.combined_counts.time_wait << std::endl;
std::cout << "CLOSE_WAIT: " << tcp_data.combined_counts.close_wait << std::endl;
std::cout << "LISTEN: " << tcp_data.combined_counts.listen << std::endl;
```

### TCP State Monitoring Availability Check

```cpp
tcp_state_collector collector;
collector.initialize({});

// Check if TCP state monitoring is available
if (collector.is_tcp_state_monitoring_available()) {
    std::cout << "TCP state monitoring is available" << std::endl;
    auto metrics = collector.collect();
} else {
    std::cout << "TCP state monitoring not available on this platform" << std::endl;
}
```

### Configuration Options

| Option | Default | Description |
|--------|---------|-------------|
| `enabled` | true | Enable/disable TCP state collection |
| `include_ipv6` | true | Include IPv6 connections in metrics |
| `time_wait_warning_threshold` | 10000 | Threshold for TIME_WAIT warning alerts |
| `close_wait_warning_threshold` | 100 | Threshold for CLOSE_WAIT warning alerts |

### Platform Implementation

| Platform | Method | Data Source |
|----------|--------|-------------|
| **Linux** | `/proc` parsing | `/proc/net/tcp` and `/proc/net/tcp6` |
| **macOS** | netstat parsing | `netstat -an -p tcp` |
| **Windows** | Stub | Future: `GetExtendedTcpTable()` API |

> [!NOTE]
> TCP state monitoring is available on Linux and macOS. On Windows, the collector returns unavailable metrics (stub implementation).

---

## Interrupt Statistics Monitoring

### Overview

The interrupt collector provides hardware and software interrupt statistics monitoring for diagnosing interrupt-related performance issues, detecting interrupt storms, and analyzing IRQ balancing problems. Interrupts are a key indicator of hardware activity and can help identify performance bottlenecks.

**Metrics Collected**:

| Metric | Description | Unit |
|--------|-------------|------|
| **interrupts_total** | Total hardware interrupt count | Count |
| **interrupts_per_sec** | Hardware interrupt rate | Count/sec |
| **soft_interrupts_total** | Total soft interrupts (Linux only) | Count |
| **soft_interrupts_per_sec** | Soft interrupt rate (Linux only) | Count/sec |

### Basic Usage

```cpp
#include <kcenon/monitoring/collectors/interrupt_collector.h>

using namespace kcenon::monitoring;

// Create and initialize collector
interrupt_collector collector;
std::unordered_map<std::string, std::string> config = {
    {"enabled", "true"},
    {"collect_per_cpu", "false"},
    {"collect_soft_interrupts", "true"}
};
collector.initialize(config);

// Collect interrupt statistics metrics
auto metrics = collector.collect();

for (const auto& metric : metrics) {
    std::cout << metric.name << ": ";
    if (std::holds_alternative<double>(metric.value)) {
        std::cout << std::get<double>(metric.value);
    }
    std::cout << std::endl;
}

// Get raw interrupt metrics
auto irq_data = collector.get_last_metrics();
std::cout << "Total interrupts: " << irq_data.interrupts_total << std::endl;
std::cout << "Interrupts/sec: " << irq_data.interrupts_per_sec << std::endl;
if (irq_data.soft_interrupts_available) {
    std::cout << "Soft interrupts: " << irq_data.soft_interrupts_total << std::endl;
}
```

### Interrupt Monitoring Availability Check

```cpp
interrupt_collector collector;
collector.initialize({});

// Check if interrupt monitoring is available
if (collector.is_interrupt_monitoring_available()) {
    std::cout << "Interrupt monitoring is available" << std::endl;
    auto metrics = collector.collect();
} else {
    std::cout << "Interrupt monitoring not available on this platform" << std::endl;
}
```

### Configuration Options

| Option | Default | Description |
|--------|---------|-------------|
| `enabled` | true | Enable/disable interrupt collection |
| `collect_per_cpu` | false | Collect per-CPU interrupt breakdown |
| `collect_soft_interrupts` | true | Collect soft interrupt metrics (Linux) |

### Platform Implementation

| Platform | Method | Data Source |
|----------|--------|-------------|
| **Linux** | `/proc` parsing | `/proc/stat` (intr line), `/proc/softirqs` |
| **macOS** | Mach API | `host_statistics64()` for system activity |
| **Windows** | Stub | Future: PDH performance counters |

> [!NOTE]
> Soft interrupt metrics are only available on Linux. On macOS and Windows, the collector returns only hardware interrupt approximations. Rate calculation requires multiple samples - the first sample will report 0 for rate metrics.

---

## Power Consumption Monitoring

### Overview

The power collector provides power consumption monitoring for energy efficiency tracking, cost analysis, and thermal management. It supports multiple power sources including batteries, RAPL (Running Average Power Limit) for Intel CPUs, and platform power metrics.

**Metrics Collected**:

| Metric | Description | Unit |
|--------|-------------|------|
| **power_consumption_watts** | Current power consumption | Watts |
| **energy_consumed_joules** | Cumulative energy consumed | Joules |
| **power_limit_watts** | Power limit/TDP | Watts |
| **voltage_volts** | Current voltage | Volts |
| **battery_percent** | Battery charge percentage | Percent |
| **battery_capacity_wh** | Battery capacity | Watt-hours |
| **battery_charge_rate** | Charging/discharging rate | Watts |
| **battery_is_charging** | Battery charging state | Boolean |

### Basic Usage

```cpp
#include <kcenon/monitoring/collectors/power_collector.h>

using namespace kcenon::monitoring;

// Create and initialize collector
power_collector collector;
std::unordered_map<std::string, std::string> config = {
    {"enabled", "true"},
    {"collect_battery", "true"},
    {"collect_rapl", "true"}
};
collector.initialize(config);

// Collect power metrics
auto metrics = collector.collect();

for (const auto& metric : metrics) {
    std::cout << metric.name << ": ";
    if (std::holds_alternative<double>(metric.value)) {
        std::cout << std::get<double>(metric.value);
    }
    std::cout << std::endl;
}

// Get raw power readings
auto readings = collector.get_last_readings();
for (const auto& reading : readings) {
    std::cout << "Source: " << reading.source.name << std::endl;
    std::cout << "  Type: " << power_source_type_to_string(reading.source.type) << std::endl;
    if (reading.power_available) {
        std::cout << "  Power: " << reading.power_watts << " W" << std::endl;
    }
    if (reading.battery_available) {
        std::cout << "  Battery: " << reading.battery_percent << "%" << std::endl;
        std::cout << "  Charging: " << (reading.is_charging ? "Yes" : "No") << std::endl;
    }
}
```

### Power Availability Check

```cpp
power_collector collector;
collector.initialize({});

// Check if power monitoring is available
if (collector.is_power_available()) {
    std::cout << "Power monitoring is available" << std::endl;
    auto metrics = collector.collect();
} else {
    std::cout << "Power monitoring not available on this platform" << std::endl;
}
```

### Configuration Options

| Option | Default | Description |
|--------|---------|-------------|
| `enabled` | true | Enable/disable power collection |
| `collect_battery` | true | Collect battery and AC adapter metrics |
| `collect_rapl` | true | Collect RAPL power metrics (Linux Intel CPUs) |

### Platform Implementation

| Platform | Method | Data Source |
|----------|--------|-------------|
| **Linux** | sysfs | `/sys/class/powercap/intel-rapl/` (RAPL), `/sys/class/power_supply/` (battery) |
| **macOS** | IOKit | SMC for power metrics, IOPowerSources for battery |
| **Windows** | WMI | `Win32_Battery` for battery metrics |

> [!NOTE]
> RAPL power metrics require Intel CPU and kernel support. The collector gracefully degrades when power sources are not available. On systems without batteries or RAPL support, the collector returns empty metrics.

---

## GPU Metrics Monitoring

### Overview

The GPU collector provides comprehensive GPU metrics monitoring for NVIDIA, AMD, Intel, and Apple GPUs. It supports utilization, memory, temperature, power, clock speed, and fan metrics with cross-platform implementations and graceful degradation when GPUs or metrics are unavailable.

**Metrics Collected**:

| Metric | Description | Unit |
|--------|-------------|------|
| **gpu_utilization_percent** | GPU compute utilization | Percent |
| **gpu_memory_used_bytes** | VRAM currently in use | Bytes |
| **gpu_memory_total_bytes** | Total VRAM capacity | Bytes |
| **gpu_memory_usage_percent** | Percentage of VRAM used | Percent |
| **gpu_temperature_celsius** | GPU temperature | Celsius |
| **gpu_power_watts** | Current power consumption | Watts |
| **gpu_power_limit_watts** | Power limit/TDP | Watts |
| **gpu_clock_mhz** | Current GPU clock speed | MHz |
| **gpu_memory_clock_mhz** | Current memory clock speed | MHz |
| **gpu_fan_speed_percent** | Fan speed | Percent |

### Basic Usage

```cpp
#include <kcenon/monitoring/collectors/gpu_collector.h>

using namespace kcenon::monitoring;

// Create and initialize collector
gpu_collector collector;
std::unordered_map<std::string, std::string> config = {
    {"enabled", "true"},
    {"collect_utilization", "true"},
    {"collect_memory", "true"},
    {"collect_temperature", "true"},
    {"collect_power", "true"},
    {"collect_clock", "true"},
    {"collect_fan", "true"}
};
collector.initialize(config);

// Collect GPU metrics from all devices
auto metrics = collector.collect();

for (const auto& metric : metrics) {
    std::cout << metric.name << ": ";
    if (std::holds_alternative<double>(metric.value)) {
        std::cout << std::get<double>(metric.value);
    }
    std::cout << std::endl;
}

// Get raw GPU readings
auto readings = collector.get_last_readings();
for (const auto& reading : readings) {
    std::cout << "GPU: " << reading.device.name << std::endl;
    std::cout << "  Vendor: " << gpu_vendor_to_string(reading.device.vendor) << std::endl;
    std::cout << "  Type: " << gpu_type_to_string(reading.device.type) << std::endl;
    if (reading.utilization_available) {
        std::cout << "  Utilization: " << reading.utilization_percent << "%" << std::endl;
    }
    if (reading.memory_available) {
        std::cout << "  Memory: " << reading.memory_used_bytes << " / " 
                  << reading.memory_total_bytes << " bytes" << std::endl;
    }
    if (reading.temperature_available) {
        std::cout << "  Temperature: " << reading.temperature_celsius << "°C" << std::endl;
    }
    if (reading.power_available) {
        std::cout << "  Power: " << reading.power_watts << " W" << std::endl;
    }
}
```

### GPU Availability Check

```cpp
gpu_collector collector;
collector.initialize({});

// Check if GPU monitoring is available
if (collector.is_gpu_available()) {
    std::cout << "GPU monitoring is available" << std::endl;
    auto metrics = collector.collect();
} else {
    std::cout << "No GPUs detected or GPU monitoring not available" << std::endl;
}
```

### Configuration Options

| Option | Default | Description |
|--------|---------|-------------|
| `enabled` | true | Enable/disable GPU collection |
| `collect_utilization` | true | Collect GPU utilization metrics |
| `collect_memory` | true | Collect VRAM usage metrics |
| `collect_temperature` | true | Collect GPU temperature |
| `collect_power` | true | Collect power consumption metrics |
| `collect_clock` | true | Collect clock speed metrics |
| `collect_fan` | true | Collect fan speed metrics |

### Platform Implementation

| Platform | Method | Data Source |
|----------|--------|-------------|
| **Linux** | sysfs | `/sys/class/drm/card*/device/` for enumeration, `hwmon` for temperature/power/fan |
| **macOS** | IOKit/SMC | IOKit for enumeration, SMC for GPU temperature |
| **Windows** | Stub | Future: DirectX/WMI/vendor APIs |

**Vendor-Specific Metrics (Linux)**:

| Vendor | Utilization | Memory | Temperature | Power | Clock | Fan |
|--------|:-----------:|:------:|:-----------:|:-----:|:-----:|:---:|
| **AMD** | Yes (gpu_busy_percent) | Yes (mem_info_vram) | Yes (hwmon) | Yes (hwmon) | Yes (pp_dpm_sclk) | Yes (hwmon) |
| **NVIDIA** | No (needs NVML) | No (needs NVML) | Yes (hwmon) | Yes (hwmon) | No (needs NVML) | Yes (hwmon) |
| **Intel** | No | No | Yes (hwmon) | No | No | No |

> [!NOTE]
> The GPU collector uses sysfs/IOKit approaches rather than vendor-specific libraries (NVML, ROCm SMI) to avoid mandatory external dependencies. AMD GPUs on Linux provide the most comprehensive metrics via sysfs. For full NVIDIA metrics, consider using NVML directly.

> [!TIP]
> On macOS, GPU temperature is available via SMC keys (TG0D, TG0P). Other metrics like utilization and memory require Metal Performance Shaders or vendor-specific APIs which are not used in this implementation.

---

## Socket Buffer Usage Monitoring

### Overview

The socket buffer collector provides socket buffer (send/receive queue) usage monitoring for network bottleneck detection. Monitoring socket buffer fill levels helps identify slow connections, dropped packets, and network tuning opportunities at the socket level.

**Metrics Collected**:

| Metric | Description | Unit |
|--------|-------------|------|
| **socket_recv_buffer_bytes** | Total bytes in receive buffers | Bytes |
| **socket_send_buffer_bytes** | Total bytes in send buffers | Bytes |
| **socket_recv_queue_full_count** | Count of non-empty recv queues | Count |
| **socket_send_queue_full_count** | Count of non-empty send queues | Count |
| **socket_memory_bytes** | Total socket buffer memory | Bytes |
| **socket_count_total** | Total number of sockets | Count |
| **socket_tcp_count** | Number of TCP sockets | Count |
| **socket_udp_count** | Number of UDP sockets | Count |

### Basic Usage

```cpp
#include <kcenon/monitoring/collectors/socket_buffer_collector.h>

using namespace kcenon::monitoring;

// Create and initialize collector
socket_buffer_collector collector;
std::unordered_map<std::string, std::string> config = {
    {"enabled", "true"},
    {"queue_full_threshold_bytes", "65536"},
    {"memory_warning_threshold_bytes", "104857600"}
};
collector.initialize(config);

// Collect socket buffer metrics
auto metrics = collector.collect();

for (const auto& metric : metrics) {
    std::cout << metric.name << ": ";
    if (std::holds_alternative<double>(metric.value)) {
        std::cout << std::get<double>(metric.value);
    }
    std::cout << std::endl;
}

// Get raw socket buffer metrics
auto buffer_data = collector.get_last_metrics();
std::cout << "Total sockets: " << buffer_data.socket_count << std::endl;
std::cout << "TCP sockets: " << buffer_data.tcp_socket_count << std::endl;
std::cout << "Recv buffer: " << buffer_data.recv_buffer_bytes << " bytes" << std::endl;
std::cout << "Send buffer: " << buffer_data.send_buffer_bytes << " bytes" << std::endl;
std::cout << "Socket memory: " << buffer_data.socket_memory_bytes << " bytes" << std::endl;
```

### Socket Buffer Monitoring Availability Check

```cpp
socket_buffer_collector collector;
collector.initialize({});

// Check if socket buffer monitoring is available
if (collector.is_socket_buffer_monitoring_available()) {
    std::cout << "Socket buffer monitoring is available" << std::endl;
    auto metrics = collector.collect();
} else {
    std::cout << "Socket buffer monitoring not available on this platform" << std::endl;
}
```

### Configuration Options

| Option | Default | Description |
|--------|---------|-------------|
| `enabled` | true | Enable/disable socket buffer collection |
| `queue_full_threshold_bytes` | 65536 | Threshold for queue warning alerts (per socket) |
| `memory_warning_threshold_bytes` | 104857600 | Threshold for memory warning alerts (100MB) |

### Platform Implementation

| Platform | Method | Data Source |
|----------|--------|-------------|
| **Linux** | `/proc` parsing | `/proc/net/tcp` (tx_queue:rx_queue), `/proc/net/sockstat` (memory) |
| **macOS** | netstat/sysctl | `netstat -m` (mbuf), `netstat -an -p tcp` (queues), `sysctl kern.ipc` |
| **Windows** | Stub | Future: `GetTcpStatistics()` API |

> [!NOTE]
> Socket buffer monitoring is available on Linux and macOS. On Windows, the collector returns unavailable metrics (stub implementation). Queue buildup (non-zero tx_queue/rx_queue) may indicate network congestion or slow receivers.

---

## Security Event Monitoring

### Overview

The Security Event Collector provides security event monitoring for audit and compliance. It tracks authentication events, privilege escalation, and account management activities with configurable privacy controls.

**Metrics Collected**:

| Metric | Description | Unit |
|--------|-------------|------|
| **security_login_success_total** | Successful login attempts | Count |
| **security_login_failure_total** | Failed login attempts | Count |
| **security_logout_total** | User logout events | Count |
| **security_sudo_usage_total** | Privilege escalation events | Count |
| **security_permission_change_total** | Permission/ACL changes | Count |
| **security_account_created_total** | New account creations | Count |
| **security_account_deleted_total** | Account deletions | Count |
| **security_events_total** | Total security events | Count |
| **security_events_per_second** | Event rate | Events/s |
| **security_active_sessions** | Current active login sessions | Sessions |

### Basic Usage

```cpp
#include <kcenon/monitoring/collectors/security_collector.h>

using namespace kcenon::monitoring;

// Create and initialize collector
security_collector collector;
std::unordered_map<std::string, std::string> config = {
    {"enabled", "true"},
    {"mask_pii", "false"},
    {"max_recent_events", "100"}
};
collector.initialize(config);

// Collect security metrics
auto metrics = collector.collect();
for (const auto& m : metrics) {
    // Process metric: security_login_success_total, etc.
}

// Get detailed security information
auto security_data = collector.get_last_metrics();
std::cout << "Login successes: " << security_data.event_counts.login_success << std::endl;
std::cout << "Login failures: " << security_data.event_counts.login_failure << std::endl;
```

### Configuration Options

| Option | Default | Description |
|--------|---------|-------------|
| `enabled` | true | Enable/disable security event collection |
| `mask_pii` | false | Mask usernames for privacy compliance |
| `max_recent_events` | 100 | Maximum recent events to store in memory |
| `login_failure_rate_limit` | 1000 | Rate limit for login failure events (events/sec) |

### Platform Implementation

| Platform | Method | Data Source |
|----------|--------|-------------|
| **Linux** | Log parsing | `/var/log/auth.log` (Debian/Ubuntu), `/var/log/secure` (RHEL/CentOS) |
| **macOS** | Stub | Future: unified logging with `log show` command |
| **Windows** | Stub | Future: Windows Event Log API (Security log) |

> [!NOTE]
> Security event monitoring requires read access to system log files. On Linux, the collector parses authentication logs for login attempts, sudo usage, and account management events. macOS and Windows use stub implementations that return unavailable metrics.

---

## Virtualization Monitoring

### Overview

The Virtualization Monitoring feature detects if the application is running inside a virtual machine and collects relevant metrics such as CPU steal time, which is critical for performance analysis in cloud environments.

**Metrics Collected**:

| Metric | Description | Unit |
|--------|-------------|------|
| **is_virtualized** | Whether the system is running in a VM | Boolean |
| **steal_time** | Percentage of time CPU is waiting for hypervisor | Percent |

### Basic Usage

```cpp
#include <kcenon/monitoring/collectors/vm_collector.h>

using namespace kcenon::monitoring;

vm_collector collector;
collector.initialize({{"enabled", "true"}});

auto metrics = collector.collect();
// Check if virtualized
// Note: Actual access to struct requires casting or specific getter if not using generic interface
```

---

## Context Switch Statistics Monitoring

### Overview

The `context_switch_collector` provides comprehensive context switch statistics monitoring to track CPU scheduling activity. Excessive context switching indicates CPU contention, poor thread pool sizing, or inefficient threading models. Monitoring context switches enables scheduling analysis and performance tuning.

**Metrics Collected**:

| Metric | Description | Unit |
|--------|-------------|------|
| **context_switches_total** | Total system-wide context switches (counter) | Count |
| **context_switches_per_sec** | Context switch rate (gauge) | Switches/sec |
| **voluntary_context_switches** | Process voluntary switches (I/O wait, sleep) | Count |
| **nonvoluntary_context_switches** | Process involuntary switches (preemption) | Count |
| **process_context_switches_total** | Total process context switches | Count |

### Basic Usage

```cpp
#include <kcenon/monitoring/collectors/context_switch_collector.h>

using namespace kcenon::monitoring;

// Create and initialize collector
context_switch_collector collector;
std::unordered_map<std::string, std::string> config = {
    {"enabled", "true"},
    {"collect_process_metrics", "true"},
    {"rate_warning_threshold", "100000"}
};
collector.initialize(config);

// Collect context switch metrics
auto metrics = collector.collect();

for (const auto& metric : metrics) {
    std::cout << metric.name << ": ";
    if (std::holds_alternative<double>(metric.value)) {
        std::cout << std::get<double>(metric.value);
    }
    std::cout << std::endl;
}

// Get raw context switch metrics
auto cs_data = collector.get_last_metrics();
std::cout << "Total context switches: " << cs_data.system_context_switches_total << std::endl;
std::cout << "Context switches/sec: " << cs_data.context_switches_per_sec << std::endl;
std::cout << "Voluntary switches: " << cs_data.process_info.voluntary_switches << std::endl;
std::cout << "Nonvoluntary switches: " << cs_data.process_info.nonvoluntary_switches << std::endl;
```

### Context Switch Monitoring Availability Check

```cpp
context_switch_collector collector;
collector.initialize({});

// Check if context switch monitoring is available
if (collector.is_context_switch_monitoring_available()) {
    std::cout << "Context switch monitoring is available" << std::endl;
    auto metrics = collector.collect();
} else {
    std::cout << "Context switch monitoring not available on this platform" << std::endl;
}
```

### Configuration Options

| Option | Default | Description |
|--------|---------|-------------|
| `enabled` | true | Enable/disable context switch collection |
| `collect_process_metrics` | true | Collect process-level context switch metrics |
| `rate_warning_threshold` | 100000 | Threshold for high context switch rate warning (switches/sec) |

### Platform Implementation

| Platform | System-wide | Process-level | Data Source |
|----------|-------------|---------------|-------------|
| **Linux** | `/proc/stat` (ctxt field) | `/proc/self/status` | voluntary/nonvoluntary_ctxt_switches |
| **macOS** | `task_info()` | `TASK_EVENTS_INFO` | Combined context switches |
| **Windows** | Stub | Stub | Not yet implemented |

> [!NOTE]
> Context switch monitoring is available on Linux and macOS. On Windows, the collector returns unavailable metrics (stub implementation). On Linux, voluntary and nonvoluntary context switches are tracked separately. On macOS, only combined context switch count is available.

---

## See Also

- [FEATURES.md](FEATURES.md) - Features index page
- [FEATURES_CORE.md](FEATURES_CORE.md) - Core capabilities and performance monitoring
- [FEATURES_ALERTS_TRACING.md](FEATURES_ALERTS_TRACING.md) - Alert pipeline, distributed tracing, exporters
- [API Reference](API_REFERENCE.md) - Complete API documentation
- [Architecture Guide](ARCHITECTURE.md) - System design and patterns
