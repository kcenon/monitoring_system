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

#include <gtest/gtest.h>
#include <kcenon/monitoring/collectors/platform_metrics_collector.h>

namespace kcenon {
namespace monitoring {
namespace {

class PlatformMetricsCollectorTest : public ::testing::Test {
   protected:
    void SetUp() override {
        collector_ = std::make_unique<platform_metrics_collector>();
    }

    std::unique_ptr<platform_metrics_collector> collector_;
};

// Test collector name
TEST_F(PlatformMetricsCollectorTest, CollectorNameIsCorrect) {
    EXPECT_EQ(collector_->name(), "platform_metrics_collector");
}

// Test platform availability
TEST_F(PlatformMetricsCollectorTest, PlatformIsAvailable) {
#if defined(__linux__) || defined(__APPLE__) || defined(_WIN32)
    EXPECT_TRUE(collector_->is_platform_available());
    EXPECT_TRUE(collector_->is_available());
#else
    // On unsupported platforms, should still return true (null_metrics_provider)
    // but platform name will be "unknown"
    EXPECT_TRUE(collector_->is_platform_available());
#endif
}

// Test platform name detection
TEST_F(PlatformMetricsCollectorTest, PlatformNameIsCorrect) {
    std::string platform_name = collector_->get_platform_name();

#if defined(__linux__)
    EXPECT_EQ(platform_name, "linux");
#elif defined(__APPLE__)
    EXPECT_EQ(platform_name, "macos");
#elif defined(_WIN32)
    EXPECT_EQ(platform_name, "windows");
#else
    EXPECT_EQ(platform_name, "unknown");
#endif
}

// Test initialization with default config
TEST_F(PlatformMetricsCollectorTest, InitializationWithDefaultConfig) {
    config_map config;
    EXPECT_TRUE(collector_->do_initialize(config));
}

// Test initialization with custom config
TEST_F(PlatformMetricsCollectorTest, InitializationWithCustomConfig) {
    config_map config;
    config["collect_uptime"] = "true";
    config["collect_context_switches"] = "false";
    config["collect_tcp_states"] = "true";
    config["collect_socket_buffers"] = "false";
    config["collect_interrupts"] = "false";

    EXPECT_TRUE(collector_->do_initialize(config));
}

// Test metric collection
TEST_F(PlatformMetricsCollectorTest, CollectReturnsMetrics) {
    if (!collector_->is_available()) {
        GTEST_SKIP() << "Platform collector not available on this platform";
    }

    auto metrics = collector_->collect();

    // Should at least have platform_info metric
    EXPECT_FALSE(metrics.empty());

    // Check for platform_info metric
    bool has_platform_info = false;
    for (const auto& m : metrics) {
        if (m.name == "platform_info") {
            has_platform_info = true;
            // Check that platform tag is present
            auto it = m.tags.find("platform");
            EXPECT_NE(it, m.tags.end());
            break;
        }
    }
    EXPECT_TRUE(has_platform_info);
}

// Test platform info retrieval
TEST_F(PlatformMetricsCollectorTest, GetPlatformInfoReturnsValidInfo) {
    auto info = collector_->get_platform_info();

#if defined(__linux__) || defined(__APPLE__) || defined(_WIN32)
    EXPECT_TRUE(info.available);
    EXPECT_FALSE(info.name.empty());
#else
    EXPECT_EQ(info.name, "unknown");
#endif
}

// Test metric types
TEST_F(PlatformMetricsCollectorTest, GetMetricTypesReturnsExpectedTypes) {
    auto types = collector_->do_get_metric_types();

    EXPECT_FALSE(types.empty());

    // Should include platform_info
    bool has_platform_info = false;
    for (const auto& type : types) {
        if (type == "platform_info") {
            has_platform_info = true;
            break;
        }
    }
    EXPECT_TRUE(has_platform_info);
}

// Test statistics
TEST_F(PlatformMetricsCollectorTest, GetStatisticsReturnsConfigValues) {
    auto stats = collector_->get_statistics();

    // Should have config-related statistics
    EXPECT_NE(stats.find("collect_uptime"), stats.end());
    EXPECT_NE(stats.find("collect_context_switches"), stats.end());
    EXPECT_NE(stats.find("collect_tcp_states"), stats.end());
    EXPECT_NE(stats.find("collect_socket_buffers"), stats.end());
    EXPECT_NE(stats.find("collect_interrupts"), stats.end());

    // Default config should have all enabled
    EXPECT_EQ(stats["collect_uptime"], 1.0);
    EXPECT_EQ(stats["collect_context_switches"], 1.0);
}

// Test last metrics caching
TEST_F(PlatformMetricsCollectorTest, GetLastMetricsReturnsCachedData) {
    if (!collector_->is_available()) {
        GTEST_SKIP() << "Platform collector not available on this platform";
    }

    // First collection
    collector_->collect();

    // Get last metrics
    auto last_metrics = collector_->get_last_metrics();

    // Timestamp should be set
    auto epoch = std::chrono::system_clock::time_point{};
    EXPECT_GT(last_metrics.timestamp, epoch);
}

// Test multiple collections
TEST_F(PlatformMetricsCollectorTest, MultipleCollectionsAreConsistent) {
    if (!collector_->is_available()) {
        GTEST_SKIP() << "Platform collector not available on this platform";
    }

    auto metrics1 = collector_->collect();
    auto metrics2 = collector_->collect();

    // Both collections should have metrics
    EXPECT_FALSE(metrics1.empty());
    EXPECT_FALSE(metrics2.empty());

    // Platform name should be the same (it's cached)
    std::string platform1, platform2;
    for (const auto& m : metrics1) {
        if (m.name == "platform_info") {
            auto it = m.tags.find("platform");
            if (it != m.tags.end()) {
                platform1 = it->second;
            }
            break;
        }
    }
    for (const auto& m : metrics2) {
        if (m.name == "platform_info") {
            auto it = m.tags.find("platform");
            if (it != m.tags.end()) {
                platform2 = it->second;
            }
            break;
        }
    }
    EXPECT_EQ(platform1, platform2);
}

// Test collector with disabled uptime collection
TEST_F(PlatformMetricsCollectorTest, DisabledUptimeCollectionExcludesUptimeMetrics) {
    platform_metrics_config config;
    config.collect_uptime = false;

    auto collector = std::make_unique<platform_metrics_collector>(config);

    if (!collector->is_available()) {
        GTEST_SKIP() << "Platform collector not available on this platform";
    }

    auto metrics = collector->collect();

    // Should not have uptime metrics
    for (const auto& m : metrics) {
        EXPECT_NE(m.name, "platform_uptime_seconds");
        EXPECT_NE(m.name, "platform_boot_timestamp");
    }
}

// Test health check
TEST_F(PlatformMetricsCollectorTest, HealthCheckReturnsCorrectly) {
    EXPECT_TRUE(collector_->is_healthy());
}

// Test collection count tracking
TEST_F(PlatformMetricsCollectorTest, CollectionCountIncrementsCorrectly) {
    if (!collector_->is_available()) {
        GTEST_SKIP() << "Platform collector not available on this platform";
    }

    EXPECT_EQ(collector_->get_collection_count(), 0u);

    collector_->collect();
    EXPECT_EQ(collector_->get_collection_count(), 1u);

    collector_->collect();
    EXPECT_EQ(collector_->get_collection_count(), 2u);
}

// Test info collector standalone
TEST(PlatformInfoCollectorTest, CreateSucceeds) {
    auto info_collector = std::make_unique<platform_info_collector>();
    EXPECT_NE(info_collector, nullptr);
}

TEST(PlatformInfoCollectorTest, PlatformAvailableOnSupportedPlatforms) {
    auto info_collector = std::make_unique<platform_info_collector>();

#if defined(__linux__) || defined(__APPLE__) || defined(_WIN32)
    EXPECT_TRUE(info_collector->is_platform_available());
#else
    // Null provider is still "available" (returns valid null object)
    EXPECT_TRUE(info_collector->is_platform_available());
#endif
}

TEST(PlatformInfoCollectorTest, GetPlatformInfoReturnsValidData) {
    auto info_collector = std::make_unique<platform_info_collector>();
    auto info = info_collector->get_platform_info();

#if defined(__linux__) || defined(__APPLE__) || defined(_WIN32)
    EXPECT_TRUE(info.available);
#endif
    EXPECT_FALSE(info.name.empty());
}

TEST(PlatformInfoCollectorTest, GetUptimeReturnsData) {
    auto info_collector = std::make_unique<platform_info_collector>();
    auto uptime = info_collector->get_uptime();

#if defined(__linux__) || defined(__APPLE__) || defined(_WIN32)
    if (uptime.available) {
        EXPECT_GT(uptime.uptime_seconds, 0);
    }
#endif
}

}  // namespace
}  // namespace monitoring
}  // namespace kcenon
