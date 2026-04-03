---
doc_id: "MON-PROJ-005"
doc_title: "SOUP List &mdash; monitoring_system"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "monitoring_system"
category: "PROJ"
---

# SOUP List &mdash; monitoring_system

> **Software of Unknown Provenance (SOUP) Register per IEC 62304:2006+AMD1:2015 &sect;8.1.2**
>
> This document is the authoritative reference for all external software dependencies.
> Every entry must include: title, manufacturer, unique version identifier, license, and known anomalies.

| Document | Version |
|----------|---------|
| IEC 62304 Reference | &sect;8.1.2 Software items from SOUP |
| Last Reviewed | 2026-03-10 |
| monitoring_system Version | 2.0.0 |

---

## Internal Ecosystem Dependencies

| ID | Name | Manufacturer | Version | Provenance Rule | License | Usage | Safety Class | Known Anomalies |
|----|------|-------------|---------|-----------------|---------|-------|-------------|-----------------|
| INT-001 | [common_system](https://github.com/kcenon/common_system) | kcenon | 0.2.0 | Resolve via the vcpkg baseline `c4af3593e1f1aa9e14a560a09e45ea2cb0dfd74d`; source builds must record the exact git tag or commit SHA in the release SBOM | BSD-3-Clause | Result&lt;T&gt; pattern, error handling primitives | B | None |
| INT-002 | [thread_system](https://github.com/kcenon/thread_system) | kcenon | 0.3.0 | Resolve via the vcpkg baseline `c4af3593e1f1aa9e14a560a09e45ea2cb0dfd74d`; source builds must record the exact git tag or commit SHA in the release SBOM | BSD-3-Clause | Thread pool, async task scheduling for metric collection | B | None |

---

## Production SOUP

### System Dependencies

| ID | Name | Manufacturer | Version | License | Usage | Safety Class | Known Anomalies |
|----|------|-------------|---------|---------|-------|-------------|-----------------|
| SOUP-001 | POSIX Threads (pthreads) | POSIX / OS vendor | System-provided | N/A (OS) | Concurrent metric collection and alerting via `find_package(Threads)` | B | None |

---

## Optional SOUP

### gRPC Transport Feature (`grpc`)

| ID | Name | Manufacturer | Version | License | Usage | Safety Class | Known Anomalies |
|----|------|-------------|---------|---------|-------|-------------|-----------------|
| SOUP-002 | [gRPC](https://github.com/grpc/grpc) | Google | 1.51.1 | Apache-2.0 | RPC transport for OTLP trace export | A | None |
| SOUP-003 | [Protocol Buffers](https://github.com/protocolbuffers/protobuf) | Google | 3.21.12 | BSD-3-Clause | Serialization for gRPC transport | A | None |

### Logging Feature (`logging` / `MONITORING_WITH_LOGGER_SYSTEM`)

| ID | Name | Manufacturer | Version | Provenance Rule | License | Usage | Safety Class | Known Anomalies |
|----|------|-------------|---------|-----------------|---------|-------|-------------|-----------------|
| INT-003 | [logger_system](https://github.com/kcenon/logger_system) | kcenon | 0.1.0 | Use overlay/vcpkg port version `0.1.0` when the `logging` feature is enabled; source builds must record the exact git tag or commit SHA in the release SBOM | BSD-3-Clause | Audit logging integration (optional) | A | None |

### Network Export Path (`MONITORING_WITH_NETWORK_SYSTEM`)

| ID | Name | Manufacturer | Version | Provenance Rule | License | Usage | Safety Class | Known Anomalies |
|----|------|-------------|---------|-----------------|---------|-------|-------------|-----------------|
| INT-004 | [network_system](https://github.com/kcenon/network_system) | kcenon | Source-defined | This path is activated only by the CMake option `MONITORING_WITH_NETWORK_SYSTEM`; release SBOMs must record the exact git tag or commit SHA because the root vcpkg manifest does not pin a package version | BSD-3-Clause | Network transport for distributed monitoring exporters (optional) | B | None |

---

## Development/Test SOUP (Not Deployed)

| ID | Name | Manufacturer | Version | License | Usage | Qualification |
|----|------|-------------|---------|---------|-------|--------------|
| SOUP-T01 | [Google Test](https://github.com/google/googletest) | Google | 1.14.0 | BSD-3-Clause | Unit testing framework (includes GMock) | Required |

---

## Safety Classification Key

| Class | Definition | Example |
|-------|-----------|---------|
| **A** | No contribution to hazardous situation | Logging, formatting, test frameworks |
| **B** | Non-serious injury possible | Data processing, network communication |
| **C** | Death or serious injury possible | Encryption, access control |

---

## Version Pinning and Provenance Rules (IEC 62304 Compliance)

Third-party versions are pinned in `vcpkg.json`, while internal ecosystem
dependencies are resolved either through the repository's pinned vcpkg baseline
or through a source-build policy that requires recording an exact git tag or
commit SHA in the release SBOM.

```json
{
  "dependencies": [
    "kcenon-common-system",
    "kcenon-thread-system"
  ],
  "overrides": [
    { "name": "grpc", "version": "1.51.1" },
    { "name": "protobuf", "version": "3.21.12" },
    { "name": "gtest", "version": "1.14.0" }
  ],
  "features": {
    "logging": {
      "dependencies": [
        "kcenon-logger-system"
      ]
    },
    "grpc": {
      "dependencies": [
        {
          "name": "grpc",
          "version>=": "1.51.1"
        },
        {
          "name": "protobuf",
          "version>=": "3.21.0"
        }
      ]
    }
  }
}
```

The vcpkg baseline is locked in `vcpkg-configuration.json`:

```json
{
  "default-registry": {
    "baseline": "c4af3593e1f1aa9e14a560a09e45ea2cb0dfd74d"
  }
}
```

## Feature-Specific SBOM Guidance

- Default builds should list `kcenon-common-system` and `kcenon-thread-system` as required internal dependencies.
- `logging` builds should add `kcenon-logger-system` as a feature-scoped internal dependency.
- `grpc` builds should add `grpc` and `protobuf` as feature-scoped third-party dependencies.
- `MONITORING_WITH_NETWORK_SYSTEM=ON` builds should annotate `network_system` as a source-resolved CMake integration and record the exact git tag or commit SHA used for that build.

---

## Version Update Process

When updating any SOUP dependency:

1. Update the version or provenance rule in `vcpkg.json`, `vcpkg-configuration.json`, or the build documentation as applicable
2. Update the corresponding row in this document
3. Verify no new known anomalies (check CVE databases)
4. Run full CI/CD pipeline to confirm compatibility
5. Document the change in the PR description

---

## License Compliance Summary

| License | Count | Copyleft | Obligation |
|---------|-------|----------|------------|
| Apache-2.0 | 1 | No | Include license + NOTICE file |
| BSD-3-Clause | 4 | No | Include copyright + no-endorsement clause |
| MIT | 0-1 | No | Include copyright notice when the network export path pulls in MIT-licensed transitive components |

> **GPL contamination**: None detected. All documented dependencies are permissively licensed.
