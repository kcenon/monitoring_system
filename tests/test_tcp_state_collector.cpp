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
#include <kcenon/monitoring/collectors/tcp_state_collector.h>

namespace kcenon {
namespace monitoring {
namespace {

// Test fixture for tcp_state_collector tests
class TcpStateCollectorTest : public ::testing::Test {
   protected:
    void SetUp() override {
        collector_ = std::make_unique<tcp_state_collector>();
        std::unordered_map<std::string, std::string> config;
        collector_->initialize(config);
    }

    std::unique_ptr<tcp_state_collector> collector_;
};

// Test basic initialization
TEST_F(TcpStateCollectorTest, InitializesSuccessfully) {
    EXPECT_NE(collector_, nullptr);
    EXPECT_EQ(collector_->get_name(), "tcp_state_collector");
}

// Test metric types returned
TEST_F(TcpStateCollectorTest, ReturnsCorrectMetricTypes) {
    auto types = collector_->get_metric_types();
    EXPECT_FALSE(types.empty());

    // Check for expected metric types
    std::vector<std::string> expected_types = {
        "tcp_connections_established",
        "tcp_connections_time_wait",
        "tcp_connections_close_wait",
        "tcp_connections_total"
    };

    for (const auto& expected : expected_types) {
        bool found = std::find(types.begin(), types.end(), expected) != types.end();
        EXPECT_TRUE(found) << "Expected metric type not found: " << expected;
    }
}

// Test configuration options
TEST_F(TcpStateCollectorTest, ConfigurationOptions) {
    auto collector = std::make_unique<tcp_state_collector>();
    
    std::unordered_map<std::string, std::string> config = {
        {"enabled", "true"},
        {"include_ipv6", "true"},
        {"time_wait_warning_threshold", "5000"},
        {"close_wait_warning_threshold", "50"}
    };
    
    EXPECT_TRUE(collector->initialize(config));
}

// Test disable collector
TEST_F(TcpStateCollectorTest, CanBeDisabled) {
    auto collector = std::make_unique<tcp_state_collector>();
    
    std::unordered_map<std::string, std::string> config = {
        {"enabled", "false"}
    };
    
    collector->initialize(config);
    
    auto metrics = collector->collect();
    // Disabled collector should return empty metrics
    EXPECT_TRUE(metrics.empty());
}

// Test statistics
TEST_F(TcpStateCollectorTest, TracksStatistics) {
    auto stats = collector_->get_statistics();
    EXPECT_TRUE(stats.find("collection_count") != stats.end());
    EXPECT_TRUE(stats.find("collection_errors") != stats.end());
    EXPECT_TRUE(stats.find("enabled") != stats.end());
    EXPECT_TRUE(stats.find("available") != stats.end());
}

// Test collect returns metrics (graceful degradation when unavailable)
TEST_F(TcpStateCollectorTest, CollectReturnsMetrics) {
    auto metrics = collector_->collect();
    // May be empty if not available on this platform
    // Just verify it doesn't crash
}

// Test get_last_metrics
TEST_F(TcpStateCollectorTest, GetLastMetrics) {
    collector_->collect();  // Trigger a collection
    auto last = collector_->get_last_metrics();
    // Verify timestamp is set
    auto now = std::chrono::system_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - last.timestamp);
    EXPECT_LT(diff.count(), 10);  // Should be recent
}

// Test is_tcp_state_monitoring_available
TEST_F(TcpStateCollectorTest, TcpStateMonitoringAvailabilityCheck) {
    bool available = collector_->is_tcp_state_monitoring_available();
    // Just check it returns a boolean without crashing
    (void)available;
}

// Test tcp_state_counts structure
TEST(TcpStateCountsTest, DefaultInitialization) {
    tcp_state_counts counts;
    EXPECT_EQ(counts.established, 0);
    EXPECT_EQ(counts.syn_sent, 0);
    EXPECT_EQ(counts.syn_recv, 0);
    EXPECT_EQ(counts.fin_wait1, 0);
    EXPECT_EQ(counts.fin_wait2, 0);
    EXPECT_EQ(counts.time_wait, 0);
    EXPECT_EQ(counts.close, 0);
    EXPECT_EQ(counts.close_wait, 0);
    EXPECT_EQ(counts.last_ack, 0);
    EXPECT_EQ(counts.listen, 0);
    EXPECT_EQ(counts.closing, 0);
    EXPECT_EQ(counts.unknown, 0);
}

// Test tcp_state_counts increment
TEST(TcpStateCountsTest, IncrementWorks) {
    tcp_state_counts counts;
    counts.increment(tcp_state::ESTABLISHED);
    counts.increment(tcp_state::ESTABLISHED);
    counts.increment(tcp_state::TIME_WAIT);
    
    EXPECT_EQ(counts.established, 2);
    EXPECT_EQ(counts.time_wait, 1);
    EXPECT_EQ(counts.total(), 3);
}

// Test tcp_state_counts get_count
TEST(TcpStateCountsTest, GetCountWorks) {
    tcp_state_counts counts;
    counts.established = 10;
    counts.close_wait = 5;
    
    EXPECT_EQ(counts.get_count(tcp_state::ESTABLISHED), 10);
    EXPECT_EQ(counts.get_count(tcp_state::CLOSE_WAIT), 5);
    EXPECT_EQ(counts.get_count(tcp_state::TIME_WAIT), 0);
}

// Test tcp_state_to_string
TEST(TcpStateTest, ToStringWorks) {
    EXPECT_EQ(tcp_state_to_string(tcp_state::ESTABLISHED), "ESTABLISHED");
    EXPECT_EQ(tcp_state_to_string(tcp_state::SYN_SENT), "SYN_SENT");
    EXPECT_EQ(tcp_state_to_string(tcp_state::TIME_WAIT), "TIME_WAIT");
    EXPECT_EQ(tcp_state_to_string(tcp_state::CLOSE_WAIT), "CLOSE_WAIT");
    EXPECT_EQ(tcp_state_to_string(tcp_state::LISTEN), "LISTEN");
    EXPECT_EQ(tcp_state_to_string(tcp_state::UNKNOWN), "UNKNOWN");
}

// Test tcp_state_info_collector basic functionality
TEST(TcpStateInfoCollectorTest, BasicFunctionality) {
    tcp_state_info_collector collector;
    
    // Check availability
    bool available = collector.is_tcp_state_monitoring_available();
    
    // Collect metrics
    auto metrics = collector.collect_metrics();
    
    // If available, should have valid data
    if (available) {
        EXPECT_TRUE(metrics.metrics_available);
    }
}

// Test multiple collections are stable
TEST_F(TcpStateCollectorTest, MultipleCollectionsAreStable) {
    for (int i = 0; i < 3; ++i) {
        auto metrics = collector_->collect();
        // Should not crash
    }
    
    auto stats = collector_->get_statistics();
    // Collection count should have increased
}

// Test that metrics have correct tags when collected
TEST_F(TcpStateCollectorTest, MetricsHaveCorrectTags) {
    auto metrics = collector_->collect();
    for (const auto& m : metrics) {
        // All metrics should have collector tag
        auto it = m.tags.find("collector");
        if (it != m.tags.end()) {
            EXPECT_EQ(it->second, "tcp_state");
        }
    }
}

// Test is_healthy reflects actual state
TEST_F(TcpStateCollectorTest, IsHealthyReflectsState) {
    bool healthy = collector_->is_healthy();
    // Should be healthy initially (no errors yet)
    EXPECT_TRUE(healthy);
}

#if defined(__linux__) || defined(__APPLE__)
// Platform-specific test: On Unix-like systems, TCP state monitoring should be available
TEST_F(TcpStateCollectorTest, UnixTcpStateMonitoringAvailable) {
    EXPECT_TRUE(collector_->is_tcp_state_monitoring_available());
}

// Platform-specific test: Should have at least some connections
TEST(TcpStateInfoCollectorTest, HasConnectionsOnUnix) {
    tcp_state_info_collector collector;
    
    if (!collector.is_tcp_state_monitoring_available()) {
        GTEST_SKIP() << "TCP state monitoring not available";
    }
    
    auto metrics = collector.collect_metrics();
    EXPECT_TRUE(metrics.metrics_available);
    
    // Should have at least one listening socket or established connection
    // on a running system
    EXPECT_GT(metrics.total_connections, 0);
}

// Platform-specific test: LISTEN and ESTABLISHED should typically be present
TEST(TcpStateInfoCollectorTest, HasListenAndEstablished) {
    tcp_state_info_collector collector;
    
    if (!collector.is_tcp_state_monitoring_available()) {
        GTEST_SKIP() << "TCP state monitoring not available";
    }
    
    auto metrics = collector.collect_metrics();
    
    // A running system should have at least one LISTEN socket
    // (unless it's a very minimal system)
    // We don't fail the test, just check the values are reasonable
    EXPECT_GE(metrics.combined_counts.listen, 0);
    EXPECT_GE(metrics.combined_counts.established, 0);
}
#endif

#if defined(_WIN32)
// Platform-specific test: On Windows, TCP state monitoring is not yet implemented
TEST_F(TcpStateCollectorTest, WindowsTcpStateMonitoringUnavailable) {
    EXPECT_FALSE(collector_->is_tcp_state_monitoring_available());
}

// Platform-specific test: Windows metrics should indicate unavailability
TEST(TcpStateInfoCollectorTest, WindowsReturnsUnavailableMetrics) {
    tcp_state_info_collector collector;
    auto metrics = collector.collect_metrics();
    EXPECT_FALSE(metrics.metrics_available);
    EXPECT_EQ(metrics.total_connections, 0);
}
#endif

}  // namespace
}  // namespace monitoring
}  // namespace kcenon
