// test_jit_native_context.cpp - 原生接管的出现账、来源续播位置与恢复计划
//
// 直接链接生产头 core/JitNativeContext.h。
#include "pch.h"
#include "core/JitNativeContext.h"

#include <string>
#include <vector>

using jit_native::Disposition;
using jit_native::NativeContext;
using jit_native::Status;

namespace {

using Ids = std::vector<std::string>;

// 来源 6 行、当前在第 current 行并已接纳；手动队列三个出现，资源可以相同，引用各不相同。
NativeContext Context(std::uint64_t current = 2, std::uint64_t total = 6) {
    NativeContext context{total, current, {{"a", "manual-a"}, {"b", "manual-b"}, {"c", "manual-c"}}};
    const std::string ref = "source-" + std::to_string(current);
    if (!context.RegisterSource(current, ref) || !context.Adopt(ref, "current-entry")) throw std::logic_error("adopt");
    return context;
}

}  // namespace

// ---------------------------------------------------------------------------
// 出现、引用与来源游标
// ---------------------------------------------------------------------------

TEST(JitNativeContext, EqualResourcesRemainSeparateOccurrences) {
    auto context = Context();
    ASSERT_TRUE(context.Submit("a"));
    ASSERT_TRUE(context.Started("manual-a"));
    EXPECT_EQ(context.Find("a")->disposition, Disposition::Consumed);
    EXPECT_EQ(context.Find("b")->disposition, Disposition::Pending);
    auto plan = context.PlanRestore("recovery");
    ASSERT_TRUE(plan);
    EXPECT_EQ(plan->occurrences, (Ids{"b", "c"}));
}

TEST(JitNativeContext, DuplicateOccurrenceIdsOrReferencesAreRejected) {
    EXPECT_THROW(NativeContext(6, 2, {{"a", "ref"}, {"a", "other"}}), std::invalid_argument);
    EXPECT_THROW(NativeContext(6, 2, {{"a", "ref"}, {"b", "ref"}}), std::invalid_argument);
    EXPECT_THROW(NativeContext(6, 6, {}), std::invalid_argument);
}

TEST(JitNativeContext, PreparedContextOnlyAssignsSourceReferencesUntilAdopted) {
    NativeContext context{6, 2, {{"a", "manual-a"}}};
    EXPECT_EQ(context.GetStatus(), Status::Prepared);
    EXPECT_FALSE(context.Submit("a"));
    EXPECT_FALSE(context.Started("manual-a"));
    EXPECT_FALSE(context.PlanEdit({}));
    EXPECT_FALSE(context.PlanRestore("recovery"));
    EXPECT_FALSE(context.Adopt("source-2", "entry"));  // 当前行的引用尚未登记
    ASSERT_TRUE(context.RegisterSource(2, "source-2"));
    ASSERT_TRUE(context.RegisterSource(5, "source-5"));
    EXPECT_FALSE(context.Adopt("source-5", "entry"));  // 只能接纳快照时的当前行
    EXPECT_FALSE(context.Adopt("source-2", ""));
    ASSERT_TRUE(context.Adopt("source-2", "entry"));
    EXPECT_EQ(context.GetStatus(), Status::Managed);
    EXPECT_FALSE(context.EntryAvailableForLocation("entry"));
    EXPECT_FALSE(context.Adopt("source-2", "entry"));
}

TEST(JitNativeContext, ManualPlaybackDoesNotAdvanceSourceCursor) {
    auto context = Context();
    EXPECT_TRUE(context.Started("manual-a"));
    EXPECT_EQ(context.SourceNext(), 3u);
}

// 续播位置跟随最近一次实际起播的来源行，回退也跟；不取已播过的最大行。
TEST(JitNativeContext, CursorTracksTheLastActualSourceStartIncludingBackwardPlay) {
    auto context = Context();
    ASSERT_TRUE(context.RegisterSource(4, "source-4"));
    ASSERT_TRUE(context.RegisterSource(1, "source-1"));
    EXPECT_EQ(context.SourceNext(), 3u);
    EXPECT_TRUE(context.Started("source-4"));
    EXPECT_EQ(context.SourceNext(), 5u);
    EXPECT_TRUE(context.Started("source-1"));
    EXPECT_EQ(context.SourceNext(), 2u);
    EXPECT_FALSE(context.Started("unknown"));
}

TEST(JitNativeContext, SourceReferenceAndEntryBindingsAreStableAndExclusive) {
    auto context = Context();
    const auto revision = context.Revision();
    EXPECT_TRUE(context.RegisterSource(4, "source-4"));
    EXPECT_TRUE(context.RegisterSource(4, "source-4"));
    EXPECT_EQ(context.Revision(), revision);  // 读页分配引用不改变版本
    EXPECT_EQ(context.SourceRef(4), "source-4");
    EXPECT_FALSE(context.RegisterSource(4, "other-ref"));
    EXPECT_FALSE(context.RegisterSource(9, "outside"));
    EXPECT_FALSE(context.RegisterSource(3, "manual-a"));
    EXPECT_TRUE(context.Bind("manual-a", "entry"));
    EXPECT_TRUE(context.Bind("manual-a", "entry"));
    EXPECT_FALSE(context.Bind("manual-a", "new-entry"));
    EXPECT_FALSE(context.Bind("manual-b", "entry"));
    EXPECT_FALSE(context.Bind("unknown", "free"));
    EXPECT_FALSE(context.EntryAvailableForLocation("entry"));
    EXPECT_TRUE(context.EntryAvailableForLocation("free"));
}

// ---------------------------------------------------------------------------
// 手动项编辑
// ---------------------------------------------------------------------------

TEST(JitNativeContext, WithdrawnQueuedOccurrenceReturnsToPendingOnlyWhileQueued) {
    auto context = Context();
    ASSERT_TRUE(context.Queue("b"));
    EXPECT_TRUE(context.WithdrawQueued("b"));
    EXPECT_EQ(context.Find("b")->disposition, Disposition::Pending);
    EXPECT_FALSE(context.WithdrawQueued("b"));
    ASSERT_TRUE(context.Queue("b"));
    ASSERT_TRUE(context.Submit("b"));  // 原生已取走：撤回不再适用
    EXPECT_FALSE(context.WithdrawQueued("b"));
    EXPECT_EQ(context.OccurrenceOfRef("manual-b")->id, "b");
    EXPECT_EQ(context.OccurrenceOfRef("source-2"), nullptr);
    EXPECT_EQ(context.SourceRowOf("source-2"), 2u);
    EXPECT_FALSE(context.SourceRowOf("manual-b"));
}

TEST(JitNativeContext, OnlyOneOccurrenceCanBeQueuedAtATime) {
    auto context = Context();
    ASSERT_TRUE(context.Queue("b"));
    EXPECT_FALSE(context.Queue("c"));
    EXPECT_FALSE(context.Queue("b"));
}

TEST(JitNativeContext, ReorderingQueuedEntryDoesNotRequireAnotherPhysicalInsertion) {
    auto context = Context();
    ASSERT_TRUE(context.Queue("b"));
    auto edit = context.PlanEdit({"c", "b", "a"});
    ASSERT_TRUE(edit);
    EXPECT_TRUE(edit->withdrawals.empty());
    ASSERT_TRUE(context.CommitEdit(*edit));
    EXPECT_EQ(context.Find("b")->disposition, Disposition::Queued);
    EXPECT_EQ(context.RestorationRank("c"), 0u);
    EXPECT_EQ(context.RestorationRank("a"), 2u);
}

TEST(JitNativeContext, OmittedQueuedEntryMustBePhysicallyWithdrawnBeforeDiscard) {
    auto context = Context();
    ASSERT_TRUE(context.Queue("b"));
    auto edit = context.PlanEdit({"a", "c"});
    ASSERT_TRUE(edit);
    EXPECT_EQ(edit->withdrawals, (Ids{"b"}));
    EXPECT_FALSE(context.CommitEdit(*edit));
    EXPECT_TRUE(context.Withdrawn(*edit, "b"));
    ASSERT_TRUE(context.CommitEdit(*edit));
    EXPECT_EQ(context.Find("b")->disposition, Disposition::Discarded);
    EXPECT_FALSE(context.RestorationRank("b"));
}

TEST(JitNativeContext, ConsumptionDuringWithdrawalCannotBecomePendingOrDiscarded) {
    auto context = Context();
    ASSERT_TRUE(context.Queue("b"));
    auto edit = context.PlanEdit({"a", "c"});
    ASSERT_TRUE(edit);
    ASSERT_TRUE(context.Submit("b"));
    EXPECT_FALSE(context.Withdrawn(*edit, "b"));
    EXPECT_FALSE(context.CommitEdit(*edit));
    EXPECT_EQ(context.Find("b")->disposition, Disposition::Submitted);
    EXPECT_FALSE(context.PlanRestore("recovery"));
}

TEST(JitNativeContext, MissingStartAfterSubmissionCannotBeRestored) {
    auto context = Context();
    ASSERT_TRUE(context.Submit("b"));
    ASSERT_TRUE(context.Indeterminate("b"));
    EXPECT_FALSE(context.PlanRestore("recovery"));
    EXPECT_FALSE(context.PlanEdit({"a", "b", "c"}));
    // 迟到的起播确认仍算消费，之后就能恢复其余出现
    ASSERT_TRUE(context.Started("manual-b"));
    auto plan = context.PlanRestore("recovery");
    ASSERT_TRUE(plan);
    EXPECT_EQ(plan->occurrences, (Ids{"a", "c"}));
}

TEST(JitNativeContext, ReplayingConsumedOrDiscardedEntriesDoesNotRestoreThem) {
    auto context = Context();
    ASSERT_TRUE(context.Started("manual-a"));
    auto edit = context.PlanEdit({"c"});
    ASSERT_TRUE(edit);
    ASSERT_TRUE(context.CommitEdit(*edit));
    ASSERT_TRUE(context.Started("manual-a"));
    ASSERT_TRUE(context.Started("manual-b"));
    EXPECT_EQ(context.Find("a")->disposition, Disposition::Consumed);
    EXPECT_EQ(context.Find("b")->disposition, Disposition::Discarded);
    auto plan = context.PlanRestore("recovery");
    ASSERT_TRUE(plan);
    EXPECT_EQ(plan->occurrences, (Ids{"c"}));
}

TEST(JitNativeContext, QueueEditsRejectDuplicatesUnknownAndAlreadyConsumedIds) {
    auto context = Context();
    ASSERT_TRUE(context.Started("manual-a"));
    EXPECT_FALSE(context.PlanEdit({"b", "b"}));
    EXPECT_FALSE(context.PlanEdit({"unknown"}));
    EXPECT_FALSE(context.PlanEdit({"a", "b"}));
}

// ---------------------------------------------------------------------------
// 恢复
// ---------------------------------------------------------------------------

TEST(JitNativeContext, ExhaustedSourceChangesOnlyTheLastRestoreOccurrence) {
    auto context = Context(5);
    auto plan = context.PlanRestore("recovery");
    ASSERT_TRUE(plan);
    EXPECT_FALSE(plan->sourceRow);
    EXPECT_EQ(plan->terminalOccurrence, "c");

    auto empty = Context(5);
    auto edit = empty.PlanEdit({});
    ASSERT_TRUE(edit);
    ASSERT_TRUE(empty.CommitEdit(*edit));
    auto none = empty.PlanRestore("recovery");
    ASSERT_TRUE(none);
    EXPECT_TRUE(none->occurrences.empty());
    EXPECT_FALSE(none->terminalOccurrence);
}

TEST(JitNativeContext, NonExhaustedRestoreUsesExactSourceSuccessor) {
    auto context = Context();
    auto plan = context.PlanRestore("recovery");
    ASSERT_TRUE(plan);
    EXPECT_EQ(plan->sourceRow, 3u);
    EXPECT_FALSE(plan->terminalOccurrence);
}

TEST(JitNativeContext, QueuedOccurrenceIsWithdrawnBeforeItsRestoreTurn) {
    auto context = Context();
    ASSERT_TRUE(context.Queue("b"));
    auto plan = context.PlanRestore("recovery");
    ASSERT_TRUE(plan);
    EXPECT_EQ(plan->occurrences, (Ids{"a", "b", "c"}));
    ASSERT_TRUE(context.Restored(*plan, "a"));
    EXPECT_FALSE(context.Restored(*plan, "b"));  // 仍在原生队列里，不能再插一次
    ASSERT_TRUE(context.RestoreWithdrawn(*plan, "b"));
    ASSERT_TRUE(context.Restored(*plan, "b"));
    ASSERT_TRUE(context.Restored(*plan, "c"));
    ASSERT_TRUE(context.FinishRestore(*plan));
    EXPECT_EQ(context.GetStatus(), Status::Restored);
    EXPECT_FALSE(context.Started("manual-a"));
}

TEST(JitNativeContext, FinishRestoreRequiresEveryOccurrence) {
    auto context = Context();
    auto plan = context.PlanRestore("recovery");
    ASSERT_TRUE(plan);
    ASSERT_TRUE(context.Restored(*plan, "a"));
    EXPECT_FALSE(context.FinishRestore(*plan));
    EXPECT_EQ(context.GetStatus(), Status::Restoring);
    EXPECT_FALSE(context.PlanEdit({"b"}));  // 恢复开始后不再接受编辑
}

TEST(JitNativeContext, PartialRestoreRetryDoesNotInsertCompletedOccurrencesAgain) {
    auto context = Context();
    auto first = context.PlanRestore("same-recovery");
    ASSERT_TRUE(first);
    ASSERT_TRUE(context.Restored(*first, "a"));
    EXPECT_FALSE(context.Restored(*first, "a"));
    auto retry = context.PlanRestore("same-recovery");
    ASSERT_TRUE(retry);
    EXPECT_EQ(retry->occurrences, (Ids{"b", "c"}));
    EXPECT_EQ(context.RecoveryId(), "same-recovery");
    EXPECT_FALSE(context.PlanRestore("different-recovery"));
    // 旧计划在重新规划后仍可继续，但不能重复交还
    EXPECT_TRUE(context.Restored(*first, "b"));
    EXPECT_FALSE(context.Restored(*retry, "b"));
}

TEST(JitNativeContext, ExternalConflictRefusesStaleRestoreAndQueueEdit) {
    auto editing = Context();
    auto edit = editing.PlanEdit({"a"});
    ASSERT_TRUE(edit);
    editing.Conflict();
    EXPECT_FALSE(editing.CommitEdit(*edit));
    EXPECT_FALSE(editing.PlanRestore("recovery"));

    auto restoring = Context();
    auto plan = restoring.PlanRestore("recovery");
    ASSERT_TRUE(plan);
    restoring.Conflict();
    EXPECT_FALSE(restoring.Restored(*plan, "a"));
    EXPECT_FALSE(restoring.PlanRestore("recovery"));
    EXPECT_EQ(restoring.GetStatus(), Status::Conflicted);
    ASSERT_TRUE(restoring.Detach());
    EXPECT_EQ(restoring.Find("a")->disposition, Disposition::Pending);  // 未交还的仍留在账里
    EXPECT_FALSE(restoring.Detach());
}

TEST(JitNativeContext, ConflictStopsControlButStillRecordsConfirmedManualConsumption) {
    auto context = Context();
    ASSERT_TRUE(context.RegisterSource(4, "source-4"));
    ASSERT_TRUE(context.Submit("b"));
    context.Conflict();
    EXPECT_TRUE(context.Started("manual-b"));
    EXPECT_EQ(context.Find("b")->disposition, Disposition::Consumed);
    EXPECT_FALSE(context.Started("source-4"));
    EXPECT_EQ(context.SourceNext(), 3u);
    EXPECT_FALSE(context.Submit("c"));
}

TEST(JitNativeContext, RestorePlansFromAnotherContextAndOutOfOrderWritesAreRejected) {
    auto context = Context();
    auto other = Context();
    auto plan = context.PlanRestore("recovery");
    ASSERT_TRUE(plan);
    ASSERT_TRUE(other.PlanRestore("recovery"));
    EXPECT_FALSE(other.Restored(*plan, "a"));
    EXPECT_FALSE(context.Restored(*plan, "b"));
    EXPECT_TRUE(context.Restored(*plan, "a"));
}
