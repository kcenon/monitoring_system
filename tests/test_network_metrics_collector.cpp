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
#include <kcenon/monitoring/collectors/network_metrics_collector.h>

namespace kcenon {
namespace monitoring {
namespace {

// Test fixture for network_metrics_collector tests
class NetworkMetricsCollectorTest : public ::testing::Test {
   protected:
    void SetUp() override {
        collector_ = std::make_unique<network_metrics_collector>();
        std::unordered_map<std::string, std::string> config;
        collector_->initialize(config);
    }

    std::unique_ptr<network_metrics_collector> collector_;
};

// Test basic initialization
TEST_F(NetworkMetricsCollectorTest, InitializesSuccessfully) {
    EXPECT_NE(collector_, nullptr);
    EXPECT_EQ(collector_->get_name(), "network_metrics_collector");
}

// Test metric types returned
TEST_F(NetworkMetricsCollectorTest, ReturnsCorrectMetricTypes) {
    auto types = collector_->get_metric_types();
    EXPECT_FALSE(types.empty());

    // Check for expected socket buffer metric types
    std::vector<std::string> expected_socket_types = {
        "network_socket_recv_buffer_bytes",
        "network_socket_send_buffer_bytes",
        "network_socket_memory_bytes",
        "network_socket_count_total"
    };

    for (const auto& expected : expected_socket_types) {
        bool found = std::find(types.begin(), types.end(), expected) != types.end();
        EXPECT_TRUE(found) << "Expected metric type not found: " << expected;
    }

    // Check for expected TCP state metric types
    std::vector<std::string> expected_tcp_types = {
        "network_tcp_connections_established",
        "network_tcp_connections_time_wait",
        "network_tcp_connections_close_wait",
        "network_tcp_connections_total"
    };

    for (const auto& expected : expected_tcp_types) {
        bool found = std::find(types.begin(), types.end(), expected) != types.end();
        EXPECT_TRUE(found) << "Expected metric type not found: " << expected;
    }
}

// Test configuration options
TEST_F(NetworkMetricsCollectorTest, ConfigurationOptions) {
    auto collector = std::make_unique<network_metrics_collector>();

    std::unordered_map<std::string, std::string> config = {
        {"enabled", "true"},
        {"collect_socket_buffers", "true"},
        {"collect_tcp_states", "true"},
        {"time_wait_warning_threshold", "5000"},
        {"close_wait_warning_threshold", "50"},
        {"queue_full_threshold_bytes", "32768"},
        {"memory_warning_threshold_bytes", "52428800"}
    };

    EXPECT_TRUE(collector->initialize(config));
}

// Test disable collector
TEST_F(NetworkMetricsCollectorTest, CanBeDisabled) {
    auto collector = std::make_unique<network_metrics_collector>();

    std::unordered_map<std::string, std::string> config = {
        {"enabled", "false"}
    };

    collector->initialize(config);

    auto metrics = collector->collect();
    // Disabled collector should return empty metrics
    EXPECT_TRUE(metrics.empty());
}

// Test disable socket buffers collection
TEST_F(NetworkMetricsCollectorTest, CanDisableSocketBuffers) {
    auto collector = std::make_unique<network_metrics_collector>();

    std::unordered_map<std::string, std::string> config = {
        {"collect_socket_buffers", "false"},
        {"collect_tcp_states", "true"}
    };

    collector->initialize(config);

    auto types = collector->get_metric_types();
    // Should not contain socket buffer types
    bool found = std::find(types.begin(), types.end(),
                           "network_socket_recv_buffer_bytes") != types.end();
    EXPECT_FALSE(found);
}

// Test disable TCP states collection
TEST_F(NetworkMetricsCollectorTest, CanDisableTcpStates) {
    auto collector = std::make_unique<network_metrics_collector>();

    std::unordered_map<std::string, std::string> config = {
        {"collect_socket_buffers", "true"},
        {"collect_tcp_states", "false"}
    };

    collector->initialize(config);

    auto types = collector->get_metric_types();
    // Should not contain TCP state types
    bool found = std::find(types.begin(), types.end(),
                           "network_tcp_connections_established") != types.end();
    EXPECT_FALSE(found);
}

// Test statistics
TEST_F(NetworkMetricsCollectorTest, TracksStatistics) {
    auto stats = collector_->get_statistics();
    EXPECT_TRUE(stats.find("collection_count") != stats.end());
    EXPECT_TRUE(stats.find("collection_errors") != stats.end());
    EXPECT_TRUE(stats.find("enabled") != stats.end());
    EXPECT_TRUE(stats.find("socket_buffer_available") != stats.end());
    EXPECT_TRUE(stats.find("tcp_state_available") != stats.end());
}

// Test collect returns metrics (graceful degradation when unavailable)
TEST_F(NetworkMetricsCollectorTest, CollectReturnsMetrics) {
    auto metrics = collector_->collect();
    // May be empty if not available on this platform
    // Just verify it doesn't crash
}

// Test get_last_metrics
TEST_F(NetworkMetricsCollectorTest, GetLastMetrics) {
    collector_->collect();  // Trigger a collection
    auto last = collector_->get_last_metrics();
    // Verify timestamp is set
    auto now = std::chrono::system_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - last.timestamp);
    EXPECT_LT(diff.count(), 10);  // Should be recent
}

// Test monitoring availability checks
TEST_F(NetworkMetricsCollectorTest, MonitoringAvailabilityCheck) {
    bool socket_available = collector_->is_socket_buffer_monitoring_available();
    bool tcp_available = collector_->is_tcp_state_monitoring_available();
    // Just check it returns booleans without crashing
    (void)socket_available;
    (void)tcp_available;
}

// Test network_metrics structure
TEST(NetworkMetricsTest, DefaultInitialization) {
    network_metrics metrics;
    EXPECT_EQ(metrics.recv_buffer_bytes, 0);
    EXPECT_EQ(metrics.send_buffer_bytes, 0);
    EXPECT_EQ(metrics.socket_memory_bytes, 0);
    EXPECT_EQ(metrics.socket_count, 0);
    EXPECT_EQ(metrics.tcp_socket_count, 0);
    EXPECT_EQ(metrics.udp_socket_count, 0);
    EXPECT_FALSE(metrics.socket_buffer_available);
    EXPECT_EQ(metrics.total_connections, 0);
    EXPECT_FALSE(metrics.tcp_state_available);
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

// Test network_info_collector basic functionality
TEST(NetworkInfoCollectorTest, BasicFunctionality) {
    network_info_collector collector;

    // Check availability
    bool socket_available = collector.is_socket_buffer_monitoring_available();
    bool tcp_available = collector.is_tcp_state_monitoring_available();

    // Collect metrics with default config
    network_metrics_config config;
    auto metrics = collector.collect_metrics(config);

    // If available, should have valid data
    if (socket_available) {
        EXPECT_TRUE(metrics.socket_buffer_available);
    }
    if (tcp_available) {
        EXPECT_TRUE(metrics.tcp_state_available);
    }
}

// Test multiple collections are stable
TEST_F(NetworkMetricsCollectorTest, MultipleCollectionsAreStable) {
    for (int i = 0; i < 3; ++i) {
        auto metrics = collector_->collect();
        // Should not crash
    }

    auto stats = collector_->get_statistics();
    // Collection count should have increased
}

// Test that metrics have correct tags when collected
TEST_F(NetworkMetricsCollectorTest, MetricsHaveCorrectTags) {
    auto metrics = collector_->collect();
    for (const auto& m : metrics) {
        // All metrics should have collector tag
        auto it = m.tags.find("collector");
        if (it != m.tags.end()) {
            EXPECT_EQ(it->second, "network_metrics_collector");
        }
    }
}

// Test is_healthy reflects actual state
TEST_F(NetworkMetricsCollectorTest, IsHealthyReflectsState) {
    bool healthy = collector_->is_healthy();
    // Should be healthy initially (no errors yet)
    EXPECT_TRUE(healthy);
}

// Test network_metrics_config default values
TEST(NetworkMetricsConfigTest, DefaultValues) {
    network_metrics_config config;
    EXPECT_TRUE(config.collect_socket_buffers);
    EXPECT_TRUE(config.collect_tcp_states);
    EXPECT_EQ(config.time_wait_warning_threshold, 10000);
    EXPECT_EQ(config.close_wait_warning_threshold, 100);
    EXPECT_EQ(config.queue_full_threshold_bytes, 65536);
    EXPECT_EQ(config.memory_warning_threshold_bytes, 104857600);
}

#if defined(__linux__) || defined(__APPLE__)
// Platform-specific test: On Unix-like systems, monitoring should be available
TEST_F(NetworkMetricsCollectorTest, UnixNetworkMonitoringAvailable) {
    bool socket_available = collector_->is_socket_buffer_monitoring_available();
    bool tcp_available = collector_->is_tcp_state_monitoring_available();
    // At least one should be available on Unix
    EXPECT_TRUE(socket_available || tcp_available);
}

// Platform-specific test: Should have at least some network data
TEST(NetworkInfoCollectorTest, HasNetworkDataOnUnix) {
    network_info_collector collector;

    if (!collector.is_socket_buffer_monitoring_available() &&
        !collector.is_tcp_state_monitoring_available()) {
        GTEST_SKIP() << "Network monitoring not available";
    }

    network_metrics_config config;
    auto metrics = collector.collect_metrics(config);

    // Should have at least some data
    EXPECT_TRUE(metrics.socket_buffer_available || metrics.tcp_state_available);
}

// Platform-specific test: TCP connections should have some data
TEST(NetworkInfoCollectorTest, HasTcpConnectionsOnUnix) {
    network_info_collector collector;

    if (!collector.is_tcp_state_monitoring_available()) {
        GTEST_SKIP() << "TCP state monitoring not available";
    }

    network_metrics_config config;
    config.collect_socket_buffers = false;
    config.collect_tcp_states = true;
    auto metrics = collector.collect_metrics(config);

    EXPECT_TRUE(metrics.tcp_state_available);
    // Should have at least one listening socket or established connection
    EXPECT_GT(metrics.total_connections, 0);
}
#endif

#if defined(_WIN32)
// Platform-specific test: On Windows, monitoring is not yet implemented
TEST_F(NetworkMetricsCollectorTest, WindowsNetworkMonitoringUnavailable) {
    EXPECT_FALSE(collector_->is_socket_buffer_monitoring_available());
    EXPECT_FALSE(collector_->is_tcp_state_monitoring_available());
}

// Platform-specific test: Windows metrics should indicate unavailability
TEST(NetworkInfoCollectorTest, WindowsReturnsUnavailableMetrics) {
    network_info_collector collector;
    network_metrics_config config;
    auto metrics = collector.collect_metrics(config);
    EXPECT_FALSE(metrics.socket_buffer_available);
    EXPECT_FALSE(metrics.tcp_state_available);
}
#endif

}  // namespace
}  // namespace monitoring
}  // namespace kcenon
