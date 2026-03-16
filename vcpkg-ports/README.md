# vcpkg Overlay Ports — Canonical Registry

This directory is the **single source of truth** for vcpkg port definitions across
the entire kcenon ecosystem.  All eight system packages are maintained here with
real SHA512 hashes, upstream patches, and CI validation.

## Canonical Status

| Package | Version | Port-Version | Patches | Dependencies |
|---------|---------|-------------|---------|--------------|
| kcenon-common-system | 0.2.0 | 0 | — | None |
| kcenon-thread-system | 0.3.0 | 0 | — | common-system, simdutf |
| kcenon-logger-system | 0.1.2 | 1 | fix-unified-deps-target-names | common-system, thread-system, fmt, libiconv |
| kcenon-container-system | 0.1.0 | 1 | — | common-system |
| kcenon-monitoring-system | 0.1.0 | 0 | — | common-system, thread-system |
| kcenon-database-system | 0.1.0 | 3 | fix-common-system-target | common-system |
| kcenon-network-system | 0.1.0 | 4 | fix-common-system-target | common-system, thread-system, logger-system, asio, openssl |
| kcenon-pacs-system | 0.1.0 | 3 | fix-vcpkg-dependency-discovery | common-system, container-system, network-system |

## PACKAGE_NAME Convention

All ecosystem ports use **snake_case** for CMake `PACKAGE_NAME`, matching vcpkg port
naming conventions. This ensures consumers use a single, consistent `find_package()` API.

### find_package() Reference

| Port | find_package() | Primary Target |
|------|---------------|----------------|
| kcenon-common-system | `find_package(common_system CONFIG)` | `kcenon::common_system` |
| kcenon-thread-system | `find_package(thread_system CONFIG)` | `thread_system::ThreadSystem` |
| kcenon-logger-system | `find_package(logger_system CONFIG)` | `LoggerSystem::LoggerSystem` |
| kcenon-container-system | `find_package(container_system CONFIG)` | `ContainerSystem::container` |
| kcenon-monitoring-system | `find_package(monitoring_system CONFIG)` | `monitoring_system::monitoring_system` |
| kcenon-database-system | `find_package(database_system CONFIG)` | `DatabaseSystem::database` |
| kcenon-network-system | `find_package(network_system CONFIG)` | `NetworkSystem::NetworkSystem` |
| kcenon-pacs-system | `find_package(pacs_system CONFIG)` | `kcenon::pacs::core` |

> **Note**: Target names still vary across ports because they are defined by upstream
> `install(EXPORT)` rules. The snake_case wrappers standardize `find_package()` names
> without modifying upstream target exports.

### Migration from PascalCase

If you previously used PascalCase package names, update your `CMakeLists.txt`:

```cmake
# Before (deprecated)
find_package(LoggerSystem CONFIG)
find_package(ContainerSystem CONFIG)
find_package(DatabaseSystem CONFIG)
find_package(NetworkSystem CONFIG)

# After (standardized)
find_package(logger_system CONFIG)
find_package(container_system CONFIG)
find_package(database_system CONFIG)
find_package(network_system CONFIG)
```

Target names in `target_link_libraries()` remain unchanged — only the `find_package()`
name changes.

### Adding New Ports

When adding a new ecosystem port, follow these rules:

1. Use `PACKAGE_NAME` in snake_case matching the port name (e.g., `kcenon-foo-system` → `foo_system`)
2. Set `CONFIG_PATH` to the upstream CMake config install path
3. If upstream uses PascalCase config files, create a snake_case wrapper:
   ```cmake
   file(WRITE "${CURRENT_PACKAGES_DIR}/share/foo_system/foo_system-config.cmake"
       "include(\"\${CMAKE_CURRENT_LIST_DIR}/FooSystemConfig.cmake\")\n"
   )
   ```

### Upstream Adoption Status

Four upstream repositories still install CMake config files under a PascalCase path.
Upstream issues have been filed requesting native snake_case support. Once each upstream
merges the change, the corresponding portfile can drop its `CONFIG_PATH` override and
wrapper config file.

| Repository | Current Config Path | Target Config Path | Upstream Issue | Portfile Cleanup |
|------------|--------------------|--------------------|----------------|------------------|
| kcenon/logger_system | `lib/cmake/LoggerSystem` | `lib/cmake/logger_system` | [#502](https://github.com/kcenon/logger_system/issues/502) | Remove wrapper + `CONFIG_PATH` |
| kcenon/container_system | `lib/cmake/ContainerSystem` | `lib/cmake/container_system` | [#424](https://github.com/kcenon/container_system/issues/424) | Remove wrapper + `CONFIG_PATH` |
| kcenon/database_system | `lib/cmake/DatabaseSystem` | `lib/cmake/database_system` | [#455](https://github.com/kcenon/database_system/issues/455) | Remove wrapper + `CONFIG_PATH` |
| kcenon/network_system | `lib/cmake/NetworkSystem` | `lib/cmake/network_system` | [#843](https://github.com/kcenon/network_system/issues/843) | Remove wrapper + `CONFIG_PATH` |

The remaining four ports (common_system, thread_system, monitoring_system, pacs_system)
already install under snake_case paths and need no wrapper.

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

- #532 — Standardize CMake PACKAGE_NAME convention across ecosystem ports (parent epic)
- #543 — Document snake_case PACKAGE_NAME convention and track upstream adoption
- #533 — Establish single source of truth for vcpkg port management
- #279 — Main tracking issue for vcpkg registry registration
- #281, #282, #283, #284 — Individual port issues
