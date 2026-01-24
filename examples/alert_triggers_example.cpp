// BSD 3-Clause License
//
// Copyright (c) 2021-2025, kcenon
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

/**
 * @file alert_triggers_example.cpp
 * @brief Comprehensive example demonstrating various alert trigger types
 *
 * This example demonstrates:
 * - ThresholdTrigger implementation (above/below/equal)
 * - RateOfChangeTrigger for trend detection
 * - AnomalyTrigger using statistical deviation
 * - CompositeTrigger with AND/OR conditions
 * - Custom trigger implementation pattern
 * - RangeTrigger, DeltaTrigger, and AbsentTrigger
 */

#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <random>
#include <thread>
#include <vector>

#include "kcenon/monitoring/alert/alert_triggers.h"

using namespace kcenon::monitoring;
using namespace std::chrono_literals;

// Helper to print trigger evaluation result
void print_eval_result(const std::string& trigger_name,
                       double value,
                       bool triggered,
                       const std::string& description = "") {
    std::cout << "    " << std::setw(25) << std::left << trigger_name
              << " | Value: " << std::setw(8) << std::fixed << std::setprecision(2) << value
              << " | Triggered: " << (triggered ? "YES" : "NO ");
    if (!description.empty()) {
        std::cout << " | " << description;
    }
    std::cout << std::endl;
}

// Custom trigger implementation: Periodic trigger (fires every N evaluations)
class periodic_trigger : public alert_trigger {
public:
    explicit periodic_trigger(size_t period)
        : period_(period), count_(0) {}

    bool evaluate(double /*value*/) const override {
        ++count_;
        if (count_ >= period_) {
            count_ = 0;
            return true;
        }
        return false;
    }

    std::string type_name() const override {
        return "periodic";
    }

    std::string description() const override {
        return "fires every " + std::to_string(period_) + " evaluations";
    }

    void reset() {
        count_ = 0;
    }

private:
    size_t period_;
    mutable size_t count_;
};

// Custom trigger: Moving average based trigger
class moving_average_trigger : public alert_trigger {
public:
    moving_average_trigger(size_t window_size, double threshold)
        : window_size_(window_size)
        , threshold_(threshold) {}

    bool evaluate(double value) const override {
        values_.push_back(value);
        if (values_.size() > window_size_) {
            values_.erase(values_.begin());
        }

        if (values_.size() < window_size_) {
            return false;  // Not enough data yet
        }

        double sum = 0.0;
        for (double v : values_) {
            sum += v;
        }
        double avg = sum / static_cast<double>(values_.size());

        return avg > threshold_;
    }

    std::string type_name() const override {
        return "moving_average";
    }

    std::string description() const override {
        return "MA(" + std::to_string(window_size_) + ") > " + std::to_string(threshold_);
    }

    double current_average() const {
        if (values_.empty()) return 0.0;
        double sum = 0.0;
        for (double v : values_) {
            sum += v;
        }
        return sum / static_cast<double>(values_.size());
    }

    void reset() {
        values_.clear();
    }

private:
    size_t window_size_;
    double threshold_;
    mutable std::vector<double> values_;
};

int main() {
    std::cout << "=== Alert Triggers Example ===" << std::endl;
    std::cout << std::endl;

    // =========================================================================
    // Section 1: ThresholdTrigger - Basic Comparisons
    // =========================================================================
    std::cout << "1. Threshold Triggers" << std::endl;
    std::cout << "   -------------------" << std::endl;

    // Create various threshold triggers using factory methods
    auto above_80 = threshold_trigger::above(80.0);
    auto above_or_equal_90 = threshold_trigger::above_or_equal(90.0);
    auto below_20 = threshold_trigger::below(20.0);
    auto below_or_equal_10 = threshold_trigger::below_or_equal(10.0);

    // Using constructor directly with comparison operators
    auto equal_50 = std::make_shared<threshold_trigger>(
        50.0, comparison_operator::equal, 0.5);  // epsilon = 0.5
    auto not_equal_100 = std::make_shared<threshold_trigger>(
        100.0, comparison_operator::not_equal);

    std::cout << "   Testing threshold triggers with various values:" << std::endl;
    std::cout << std::endl;

    std::vector<double> test_values = {5.0, 10.0, 20.0, 50.0, 50.3, 80.0, 85.0, 90.0, 100.0};

    for (double val : test_values) {
        std::cout << "   Value: " << val << std::endl;
        print_eval_result("above(80)", val, above_80->evaluate(val), above_80->description());
        print_eval_result("above_or_equal(90)", val, above_or_equal_90->evaluate(val));
        print_eval_result("below(20)", val, below_20->evaluate(val));
        print_eval_result("below_or_equal(10)", val, below_or_equal_10->evaluate(val));
        print_eval_result("equal(50, eps=0.5)", val, equal_50->evaluate(val));
        print_eval_result("not_equal(100)", val, not_equal_100->evaluate(val));
        std::cout << std::endl;
    }

    // =========================================================================
    // Section 2: RangeTrigger - In/Out of Range
    // =========================================================================
    std::cout << "2. Range Triggers" << std::endl;
    std::cout << "   ---------------" << std::endl;

    auto in_range_40_60 = threshold_trigger::in_range(40.0, 60.0);
    auto out_of_range_40_60 = threshold_trigger::out_of_range(40.0, 60.0);

    std::cout << "   Range triggers test [40, 60]:" << std::endl;
    std::cout << std::endl;

    std::vector<double> range_values = {30.0, 40.0, 50.0, 60.0, 70.0};
    for (double val : range_values) {
        print_eval_result("in_range(40,60)", val, in_range_40_60->evaluate(val),
                         in_range_40_60->description());
        print_eval_result("out_of_range(40,60)", val, out_of_range_40_60->evaluate(val),
                         out_of_range_40_60->description());
        std::cout << std::endl;
    }

    // =========================================================================
    // Section 3: RateOfChangeTrigger - Trend Detection
    // =========================================================================
    std::cout << "3. Rate of Change Trigger" << std::endl;
    std::cout << "   -----------------------" << std::endl;

    // Trigger when value increases by more than 10 per 500ms window
    auto rate_increasing = std::make_shared<rate_of_change_trigger>(
        10.0,                                                   // rate threshold
        500ms,                                                  // time window
        rate_of_change_trigger::rate_direction::increasing,     // direction
        3                                                       // minimum samples
    );

    // Trigger on rapid decrease
    auto rate_decreasing = std::make_shared<rate_of_change_trigger>(
        5.0,
        500ms,
        rate_of_change_trigger::rate_direction::decreasing,
        3
    );

    // Trigger on any rapid change
    auto rate_either = std::make_shared<rate_of_change_trigger>(
        8.0,
        500ms,
        rate_of_change_trigger::rate_direction::either,
        3
    );

    std::cout << "   Simulating rapidly increasing values:" << std::endl;

    // Simulate increasing values
    std::vector<double> increasing_values = {10.0, 15.0, 25.0, 40.0, 60.0, 85.0};
    for (size_t i = 0; i < increasing_values.size(); ++i) {
        double val = increasing_values[i];
        bool triggered = rate_increasing->evaluate(val);
        std::cout << "    Sample " << (i + 1) << ": value=" << val
                  << " | Rate trigger: " << (triggered ? "YES" : "NO") << std::endl;
        std::this_thread::sleep_for(100ms);  // Simulate time between samples
    }
    std::cout << std::endl;

    // Reset and simulate decreasing values
    rate_decreasing->reset();
    std::cout << "   Simulating rapidly decreasing values:" << std::endl;

    std::vector<double> decreasing_values = {100.0, 95.0, 85.0, 70.0, 50.0, 25.0};
    for (size_t i = 0; i < decreasing_values.size(); ++i) {
        double val = decreasing_values[i];
        bool triggered = rate_decreasing->evaluate(val);
        std::cout << "    Sample " << (i + 1) << ": value=" << val
                  << " | Rate trigger: " << (triggered ? "YES" : "NO") << std::endl;
        std::this_thread::sleep_for(100ms);
    }
    std::cout << std::endl;

    // =========================================================================
    // Section 4: AnomalyTrigger - Statistical Deviation
    // =========================================================================
    std::cout << "4. Anomaly Trigger (Statistical)" << std::endl;
    std::cout << "   ------------------------------" << std::endl;

    // Trigger when value is more than 2 standard deviations from mean
    auto anomaly_trigger_ptr = std::make_shared<anomaly_trigger>(
        2.0,    // sensitivity (number of std devs)
        20,     // window size for baseline
        5       // minimum samples before detection
    );

    std::cout << "   Building baseline with normal values (around 50):" << std::endl;

    // Random number generator for realistic simulation
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<> normal_dist(50.0, 5.0);  // mean=50, stddev=5

    // Feed normal values to build baseline
    for (int i = 0; i < 15; ++i) {
        double val = normal_dist(gen);
        bool triggered = anomaly_trigger_ptr->evaluate(val);
        std::cout << "    Sample " << (i + 1) << ": value=" << std::fixed
                  << std::setprecision(1) << val
                  << " | Anomaly: " << (triggered ? "YES" : "NO") << std::endl;
    }

    std::cout << std::endl;
    std::cout << "   Current baseline - Mean: " << std::fixed << std::setprecision(2)
              << anomaly_trigger_ptr->current_mean()
              << ", StdDev: " << anomaly_trigger_ptr->current_stddev() << std::endl;
    std::cout << std::endl;

    // Now introduce anomalous values
    std::cout << "   Introducing anomalous values:" << std::endl;
    std::vector<double> anomalous_values = {80.0, 20.0, 100.0, 52.0};  // Mix of anomalies and normal
    for (double val : anomalous_values) {
        bool triggered = anomaly_trigger_ptr->evaluate(val);
        std::cout << "    Value: " << val << " | Anomaly: " << (triggered ? "YES" : "NO")
                  << " (>" << (anomaly_trigger_ptr->current_stddev() * 2)
                  << " from mean " << anomaly_trigger_ptr->current_mean() << ")" << std::endl;
    }
    std::cout << std::endl;

    // =========================================================================
    // Section 5: CompositeTrigger - Logical Combinations
    // =========================================================================
    std::cout << "5. Composite Triggers (AND/OR/NOT)" << std::endl;
    std::cout << "   --------------------------------" << std::endl;

    // Create individual triggers for composition
    auto cpu_high = threshold_trigger::above(80.0);
    auto memory_high = threshold_trigger::above(90.0);
    auto disk_high = threshold_trigger::above(85.0);

    // AND: All conditions must be true
    auto all_high = composite_trigger::all_of({cpu_high, memory_high, disk_high});

    // OR: Any condition can trigger
    auto any_high = composite_trigger::any_of({cpu_high, memory_high, disk_high});

    // NOT: Inverts a trigger
    auto cpu_not_high = composite_trigger::invert(threshold_trigger::above(80.0));

    // XOR: Exactly one condition true
    auto xor_trigger = std::make_shared<composite_trigger>(
        composite_operation::XOR,
        std::vector<std::shared_ptr<alert_trigger>>{cpu_high, memory_high}
    );

    std::cout << "   Composite trigger descriptions:" << std::endl;
    std::cout << "    - ALL (AND): " << all_high->description() << std::endl;
    std::cout << "    - ANY (OR): " << any_high->description() << std::endl;
    std::cout << "    - NOT: " << cpu_not_high->description() << std::endl;
    std::cout << "    - XOR: " << xor_trigger->description() << std::endl;
    std::cout << std::endl;

    // Test with different value combinations
    struct TestCase {
        double cpu;
        double memory;
        double disk;
    };

    std::vector<TestCase> composite_tests = {
        {50.0, 50.0, 50.0},   // All low
        {85.0, 50.0, 50.0},   // Only CPU high
        {85.0, 95.0, 50.0},   // CPU and memory high
        {85.0, 95.0, 90.0},   // All high
    };

    std::cout << "   Testing composite triggers:" << std::endl;
    for (const auto& tc : composite_tests) {
        std::cout << "    CPU=" << tc.cpu << ", Memory=" << tc.memory
                  << ", Disk=" << tc.disk << std::endl;

        // For multi-value evaluation
        std::vector<double> values = {tc.cpu, tc.memory, tc.disk};

        // Note: evaluate() uses same value for all; evaluate_multi() uses individual values
        bool all_result = all_high->evaluate_multi(values);
        bool any_result = any_high->evaluate_multi(values);
        bool not_result = cpu_not_high->evaluate(tc.cpu);
        bool xor_result = xor_trigger->evaluate_multi({tc.cpu, tc.memory});

        std::cout << "      ALL: " << (all_result ? "YES" : "NO")
                  << " | ANY: " << (any_result ? "YES" : "NO")
                  << " | NOT(cpu>80): " << (not_result ? "YES" : "NO")
                  << " | XOR(cpu,mem): " << (xor_result ? "YES" : "NO") << std::endl;
    }
    std::cout << std::endl;

    // =========================================================================
    // Section 6: DeltaTrigger - Change Detection
    // =========================================================================
    std::cout << "6. Delta Trigger (Change Detection)" << std::endl;
    std::cout << "   ----------------------------------" << std::endl;

    // Trigger on any change > 10 (absolute)
    auto delta_absolute = std::make_shared<delta_trigger>(10.0, true);

    // Trigger on positive change > 5
    auto delta_positive = std::make_shared<delta_trigger>(5.0, false);

    std::cout << "   Testing delta triggers with sequential values:" << std::endl;
    std::vector<double> delta_values = {50.0, 52.0, 55.0, 70.0, 68.0, 55.0};

    for (size_t i = 0; i < delta_values.size(); ++i) {
        double val = delta_values[i];
        bool abs_triggered = delta_absolute->evaluate(val);
        bool pos_triggered = delta_positive->evaluate(val);

        std::cout << "    Value: " << val;
        if (i > 0) {
            std::cout << " (delta=" << (val - delta_values[i-1]) << ")";
        }
        std::cout << " | Absolute(>10): " << (abs_triggered ? "YES" : "NO")
                  << " | Positive(>5): " << (pos_triggered ? "YES" : "NO") << std::endl;
    }
    std::cout << std::endl;

    // =========================================================================
    // Section 7: AbsentTrigger - Missing Data Detection
    // =========================================================================
    std::cout << "7. Absent Trigger (Missing Data)" << std::endl;
    std::cout << "   ------------------------------" << std::endl;

    // Trigger if no data received for 200ms
    auto absent_trigger_ptr = std::make_shared<absent_trigger>(200ms);

    std::cout << "   " << absent_trigger_ptr->description() << std::endl;
    std::cout << "   Simulating data with gaps:" << std::endl;

    // Regular data
    for (int i = 0; i < 3; ++i) {
        bool triggered = absent_trigger_ptr->evaluate(100.0);
        std::cout << "    Evaluation " << (i + 1) << " (immediate): "
                  << (triggered ? "ABSENT" : "present") << std::endl;
        std::this_thread::sleep_for(50ms);
    }

    // Simulate gap in data
    std::cout << "    ... waiting 300ms (simulating data gap) ..." << std::endl;
    std::this_thread::sleep_for(300ms);

    bool after_gap = absent_trigger_ptr->evaluate(100.0);
    std::cout << "    Evaluation after gap: " << (after_gap ? "ABSENT" : "present") << std::endl;
    std::cout << std::endl;

    // =========================================================================
    // Section 8: Custom Trigger Implementation
    // =========================================================================
    std::cout << "8. Custom Trigger Implementations" << std::endl;
    std::cout << "   -------------------------------" << std::endl;

    // Periodic trigger - fires every 3rd evaluation
    auto periodic = std::make_shared<periodic_trigger>(3);
    std::cout << "   Periodic trigger: " << periodic->description() << std::endl;

    for (int i = 1; i <= 9; ++i) {
        bool triggered = periodic->evaluate(0);  // Value doesn't matter
        std::cout << "    Evaluation " << i << ": " << (triggered ? "FIRE" : "-") << std::endl;
    }
    std::cout << std::endl;

    // Moving average trigger
    auto ma_trigger = std::make_shared<moving_average_trigger>(5, 60.0);
    std::cout << "   Moving average trigger: " << ma_trigger->description() << std::endl;

    std::vector<double> ma_values = {50.0, 55.0, 60.0, 65.0, 70.0, 75.0, 80.0};
    for (size_t i = 0; i < ma_values.size(); ++i) {
        double val = ma_values[i];
        bool triggered = ma_trigger->evaluate(val);
        std::cout << "    Value: " << val << " | MA(5)=" << std::fixed << std::setprecision(1)
                  << ma_trigger->current_average()
                  << " | Triggered: " << (triggered ? "YES" : "NO") << std::endl;
    }
    std::cout << std::endl;

    // =========================================================================
    // Section 9: Combining Triggers with Alert Rules
    // =========================================================================
    std::cout << "9. Using Triggers with Alert Rules" << std::endl;
    std::cout << "   ---------------------------------" << std::endl;

    // Create an alert rule with a composite trigger
    alert_rule complex_rule("complex_system_alert");
    complex_rule.set_metric_name("system_health")
                .set_severity(alert_severity::critical)
                .set_summary("System health degraded")
                .set_description("Multiple system metrics exceeded thresholds")
                .add_label("team", "ops")
                .add_label("priority", "p1");

    // Complex trigger: (CPU > 80 AND Memory > 85) OR Disk > 95
    auto cpu_mem_trigger = composite_trigger::all_of({
        threshold_trigger::above(80.0),
        threshold_trigger::above(85.0)
    });

    auto disk_critical = threshold_trigger::above(95.0);

    auto complex_composite = composite_trigger::any_of({
        cpu_mem_trigger,
        disk_critical
    });

    complex_rule.set_trigger(complex_composite);

    // Validate rule
    if (auto result = complex_rule.validate(); result.is_ok()) {
        std::cout << "   Rule '" << complex_rule.name() << "' validated successfully" << std::endl;
        std::cout << "   Trigger type: " << complex_rule.trigger()->type_name() << std::endl;
        std::cout << "   Description: " << complex_rule.trigger()->description() << std::endl;
    }
    std::cout << std::endl;

    // =========================================================================
    // Summary
    // =========================================================================
    std::cout << "=== Alert Triggers Example Completed ===" << std::endl;
    std::cout << std::endl;
    std::cout << "Triggers demonstrated:" << std::endl;
    std::cout << "  - ThresholdTrigger (>, >=, <, <=, ==, !=)" << std::endl;
    std::cout << "  - RangeTrigger (in_range, out_of_range)" << std::endl;
    std::cout << "  - RateOfChangeTrigger (increasing, decreasing, either)" << std::endl;
    std::cout << "  - AnomalyTrigger (statistical deviation)" << std::endl;
    std::cout << "  - CompositeTrigger (AND, OR, XOR, NOT)" << std::endl;
    std::cout << "  - DeltaTrigger (change detection)" << std::endl;
    std::cout << "  - AbsentTrigger (missing data)" << std::endl;
    std::cout << "  - Custom triggers (periodic, moving average)" << std::endl;

    return 0;
}
