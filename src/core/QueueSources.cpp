/**
 * QueueSources.cpp - JIT 队列的曲目来源
 *
 * 判断本地文件与流媒体地址、批量预载、拆解 `|subsong:N`、把地址转成 metadb handle，
 * 以及把流媒体地址或本地文件异步加入影子列表。
 */

#include "pch.h"
#include "core/QueueManager.h"
#include "core/QueueManagerInternal.h"
#include "api/generated/JitQueueSchema.h"
#include "utils/PlaylistFormatUtils.h"
#include "utils/SubsongUtils.h"
#include <algorithm>
#include <cctype>

namespace {

namespace jitq = api::jitQueue;

using jit_queue_detail::Announce;

// A track given to playNow / enqueueNext could not be added: `url` for a stream or playlist
// location, `path` for a local file.
void AnnounceUrlError(const JitSessionCaller& to, const std::string& trackId, const std::string& error,
                      const std::string& url) {
    jitq::ErrorPayload payload;
    payload.trackId = trackId;
    payload.error = error;
    payload.url = url;
    Announce<jitq::events::Error>(to, payload);
}

void AnnouncePathError(const JitSessionCaller& to, const std::string& trackId, const std::string& error,
                       const std::string& path) {
    jitq::ErrorPayload payload;
    payload.trackId = trackId;
    payload.error = error;
    payload.path = path;
    Announce<jitq::events::Error>(to, payload);
}

}  // namespace

//==============================================================================
// URL Type Detection
//==============================================================================

bool QueueManager::IsLocalFilePath(const std::string& url) {
    if (url.empty()) return false;
    
    // HTTP/HTTPS URLs are NOT local files — use case-insensitive prefix check
    {
        std::string lowerPrefix;
        if (url.length() >= 8) {
            lowerPrefix = url.substr(0, 8);
        } else if (url.length() >= 7) {
            lowerPrefix = url.substr(0, 7);
        }
        if (!lowerPrefix.empty()) {
            std::transform(lowerPrefix.begin(), lowerPrefix.end(), lowerPrefix.begin(), ::tolower);
            if (lowerPrefix.starts_with("http://") || lowerPrefix.starts_with("https://")) {
                return false;
            }
        }
    }
    
    // Check for Windows absolute path (e.g., "C:\..." or "D:/...")
    if (url.length() >= 2 && std::isalpha(static_cast<unsigned char>(url[0])) && 
        (url[1] == ':' || url[1] == '|')) {
        return true;
    }
    
    // Check for UNC path (e.g., "\\server\share")
    if (url.length() >= 2 && url[0] == '\\' && url[1] == '\\') {
        return true;
    }
    
    // Check for file:// protocol
    if (url.length() >= 7) {
        std::string prefix = url.substr(0, 7);
        std::transform(prefix.begin(), prefix.end(), prefix.begin(), ::tolower);
        if (prefix == "file://") {
            return true;
        }
    }
    
    // Assume local if contains backslashes (Windows path separator)
    if (url.find('\\') != std::string::npos) {
        return true;
    }
    
    // Check for common audio extensions (fallback heuristic)
    std::string lowerUrl = url;
    std::transform(lowerUrl.begin(), lowerUrl.end(), lowerUrl.begin(), ::tolower);
    const char* audioExtensions[] = {".flac", ".mp3", ".wav", ".m4a", ".ogg", ".opus", ".aac", ".wma", ".ape", ".tta", ".tak"};
    for (const char* ext : audioExtensions) {
        if (lowerUrl.length() > strlen(ext) && 
            lowerUrl.compare(lowerUrl.length() - strlen(ext), strlen(ext), ext) == 0) {
            // Has audio extension but no http prefix - likely local
            if (lowerUrl.find("://") == std::string::npos) {
                return true;
            }
        }
    }
    
    return false;
}

//==============================================================================
// Batch Preload
//==============================================================================

QueueManager::PreloadResult QueueManager::PreloadBatch(
    const std::vector<std::string>& urls, size_t startIndex, bool replace, const JitSessionCaller& caller) {
    
    PreloadResult result;
    
    if (urls.empty()) {
        result.error = "Empty URL list";
        return result;
    }
    
    if (startIndex >= urls.size()) {
        result.error = "startIndex out of range";
        return result;
    }
    
    FB2K_console_print("[JIT Queue] PreloadBatch: ", urls.size(), " tracks, startIndex=", startIndex, 
                       ", replace=", replace ? "true" : "false");
    
    // Cancel any pending async operation to prevent stale callback corruption
    m_pendingOperation = false;
    
    // Save and force playbackOrder to Default (0)
    {
        auto plm = playlist_manager::get();
        int currentOrder = static_cast<int>(plm->playback_order_get_active());
        if (m_state.load() == State::Idle) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_savedPlaybackOrder = currentOrder;
            m_savedActivePlaylist = plm->get_active_playlist();
            FB2K_console_print("[JIT Queue] PreloadBatch: Saved playbackOrder=", currentOrder, 
                               ", activePlaylist=", m_savedActivePlaylist);
        }
        if (currentOrder != 0) {
            plm->playback_order_set_active(0);
            FB2K_console_print("[JIT Queue] PreloadBatch: Forced playbackOrder to 0 (Default)");
        }
    }
    
    // Ensure shadow playlist exists
    if (m_shadowPlaylistIndex == pfc::infinite_size) {
        m_shadowPlaylistIndex = GetOrCreateShadowPlaylist();
    }
    
    if (m_shadowPlaylistIndex == pfc::infinite_size) {
        result.error = "Failed to create shadow playlist";
        return result;
    }
    
    try {
        auto plm = playlist_manager::get();
        
        if (m_shadowPlaylistIndex >= plm->get_playlist_count()) {
            result.error = "Shadow playlist no longer exists";
            return result;
        }
        
        if (replace) {
            plm->playlist_clear(m_shadowPlaylistIndex);
        }
        
        // Bulk handle_create — pure in-memory, no I/O
        metadb_handle_list allHandles = BuildHandlesFromUrls(urls);
        
        if (allHandles.get_count() == 0) {
            result.error = "No valid handles created from URLs";
            return result;
        }
        
        // Bulk insert into shadow playlist
        size_t base = plm->playlist_get_item_count(m_shadowPlaylistIndex);
        plm->playlist_insert_items(m_shadowPlaylistIndex, base, allHandles, pfc::bit_array_true());
        
        FB2K_console_print("[JIT Queue] PreloadBatch: Inserted ", allHandles.get_count(), 
                           " tracks at position ", base);
        
        // Only start/restart playback when replacing or not yet Active
        bool alreadyPlaying = (m_state.load() == State::Active) && !replace;
        
        if (!alreadyPlaying) {
            // Start playback from startIndex
            size_t playFrom = base + startIndex;
            if (playFrom >= plm->playlist_get_item_count(m_shadowPlaylistIndex)) {
                playFrom = base;  // fallback to first inserted track
            }
            
            plm->set_active_playlist(m_shadowPlaylistIndex);
            plm->set_playing_playlist(m_shadowPlaylistIndex);
            plm->playlist_execute_default_action(m_shadowPlaylistIndex, playFrom);
            
            // Update state
            m_state = State::Active;
            
            // Only clear JIT URLs when starting new playback (not appending)
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                // Starting playback starts a session owned by this page; appending to a playing
                // session leaves its owner, who also gets the preloadComplete below.
                m_sessionCaller = caller;
                m_currentTrackId.clear();
                m_nextTrackId.clear();
                m_currentTitle.clear();
                m_nextTitle.clear();
                m_currentJitUrl.clear();
                m_nextJitUrl.clear();
                m_needNextRequested = false;
            }
        }
        
        result.success = true;
        result.tracksAdded = allHandles.get_count();
        
        // Emit completion event
        jitq::PreloadCompletePayload done;
        done.count = static_cast<std::int64_t>(allHandles.get_count());
        done.startIndex = static_cast<std::int64_t>(startIndex);
        done.replace = replace;
        Announce<jitq::events::PreloadComplete>(GetSessionCaller(), done);
        
        FB2K_console_print("[JIT Queue] PreloadBatch complete: ", allHandles.get_count(), " tracks loaded");
        
    } catch (const std::exception& e) {
        result.error = e.what();
        FB2K_console_print("[JIT Queue] PreloadBatch exception: ", e.what());
    } catch (...) {
        result.error = "Unknown error";
        FB2K_console_print("[JIT Queue] PreloadBatch unknown exception");
    }
    
    return result;
}

//==============================================================================
// Extracted Helpers (cognitive complexity reduction)
//==============================================================================

std::pair<pfc::string8, t_uint32> QueueManager::ParseSubsongUrl(const std::string& url) {
    pfc::string8 pathStr;
    t_uint32 subsongIndex = 0;
    
    auto pipePos = url.find('|');
    if (pipePos != std::string::npos) {
        auto subsongPrefix = url.substr(pipePos + 1);
        if (subsongPrefix.rfind("subsong:", 0) == 0) {
            pathStr = pfc::string8(url.substr(0, pipePos).c_str());
            try {
                subsongIndex = static_cast<t_uint32>(std::stoul(subsongPrefix.substr(8)));
            } catch (...) {
                FB2K_console_print("[JIT Queue] Invalid subsong format: ", url.c_str());
                pathStr = pfc::string8(url.c_str());
            }
        } else {
            pathStr = pfc::string8(url.c_str());
        }
    } else {
        pathStr = pfc::string8(url.c_str());
    }
    
    return { pathStr, subsongIndex };
}

metadb_handle_list QueueManager::BuildHandlesFromUrls(const std::vector<std::string>& urls) {
    metadb_handle_list allHandles;
    
    for (const auto& url : urls) {
        if (url.empty()) continue;
        
        // 规范化后建 handle：流媒体直链没有 filesystem 认领时 g_get_canonical_path
        // 原样透传，有 http filesystem 时得到的正是
        // process_locations 慢路径会产出的同一身份。
        auto [pathStr, subsongIndex] = ParseSubsongUrl(url);
        metadb_handle_ptr handle = SubsongUtils::CreateCanonicalHandle(pathStr.get_ptr(), subsongIndex);
        if (handle.is_valid()) {
            allHandles.add_item(handle);
        } else {
            FB2K_console_print("[JIT Queue] Failed to create handle for: ", url.c_str());
        }
    }
    
    return allHandles;
}

void QueueManager::HandleResolvedItems(metadb_handle_list_cref items, const std::string& trackId,
                                        const std::string& sourceUrl, size_t playlistIndex, bool startPlayback,
                                        bool replaceBufferedNext) {
    if (items.get_count() == 0) {
        FB2K_console_print("[JIT Queue] Error: Failed to resolve URL: ", sourceUrl.c_str());
        AnnounceUrlError(GetSessionCaller(), trackId, "Failed to resolve URL", sourceUrl);
        if (startPlayback) m_pendingOperation = false;
        return;
    }

    // 影子列表在解析期间被删掉时序号为 infinite_size，不能再激活播放。
    auto plm = playlist_manager::get();
    if (playlistIndex >= plm->get_playlist_count()) {
        FB2K_console_print("[JIT Queue] Error: Shadow playlist no longer exists");
        AnnounceUrlError(GetSessionCaller(), trackId, "Shadow playlist no longer exists", sourceUrl);
        if (startPlayback) m_pendingOperation = false;
        return;
    }
    
    // Drop any stale buffered next first so this insert lands right after current,
    // keeping the shadow playlist equal to the [current, next] window.
    if (replaceBufferedNext) {
        RemoveBufferedNextFromShadow(playlistIndex);
    }
    
    size_t currentCount = plm->playlist_get_item_count(playlistIndex);
    plm->playlist_insert_items(playlistIndex, currentCount, items, bit_array_false());
    FB2K_console_print("[JIT Queue] Added to buffer: ", trackId.c_str(), " at position ", currentCount);
    
    if (startPlayback) {
        ActivatePlaybackOnShadow(playlistIndex);
        FB2K_console_print("[JIT Queue] Playback started");
    }
    
    if (startPlayback) m_pendingOperation = false;
}

void QueueManager::AddUrlToPlaylistAsync(const std::string& url, const std::string& trackId,
                                          const std::string& title, bool clearFirst, bool startPlayback,
                                          bool replaceBufferedNext) {
    // Lazy initialization: create shadow playlist on first use
    if (m_shadowPlaylistIndex == pfc::infinite_size) {
        m_shadowPlaylistIndex = GetOrCreateShadowPlaylist();
    }
    
    // Capture copies for the lambda
    std::string urlCopy = url;
    std::string trackIdCopy = trackId;
    
    // ALL playlist operations MUST be done on the main thread
    // Re-read shadow index inside lambda to avoid stale capture
    fb2k::inMainThread([this, urlCopy, trackIdCopy, clearFirst, startPlayback, replaceBufferedNext]() {
        size_t playlistIndex = m_shadowPlaylistIndex.load();
        try {
            auto plm = playlist_manager::get();
            
            // Validate playlist still exists
            if (playlistIndex >= plm->get_playlist_count()) {
                FB2K_console_print("[JIT Queue] Error: Shadow playlist no longer exists");
                if (startPlayback) m_pendingOperation = false;
                AnnounceUrlError(GetSessionCaller(), trackIdCopy, "Shadow playlist no longer exists", urlCopy);
                return;
            }
            
            if (clearFirst) {
                plm->playlist_clear(playlistIndex);
            }

            // Fast path: 流媒体直链 / 本地文件路径规范化后同步生成 handle,
            // 完全绕开 process_locations_async 的 fb2k 进度对话框 (零弹窗、瞬间返回)。
            // 与 BuildHandlesFromUrls 同一解析：`|subsong:N` 拆出、路径规范化，
            // 使快路径产出的身份与慢路径一致。
            // Slow path: .pls / .m3u / .cue 等 wrapper 必须走 process_locations_async 展开。
            if (!PlaylistFormatUtils::LooksLikePlaylistWrapper(urlCopy)) {
                auto [pathStr, subsongIndex] = ParseSubsongUrl(urlCopy);
                metadb_handle_ptr handle = SubsongUtils::CreateCanonicalHandle(pathStr.get_ptr(), subsongIndex);
                if (handle.is_valid()) {
                    metadb_handle_list items;
                    items.add_item(handle);
                    HandleResolvedItems(items, trackIdCopy, urlCopy, playlistIndex, startPlayback, replaceBufferedNext);
                    return;
                }
                // handle_create 失败 → 回退到 slow path 让 fb2k 自己尝试解析
                FB2K_console_print("[JIT Queue] handle_create failed for URL, falling back to process_locations_async: ", urlCopy.c_str());
            }

            pfc::string_list_impl pathList;
            pathList.add_item(urlCopy.c_str());

            auto filter = playlist_incoming_item_filter_v2::get();
            // 展开期间影子列表可能被挪动或删掉，完成时重读锁回调维护的序号，不用发起时的。
            auto notify = process_locations_notify::create(
                [this, trackIdCopy, urlCopy, startPlayback, replaceBufferedNext](metadb_handle_list_cref items) {
                    HandleResolvedItems(items, trackIdCopy, urlCopy, m_shadowPlaylistIndex.load(), startPlayback,
                                        replaceBufferedNext);
                }
            );

            filter->process_locations_async(
                pathList,
                playlist_incoming_item_filter_v2::op_flag_delay_ui | playlist_incoming_item_filter_v2::op_flag_background,
                nullptr, nullptr, nullptr, notify
            );
            
        } catch (const std::exception& e) {
            FB2K_console_print("[JIT Queue] Exception in AddUrlToPlaylistAsync: ", e.what());
            if (startPlayback) m_pendingOperation = false;
            AnnounceUrlError(GetSessionCaller(), trackIdCopy, e.what(), urlCopy);
        } catch (...) {
            FB2K_console_print("[JIT Queue] Unknown exception in AddUrlToPlaylistAsync");
            if (startPlayback) m_pendingOperation = false;
            AnnounceUrlError(GetSessionCaller(), trackIdCopy, "Unknown error", urlCopy);
        }
    });
}

//==============================================================================
// Local File Handling (for non-streaming paths)
//==============================================================================

void QueueManager::AddLocalFileToPlaylistAsync(const std::string& path, const std::string& trackId,
                                                const std::string& title, bool clearFirst, bool startPlayback,
                                                bool replaceBufferedNext) {
    // Lazy initialization: create shadow playlist on first use
    if (m_shadowPlaylistIndex == pfc::infinite_size) {
        m_shadowPlaylistIndex = GetOrCreateShadowPlaylist();
    }
    
    // Capture copies for the lambda
    std::string pathCopy = path;
    std::string trackIdCopy = trackId;
    
    FB2K_console_print("[JIT Queue] AddLocalFileToPlaylistAsync (LOCAL): ", path.c_str());
    
    // ALL playlist operations MUST be done on the main thread
    // Re-read shadow index inside lambda to avoid stale capture
    fb2k::inMainThread([this, pathCopy, trackIdCopy, clearFirst, startPlayback, replaceBufferedNext]() {
        try {
            size_t playlistIndex = m_shadowPlaylistIndex.load();
            auto plm = playlist_manager::get();
            
            if (playlistIndex >= plm->get_playlist_count()) {
                FB2K_console_print("[JIT Queue] Error: Shadow playlist no longer exists");
                if (startPlayback) m_pendingOperation = false;
                AnnouncePathError(GetSessionCaller(), trackIdCopy, "Shadow playlist no longer exists", pathCopy);
                return;
            }
            
            if (clearFirst) {
                plm->playlist_clear(playlistIndex);
            }
            
            // Synchronous process_locations — proven method for local files
            pfc::string_list_impl pathList;
            pathList.add_item(pathCopy.c_str());
            
            auto piif = playlist_incoming_item_filter::get();
            metadb_handle_list items;
            piif->process_locations(pathList, items, true, nullptr, nullptr, core_api::get_main_window());
            
            if (items.get_count() == 0) {
                FB2K_console_print("[JIT Queue] Error: Failed to resolve local path: ", pathCopy.c_str());
                AnnouncePathError(GetSessionCaller(), trackIdCopy, "Failed to resolve local file path", pathCopy);
                if (startPlayback) m_pendingOperation = false;
                return;
            }

            // process_locations 开着模态进度框，影子列表可能被挪动或删掉，重读锁回调维护的序号。
            playlistIndex = m_shadowPlaylistIndex.load();
            if (playlistIndex >= plm->get_playlist_count()) {
                FB2K_console_print("[JIT Queue] Error: Shadow playlist no longer exists");
                if (startPlayback) m_pendingOperation = false;
                AnnouncePathError(GetSessionCaller(), trackIdCopy, "Shadow playlist no longer exists", pathCopy);
                return;
            }
            
            // Drop any stale buffered next first so this insert lands right after
            // current, keeping the shadow playlist equal to the [current, next] window.
            if (replaceBufferedNext) {
                RemoveBufferedNextFromShadow(playlistIndex);
            }
            
            size_t currentCount = plm->playlist_get_item_count(playlistIndex);
            plm->playlist_insert_items(playlistIndex, currentCount, items, bit_array_false());
            
            FB2K_console_print("[JIT Queue] Local file added to buffer: ", trackIdCopy.c_str(), 
                               " at position ", currentCount, " (", items.get_count(), " items)");
            
            if (startPlayback) {
                ActivatePlaybackOnShadow(playlistIndex);
                FB2K_console_print("[JIT Queue] Local file playback started");
            }
            
            if (startPlayback) m_pendingOperation = false;
            
        } catch (const std::exception& e) {
            FB2K_console_print("[JIT Queue] Exception in AddLocalFileToPlaylistAsync: ", e.what());
            if (startPlayback) m_pendingOperation = false;
            AnnouncePathError(GetSessionCaller(), trackIdCopy, e.what(), pathCopy);
        } catch (...) {
            FB2K_console_print("[JIT Queue] Unknown exception in AddLocalFileToPlaylistAsync");
            if (startPlayback) m_pendingOperation = false;
            AnnouncePathError(GetSessionCaller(), trackIdCopy, "Unknown error", pathCopy);
        }
    });
}
