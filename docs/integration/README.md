# Monitoring System Integration Guide

## Overview

This directory contains integration guides for using monitoring_system with other KCENON systems.

## Integration Guides

- [With Common System](with-common-system.md) - IMonitor interface implementation
- [With Thread System](with-thread-system.md) - Thread pool metrics
- [With Logger System](with-logger.md) - Log-based metrics
- [With Network System](with-network-system.md) - Network metrics
- [With Database System](with-database-system.md) - Database performance metrics

## Quick Start

### Basic Metrics

```cpp
#include "monitoring_system/Monitor.h"

int main() {
    auto monitor = monitoring_system::createMonitor();

    // Counter
    monitor->incrementCounter("requests_total");

    // Gauge
    monitor->setGauge("active_connections", 42);

    // Histogram
    monitor->recordValue("request_duration_ms", 156.7);

    // Timer
    auto timer = monitor->startTimer("operation_duration");
    // ... do work ...
    timer.stop();  // Automatically recorded
}
```

### Integration with Services

```cpp
class MyService {
public:
    MyService(std::shared_ptr<IMonitor> monitor)
        : monitor_(std::move(monitor)) {}

    void processRequest() {
        monitor_->incrementCounter("requests.total");
        auto timer = monitor_->startTimer("requests.duration");

        try {
            // ... process ...
            monitor_->incrementCounter("requests.success");
        } catch (const std::exception& e) {
            monitor_->incrementCounter("requests.error");
            throw;
        }
    }

private:
    std::shared_ptr<IMonitor> monitor_;
};
```

## Integration Patterns

### Prometheus Export

```cpp
auto monitor = monitoring_system::createPrometheusMonitor({
    .port = 9090,
    .endpoint = "/metrics"
});

// Metrics automatically exposed at http://localhost:9090/metrics
```

### Custom Metrics

```cpp
// Define custom metric
monitor->registerCounter("cache.hits", "Number of cache hits");
monitor->registerCounter("cache.misses", "Number of cache misses");

// Use metrics
if (cache.contains(key)) {
    monitor->incrementCounter("cache.hits");
} else {
    monitor->incrementCounter("cache.misses");
}
```

## Common Use Cases

### 1. SLO Monitoring

```cpp
// Track SLO compliance
auto slo_monitor = monitor->createSLOMonitor({
    .target_latency = std::chrono::milliseconds(100),
    .target_success_rate = 0.999
});

slo_monitor->recordRequest(duration, success);
```

### 2. Resource Utilization

```cpp
monitor->setGauge("memory.used_bytes", get_memory_usage());
monitor->setGauge("cpu.usage_percent", get_cpu_usage());
monitor->setGauge("disk.free_bytes", get_disk_free());
```

### 3. Business Metrics

```cpp
monitor->incrementCounter("orders.placed");
monitor->recordValue("order.value_usd", order_total);
monitor->setGauge("users.active", active_user_count);
```

## Best Practices

- Use consistent naming conventions for metrics
- Add labels for multi-dimensional metrics
- Monitor both technical and business metrics
- Set up alerts for critical metrics
- Export metrics to visualization tools (Grafana, etc.)

## Additional Resources

- [Monitoring System API Reference](../API_REFERENCE.md)
- [Ecosystem Integration Guide](../../../ECOSYSTEM_INTEGRATION.md)
