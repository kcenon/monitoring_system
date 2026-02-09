# Stream Processing and Aggregation Framework

> **Version**: 0.1.0
> **Status**: Active
> **Last Updated**: 2025-12-01

## Table of Contents

1. [Overview](#1-overview)
2. [Stream Aggregator](#2-stream-aggregator)
3. [Aggregation Processor](#3-aggregation-processor)
4. [Time Series Management](#4-time-series-management)
5. [Time Series Buffer](#5-time-series-buffer)
6. [Ring Buffer](#6-ring-buffer)
7. [Statistics Engine](#7-statistics-engine)
8. [Metric Storage](#8-metric-storage)
9. [Memory Management](#9-memory-management)
10. [Usage Examples](#10-usage-examples)
- [Appendix A: Component Reference](#appendix-a-component-reference)
- [Appendix B: File Reference](#appendix-b-file-reference)

---

## 1. Overview

The monitoring_system stream processing framework provides a complete pipeline for
collecting, aggregating, storing, and querying real-time metric data. The framework
is designed for high-throughput, memory-bounded operation with thread-safe access.

### Architecture

```
Raw Metrics (double values)
    │
    ▼
┌─────────────────────┐
│   metric_storage     │  ← store_metric() / store_metrics_batch()
│  ┌───────────────┐   │
│  │  ring_buffer   │   │  Lock-free incoming buffer (power-of-2 capacity)
│  │  <compact_     │   │
│  │   metric_value>│   │
│  └───────┬───────┘   │
│          │ flush()    │  Background thread (configurable interval)
│  ┌───────▼───────┐   │
│  │  time_series   │   │  Per-metric historical storage with compression
│  │  (per metric)  │   │
│  └───────────────┘   │
└─────────┬───────────┘
          │ query_metric()
          ▼
┌─────────────────────┐     ┌─────────────────────┐
│ aggregation_         │     │ time_series_buffer   │
│   processor          │     │ (bounded ring        │
│  ┌───────────────┐   │     │  buffer storage)     │
│  │ stream_        │   │     └─────────────────────┘
│  │  aggregator    │   │
│  │ (per rule)     │   │
│  └───────────────┘   │
└─────────────────────┘
          │
          ▼
  streaming_statistics
  (mean, variance, percentiles, outliers)
```

### Data Flow

1. **Ingestion**: Raw metric values enter through `metric_storage::store_metric()`
2. **Buffering**: Values are written to a lock-free `ring_buffer` for minimal latency
3. **Flushing**: A background thread periodically drains the ring buffer into per-metric `time_series` instances
4. **Aggregation**: `aggregation_processor` routes observations to per-metric `stream_aggregator` instances
5. **Statistics**: Each `stream_aggregator` maintains streaming statistics via Welford's algorithm and P² quantile estimation
6. **Querying**: Historical data can be queried through `time_series::query()` with step-based aggregation

### Component Relationships

| Component | Role | Thread Safety |
|-----------|------|---------------|
| `ring_buffer<T>` | Lock-free circular buffer | Lock-free (CAS) |
| `metric_storage` | Incoming buffer + time series map | `shared_mutex` |
| `time_series` | Per-metric historical data | `mutex` |
| `time_series_buffer<T>` | Bounded ring buffer with stats | `mutex` |
| `stream_aggregator` | Streaming statistics + percentiles | `shared_mutex` |
| `aggregation_processor` | Multi-metric aggregation pipeline | `shared_mutex` |
| `stats::compute<T>()` | Batch statistics computation | Stateless (thread-safe) |

---

## 2. Stream Aggregator

**Header**: `include/kcenon/monitoring/utils/stream_aggregator.h`

The stream aggregator provides real-time streaming statistics without storing all
observations. It combines three algorithms into a unified interface.

### 2.1 Online Statistics (Welford's Algorithm)

`online_statistics` computes running mean, variance, and standard deviation using
Welford's numerically stable online algorithm. Unlike naive `sum / count`, Welford's
algorithm avoids catastrophic cancellation with floating-point arithmetic.

```cpp
#include <kcenon/monitoring/utils/stream_aggregator.h>

using namespace kcenon::monitoring;

online_statistics stats;

// Add observations one at a time
stats.add_value(100.0);
stats.add_value(102.5);
stats.add_value(98.3);
stats.add_value(101.7);

// Query running statistics (all thread-safe)
size_t n      = stats.count();     // 4
double avg    = stats.mean();      // ~100.625
double var    = stats.variance();  // Sample variance
double sd     = stats.stddev();    // Standard deviation
double lo     = stats.min();       // 98.3
double hi     = stats.max();       // 102.5
double total  = stats.sum();       // 402.5

// Get all statistics at once (single lock acquisition)
streaming_statistics full = stats.get_statistics();

// Reset for next interval
stats.reset();
```

**Key properties**:
- O(1) time and space per observation
- Numerically stable for large datasets
- Thread-safe via `shared_mutex` (concurrent reads, exclusive writes)

### 2.2 Quantile Estimator (P² Algorithm)

`quantile_estimator` estimates quantiles (e.g., p50, p95, p99) in a streaming fashion
using the P² algorithm. It uses only 5 markers regardless of dataset size.

```cpp
// Create estimator for p99
quantile_estimator p99(0.99);

// Add observations
for (double latency : latencies) {
    p99.add_observation(latency);
}

// Get estimated p99 (O(1) space)
double p99_value = p99.get_quantile();
size_t count = p99.count();
```

**How P² works**:
1. Maintains 5 marker heights (quantile values) and positions
2. For each new observation, updates markers using piecewise-parabolic interpolation
3. Falls back to linear interpolation when parabolic gives out-of-range values
4. For the first 5 observations, uses simple sorting

**Limitations**:
- Accuracy improves with more observations (initial estimates may be rough)
- Move-only semantics (copy is disabled to prevent accidental duplication)
- One estimator per quantile — create multiple for different percentiles

### 2.3 Moving Window Aggregator

`moving_window_aggregator<T>` maintains a sliding time window of values with automatic
expiration. Values older than the window duration are removed on each insertion.

```cpp
using namespace std::chrono_literals;

// 5-minute sliding window, max 10000 entries
moving_window_aggregator<double> window(5min, 10000);

auto now = std::chrono::system_clock::now();
window.add_value(42.0, now);
window.add_value(43.5, now + 1s);

// Get all values currently in the window
std::vector<double> values = window.get_values();
size_t count = window.size();
```

**Expiration behavior**:
- Old entries are removed lazily on each `add_value()` call
- Both time-based (`window_duration_`) and size-based (`max_size_`) bounds
- Uses `std::deque` for efficient front removal

### 2.4 Full-Featured Stream Aggregator

`stream_aggregator` combines all three components for comprehensive streaming aggregation.

```cpp
// Configure the aggregator
stream_aggregator_config config;
config.window_size = 10000;
config.window_duration = std::chrono::milliseconds(60000);  // 1 minute
config.enable_outlier_detection = true;
config.outlier_threshold = 3.0;  // 3 standard deviations
config.percentiles_to_track = {0.5, 0.9, 0.95, 0.99};

// Validate configuration
auto validation = config.validate();
if (validation.is_err()) {
    // Handle invalid config
}

stream_aggregator aggregator(config);

// Add observations
for (double value : data_stream) {
    auto result = aggregator.add_observation(value);
    // result is always ok() in current implementation
}

// Get comprehensive statistics
streaming_statistics stats = aggregator.get_statistics();
// stats.count, stats.mean, stats.variance, stats.std_deviation
// stats.min_value, stats.max_value, stats.sum
// stats.outlier_count, stats.outliers (vector<double>)
// stats.percentiles (map: {0.5 → value, 0.9 → value, ...})

// Get a specific percentile
auto p95 = aggregator.get_percentile(0.95);  // std::optional<double>
if (p95.has_value()) {
    // Use *p95
}

// Convenience accessors
size_t n   = aggregator.count();
double avg = aggregator.mean();
double var = aggregator.variance();
double sd  = aggregator.stddev();
```

**Outlier detection**:
- Activated after 10+ observations (needs stable mean/stddev)
- Uses z-score: `|value - mean| / stddev > threshold`
- Stores up to 100 most recent outliers
- Outliers are still included in statistics computation

### 2.5 Pearson Correlation

A free function for computing correlation between two data series:

```cpp
std::vector<double> cpu_usage = {10.0, 20.0, 30.0, 40.0, 50.0};
std::vector<double> response_time = {12.0, 22.0, 35.0, 45.0, 52.0};

double r = pearson_correlation(cpu_usage, response_time);
// r ≈ 0.998 (strong positive correlation)
// Returns 0.0 if sizes differ or size < 2
```

---

## 3. Aggregation Processor

**Header**: `include/kcenon/monitoring/utils/aggregation_processor.h`

The `aggregation_processor` manages a pipeline of stream aggregators, one per configured
metric. It receives raw observations, routes them to the correct aggregator, and can
flush aggregated results to a `metric_storage` backend.

### 3.1 Aggregation Rules

Each metric requires an `aggregation_rule` that defines how it should be aggregated:

```cpp
aggregation_rule rule;
rule.source_metric = "http.request.latency";
rule.target_metric_prefix = "http.request.latency_agg";
rule.aggregation_interval = std::chrono::milliseconds(60000);  // 1 min
rule.percentiles = {0.5, 0.9, 0.95, 0.99};
rule.compute_rate = false;       // Not a counter metric
rule.detect_outliers = true;
rule.outlier_threshold = 3.0;    // Z-score threshold

auto validation = rule.validate();
// Validates: source_metric non-empty, prefix non-empty, interval > 0
```

### 3.2 Setting Up the Processor

```cpp
// Create storage backend (optional, for persisting aggregation results)
auto storage = std::make_shared<metric_storage>();

// Create processor with storage
aggregation_processor processor(storage);

// Add aggregation rules
processor.add_aggregation_rule(rule);  // Returns VoidResult

// Check configured metrics
auto metrics = processor.get_configured_metrics();  // vector<string>
bool has = processor.has_rule("http.request.latency");  // true
size_t count = processor.rule_count();  // 1
```

### 3.3 Processing Observations

```cpp
// Process incoming observations
processor.process_observation("http.request.latency", 45.2);
processor.process_observation("http.request.latency", 52.8);
processor.process_observation("http.request.latency", 38.1);

// Observations for unknown metrics are silently accepted (returns ok())
processor.process_observation("unknown.metric", 100.0);  // No-op, ok()

// Get current statistics without flushing
auto stats_result = processor.get_current_statistics("http.request.latency");
if (stats_result.is_ok()) {
    auto& stats = stats_result.value();
    // Use stats.count, stats.mean, etc.
}
```

### 3.4 Force Aggregation

`force_aggregation()` computes final statistics, stores results to the metric storage
backend, and resets the aggregator for the next interval:

```cpp
auto agg_result = processor.force_aggregation("http.request.latency");
if (agg_result.is_ok()) {
    auto& result = agg_result.value();
    // result.source_metric
    // result.samples_processed
    // result.processing_duration (microseconds)
    // result.statistics (streaming_statistics)
    // result.timestamp
    // result.stored_successfully (true if storage available)
}
```

**What gets stored** (when storage is available):
- `{prefix}.mean` (gauge)
- `{prefix}.min` (gauge)
- `{prefix}.max` (gauge)
- `{prefix}.stddev` (gauge)
- `{prefix}.count` (counter)
- `{prefix}.p50`, `{prefix}.p90`, `{prefix}.p95`, `{prefix}.p99` (gauges)

### 3.5 Standard Aggregation Rules

A factory function provides pre-configured rules for common metrics:

```cpp
auto rules = create_standard_aggregation_rules();
// Returns rules for:
// - response_time (outlier detection, no rate)
// - request_count (rate computation, no outlier detection)
// - error_count (both, with lower outlier threshold = 2.0)
// - cpu_usage (outlier detection, no rate)
// - memory_usage (outlier detection, no rate)

for (const auto& rule : rules) {
    processor.add_aggregation_rule(rule);
}
```

---

## 4. Time Series Management

**Header**: `include/kcenon/monitoring/utils/time_series.h`

The `time_series` class provides chronologically-ordered metric storage with
configurable retention, dead-band compression, and step-based querying.

### 4.1 Configuration

```cpp
time_series_config config;
config.retention_period = std::chrono::seconds(3600);    // Keep 1 hour
config.resolution = std::chrono::milliseconds(1000);     // 1s resolution
config.max_points = 3600;                                 // Max data points
config.enable_compression = true;                          // Enable dead-band compression
config.compression_threshold = 0.01;                       // Deviation threshold

auto validation = config.validate();
```

### 4.2 Creating and Adding Data

`time_series` uses a factory method pattern for validated construction:

```cpp
auto result = time_series::create("cpu.usage", config);
if (result.is_err()) {
    // Handle error
    return;
}

auto& series = result.value();  // std::unique_ptr<time_series>

// Add data points (default timestamp = now)
series->add_point(45.2);
series->add_point(48.7, some_timestamp);

// Batch insertion
std::vector<time_point_data> points = {
    {t1, 10.0, 1},
    {t2, 12.5, 1},
    {t3, 11.8, 1}
};
series->add_points(points);
```

**Insertion optimization**:
- If the new timestamp is >= the latest point, it appends in O(1)
- Otherwise, it uses `std::upper_bound` for chronological insertion in O(log n)
- Maintenance (cleanup, compression, size enforcement) runs every 100 insertions

### 4.3 Data Compression

When `enable_compression` is true, the time series uses **dead-band compression**:
for each interior point, it checks if the value differs significantly from the linear
interpolation between its neighbors. Points close to the interpolation line (within
`compression_threshold`) are removed.

```
Before compression:    After compression (threshold = 0.01):
  •  •  •  •  •          •           •
 /  \/  \/  \  \        / \         / \
•    •    •    •       •   •-------•   •
     ↑         ↑              removed
   (close to line)
```

This is similar to the Ramer-Douglas-Peucker algorithm for line simplification.

### 4.4 Querying

```cpp
time_series_query query;
query.start_time = now - std::chrono::hours(1);
query.end_time = now;
query.step = std::chrono::milliseconds(60000);  // 1-minute steps

auto result = series->query(query);
if (result.is_ok()) {
    auto& agg = result.value();
    // agg.points: vector<time_point_data> (one per step)
    // agg.total_samples: total samples in range
    // agg.query_start, agg.query_end

    // Built-in analysis
    summary_data summary = agg.get_summary();  // min, max, mean
    double avg = agg.get_average();            // Weighted average
    double rate = agg.get_rate();              // Value change per second
}
```

**Step aggregation**: Points within each step are merged using weighted averaging
(weighted by `sample_count`). The aggregated timestamp is placed at the step midpoint.

### 4.5 Utility Methods

```cpp
series->size();              // Current data point count
series->empty();             // Check if empty
series->name();              // Series name
series->get_config();        // Get configuration
series->get_latest_value();  // Result<double> - latest value
series->memory_footprint();  // Estimated bytes used
series->clear();             // Remove all data
```

---

## 5. Time Series Buffer

**Header**: `include/kcenon/monitoring/utils/time_series_buffer.h`

`time_series_buffer<T>` provides a fixed-capacity ring buffer for time-stamped samples
with automatic statistics calculation. Unlike `time_series` (which uses a growable
vector with compression), `time_series_buffer` uses a true ring buffer with O(1)
insertion and bounded memory.

### 5.1 Core Buffer

```cpp
time_series_buffer_config config;
config.max_samples = 1000;

time_series_buffer<double> buffer(config);

// Add samples (timestamp defaults to now)
buffer.add_sample(42.5);
buffer.add_sample(43.8, specific_timestamp);

// Get samples by duration
auto recent = buffer.get_samples(std::chrono::minutes(5));

// Get samples since a timestamp
auto since = buffer.get_samples_since(cutoff_time);

// Get all samples in chronological order
auto all = buffer.get_all_samples();

// Get latest
auto latest = buffer.get_latest();       // Result<double>
auto sample = buffer.get_latest_sample(); // Result<time_series_sample<double>>

// Properties
buffer.size();              // Current count
buffer.capacity();          // Maximum capacity
buffer.empty();
buffer.memory_footprint();  // Bytes used
buffer.clear();
```

**Type constraint**: `T` must be an arithmetic type (`static_assert(std::is_arithmetic_v<T>)`).

### 5.2 Automatic Statistics

```cpp
// Statistics for all samples
time_series_statistics stats = buffer.get_statistics();
// stats.min_value, stats.max_value, stats.avg, stats.stddev
// stats.p95, stats.p99, stats.sample_count
// stats.oldest_timestamp, stats.newest_timestamp

// Statistics for a time window
auto recent_stats = buffer.get_statistics(std::chrono::minutes(5));
```

Statistics are computed on-demand by extracting values, sorting, and calculating
percentiles. This is a batch operation — not streaming.

### 5.3 Load Average History

A specialized buffer for tracking system load averages:

```cpp
load_average_history history(1000);  // 1000 samples

// Add load average samples
history.add_sample(1.2, 0.8, 0.5);  // 1m, 5m, 15m
history.add_sample(1.5, 0.9, 0.5, specific_timestamp);

// Get per-field statistics
load_average_statistics stats = history.get_statistics();
// stats.load_1m_stats.avg, stats.load_1m_stats.p99, etc.
// stats.load_5m_stats.avg, stats.load_5m_stats.p99, etc.
// stats.load_15m_stats.avg, stats.load_15m_stats.p99, etc.

auto latest = history.get_latest();  // Result<load_average_sample>
```

### 5.4 Internal Ring Buffer

Both `time_series_buffer<T>` and `load_average_history` delegate to
`detail::time_series_ring_buffer<Sample>`:

- Pre-allocates `max_samples` elements on construction
- Overwrites oldest when full (head/count tracking)
- `get_all_samples()` and `get_samples_since()` return chronologically sorted results
- Thread-safe via `std::mutex`

---

## 6. Ring Buffer

**Public Header**: `include/kcenon/monitoring/utils/ring_buffer.h`
**Internal Implementation**: `src/utils/ring_buffer.h`

The `ring_buffer<T>` is a high-performance, lock-free circular buffer designed for the
metric ingestion hot path. It uses atomic compare-and-swap (CAS) operations for
thread-safety without mutexes.

### 6.1 Configuration

```cpp
ring_buffer_config config;
config.capacity = 8192;           // Must be power of 2
config.overwrite_old = true;      // Overwrite oldest when full
config.batch_size = 64;           // Max items per batch read
config.gc_interval = std::chrono::milliseconds(1000);

auto validation = config.validate();
// Validates: capacity is power of 2, batch_size > 0 and <= capacity
```

**Power-of-2 requirement**: The capacity must be a power of 2 so that modulo operations
can be replaced with bitwise AND (`index & (capacity - 1)`), which is significantly
faster.

### 6.2 Write Operations

```cpp
ring_buffer<compact_metric_value> buffer(config);

// Single write (move semantics)
compact_metric_value metric(metadata, 42.0);
auto result = buffer.write(std::move(metric));
if (result.is_err()) {
    // Buffer full and overwrite_old = false
}

// Batch write
std::vector<compact_metric_value> batch = {metric1, metric2, metric3};
size_t written = buffer.write_batch(std::move(batch));
```

**Overflow behavior** (when `overwrite_old = true`):
1. Writer detects buffer is full via CAS
2. Advances the read index to discard the oldest entry
3. Claims the write slot atomically
4. Maximum 100 CAS retries before returning error (extreme contention guard)

### 6.3 Read Operations

```cpp
compact_metric_value item;

// Single read
auto result = buffer.read(item);  // Moves item out
if (result.is_err()) {
    // Buffer empty
}

// Batch read
std::vector<compact_metric_value> items;
size_t read_count = buffer.read_batch(items, 100);  // Read up to 100

// Peek without consuming
result = buffer.peek(item);  // Copies item (does not move)
```

### 6.4 Performance Characteristics

| Operation | Complexity | Lock-Free |
|-----------|-----------|-----------|
| `write()` | O(1) amortized | Yes (CAS loop) |
| `read()` | O(1) | Yes (atomic store) |
| `peek()` | O(1) | Yes (atomic load) |
| `size()` | O(1) | Yes (atomic loads) |
| `write_batch()` | O(n) | Per-item CAS |
| `read_batch()` | O(n) | Per-item atomic |

**Cache optimization**: Write and read indices are `alignas(64)` (cache-line aligned)
to prevent false sharing between producer and consumer threads.

### 6.5 Statistics and Monitoring

```cpp
const ring_buffer_stats& stats = buffer.get_stats();

stats.total_writes.load();        // Total write attempts
stats.total_reads.load();         // Total read attempts
stats.overwrites.load();          // Oldest-data overwrites
stats.failed_writes.load();       // Failed writes (buffer full, no overwrite)
stats.failed_reads.load();        // Failed reads (buffer empty)
stats.contention_retries.load();  // CAS retries due to contention

// Derived metrics
stats.get_write_success_rate();   // Percentage
stats.get_overflow_rate();        // Overwrites per total writes
stats.is_overflow_rate_high();    // > 10% overflow
stats.get_avg_contention();       // Retries per write
stats.get_read_success_rate();    // Percentage

// Check via buffer
buffer.is_overflow_rate_high();   // Convenience method
buffer.get_overflow_rate();       // Convenience method

// Reset statistics
buffer.reset_stats();
```

### 6.6 Factory Helpers

```cpp
// Default capacity (8192)
auto buf1 = make_ring_buffer<compact_metric_value>();

// Custom capacity
auto buf2 = make_ring_buffer<compact_metric_value>(16384);

// Custom configuration
ring_buffer_config cfg;
cfg.capacity = 4096;
cfg.overwrite_old = false;
auto buf3 = make_ring_buffer<compact_metric_value>(cfg);
```

---

## 7. Statistics Engine

**Header**: `include/kcenon/monitoring/utils/statistics.h`

The `stats` namespace provides generic, reusable statistics functions that work with
any numeric type, including `std::chrono::duration` types.

### 7.1 Statistics Structure

```cpp
using namespace kcenon::monitoring::stats;

statistics<double> result;
// result.min, result.max, result.mean, result.median
// result.p95, result.p99, result.total, result.count
```

### 7.2 Computing Statistics

Three computation modes are available:

```cpp
std::vector<double> values = {10.0, 20.0, 15.0, 25.0, 30.0};

// Mode 1: From unsorted data (copies + sorts internally)
auto stats1 = compute(values);  // values unchanged

// Mode 2: In-place (sorts the input vector)
auto stats2 = compute_inplace(values);  // values is now sorted

// Mode 3: From pre-sorted data (most efficient)
std::vector<double> sorted = {10.0, 15.0, 20.0, 25.0, 30.0};
auto stats3 = compute_sorted(sorted);
```

### 7.3 Percentile Calculation

```cpp
std::vector<double> sorted = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0};

double p50 = percentile(sorted, 50.0);   // 5.5 (linear interpolation)
double p95 = percentile(sorted, 95.0);   // 9.55
double p99 = percentile(sorted, 99.0);   // 9.91
```

**Interpolation method**:
- Numeric types: Linear interpolation between adjacent sorted values
- `std::chrono::duration` types: Nearest-rank method (rounds to nearest index)

### 7.4 Chrono Duration Support

The statistics engine transparently handles `std::chrono::duration` types:

```cpp
using namespace std::chrono;

std::vector<nanoseconds> durations = {
    nanoseconds(100), nanoseconds(200), nanoseconds(150),
    nanoseconds(300), nanoseconds(250)
};

auto stats = compute(durations);
// stats.min    == 100ns
// stats.max    == 300ns
// stats.mean   == 200ns
// stats.median == 200ns
// stats.p95    == 300ns (nearest-rank)
// stats.total  == 1000ns
// stats.count  == 5
```

This works through the `detail::is_chrono_duration<T>` type trait, which specializes
zero, min/max values, and division operations for duration types.

---

## 8. Metric Storage

**Header**: `include/kcenon/monitoring/utils/metric_storage.h`

`metric_storage` is the central storage component that connects the ring buffer
(ingestion) with time series (historical storage) and provides a unified query
interface.

### 8.1 Configuration

```cpp
metric_storage_config config;
config.ring_buffer_capacity = 8192;              // Must be power of 2
config.max_metrics = 10000;                       // Max unique metric series
config.enable_background_processing = true;       // Background flush thread
config.flush_interval = std::chrono::milliseconds(1000);  // 1s flush
config.time_series_max_points = 3600;             // Points per series
config.retention_period = std::chrono::seconds(3600);     // 1 hour retention

auto validation = config.validate();
```

### 8.2 Storing Metrics

```cpp
metric_storage storage(config);

// Single metric
auto result = storage.store_metric("cpu.usage", 45.2, metric_type::gauge);
auto result2 = storage.store_metric("requests.total", 1.0, metric_type::counter);

// Batch storage
metric_batch batch;
batch.add_metric(compact_metric_value(
    create_metric_metadata("cpu.usage", metric_type::gauge), 46.1));
batch.add_metric(compact_metric_value(
    create_metric_metadata("mem.used", metric_type::gauge), 1024.0));

size_t stored = storage.store_metrics_batch(batch);
```

**Write path**:
1. `store_metric()` creates `compact_metric_value` with FNV-1a name hash
2. Writes to ring buffer (lock-free, returns immediately)
3. Stores name → hash mapping under `shared_mutex`
4. Increments `total_metrics_stored` or `total_metrics_dropped` atomically

### 8.3 Background Flushing

When `enable_background_processing = true`, a dedicated thread runs:

```
┌─────────────────────────────────────────────────┐
│  Background Thread Loop                          │
│                                                   │
│  while (running_) {                               │
│      sleep_for(flush_interval);                   │
│      flush();  ←─── Drain ring buffer             │
│  }                      │                         │
│                         ▼                         │
│  For each compact_metric_value:                   │
│    1. Look up name from hash (hash_to_name_)      │
│    2. Get or create time_series for that metric    │
│    3. Add data point (value + timestamp)           │
│                                                   │
│  Atomically increment flush_count                 │
└─────────────────────────────────────────────────┘
```

Manual flush is also available:

```cpp
storage.flush();  // Explicitly drain ring buffer to time series
```

### 8.4 Querying

```cpp
// Get latest value
auto latest = storage.get_latest_value("cpu.usage");  // Result<double>

// Time-range query
time_series_query query(start_time, end_time, step_size);
auto result = storage.query_metric("cpu.usage", query);
if (result.is_ok()) {
    auto& agg = result.value();
    // agg.points, agg.total_samples, etc.
}

// List all metric names
auto names = storage.get_metric_names();  // vector<string>
```

### 8.5 Monitoring Storage Health

```cpp
const auto& stats = storage.get_stats();

stats.total_metrics_stored.load();   // Total successful stores
stats.total_metrics_dropped.load();  // Dropped due to buffer full
stats.active_metric_series.load();   // Unique metric series count
stats.flush_count.load();            // Number of flush operations
stats.failed_flushes.load();         // Individual flush failures

// Other utilities
storage.series_count();              // Active time series count
storage.memory_footprint();          // Estimated total memory usage
```

### 8.6 Lifecycle

```cpp
{
    metric_storage storage(config);
    // Background thread starts automatically if configured

    storage.store_metric("test", 1.0);
    // ...

}  // Destructor stops background thread and joins
```

The destructor sets `running_ = false` and joins the background thread. Any buffered
data not yet flushed will be lost — call `flush()` explicitly before destruction if
data completeness matters.

---

## 9. Memory Management

### 9.1 Memory Bounds by Component

| Component | Memory Formula | Default |
|-----------|---------------|---------|
| `ring_buffer<T>` | `capacity × sizeof(T)` | 8192 × sizeof(T) |
| `time_series` | `max_points × sizeof(time_point_data)` | 3600 × 24 bytes |
| `time_series_buffer<T>` | `max_samples × sizeof(time_series_sample<T>)` | 1000 × sizeof(sample) |
| `metric_storage` | ring_buffer + (max_metrics × time_series) | ~86 KB + n × 86 KB |
| `stream_aggregator` | O(1) — 5 markers per quantile + stats | ~500 bytes |
| `aggregation_processor` | n × stream_aggregator | ~500 bytes per metric |

### 9.2 Memory-Bounding Strategies

**Ring buffer**: Fixed capacity, overwrites oldest data when full:
```cpp
ring_buffer_config config;
config.capacity = 8192;        // Fixed at construction
config.overwrite_old = true;   // Never exceeds capacity
```

**Time series**: Three mechanisms work together:
1. **Retention period**: Removes data older than `retention_period`
2. **Compression**: Dead-band compression reduces point count
3. **Size limit**: Hard cap at `max_points`, removes oldest first

```cpp
time_series_config config;
config.retention_period = std::chrono::seconds(3600);  // Time bound
config.enable_compression = true;                       // Compression
config.max_points = 3600;                               // Size bound
```

**Metric storage**: Limits the total number of unique metric series:
```cpp
metric_storage_config config;
config.max_metrics = 10000;  // New series rejected after this
```

### 9.3 Monitoring Memory Usage

```cpp
// Per time series
size_t bytes = series->memory_footprint();

// Total metric storage
size_t total = storage.memory_footprint();

// Time series buffer
size_t buf_bytes = buffer.memory_footprint();
```

### 9.4 Sizing Guidelines

| Metric Rate | Ring Buffer | Time Series | Total per Metric |
|-------------|------------|-------------|------------------|
| 1/sec | 1024 | 3600 pts (1h) | ~90 KB |
| 10/sec | 4096 | 3600 pts (1h) | ~100 KB |
| 100/sec | 8192 | 3600 pts (1h) | ~115 KB |
| 1000/sec | 16384 | 7200 pts (2h) | ~250 KB |

For 1000 unique metrics at 10/sec ingestion rate:
- Ring buffer: ~8192 × 48 bytes = **384 KB** (shared)
- Time series: ~1000 × 3600 × 24 bytes = **82 MB**
- Total: ~**83 MB**

---

## 10. Usage Examples

### 10.1 Real-Time Request Latency Monitoring

```cpp
#include <kcenon/monitoring/utils/stream_aggregator.h>
#include <kcenon/monitoring/utils/aggregation_processor.h>
#include <kcenon/monitoring/utils/metric_storage.h>

using namespace kcenon::monitoring;

// Set up storage
metric_storage_config storage_config;
storage_config.ring_buffer_capacity = 8192;
storage_config.flush_interval = std::chrono::milliseconds(500);
auto storage = std::make_shared<metric_storage>(storage_config);

// Set up aggregation processor
aggregation_processor processor(storage);

aggregation_rule latency_rule;
latency_rule.source_metric = "http.latency";
latency_rule.target_metric_prefix = "http.latency.agg";
latency_rule.aggregation_interval = std::chrono::milliseconds(60000);
latency_rule.percentiles = {0.5, 0.9, 0.95, 0.99};
latency_rule.detect_outliers = true;
latency_rule.outlier_threshold = 3.0;

processor.add_aggregation_rule(latency_rule);

// In request handler:
void on_request_complete(double latency_ms) {
    // Store raw metric
    storage->store_metric("http.latency", latency_ms, metric_type::timer);

    // Feed to aggregation pipeline
    processor.process_observation("http.latency", latency_ms);
}

// Periodic aggregation (e.g., every minute via timer)
void on_aggregation_tick() {
    auto result = processor.force_aggregation("http.latency");
    if (result.is_ok()) {
        auto& stats = result.value().statistics;

        // stats.mean         → average latency
        // stats.percentiles  → {0.5: p50, 0.9: p90, 0.95: p95, 0.99: p99}
        // stats.outlier_count → number of anomalous requests
        // stats.min_value     → fastest request
        // stats.max_value     → slowest request
    }
}
```

### 10.2 System Resource Trend Analysis

```cpp
#include <kcenon/monitoring/utils/time_series_buffer.h>

using namespace kcenon::monitoring;

// Track CPU usage with 1-hour history
time_series_buffer_config cpu_config;
cpu_config.max_samples = 3600;  // 1 sample/sec for 1 hour
time_series_buffer<double> cpu_history(cpu_config);

// Track load averages
load_average_history load_history(3600);

// Periodic sampling (every second)
void on_sample_tick() {
    double cpu = get_current_cpu_usage();
    cpu_history.add_sample(cpu);

    auto [l1, l5, l15] = get_load_averages();
    load_history.add_sample(l1, l5, l15);
}

// Analysis
void analyze_trends() {
    // CPU statistics for last 5 minutes
    auto cpu_stats = cpu_history.get_statistics(std::chrono::minutes(5));
    if (cpu_stats.p95 > 80.0) {
        // CPU is consistently high
    }

    // Load average trends
    auto load_stats = load_history.get_statistics(std::chrono::minutes(15));
    if (load_stats.load_1m_stats.avg > load_stats.load_15m_stats.avg * 1.5) {
        // Load is increasing (1m avg >> 15m avg)
    }

    // Historical query via time series
    auto recent_samples = cpu_history.get_samples(std::chrono::minutes(10));
    // Process samples for visualization or alerting
}
```

### 10.3 Custom Aggregation Pipeline with Correlation

```cpp
#include <kcenon/monitoring/utils/stream_aggregator.h>
#include <kcenon/monitoring/utils/statistics.h>

using namespace kcenon::monitoring;

// Two correlated metrics
stream_aggregator_config config;
config.percentiles_to_track = {0.5, 0.95, 0.99};
config.enable_outlier_detection = true;

stream_aggregator cpu_agg(config);
stream_aggregator latency_agg(config);

// Collect paired observations for correlation
std::vector<double> cpu_values;
std::vector<double> latency_values;

void on_observation(double cpu, double latency) {
    cpu_agg.add_observation(cpu);
    latency_agg.add_observation(latency);

    cpu_values.push_back(cpu);
    latency_values.push_back(latency);
}

void generate_report() {
    auto cpu_stats = cpu_agg.get_statistics();
    auto lat_stats = latency_agg.get_statistics();

    // Compute batch statistics with the generic engine
    auto cpu_batch = stats::compute(cpu_values);
    auto lat_batch = stats::compute(latency_values);

    // Compute correlation
    double r = pearson_correlation(cpu_values, latency_values);

    // Report
    // cpu_stats.mean, cpu_stats.percentiles[0.99]
    // lat_stats.mean, lat_stats.percentiles[0.99]
    // r > 0.7 → strong positive correlation between CPU and latency

    // Reset for next interval
    cpu_agg.reset();
    latency_agg.reset();
    cpu_values.clear();
    latency_values.clear();
}
```

### 10.4 High-Throughput Metric Ingestion

```cpp
#include <kcenon/monitoring/utils/metric_storage.h>

using namespace kcenon::monitoring;

// Configure for high throughput
metric_storage_config config;
config.ring_buffer_capacity = 16384;  // Larger buffer for burst absorption
config.max_metrics = 5000;
config.enable_background_processing = true;
config.flush_interval = std::chrono::milliseconds(500);  // Faster flushing
config.time_series_max_points = 7200;  // 2 hours at 1/sec
config.retention_period = std::chrono::seconds(7200);

metric_storage storage(config);

// Batch ingestion from multiple threads
void ingest_metrics(const std::vector<std::pair<std::string, double>>& metrics) {
    metric_batch batch;
    batch.reserve(metrics.size());

    for (const auto& [name, value] : metrics) {
        auto metadata = create_metric_metadata(name, metric_type::gauge);
        batch.add_metric(compact_metric_value(metadata, value));
    }

    size_t stored = storage.store_metrics_batch(batch);

    // Monitor health
    if (storage.get_stats().total_metrics_dropped.load() > 0) {
        // Consider increasing ring_buffer_capacity or flush frequency
    }
}

// Query across time
void query_history(const std::string& metric_name) {
    auto now = std::chrono::system_clock::now();
    time_series_query query(
        now - std::chrono::hours(1),
        now,
        std::chrono::minutes(1)  // 1-minute resolution
    );

    auto result = storage.query_metric(metric_name, query);
    if (result.is_ok()) {
        for (const auto& point : result.value().points) {
            // point.timestamp, point.value, point.sample_count
        }
    }
}
```

---

## Appendix A: Component Reference

### Metric Types

| Type | Enum | Description |
|------|------|-------------|
| Counter | `metric_type::counter` | Monotonically increasing |
| Gauge | `metric_type::gauge` | Instantaneous value |
| Histogram | `metric_type::histogram` | Value distribution |
| Summary | `metric_type::summary` | Statistical summary |
| Timer | `metric_type::timer` | Duration measurements |
| Set | `metric_type::set` | Unique value counting |

### Configuration Defaults

| Component | Parameter | Default | Constraint |
|-----------|-----------|---------|-----------|
| `ring_buffer_config` | capacity | 8192 | Power of 2 |
| `ring_buffer_config` | overwrite_old | true | — |
| `ring_buffer_config` | batch_size | 64 | > 0, <= capacity |
| `stream_aggregator_config` | window_size | 10000 | > 0 |
| `stream_aggregator_config` | window_duration | 60000ms | > 0 |
| `stream_aggregator_config` | outlier_threshold | 3.0 | > 0 |
| `stream_aggregator_config` | percentiles | {0.5, 0.9, 0.95, 0.99} | 0.0–1.0 |
| `time_series_config` | retention_period | 3600s | > 0 |
| `time_series_config` | resolution | 1000ms | > 0 |
| `time_series_config` | max_points | 3600 | > 0 |
| `time_series_config` | compression_threshold | 0.01 | — |
| `time_series_buffer_config` | max_samples | 1000 | > 0 |
| `metric_storage_config` | ring_buffer_capacity | 8192 | Power of 2 |
| `metric_storage_config` | max_metrics | 10000 | > 0 |
| `metric_storage_config` | flush_interval | 1000ms | — |
| `metric_storage_config` | time_series_max_points | 3600 | > 0 |
| `metric_storage_config` | retention_period | 3600s | > 0 |
| `aggregation_rule` | aggregation_interval | 60000ms | > 0 |
| `aggregation_rule` | outlier_threshold | 3.0 | — |
| `aggregation_rule` | compute_rate | false | — |
| `aggregation_rule` | detect_outliers | true | — |

### Error Codes

| Error | When |
|-------|------|
| `invalid_configuration` | Config validation failure |
| `storage_full` | Ring buffer full, overwrite disabled |
| `collection_failed` | Read from empty buffer, no data available |
| `metric_not_found` | Query for non-existent metric |
| `already_exists` | Duplicate aggregation rule |
| `invalid_argument` | Invalid query parameters |

---

## Appendix B: File Reference

| File | Lines | Description |
|------|-------|-------------|
| `include/kcenon/monitoring/utils/stream_aggregator.h` | 715 | Streaming statistics, P² quantiles, moving window, outlier detection |
| `include/kcenon/monitoring/utils/aggregation_processor.h` | 401 | Multi-metric aggregation pipeline with storage integration |
| `include/kcenon/monitoring/utils/time_series.h` | 501 | Chronological data storage with compression and querying |
| `include/kcenon/monitoring/utils/time_series_buffer.h` | 674 | Ring buffer-backed time series with statistics |
| `include/kcenon/monitoring/utils/statistics.h` | 285 | Generic statistics and percentile computation |
| `include/kcenon/monitoring/utils/ring_buffer.h` | 26 | Public re-export of internal ring buffer |
| `src/utils/ring_buffer.h` | 498 | Lock-free ring buffer with CAS and cache-line alignment |
| `include/kcenon/monitoring/utils/metric_storage.h` | 401 | Unified storage: ring buffer ingestion + time series history |
| `include/kcenon/monitoring/utils/metric_types.h` | 628 | Metric type definitions, compact values, histograms, timers |
