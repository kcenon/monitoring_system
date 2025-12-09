
#include <gtest/gtest.h>
#include <kcenon/monitoring/collectors/system_resource_collector.h>
#include <thread>
#include <chrono>

using namespace kcenon::monitoring;

class SystemResourceCollectorTest : public ::testing::Test {
protected:
    void SetUp() override {
        collector = std::make_unique<system_resource_collector>();
    }

    std::unique_ptr<system_resource_collector> collector;
};

TEST_F(SystemResourceCollectorTest, Initialization) {
    std::unordered_map<std::string, std::string> config;
    EXPECT_TRUE(collector->initialize(config));
    EXPECT_EQ(collector->get_name(), "system_resource_collector");
}

TEST_F(SystemResourceCollectorTest, CollectMetrics) {
    // First collection might be zero for rates
    auto metrics1 = collector->collect();
    EXPECT_FALSE(metrics1.empty());
    
    // Check metric names exist
    bool has_cpu = false;
    bool has_memory = false;
    bool has_context = false;
    
    for (const auto& m : metrics1) {
        if (m.name == "cpu_usage_percent") has_cpu = true;
        if (m.name == "memory_usage_percent") has_memory = true;
        if (m.name == "context_switches_total") has_context = true;
    }
    
    EXPECT_TRUE(has_cpu);
    EXPECT_TRUE(has_memory);
    EXPECT_TRUE(has_context);
}

TEST_F(SystemResourceCollectorTest, ContextSwitchMonitoring) {
    // First collection
    auto metrics1 = collector->collect();
    uint64_t csw_total_1 = 0;
    
    for (const auto& m : metrics1) {
        if (m.name == "context_switches_total") {
            try {
                csw_total_1 = static_cast<uint64_t>(std::get<double>(m.value));
            } catch (...) {
                 try { csw_total_1 = static_cast<uint64_t>(std::get<int64_t>(m.value)); } catch(...) {}
            }
        }
    }
    
    EXPECT_GT(csw_total_1, 0) << "Context switches should be non-zero (unless platform stubbed)";

    // Sleep to allow context switches to happen
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Second collection
    auto metrics2 = collector->collect();
    uint64_t csw_total_2 = 0;
    double csw_rate = 0.0;
    
    for (const auto& m : metrics2) {
        if (m.name == "context_switches_total") {
            try {
                csw_total_2 = static_cast<uint64_t>(std::get<double>(m.value));
            } catch (...) {
                 // Try int64_t if double fails
                 try { csw_total_2 = static_cast<uint64_t>(std::get<int64_t>(m.value)); } catch(...) {}
            }
        }
        if (m.name == "context_switches_per_sec") {
            try {
                csw_rate = std::get<double>(m.value);
            } catch (...) {
                 try { csw_rate = static_cast<double>(std::get<int64_t>(m.value)); } catch(...) {}
            }
        }
    }
    
    // On stub platforms (Windows) this might be equal or zero
    #if defined(__APPLE__) || defined(__linux__)
        EXPECT_GE(csw_total_2, csw_total_1);
        // It's possible rate is 0 if no swithces happened, but unlikely.
    #endif
}
