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

class QueueLocationNotify : public process_locations_notify {
public:
    QueueLocationNotify(func_t completed, std::function<void()> aborted)
        : m_completed(std::move(completed)), m_aborted(std::move(aborted)) {}
    void on_completion(metadb_handle_list_cref items) override { m_completed(items); }
    void on_aborted() override { m_aborted(); }
private:
    func_t m_completed;
    std::function<void()> m_aborted;
};

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
    
    if (url.length() >= 7) {
        std::string prefix = url.substr(0, 7);
        std::transform(prefix.begin(), prefix.end(), prefix.begin(), ::tolower);
        if (prefix == "file://") {
            return true;
        }
    }
    
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

    // 先把会失败的输入判定完，再撤销新协议的控制；转换只建立 handle，不访问文件内容。
    metadb_handle_list allHandles;
    try {
        allHandles = BuildHandlesFromUrls(urls);
    } catch (const std::exception& error) {
        result.error = error.what();
        return result;
    } catch (...) {
        result.error = "Unknown error";
        return result;
    }
    if (allHandles.get_count() == 0) {
        result.error = "No valid handles created from URLs";
        return result;
    }
    
    FB2K_console_print("[JIT Queue] PreloadBatch: ", urls.size(), " tracks, startIndex=", startIndex,
                       ", replace=", replace ? "true" : "false");

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

        LegacyTakeover();
        
        if (replace) {
            plm->playlist_clear(m_shadowPlaylistIndex);
            std::lock_guard<std::mutex> lock(m_mutex);
            m_rows.Reset();
        }
        
        VerifyRows(m_shadowPlaylistIndex);
        size_t base = plm->playlist_get_item_count(m_shadowPlaylistIndex);
        plm->playlist_insert_items(m_shadowPlaylistIndex, base, allHandles, pfc::bit_array_true());
        
        FB2K_console_print("[JIT Queue] PreloadBatch: Inserted ", allHandles.get_count(), 
                           " tracks at position ", base);
        
        bool alreadyPlaying = (m_state.load() == State::Active) && !replace;

        // Appended rows are batch rows of the playing session; a batch that starts playback makes
        // every row of the shadow playlist a batch row, so any of them counts as the session's.
        const std::vector<jit_shadow::HandleKey> batchRows =
            alreadyPlaying ? jit_queue_detail::HandleKeys(allHandles) : ReadRowHandles(m_shadowPlaylistIndex);
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            const jit_shadow::EntrySerial entry = ++m_lastEntry;
            if (alreadyPlaying) {
                m_rows.Insert(base, entry, jit_shadow::EntryKind::Batch, batchRows);
            } else {
                m_rows.ResetAsBatch(batchRows, entry);
                // Starting playback replaces the session: its pending adds must not land
                m_generations.EndSession();
            }
        }
        
        if (!alreadyPlaying) {
            size_t playFrom = base + startIndex;
            if (playFrom >= plm->playlist_get_item_count(m_shadowPlaylistIndex)) {
                playFrom = base;
            }
            
            ActivatePlaybackOnShadow(m_shadowPlaylistIndex, playFrom);
            
            m_state = State::Active;
            
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                // Starting playback starts a session owned by this page; appending to a playing
                // session leaves its owner, who also gets the preloadComplete below.
                m_sessionCaller = caller;
                m_currentTrackId.clear();
                m_nextTrackId.clear();
                m_currentTitle.clear();
                m_nextTitle.clear();
                m_currentEntry = 0;
                m_nextEntry = 0;
                m_needNextRequested = false;
            }
        }
        
        result.success = true;
        result.tracksAdded = allHandles.get_count();
        
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
// Helpers
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
                                        const std::string& location, bool locationIsPath,
                                        jit_shadow::EntrySerial entry, jit_shadow::Ticket ticket) {
    // A later command superseded this add: no insert, no playback, no event
    if (!AddIsLive(ticket, trackId, "resolved")) {
        return;
    }
    const bool currentSlot = ticket.slot == jit_shadow::Slot::Current;

    auto announceError = [&](const std::string& error) {
        if (ReportSessionAdd(entry, false, error)) return;
        if (locationIsPath) {
            AnnouncePathError(GetSessionCaller(), trackId, error, location);
        } else {
            AnnounceUrlError(GetSessionCaller(), trackId, error, location);
        }
    };

    if (items.get_count() == 0) {
        const char* error = locationIsPath ? "Failed to resolve local file path" : "Failed to resolve URL";
        FB2K_console_print("[JIT Queue] Error: ", error, ": ", location.c_str());
        announceError(error);
        return;
    }

    if (items.get_count() > 1 && SessionOwnsEntry(entry)) {
        ReportSessionAdd(entry, false, "multipleResources");
        return;
    }
    if (SessionOwnsEntry(entry)) {
        const auto [path, subsong] = ParseSubsongUrl(location);
        (void)subsong;
        if (strcmp(path.get_ptr(), location.c_str()) == 0 &&
            PlaylistFormatUtils::ExtensionEquals(PlaylistFormatUtils::ExtractExtension(location), ".cue")) {
            ReportSessionAdd(entry, false, "subsongRequired");
            return;
        }
    }

    // 解析期间（流媒体异步展开，或本地 process_locations 开着模态进度框）影子列表可能被挪动或
    // 删掉，重读锁回调维护的序号；被删掉时序号为 infinite_size，不能再激活播放。
    const size_t playlistIndex = m_shadowPlaylistIndex.load();
    auto plm = playlist_manager::get();
    if (playlistIndex >= plm->get_playlist_count()) {
        FB2K_console_print("[JIT Queue] Error: Shadow playlist no longer exists");
        announceError("Shadow playlist no longer exists");
        return;
    }

    if (!currentSlot) {
        RemoveBufferedNextFromShadow(playlistIndex);
    }

    VerifyRows(playlistIndex);
    const size_t count = plm->playlist_get_item_count(playlistIndex);
    size_t at;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        // playNow adds the current entry: its rows go before a next entry that resolved first
        at = currentSlot ? m_rows.CurrentInsertPosition(m_nextEntry, count) : count;
    }
    const size_t base = plm->playlist_insert_items(playlistIndex, at, items, bit_array_false());
    if (base != pfc::infinite_size) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_rows.Insert(base, entry, jit_shadow::EntryKind::Single, jit_queue_detail::HandleKeys(items));
    }
    FB2K_console_print("[JIT Queue] Added to buffer: ", trackId.c_str(), " at position ", at, " (",
                       items.get_count(), " items)");

    // The current entry's first row: a next entry that resolved first stays after it
    if (currentSlot && base != pfc::infinite_size) {
        ActivatePlaybackOnShadow(playlistIndex, base);
        FB2K_console_print("[JIT Queue] Playback started");
    }
    ReportSessionAdd(entry, base != pfc::infinite_size, "insertFailed");
}

void QueueManager::AddUrlToPlaylistAsync(const std::string& url, const std::string& trackId,
                                          const std::string& title, jit_shadow::EntrySerial entry,
                                          jit_shadow::Ticket ticket) {
    if (m_shadowPlaylistIndex == pfc::infinite_size) {
        m_shadowPlaylistIndex = GetOrCreateShadowPlaylist();
    }

    std::string urlCopy = url;
    std::string trackIdCopy = trackId;

    // Playlist edits must run on the main thread; the index is read there, not captured.
    fb2k::inMainThread([this, urlCopy, trackIdCopy, entry, ticket]() {
        if (!AddIsLive(ticket, trackIdCopy, "queued")) {
            return;
        }
        size_t playlistIndex = m_shadowPlaylistIndex.load();
        try {
            auto plm = playlist_manager::get();

            if (playlistIndex >= plm->get_playlist_count()) {
                FB2K_console_print("[JIT Queue] Error: Shadow playlist no longer exists");
                if (!ReportSessionAdd(entry, false, "shadowPlaylistMissing")) {
                    AnnounceUrlError(GetSessionCaller(), trackIdCopy, "Shadow playlist no longer exists", urlCopy);
                }
                return;
            }

            if (ticket.slot == jit_shadow::Slot::Current) {
                plm->playlist_clear(playlistIndex);
                std::lock_guard<std::mutex> lock(m_mutex);
                m_rows.Reset();
            }

            // 快路径同步建 handle，不经 process_locations_async，不弹进度框；解析与 BuildHandlesFromUrls
            // 相同，所得身份与慢路径一致。.pls、.m3u、.cue 等容器须走慢路径展开。
            // 写明 `|subsong:N` 时直接按它建 handle：带查询参数的网络 CUE（`a.cue?token=x|subsong:2`）
            // 裁掉查询后像容器，进慢路径会把后缀当成地址的一部分。
            auto [pathStr, subsongIndex] = ParseSubsongUrl(urlCopy);
            const bool namesSubsong = strcmp(pathStr.get_ptr(), urlCopy.c_str()) != 0;
            if (namesSubsong || !PlaylistFormatUtils::LooksLikePlaylistWrapper(urlCopy)) {
                metadb_handle_ptr handle = SubsongUtils::CreateCanonicalHandle(pathStr.get_ptr(), subsongIndex);
                if (handle.is_valid()) {
                    metadb_handle_list items;
                    items.add_item(handle);
                    HandleResolvedItems(items, trackIdCopy, urlCopy, false, entry, ticket);
                    return;
                }
                FB2K_console_print("[JIT Queue] handle_create failed for URL, falling back to process_locations_async: ", urlCopy.c_str());
            }

            pfc::string_list_impl pathList;
            pathList.add_item(urlCopy.c_str());

            auto filter = playlist_incoming_item_filter_v2::get();
            // 展开期间影子列表可能被挪动或删掉，HandleResolvedItems 在完成时重读序号，不用发起时的。
            process_locations_notify::func_t completed = [this, trackIdCopy, urlCopy, entry, ticket](metadb_handle_list_cref items) {
                try {
                    HandleResolvedItems(items, trackIdCopy, urlCopy, false, entry, ticket);
                } catch (...) {
                    if (AddIsLive(ticket, trackIdCopy, "completion exception") && !ReportSessionAdd(entry, false, "insertFailed")) {
                        AnnounceUrlError(GetSessionCaller(), trackIdCopy, "Failed to insert resolved resources", urlCopy);
                    }
                }
            };
            process_locations_notify::ptr notify;
            if (SessionOwnsEntry(entry)) {
                notify = new service_impl_t<QueueLocationNotify>(completed, [this, entry, ticket, trackIdCopy]() {
                    if (AddIsLive(ticket, trackIdCopy, "aborted")) ReportSessionAdd(entry, false, "resolutionAborted");
                });
            } else {
                notify = process_locations_notify::create(std::move(completed));
            }

            filter->process_locations_async(
                pathList,
                playlist_incoming_item_filter_v2::op_flag_delay_ui | playlist_incoming_item_filter_v2::op_flag_background,
                nullptr, nullptr, nullptr, notify
            );

        } catch (const std::exception& e) {
            FB2K_console_print("[JIT Queue] Exception in AddUrlToPlaylistAsync: ", e.what());
            if (AddIsLive(ticket, trackIdCopy, "exception") && !ReportSessionAdd(entry, false, "resolutionFailed")) {
                AnnounceUrlError(GetSessionCaller(), trackIdCopy, e.what(), urlCopy);
            }
        } catch (...) {
            FB2K_console_print("[JIT Queue] Unknown exception in AddUrlToPlaylistAsync");
            if (AddIsLive(ticket, trackIdCopy, "exception") && !ReportSessionAdd(entry, false, "resolutionFailed")) {
                AnnounceUrlError(GetSessionCaller(), trackIdCopy, "Unknown error", urlCopy);
            }
        }
    });
}

//==============================================================================
// Local File Handling (for non-streaming paths)
//==============================================================================

void QueueManager::AddLocalFileToPlaylistAsync(const std::string& path, const std::string& trackId,
                                                const std::string& title, jit_shadow::EntrySerial entry,
                                                jit_shadow::Ticket ticket) {
    if (m_shadowPlaylistIndex == pfc::infinite_size) {
        m_shadowPlaylistIndex = GetOrCreateShadowPlaylist();
    }

    std::string pathCopy = path;
    std::string trackIdCopy = trackId;

    FB2K_console_print("[JIT Queue] AddLocalFileToPlaylistAsync (LOCAL): ", path.c_str());

    // Playlist edits must run on the main thread; the index is read there, not captured.
    fb2k::inMainThread([this, pathCopy, trackIdCopy, entry, ticket]() {
        if (!AddIsLive(ticket, trackIdCopy, "queued")) {
            return;
        }
        try {
            size_t playlistIndex = m_shadowPlaylistIndex.load();
            auto plm = playlist_manager::get();

            if (playlistIndex >= plm->get_playlist_count()) {
                FB2K_console_print("[JIT Queue] Error: Shadow playlist no longer exists");
                if (!ReportSessionAdd(entry, false, "shadowPlaylistMissing")) {
                    AnnouncePathError(GetSessionCaller(), trackIdCopy, "Shadow playlist no longer exists", pathCopy);
                }
                return;
            }

            if (ticket.slot == jit_shadow::Slot::Current) {
                plm->playlist_clear(playlistIndex);
                std::lock_guard<std::mutex> lock(m_mutex);
                m_rows.Reset();
            }

            metadb_handle_list items;
            auto [pathStr, subsongIndex] = ParseSubsongUrl(pathCopy);
            if (strcmp(pathStr.get_ptr(), pathCopy.c_str()) != 0) {
                // `path|subsong:N` names one subsong: build its handle as BuildHandlesFromUrls does.
                // process_locations would take the suffix as part of the file name.
                metadb_handle_ptr handle = SubsongUtils::CreateCanonicalHandle(pathStr.get_ptr(), subsongIndex);
                if (handle.is_valid()) items.add_item(handle);
            } else {
                // process_locations also expands embedded cuesheets and multi-subsong files.
                pfc::string_list_impl pathList;
                pathList.add_item(pathCopy.c_str());

                auto piif = playlist_incoming_item_filter::get();
                piif->process_locations(pathList, items, true, nullptr, nullptr, core_api::get_main_window());
            }

            HandleResolvedItems(items, trackIdCopy, pathCopy, true, entry, ticket);

        } catch (const std::exception& e) {
            FB2K_console_print("[JIT Queue] Exception in AddLocalFileToPlaylistAsync: ", e.what());
            if (AddIsLive(ticket, trackIdCopy, "exception") && !ReportSessionAdd(entry, false, "resolutionFailed")) {
                AnnouncePathError(GetSessionCaller(), trackIdCopy, e.what(), pathCopy);
            }
        } catch (...) {
            FB2K_console_print("[JIT Queue] Unknown exception in AddLocalFileToPlaylistAsync");
            if (AddIsLive(ticket, trackIdCopy, "exception") && !ReportSessionAdd(entry, false, "resolutionFailed")) {
                AnnouncePathError(GetSessionCaller(), trackIdCopy, "Unknown error", pathCopy);
            }
        }
    });
}
