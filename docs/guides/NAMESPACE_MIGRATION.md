# Namespace Migration Guide

> **Version:** 1.0.0
> **Last Updated:** 2025-02-05
> **Status:** Migration Completed (v1.0)

This guide documents the namespace transition from legacy `monitoring_system` to the canonical `kcenon::monitoring` namespace, which aligns with the kcenon ecosystem naming conventions.

---

## Table of Contents

1. [Overview](#overview)
2. [Current vs Legacy](#current-vs-legacy)
3. [Timeline](#timeline)
4. [Migration Steps](#migration-steps)
5. [Compatibility Layer](#compatibility-layer)
6. [Migration Checklist](#migration-checklist)
7. [FAQ](#faq)

---

## Overview

monitoring_system has transitioned from the `monitoring_system` namespace to `kcenon::monitoring` to align with ecosystem conventions used by other kcenon projects:

| Project | Namespace |
|---------|-----------|
| common_system | `kcenon::common` |
| thread_system | `kcenon::thread` |
| logger_system | `kcenon::logger` |
| network_system | `kcenon::network` |
| container_system | `kcenon::container` |
| **monitoring_system** | **`kcenon::monitoring`** |

---

## Current vs Legacy

| Version | Namespace | Status |
|---------|-----------|--------|
| v1.x (current) | `kcenon::monitoring` | **Canonical** (recommended) |
| v1.x (current) | `monitoring_system` | Alias (deprecated) |
| v2.0 (planned) | `kcenon::monitoring` | Only supported namespace |

---

## Timeline

| Phase | Version | Status | Description |
|-------|---------|--------|-------------|
| Phase 1 | v0.x | Completed | Legacy namespace (`monitoring_system`) |
| Phase 2 | v1.0 | **Current** | New namespace (`kcenon::monitoring`) with compatibility alias |
| Phase 3 | v1.x | Planned | Deprecation warnings for legacy namespace |
| Phase 4 | v2.0 | Planned | Legacy namespace alias removed |

---

## Migration Steps

### Before (Legacy Code)

```cpp
#include <kcenon/monitoring/core/performance_monitor.h>

// Legacy namespace (deprecated)
using namespace monitoring_system;

int main() {
    performance_monitor monitor("my_service");

    monitoring_system::central_collector collector;
    collector.start();

    return 0;
}
```

### After (Modern Code)

```cpp
#include <kcenon/monitoring/core/performance_monitor.h>

// Canonical namespace (recommended)
using namespace kcenon::monitoring;

int main() {
    performance_monitor monitor("my_service");

    kcenon::monitoring::central_collector collector;
    collector.start();

    return 0;
}
```

---

## Compatibility Layer

During the transition period (v1.x), a compatibility layer is provided via `<kcenon/monitoring/compatibility.h>`:

```cpp
// Backward compatibility namespace alias
// Legacy code can use monitoring_system:: which maps to kcenon::monitoring::
namespace monitoring_system = ::kcenon::monitoring;

// Additional legacy alias
namespace monitoring_module = ::kcenon::monitoring;
```

### Using the Compatibility Layer

If you need to support both old and new code during migration:

```cpp
#include <kcenon/monitoring/compatibility.h>  // Includes aliases
#include <kcenon/monitoring/core/performance_monitor.h>

// Both work during v1.x:
kcenon::monitoring::performance_monitor monitor1("service1");  // Recommended
monitoring_system::performance_monitor monitor2("service2");    // Deprecated
```

**Note:** The compatibility header is automatically included by most monitoring_system headers, but explicitly including it makes migration intent clear.

---

## Migration Checklist

Use this checklist when migrating your codebase:

### Code Changes

- [ ] Replace `using namespace monitoring_system` with `using namespace kcenon::monitoring`
- [ ] Replace explicit `monitoring_system::` prefixes with `kcenon::monitoring::`
- [ ] Replace `monitoring_module::` prefixes with `kcenon::monitoring::` (if applicable)
- [ ] Update any typedef or alias declarations

### Build System Changes

- [ ] Update CMake target names if applicable (typically unchanged)
- [ ] Update pkg-config references if applicable

### Search and Replace Patterns

| Find | Replace |
|------|---------|
| `using namespace monitoring_system` | `using namespace kcenon::monitoring` |
| `monitoring_system::` | `kcenon::monitoring::` |
| `monitoring_module::` | `kcenon::monitoring::` |

### Verification

- [ ] Build succeeds without errors
- [ ] All tests pass
- [ ] No deprecation warnings (when enabled)

---

## FAQ

### Q: Do I need to migrate immediately?

**A:** No. The compatibility layer ensures legacy code continues to work throughout v1.x. However, we recommend migrating to avoid issues when v2.0 is released.

### Q: Will `monitoring_system` namespace stop working?

**A:** In v2.0, the `monitoring_system` and `monitoring_module` namespace aliases will be removed. Until then, they work as aliases to `kcenon::monitoring`.

### Q: How do I enable deprecation warnings?

**A:** Deprecation warnings will be introduced in a future v1.x release. Check the CHANGELOG for updates.

### Q: What if I have both monitoring_system and kcenon::monitoring in the same codebase?

**A:** Since `monitoring_system` is an alias for `kcenon::monitoring`, they refer to the same types. You can mix them during migration, but this is not recommended for code clarity.

### Q: Are header paths changing?

**A:** No. Header paths remain `<kcenon/monitoring/...>`. Only the namespace is affected.

---

## Related Documentation

- [Quick Start Guide](QUICK_START.md) - Getting started with monitoring_system
- [API Reference](../API_REFERENCE.md) - Complete API documentation
- [CHANGELOG](../CHANGELOG.md) - Version history and migration notes

---

*For questions or issues with migration, please open an issue on the [GitHub repository](https://github.com/kcenon/monitoring_system/issues).*
