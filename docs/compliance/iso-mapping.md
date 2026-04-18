---
doc_id: "MON-COMP-001"
doc_title: "ISO/IEC 27001 and 20000 Compliance Mapping"
doc_version: "1.0.0"
doc_date: "2026-04-18"
doc_status: "Released"
project: "monitoring_system"
category: "COMP"
---

# ISO/IEC 27001 and ISO/IEC 20000 Compliance Mapping

> **SSOT**: This document is the single source of truth for **ISO/IEC 27001 and
> ISO/IEC 20000 control mappings** in `monitoring_system`.

> **Language:** **English**

## Table of Contents

- [Purpose and Scope](#purpose-and-scope)
  - [What This Mapping Claims](#what-this-mapping-claims)
  - [What This Mapping Does Not Claim](#what-this-mapping-does-not-claim)
  - [Audience](#audience)
- [Reference Standards](#reference-standards)
- [ISO/IEC 27001:2022 Annex A Mapping](#isoiec-270012022-annex-a-mapping)
  - [A.5 Organizational Controls](#a5-organizational-controls)
  - [A.8 Technological Controls](#a8-technological-controls)
  - [Summary Table - ISO 27001](#summary-table---iso-27001)
- [ISO/IEC 20000-1:2018 Service Management Mapping](#isoiec-20000-12018-service-management-mapping)
  - [Clause 8.3 Service Design, Build and Transition](#clause-83-service-design-build-and-transition)
  - [Clause 8.4 Resolution and Fulfilment](#clause-84-resolution-and-fulfilment)
  - [Clause 8.5 Service Assurance](#clause-85-service-assurance)
  - [Clause 8.6 Service Reporting](#clause-86-service-reporting)
  - [Summary Table - ISO 20000](#summary-table---iso-20000)
- [Known Gaps](#known-gaps)
- [Operator Responsibilities](#operator-responsibilities)
- [Change History](#change-history)

## Purpose and Scope

`monitoring_system` is a C++20 observability library that provides metrics
collection, distributed tracing, health checks, alerting, and reliability
patterns. This document maps the features it currently ships against the
control and process clauses of **ISO/IEC 27001:2022** (information security
management) and **ISO/IEC 20000-1:2018** (IT service management) so that
operators integrating the library into a broader management system can
identify where it contributes to compliance evidence and where additional
controls must come from elsewhere.

### What This Mapping Claims

- Specific `monitoring_system` features **help satisfy** named ISO clauses when
  the operator wires them into an existing Information Security Management
  System (ISMS) or Service Management System (SMS).
- Each mapped feature is traceable to a public header or documented subsystem
  under `include/kcenon/monitoring/`.
- The mapping identifies gaps where additional organisational, procedural, or
  infrastructure controls are required.

### What This Mapping Does Not Claim

- `monitoring_system` is **not** by itself a certified ISMS or SMS. Certification
  is granted to an organisation, not to a library.
- Installing the library does **not** constitute compliance with any ISO
  standard. Compliance requires policies, procedures, management review,
  internal audits, training, risk treatment, and other organisational
  elements outside the scope of this code base.
- The mapping is not a legal or audit opinion. Operators should consult their
  own auditors and legal counsel before relying on it for certification
  evidence.
- Features described as "help satisfy" a clause **partially** address the
  technical control surface; the operator remains responsible for the
  management, policy, and review layers around them.

### Audience

- Platform, SRE, and security engineers who need to justify the inclusion of
  `monitoring_system` in a certified environment.
- Compliance officers and auditors evaluating whether the library's controls
  map to required evidence.
- Product teams planning control coverage across the observability stack.

## Reference Standards

| Standard | Title | Edition |
|----------|-------|---------|
| ISO/IEC 27001 | Information security, cybersecurity and privacy protection - Information security management systems - Requirements | 2022 |
| ISO/IEC 20000-1 | Information technology - Service management - Part 1: Service management system requirements | 2018 |

Clause numbering below follows these editions. ISO/IEC 27001:2022 reorganised
Annex A into four themed groups; this document uses the 2022 themes.

## ISO/IEC 27001:2022 Annex A Mapping

### A.5 Organizational Controls

| Control | Title | How monitoring_system helps | Feature references |
|---------|-------|----------------------------|--------------------|
| A.5.7 | Threat intelligence | Security event collector records authentication failures, sudo usage, and permission changes, producing time-series signals that feed threat-intelligence pipelines. | `collectors/security_collector.h` |
| A.5.23 | Information security for use of cloud services | Exporters (OTLP, Jaeger, Zipkin, OpenTelemetry Collector sidecar) integrate with cloud-hosted observability backends under operator-controlled transport settings. | `exporters/trace_exporters.h`, `exporters/otlp_grpc_exporter.h`, `docs/guides/OTEL_COLLECTOR_SIDECAR.md` |
| A.5.25 | Assessment and decision on information security events | Alert pipeline classifies, routes, and suppresses events so that operators can triage signals with defined severity and deduplication. | `alert/alert_manager.h`, `alert/alert_pipeline.h`, `alert/alert_triggers.h` |
| A.5.26 | Response to information security incidents | Alert notifiers emit events to downstream incident channels (log, webhook-style sinks); reliability components surface degradation state for responders. | `alert/alert_notifiers.h`, `reliability/circuit_breaker.h`, `reliability/graceful_degradation.h` |
| A.5.27 | Learning from information security incidents | Historical metrics and traces are retained in configurable time-series storage for post-incident review. | `utils/time_series.h`, `utils/metric_storage.h`, `storage/storage_backends.h` |
| A.5.30 | ICT readiness for business continuity | Health monitoring, circuit breakers, retry policies, and graceful degradation give operators programmable responses to partial outages. | `health/health_monitor.h`, `reliability/circuit_breaker.h`, `reliability/retry_policy.h`, `reliability/graceful_degradation.h`, `reliability/fault_tolerance_manager.h` |

### A.8 Technological Controls

| Control | Title | How monitoring_system helps | Feature references |
|---------|-------|----------------------------|--------------------|
| A.8.8 | Management of technical vulnerabilities | Static-analysis, sanitizer, and SOUP baselines are published with the release; CI runs CVE scanning and SBOM generation to surface vulnerable dependencies. | `docs/SOUP.md`, `docs/performance/STATIC_ANALYSIS_BASELINE.md`, `docs/performance/SANITIZER_BASELINE.md`, `.github/workflows/` (ci, static-analysis, coverage) |
| A.8.15 | Logging | Performance monitor and collectors emit structured metrics; the library integrates with `logger_system` for operational logs and produces tracing spans with correlation IDs. | `core/performance_monitor.h`, `tracing/distributed_tracer.h`, `collectors/logger_system_collector.h` |
| A.8.16 | Monitoring activities | Central collector aggregates system, process, network, security, and custom metrics; alert pipeline and health monitor watch for anomalous state. | `factory/metric_factory` (see `include/kcenon/monitoring/factory/`), `core/central_collector.h`, `health/health_monitor.h`, `alert/alert_pipeline.h` |
| A.8.17 | Clock synchronization | Distributed tracer and time-series storage use `std::chrono::system_clock` timestamps. Accurate synchronization must be provided by the host (NTP / PTP); the library records but does not enforce time sync. | `tracing/distributed_tracer.h`, `utils/time_series.h` |
| A.8.20 | Networks security | Exporter transports support TLS when configured by the operator (HTTP/gRPC) and document deployment patterns (OpenTelemetry Collector sidecar) for network segmentation. | `exporters/http_transport.h`, `exporters/grpc_transport.h`, `docs/guides/OTEL_COLLECTOR_SIDECAR.md`, `docs/guides/SECURITY.md` |
| A.8.23 | Web filtering | Not in scope - monitoring_system does not act as a web proxy or filter. | - |
| A.8.28 | Secure coding | Project follows C++20, RAII, smart pointers; coverage, static analysis, sanitizer (ASAN/TSAN/UBSAN) runs, and code-review rules are enforced in CI. | `CONTRIBUTING.md`, `docs/contributing/TESTING_GUIDE.md`, `.github/workflows/ci.yml`, `docs/performance/SANITIZER_BASELINE.md` |

### Summary Table - ISO 27001

| Clause theme | Help-satisfy coverage | Out of scope or operator-provided |
|--------------|----------------------|-----------------------------------|
| A.5 Organizational | A.5.7, A.5.23, A.5.25, A.5.26, A.5.27, A.5.30 | A.5.1-A.5.6 (policy, roles), A.5.8-A.5.22 (supplier, asset, classification), A.5.24 (incident planning), A.5.28-A.5.29 (evidence collection, continuity planning), A.5.31-A.5.37 (legal, intellectual property, records, privacy, documented procedures) |
| A.6 People | - | All A.6 controls (screening, terms, awareness, disciplinary) are organisational and out of scope. |
| A.7 Physical | - | All A.7 controls are physical and out of scope. |
| A.8 Technological | A.8.8, A.8.15, A.8.16, A.8.17 (partial), A.8.20 (partial), A.8.28 | A.8.1-A.8.7 (endpoint, privilege, access), A.8.9-A.8.14 (config, data, identity, authentication, capacity, redundancy), A.8.18-A.8.19 (privileged utilities, installed software), A.8.21-A.8.22 (service security, segregation), A.8.24-A.8.27 (cryptography, SDLC, change, outsourcing), A.8.29-A.8.34 (security testing, supplier, specification, audit) |

## ISO/IEC 20000-1:2018 Service Management Mapping

ISO/IEC 20000-1:2018 is a process-oriented standard. The mapping below
focuses on the service operation processes most directly enabled by an
observability library.

### Clause 8.3 Service Design, Build and Transition

| Clause | Process | How monitoring_system helps | Feature references |
|--------|---------|----------------------------|--------------------|
| 8.3.3 | Change management | Metrics and traces emitted before/after a change give the operator data for pre/post-change comparison. The library does not manage change tickets. | `core/performance_monitor.h`, `tracing/distributed_tracer.h` |
| 8.3.4 | Release and deployment management | CI pipelines (coverage, static analysis, sanitizers, benchmarks) produce per-release evidence. Release information is published in `CHANGELOG.md` and the documentation registry. | `docs/CHANGELOG.md`, `docs/PRODUCTION_QUALITY.md`, `docs/README.md` |

### Clause 8.4 Resolution and Fulfilment

| Clause | Process | How monitoring_system helps | Feature references |
|--------|---------|----------------------------|--------------------|
| 8.4.2 | Incident management | Alert pipeline raises, routes, deduplicates, and escalates incidents; distributed tracing provides the request-path context required for investigation; reliability patterns help contain blast radius. | `alert/alert_manager.h`, `alert/alert_pipeline.h`, `alert/alert_notifiers.h`, `tracing/distributed_tracer.h`, `reliability/circuit_breaker.h`, `reliability/error_boundary.h` |
| 8.4.3 | Service request management | Not in scope - monitoring_system is not a request-fulfilment workflow engine. | - |
| 8.4.4 | Problem management | Retained time-series metrics, traces, and health state allow trend analysis and root-cause investigation across recurring incidents. | `utils/time_series.h`, `utils/metric_storage.h`, `storage/storage_backends.h`, `tracing/distributed_tracer.h` |

### Clause 8.5 Service Assurance

| Clause | Process | How monitoring_system helps | Feature references |
|--------|---------|----------------------------|--------------------|
| 8.5.1 | Service availability management | Health monitor and uptime collector produce availability signals; circuit breakers and graceful degradation keep partial functionality available during failure. | `health/health_monitor.h`, `collectors/uptime_collector.h`, `reliability/circuit_breaker.h`, `reliability/graceful_degradation.h` |
| 8.5.2 | Service continuity management | Reliability patterns (retry, fault-tolerance manager, resource manager) give deterministic behaviour during dependency loss; exporter fan-out allows redundant telemetry sinks. | `reliability/retry_policy.h`, `reliability/fault_tolerance_manager.h`, `reliability/resource_manager.h`, `exporters/trace_exporters.h` |
| 8.5.3 | Capacity management | Process, system, network, GPU, SMART disk, inode, file-descriptor, and container collectors expose utilisation signals used for capacity planning. | `collectors/system_resource_collector.h`, `collectors/process_metrics_collector.h`, `collectors/network_metrics_collector.h`, `collectors/gpu_collector.h`, `collectors/smart_collector.h`, `collectors/container_collector.h` |
| 8.5.4 | Information security management | See ISO 27001 mapping above. The library contributes the detection and telemetry layer, not the organisational controls. | - |

### Clause 8.6 Service Reporting

| Clause | Process | How monitoring_system helps | Feature references |
|--------|---------|----------------------------|--------------------|
| 8.6 | Service reporting | Metric exporters (Prometheus, StatsD, InfluxDB, Graphite) and trace exporters (Jaeger, Zipkin, OTLP) feed external dashboards and report generators; the library itself does not generate management reports. | `exporters/metric_exporters.h`, `exporters/trace_exporters.h`, `exporters/otlp_grpc_exporter.h`, `exporters/opentelemetry_adapter.h` |

### Summary Table - ISO 20000

| Clause | Help-satisfy coverage | Out of scope |
|--------|----------------------|--------------|
| 4 Context, 5 Leadership, 6 Planning, 7 Support, 9 Performance evaluation, 10 Improvement | - | Organisational / management-system clauses not addressed by a library. |
| 8.1 Operational planning, 8.2 Service portfolio | - | Management and portfolio clauses are operator-owned. |
| 8.3 Service design, build and transition | Partial (8.3.3, 8.3.4) | 8.3.1 service planning, 8.3.2 service catalogue, 8.3.5 information security in service transition (operator-owned parts). |
| 8.4 Resolution and fulfilment | Partial (8.4.2, 8.4.4) | 8.4.1 general, 8.4.3 service request management. |
| 8.5 Service assurance | 8.5.1, 8.5.2, 8.5.3 (partial); 8.5.4 via ISO 27001 mapping | 8.5 governance remains operator-owned. |
| 8.6 Service reporting | Partial (exporter-driven feed) | Report generation, SLA definition, management reporting are operator-owned. |
| 8.7 Service level management, 8.8 Supplier relationships, 8.9 Budgeting and accounting for services | - | Commercial and contract-level clauses. |

## Known Gaps

The items below are **not** provided by `monitoring_system` today and must be
supplied by surrounding systems, procedures, or future work to achieve full
compliance evidence.

1. **Privileged access auditing for the monitoring pipeline.** The library
   does not itself authenticate operators who configure collectors, alerts,
   or exporters. Access control to the host process, configuration files, and
   exporter endpoints is operator-owned (see `docs/guides/SECURITY.md`).
2. **Tamper-evident / signed audit logs.** The security event collector and
   alert pipeline produce structured records, but the library does not sign
   or cryptographically seal them. Operators needing ISO 27001 A.8.15 and
   A.5.27 tamper-evidence should forward records to a write-once,
   integrity-protected sink.
3. **Data retention policy enforcement across backends.** `utils/time_series.h`
   and `utils/metric_storage.h` expose a `retention_period` configuration,
   but long-term retention, legal-hold, and disposal workflows must be
   implemented by the downstream storage backend.
4. **Configuration change auditing.** The library does not record who changed
   which configuration value at what time; this should be handled by the
   operator's configuration-management tooling (GitOps, signed commits).
5. **Data classification and privacy tagging.** Metric and trace payloads are
   not automatically classified. Operators must avoid recording personal or
   regulated data, or must add classification and redaction before export.
6. **Cryptographic transport defaults.** Exporter transports support TLS but
   do not require it at the type level. Operator deployment guidance (see
   `docs/guides/SECURITY.md`) must enforce TLS-only configuration.
7. **Formal SLA / SLO management.** Service-level targets, reporting cadence,
   and management review for ISO/IEC 20000-1 clauses 8.6 and 8.7 are
   outside the library; it provides the raw signals only.
8. **Windows security event source.** `collectors/security_collector.h`
   implements Linux and macOS event sources; Windows Event Log integration
   is a stub. Organisations relying on A.5.7 / A.5.25 on Windows hosts
   must supply this.
9. **End-to-end exporter integration tests.** Jaeger and Zipkin protobuf
   wire-format encoders are implemented, but full Docker-based end-to-end
   tests against `jaegertracing/all-in-one` and `openzipkin/zipkin` remain a
   follow-up (see `CLAUDE.md` "Known Constraints").
10. **ISMS and SMS artefacts.** Policies, risk register, statement of
    applicability, management review minutes, internal audit records,
    training records, and supplier assessments are entirely operator-owned.

## Operator Responsibilities

To translate this mapping into certification evidence, the operator must at a
minimum:

- Maintain the ISMS / SMS governance layer (policy, scope, objectives, risk
  treatment plan, statement of applicability).
- Configure the library's transport, retention, and alerting features per
  the `docs/guides/SECURITY.md` hardening guide and their own information
  classification policy.
- Forward monitoring records to a retention-controlled, tamper-evident sink.
- Run regular management reviews and internal audits against the mapping in
  this document, re-verifying feature presence with each release.
- Keep this document in sync with the library version deployed (see
  `doc_version` and `doc_date` in the front matter).

## Change History

| Version | Date | Change |
|---------|------|--------|
| 1.0.0 | 2026-04-18 | Initial mapping for ISO/IEC 27001:2022 and ISO/IEC 20000-1:2018. |

---

*See also: [Security Policy](../guides/SECURITY.md), [Known Issues](../KNOWN_ISSUES.md), [SOUP List](../SOUP.md), [Production Quality](../PRODUCTION_QUALITY.md).*
