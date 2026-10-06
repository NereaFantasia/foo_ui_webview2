// test_queue_rebuild_plan.cpp - pure target-sequence computation for the
// playback queue single rebuild unit (docs/PLAYBACK_QUEUE_INSERT_SPEC.md
// §5.1 / DEC-7). SDK interaction (actual queue_flush / queue_add_item*
// calls) is covered by manual acceptance, not here -- these tests only
// exercise the coordinate-vs-handle resolution and target-order math.
#include "pch.h"
#include "../src/api/QueueRebuildPlan.h"

using fb2k_queue::ComputeInsertNextPlan;
using fb2k_queue::ComputeMoveToTopPlan;
using fb2k_queue::ComputeSetContentsPlan;
using fb2k_queue::CoordinateWritablePredicate;
using fb2k_queue::InsertNextNewItem;
using fb2k_queue::kInvalidCoordinate;
using fb2k_queue::QueueSlotSnapshot;
using fb2k_queue::ResolvedItemKind;
using fb2k_queue::ResolveQueueSlot;
using fb2k_queue::ResolveTargetSequence;
using fb2k_queue::SetContentsItemRef;
using fb2k_queue::SetContentsRefKind;

namespace {

// All-writable predicate: every coordinate is treated as writable, i.e. the
// live playlist_manager state has none of these positions truncated.
CoordinateWritablePredicate AlwaysWritable() {
    return [](size_t, size_t) { return true; };
}

// Mirrors the real IsCoordinateWritable's `item != infinite_size` half of
// the check, which is the half that EXP-0(a)'s half-degraded coordinate
// (m_item -> infinite_size, m_playlist unchanged) actually fails on.
CoordinateWritablePredicate WritableUnlessItemInvalid() {
    return [](size_t, size_t item) { return item != kInvalidCoordinate; };
}

}  // namespace

// ---------------------------------------------------------------------------
// ResolveQueueSlot: single-slot resolution priority (coordinate first,
// handle fallback, else unresolvable).
// ---------------------------------------------------------------------------

TEST(ResolveQueueSlot, WritableCoordinateResolvesToCoordinateKind) {
    QueueSlotSnapshot slot{/*playlist*/ 1, /*item*/ 2, /*handleIndex*/ kInvalidCoordinate};
    auto resolved = ResolveQueueSlot(slot, AlwaysWritable());

    EXPECT_EQ(resolved.kind, ResolvedItemKind::Coordinate);
    EXPECT_EQ(resolved.playlist, 1u);
    EXPECT_EQ(resolved.item, 2u);
}

TEST(ResolveQueueSlot, CoordinateTakesPriorityOverHandleWhenBothUsable) {
    // §5.1 "坐标优先、handle 兜底": even if a handle is also available, the
    // coordinate form must win when it is writable.
    QueueSlotSnapshot slot{/*playlist*/ 3, /*item*/ 4, /*handleIndex*/ 7};
    auto resolved = ResolveQueueSlot(slot, AlwaysWritable());

    EXPECT_EQ(resolved.kind, ResolvedItemKind::Coordinate);
    EXPECT_EQ(resolved.playlist, 3u);
    EXPECT_EQ(resolved.item, 4u);
}

TEST(ResolveQueueSlot, UnwritableCoordinateFallsBackToHandle) {
    // EXP-0(a): playlist.clear degrades m_item to infinite_size while
    // m_playlist survives; the item is still consumable via the surviving
    // handle. This is the D1 fix path.
    QueueSlotSnapshot slot{/*playlist*/ 6, /*item*/ kInvalidCoordinate, /*handleIndex*/ 0};
    auto resolved = ResolveQueueSlot(slot, WritableUnlessItemInvalid());

    EXPECT_EQ(resolved.kind, ResolvedItemKind::Handle);
    EXPECT_EQ(resolved.handleIndex, 0u);
}

TEST(ResolveQueueSlot, NeitherCoordinateNorHandleIsUnresolvable) {
    QueueSlotSnapshot slot{/*playlist*/ kInvalidCoordinate, /*item*/ kInvalidCoordinate,
                            /*handleIndex*/ kInvalidCoordinate};
    auto resolved = ResolveQueueSlot(slot, WritableUnlessItemInvalid());

    EXPECT_EQ(resolved.kind, ResolvedItemKind::Unresolvable);
}

TEST(ResolveQueueSlot, NullPredicateTreatedAsUnwritable) {
    // Defensive: an empty std::function must not be invoked, only treated
    // as "not writable", so callers can't crash by forgetting to inject one.
    QueueSlotSnapshot slot{/*playlist*/ 1, /*item*/ 2, /*handleIndex*/ 5};
    CoordinateWritablePredicate empty;
    auto resolved = ResolveQueueSlot(slot, empty);

    EXPECT_EQ(resolved.kind, ResolvedItemKind::Handle);
    EXPECT_EQ(resolved.handleIndex, 5u);
}

// ---------------------------------------------------------------------------
// ResolveTargetSequence: shared step-1 resolution for a full ordered plan.
// ---------------------------------------------------------------------------

TEST(ResolveTargetSequence, AllResolvableProducesOkWithMatchingOrder) {
    std::vector<QueueSlotSnapshot> order = {
        {0, 0, kInvalidCoordinate},
        {0, 1, kInvalidCoordinate},
        {0, 2, kInvalidCoordinate},
    };
    auto result = ResolveTargetSequence(order, AlwaysWritable());

    ASSERT_TRUE(result.ok);
    ASSERT_EQ(result.resolvedItems.size(), 3u);
    EXPECT_EQ(result.resolvedItems[0].item, 0u);
    EXPECT_EQ(result.resolvedItems[1].item, 1u);
    EXPECT_EQ(result.resolvedItems[2].item, 2u);
}

TEST(ResolveTargetSequence, DuplicateReferencesArePreservedLiterally) {
    // §4.1 约束 4: repeated references are legal and must not be
    // deduplicated by the target-sequence computation -- declarative
    // callers (setContents, once built on this shared step) get exactly
    // the sequence they declared, repeats included.
    std::vector<QueueSlotSnapshot> order = {
        {2, 5, kInvalidCoordinate},
        {2, 5, kInvalidCoordinate},
    };
    auto result = ResolveTargetSequence(order, AlwaysWritable());

    ASSERT_TRUE(result.ok);
    ASSERT_EQ(result.resolvedItems.size(), 2u);
    EXPECT_EQ(result.resolvedItems[0].playlist, 2u);
    EXPECT_EQ(result.resolvedItems[0].item, 5u);
    EXPECT_EQ(result.resolvedItems[1].playlist, 2u);
    EXPECT_EQ(result.resolvedItems[1].item, 5u);
}

TEST(ResolveTargetSequence, FirstUnresolvableSlotFailsWholeSequence) {
    std::vector<QueueSlotSnapshot> order = {
        {0, 0, kInvalidCoordinate},
        {kInvalidCoordinate, kInvalidCoordinate, kInvalidCoordinate},  // unresolvable
        {0, 2, kInvalidCoordinate},
    };
    auto result = ResolveTargetSequence(order, WritableUnlessItemInvalid());

    EXPECT_FALSE(result.ok);
    EXPECT_TRUE(result.resolvedItems.empty());
    EXPECT_FALSE(result.error.empty());
}

// ---------------------------------------------------------------------------
// ComputeMoveToTopPlan: moveToTop-specific target ordering.
// ---------------------------------------------------------------------------

TEST(ComputeMoveToTopPlan, MovesMiddleItemToFrontPreservingRest) {
    std::vector<QueueSlotSnapshot> queue = {
        {0, 0, kInvalidCoordinate},  // A
        {0, 1, kInvalidCoordinate},  // B
        {0, 2, kInvalidCoordinate},  // C
        {0, 3, kInvalidCoordinate},  // D
    };
    auto result = ComputeMoveToTopPlan(queue, /*index*/ 2, AlwaysWritable());

    ASSERT_TRUE(result.ok);
    ASSERT_EQ(result.resolvedItems.size(), 4u);
    // Expect [C, A, B, D]
    EXPECT_EQ(result.resolvedItems[0].item, 2u);
    EXPECT_EQ(result.resolvedItems[1].item, 0u);
    EXPECT_EQ(result.resolvedItems[2].item, 1u);
    EXPECT_EQ(result.resolvedItems[3].item, 3u);
}

TEST(ComputeMoveToTopPlan, IndexZeroLeavesOrderUnchanged) {
    std::vector<QueueSlotSnapshot> queue = {
        {0, 0, kInvalidCoordinate},
        {0, 1, kInvalidCoordinate},
        {0, 2, kInvalidCoordinate},
    };
    auto result = ComputeMoveToTopPlan(queue, /*index*/ 0, AlwaysWritable());

    ASSERT_TRUE(result.ok);
    ASSERT_EQ(result.resolvedItems.size(), 3u);
    EXPECT_EQ(result.resolvedItems[0].item, 0u);
    EXPECT_EQ(result.resolvedItems[1].item, 1u);
    EXPECT_EQ(result.resolvedItems[2].item, 2u);
}

TEST(ComputeMoveToTopPlan, LastIndexMovesToFront) {
    std::vector<QueueSlotSnapshot> queue = {
        {0, 0, kInvalidCoordinate},
        {0, 1, kInvalidCoordinate},
        {0, 2, kInvalidCoordinate},
        {0, 3, kInvalidCoordinate},
    };
    auto result = ComputeMoveToTopPlan(queue, /*index*/ 3, AlwaysWritable());

    ASSERT_TRUE(result.ok);
    ASSERT_EQ(result.resolvedItems.size(), 4u);
    EXPECT_EQ(result.resolvedItems[0].item, 3u);
    EXPECT_EQ(result.resolvedItems[1].item, 0u);
    EXPECT_EQ(result.resolvedItems[2].item, 1u);
    EXPECT_EQ(result.resolvedItems[3].item, 2u);
}

TEST(ComputeMoveToTopPlan, OutOfRangeIndexFailsWithoutTouchingQueue) {
    std::vector<QueueSlotSnapshot> queue = {
        {0, 0, kInvalidCoordinate},
        {0, 1, kInvalidCoordinate},
        {0, 2, kInvalidCoordinate},
    };
    auto result = ComputeMoveToTopPlan(queue, /*index*/ 5, AlwaysWritable());

    EXPECT_FALSE(result.ok);
    EXPECT_TRUE(result.resolvedItems.empty());
    EXPECT_FALSE(result.error.empty());
}

TEST(ComputeMoveToTopPlan, EmptyQueueAlwaysFails) {
    std::vector<QueueSlotSnapshot> queue;
    auto result = ComputeMoveToTopPlan(queue, /*index*/ 0, AlwaysWritable());

    EXPECT_FALSE(result.ok);
    EXPECT_TRUE(result.resolvedItems.empty());
}

TEST(ComputeMoveToTopPlan, HalfDegradedCoordinateFallsBackToHandleAndStillMoves) {
    // EXP-0(a) shape: after playlist.clear, the moved-to-top item's m_item
    // degraded to infinite_size but its handle survived (EXP-1: handle-only
    // items are consumable). D1's "silently drops the item" bug must not
    // reproduce here -- the item must resolve via the handle fallback and
    // keep its place in the target order.
    std::vector<QueueSlotSnapshot> queue = {
        {0, 0, kInvalidCoordinate},              // A - still coordinate-valid
        {6, kInvalidCoordinate, /*handle*/ 42},  // B - half-degraded, handle 42
        {0, 2, kInvalidCoordinate},              // C - still coordinate-valid
    };
    auto result = ComputeMoveToTopPlan(queue, /*index*/ 1, WritableUnlessItemInvalid());

    ASSERT_TRUE(result.ok);
    ASSERT_EQ(result.resolvedItems.size(), 3u);
    EXPECT_EQ(result.resolvedItems[0].kind, ResolvedItemKind::Handle);
    EXPECT_EQ(result.resolvedItems[0].handleIndex, 42u);
    EXPECT_EQ(result.resolvedItems[1].kind, ResolvedItemKind::Coordinate);
    EXPECT_EQ(result.resolvedItems[1].item, 0u);
    EXPECT_EQ(result.resolvedItems[2].kind, ResolvedItemKind::Coordinate);
    EXPECT_EQ(result.resolvedItems[2].item, 2u);
}

TEST(ComputeMoveToTopPlan, AnyUnresolvableSlotFailsEntirePlanRegardlessOfPosition) {
    // The unresolvable slot here is NOT the one being moved to top -- the
    // whole rebuild must still fail (§5.1 step 1: "any item unresolvable
    // -> return failure, queue untouched"), not just skip that one slot.
    std::vector<QueueSlotSnapshot> queue = {
        {0, 0, kInvalidCoordinate},
        {kInvalidCoordinate, kInvalidCoordinate, kInvalidCoordinate},  // unresolvable
        {0, 2, kInvalidCoordinate},
    };
    auto result = ComputeMoveToTopPlan(queue, /*index*/ 0, WritableUnlessItemInvalid());

    EXPECT_FALSE(result.ok);
    EXPECT_TRUE(result.resolvedItems.empty());
}

// ---------------------------------------------------------------------------
// ComputeSetContentsPlan: setContents-specific reference resolution
// (§4.1 QueueItemRef's two forms).
// ---------------------------------------------------------------------------

namespace {

SetContentsItemRef QueueRef(size_t queueIndex) {
    SetContentsItemRef ref;
    ref.kind = SetContentsRefKind::QueueReference;
    ref.queueIndex = queueIndex;
    return ref;
}

SetContentsItemRef ListRef(size_t playlist, size_t item) {
    SetContentsItemRef ref;
    ref.kind = SetContentsRefKind::ListReference;
    ref.playlist = playlist;
    ref.item = item;
    return ref;
}

}  // namespace

TEST(ComputeSetContentsPlan, ReversedQueueReferencesReverseTheQueue) {
    // Current queue [A,B,C,D] (item 0..3); items:[{queueIndex:3},{2},{1},{0}]
    // must produce [D,C,B,A].
    std::vector<QueueSlotSnapshot> currentQueue = {
        {0, 0, kInvalidCoordinate},  // A
        {0, 1, kInvalidCoordinate},  // B
        {0, 2, kInvalidCoordinate},  // C
        {0, 3, kInvalidCoordinate},  // D
    };
    std::vector<SetContentsItemRef> refs = {QueueRef(3), QueueRef(2), QueueRef(1), QueueRef(0)};

    auto result = ComputeSetContentsPlan(refs, currentQueue, AlwaysWritable());

    ASSERT_TRUE(result.ok);
    ASSERT_EQ(result.resolvedItems.size(), 4u);
    EXPECT_EQ(result.resolvedItems[0].item, 3u);
    EXPECT_EQ(result.resolvedItems[1].item, 2u);
    EXPECT_EQ(result.resolvedItems[2].item, 1u);
    EXPECT_EQ(result.resolvedItems[3].item, 0u);
}

TEST(ComputeSetContentsPlan, OutOfRangeQueueIndexFailsAndNamesTheItemPosition) {
    // §4.1 约束 1: flush-before-touch full validation -- the error must
    // name which items[i] was the offender so the caller can report it.
    std::vector<QueueSlotSnapshot> currentQueue = {
        {0, 0, kInvalidCoordinate},
        {0, 1, kInvalidCoordinate},
    };
    std::vector<SetContentsItemRef> refs = {QueueRef(0), QueueRef(5)};

    auto result = ComputeSetContentsPlan(refs, currentQueue, AlwaysWritable());

    EXPECT_FALSE(result.ok);
    EXPECT_TRUE(result.resolvedItems.empty());
    EXPECT_NE(result.error.find("items[1]"), std::string::npos) << result.error;
}

TEST(ComputeSetContentsPlan, EmptyRefsProducesOkEmptyPlan) {
    // §4.1 参数表: empty items == explicit clear. The pure function itself
    // has no special case for this -- an empty targetOrder simply resolves
    // to an empty, ok plan, and RebuildQueue's flush + zero writes + count
    // check (0 == 0) does the rest.
    std::vector<QueueSlotSnapshot> currentQueue = {
        {0, 0, kInvalidCoordinate},
        {0, 1, kInvalidCoordinate},
    };
    std::vector<SetContentsItemRef> refs;

    auto result = ComputeSetContentsPlan(refs, currentQueue, AlwaysWritable());

    ASSERT_TRUE(result.ok);
    EXPECT_TRUE(result.resolvedItems.empty());
}

TEST(ComputeSetContentsPlan, DuplicateQueueIndexIsPreservedLiterally) {
    // §4.1 约束 4: repeated references are legal and must not be
    // deduplicated -- the same queue slot referenced twice yields two
    // resolved items.
    std::vector<QueueSlotSnapshot> currentQueue = {
        {0, 0, kInvalidCoordinate},  // A
        {0, 1, kInvalidCoordinate},  // B
    };
    std::vector<SetContentsItemRef> refs = {QueueRef(0), QueueRef(0), QueueRef(1)};

    auto result = ComputeSetContentsPlan(refs, currentQueue, AlwaysWritable());

    ASSERT_TRUE(result.ok);
    ASSERT_EQ(result.resolvedItems.size(), 3u);
    EXPECT_EQ(result.resolvedItems[0].item, 0u);
    EXPECT_EQ(result.resolvedItems[1].item, 0u);
    EXPECT_EQ(result.resolvedItems[2].item, 1u);
}

TEST(ComputeSetContentsPlan, MixedQueueAndListReferencesResolveTogether) {
    std::vector<QueueSlotSnapshot> currentQueue = {
        {0, 0, kInvalidCoordinate},  // A
        {0, 1, kInvalidCoordinate},  // B
    };
    // items: [{queueIndex:1}, {playlist:2,item:9}, {queueIndex:0}]
    std::vector<SetContentsItemRef> refs = {QueueRef(1), ListRef(2, 9), QueueRef(0)};

    auto result = ComputeSetContentsPlan(refs, currentQueue, AlwaysWritable());

    ASSERT_TRUE(result.ok);
    ASSERT_EQ(result.resolvedItems.size(), 3u);
    EXPECT_EQ(result.resolvedItems[0].kind, ResolvedItemKind::Coordinate);
    EXPECT_EQ(result.resolvedItems[0].item, 1u);
    EXPECT_EQ(result.resolvedItems[1].kind, ResolvedItemKind::Coordinate);
    EXPECT_EQ(result.resolvedItems[1].playlist, 2u);
    EXPECT_EQ(result.resolvedItems[1].item, 9u);
    EXPECT_EQ(result.resolvedItems[2].kind, ResolvedItemKind::Coordinate);
    EXPECT_EQ(result.resolvedItems[2].item, 0u);
}

TEST(ComputeSetContentsPlan, ListReferenceWithUnwritableCoordinateFailsWholeCall) {
    // §4.1 约束 1 (audit ruling): a list reference's coordinate is
    // precheck'd with an index-carrying error -- it has no handle fallback
    // (it is an explicit caller-supplied coordinate; a stale one must
    // fail, not be silently substituted), and unlike a queue reference's
    // half-degraded-coordinate case, this failure must name `items[i]`.
    std::vector<QueueSlotSnapshot> currentQueue = {
        {0, 0, kInvalidCoordinate},
    };
    std::vector<SetContentsItemRef> refs = {QueueRef(0), ListRef(9, 9)};

    // Predicate rejects the (9,9) coordinate but accepts (0,0).
    CoordinateWritablePredicate onlyZeroZeroWritable = [](size_t playlist, size_t item) {
        return playlist == 0 && item == 0;
    };

    auto result = ComputeSetContentsPlan(refs, currentQueue, onlyZeroZeroWritable);

    EXPECT_FALSE(result.ok);
    EXPECT_TRUE(result.resolvedItems.empty());
    EXPECT_NE(result.error.find("items[1]"), std::string::npos) << result.error;
}

TEST(ComputeSetContentsPlan, SecondOfMultipleListReferencesUnwritableNamesItsOwnIndex) {
    // Index-correctness check: with several list references in the array,
    // the precheck must name the specific offending index, not just "some"
    // index or the first one -- here refs[0] and refs[2] are writable,
    // only refs[1] (a distinct list reference) is not.
    std::vector<QueueSlotSnapshot> currentQueue;  // unused: all refs are list references
    std::vector<SetContentsItemRef> refs = {
        ListRef(0, 0),  // writable
        ListRef(9, 9),  // NOT writable -- expect items[1] in the error
        ListRef(0, 1),  // writable, never reached
    };

    CoordinateWritablePredicate rejectNineNine = [](size_t playlist, size_t item) {
        return !(playlist == 9 && item == 9);
    };

    auto result = ComputeSetContentsPlan(refs, currentQueue, rejectNineNine);

    EXPECT_FALSE(result.ok);
    EXPECT_TRUE(result.resolvedItems.empty());
    EXPECT_NE(result.error.find("items[1]"), std::string::npos) << result.error;
    EXPECT_EQ(result.error.find("items[0]"), std::string::npos) << result.error;
    EXPECT_EQ(result.error.find("items[2]"), std::string::npos) << result.error;
}

// ---------------------------------------------------------------------------
// ComputeInsertNextPlan: insertNext-specific target ordering (§4.2).
// Queue slots below use identityKey as the 4th field (playlist, item,
// handleIndex, identityKey); identity values are small distinct ints
// standing in for "interned handle pointer" (§5.3 audit ruling 2) --
// what matters here is equality, not the concrete values.
// ---------------------------------------------------------------------------

namespace {

InsertNextNewItem NewItem(size_t identityKey, size_t handleIndex) {
    InsertNextNewItem item;
    item.identityKey = identityKey;
    item.handleIndex = handleIndex;
    return item;
}

}  // namespace

TEST(ComputeInsertNextPlan, A8_MiddlePositionMovesExistingItemWithoutDuplication) {
    // Queue [A,B,C,D] (identities 1,2,3,4) -> insertNext(['A'], position=2)
    // -> [B,C,A,D], movedCount==1, no duplicate A.
    std::vector<QueueSlotSnapshot> currentQueue = {
        {0, 0, kInvalidCoordinate, /*identity*/ 1},  // A
        {0, 1, kInvalidCoordinate, /*identity*/ 2},  // B
        {0, 2, kInvalidCoordinate, /*identity*/ 3},  // C
        {0, 3, kInvalidCoordinate, /*identity*/ 4},  // D
    };
    std::vector<InsertNextNewItem> newItems = {NewItem(/*identity*/ 1, /*handleIndex*/ 100)};

    auto result = ComputeInsertNextPlan(currentQueue, newItems, /*position*/ 2, AlwaysWritable());

    ASSERT_TRUE(result.ok);
    EXPECT_EQ(result.movedCount, 1u);
    EXPECT_EQ(result.insertedCount, 0u);
    ASSERT_EQ(result.resolvedItems.size(), 4u);
    // [B,C,A,D] -- A keeps its original coordinate (item==0), not a
    // handle-only slot, because DEC-4 says the *existing* slot moves.
    EXPECT_EQ(result.resolvedItems[0].kind, ResolvedItemKind::Coordinate);
    EXPECT_EQ(result.resolvedItems[0].item, 1u);  // B
    EXPECT_EQ(result.resolvedItems[1].kind, ResolvedItemKind::Coordinate);
    EXPECT_EQ(result.resolvedItems[1].item, 2u);  // C
    EXPECT_EQ(result.resolvedItems[2].kind, ResolvedItemKind::Coordinate);
    EXPECT_EQ(result.resolvedItems[2].item, 0u);  // A, original coordinate preserved
    EXPECT_EQ(result.resolvedItems[3].kind, ResolvedItemKind::Coordinate);
    EXPECT_EQ(result.resolvedItems[3].item, 3u);  // D
    EXPECT_FALSE(result.canUseAppendFastPath);
}

TEST(ComputeInsertNextPlan, A8b_IntersectionAtExtremePositionStillRebuildsNotAppends) {
    // Queue [A,B,C] -> insertNext(['A'], position=999) -> [B,C,A]. Even
    // though position is far past the tail, 语义 4 says intersection
    // (A already in queue) forces a move, not a fast-path append.
    std::vector<QueueSlotSnapshot> currentQueue = {
        {0, 0, kInvalidCoordinate, /*identity*/ 1},  // A
        {0, 1, kInvalidCoordinate, /*identity*/ 2},  // B
        {0, 2, kInvalidCoordinate, /*identity*/ 3},  // C
    };
    std::vector<InsertNextNewItem> newItems = {NewItem(/*identity*/ 1, /*handleIndex*/ 100)};

    auto result = ComputeInsertNextPlan(currentQueue, newItems, /*position*/ 999, AlwaysWritable());

    ASSERT_TRUE(result.ok);
    EXPECT_EQ(result.movedCount, 1u);
    EXPECT_EQ(result.insertedCount, 0u);
    ASSERT_EQ(result.resolvedItems.size(), 3u);
    EXPECT_EQ(result.resolvedItems[0].item, 1u);  // B
    EXPECT_EQ(result.resolvedItems[1].item, 2u);  // C
    EXPECT_EQ(result.resolvedItems[2].item, 0u);  // A
    // 语义 4's whole point: has-intersection beats position>=queueCount.
    EXPECT_FALSE(result.canUseAppendFastPath);
}

TEST(ComputeInsertNextPlan, ExistingDuplicateIdentity_OnlyFrontmostOccurrenceMoves_PositionUnchanged) {
    // Audit ruling (遗留疑问 2): queue duplicates are a structure the
    // caller/user deliberately set up -- insertNext only moves the
    // FRONTMOST occurrence of a repeated identity, the rest stay put.
    // Queue [X,A,X,B] -> insertNext(['X'], position=0): the frontmost X
    // is already at index 0, so this particular case happens to leave
    // the visible order unchanged -- but movedCount must still read 1,
    // and there must still be exactly one untouched X left in place.
    std::vector<QueueSlotSnapshot> currentQueue = {
        {0, 0, kInvalidCoordinate, /*identity*/ 100},  // X (frontmost)
        {0, 1, kInvalidCoordinate, /*identity*/ 1},    // A
        {0, 2, kInvalidCoordinate, /*identity*/ 100},  // X (second occurrence)
        {0, 3, kInvalidCoordinate, /*identity*/ 2},    // B
    };
    std::vector<InsertNextNewItem> newItems = {NewItem(/*identity*/ 100, /*handleIndex*/ 0)};

    auto result = ComputeInsertNextPlan(currentQueue, newItems, /*position*/ 0, AlwaysWritable());

    ASSERT_TRUE(result.ok);
    EXPECT_EQ(result.movedCount, 1u);
    EXPECT_EQ(result.insertedCount, 0u);
    ASSERT_EQ(result.resolvedItems.size(), 4u);
    // [X,A,X,B] -- same visible order, but the frontmost X is the moved
    // one (item==0) and the second X (item==2) is untouched, not merged.
    EXPECT_EQ(result.resolvedItems[0].item, 0u);  // moved X (frontmost)
    EXPECT_EQ(result.resolvedItems[1].item, 1u);  // A
    EXPECT_EQ(result.resolvedItems[2].item, 2u);  // untouched second X
    EXPECT_EQ(result.resolvedItems[3].item, 3u);  // B
}

TEST(ComputeInsertNextPlan, ExistingDuplicateIdentity_SecondOccurrenceStaysBehindWhenFrontmostMoves) {
    // More discriminating variant: queue [A,X,B,X] -> insertNext(['X'],
    // position=0) -> [X,A,B,X]. If the implementation wrongly collapsed
    // *every* matching occurrence into the move, both X's would land at
    // the front and `remaining` would only contain [A,B]. The audit
    // ruling requires the second X to stay exactly where it was
    // (relative to the other untouched items), at the tail.
    std::vector<QueueSlotSnapshot> currentQueue = {
        {0, 0, kInvalidCoordinate, /*identity*/ 1},    // A
        {0, 1, kInvalidCoordinate, /*identity*/ 100},  // X (frontmost)
        {0, 2, kInvalidCoordinate, /*identity*/ 2},    // B
        {0, 3, kInvalidCoordinate, /*identity*/ 100},  // X (second occurrence)
    };
    std::vector<InsertNextNewItem> newItems = {NewItem(/*identity*/ 100, /*handleIndex*/ 0)};

    auto result = ComputeInsertNextPlan(currentQueue, newItems, /*position*/ 0, AlwaysWritable());

    ASSERT_TRUE(result.ok);
    EXPECT_EQ(result.movedCount, 1u);
    EXPECT_EQ(result.insertedCount, 0u);
    ASSERT_EQ(result.resolvedItems.size(), 4u);
    // [X,A,B,X]: moved (frontmost) X first, then the untouched remainder
    // [A,B,second-X] in its original relative order.
    EXPECT_EQ(result.resolvedItems[0].item, 1u);  // moved X (frontmost, was index 1)
    EXPECT_EQ(result.resolvedItems[1].item, 0u);  // A
    EXPECT_EQ(result.resolvedItems[2].item, 2u);  // B
    EXPECT_EQ(result.resolvedItems[3].item, 3u);  // second X, untouched, still at the tail
}

TEST(ComputeInsertNextPlan, A15_DuplicateNewPathsOnlyInsertOnce) {
    // insertNext(['X','X'], position=0) against an empty queue -> only one
    // X ends up in the queue (语义 6: dedup by identity, first occurrence).
    std::vector<QueueSlotSnapshot> currentQueue;
    std::vector<InsertNextNewItem> newItems = {
        NewItem(/*identity*/ 42, /*handleIndex*/ 0),
        NewItem(/*identity*/ 42, /*handleIndex*/ 1),  // same identity, later handle -- dropped
    };

    // A brand-new item is coordinate-less by construction (kInvalidCoordinate,
    // kInvalidCoordinate); AlwaysWritable() would (wrongly, for this test's
    // purpose) call that "writable" and resolve it as Coordinate instead of
    // Handle, so use the predicate that actually rejects invalid coordinates.
    auto result = ComputeInsertNextPlan(currentQueue, newItems, /*position*/ 0, WritableUnlessItemInvalid());

    ASSERT_TRUE(result.ok);
    EXPECT_EQ(result.movedCount, 0u);
    EXPECT_EQ(result.insertedCount, 1u);
    ASSERT_EQ(result.resolvedItems.size(), 1u);
    EXPECT_EQ(result.resolvedItems[0].kind, ResolvedItemKind::Handle);
    EXPECT_EQ(result.resolvedItems[0].handleIndex, 0u);  // first occurrence wins
}

TEST(ComputeInsertNextPlan, FastPathEligible_NoIntersectionAndPositionAtOrPastTail) {
    // A6-shaped case: 10 new paths (none matching the queue), position far
    // past the tail -- canUseAppendFastPath must be true.
    std::vector<QueueSlotSnapshot> currentQueue = {
        {0, 0, kInvalidCoordinate, /*identity*/ 1},
        {0, 1, kInvalidCoordinate, /*identity*/ 2},
    };
    std::vector<InsertNextNewItem> newItems = {
        NewItem(/*identity*/ 10, 0),
        NewItem(/*identity*/ 11, 1),
    };

    auto result = ComputeInsertNextPlan(currentQueue, newItems, /*position*/ 999, WritableUnlessItemInvalid());

    ASSERT_TRUE(result.ok);
    EXPECT_EQ(result.movedCount, 0u);
    EXPECT_EQ(result.insertedCount, 2u);
    EXPECT_TRUE(result.canUseAppendFastPath);
    // Order is still correctly appended even though this is fast-path
    // eligible, so the same plan can also be consumed by the rebuild path.
    ASSERT_EQ(result.resolvedItems.size(), 4u);
    EXPECT_EQ(result.resolvedItems[2].handleIndex, 0u);
    EXPECT_EQ(result.resolvedItems[3].handleIndex, 1u);
}

TEST(ComputeInsertNextPlan, FastPathIneligible_PositionBeforeTailEvenWithoutIntersection) {
    // No intersection, but position (0) lands before the tail -- must
    // still go through the rebuild unit, not the append fast path.
    std::vector<QueueSlotSnapshot> currentQueue = {
        {0, 0, kInvalidCoordinate, /*identity*/ 1},
        {0, 1, kInvalidCoordinate, /*identity*/ 2},
    };
    std::vector<InsertNextNewItem> newItems = {NewItem(/*identity*/ 10, 0)};

    auto result = ComputeInsertNextPlan(currentQueue, newItems, /*position*/ 0, WritableUnlessItemInvalid());

    ASSERT_TRUE(result.ok);
    EXPECT_EQ(result.movedCount, 0u);
    EXPECT_FALSE(result.canUseAppendFastPath);
}

TEST(ComputeInsertNextPlan, PositionZeroInsertsAtQueueFront) {
    std::vector<QueueSlotSnapshot> currentQueue = {
        {0, 0, kInvalidCoordinate, /*identity*/ 1},  // A
        {0, 1, kInvalidCoordinate, /*identity*/ 2},  // B
    };
    std::vector<InsertNextNewItem> newItems = {NewItem(/*identity*/ 99, /*handleIndex*/ 7)};

    auto result = ComputeInsertNextPlan(currentQueue, newItems, /*position*/ 0, WritableUnlessItemInvalid());

    ASSERT_TRUE(result.ok);
    ASSERT_EQ(result.resolvedItems.size(), 3u);
    EXPECT_EQ(result.resolvedItems[0].kind, ResolvedItemKind::Handle);
    EXPECT_EQ(result.resolvedItems[0].handleIndex, 7u);
    EXPECT_EQ(result.resolvedItems[1].item, 0u);  // A
    EXPECT_EQ(result.resolvedItems[2].item, 1u);  // B
}

TEST(ComputeInsertNextPlan, EmptyNewItemsLeavesQueueUnchanged) {
    // §4.2 语义 7 (the "reject entirely" business decision) lives in the
    // handler, not here -- this pure function just reports a no-op plan
    // when there is nothing to move or insert.
    std::vector<QueueSlotSnapshot> currentQueue = {
        {0, 0, kInvalidCoordinate, /*identity*/ 1},
        {0, 1, kInvalidCoordinate, /*identity*/ 2},
    };
    std::vector<InsertNextNewItem> newItems;

    auto result = ComputeInsertNextPlan(currentQueue, newItems, /*position*/ 0, AlwaysWritable());

    ASSERT_TRUE(result.ok);
    EXPECT_EQ(result.movedCount, 0u);
    EXPECT_EQ(result.insertedCount, 0u);
    ASSERT_EQ(result.resolvedItems.size(), 2u);
    EXPECT_EQ(result.resolvedItems[0].item, 0u);
    EXPECT_EQ(result.resolvedItems[1].item, 1u);
}

// ---------------------------------------------------------------------------
// ComputeInsertNextPlan: the `items` (coordinate-carrying) new-item form
// (§4.2 语义 10 / 11 / 12). Coordinates below use playlist 5 with item 3 or
// 7; identity 100 stands for the same track reachable at both positions.
// ---------------------------------------------------------------------------

namespace {

// Coordinate-carrying counterpart of NewItem(): the `items` request form.
// NewItem() supplies the coordinate-less `paths` form; this helper supplies
// playlist/item coordinates so the matching rules can be tested separately.
InsertNextNewItem NewItemAt(size_t identityKey, size_t handleIndex, size_t playlist, size_t item) {
    InsertNextNewItem entry;
    entry.identityKey = identityKey;
    entry.handleIndex = handleIndex;
    entry.playlist = playlist;
    entry.item = item;
    return entry;
}

}  // namespace

TEST(ComputeInsertNextPlan, A26_CoordinateEqualSlotWinsOverFrontmostIdentity) {
    // 语义 11a: queue [X@(5,3), A, X@(5,7)], items:[{playlist:5,item:7}],
    // position 0 -> [X@(5,7), X@(5,3), A]. Matching the frontmost identity
    // first (11b only) would move X@(5,3) and rewrite it to (5,7), so the
    // queue would end up holding two (5,7) entries while the (5,3) entry the
    // caller never referenced silently disappears.
    std::vector<QueueSlotSnapshot> currentQueue = {
        {5, 3, kInvalidCoordinate, /*identity*/ 100},  // X at (5,3)
        {0, 1, kInvalidCoordinate, /*identity*/ 1},    // A
        {5, 7, kInvalidCoordinate, /*identity*/ 100},  // X at (5,7)
    };
    std::vector<InsertNextNewItem> newItems = {
        NewItemAt(/*identity*/ 100, /*handleIndex*/ 50, /*playlist*/ 5, /*item*/ 7)};

    auto result = ComputeInsertNextPlan(currentQueue, newItems, /*position*/ 0, WritableUnlessItemInvalid());

    ASSERT_TRUE(result.ok);
    EXPECT_EQ(result.movedCount, 1u);
    EXPECT_EQ(result.insertedCount, 0u);
    ASSERT_EQ(result.resolvedItems.size(), 3u);
    EXPECT_EQ(result.resolvedItems[0].kind, ResolvedItemKind::Coordinate);
    EXPECT_EQ(result.resolvedItems[0].playlist, 5u);
    EXPECT_EQ(result.resolvedItems[0].item, 7u);
    // The (5,3) slot stays exactly as it was, one position behind.
    EXPECT_EQ(result.resolvedItems[1].kind, ResolvedItemKind::Coordinate);
    EXPECT_EQ(result.resolvedItems[1].playlist, 5u);
    EXPECT_EQ(result.resolvedItems[1].item, 3u);
    EXPECT_EQ(result.resolvedItems[2].kind, ResolvedItemKind::Coordinate);
    EXPECT_EQ(result.resolvedItems[2].item, 1u);  // A
}

TEST(ComputeInsertNextPlan, A27_IdentityMatchRewritesCoordinate) {
    // 语义 11b: queue [X@(5,3), A], items:[{playlist:5,item:7}] -- no slot
    // carries (5,7), so the identity match wins and the moved slot is
    // repointed to the coordinate the caller supplied. Exactly one X remains.
    std::vector<QueueSlotSnapshot> currentQueue = {
        {5, 3, kInvalidCoordinate, /*identity*/ 100},  // X at (5,3)
        {0, 1, kInvalidCoordinate, /*identity*/ 1},    // A
    };
    std::vector<InsertNextNewItem> newItems = {
        NewItemAt(/*identity*/ 100, /*handleIndex*/ 50, /*playlist*/ 5, /*item*/ 7)};

    auto result = ComputeInsertNextPlan(currentQueue, newItems, /*position*/ 0, WritableUnlessItemInvalid());

    ASSERT_TRUE(result.ok);
    EXPECT_EQ(result.movedCount, 1u);
    EXPECT_EQ(result.insertedCount, 0u);
    ASSERT_EQ(result.resolvedItems.size(), 2u);  // no duplicate X
    EXPECT_EQ(result.resolvedItems[0].kind, ResolvedItemKind::Coordinate);
    EXPECT_EQ(result.resolvedItems[0].playlist, 5u);
    EXPECT_EQ(result.resolvedItems[0].item, 7u);  // rewritten from (5,3)
    EXPECT_EQ(result.resolvedItems[1].item, 1u);  // A
}

TEST(ComputeInsertNextPlan, A22_HandleOnlySlotUpgradedToCoordinate) {
    // 语义 11b upgrade path: the queue already holds X as a handle-only slot.
    // Re-inserting it via the `items` form must supply playlist/item coordinates,
    // turning that slot into a coordinate slot so the
    // playback cursor can follow it once consumed -- and it must count as a
    // move, not an insertion.
    std::vector<QueueSlotSnapshot> currentQueue = {
        {kInvalidCoordinate, kInvalidCoordinate, /*handleIndex*/ 9, /*identity*/ 100},  // X, handle only
        {0, 1, kInvalidCoordinate, /*identity*/ 1},                                     // A
    };
    std::vector<InsertNextNewItem> newItems = {
        NewItemAt(/*identity*/ 100, /*handleIndex*/ 50, /*playlist*/ 5, /*item*/ 4)};

    auto result = ComputeInsertNextPlan(currentQueue, newItems, /*position*/ 0, WritableUnlessItemInvalid());

    ASSERT_TRUE(result.ok);
    EXPECT_EQ(result.movedCount, 1u);
    EXPECT_EQ(result.insertedCount, 0u);
    ASSERT_EQ(result.resolvedItems.size(), 2u);
    EXPECT_EQ(result.resolvedItems[0].kind, ResolvedItemKind::Coordinate);
    EXPECT_EQ(result.resolvedItems[0].playlist, 5u);
    EXPECT_EQ(result.resolvedItems[0].item, 4u);
    EXPECT_EQ(result.resolvedItems[1].item, 1u);  // A
}

TEST(ComputeInsertNextPlan, A28_SameIdentityTwoCoordinatesFoldToFirst) {
    // 语义 10: the same track referenced at two playlist positions folds to
    // ONE queue entry. Neither coordinate is already queued here, so the
    // first one in input order wins.
    std::vector<QueueSlotSnapshot> currentQueue = {
        {0, 1, kInvalidCoordinate, /*identity*/ 1},  // A -- unrelated track
    };
    std::vector<InsertNextNewItem> newItems = {
        NewItemAt(/*identity*/ 100, /*handleIndex*/ 50, /*playlist*/ 5, /*item*/ 3),
        NewItemAt(/*identity*/ 100, /*handleIndex*/ 51, /*playlist*/ 5, /*item*/ 7),
    };

    auto result = ComputeInsertNextPlan(currentQueue, newItems, /*position*/ 0, WritableUnlessItemInvalid());

    ASSERT_TRUE(result.ok);
    EXPECT_EQ(result.movedCount, 0u);
    EXPECT_EQ(result.insertedCount, 1u);
    ASSERT_EQ(result.resolvedItems.size(), 2u);
    EXPECT_EQ(result.resolvedItems[0].kind, ResolvedItemKind::Coordinate);
    EXPECT_EQ(result.resolvedItems[0].playlist, 5u);
    EXPECT_EQ(result.resolvedItems[0].item, 3u);  // first occurrence wins
    EXPECT_EQ(result.resolvedItems[1].item, 1u);  // A
}

TEST(ComputeInsertNextPlan, A30_FoldPrefersCoordinateAlreadyInQueue) {
    // 语义 10 folding rule: queue [X@(5,7), A], items:[{5,3},{5,7}]. Folding
    // blindly to the first occurrence would keep (5,3), which then has no
    // coordinate-equal slot and would go through 11b, rewriting the already
    // queued X@(5,7) into X@(5,3). The coordinate that the caller named AND
    // that is already queued must win instead, leaving the slot untouched.
    std::vector<QueueSlotSnapshot> currentQueue = {
        {5, 7, kInvalidCoordinate, /*identity*/ 100},  // X at (5,7)
        {0, 1, kInvalidCoordinate, /*identity*/ 1},    // A
    };
    std::vector<InsertNextNewItem> newItems = {
        NewItemAt(/*identity*/ 100, /*handleIndex*/ 50, /*playlist*/ 5, /*item*/ 3),
        NewItemAt(/*identity*/ 100, /*handleIndex*/ 51, /*playlist*/ 5, /*item*/ 7),
    };

    auto result = ComputeInsertNextPlan(currentQueue, newItems, /*position*/ 0, WritableUnlessItemInvalid());

    ASSERT_TRUE(result.ok);
    EXPECT_EQ(result.movedCount, 1u);
    EXPECT_EQ(result.insertedCount, 0u);
    ASSERT_EQ(result.resolvedItems.size(), 2u);
    EXPECT_EQ(result.resolvedItems[0].kind, ResolvedItemKind::Coordinate);
    EXPECT_EQ(result.resolvedItems[0].playlist, 5u);
    EXPECT_EQ(result.resolvedItems[0].item, 7u);  // NOT rewritten to (5,3)
    EXPECT_EQ(result.resolvedItems[1].item, 1u);  // A
}

TEST(ComputeInsertNextPlan, FoldTakesEarliestOfMultipleCoordinateHits) {
    // 语义 10 tie-break: BOTH coordinates are already queued, so "prefer the
    // one 11a will hit" cannot decide alone -- input order does. Keeping
    // (5,7) means the (5,7) slot moves to the front and the (5,3) slot is
    // left exactly where it was.
    std::vector<QueueSlotSnapshot> currentQueue = {
        {5, 3, kInvalidCoordinate, /*identity*/ 100},  // X at (5,3)
        {5, 7, kInvalidCoordinate, /*identity*/ 100},  // X at (5,7)
    };
    std::vector<InsertNextNewItem> newItems = {
        NewItemAt(/*identity*/ 100, /*handleIndex*/ 50, /*playlist*/ 5, /*item*/ 7),
        NewItemAt(/*identity*/ 100, /*handleIndex*/ 51, /*playlist*/ 5, /*item*/ 3),
    };

    auto result = ComputeInsertNextPlan(currentQueue, newItems, /*position*/ 0, WritableUnlessItemInvalid());

    ASSERT_TRUE(result.ok);
    EXPECT_EQ(result.movedCount, 1u);
    EXPECT_EQ(result.insertedCount, 0u);
    ASSERT_EQ(result.resolvedItems.size(), 2u);
    EXPECT_EQ(result.resolvedItems[0].item, 7u);  // earliest hit, moved
    EXPECT_EQ(result.resolvedItems[1].item, 3u);  // untouched, still behind
}

TEST(ComputeInsertNextPlan, A25_ItemsBlockBeforePathsBlockAndSameIdentityDedups) {
    // 语义 8 + 语义 10: the handler always feeds the `items` block first and
    // the `paths` block second. When both name the same track, one entry
    // survives and it is the coordinate-carrying one, so the cursor still
    // follows it.
    std::vector<QueueSlotSnapshot> currentQueue;
    std::vector<InsertNextNewItem> newItems = {
        NewItemAt(/*identity*/ 100, /*handleIndex*/ 50, /*playlist*/ 5, /*item*/ 4),  // items block
        NewItem(/*identity*/ 100, /*handleIndex*/ 51),                                // paths block
    };

    auto result = ComputeInsertNextPlan(currentQueue, newItems, /*position*/ 0, WritableUnlessItemInvalid());

    ASSERT_TRUE(result.ok);
    EXPECT_EQ(result.movedCount, 0u);
    EXPECT_EQ(result.insertedCount, 1u);
    ASSERT_EQ(result.resolvedItems.size(), 1u);
    EXPECT_EQ(result.resolvedItems[0].kind, ResolvedItemKind::Coordinate);
    EXPECT_EQ(result.resolvedItems[0].playlist, 5u);
    EXPECT_EQ(result.resolvedItems[0].item, 4u);
}

TEST(ComputeInsertNextPlan, FastPath_CoordinateNewItemsResolveAsCoordinateKind) {
    // 语义 12: coordinate-carrying new items are fast-path eligible just like
    // coordinate-less ones, and the tail entries must resolve as Coordinate
    // so the handler writes them through queue_add_item_playlist rather than
    // the handle fallback.
    std::vector<QueueSlotSnapshot> currentQueue = {
        {0, 0, kInvalidCoordinate, /*identity*/ 1},
        {0, 1, kInvalidCoordinate, /*identity*/ 2},
    };
    std::vector<InsertNextNewItem> newItems = {
        NewItemAt(/*identity*/ 10, /*handleIndex*/ 100, /*playlist*/ 5, /*item*/ 3),
        NewItemAt(/*identity*/ 11, /*handleIndex*/ 101, /*playlist*/ 5, /*item*/ 7),
    };

    auto result = ComputeInsertNextPlan(currentQueue, newItems, /*position*/ 999, WritableUnlessItemInvalid());

    ASSERT_TRUE(result.ok);
    EXPECT_EQ(result.movedCount, 0u);
    EXPECT_EQ(result.insertedCount, 2u);
    EXPECT_TRUE(result.canUseAppendFastPath);
    ASSERT_EQ(result.resolvedItems.size(), 4u);
    EXPECT_EQ(result.resolvedItems[2].kind, ResolvedItemKind::Coordinate);
    EXPECT_EQ(result.resolvedItems[2].playlist, 5u);
    EXPECT_EQ(result.resolvedItems[2].item, 3u);
    EXPECT_EQ(result.resolvedItems[3].kind, ResolvedItemKind::Coordinate);
    EXPECT_EQ(result.resolvedItems[3].playlist, 5u);
    EXPECT_EQ(result.resolvedItems[3].item, 7u);
}
