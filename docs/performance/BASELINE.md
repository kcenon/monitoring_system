# Baseline Performance Metrics

**Document Version**: 3.0
**Created**: 2025-10-07
**Updated**: 2026-02-12
**System**: monitoring_system
**Purpose**: Establish baseline performance metrics for regression detection

---

## Overview

This document defines the benchmark suite and performance targets for the monitoring_system. Actual measurement values are populated by the CI workflow (`benchmarks.yml`) on each benchmark run. Results are stored in `benchmark_results.json`.

**Regression Threshold**: Any degradation >5% from baseline triggers investigation.
**Critical Requirement**: Monitoring overhead must remain <1% of monitored operation time.

---

## Test Environment

| Property | Value |
|----------|-------|
| **C++ Standard** | C++20 |
| **Build Type** | Release (`-O3 -DNDEBUG`) |
| **Framework** | Google Benchmark |
| **CMake Version** | 3.20+ |

Hardware details are recorded automatically in `benchmark_results.json` by Google Benchmark.

---

## Benchmark Suite

### 1. Metric Collection — `metric_collection_bench.cpp`

Measures profiler recording and retrieval latency.

| Benchmark | Description | Target |
|-----------|-------------|--------|
| `BM_ProfilerRecording_Single` | Record a single profiling entry | <100 ns |
| `BM_ProfilerRecording_Multiple` | Record multiple profiling entries sequentially | <500 ns |
| `BM_ProfilerRetrieval_Single` | Retrieve a single recorded profile | <200 ns |
| `BM_ProfilerRetrieval_All` | Retrieve all recorded profiles | <1 μs |
| `BM_ProfilerRecording_Concurrent` | Concurrent recording from 4 threads | Linear scaling |
| `BM_ScopedTimer_Overhead` | RAII scoped timer construction + destruction | <50 ns |

### 2. Collector Overhead — `collector_overhead_bench.cpp`

Measures monitoring infrastructure overhead: TLS buffering, central collection, and real workload impact.

#### 2.1 TLS Buffer Performance

| Benchmark | Description | Target |
|-----------|-------------|--------|
| `BM_TLSBuffer_Record_Single` | Record single entry to thread-local buffer | <100 ns |
| `BM_TLSBuffer_Record_AutoFlush` | Record triggering automatic flush | — |
| `BM_TLSBuffer_Flush` | Manual buffer flush to central collector | <1 μs |
| `BM_TLSBuffer_BufferSize` | Buffer size variants: 64, 128, 256, 512, 1024 | Sublinear growth |

#### 2.2 Central Collector Performance

| Benchmark | Description | Target |
|-----------|-------------|--------|
| `BM_CentralCollector_ReceiveBatch` | Receive batch: 64, 128, 256, 512 entries | Linear scaling |
| `BM_CentralCollector_SingleOperation` | Record and collect single operation | <500 ns |
| `BM_CentralCollector_ManyOperations` | Record 10–500 operations then collect | Linear scaling |
| `BM_CentralCollector_GetProfile` | Retrieve collected profile data | <1 μs |

#### 2.3 Monitoring Overhead (Workload Comparison)

| Benchmark | Description | Target |
|-----------|-------------|--------|
| `BM_Workload_Baseline` | Compute workload without monitoring | Reference |
| `BM_Workload_WithMonitoring` | Same workload with monitoring enabled | <1% overhead |
| `BM_IO_Workload_Baseline` | I/O workload without monitoring | Reference |
| `BM_IO_Workload_WithMonitoring` | Same I/O workload with monitoring enabled | <1% overhead |

#### 2.4 Concurrency & Memory

| Benchmark | Description | Target |
|-----------|-------------|--------|
| `BM_Concurrent_TLSBuffer` | TLS buffer recording at 1, 2, 4, 8 threads | >80% scaling at 8T |
| `BM_Concurrent_Flush` | Concurrent flush at 1, 2, 4, 8 threads | >80% scaling at 8T |
| `BM_Memory_SampleSize` | Memory footprint of sample storage | <1 KB per metric |

### 3. Adaptive Monitor — `adaptive_monitor_bench.cpp`

Measures adaptive monitoring behavior: configuration lookup, collection, and adaptation cycles.

#### 3.1 Configuration Lookup

| Benchmark | Description | Target |
|-----------|-------------|--------|
| `BM_AdaptiveConfig_GetIntervalForLoad` | Interval lookup by load level | <10 ns |
| `BM_AdaptiveConfig_GetSamplingRateForLoad` | Sampling rate lookup by load level | <10 ns |

#### 3.2 Adaptive Collection

| Benchmark | Description | Target |
|-----------|-------------|--------|
| `BM_AdaptiveCollector_Collect_Enabled` | Collect with adaptive sampling enabled | <500 ns |
| `BM_AdaptiveCollector_Collect_Disabled` | Collect with adaptive sampling disabled | <300 ns |
| `BM_AdaptiveCollector_Adapt` | Single adaptation cycle | <1 μs |
| `BM_AdaptiveCollector_Adapt_LoadTransition` | Adaptation across load level transitions | <1 μs |
| `BM_AdaptiveCollector_GetStats` | Statistics retrieval | <500 ns |
| `BM_AdaptiveMonitor_RegisterCollector` | Register a new collector | — |
| `BM_AdaptiveMonitor_GetAllStats` | Retrieve stats for all collectors | — |

#### 3.3 Concurrent Adaptive Collection

| Benchmark | Description | Target |
|-----------|-------------|--------|
| `BM_AdaptiveCollector_Concurrent_Collect` | Concurrent collect from 4 threads | >90% scaling |
| `BM_AdaptiveCollector_Concurrent_CollectAndAdapt` | Concurrent collect + adapt from 4 threads | >90% scaling |

#### 3.4 Strategy Comparison

| Benchmark | Description | Target |
|-----------|-------------|--------|
| `BM_AdaptiveCollector_Strategy_Conservative` | Conservative adaptation strategy | Fewer adaptations |
| `BM_AdaptiveCollector_Strategy_Aggressive` | Aggressive adaptation strategy | More responsive |

#### 3.5 Memory Pressure

| Benchmark | Description | Target |
|-----------|-------------|--------|
| `BM_AdaptiveCollector_HighMemoryPressure` | Collection under high memory pressure | Graceful degradation |

---

## Planned Benchmarks

The following areas are not yet covered by benchmarks:

- **Core metric types** (counter, gauge, histogram, summary, timer) — see [#476](https://github.com/kcenon/monitoring_system/issues/476)
- **Event bus throughput** — `event_bus_bench.cpp` currently contains only a placeholder
- **Time series insertion and query performance**
- **Tagged/labeled metric overhead**

---

## How to Run Benchmarks

```bash
# Build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target monitoring_benchmarks

# Run (console output)
./build/benchmarks/monitoring_benchmarks

# Run (JSON output for CI comparison)
./build/benchmarks/monitoring_benchmarks \
  --benchmark_format=json \
  --benchmark_out=benchmark_results.json
```

---

## Regression Detection

### Critical Performance Requirements

| Requirement | Threshold |
|-------------|-----------|
| Monitoring overhead | <1% of monitored operation |
| Profiler recording | <100 ns |
| Scoped timer overhead | <50 ns |
| Thread scaling efficiency | >80% at 8 threads |
| Memory per metric | <1 KB |
| Adaptive config lookup | <10 ns |
| Adaptive overhead vs direct | <20% |

### CI Workflow

The `benchmarks.yml` workflow:
1. Builds benchmarks in Release mode
2. Runs the full suite with JSON output
3. Compares against the previous baseline
4. Flags any regression >5%

---

## Historical Changes

| Date | Version | Change |
|------|---------|--------|
| 2025-10-07 | 1.0 | Initial baseline document created |
| 2025-11-26 | 2.0 | Consolidated user-friendly and technical benchmarks |
| 2026-02-12 | 3.0 | Removed 66 unmeasured placeholders; restructured around actual benchmarks |
