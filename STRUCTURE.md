# Monitoring System - New Structure

## Directory Layout

```
monitoring_system/
├── include/kcenon/monitoring/   # Public headers
│   ├── core/                   # Core APIs
│   ├── interfaces/             # Interface definitions
│   ├── collectors/             # Collector interfaces
│   ├── exporters/              # Exporter interfaces
│   └── utils/                  # Public utilities
├── src/                        # Implementation
│   ├── core/                   # Core implementation
│   ├── impl/                   # Private implementations
│   │   ├── collectors/         # Metric collectors
│   │   ├── exporters/          # Data exporters
│   │   ├── storage/            # Storage engines
│   │   ├── web/                # Web dashboard
│   │   ├── alerting/           # Alert system
│   │   └── tracing/            # Distributed tracing
│   └── utils/                  # Utility implementations
├── tests/                      # All tests
│   ├── unit/                   # Unit tests
│   ├── integration/            # Integration tests
│   └── benchmarks/             # Performance tests
├── examples/                   # Usage examples
├── docs/                       # Documentation
└── CMakeLists.txt             # Build configuration
```

## Namespace Structure

- Root: `kcenon::monitoring`
- Core functionality: `kcenon::monitoring::core`
- Interfaces: `kcenon::monitoring::interfaces`
- Collectors: `kcenon::monitoring::collectors`
- Exporters: `kcenon::monitoring::exporters`
- Implementation details: `kcenon::monitoring::impl`
- Utilities: `kcenon::monitoring::utils`

## Key Components

### Public API (include/kcenon/monitoring/)
- `core/performance_monitor.h` - Main monitoring class
- `interfaces/monitoring_interface.h` - Core interface
- `collectors/*.h` - Metric collector interfaces
- `exporters/*.h` - Data exporter interfaces
- `utils/metric_types.h` - Metric type definitions

### Implementation (src/)
- Real-time collectors in `impl/collectors/`
- Export adapters in `impl/exporters/`
- Web dashboard in `impl/web/`
- Alert engine in `impl/alerting/`
- Distributed tracing in `impl/tracing/`

## Features

- **Real-time Metrics**: System, application, and custom metrics
- **Web Dashboard**: Interactive visualization at port 8080
- **Alerting**: Rule-based notifications
- **Distributed Tracing**: End-to-end request tracking
- **Export Formats**: Prometheus, OpenTelemetry, JSON

## Migration Notes

1. Old structure backed up in `old_structure/` directory
2. Compatibility header at `include/kcenon/monitoring/compatibility.h`
3. Run `./migrate_namespaces.sh` to update all namespaces
4. Update CMakeLists.txt for new structure
