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
#include <kcenon/monitoring/platform/metrics_provider.h>

namespace kcenon {
namespace monitoring {
namespace platform {
namespace {

class MetricsProviderTest : public ::testing::Test {
   protected:
    void SetUp() override {
        provider_ = metrics_provider::create();
    }

    std::unique_ptr<metrics_provider> provider_;
};

// Test factory creates appropriate provider
TEST_F(MetricsProviderTest, FactoryCreatesProvider) {
#if defined(__linux__) || defined(__APPLE__) || defined(_WIN32)
    ASSERT_NE(provider_, nullptr);
#else
    // On unsupported platforms, provider may be nullptr
    GTEST_SKIP() << "Unsupported platform";
#endif
}

// Test platform name
TEST_F(MetricsProviderTest, ReturnsCorrectPlatformName) {
    if (!provider_) {
        GTEST_SKIP() << "Provider not available on this platform";
    }

    std::string platform_name = provider_->get_platform_name();
    EXPECT_FALSE(platform_name.empty());

#if defined(__linux__)
    EXPECT_EQ(platform_name, "linux");
#elif defined(__APPLE__)
    EXPECT_EQ(platform_name, "macos");
#elif defined(_WIN32)
    EXPECT_EQ(platform_name, "windows");
#endif
}

// Test uptime info
TEST_F(MetricsProviderTest, GetUptimeReturnsValidInfo) {
    if (!provider_) {
        GTEST_SKIP() << "Provider not available on this platform";
    }

    auto uptime = provider_->get_uptime();

    // Uptime should be available on all supported platforms
    EXPECT_TRUE(uptime.available);
    EXPECT_GT(uptime.uptime_seconds, 0);
}

// Test context switches
TEST_F(MetricsProviderTest, GetContextSwitchesReturnsInfo) {
    if (!provider_) {
        GTEST_SKIP() << "Provider not available on this platform";
    }

    auto ctx_switches = provider_->get_context_switches();

    // On most systems, we should be able to get context switch info
    // The availability flag indicates if the metric was successfully retrieved
    if (ctx_switches.available) {
        EXPECT_GT(ctx_switches.total_switches, 0ULL);
    }
}

// Test file descriptor stats
TEST_F(MetricsProviderTest, GetFdStatsReturnsInfo) {
    if (!provider_) {
        GTEST_SKIP() << "Provider not available on this platform";
    }

    auto fd_stats = provider_->get_fd_stats();

    if (fd_stats.available) {
        // There should be at least some open file descriptors
        EXPECT_GT(fd_stats.open_fds, 0ULL);
        EXPECT_GT(fd_stats.max_fds, 0ULL);
        EXPECT_GE(fd_stats.usage_percent, 0.0);
        EXPECT_LE(fd_stats.usage_percent, 100.0);
    }
}

// Test inode stats
TEST_F(MetricsProviderTest, GetInodeStatsReturnsInfo) {
    if (!provider_) {
        GTEST_SKIP() << "Provider not available on this platform";
    }

    auto inode_stats = provider_->get_inode_stats();

    // Should return at least one filesystem on most systems
    // (though some systems may have no traditional filesystems)
    for (const auto& inode : inode_stats) {
        if (inode.available) {
            EXPECT_GT(inode.total_inodes, 0ULL);
        }
    }
}

// Test TCP states
TEST_F(MetricsProviderTest, GetTcpStatesReturnsInfo) {
    if (!provider_) {
        GTEST_SKIP() << "Provider not available on this platform";
    }

    auto tcp_states = provider_->get_tcp_states();

    if (tcp_states.available) {
        // Total should be sum of all states
        // At minimum, should have at least localhost listening sockets
        EXPECT_GE(tcp_states.total, tcp_states.established);
    }
}

// Test socket buffer stats
TEST_F(MetricsProviderTest, GetSocketBufferStatsReturnsInfo) {
    if (!provider_) {
        GTEST_SKIP() << "Provider not available on this platform";
    }

    auto socket_stats = provider_->get_socket_buffer_stats();

    // Just verify it doesn't crash and returns valid structure
    if (socket_stats.available) {
        // Buffer sizes should be non-negative
        EXPECT_GE(socket_stats.rx_buffer_used, 0ULL);
        EXPECT_GE(socket_stats.tx_buffer_used, 0ULL);
    }
}

// Test interrupt stats
TEST_F(MetricsProviderTest, GetInterruptStatsReturnsInfo) {
    if (!provider_) {
        GTEST_SKIP() << "Provider not available on this platform";
    }

    auto interrupt_stats = provider_->get_interrupt_stats();

    // Should return at least total interrupts on Linux
    for (const auto& irq : interrupt_stats) {
        if (irq.available && irq.name == "total_interrupts") {
            EXPECT_GT(irq.count, 0ULL);
        }
    }
}

// Test security info
TEST_F(MetricsProviderTest, GetSecurityInfoReturnsInfo) {
    if (!provider_) {
        GTEST_SKIP() << "Provider not available on this platform";
    }

    auto security_info = provider_->get_security_info();

    // Just verify it returns without crashing
    // The availability and values depend on system configuration
    SUCCEED();
}

// Test battery availability check
TEST_F(MetricsProviderTest, BatteryAvailabilityCheck) {
    if (!provider_) {
        GTEST_SKIP() << "Provider not available on this platform";
    }

    // This should not crash regardless of whether battery is present
    bool battery_available = provider_->is_battery_available();

    // If battery is available, readings should return data
    if (battery_available) {
        auto readings = provider_->get_battery_readings();
        EXPECT_FALSE(readings.empty());
    }
}

// Test temperature availability check
TEST_F(MetricsProviderTest, TemperatureAvailabilityCheck) {
    if (!provider_) {
        GTEST_SKIP() << "Provider not available on this platform";
    }

    // This should not crash regardless of whether sensors are present
    bool temp_available = provider_->is_temperature_available();

    // If temperature is available, readings should return data
    if (temp_available) {
        auto readings = provider_->get_temperature_readings();
        EXPECT_FALSE(readings.empty());
    }
}

// Test power availability check
TEST_F(MetricsProviderTest, PowerAvailabilityCheck) {
    if (!provider_) {
        GTEST_SKIP() << "Provider not available on this platform";
    }

    // This should not crash regardless of whether power info is available
    bool power_available = provider_->is_power_available();

    // If power is available, info should have valid data
    if (power_available) {
        auto power_info = provider_->get_power_info();
        EXPECT_TRUE(power_info.available);
    }
}

// Test GPU availability check
TEST_F(MetricsProviderTest, GpuAvailabilityCheck) {
    if (!provider_) {
        GTEST_SKIP() << "Provider not available on this platform";
    }

    // This should not crash regardless of whether GPU is present
    bool gpu_available = provider_->is_gpu_available();

    // If GPU is available, info should return data
    if (gpu_available) {
        auto gpu_info = provider_->get_gpu_info();
        EXPECT_FALSE(gpu_info.empty());
    }
}

// Test that multiple calls return consistent results
TEST_F(MetricsProviderTest, MultipleCallsAreConsistent) {
    if (!provider_) {
        GTEST_SKIP() << "Provider not available on this platform";
    }

    // Get uptime twice - should be close
    auto uptime1 = provider_->get_uptime();
    auto uptime2 = provider_->get_uptime();

    if (uptime1.available && uptime2.available) {
        // Uptime should not decrease
        EXPECT_GE(uptime2.uptime_seconds, uptime1.uptime_seconds);
        // Should be within 1 second of each other
        EXPECT_LE(uptime2.uptime_seconds - uptime1.uptime_seconds, 1);
    }
}

}  // namespace
}  // namespace platform
}  // namespace monitoring
}  // namespace kcenon
