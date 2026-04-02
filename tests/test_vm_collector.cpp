// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include <gtest/gtest.h>
#include <kcenon/monitoring/collectors/vm_collector.h>

namespace kcenon {
namespace monitoring {
namespace {

// Test fixture for vm_collector tests
class VMCollectorTest : public ::testing::Test {
   protected:
    void SetUp() override {
        collector_ = std::make_unique<vm_collector>();
        std::unordered_map<std::string, std::string> config;
        collector_->initialize(config);
    }

    std::unique_ptr<vm_collector> collector_;
};

// Test basic initialization
TEST_F(VMCollectorTest, InitializesSuccessfully) {
    EXPECT_NE(collector_, nullptr);
    EXPECT_EQ(collector_->name(), "vm_collector");
}

// Test metric types returned
TEST_F(VMCollectorTest, ReturnsCorrectMetricTypes) {
    auto types = collector_->get_metric_types();
    EXPECT_FALSE(types.empty());

    // Check for expected metric types
    std::vector<std::string> expected_types = {
        "system.vm.is_virtualized",
        "system.vm.steal_time"
    };

    for (const auto& expected : expected_types) {
        bool found = std::find(types.begin(), types.end(), expected) != types.end();
        EXPECT_TRUE(found) << "Expected metric type not found: " << expected;
    }
}

// Test configuration options
TEST_F(VMCollectorTest, ConfigurationOptions) {
    auto collector = std::make_unique<vm_collector>();
    
    std::unordered_map<std::string, std::string> config = {
        {"enabled", "true"}
    };
    
    EXPECT_TRUE(collector->initialize(config));
}

// Test disable collector
TEST_F(VMCollectorTest, CanBeDisabled) {
    auto collector = std::make_unique<vm_collector>();
    
    std::unordered_map<std::string, std::string> config = {
        {"enabled", "false"}
    };
    
    collector->initialize(config);
    
    auto metrics = collector->collect();
    // Disabled collector should return empty metrics
    EXPECT_TRUE(metrics.empty());
}

// Test statistics
TEST_F(VMCollectorTest, TracksStatistics) {
    auto stats = collector_->get_statistics();
    EXPECT_TRUE(stats.find("collection_count") != stats.end());
    EXPECT_TRUE(stats.find("collection_errors") != stats.end());
}

// Test collect returns metrics
TEST_F(VMCollectorTest, CollectReturnsMetrics) {
    auto metrics = collector_->collect();
    // Should return at least 'system.vm.is_virtualized'
    EXPECT_FALSE(metrics.empty());
    
    bool found_is_virt = false;
    for (const auto& m : metrics) {
        if (m.name == "system.vm.is_virtualized") found_is_virt = true;
    }
    EXPECT_TRUE(found_is_virt);
}

// Test vm_type_to_string
TEST(VMTypeTest, ToStringWorks) {
    EXPECT_EQ(vm_type_to_string(vm_type::kvm), "KVM");
    EXPECT_EQ(vm_type_to_string(vm_type::vmware), "VMWARE");
    EXPECT_EQ(vm_type_to_string(vm_type::none), "NONE");
}

}  // namespace
}  // namespace monitoring
}  // namespace kcenon
