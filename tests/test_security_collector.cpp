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
#include <kcenon/monitoring/collectors/security_collector.h>

namespace kcenon {
namespace monitoring {
namespace {

// Test fixture for security_collector tests
class SecurityCollectorTest : public ::testing::Test {
   protected:
    void SetUp() override {
        collector_ = std::make_unique<security_collector>();
        std::unordered_map<std::string, std::string> config;
        collector_->initialize(config);
    }

    std::unique_ptr<security_collector> collector_;
};

// Test basic initialization
TEST_F(SecurityCollectorTest, InitializesSuccessfully) {
    EXPECT_NE(collector_, nullptr);
    EXPECT_EQ(collector_->name(), "security_collector");
}

// Test metric types returned
TEST_F(SecurityCollectorTest, ReturnsCorrectMetricTypes) {
    auto types = collector_->get_metric_types();
    EXPECT_FALSE(types.empty());

    // Check for expected metric types
    std::vector<std::string> expected_types = {
        "security_login_success_total",
        "security_login_failure_total",
        "security_sudo_usage_total",
        "security_events_total"
    };

    for (const auto& expected : expected_types) {
        bool found = std::find(types.begin(), types.end(), expected) != types.end();
        EXPECT_TRUE(found) << "Expected metric type not found: " << expected;
    }
}

// Test configuration options
TEST_F(SecurityCollectorTest, ConfigurationOptions) {
    auto collector = std::make_unique<security_collector>();
    
    std::unordered_map<std::string, std::string> config = {
        {"enabled", "true"},
        {"mask_pii", "true"},
        {"max_recent_events", "50"},
        {"login_failure_rate_limit", "500"}
    };
    
    EXPECT_TRUE(collector->initialize(config));
}

// Test disable collector
TEST_F(SecurityCollectorTest, CanBeDisabled) {
    auto collector = std::make_unique<security_collector>();
    
    std::unordered_map<std::string, std::string> config = {
        {"enabled", "false"}
    };
    
    collector->initialize(config);
    
    auto metrics = collector->collect();
    // Disabled collector should return empty metrics
    EXPECT_TRUE(metrics.empty());
}

// Test statistics
TEST_F(SecurityCollectorTest, TracksStatistics) {
    auto stats = collector_->get_statistics();
    EXPECT_TRUE(stats.find("collection_count") != stats.end());
    EXPECT_TRUE(stats.find("collection_errors") != stats.end());
    EXPECT_TRUE(stats.find("enabled") != stats.end());
    EXPECT_TRUE(stats.find("available") != stats.end());
    EXPECT_TRUE(stats.find("mask_pii") != stats.end());
}

// Test collect returns metrics (graceful degradation when unavailable)
TEST_F(SecurityCollectorTest, CollectReturnsMetrics) {
    auto metrics = collector_->collect();
    // May be empty if not available on this platform
    // Just verify it doesn't crash
}

// Test get_last_metrics
TEST_F(SecurityCollectorTest, GetLastMetrics) {
    collector_->collect();  // Trigger a collection
    auto last = collector_->get_last_metrics();
    // Verify timestamp is set
    auto now = std::chrono::system_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - last.timestamp);
    EXPECT_LT(diff.count(), 10);  // Should be recent
}

// Test is_security_monitoring_available
TEST_F(SecurityCollectorTest, SecurityMonitoringAvailabilityCheck) {
    bool available = collector_->is_security_monitoring_available();
    // Just check it returns a boolean without crashing
    (void)available;
}

// Test security_event_counts structure
TEST(SecurityEventCountsTest, DefaultInitialization) {
    security_event_counts counts;
    EXPECT_EQ(counts.login_success, 0);
    EXPECT_EQ(counts.login_failure, 0);
    EXPECT_EQ(counts.logout, 0);
    EXPECT_EQ(counts.sudo_usage, 0);
    EXPECT_EQ(counts.permission_change, 0);
    EXPECT_EQ(counts.account_created, 0);
    EXPECT_EQ(counts.account_deleted, 0);
    EXPECT_EQ(counts.unknown, 0);
}

// Test security_event_counts increment
TEST(SecurityEventCountsTest, IncrementWorks) {
    security_event_counts counts;
    counts.increment(security_event_type::login_success);
    counts.increment(security_event_type::login_success);
    counts.increment(security_event_type::login_failure);
    
    EXPECT_EQ(counts.login_success, 2);
    EXPECT_EQ(counts.login_failure, 1);
    EXPECT_EQ(counts.total(), 3);
}

// Test security_event_counts get_count
TEST(SecurityEventCountsTest, GetCountWorks) {
    security_event_counts counts;
    counts.login_success = 10;
    counts.login_failure = 5;
    
    EXPECT_EQ(counts.get_count(security_event_type::login_success), 10);
    EXPECT_EQ(counts.get_count(security_event_type::login_failure), 5);
    EXPECT_EQ(counts.get_count(security_event_type::sudo_usage), 0);
}

// Test security_event_type_to_string
TEST(SecurityEventTypeTest, ToStringWorks) {
    EXPECT_EQ(security_event_type_to_string(security_event_type::login_success), "LOGIN_SUCCESS");
    EXPECT_EQ(security_event_type_to_string(security_event_type::login_failure), "LOGIN_FAILURE");
    EXPECT_EQ(security_event_type_to_string(security_event_type::sudo_usage), "SUDO_USAGE");
    EXPECT_EQ(security_event_type_to_string(security_event_type::account_created), "ACCOUNT_CREATED");
    EXPECT_EQ(security_event_type_to_string(security_event_type::unknown), "UNKNOWN");
}

// Test security_info_collector basic functionality
TEST(SecurityInfoCollectorTest, BasicFunctionality) {
    security_info_collector collector;
    
    // Check availability
    bool available = collector.is_security_monitoring_available();
    
    // Collect metrics
    auto metrics = collector.collect_metrics();
    
    // If available, should have valid data
    if (available) {
        EXPECT_TRUE(metrics.metrics_available);
    }
}

// Test multiple collections are stable
TEST_F(SecurityCollectorTest, MultipleCollectionsAreStable) {
    for (int i = 0; i < 3; ++i) {
        auto metrics = collector_->collect();
        // Should not crash
    }
    
    auto stats = collector_->get_statistics();
    // Collection count should have increased
}

// Test that metrics have correct tags when collected
TEST_F(SecurityCollectorTest, MetricsHaveCorrectTags) {
    auto metrics = collector_->collect();
    for (const auto& m : metrics) {
        // All metrics should have collector tag
        auto it = m.tags.find("collector");
        if (it != m.tags.end()) {
            EXPECT_EQ(it->second, "security");
        }
    }
}

// Test is_healthy reflects actual state
TEST_F(SecurityCollectorTest, IsHealthyReflectsState) {
    bool healthy = collector_->is_healthy();
    // Should be healthy initially (no errors yet)
    EXPECT_TRUE(healthy);
}

// Test PII masking configuration
TEST_F(SecurityCollectorTest, PIIMaskingConfiguration) {
    auto collector = std::make_unique<security_collector>();
    
    std::unordered_map<std::string, std::string> config = {
        {"enabled", "true"},
        {"mask_pii", "true"}
    };
    
    EXPECT_TRUE(collector->initialize(config));
    
    auto stats = collector->get_statistics();
    EXPECT_EQ(stats["mask_pii"], 1.0);
}

#if defined(__linux__) || defined(__APPLE__)
// Platform-specific test: On Unix-like systems, check availability
TEST_F(SecurityCollectorTest, UnixSecurityMonitoringCheck) {
    // On Linux/macOS, security monitoring may or may not be available
    // depending on permissions and log file presence
    bool available = collector_->is_security_monitoring_available();
    // Just verify the check completes without error
    (void)available;
}
#endif

#if defined(_WIN32)
// Platform-specific test: On Windows, security monitoring is not yet implemented
TEST_F(SecurityCollectorTest, WindowsSecurityMonitoringUnavailable) {
    EXPECT_FALSE(collector_->is_security_monitoring_available());
}

// Platform-specific test: Windows metrics should indicate unavailability
TEST(SecurityInfoCollectorTest, WindowsReturnsUnavailableMetrics) {
    security_info_collector collector;
    auto metrics = collector.collect_metrics();
    EXPECT_FALSE(metrics.metrics_available);
}
#endif

}  // namespace
}  // namespace monitoring
}  // namespace kcenon
