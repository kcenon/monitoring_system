#pragma once

/**
 * @file forward.h
 * @brief Forward declarations for monitoring_system types
 *
 * This header provides forward declarations for commonly used types
 * in the monitoring_system module to reduce compilation dependencies.
 */

namespace kcenon::monitoring {

// Core classes
namespace core {
    class monitor;
    class performance_monitor;
    class resource_monitor;
    class health_monitor;
    class metric_registry;
}

// Metrics types
namespace metrics {
    class counter;
    class gauge;
    class histogram;
    class timer;
    class meter;
    enum class metric_type;
}

// Collectors
namespace collectors {
    class collector;
    class cpu_collector;
    class memory_collector;
    class disk_collector;
    class network_collector;
}

// Exporters
namespace exporters {
    class exporter;
    class prometheus_exporter;
    class json_exporter;
    class graphite_exporter;
}

// Interfaces
namespace interfaces {
    class monitoring_interface;
    class observable;
    class metric_provider;
}

// Utilities
namespace utils {
    class sampling;
    class aggregator;
    class statistics;
}

} // namespace kcenon::monitoring