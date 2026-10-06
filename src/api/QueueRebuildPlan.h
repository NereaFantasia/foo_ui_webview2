// QueueRebuildPlan.h - target-sequence computation for the playback queue
// single rebuild unit.
//
// SDK-free by design: no foobar2000 SDK, Win32 or core_api dependency, so
// this header and its .cpp are constructible in the test project, which
// never links against the SDK. Coordinates and handles are represented as
// plain size_t; the caller (QueueApi.cpp) is responsible for translating
// them to/from real t_size / metadb_handle_ptr values and for injecting
// the actual SDK-backed coordinate-writability check.
#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace fb2k_queue {

// Sentinel for "no coordinate" / "no handle". Numerically equal to
// pfc::infinite_size (t_size)(~0) on the caller side, but this module never
// includes pfc headers -- the equality is a documented convention, not a
// compile-time dependency.
constexpr size_t kInvalidCoordinate = static_cast<size_t>(-1);

// One existing (or about-to-exist) queue slot, expressed without any SDK
// type. `handleIndex` is an opaque index into the caller's own handle
// table (e.g. a position in a metadb_handle_list); this module never
// dereferences it, it only tests it against kInvalidCoordinate.
struct QueueSlotSnapshot {
    size_t playlist = kInvalidCoordinate;
    size_t item = kInvalidCoordinate;
    size_t handleIndex = kInvalidCoordinate;

    // Opaque per-track identity, only populated/consulted by
    // ComputeInsertNextPlan (moveToTop / setContents never set or read
    // this). Caller convention:
    // this slot's valid interned m_handle pointer, cast to size_t.
    // Two slots backed by the same valid handle refer to the same playable
    // location -- "there can be only one metadb_handle object referencing
    // specific location" (SDK metadb.h) -- PROVIDED both sides are
    // canonicalized before the handle was created/looked up. This module
    // never dereferences it, only compares it for equality.
    size_t identityKey = kInvalidCoordinate;
};

enum class ResolvedItemKind {
    Coordinate,    // write via queue_add_item_playlist(playlist, item)
    Handle,        // write via queue_add_item(handle) fallback
    Unresolvable,  // neither coordinate nor handle usable
};

struct ResolvedItem {
    ResolvedItemKind kind = ResolvedItemKind::Unresolvable;
    size_t playlist = kInvalidCoordinate;
    size_t item = kInvalidCoordinate;
    size_t handleIndex = kInvalidCoordinate;
};

// Injected by the caller: playlist/item -> writable, per QueueApi.cpp's
// IsCoordinateWritable (checked against the live playlist_manager state).
// This module treats it as an opaque predicate and never re-derives any
// part of the check itself.
using CoordinateWritablePredicate = std::function<bool(size_t playlist, size_t item)>;

struct RebuildPlanResult {
    bool ok = false;
    std::vector<ResolvedItem> resolvedItems;  // meaningful only when ok == true
    std::string error;                         // set when ok == false
};

// Resolves a single queue slot: coordinate form if
// writable, handle form as fallback, else Unresolvable.
ResolvedItem ResolveQueueSlot(const QueueSlotSnapshot& slot,
                               const CoordinateWritablePredicate& isCoordinateWritable);

// Resolves an already-ordered target sequence. Bails out on the first
// Unresolvable slot (result.ok == false, resolvedItems cleared) so the
// caller can honor "any item unresolvable -> fail, queue untouched"
// without touching the queue itself. Callers compute their targetOrder,
// including any deduplication and position adjustment, before calling this.
RebuildPlanResult ResolveTargetSequence(const std::vector<QueueSlotSnapshot>& targetOrder,
                                         const CoordinateWritablePredicate& isCoordinateWritable);

// moveToTop target sequence: slot `index` moves to the front, the rest
// keep their relative order. `index` is expected to already be validated
// by the caller (0 <= index < currentQueue.size()); out-of-range values
// still fail safely here rather than reading out of bounds.
RebuildPlanResult ComputeMoveToTopPlan(const std::vector<QueueSlotSnapshot>& currentQueue,
                                        size_t index,
                                        const CoordinateWritablePredicate& isCoordinateWritable);

// setContents item reference kind (the two accepted reference forms).
// The caller (QueueApi.cpp) is responsible for classifying each JSON
// element into one of these two shapes before building the ref list --
// "any other shape" (e.g. {path:...}) is rejected by the handler itself
// and never reaches this module (shape classification is entirely the handler's job).
enum class SetContentsRefKind {
    QueueReference,  // { queueIndex } -> resolves to currentQueue[queueIndex]
    ListReference,   // { playlist, item } -> passed straight through as a coordinate slot
};

struct SetContentsItemRef {
    SetContentsRefKind kind = SetContentsRefKind::ListReference;
    size_t queueIndex = kInvalidCoordinate;  // meaningful when kind == QueueReference
    size_t playlist = kInvalidCoordinate;    // meaningful when kind == ListReference
    size_t item = kInvalidCoordinate;        // meaningful when kind == ListReference
};

// setContents target-sequence computation. `refs` is the
// already-classified reference list (see SetContentsItemRef above);
// `currentQueue` is the live queue snapshot (same shape ComputeMoveToTopPlan
// consumes) used to resolve queue references. Reference resolution rules:
//   - QueueReference: `queueIndex >= currentQueue.size()` fails the whole
//     call immediately with an error naming `items[i]` (full validation
//     before any flush), queue untouched by construction
//     (this function never mutates anything). Otherwise resolves to
//     currentQueue[queueIndex] verbatim, including its handleIndex --
//     matches ComputeMoveToTopPlan's own coordinate/handle fallback.
//   - ListReference: no handle fallback (handleIndex stays
//     kInvalidCoordinate). Its coordinate writability is precheck'd here
//     (not deferred to ResolveTargetSequence): `!isCoordinateWritable(ref)`
//     fails the whole call immediately with an error naming `items[i]`
//     ("playlist/item 越界" is also a failure that
//     must name the offending item, which ResolveTargetSequence's shared,
//     index-less Unresolvable error cannot do). Only once writable does it
//     become a QueueSlotSnapshot passed to ResolveTargetSequence.
// Duplicate references are preserved literally: this
// function never deduplicates `refs`, mirroring
// ResolveTargetSequence's DuplicateReferencesArePreservedLiterally
// property, which this layer must not break.
RebuildPlanResult ComputeSetContentsPlan(const std::vector<SetContentsItemRef>& refs,
                                          const std::vector<QueueSlotSnapshot>& currentQueue,
                                          const CoordinateWritablePredicate& isCoordinateWritable);

// insertNext new-item entry.
// Two request shapes share this struct, differing in whether they carry
// a coordinate:
//   - `paths` form: no coordinate (playlist / item stay kInvalidCoordinate).
//     A newly inserted item uses `handleIndex` (queue_add_item), without
//     a playlist position for the playback cursor to follow. A match
//     against an existing queue slot preserves that slot's coordinate.
//   - `items` form: a caller-supplied playlist coordinate, already
//     checked for writability and re-verified against the live playlist
//     by the handler after path resolution. It is written through
//     queue_add_item_playlist, so the cursor does follow it.
// `identityKey` is the caller-computed identity of this item's resolved
// handle (see QueueSlotSnapshot::identityKey), used to match it against
// `currentQueue` entries and to deduplicate entries across both request arrays.
struct InsertNextNewItem {
    size_t identityKey = kInvalidCoordinate;
    size_t handleIndex = kInvalidCoordinate;
    // `items` form only; both stay kInvalidCoordinate for the `paths` form.
    size_t playlist = kInvalidCoordinate;
    size_t item = kInvalidCoordinate;
};

struct InsertNextPlanResult {
    bool ok = false;
    std::vector<ResolvedItem> resolvedItems;  // full target sequence; meaningful only when ok == true
    // Existing queue items selected for relocation, at most one per identity.
    // Prefer the frontmost coordinate-and-identity match, then the frontmost
    // identity match. Other duplicates keep their contents and relative order.
    size_t movedCount = 0;
    size_t insertedCount = 0;                  // brand-new items with no existing match
    // True only when nothing needed to move (no identity
    // intersection with currentQueue) AND the effective insertion point
    // is at/after the tail -- i.e. the whole call degenerates to a plain
    // append. The caller may then skip RebuildQueue entirely and go
    // straight to queue_add_item_playlist() or queue_add_item(), according
    // to each resolved item's kind, in insertion order. Requires ok == true.
    bool canUseAppendFastPath = false;
    std::string error;  // set when ok == false
};

// insertNext target-sequence computation. `currentQueue` is
// the live queue snapshot (same shape ComputeMoveToTopPlan /
// ComputeSetContentsPlan consume, but this is the only caller that reads
// identityKey). `newItems` is the already-resolved, order-preserving list
// built by the handler from BOTH request arrays -- the `items` block first,
// then the `paths` block. Path resolution uses `p_filter = false` to preserve
// order and duplicates; this function performs identity deduplication.
// The handler rejects invalid item coordinates as a whole request and
// rechecks their handle identities after modal path resolution, before
// taking the queue snapshot. No queue writes occur on validation failure.
// `position` indexes the queue after matched slots are removed; values at
// or beyond that queue's length append to its tail.
//
// Algorithm (pure, no SDK):
//   1. Fold `newItems` by identityKey down to one entry per identity,
//      keeping the FIRST occurrence's position in the
//      sequence. Which entry's coordinate survives is not simply "the
//      first": if some entry in the identity group carries a coordinate
//      that an existing `currentQueue` slot already matches exactly
//      (playlist + item + identityKey), that entry wins, because it is
//      the one step 2 will match without changing its coordinate. Ties go
//      to the earliest such entry in input order. With no such
//      entry, the first occurrence wins. Folding blindly to the first
//      occurrence could rewrite the coordinate of a slot the
//      caller explicitly named and that is already queued.
//   2. For each folded new item, claim at most ONE unconsumed
//      `currentQueue` slot, in two ordered levels:
//        (a) if the new item carries a coordinate: the frontmost
//            unconsumed slot whose (playlist, item) AND identityKey both
//            equal the new item's. The slot moves verbatim, coordinate
//            untouched. Checking identityKey as well keeps
//            a stale slot (coordinate equal, different track) from being
//            mistaken for a match.
//        (b) if no coordinate-and-identity match exists: the frontmost
//            unconsumed slot whose identityKey equals the new item's.
//            A coordinate-carrying new item
//            rewrites the claimed slot's coordinate to its own -- a
//            handle-only slot is upgraded to a coordinate slot, a slot
//            pointing elsewhere is repointed, since supplying a
//            coordinate is the caller stating "from now on this entry
//            follows that playlist position". A coordinate-less new item
//            (`paths` form) reuses the claimed slot verbatim. Keep the
//            matched slot's handleIndex; if it is missing when a coordinate
//            is rewritten, use the same-identity new item's handleIndex.
//      Either level counts into `movedCount`. Any OTHER currentQueue
//      slots sharing that identity keep their contents and relative order
//      in `remaining`, preserving any repeated playback already queued.
//      This keeps `movedCount` bounded by the folded new-item count
//      (at most 1 per identity). If
//      neither level matches, it is an "insertion": a slot carrying the
//      new item's own coordinate (kInvalidCoordinate for the `paths`
//      form), `handleIndex` and identity is built, and counted into
//      `insertedCount`.
//   3. `remaining` = currentQueue slots that were NOT matched by any
//      folded new item, in original relative order.
//   4. effective position = min(position, remaining.size()).
//   5. Splice the moved-or-inserted groups into `remaining` at the effective
//      position, in folded input order: the items block, then the paths block.
//   6. The combined targetOrder is resolved via the same shared
//      ResolveTargetSequence step ComputeMoveToTopPlan /
//      ComputeSetContentsPlan use (coordinate-priority, handle-fallback,
//      fail-the-whole-call on any unresolvable slot) -- no duplicate
//      resolution logic for this entry point.
//   7. canUseAppendFastPath = (movedCount == 0) && (position >=
//      currentQueue.size()) -- equivalent to "no intersection AND
//      effective position lands at the original tail", since zero moves
//      means `remaining.size() == currentQueue.size()`.
InsertNextPlanResult ComputeInsertNextPlan(const std::vector<QueueSlotSnapshot>& currentQueue,
                                            const std::vector<InsertNextNewItem>& newItems,
                                            size_t position,
                                            const CoordinateWritablePredicate& isCoordinateWritable);

}  // namespace fb2k_queue
