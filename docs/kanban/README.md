# Monitoring System Kanban Board

This folder contains tickets for tracking improvement work on the Monitoring System.

**Last Updated**: 2025-11-23

---

## Ticket Status

### Summary

| Category | Total | Done | In Progress | Pending |
|----------|-------|------|-------------|---------|
| CORE | 4 | 0 | 0 | 4 |
| TEST | 5 | 0 | 0 | 5 |
| DOC | 4 | 0 | 0 | 4 |
| BUILD | 3 | 0 | 0 | 3 |
| PERF | 4 | 0 | 0 | 4 |
| **Total** | **20** | **0** | **0** | **20** |

---

## Ticket List

### CORE: Complete Core Features

Complete CircuitBreaker and platform metrics.

| ID | Title | Priority | Est. Duration | Dependencies | Status |
|----|-------|----------|---------------|--------------|--------|
| [MON-001](MON-001-circuit-breaker.md) | Complete CircuitBreaker Implementation | HIGH | 8h | - | TODO |
| [MON-004](MON-004-platform-metrics.md) | Implement Linux/Windows Platform Metrics | HIGH | 16h | - | TODO |
| [MON-005](MON-005-jaeger-zipkin.md) | Implement Jaeger/Zipkin Export | MEDIUM | 12h | - | TODO |
| [MON-008](MON-008-plugin-system.md) | Complete Plugin System Implementation | MEDIUM | 10h | - | TODO |

**Recommended Execution Order**: MON-001 → MON-004 → MON-005 → MON-008

---

### TEST: Expand Test Coverage

Activate disabled tests and improve coverage.

| ID | Title | Priority | Est. Duration | Dependencies | Status |
|----|-------|----------|---------------|--------------|--------|
| [MON-002](MON-002-test-activation.md) | Migrate 24 Disabled Tests | HIGH | 12h | - | TODO |
| [MON-007](MON-007-integration-tests.md) | Complete Integration Test Suite | MEDIUM | 10h | MON-002 | TODO |
| [MON-009](MON-009-coverage-90.md) | Achieve 90%+ Test Coverage (from 87%) | MEDIUM | 8h | MON-002 | TODO |
| [MON-014](MON-014-system-integration.md) | System Interface Integration Tests | MEDIUM | 6h | - | TODO |
| [MON-015](MON-015-tsan-verify.md) | ThreadSanitizer Data Race Verification | LOW | 4h | - | TODO |

**Recommended Execution Order**: MON-002 → MON-007 → MON-009 → MON-014 → MON-015

---

### DOC: Documentation Improvement

Write KNOWN_ISSUES and guides.

| ID | Title | Priority | Est. Duration | Dependencies | Status |
|----|-------|----------|---------------|--------------|--------|
| [MON-003](MON-003-known-issues.md) | Write KNOWN_ISSUES.md Document | HIGH | 4h | - | TODO |
| [MON-006](MON-006-korean-docs.md) | Define Korean Documentation Policy | MEDIUM | 6h | - | TODO |
| [MON-012](MON-012-examples-readme.md) | Write Example Program READMEs | LOW | 4h | - | TODO |
| [MON-016](MON-016-migration-guide.md) | Compatibility API Migration Guide | LOW | 3h | - | TODO |

---

### BUILD: Build & CI/CD Improvements

Clean up CMake options and strengthen automation.

| ID | Title | Priority | Est. Duration | Dependencies | Status |
|----|-------|----------|---------------|--------------|--------|
| [MON-010](MON-010-cmake-options.md) | Redefine CMake Option Consistency | MEDIUM | 5h | - | TODO |
| [MON-011](MON-011-benchmark-ci.md) | Automate Performance Benchmarks | LOW | 8h | - | TODO |
| [MON-020](MON-020-changelog.md) | Automate CHANGELOG Generation | LOW | 5h | - | TODO |

---

### PERF: Performance & Feature Improvements

Expand CircuitBreaker tag support and OpenTelemetry.

| ID | Title | Priority | Est. Duration | Dependencies | Status |
|----|-------|----------|---------------|--------------|--------|
| [MON-013](MON-013-otel-adapter.md) | Expand OpenTelemetry Adapter | LOW | 10h | - | TODO |
| [MON-017](MON-017-status-matrix.md) | Implementation Status Matrix Dashboard | LOW | 5h | - | TODO |
| [MON-018](MON-018-profiling-guide.md) | Improve Performance Profiling Guide | LOW | 3h | - | TODO |
| [MON-019](MON-019-circuit-tags.md) | Expand CircuitBreaker Tag Support | MEDIUM | 6h | MON-001 | TODO |

---

## Execution Plan

### Phase 1: Core Features (Weeks 1-2)
1. MON-001: Complete CircuitBreaker Implementation
2. MON-003: Write KNOWN_ISSUES.md
3. MON-010: Clean up CMake Options

### Phase 2: Test Activation (Weeks 2-3)
1. MON-002: Activate 24 Tests
2. MON-004: Implement Platform Metrics
3. MON-007: Complete Integration Tests

### Phase 3: Documentation & Stabilization (Weeks 3-4)
1. MON-009: Achieve 90%+ Coverage
2. MON-014: System Integration Tests
3. MON-006: Korean Documentation Policy

### Phase 4: Additional Features (Week 4+)
1. MON-005: Jaeger/Zipkin Export
2. MON-008: Plugin System
3. MON-011: Benchmark Automation

---

## Status Definitions

- **TODO**: Not yet started
- **IN_PROGRESS**: Work in progress
- **REVIEW**: Awaiting code review
- **DONE**: Completed

---

**Maintainer**: TBD
**Contact**: Use issue tracker
