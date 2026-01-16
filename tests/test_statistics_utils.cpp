// BSD 3-Clause License
//
// Copyright (c) 2021-2025, monitoring_system contributors
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
 * @file test_statistics_utils.cpp
 * @brief Unit tests for statistics utilities
 */

#include <gtest/gtest.h>
#include <kcenon/monitoring/utils/statistics.h>

#include <chrono>
#include <vector>

using namespace kcenon::monitoring::stats;

class StatisticsUtilsTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// =========================================================================
// Percentile Tests with Double Values
// =========================================================================

TEST_F(StatisticsUtilsTest, PercentileEmptyVector) {
    std::vector<double> empty;
    EXPECT_DOUBLE_EQ(percentile(empty, 50.0), 0.0);
}

TEST_F(StatisticsUtilsTest, PercentileSingleValue) {
    std::vector<double> single = {42.0};
    EXPECT_DOUBLE_EQ(percentile(single, 0.0), 42.0);
    EXPECT_DOUBLE_EQ(percentile(single, 50.0), 42.0);
    EXPECT_DOUBLE_EQ(percentile(single, 100.0), 42.0);
}

TEST_F(StatisticsUtilsTest, PercentileTwoValues) {
    std::vector<double> values = {10.0, 20.0};
    EXPECT_DOUBLE_EQ(percentile(values, 0.0), 10.0);
    EXPECT_DOUBLE_EQ(percentile(values, 50.0), 15.0);
    EXPECT_DOUBLE_EQ(percentile(values, 100.0), 20.0);
}

TEST_F(StatisticsUtilsTest, PercentileFiveValues) {
    std::vector<double> values = {1.0, 2.0, 3.0, 4.0, 5.0};
    EXPECT_DOUBLE_EQ(percentile(values, 0.0), 1.0);
    EXPECT_DOUBLE_EQ(percentile(values, 25.0), 2.0);
    EXPECT_DOUBLE_EQ(percentile(values, 50.0), 3.0);
    EXPECT_DOUBLE_EQ(percentile(values, 75.0), 4.0);
    EXPECT_DOUBLE_EQ(percentile(values, 100.0), 5.0);
}

TEST_F(StatisticsUtilsTest, PercentileP95P99) {
    std::vector<double> values;
    for (int i = 1; i <= 100; ++i) {
        values.push_back(static_cast<double>(i));
    }

    double p95 = percentile(values, 95.0);
    double p99 = percentile(values, 99.0);

    EXPECT_GE(p95, 94.0);
    EXPECT_LE(p95, 96.0);
    EXPECT_GE(p99, 98.0);
    EXPECT_LE(p99, 100.0);
}

TEST_F(StatisticsUtilsTest, PercentileBoundaryValues) {
    std::vector<double> values = {1.0, 2.0, 3.0, 4.0, 5.0};
    EXPECT_DOUBLE_EQ(percentile(values, -10.0), 1.0);
    EXPECT_DOUBLE_EQ(percentile(values, 110.0), 5.0);
}

// =========================================================================
// Percentile Tests with Chrono Duration
// =========================================================================

TEST_F(StatisticsUtilsTest, PercentileChronoEmpty) {
    std::vector<std::chrono::nanoseconds> empty;
    EXPECT_EQ(percentile(empty, 50.0), std::chrono::nanoseconds::zero());
}

TEST_F(StatisticsUtilsTest, PercentileChronoValues) {
    std::vector<std::chrono::nanoseconds> values = {
        std::chrono::nanoseconds(100),
        std::chrono::nanoseconds(200),
        std::chrono::nanoseconds(300),
        std::chrono::nanoseconds(400),
        std::chrono::nanoseconds(500)
    };

    auto p0 = percentile(values, 0.0);
    auto p50 = percentile(values, 50.0);
    auto p100 = percentile(values, 100.0);

    EXPECT_EQ(p0, std::chrono::nanoseconds(100));
    EXPECT_EQ(p50, std::chrono::nanoseconds(300));
    EXPECT_EQ(p100, std::chrono::nanoseconds(500));
}

// =========================================================================
// Compute Statistics Tests with Double Values
// =========================================================================

TEST_F(StatisticsUtilsTest, ComputeEmptyVector) {
    std::vector<double> empty;
    auto stats = compute(empty);

    EXPECT_EQ(stats.count, 0);
    EXPECT_DOUBLE_EQ(stats.min, 0.0);
    EXPECT_DOUBLE_EQ(stats.max, 0.0);
    EXPECT_DOUBLE_EQ(stats.mean, 0.0);
    EXPECT_DOUBLE_EQ(stats.total, 0.0);
}

TEST_F(StatisticsUtilsTest, ComputeSingleValue) {
    std::vector<double> single = {42.0};
    auto stats = compute(single);

    EXPECT_EQ(stats.count, 1);
    EXPECT_DOUBLE_EQ(stats.min, 42.0);
    EXPECT_DOUBLE_EQ(stats.max, 42.0);
    EXPECT_DOUBLE_EQ(stats.mean, 42.0);
    EXPECT_DOUBLE_EQ(stats.median, 42.0);
    EXPECT_DOUBLE_EQ(stats.total, 42.0);
}

TEST_F(StatisticsUtilsTest, ComputeFiveValues) {
    std::vector<double> values = {1.0, 2.0, 3.0, 4.0, 5.0};
    auto stats = compute(values);

    EXPECT_EQ(stats.count, 5);
    EXPECT_DOUBLE_EQ(stats.min, 1.0);
    EXPECT_DOUBLE_EQ(stats.max, 5.0);
    EXPECT_DOUBLE_EQ(stats.mean, 3.0);
    EXPECT_DOUBLE_EQ(stats.median, 3.0);
    EXPECT_DOUBLE_EQ(stats.total, 15.0);
}

TEST_F(StatisticsUtilsTest, ComputeUnsortedValues) {
    std::vector<double> values = {5.0, 1.0, 3.0, 2.0, 4.0};
    auto stats = compute(values);

    EXPECT_EQ(stats.count, 5);
    EXPECT_DOUBLE_EQ(stats.min, 1.0);
    EXPECT_DOUBLE_EQ(stats.max, 5.0);
    EXPECT_DOUBLE_EQ(stats.mean, 3.0);
    EXPECT_DOUBLE_EQ(stats.median, 3.0);
}

TEST_F(StatisticsUtilsTest, ComputeP95P99) {
    std::vector<double> values;
    for (int i = 1; i <= 100; ++i) {
        values.push_back(static_cast<double>(i));
    }
    auto stats = compute(values);

    EXPECT_EQ(stats.count, 100);
    EXPECT_DOUBLE_EQ(stats.min, 1.0);
    EXPECT_DOUBLE_EQ(stats.max, 100.0);
    EXPECT_DOUBLE_EQ(stats.mean, 50.5);

    EXPECT_GE(stats.p95, 94.0);
    EXPECT_LE(stats.p95, 96.0);
    EXPECT_GE(stats.p99, 98.0);
    EXPECT_LE(stats.p99, 100.0);
}

// =========================================================================
// Compute Statistics Tests with Chrono Duration
// =========================================================================

TEST_F(StatisticsUtilsTest, ComputeChronoEmpty) {
    std::vector<std::chrono::nanoseconds> empty;
    auto stats = compute(empty);

    EXPECT_EQ(stats.count, 0);
    EXPECT_EQ(stats.min, std::chrono::nanoseconds::zero());
    EXPECT_EQ(stats.max, std::chrono::nanoseconds::zero());
    EXPECT_EQ(stats.mean, std::chrono::nanoseconds::zero());
}

TEST_F(StatisticsUtilsTest, ComputeChronoValues) {
    std::vector<std::chrono::nanoseconds> values = {
        std::chrono::nanoseconds(1000000),
        std::chrono::nanoseconds(2000000),
        std::chrono::nanoseconds(3000000),
        std::chrono::nanoseconds(4000000),
        std::chrono::nanoseconds(5000000)
    };
    auto stats = compute(values);

    EXPECT_EQ(stats.count, 5);
    EXPECT_EQ(stats.min, std::chrono::nanoseconds(1000000));
    EXPECT_EQ(stats.max, std::chrono::nanoseconds(5000000));
    EXPECT_EQ(stats.mean, std::chrono::nanoseconds(3000000));
    EXPECT_EQ(stats.median, std::chrono::nanoseconds(3000000));
    EXPECT_EQ(stats.total, std::chrono::nanoseconds(15000000));
}

TEST_F(StatisticsUtilsTest, ComputeChronoPercentiles) {
    std::vector<std::chrono::nanoseconds> values;
    for (int i = 1; i <= 100; ++i) {
        values.push_back(std::chrono::nanoseconds(i * 1000000));
    }
    auto stats = compute(values);

    EXPECT_EQ(stats.count, 100);
    EXPECT_EQ(stats.min.count(), 1000000);
    EXPECT_EQ(stats.max.count(), 100000000);

    EXPECT_GE(stats.p95.count(), 94000000);
    EXPECT_LE(stats.p95.count(), 96000000);
    EXPECT_GE(stats.p99.count(), 98000000);
    EXPECT_LE(stats.p99.count(), 100000000);
}

// =========================================================================
// Compute Sorted and Inplace Tests
// =========================================================================

TEST_F(StatisticsUtilsTest, ComputeSortedValues) {
    std::vector<double> sorted = {1.0, 2.0, 3.0, 4.0, 5.0};
    auto stats = compute_sorted(sorted);

    EXPECT_EQ(stats.count, 5);
    EXPECT_DOUBLE_EQ(stats.min, 1.0);
    EXPECT_DOUBLE_EQ(stats.max, 5.0);
    EXPECT_DOUBLE_EQ(stats.mean, 3.0);
}

TEST_F(StatisticsUtilsTest, ComputeInplaceModifiesInput) {
    std::vector<double> values = {5.0, 1.0, 3.0, 2.0, 4.0};
    auto stats = compute_inplace(values);

    EXPECT_EQ(stats.count, 5);
    EXPECT_DOUBLE_EQ(stats.min, 1.0);
    EXPECT_DOUBLE_EQ(stats.max, 5.0);

    EXPECT_DOUBLE_EQ(values[0], 1.0);
    EXPECT_DOUBLE_EQ(values[4], 5.0);
}

// =========================================================================
// Edge Cases
// =========================================================================

TEST_F(StatisticsUtilsTest, ComputeNegativeValues) {
    std::vector<double> values = {-5.0, -3.0, -1.0, 1.0, 3.0, 5.0};
    auto stats = compute(values);

    EXPECT_EQ(stats.count, 6);
    EXPECT_DOUBLE_EQ(stats.min, -5.0);
    EXPECT_DOUBLE_EQ(stats.max, 5.0);
    EXPECT_DOUBLE_EQ(stats.mean, 0.0);
    EXPECT_DOUBLE_EQ(stats.total, 0.0);
}

TEST_F(StatisticsUtilsTest, ComputeAllSameValues) {
    std::vector<double> values = {42.0, 42.0, 42.0, 42.0, 42.0};
    auto stats = compute(values);

    EXPECT_EQ(stats.count, 5);
    EXPECT_DOUBLE_EQ(stats.min, 42.0);
    EXPECT_DOUBLE_EQ(stats.max, 42.0);
    EXPECT_DOUBLE_EQ(stats.mean, 42.0);
    EXPECT_DOUBLE_EQ(stats.median, 42.0);
    EXPECT_DOUBLE_EQ(stats.p95, 42.0);
    EXPECT_DOUBLE_EQ(stats.p99, 42.0);
}

TEST_F(StatisticsUtilsTest, ComputeLargeValues) {
    std::vector<double> values;
    for (int i = 0; i < 10000; ++i) {
        values.push_back(static_cast<double>(i));
    }
    auto stats = compute(values);

    EXPECT_EQ(stats.count, 10000);
    EXPECT_DOUBLE_EQ(stats.min, 0.0);
    EXPECT_DOUBLE_EQ(stats.max, 9999.0);
    EXPECT_DOUBLE_EQ(stats.mean, 4999.5);
}
