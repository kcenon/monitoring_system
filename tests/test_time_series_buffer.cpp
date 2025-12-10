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

#include <gtest/gtest.h>
#include <kcenon/monitoring/utils/time_series_buffer.h>

#include <chrono>
#include <cmath>
#include <thread>

namespace kcenon {
namespace monitoring {
namespace {

class TimeSeriesBufferTest : public ::testing::Test {
  protected:
    void SetUp() override {
        time_series_buffer_config config;
        config.max_samples = 100;
        buffer_ = std::make_unique<time_series_buffer<double>>(config);
    }

    std::unique_ptr<time_series_buffer<double>> buffer_;
};

TEST_F(TimeSeriesBufferTest, InitializesCorrectly) {
    EXPECT_EQ(buffer_->capacity(), 100);
    EXPECT_EQ(buffer_->size(), 0);
    EXPECT_TRUE(buffer_->empty());
}

TEST_F(TimeSeriesBufferTest, AddSampleIncreasesSize) {
    buffer_->add_sample(1.0);
    EXPECT_EQ(buffer_->size(), 1);
    EXPECT_FALSE(buffer_->empty());

    buffer_->add_sample(2.0);
    EXPECT_EQ(buffer_->size(), 2);
}

TEST_F(TimeSeriesBufferTest, GetLatestReturnsLastSample) {
    buffer_->add_sample(1.0);
    buffer_->add_sample(2.0);
    buffer_->add_sample(3.0);

    auto result = buffer_->get_latest();
    EXPECT_TRUE(result.is_ok());
    EXPECT_DOUBLE_EQ(result.value(), 3.0);
}

TEST_F(TimeSeriesBufferTest, GetLatestOnEmptyBufferReturnsError) {
    auto result = buffer_->get_latest();
    EXPECT_TRUE(result.is_err());
}

TEST_F(TimeSeriesBufferTest, RingBufferBehavior) {
    time_series_buffer_config config;
    config.max_samples = 5;
    auto small_buffer = std::make_unique<time_series_buffer<double>>(config);

    for (int i = 1; i <= 10; ++i) {
        small_buffer->add_sample(static_cast<double>(i));
    }

    EXPECT_EQ(small_buffer->size(), 5);

    auto samples = small_buffer->get_all_samples();
    EXPECT_EQ(samples.size(), 5);

    // Last 5 values should be 6, 7, 8, 9, 10
    std::vector<double> expected = {6.0, 7.0, 8.0, 9.0, 10.0};
    for (size_t i = 0; i < samples.size(); ++i) {
        EXPECT_DOUBLE_EQ(samples[i].value, expected[i]);
    }
}

TEST_F(TimeSeriesBufferTest, StatisticsCalculation) {
    for (int i = 1; i <= 10; ++i) {
        buffer_->add_sample(static_cast<double>(i));
    }

    auto stats = buffer_->get_statistics();

    EXPECT_EQ(stats.sample_count, 10);
    EXPECT_DOUBLE_EQ(stats.min_value, 1.0);
    EXPECT_DOUBLE_EQ(stats.max_value, 10.0);
    EXPECT_DOUBLE_EQ(stats.avg, 5.5);
    EXPECT_GT(stats.stddev, 0.0);
    EXPECT_GT(stats.p95, stats.avg);
    EXPECT_GT(stats.p99, stats.p95);
}

TEST_F(TimeSeriesBufferTest, GetSamplesWithDuration) {
    auto now = std::chrono::system_clock::now();

    buffer_->add_sample(1.0, now - std::chrono::minutes(10));
    buffer_->add_sample(2.0, now - std::chrono::minutes(5));
    buffer_->add_sample(3.0, now - std::chrono::minutes(1));

    auto samples = buffer_->get_samples(std::chrono::minutes(3));
    EXPECT_EQ(samples.size(), 1);
    EXPECT_DOUBLE_EQ(samples[0].value, 3.0);

    samples = buffer_->get_samples(std::chrono::minutes(7));
    EXPECT_EQ(samples.size(), 2);
}

TEST_F(TimeSeriesBufferTest, Clear) {
    buffer_->add_sample(1.0);
    buffer_->add_sample(2.0);
    EXPECT_EQ(buffer_->size(), 2);

    buffer_->clear();
    EXPECT_EQ(buffer_->size(), 0);
    EXPECT_TRUE(buffer_->empty());
}

TEST_F(TimeSeriesBufferTest, MemoryFootprint) {
    size_t footprint = buffer_->memory_footprint();
    EXPECT_GT(footprint, 0);
}

TEST_F(TimeSeriesBufferTest, InvalidConfigThrows) {
    time_series_buffer_config config;
    config.max_samples = 0;

    EXPECT_THROW(
        {
            time_series_buffer<double> buf(config);
        },
        std::invalid_argument);
}

class LoadAverageHistoryTest : public ::testing::Test {
  protected:
    void SetUp() override { history_ = std::make_unique<load_average_history>(100); }

    std::unique_ptr<load_average_history> history_;
};

TEST_F(LoadAverageHistoryTest, InitializesCorrectly) {
    EXPECT_EQ(history_->capacity(), 100);
    EXPECT_EQ(history_->size(), 0);
    EXPECT_TRUE(history_->empty());
}

TEST_F(LoadAverageHistoryTest, AddSampleIncreasesSize) {
    history_->add_sample(1.0, 0.5, 0.3);
    EXPECT_EQ(history_->size(), 1);
    EXPECT_FALSE(history_->empty());
}

TEST_F(LoadAverageHistoryTest, GetLatestReturnsLastSample) {
    history_->add_sample(1.0, 0.5, 0.3);
    history_->add_sample(2.0, 1.5, 1.0);

    auto result = history_->get_latest();
    EXPECT_TRUE(result.is_ok());
    EXPECT_DOUBLE_EQ(result.value().load_1m, 2.0);
    EXPECT_DOUBLE_EQ(result.value().load_5m, 1.5);
    EXPECT_DOUBLE_EQ(result.value().load_15m, 1.0);
}

TEST_F(LoadAverageHistoryTest, GetLatestOnEmptyHistoryReturnsError) {
    auto result = history_->get_latest();
    EXPECT_TRUE(result.is_err());
}

TEST_F(LoadAverageHistoryTest, RingBufferBehavior) {
    auto small_history = std::make_unique<load_average_history>(5);

    for (int i = 1; i <= 10; ++i) {
        small_history->add_sample(static_cast<double>(i), static_cast<double>(i) / 2,
                                  static_cast<double>(i) / 4);
    }

    EXPECT_EQ(small_history->size(), 5);

    auto samples = small_history->get_all_samples();
    EXPECT_EQ(samples.size(), 5);

    // Last 5 values should be 6, 7, 8, 9, 10
    for (size_t i = 0; i < samples.size(); ++i) {
        EXPECT_DOUBLE_EQ(samples[i].load_1m, 6.0 + i);
    }
}

TEST_F(LoadAverageHistoryTest, StatisticsCalculation) {
    for (int i = 1; i <= 10; ++i) {
        history_->add_sample(static_cast<double>(i), static_cast<double>(i) / 2,
                             static_cast<double>(i) / 4);
    }

    auto stats = history_->get_statistics();

    EXPECT_EQ(stats.load_1m_stats.sample_count, 10);
    EXPECT_DOUBLE_EQ(stats.load_1m_stats.min_value, 1.0);
    EXPECT_DOUBLE_EQ(stats.load_1m_stats.max_value, 10.0);
    EXPECT_DOUBLE_EQ(stats.load_1m_stats.avg, 5.5);

    EXPECT_EQ(stats.load_5m_stats.sample_count, 10);
    EXPECT_DOUBLE_EQ(stats.load_5m_stats.min_value, 0.5);
    EXPECT_DOUBLE_EQ(stats.load_5m_stats.max_value, 5.0);

    EXPECT_EQ(stats.load_15m_stats.sample_count, 10);
    EXPECT_DOUBLE_EQ(stats.load_15m_stats.min_value, 0.25);
    EXPECT_DOUBLE_EQ(stats.load_15m_stats.max_value, 2.5);
}

TEST_F(LoadAverageHistoryTest, GetSamplesWithDuration) {
    auto now = std::chrono::system_clock::now();

    history_->add_sample(1.0, 0.5, 0.3, now - std::chrono::minutes(10));
    history_->add_sample(2.0, 1.0, 0.5, now - std::chrono::minutes(5));
    history_->add_sample(3.0, 1.5, 0.7, now - std::chrono::minutes(1));

    auto samples = history_->get_samples(std::chrono::minutes(3));
    EXPECT_EQ(samples.size(), 1);
    EXPECT_DOUBLE_EQ(samples[0].load_1m, 3.0);

    samples = history_->get_samples(std::chrono::minutes(7));
    EXPECT_EQ(samples.size(), 2);
}

TEST_F(LoadAverageHistoryTest, Clear) {
    history_->add_sample(1.0, 0.5, 0.3);
    history_->add_sample(2.0, 1.0, 0.5);
    EXPECT_EQ(history_->size(), 2);

    history_->clear();
    EXPECT_EQ(history_->size(), 0);
    EXPECT_TRUE(history_->empty());
}

TEST_F(LoadAverageHistoryTest, MemoryFootprint) {
    size_t footprint = history_->memory_footprint();
    EXPECT_GT(footprint, 0);
}

TEST_F(LoadAverageHistoryTest, InvalidMaxSamplesThrows) {
    EXPECT_THROW(load_average_history(0), std::invalid_argument);
}

TEST(TimeSeriesStatisticsTest, PercentileCalculation) {
    time_series_buffer_config config;
    config.max_samples = 1000;
    time_series_buffer<double> buffer(config);

    for (int i = 1; i <= 100; ++i) {
        buffer.add_sample(static_cast<double>(i));
    }

    auto stats = buffer.get_statistics();

    EXPECT_NEAR(stats.p95, 95.0, 1.0);
    EXPECT_NEAR(stats.p99, 99.0, 1.0);
}

TEST(TimeSeriesStatisticsTest, StandardDeviationCalculation) {
    time_series_buffer_config config;
    config.max_samples = 100;
    time_series_buffer<double> buffer(config);

    buffer.add_sample(2.0);
    buffer.add_sample(4.0);
    buffer.add_sample(4.0);
    buffer.add_sample(4.0);
    buffer.add_sample(5.0);
    buffer.add_sample(5.0);
    buffer.add_sample(7.0);
    buffer.add_sample(9.0);

    auto stats = buffer.get_statistics();

    EXPECT_DOUBLE_EQ(stats.avg, 5.0);
    EXPECT_NEAR(stats.stddev, 2.0, 0.01);
}

TEST(TimeSeriesStatisticsTest, EmptyBufferStatistics) {
    time_series_buffer_config config;
    config.max_samples = 100;
    time_series_buffer<double> buffer(config);

    auto stats = buffer.get_statistics();

    EXPECT_EQ(stats.sample_count, 0);
    EXPECT_DOUBLE_EQ(stats.min_value, 0.0);
    EXPECT_DOUBLE_EQ(stats.max_value, 0.0);
    EXPECT_DOUBLE_EQ(stats.avg, 0.0);
}

TEST(TimeSeriesStatisticsTest, SingleSampleStatistics) {
    time_series_buffer_config config;
    config.max_samples = 100;
    time_series_buffer<double> buffer(config);

    buffer.add_sample(42.0);

    auto stats = buffer.get_statistics();

    EXPECT_EQ(stats.sample_count, 1);
    EXPECT_DOUBLE_EQ(stats.min_value, 42.0);
    EXPECT_DOUBLE_EQ(stats.max_value, 42.0);
    EXPECT_DOUBLE_EQ(stats.avg, 42.0);
    EXPECT_DOUBLE_EQ(stats.stddev, 0.0);
    EXPECT_DOUBLE_EQ(stats.p95, 42.0);
    EXPECT_DOUBLE_EQ(stats.p99, 42.0);
}

TEST(TimeSeriesBufferThreadSafety, ConcurrentReadWrite) {
    time_series_buffer_config config;
    config.max_samples = 1000;
    auto buffer = std::make_shared<time_series_buffer<double>>(config);

    std::atomic<bool> stop{false};
    std::atomic<size_t> write_count{0};
    std::atomic<size_t> read_count{0};

    auto writer = [&]() {
        while (!stop) {
            buffer->add_sample(static_cast<double>(write_count++));
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    };

    auto reader = [&]() {
        while (!stop) {
            auto samples = buffer->get_all_samples();
            read_count++;
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    };

    std::thread writer_thread(writer);
    std::thread reader_thread(reader);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stop = true;

    writer_thread.join();
    reader_thread.join();

    EXPECT_GT(write_count.load(), 0);
    EXPECT_GT(read_count.load(), 0);
}

TEST(LoadAverageHistoryThreadSafety, ConcurrentReadWrite) {
    auto history = std::make_shared<load_average_history>(1000);

    std::atomic<bool> stop{false};
    std::atomic<size_t> write_count{0};
    std::atomic<size_t> read_count{0};

    auto writer = [&]() {
        while (!stop) {
            double val = static_cast<double>(write_count++);
            history->add_sample(val, val / 2, val / 4);
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    };

    auto reader = [&]() {
        while (!stop) {
            auto samples = history->get_all_samples();
            read_count++;
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    };

    std::thread writer_thread(writer);
    std::thread reader_thread(reader);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stop = true;

    writer_thread.join();
    reader_thread.join();

    EXPECT_GT(write_count.load(), 0);
    EXPECT_GT(read_count.load(), 0);
}

}  // namespace
}  // namespace monitoring
}  // namespace kcenon
