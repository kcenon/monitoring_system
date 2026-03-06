# SOUP List &mdash; monitoring_system

> **Software of Unknown Provenance (SOUP) Register per IEC 62304:2006+AMD1:2015 &sect;8.1.2**
>
> This document is the authoritative reference for all external software dependencies.
> Every entry must include: title, manufacturer, unique version identifier, license, and known anomalies.

| Document | Version |
|----------|---------|
| IEC 62304 Reference | &sect;8.1.2 Software items from SOUP |
| Last Reviewed | 2026-03-06 |
| monitoring_system Version | 2.0.0 |

---

## Internal Ecosystem Dependencies

| ID | Name | Manufacturer | Version | License | Usage | Safety Class | Known Anomalies |
|----|------|-------------|---------|---------|-------|-------------|-----------------|
| INT-001 | [common_system](https://github.com/kcenon/common_system) | kcenon | Latest (vcpkg / source) | BSD-3-Clause | Result&lt;T&gt; pattern, error handling primitives | B | None |
| INT-002 | [thread_system](https://github.com/kcenon/thread_system) | kcenon | Latest (vcpkg / source) | BSD-3-Clause | Thread pool, async task scheduling for metric collection | B | None |

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

### Logging Feature (`logging`)

| ID | Name | Manufacturer | Version | License | Usage | Safety Class | Known Anomalies |
|----|------|-------------|---------|---------|-------|-------------|-----------------|
| INT-003 | [logger_system](https://github.com/kcenon/logger_system) | kcenon | Latest (vcpkg / source) | BSD-3-Clause | Audit logging integration (optional) | A | None |

### Network Feature (optional)

| ID | Name | Manufacturer | Version | License | Usage | Safety Class | Known Anomalies |
|----|------|-------------|---------|---------|-------|-------------|-----------------|
| INT-004 | [network_system](https://github.com/kcenon/network_system) | kcenon | Latest (source) | BSD-3-Clause | Network transport for distributed monitoring (optional) | B | None |

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

## Version Pinning (IEC 62304 Compliance)

All SOUP versions are pinned in `vcpkg.json` via the `overrides` field:

```json
{
  "overrides": [
    { "name": "grpc", "version": "1.51.1" },
    { "name": "protobuf", "version": "3.21.12" },
    { "name": "gtest", "version": "1.14.0" }
  ]
}
```

The vcpkg baseline is locked in `vcpkg-configuration.json` to ensure reproducible builds.

---

## Version Update Process

When updating any SOUP dependency:

1. Update the version in `vcpkg.json` (overrides section)
2. Update the corresponding row in this document
3. Verify no new known anomalies (check CVE databases)
4. Run full CI/CD pipeline to confirm compatibility
5. Document the change in the PR description

---

## License Compliance Summary

| License | Count | Copyleft | Obligation |
|---------|-------|----------|------------|
| Apache-2.0 | 1 | No | Include license + NOTICE file |
| BSD-3-Clause | 1 | No | Include copyright + no-endorsement clause |

> **GPL contamination**: None detected. All dependencies are permissively licensed.
