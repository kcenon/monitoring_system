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
#include <kcenon/monitoring/collectors/process_metrics_collector.h>

#include <fstream>

namespace kcenon {
namespace monitoring {
namespace {

class ProcessMetricsCollectorTest : public ::testing::Test {
   protected:
    void SetUp() override {
        collector_ = std::make_unique<process_metrics_collector>();
        std::unordered_map<std::string, std::string> config;
        collector_->initialize(config);
    }

    std::unique_ptr<process_metrics_collector> collector_;
};

TEST_F(ProcessMetricsCollectorTest, InitializesSuccessfully) {
    EXPECT_NE(collector_, nullptr);
    EXPECT_EQ(collector_->name(), "process_metrics_collector");
}

TEST_F(ProcessMetricsCollectorTest, ReturnsCorrectMetricTypes) {
    auto types = collector_->get_metric_types();
    EXPECT_FALSE(types.empty());

    std::vector<std::string> expected = {
        "process.fd.open_count",
        "process.fd.usage_percent",
        "process.fs.inodes_total",
        "process.fs.inodes_used",
        "process.context_switches.total",
        "process.context_switches.voluntary"
    };

    for (const auto& expected_type : expected) {
        EXPECT_NE(std::find(types.begin(), types.end(), expected_type), types.end())
            << "Missing metric type: " << expected_type;
    }
}

TEST_F(ProcessMetricsCollectorTest, ConfigurationOptions) {
    auto collector = std::make_unique<process_metrics_collector>();
    std::unordered_map<std::string, std::string> config;
    config["fd_warning_threshold"] = "70.0";
    config["fd_critical_threshold"] = "90.0";
    config["collect_inodes"] = "false";
    EXPECT_TRUE(collector->initialize(config));

    auto stats = collector->get_statistics();
    EXPECT_DOUBLE_EQ(stats["fd_warning_threshold"], 70.0);
    EXPECT_DOUBLE_EQ(stats["fd_critical_threshold"], 90.0);
    EXPECT_DOUBLE_EQ(stats["collect_inodes"], 0.0);
}

TEST_F(ProcessMetricsCollectorTest, CanBeDisabled) {
    auto collector = std::make_unique<process_metrics_collector>();
    std::unordered_map<std::string, std::string> config;
    config["enabled"] = "false";
    collector->initialize(config);

    auto metrics = collector->collect();
    EXPECT_TRUE(metrics.empty());

    auto stats = collector->get_statistics();
    EXPECT_DOUBLE_EQ(stats["enabled"], 0.0);
}

TEST_F(ProcessMetricsCollectorTest, TracksStatistics) {
    collector_->collect();
    collector_->collect();

    auto stats = collector_->get_statistics();
    EXPECT_GE(stats["collection_count"], 2.0);
    EXPECT_GE(stats["collection_errors"], 0.0);
}

TEST_F(ProcessMetricsCollectorTest, CollectReturnsMetrics) {
    EXPECT_NO_THROW(collector_->collect());
}

TEST_F(ProcessMetricsCollectorTest, GetLastMetrics) {
    collector_->collect();
    auto last = collector_->get_last_metrics();

    auto now = std::chrono::system_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - last.timestamp);
    EXPECT_LT(diff.count(), 10);
}

TEST_F(ProcessMetricsCollectorTest, MonitoringAvailabilityChecks) {
    EXPECT_NO_THROW(collector_->is_fd_monitoring_available());
    EXPECT_NO_THROW(collector_->is_inode_monitoring_available());
    EXPECT_NO_THROW(collector_->is_context_switch_monitoring_available());
}

TEST_F(ProcessMetricsCollectorTest, SelectiveCollectionFdOnly) {
    auto collector = std::make_unique<process_metrics_collector>();
    std::unordered_map<std::string, std::string> config;
    config["collect_fd"] = "true";
    config["collect_inodes"] = "false";
    config["collect_context_switches"] = "false";
    collector->initialize(config);

    auto types = collector->get_metric_types();
    bool has_fd_metric = false;
    bool has_inode_metric = false;
    bool has_cs_metric = false;

    for (const auto& t : types) {
        if (t.find("process.fd.") != std::string::npos) has_fd_metric = true;
        if (t.find("process.fs.") != std::string::npos) has_inode_metric = true;
        if (t.find("process.context_switches.") != std::string::npos) has_cs_metric = true;
    }

    EXPECT_TRUE(has_fd_metric);
    EXPECT_FALSE(has_inode_metric);
    EXPECT_FALSE(has_cs_metric);
}

TEST_F(ProcessMetricsCollectorTest, SelectiveCollectionInodesOnly) {
    auto collector = std::make_unique<process_metrics_collector>();
    std::unordered_map<std::string, std::string> config;
    config["collect_fd"] = "false";
    config["collect_inodes"] = "true";
    config["collect_context_switches"] = "false";
    collector->initialize(config);

    auto types = collector->get_metric_types();
    bool has_fd_metric = false;
    bool has_inode_metric = false;
    bool has_cs_metric = false;

    for (const auto& t : types) {
        if (t.find("process.fd.") != std::string::npos) has_fd_metric = true;
        if (t.find("process.fs.") != std::string::npos) has_inode_metric = true;
        if (t.find("process.context_switches.") != std::string::npos) has_cs_metric = true;
    }

    EXPECT_FALSE(has_fd_metric);
    EXPECT_TRUE(has_inode_metric);
    EXPECT_FALSE(has_cs_metric);
}

TEST_F(ProcessMetricsCollectorTest, MultipleCollectionsAreStable) {
    for (int i = 0; i < 10; ++i) {
        auto metrics = collector_->collect();
        EXPECT_NO_THROW(collector_->get_statistics());
    }

    auto stats = collector_->get_statistics();
    EXPECT_GE(stats["collection_count"], 10.0);
}

TEST_F(ProcessMetricsCollectorTest, MetricsHaveCorrectTags) {
    auto metrics = collector_->collect();

    for (const auto& m : metrics) {
        auto it = m.tags.find("collector");
        if (it != m.tags.end()) {
            EXPECT_EQ(it->second, "process_metrics_collector");
        }
    }
}

TEST_F(ProcessMetricsCollectorTest, IsHealthyReflectsState) {
    EXPECT_NO_THROW(collector_->is_healthy());

    auto disabled_collector = std::make_unique<process_metrics_collector>();
    std::unordered_map<std::string, std::string> config;
    config["enabled"] = "false";
    disabled_collector->initialize(config);
    EXPECT_TRUE(disabled_collector->is_healthy());
}

TEST_F(ProcessMetricsCollectorTest, ConfigConstructor) {
    process_metrics_config config;
    config.collect_fd = true;
    config.collect_inodes = false;
    config.collect_context_switches = false;
    config.fd_warning_threshold = 75.0;

    auto collector = std::make_unique<process_metrics_collector>(config);
    std::unordered_map<std::string, std::string> init_config;
    collector->initialize(init_config);

    auto stats = collector->get_statistics();
    EXPECT_DOUBLE_EQ(stats["collect_fd"], 1.0);
    EXPECT_DOUBLE_EQ(stats["collect_inodes"], 0.0);
    EXPECT_DOUBLE_EQ(stats["collect_context_switches"], 0.0);
    EXPECT_DOUBLE_EQ(stats["fd_warning_threshold"], 75.0);
}

TEST(ProcessMetricsStructTest, DefaultInitialization) {
    process_metrics metrics;
    EXPECT_EQ(metrics.fd.fd_used_process, 0);
    EXPECT_EQ(metrics.inodes.total_inodes, 0);
    EXPECT_EQ(metrics.context_switches.system_context_switches_total, 0);
}

TEST(FdInfoCollectorTest, BasicFunctionality) {
    fd_info_collector collector;

    EXPECT_NO_THROW(collector.is_fd_monitoring_available());

    auto metrics = collector.collect_metrics();

    auto now = std::chrono::system_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - metrics.timestamp);
    EXPECT_LT(diff.count(), 10);
}

TEST(FdInfoCollectorTest, ProcessFDCountChangesWithOpenFiles) {
    fd_info_collector collector;

    auto initial = collector.collect_metrics();

    std::vector<std::unique_ptr<std::fstream>> files;
    for (int i = 0; i < 5; ++i) {
        auto file = std::make_unique<std::fstream>();
        file->open("/dev/null", std::ios::in);
        if (file->is_open()) {
            files.push_back(std::move(file));
        }
    }

    auto after_open = collector.collect_metrics();

    files.clear();

    if (initial.fd_used_process > 0 && after_open.fd_used_process > 0) {
        EXPECT_GE(after_open.fd_used_process, initial.fd_used_process);
    }
}

TEST(InodeInfoCollectorTest, BasicFunctionality) {
    inode_info_collector collector;

    EXPECT_NO_THROW(collector.is_inode_monitoring_available());

    auto metrics = collector.collect_metrics();

    auto now = std::chrono::system_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - metrics.timestamp);
    EXPECT_LT(diff.count(), 10);
}

TEST(ContextSwitchInfoCollectorTest, BasicFunctionality) {
    context_switch_info_collector collector;

    EXPECT_NO_THROW(collector.is_context_switch_monitoring_available());

    auto metrics = collector.collect_metrics();

    auto now = std::chrono::system_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - metrics.timestamp);
    EXPECT_LT(diff.count(), 10);
}

#if defined(__linux__) || defined(__APPLE__)
TEST_F(ProcessMetricsCollectorTest, UnixFdMonitoringAvailable) {
    EXPECT_TRUE(collector_->is_fd_monitoring_available());
}

TEST_F(ProcessMetricsCollectorTest, UnixInodeMonitoringAvailable) {
    EXPECT_TRUE(collector_->is_inode_monitoring_available());
}

TEST_F(ProcessMetricsCollectorTest, UnixContextSwitchMonitoringAvailable) {
    EXPECT_TRUE(collector_->is_context_switch_monitoring_available());
}

TEST(InodeInfoCollectorTest, HasFilesystemsOnUnix) {
    inode_info_collector collector;

    if (collector.is_inode_monitoring_available()) {
        auto metrics = collector.collect_metrics();
        EXPECT_TRUE(metrics.metrics_available);
        EXPECT_FALSE(metrics.filesystems.empty());
    }
}

TEST(ContextSwitchInfoCollectorTest, ProcessSwitchesNonNegative) {
    context_switch_info_collector collector;

    if (collector.is_context_switch_monitoring_available()) {
        auto metrics = collector.collect_metrics();
        EXPECT_GE(metrics.process_info.voluntary_switches, 0);
        EXPECT_GE(metrics.process_info.nonvoluntary_switches, 0);
        EXPECT_GE(metrics.process_info.total_switches, 0);
    }
}
#endif

#if defined(_WIN32)
TEST_F(ProcessMetricsCollectorTest, WindowsInodeMonitoringUnavailable) {
    EXPECT_FALSE(collector_->is_inode_monitoring_available());
}

TEST_F(ProcessMetricsCollectorTest, WindowsContextSwitchMonitoringUnavailable) {
    EXPECT_FALSE(collector_->is_context_switch_monitoring_available());
}
#endif

}  // namespace
}  // namespace monitoring
}  // namespace kcenon
