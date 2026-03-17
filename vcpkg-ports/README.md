# vcpkg Overlay Ports — Canonical Registry

This directory is the **single source of truth** for vcpkg port definitions across
the entire kcenon ecosystem.  All eight system packages are maintained here, mirroring
the canonical [vcpkg-registry](https://github.com/kcenon/vcpkg-registry) with CI validation.

## Canonical Status

| Package | Version | Port-Version | Patches | Dependencies |
|---------|---------|-------------|---------|--------------|
| kcenon-common-system | 0.2.0 | 0 | — | None |
| kcenon-thread-system | 0.3.1 | — | — | common-system, simdutf |
| kcenon-logger-system | 0.1.3 | — | — | common-system, libiconv |
| kcenon-container-system | 0.1.0 | 2 | — | common-system |
| kcenon-monitoring-system | 0.1.0 | 1 | — | common-system, thread-system |
| kcenon-database-system | 0.1.0 | 3 | — | common-system, asio |
| kcenon-network-system | 0.1.1 | 1 | — | common-system, thread-system, asio, openssl |
| kcenon-pacs-system | 0.1.0 | 4 | — | common-system, container-system, network-system |

## PACKAGE_NAME Convention

All ecosystem ports support **snake_case** `find_package()` names for consumer consistency.
Ports whose upstream uses PascalCase config files include snake_case wrapper files so
both naming conventions work.

### find_package() Reference

| Port | find_package() | Primary Target | Also accepts |
|------|---------------|----------------|-------------|
| kcenon-common-system | `find_package(common_system CONFIG)` | `kcenon::common_system` | — |
| kcenon-thread-system | `find_package(thread_system CONFIG)` | `thread_system::ThreadSystem` | — |
| kcenon-logger-system | `find_package(logger_system CONFIG)` | `LoggerSystem::LoggerSystem` | `LoggerSystem` |
| kcenon-container-system | `find_package(container_system CONFIG)` | `ContainerSystem::container` | `ContainerSystem` |
| kcenon-monitoring-system | `find_package(monitoring_system CONFIG)` | `monitoring_system::monitoring_system` | — |
| kcenon-database-system | `find_package(database_system CONFIG)` | `DatabaseSystem::database` | `DatabaseSystem` |
| kcenon-network-system | `find_package(network_system CONFIG)` | `NetworkSystem::NetworkSystem` | `NetworkSystem` |
| kcenon-pacs-system | `find_package(pacs_system CONFIG)` | `kcenon::pacs::core` | — |

> **Note**: Target names in `target_link_libraries()` are defined by upstream `install(EXPORT)` rules
> and use PascalCase. The snake_case wrappers only affect `find_package()` resolution.

### Upstream Adoption Status

Three upstream repositories still install CMake config files under a PascalCase path.
Upstream issues have been filed requesting native snake_case support:

| Repository | Current Config Path | Target Config Path | Upstream Issue |
|------------|--------------------|--------------------|----------------|
| kcenon/container_system | `lib/cmake/ContainerSystem` | `lib/cmake/container_system` | [#424](https://github.com/kcenon/container_system/issues/424) |
| kcenon/database_system | `lib/cmake/DatabaseSystem` | `lib/cmake/database_system` | [#455](https://github.com/kcenon/database_system/issues/455) |
| kcenon/network_system | `lib/cmake/NetworkSystem` | `lib/cmake/network_system` | [#843](https://github.com/kcenon/network_system/issues/843) |

The remaining five ports (common_system, thread_system, logger_system, monitoring_system,
pacs_system) already install under snake_case paths.

## Quick Start

```bash
# Install with overlay ports
vcpkg install kcenon-monitoring-system --overlay-ports=./vcpkg-ports

# Or use with CMake
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_OVERLAY_PORTS=$(pwd)/vcpkg-ports
```

## Port Management

See [PORT_MANAGEMENT.md](../docs/guides/PORT_MANAGEMENT.md) for the authoritative
guide on updating ports, syncing to the remote registry, and onboarding consumer
repositories.

## Documentation

See [VCPKG_OVERLAY_PORTS.md](../docs/guides/VCPKG_OVERLAY_PORTS.md) for detailed
installation and integration instructions.

## CI Validation

Every change to this directory triggers the `validate-vcpkg-chain` workflow, which
builds the consumer integration test on Ubuntu, macOS, and Windows.

## Status

- [x] kcenon-common-system — canonical port ready, CI validated
- [x] kcenon-thread-system — canonical port ready, CI validated
- [x] kcenon-logger-system — canonical port ready, CI validated
- [x] kcenon-container-system — canonical port ready, CI validated
- [x] kcenon-monitoring-system — canonical port ready, CI validated
- [x] kcenon-database-system — canonical port ready, CI validated
- [x] kcenon-network-system — canonical port ready, CI validated
- [x] kcenon-pacs-system — canonical port ready, CI validated
- [ ] Official vcpkg registry submission — pending

## Related Issues

- #550 — Synchronize overlay ports with vcpkg-registry (parent tracking issue)
- #551 — Update stale overlay port versions (thread, logger, network)
- #552 — Align port metadata and features with registry (container, monitoring, database, pacs)
- #532 — Standardize CMake PACKAGE_NAME convention across ecosystem ports (parent epic)
- #543 — Document snake_case PACKAGE_NAME convention and track upstream adoption
- #533 — Establish single source of truth for vcpkg port management
- #279 — Main tracking issue for vcpkg registry registration
- #281, #282, #283, #284 — Individual port issues
