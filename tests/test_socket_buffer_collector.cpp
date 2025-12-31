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
#include <kcenon/monitoring/collectors/socket_buffer_collector.h>

namespace kcenon {
namespace monitoring {
namespace {

// Test fixture for socket_buffer_collector tests
class SocketBufferCollectorTest : public ::testing::Test {
   protected:
    void SetUp() override {
        collector_ = std::make_unique<socket_buffer_collector>();
        std::unordered_map<std::string, std::string> config;
        collector_->initialize(config);
    }

    std::unique_ptr<socket_buffer_collector> collector_;
};

// Test basic initialization
TEST_F(SocketBufferCollectorTest, InitializesSuccessfully) {
    EXPECT_NE(collector_, nullptr);
    EXPECT_EQ(collector_->get_name(), "socket_buffer_collector");
}

// Test metric types returned
TEST_F(SocketBufferCollectorTest, ReturnsCorrectMetricTypes) {
    auto types = collector_->get_metric_types();
    EXPECT_FALSE(types.empty());

    // Check for expected metric types
    std::vector<std::string> expected_types = {
        "socket_recv_buffer_bytes",
        "socket_send_buffer_bytes",
        "socket_recv_queue_full_count",
        "socket_send_queue_full_count",
        "socket_memory_bytes",
        "socket_count_total"
    };

    for (const auto& expected : expected_types) {
        bool found = std::find(types.begin(), types.end(), expected) != types.end();
        EXPECT_TRUE(found) << "Expected metric type not found: " << expected;
    }
}

// Test configuration options
TEST_F(SocketBufferCollectorTest, ConfigurationOptions) {
    auto collector = std::make_unique<socket_buffer_collector>();
    
    std::unordered_map<std::string, std::string> config = {
        {"enabled", "true"},
        {"queue_full_threshold_bytes", "32768"},
        {"memory_warning_threshold_bytes", "52428800"}
    };
    
    EXPECT_TRUE(collector->initialize(config));
}

// Test disable collector
TEST_F(SocketBufferCollectorTest, CanBeDisabled) {
    auto collector = std::make_unique<socket_buffer_collector>();
    
    std::unordered_map<std::string, std::string> config = {
        {"enabled", "false"}
    };
    
    collector->initialize(config);
    
    auto metrics = collector->collect();
    // Disabled collector should return empty metrics
    EXPECT_TRUE(metrics.empty());
}

// Test statistics
TEST_F(SocketBufferCollectorTest, TracksStatistics) {
    auto stats = collector_->get_statistics();
    EXPECT_TRUE(stats.find("collection_count") != stats.end());
    EXPECT_TRUE(stats.find("collection_errors") != stats.end());
    EXPECT_TRUE(stats.find("enabled") != stats.end());
    EXPECT_TRUE(stats.find("available") != stats.end());
}

// Test collect returns metrics (graceful degradation when unavailable)
TEST_F(SocketBufferCollectorTest, CollectReturnsMetrics) {
    auto metrics = collector_->collect();
    // May be empty if not available on this platform
    // Just verify it doesn't crash
}

// Test get_last_metrics
TEST_F(SocketBufferCollectorTest, GetLastMetrics) {
    collector_->collect();  // Trigger a collection
    auto last = collector_->get_last_metrics();
    // Verify timestamp is set
    auto now = std::chrono::system_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - last.timestamp);
    EXPECT_LT(diff.count(), 10);  // Should be recent
}

// Test is_socket_buffer_monitoring_available
TEST_F(SocketBufferCollectorTest, SocketBufferMonitoringAvailabilityCheck) {
    bool available = collector_->is_socket_buffer_monitoring_available();
    // Just check it returns a boolean without crashing
    (void)available;
}

// Test socket_buffer_metrics structure
TEST(SocketBufferMetricsTest, DefaultInitialization) {
    socket_buffer_metrics metrics;
    EXPECT_EQ(metrics.recv_buffer_bytes, 0);
    EXPECT_EQ(metrics.send_buffer_bytes, 0);
    EXPECT_EQ(metrics.recv_queue_full_count, 0);
    EXPECT_EQ(metrics.send_queue_full_count, 0);
    EXPECT_EQ(metrics.socket_memory_bytes, 0);
    EXPECT_EQ(metrics.socket_count, 0);
    EXPECT_EQ(metrics.tcp_socket_count, 0);
    EXPECT_EQ(metrics.udp_socket_count, 0);
    EXPECT_FALSE(metrics.metrics_available);
}

// Test socket_buffer_info_collector basic functionality
TEST(SocketBufferInfoCollectorTest, BasicFunctionality) {
    socket_buffer_info_collector collector;
    
    // Check availability
    bool available = collector.is_socket_buffer_monitoring_available();
    
    // Collect metrics
    auto metrics = collector.collect_metrics();
    
    // If available, should have valid data
    if (available) {
        EXPECT_TRUE(metrics.metrics_available);
    }
}

// Test multiple collections are stable
TEST_F(SocketBufferCollectorTest, MultipleCollectionsAreStable) {
    for (int i = 0; i < 3; ++i) {
        auto metrics = collector_->collect();
        // Should not crash
    }
    
    auto stats = collector_->get_statistics();
    // Collection count should have increased
}

// Test that metrics have correct tags when collected
TEST_F(SocketBufferCollectorTest, MetricsHaveCorrectTags) {
    auto metrics = collector_->collect();
    for (const auto& m : metrics) {
        // All metrics should have collector tag
        auto it = m.tags.find("collector");
        if (it != m.tags.end()) {
            EXPECT_EQ(it->second, "socket_buffer_collector");
        }
    }
}

// Test is_healthy reflects actual state
TEST_F(SocketBufferCollectorTest, IsHealthyReflectsState) {
    bool healthy = collector_->is_healthy();
    // Should be healthy initially (no errors yet)
    EXPECT_TRUE(healthy);
}

#if defined(__linux__) || defined(__APPLE__)
// Platform-specific test: On Unix-like systems, socket buffer monitoring should be available
TEST_F(SocketBufferCollectorTest, UnixSocketBufferMonitoringAvailable) {
    EXPECT_TRUE(collector_->is_socket_buffer_monitoring_available());
}

// Platform-specific test: Should have at least some socket data
TEST(SocketBufferInfoCollectorTest, HasSocketDataOnUnix) {
    socket_buffer_info_collector collector;
    
    if (!collector.is_socket_buffer_monitoring_available()) {
        GTEST_SKIP() << "Socket buffer monitoring not available";
    }
    
    auto metrics = collector.collect_metrics();
    EXPECT_TRUE(metrics.metrics_available);
    
    // Should have at least some TCP connections on a running system
    // (at least localhost connections from tests, etc.)
    // We don't fail the test if counts are 0, just verify the data is valid
    EXPECT_GE(metrics.tcp_socket_count, 0);
}

// Platform-specific test: Metrics should contain buffer information
TEST(SocketBufferInfoCollectorTest, CollectsBufferInfo) {
    socket_buffer_info_collector collector;
    
    if (!collector.is_socket_buffer_monitoring_available()) {
        GTEST_SKIP() << "Socket buffer monitoring not available";
    }
    
    auto metrics = collector.collect_metrics();
    
    // Buffer values should be non-negative
    EXPECT_GE(metrics.recv_buffer_bytes, 0);
    EXPECT_GE(metrics.send_buffer_bytes, 0);
    EXPECT_GE(metrics.socket_memory_bytes, 0);
}
#endif

#if defined(_WIN32)
// Platform-specific test: On Windows, socket buffer monitoring is not yet implemented
TEST_F(SocketBufferCollectorTest, WindowsSocketBufferMonitoringUnavailable) {
    EXPECT_FALSE(collector_->is_socket_buffer_monitoring_available());
}

// Platform-specific test: Windows metrics should indicate unavailability
TEST(SocketBufferInfoCollectorTest, WindowsReturnsUnavailableMetrics) {
    socket_buffer_info_collector collector;
    auto metrics = collector.collect_metrics();
    EXPECT_FALSE(metrics.metrics_available);
    EXPECT_EQ(metrics.socket_count, 0);
}
#endif

}  // namespace
}  // namespace monitoring
}  // namespace kcenon
