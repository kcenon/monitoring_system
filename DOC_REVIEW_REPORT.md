# Document Review Report - monitoring_system

**Generated**: 2026-04-14
**Mode**: Report-only (no source files modified)
**Files analyzed**: 104 markdown files
**Headings indexed**: 3,369
**Links validated**: 1,387

## Findings Summary

| Severity | Phase 1 | Phase 2 | Phase 3 | Total |
|----------|---------|---------|---------|-------|
| Must-Fix | 138 | 3 | 2 | 143 |
| Should-Fix | 0 | 5 | 5 | 10 |
| Nice-to-Have | 0 | 3 | 6 | 9 |
| **Total** | **138** | **11** | **13** | **162** |

Phase 1 anchor/link breakdown:
- Broken file references: 112
- Broken intra-file anchors: 26
- Affected files: 40 (38% of corpus)

---

## Must-Fix Items

### Phase 1 - Broken Links / Missing Files (112 items, top recurring targets)

Most-referenced missing files (counts indicate reference density):

1. `ARCHITECTURE_GUIDE.md` - 7 refs from `docs/CHANGELOG.md:768`, `docs/CHANGELOG.kr.md:724`, `docs/README.kr.md:24,35`, `docs/API_REFERENCE.kr.md:484` (Phase 1)
2. `guides/USER_GUIDE.md` - 5 refs from `docs/BENCHMARKS.md:485`, `docs/FEATURES.md:30`, `docs/FEATURES_CORE.md:1173`, `docs/PRODUCTION_QUALITY.md:594`, `docs/PROJECT_STRUCTURE.kr.md:477` (Phase 1)
3. `SECURITY.md` (under docs/) - 5 refs from `docs/CHANGELOG.md:774`, `docs/CHANGELOG.kr.md:730`, `docs/README.kr.md:28,169`, `docs/guides/SECURITY.md` cross-doc context (Phase 1)
4. `01-ARCHITECTURE.md` - 4 refs from `docs/BENCHMARKS.md:483`, `docs/PRODUCTION_QUALITY.md:591`, `docs/PRODUCTION_QUALITY.kr.md:325`, `docs/PROJECT_STRUCTURE.kr.md:473` (Phase 1)
5. `02-API_REFERENCE.md` - 4 refs from `docs/BENCHMARKS.md:484`, `docs/PRODUCTION_QUALITY.md:593`, `docs/PRODUCTION_QUALITY.kr.md:327`, `docs/PROJECT_STRUCTURE.kr.md:474` (Phase 1)
6. `API_REFERENCE.md` (as plain relative, not at doc root level) - 4 refs from `docs/advanced/ARCHITECTURE_GUIDE.md:810`, `docs/advanced/ARCHITECTURE_GUIDE.kr.md:810`, `docs/guides/TROUBLESHOOTING.md:731`, `docs/guides/TROUBLESHOOTING.kr.md:731` (Phase 1)
7. `../../PHASE3_VERIFICATION_REPORT.md` - 4 refs from `docs/advanced/MIGRATION_GUIDE_V2.md:520,591` and `.kr.md:528,599` (Phase 1)
8. `../CONTRIBUTING.md` - 4 refs from `docs/contributing/CI_CD_GUIDE.md:954`, `docs/guides/BEST_PRACTICES.md:1197`, `docs/guides/FAQ.md:998`, `docs/guides/QUICK_START.md:660` (Phase 1)
9. `../TROUBLESHOOTING.md` - 4 refs from `docs/contributing/CI_CD_GUIDE.md:956`, `docs/guides/BEST_PRACTICES.md:1192`, `docs/guides/FAQ.md:993`, `docs/guides/QUICK_START.md:651` (Phase 1)
10. `PHASE4.md` - 3 refs from `docs/API_REFERENCE.md:46`, `docs/API_REFERENCE.kr.md:483`, `docs/API_REFERENCE_CORE.md:669` (Phase 1)
11. `CONTRIBUTING.md` (referenced in docs/ with no `../` prefix) - 3 refs from `docs/README.kr.md:27,50,144` (Phase 1)
12. `TROUBLESHOOTING.md` (docs/-relative, target missing) - 3 refs from `docs/README.kr.md:32,46,57` (Phase 1)
13. `../ARCHITECTURE_GUIDE.md` - 3 refs from `docs/guides/FAQ.md:991`, `docs/guides/QUICK_START.md:647`, `docs/guides/QUICK_START.kr.md:195` (Phase 1)
14. `README.md:94` - `docs/guides/QUICK_START_KO.md` (should be `QUICK_START.kr.md`) (Phase 1)
15. `README.md:116` - `../ECOSYSTEM.md` resolves outside repo root (Phase 1)
16. `README.md:189,386` - `docs/guides/USER_GUIDE.md` (does not exist) (Phase 1)
17. `README.md:268,391,394` - `docs/02-API_REFERENCE.md`, `docs/01-ARCHITECTURE.md` (legacy numbered names) (Phase 1)
18. `README.md:401` - `docs/guides/MIGRATION_GUIDE.md` (should likely be `docs/advanced/MIGRATION_GUIDE_V2.md`) (Phase 1)
19. `docs/plugin_development_guide.md:820` - `migration_guide.md` not found (Phase 1)
20. `docs/advanced/MIGRATION.md:15` - `MIGRATION.kr.md` not present (Phase 1)
21. `docs/contributing/CI_CD_GUIDE.md:955` - `../TESTING_GUIDE.md` (belongs under `../contributing/TESTING_GUIDE.md`) (Phase 1)
22. `docs/integration/README.md:21-25` - `with-common-system.md`, `with-thread-system.md`, `with-logger.md`, `with-network-system.md`, `with-database-system.md` (none exist) (Phase 1)
23. `docs/integration/README.md:148` - `../../../ECOSYSTEM.md` (wrong depth) (Phase 1)
24. `docs/guides/PORT_MANAGEMENT.md:294` - `../../.github/workflows/sync-vcpkg-registry.yml` (file missing) (Phase 1)
25. `docs/guides/INTEGRATION.md:15` - `INTEGRATION.kr.md` referenced but not created (Phase 1)
26. `docs/guides/INTEGRATION.md:538` - `examples/` (outside scope of md path) (Phase 1)
27. `docs/guides/INTEGRATION.md:565-567` - `docs/`, `docs/API_REFERENCE.md`, `docs/ARCHITECTURE.md` (wrong relative path from `docs/guides/`) (Phase 1)
28. `docs/guides/COLLECTOR_DEVELOPMENT.md:15` - `COLLECTOR_DEVELOPMENT.kr.md` referenced but absent (Phase 1)
29. `docs/guides/ALERT_PIPELINE.md:318` - `../performance/TUNING.md` (should be `PERFORMANCE_TUNING.md`) (Phase 1)
30. `benchmarks/README.md:296` - `../docs/LOGGER_SYSTEM_ARCHITECTURE.md` (belongs to logger_system repo) (Phase 1)
31. `docs/guides/TUTORIAL.md:153,241,335,515` (+ `.kr.md` pair) - `basic_monitoring_example.cpp`, `distributed_tracing_example.cpp`, `health_reliability_example.cpp`, `result_pattern_example.cpp` (all relative to tutorial page but files don't exist) (Phase 1)
32. `docs/guides/TUTORIAL.md:759,765-767,774,776` (+ `.kr.md`) - `../docs/TROUBLESHOOTING.md`, `../docs/API_REFERENCE.md`, `../docs/ARCHITECTURE_GUIDE.md`, `../docs/PERFORMANCE_TUNING.md`, `../docs/`, `../tests/` (paths wrong; TUTORIAL.md is already inside `docs/guides/`) (Phase 1)
33. `docs/advanced/MIGRATION_GUIDE_V2.md:592` and `.kr.md:600` - `../examples/bidirectional_di_example.cpp` (file absent) (Phase 1)
34. `docs/advanced/ARCHITECTURE_ISSUES.md:311` and `.kr.md:287` - `./API_REFERENCE.md` (wrong path from `docs/advanced/`) (Phase 1)
35. `docs/guides/DI_AND_CONCEPTS.md:1424`, `docs/guides/PERFORMANCE_COOKBOOK.md:1682` - `../plugins/PLUGIN_DEVELOPMENT.md` (no such directory) (Phase 1)

### Phase 1 - Broken Intra-File Anchors (26 items)

36. `README.md:30` - `#contributing` (no matching heading; nearest is `## Support & Contributing` -> `#support--contributing`) (Phase 1)
37. `docs/advanced/ARCHITECTURE_GUIDE.md:29-32` - ToC entries `#design-patterns`, `#test-architecture`, `#build-and-integration`, `#future-architecture` do not match actual headings `## Core Design Patterns`, missing `## Test Architecture` and `## Build and Integration`, `## Future Architecture Directions` (Phase 1)
38. `docs/advanced/ARCHITECTURE_GUIDE.kr.md:29-32` - Korean ToC entries `#설계-패턴`, `#테스트-아키텍처`, `#빌드-및-통합`, `#미래-아키텍처` do not match actual headings (Phase 1)
39. `docs/advanced/ARCHITECTURE_ISSUES.md:21-36` - ToC uses single-dash slugs (`#1-testing-quality`, `#2-concurrency-thread-safety`, `#3-performance-optimization`, `#4-features-functionality`, `#issue-arc-002-missing-performance-benchmarks`, `#issue-arc-003-...`, `#issue-arc-004-...`, `#issue-arc-005-...`, `#issue-arc-006-...`, `#issue-arc-007-...`, `#issue-arc-009-...`, `#issue-arc-010-...`) but actual headings contain `&` (produces double-dashes) and strikethrough `~~...~~ ✅ RESOLVED` (produces extra suffix). 12 anchors in this one file. (Phase 1)
40. `docs/advanced/ARCHITECTURE_ISSUES.kr.md:25,28,31,34,36` - Same strikethrough/emoji problem for 5 Korean ToC entries. (Phase 1)

### Phase 2 - Factual Errors (Must-Fix)

41. `docs/API_REFERENCE_CORE.md:457` - States "Compatible with: GCC 10+, Clang 10+, MSVC 2019 16.3+" which contradicts the authoritative policy in `CLAUDE.md:96` and `README.md:51,531-533` that require GCC 13+/Clang 17+/MSVC 2022+/Apple Clang 14+. (Phase 2)
42. `docs/CHANGELOG.md:526` and `docs/CHANGELOG.kr.md:482` - Record "Updated compiler requirements: GCC 10+, Clang 10+, MSVC 2019 16.3+". Either historical changelog entry needs a follow-up entry recording the later bump to GCC 13+/Clang 17+, or this line is stale. (Phase 2)
43. `docs/guides/FAQ.md:81-84` - Lists minimum compilers as "GCC 11+, Clang 14+, MSVC 2019 16.11+, Apple Clang 13+" - all below the authoritative floors. Users following FAQ will face build failures. (Phase 2)

### Phase 3 - SSOT Contradictions (Must-Fix)

44. Version string SSOT inconsistency: the authoritative index docs declare version `0.4.0.0` (FEATURES.md, API_REFERENCE.md, ARCHITECTURE.md, FEATURES_CORE.md, FEATURES_COLLECTORS.md, FEATURES_ALERTS_TRACING.md, guides/COLLECTOR_DEVELOPMENT.md), but peer documents claim conflicting versions:
    - `0.1.0` in `docs/guides/STREAM_PROCESSING.md:15`, `docs/guides/DISTRIBUTED_TRACING.md:15`, `docs/guides/ADVANCED_ALERTS.md:15`
    - `0.1.0.0` in `docs/PRODUCTION_QUALITY.md:15`, `docs/BENCHMARKS.md:15`, `docs/advanced/INTERFACE_SEPARATION_STRATEGY.md:15`, `docs/performance/BASELINE.kr.md:18`, `docs/performance/STATIC_ANALYSIS_BASELINE.md:18`
    - `0.2.0.0` in `docs/KNOWN_ISSUES.md:15`
    - `0.3.0.0` in `docs/PROJECT_STRUCTURE.md:15`
    - `1.0.0` in `docs/guides/STORAGE_BACKENDS.md:15`, `docs/guides/EXPORTER_DEVELOPMENT.md:15`, `docs/guides/RELIABILITY_PATTERNS.md:15` (Phase 3)
45. Split-index SSOT boundary mismatch between FEATURES and API_REFERENCE splits: the FEATURES.md index states FEATURES_CORE.md contains "health monitoring, reliability patterns, storage backends" (lines 22-24), but the API_REFERENCE.md index states API_REFERENCE_ALERTS_EXPORT.md contains "Health monitoring, storage backends, stream processing, reliability features" (line 26). Same topics live in different split boundaries across the two parallel hierarchies. The FEATURES_ALERTS_TRACING.md table of contents (H2 inspection) lists only `## Alert Pipeline` and `## Distributed Tracing`, confirming the asymmetry. Either:
    - Move `## Health Monitoring`, `## Storage Backends`, and `## Reliability Patterns` out of FEATURES_CORE.md into FEATURES_ALERTS_TRACING.md (or a FEATURES_RELIABILITY.md) to mirror API_REFERENCE_ALERTS_EXPORT.md, **or**
    - Rename API_REFERENCE_ALERTS_EXPORT.md to API_REFERENCE_RELIABILITY_TRACING_EXPORT.md and re-split content to match FEATURES_CORE.md. (Phase 3)

---

## Should-Fix Items

### Phase 2 - Terminology / Accuracy

46. `docs/PRODUCTION_QUALITY.md:58-60` and `.kr.md:46-48` - Test matrix still advertises Ubuntu 22.04 GCC 11 / Clang 14 and Apple Clang 14 as the tested toolchains, which is below the documented required floors (GCC 13+/Clang 17+). Either CI is running below requirements, or the "tested matrix" is stale. (Phase 2)
47. `docs/advanced/ARCHITECTURE.md:550,598` - "Ubuntu (GCC 11+, Clang 14+)" and "Compilers: GCC 11+, Clang 14+, MSVC 2019+". Out of sync with README.md. (Phase 2)
48. `docs/advanced/CURRENT_STATE.md:47-93` and `.kr.md:47-93` - Reports tested configurations as "Ubuntu 22.04 (GCC 12, Clang 15)". Inconsistent with the required-GCC-13/Clang-17 claim. (Phase 2)
49. `docs/contributing/CONTRIBUTING.md:34`, `docs/contributing/CONTRIBUTING.kr.md:34`, `CONTRIBUTING.md:18`, `docs/guides/QUICK_START.md:39`, `docs/guides/QUICK_START.kr.md:41`, `docs/guides/DI_AND_CONCEPTS.md:949` - Various contributor-facing mentions of C++20 minimum compilers use the lower floor (GCC 10/11+, Clang 12/14+, MSVC 2019+). (Phase 2)
50. `docs/guides/OTEL_COLLECTOR_SIDECAR.md:424` and `.kr.md:424` - Kubernetes example uses `apiVersion: opentelemetry.io/v1alpha1`. OpenTelemetry Operator stabilised to `opentelemetry.io/v1beta1` in 2024; `v1alpha1` is still accepted but deprecated. Recommend updating example to `v1beta1` and noting the migration. (Phase 2)

### Phase 3 - Missing / Incomplete Cross-References

51. Orphan docs (not linked from any other markdown in the corpus): `benchmarks/README.md`, `integration_tests/README.md`, `docs/ECOSYSTEM.md`, `docs/GETTING_STARTED.md`. `README.md` mentions `ECOSYSTEM` and `benchmarks/` but not via proper relative links; `GETTING_STARTED.md` is not referenced anywhere; `integration_tests/README.md` has no incoming link. (Phase 3)
52. `README.md` does not link to the top-level `CHANGELOG.md`, `SECURITY.md`, or `CODE_OF_CONDUCT.md`, breaking the GitHub convention that the root README surfaces these. (Phase 3)
53. KR/EN parity gap: 25 pairs have both languages but 54 documents have only an English version. Notable EN-only files that would benefit from Korean counterparts or explicit "EN only" notice: `docs/guides/BEST_PRACTICES.md`, `docs/guides/FAQ.md`, `docs/guides/ALERT_PIPELINE.md`, `docs/guides/PERFORMANCE_COOKBOOK.md`, `docs/guides/RELIABILITY_PATTERNS.md`, `docs/guides/STREAM_PROCESSING.md`, `docs/guides/STORAGE_BACKENDS.md`, `docs/guides/EXPORTER_DEVELOPMENT.md`, `docs/guides/COLLECTOR_DEVELOPMENT.md`, `docs/guides/DISTRIBUTED_TRACING.md`, `docs/guides/ADVANCED_ALERTS.md`, `docs/guides/DI_AND_CONCEPTS.md`. The repo pattern clearly aims for bilingual parity given most top-level and primary guides are paired. (Phase 3)
54. `docs/README.md`, `docs/README.kr.md`, `README.md` do not back-link to `docs/TRACEABILITY.md` even though TRACEABILITY.md is SSOT for Feature-Test-Module mapping. (Phase 3)
55. `docs/integration/README.md` is a stub referencing five non-existent sub-guides (`with-common-system.md`, `with-thread-system.md`, `with-logger.md`, `with-network-system.md`, `with-database-system.md`) - this entire sub-directory is effectively dead. Either populate the files or mark the directory as planned/TBD. (Phase 3)

---

## Nice-to-Have Items

### Phase 2 - Style / Minor

56. `docs/guides/OTEL_COLLECTOR_SIDECAR.md:590` has `**Last Updated**: 2025-01-09` which pre-dates sibling docs (2026-02-09). Refresh the timestamp or verify content is still current. (Phase 2)
57. `docs/performance/BASELINE.kr.md:33` references "Apple Clang 17.0.0.17000319" while `docs/BENCHMARKS.md:390` says "Apple Clang 15.0.0". Benchmarks appear to be reported on different hardware/toolchain combinations but the prose does not explain why the two performance-baseline docs differ. (Phase 2)
58. Capitalization variants of core terms are inconsistent but defensible:
    - `metric/Metric/metrics/Metrics`: ~1,600 occurrences split almost evenly.
    - `collector/Collector/collectors/Collectors`: ~1,030 occurrences.
    - `alert/Alert` and `Counter/counter/Gauge/gauge/Histogram/histogram` all mix cases freely. A short style glossary ("use lowercase in prose; use PascalCase only when referring to a C++ type or class name") would help. (Phase 2)

### Phase 3 - Redundancy / Structure

59. Duplicate H2 "Overview", "Table of Contents", "See Also" in FEATURES_CORE, FEATURES_COLLECTORS, FEATURES_ALERTS_TRACING is expected for split docs. Consider suppressing the duplicated "See Also" blocks by moving to a single `FEATURES.md` appendix so each split does not maintain its own copy. (Phase 3)
60. `docs/advanced/PROFILING_GUIDE.md`, `docs/advanced/THREAD_LOCAL_COLLECTOR_DESIGN.md`, `docs/advanced/INTERFACE_SEPARATION_STRATEGY.md`, `docs/advanced/CURRENT_STATE.md` - these advanced design docs have dates 2025-10 to 2025-11, more than 5 months old. Refresh dates if still accurate or mark as "historical / design rationale". (Phase 3)
61. `docs/FEATURES.kr.md`, `docs/CHANGELOG.kr.md`, `docs/API_REFERENCE.kr.md`, `docs/PROJECT_STRUCTURE.kr.md` appear to still track pre-split single-file content while the English side has been split into three. When the EN hierarchy changes, the Korean hierarchy should be refreshed or explicitly labelled "out-of-date; see English for split content". (Phase 3)
62. `docs/ARCHITECTURE.md` and `docs/advanced/ARCHITECTURE.md` both exist. `docs/advanced/ARCHITECTURE_GUIDE.md` is separate again. Consolidate or add a short "how these three differ" note to the top of each. (Phase 3)
63. `docs/performance/BASELINE.md`, `docs/performance/PERFORMANCE_BASELINE.md`, and `benchmarks/README.md` all describe performance baselines. An index stating which document is SSOT would reduce reader confusion. (Phase 3)
64. `docs/SOUP.md:53` cites "gRPC 1.51.1" while `CLAUDE.md:90` cites "gRPC 1.60.0 + protobuf 4.25.1" as optional external - different versions pinned in different docs. Align both to a single authoritative version in one file. (Phase 3)

---

## Score

- **Overall**: 6.8 / 10
- **Phase 1 (Anchors & Links)**: 5 / 10  (138 broken links / 1,387 total = 9.9% broken; 40 / 104 files contain at least one bad link)
- **Phase 2 (Accuracy)**: 7 / 10  (compiler-version fact drift is the main concern; Prometheus/OTel terminology is otherwise solid)
- **Phase 3 (SSOT & Redundancy)**: 7.5 / 10  (explicit SSOT banners exist, but version-header inconsistency and the FEATURES vs API_REFERENCE split-boundary asymmetry are structural defects)

Rationale: the SSOT architecture (FEATURES.md -> split trio, API_REFERENCE.md -> split trio, TRACEABILITY.md as QUAL index) is thoughtful and generally well-executed. Most drift is mechanical (stale paths to renamed `01-ARCHITECTURE.md` / `02-API_REFERENCE.md` / `USER_GUIDE.md` / `PHASE3`/`PHASE4.md`) rather than conceptual.

---

## Notes

1. **Top recurring pattern (Phase 1)**: legacy pre-rename paths. References to `01-ARCHITECTURE.md`, `02-API_REFERENCE.md`, `guides/USER_GUIDE.md`, `PHASE3.md`, `PHASE4.md`, `ARCHITECTURE_GUIDE.md` (without `advanced/` prefix) appear to be artefacts of an earlier documentation restructure. A single cleanup pass replacing each stale target would fix ~60 of the 112 broken file refs.

2. **Top recurring pattern (Phase 1, anchors)**: ToCs written by hand with oversimplified slug rules. The analyzer confirms that GitHub's slug algorithm (lowercase, keep underscores, replace each whitespace char 1:1 with `-`, keep strikethrough text stripped of `~~` but keep RESOLVED/emoji residue) is not what the authors assumed. Regenerating the ToCs from actual headings (using `markdown-toc`, `doctoc`, etc.) would resolve all 26 anchor mismatches.

3. **Top recurring pattern (Phase 2)**: compiler-version drift. The required-floor bumped from GCC 11/Clang 14 -> GCC 13/Clang 17 (due to thread_system dependency) but at least 8 documents still advertise the lower floor. The canonical statement in `CLAUDE.md:96` and `README.md:51` is correct; peer docs should be aligned in one sweep.

4. **Cross-reference graph**: of the 104 md files, 12 are orphans (never linked from another md). Of the 92 linked files, the most-linked hubs are `README.md`, `docs/README.md`, `docs/FEATURES.md`, `docs/API_REFERENCE.md`, `docs/ARCHITECTURE.md`, and `docs/TRACEABILITY.md`. Orphaning concentrates in CI/GH-template files (which is fine) and in `docs/ECOSYSTEM.md` + `docs/GETTING_STARTED.md` + `benchmarks/README.md` + `integration_tests/README.md` (which ought to be linked from README.md).

5. **Korean localisation**: 25 KR/EN pairs, 54 English-only docs, 0 Korean-only docs. The corpus appears to be gradually localising to Korean starting from top-level docs. No inconsistencies were found where the Korean version contradicts the English version on facts (compiler versions, feature names), but the Korean CHANGELOG.kr.md and FEATURES.kr.md still reflect the pre-split single-file structure while the English side is split; this creates a non-obvious SSOT divergence.

6. **Verification methodology**: a Python analyzer replicated GitHub's slug algorithm (lowercase, strip `~~`/``` ` ```/`*`, remove non-`\w\s-` chars, replace each whitespace with a single `-`, preserve underscores and consecutive hyphens produced by removed punctuation). Fenced code blocks were skipped. External URLs (anything containing `:` in the path portion) were excluded from link validation. Inline code spans were stripped before link extraction to avoid matching brackets inside ``` `[]` ```.

7. **Out of scope**: `.dox`/`.html`/`.css` artefacts under `docs/` (faq.dox, mainpage.dox, troubleshooting.dox, tutorial_*.dox, custom.css, header.html, doxygen-awesome-css/) were not analysed; they feed Doxygen, not GitHub rendering.

8. **Files requiring attention most urgently (by density of issues)**:
   1. `docs/README.kr.md` - 15 broken links
   2. `README.md` - 10 broken links + 1 bad anchor
   3. `docs/guides/TUTORIAL.md` + `.kr.md` - 10 broken links each (20 total), all relative-path errors
   4. `docs/advanced/ARCHITECTURE_ISSUES.md` - 9 bad anchors (ToC regeneration)
   5. `docs/guides/INTEGRATION.md` - 8 broken links
   6. `docs/advanced/ARCHITECTURE_ISSUES.kr.md` - 6 bad anchors
   7. `docs/integration/README.md` - 6 broken links (all 5 sub-guides missing + 1 wrong-depth ECOSYSTEM link)
