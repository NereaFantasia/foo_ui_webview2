#include "pch.h"
#include "api/QueueRebuildPlan.h"

namespace fb2k_queue {

namespace {

// items 形态带播放列表坐标，优先按坐标和身份匹配；paths 形态仅按身份匹配。
bool NewItemHasCoordinate(const InsertNextNewItem& item) {
    return item.playlist != kInvalidCoordinate && item.item != kInvalidCoordinate;
}

// 找到最靠前的未消费队列槽，要求 (playlist, item) 与 identityKey 均匹配；
// 找不到返回 kInvalidCoordinate。空 consumed 表示全部未消费，供新项去重
// 时选择坐标使用，使去重与后续匹配采用相同判据。
size_t FindCoordinateEqualSlot(const std::vector<QueueSlotSnapshot>& currentQueue,
                                const std::vector<bool>& consumed,
                                const InsertNextNewItem& item) {
    if (!NewItemHasCoordinate(item)) {
        return kInvalidCoordinate;
    }
    for (size_t i = 0; i < currentQueue.size(); ++i) {
        if (!consumed.empty() && consumed[i]) {
            continue;
        }
        if (currentQueue[i].playlist == item.playlist
            && currentQueue[i].item == item.item
            && currentQueue[i].identityKey == item.identityKey) {
            return i;
        }
    }
    return kInvalidCoordinate;
}

}  // namespace

ResolvedItem ResolveQueueSlot(const QueueSlotSnapshot& slot,
                               const CoordinateWritablePredicate& isCoordinateWritable) {
    if (isCoordinateWritable && isCoordinateWritable(slot.playlist, slot.item)) {
        ResolvedItem resolved;
        resolved.kind = ResolvedItemKind::Coordinate;
        resolved.playlist = slot.playlist;
        resolved.item = slot.item;
        return resolved;
    }

    if (slot.handleIndex != kInvalidCoordinate) {
        ResolvedItem resolved;
        resolved.kind = ResolvedItemKind::Handle;
        resolved.handleIndex = slot.handleIndex;
        return resolved;
    }

    return ResolvedItem{};  // kind defaults to Unresolvable
}

RebuildPlanResult ResolveTargetSequence(const std::vector<QueueSlotSnapshot>& targetOrder,
                                         const CoordinateWritablePredicate& isCoordinateWritable) {
    RebuildPlanResult result;
    result.resolvedItems.reserve(targetOrder.size());

    for (const auto& slot : targetOrder) {
        ResolvedItem resolved = ResolveQueueSlot(slot, isCoordinateWritable);
        if (resolved.kind == ResolvedItemKind::Unresolvable) {
            result.ok = false;
            result.resolvedItems.clear();
            result.error = "unresolvable queue item: neither coordinate nor handle usable";
            return result;
        }
        result.resolvedItems.push_back(resolved);
    }

    result.ok = true;
    return result;
}

RebuildPlanResult ComputeMoveToTopPlan(const std::vector<QueueSlotSnapshot>& currentQueue,
                                        size_t index,
                                        const CoordinateWritablePredicate& isCoordinateWritable) {
    if (index >= currentQueue.size()) {
        RebuildPlanResult result;
        result.ok = false;
        result.error = "index out of range";
        return result;
    }

    std::vector<QueueSlotSnapshot> targetOrder;
    targetOrder.reserve(currentQueue.size());
    targetOrder.push_back(currentQueue[index]);
    for (size_t i = 0; i < currentQueue.size(); ++i) {
        if (i != index) {
            targetOrder.push_back(currentQueue[i]);
        }
    }

    return ResolveTargetSequence(targetOrder, isCoordinateWritable);
}

RebuildPlanResult ComputeSetContentsPlan(const std::vector<SetContentsItemRef>& refs,
                                          const std::vector<QueueSlotSnapshot>& currentQueue,
                                          const CoordinateWritablePredicate& isCoordinateWritable) {
    std::vector<QueueSlotSnapshot> targetOrder;
    targetOrder.reserve(refs.size());

    for (size_t i = 0; i < refs.size(); ++i) {
        const SetContentsItemRef& ref = refs[i];

        if (ref.kind == SetContentsRefKind::QueueReference) {
            if (ref.queueIndex >= currentQueue.size()) {
                RebuildPlanResult result;
                result.ok = false;
                result.error = "items[" + std::to_string(i) + "]: queueIndex out of range";
                return result;
            }
            // Verbatim copy, including handleIndex -- a queue reference to a
            // half-degraded slot must still be able to fall
            // back to its handle the same way ComputeMoveToTopPlan does.
            targetOrder.push_back(currentQueue[ref.queueIndex]);
            continue;
        }

        // ListReference: no handle fallback by design (the caller supplied this
        // coordinate explicitly; if it is no longer writable the input is stale and
        // the call must fail, not silently substitute a handle the caller never
        // asked for).
        //
        // Indexed precheck: "playlist/item
        // 越界" is one of the three enumerated failure reasons that must
        // name `items[i]` in the error. ResolveTargetSequence's shared
        // Unresolvable error has no index, so a list reference's
        // writability is checked here, before it ever reaches that shared
        // step -- queue references are deliberately NOT precheck'd here,
        // since their half-degraded-coordinate-falls-back-to-handle path
        // is a legitimate outcome that ResolveTargetSequence must still
        // decide (see the QueueReference branch above).
        if (!isCoordinateWritable || !isCoordinateWritable(ref.playlist, ref.item)) {
            RebuildPlanResult result;
            result.ok = false;
            result.error = "items[" + std::to_string(i) + "]: playlist/item out of range or stale";
            return result;
        }

        QueueSlotSnapshot slot;
        slot.playlist = ref.playlist;
        slot.item = ref.item;
        targetOrder.push_back(slot);
    }

    return ResolveTargetSequence(targetOrder, isCoordinateWritable);
}

InsertNextPlanResult ComputeInsertNextPlan(const std::vector<QueueSlotSnapshot>& currentQueue,
                                            const std::vector<InsertNextNewItem>& newItems,
                                            size_t position,
                                            const CoordinateWritablePredicate& isCoordinateWritable) {
    // 步骤 1：按 identityKey 去重，保留各身份首次出现的位置。
    // 同身份组内优先保留与既有队列槽坐标、身份均匹配的项；多个匹配项取
    // 输入顺序最先的，没有匹配项则取首项。否则队列已有 X@(5,7) 时，输入
    // [(5,3),(5,7)] 会错误地选中 (5,3)，在后续身份匹配中改写已有坐标。
    std::vector<InsertNextNewItem> folded;
    folded.reserve(newItems.size());
    const std::vector<bool> nothingConsumedYet;
    for (const auto& candidate : newItems) {
        size_t keptIndex = kInvalidCoordinate;
        for (size_t d = 0; d < folded.size(); ++d) {
            if (folded[d].identityKey == candidate.identityKey) {
                keptIndex = d;
                break;
            }
        }
        if (keptIndex == kInvalidCoordinate) {
            folded.push_back(candidate);
            continue;
        }
        if (FindCoordinateEqualSlot(currentQueue, nothingConsumedYet, folded[keptIndex])
            != kInvalidCoordinate) {
            continue;  // 已保留最先匹配既有坐标的项，不再替换。
        }
        if (FindCoordinateEqualSlot(currentQueue, nothingConsumedYet, candidate)
            != kInvalidCoordinate) {
            folded[keptIndex] = candidate;
        }
    }

    // 步骤 2：每个去重后的新项至多匹配一个未消费队列槽，优先取坐标和
    // 身份均相等的最靠前槽，未命中再取身份相等的最靠前槽。
    // 同身份的其余重复项保留在 remaining 中，保持相对顺序，避免改变用户
    // 安排的重复播放。consumed 记录已匹配槽，movedCount 至多为去重后的新项数。
    std::vector<bool> consumed(currentQueue.size(), false);
    std::vector<std::vector<QueueSlotSnapshot>> perNewItemGroups(folded.size());
    size_t movedCount = 0;
    size_t insertedCount = 0;

    for (size_t k = 0; k < folded.size(); ++k) {
        const InsertNextNewItem& newItem = folded[k];

        // 无坐标新项不参与坐标匹配。
        size_t matchedIndex = FindCoordinateEqualSlot(currentQueue, consumed, newItem);
        const bool coordinateExactMatch = matchedIndex != kInvalidCoordinate;

        if (!coordinateExactMatch) {
            // 坐标未匹配时，按身份取最靠前的未消费槽。
            for (size_t i = 0; i < currentQueue.size(); ++i) {
                if (!consumed[i] && currentQueue[i].identityKey == newItem.identityKey) {
                    matchedIndex = i;
                    break;
                }
            }
        }

        if (matchedIndex != kInvalidCoordinate) {
            consumed[matchedIndex] = true;
            QueueSlotSnapshot slot = currentQueue[matchedIndex];
            if (!coordinateExactMatch && NewItemHasCoordinate(newItem)) {
                // 仅身份匹配时采用新项坐标，使播放游标跟随调用方指定的
                // 列表位置；无坐标新项则原样复用匹配槽。
                // 保留匹配槽的 handle；缺失时取同身份新项的 handle 作回退。
                slot.playlist = newItem.playlist;
                slot.item = newItem.item;
                if (slot.handleIndex == kInvalidCoordinate) {
                    slot.handleIndex = newItem.handleIndex;
                }
            }
            perNewItemGroups[k].push_back(slot);
            movedCount++;
        } else {
            // 队列里没有同身份项：全新项。items 形态带着调用方给的坐标入
            // 队；paths 形态的坐标保持 kInvalidCoordinate，由步骤 6 解析成
            // handle 形态。
            QueueSlotSnapshot freshSlot;
            freshSlot.playlist = newItem.playlist;
            freshSlot.item = newItem.item;
            freshSlot.handleIndex = newItem.handleIndex;
            freshSlot.identityKey = newItem.identityKey;
            perNewItemGroups[k].push_back(freshSlot);
            insertedCount++;
        }
    }

    // 步骤 3：未匹配的队列槽保留在 remaining 中，保持原有相对顺序。
    std::vector<QueueSlotSnapshot> remaining;
    remaining.reserve(currentQueue.size());
    for (size_t i = 0; i < currentQueue.size(); ++i) {
        if (!consumed[i]) {
            remaining.push_back(currentQueue[i]);
        }
    }

    // 步骤 4：position 以移除匹配槽后的队列为基准，达到或超过长度时追加到尾部。
    size_t effectivePosition = position < remaining.size() ? position : remaining.size();

    // 步骤 5：按 newItems 顺序插入移动项或新增项，即 items 块在前、
    // paths 块在后，插入点为 remaining 中的 effectivePosition。
    std::vector<QueueSlotSnapshot> targetOrder;
    targetOrder.reserve(remaining.size() + newItems.size());
    targetOrder.insert(targetOrder.end(), remaining.begin(), remaining.begin() + effectivePosition);
    for (const auto& group : perNewItemGroups) {
        targetOrder.insert(targetOrder.end(), group.begin(), group.end());
    }
    targetOrder.insert(targetOrder.end(), remaining.begin() + effectivePosition, remaining.end());

    // 步骤 6：共用坐标优先、handle 回退的解析；
    // 任一项的坐标和 handle 都不可用则整批失败。
    RebuildPlanResult resolved = ResolveTargetSequence(targetOrder, isCoordinateWritable);

    InsertNextPlanResult result;
    result.ok = resolved.ok;
    result.resolvedItems = std::move(resolved.resolvedItems);
    result.error = std::move(resolved.error);
    result.movedCount = movedCount;
    result.insertedCount = insertedCount;
    // 步骤 7：仅无移动且追加到尾部时可跳过重建，任何移动都必须重建。
    result.canUseAppendFastPath = (movedCount == 0) && (position >= currentQueue.size());
    return result;
}

}  // namespace fb2k_queue
