# Reliability Patterns Usage Guide

> **Version**: 1.0.0
> **Last Updated**: 2026-02-09
> **Parent Epic**: [Documentation Gap Analysis](https://github.com/kcenon/common_system/issues/325)
> **Related Issue**: [#458](https://github.com/kcenon/monitoring_system/issues/458)

## Table of Contents

1. [Overview](#1-overview)
2. [Circuit Breaker](#2-circuit-breaker)
3. [Retry Policy](#3-retry-policy)
4. [Error Boundary](#4-error-boundary)
5. [Graceful Degradation](#5-graceful-degradation)
6. [Fault Tolerance Manager](#6-fault-tolerance-manager)
7. [Resource Manager](#7-resource-manager)
8. [Data Consistency](#8-data-consistency)
9. [Composition Patterns](#9-composition-patterns)
10. [Real-World Examples](#10-real-world-examples)

---

## 1. Overview

The monitoring_system reliability subsystem provides 7 resilience patterns
that protect the system from cascading failures, resource exhaustion, and
data corruption. Each pattern addresses a specific failure mode and can be
composed together for comprehensive fault tolerance.

### 1.1 Pattern Summary

| Pattern | Purpose | Failure Mode Addressed |
|---------|---------|----------------------|
| **Circuit Breaker** | Prevent repeated calls to failing services | Cascading failures |
| **Retry Policy** | Automatically retry transient failures | Temporary outages |
| **Error Boundary** | Isolate failures and provide fallbacks | Component failures |
| **Graceful Degradation** | Reduce functionality under load | Overload / capacity |
| **Fault Tolerance Manager** | Orchestrate CB + Retry together | Combined failures |
| **Resource Manager** | Enforce rate limits and quotas | Resource exhaustion |
| **Data Consistency** | Ensure transactional integrity | Data corruption |

### 1.2 Source File Map

| File | Lines | Purpose |
|------|-------|---------|
| [`circuit_breaker.h`](../../include/kcenon/monitoring/reliability/circuit_breaker.h) | 276 | Circuit breaker adapter wrapping common_system |
| [`retry_policy.h`](../../include/kcenon/monitoring/reliability/retry_policy.h) | 308 | Retry strategies and executor |
| [`error_boundary.h`](../../include/kcenon/monitoring/reliability/error_boundary.h) | 412 | Error isolation with fallback strategies |
| [`graceful_degradation.h`](../../include/kcenon/monitoring/reliability/graceful_degradation.h) | 517 | Service degradation management |
| [`fault_tolerance_manager.h`](../../include/kcenon/monitoring/reliability/fault_tolerance_manager.h) | 448 | Orchestrates circuit breaker + retry |
| [`resource_manager.h`](../../include/kcenon/monitoring/reliability/resource_manager.h) | 655 | Rate limiting, memory quotas, CPU throttling |
| [`data_consistency.h`](../../include/kcenon/monitoring/reliability/data_consistency.h) | 620 | Transactions, validation, auto-repair |

### 1.3 Decision Guide

```
What failure are you protecting against?
│
├─ Downstream service failures
│   ├─ Transient (network blip, timeout) → Retry Policy
│   ├─ Prolonged outage → Circuit Breaker
│   └─ Both → Fault Tolerance Manager (CB + Retry)
│
├─ Component failures within the system
│   ├─ Need fallback values → Error Boundary (fallback policy)
│   ├─ Need error isolation → Error Boundary (isolate policy)
│   └─ Need coordinated shutdown → Graceful Degradation
│
├─ Resource exhaustion
│   ├─ Too many requests → Resource Manager (rate limiter)
│   ├─ Memory pressure → Resource Manager (memory quota)
│   └─ CPU overload → Resource Manager (CPU throttler)
│
└─ Data integrity
    ├─ Multi-step operations → Data Consistency (transactions)
    └─ State validation → Data Consistency (state validator)
```

---

## 2. Circuit Breaker

**File**: [`circuit_breaker.h`](../../include/kcenon/monitoring/reliability/circuit_breaker.h)

The circuit breaker prevents repeated calls to a failing service. It tracks
failures and "trips" (opens) when a threshold is reached, fast-failing
subsequent requests until a recovery period elapses.

### 2.1 State Machine

```
         success                failure count
    ┌───────────┐          reaches threshold
    │           │                 │
    ▼           │                 ▼
 ┌──────┐      │            ┌──────┐
 │CLOSED│──────┘            │ OPEN │
 └──┬───┘                   └──┬───┘
    │                           │
    │  All requests             │  All requests
    │  are allowed              │  are rejected
    │                           │
    │                           │  timeout expires
    │      success              │
    │   (success_threshold      ▼
    │    consecutive)     ┌───────────┐
    └─────────────────────│ HALF_OPEN │
                          └───────────┘
                           Limited requests
                           allowed to test
```

| State | Behavior | Transition |
|-------|----------|------------|
| **CLOSED** | All requests pass through | → OPEN when failures reach `failure_threshold` |
| **OPEN** | All requests rejected immediately | → HALF_OPEN after `reset_timeout` elapses |
| **HALF_OPEN** | Limited requests allowed | → CLOSED on `success_threshold` successes, → OPEN on any failure |

### 2.2 Configuration

```cpp
// Legacy config (monitoring_system adapter)
circuit_breaker_config config;
config.failure_threshold = 5;        // Failures before opening
config.timeout = std::chrono::milliseconds(30000);
config.reset_timeout = std::chrono::milliseconds(60000);  // OPEN → HALF_OPEN time
config.success_threshold = 3;        // Successes to close from half-open
```

> **Note**: The monitoring_system circuit breaker is an adapter wrapping
> `common::resilience::circuit_breaker`. Direct use of the common_system
> implementation is recommended for new code.

### 2.3 Usage

```cpp
#include <kcenon/monitoring/reliability/circuit_breaker.h>

// Create circuit breaker for an exporter connection
circuit_breaker<bool> cb("otlp_exporter", circuit_breaker_config{
    .failure_threshold = 5,
    .reset_timeout = std::chrono::seconds(30),
    .success_threshold = 2
});

// Execute with circuit breaker protection
auto result = cb.execute([&]() -> common::Result<bool> {
    return exporter->export_spans(spans);
});

if (result.is_err()) {
    // Could be: operation failed OR circuit is open
}

// Execute with fallback
auto result_with_fallback = cb.execute(
    [&]() -> common::Result<bool> {
        return exporter->export_spans(spans);
    },
    [&]() -> common::Result<bool> {
        // Fallback: buffer spans for later
        buffer.push(spans);
        return common::ok(true);
    }
);

// Monitor state
circuit_state state = cb.get_state();
auto metrics = cb.get_metrics();
double success_rate = metrics.get_success_rate();
```

---

## 3. Retry Policy

**File**: [`retry_policy.h`](../../include/kcenon/monitoring/reliability/retry_policy.h)

The retry executor automatically retries failed operations using configurable
delay strategies.

### 3.1 Retry Strategies

| Strategy | Delay Pattern | Formula | Example (1s base) |
|----------|--------------|---------|-------------------|
| `fixed_delay` | Same delay each time | `initial_delay` | 1s, 1s, 1s, 1s |
| `exponential_backoff` | Doubles each attempt | `initial × 2^(n-1)` | 1s, 2s, 4s, 8s |
| `linear_backoff` | Increases linearly | `initial × n` | 1s, 2s, 3s, 4s |
| `fibonacci_backoff` | Fibonacci sequence | `initial × fib(n)` | 1s, 1s, 2s, 3s, 5s |

All strategies are capped by `max_delay` to prevent excessively long waits.

### 3.2 Configuration

```cpp
struct retry_config {
    size_t max_attempts = 3;
    retry_strategy strategy = retry_strategy::exponential_backoff;
    std::chrono::milliseconds initial_delay{1000};
    std::chrono::milliseconds max_delay{30000};
    double backoff_multiplier = 2.0;
    std::function<bool(const error_info&)> should_retry = nullptr;
};
```

#### Factory Functions

```cpp
// Exponential backoff (most common)
auto config = create_exponential_backoff_config(
    5,                                    // max attempts
    std::chrono::milliseconds(500)        // initial delay
);

// Fixed delay
auto config = create_fixed_delay_config(
    3,                                    // max attempts
    std::chrono::milliseconds(2000)       // fixed delay
);

// Fibonacci backoff
auto config = create_fibonacci_backoff_config(
    4,                                    // max attempts
    std::chrono::milliseconds(1000)       // base delay
);
```

### 3.3 Usage

```cpp
#include <kcenon/monitoring/reliability/retry_policy.h>

// Create retry executor
retry_executor<bool> retry("metric_export", create_exponential_backoff_config(3));

// Execute with retry
auto result = retry.execute([&]() -> common::Result<bool> {
    return exporter->export_metrics(data);
});

// Conditional retry with predicate
retry_config config;
config.max_attempts = 5;
config.strategy = retry_strategy::exponential_backoff;
config.should_retry = [](const error_info& err) {
    // Only retry on network errors, not validation errors
    return err.code() == monitoring_error_code::network_error;
};

retry_executor<bool> selective_retry("selective", config);
auto result = selective_retry.execute(func);

// Check metrics
auto metrics = retry.get_metrics();
// metrics.total_retries, metrics.successful_executions, etc.
```

---

## 4. Error Boundary

**File**: [`error_boundary.h`](../../include/kcenon/monitoring/reliability/error_boundary.h)

Error boundaries isolate component failures and provide fallback mechanisms.
They track degradation levels and support automatic recovery.

### 4.1 Degradation Levels

```cpp
enum class degradation_level {
    normal = 0,      // Full functionality
    limited = 1,     // Some features reduced
    minimal = 2,     // Only essential features
    emergency = 3    // Bare minimum operation
};
```

### 4.2 Error Boundary Policies

| Policy | Behavior |
|--------|----------|
| `fail_fast` | Return error immediately |
| `isolate` | Return isolation error, prevent propagation |
| `degrade` | Escalate degradation level, return error |
| `fallback` | Use configured fallback strategy |

### 4.3 Fallback Strategies

Three built-in fallback strategies:

```cpp
// 1. Default value: always return a constant
auto strategy = std::make_shared<default_value_strategy<int>>(0);

// 2. Cached value: return last known good value (with TTL)
auto strategy = std::make_shared<cached_value_strategy<MetricsData>>(
    std::chrono::seconds(300)  // Cache TTL: 5 minutes
);
strategy->update_cache(latest_data);  // Update when data is fresh

// 3. Alternative service: delegate to a backup
auto strategy = std::make_shared<alternative_service_strategy<bool>>(
    [&]() -> common::Result<bool> {
        return backup_exporter->export_metrics(data);
    }
);
```

### 4.4 Configuration

```cpp
error_boundary_config config;
config.name = "collector_boundary";
config.error_threshold = 5;           // Errors before degradation
config.error_window = std::chrono::seconds(60);
config.policy = error_boundary_policy::fallback;
config.enable_automatic_recovery = true;
config.recovery_timeout = std::chrono::milliseconds(5000);
config.max_degradation = degradation_level::minimal;
```

### 4.5 Usage

```cpp
#include <kcenon/monitoring/reliability/error_boundary.h>

// Create error boundary with fallback
error_boundary<metrics_snapshot> boundary("collector", config);
boundary.set_fallback_strategy(
    std::make_shared<cached_value_strategy<metrics_snapshot>>(
        std::chrono::seconds(60)
    )
);

// Set error handler for logging
boundary.set_error_handler([](const error_info& err, degradation_level level) {
    log_error("Boundary error at level {}: {}", static_cast<int>(level), err.message());
});

// Execute with protection
auto result = boundary.execute([&]() -> common::Result<metrics_snapshot> {
    return collector->collect();
});

// Check health
auto healthy = boundary.is_healthy();
degradation_level level = boundary.get_degradation_level();
```

---

## 5. Graceful Degradation

**File**: [`graceful_degradation.h`](../../include/kcenon/monitoring/reliability/graceful_degradation.h)

The graceful degradation manager coordinates service-level degradation and
recovery. It manages multiple services with priority levels and supports
degradation plans for coordinated responses.

### 5.1 Service Priorities

```cpp
enum class service_priority {
    optional = 0,    // Can be disabled first
    normal = 1,      // Standard priority
    important = 2,   // Should remain active longer
    critical = 3     // Last to be degraded
};
```

### 5.2 Degradation Plans

Plans define coordinated degradation across multiple services:

```cpp
degradation_plan plan;
plan.name = "high_load";
plan.services_to_maintain = {"metrics_collection", "alerting"};
plan.services_to_disable = {"analytics", "reporting"};
plan.target_level = degradation_level::limited;
```

When executed, services in `services_to_maintain` are set to the target
degradation level, while services in `services_to_disable` are set to
`emergency` (effectively disabled).

### 5.3 Configuration

```cpp
service_config svc_config;
svc_config.name = "metrics_collection";
svc_config.priority = service_priority::critical;
svc_config.error_rate_threshold = 0.5;     // Degrade at 50% error rate
svc_config.health_check_interval = std::chrono::milliseconds(5000);
svc_config.auto_recover = true;
```

### 5.4 Usage

```cpp
#include <kcenon/monitoring/reliability/graceful_degradation.h>

// Create manager
auto manager = create_degradation_manager("monitoring");

// Register services with priorities
manager->register_service(create_service_config("metrics", service_priority::critical));
manager->register_service(create_service_config("traces", service_priority::important));
manager->register_service(create_service_config("analytics", service_priority::optional));

// Add a degradation plan
manager->add_degradation_plan(create_degradation_plan(
    "memory_pressure",
    {"metrics"},              // Keep these running
    {"analytics", "traces"},  // Disable these
    degradation_level::limited
));

// Execute plan when memory is low
manager->execute_plan("memory_pressure", "Memory usage above 90%");

// Check individual service state
auto level = manager->get_service_level("analytics");
// level == degradation_level::emergency (disabled by plan)

// Recover when load normalizes
manager->recover_all_services();

// Wrap services with degradation awareness
auto degradable = create_degradable_service<metrics_snapshot>(
    "metrics",
    manager,
    [&]() { return collector->full_collect(); },           // Normal operation
    [&](degradation_level level) {                          // Degraded operation
        if (level == degradation_level::limited)
            return collector->collect_essential_only();
        return common::Result<metrics_snapshot>::err(/* ... */);
    }
);

auto result = degradable->execute();  // Auto-selects normal or degraded
```

---

## 6. Fault Tolerance Manager

**File**: [`fault_tolerance_manager.h`](../../include/kcenon/monitoring/reliability/fault_tolerance_manager.h)

The fault tolerance manager orchestrates circuit breaker and retry patterns
together. It supports two composition strategies and includes timeout support.

### 6.1 Composition Strategies

| Strategy | Order | Use When |
|----------|-------|----------|
| `circuit_breaker_first` (default) | CB → Retry | Want to fail fast if service is known down |
| Retry first | Retry → CB | Want to retry before tripping the circuit |

**Circuit breaker first** (recommended):
```
Request → Circuit Breaker check → [If CLOSED] → Retry loop → Service call
                                  [If OPEN]   → Reject immediately
```

**Retry first**:
```
Request → Retry loop → Circuit Breaker check → [If CLOSED] → Service call
                                                [If OPEN]   → Count as retry failure
```

### 6.2 Configuration

```cpp
fault_tolerance_config config;
config.enable_circuit_breaker = true;
config.enable_retry = true;
config.circuit_breaker_first = true;  // CB wraps retry
config.circuit_config = circuit_breaker_config{
    .failure_threshold = 5,
    .reset_timeout = std::chrono::seconds(30),
    .success_threshold = 2
};
config.retry_cfg = create_exponential_backoff_config(3);
```

### 6.3 Usage

```cpp
#include <kcenon/monitoring/reliability/fault_tolerance_manager.h>

// Create manager with both patterns
fault_tolerance_manager<bool> ftm("exporter", config);

// Execute with combined protection
auto result = ftm.execute([&]() -> common::Result<bool> {
    return exporter->export_metrics(data);
});

// Execute with timeout
auto result = ftm.execute_with_timeout(
    [&]() -> common::Result<bool> {
        return slow_exporter->export_metrics(data);
    },
    std::chrono::seconds(5)  // 5-second timeout
);

// Check health (circuit breaker state)
auto healthy = ftm.is_healthy();

// Metrics
auto metrics = ftm.get_metrics();
// metrics.total_operations, metrics.circuit_breaker_rejections, metrics.timeouts
```

### 6.4 Registries

Global registries allow centralized management:

```cpp
// Register a fault tolerance manager globally
auto ftm = std::make_shared<fault_tolerance_manager<bool>>("exporter", config);
global_fault_tolerance_registry().register_manager<bool>("exporter", ftm);

// Retrieve from anywhere
auto manager = global_fault_tolerance_registry().get_manager<bool>("exporter");
```

---

## 7. Resource Manager

**File**: [`resource_manager.h`](../../include/kcenon/monitoring/reliability/resource_manager.h)

The resource manager enforces rate limits, memory quotas, and CPU throttling
to prevent resource exhaustion.

### 7.1 Rate Limiting

Two rate limiting algorithms are available:

| Algorithm | Behavior | Best For |
|-----------|----------|----------|
| **Token Bucket** | Tokens added at fixed rate, consumed by requests. Allows bursts up to capacity | APIs with burst tolerance |
| **Leaky Bucket** | Requests fill bucket, leaks at constant rate. Smooths traffic | Consistent throughput |

```cpp
// Token bucket: 100 req/s with burst of 10
auto limiter = create_token_bucket_limiter("api", 100.0, 10);

// Leaky bucket: 50 req/s with capacity of 20
auto limiter = create_leaky_bucket_limiter("export", 50.0, 20);

// Check if request is allowed
if (limiter->try_acquire()) {
    // Proceed with request
}

// Execute with rate limiting
auto result = limiter->execute([&]() -> common::Result<bool> {
    return exporter->export_metrics(data);
});
// Returns error with monitoring_error_code::resource_exhausted if rate limited
```

### 7.2 Memory Quota

```cpp
resource_quota quota(resource_type::memory, 100 * 1024 * 1024);  // 100 MB
// Automatically sets: warning_threshold = 70 MB, critical_threshold = 90 MB

auto quota_mgr = create_memory_quota_manager("snapshot_buffer", 100 * 1024 * 1024);

// Allocate
auto result = quota_mgr->allocate(1024);  // 1 KB
if (result.is_err()) {
    // Quota exceeded
}

// Deallocate
quota_mgr->deallocate(1024);

// Monitor thresholds
if (quota_mgr->is_over_warning_threshold()) {
    // Start reducing memory usage
}
if (quota_mgr->is_over_critical_threshold()) {
    // Emergency cleanup
}
```

### 7.3 CPU Throttling

```cpp
cpu_throttle_config cpu_config;
cpu_config.max_cpu_usage = 0.8;       // 80% max
cpu_config.warning_threshold = 0.7;   // Warn at 70%
cpu_config.strategy = throttling_strategy::reject;
```

### 7.4 Coordinated Resource Management

The `resource_manager` coordinates all resource types:

```cpp
auto rm = create_resource_manager("monitoring");

// Add rate limiter
rm->add_rate_limiter("api", rate_limit_config{
    .rate_per_second = 1000.0,
    .burst_capacity = 50,
    .strategy = throttling_strategy::reject
});

// Add memory quota
rm->add_memory_quota("snapshots", resource_quota{
    resource_type::memory, 256 * 1024 * 1024  // 256 MB
});

// Add CPU throttler
rm->add_cpu_throttler("collection", cpu_throttle_config{
    .max_cpu_usage = 0.8
});

// Health check across all resources
auto healthy = rm->is_healthy();

// Get all metrics
auto all_metrics = rm->get_all_metrics();
```

---

## 8. Data Consistency

**File**: [`data_consistency.h`](../../include/kcenon/monitoring/reliability/data_consistency.h)

The data consistency subsystem provides transactional operations with rollback
and periodic state validation with auto-repair.

### 8.1 Transactions

Transactions group multiple operations into an atomic unit. If any operation
fails, all previously executed operations are rolled back in reverse order.

```cpp
// Transaction lifecycle
Active ──── commit() ────► Committed
  │
  │         abort()
  └──── or timeout ────► Aborted
```

#### Transaction Operations

Each operation has an `execute` function and an optional `rollback` function:

```cpp
auto op = std::make_unique<transaction_operation>(
    "store_metrics",
    [&]() -> common::VoidResult {
        return storage->store(snapshot);
    },
    [&]() -> common::VoidResult {
        return storage->clear_latest();  // Rollback
    }
);
```

#### Transaction Manager

```cpp
#include <kcenon/monitoring/reliability/data_consistency.h>

transaction_config config;
config.timeout = std::chrono::seconds(30);
config.lock_timeout = std::chrono::seconds(10);
config.max_retries = 3;

auto tm = std::make_shared<transaction_manager>("metrics_tx", config);

// Begin transaction
auto tx_result = tm->begin_transaction("tx_001");
if (tx_result.is_ok()) {
    auto tx = tx_result.value();

    // Add operations
    tx->add_operation(std::make_unique<transaction_operation>(
        "write_metrics", write_func, rollback_func
    ));
    tx->add_operation(std::make_unique<transaction_operation>(
        "update_index", index_func, index_rollback_func
    ));

    // Commit (executes all operations, rolls back on failure)
    bool success = tm->commit_transaction("tx_001");
}

// Deadlock detection
auto deadlocked = tm->detect_deadlocks();
for (const auto& id : deadlocked.value()) {
    tm->abort_transaction(id);
}
```

### 8.2 State Validation

The state validator runs periodic validation checks and optionally
auto-repairs inconsistencies:

```cpp
validation_config config;
config.validation_interval = std::chrono::seconds(60);
config.max_validation_failures = 5;
config.corruption_threshold = 0.1;  // 10% corruption tolerance
config.enable_auto_repair = true;

auto validator = std::make_shared<state_validator>("metrics_state", config);

// Add validation rules
validator->add_validation_rule(
    "snapshot_count",
    [&]() -> validation_result {
        return storage->size() <= storage->capacity()
            ? validation_result::valid
            : validation_result::invalid;
    },
    [&]() -> common::VoidResult {
        // Auto-repair: trim to capacity
        return storage->trim_to_capacity();
    }
);

// Start periodic validation
validator->start();

// Manual validation
auto results = validator->validate();
// results.value() == {"snapshot_count": valid, "snapshot_count_after_repair": valid}
```

### 8.3 Data Consistency Manager

Coordinates multiple transaction managers and validators:

```cpp
auto dcm = create_data_consistency_manager("monitoring");

// Add transaction manager for metric operations
dcm->add_transaction_manager("metrics", transaction_config{
    .timeout = std::chrono::seconds(30),
    .max_retries = 3
});

// Add state validator
dcm->add_state_validator("integrity", validation_config{
    .validation_interval = std::chrono::seconds(60),
    .enable_auto_repair = true
});

// Start all validators
dcm->start_all_validators();

// Use transaction manager
auto* tm = dcm->get_transaction_manager("metrics");
auto tx = tm->begin_transaction("batch_write");

// Shutdown
dcm->stop_all_validators();
```

---

## 9. Composition Patterns

### 9.1 Pattern: CB + Retry + Error Boundary

The most common production composition layers all three patterns:

```cpp
// Layer 1: Error boundary (outermost — catches everything)
error_boundary_config eb_config;
eb_config.name = "exporter";
eb_config.policy = error_boundary_policy::fallback;
eb_config.enable_automatic_recovery = true;

error_boundary<bool> boundary("exporter", eb_config);
boundary.set_fallback_strategy(
    std::make_shared<cached_value_strategy<bool>>(std::chrono::seconds(300))
);

// Layer 2: Fault tolerance manager (circuit breaker + retry)
fault_tolerance_config ft_config;
ft_config.enable_circuit_breaker = true;
ft_config.enable_retry = true;
ft_config.circuit_breaker_first = true;
ft_config.retry_cfg = create_exponential_backoff_config(3);

fault_tolerance_manager<bool> ftm("exporter", ft_config);

// Combined execution: Error Boundary → Circuit Breaker → Retry → Operation
auto result = boundary.execute([&]() -> common::Result<bool> {
    return ftm.execute([&]() -> common::Result<bool> {
        return exporter->export_metrics(data);
    });
});
```

### 9.2 Pattern: Degradation + Resource Manager

Combine resource monitoring with graceful degradation for load management:

```cpp
auto degradation = create_degradation_manager("monitoring");
auto resources = create_resource_manager("monitoring");

// Register services
degradation->register_service(create_service_config("full_collection", service_priority::important));
degradation->register_service(create_service_config("analytics", service_priority::optional));

// Add memory quota
resources->add_memory_quota("buffer", resource_quota{resource_type::memory, 512 * 1024 * 1024});

// Monitor and degrade
auto* quota = resources->get_memory_quota("buffer");
if (quota && quota->is_over_critical_threshold()) {
    degradation->degrade_service("analytics", degradation_level::emergency, "Memory critical");
    degradation->degrade_service("full_collection", degradation_level::limited, "Memory critical");
}
```

### 9.3 Pattern: Transactions + Validation

Ensure data operations are atomic and periodically validated:

```cpp
auto consistency = create_data_consistency_manager("metrics_pipeline");

// Transaction manager for batch writes
consistency->add_transaction_manager("batch", transaction_config{
    .timeout = std::chrono::seconds(10)
});

// Validator to check data integrity
consistency->add_state_validator("integrity", validation_config{
    .validation_interval = std::chrono::minutes(5),
    .enable_auto_repair = true
});

auto* validator = consistency->get_state_validator("integrity");
validator->add_validation_rule("data_freshness",
    [&]() -> validation_result {
        auto age = get_latest_snapshot_age();
        return age < std::chrono::minutes(5)
            ? validation_result::valid
            : validation_result::invalid;
    },
    [&]() -> common::VoidResult {
        return trigger_immediate_collection();
    }
);

consistency->start_all_validators();
```

---

## 10. Real-World Examples

### 10.1 Protecting an OTLP Exporter

Complete fault tolerance for an OTLP gRPC exporter connection:

```cpp
#include <kcenon/monitoring/reliability/fault_tolerance_manager.h>
#include <kcenon/monitoring/reliability/error_boundary.h>

// Configure fault tolerance
fault_tolerance_config ft_config;
ft_config.enable_circuit_breaker = true;
ft_config.enable_retry = true;
ft_config.circuit_breaker_first = true;
ft_config.circuit_config.failure_threshold = 3;
ft_config.circuit_config.reset_timeout = std::chrono::seconds(60);
ft_config.retry_cfg = create_exponential_backoff_config(3, std::chrono::seconds(1));
ft_config.retry_cfg.should_retry = [](const error_info& err) {
    // Only retry on network errors
    return err.code() == monitoring_error_code::network_error
        || err.code() == monitoring_error_code::operation_timeout;
};

fault_tolerance_manager<bool> ftm("otlp_export", ft_config);

// Export with full protection
auto result = ftm.execute_with_timeout(
    [&]() -> common::Result<bool> {
        return otlp_exporter->export_spans(spans);
    },
    std::chrono::seconds(10)
);

if (result.is_err()) {
    // Log and buffer for later
    span_buffer.push(spans);
}
```

### 10.2 Rate-Limited Metric Collection

Prevent metric collection from consuming excessive resources:

```cpp
#include <kcenon/monitoring/reliability/resource_manager.h>

auto rm = create_resource_manager("collection");

// Rate limit collection to 100 snapshots/second
rm->add_rate_limiter("collection_rate", rate_limit_config{
    .rate_per_second = 100.0,
    .burst_capacity = 20
});

// Memory quota for snapshot buffer: 256 MB
rm->add_memory_quota("snapshot_buffer", resource_quota{
    resource_type::memory, 256 * 1024 * 1024
});

// Collect with rate limiting
auto* limiter = rm->get_rate_limiter("collection_rate");
auto* quota = rm->get_memory_quota("snapshot_buffer");

auto result = limiter->execute([&]() -> common::Result<bool> {
    auto alloc = quota->allocate(estimated_snapshot_size);
    if (alloc.is_err()) return alloc;

    auto collect_result = collector->collect();
    if (collect_result.is_err()) {
        quota->deallocate(estimated_snapshot_size);
    }
    return common::ok(true);
});
```

### 10.3 Coordinated Service Degradation Under Load

Systematically reduce features when the system is under pressure:

```cpp
#include <kcenon/monitoring/reliability/graceful_degradation.h>

auto manager = create_degradation_manager("monitoring_system");

// Register services by priority
manager->register_service(create_service_config("core_metrics", service_priority::critical));
manager->register_service(create_service_config("trace_export", service_priority::important));
manager->register_service(create_service_config("analytics", service_priority::normal));
manager->register_service(create_service_config("reporting", service_priority::optional));

// Define degradation plans
manager->add_degradation_plan(create_degradation_plan(
    "moderate_load",
    {"core_metrics", "trace_export", "analytics"},  // Maintain (at limited level)
    {"reporting"},                                    // Disable
    degradation_level::limited
));

manager->add_degradation_plan(create_degradation_plan(
    "severe_load",
    {"core_metrics"},                                 // Only keep critical
    {"trace_export", "analytics", "reporting"},        // Disable everything else
    degradation_level::minimal
));

// Respond to load conditions
if (cpu_usage > 0.9 || memory_usage > 0.95) {
    manager->execute_plan("severe_load", "System resources critical");
} else if (cpu_usage > 0.7 || memory_usage > 0.8) {
    manager->execute_plan("moderate_load", "System resources high");
} else {
    manager->recover_all_services();
}
```

### 10.4 Atomic Metric Pipeline Operations

Ensure metric batch writes are transactional:

```cpp
#include <kcenon/monitoring/reliability/data_consistency.h>

auto tm = create_transaction_manager("pipeline");

// Begin atomic batch operation
auto tx_result = tm->begin_transaction("batch_001");
auto tx = tx_result.value();

// Add store operation with rollback
tx->add_operation(std::make_unique<transaction_operation>(
    "store_snapshot",
    [&]() -> common::VoidResult {
        return primary_storage->store(snapshot);
    },
    [&]() -> common::VoidResult {
        return primary_storage->remove_latest();
    }
));

// Add backup operation
tx->add_operation(std::make_unique<transaction_operation>(
    "backup_snapshot",
    [&]() -> common::VoidResult {
        return backup_storage->store(snapshot);
    },
    [&]() -> common::VoidResult {
        return backup_storage->remove_latest();
    }
));

// Add index update
tx->add_operation(std::make_unique<transaction_operation>(
    "update_index",
    [&]() -> common::VoidResult {
        return index->add_entry(snapshot.source_id, snapshot.capture_time);
    },
    [&]() -> common::VoidResult {
        return index->remove_latest_entry();
    }
));

// Commit: executes all 3 operations, rolls back on any failure
bool success = tm->commit_transaction("batch_001");
if (!success) {
    // All operations rolled back automatically
    log_error("Batch write failed, all changes rolled back");
}
```

---

## Appendix A: Metrics Reference

All reliability components expose metrics for monitoring:

| Component | Metric | Type |
|-----------|--------|------|
| Circuit Breaker | `total_calls`, `successful_calls`, `failed_calls`, `rejected_calls` | `circuit_breaker_metrics` |
| Retry Executor | `total_executions`, `successful_executions`, `failed_executions`, `total_retries` | `retry_metrics` |
| Error Boundary | `total_operations`, `successful_operations`, `failed_operations`, `recovered_operations` | `error_boundary_metrics` |
| Graceful Degradation | `total_degradations`, `successful_degradations`, `recovery_attempts`, `successful_recoveries` | `graceful_degradation_metrics` |
| Fault Tolerance | `total_operations`, `successful_operations`, `failed_operations`, `circuit_breaker_rejections`, `timeouts` | `fault_tolerance_metrics` |
| Resource Manager | `current_usage`, `total_allocations`, `peak_usage`, `rejected_operations` | `resource_metrics` |
| Data Consistency | `total_transactions`, `committed_transactions`, `aborted_transactions`, `deadlocks_detected` | `transaction_metrics` |

## Appendix B: Error Codes

| Error Code | Used By | Meaning |
|-----------|---------|---------|
| `circuit_breaker_open` | Circuit Breaker | Circuit is open, request rejected |
| `operation_failed` | Retry, Error Boundary | Operation failed after all attempts |
| `operation_timeout` | Fault Tolerance | Operation exceeded timeout |
| `service_degraded` | Error Boundary, Degradation | Service is in degraded state |
| `resource_exhausted` | Resource Manager | Rate limit or quota exceeded |
| `already_exists` | Degradation, Consistency | Duplicate registration |
| `not_found` | Degradation, Consistency | Entity not found |
| `already_started` | State Validator | Validator already running |

## Appendix C: File Reference

| File | Path |
|------|------|
| `circuit_breaker.h` | `include/kcenon/monitoring/reliability/circuit_breaker.h` |
| `retry_policy.h` | `include/kcenon/monitoring/reliability/retry_policy.h` |
| `error_boundary.h` | `include/kcenon/monitoring/reliability/error_boundary.h` |
| `graceful_degradation.h` | `include/kcenon/monitoring/reliability/graceful_degradation.h` |
| `fault_tolerance_manager.h` | `include/kcenon/monitoring/reliability/fault_tolerance_manager.h` |
| `resource_manager.h` | `include/kcenon/monitoring/reliability/resource_manager.h` |
| `data_consistency.h` | `include/kcenon/monitoring/reliability/data_consistency.h` |

---

*This guide is part of the monitoring_system documentation suite.*
*See also: [Collector Development Guide](COLLECTOR_DEVELOPMENT.md) | [Exporter Development Guide](EXPORTER_DEVELOPMENT.md) | [Storage Backends](STORAGE_BACKENDS.md) | [Architecture](../ARCHITECTURE.md)*
