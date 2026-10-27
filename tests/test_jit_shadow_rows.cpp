// test_jit_shadow_rows.cpp - JIT 影子列表的行归属、归属判定与添加代次
//
// 直接链接生产头 core/JitShadowRows.h。handle 用小整数代替指针值；相同数值表示
// 同一资源，用来复现“同一资源连续两行”的场景。
#include "pch.h"
#include "core/JitShadowRows.h"

using jit_shadow::Attribute;
using jit_shadow::Attribution;
using jit_shadow::EntryKind;
using jit_shadow::Generations;
using jit_shadow::HandleKey;
using jit_shadow::kNoRow;
using jit_shadow::PlayingLocation;
using jit_shadow::RowTable;
using jit_shadow::Slot;

namespace {

constexpr HandleKey kA = 0xA0;
constexpr HandleKey kB = 0xB0;
constexpr HandleKey kC = 0xC0;

PlayingLocation InShadow(std::size_t row) { return {true, true, row}; }

// 当前条目 1 与下一条目 2 是同一资源 A。
RowTable DuplicateCurrentAndNext() {
    RowTable table;
    table.Insert(0, 1, EntryKind::Single, {kA});
    table.Insert(1, 2, EntryKind::Single, {kA});
    return table;
}

}  // namespace

// ---------------------------------------------------------------------------
// 归属判定
// ---------------------------------------------------------------------------

TEST(JitShadowAttribute, ReplayOfCurrentKeepsDuplicateNext) {
    const RowTable table = DuplicateCurrentAndNext();
    EXPECT_EQ(Attribute(table, InShadow(0), 1, 2, false), Attribution::SameEntry);
}

TEST(JitShadowAttribute, AdvanceToDuplicateNextIsByRowNotHandle) {
    const RowTable table = DuplicateCurrentAndNext();
    EXPECT_EQ(Attribute(table, InShadow(1), 1, 2, false), Attribution::Advance);
}

TEST(JitShadowAttribute, ReleasesWhenNotLocatedOrElsewhere) {
    const RowTable table = DuplicateCurrentAndNext();
    EXPECT_EQ(Attribute(table, {false, false, kNoRow}, 1, 2, false), Attribution::Release);
    // 其他列表播放同一资源：位置在别的列表，与 handle 无关
    EXPECT_EQ(Attribute(table, {true, false, 0}, 1, 2, false), Attribution::Release);
}

TEST(JitShadowAttribute, ReleasesOnStaleOrOutOfRangeOrOwnerless) {
    RowTable table = DuplicateCurrentAndNext();
    EXPECT_EQ(Attribute(table, InShadow(2), 1, 2, false), Attribution::Release);
    table.Insert(2, 0, EntryKind::Single, {kC});
    EXPECT_EQ(Attribute(table, InShadow(2), 1, 2, false), Attribution::Release);
    table.MarkStale();
    EXPECT_EQ(Attribute(table, InShadow(0), 1, 2, false), Attribution::Release);
}

TEST(JitShadowAttribute, ReleasesRowOfAnotherSingleEntry) {
    RowTable table;
    table.Insert(0, 7, EntryKind::Single, {kA});
    EXPECT_EQ(Attribute(table, InShadow(0), 1, 2, false), Attribution::Release);
}

TEST(JitShadowAttribute, NextRowWithoutNextEntryIsNotAdvance) {
    RowTable table;
    table.Insert(0, 1, EntryKind::Single, {kA});
    table.Insert(1, 2, EntryKind::Single, {kB});
    // 下一条已转为当前之后，旧序号不再是下一条目
    EXPECT_EQ(Attribute(table, InShadow(1), 2, 0, false), Attribution::SameEntry);
    EXPECT_EQ(Attribute(table, InShadow(0), 2, 0, false), Attribution::Release);
}

TEST(JitShadowAttribute, ExpandedRowsBelongToOneEntry) {
    RowTable table;
    table.Insert(0, 1, EntryKind::Single, {kA, kB, kC});
    EXPECT_EQ(Attribute(table, InShadow(2), 1, 0, false), Attribution::SameEntry);
}

TEST(JitShadowAttribute, BatchRowsActivateOnlyALiveSession) {
    RowTable table;
    table.ResetAsBatch({kA, kB}, 5);
    EXPECT_EQ(Attribute(table, InShadow(1), 0, 0, false), Attribution::BatchRow);
    EXPECT_EQ(Attribute(table, InShadow(1), 0, 0, true), Attribution::Release);
}

TEST(JitShadowAttribute, IdleSessionReactivatesFromItsSingleRows) {
    const RowTable table = DuplicateCurrentAndNext();
    EXPECT_EQ(Attribute(table, InShadow(0), 1, 2, true), Attribution::SameEntry);
}

// ---------------------------------------------------------------------------
// 归属表维护
// ---------------------------------------------------------------------------

TEST(JitShadowRowTable, CurrentRowsGoBeforeNextRows) {
    RowTable table;
    table.Insert(0, 2, EntryKind::Single, {kB});
    const std::size_t at = table.CurrentInsertPosition(2, table.Size());
    EXPECT_EQ(at, 0u);
    table.Insert(at, 1, EntryKind::Single, {kA});
    EXPECT_EQ(table.At(0).entry, 1u);
    EXPECT_EQ(table.At(1).entry, 2u);
    EXPECT_EQ(table.CurrentInsertPosition(0, table.Size()), 2u);
    // 作废后追加到列表的实际末尾
    table.MarkStale();
    EXPECT_EQ(table.CurrentInsertPosition(2, 5), 5u);
}

TEST(JitShadowRowTable, RemovingPlayedPrefixKeepsAlignment) {
    RowTable table;
    table.Insert(0, 1, EntryKind::Single, {kA});
    table.Insert(1, 2, EntryKind::Single, {kA});
    table.Insert(2, 3, EntryKind::Single, {kC});
    table.Remove(0, 1);
    ASSERT_EQ(table.Size(), 2u);
    EXPECT_EQ(table.At(0).entry, 2u);
    EXPECT_TRUE(table.Verify({kA, kC}));
}

TEST(JitShadowRowTable, FirstAndLastRowOfEntry) {
    RowTable table;
    table.Insert(0, 1, EntryKind::Single, {kA, kB});
    table.Insert(2, 2, EntryKind::Single, {kC});
    EXPECT_EQ(table.FirstRowOf(1), 0u);
    EXPECT_EQ(table.LastRowOf(1), 1u);
    EXPECT_EQ(table.FirstRowOf(2), 2u);
    EXPECT_EQ(table.FirstRowOf(9), kNoRow);
    EXPECT_EQ(table.FirstRowOf(0), kNoRow);
}

TEST(JitShadowRowTable, VerifyDetectsExternalChange) {
    RowTable table = DuplicateCurrentAndNext();
    EXPECT_TRUE(table.Verify({kA, kA}));
    EXPECT_FALSE(table.Verify({kA, kB}));
    EXPECT_TRUE(table.IsStale());
    // 作废后不再恢复，直到重置
    EXPECT_FALSE(table.Verify({kA, kA}));

    RowTable shorter = DuplicateCurrentAndNext();
    EXPECT_FALSE(shorter.Verify({kA}));

    // 末尾多出的行之前有一行被换掉：仍是外部修改
    RowTable replaced = DuplicateCurrentAndNext();
    EXPECT_FALSE(replaced.Verify({kA, kB, kC}));
}

TEST(JitShadowRowTable, TailAppendBecomesOwnerlessRows) {
    RowTable table = DuplicateCurrentAndNext();
    EXPECT_TRUE(table.Verify({kA, kA, kC}));
    ASSERT_EQ(table.Size(), 3u);
    EXPECT_EQ(table.At(2).entry, 0u);
    EXPECT_EQ(table.At(2).handle, kC);
    // 播到追加的行让出，已登记的行照常判定
    EXPECT_EQ(Attribute(table, InShadow(2), 1, 2, false), Attribution::Release);
    EXPECT_EQ(Attribute(table, InShadow(1), 1, 2, false), Attribution::Advance);
    // 下一条目的行之后追加了行，当前条目的行仍插在下一条目之前
    EXPECT_EQ(table.CurrentInsertPosition(2, table.Size()), 1u);

    // 清空后列表里剩下的行全部成为无主行
    table.Reset();
    EXPECT_TRUE(table.Verify({kA, kA, kC}));
    EXPECT_EQ(table.FirstRowOf(1), kNoRow);
    EXPECT_EQ(Attribute(table, InShadow(0), 1, 2, false), Attribution::Release);
}

TEST(JitShadowRowTable, StaleTableRefusesLookupsAndEdits) {
    RowTable table = DuplicateCurrentAndNext();
    table.MarkStale();
    table.Insert(0, 3, EntryKind::Single, {kC});
    table.Remove(0, 1);
    EXPECT_EQ(table.Size(), 2u);
    EXPECT_EQ(table.LastRowOf(1), kNoRow);
    table.Reset();
    EXPECT_FALSE(table.IsStale());
    EXPECT_EQ(table.Size(), 0u);
}

TEST(JitShadowRowTable, ResetAsBatchRecoversFromStale) {
    RowTable table;
    table.MarkStale();
    table.ResetAsBatch({kA, kB}, 4);
    EXPECT_FALSE(table.IsStale());
    ASSERT_EQ(table.Size(), 2u);
    EXPECT_EQ(table.At(1).kind, EntryKind::Batch);
    EXPECT_TRUE(table.Verify({kA, kB}));
}

// ---------------------------------------------------------------------------
// 添加代次
// ---------------------------------------------------------------------------

TEST(JitShadowGenerations, EndingTheSessionInvalidatesEveryPendingAdd) {
    Generations gens;
    const auto current = gens.TakeCurrent();
    const auto next = gens.TakeNext();
    gens.EndSession();
    EXPECT_FALSE(gens.IsLive(current));
    EXPECT_FALSE(gens.IsLive(next));
    EXPECT_TRUE(gens.IsLive(gens.TakeCurrent()));
}

TEST(JitShadowGenerations, ReplacingNextKeepsCurrentLive) {
    Generations gens;
    const auto current = gens.TakeCurrent();
    const auto firstNext = gens.TakeNext();
    const auto secondNext = gens.TakeNext();
    EXPECT_TRUE(gens.IsLive(current));
    EXPECT_FALSE(gens.IsLive(firstNext));
    EXPECT_TRUE(gens.IsLive(secondNext));
    EXPECT_EQ(secondNext.slot, Slot::Next);
}

TEST(JitShadowGenerations, OnlyTheLatestTrackIsAttributed) {
    Generations gens;
    const auto first = gens.TrackStarted();
    const auto second = gens.TrackStarted();
    EXPECT_FALSE(gens.IsLatestTrack(first));
    EXPECT_TRUE(gens.IsLatestTrack(second));
}

TEST(JitShadowGenerations, EndingTheSessionDropsAQueuedAttribution) {
    // 旧会话的曲目刚开始、归属步骤还没执行时 playNow 替换了会话：这一步作废，
    // 不能把它判为让出，进而作废新 playNow 的添加。
    Generations gens;
    const auto track = gens.TrackStarted();
    gens.EndSession();
    const auto add = gens.TakeCurrent();
    EXPECT_FALSE(gens.IsLatestTrack(track));
    EXPECT_TRUE(gens.IsLive(add));
}
