// BSD 3-Clause License
//
// Copyright (c) 2021-2025, monitoring_system contributors
// All rights reserved.

#include <gtest/gtest.h>
#include "kcenon/monitoring/utils/metric_types.h"
#include <thread>
#include <cmath>

using namespace kcenon::monitoring;

/**
 * @brief Test suite for timer_data with percentile calculations
 */
class TimerMetricsTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TimerMetricsTest, EmptyTimerReturnsZero) {
    timer_data timer;

    EXPECT_EQ(timer.count(), 0);
    EXPECT_DOUBLE_EQ(timer.mean(), 0.0);
    EXPECT_DOUBLE_EQ(timer.min(), 0.0);
    EXPECT_DOUBLE_EQ(timer.max(), 0.0);
    EXPECT_DOUBLE_EQ(timer.median(), 0.0);
    EXPECT_DOUBLE_EQ(timer.p99(), 0.0);
}

TEST_F(TimerMetricsTest, SingleSample) {
    timer_data timer;
    timer.record(100.0);

    EXPECT_EQ(timer.count(), 1);
    EXPECT_DOUBLE_EQ(timer.mean(), 100.0);
    EXPECT_DOUBLE_EQ(timer.min(), 100.0);
    EXPECT_DOUBLE_EQ(timer.max(), 100.0);
    EXPECT_DOUBLE_EQ(timer.median(), 100.0);
}

TEST_F(TimerMetricsTest, MultipleSamples) {
    timer_data timer;
    for (int i = 1; i <= 100; ++i) {
        timer.record(static_cast<double>(i));
    }

    EXPECT_EQ(timer.count(), 100);
    EXPECT_DOUBLE_EQ(timer.mean(), 50.5);
    EXPECT_DOUBLE_EQ(timer.min(), 1.0);
    EXPECT_DOUBLE_EQ(timer.max(), 100.0);
}

TEST_F(TimerMetricsTest, MedianCalculation) {
    timer_data timer;
    // Add values 1-100
    for (int i = 1; i <= 100; ++i) {
        timer.record(static_cast<double>(i));
    }

    // Median of 1-100 should be around 50.5
    double median = timer.median();
    EXPECT_NEAR(median, 50.5, 1.0);
}

TEST_F(TimerMetricsTest, PercentileCalculations) {
    timer_data timer;
    // Add values 1-1000
    for (int i = 1; i <= 1000; ++i) {
        timer.record(static_cast<double>(i));
    }

    // p50 should be around 500
    EXPECT_NEAR(timer.median(), 500.5, 5.0);

    // p90 should be around 900
    EXPECT_NEAR(timer.p90(), 900.0, 10.0);

    // p95 should be around 950
    EXPECT_NEAR(timer.p95(), 950.0, 10.0);

    // p99 should be around 990
    EXPECT_NEAR(timer.p99(), 990.0, 10.0);
}

TEST_F(TimerMetricsTest, BoundaryPercentiles) {
    timer_data timer;
    timer.record(10.0);
    timer.record(20.0);
    timer.record(30.0);

    // Percentile at 0 should return min
    EXPECT_DOUBLE_EQ(timer.get_percentile(0), 10.0);

    // Percentile at 100 should return max
    EXPECT_DOUBLE_EQ(timer.get_percentile(100), 30.0);
}

TEST_F(TimerMetricsTest, StandardDeviation) {
    timer_data timer;
    // Add known values for predictable stddev
    timer.record(2.0);
    timer.record(4.0);
    timer.record(4.0);
    timer.record(4.0);
    timer.record(5.0);
    timer.record(5.0);
    timer.record(7.0);
    timer.record(9.0);

    // Mean = 5.0, variance = 4.0, stddev = 2.0
    EXPECT_NEAR(timer.mean(), 5.0, 0.01);
    EXPECT_NEAR(timer.stddev(), 2.0, 0.01);
}

TEST_F(TimerMetricsTest, Reset) {
    timer_data timer;
    timer.record(100.0);
    timer.record(200.0);

    EXPECT_EQ(timer.count(), 2);

    timer.reset();

    EXPECT_EQ(timer.count(), 0);
    EXPECT_DOUBLE_EQ(timer.mean(), 0.0);
    EXPECT_DOUBLE_EQ(timer.min(), 0.0);
    EXPECT_DOUBLE_EQ(timer.max(), 0.0);
}

TEST_F(TimerMetricsTest, Snapshot) {
    timer_data timer;
    for (int i = 1; i <= 100; ++i) {
        timer.record(static_cast<double>(i));
    }

    auto snap = timer.get_snapshot();

    EXPECT_EQ(snap.count, 100);
    EXPECT_DOUBLE_EQ(snap.mean, 50.5);
    EXPECT_DOUBLE_EQ(snap.min, 1.0);
    EXPECT_DOUBLE_EQ(snap.max, 100.0);
    EXPECT_NEAR(snap.p50, 50.5, 1.0);
    EXPECT_NEAR(snap.p99, 99.0, 1.0);
}

TEST_F(TimerMetricsTest, CustomReservoirSize) {
    timer_data timer(100);  // Small reservoir

    // Add more samples than reservoir size
    for (int i = 0; i < 1000; ++i) {
        timer.record(static_cast<double>(i));
    }

    EXPECT_EQ(timer.count(), 1000);
    EXPECT_LE(timer.samples.size(), 100);  // Should not exceed reservoir size
}

TEST_F(TimerMetricsTest, ChronoDurationRecording) {
    timer_data timer;

    auto duration = std::chrono::milliseconds(150);
    timer.record(duration);

    EXPECT_EQ(timer.count(), 1);
    EXPECT_NEAR(timer.mean(), 150.0, 0.01);
}

TEST_F(TimerMetricsTest, TimerScopeRecording) {
    timer_data timer;

    {
        timer_scope scope(timer);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_EQ(timer.count(), 1);
    EXPECT_GE(timer.mean(), 10.0);  // At least 10ms
}

TEST_F(TimerMetricsTest, P999Percentile) {
    timer_data timer;
    // Add 1000 samples
    for (int i = 1; i <= 1000; ++i) {
        timer.record(static_cast<double>(i));
    }

    // p999 should be close to 999
    EXPECT_NEAR(timer.p999(), 999.0, 2.0);
}

// Test histogram_data improvements
class HistogramMetricsTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(HistogramMetricsTest, StandardBuckets) {
    histogram_data hist;
    hist.init_standard_buckets();

    EXPECT_EQ(hist.buckets.size(), 15);
    EXPECT_DOUBLE_EQ(hist.buckets[0].upper_bound, 0.005);
}

TEST_F(HistogramMetricsTest, AddSample) {
    histogram_data hist;
    hist.init_standard_buckets();

    hist.add_sample(0.1);
    hist.add_sample(0.5);
    hist.add_sample(1.0);

    EXPECT_EQ(hist.total_count, 3);
    EXPECT_DOUBLE_EQ(hist.sum, 1.6);
    EXPECT_NEAR(hist.mean(), 0.533, 0.01);
}

// Test summary_data improvements
class SummaryMetricsTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(SummaryMetricsTest, BasicStatistics) {
    summary_data summary;

    summary.add_sample(10.0);
    summary.add_sample(20.0);
    summary.add_sample(30.0);

    EXPECT_EQ(summary.count, 3);
    EXPECT_DOUBLE_EQ(summary.sum, 60.0);
    EXPECT_DOUBLE_EQ(summary.mean(), 20.0);
    EXPECT_DOUBLE_EQ(summary.min_value, 10.0);
    EXPECT_DOUBLE_EQ(summary.max_value, 30.0);
}

TEST_F(SummaryMetricsTest, Reset) {
    summary_data summary;
    summary.add_sample(100.0);

    summary.reset();

    EXPECT_EQ(summary.count, 0);
    EXPECT_DOUBLE_EQ(summary.sum, 0.0);
    EXPECT_DOUBLE_EQ(summary.mean(), 0.0);
}
