# Baseline Performance Metrics

**Document Version**: 1.0
**Created**: 2025-10-07
**System**: monitoring_system
**Purpose**: Establish baseline performance metrics for regression detection

---

> **📖 User Documentation**: For user-friendly performance documentation and analysis,
> see [`docs/performance/BASELINE.md`](../docs/performance/BASELINE.md)

---

## Overview

This document records baseline performance metrics for the monitoring_system. These metrics serve as reference points for detecting performance regressions during development.

**Regression Threshold**: <5% performance degradation is acceptable. Any regression >5% should be investigated and justified.

**Critical Requirement**: Monitoring overhead must remain <1% of monitored operation time.

---

## Test Environment

### Hardware Specifications
- **CPU**: To be recorded on first benchmark run
- **Cores**: To be recorded on first benchmark run
- **RAM**: To be recorded on first benchmark run
- **OS**: macOS / Linux / Windows

### Software Configuration
- **Compiler**: Clang/GCC/MSVC (see CI workflow)
- **C++ Standard**: C++20
- **Build Type**: Release with optimizations
- **CMake Version**: 3.16+

---

## Benchmark Categories

### 1. Metric Collection Performance

#### 1.1 Counter Operations
**Metric**: Time to increment/record a counter
**Test File**: `metric_collection_bench.cpp`

| Operation | Mean (ns) | Median (ns) | P95 (ns) | P99 (ns) | Notes |
|-----------|-----------|-------------|----------|----------|-------|
| Increment (atomic) | TBD | TBD | TBD | TBD | Thread-safe |
| Increment (local) | TBD | TBD | TBD | TBD | Thread-local |
| Record value | TBD | TBD | TBD | TBD | |
| Get value | TBD | TBD | TBD | TBD | Read operation |

**Target**: <100ns for atomic increment
**Status**: ⏳ Awaiting initial benchmark run

#### 1.2 Histogram Updates
**Metric**: Time to record value in histogram
**Test File**: `metric_collection_bench.cpp`

| Histogram Type | Mean (ns) | Median (ns) | P95 (ns) | P99 (ns) | Notes |
|----------------|-----------|-------------|----------|----------|-------|
| Linear buckets | TBD | TBD | TBD | TBD | Fixed intervals |
| Exponential buckets | TBD | TBD | TBD | TBD | Powers of 2 |
| Custom buckets | TBD | TBD | TBD | TBD | User-defined |

**Target**: <200ns per update
**Status**: ⏳ Awaiting initial benchmark run

#### 1.3 Gauge Operations
**Metric**: Time to set/update a gauge value
**Test File**: `metric_collection_bench.cpp`

| Operation | Mean (ns) | Median (ns) | P95 (ns) | P99 (ns) | Notes |
|-----------|-----------|-------------|----------|----------|-------|
| Set value | TBD | TBD | TBD | TBD | |
| Increment | TBD | TBD | TBD | TBD | |
| Decrement | TBD | TBD | TBD | TBD | |
| Get value | TBD | TBD | TBD | TBD | |

**Status**: ⏳ Awaiting initial benchmark run

### 2. Event Bus Performance

#### 2.1 Event Publication
**Metric**: Time to publish event to subscribers
**Test File**: `event_bus_bench.cpp`

| Subscriber Count | Mean (μs) | Median (μs) | P95 (μs) | P99 (μs) | Notes |
|------------------|-----------|-------------|----------|----------|-------|
| 0 | TBD | TBD | TBD | TBD | No-op case |
| 1 | TBD | TBD | TBD | TBD | |
| 10 | TBD | TBD | TBD | TBD | |
| 100 | TBD | TBD | TBD | TBD | |

**Target**: Linear scaling with subscriber count
**Status**: ⏳ Awaiting initial benchmark run

#### 2.2 Event Queue Throughput
**Metric**: Events processed per second
**Test File**: `event_bus_bench.cpp`

| Queue Type | Events/sec | Latency (μs) | Notes |
|------------|------------|--------------|-------|
| Lock-free | TBD | TBD | Concurrent |
| Mutex-based | TBD | TBD | Simple |

**Target**: >1M events/sec for lock-free queue
**Status**: ⏳ Awaiting initial benchmark run

### 3. Collector Overhead

#### 3.1 Monitoring Overhead
**Metric**: Percentage overhead added by monitoring
**Test File**: `collector_overhead_bench.cpp`

| Operation Type | Baseline (ns) | With Monitoring (ns) | Overhead (%) | Notes |
|----------------|---------------|---------------------|--------------|-------|
| Function call | TBD | TBD | TBD | Simple function |
| I/O operation | TBD | TBD | TBD | File read/write |
| Network call | TBD | TBD | TBD | Socket operation |
| Database query | TBD | TBD | TBD | SQL execution |

**Target**: <1% overhead for all operations
**Status**: ⏳ Awaiting initial benchmark run

#### 3.2 Memory Overhead
**Metric**: Memory used by monitoring infrastructure

| Component | Memory (KB) | Per-Metric (bytes) | Notes |
|-----------|-------------|-------------------|-------|
| Counter | TBD | TBD | Atomic variable |
| Gauge | TBD | TBD | |
| Histogram (10 buckets) | TBD | TBD | |
| Histogram (100 buckets) | TBD | TBD | |
| Event bus | TBD | TBD | Queue + subscribers |

**Status**: ⏳ Awaiting measurement

### 4. Time Series Performance

#### 4.1 Data Point Insertion
**Metric**: Time to add data point to time series
**Test File**: `metric_collection_bench.cpp`

| Series Size | Mean (ns) | Median (ns) | P95 (ns) | P99 (ns) | Notes |
|-------------|-----------|-------------|----------|----------|-------|
| 100 points | TBD | TBD | TBD | TBD | Small series |
| 1,000 points | TBD | TBD | TBD | TBD | Medium series |
| 10,000 points | TBD | TBD | TBD | TBD | Large series |

**Status**: ⏳ Awaiting initial benchmark run

#### 4.2 Query Performance
**Metric**: Time to query time range from series

| Query Range | Mean (μs) | Median (μs) | Notes |
|-------------|-----------|-------------|-------|
| Last 10 points | TBD | TBD | Recent data |
| Last 100 points | TBD | TBD | |
| Last 1000 points | TBD | TBD | |
| All data | TBD | TBD | Full scan |

**Status**: ⏳ Awaiting initial benchmark run

---

## Concurrent Access Benchmarks

### 5. Thread Safety Performance

#### 5.1 Concurrent Metric Updates
**Metric**: Throughput with multiple writers

| Thread Count | Updates/sec | Contention (%) | Notes |
|--------------|-------------|----------------|-------|
| 1 | TBD | 0% (baseline) | |
| 2 | TBD | TBD | |
| 4 | TBD | TBD | |
| 8 | TBD | TBD | |
| 16 | TBD | TBD | |

**Target**: >80% scaling efficiency at 8 threads
**Status**: ⏳ Awaiting initial benchmark run

---

## How to Run Benchmarks

### Building Benchmarks
```bash
cd monitoring_system
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
cmake --build build --target benchmarks
```

### Running Benchmarks
```bash
cd build/benchmarks
./metric_collection_bench
./event_bus_bench
./collector_overhead_bench
```

### Recording Results
1. Run each benchmark 10 times
2. Ensure no other CPU-intensive processes running
3. Record statistics: min, max, mean, median, p95, p99
4. Update this document with actual values
5. Commit updated BASELINE.md

---

## Regression Detection

### Automated Checks
The benchmarks.yml workflow runs benchmarks on every PR and compares results against this baseline.

### Critical Performance Requirements
- **Monitoring overhead**: <1% of monitored operation
- **Counter increment**: <100ns
- **Event publication**: <10μs with 10 subscribers
- **Memory per metric**: <1KB
- **Thread scaling**: >80% efficiency at 8 threads

### Acceptable Trade-offs
- Slightly higher latency acceptable for added thread safety
- Memory overhead acceptable for better accuracy
- Must document any intentional performance reduction

---

## Historical Changes

| Date | Version | Change | Impact | Approved By |
|------|---------|--------|--------|-------------|
| 2025-10-07 | 1.0 | Initial baseline document created | N/A | Initial setup |

---

## Notes

- All benchmarks use Google Benchmark framework
- Overhead benchmarks compare with/without monitoring enabled
- Results may vary based on hardware and system load
- For accurate comparisons, run benchmarks on same hardware
- CI environment results are used as primary baseline
- Lock-free implementations should show better scaling than mutex-based

---

**Status**: 📝 Template created - awaiting initial benchmark data collection
