# vcpkg Overlay Ports Guide

This guide explains how to use the vcpkg overlay ports for the kcenon ecosystem packages.

## Overview

The `vcpkg-ports/` directory contains overlay port definitions for local testing before official vcpkg registry submission. These ports allow you to install and test the kcenon ecosystem packages locally.

## Available Ports

| Port | Version | Description |
|------|---------|-------------|
| kcenon-common-system | 0.2.0 | Foundation library with Result<T> pattern and interfaces |
| kcenon-thread-system | 3.0.0 | High-performance multithreading framework |
| kcenon-logger-system | 1.0.0 | Async logging library with file rotation |
| kcenon-monitoring-system | 2.0.0 | Monitoring system with metrics and tracing |

## Prerequisites

- [vcpkg](https://github.com/microsoft/vcpkg) installed
- CMake 3.20+
- C++20 compatible compiler

## Usage

### Using Overlay Ports

```bash
# Set the overlay ports path
export VCPKG_OVERLAY_PORTS="/path/to/monitoring_system/vcpkg-ports"

# Install packages using overlay ports
vcpkg install kcenon-common-system --overlay-ports=./vcpkg-ports
vcpkg install kcenon-thread-system --overlay-ports=./vcpkg-ports
vcpkg install kcenon-logger-system --overlay-ports=./vcpkg-ports
vcpkg install kcenon-monitoring-system --overlay-ports=./vcpkg-ports
```

### CMake Integration

Add to your `CMakeLists.txt`:

```cmake
# Using vcpkg toolchain
cmake_minimum_required(VERSION 3.20)
project(your_project)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(common_system REQUIRED)
find_package(thread_system REQUIRED)
find_package(monitoring_system REQUIRED)

add_executable(your_app main.cpp)
target_link_libraries(your_app PRIVATE
    common_system::common_system
    thread_system::thread_system
    monitoring_system::monitoring_system
)
```

### vcpkg.json Manifest Mode

Create a `vcpkg.json` in your project:

```json
{
  "name": "your-project",
  "version": "1.0.0",
  "dependencies": [
    "kcenon-common-system",
    "kcenon-thread-system",
    "kcenon-monitoring-system"
  ]
}
```

Configure CMake with overlay ports:

```bash
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_OVERLAY_PORTS=/path/to/monitoring_system/vcpkg-ports
```

## Dependency Graph

```
kcenon-monitoring-system
├── kcenon-common-system
├── kcenon-thread-system
│   └── kcenon-common-system
│   └── libiconv (non-Windows)
└── [optional] kcenon-logger-system
    ├── kcenon-common-system
    ├── kcenon-thread-system
    └── fmt
```

## Testing Status

| Port | vcpkg Install | Build | Notes |
|------|---------------|-------|-------|
| kcenon-common-system | ✅ Pass | ✅ Pass | Header-only library |
| kcenon-thread-system | ✅ Pass | ⚠️ Blocked | Upstream CMake issue (common_system linking) |
| kcenon-logger-system | ✅ Pass | ⚠️ Blocked | Depends on thread_system fix |
| kcenon-monitoring-system | ✅ Pass | ⚠️ Blocked | Depends on thread_system fix |

### Known Issues

The `thread_system`, `logger_system`, and `monitoring_system` builds are blocked due to an upstream CMake configuration issue:

- **Problem**: `ThreadSystemDependencies.cmake` finds `common_system` via `find_package()` but doesn't link the `kcenon::common_system` target to the `ThreadSystem` library
- **Symptom**: Build fails with "fatal error: 'kcenon/common/patterns/result.h' file not found"
- **Solution**: Upstream repositories need to update their CMake configuration to properly link the `kcenon::common_system` target

## Notes

- These overlay ports use `HEAD_REF main` for development
- SHA512 hashes are placeholder values (update after release)
- For production, wait for official vcpkg registry submission
- Port definitions include `MAYBE_UNUSED_VARIABLES` to suppress CMake warnings

## Related Issues

- [#279](https://github.com/kcenon/monitoring_system/issues/279) - vcpkg registry registration tracking
- [#281](https://github.com/kcenon/monitoring_system/issues/281) - common_system port
- [#282](https://github.com/kcenon/monitoring_system/issues/282) - thread_system port
- [#283](https://github.com/kcenon/monitoring_system/issues/283) - logger_system port
- [#284](https://github.com/kcenon/monitoring_system/issues/284) - monitoring_system port
