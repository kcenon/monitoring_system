# Monitoring System API Reference

> **Language:** **English** | [한국어](API_REFERENCE_KO.md)

**Phase 4 - Current Implementation Status**

This document describes the **actually implemented** APIs and interfaces in the monitoring system Phase 4. Items marked with *(Stub Implementation)* provide functional interfaces but with simplified implementations as foundation for future development.

## Table of Contents

1. [Core Components](#core-components) ✅ **Implemented**
2. [Monitoring Interfaces](#monitoring-interfaces) ✅ **Implemented**
3. [Performance Monitoring](#performance-monitoring) ✅ **Implemented**
4. [Distributed Tracing](#distributed-tracing) ⚠️ **Stub Implementation**
5. [Storage Backends](#storage-backends) ⚠️ **Stub Implementation**
6. [Reliability Features](#reliability-features) ⚠️ **Stub Implementation**
7. [Test Coverage](#test-coverage) ✅ **37 Tests Passing**

---

## Core Components

### Result Types ✅ **Fully Implemented**
**Header:** `include/kcenon/monitoring/core/result_types.h`

#### `result<T>`
A monadic result type for error handling without exceptions. **Fully tested with 13 passing tests.**

```cpp
template<typename T>
class result {
public:
    // Constructors
    result(const T& value);
    result(T&& value);
    result(const error_info& error);

    // Check if result contains a value
    bool has_value() const;
    bool is_ok() const;
    bool is_error() const;
    explicit operator bool() const;

    // Access the value
    T& value();
    const T& value() const;
    T value_or(const T& default_value) const;

    // Access the error
    error_info get_error() const;

    // Monadic operations
    template<typename F>
    auto map(F&& f) const;

    template<typename F>
    auto and_then(F&& f) const;
};
```

#### `result_void`
Specialized result type for operations that don't return values.

```cpp
class result_void {
public:
    static result_void success();
    static result_void error(const error_info& error);

    bool is_ok() const;
    bool is_error() const;
    error_info get_error() const;
};
```

**Usage Example:**
```cpp
result<int> divide(int a, int b) {
    if (b == 0) {
        return result<int>(error_info{
            .code = monitoring_error_code::invalid_argument,
            .message = "Division by zero"
        });
    }
    return result<int>(a / b);
}

// Chain operations
auto result = divide(10, 2)
    .map([](int x) { return x * 2; })
    .and_then([](int x) { return divide(x, 3); });
```

### Thread Context ✅ **Fully Implemented**
**Header:** `include/kcenon/monitoring/context/thread_context.h`

#### `thread_context`
Manages thread-local context for correlation and tracing. **Fully tested with 6 passing tests.**

```cpp
class thread_context {
public:
    // Get current thread context
    static context_metadata& current();

    // Create new context with optional request ID
    static context_metadata& create(const std::string& request_id = "");

    // Clear current context
    static void clear();

    // Generate unique IDs
    static std::string generate_request_id();
    static std::string generate_correlation_id();

    // Check if context exists
    static bool has_current();
};
```

#### `context_metadata`
Thread-local context information for tracing and correlation.

```cpp
struct context_metadata {
    std::string request_id;
    std::string correlation_id;
    std::string trace_id;
    std::string span_id;
    std::chrono::system_clock::time_point created_at;
    std::unordered_map<std::string, std::string> baggage;
};
```

### Dependency Injection Container ✅ **Fully Implemented**
**Header:** `include/kcenon/monitoring/di/di_container.h`

#### `di_container`
Lightweight dependency injection container for managing service instances. **Fully tested with 9 passing tests.**

```cpp
class di_container {
public:
    // Service registration
    template<typename T>
    void register_transient(std::function<std::shared_ptr<T>()> factory);

    template<typename T>
    void register_singleton(std::function<std::shared_ptr<T>()> factory);

    template<typename T>
    void register_singleton_instance(std::shared_ptr<T> instance);

    // Named service registration
    template<typename T>
    void register_named(const std::string& name,
                       std::function<std::shared_ptr<T>()> factory);

    // Service resolution
    template<typename T>
    std::shared_ptr<T> resolve();

    template<typename T>
    std::shared_ptr<T> resolve_named(const std::string& name);

    // Utilities
    void clear();
    bool has_service(const std::string& type_name) const;
    size_t service_count() const;
};
```

**Usage Example:**
```cpp
di_container container;

// Register services
container.register_singleton<database_service>([]() {
    return std::make_shared<sqlite_database>();
});

container.register_transient<user_service>([&]() {
    auto db = container.resolve<database_service>();
    return std::make_shared<user_service>(db);
});

// Resolve services
auto user_svc = container.resolve<user_service>();
```

---

## Monitoring Interfaces

### Monitoring Interface
**Header:** `sources/monitoring/interfaces/monitoring_interface.h`

#### `metrics_collector`
Base interface for all metric collectors.

```cpp
class metrics_collector {
public:
    virtual std::string get_name() const = 0;
    virtual bool is_enabled() const = 0;
    virtual result_void set_enabled(bool enable) = 0;
    virtual result_void initialize() = 0;
    virtual result_void cleanup() = 0;
    virtual result<metrics_snapshot> collect() = 0;
};
```

#### `monitorable`
Interface for objects that can be monitored.

```cpp
class monitorable {
public:
    virtual std::string get_name() const = 0;
    virtual result<metrics_snapshot> get_metrics() const = 0;
    virtual result_void reset_metrics() = 0;
    virtual result<std::string> get_status() const = 0;
};
```

---

## Performance Monitoring

### Performance Monitor
**Header:** `sources/monitoring/performance/performance_monitor.h`

#### `performance_monitor`
Monitors system and application performance metrics.

```cpp
class performance_monitor : public metrics_collector {
public:
    explicit performance_monitor(const std::string& name = "performance_monitor");
    
    // Create scoped timer
    scoped_timer time_operation(const std::string& operation_name);
    
    // Get profiler
    performance_profiler& get_profiler();
    
    // Get system monitor
    system_monitor& get_system_monitor();
    
    // Set thresholds
    void set_cpu_threshold(double threshold);
    void set_memory_threshold(double threshold);
    void set_latency_threshold(std::chrono::milliseconds threshold);
};
```

#### `scoped_timer`
RAII timer for measuring operation duration.

```cpp
class scoped_timer {
public:
    scoped_timer(performance_profiler* profiler, const std::string& operation_name);
    ~scoped_timer();
    
    void mark_failed();
    void complete();
    std::chrono::nanoseconds elapsed() const;
};
```

### Adaptive Optimizer
**Header:** `sources/monitoring/performance/adaptive_optimizer.h`

#### `adaptive_optimizer`
Dynamically optimizes monitoring parameters based on system load.

```cpp
class adaptive_optimizer {
public:
    struct optimization_config {
        double cpu_threshold = 80.0;
        double memory_threshold = 90.0;
        double target_overhead = 5.0;
        std::chrono::seconds adaptation_interval{60};
    };
    
    explicit adaptive_optimizer(const optimization_config& config = {});
    
    result<bool> start();
    result<bool> stop();
    result<optimization_decision> analyze_and_optimize();
    result<bool> apply_optimization(const optimization_decision& decision);
};
```

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

## Health Monitoring


### Health Monitor
**Header:** `sources/monitoring/health/health_monitor.h`

#### `health_monitor`
Manages health checks and service dependencies.

```cpp
class health_monitor {
public:
    health_monitor(const health_monitor_config& config = {});
    
    // Register health checks
    result<bool> register_check(const std::string& name,
                                std::shared_ptr<health_check> check);
    
    // Add dependencies
    result<bool> add_dependency(const std::string& dependent,
                                const std::string& dependency);
    
    // Perform checks
    result<health_check_result> check(const std::string& name);
    std::unordered_map<std::string, health_check_result> check_all();
    
    // Get status
    health_status get_overall_status() const;
    
    // Recovery handlers
    void register_recovery_handler(const std::string& check_name,
                                  std::function<bool()> handler);
};
```

#### `health_check`
Abstract base class for health checks.

```cpp
class health_check {
public:
    virtual std::string get_name() const = 0;
    virtual health_check_type get_type() const = 0;
    virtual health_check_result check() = 0;
    virtual std::chrono::milliseconds get_timeout() const;
    virtual bool is_critical() const;
};
```

#### Health Check Builder
```cpp
health_check_builder builder;
auto check = builder
    .with_name("database_check")
    .with_type(health_check_type::readiness)
    .with_check([]() { 
        // Check database connection
        return health_check_result::healthy("Database connected");
    })
    .with_timeout(5s)
    .critical(true)
    .build();
```

---

## Distributed Tracing

### Distributed Tracer
**Header:** `sources/monitoring/tracing/distributed_tracer.h`

#### `distributed_tracer`
Manages distributed traces across services.

```cpp
class distributed_tracer {
public:
    // Start spans
    result<std::shared_ptr<trace_span>> start_span(
        const std::string& operation_name,
        const std::string& service_name = "monitoring_system");
    
    result<std::shared_ptr<trace_span>> start_child_span(
        const trace_span& parent,
        const std::string& operation_name);
    
    // Context propagation
    trace_context extract_context(const trace_span& span) const;
    
    template<typename Carrier>
    void inject_context(const trace_context& context, Carrier& carrier);
    
    template<typename Carrier>
    result<trace_context> extract_context_from_carrier(const Carrier& carrier);
    
    // Finish span
    result<bool> finish_span(std::shared_ptr<trace_span> span);
};
```

#### `trace_span`
Represents a unit of work in distributed tracing.

```cpp
struct trace_span {
    std::string trace_id;
    std::string span_id;
    std::string parent_span_id;
    std::string operation_name;
    std::string service_name;
    
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point end_time;
    
    std::unordered_map<std::string, std::string> tags;
    std::unordered_map<std::string, std::string> baggage;
    
    enum class status_code { unset, ok, error };
    status_code status;
    std::string status_message;
};
```

#### Scoped Span Macro
```cpp
// Automatically creates and finishes a span
TRACE_SPAN("database_query");

// Create child span
TRACE_CHILD_SPAN(parent_span, "nested_operation");
```

---

## Storage Backends

### Storage Backend Interface
**Header:** `sources/monitoring/storage/storage_backends.h`

#### `storage_backend`
Abstract base class for all storage implementations.

```cpp
class storage_backend {
public:
    virtual result<bool> initialize() = 0;
    virtual result<bool> write(const std::string& key, const std::string& value) = 0;
    virtual result<std::string> read(const std::string& key) = 0;
    virtual result<bool> remove(const std::string& key) = 0;
    virtual result<std::vector<std::string>> list(const std::string& prefix = "") = 0;
    virtual result<bool> flush() = 0;
    virtual storage_backend_type get_type() const = 0;
};
```

### Available Backends

#### File Storage Backends
- `file_storage_backend` - JSON file storage
- `binary_storage_backend` - Binary file storage
- `csv_storage_backend` - CSV file storage

#### Database Backends
- `sqlite_storage_backend` - SQLite database
- `postgres_storage_backend` - PostgreSQL database
- `mysql_storage_backend` - MySQL database

#### Cloud Storage Backends
- `s3_storage_backend` - Amazon S3
- `gcs_storage_backend` - Google Cloud Storage
- `azure_storage_backend` - Azure Blob Storage

#### Memory Backend
- `memory_buffer_backend` - In-memory storage

### Storage Factory
```cpp
auto storage = storage_factory::create(storage_backend_type::sqlite, config);
```

---

## Stream Processing

### Stream Aggregator
**Header:** `sources/monitoring/stream/stream_aggregator.h`

#### `stream_aggregator`
Performs real-time aggregation on metric streams.

```cpp
class stream_aggregator {
public:
    // Configure aggregation
    result<bool> add_aggregation(const std::string& metric_name,
                                 aggregation_type type,
                                 std::chrono::seconds window);
    
    // Process values
    result<bool> add_value(const std::string& metric_name, double value);
    
    // Get results
    result<aggregation_result> get_aggregation(const std::string& metric_name);
    
    // Windowing
    result<bool> set_window_size(std::chrono::seconds size);
    result<bool> set_sliding_interval(std::chrono::seconds interval);
};
```

#### Aggregation Types
```cpp
enum class aggregation_type {
    sum,
    average,
    min,
    max,
    count,
    percentile_50,
    percentile_95,
    percentile_99,
    stddev,
    rate
};
```

### Buffering Strategies
**Header:** `sources/monitoring/stream/buffering_strategies.h`

#### `buffering_strategy`
Base class for different buffering strategies.

```cpp
class buffering_strategy {
public:
    virtual result<bool> add(const metric_data& data) = 0;
    virtual result<std::vector<metric_data>> get_batch() = 0;
    virtual bool should_flush() const = 0;
    virtual result<bool> flush() = 0;
};
```

Available Strategies:
- `time_based_buffer` - Flush after time interval
- `size_based_buffer` - Flush after size threshold
- `adaptive_buffer` - Dynamic buffering based on load

---

## Reliability Features

### Circuit Breaker
**Header:** `sources/monitoring/reliability/circuit_breaker.h`

#### `circuit_breaker<T>`
Prevents cascading failures by breaking circuits to failing services.

```cpp
template<typename T>
class circuit_breaker {
public:
    circuit_breaker(std::string name, circuit_breaker_config config = {});
    
    // Execute with circuit breaker protection
    result<T> execute(std::function<result<T>()> operation,
                     std::function<result<T>()> fallback = nullptr);
    
    // Manual control
    void open();
    void close();
    void half_open();
    
    // Get state
    circuit_state get_state() const;
    circuit_breaker_metrics get_metrics() const;
};
```

#### Circuit Breaker Configuration
```cpp
struct circuit_breaker_config {
    std::size_t failure_threshold = 5;
    double failure_ratio = 0.5;
    std::chrono::milliseconds timeout{5000};
    std::chrono::milliseconds reset_timeout{60000};
    std::size_t success_threshold = 3;
};
```

### Retry Policy
**Header:** `sources/monitoring/reliability/retry_policy.h`

#### `retry_policy<T>`
Implements retry logic with various strategies.

```cpp
template<typename T>
class retry_policy {
public:
    retry_policy(retry_config config = {});
    
    // Execute with retry
    result<T> execute(std::function<result<T>()> operation);
    
    // Execute async with retry
    std::future<result<T>> execute_async(std::function<result<T>()> operation);
};
```

#### Retry Strategies
```cpp
enum class retry_strategy {
    fixed_delay,        // Fixed delay between retries
    exponential_backoff,// Exponential increase in delay
    linear_backoff,     // Linear increase in delay
    fibonacci_backoff,  // Fibonacci sequence delays
    random_jitter      // Random delay with jitter
};
```

### Error Boundaries
**Header:** `sources/monitoring/reliability/error_boundaries.h`

#### `error_boundary`
Isolates errors to prevent system-wide failures.

```cpp
class error_boundary {
public:
    error_boundary(const std::string& name,
                  error_boundary_config config = {});
    
    // Execute within boundary
    template<typename T>
    result<T> execute(std::function<result<T>()> operation);
    
    // Set error handler
    void set_error_handler(std::function<void(const error_info&)> handler);
    
    // Get statistics
    error_boundary_stats get_stats() const;
};
```

---

## OpenTelemetry Integration

### OpenTelemetry Adapter
**Header:** `sources/monitoring/adapters/opentelemetry_adapter.h`

#### `opentelemetry_adapter`
Bridges monitoring system with OpenTelemetry.

```cpp
class opentelemetry_adapter {
public:
    opentelemetry_adapter(const otel_config& config = {});
    
    // Convert to OpenTelemetry formats
    result<otel_span> convert_span(const trace_span& span);
    result<otel_metric> convert_metric(const metric_data& metric);
    result<otel_log> convert_log(const log_entry& log);
    
    // Export to OpenTelemetry
    result<bool> export_traces(const std::vector<trace_span>& spans);
    result<bool> export_metrics(const std::vector<metric_data>& metrics);
    result<bool> export_logs(const std::vector<log_entry>& logs);
};
```

### Trace Exporters
**Header:** `sources/monitoring/exporters/trace_exporters.h`

Available Exporters:
- `jaeger_exporter` - Export to Jaeger
- `zipkin_exporter` - Export to Zipkin
- `otlp_exporter` - OTLP protocol exporter
- `console_exporter` - Console output for debugging

### Metric Exporters
**Header:** `sources/monitoring/exporters/metric_exporters.h`

Available Exporters:
- `prometheus_exporter` - Prometheus format
- `statsd_exporter` - StatsD protocol
- `influxdb_exporter` - InfluxDB line protocol
- `graphite_exporter` - Graphite plaintext protocol

---

## Usage Examples

### Basic Monitoring Setup
```cpp
// Create monitoring instance
monitoring_builder builder;
auto monitoring = builder
    .with_history_size(1000)
    .with_collection_interval(1s)
    .add_collector(std::make_unique<performance_monitor>())
    .with_storage(std::make_unique<sqlite_storage_backend>(config))
    .enable_compression(true)
    .build();

// Start monitoring
monitoring.value()->start();
```

### Health Check Setup
```cpp
// Create health monitor
health_monitor monitor;

// Register health checks
monitor.register_check("database", 
    health_check_builder()
        .with_name("database_check")
        .with_type(health_check_type::readiness)
        .with_check([]() { return check_database(); })
        .build()
);

// Add dependencies
monitor.add_dependency("api", "database");

// Start monitoring
monitor.start();
```

### Distributed Tracing
```cpp
// Start a trace
distributed_tracer& tracer = global_tracer();
auto span = tracer.start_span("process_request");

// Add tags
span.value()->tags["user_id"] = "12345";
span.value()->tags["endpoint"] = "/api/users";

// Create child span
auto child_span = tracer.start_child_span(*span.value(), "database_query");

// Finish spans
tracer.finish_span(child_span.value());
tracer.finish_span(span.value());
```

### Circuit Breaker Usage
```cpp
circuit_breaker_config config;
config.failure_threshold = 5;
config.reset_timeout = 30s;

circuit_breaker<std::string> breaker("external_api", config);

auto result = breaker.execute(
    []() { return call_external_api(); },
    []() { return result<std::string>::success("fallback_value"); }
);
```

---

## Error Codes

Common error codes used throughout the system:

```cpp
enum class monitoring_error_code {
    success = 0,
    unknown_error,
    invalid_argument,
    out_of_range,
    not_found,
    already_exists,
    permission_denied,
    resource_exhausted,
    operation_cancelled,
    operation_timeout,
    not_implemented,
    internal_error,
    unavailable,
    data_loss,
    unauthenticated,
    circuit_breaker_open,
    retry_exhausted,
    validation_failed,
    initialization_failed,
    configuration_error
};
```

---

## Thread Safety

All public interfaces in the monitoring system are thread-safe unless explicitly documented otherwise. Internal synchronization uses:
- `std::mutex` for exclusive access
- `std::shared_mutex` for read-write locks
- `std::atomic` for lock-free operations
- Thread-local storage for context management

---

## Performance Considerations

1. **Metric Collection**: Designed for minimal overhead (<5% CPU)
2. **Storage**: Async writes with batching for efficiency
3. **Tracing**: Sampling support to reduce overhead
4. **Health Checks**: Cached results with configurable TTL
5. **Circuit Breakers**: Lock-free state transitions
6. **Stream Processing**: Zero-copy where possible

---

## Best Practices

1. **Error Handling**: Always check `result<T>` return values
2. **Resource Management**: Use RAII patterns (scoped_timer, scoped_span)
3. **Configuration**: Validate configs before use
4. **Monitoring**: Start with conservative thresholds, tune based on metrics
5. **Tracing**: Use sampling in production to control overhead
6. **Health Checks**: Keep checks lightweight and fast
7. **Storage**: Choose backend based on durability vs performance needs

---

## Migration Guide

### From Direct Metrics to Collectors
```cpp
// Old way
metrics.record("latency", 150.0);

// New way
auto timer = perf_monitor.time_operation("process_request");
// ... operation ...
timer.complete();
```

### From Callbacks to Health Checks
```cpp
// Old way
register_health_callback([]() { return check_health(); });

// New way
monitor.register_check("service",
    std::make_shared<functional_health_check>(
        "service_check",
        health_check_type::liveness,
        []() { return check_health(); }
    )
);
```

---

## Version Compatibility

- C++20 required (C++17 no longer supported)
- Thread support required
- C++20 Concepts used for type-safe APIs with clear error messages
- Compatible with: GCC 10+, Clang 10+, MSVC 2019 16.3+

## C++20 Concepts

The monitoring system uses C++20 Concepts for compile-time type validation with clear, actionable error messages.

### Available Concepts

#### Event Bus Concepts (`interfaces/event_bus_interface.h`)
```cpp
namespace kcenon::monitoring::concepts {
    // A type that can be used as an event (class type, copy-constructible)
    template <typename T>
    concept EventType = std::is_class_v<T> && std::is_copy_constructible_v<T>;

    // A callable that handles events (invocable with const E&, returns void)
    template <typename H, typename E>
    concept EventHandler = std::invocable<H, const E&> &&
        std::is_void_v<std::invoke_result_t<H, const E&>>;

    // A callable that filters events (invocable with const E&, returns bool)
    template <typename F, typename E>
    concept EventFilter = std::invocable<F, const E&> &&
        std::convertible_to<std::invoke_result_t<F, const E&>, bool>;
}
```

#### Metric Collector Concepts (`interfaces/metric_collector_interface.h`)
```cpp
namespace kcenon::monitoring::concepts {
    // A configuration type that can validate itself
    template <typename T>
    concept Validatable = requires(const T t) {
        { t.validate() };
    };

    // A type that provides metrics
    template <typename T>
    concept MetricSourceLike = requires(const T t) {
        { t.get_current_metrics() };
        { t.get_source_name() } -> std::convertible_to<std::string>;
        { t.is_healthy() } -> std::convertible_to<bool>;
    };

    // A type that collects metrics
    template <typename T>
    concept MetricCollectorLike = requires(T t) {
        { t.collect_metrics() };
        { t.is_collecting() } -> std::convertible_to<bool>;
        { t.get_metric_types() };
    };
}
```

### Usage Examples

#### Type-Safe Event Publishing
```cpp
// Only class types that are copy-constructible can be published
struct my_event { std::string data; };
event_bus.publish_event(my_event{"hello"});  // OK

// Compile error: int is not a class type
// event_bus.publish_event(42);  // Error: concepts::EventType not satisfied
```

#### Constrained Event Handlers
```cpp
// Handler with correct signature
event_bus.subscribe_event<my_event>([](const my_event& e) {
    std::cout << e.data << std::endl;
});

// Handler must return void - compile error otherwise
// event_bus.subscribe_event<my_event>([](const my_event& e) {
//     return e.data.size();  // Error: must return void
// });
```

#### Configuration Validation
```cpp
// collection_config satisfies Validatable concept
collection_config config;
config.interval = std::chrono::seconds(1);
auto result = config.validate();  // Compile-time verified to exist
if (result.is_err()) {
    // Handle validation error
}
```

### Benefits of Concepts

1. **Clear Error Messages**: Instead of cryptic SFINAE errors, you get:
   ```
   error: constraints not satisfied for 'publish_event'
   note: concept 'EventType<int>' was not satisfied
   ```

2. **Self-Documenting APIs**: Concept names describe requirements explicitly

3. **Better IDE Support**: Accurate auto-completion and type hints

4. **Code Simplification**: No more `std::enable_if` boilerplate

---

## Test Coverage ✅ **37 Tests Passing (100% Success Rate)**

### Test Suite Status

**Phase 4 Test Results:** All core functionality is thoroughly tested with comprehensive test coverage.

| Test Suite | Tests | Status | Coverage |
|-------------|-------|--------|----------|
| **Result Types** | 13 tests | ✅ PASS | Complete error handling, Result<T> pattern, metadata operations |
| **DI Container** | 9 tests | ✅ PASS | Service registration, resolution, singleton/transient lifecycles |
| **Monitorable Interface** | 9 tests | ✅ PASS | Basic monitoring interfaces and stub implementations |
| **Thread Context** | 6 tests | ✅ PASS | Thread-safe context management and ID generation |

### Running Tests

```bash
# Navigate to build directory
cd build

# Run all tests
./tests/monitoring_system_tests

# Run specific test suites
./tests/monitoring_system_tests --gtest_filter="ResultTypesTest.*"
./tests/monitoring_system_tests --gtest_filter="DIContainerTest.*"
./tests/monitoring_system_tests --gtest_filter="MonitorableInterfaceTest.*"
./tests/monitoring_system_tests --gtest_filter="ThreadContextTest.*"

# List all available tests
./tests/monitoring_system_tests --gtest_list_tests
```

### Test Implementation Details

#### Result Types Test Coverage
- Success and error result creation
- Value access and error handling
- `value_or()` default value behavior
- Monadic operations (`map`, `and_then`)
- Result void operations
- Error code to string conversion
- Metadata operations

#### DI Container Test Coverage
- Transient service registration and resolution
- Singleton service lifecycle management
- Named service registration
- Instance registration
- Service resolution verification
- Container state management

#### Monitorable Interface Test Coverage
- Basic interface implementations
- Metrics collection stubs
- Status reporting
- Configuration management
- Monitoring lifecycle

#### Thread Context Test Coverage
- Context creation and retrieval
- Request ID and correlation ID generation
- Thread-local storage behavior
- Context clearing and state management

---

## Current Implementation Status

### ✅ Fully Implemented & Tested
- **Result Types**: Complete error handling framework
- **DI Container**: Full dependency injection functionality
- **Thread Context**: Thread-safe context management
- **Core Interfaces**: Basic monitoring abstractions

### ⚠️ Stub Implementations (Foundation for Future Development)
- **Performance Monitoring**: Interface complete, basic implementation
- **Distributed Tracing**: Interface complete, stub functionality
- **Storage Backends**: Interface complete, file storage stub
- **Health Monitoring**: Interface complete, basic health checks
- **Circuit Breakers**: Interface complete, basic state management

### 🔄 Future Implementation Phases
- Advanced alerting system
- Real-time web dashboard
- Advanced storage backends
- OpenTelemetry integration
- Stream processing capabilities

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

## Migration Notes for Phase 4

Phase 4 focuses on **core foundation stability** rather than feature completeness. This approach provides:

1. **Solid Foundation**: All core types and patterns are fully implemented and tested
2. **Extensible Architecture**: Stub implementations provide clear interfaces for future expansion
3. **Production Ready Core**: Error handling, DI, and context management are production-quality
4. **Incremental Development**: Features can be added incrementally without breaking existing code

---

## Further Reading

- [Phase 4 Documentation](PHASE4.md) - Current implementation details
- [Architecture Guide](ARCHITECTURE_GUIDE.md) - System design and patterns
- [Examples](../examples/) - Working code examples
- [Changelog](CHANGELOG.md) - Version history and changes
---

*Last Updated: 2025-10-20*

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

