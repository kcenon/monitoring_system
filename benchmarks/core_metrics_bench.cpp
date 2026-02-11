// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/**
 * @file core_metrics_bench.cpp
 * @brief Benchmarks for core metric types (counter, gauge, histogram, summary, timer)
 * @details Measures fundamental metric operation overhead to fill BASELINE.md targets.
 *
 * Target Metrics (from BASELINE.md):
 * - Counter increment: < 100ns
 * - Gauge set/get: < 100ns
 * - Histogram update: < 200ns
 * - Summary add_sample: < 200ns
 * - Timer record: < 200ns
 * - Metric batch operations: < 1μs for 10-item batch
 * - Hash function: < 50ns
 *
 * Closes #476
 */

#include <benchmark/benchmark.h>
#include <kcenon/monitoring/utils/metric_types.h>
#include <string>
#include <cstdint>

using namespace kcenon::monitoring;

// =============================================================================
// Counter-like operations (compact_metric_value with int64_t increment)
// =============================================================================

static void BM_CounterIncrement(benchmark::State& state) {
    auto meta = create_metric_metadata("requests_total", metric_type::counter);
    int64_t counter = 0;

    for (auto _ : state) {
        counter++;
        compact_metric_value val(meta, counter);
        benchmark::DoNotOptimize(val);
    }

    state.SetItemsProcessed(state.iterations());
    state.SetLabel("counter_increment");
}
BENCHMARK(BM_CounterIncrement);

static void BM_CounterValueRetrieval(benchmark::State& state) {
    auto meta = create_metric_metadata("requests_total", metric_type::counter);
    compact_metric_value val(meta, int64_t(42));

    for (auto _ : state) {
        auto v = val.as_int64();
        benchmark::DoNotOptimize(v);
    }

    state.SetItemsProcessed(state.iterations());
    state.SetLabel("counter_get");
}
BENCHMARK(BM_CounterValueRetrieval);

// =============================================================================
// Gauge operations (compact_metric_value with double set/get)
// =============================================================================

static void BM_GaugeSet(benchmark::State& state) {
    auto meta = create_metric_metadata("cpu_usage", metric_type::gauge);
    double value = 0.0;

    for (auto _ : state) {
        value += 0.1;
        compact_metric_value val(meta, value);
        benchmark::DoNotOptimize(val);
    }

    state.SetItemsProcessed(state.iterations());
    state.SetLabel("gauge_set");
}
BENCHMARK(BM_GaugeSet);

static void BM_GaugeGet(benchmark::State& state) {
    auto meta = create_metric_metadata("cpu_usage", metric_type::gauge);
    compact_metric_value val(meta, 73.5);

    for (auto _ : state) {
        auto v = val.as_double();
        benchmark::DoNotOptimize(v);
    }

    state.SetItemsProcessed(state.iterations());
    state.SetLabel("gauge_get");
}
BENCHMARK(BM_GaugeGet);

// =============================================================================
// Histogram operations
// =============================================================================

static void BM_HistogramUpdate(benchmark::State& state) {
    histogram_data hist;
    hist.init_standard_buckets();
    double sample = 0.001;

    for (auto _ : state) {
        hist.add_sample(sample);
        sample += 0.001;
        if (sample > 10.0) sample = 0.001;
    }

    state.SetItemsProcessed(state.iterations());
    state.SetLabel("histogram_add_sample");
}
BENCHMARK(BM_HistogramUpdate);

static void BM_HistogramUpdate_HotPath(benchmark::State& state) {
    histogram_data hist;
    hist.init_standard_buckets();

    // All samples fall in first bucket — best case
    for (auto _ : state) {
        hist.add_sample(0.001);
    }

    state.SetItemsProcessed(state.iterations());
    state.SetLabel("histogram_hot_path");
}
BENCHMARK(BM_HistogramUpdate_HotPath);

static void BM_HistogramMean(benchmark::State& state) {
    histogram_data hist;
    hist.init_standard_buckets();
    for (int i = 0; i < 10000; ++i) {
        hist.add_sample(static_cast<double>(i) * 0.001);
    }

    for (auto _ : state) {
        auto m = hist.mean();
        benchmark::DoNotOptimize(m);
    }

    state.SetItemsProcessed(state.iterations());
    state.SetLabel("histogram_mean");
}
BENCHMARK(BM_HistogramMean);

// =============================================================================
// Summary operations
// =============================================================================

static void BM_SummaryAddSample(benchmark::State& state) {
    summary_data summary;
    double value = 1.0;

    for (auto _ : state) {
        summary.add_sample(value);
        value += 0.1;
    }

    state.SetItemsProcessed(state.iterations());
    state.SetLabel("summary_add_sample");
}
BENCHMARK(BM_SummaryAddSample);

static void BM_SummaryMean(benchmark::State& state) {
    summary_data summary;
    for (int i = 0; i < 10000; ++i) {
        summary.add_sample(static_cast<double>(i));
    }

    for (auto _ : state) {
        auto m = summary.mean();
        benchmark::DoNotOptimize(m);
    }

    state.SetItemsProcessed(state.iterations());
    state.SetLabel("summary_mean");
}
BENCHMARK(BM_SummaryMean);

// =============================================================================
// Timer operations
// =============================================================================

static void BM_TimerRecord(benchmark::State& state) {
    timer_data timer;
    double duration = 1.0;

    for (auto _ : state) {
        timer.record(duration);
        duration += 0.1;
        if (duration > 100.0) duration = 1.0;
    }

    state.SetItemsProcessed(state.iterations());
    state.SetLabel("timer_record");
}
BENCHMARK(BM_TimerRecord);

static void BM_TimerRecord_ReservoirFull(benchmark::State& state) {
    timer_data timer(256);  // Small reservoir for fast fill

    // Fill reservoir first
    for (size_t i = 0; i < 256; ++i) {
        timer.record(static_cast<double>(i));
    }

    double duration = 1.0;
    for (auto _ : state) {
        timer.record(duration);
        duration += 0.1;
    }

    state.SetItemsProcessed(state.iterations());
    state.SetLabel("timer_reservoir_sampling");
}
BENCHMARK(BM_TimerRecord_ReservoirFull);

static void BM_TimerPercentile(benchmark::State& state) {
    timer_data timer;
    for (int i = 0; i < 1000; ++i) {
        timer.record(static_cast<double>(i) * 0.1);
    }

    for (auto _ : state) {
        auto p99 = timer.p99();
        benchmark::DoNotOptimize(p99);
    }

    state.SetItemsProcessed(state.iterations());
    state.SetLabel("timer_p99");
}
BENCHMARK(BM_TimerPercentile);

static void BM_TimerSnapshot(benchmark::State& state) {
    timer_data timer;
    for (int i = 0; i < 1000; ++i) {
        timer.record(static_cast<double>(i) * 0.1);
    }

    for (auto _ : state) {
        auto snap = timer.get_snapshot();
        benchmark::DoNotOptimize(snap);
    }

    state.SetItemsProcessed(state.iterations());
    state.SetLabel("timer_snapshot");
}
BENCHMARK(BM_TimerSnapshot);

// =============================================================================
// Metric batch operations
// =============================================================================

static void BM_MetricBatchAdd(benchmark::State& state) {
    auto batch_size = state.range(0);

    for (auto _ : state) {
        metric_batch batch(1);
        batch.reserve(batch_size);

        for (int64_t i = 0; i < batch_size; ++i) {
            auto meta = metric_metadata(static_cast<uint32_t>(i), metric_type::counter);
            batch.add_metric(compact_metric_value(meta, i));
        }

        benchmark::DoNotOptimize(batch);
    }

    state.SetItemsProcessed(state.iterations() * batch_size);
    state.SetLabel("batch_add");
}
BENCHMARK(BM_MetricBatchAdd)->Arg(10)->Arg(100)->Arg(1000);

static void BM_MetricBatchMemoryFootprint(benchmark::State& state) {
    metric_batch batch(1);
    for (int64_t i = 0; i < 100; ++i) {
        auto meta = metric_metadata(static_cast<uint32_t>(i), metric_type::gauge);
        batch.add_metric(compact_metric_value(meta, static_cast<double>(i)));
    }

    for (auto _ : state) {
        auto footprint = batch.memory_footprint();
        benchmark::DoNotOptimize(footprint);
    }

    state.SetItemsProcessed(state.iterations());
    state.SetLabel("batch_memory_footprint");
}
BENCHMARK(BM_MetricBatchMemoryFootprint);

// =============================================================================
// Hash function performance
// =============================================================================

static void BM_MetricNameHash(benchmark::State& state) {
    std::string name = "http_requests_total";

    for (auto _ : state) {
        auto hash = hash_metric_name(name);
        benchmark::DoNotOptimize(hash);
    }

    state.SetItemsProcessed(state.iterations());
    state.SetLabel("fnv1a_hash");
}
BENCHMARK(BM_MetricNameHash);

static void BM_MetricNameHash_Long(benchmark::State& state) {
    std::string name = "http_server_request_duration_seconds_bucket_le_0.5";

    for (auto _ : state) {
        auto hash = hash_metric_name(name);
        benchmark::DoNotOptimize(hash);
    }

    state.SetItemsProcessed(state.iterations());
    state.SetLabel("fnv1a_hash_long");
}
BENCHMARK(BM_MetricNameHash_Long);

static void BM_CreateMetricMetadata(benchmark::State& state) {
    for (auto _ : state) {
        auto meta = create_metric_metadata("test_metric", metric_type::counter, 3);
        benchmark::DoNotOptimize(meta);
    }

    state.SetItemsProcessed(state.iterations());
    state.SetLabel("create_metadata");
}
BENCHMARK(BM_CreateMetricMetadata);

// =============================================================================
// TimerScope RAII overhead
// =============================================================================

static void BM_TimerScopeOverhead(benchmark::State& state) {
    timer_data timer;

    for (auto _ : state) {
        timer_scope scope(timer);
        benchmark::DoNotOptimize(&scope);
    }

    state.SetItemsProcessed(state.iterations());
    state.SetLabel("timer_scope_raii");
}
BENCHMARK(BM_TimerScopeOverhead);
