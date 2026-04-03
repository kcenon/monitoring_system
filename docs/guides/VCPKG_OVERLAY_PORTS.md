---
doc_id: "MON-GUID-022"
doc_title: "vcpkg Overlay Ports Guide"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "monitoring_system"
category: "GUID"
---

# vcpkg Overlay Ports Guide

> **SSOT**: This document is the single source of truth for **vcpkg Overlay Ports Guide**.

This guide explains how to use the vcpkg overlay ports provided by the
`monitoring_system` repository, which acts as the **canonical port registry**
for the entire kcenon ecosystem.

## Overview

The `vcpkg-ports/` directory contains port definitions for all eight kcenon
ecosystem packages.  These overlay ports let you build against any version of
the kcenon libraries — including unreleased commits — without waiting for the
official vcpkg registry to be updated.

## Available Ports

| Port | Version | Port-Version | Description |
|------|---------|-------------|-------------|
| kcenon-common-system | 0.2.0 | 0 | Foundation library with Result\<T\> pattern and interfaces |
| kcenon-thread-system | 0.3.0 | 0 | High-performance multithreading framework |
| kcenon-logger-system | 0.1.2 | 0 | High-performance async logging framework |
| kcenon-container-system | 0.1.0 | 0 | Advanced container system with thread-safe operations |
| kcenon-monitoring-system | 0.1.0 | 0 | Monitoring system with metrics, tracing, and container monitoring |
| kcenon-database-system | 0.1.0 | 2 | Core DAL library with multi-backend support |
| kcenon-network-system | 0.1.0 | 3 | Async network library with TCP/UDP, HTTP/1.1, WebSocket, TLS 1.3 |
| kcenon-pacs-system | 0.1.0 | 2 | Modern C++20 PACS implementation |

## Prerequisites

- [vcpkg](https://github.com/microsoft/vcpkg) installed
- CMake 3.20+
- C++20 compatible compiler

## Usage

### Using Overlay Ports

```bash
# Set the overlay ports path
export VCPKG_OVERLAY_PORTS="/path/to/monitoring_system/vcpkg-ports"

# Install individual packages
vcpkg install kcenon-common-system --overlay-ports=./vcpkg-ports
vcpkg install kcenon-thread-system --overlay-ports=./vcpkg-ports
vcpkg install kcenon-logger-system --overlay-ports=./vcpkg-ports
vcpkg install kcenon-monitoring-system --overlay-ports=./vcpkg-ports
```

### CMake Integration

Add to your `CMakeLists.txt`:

```cmake
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
├── kcenon-common-system (required)
├── kcenon-thread-system (required)
│   └── kcenon-common-system
│   └── simdutf
├── [logging] kcenon-logger-system
│   ├── kcenon-common-system
│   ├── kcenon-thread-system
│   ├── fmt
│   └── libiconv (non-Windows)
└── [network] kcenon-network-system
    ├── kcenon-common-system
    ├── kcenon-thread-system
    ├── kcenon-logger-system
    ├── asio
    └── openssl

kcenon-database-system
└── kcenon-common-system

kcenon-pacs-system
├── kcenon-common-system
├── kcenon-container-system
│   └── kcenon-common-system
└── kcenon-network-system
```

## Optional Features

### monitoring-system Features

| Feature | Description |
|---------|-------------|
| `logging` | Enable logger_system integration for structured logging and audit trails |
| `network` | Enable network_system integration for HTTP metrics export (OTLP, Prometheus) |

```bash
# Install with optional features
vcpkg install kcenon-monitoring-system[logging] --overlay-ports=./vcpkg-ports
vcpkg install kcenon-monitoring-system[logging,network] --overlay-ports=./vcpkg-ports
```

## Consumer Repository Setup

Projects that depend on kcenon packages should reference monitoring_system's
overlay ports rather than maintaining their own local port copies.

Add the monitoring_system overlay path to your CMake configure step:

```bash
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_OVERLAY_PORTS=/path/to/monitoring_system/vcpkg-ports
```

Or set the `VCPKG_OVERLAY_PORTS` environment variable in your CI pipeline.

## Port Update Procedure

See [PORT_MANAGEMENT.md](PORT_MANAGEMENT.md) for the step-by-step procedure for
updating port definitions when a new upstream release is made.

## Notes

- Port files pin specific upstream commits via SHA512 hashes for reproducible builds
- Patches in each port directory fix upstream issues that are not yet merged upstream
- Port-version increments indicate portfile-only fixes without upstream changes

## Related Issues

- [#533](https://github.com/kcenon/monitoring_system/issues/533) - Single source of truth for vcpkg port management
- [#279](https://github.com/kcenon/monitoring_system/issues/279) - vcpkg registry registration tracking
- [#281](https://github.com/kcenon/monitoring_system/issues/281) - common_system port
- [#282](https://github.com/kcenon/monitoring_system/issues/282) - thread_system port
- [#283](https://github.com/kcenon/monitoring_system/issues/283) - logger_system port
- [#284](https://github.com/kcenon/monitoring_system/issues/284) - monitoring_system port
