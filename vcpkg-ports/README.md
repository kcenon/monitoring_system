# vcpkg Overlay Ports — Canonical Registry

This directory is the **single source of truth** for vcpkg port definitions across
the entire kcenon ecosystem.  All eight system packages are maintained here with
real SHA512 hashes, upstream patches, and CI validation.

## Canonical Status

| Package | Version | Port-Version | Patches | Dependencies |
|---------|---------|-------------|---------|--------------|
| kcenon-common-system | 0.2.0 | 0 | — | None |
| kcenon-thread-system | 0.3.0 | 0 | — | common-system, simdutf |
| kcenon-logger-system | 0.1.2 | 0 | fix-unified-deps-target-names | common-system, thread-system, fmt, libiconv |
| kcenon-container-system | 0.1.0 | 0 | — | common-system |
| kcenon-monitoring-system | 0.1.0 | 0 | — | common-system, thread-system |
| kcenon-database-system | 0.1.0 | 2 | fix-common-system-target | common-system |
| kcenon-network-system | 0.1.0 | 3 | fix-common-system-target | common-system, thread-system, logger-system, asio, openssl |
| kcenon-pacs-system | 0.1.0 | 2 | fix-vcpkg-dependency-discovery | common-system, container-system, network-system |

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

- #533 — Establish single source of truth for vcpkg port management
- #279 — Main tracking issue for vcpkg registry registration
- #281, #282, #283, #284 — Individual port issues
