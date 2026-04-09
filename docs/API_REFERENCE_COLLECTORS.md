---
doc_id: "MON-API-COLL-001"
doc_title: "Monitoring System API Reference - Collectors"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "monitoring_system"
category: "API"
---

# Monitoring System API Reference - Collectors

> **SSOT**: This document is the single source of truth for **Monitoring System API - Collector Classes** (class, struct, and enum references for all built-in metric collectors).

For core components, performance monitor, and fundamental types, see [API_REFERENCE_CORE.md](API_REFERENCE_CORE.md).
For alerts, exporters, tracing, reliability, and storage APIs, see [API_REFERENCE_ALERTS_EXPORT.md](API_REFERENCE_ALERTS_EXPORT.md).

## Table of Contents

- [SMART Disk Health Collector](#smart-disk-health-collector)
- [File Descriptor Usage Collector](#file-descriptor-usage-collector)
- [Inode Usage Collector](#inode-usage-collector)
- [TCP Connection State Collector](#tcp-connection-state-collector)
- [Interrupt Statistics Collector](#interrupt-statistics-collector)
- [Power Consumption Collector](#power-consumption-collector)
- [GPU Metrics Collector](#gpu-metrics-collector)
- [Socket Buffer Collector](#socket-buffer-collector)
- [Security Event Collector](#security-event-collector)
- [Container Metrics Collector](#container-metrics-collector)
- [Hardware Temperature Collector](#hardware-temperature-collector)
- [Context Switch Statistics Monitoring](#context-switch-statistics-monitoring)
- [System Uptime Monitoring](#system-uptime-monitoring)
- [Battery Status Monitoring](#battery-status-monitoring)
- [Virtualization Monitoring](#virtualization-monitoring)

---

### SMART Disk Health Collector
**Header:** `include/kcenon/monitoring/collectors/smart_collector.h`

#### `smart_collector`
Collects S.M.A.R.T. disk health metrics using smartctl (smartmontools).

```cpp
class smart_collector {
public:
    smart_collector();

    // Initialize with configuration
    bool initialize(const std::unordered_map<std::string, std::string>& config);

    // Collect SMART metrics from all disks
    std::vector<metric> collect();

    // Get collector name
    std::string get_name() const;

    // Get supported metric types
    std::vector<std::string> get_metric_types() const;

    // Check if collector is healthy
    bool is_healthy() const;

    // Get collection statistics
    std::unordered_map<std::string, double> get_statistics() const;

    // Get last collected metrics
    std::vector<smart_disk_metrics> get_last_metrics() const;

    // Check if SMART monitoring is available
    bool is_smart_available() const;
};
```

#### `smart_disk_metrics`
Structure containing per-disk SMART health data.

```cpp
struct smart_disk_metrics {
    std::string device_path;        // Device path (e.g., /dev/sda)
    std::string model_name;         // Disk model name
    std::string serial_number;      // Disk serial number
    bool smart_supported;           // SMART support available
    bool smart_enabled;             // SMART enabled
    bool health_ok;                 // Overall health (PASSED = true)
    double temperature_celsius;     // Current temperature
    uint64_t reallocated_sectors;   // Reallocated sector count
    uint64_t power_on_hours;        // Total power-on hours
    uint64_t power_cycle_count;     // Power cycle count
    uint64_t pending_sectors;       // Pending sector count
    uint64_t uncorrectable_errors;  // Uncorrectable errors
};
```

---

### File Descriptor Usage Collector
**Header:** `include/kcenon/monitoring/collectors/fd_collector.h`

#### `fd_collector`
Collects file descriptor usage metrics for proactive FD exhaustion detection.

```cpp
class fd_collector {
public:
    fd_collector();

    // Initialize with configuration
    bool initialize(const std::unordered_map<std::string, std::string>& config);

    // Collect FD usage metrics
    std::vector<metric> collect();

    // Get collector name
    std::string get_name() const;

    // Get supported metric types
    std::vector<std::string> get_metric_types() const;

    // Check if collector is healthy
    bool is_healthy() const;

    // Get collection statistics
    std::unordered_map<std::string, double> get_statistics() const;

    // Get last collected metrics
    fd_metrics get_last_metrics() const;

    // Check if FD monitoring is available
    bool is_fd_monitoring_available() const;
};
```

#### `fd_metrics`
Structure containing FD usage data.

```cpp
struct fd_metrics {
    uint64_t fd_used_system;        // Total system FDs in use (Linux only)
    uint64_t fd_max_system;         // System FD limit (Linux only)
    uint64_t fd_used_process;       // Current process FD count
    uint64_t fd_soft_limit;         // Process FD soft limit
    uint64_t fd_hard_limit;         // Process FD hard limit
    double fd_usage_percent;        // Percentage of soft limit used
    bool system_metrics_available;  // Whether system-wide metrics are available
    std::chrono::system_clock::time_point timestamp;
};
```

---

### Inode Usage Collector
**Header:** `include/kcenon/monitoring/collectors/inode_collector.h`

#### `inode_collector`
Collects filesystem inode usage metrics for proactive inode exhaustion detection.

```cpp
class inode_collector {
public:
    inode_collector();

    // Initialize with configuration
    bool initialize(const std::unordered_map<std::string, std::string>& config);

    // Collect inode usage metrics
    std::vector<metric> collect();

    // Get collector name
    std::string get_name() const;

    // Get supported metric types
    std::vector<std::string> get_metric_types() const;

    // Check if collector is healthy
    bool is_healthy() const;

    // Get collection statistics
    std::unordered_map<std::string, double> get_statistics() const;

    // Get last collected metrics
    inode_metrics get_last_metrics() const;

    // Check if inode monitoring is available
    bool is_inode_monitoring_available() const;
};
```

#### `inode_metrics`
Structure containing aggregated inode usage data.

```cpp
struct inode_metrics {
    std::vector<filesystem_inode_info> filesystems;  // Per-filesystem info
    uint64_t total_inodes;           // Sum of all filesystem inodes
    uint64_t total_inodes_used;      // Sum of all used inodes
    uint64_t total_inodes_free;      // Sum of all free inodes
    double average_usage_percent;    // Average usage across filesystems
    double max_usage_percent;        // Maximum usage among filesystems
    std::string max_usage_mount_point; // Mount point with highest usage
    bool metrics_available;          // Whether inode metrics are available
    std::chrono::system_clock::time_point timestamp;
};
```

#### `filesystem_inode_info`
Structure containing per-filesystem inode data.

```cpp
struct filesystem_inode_info {
    std::string mount_point;       // Filesystem mount point (e.g., "/")
    std::string filesystem_type;   // Filesystem type (e.g., "ext4", "apfs")
    std::string device;            // Device path (e.g., "/dev/sda1")
    uint64_t inodes_total;         // Total inodes on filesystem
    uint64_t inodes_used;          // Used inodes
    uint64_t inodes_free;          // Free inodes
    double inodes_usage_percent;   // Percentage of inodes used
};
```

**Platform Support:**
- **Linux**: Uses `statvfs()` and `/proc/mounts` for filesystem enumeration
- **macOS**: Uses `statvfs()` and `getmntinfo()` for filesystem enumeration
- **Windows**: Not applicable (NTFS uses MFT, not traditional inodes)

---

### TCP Connection State Collector
**Header:** `include/kcenon/monitoring/collectors/tcp_state_collector.h`

#### `tcp_state_collector`
Collects TCP connection state metrics for connection leak detection and capacity planning.

```cpp
class tcp_state_collector {
public:
    tcp_state_collector();

    // Initialize with configuration
    bool initialize(const std::unordered_map<std::string, std::string>& config);

    // Collect TCP state metrics
    std::vector<metric> collect();

    // Get collector name
    std::string get_name() const;

    // Get supported metric types
    std::vector<std::string> get_metric_types() const;

    // Check if collector is healthy
    bool is_healthy() const;

    // Get collection statistics
    std::unordered_map<std::string, double> get_statistics() const;

    // Get last collected metrics
    tcp_state_metrics get_last_metrics() const;

    // Check if TCP state monitoring is available
    bool is_tcp_state_monitoring_available() const;
};
```

#### `tcp_state_metrics`
Structure containing aggregated TCP connection state data.

```cpp
struct tcp_state_metrics {
    tcp_state_counts ipv4_counts;       // IPv4 connection counts by state
    tcp_state_counts ipv6_counts;       // IPv6 connection counts by state  
    tcp_state_counts combined_counts;   // Combined IPv4+IPv6 counts
    uint64_t total_connections;         // Total connection count
    bool metrics_available;             // Whether metrics are available
    std::chrono::system_clock::time_point timestamp;
};
```

#### `tcp_state_counts`
Structure containing counts of connections in each TCP state.

```cpp
struct tcp_state_counts {
    uint64_t established;   // ESTABLISHED connections
    uint64_t syn_sent;      // SYN_SENT connections
    uint64_t syn_recv;      // SYN_RECV connections
    uint64_t fin_wait1;     // FIN_WAIT1 connections
    uint64_t fin_wait2;     // FIN_WAIT2 connections
    uint64_t time_wait;     // TIME_WAIT connections (accumulation indicator)
    uint64_t close;         // CLOSE connections
    uint64_t close_wait;    // CLOSE_WAIT connections (leak indicator)
    uint64_t last_ack;      // LAST_ACK connections
    uint64_t listen;        // LISTEN sockets
    uint64_t closing;       // CLOSING connections
    
    uint64_t total() const;
    uint64_t get_count(tcp_state state) const;
    void increment(tcp_state state);
};
```

#### `tcp_state` Enum
```cpp
enum class tcp_state {
    ESTABLISHED = 1,   // Connection established
    SYN_SENT = 2,      // SYN sent, waiting for SYN-ACK
    SYN_RECV = 3,      // SYN received, SYN-ACK sent
    FIN_WAIT1 = 4,     // FIN sent, waiting for ACK or FIN
    FIN_WAIT2 = 5,     // FIN-ACK received, waiting for FIN
    TIME_WAIT = 6,     // Waiting for time to pass (2MSL)
    CLOSE = 7,         // Connection closed
    CLOSE_WAIT = 8,    // Remote closed, waiting for local close
    LAST_ACK = 9,      // FIN sent after CLOSE_WAIT
    LISTEN = 10,       // Listening for connections
    CLOSING = 11,      // Both sides sent FIN
    UNKNOWN = 0        // Unknown state
};
```

**Platform Support:**
- **Linux**: Uses `/proc/net/tcp` and `/proc/net/tcp6` for connection enumeration
- **macOS**: Uses `netstat -an -p tcp` for connection enumeration
- **Windows**: Stub implementation (future: `GetExtendedTcpTable()` API)

**Configuration Options:**
- `enabled`: "true"/"false" (default: true)
- `include_ipv6`: "true"/"false" (default: true)
- `time_wait_warning_threshold`: count (default: 10000)
- `close_wait_warning_threshold`: count (default: 100)

---

### Interrupt Statistics Collector
**Header:** `include/kcenon/monitoring/collectors/interrupt_collector.h`

#### `interrupt_collector`
Collects hardware and software interrupt statistics for performance analysis.

```cpp
class interrupt_collector {
public:
    interrupt_collector();

    // Initialize with configuration
    bool initialize(const std::unordered_map<std::string, std::string>& config);

    // Collect interrupt statistics metrics
    std::vector<metric> collect();

    // Get collector name
    std::string get_name() const;

    // Get supported metric types
    std::vector<std::string> get_metric_types() const;

    // Check if collector is healthy
    bool is_healthy() const;

    // Get collection statistics
    std::unordered_map<std::string, double> get_statistics() const;

    // Get last collected metrics
    interrupt_metrics get_last_metrics() const;

    // Check if interrupt monitoring is available
    bool is_interrupt_monitoring_available() const;
};
```

#### `interrupt_metrics`
Structure containing aggregated interrupt statistics.

```cpp
struct interrupt_metrics {
    uint64_t interrupts_total;          // Total hardware interrupt count
    double interrupts_per_sec;          // Hardware interrupt rate
    uint64_t soft_interrupts_total;     // Total soft interrupts (Linux only)
    double soft_interrupts_per_sec;     // Soft interrupt rate (Linux only)
    std::vector<cpu_interrupt_info> per_cpu;  // Per-CPU breakdown (optional)
    bool metrics_available;             // Whether metrics are available
    bool soft_interrupts_available;     // Whether soft interrupt metrics are available
    std::chrono::system_clock::time_point timestamp;
};
```

#### `cpu_interrupt_info`
Structure containing per-CPU interrupt data.

```cpp
struct cpu_interrupt_info {
    uint32_t cpu_id;             // CPU identifier
    uint64_t interrupt_count;    // Total interrupts on this CPU
    double interrupts_per_sec;   // Interrupt rate on this CPU
};
```

**Platform Support:**
- **Linux**: Uses `/proc/stat` (intr line) for total interrupts and `/proc/softirqs` for soft interrupt breakdown
- **macOS**: Uses `host_statistics64()` for system activity metrics
- **Windows**: Stub implementation (future: PDH performance counters)

**Configuration Options:**
- `enabled`: "true"/"false" (default: true)
- `collect_per_cpu`: "true"/"false" (default: false)
- `collect_soft_interrupts`: "true"/"false" (default: true)

---

### Power Consumption Collector
**Header:** `include/kcenon/monitoring/collectors/power_collector.h`

#### `power_collector`
Collects power consumption metrics for energy efficiency monitoring and cost tracking.

```cpp
class power_collector {
public:
    power_collector();

    // Initialize with configuration
    bool initialize(const std::unordered_map<std::string, std::string>& config);

    // Collect power metrics
    std::vector<metric> collect();

    // Get collector name
    std::string get_name() const;

    // Get supported metric types
    std::vector<std::string> get_metric_types() const;

    // Check if collector is healthy
    bool is_healthy() const;

    // Get collection statistics
    std::unordered_map<std::string, double> get_statistics() const;

    // Get last collected readings
    std::vector<power_reading> get_last_readings() const;

    // Check if power monitoring is available
    bool is_power_available() const;
};
```

#### `power_reading`
Structure containing power consumption data.

```cpp
struct power_reading {
    power_source_info source;      // Power source information
    double power_watts;            // Current power consumption in Watts
    double energy_joules;          // Cumulative energy consumed in Joules
    double power_limit_watts;      // Power limit/TDP in Watts
    double voltage_volts;          // Current voltage in Volts
    double battery_percent;        // Battery charge percentage (0-100)
    double battery_capacity_wh;    // Battery capacity in Watt-hours
    double battery_charge_rate;    // Charging/discharging rate in Watts
    bool is_charging;              // True if battery is charging
    bool is_discharging;           // True if battery is discharging
    bool power_available;          // Whether power metrics are available
    bool battery_available;        // Whether battery metrics are available
    std::chrono::system_clock::time_point timestamp;
};
```

#### `power_source_type` Enum
```cpp
enum class power_source_type {
    unknown,   // Unknown power source
    battery,   // Battery power source
    ac,        // AC adapter / mains power
    usb,       // USB power delivery
    wireless,  // Wireless charging
    cpu,       // CPU power domain (RAPL)
    gpu,       // GPU power domain
    memory,    // Memory/DRAM power domain (RAPL)
    package,   // Processor package power domain (RAPL)
    platform,  // Platform/system power domain
    other      // Other power source type
};
```

**Platform Support:**
- **Linux**: Uses RAPL (`/sys/class/powercap/intel-rapl/`) for CPU/package power and `/sys/class/power_supply/` for battery info
- **macOS**: Uses IOKit SMC for power metrics and IOPowerSources for battery info
- **Windows**: Uses WMI (Win32_Battery) for battery metrics

**Configuration Options:**
- `enabled`: "true"/"false" (default: true)
- `collect_battery`: "true"/"false" (default: true)
- `collect_rapl`: "true"/"false" (default: true)

---

### GPU Metrics Collector
**Header:** `include/kcenon/monitoring/collectors/gpu_collector.h`

#### `gpu_collector`
Collects GPU metrics for NVIDIA, AMD, Intel, and Apple GPUs.

```cpp
class gpu_collector {
public:
    gpu_collector();

    // Initialize with configuration
    bool initialize(const std::unordered_map<std::string, std::string>& config);

    // Collect GPU metrics
    std::vector<metric> collect();

    // Get collector name
    std::string get_name() const;

    // Get supported metric types
    std::vector<std::string> get_metric_types() const;

    // Check if collector is healthy
    bool is_healthy() const;

    // Get collection statistics
    std::unordered_map<std::string, double> get_statistics() const;

    // Get last collected readings
    std::vector<gpu_reading> get_last_readings() const;

    // Check if GPU monitoring is available
    bool is_gpu_available() const;
};
```

#### `gpu_reading`
Structure containing GPU metrics data.

```cpp
struct gpu_reading {
    gpu_device_info device;           // GPU device information
    double utilization_percent;       // GPU compute utilization (0-100)
    uint64_t memory_used_bytes;       // VRAM currently used
    uint64_t memory_total_bytes;      // Total VRAM capacity
    double temperature_celsius;       // GPU temperature
    double power_watts;               // Current power consumption
    double power_limit_watts;         // Power limit/TDP
    double clock_mhz;                 // Current GPU clock speed
    double memory_clock_mhz;          // Current memory clock speed
    double fan_speed_percent;         // Fan speed (0-100)
    bool utilization_available;       // Whether utilization metrics available
    bool memory_available;            // Whether memory metrics available
    bool temperature_available;       // Whether temperature metrics available
    bool power_available;             // Whether power metrics available
    bool clock_available;             // Whether clock metrics available
    bool fan_available;               // Whether fan metrics available
    std::chrono::system_clock::time_point timestamp;
};
```

#### `gpu_device_info`
Structure containing GPU device information.

```cpp
struct gpu_device_info {
    std::string id;              // Unique device identifier (e.g., "gpu0")
    std::string name;            // Human-readable device name
    std::string device_path;     // Platform-specific path
    std::string driver_version;  // Driver version string
    gpu_vendor vendor;           // GPU vendor (nvidia, amd, intel, apple)
    gpu_type type;               // GPU type (discrete, integrated, virtual)
    uint32_t device_index;       // Device index for multi-GPU systems
};
```

#### `gpu_vendor` Enum
```cpp
enum class gpu_vendor {
    unknown,  // Unknown vendor
    nvidia,   // NVIDIA Corporation
    amd,      // Advanced Micro Devices
    intel,    // Intel Corporation
    apple,    // Apple (Apple Silicon GPU)
    other     // Other vendor
};
```

#### `gpu_type` Enum
```cpp
enum class gpu_type {
    unknown,     // Unknown GPU type
    discrete,    // Discrete GPU (dedicated graphics card)
    integrated,  // Integrated GPU (part of CPU/SoC)
    virtual_gpu  // Virtual GPU (cloud/VM)
};
```

**Platform Support:**
- **Linux**: Uses sysfs (`/sys/class/drm/card*/device/`) for GPU enumeration and metrics. AMD GPUs support utilization, memory, and clock via AMDGPU sysfs. All vendors support temperature, power, and fan via hwmon.
- **macOS**: Uses IOKit for GPU enumeration and SMC for GPU temperature. Supports NVIDIA, AMD, Intel, and Apple GPUs with graceful degradation.
- **Windows**: Stub implementation (future: DirectX/WMI/vendor APIs)

**Configuration Options:**
- `enabled`: "true"/"false" (default: true)
- `collect_utilization`: "true"/"false" (default: true)
- `collect_memory`: "true"/"false" (default: true)
- `collect_temperature`: "true"/"false" (default: true)
- `collect_power`: "true"/"false" (default: true)
- `collect_clock`: "true"/"false" (default: true)
- `collect_fan`: "true"/"false" (default: true)

---

### Socket Buffer Collector
**Header:** `include/kcenon/monitoring/collectors/socket_buffer_collector.h`

#### `socket_buffer_collector`
Collects socket buffer (send/receive queue) usage metrics for network bottleneck detection.

```cpp
class socket_buffer_collector {
public:
    socket_buffer_collector();

    // Initialize with configuration
    bool initialize(const std::unordered_map<std::string, std::string>& config);

    // Collect socket buffer metrics
    std::vector<metric> collect();

    // Get collector name
    std::string get_name() const;

    // Get supported metric types
    std::vector<std::string> get_metric_types() const;

    // Check if collector is healthy
    bool is_healthy() const;

    // Get collection statistics
    std::unordered_map<std::string, double> get_statistics() const;

    // Get last collected metrics
    socket_buffer_metrics get_last_metrics() const;

    // Check if socket buffer monitoring is available
    bool is_socket_buffer_monitoring_available() const;
};
```

#### `socket_buffer_metrics`
Structure containing aggregated socket buffer data.

```cpp
struct socket_buffer_metrics {
    uint64_t recv_buffer_bytes;      // Total bytes in receive buffers
    uint64_t send_buffer_bytes;      // Total bytes in send buffers
    uint64_t recv_queue_full_count;  // Count of non-empty recv queues
    uint64_t send_queue_full_count;  // Count of non-empty send queues
    uint64_t socket_memory_bytes;    // Total socket buffer memory used
    uint64_t socket_count;           // Total number of sockets
    uint64_t tcp_socket_count;       // Number of TCP sockets
    uint64_t udp_socket_count;       // Number of UDP sockets
    bool metrics_available;          // Whether metrics are available
    std::chrono::system_clock::time_point timestamp;
};
```

**Platform Support:**
- **Linux**: Uses `/proc/net/tcp` and `/proc/net/tcp6` for tx_queue/rx_queue parsing, `/proc/net/sockstat` for socket memory statistics
- **macOS**: Uses `netstat -m` for mbuf statistics, `netstat -an -p tcp` for queue info, `sysctl kern.ipc` for buffer settings
- **Windows**: Stub implementation (future: `GetTcpStatistics()` API)

**Configuration Options:**
- `enabled`: "true"/"false" (default: true)
- `queue_full_threshold_bytes`: bytes (default: 65536)
- `memory_warning_threshold_bytes`: bytes (default: 104857600 = 100MB)

---

### Security Event Collector
**Header:** `include/kcenon/monitoring/collectors/security_collector.h`

#### `security_collector`
Collects security event metrics for audit and compliance monitoring.

```cpp
class security_collector {
public:
    security_collector();

    // Initialize with configuration
    bool initialize(const std::unordered_map<std::string, std::string>& config);

    // Collect security event metrics
    std::vector<metric> collect();

    // Get collector name
    std::string get_name() const;

    // Get supported metric types
    std::vector<std::string> get_metric_types() const;

    // Check if collector is healthy
    bool is_healthy() const;

    // Get collection statistics
    std::unordered_map<std::string, double> get_statistics() const;

    // Get last collected metrics
    security_metrics get_last_metrics() const;

    // Check if security monitoring is available
    bool is_security_monitoring_available() const;
};
```

#### `security_metrics`
Structure containing aggregated security event data.

```cpp
struct security_metrics {
    security_event_counts event_counts;              // Event counts by type
    uint64_t active_sessions;                        // Current active sessions
    std::vector<security_event> recent_events;       // Recent security events
    double events_per_second;                        // Event rate
    bool metrics_available;                          // Whether metrics are available
    std::chrono::system_clock::time_point timestamp;
};
```

#### `security_event_counts`
Structure containing counts of security events by type.

```cpp
struct security_event_counts {
    uint64_t login_success;      // Successful login count
    uint64_t login_failure;      // Failed login count
    uint64_t logout;             // Logout count
    uint64_t sudo_usage;         // Sudo/privilege escalation count
    uint64_t permission_change;  // Permission change count
    uint64_t account_created;    // Account creation count
    uint64_t account_deleted;    // Account deletion count
    
    uint64_t total() const;
    uint64_t get_count(security_event_type type) const;
    void increment(security_event_type type);
};
```

#### `security_event_type` Enum
```cpp
enum class security_event_type {
    login_success = 1,     // Successful login attempt
    login_failure = 2,     // Failed login attempt
    logout = 3,            // User logout
    sudo_usage = 4,        // Privilege escalation
    permission_change = 5, // Permission/ACL change
    account_created = 6,   // New account creation
    account_deleted = 7,   // Account deletion
    account_modified = 8,  // Account modification
    session_start = 9,     // Session started
    session_end = 10,      // Session ended
    unknown = 0            // Unknown event type
};
```

**Platform Support:**
- **Linux**: Uses `/var/log/auth.log` or `/var/log/secure` for authentication event parsing
- **macOS**: Stub implementation (future: unified logging with `log show`)
- **Windows**: Stub implementation (future: Windows Event Log API)

**Configuration Options:**
- `enabled`: "true"/"false" (default: true)
- `mask_pii`: "true"/"false" (default: false) - Mask usernames for privacy
- `max_recent_events`: count (default: 100)
- `login_failure_rate_limit`: events/sec (default: 1000)

---

### Container Metrics Collector
**Header:** `include/kcenon/monitoring/collectors/container_collector.h`

#### `container_collector`
Collects container-level metrics from Docker/Podman containers via cgroups.

```cpp
class container_collector {
public:
    container_collector();

    // Initialize with configuration
    bool initialize(const std::unordered_map<std::string, std::string>& config);

    // Collect container metrics
    std::vector<metric> collect();

    // Get collector name
    std::string get_name() const;

    // Get supported metric types
    std::vector<std::string> get_metric_types() const;

    // Check if collector is healthy
    bool is_healthy() const;

    // Get collection statistics
    std::unordered_map<std::string, double> get_statistics() const;

    // Get last collected metrics
    std::vector<container_metrics> get_last_metrics() const;

    // Check if running in container environment
    bool is_container_environment() const;
};
```

#### `container_metrics`
Structure containing per-container resource usage data.

```cpp
struct container_metrics {
    std::string container_id;         // Short container ID
    std::string container_name;       // Container name (Docker only)
    std::string image_name;           // Image name (Docker only)
    double cpu_usage_percent;         // CPU utilization percentage
    uint64_t cpu_usage_ns;            // Total CPU time in nanoseconds
    uint64_t memory_usage_bytes;      // Current memory usage
    uint64_t memory_limit_bytes;      // Memory limit
    double memory_usage_percent;      // Memory usage percentage
    uint64_t network_rx_bytes;        // Total bytes received
    uint64_t network_tx_bytes;        // Total bytes transmitted
    uint64_t blkio_read_bytes;        // Total bytes read from disk
    uint64_t blkio_write_bytes;       // Total bytes written to disk
    uint64_t pids_current;            // Current number of processes
    uint64_t pids_limit;              // Process limit (0 = unlimited)
    std::chrono::system_clock::time_point timestamp;
};
```

#### `cgroup_version` Enum
```cpp
enum class cgroup_version {
    none = 0,  // Not in a cgroup or not Linux
    v1 = 1,    // Legacy cgroups v1
    v2 = 2     // Unified cgroups v2 hierarchy
};
```

**Platform Support:**
- **Linux**: Uses cgroups v1/v2 and `/proc` filesystem for container detection and metrics
- **macOS**: Stub implementation (containers typically run in Linux VMs)
- **Windows**: Stub implementation (containers typically run in Linux VMs)

**Configuration Options:**
- `enabled`: "true"/"false" (default: true)
- `collect_network_metrics`: "true"/"false" (default: true)
- `collect_blkio_metrics`: "true"/"false" (default: true)

---

### Hardware Temperature Collector
**Header:** `include/kcenon/monitoring/collectors/temperature_collector.h`

#### `temperature_collector`
Collects hardware temperature data from thermal sensors.

```cpp
class temperature_collector {
public:
    temperature_collector();

    // Initialize with configuration
    bool initialize(const std::unordered_map<std::string, std::string>& config);

    // Collect temperature metrics
    std::vector<metric> collect();

    // Get collector name
    std::string get_name() const;

    // Get supported metric types
    std::vector<std::string> get_metric_types() const;

    // Check if collector is healthy
    bool is_healthy() const;

    // Get collection statistics
    std::unordered_map<std::string, double> get_statistics() const;

    // Get last collected readings
    std::vector<temperature_reading> get_last_readings() const;

    // Check if thermal monitoring is available
    bool is_thermal_available() const;
};
```

#### `temperature_reading`
Structure containing temperature sensor data.

```cpp
struct temperature_reading {
    temperature_sensor_info sensor;          // Sensor information
    double temperature_celsius;              // Current temperature in Celsius
    double critical_threshold_celsius;       // Critical temperature threshold
    double warning_threshold_celsius;        // Warning threshold
    bool thresholds_available;               // Whether thresholds are available
    bool is_critical;                        // True if exceeds critical threshold
    bool is_warning;                         // True if exceeds warning threshold
    std::chrono::system_clock::time_point timestamp;
};
```

#### `temperature_sensor_info`
Structure containing sensor identification.

```cpp
struct temperature_sensor_info {
    std::string id;         // Unique sensor identifier
    std::string name;       // Human-readable sensor name
    std::string zone_path;  // Platform-specific path
    sensor_type type;       // Sensor type classification
};
```

#### `sensor_type` Enum
```cpp
enum class sensor_type {
    unknown,      // Unknown sensor type
    cpu,          // CPU temperature sensor
    gpu,          // GPU temperature sensor
    motherboard,  // Motherboard/chipset sensor
    storage,      // Storage device sensor
    ambient,      // Ambient/case temperature
    other         // Other sensor type
};
```

**Platform Support:**
- **Linux**: Uses `/sys/class/thermal/thermal_zone*` and hwmon sysfs interfaces
- **macOS**: Uses IOKit SMC (System Management Controller)
- **Windows**: Uses WMI (MSAcpi_ThermalZoneTemperature) - stub implementation

**Configuration Options:**
- `enabled`: "true"/"false" (default: true)
- `collect_thresholds`: "true"/"false" (default: true)
- `collect_warnings`: "true"/"false" (default: true)

---

### Context Switch Statistics Monitoring

#### Overview

The `context_switch_collector` provides comprehensive context switch statistics monitoring to track CPU scheduling activity and identify scheduling overhead and contention.

#### Classes

**`context_switch_collector`**: Main collector class for context switch metrics.

| Method | Description | Return Type |
|--------|-------------|-------------|
| `initialize(config)` | Initialize with configuration map | `bool` |
| `collect()` | Collect context switch metrics | `std::vector<metric>` |
| `get_name()` | Get collector name | `std::string` |
| `get_metric_types()` | Get supported metric types | `std::vector<std::string>` |
| `is_healthy()` | Check if collector is operational | `bool` |
| `get_statistics()` | Get collector statistics | `std::unordered_map<std::string, double>` |
| `get_last_metrics()` | Get last collected metrics | `context_switch_metrics` |
| `is_context_switch_monitoring_available()` | Check availability | `bool` |

**`context_switch_info_collector`**: Low-level platform-specific collector.

| Method | Description | Return Type |
|--------|-------------|-------------|
| `is_context_switch_monitoring_available()` | Check if monitoring is available | `bool` |
| `collect_metrics()` | Collect raw context switch metrics | `context_switch_metrics` |

#### Data Structures

**`context_switch_metrics`**:
```cpp
struct context_switch_metrics {
    uint64_t system_context_switches_total;  // Total system context switches
    double context_switches_per_sec;         // Context switch rate
    process_context_switch_info process_info; // Process-level info
    bool metrics_available;                   // Whether metrics are available
    bool rate_available;                      // Whether rate calculation is available
    std::chrono::system_clock::time_point timestamp;
};
```

**`process_context_switch_info`**:
```cpp
struct process_context_switch_info {
    uint64_t voluntary_switches;     // Voluntary context switches (I/O wait, sleep)
    uint64_t nonvoluntary_switches;  // Involuntary context switches (preemption)
    uint64_t total_switches;         // Total process context switches
};
```

**Metrics Collected**:

| Metric | Description | Unit |
|--------|-------------|------|
| **context_switches_total** | Total system-wide context switches | Count |
| **context_switches_per_sec** | Context switch rate | Switches/sec |
| **voluntary_context_switches** | Process voluntary switches | Count |
| **nonvoluntary_context_switches** | Process involuntary switches | Count |
| **process_context_switches_total** | Total process context switches | Count |

**Configuration Options**:

| Option | Default | Description |
|--------|---------|-------------|
| `enabled` | `true` | Enable/disable collection |
| `collect_process_metrics` | `true` | Collect process-level metrics |
| `rate_warning_threshold` | `100000` | High rate warning threshold (switches/sec) |

**Platform Support**:
- **Linux**: Uses `/proc/stat` (ctxt field) and `/proc/self/status`
- **macOS**: Uses `task_info()` with `TASK_EVENTS_INFO`
- **Windows**: Stub implementation (returns unavailable metrics)

---

### System Uptime Monitoring

#### Overview

The `uptime_collector` provides system uptime monitoring to track boot time, uptime duration, and system availability for SLA compliance and stability analysis.

#### Classes

**`uptime_collector`**: Main collector class for uptime metrics.

| Method | Description | Return Type |
|--------|-------------|-------------|
| `initialize(config)` | Initialize with configuration map | `bool` |
| `collect()` | Collect uptime metrics | `std::vector<metric>` |
| `get_name()` | Get collector name | `std::string` |
| `get_metric_types()` | Get supported metric types | `std::vector<std::string>` |
| `is_healthy()` | Check if collector is operational | `bool` |
| `get_statistics()` | Get collector statistics | `std::unordered_map<std::string, double>` |
| `get_last_metrics()` | Get last collected metrics | `uptime_metrics` |
| `is_uptime_monitoring_available()` | Check availability | `bool` |

**`uptime_info_collector`**: Low-level platform-specific collector.

| Method | Description | Return Type |
|--------|-------------|-------------|
| `is_uptime_monitoring_available()` | Check if monitoring is available | `bool` |
| `collect_metrics()` | Collect raw uptime metrics | `uptime_metrics` |

#### Data Structures

**`uptime_metrics`**:
```cpp
struct uptime_metrics {
    double uptime_seconds;         // Time since boot in seconds
    int64_t boot_timestamp;        // Unix timestamp of last boot
    double idle_seconds;           // Total idle time (Linux only)
    bool metrics_available;        // Whether metrics are available
    std::chrono::system_clock::time_point timestamp;
};
```

**Metrics Collected**:

| Metric | Description | Unit |
|--------|-------------|------|
| **system_uptime_seconds** | Time since system boot | Seconds |
| **system_boot_timestamp** | Unix timestamp of last boot | Timestamp |
| **system_idle_seconds** | Total system idle time (Linux only) | Seconds |

**Configuration Options**:

| Option | Default | Description |
|--------|---------|-------------|
| `enabled` | `true` | Enable/disable collection |
| `collect_idle_time` | `true` | Collect idle time metrics (Linux only) |

**Platform Support**:
- **Linux**: Uses `/proc/uptime` for uptime and idle seconds
- **macOS**: Uses `sysctl(KERN_BOOTTIME)` for boot timestamp
- **Windows**: Uses `GetTickCount64()` for uptime in milliseconds

---

### Battery Status Monitoring

#### Overview

The `battery_collector` provides battery status monitoring for portable devices (laptops, mobile) with cross-platform support. It collects battery level, charging status, health information, and time estimates.

#### Classes

**`battery_collector`**: Main collector class for battery metrics.

| Method | Description | Return Type |
|--------|-------------|-------------|
| `initialize(config)` | Initialize with configuration map | `bool` |
| `collect()` | Collect battery metrics | `std::vector<metric>` |
| `get_name()` | Get collector name | `std::string` |
| `get_metric_types()` | Get supported metric types | `std::vector<std::string>` |
| `is_healthy()` | Check if collector is operational | `bool` |
| `get_statistics()` | Get collector statistics | `std::unordered_map<std::string, double>` |
| `get_last_readings()` | Get last collected readings | `std::vector<battery_reading>` |
| `is_battery_available()` | Check battery availability | `bool` |

**`battery_info_collector`**: Low-level platform-specific collector.

| Method | Description | Return Type |
|--------|-------------|-------------|
| `is_battery_available()` | Check if battery monitoring is available | `bool` |
| `enumerate_batteries()` | List all available batteries | `std::vector<battery_info>` |
| `read_battery(battery)` | Read specific battery status | `battery_reading` |
| `read_all_batteries()` | Read all batteries | `std::vector<battery_reading>` |

#### Data Structures

**`battery_status`** enum:
```cpp
enum class battery_status {
    unknown,      // Unknown status
    charging,     // Battery is charging
    discharging,  // Battery is discharging
    not_charging, // Plugged in but not charging
    full          // Battery is fully charged
};
```

**`battery_info`**:
```cpp
struct battery_info {
    std::string id;           // Unique battery identifier
    std::string name;         // Human-readable name
    std::string path;         // Platform-specific path
    std::string manufacturer; // Battery manufacturer
    std::string model;        // Battery model name
    std::string serial;       // Serial number
    std::string technology;   // Battery technology (Li-ion, etc.)
};
```

**`battery_reading`**:
```cpp
struct battery_reading {
    battery_info info;                    // Battery information
    double level_percent;                 // Current charge (0-100)
    battery_status status;                // Charging status
    bool is_charging;                     // Is charging
    bool ac_connected;                    // AC power connected
    int64_t time_to_empty_seconds;        // Estimated time to empty
    int64_t time_to_full_seconds;         // Estimated time to full
    double design_capacity_wh;            // Original design capacity
    double full_charge_capacity_wh;       // Current full charge capacity
    double current_capacity_wh;           // Current energy stored
    double health_percent;                // Battery health percentage
    double voltage_volts;                 // Current voltage
    double current_amps;                  // Current in Amps
    double power_watts;                   // Current power
    double temperature_celsius;           // Battery temperature
    bool temperature_available;           // Whether temperature is available
    int64_t cycle_count;                  // Battery charge cycles
    bool battery_present;                 // Whether battery is present
    bool metrics_available;               // Whether metrics are available
    std::chrono::system_clock::time_point timestamp;
};
```

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

**Configuration Options**:

| Option | Default | Description |
|--------|---------|-------------|
| `enabled` | `true` | Enable/disable collection |
| `collect_health` | `true` | Collect health metrics |
| `collect_thermal` | `true` | Collect temperature metrics |

**Platform Support**:
- **Linux**: Uses `/sys/class/power_supply/BAT*` sysfs files
- **macOS**: Uses IOKit (AppleSmartBattery)
- **Windows**: Uses GetSystemPowerStatus() and WMI Win32_Battery

---

## Virtualization Monitoring

### VM Metrics Collector
**Header:** `include/kcenon/monitoring/collectors/vm_collector.h`

#### `vm_collector`
Collects virtualization environment metrics.

```cpp
class vm_collector {
public:
    vm_collector();
    bool initialize(const std::unordered_map<std::string, std::string>& config);
    std::vector<metric> collect();
    // ... standard interface methods
};
```

#### `vm_metrics`
Structure containing virtualization data.

```cpp
struct vm_metrics {
    bool is_virtualized;             // True if running in a VM
    vm_type type;                    // Detected hypervisor type
    double guest_cpu_steal_time;     // % CPU time stolen by hypervisor
    std::string hypervisor_vendor;   // Vendor string
};
```

**Platform Support:**
- **Linux**: Uses `/sys/class/dmi` and `/proc/cpuinfo`.
- **macOS**: Uses `sysctl` (`machdep.cpu.features`, `kern.hv_vmm_present`).
- **Windows**: Stub implementation.

---

## See Also

- [API_REFERENCE.md](API_REFERENCE.md) - API Reference index
- [API_REFERENCE_CORE.md](API_REFERENCE_CORE.md) - Core components and performance monitor
- [API_REFERENCE_ALERTS_EXPORT.md](API_REFERENCE_ALERTS_EXPORT.md) - Alerts, exporters, tracing, reliability, storage APIs
- [FEATURES_COLLECTORS.md](FEATURES_COLLECTORS.md) - Feature-level collector documentation
