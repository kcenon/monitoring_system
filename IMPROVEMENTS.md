# Monitoring System - Improvement Plan

> **Language:** **English** | [한국어](IMPROVEMENTS_KO.md)

## Current Status

**Version:** 1.0.0
**Last Review:** 2025-01-20
**Overall Score:** 4.0/5

### Strengths
- Clean Result<T> pattern usage
- Good interface separation
- Comprehensive metrics structure

### Areas for Improvement
- No time-series aggregation
- Missing percentile calculations
- Limited storage backend options

---

## High Priority Improvements

### 1. Add Time-Series Aggregation

**Proposed Feature:**
```cpp
class time_series_aggregator {
public:
    void record(const std::string& metric_name, double value,
               std::chrono::system_clock::time_point timestamp) {
        auto& series = metrics_[metric_name];
        series.add_point(value, timestamp);

        // Downsample old data
        if (series.size() > max_raw_points_) {
            series.downsample();
        }
    }

    result<aggregated_stats> query(
        const std::string& metric_name,
        std::chrono::system_clock::time_point start,
        std::chrono::system_clock::time_point end,
        std::chrono::seconds resolution) {

        auto& series = metrics_[metric_name];
        return series.aggregate(start, end, resolution);
    }

private:
    struct aggregated_stats {
        double min;
        double max;
        double avg;
        double p50;
        double p95;
        double p99;
        size_t count;
    };

    struct time_series {
        void add_point(double value, std::chrono::system_clock::time_point ts);
        void downsample();  // Aggregate old data
        aggregated_stats aggregate(
            std::chrono::system_clock::time_point start,
            std::chrono::system_clock::time_point end,
            std::chrono::seconds resolution);

        std::map<std::chrono::system_clock::time_point, double> raw_points_;
        std::map<std::chrono::system_clock::time_point, aggregated_stats> aggregated_points_;
    };

    std::unordered_map<std::string, time_series> metrics_;
    size_t max_raw_points_ = 10000;
};
```

**Priority:** P2
**Effort:** 5-7 days

---

### 2. Add Percentile Calculations

**Current Issue:**
`metrics_snapshot` only has basic stats, no percentiles.

**Solution:**
```cpp
class percentile_calculator {
public:
    void add_sample(double value) {
        samples_.push_back(value);
        if (samples_.size() > max_samples_) {
            // Use reservoir sampling
            std::uniform_int_distribution<> dist(0, total_samples_);
            size_t idx = dist(rng_);
            if (idx < max_samples_) {
                samples_[idx] = value;
            }
        }
        total_samples_++;
    }

    struct percentiles {
        double p50;  // Median
        double p90;
        double p95;
        double p99;
        double p999;
    };

    percentiles calculate() const {
        if (samples_.empty()) {
            return {0, 0, 0, 0, 0};
        }

        auto sorted = samples_;
        std::sort(sorted.begin(), sorted.end());

        return {
            get_percentile(sorted, 0.50),
            get_percentile(sorted, 0.90),
            get_percentile(sorted, 0.95),
            get_percentile(sorted, 0.99),
            get_percentile(sorted, 0.999)
        };
    }

private:
    static double get_percentile(const std::vector<double>& sorted, double p) {
        if (sorted.empty()) return 0;
        size_t idx = static_cast<size_t>(sorted.size() * p);
        idx = std::min(idx, sorted.size() - 1);
        return sorted[idx];
    }

    std::vector<double> samples_;
    size_t max_samples_ = 10000;
    size_t total_samples_ = 0;
    std::mt19937 rng_;
};
```

**Priority:** P2
**Effort:** 2-3 days

---

### 3. Add Prometheus Exporter

```cpp
class prometheus_exporter {
public:
    std::string export_metrics(const metrics_snapshot& snapshot) const {
        std::ostringstream oss;

        for (const auto& metric : snapshot.metrics) {
            // Prometheus format:
            // # HELP metric_name Description
            // # TYPE metric_name gauge|counter|histogram
            // metric_name{label="value"} 42.0 timestamp

            oss << "# HELP " << sanitize_name(metric.name)
                << " " << get_description(metric.name) << "\n";

            oss << "# TYPE " << sanitize_name(metric.name)
                << " " << get_metric_type(metric.name) << "\n";

            oss << sanitize_name(metric.name);

            // Add labels
            if (!metric.tags.empty()) {
                oss << "{";
                bool first = true;
                for (const auto& [key, value] : metric.tags) {
                    if (!first) oss << ",";
                    oss << sanitize_name(key) << "=\""
                        << escape_label_value(value) << "\"";
                    first = false;
                }
                oss << "}";
            }

            oss << " " << metric.value << " "
                << to_prometheus_timestamp(metric.timestamp) << "\n";
        }

        return oss.str();
    }

private:
    static std::string sanitize_name(std::string_view name) {
        std::string result;
        for (char c : name) {
            if (std::isalnum(c) || c == '_') {
                result += c;
            } else {
                result += '_';
            }
        }
        return result;
    }
};

// HTTP server to expose metrics
class prometheus_http_server {
public:
    void start(unsigned short port, monitoring_interface* monitor) {
        // Simple HTTP server that responds to /metrics
        // Returns Prometheus-formatted metrics
    }
};
```

**Priority:** P2
**Effort:** 3-4 days

---

### 4. Add Alerting System

```cpp
class alert_rule {
public:
    alert_rule(std::string metric_name,
              std::function<bool(double)> condition,
              std::chrono::seconds evaluation_interval)
        : metric_name_(std::move(metric_name))
        , condition_(std::move(condition))
        , evaluation_interval_(evaluation_interval) {}

    bool evaluate(const metrics_snapshot& snapshot) const {
        auto metric = snapshot.get_metric(metric_name_);
        if (!metric) return false;
        return condition_(metric->value);
    }

private:
    std::string metric_name_;
    std::function<bool(double)> condition_;
    std::chrono::seconds evaluation_interval_;
};

class alert_manager {
public:
    void add_rule(std::unique_ptr<alert_rule> rule) {
        rules_.push_back(std::move(rule));
    }

    void evaluate_rules(const metrics_snapshot& snapshot) {
        for (const auto& rule : rules_) {
            if (rule->evaluate(snapshot)) {
                fire_alert(rule->name(), rule->severity());
            }
        }
    }

    void on_alert(std::function<void(const alert&)> handler) {
        alert_handlers_.push_back(std::move(handler));
    }

private:
    std::vector<std::unique_ptr<alert_rule>> rules_;
    std::vector<std::function<void(const alert&)>> alert_handlers_;
};

// Usage:
alert_manager alerts;

alerts.add_rule(std::make_unique<alert_rule>(
    "cpu_usage_percent",
    [](double value) { return value > 80.0; },  // Trigger at 80%
    std::chrono::seconds(60)
));

alerts.on_alert([](const alert& a) {
    send_email("admin@example.com", a.message);
    send_slack_notification(a.message);
});
```

**Priority:** P3
**Effort:** 5-7 days

---

## Medium Priority Improvements

### 5. Add Distributed Tracing

```cpp
class trace_context {
public:
    trace_context(std::string trace_id, std::string span_id)
        : trace_id_(std::move(trace_id))
        , span_id_(std::move(span_id))
        , start_time_(std::chrono::steady_clock::now()) {}

    ~trace_context() {
        auto duration = std::chrono::steady_clock::now() - start_time_;
        record_span(trace_id_, span_id_, duration);
    }

    trace_context create_child_span(std::string operation_name) {
        return trace_context(trace_id_, generate_span_id(), span_id_);
    }

private:
    std::string trace_id_;
    std::string span_id_;
    std::string parent_span_id_;
    std::chrono::steady_clock::time_point start_time_;
};

// Usage:
void handle_request(const Request& req) {
    trace_context trace("trace-123", "span-1");

    {
        auto db_span = trace.create_child_span("database_query");
        execute_query();
    }  // db_span auto-recorded

    {
        auto cache_span = trace.create_child_span("cache_lookup");
        check_cache();
    }  // cache_span auto-recorded
}
```

**Priority:** P3
**Effort:** 7-10 days

---

## Testing Requirements

```cpp
TEST(TimeSeriesAggregator, DownsamplesCorrectly) {
    time_series_aggregator agg;

    // Add 10000 points over 1 hour
    auto start = std::chrono::system_clock::now();
    for (int i = 0; i < 10000; ++i) {
        agg.record("test_metric", i,
                  start + std::chrono::seconds(i));
    }

    // Query with 1-minute resolution
    auto result = agg.query("test_metric", start,
                           start + std::chrono::hours(1),
                           std::chrono::minutes(1));

    ASSERT_TRUE(result.is_ok());
    // Should have ~60 aggregated points
    EXPECT_EQ(result.value().size(), 60);
}

TEST(PercentileCalculator, AccurateForKnownDistribution) {
    percentile_calculator calc;

    // Add 1000 samples: 0-999
    for (int i = 0; i < 1000; ++i) {
        calc.add_sample(i);
    }

    auto pct = calc.calculate();
    EXPECT_NEAR(pct.p50, 500, 10);   // Median ~500
    EXPECT_NEAR(pct.p95, 950, 10);   // 95th ~950
    EXPECT_NEAR(pct.p99, 990, 10);   // 99th ~990
}
```

**Total Effort:** 22-31 days

---

## Integration Examples

### Complete Monitoring Setup

```cpp
// 1. Create monitoring system
auto monitor = std::make_shared<monitoring_system>();

// 2. Add collectors
monitor->add_collector(std::make_unique<cpu_collector>());
monitor->add_collector(std::make_unique<memory_collector>());
monitor->add_collector(std::make_unique<thread_pool_collector>(thread_pool));
monitor->add_collector(std::make_unique<logger_collector>(logger));

// 3. Setup alerting
alert_manager alerts;
alerts.add_rule(/* ... */);
alerts.on_alert([](const alert& a) {
    // Send notifications
});

// 4. Start Prometheus exporter
prometheus_http_server prom_server;
prom_server.start(9090, monitor.get());

// 5. Start collection
monitor->start();

// 6. Query metrics
auto metrics = monitor->collect_now();
auto cpu_usage = metrics.value().get_metric("cpu_usage_percent");
```

---

## References

- [Prometheus Data Model](https://prometheus.io/docs/concepts/data_model/)
- [OpenTelemetry Tracing](https://opentelemetry.io/docs/concepts/signals/traces/)
- [Percentile Estimation Algorithms](https://www.influxdata.com/blog/tldigest-compression-algorithm/)
