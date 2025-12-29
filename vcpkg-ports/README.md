# vcpkg Overlay Ports

This directory contains vcpkg overlay port definitions for the kcenon ecosystem packages.

## Purpose

These overlay ports enable local testing of the kcenon packages before official vcpkg registry submission.

## Ports

| Package | Version | Dependencies |
|---------|---------|--------------|
| kcenon-common-system | 0.2.0 | None |
| kcenon-thread-system | 3.0.0 | common-system, libiconv |
| kcenon-logger-system | 1.0.0 | common-system, thread-system, fmt |
| kcenon-monitoring-system | 2.0.0 | common-system, thread-system |

## Quick Start

```bash
# Install with overlay ports
vcpkg install kcenon-monitoring-system --overlay-ports=./vcpkg-ports

# Or use with CMake
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_OVERLAY_PORTS=$(pwd)/vcpkg-ports
```

## Documentation

See [VCPKG_OVERLAY_PORTS.md](../docs/guides/VCPKG_OVERLAY_PORTS.md) for detailed usage instructions.

## Status

- [ ] common_system - overlay port ready
- [ ] thread_system - overlay port ready
- [ ] logger_system - overlay port ready
- [ ] monitoring_system - overlay port ready
- [ ] Official vcpkg registry submission - pending

## Related Issues

- #279 - Main tracking issue
- #281, #282, #283, #284 - Individual port issues
