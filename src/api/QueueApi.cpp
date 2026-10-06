/**
 * QueueApi.cpp - Playback Queue API
 *
 * Provides APIs for foobar2000's native playback queue functionality.
 * Allows "Play Next" and queue management without maintaining frontend state.
 *
 * Also includes JIT Queue API for streaming media with dual-layer architecture.
 *
 * Shapes are declared in src/api/schema/queue.ts and src/api/schema/jitQueue.ts;
 * the structs and parameter parsing come from the generated headers.
 */

#include "pch.h"
#include "api/QueueApi.h"
#include "api/ApiConstants.h"
#include "api/PlaylistApi.h"
#include "api/PlaylistLock.h"
#include "api/PlaylistTarget.h"
#include "api/QueueRebuildPlan.h"
#include "api/TrackRow.h"
#include "api/TypedApi.h"
#include "api/generated/JitQueueSchema.h"
#include "api/generated/QueueSchema.h"
#include "callbacks/QueueCallback.h"
#include "core/QueueManager.h"
#include "utils/SubsongUtils.h"

namespace {
    namespace queue = api::queue;
    namespace jitq = api::jitQueue;

    struct ParsedPlayablePath {
        std::string path;
        t_uint32 subsong = 0;
        bool hasSubsong = false;
    };

    // 解析 path|subsong:N，避免把后缀当成普通路径文本
    ParsedPlayablePath ParsePlayablePath(const std::string& input) {
        ParsedPlayablePath result;
        result.path = input;

        size_t pos = input.find("|subsong:");
        if (pos == std::string::npos) {
            return result;
        }

        result.hasSubsong = true;
        result.path = input.substr(0, pos);
        try {
            result.subsong = static_cast<t_uint32>(std::stoul(input.substr(pos + 9)));
        } catch (...) {
            result.subsong = 0;
        }
        return result;
    }

    void ResolveQueuePathsToHandles(const std::vector<std::string>& paths, metadb_handle_list& outItems, size_t& invalidCount) {
        auto piif = playlist_incoming_item_filter::get();

        // 批量收集普通路径，一次性调用 process_locations（避免弹窗风暴）
        pfc::string_list_impl batchPaths;
        metadb_handle_list subsongHandles;

        for (const std::string& pathValue : paths) {
            ParsedPlayablePath parsed = ParsePlayablePath(pathValue);
            if (parsed.path.empty()) {
                invalidCount++;
                continue;
            }

            // Reject URLs longer than the streaming-path limit; otherwise
            // foo_httpstream.dll throws ERROR_BUFFER_OVERFLOW deep inside
            // playback and surfaces a native fb2k dialog.
            if (parsed.path.length() > ApiLimits::MAX_STREAM_URL_LENGTH) {
                invalidCount++;
                continue;
            }

            if (parsed.hasSubsong) {
                // 创建 subsong handle 前先规范化路径，使同一播放位置与
                // 普通路径解析结果具有一致的 metadb 身份。
                metadb_handle_ptr handle = SubsongUtils::CreateCanonicalHandle(parsed.path, parsed.subsong);
                if (handle.is_valid()) {
                    subsongHandles.add_item(handle);
                } else {
                    invalidCount++;
                }
                continue;
            }

            batchPaths.add_item(parsed.path.c_str());
        }

        // 一次性解析所有普通路径
        metadb_handle_list batchResolved;
        if (batchPaths.get_count() > 0) {
            piif->process_locations(batchPaths, batchResolved, true, nullptr, nullptr, core_api::get_main_window());
            if (batchResolved.get_count() == 0) {
                invalidCount += batchPaths.get_count();
            }
        }

        // 合并结果：subsong handles 在前，batch 结果在后
        outItems += subsongHandles;
        outItems += batchResolved;
    }

    // queue.insertNext 专用解析结果，携带身份 key（供
    // fb2k_queue::ComputeInsertNextPlan 做去重与"已在队列中"匹配）。
    struct InsertNextResolvedPath {
        size_t identityKey = 0;
        metadb_handle_ptr handle;
    };

    // insertNext 按输入顺序解析路径：
    //   1. process_locations 传 p_filter=false，保留顺序和重复项，身份去重
    //      由 ComputeInsertNextPlan 完成。
    //   2. 仅将连续裸路径合批，遇到 subsong 项先输出此前的批次，再输出该项。
    //      不采用 queue.addPaths 的 subsong 前置合并方式，避免打乱混排顺序。
    //      一条裸路径展开出的多个 handle 保留在该路径对应的位置。
    //   3. subsong 路径通过共享 CreateCanonicalHandle 先规范化再创建句柄，
    //      与普通路径解析结果使用一致的播放位置身份。
    // invalidCount 只统计 paths，由调用方取输入数与解析 handle 数之差，
    // 最低为 0。路径可展开为 0/1/N 个 handle，因此该计数不能归属到逐条路径。
    void ResolveInsertNextPaths(const std::vector<std::string>& paths, std::vector<InsertNextResolvedPath>& outResolved) {
        auto piif = playlist_incoming_item_filter::get();

        pfc::string_list_impl runBatch;
        auto flushRun = [&]() {
            if (runBatch.get_count() == 0) {
                return;
            }
            metadb_handle_list resolved;
            piif->process_locations(runBatch, resolved, /*p_filter=*/false, nullptr, nullptr,
                                     core_api::get_main_window());
            for (size_t i = 0; i < resolved.get_count(); i++) {
                InsertNextResolvedPath item;
                item.handle = resolved[i];
                item.identityKey = reinterpret_cast<size_t>(item.handle.get_ptr());
                outResolved.push_back(item);
            }
            runBatch.remove_all();
        };

        for (const std::string& pathValue : paths) {
            ParsedPlayablePath parsed = ParsePlayablePath(pathValue);
            if (parsed.path.empty()) {
                continue;
            }

            if (parsed.path.length() > ApiLimits::MAX_STREAM_URL_LENGTH) {
                continue;
            }

            if (parsed.hasSubsong) {
                // 先 flush 掉之前攒的连续裸路径段——它在原始数组中出现
                // 得更早，必须排在本 subsong 项前面（保序）。
                flushRun();

                // 与 queue.addPaths 共用路径规范化，避免同一播放位置产生不同身份。
                metadb_handle_ptr handle = SubsongUtils::CreateCanonicalHandle(parsed.path, parsed.subsong);
                if (handle.is_valid()) {
                    InsertNextResolvedPath item;
                    item.handle = handle;
                    item.identityKey = reinterpret_cast<size_t>(handle.get_ptr());
                    outResolved.push_back(item);
                }
                continue;
            }

            runBatch.add_item(parsed.path.c_str());
        }

        flushRun();
    }

    //==========================================================================
    // Helper: Find or create the queue-dedicated playlist
    //==========================================================================
    size_t GetOrCreateQueuePlaylist() {
        auto plm = playlist_manager::get();
        size_t count = plm->get_playlist_count();

        // Look for existing queue playlist
        for (size_t i = 0; i < count; i++) {
            pfc::string8 name;
            plm->playlist_get_name(i, name);
            if (strcmp(name.c_str(), QUEUE_PLAYLIST_NAME) == 0) {
                return i;
            }
        }

        // Create new queue playlist at the end
        size_t newIndex = plm->create_playlist(QUEUE_PLAYLIST_NAME, pfc::infinite_size, pfc::infinite_size);
        return newIndex;
    }

    //==========================================================================
    // Helper: one queue entry from a t_playback_queue_item
    //==========================================================================
    queue::QueueItem QueueItemOf(const t_playback_queue_item& item, size_t queueIndex) {
        queue::QueueItem row;
        // The shared track row carries the identity and metadata; a handle
        // without loaded information yields empty metadata, as everywhere else.
        if (item.m_handle.is_valid()) {
            static_cast<api::common::Track&>(row) = BuildTrackRow(item.m_handle);
        }
        row.queueIndex = static_cast<std::int64_t>(queueIndex);
        // 坐标只在它仍指向这首曲目时报出，否则 playlist / playlistGuid / playlistItem 三键一并
        // 写 null（键必有）：内核记的坐标在行或列表被删、各行挪动之后可能已越界，或落在另一首
        // 曲目上，照报会让页面把队列项对到错的行。
        if (item.m_playlist != pfc::infinite_size && item.m_item != pfc::infinite_size
            && item.m_handle.is_valid()) {
            auto plm = playlist_manager::get();
            metadb_handle_ptr atCoordinate;
            if (item.m_playlist < plm->get_playlist_count()
                && plm->playlist_get_item_handle(atCoordinate, item.m_playlist, item.m_item)
                && atCoordinate == item.m_handle) {
                row.playlist = static_cast<std::int64_t>(item.m_playlist);
                row.playlistGuid = api::PlaylistGuidOf(*GetPlaylistService(), item.m_playlist);
                row.playlistItem = static_cast<std::int64_t>(item.m_item);
            }
        }
        return row;
    }

    std::int64_t CurrentQueueCount() {
        return static_cast<std::int64_t>(playlist_manager::get()->queue_get_count());
    }

    //==========================================================================
    // queue.get - Get current playback queue contents
    //==========================================================================
    api::Result<queue::GetResult> QueueGet(const queue::GetParams& /*params*/) {
        auto plm = playlist_manager::get();

        pfc::list_t<t_playback_queue_item> queueItems;
        plm->queue_get_contents(queueItems);

        queue::GetResult result;
        const size_t count = queueItems.get_count();
        result.items.reserve(count);
        for (size_t i = 0; i < count; i++) {
            result.items.push_back(QueueItemOf(queueItems[i], i));
        }
        result.count = static_cast<std::int64_t>(count);
        return result;
    }

    //==========================================================================
    // queue.add - Add playlist items to queue
    //==========================================================================
    api::Result<queue::AddResult> QueueAdd(const queue::AddParams& params) {
        auto plm = playlist_manager::get();

        size_t playlistIndex = 0;
        if (auto failure = api::ResolvePlaylistTarget(*GetPlaylistService(), params.playlist,
                                                      params.playlistGuid, playlistIndex)) {
            return std::move(*failure);
        }

        if (!params.tracks.has_value() && !params.track.has_value()) {
            return api::Fail("tracks or track is required", ApiErrorCode::INVALID_PARAMS);
        }

        const size_t itemCount = plm->playlist_get_item_count(playlistIndex);
        size_t addedCount = 0;

        if (params.tracks.has_value()) {
            // Add by track indices. 本段可能逐项触发多次内核 on_changed（每次
            // queue_add_item_playlist 一次），用抑制器把它们合并为析构时
            // 的 1 次广播，origin 透传抑制期内最后一次记录值（均为
            // user_added，语义正确）。
            QueueBroadcastSuppressor suppressor;
            for (const std::int64_t trackIdx : *params.tracks) {
                if (trackIdx >= 0 && static_cast<size_t>(trackIdx) < itemCount) {
                    plm->queue_add_item_playlist(playlistIndex, static_cast<size_t>(trackIdx));
                    addedCount++;
                }
            }
        } else {
            // Add single track
            const size_t idx = static_cast<size_t>(*params.track);
            if (idx < itemCount) {
                plm->queue_add_item_playlist(playlistIndex, idx);
                addedCount = 1;
            }
        }

        if (addedCount == 0) {
            return api::Fail("No track index is in range", ApiErrorCode::INVALID_INDEX,
                             {{"addedCount", 0}, {"queueCount", CurrentQueueCount()}});
        }

        queue::AddResult result;
        result.addedCount = static_cast<std::int64_t>(addedCount);
        result.queueCount = CurrentQueueCount();
        return result;
    }

    //==========================================================================
    // queue.addPaths - Add paths/URLs to queue (combo API)
    // Internally: add to playlist -> add to queue
    //==========================================================================
    api::Result<queue::AddPathsResult> QueueAddPaths(const queue::AddPathsParams& params) {
        auto plm = playlist_manager::get();

        // Determine target playlist
        size_t playlistIndex;
        if (params.useQueuePlaylist) {
            playlistIndex = GetOrCreateQueuePlaylist();
        } else if (auto failure = api::ResolvePlaylistTarget(*GetPlaylistService(), params.playlist,
                                                             params.playlistGuid, playlistIndex)) {
            return std::move(*failure);
        }

        if (plm->playlist_lock_is_present(playlistIndex)) {
            return PlaylistLocked(playlistIndex, "queue.addPaths");
        }

        // Add to playlist, path|subsong:N 走 handle_create，普通路径走 process_locations
        const api::PinnedPlaylist target(*GetPlaylistService(), playlistIndex);
        metadb_handle_list items;
        size_t invalidCount = 0;
        ResolveQueuePathsToHandles(params.paths, items, invalidCount);

        // 解析时开过模态进度框，列表可能已被拖动或删掉；按 GUID 找回，删掉了就不写。一首都没
        // 解析出来也先找回，被删掉时报删掉而不是报没有曲目，与 playlist.addPaths 一致。
        const auto located = target.Locate(*GetPlaylistService());
        if (!located.has_value()) {
            return api::Fail("The playlist was removed while the paths were being resolved",
                             ApiErrorCode::OPERATION_FAILED, {{"invalidCount", invalidCount}});
        }
        playlistIndex = *located;

        if (items.get_count() == 0) {
            return api::Fail("No valid tracks found", ApiErrorCode::NOT_FOUND,
                             {{"invalidCount", invalidCount}});
        }

        // 插入点（列表末尾）紧挨插入再取，不跨过上面的路径解析：process_locations 带父窗口，
        // 可能弹出界面，不保证这段时间里列表长度不变。
        size_t insertPos = plm->playlist_get_item_count(playlistIndex);

        // Undo backup before modification
        plm->playlist_undo_backup(playlistIndex);

        // 整批插入或整批拒绝，拒绝时返回 SIZE_MAX；此时 insertPos 起没有新曲目，不能入队。
        if (plm->playlist_insert_items(playlistIndex, insertPos, items, bit_array_false()) == SIZE_MAX) {
            if (plm->playlist_lock_is_present(playlistIndex)) {
                return PlaylistLocked(playlistIndex, "queue.addPaths");
            }
            return api::Fail("Failed to add tracks to the playlist", ApiErrorCode::OPERATION_FAILED);
        }

        // Add the newly inserted items to queue
        // 同 queue.add 的 tracks 循环，用抑制器把逐项广播
        // 合并为析构时的 1 次，origin 透传抑制期内最后一次记录值（均为
        // user_added，语义正确）。
        size_t addedCount = items.get_count();
        {
            QueueBroadcastSuppressor suppressor;
            for (size_t i = 0; i < addedCount; i++) {
                plm->queue_add_item_playlist(playlistIndex, insertPos + i);
            }
        }

        queue::AddPathsResult result;
        result.addedCount = static_cast<std::int64_t>(addedCount);
        result.invalidCount = static_cast<std::int64_t>(invalidCount);
        result.playlist = static_cast<std::int64_t>(playlistIndex);
        result.playlistGuid = api::PlaylistGuidOf(*GetPlaylistService(), playlistIndex);
        result.queueCount = CurrentQueueCount();
        return result;
    }

    //==========================================================================
    // queue.remove - Remove item from queue by index
    //==========================================================================
    api::Result<queue::RemoveResult> QueueRemove(const queue::RemoveParams& params) {
        auto plm = playlist_manager::get();

        size_t queueCount = plm->queue_get_count();
        if (queueCount == 0) {
            return api::Fail("Queue is empty", ApiErrorCode::NOT_FOUND);
        }

        // Remove by index
        if (params.index.has_value()) {
            const size_t index = static_cast<size_t>(*params.index);
            if (index >= queueCount) {
                return api::Fail("Invalid queue index", ApiErrorCode::INVALID_INDEX);
            }

            // Create bit array with only the specified index set
            pfc::bit_array_one mask(index);
            plm->queue_remove_mask(mask);

            queue::RemoveResult result;
            result.removedIndex = static_cast<std::int64_t>(index);
            result.queueCount = CurrentQueueCount();
            return result;
        }

        // Remove by indices array
        if (params.indices.has_value()) {
            pfc::bit_array_bittable mask(queueCount);
            size_t removeCount = 0;

            for (const std::int64_t idx : *params.indices) {
                if (idx < 0) continue;
                const size_t index = static_cast<size_t>(idx);
                if (index < queueCount && !mask.get(index)) {
                    mask.set(index, true);
                    removeCount++;
                }
            }

            if (removeCount == 0) {
                return api::Fail("No queue index is in range", ApiErrorCode::INVALID_INDEX,
                                 {{"removedCount", 0}, {"queueCount", queueCount}});
            }
            plm->queue_remove_mask(mask);

            queue::RemoveResult result;
            result.removedCount = static_cast<std::int64_t>(removeCount);
            result.queueCount = CurrentQueueCount();
            return result;
        }

        return api::Fail("index or indices is required", ApiErrorCode::INVALID_PARAMS);
    }

    //==========================================================================
    // queue.clear / queue.flush - Clear entire queue
    //==========================================================================
    std::int64_t ClearQueue() {
        auto plm = playlist_manager::get();
        size_t previousCount = plm->queue_get_count();
        plm->queue_flush();
        return static_cast<std::int64_t>(previousCount);
    }

    api::Result<queue::ClearResult> QueueClear(const queue::ClearParams& /*params*/) {
        queue::ClearResult result;
        result.clearedCount = ClearQueue();
        return result;
    }

    // The older name of queue.clear; both stay registered.
    api::Result<queue::FlushResult> QueueFlush(const queue::FlushParams& /*params*/) {
        queue::FlushResult result;
        result.clearedCount = ClearQueue();
        return result;
    }

    //==========================================================================
    // queue.getCount - Get queue item count
    //==========================================================================
    api::Result<queue::GetCountResult> QueueGetCount(const queue::GetCountParams& /*params*/) {
        auto plm = playlist_manager::get();
        queue::GetCountResult result;
        result.count = static_cast<std::int64_t>(plm->queue_get_count());
        result.hasItems = plm->queue_is_active();
        return result;
    }

    //==========================================================================
    // 单一重建单元：坐标可写性判据
    // 不能简化成只判 != infinite_size —— 列表已被截断但坐标数值仍"看起来有效"的情形
    // （m_playlist 有效但已超出该列表现有项数）同样要拦住，否则会静默丢项。
    //==========================================================================
    bool IsCoordinateWritable(t_size playlist, t_size item) {
        auto plm = playlist_manager::get();
        return playlist != pfc::infinite_size
            && item != pfc::infinite_size
            && playlist < plm->get_playlist_count()
            && item < plm->playlist_get_item_count(playlist);
    }

    // setContents 与 insertNext 按 GUID 给出的行：换成那张列表现在的序号。失败码取
    // ResolvePlaylistTarget 的（两者都给或格式不对 INVALID_PARAMS，找不到 NOT_FOUND），报错
    // 文字前缀出错项的下标，与两处其余的逐项失败同形，也带上 queueCount。只在给了 GUID 时
    // 调用：按序号给出的行仍走原来的坐标判据，越界的报错不变。
    std::optional<api::Failure> ResolveRefPlaylist(size_t entryIndex, const std::optional<std::int64_t>& playlist,
                                                   const std::optional<std::string>& playlistGuid, size_t& out,
                                                   size_t queueCount) {
        auto failure = api::ResolvePlaylistTarget(*GetPlaylistService(), playlist, playlistGuid, out,
                                                  api::PlaylistOmitted::Refuse);
        if (!failure) return std::nullopt;
        failure->error = "items[" + std::to_string(entryIndex) + "]: " + failure->error;
        failure->extra["queueCount"] = queueCount;
        return failure;
    }

    struct RebuildQueueResult {
        bool success = false;
        size_t queueCount = 0;
        std::string error;
    };

    // 按已解析的目标序列清空、写入并校验队列。排序、去重和 position 换算
    // 由调用方通过 fb2k_queue 纯函数完成；plan 中的 handleIndex 引用传入的
    // handles 表，此处不重新计算目标顺序。
    //
    // 广播抑制器覆盖 setContents、insertNext 重建分支、playNow(index>0)
    // 和 moveToTop。析构广播的 origin 固定为 "unknown"：
    // 重建语义是移动/重排，抑制期内核给出的 user_added 对这四个入口都是
    // 误导性描述，不得透传。playNow 的 start(track_command_next) 在本函
    // 数返回之后才调用，天然落在抑制器作用域外，advance 通知不会被合并
    // （该调用序由 playNow 自身保证，本函数不改变这一点）。
    RebuildQueueResult RebuildQueue(const fb2k_queue::RebuildPlanResult& plan,
                                     const metadb_handle_list& handles) {
        auto plm = playlist_manager::get();
        RebuildQueueResult result;

        if (!plan.ok) {
            // 队列一字未改：步骤 1 的解析已在调用方失败，直接返回。未进
            // 入写入段，不建立抑制器。
            result.success = false;
            result.error = "Failed to resolve queue item: " + plan.error;
            result.queueCount = plm->queue_get_count();
            return result;
        }

        // 进入写入段：构造抑制器，本函数的所有返回路径（含步骤 5 失败
        // 分支）都会在此处析构并经 EmitQueueChanged 统一广播一次——即便
        // 校验失败，也要如实反映当前 queue_get_count()（重建失败时析构仍须
        // 广播，不得静默吞掉状态变化）。
        QueueBroadcastSuppressor suppressor("unknown");

        // 步骤 3：flush
        plm->queue_flush();

        // 步骤 4：逐项写入。坐标优先、handle 兜底，不得统一用 handle
        // （t_playback_queue_item::operator== 比较三字段，handle 版造出的项
        // 坐标为 infinite_size，会把本来有效坐标的项降级成无坐标）。
        for (const auto& resolved : plan.resolvedItems) {
            if (resolved.kind == fb2k_queue::ResolvedItemKind::Coordinate) {
                plm->queue_add_item_playlist(resolved.playlist, resolved.item);
            } else {
                plm->queue_add_item(handles[resolved.handleIndex]);
            }
        }

        // 步骤 5：校验，不一致时如实报告失败（禁止在此处返回
        // success:true 掩盖队列已处于不确定状态的事实）。
        result.queueCount = plm->queue_get_count();
        if (result.queueCount != plan.resolvedItems.size()) {
            result.success = false;
            result.error = "Queue rebuild count mismatch after write; queue is now in an indeterminate state";
            return result;
        }

        result.success = true;
        return result;
    }

    // Snapshot of the current queue as the SDK-free slots the plan functions
    // consume, with the handles table they index into.
    void SnapshotQueue(std::vector<fb2k_queue::QueueSlotSnapshot>& slots,
                       metadb_handle_list& handles,
                       bool withIdentity) {
        auto plm = playlist_manager::get();
        pfc::list_t<t_playback_queue_item> queueItems;
        plm->queue_get_contents(queueItems);
        const size_t queueCount = queueItems.get_count();
        slots.reserve(queueCount);
        for (size_t i = 0; i < queueCount; i++) {
            const auto& item = queueItems[i];
            fb2k_queue::QueueSlotSnapshot slot;
            slot.playlist = item.m_playlist;
            slot.item = item.m_item;
            if (item.m_handle.is_valid()) {
                slot.handleIndex = handles.get_count();
                if (withIdentity) slot.identityKey = reinterpret_cast<size_t>(item.m_handle.get_ptr());
                handles.add_item(item.m_handle);
            }
            slots.push_back(slot);
        }
    }

    //==========================================================================
    // queue.moveToTop - Move queue item to top (play next)
    //==========================================================================
    api::Result<queue::MoveToTopResult> QueueMoveToTop(const queue::MoveToTopParams& params) {
        auto plm = playlist_manager::get();

        const size_t index = static_cast<size_t>(params.index);
        const size_t queueCount = plm->queue_get_count();

        if (index >= queueCount || index == 0) {
            return api::Fail("Invalid index or already at top", ApiErrorCode::INVALID_INDEX);
        }

        // 步骤 1 的输入：把当前队列（原顺序）转成 SDK-free 的坐标+handle 快照，
        // handle 有效性在此处（有 SDK）判定好后转成不透明索引再传入纯函数。
        std::vector<fb2k_queue::QueueSlotSnapshot> slots;
        metadb_handle_list handles;
        SnapshotQueue(slots, handles, /*withIdentity=*/false);

        // 目标顺序的计算与解析（步骤 1）全部交给单测覆盖的纯函数——生产路径
        // 与单测路径必须是同一份代码，不得在此重新写一遍"移到队首、其余保
        // 序"的循环。
        fb2k_queue::RebuildPlanResult plan = fb2k_queue::ComputeMoveToTopPlan(
            slots,
            index,
            [](size_t playlist, size_t item) {
                return IsCoordinateWritable(static_cast<t_size>(playlist), static_cast<t_size>(item));
            });

        RebuildQueueResult rebuildResult = RebuildQueue(plan, handles);
        if (!rebuildResult.success) {
            return api::Fail(rebuildResult.error, ApiErrorCode::OPERATION_FAILED,
                             {{"queueCount", rebuildResult.queueCount}});
        }

        queue::MoveToTopResult result;
        result.movedIndex = static_cast<std::int64_t>(index);
        result.queueCount = static_cast<std::int64_t>(rebuildResult.queueCount);
        return result;
    }

    //==========================================================================
    // queue.setContents - 队列重排原语
    // 只收队列/列表引用，不接受路径。生成的解析器只保证每项是带可选
    // queueIndex / playlist / item 的对象；两种形态的判定仍归本 handler。
    //==========================================================================
    api::Result<queue::SetContentsResult> QueueSetContents(const queue::SetContentsParams& params) {
        auto plm = playlist_manager::get();

        // 空数组 = 显式清空（等价 queue.clear），不特判：ComputeSetContentsPlan
        // 对空 refs 天然返回 ok=true + 空 resolvedItems，RebuildQueue flush 后
        // 校验 queue_get_count()==0 自然通过，回执与非空路径共用同一段代码。

        const size_t queueCountBeforeValidation = plm->queue_get_count();
        const size_t sizeLimit = std::max(ApiLimits::MAX_QUEUE_ITEMS, queueCountBeforeValidation);
        if (params.items.size() > sizeLimit) {
            return api::Fail("items exceeds maximum size", ApiErrorCode::INVALID_PARAMS,
                             {{"queueCount", queueCountBeforeValidation}});
        }

        // 逐项分类为 SDK-free 引用形态：既无 queueIndex 也无完整
        // (playlist, item) -> 拒绝，队列一字未改。
        std::vector<fb2k_queue::SetContentsItemRef> refs;
        refs.reserve(params.items.size());
        for (size_t i = 0; i < params.items.size(); i++) {
            const queue::QueueContentRef& itemRef = params.items[i];
            fb2k_queue::SetContentsItemRef ref;

            if (itemRef.queueIndex.has_value()) {
                ref.kind = fb2k_queue::SetContentsRefKind::QueueReference;
                ref.queueIndex = static_cast<size_t>(*itemRef.queueIndex);
            } else if (itemRef.playlistGuid.has_value() && itemRef.item.has_value()) {
                // GUID 在这里换成现在的序号；两者都给、格式不对、找不到都整批失败，队列一字未改。
                size_t playlist = 0;
                if (auto failure = ResolveRefPlaylist(i, itemRef.playlist, itemRef.playlistGuid, playlist,
                                                      queueCountBeforeValidation)) {
                    return std::move(*failure);
                }
                ref.kind = fb2k_queue::SetContentsRefKind::ListReference;
                ref.playlist = playlist;
                ref.item = static_cast<size_t>(*itemRef.item);
            } else if (itemRef.playlist.has_value() && itemRef.item.has_value()) {
                ref.kind = fb2k_queue::SetContentsRefKind::ListReference;
                ref.playlist = static_cast<size_t>(*itemRef.playlist);
                ref.item = static_cast<size_t>(*itemRef.item);
            } else {
                return api::Fail("items[" + std::to_string(i)
                                     + "]: unrecognized reference shape (expected queueIndex, or playlist or "
                                       "playlistGuid with item)",
                                 ApiErrorCode::INVALID_PARAMS,
                                 {{"queueCount", queueCountBeforeValidation}});
            }
            refs.push_back(ref);
        }

        // 步骤 1 的输入：当前队列快照，与 moveToTop 同型 —— 队列引用需要
        // 按 handle 兜底，故建 handles 表；列表引用无兜底，直通为坐标 slot
        // （ComputeSetContentsPlan 内部处理）。
        std::vector<fb2k_queue::QueueSlotSnapshot> currentQueue;
        metadb_handle_list handles;
        SnapshotQueue(currentQueue, handles, /*withIdentity=*/false);

        // 目标顺序的计算与解析（步骤 1）交给单测覆盖的纯函数，与 moveToTop
        // 共用同一份坐标可写性判据 IsCoordinateWritable。
        fb2k_queue::RebuildPlanResult plan = fb2k_queue::ComputeSetContentsPlan(
            refs,
            currentQueue,
            [](size_t playlist, size_t item) {
                return IsCoordinateWritable(static_cast<t_size>(playlist), static_cast<t_size>(item));
            });

        RebuildQueueResult rebuildResult = RebuildQueue(plan, handles);
        if (!rebuildResult.success) {
            return api::Fail(rebuildResult.error, ApiErrorCode::OPERATION_FAILED,
                             {{"queueCount", rebuildResult.queueCount}});
        }

        queue::SetContentsResult result;
        result.queueCount = static_cast<std::int64_t>(rebuildResult.queueCount);
        return result;
    }

    //==========================================================================
    // queue.insertNext - 插播
    // 两种条目形态各占一个同质数组：paths 产出无坐标项，items 产出坐标项。
    // paths 由声明的 @security MediaRead 经 kPathParams 校验；items 不含路径，
    // 形态由生成的解析器保证，坐标范围由本 handler 校验。
    //==========================================================================
    api::Result<queue::InsertNextResult> QueueInsertNext(const queue::InsertNextParams& params) {
        auto plm = playlist_manager::get();

        static const std::vector<std::string> kNoPaths;
        static const std::vector<queue::QueueListRef> kNoItems;
        const std::vector<std::string>& paths = params.paths.has_value() ? *params.paths : kNoPaths;
        const std::vector<queue::QueueListRef>& items = params.items.has_value() ? *params.items : kNoItems;

        if (paths.empty() && items.empty()) {
            return api::Fail("No paths or items specified", ApiErrorCode::INVALID_PARAMS);
        }

        const size_t position = static_cast<size_t>(params.position);

        // items 数量上限与 setContents 一致，仅限制输入规模，不保证队列剩余容量。
        const size_t queueCountBeforeValidation = plm->queue_get_count();
        const size_t itemsSizeLimit = std::max(ApiLimits::MAX_QUEUE_ITEMS, queueCountBeforeValidation);
        if (items.size() > itemsSizeLimit) {
            return api::Fail("items exceeds maximum size", ApiErrorCode::INVALID_PARAMS,
                             {{"queueCount", queueCountBeforeValidation}});
        }

        // 解析 paths 前校验全部 items。任一坐标无效即整批失败，
        // 不跳过错误项、不弹解析进度框，也不向队列写入同一请求的 paths。
        struct PrecheckedListItem {
            size_t playlist = 0;
            size_t item = 0;
            size_t identityKey = 0;
            metadb_handle_ptr handle;
        };
        std::vector<PrecheckedListItem> listItems;
        listItems.reserve(items.size());
        for (size_t i = 0; i < items.size(); i++) {
            const std::string prefix = "items[" + std::to_string(i) + "]: ";

            PrecheckedListItem entry;
            if (items[i].playlistGuid.has_value()) {
                if (auto failure = ResolveRefPlaylist(i, items[i].playlist, items[i].playlistGuid,
                                                      entry.playlist, queueCountBeforeValidation)) {
                    return std::move(*failure);
                }
            } else if (items[i].playlist.has_value()) {
                entry.playlist = static_cast<size_t>(*items[i].playlist);
            } else {
                return api::Fail(prefix + "playlist or playlistGuid is required", ApiErrorCode::INVALID_PARAMS,
                                 {{"queueCount", queueCountBeforeValidation}});
            }
            entry.item = static_cast<size_t>(items[i].item);

            if (!IsCoordinateWritable(static_cast<t_size>(entry.playlist),
                                       static_cast<t_size>(entry.item))) {
                return api::Fail(prefix + "playlist/item out of range", ApiErrorCode::INVALID_INDEX,
                                 {{"queueCount", queueCountBeforeValidation}});
            }

            // 使用 bool 重载，使取句柄失败可返回失败信封；ptr 重载会抛异常。
            // 核心持有的 interned handle 指针作为 identityKey，与 paths 使用同一判据。
            if (!plm->playlist_get_item_handle(entry.handle,
                                                static_cast<t_size>(entry.playlist),
                                                static_cast<t_size>(entry.item))
                || !entry.handle.is_valid()) {
                return api::Fail(prefix + "playlist item is not available", ApiErrorCode::NOT_FOUND,
                                 {{"queueCount", queueCountBeforeValidation}});
            }
            entry.identityKey = reinterpret_cast<size_t>(entry.handle.get_ptr());
            listItems.push_back(entry);
        }

        // 保序解析：p_filter=false，按原始数组位置交织合并（不沿用
        // ResolveQueuePathsToHandles 的合并方式，见该函数注释）。paths 为
        // 空数组时自然产出 0 项。
        std::vector<InsertNextResolvedPath> resolvedPaths;
        ResolveInsertNextPaths(paths, resolvedPaths);

        // process_locations 的模态消息循环允许重入，播放列表可能已被修改。
        // 写入前逐项重取 handle 并比对预检身份，任一缺失或变化即整批失败，
        // 本请求不修改队列，不能回退为 handle-only 后继续写入。
        // 重取成功也确认坐标仍可取到条目；items-only 调用同样执行重验。
        // 重验后本 handler 不再主动运行消息循环，随即计算并写入目标队列。
        for (size_t i = 0; i < listItems.size(); i++) {
            metadb_handle_ptr recheck;
            if (!plm->playlist_get_item_handle(recheck,
                                                static_cast<t_size>(listItems[i].playlist),
                                                static_cast<t_size>(listItems[i].item))
                || !recheck.is_valid()
                || reinterpret_cast<size_t>(recheck.get_ptr()) != listItems[i].identityKey) {
                return api::Fail("items[" + std::to_string(i) + "]: playlist changed during resolution",
                                 ApiErrorCode::OPERATION_FAILED,
                                 {{"queueCount", plm->queue_get_count()}});
            }
        }

        const size_t invalidCount = paths.size() > resolvedPaths.size() ? paths.size() - resolvedPaths.size() : 0;

        // 两种输入都未产出条目时，没有身份可用于匹配已有队列项；
        // 直接失败，不读取队列快照或修改队列。
        if (resolvedPaths.empty() && listItems.empty()) {
            return api::Fail("No valid tracks found", ApiErrorCode::NOT_FOUND,
                             {{"invalidCount", paths.size()}});
        }

        // 当前队列快照：与 moveToTop / setContents 同型，额外填充
        // identityKey（核心持有的 interned metadb_handle
        // 指针，等价于按规范化 (path, subsong) 二元组比较——前提是双方
        // 来源都已规范化：这里的 m_handle 是核心持有的 interned 实例，
        // ResolveInsertNextPaths 产出的新项同样已规范化，见其注释）。
        std::vector<fb2k_queue::QueueSlotSnapshot> currentQueue;
        metadb_handle_list handles;
        SnapshotQueue(currentQueue, handles, /*withIdentity=*/true);
        const size_t currentQueueCount = currentQueue.size();

        // 新项表：句柄续接在既有队列句柄之后，handleIndex 指向同一张合并
        // 表——RebuildQueue 与快路径消费的是这同一个 handles 列表。
        // 目标位置处先放 items 块，再放 paths 块，两块之
        // 间不交错（纯函数按 newItems 的传入顺序拼接组，所以块序在这里定）。
        std::vector<fb2k_queue::InsertNextNewItem> newItems;
        newItems.reserve(listItems.size() + resolvedPaths.size());
        for (const auto& entry : listItems) {
            fb2k_queue::InsertNextNewItem newItem;
            newItem.identityKey = entry.identityKey;
            newItem.handleIndex = handles.get_count();
            // items 形态带坐标：消费后播放游标跟到该位置继续。
            newItem.playlist = entry.playlist;
            newItem.item = entry.item;
            handles.add_item(entry.handle);
            newItems.push_back(newItem);
        }
        for (const auto& resolved : resolvedPaths) {
            fb2k_queue::InsertNextNewItem newItem;
            newItem.identityKey = resolved.identityKey;
            newItem.handleIndex = handles.get_count();
            // paths 形态不带坐标，playlist / item 保持 kInvalidCoordinate。
            handles.add_item(resolved.handle);
            newItems.push_back(newItem);
        }

        // 目标顺序、去重、身份匹配和快路径判定均由纯函数完成，写入前不修改队列。
        fb2k_queue::InsertNextPlanResult plan = fb2k_queue::ComputeInsertNextPlan(
            currentQueue,
            newItems,
            position,
            [](size_t playlist, size_t item) {
                return IsCoordinateWritable(static_cast<t_size>(playlist), static_cast<t_size>(item));
            });

        if (!plan.ok) {
            return api::Fail("Failed to resolve queue item: " + plan.error, ApiErrorCode::OPERATION_FAILED,
                             {{"queueCount", plm->queue_get_count()}, {"invalidCount", invalidCount}});
        }

        queue::InsertNextResult result;
        result.insertedCount = static_cast<std::int64_t>(plan.insertedCount);
        result.movedCount = static_cast<std::int64_t>(plan.movedCount);
        result.invalidCount = static_cast<std::int64_t>(invalidCount);

        // 快路径仅适用于无移动且落点在尾部的情况，不清空队列。
        // plan.resolvedItems 前 currentQueueCount 项恒为原队列本身（快路径
        // 定义排除任何"移动"），尾部 insertedCount 项即本次新增，顺序已由
        // 纯函数保证。
        if (plan.canUseAppendFastPath) {
            // 快路径不经过 RebuildQueue，单独合并逐项 on_changed 为一次广播。
            // 此处仅追加新项，origin 保留内核记录的 user_added。
            {
                QueueBroadcastSuppressor suppressor;
                for (size_t i = currentQueueCount; i < plan.resolvedItems.size(); i++) {
                    const auto& resolved = plan.resolvedItems[i];
                    // 按 ResolvedItem::kind 分派，不得假定尾部
                    // 新项恒为 handle 形态；items 形态的新项带坐标，必须走
                    // queue_add_item_playlist 才能让游标跟随。Unresolvable 已
                    // 被上面的 !plan.ok 拦掉，此处不列该分支即等于防御式跳过，
                    // 不会拿 kInvalidCoordinate 去索引 handles。
                    if (resolved.kind == fb2k_queue::ResolvedItemKind::Coordinate) {
                        plm->queue_add_item_playlist(resolved.playlist, resolved.item);
                    } else if (resolved.kind == fb2k_queue::ResolvedItemKind::Handle) {
                        plm->queue_add_item(handles[resolved.handleIndex]);
                    }
                }
            }

            result.queueCount = CurrentQueueCount();
            return result;
        }

        // 需要移动已有项或落点不在尾部时，统一重建队列。
        fb2k_queue::RebuildPlanResult rebuildInput;
        rebuildInput.ok = plan.ok;
        rebuildInput.resolvedItems = std::move(plan.resolvedItems);
        rebuildInput.error = std::move(plan.error);

        RebuildQueueResult rebuildResult = RebuildQueue(rebuildInput, handles);
        if (!rebuildResult.success) {
            return api::Fail(rebuildResult.error, ApiErrorCode::OPERATION_FAILED,
                             {{"queueCount", rebuildResult.queueCount}, {"invalidCount", invalidCount}});
        }

        result.queueCount = static_cast<std::int64_t>(rebuildResult.queueCount);
        return result;
    }

    //==========================================================================
    // queue.playNow - 播放队列第 N 项
    // 不接受路径参数，与 queue.setContents 同级。
    //==========================================================================
    api::Result<queue::PlayNowResult> QueuePlayNow(const queue::PlayNowParams& params) {
        auto plm = playlist_manager::get();

        const size_t queueCount = plm->queue_get_count();
        if (queueCount == 0) {
            return api::Fail("Queue is empty", ApiErrorCode::NOT_FOUND);
        }

        // 负数已由生成的解析器（@minimum 0）拒绝。
        const size_t index = static_cast<size_t>(params.index);
        if (index >= queueCount) {
            return api::Fail("Invalid queue index", ApiErrorCode::INVALID_INDEX);
        }

        auto pc = playback_control::get();

        // 快路径：index==0 时队列头本就是下一个要播的项，零重建，
        // 直接消费。"队列优先于 playlist 拦截 next" 是 SDK 未记载行为，
        // 实测停止态同样成立，因此没有停止态的退路分支。
        if (index == 0) {
            pc->start(playback_control::track_command_next);

            // 注意：核心对队列头的消费相对 start() 调用的同步性未经验证，
            // 此处读到的 queueCount 可能是消费前的即时值。对外文档已声明
            // queueCount 的时序不作保证，因此不为此加 sleep 或轮询。
            queue::PlayNowResult result;
            result.playedIndex = static_cast<std::int64_t>(index);
            result.queueCount = CurrentQueueCount();
            return result;
        }

        // index > 0：复用单一重建单元，把该项移到队首（与
        // moveToTop 完全同型的快照构造），成功后再 start()。start() 必须
        // 在 RebuildQueue 返回之后调用（advance 通知须留在 RebuildQueue 自带
        // 抑制器的作用域之外）；重建失败则不起播，
        // 原样返回失败回执。
        std::vector<fb2k_queue::QueueSlotSnapshot> slots;
        metadb_handle_list handles;
        SnapshotQueue(slots, handles, /*withIdentity=*/false);

        fb2k_queue::RebuildPlanResult plan = fb2k_queue::ComputeMoveToTopPlan(
            slots,
            index,
            [](size_t playlist, size_t item) {
                return IsCoordinateWritable(static_cast<t_size>(playlist), static_cast<t_size>(item));
            });

        RebuildQueueResult rebuildResult = RebuildQueue(plan, handles);
        if (!rebuildResult.success) {
            return api::Fail(rebuildResult.error, ApiErrorCode::OPERATION_FAILED,
                             {{"queueCount", rebuildResult.queueCount}});
        }

        pc->start(playback_control::track_command_next);

        // 注意：同上方 index==0 分支：queueCount 相对 start() 的同步性
        // 未经验证，对外文档已声明其时序不作保证。
        queue::PlayNowResult result;
        result.playedIndex = static_cast<std::int64_t>(index);
        result.queueCount = CurrentQueueCount();
        return result;
    }

} // anonymous namespace

//==========================================================================
// Register all Queue APIs
//==========================================================================
void RegisterQueueApi() {
    // Core queue operations
    api::RegisterApi("queue.get", QueueGet);
    api::RegisterApi("queue.add", QueueAdd);
    api::RegisterApi("queue.addPaths", QueueAddPaths);
    api::RegisterApi("queue.remove", QueueRemove);
    api::RegisterApi("queue.clear", QueueClear);
    api::RegisterApi("queue.getCount", QueueGetCount);
    api::RegisterApi("queue.moveToTop", QueueMoveToTop);
    api::RegisterApi("queue.setContents", QueueSetContents);
    api::RegisterApi("queue.insertNext", QueueInsertNext);
    api::RegisterApi("queue.playNow", QueuePlayNow);

    // The older name of queue.clear
    api::RegisterApi("queue.flush", QueueFlush);
}

//==========================================================================
// JIT Queue API Implementation
//==========================================================================
namespace {

    //----------------------------------------------------------------------
    // jitQueue.playNow - Immediately play a track
    //----------------------------------------------------------------------
    api::Result<jitq::PlayNowResult> JitQueuePlayNow(const jitq::PlayNowParams& params, const CallerContext& caller) {
        if (params.url.length() > ApiLimits::MAX_STREAM_URL_LENGTH) {
            return api::Fail(ApiError::URL_TOO_LONG, ApiErrorCode::INVALID_PARAMS);
        }

        const std::int64_t shadowPlaylist = static_cast<std::int64_t>(g_QueueManager.GetShadowPlaylistIndex());
        // The calling page owns the new session: the jitQueue:* events go back to it.
        if (!g_QueueManager.PlayNow(params.trackId, params.title, params.url,
                                    JitSessionCaller{caller.windowId, caller.callerHwnd})) {
            // The manager refuses while a previous playNow is still being resolved.
            return api::Fail("The JIT queue is still starting a previous track", ApiErrorCode::OPERATION_FAILED,
                             {{"trackId", params.trackId}, {"shadowPlaylist", shadowPlaylist}});
        }

        jitq::PlayNowResult result;
        result.trackId = params.trackId;
        result.shadowPlaylist = static_cast<std::int64_t>(g_QueueManager.GetShadowPlaylistIndex());
        return result;
    }

    //----------------------------------------------------------------------
    // jitQueue.enqueueNext - Preload the next track
    //----------------------------------------------------------------------
    api::Result<jitq::EnqueueNextResult> JitQueueEnqueueNext(const jitq::EnqueueNextParams& params) {
        if (params.url.length() > ApiLimits::MAX_STREAM_URL_LENGTH) {
            return api::Fail(ApiError::URL_TOO_LONG, ApiErrorCode::INVALID_PARAMS);
        }

        if (!g_QueueManager.EnqueueNext(params.trackId, params.title, params.url)) {
            // Only an Active or WaitingNext session takes a next track.
            return api::Fail("The JIT queue is not playing, so there is no next slot", ApiErrorCode::NO_ACTIVE_ITEM,
                             {{"trackId", params.trackId},
                              {"bufferSize", static_cast<std::int64_t>(g_QueueManager.GetBufferSize())}});
        }

        jitq::EnqueueNextResult result;
        result.trackId = params.trackId;
        result.bufferSize = static_cast<std::int64_t>(g_QueueManager.GetBufferSize());
        return result;
    }

    //----------------------------------------------------------------------
    // jitQueue.skip - Skip to next track
    //----------------------------------------------------------------------
    api::Result<jitq::SkipResult> JitQueueSkip(const jitq::SkipParams& /*params*/) {
        if (!g_QueueManager.Skip()) {
            return api::Fail("Nothing is playing from the JIT queue", ApiErrorCode::NO_ACTIVE_ITEM,
                             {{"currentTrackId", g_QueueManager.GetCurrentTrackId()}});
        }

        jitq::SkipResult result;
        result.currentTrackId = g_QueueManager.GetCurrentTrackId();
        return result;
    }

    //----------------------------------------------------------------------
    // jitQueue.stop - Stop playback
    //----------------------------------------------------------------------
    api::Result<void> JitQueueStop(const jitq::StopParams& params) {
        g_QueueManager.Stop(params.clearBuffer);
        return api::Ok();
    }

    //----------------------------------------------------------------------
    // jitQueue.clear - Clear the buffer
    //----------------------------------------------------------------------
    api::Result<void> JitQueueClear(const jitq::ClearParams& /*params*/) {
        g_QueueManager.Clear();
        return api::Ok();
    }

    //----------------------------------------------------------------------
    // jitQueue.getState - Get current JIT queue state
    //----------------------------------------------------------------------
    api::Result<jitq::GetStateResult> JitQueueGetState(const jitq::GetStateParams& /*params*/) {
        jitq::GetStateResult result;
        result.isActive = g_QueueManager.IsActive();
        result.state = QueueManager::StateToString(g_QueueManager.GetState());
        result.currentTrackId = g_QueueManager.GetCurrentTrackId();
        result.nextTrackId = g_QueueManager.GetNextTrackId();
        result.bufferSize = static_cast<std::int64_t>(g_QueueManager.GetBufferSize());
        // pfc::infinite_size (the "not created" marker) casts to -1.
        result.shadowPlaylist = static_cast<std::int64_t>(g_QueueManager.GetShadowPlaylistIndex());
        return result;
    }

    //----------------------------------------------------------------------
    // jitQueue.notifyEmpty - Frontend notifies no more tracks
    //----------------------------------------------------------------------
    api::Result<void> JitQueueNotifyEmpty(const jitq::NotifyEmptyParams& /*params*/) {
        // Frontend has no more tracks to provide
        // Actually trigger the exhaustion flow so C++ side knows
        FB2K_console_print("[JIT Queue] Frontend reports: queue empty");
        g_QueueManager.NotifyListExhausted();
        return api::Ok();
    }

    //----------------------------------------------------------------------
    // jitQueue.preloadBatch - Bulk-insert tracks
    //----------------------------------------------------------------------
    api::Result<jitq::PreloadBatchResult> JitQueuePreloadBatch(const jitq::PreloadBatchParams& params,
                                                               const CallerContext& caller) {
        // Extract URL list (skipping URLs that exceed the streaming-path
        // limit; counted as invalid below).
        std::vector<std::string> urls;
        std::int64_t invalidUrlCount = 0;
        if (params.urls.has_value()) {
            for (const std::string& urlStr : *params.urls) {
                if (urlStr.length() > ApiLimits::MAX_STREAM_URL_LENGTH) {
                    invalidUrlCount++;
                    continue;
                }
                urls.push_back(urlStr);
            }
        }

        // MEDIUM: Batch size limit to prevent main-thread stall
        constexpr size_t MAX_PRELOAD_BATCH = 10000;
        if (urls.size() > MAX_PRELOAD_BATCH) {
            return api::Fail("Batch exceeds maximum size (10000)", ApiErrorCode::INVALID_PARAMS);
        }

        // The two argument checks QueueManager also performs are made here so
        // that they answer with INVALID_PARAMS; what remains of its failures is
        // the host not doing the work.
        const size_t startIndex = static_cast<size_t>(params.startIndex);
        if (urls.empty()) {
            return api::Fail("Empty URL list", ApiErrorCode::INVALID_PARAMS,
                             {{"tracksAdded", 0}, {"invalidCount", invalidUrlCount}});
        }
        if (startIndex >= urls.size()) {
            return api::Fail("startIndex out of range", ApiErrorCode::INVALID_PARAMS,
                             {{"tracksAdded", 0}, {"invalidCount", invalidUrlCount}});
        }

        auto batch = g_QueueManager.PreloadBatch(urls, startIndex, params.replace,
                                                 JitSessionCaller{caller.windowId, caller.callerHwnd});
        if (!batch.success) {
            return api::Fail(batch.error.empty() ? std::string("PreloadBatch failed") : batch.error,
                             ApiErrorCode::OPERATION_FAILED,
                             {{"tracksAdded", static_cast<std::int64_t>(batch.tracksAdded)},
                              {"invalidCount", invalidUrlCount}});
        }

        jitq::PreloadBatchResult result;
        result.tracksAdded = static_cast<std::int64_t>(batch.tracksAdded);
        result.invalidCount = invalidUrlCount;
        return result;
    }

} // anonymous namespace

//==========================================================================
// Register JIT Queue APIs
//==========================================================================
void RegisterJitQueueApi() {
    // Core JIT queue operations
    api::RegisterApi("jitQueue.playNow", JitQueuePlayNow);
    api::RegisterApi("jitQueue.enqueueNext", JitQueueEnqueueNext);
    api::RegisterApi("jitQueue.skip", JitQueueSkip);
    api::RegisterApi("jitQueue.stop", JitQueueStop);
    api::RegisterApi("jitQueue.clear", JitQueueClear);
    api::RegisterApi("jitQueue.getState", JitQueueGetState);
    api::RegisterApi("jitQueue.notifyEmpty", JitQueueNotifyEmpty);
    api::RegisterApi("jitQueue.preloadBatch", JitQueuePreloadBatch);

    FB2K_console_print("[JIT Queue] API registered");
}

//==========================================================================
// Initialize JIT Queue
//==========================================================================
void InitializeJitQueue() {
    g_QueueManager.Initialize();
}
