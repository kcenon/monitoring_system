/**
 * BSD 3-Clause License
 * Copyright (c) 2021-2025, kcenon
 *
 * Alert Triggers Tests
 *
 * Tests for alert_triggers.h covering:
 * - threshold_trigger (all comparison operators, factory methods)
 * - range_trigger (inside/outside range)
 * - rate_of_change_trigger (increasing/decreasing/either, window, reset)
 * - anomaly_trigger (z-score, statistics, reset)
 * - composite_trigger (AND/OR/XOR/NOT, evaluate_multi, factory methods)
 * - absent_trigger (gap detection, reset)
 * - delta_trigger (absolute/signed, reset)
 */

#include <gtest/gtest.h>
#include <kcenon/monitoring/alert/alert_triggers.h>

#include <chrono>
#include <cmath>
#include <memory>
#include <thread>
#include <vector>

using namespace kcenon::monitoring;
using namespace std::chrono_literals;

// =============================================================================
// threshold_trigger Tests
// =============================================================================

class ThresholdTriggerTest : public ::testing::Test {
protected:
    std::shared_ptr<threshold_trigger> trigger_;
};

TEST_F(ThresholdTriggerTest, GreaterThan) {
    trigger_ = std::make_shared<threshold_trigger>(80.0, comparison_operator::greater_than);
    EXPECT_TRUE(trigger_->evaluate(81.0));
    EXPECT_FALSE(trigger_->evaluate(80.0));
    EXPECT_FALSE(trigger_->evaluate(79.0));
}

TEST_F(ThresholdTriggerTest, GreaterOrEqual) {
    trigger_ = std::make_shared<threshold_trigger>(80.0, comparison_operator::greater_or_equal);
    EXPECT_TRUE(trigger_->evaluate(81.0));
    EXPECT_TRUE(trigger_->evaluate(80.0));
    EXPECT_FALSE(trigger_->evaluate(79.0));
}

TEST_F(ThresholdTriggerTest, LessThan) {
    trigger_ = std::make_shared<threshold_trigger>(20.0, comparison_operator::less_than);
    EXPECT_TRUE(trigger_->evaluate(19.0));
    EXPECT_FALSE(trigger_->evaluate(20.0));
    EXPECT_FALSE(trigger_->evaluate(21.0));
}

TEST_F(ThresholdTriggerTest, LessOrEqual) {
    trigger_ = std::make_shared<threshold_trigger>(20.0, comparison_operator::less_or_equal);
    EXPECT_TRUE(trigger_->evaluate(19.0));
    EXPECT_TRUE(trigger_->evaluate(20.0));
    EXPECT_FALSE(trigger_->evaluate(21.0));
}

TEST_F(ThresholdTriggerTest, Equal) {
    trigger_ = std::make_shared<threshold_trigger>(50.0, comparison_operator::equal);
    EXPECT_TRUE(trigger_->evaluate(50.0));
    EXPECT_TRUE(trigger_->evaluate(50.0 + 1e-10)); // Within epsilon
    EXPECT_FALSE(trigger_->evaluate(50.1));
}

TEST_F(ThresholdTriggerTest, NotEqual) {
    trigger_ = std::make_shared<threshold_trigger>(50.0, comparison_operator::not_equal);
    EXPECT_FALSE(trigger_->evaluate(50.0));
    EXPECT_TRUE(trigger_->evaluate(50.1));
    EXPECT_TRUE(trigger_->evaluate(49.9));
}

TEST_F(ThresholdTriggerTest, FactoryAbove) {
    auto t = threshold_trigger::above(80.0);
    EXPECT_TRUE(t->evaluate(81.0));
    EXPECT_FALSE(t->evaluate(80.0));
    EXPECT_EQ(t->threshold(), 80.0);
    EXPECT_EQ(t->op(), comparison_operator::greater_than);
}

TEST_F(ThresholdTriggerTest, FactoryBelow) {
    auto t = threshold_trigger::below(20.0);
    EXPECT_TRUE(t->evaluate(19.0));
    EXPECT_FALSE(t->evaluate(20.0));
}

TEST_F(ThresholdTriggerTest, FactoryAboveOrEqual) {
    auto t = threshold_trigger::above_or_equal(80.0);
    EXPECT_TRUE(t->evaluate(80.0));
    EXPECT_FALSE(t->evaluate(79.9));
}

TEST_F(ThresholdTriggerTest, FactoryBelowOrEqual) {
    auto t = threshold_trigger::below_or_equal(20.0);
    EXPECT_TRUE(t->evaluate(20.0));
    EXPECT_FALSE(t->evaluate(20.1));
}

TEST_F(ThresholdTriggerTest, TypeName) {
    trigger_ = threshold_trigger::above(80.0);
    EXPECT_EQ(trigger_->type_name(), "threshold");
}

TEST_F(ThresholdTriggerTest, Description) {
    trigger_ = threshold_trigger::above(80.0);
    auto desc = trigger_->description();
    EXPECT_FALSE(desc.empty());
    EXPECT_NE(desc.find(">"), std::string::npos);
}

TEST_F(ThresholdTriggerTest, NegativeThreshold) {
    trigger_ = threshold_trigger::below(-10.0);
    EXPECT_TRUE(trigger_->evaluate(-11.0));
    EXPECT_FALSE(trigger_->evaluate(-9.0));
}

TEST_F(ThresholdTriggerTest, ZeroThreshold) {
    trigger_ = threshold_trigger::above(0.0);
    EXPECT_TRUE(trigger_->evaluate(0.001));
    EXPECT_FALSE(trigger_->evaluate(0.0));
    EXPECT_FALSE(trigger_->evaluate(-1.0));
}

// =============================================================================
// range_trigger Tests
// =============================================================================

class RangeTriggerTest : public ::testing::Test {};

TEST_F(RangeTriggerTest, InsideRange) {
    auto trigger = std::make_shared<range_trigger>(10.0, 90.0, true);
    EXPECT_TRUE(trigger->evaluate(50.0));
    EXPECT_TRUE(trigger->evaluate(10.0));  // inclusive
    EXPECT_TRUE(trigger->evaluate(90.0));  // inclusive
    EXPECT_FALSE(trigger->evaluate(9.9));
    EXPECT_FALSE(trigger->evaluate(90.1));
}

TEST_F(RangeTriggerTest, OutsideRange) {
    auto trigger = std::make_shared<range_trigger>(10.0, 90.0, false);
    EXPECT_TRUE(trigger->evaluate(5.0));
    EXPECT_TRUE(trigger->evaluate(95.0));
    EXPECT_FALSE(trigger->evaluate(50.0));
    EXPECT_FALSE(trigger->evaluate(10.0));
    EXPECT_FALSE(trigger->evaluate(90.0));
}

TEST_F(RangeTriggerTest, FactoryInRange) {
    auto trigger = threshold_trigger::in_range(20.0, 80.0);
    EXPECT_TRUE(trigger->evaluate(50.0));
    EXPECT_FALSE(trigger->evaluate(10.0));
}

TEST_F(RangeTriggerTest, FactoryOutOfRange) {
    auto trigger = threshold_trigger::out_of_range(20.0, 80.0);
    EXPECT_TRUE(trigger->evaluate(10.0));
    EXPECT_TRUE(trigger->evaluate(90.0));
    EXPECT_FALSE(trigger->evaluate(50.0));
}

TEST_F(RangeTriggerTest, TypeNameAndDescription) {
    auto trigger = std::make_shared<range_trigger>(10.0, 90.0, true);
    EXPECT_EQ(trigger->type_name(), "range");
    auto desc = trigger->description();
    EXPECT_NE(desc.find("in"), std::string::npos);
}

TEST_F(RangeTriggerTest, OutsideRangeDescription) {
    auto trigger = std::make_shared<range_trigger>(10.0, 90.0, false);
    auto desc = trigger->description();
    EXPECT_NE(desc.find("outside"), std::string::npos);
}

// =============================================================================
// rate_of_change_trigger Tests
// =============================================================================

class RateOfChangeTriggerTest : public ::testing::Test {};

TEST_F(RateOfChangeTriggerTest, InsufficientSamplesDoesNotFire) {
    auto trigger = std::make_shared<rate_of_change_trigger>(
        10.0, 1000ms, rate_of_change_trigger::rate_direction::either, 3);
    // Only 1 sample
    EXPECT_FALSE(trigger->evaluate(50.0));
    // Only 2 samples
    EXPECT_FALSE(trigger->evaluate(60.0));
}

TEST_F(RateOfChangeTriggerTest, IncreasingRateDetected) {
    auto trigger = std::make_shared<rate_of_change_trigger>(
        5.0, 1000ms, rate_of_change_trigger::rate_direction::increasing, 2);

    trigger->evaluate(0.0);
    std::this_thread::sleep_for(10ms);
    // Large jump should produce high positive rate
    bool fired = trigger->evaluate(100.0);
    // Rate depends on timing, but a jump of 100 in ~10ms over a 1s window
    // should yield a very high rate
    EXPECT_TRUE(fired);
}

TEST_F(RateOfChangeTriggerTest, DecreasingDirection) {
    auto trigger = std::make_shared<rate_of_change_trigger>(
        5.0, 1000ms, rate_of_change_trigger::rate_direction::decreasing, 2);

    trigger->evaluate(100.0);
    std::this_thread::sleep_for(10ms);
    bool fired = trigger->evaluate(0.0);
    EXPECT_TRUE(fired);
}

TEST_F(RateOfChangeTriggerTest, EitherDirection) {
    auto trigger = std::make_shared<rate_of_change_trigger>(
        5.0, 1000ms, rate_of_change_trigger::rate_direction::either, 2);

    trigger->evaluate(50.0);
    std::this_thread::sleep_for(10ms);
    // Large change in either direction
    EXPECT_TRUE(trigger->evaluate(200.0));
}

TEST_F(RateOfChangeTriggerTest, Reset) {
    auto trigger = std::make_shared<rate_of_change_trigger>(
        5.0, 1000ms, rate_of_change_trigger::rate_direction::either, 2);

    trigger->evaluate(0.0);
    trigger->evaluate(100.0);
    trigger->reset();

    // After reset, insufficient samples again
    EXPECT_FALSE(trigger->evaluate(50.0));
}

TEST_F(RateOfChangeTriggerTest, TypeNameAndDescription) {
    auto trigger = std::make_shared<rate_of_change_trigger>(
        10.0, 1000ms, rate_of_change_trigger::rate_direction::increasing);
    EXPECT_EQ(trigger->type_name(), "rate_of_change");
    EXPECT_FALSE(trigger->description().empty());
}

// =============================================================================
// anomaly_trigger Tests
// =============================================================================

class AnomalyTriggerTest : public ::testing::Test {};

TEST_F(AnomalyTriggerTest, InsufficientSamplesDoesNotFire) {
    auto trigger = std::make_shared<anomaly_trigger>(3.0, 100, 10);
    // Feed fewer than min_samples
    for (int i = 0; i < 9; ++i) {
        EXPECT_FALSE(trigger->evaluate(50.0));
    }
}

TEST_F(AnomalyTriggerTest, NormalValuesDoNotFire) {
    auto trigger = std::make_shared<anomaly_trigger>(3.0, 100, 10);
    // Feed stable values
    for (int i = 0; i < 50; ++i) {
        EXPECT_FALSE(trigger->evaluate(50.0 + (i % 3) * 0.1));
    }
}

TEST_F(AnomalyTriggerTest, AnomalousValueFires) {
    auto trigger = std::make_shared<anomaly_trigger>(2.0, 100, 10);
    // Build up history with values around 50
    for (int i = 0; i < 20; ++i) {
        trigger->evaluate(50.0 + (i % 2 == 0 ? 0.5 : -0.5));
    }
    // Now inject a value far from the mean
    bool fired = trigger->evaluate(200.0);
    EXPECT_TRUE(fired);
}

TEST_F(AnomalyTriggerTest, Statistics) {
    auto trigger = std::make_shared<anomaly_trigger>(3.0, 100, 5);
    for (int i = 0; i < 10; ++i) {
        trigger->evaluate(10.0);
    }
    EXPECT_NEAR(trigger->current_mean(), 10.0, 0.01);
    EXPECT_NEAR(trigger->current_stddev(), 0.0, 0.01);
}

TEST_F(AnomalyTriggerTest, Reset) {
    auto trigger = std::make_shared<anomaly_trigger>(3.0, 100, 10);
    for (int i = 0; i < 20; ++i) {
        trigger->evaluate(50.0);
    }
    trigger->reset();
    // After reset, should need min_samples again
    EXPECT_FALSE(trigger->evaluate(200.0));
}

TEST_F(AnomalyTriggerTest, ZeroStddevDoesNotFire) {
    // When all values are the same, stddev = 0, should not fire (avoid division by zero)
    auto trigger = std::make_shared<anomaly_trigger>(3.0, 100, 5);
    for (int i = 0; i < 10; ++i) {
        trigger->evaluate(50.0);
    }
    // Evaluating the same value keeps stddev at 0, guard should return false
    EXPECT_FALSE(trigger->evaluate(50.0));
}

TEST_F(AnomalyTriggerTest, TypeNameAndDescription) {
    auto trigger = std::make_shared<anomaly_trigger>(3.0);
    EXPECT_EQ(trigger->type_name(), "anomaly");
    EXPECT_NE(trigger->description().find("std devs"), std::string::npos);
}

// =============================================================================
// composite_trigger Tests
// =============================================================================

class CompositeTriggerTest : public ::testing::Test {
protected:
    std::shared_ptr<threshold_trigger> high_ = threshold_trigger::above(80.0);
    std::shared_ptr<threshold_trigger> low_ = threshold_trigger::below(20.0);
};

TEST_F(CompositeTriggerTest, AndBothTrue) {
    auto composite = composite_trigger::all_of({high_, low_});
    // Single value evaluated against both: 90 > 80 (true) but 90 < 20 (false)
    EXPECT_FALSE(composite->evaluate(90.0));
}

TEST_F(CompositeTriggerTest, AndEvaluateMulti) {
    auto composite = composite_trigger::all_of({high_, low_});
    // 90 > 80 (true), 10 < 20 (true) => AND = true
    EXPECT_TRUE(composite->evaluate_multi({90.0, 10.0}));
    // 90 > 80 (true), 30 < 20 (false) => AND = false
    EXPECT_FALSE(composite->evaluate_multi({90.0, 30.0}));
}

TEST_F(CompositeTriggerTest, OrAnyTrue) {
    auto composite = composite_trigger::any_of({high_, low_});
    // 90 > 80 = true, so OR = true
    EXPECT_TRUE(composite->evaluate(90.0));
    // 10 < 20 = true, 10 > 80 = false, but OR = true
    EXPECT_TRUE(composite->evaluate(10.0));
    // 50: not > 80 and not < 20
    EXPECT_FALSE(composite->evaluate(50.0));
}

TEST_F(CompositeTriggerTest, XorExactlyOne) {
    auto composite = std::make_shared<composite_trigger>(
        composite_operation::XOR,
        std::vector<std::shared_ptr<alert_trigger>>{high_, low_});

    // 90: high fires (true), low doesn't (false) => XOR = true (exactly 1)
    EXPECT_TRUE(composite->evaluate(90.0));
    // 50: neither fires => XOR = false
    EXPECT_FALSE(composite->evaluate(50.0));
}

TEST_F(CompositeTriggerTest, XorBothTrueIsFalse) {
    auto composite = std::make_shared<composite_trigger>(
        composite_operation::XOR,
        std::vector<std::shared_ptr<alert_trigger>>{high_, low_});

    // Both true via evaluate_multi => XOR = false
    EXPECT_FALSE(composite->evaluate_multi({90.0, 10.0}));
}

TEST_F(CompositeTriggerTest, Not) {
    auto composite = composite_trigger::invert(high_);
    // 90 > 80 = true, NOT = false
    EXPECT_FALSE(composite->evaluate(90.0));
    // 50 > 80 = false, NOT = true
    EXPECT_TRUE(composite->evaluate(50.0));
}

TEST_F(CompositeTriggerTest, EmptyTriggersIsFalse) {
    auto composite = std::make_shared<composite_trigger>(
        composite_operation::AND,
        std::vector<std::shared_ptr<alert_trigger>>{});
    EXPECT_FALSE(composite->evaluate(50.0));
}

TEST_F(CompositeTriggerTest, EvaluateMultiFewerValuesThanTriggers) {
    auto composite = composite_trigger::all_of({high_, low_});
    // Only one value provided: last value repeated for missing triggers
    // 90 > 80 (true), 90 < 20 (false) => AND = false
    EXPECT_FALSE(composite->evaluate_multi({90.0}));
}

TEST_F(CompositeTriggerTest, TriggersAccessor) {
    auto composite = composite_trigger::all_of({high_, low_});
    EXPECT_EQ(composite->triggers().size(), 2u);
}

TEST_F(CompositeTriggerTest, TypeNameAndDescription) {
    auto composite = composite_trigger::all_of({high_, low_});
    EXPECT_EQ(composite->type_name(), "composite");
    auto desc = composite->description();
    EXPECT_NE(desc.find("AND"), std::string::npos);
}

TEST_F(CompositeTriggerTest, NotDescription) {
    auto composite = composite_trigger::invert(high_);
    auto desc = composite->description();
    EXPECT_NE(desc.find("NOT"), std::string::npos);
}

// =============================================================================
// absent_trigger Tests
// =============================================================================

class AbsentTriggerTest : public ::testing::Test {};

TEST_F(AbsentTriggerTest, FirstEvaluationDoesNotFire) {
    auto trigger = std::make_shared<absent_trigger>(100ms);
    EXPECT_FALSE(trigger->evaluate(1.0));
}

TEST_F(AbsentTriggerTest, QuickSecondEvaluationDoesNotFire) {
    auto trigger = std::make_shared<absent_trigger>(100ms);
    trigger->evaluate(1.0);
    // Immediately evaluate again - gap is tiny
    EXPECT_FALSE(trigger->evaluate(2.0));
}

TEST_F(AbsentTriggerTest, GapExceedingDurationFires) {
    auto trigger = std::make_shared<absent_trigger>(50ms);
    trigger->evaluate(1.0);
    std::this_thread::sleep_for(60ms);
    EXPECT_TRUE(trigger->evaluate(2.0));
}

TEST_F(AbsentTriggerTest, ResetClearsState) {
    auto trigger = std::make_shared<absent_trigger>(50ms);
    trigger->evaluate(1.0);
    trigger->reset();
    // After reset, first evaluation again
    EXPECT_FALSE(trigger->evaluate(2.0));
}

TEST_F(AbsentTriggerTest, TypeNameAndDescription) {
    auto trigger = std::make_shared<absent_trigger>(5000ms);
    EXPECT_EQ(trigger->type_name(), "absent");
    auto desc = trigger->description();
    EXPECT_NE(desc.find("no data"), std::string::npos);
}

// =============================================================================
// delta_trigger Tests
// =============================================================================

class DeltaTriggerTest : public ::testing::Test {};

TEST_F(DeltaTriggerTest, FirstEvaluationDoesNotFire) {
    auto trigger = std::make_shared<delta_trigger>(10.0);
    EXPECT_FALSE(trigger->evaluate(50.0));
}

TEST_F(DeltaTriggerTest, AbsoluteSmallChangeDoesNotFire) {
    auto trigger = std::make_shared<delta_trigger>(10.0, true);
    trigger->evaluate(50.0);
    EXPECT_FALSE(trigger->evaluate(55.0)); // |5| <= 10
}

TEST_F(DeltaTriggerTest, AbsoluteLargeChangeFires) {
    auto trigger = std::make_shared<delta_trigger>(10.0, true);
    trigger->evaluate(50.0);
    EXPECT_TRUE(trigger->evaluate(70.0)); // |20| > 10
}

TEST_F(DeltaTriggerTest, AbsoluteNegativeChangeFires) {
    auto trigger = std::make_shared<delta_trigger>(10.0, true);
    trigger->evaluate(50.0);
    EXPECT_TRUE(trigger->evaluate(30.0)); // |-20| > 10
}

TEST_F(DeltaTriggerTest, SignedPositiveChangeOnly) {
    auto trigger = std::make_shared<delta_trigger>(10.0, false);
    trigger->evaluate(50.0);
    // Decrease: delta = -20, not > 10
    EXPECT_FALSE(trigger->evaluate(30.0));
    // Increase: delta = 40, > 10
    EXPECT_TRUE(trigger->evaluate(70.0));
}

TEST_F(DeltaTriggerTest, ContinuousTracking) {
    auto trigger = std::make_shared<delta_trigger>(5.0, true);
    trigger->evaluate(10.0);   // first - no fire
    trigger->evaluate(12.0);   // |2| <= 5 - no fire
    EXPECT_FALSE(trigger->evaluate(14.0));  // |2| <= 5
    EXPECT_TRUE(trigger->evaluate(25.0));   // |11| > 5
    EXPECT_FALSE(trigger->evaluate(27.0));  // |2| <= 5 (relative to 25)
}

TEST_F(DeltaTriggerTest, Reset) {
    auto trigger = std::make_shared<delta_trigger>(5.0, true);
    trigger->evaluate(10.0);
    trigger->evaluate(20.0);
    trigger->reset();
    // After reset, first evaluation again
    EXPECT_FALSE(trigger->evaluate(100.0));
}

TEST_F(DeltaTriggerTest, TypeNameAndDescription) {
    auto trigger = std::make_shared<delta_trigger>(10.0, true);
    EXPECT_EQ(trigger->type_name(), "delta");
    auto desc = trigger->description();
    EXPECT_NE(desc.find("delta"), std::string::npos);
}

TEST_F(DeltaTriggerTest, SignedDescription) {
    auto trigger = std::make_shared<delta_trigger>(10.0, false);
    auto desc = trigger->description();
    // Should NOT contain "|delta|" for signed mode
    EXPECT_EQ(desc.find("|delta|"), std::string::npos);
}

// =============================================================================
// comparison_operator string conversion Tests
// =============================================================================

TEST(ComparisonOperatorTest, AllOperatorsHaveStrings) {
    EXPECT_STREQ(comparison_operator_to_string(comparison_operator::greater_than), ">");
    EXPECT_STREQ(comparison_operator_to_string(comparison_operator::greater_or_equal), ">=");
    EXPECT_STREQ(comparison_operator_to_string(comparison_operator::less_than), "<");
    EXPECT_STREQ(comparison_operator_to_string(comparison_operator::less_or_equal), "<=");
    EXPECT_STREQ(comparison_operator_to_string(comparison_operator::equal), "==");
    EXPECT_STREQ(comparison_operator_to_string(comparison_operator::not_equal), "!=");
}
