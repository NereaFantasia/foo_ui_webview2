/**
 * QueueApi.cpp - Playback Queue API
 * 
 * Provides APIs for foobar2000's native playback queue functionality.
 * Allows "Play Next" and queue management without maintaining frontend state.
 * 
 * Also includes JIT Queue API for streaming media with dual-layer architecture.
 */

#include "pch.h"
#include "api/QueueApi.h"
#include "api/ApiConstants.h"
#include "api/BridgeCore.h"
#include "api/MetaAccess.h"
#include "api/PlaylistApi.h"
#include "api/QueueRebuildPlan.h"
#include "callbacks/QueueCallback.h"
#include "core/QueueManager.h"
#include "utils/SubsongUtils.h"

namespace {
    using json = nlohmann::json;

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

    void ResolveQueuePathsToHandles(const json& paths, metadb_handle_list& outItems, size_t& invalidCount) {
        auto piif = playlist_incoming_item_filter::get();

        // 批量收集普通路径，一次性调用 process_locations（避免弹窗风暴）
        pfc::string_list_impl batchPaths;
        metadb_handle_list subsongHandles;

        for (const auto& pathValue : paths) {
            if (!pathValue.is_string()) {
                invalidCount++;
                continue;
            }

            ParsedPlayablePath parsed = ParsePlayablePath(pathValue.get<std::string>());
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
    void ResolveInsertNextPaths(const json& paths, std::vector<InsertNextResolvedPath>& outResolved) {
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

        for (const auto& pathValue : paths) {
            if (!pathValue.is_string()) {
                continue;
            }

            ParsedPlayablePath parsed = ParsePlayablePath(pathValue.get<std::string>());
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
    // Helper: Get track info from metadb_handle
    //==========================================================================
    json GetTrackInfoFromHandle(const metadb_handle_ptr& handle, size_t queueIndex) {
        if (!handle.is_valid()) {
            return json::object();
        }
        
        // Get native filesystem path
        pfc::string8 nativePath;
        filesystem::g_get_native_path(handle->get_path(), nativePath);
        std::string absolutePath = nativePath.get_ptr();
        
        json track;
        track["queueIndex"] = queueIndex;
        track["path"] = handle->get_path();
        track["absolutePath"] = absolutePath;
        track["subsong"] = handle->get_subsong_index();
        track["fileSize"] = static_cast<int64_t>(handle->get_filesize());
        
        // Get metadata using get_info_ref()
        metadb_info_container::ptr infoContainer = handle->get_info_ref();
        if (infoContainer.is_valid()) {
            const file_info& fi = infoContainer->info();
            
            // Basic metadata
            const char* title = fi.meta_get("TITLE", 0);
            const char* album = fi.meta_get("ALBUM", 0);
            const char* date = fi.meta_get("DATE", 0);
            const char* tracknumber = fi.meta_get("TRACKNUMBER", 0);
            const char* discnumber = fi.meta_get("DISCNUMBER", 0);
            const char* codec = fi.info_get("codec");

            track["title"] = title ? title : "";
            track["artist"] = MetaJoined(fi, "ARTIST");
            track["album"] = album ? album : "";
            track["albumArtist"] = MetaJoined(fi, "ALBUM ARTIST");
            track["genre"] = MetaJoined(fi, "GENRE");
            track["date"] = date ? date : "";
            track["trackNumber"] = tracknumber ? atoi(tracknumber) : 0;
            track["discNumber"] = discnumber ? atoi(discnumber) : 0;
            track["duration"] = fi.get_length();
            track["bitrate"] = static_cast<int>(fi.info_get_bitrate());
            track["sampleRate"] = static_cast<int>(fi.info_get_int("samplerate"));
            track["channels"] = static_cast<int>(fi.info_get_int("channels"));
            track["codec"] = codec ? codec : "";
        } else {
            // Fallback: extract filename as title
            pfc::string8 path = handle->get_path();
            const char* filename = pfc::string_filename(path);
            track["title"] = filename ? filename : path.c_str();
            track["artist"] = "";
            track["album"] = "";
            track["albumArtist"] = "";
            track["genre"] = "";
            track["date"] = "";
            track["trackNumber"] = 0;
            track["discNumber"] = 0;
            track["duration"] = 0.0;
            track["bitrate"] = 0;
            track["sampleRate"] = 0;
            track["channels"] = 0;
            track["codec"] = "";
        }
        
        return track;
    }
    
    //==========================================================================
    // queue.get - Get current playback queue contents
    //==========================================================================
    json QueueGet(const json& params) {
        auto plm = playlist_manager::get();
        
        pfc::list_t<t_playback_queue_item> queueItems;
        plm->queue_get_contents(queueItems);
        
        json items = json::array();
        size_t count = queueItems.get_count();
        
        for (size_t i = 0; i < count; i++) {
            const auto& item = queueItems[i];
            json trackInfo = GetTrackInfoFromHandle(item.m_handle, i);
            // 任一坐标为 pfc::infinite_size 则 playlist / playlistItem 两键
            // 一并写 null（键必有），不把 SIZE_MAX 原值写进 JSON。
            if (item.m_playlist == pfc::infinite_size || item.m_item == pfc::infinite_size) {
                trackInfo["playlist"] = nullptr;
                trackInfo["playlistItem"] = nullptr;
            } else {
                trackInfo["playlist"] = item.m_playlist;
                trackInfo["playlistItem"] = item.m_item;
            }
            items.push_back(trackInfo);
        }
        
        return {
            {"items", items},
            {"count", count}
        };
    }
    
    //==========================================================================
    // queue.add - Add playlist items to queue
    //==========================================================================
    json QueueAdd(const json& params) {
        auto plm = playlist_manager::get();
        
        // Get playlist index
        size_t playlistIndex = params.value("playlist", pfc::infinite_size);
        if (playlistIndex == pfc::infinite_size) {
            playlistIndex = plm->get_active_playlist();
        }
        
        if (playlistIndex >= plm->get_playlist_count()) {
            return {{"success", false}, {"error", "Invalid playlist index"}};
        }
        
        size_t addedCount = 0;
        
        // Add by track indices
        if (params.contains("tracks") && params["tracks"].is_array()) {
            // 本段可能逐项触发多次内核 on_changed（每次
            // queue_add_item_playlist 一次），用抑制器把它们合并为析构时
            // 的 1 次广播，origin 透传抑制期内最后一次记录值（均为
            // user_added，语义正确）。
            QueueBroadcastSuppressor suppressor;
            for (const auto& trackIdx : params["tracks"]) {
                size_t idx = trackIdx.get<size_t>();
                if (idx < plm->playlist_get_item_count(playlistIndex)) {
                    plm->queue_add_item_playlist(playlistIndex, idx);
                    addedCount++;
                }
            }
        }
        // Add single track
        else if (params.contains("track")) {
            size_t idx = params.value("track", pfc::infinite_size);
            if (idx < plm->playlist_get_item_count(playlistIndex)) {
                plm->queue_add_item_playlist(playlistIndex, idx);
                addedCount = 1;
            }
        }
        
        return {
            {"success", addedCount > 0},
            {"addedCount", addedCount},
            {"queueCount", plm->queue_get_count()}
        };
    }
    
    //==========================================================================
    // queue.addPaths - Add paths/URLs to queue (combo API)
    // Internally: add to playlist -> add to queue
    //==========================================================================
    json QueueAddPaths(const json& params) {
        auto paths = params.value("paths", json::array());
        if (paths.empty()) {
            return {{"success", false}, {"error", "No paths specified"}};
        }
        
        auto plm = playlist_manager::get();
        
        // Determine target playlist
        size_t playlistIndex;
        bool useQueuePlaylist = params.value("useQueuePlaylist", true);
        
        if (useQueuePlaylist) {
            playlistIndex = GetOrCreateQueuePlaylist();
        } else if (params.contains("playlist")) {
            playlistIndex = params.value("playlist", pfc::infinite_size);
            if (playlistIndex >= plm->get_playlist_count()) {
                return {{"success", false}, {"error", "Invalid playlist index"}};
            }
        } else {
            playlistIndex = plm->get_active_playlist();
            if (playlistIndex == pfc::infinite_size) {
                return {{"success", false}, {"error", "No active playlist"}};
            }
        }

        if (plm->playlist_lock_is_present(playlistIndex)) {
            return {
                {"success", false},
                {"error", "Playlist is locked"},
                {"playlist", playlistIndex},
                {"isLocked", true}
            };
        }
        
        // Get current item count (insertion point)
        size_t insertPos = plm->playlist_get_item_count(playlistIndex);
        
        // Add to playlist, path|subsong:N 走 handle_create，普通路径走 process_locations
        metadb_handle_list items;
        size_t invalidCount = 0;
        ResolveQueuePathsToHandles(paths, items, invalidCount);
        
        if (items.get_count() == 0) {
            return {
                {"success", false},
                {"error", "No valid tracks found"},
                {"invalidCount", invalidCount}
            };
        }
        
        // Undo backup before modification
        plm->playlist_undo_backup(playlistIndex);
        
        // Insert into playlist
        plm->playlist_insert_items(playlistIndex, insertPos, items, bit_array_false());
        
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
        
        return {
            {"success", true},
            {"addedCount", addedCount},
            {"invalidCount", invalidCount},
            {"playlist", playlistIndex},
            {"queueCount", plm->queue_get_count()}
        };
    }
    
    //==========================================================================
    // queue.remove - Remove item from queue by index
    //==========================================================================
    json QueueRemove(const json& params) {
        auto plm = playlist_manager::get();
        
        size_t queueCount = plm->queue_get_count();
        if (queueCount == 0) {
            return {{"success", false}, {"error", "Queue is empty"}};
        }
        
        // Remove by index
        if (params.contains("index")) {
            size_t index = params.value("index", pfc::infinite_size);
            if (index >= queueCount) {
                return {{"success", false}, {"error", "Invalid queue index"}};
            }
            
            // Create bit array with only the specified index set
            pfc::bit_array_one mask(index);
            plm->queue_remove_mask(mask);
            
            return {
                {"success", true},
                {"removedIndex", index},
                {"queueCount", plm->queue_get_count()}
            };
        }
        // Remove by indices array
        else if (params.contains("indices") && params["indices"].is_array()) {
            pfc::bit_array_bittable mask(queueCount);
            size_t removeCount = 0;
            
            for (const auto& idx : params["indices"]) {
                size_t index = idx.get<size_t>();
                if (index < queueCount && !mask.get(index)) {
                    mask.set(index, true);
                    removeCount++;
                }
            }
            
            if (removeCount > 0) {
                plm->queue_remove_mask(mask);
            }
            
            return {
                {"success", removeCount > 0},
                {"removedCount", removeCount},
                {"queueCount", plm->queue_get_count()}
            };
        }
        
        return {{"success", false}, {"error", "No index or indices specified"}};
    }
    
    //==========================================================================
    // queue.clear - Clear entire queue
    //==========================================================================
    json QueueClear(const json& /*params*/) {
        auto plm = playlist_manager::get();
        size_t previousCount = plm->queue_get_count();
        
        plm->queue_flush();
        
        return {
            {"success", true},
            {"clearedCount", previousCount}
        };
    }
    
    //==========================================================================
    // queue.getCount - Get queue item count
    //==========================================================================
    json QueueGetCount(const json& /*params*/) {
        auto plm = playlist_manager::get();
        return {
            {"count", plm->queue_get_count()},
            {"hasItems", plm->queue_is_active()}
        };
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

    //==========================================================================
    // queue.moveToTop - Move queue item to top (play next)
    //==========================================================================
    json QueueMoveToTop(const json& params) {
        auto plm = playlist_manager::get();
        
        size_t index = params.value("index", pfc::infinite_size);
        size_t queueCount = plm->queue_get_count();
        
        if (index == pfc::infinite_size || index >= queueCount || index == 0) {
            return {{"success", false}, {"error", "Invalid index or already at top"}};
        }
        
        // Get current queue
        pfc::list_t<t_playback_queue_item> queueItems;
        plm->queue_get_contents(queueItems);
        
        // 步骤 1 的输入：把当前队列（原顺序）转成 SDK-free 的坐标+handle 快照，
        // handle 有效性在此处（有 SDK）判定好后转成不透明索引再传入纯函数。
        std::vector<fb2k_queue::QueueSlotSnapshot> slots;
        metadb_handle_list handles;
        slots.reserve(queueCount);
        for (size_t i = 0; i < queueCount; i++) {
            const auto& item = queueItems[i];
            fb2k_queue::QueueSlotSnapshot slot;
            slot.playlist = item.m_playlist;
            slot.item = item.m_item;
            if (item.m_handle.is_valid()) {
                slot.handleIndex = handles.get_count();
                handles.add_item(item.m_handle);
            }
            slots.push_back(slot);
        }
        
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
            return {
                {"success", false},
                {"error", rebuildResult.error},
                {"queueCount", rebuildResult.queueCount}
            };
        }
        
        return {
            {"success", true},
            {"movedIndex", index},
            {"queueCount", rebuildResult.queueCount}
        };
    }

    //==========================================================================
    // queue.setContents - 队列重排原语
    // 只收队列/列表引用，不接受路径，因而没有 PathSecuritySpec ——
    // ValidateNestedArrayParam 对异构数组元素 fail-fast，会把纯重排主用例
    // 拒在 items[0]；形态判定的责任全在本 handler。
    //==========================================================================
    json QueueSetContents(const json& params) {
        auto plm = playlist_manager::get();

        if (!params.contains("items") || !params["items"].is_array()) {
            return {
                {"success", false},
                {"error", "items must be an array"},
                {"queueCount", plm->queue_get_count()}
            };
        }
        const json& itemsJson = params["items"];

        // 空数组 = 显式清空（等价 queue.clear），不特判：ComputeSetContentsPlan
        // 对空 refs 天然返回 ok=true + 空 resolvedItems，RebuildQueue flush 后
        // 校验 queue_get_count()==0 自然通过，回执与非空路径共用同一段代码。

        size_t queueCountBeforeValidation = plm->queue_get_count();
        size_t sizeLimit = std::max(ApiLimits::MAX_QUEUE_ITEMS, queueCountBeforeValidation);
        if (itemsJson.size() > sizeLimit) {
            return {
                {"success", false},
                {"error", "items exceeds maximum size"},
                {"queueCount", queueCountBeforeValidation}
            };
        }

        // 逐项分类为 SDK-free 引用形态：既无 queueIndex 也无完整
        // (playlist, item) -> 业务响应体拒绝，队列一字未改（判据 A13：走
        // 业务响应体，不是 ApiEnvelope::MakeError 的框架信封）。
        std::vector<fb2k_queue::SetContentsItemRef> refs;
        refs.reserve(itemsJson.size());
        for (size_t i = 0; i < itemsJson.size(); i++) {
            const json& itemJson = itemsJson[i];
            fb2k_queue::SetContentsItemRef ref;

            if (itemJson.is_object() && itemJson.contains("queueIndex") && itemJson["queueIndex"].is_number()) {
                ref.kind = fb2k_queue::SetContentsRefKind::QueueReference;
                ref.queueIndex = itemJson["queueIndex"].get<size_t>();
            } else if (itemJson.is_object()
                       && itemJson.contains("playlist") && itemJson["playlist"].is_number()
                       && itemJson.contains("item") && itemJson["item"].is_number()) {
                ref.kind = fb2k_queue::SetContentsRefKind::ListReference;
                ref.playlist = itemJson["playlist"].get<size_t>();
                ref.item = itemJson["item"].get<size_t>();
            } else {
                return {
                    {"success", false},
                    {"error", "items[" + std::to_string(i)
                        + "]: unrecognized reference shape (expected queueIndex or playlist+item)"},
                    {"queueCount", queueCountBeforeValidation}
                };
            }
            refs.push_back(ref);
        }

        // 步骤 1 的输入：当前队列快照，与 moveToTop 同型 —— 队列引用需要
        // 按 handle 兜底，故建 handles 表；列表引用无兜底，直通为坐标 slot
        // （ComputeSetContentsPlan 内部处理）。
        pfc::list_t<t_playback_queue_item> queueItems;
        plm->queue_get_contents(queueItems);
        size_t currentQueueCount = queueItems.get_count();

        std::vector<fb2k_queue::QueueSlotSnapshot> currentQueue;
        metadb_handle_list handles;
        currentQueue.reserve(currentQueueCount);
        for (size_t i = 0; i < currentQueueCount; i++) {
            const auto& item = queueItems[i];
            fb2k_queue::QueueSlotSnapshot slot;
            slot.playlist = item.m_playlist;
            slot.item = item.m_item;
            if (item.m_handle.is_valid()) {
                slot.handleIndex = handles.get_count();
                handles.add_item(item.m_handle);
            }
            currentQueue.push_back(slot);
        }

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
            return {
                {"success", false},
                {"error", rebuildResult.error},
                {"queueCount", rebuildResult.queueCount}
            };
        }

        return {
            {"success", true},
            {"queueCount", rebuildResult.queueCount}
        };
    }

    //==========================================================================
    // queue.insertNext - 插播
    // 两种条目形态各占一个同质数组：paths 产出无坐标项，items 产出坐标项。
    // paths 由注册时的 PathSecuritySpec 校验，越权或形态错误返回框架错误
    // 信封，不回显路径本体。items 不含路径，由本 handler 校验形态和坐标，
    // 校验失败返回 success:false 业务响应体。
    //==========================================================================
    json QueueInsertNext(const json& params) {
        auto plm = playlist_manager::get();

        // paths 用 value() 读：注册的 PathSecuritySpec 保证键存在时必是字
        // 符串数组（ValidateArrayParam 在 skipInvalid=false 下对首个非字符
        // 串元素 fail-fast），缺键则被 ValidatePathParam 直接放行，故
        // items-only 调用不经路径校验层。
        auto paths = params.value("paths", json::array());

        // items 不经过路径校验层，形态错误返回业务响应体，不是框架错误信封。
        if (params.contains("items") && !params["items"].is_array()) {
            return {{"success", false}, {"error", "items must be an array"}};
        }
        const json emptyItems = json::array();
        const json& items = params.contains("items") ? params["items"] : emptyItems;

        if (paths.empty() && items.empty()) {
            return {{"success", false}, {"error", "No paths or items specified"}};
        }

        // position 必须是非负整数，先校验再转成 size_t，避免负数转成大下标。
        size_t position = 0;
        if (params.contains("position")) {
            const json& positionValue = params["position"];
            if (!positionValue.is_number_integer() || positionValue.get<int64_t>() < 0) {
                return {{"success", false}, {"error", "position must be a non-negative integer"}};
            }
            position = static_cast<size_t>(positionValue.get<int64_t>());
        }

        // items 数量上限与 setContents 一致，仅限制输入规模，不保证队列剩余容量。
        size_t queueCountBeforeValidation = plm->queue_get_count();
        size_t itemsSizeLimit = std::max(ApiLimits::MAX_QUEUE_ITEMS, queueCountBeforeValidation);
        if (items.size() > itemsSizeLimit) {
            return {
                {"success", false},
                {"error", "items exceeds maximum size"},
                {"queueCount", queueCountBeforeValidation}
            };
        }

        // 解析 paths 前校验全部 items。任一形态或坐标无效即整批失败，
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
            const json& itemJson = items[i];
            const std::string prefix = "items[" + std::to_string(i) + "]: ";

            if (!itemJson.is_object()) {
                return {
                    {"success", false},
                    {"error", prefix + "expected an object with playlist and item"},
                    {"queueCount", queueCountBeforeValidation}
                };
            }
            if (!itemJson.contains("playlist") || !itemJson["playlist"].is_number_integer()
                || !itemJson.contains("item") || !itemJson["item"].is_number_integer()) {
                return {
                    {"success", false},
                    {"error", prefix + "playlist and item must be integers"},
                    {"queueCount", queueCountBeforeValidation}
                };
            }

            const json& playlistJson = itemJson["playlist"];
            const json& itemIndexJson = itemJson["item"];
            // is_number_integer() 对无符号整数同样为真；负数只可能以有符号
            // 形态出现，故只对非无符号的那支取 int64 判正负。
            if ((!playlistJson.is_number_unsigned() && playlistJson.get<int64_t>() < 0)
                || (!itemIndexJson.is_number_unsigned() && itemIndexJson.get<int64_t>() < 0)) {
                return {
                    {"success", false},
                    {"error", prefix + "playlist and item must be non-negative"},
                    {"queueCount", queueCountBeforeValidation}
                };
            }

            PrecheckedListItem entry;
            entry.playlist = playlistJson.get<size_t>();
            entry.item = itemIndexJson.get<size_t>();

            if (!IsCoordinateWritable(static_cast<t_size>(entry.playlist),
                                       static_cast<t_size>(entry.item))) {
                return {
                    {"success", false},
                    {"error", prefix + "playlist/item out of range"},
                    {"queueCount", queueCountBeforeValidation}
                };
            }

            // 使用 bool 重载，使取句柄失败可返回业务响应体；ptr 重载会抛异常。
            // 核心持有的 interned handle 指针作为 identityKey，与 paths 使用同一判据。
            if (!plm->playlist_get_item_handle(entry.handle,
                                                static_cast<t_size>(entry.playlist),
                                                static_cast<t_size>(entry.item))
                || !entry.handle.is_valid()) {
                return {
                    {"success", false},
                    {"error", prefix + "playlist item is not available"},
                    {"queueCount", queueCountBeforeValidation}
                };
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
                return {
                    {"success", false},
                    {"error", "items[" + std::to_string(i) + "]: playlist changed during resolution"},
                    {"queueCount", plm->queue_get_count()}
                };
            }
        }

        // 两种输入都未产出条目时，没有身份可用于匹配已有队列项；
        // 直接失败，不读取队列快照或修改队列。
        if (resolvedPaths.empty() && listItems.empty()) {
            return {
                {"success", false},
                {"error", "No valid tracks found"},
                {"invalidCount", paths.size()}
            };
        }

        // 当前队列快照：与 moveToTop / setContents 同型，额外填充
        // identityKey（核心持有的 interned metadb_handle
        // 指针，等价于按规范化 (path, subsong) 二元组比较——前提是双方
        // 来源都已规范化：这里的 m_handle 是核心持有的 interned 实例，
        // ResolveInsertNextPaths 产出的新项同样已规范化，见其注释）。
        pfc::list_t<t_playback_queue_item> queueItems;
        plm->queue_get_contents(queueItems);
        size_t currentQueueCount = queueItems.get_count();

        std::vector<fb2k_queue::QueueSlotSnapshot> currentQueue;
        metadb_handle_list handles;
        currentQueue.reserve(currentQueueCount);
        for (size_t i = 0; i < currentQueueCount; i++) {
            const auto& item = queueItems[i];
            fb2k_queue::QueueSlotSnapshot slot;
            slot.playlist = item.m_playlist;
            slot.item = item.m_item;
            if (item.m_handle.is_valid()) {
                slot.handleIndex = handles.get_count();
                slot.identityKey = reinterpret_cast<size_t>(item.m_handle.get_ptr());
                handles.add_item(item.m_handle);
            }
            currentQueue.push_back(slot);
        }

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

        size_t invalidCount = paths.size() > resolvedPaths.size() ? paths.size() - resolvedPaths.size() : 0;

        if (!plan.ok) {
            return {
                {"success", false},
                {"error", "Failed to resolve queue item: " + plan.error},
                {"queueCount", plm->queue_get_count()},
                {"invalidCount", invalidCount}
            };
        }

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

            return {
                {"success", true},
                {"insertedCount", plan.insertedCount},
                {"movedCount", plan.movedCount},
                {"queueCount", plm->queue_get_count()},
                {"invalidCount", invalidCount}
            };
        }

        // 需要移动已有项或落点不在尾部时，统一重建队列。
        fb2k_queue::RebuildPlanResult rebuildInput;
        rebuildInput.ok = plan.ok;
        rebuildInput.resolvedItems = std::move(plan.resolvedItems);
        rebuildInput.error = std::move(plan.error);

        RebuildQueueResult rebuildResult = RebuildQueue(rebuildInput, handles);
        if (!rebuildResult.success) {
            return {
                {"success", false},
                {"error", rebuildResult.error},
                {"queueCount", rebuildResult.queueCount},
                {"invalidCount", invalidCount}
            };
        }

        return {
            {"success", true},
            {"insertedCount", plan.insertedCount},
            {"movedCount", plan.movedCount},
            {"queueCount", rebuildResult.queueCount},
            {"invalidCount", invalidCount}
        };
    }

    //==========================================================================
    // queue.playNow - 播放队列第 N 项
    // 无 PathSecuritySpec：不接受路径参数，与 queue.setContents 同级。
    //==========================================================================
    json QueuePlayNow(const json& params) {
        auto plm = playlist_manager::get();

        size_t queueCount = plm->queue_get_count();
        if (queueCount == 0) {
            return {{"success", false}, {"error", "Queue is empty"}};
        }

        // 负数/非整数先经 is_number_integer() 判定，不依赖
        // nlohmann 对负数取 size_t 的未定义换算（与 insertNext 的
        // position 参数处理方式一致）。
        size_t index = 0;
        if (params.contains("index")) {
            const json& indexValue = params["index"];
            if (!indexValue.is_number_integer() || indexValue.get<int64_t>() < 0) {
                return {{"success", false}, {"error", "Invalid queue index"}};
            }
            index = static_cast<size_t>(indexValue.get<int64_t>());
        }

        if (index >= queueCount) {
            return {{"success", false}, {"error", "Invalid queue index"}};
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
            return {
                {"success", true},
                {"playedIndex", index},
                {"queueCount", plm->queue_get_count()}
            };
        }

        // index > 0：复用单一重建单元，把该项移到队首（与
        // moveToTop 完全同型的快照构造），成功后再 start()。start() 必须
        // 在 RebuildQueue 返回之后调用（advance 通知须留在 RebuildQueue 自带
        // 抑制器的作用域之外）；重建失败则不起播，
        // 原样返回失败回执。
        pfc::list_t<t_playback_queue_item> queueItems;
        plm->queue_get_contents(queueItems);

        std::vector<fb2k_queue::QueueSlotSnapshot> slots;
        metadb_handle_list handles;
        slots.reserve(queueCount);
        for (size_t i = 0; i < queueCount; i++) {
            const auto& item = queueItems[i];
            fb2k_queue::QueueSlotSnapshot slot;
            slot.playlist = item.m_playlist;
            slot.item = item.m_item;
            if (item.m_handle.is_valid()) {
                slot.handleIndex = handles.get_count();
                handles.add_item(item.m_handle);
            }
            slots.push_back(slot);
        }

        fb2k_queue::RebuildPlanResult plan = fb2k_queue::ComputeMoveToTopPlan(
            slots,
            index,
            [](size_t playlist, size_t item) {
                return IsCoordinateWritable(static_cast<t_size>(playlist), static_cast<t_size>(item));
            });

        RebuildQueueResult rebuildResult = RebuildQueue(plan, handles);
        if (!rebuildResult.success) {
            return {
                {"success", false},
                {"error", rebuildResult.error},
                {"queueCount", rebuildResult.queueCount}
            };
        }

        pc->start(playback_control::track_command_next);

        // 注意：同上方 index==0 分支：queueCount 相对 start() 的同步性
        // 未经验证，对外文档已声明其时序不作保证。
        return {
            {"success", true},
            {"playedIndex", index},
            {"queueCount", plm->queue_get_count()}
        };
    }

} // anonymous namespace

//==========================================================================
// Register all Queue APIs
//==========================================================================
void RegisterQueueApi() {
    auto& bridge = BridgeCore::GetInstance();
    
    // Core queue operations
    bridge.RegisterApi("queue.get", QueueGet);
    bridge.RegisterApi("queue.add", QueueAdd);
    bridge.RegisterApi("queue.addPaths", QueueAddPaths, {{"paths", SecurityLevel::MediaRead, true}});
    bridge.RegisterApi("queue.remove", QueueRemove);
    bridge.RegisterApi("queue.clear", QueueClear);
    bridge.RegisterApi("queue.getCount", QueueGetCount);
    bridge.RegisterApi("queue.moveToTop", QueueMoveToTop);
    // 只收引用形态，不接受路径，无需 PathSecuritySpec —— 与无 spec
    // 的 queue.add 同级。
    bridge.RegisterApi("queue.setContents", QueueSetContents);
    // 与 queue.addPaths 使用相同的路径安全校验；items 坐标由 handler 校验。
    bridge.RegisterApi("queue.insertNext", QueueInsertNext, {{"paths", SecurityLevel::MediaRead, true}});
    // 只收 index（number），不接受路径，无需 PathSecuritySpec ——
    // 与 queue.setContents 同级。
    bridge.RegisterApi("queue.playNow", QueuePlayNow);
    
    // Aliases for convenience
    bridge.RegisterApi("queue.flush", QueueClear);
}

//==========================================================================
// JIT Queue API Implementation
//==========================================================================
namespace {
    
    //----------------------------------------------------------------------
    // jitQueue.playNow - Immediately play a track
    //----------------------------------------------------------------------
    json JitQueuePlayNow(const json& params) {
        std::string trackId = params.value("trackId", "");
        std::string title = params.value("title", "");
        std::string url = params.value("url", "");
        
        if (trackId.empty()) {
            return {{"success", false}, {"error", "trackId is required"}};
        }
        if (url.empty()) {
            return {{"success", false}, {"error", "url is required"}};
        }
        if (url.length() > ApiLimits::MAX_STREAM_URL_LENGTH) {
            return {{"success", false}, {"error", ApiError::URL_TOO_LONG}};
        }
        
        bool accepted = g_QueueManager.PlayNow(trackId, title, url);
        
        return {
            {"success", accepted},
            {"trackId", trackId},
            {"shadowPlaylist", g_QueueManager.GetShadowPlaylistIndex()}
        };
    }
    
    //----------------------------------------------------------------------
    // jitQueue.enqueueNext - Preload the next track
    //----------------------------------------------------------------------
    json JitQueueEnqueueNext(const json& params) {
        std::string trackId = params.value("trackId", "");
        std::string title = params.value("title", "");
        std::string url = params.value("url", "");
        
        if (trackId.empty()) {
            return {{"success", false}, {"error", "trackId is required"}};
        }
        if (url.empty()) {
            return {{"success", false}, {"error", "url is required"}};
        }
        if (url.length() > ApiLimits::MAX_STREAM_URL_LENGTH) {
            return {{"success", false}, {"error", ApiError::URL_TOO_LONG}};
        }
        
        bool accepted = g_QueueManager.EnqueueNext(trackId, title, url);
        
        return {
            {"success", accepted},
            {"trackId", trackId},
            {"bufferSize", g_QueueManager.GetBufferSize()}
        };
    }
    
    //----------------------------------------------------------------------
    // jitQueue.skip - Skip to next track
    //----------------------------------------------------------------------
    json JitQueueSkip(const json& /*params*/) {
        bool success = g_QueueManager.Skip();
        
        return {
            {"success", success},
            {"currentTrackId", g_QueueManager.GetCurrentTrackId()}
        };
    }
    
    //----------------------------------------------------------------------
    // jitQueue.stop - Stop playback
    //----------------------------------------------------------------------
    json JitQueueStop(const json& params) {
        bool clearBuffer = params.value("clearBuffer", true);
        g_QueueManager.Stop(clearBuffer);
        
        return {{"success", true}};
    }
    
    //----------------------------------------------------------------------
    // jitQueue.clear - Clear the buffer
    //----------------------------------------------------------------------
    json JitQueueClear(const json& /*params*/) {
        g_QueueManager.Clear();
        
        return {{"success", true}};
    }
    
    //----------------------------------------------------------------------
    // jitQueue.getState - Get current JIT queue state
    //----------------------------------------------------------------------
    json JitQueueGetState(const json& /*params*/) {
        return {
            {"isActive", g_QueueManager.IsActive()},
            {"state", QueueManager::StateToString(g_QueueManager.GetState())},
            {"currentTrackId", g_QueueManager.GetCurrentTrackId()},
            {"nextTrackId", g_QueueManager.GetNextTrackId()},
            {"bufferSize", g_QueueManager.GetBufferSize()},
            {"shadowPlaylist", g_QueueManager.GetShadowPlaylistIndex()}
        };
    }
    
    //----------------------------------------------------------------------
    // jitQueue.notifyEmpty - Frontend notifies no more tracks
    //----------------------------------------------------------------------
    json JitQueueNotifyEmpty(const json& /*params*/) {
        // Frontend has no more tracks to provide
        // Actually trigger the exhaustion flow so C++ side knows
        FB2K_console_print("[JIT Queue] Frontend reports: queue empty");
        g_QueueManager.NotifyListExhausted();
        
        return {{"success", true}};
    }

    //----------------------------------------------------------------------
    // jitQueue.preloadBatch - Bulk-insert tracks
    //----------------------------------------------------------------------
    json JitQueuePreloadBatch(const json& params) {
        // Extract URL list (skipping URLs that exceed the streaming-path
        // limit; counted as invalid below).
        std::vector<std::string> urls;
        size_t invalidUrlCount = 0;
        if (params.contains("urls") && params["urls"].is_array()) {
            for (const auto& item : params["urls"]) {
                if (!item.is_string()) {
                    invalidUrlCount++;
                    continue;
                }
                std::string urlStr = item.get<std::string>();
                if (urlStr.length() > ApiLimits::MAX_STREAM_URL_LENGTH) {
                    invalidUrlCount++;
                    continue;
                }
                urls.push_back(std::move(urlStr));
            }
        }
        
        // MEDIUM: Batch size limit to prevent main-thread stall
        constexpr size_t MAX_PRELOAD_BATCH = 10000;
        if (urls.size() > MAX_PRELOAD_BATCH) {
            return {{"success", false}, {"error", "Batch exceeds maximum size (10000)"}};
        }
        
        size_t startIndex = params.value("startIndex", static_cast<size_t>(0));
        bool replace = params.value("replace", true);
        
        auto result = g_QueueManager.PreloadBatch(urls, startIndex, replace);
        
        json response = {
            {"success", result.success},
            {"tracksAdded", result.tracksAdded}
        };
        if (invalidUrlCount > 0) {
            response["invalidCount"] = invalidUrlCount;
        }
        
        if (!result.error.empty()) {
            response["error"] = result.error;
        }
        
        return response;
    }

} // anonymous namespace

//==========================================================================
// Register JIT Queue APIs
//==========================================================================
void RegisterJitQueueApi() {
    auto& bridge = BridgeCore::GetInstance();
    
    // Core JIT queue operations
    bridge.RegisterApi("jitQueue.playNow", JitQueuePlayNow, {{"url", SecurityLevel::MediaRead}});
    bridge.RegisterApi("jitQueue.enqueueNext", JitQueueEnqueueNext, {{"url", SecurityLevel::MediaRead}});
    bridge.RegisterApi("jitQueue.skip", JitQueueSkip);
    bridge.RegisterApi("jitQueue.stop", JitQueueStop);
    bridge.RegisterApi("jitQueue.clear", JitQueueClear);
    bridge.RegisterApi("jitQueue.getState", JitQueueGetState);
    bridge.RegisterApi("jitQueue.notifyEmpty", JitQueueNotifyEmpty);
    bridge.RegisterApi("jitQueue.preloadBatch", JitQueuePreloadBatch, {{"urls", SecurityLevel::MediaRead, true}});
    
    FB2K_console_print("[JIT Queue] API registered");
}

//==========================================================================
// Initialize JIT Queue
//==========================================================================
void InitializeJitQueue() {
    g_QueueManager.Initialize();
}
