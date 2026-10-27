#include "pch.h"
#include "../src/domain/autoplaylist/AutoplaylistConfig.h"
#include <functional>

namespace {
using namespace managed_autoplaylist;

class Host final : public IUpdateHost {
public:
    bool current = true;
    bool flags = false;
    bool failRead = false;
    bool failCompile = false;
    bool failSet = false;
    bool applyBeforeSetFailure = false;
    bool failNotify = false;
    int setters = 0;
    int compiles = 0;
    int publishes = 0;
    int notifications = 0;
    std::vector<Snapshot> changes;
    std::function<void()> duringCompile;
    std::function<void()> duringSet;
    std::function<void()> duringNotify;
    bool IsCurrent() override { return current; }
    bool ReadKeepSorted() override {
        if (failRead) throw std::runtime_error("read failed");
        return flags;
    }
    void SetKeepSorted(bool value) override {
        ++setters;
        if (applyBeforeSetFailure || !failSet) flags = value;
        if (duringSet) duringSet();
        if (failSet) throw std::runtime_error("set failed");
    }
    std::shared_ptr<const CompiledConfig> Compile(const Config&) override {
        ++compiles;
        if (duringCompile) duringCompile();
        if (failCompile) throw ValidationError("query", "bad query");
        return std::make_shared<CompiledConfig>();
    }
    void Publish(const Config&, std::shared_ptr<const CompiledConfig>) override { ++publishes; }
    void Notify() override {
        ++notifications;
        if (duringNotify) duringNotify();
        if (failNotify) throw std::runtime_error("notify failed");
    }
    void Changed(const Snapshot& snapshot) override { changes.push_back(snapshot); }
};

class AutoplaylistConfigTest : public ::testing::Test {
protected:
    ConfigState state{{"ALL", "%path%", false}};
    Host host;
    Snapshot Read() { return std::get<Snapshot>(state.Read(host)); }
    Outcome Update(Patch patch) { const auto before = Read(); return state.Update(patch, before.revision, host); }
};

TEST(AutoplaylistConfigValidation, RejectsEmptyBlankNulAndInvalidUtf8Queries) {
    for (const auto& query : {std::string(), std::string(" \t\r\n\v\f"), std::string("ALL\0x", 5),
                             std::string("\xc0\xaf", 2), std::string("\xed\xa0\x80", 3),
                             std::string("\xf4\x90\x80\x80", 4), std::string("\xe2\x82", 2)}) {
        EXPECT_THROW(ValidateText(query, "query", true), ValidationError);
    }
}

TEST(AutoplaylistConfigValidation, ValidatesByteLimitsAndSortAndNameText) {
    for (const auto* param : {"query", "sort", "name"}) {
        EXPECT_NO_THROW(ValidateText(std::string(MaxTextBytes, 'a'), param));
        EXPECT_THROW(ValidateText(std::string(MaxTextBytes + 1, 'a'), param), ValidationError);
        EXPECT_THROW(ValidateText(std::string("a\0b", 3), param), ValidationError);
        EXPECT_THROW(ValidateText(std::string("\xff", 1), param), ValidationError);
    }
    EXPECT_NO_THROW(ValidateText("\xe4\xb8\xad ALL ", "query", true));
}

TEST(AutoplaylistConfigValidation, PatchPreservesAbsentFieldsAndAcceptsEmptySortAndFalse) {
    const Config original{" ALL ", "%path%", true};
    const auto candidate = ApplyPatch(original, {std::nullopt, "", false});
    EXPECT_EQ(candidate.query, " ALL ");
    EXPECT_EQ(candidate.sort, "");
    EXPECT_FALSE(candidate.keepSorted);
    EXPECT_THROW(ApplyPatch(original, {}), ValidationError);
}

TEST(AutoplaylistConfigPersistence, RoundTripsTextWithoutFlagsOrRevision) {
    const auto restored = DecodeConfiguration(EncodeConfiguration({" ALL ", "%path%", true}));
    EXPECT_EQ(restored.query, " ALL ");
    EXPECT_EQ(restored.sort, "%path%");
    EXPECT_FALSE(restored.keepSorted);
}

TEST(AutoplaylistConfigPersistence, RejectsEveryTruncatedPrefixAndUnknownFormat) {
    auto encoded = EncodeConfiguration({"ALL", "", false});
    ASSERT_GT(encoded.size(), 8u);
    for (size_t size = 0; size < encoded.size(); ++size) {
        const std::vector<uint8_t> truncated(encoded.begin(), encoded.begin() + size);
        EXPECT_THROW(DecodeConfiguration(truncated), ValidationError);
    }
    encoded[4] = 0xff;
    EXPECT_THROW(DecodeConfiguration(encoded), ValidationError);
}

TEST_F(AutoplaylistConfigTest, FirstFlagsObservationIsBaselineAndInstancesHaveDistinctRevisions) {
    const auto first = Read();
    EXPECT_EQ(first.revision, Read().revision);
    EXPECT_TRUE(host.changes.empty());
    ConfigState second{{"ALL", "%path%", false}};
    EXPECT_NE(first.revision, std::get<Snapshot>(second.Read(host)).revision);
}

TEST_F(AutoplaylistConfigTest, NewSessionChangesRevisionAndPreservesPublishedText) {
    const auto before = Read();
    state.BeginSession();
    const auto after = Read();
    EXPECT_NE(after.revision, before.revision);
    EXPECT_EQ(after.config.query, before.config.query);
    EXPECT_EQ(after.config.sort, before.config.sort);
    EXPECT_TRUE(host.changes.empty());
}

TEST_F(AutoplaylistConfigTest, NewSessionRejectsOldRevisionBeforeAnyRead) {
    const auto before = Read();
    state.BeginSession();
    const auto outcome = state.Update({"artist HAS replacement", {}, true}, before.revision, host);
    ASSERT_TRUE(std::holds_alternative<Failure>(outcome));
    const auto failure = std::get<Failure>(outcome);
    EXPECT_EQ(failure.reason, "revisionConflict");
    ASSERT_TRUE(failure.current);
    EXPECT_NE(failure.current->revision, before.revision);
    EXPECT_EQ(failure.current->config.query, before.config.query);
    EXPECT_EQ(host.compiles, 0);
    EXPECT_EQ(host.setters, 0);
    EXPECT_EQ(host.publishes, 0);
    EXPECT_EQ(host.notifications, 0);
    EXPECT_TRUE(host.changes.empty());
}

TEST_F(AutoplaylistConfigTest, NewSessionStartsFlagsBaselineWithoutReportingAChange) {
    const auto before = Read();
    state.BeginSession();
    host.flags = true;
    const auto baseline = Read();
    EXPECT_NE(baseline.revision, before.revision);
    EXPECT_TRUE(baseline.config.keepSorted);
    EXPECT_TRUE(host.changes.empty());
    EXPECT_EQ(Read().revision, baseline.revision);
    const auto noChange = state.Update({"ALL", "%path%", true}, baseline.revision, host);
    ASSERT_TRUE(std::holds_alternative<Snapshot>(noChange));
    EXPECT_EQ(std::get<Snapshot>(noChange).revision, baseline.revision);
    EXPECT_EQ(host.setters, 0);
    EXPECT_EQ(host.notifications, 0);
}

TEST_F(AutoplaylistConfigTest, ConflictCarriesCurrentWithoutWrites) {
    const auto before = Read();
    auto outcome = state.Update({"artist HAS test", {}, {}}, "obsolete", host);
    ASSERT_TRUE(std::holds_alternative<Failure>(outcome));
    const auto failure = std::get<Failure>(outcome);
    EXPECT_EQ(failure.reason, "revisionConflict");
    ASSERT_TRUE(failure.current);
    EXPECT_EQ(failure.current->revision, before.revision);
    EXPECT_EQ(host.setters, 0);
    EXPECT_EQ(host.compiles, 0);
}

TEST_F(AutoplaylistConfigTest, SameValueDoesNotCompileSetNotifyOrRevise) {
    const auto before = Read();
    const auto after = std::get<Snapshot>(Update({"ALL", "%path%", false}));
    EXPECT_EQ(after.revision, before.revision);
    EXPECT_EQ(host.compiles, 0);
    EXPECT_EQ(host.setters, 0);
    EXPECT_EQ(host.notifications, 0);
}

TEST_F(AutoplaylistConfigTest, TextOnlyAndCombinedChangesEachCommitOneRevision) {
    const auto before = Read();
    const auto text = std::get<Snapshot>(Update({" ALL ", {}, {}}));
    EXPECT_NE(text.revision, before.revision);
    EXPECT_EQ(text.config.query, " ALL ");
    EXPECT_EQ(host.setters, 0);
    const auto combined = std::get<Snapshot>(Update({"ALL", "", true}));
    EXPECT_NE(combined.revision, text.revision);
    EXPECT_EQ(host.changes.size(), 2u);
    EXPECT_EQ(Read().revision, combined.revision);
    EXPECT_EQ(host.changes.size(), 2u);
}

TEST_F(AutoplaylistConfigTest, ExternalFlagsChangesAreObservedOnce) {
    const auto before = Read();
    host.flags = true;
    const auto after = Read();
    EXPECT_NE(after.revision, before.revision);
    EXPECT_EQ(host.changes.size(), 1u);
    EXPECT_EQ(after.revision, Read().revision);
    EXPECT_EQ(host.changes.size(), 1u);
}

TEST_F(AutoplaylistConfigTest, CandidateCompileFailureDoesNotWrite) {
    host.failCompile = true;
    const auto outcome = Update({"invalid", {}, true});
    ASSERT_TRUE(std::holds_alternative<Failure>(outcome));
    const auto failure = std::get<Failure>(outcome);
    EXPECT_TRUE(failure.invalidParams);
    EXPECT_EQ(failure.param, "query");
    EXPECT_EQ(host.setters, 0);
    EXPECT_EQ(state.Published().query, "ALL");
}

TEST_F(AutoplaylistConfigTest, FlagsSetterFailureReportsConfirmedUnchanged) {
    host.failSet = true;
    const auto outcome = Update({"artist HAS test", {}, true});
    ASSERT_TRUE(std::holds_alternative<Failure>(outcome));
    const auto failure = std::get<Failure>(outcome);
    EXPECT_EQ(failure.state, "unchanged");
    ASSERT_TRUE(failure.current);
    EXPECT_FALSE(failure.current->config.keepSorted);
    EXPECT_EQ(host.publishes, 0);
    EXPECT_TRUE(host.changes.empty());
}

TEST_F(AutoplaylistConfigTest, FlagsSetterFailureReportsActualPartialChangeAndEvent) {
    host.failSet = true;
    host.applyBeforeSetFailure = true;
    const auto before = Read();
    const auto outcome = state.Update({"artist HAS test", {}, true}, before.revision, host);
    ASSERT_TRUE(std::holds_alternative<Failure>(outcome));
    const auto failure = std::get<Failure>(outcome);
    EXPECT_EQ(failure.state, "changed");
    ASSERT_TRUE(failure.current);
    EXPECT_TRUE(failure.current->config.keepSorted);
    EXPECT_EQ(failure.current->config.query, "ALL");
    EXPECT_NE(failure.current->revision, before.revision);
    EXPECT_EQ(host.changes.size(), 1u);
    EXPECT_EQ(Read().revision, failure.current->revision);
    EXPECT_EQ(host.changes.size(), 1u);
}

TEST_F(AutoplaylistConfigTest, ReadbackFailureHasNoCurrentAndNoInventedEvent) {
    host.duringSet = [&] { host.failRead = true; };
    const auto outcome = Update({"artist HAS test", {}, true});
    ASSERT_TRUE(std::holds_alternative<Failure>(outcome));
    const auto failure = std::get<Failure>(outcome);
    EXPECT_EQ(failure.state, "unknown");
    EXPECT_FALSE(failure.current);
    EXPECT_TRUE(host.changes.empty());
    EXPECT_EQ(host.publishes, 0);
}

TEST_F(AutoplaylistConfigTest, NotificationFailureRetainsPublishedTextAndEmitsOnce) {
    host.failNotify = true;
    const auto outcome = Update({"artist HAS test", {}, {}});
    ASSERT_TRUE(std::holds_alternative<Failure>(outcome));
    const auto failure = std::get<Failure>(outcome);
    EXPECT_EQ(failure.state, "changed");
    ASSERT_TRUE(failure.current);
    EXPECT_EQ(failure.current->config.query, "artist HAS test");
    EXPECT_EQ(host.changes.size(), 1u);
}

TEST_F(AutoplaylistConfigTest, TargetOrProviderChangePreventsFurtherPublication) {
    host.duringSet = [&] { host.current = false; };
    const auto outcome = Update({"artist HAS test", {}, true});
    ASSERT_TRUE(std::holds_alternative<Failure>(outcome));
    const auto failure = std::get<Failure>(outcome);
    EXPECT_EQ(failure.state, "unknown");
    EXPECT_FALSE(failure.current);
    EXPECT_EQ(host.publishes, 0);
    EXPECT_TRUE(host.changes.empty());
}

TEST_F(AutoplaylistConfigTest, IdentityChangeDuringCompilePreventsHostWrites) {
    host.duringCompile = [&] { host.current = false; };
    const auto outcome = Update({"artist HAS test", {}, true});
    ASSERT_TRUE(std::holds_alternative<Failure>(outcome));
    EXPECT_EQ(std::get<Failure>(outcome).state, "unknown");
    EXPECT_EQ(host.setters, 0);
}

TEST_F(AutoplaylistConfigTest, ReentrantApisFailWhileNativePublishedTextRemainsReadable) {
    const auto before = Read();
    host.duringSet = [&] {
        const auto read = state.Read(host);
        ASSERT_TRUE(std::holds_alternative<Failure>(read));
        EXPECT_EQ(std::get<Failure>(read).reason, "updateInProgress");
        const auto write = state.Update({"ALL", {}, {}}, before.revision, host);
        ASSERT_TRUE(std::holds_alternative<Failure>(write));
        EXPECT_EQ(std::get<Failure>(write).reason, "updateInProgress");
        EXPECT_EQ(state.Published().query, "ALL");
    };
    host.duringNotify = [&] { EXPECT_EQ(state.Published().query, "artist HAS test"); };
    const auto outcome = state.Update({"artist HAS test", {}, true}, before.revision, host);
    ASSERT_TRUE(std::holds_alternative<Snapshot>(outcome));
    EXPECT_EQ(std::get<Snapshot>(outcome).config.query, "artist HAS test");
    EXPECT_FALSE(state.Busy());
}

} // namespace
