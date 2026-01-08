
#include <gtest/gtest.h>
#include <kcenon/monitoring/collectors/system_resource_collector.h>
#include <thread>
#include <chrono>
#include <unordered_set>

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
    #if defined(__linux__)
        // On Linux, system-wide context switches should be monotonically increasing
        EXPECT_GE(csw_total_2, csw_total_1);
    #elif defined(__APPLE__)
        // On macOS, we read process-level context switches which may not be
        // monotonically increasing in CI environments due to process lifecycle
        // Just verify we got valid readings
        EXPECT_GT(csw_total_1, 0u);
        EXPECT_GT(csw_total_2, 0u);
    #endif
}

TEST_F(SystemResourceCollectorTest, DiskMetricsCollection) {
    // First collection to initialize state
    collector->collect();

    // Wait a bit to allow I/O to happen
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Second collection should have disk metrics
    auto metrics = collector->collect();

    bool has_disk_usage = false;
    bool has_disk_total = false;
    bool has_disk_used = false;
    bool has_disk_available = false;
    bool has_disk_read_bytes = false;
    bool has_disk_write_bytes = false;

    double disk_usage_percent = 0.0;
    double disk_total = 0.0;

    for (const auto& m : metrics) {
        if (m.name == "disk_usage_percent") {
            has_disk_usage = true;
            disk_usage_percent = std::get<double>(m.value);
        }
        if (m.name == "disk_total_bytes") {
            has_disk_total = true;
            disk_total = std::get<double>(m.value);
        }
        if (m.name == "disk_used_bytes") has_disk_used = true;
        if (m.name == "disk_available_bytes") has_disk_available = true;
        if (m.name == "disk_read_bytes_per_sec") has_disk_read_bytes = true;
        if (m.name == "disk_write_bytes_per_sec") has_disk_write_bytes = true;
    }

    // All disk metrics should be present
    EXPECT_TRUE(has_disk_usage) << "disk_usage_percent metric should be present";
    EXPECT_TRUE(has_disk_total) << "disk_total_bytes metric should be present";
    EXPECT_TRUE(has_disk_used) << "disk_used_bytes metric should be present";
    EXPECT_TRUE(has_disk_available) << "disk_available_bytes metric should be present";
    EXPECT_TRUE(has_disk_read_bytes) << "disk_read_bytes_per_sec metric should be present";
    EXPECT_TRUE(has_disk_write_bytes) << "disk_write_bytes_per_sec metric should be present";

    // Disk usage should be a valid percentage
    EXPECT_GE(disk_usage_percent, 0.0);
    EXPECT_LE(disk_usage_percent, 100.0);

    // Total disk should be non-zero
    EXPECT_GT(disk_total, 0.0) << "Total disk space should be non-zero";
}

TEST_F(SystemResourceCollectorTest, NetworkMetricsCollection) {
    // First collection to initialize state
    collector->collect();

    // Wait a bit to allow network activity
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Second collection should have network metrics
    auto metrics = collector->collect();

    bool has_rx_bytes = false;
    bool has_tx_bytes = false;
    bool has_rx_packets = false;
    bool has_tx_packets = false;
    bool has_rx_errors = false;
    bool has_tx_errors = false;
    bool has_rx_dropped = false;
    bool has_tx_dropped = false;

    for (const auto& m : metrics) {
        if (m.name == "network_rx_bytes_per_sec") has_rx_bytes = true;
        if (m.name == "network_tx_bytes_per_sec") has_tx_bytes = true;
        if (m.name == "network_rx_packets_per_sec") has_rx_packets = true;
        if (m.name == "network_tx_packets_per_sec") has_tx_packets = true;
        if (m.name == "network_rx_errors") has_rx_errors = true;
        if (m.name == "network_tx_errors") has_tx_errors = true;
        if (m.name == "network_rx_dropped") has_rx_dropped = true;
        if (m.name == "network_tx_dropped") has_tx_dropped = true;
    }

    // All network metrics should be present
    EXPECT_TRUE(has_rx_bytes) << "network_rx_bytes_per_sec metric should be present";
    EXPECT_TRUE(has_tx_bytes) << "network_tx_bytes_per_sec metric should be present";
    EXPECT_TRUE(has_rx_packets) << "network_rx_packets_per_sec metric should be present";
    EXPECT_TRUE(has_tx_packets) << "network_tx_packets_per_sec metric should be present";
    EXPECT_TRUE(has_rx_errors) << "network_rx_errors metric should be present";
    EXPECT_TRUE(has_tx_errors) << "network_tx_errors metric should be present";
    EXPECT_TRUE(has_rx_dropped) << "network_rx_dropped metric should be present";
    EXPECT_TRUE(has_tx_dropped) << "network_tx_dropped metric should be present";
}

TEST_F(SystemResourceCollectorTest, GetMetricTypesIncludesNewMetrics) {
    auto types = collector->get_metric_types();

    // Convert to set for easier lookup
    std::unordered_set<std::string> type_set(types.begin(), types.end());

    // Check disk metrics are listed
    EXPECT_TRUE(type_set.count("disk_usage_percent") > 0);
    EXPECT_TRUE(type_set.count("disk_total_bytes") > 0);
    EXPECT_TRUE(type_set.count("disk_read_bytes_per_sec") > 0);
    EXPECT_TRUE(type_set.count("disk_read_ops_per_sec") > 0);

    // Check network metrics are listed
    EXPECT_TRUE(type_set.count("network_rx_bytes_per_sec") > 0);
    EXPECT_TRUE(type_set.count("network_tx_bytes_per_sec") > 0);
    EXPECT_TRUE(type_set.count("network_rx_errors") > 0);
    EXPECT_TRUE(type_set.count("network_rx_dropped") > 0);
}

TEST_F(SystemResourceCollectorTest, CollectionFiltersWork) {
    // Disable disk and network metrics
    collector->set_collection_filters(true, true, false, false);

    auto metrics = collector->collect();

    bool has_cpu = false;
    bool has_disk = false;
    bool has_network = false;

    for (const auto& m : metrics) {
        if (m.name == "cpu_usage_percent") has_cpu = true;
        if (m.name == "disk_usage_percent") has_disk = true;
        if (m.name == "network_rx_bytes_per_sec") has_network = true;
    }

    EXPECT_TRUE(has_cpu) << "CPU metrics should be collected when enabled";
    EXPECT_FALSE(has_disk) << "Disk metrics should not be collected when disabled";
    EXPECT_FALSE(has_network) << "Network metrics should not be collected when disabled";

    // Re-enable all metrics
    collector->set_collection_filters(true, true, true, true);

    metrics = collector->collect();
    has_disk = false;
    has_network = false;

    for (const auto& m : metrics) {
        if (m.name == "disk_usage_percent") has_disk = true;
        if (m.name == "network_rx_bytes_per_sec") has_network = true;
    }

    EXPECT_TRUE(has_disk) << "Disk metrics should be collected when re-enabled";
    EXPECT_TRUE(has_network) << "Network metrics should be collected when re-enabled";
}
