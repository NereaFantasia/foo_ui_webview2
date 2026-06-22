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
#include "api/PlaylistApi.h"
#include "core/QueueManager.h"

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
        auto mdb = metadb::get();

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
                metadb_handle_ptr handle = mdb->handle_create(parsed.path.c_str(), parsed.subsong);
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
            const char* artist = fi.meta_get("ARTIST", 0);
            const char* album = fi.meta_get("ALBUM", 0);
            const char* albumArtist = fi.meta_get("ALBUM ARTIST", 0);
            const char* genre = fi.meta_get("GENRE", 0);
            const char* date = fi.meta_get("DATE", 0);
            const char* tracknumber = fi.meta_get("TRACKNUMBER", 0);
            const char* discnumber = fi.meta_get("DISCNUMBER", 0);
            const char* codec = fi.info_get("codec");
            
            track["title"] = title ? title : "";
            track["artist"] = artist ? artist : "";
            track["album"] = album ? album : "";
            track["albumArtist"] = albumArtist ? albumArtist : "";
            track["genre"] = genre ? genre : "";
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
            trackInfo["playlist"] = item.m_playlist;
            trackInfo["playlistItem"] = item.m_item;
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
        size_t addedCount = items.get_count();
        for (size_t i = 0; i < addedCount; i++) {
            plm->queue_add_item_playlist(playlistIndex, insertPos + i);
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
        
        // Save the item to move
        t_playback_queue_item itemToMove = queueItems[index];
        
        // Clear and rebuild queue with item at top
        plm->queue_flush();
        
        // Add the moved item first
        plm->queue_add_item_playlist(itemToMove.m_playlist, itemToMove.m_item);
        
        // Add remaining items in order
        for (size_t i = 0; i < queueCount; i++) {
            if (i != index) {
                const auto& item = queueItems[i];
                plm->queue_add_item_playlist(item.m_playlist, item.m_item);
            }
        }
        
        return {
            {"success", true},
            {"movedIndex", index},
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
