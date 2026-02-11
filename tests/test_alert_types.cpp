/**
 * BSD 3-Clause License
 * Copyright (c) 2021-2025, kcenon
 *
 * Alert Types Tests
 *
 * Tests for alert_types.h covering:
 * - Severity and state enums with string conversions
 * - alert_labels (set/get/has/fingerprint/equality)
 * - alert_annotations construction
 * - alert struct (construction, fingerprint, is_active, state transitions)
 * - alert_group (add_alert, size, max_severity)
 * - alert_silence (is_active, matches)
 */

#include <gtest/gtest.h>
#include <kcenon/monitoring/alert/alert_types.h>

#include <chrono>
#include <thread>

using namespace kcenon::monitoring;
using namespace std::chrono_literals;

// =============================================================================
// alert_severity Tests
// =============================================================================

TEST(AlertSeverityTest, ToStringConversions) {
    EXPECT_STREQ(alert_severity_to_string(alert_severity::info), "info");
    EXPECT_STREQ(alert_severity_to_string(alert_severity::warning), "warning");
    EXPECT_STREQ(alert_severity_to_string(alert_severity::critical), "critical");
    EXPECT_STREQ(alert_severity_to_string(alert_severity::emergency), "emergency");
}

TEST(AlertSeverityTest, OrderingByValue) {
    EXPECT_LT(static_cast<uint8_t>(alert_severity::info),
              static_cast<uint8_t>(alert_severity::warning));
    EXPECT_LT(static_cast<uint8_t>(alert_severity::warning),
              static_cast<uint8_t>(alert_severity::critical));
    EXPECT_LT(static_cast<uint8_t>(alert_severity::critical),
              static_cast<uint8_t>(alert_severity::emergency));
}

// =============================================================================
// alert_state Tests
// =============================================================================

TEST(AlertStateTest, ToStringConversions) {
    EXPECT_STREQ(alert_state_to_string(alert_state::inactive), "inactive");
    EXPECT_STREQ(alert_state_to_string(alert_state::pending), "pending");
    EXPECT_STREQ(alert_state_to_string(alert_state::firing), "firing");
    EXPECT_STREQ(alert_state_to_string(alert_state::resolved), "resolved");
    EXPECT_STREQ(alert_state_to_string(alert_state::suppressed), "suppressed");
}

// =============================================================================
// alert_labels Tests
// =============================================================================

class AlertLabelsTest : public ::testing::Test {
protected:
    alert_labels labels_;
};

TEST_F(AlertLabelsTest, DefaultConstructionIsEmpty) {
    EXPECT_TRUE(labels_.labels.empty());
}

TEST_F(AlertLabelsTest, ConstructFromMap) {
    std::unordered_map<std::string, std::string> map{
        {"env", "production"}, {"service", "api"}};
    alert_labels lbl(map);
    EXPECT_EQ(lbl.labels.size(), 2u);
    EXPECT_EQ(lbl.get("env"), "production");
}

TEST_F(AlertLabelsTest, SetAndGet) {
    labels_.set("team", "infra");
    EXPECT_EQ(labels_.get("team"), "infra");
}

TEST_F(AlertLabelsTest, GetNonexistentReturnsEmpty) {
    EXPECT_EQ(labels_.get("missing"), "");
}

TEST_F(AlertLabelsTest, HasExistingKey) {
    labels_.set("region", "us-east");
    EXPECT_TRUE(labels_.has("region"));
    EXPECT_FALSE(labels_.has("zone"));
}

TEST_F(AlertLabelsTest, SetOverwritesExisting) {
    labels_.set("env", "staging");
    labels_.set("env", "production");
    EXPECT_EQ(labels_.get("env"), "production");
}

TEST_F(AlertLabelsTest, FingerprintIsDeterministic) {
    labels_.set("b", "2");
    labels_.set("a", "1");
    auto fp1 = labels_.fingerprint();

    alert_labels other;
    other.set("a", "1");
    other.set("b", "2");
    auto fp2 = other.fingerprint();

    // Sorted order: a=1,b=2, regardless of insertion order
    EXPECT_EQ(fp1, fp2);
}

TEST_F(AlertLabelsTest, FingerprintDiffersForDifferentValues) {
    labels_.set("key", "value1");
    auto fp1 = labels_.fingerprint();

    alert_labels other;
    other.set("key", "value2");
    auto fp2 = other.fingerprint();

    EXPECT_NE(fp1, fp2);
}

TEST_F(AlertLabelsTest, EqualityOperator) {
    labels_.set("a", "1");
    labels_.set("b", "2");

    alert_labels other;
    other.set("a", "1");
    other.set("b", "2");

    EXPECT_EQ(labels_, other);
}

TEST_F(AlertLabelsTest, InequalityWhenDifferent) {
    labels_.set("a", "1");

    alert_labels other;
    other.set("a", "2");

    EXPECT_FALSE(labels_ == other);
}

// =============================================================================
// alert_annotations Tests
// =============================================================================

TEST(AlertAnnotationsTest, DefaultConstruction) {
    alert_annotations ann;
    EXPECT_TRUE(ann.summary.empty());
    EXPECT_TRUE(ann.description.empty());
    EXPECT_FALSE(ann.runbook_url.has_value());
    EXPECT_TRUE(ann.custom.empty());
}

TEST(AlertAnnotationsTest, ConstructWithSummaryAndDescription) {
    alert_annotations ann("High CPU", "CPU usage exceeded 80%");
    EXPECT_EQ(ann.summary, "High CPU");
    EXPECT_EQ(ann.description, "CPU usage exceeded 80%");
}

TEST(AlertAnnotationsTest, RunbookUrl) {
    alert_annotations ann;
    ann.runbook_url = "https://runbooks.example.com/cpu";
    EXPECT_TRUE(ann.runbook_url.has_value());
    EXPECT_EQ(*ann.runbook_url, "https://runbooks.example.com/cpu");
}

TEST(AlertAnnotationsTest, CustomAnnotations) {
    alert_annotations ann;
    ann.custom["dashboard"] = "grafana/cpu";
    EXPECT_EQ(ann.custom.at("dashboard"), "grafana/cpu");
}

// =============================================================================
// alert struct Tests
// =============================================================================

class AlertTest : public ::testing::Test {
protected:
    alert create_test_alert() {
        alert_labels labels;
        labels.set("service", "api");
        labels.set("env", "prod");
        alert a("high_cpu", labels);
        a.severity = alert_severity::critical;
        a.value = 95.0;
        return a;
    }
};

TEST_F(AlertTest, DefaultConstruction) {
    alert a;
    EXPECT_TRUE(a.name.empty());
    EXPECT_EQ(a.state, alert_state::inactive);
    EXPECT_EQ(a.severity, alert_severity::warning);
    EXPECT_EQ(a.value, 0.0);
    EXPECT_FALSE(a.started_at.has_value());
    EXPECT_FALSE(a.resolved_at.has_value());
}

TEST_F(AlertTest, ConstructWithNameAndLabels) {
    auto a = create_test_alert();
    EXPECT_EQ(a.name, "high_cpu");
    EXPECT_EQ(a.labels.get("service"), "api");
    EXPECT_EQ(a.severity, alert_severity::critical);
}

TEST_F(AlertTest, UniqueIds) {
    alert a1;
    alert a2;
    EXPECT_NE(a1.id, a2.id);
}

TEST_F(AlertTest, FingerprintIncludesNameAndLabels) {
    auto a = create_test_alert();
    auto fp = a.fingerprint();
    EXPECT_FALSE(fp.empty());
    EXPECT_NE(fp.find("high_cpu"), std::string::npos);
}

TEST_F(AlertTest, FingerprintConsistency) {
    auto a1 = create_test_alert();
    auto a2 = create_test_alert();
    // Same name + same labels = same fingerprint (for dedup)
    EXPECT_EQ(a1.fingerprint(), a2.fingerprint());
}

TEST_F(AlertTest, IsActiveForPendingAndFiring) {
    alert a;
    EXPECT_FALSE(a.is_active()); // inactive

    a.state = alert_state::pending;
    EXPECT_TRUE(a.is_active());

    a.state = alert_state::firing;
    EXPECT_TRUE(a.is_active());

    a.state = alert_state::resolved;
    EXPECT_FALSE(a.is_active());

    a.state = alert_state::suppressed;
    EXPECT_FALSE(a.is_active());
}

TEST_F(AlertTest, StateDurationIsPositive) {
    alert a;
    // Sleep briefly to ensure non-zero duration
    std::this_thread::sleep_for(1ms);
    auto dur = a.state_duration();
    EXPECT_GT(dur.count(), 0);
}

TEST_F(AlertTest, FiringDurationZeroWhenNotFiring) {
    alert a;
    EXPECT_EQ(a.firing_duration().count(), 0);
}

// =============================================================================
// alert State Transition Tests
// =============================================================================

class AlertTransitionTest : public ::testing::Test {
protected:
    alert a_;
};

TEST_F(AlertTransitionTest, InactiveToLending) {
    EXPECT_TRUE(a_.transition_to(alert_state::pending));
    EXPECT_EQ(a_.state, alert_state::pending);
}

TEST_F(AlertTransitionTest, InactiveToFiringInvalid) {
    EXPECT_FALSE(a_.transition_to(alert_state::firing));
    EXPECT_EQ(a_.state, alert_state::inactive);
}

TEST_F(AlertTransitionTest, InactiveToResolvedInvalid) {
    EXPECT_FALSE(a_.transition_to(alert_state::resolved));
    EXPECT_EQ(a_.state, alert_state::inactive);
}

TEST_F(AlertTransitionTest, PendingToFiring) {
    a_.transition_to(alert_state::pending);
    EXPECT_TRUE(a_.transition_to(alert_state::firing));
    EXPECT_EQ(a_.state, alert_state::firing);
    EXPECT_TRUE(a_.started_at.has_value());
}

TEST_F(AlertTransitionTest, PendingToInactive) {
    a_.transition_to(alert_state::pending);
    EXPECT_TRUE(a_.transition_to(alert_state::inactive));
    EXPECT_EQ(a_.state, alert_state::inactive);
}

TEST_F(AlertTransitionTest, FiringToResolved) {
    a_.transition_to(alert_state::pending);
    a_.transition_to(alert_state::firing);
    EXPECT_TRUE(a_.transition_to(alert_state::resolved));
    EXPECT_EQ(a_.state, alert_state::resolved);
    EXPECT_TRUE(a_.resolved_at.has_value());
}

TEST_F(AlertTransitionTest, FiringToPendingInvalid) {
    a_.transition_to(alert_state::pending);
    a_.transition_to(alert_state::firing);
    EXPECT_FALSE(a_.transition_to(alert_state::pending));
    EXPECT_EQ(a_.state, alert_state::firing);
}

TEST_F(AlertTransitionTest, ResolvedToPending) {
    a_.transition_to(alert_state::pending);
    a_.transition_to(alert_state::firing);
    a_.transition_to(alert_state::resolved);
    EXPECT_TRUE(a_.transition_to(alert_state::pending));
    EXPECT_EQ(a_.state, alert_state::pending);
}

TEST_F(AlertTransitionTest, ResolvedToInactive) {
    a_.transition_to(alert_state::pending);
    a_.transition_to(alert_state::firing);
    a_.transition_to(alert_state::resolved);
    EXPECT_TRUE(a_.transition_to(alert_state::inactive));
}

TEST_F(AlertTransitionTest, AnyStateToSuppressed) {
    EXPECT_TRUE(a_.transition_to(alert_state::suppressed));
    EXPECT_EQ(a_.state, alert_state::suppressed);
}

TEST_F(AlertTransitionTest, SuppressedToAnyState) {
    a_.transition_to(alert_state::suppressed);

    // From suppressed, all transitions should be valid
    EXPECT_TRUE(a_.transition_to(alert_state::firing));
    EXPECT_EQ(a_.state, alert_state::firing);
}

TEST_F(AlertTransitionTest, FullLifecycle) {
    // inactive -> pending -> firing -> resolved -> pending -> firing -> resolved
    EXPECT_TRUE(a_.transition_to(alert_state::pending));
    EXPECT_TRUE(a_.transition_to(alert_state::firing));
    EXPECT_TRUE(a_.transition_to(alert_state::resolved));
    EXPECT_TRUE(a_.transition_to(alert_state::pending));
    EXPECT_TRUE(a_.transition_to(alert_state::firing));
    EXPECT_TRUE(a_.transition_to(alert_state::resolved));
}

TEST_F(AlertTransitionTest, FiringStartedAtSetOnlyOnce) {
    a_.transition_to(alert_state::pending);
    a_.transition_to(alert_state::firing);
    auto first_started = a_.started_at;

    // Resolve and re-fire
    a_.transition_to(alert_state::resolved);
    a_.transition_to(alert_state::pending);
    a_.transition_to(alert_state::firing);

    // started_at should remain the same (first firing)
    EXPECT_EQ(a_.started_at, first_started);
}

TEST_F(AlertTransitionTest, UpdatedAtChangesOnTransition) {
    auto initial_updated = a_.updated_at;
    std::this_thread::sleep_for(1ms);
    a_.transition_to(alert_state::pending);
    EXPECT_GT(a_.updated_at, initial_updated);
}

// =============================================================================
// alert_group Tests
// =============================================================================

class AlertGroupTest : public ::testing::Test {
protected:
    alert_group group_{"test_group"};
};

TEST_F(AlertGroupTest, DefaultConstruction) {
    alert_group g;
    EXPECT_TRUE(g.group_key.empty());
    EXPECT_TRUE(g.empty());
    EXPECT_EQ(g.size(), 0u);
}

TEST_F(AlertGroupTest, ConstructWithKey) {
    EXPECT_EQ(group_.group_key, "test_group");
    EXPECT_TRUE(group_.empty());
}

TEST_F(AlertGroupTest, AddAlert) {
    alert a("test", alert_labels{});
    group_.add_alert(a);
    EXPECT_EQ(group_.size(), 1u);
    EXPECT_FALSE(group_.empty());
}

TEST_F(AlertGroupTest, AddMultipleAlerts) {
    for (int i = 0; i < 5; ++i) {
        alert a("alert_" + std::to_string(i), alert_labels{});
        group_.add_alert(a);
    }
    EXPECT_EQ(group_.size(), 5u);
}

TEST_F(AlertGroupTest, MaxSeverityEmptyGroup) {
    EXPECT_EQ(group_.max_severity(), alert_severity::info);
}

TEST_F(AlertGroupTest, MaxSeveritySingleAlert) {
    alert a;
    a.severity = alert_severity::critical;
    group_.add_alert(a);
    EXPECT_EQ(group_.max_severity(), alert_severity::critical);
}

TEST_F(AlertGroupTest, MaxSeverityMultipleAlerts) {
    alert a1;
    a1.severity = alert_severity::info;
    group_.add_alert(a1);

    alert a2;
    a2.severity = alert_severity::emergency;
    group_.add_alert(a2);

    alert a3;
    a3.severity = alert_severity::warning;
    group_.add_alert(a3);

    EXPECT_EQ(group_.max_severity(), alert_severity::emergency);
}

TEST_F(AlertGroupTest, UpdatedAtChangesOnAdd) {
    auto initial = group_.updated_at;
    std::this_thread::sleep_for(1ms);
    alert a;
    group_.add_alert(a);
    EXPECT_GT(group_.updated_at, initial);
}

// =============================================================================
// alert_silence Tests
// =============================================================================

class AlertSilenceTest : public ::testing::Test {
protected:
    alert_silence silence_;
};

TEST_F(AlertSilenceTest, DefaultConstructionIsActive) {
    // Default: starts_at = now, ends_at = now + 1 hour
    EXPECT_TRUE(silence_.is_active());
}

TEST_F(AlertSilenceTest, UniqueIds) {
    alert_silence s1;
    alert_silence s2;
    EXPECT_NE(s1.id, s2.id);
}

TEST_F(AlertSilenceTest, ExpiredSilenceNotActive) {
    silence_.starts_at = std::chrono::steady_clock::now() - 2h;
    silence_.ends_at = std::chrono::steady_clock::now() - 1h;
    EXPECT_FALSE(silence_.is_active());
}

TEST_F(AlertSilenceTest, FutureSilenceNotActive) {
    silence_.starts_at = std::chrono::steady_clock::now() + 1h;
    silence_.ends_at = std::chrono::steady_clock::now() + 2h;
    EXPECT_FALSE(silence_.is_active());
}

TEST_F(AlertSilenceTest, MatchesAlertWithMatchingLabels) {
    silence_.matchers.set("service", "api");

    alert a;
    a.labels.set("service", "api");
    a.labels.set("env", "prod");

    EXPECT_TRUE(silence_.matches(a));
}

TEST_F(AlertSilenceTest, DoesNotMatchAlertWithDifferentLabels) {
    silence_.matchers.set("service", "api");

    alert a;
    a.labels.set("service", "web");

    EXPECT_FALSE(silence_.matches(a));
}

TEST_F(AlertSilenceTest, DoesNotMatchAlertMissingLabel) {
    silence_.matchers.set("service", "api");

    alert a;
    a.labels.set("env", "prod");

    EXPECT_FALSE(silence_.matches(a));
}

TEST_F(AlertSilenceTest, EmptyMatchersMatchesAll) {
    // No matcher labels means all alerts match
    alert a;
    a.labels.set("anything", "value");
    EXPECT_TRUE(silence_.matches(a));
}

TEST_F(AlertSilenceTest, ExpiredSilenceDoesNotMatch) {
    silence_.matchers.set("service", "api");
    silence_.starts_at = std::chrono::steady_clock::now() - 2h;
    silence_.ends_at = std::chrono::steady_clock::now() - 1h;

    alert a;
    a.labels.set("service", "api");

    EXPECT_FALSE(silence_.matches(a));
}

TEST_F(AlertSilenceTest, MultipleMatchersMustAllMatch) {
    silence_.matchers.set("service", "api");
    silence_.matchers.set("env", "prod");

    alert a1;
    a1.labels.set("service", "api");
    a1.labels.set("env", "prod");
    EXPECT_TRUE(silence_.matches(a1));

    alert a2;
    a2.labels.set("service", "api");
    a2.labels.set("env", "staging");
    EXPECT_FALSE(silence_.matches(a2));
}

TEST_F(AlertSilenceTest, CommentAndCreatedBy) {
    silence_.comment = "Maintenance window";
    silence_.created_by = "admin@example.com";
    EXPECT_EQ(silence_.comment, "Maintenance window");
    EXPECT_EQ(silence_.created_by, "admin@example.com");
}
