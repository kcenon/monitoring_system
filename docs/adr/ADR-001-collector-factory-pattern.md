---
doc_id: "MON-ADR-001"
doc_title: "ADR-001: Collector Factory Pattern"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Accepted"
project: "monitoring_system"
category: "ADR"
---

# ADR-001: Collector Factory Pattern

> **SSOT**: This document is the single source of truth for **ADR-001: Collector Factory Pattern**.

| Field | Value |
|-------|-------|
| Status | Accepted |
| Date | 2025-03-15 |
| Decision Makers | kcenon ecosystem maintainers |

## Context

monitoring_system provides 16+ metric collectors (system resources, process metrics,
network I/O, GPU, battery, etc.) plus a plugin architecture for custom collectors.
Each collector has different platform availability, dependencies, and configuration.

The system needs to:
1. Instantiate collectors by name at runtime (configuration-driven).
2. Support both built-in and plugin-provided collectors.
3. Provide platform-appropriate defaults (e.g., GPU collector only on systems with GPUs).
4. Allow downstream projects to register custom collectors without modifying
   monitoring_system source.

## Decision

**Implement a singleton `metric_factory`** with self-registration via the
`REGISTER_COLLECTOR(Type)` macro.

```cpp
// Registration (in collector source file)
REGISTER_COLLECTOR(system_resource_collector)

// Creation (at runtime)
auto collector = metric_factory::instance()
    .create("system_resource_collector", config);
```

Design:
1. **`metric_factory`** — Thread-safe singleton holding a registry of
   `string -> factory_function` mappings.
2. **`REGISTER_COLLECTOR(Type)`** — Macro that registers a collector at static
   initialization time (before `main()`).
3. **`collector_plugin`** — Plugin interface for external collectors loaded via
   `plugin_loader` from shared libraries.
4. **`builtin_collectors`** — Convenience class that registers all built-in
   collectors and provides platform-aware defaults.

The factory returns `unique_ptr<collector_interface>`, ensuring the caller owns
the created collector with no shared state.

## Alternatives Considered

### Manual Instantiation (No Factory)

- **Pros**: Simple, explicit, no global state.
- **Cons**: Configuration-driven collector selection requires switch/if-else
  chains that must be updated for every new collector. Plugins cannot register
  themselves.

### Abstract Factory Hierarchy

- **Pros**: Type-safe, supports families of related objects.
- **Cons**: Over-engineered for this use case. Collectors are independent — they
  don't form families. The hierarchy adds unnecessary indirection.

### Service Locator (DI Container)

- **Pros**: Powerful, supports complex dependency graphs.
- **Cons**: common_system's `service_container` is available but designed for
  larger components. Using DI for lightweight collector instantiation adds
  unnecessary coupling and configuration overhead.

## Consequences

### Positive

- **Self-registration**: New collectors are automatically available by including
  their source file. No central registry file to update.
- **Plugin extensibility**: External collectors register through the same factory
  via the plugin API, enabling per-deployment customization.
- **Platform awareness**: `builtin_collectors` queries platform capabilities and
  only registers available collectors.
- **Configuration-driven**: Collector selection is driven by string names in
  configuration, enabling runtime customization without recompilation.

### Negative

- **Static initialization order**: `REGISTER_COLLECTOR` relies on static
  initialization, which has undefined order across translation units. Mitigated
  by the factory using lazy initialization (Meyers' singleton).
- **Global singleton**: `metric_factory::instance()` is global mutable state.
  Testing requires careful factory reset between test cases.
- **String-based lookup**: Collector names are strings, so typos cause runtime
  errors rather than compile-time errors. Mitigated by the `builtin_collectors`
  helper which uses constants.
