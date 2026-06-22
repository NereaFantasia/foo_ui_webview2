/**
 * QueueManager.cpp - JIT Queue Manager Implementation
 * 
 * Implements dual-layer queue architecture for streaming media playback.
 * Uses fb2k's shadow playlist as a minimal buffer while frontend manages
 * the full logical queue with play modes.
 * 
 * v1.1.2+: Now supports both HTTP streaming URLs and local file paths.
 * - HTTP URLs: Uses process_locations_async with streaming configuration
 * - Local files: Uses native playlist insertion for optimal playback
 */

#include "pch.h"
#include "core/QueueManager.h"
#include "api/BridgeCore.h"
#include <algorithm>
#include <cctype>

//==============================================================================
// 检测 URL 是否是 playlist wrapper(.pls/.m3u/.cue 等),需要展开成多个 handle。
// 非 wrapper 的本地路径 / 流媒体直链可以直接 metadb::handle_create 同步处理,
// 完全绕开 process_locations_async 的 fb2k 进度对话框。
//==============================================================================
static bool LooksLikePlaylistWrapper(const std::string& url) {
    if (url.empty()) return false;
    auto dotPos = url.find_last_of('.');
    auto sepPos = url.find_last_of("/\\");
    if (dotPos == std::string::npos) return false;
    if (sepPos != std::string::npos && dotPos < sepPos) return false;

    std::string ext = url.substr(dotPos);
    auto q = ext.find_first_of("?#");
    if (q != std::string::npos) ext = ext.substr(0, q);
    std::transform(ext.begin(), ext.end(), ext.begin(),
        [](unsigned char c) { return static_cast<char>(::tolower(c)); });

    static const char* const kWrappers[] = {
        ".pls", ".m3u", ".m3u8", ".asx", ".wpl", ".xspf", ".fpl", ".cue"
    };
    for (auto w : kWrappers) {
        if (ext == w) return true;
    }
    return false;
}

//==============================================================================
// Shadow Playlist Lock
// - Tracks index changes when other playlists are created/deleted/reordered
// - Blocks user from renaming or deleting the shadow playlist
//==============================================================================

class ShadowPlaylistLock : public playlist_lock {
public:
    explicit ShadowPlaylistLock(QueueManager& owner) : m_owner(owner) {}

    // Allow all item-level operations (QueueManager needs add/remove/clear)
    bool query_items_add(t_size, const pfc::list_base_const_t<metadb_handle_ptr>&, const bit_array&) override { return true; }
    bool query_items_reorder(const t_size*, t_size) override { return true; }
    bool query_items_remove(const bit_array&, bool) override { return true; }
    bool query_item_replace(t_size, const metadb_handle_ptr&, const metadb_handle_ptr&) override { return true; }

    // Block rename and delete to protect shadow playlist
    bool query_playlist_rename(const char*, t_size) override { return false; }
    bool query_playlist_remove() override { return false; }

    // Fall through to default action (start playback)
    bool execute_default_action(t_size) override { return false; }

    // SDK notifies us when playlist index shifts
    void on_playlist_index_change(t_size p_new_index) override {
        m_owner.OnShadowPlaylistIndexChanged(p_new_index);
    }

    // SDK notifies us when the locked playlist is removed
    void on_playlist_remove() override {
        m_owner.OnShadowPlaylistRemoved();
    }

    void get_lock_name(pfc::string_base& p_out) override {
        p_out = "JIT Queue Buffer";
    }

    void show_ui() override { /* Shadow playlist has no lock UI */ }

    t_uint32 get_filter_mask() override {
        return filter_rename | filter_remove_playlist;
    }

private:
    QueueManager& m_owner;
};

//==============================================================================
// Singleton Instance
//==============================================================================

QueueManager& QueueManager::GetInstance() {
    static QueueManager instance;
    return instance;
}

//==============================================================================
// Initialization
//==============================================================================

void QueueManager::Initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Don't create shadow playlist on startup - lazy initialization
    // It will be created on first use of JIT Queue
    m_shadowPlaylistIndex = pfc::infinite_size;
    
    FB2K_console_print("[JIT Queue] Initialized (lazy mode - playlist created on first use)");
}

void QueueManager::Shutdown() {
    // Uninstall playlist lock before taking the mutex (SDK calls may trigger callbacks)
    if (m_playlistLock.is_valid()) {
        auto plm = playlist_manager::get();
        size_t idx = m_shadowPlaylistIndex.load();
        if (idx != pfc::infinite_size && idx < plm->get_playlist_count()) {
            plm->playlist_lock_uninstall(idx, m_playlistLock);
        }
        m_playlistLock.release();
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_state = State::Idle;
    m_currentTrackId.clear();
    m_nextTrackId.clear();
    m_currentTitle.clear();
    m_nextTitle.clear();
    
    FB2K_console_print("[JIT Queue] Shutdown");
}

//==============================================================================
// Playlist Lock Callbacks
//==============================================================================

void QueueManager::OnShadowPlaylistIndexChanged(size_t newIndex) {
    size_t oldIndex = m_shadowPlaylistIndex.exchange(newIndex);
    FB2K_console_print("[JIT Queue] Shadow playlist index updated: ", oldIndex, " -> ", newIndex);
}

void QueueManager::OnShadowPlaylistRemoved() {
    m_shadowPlaylistIndex = pfc::infinite_size;
    m_state = State::Idle;
    m_playlistLock.release();
    FB2K_console_print("[JIT Queue] Shadow playlist was removed externally, JIT deactivated");
}

void QueueManager::InstallPlaylistLock(size_t playlistIndex) {
    auto plm = playlist_manager::get();
    if (plm->playlist_lock_is_present(playlistIndex)) {
        return;  // Already locked (e.g., found existing playlist with lock still installed)
    }
    if (!m_playlistLock.is_valid()) {
        m_playlistLock = new service_impl_t<ShadowPlaylistLock>(*this);
    }
    if (plm->playlist_lock_install(playlistIndex, m_playlistLock)) {
        FB2K_console_print("[JIT Queue] Installed playlist lock on shadow playlist at index ", playlistIndex);
    }
}

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
// Core API Methods
//==============================================================================

bool QueueManager::PlayNow(const std::string& trackId, const std::string& title, const std::string& url) {
    // Check for pending operation with timeout safety net
    if (m_pendingOperation.exchange(true)) {
        auto elapsed = std::chrono::steady_clock::now() - m_pendingStartTime;
        if (elapsed < std::chrono::seconds(PENDING_TIMEOUT_SECONDS)) {
            FB2K_console_print("[JIT Queue] PlayNow rejected - operation pending");
            return false;
        }
        // Timeout exceeded — force-reset and proceed
        FB2K_console_print("[JIT Queue] WARNING: pendingOperation timeout (", PENDING_TIMEOUT_SECONDS, "s), force-resetting");
    }
    m_pendingStartTime = std::chrono::steady_clock::now();
    
    bool isLocal = IsLocalFilePath(url);
    FB2K_console_print("[JIT Queue] PlayNow: ", trackId.c_str(), " - ", title.c_str());
    FB2K_console_print("[JIT Queue] URL: ", url.c_str(), " (", isLocal ? "LOCAL" : "STREAM", ")");
    
    // Save and force playbackOrder to Default (0) to prevent wrap-around
    {
        auto plm = playlist_manager::get();
        int currentOrder = static_cast<int>(plm->playback_order_get_active());
        if (m_state.load() == State::Idle) {
            // First activation: save user's playbackOrder and active playlist
            std::lock_guard<std::mutex> lock(m_mutex);
            m_savedPlaybackOrder = currentOrder;
            m_savedActivePlaylist = plm->get_active_playlist();
            FB2K_console_print("[JIT Queue] Saved playbackOrder=", currentOrder, ", activePlaylist=", m_savedActivePlaylist);
        }
        if (currentOrder != 0) {
            plm->playback_order_set_active(0);
            FB2K_console_print("[JIT Queue] Forced playbackOrder to 0 (Default)");
        }
    }
    
    // Update state immediately
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_currentTrackId = trackId;
        m_currentTitle = title;
        m_currentJitUrl = url;  // Save URL for JIT detection
        m_nextTrackId.clear();
        m_nextTitle.clear();
        m_nextJitUrl.clear();
        m_needNextRequested = false;
    }
    
    m_state = State::Active;
    
    // Route based on URL type
    if (isLocal) {
        AddLocalFileToPlaylistAsync(url, trackId, title, true, true);
    } else {
        AddUrlToPlaylistAsync(url, trackId, title, true, true);
    }
    
    return true;
}

bool QueueManager::EnqueueNext(const std::string& trackId, const std::string& title, const std::string& url) {
    // State guard — only accept if Active or WaitingNext
    State currentState = m_state.load();
    if (currentState != State::Active && currentState != State::WaitingNext) {
        FB2K_console_print("[JIT Queue] EnqueueNext rejected - state=", StateToString(currentState));
        return false;
    }
    
    bool isLocal = IsLocalFilePath(url);
    FB2K_console_print("[JIT Queue] EnqueueNext: ", trackId.c_str(), " - ", title.c_str());
    FB2K_console_print("[JIT Queue] URL: ", url.c_str(), " (", isLocal ? "LOCAL" : "STREAM", ")");
    
    // Update state
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        // Warn if overwriting a previously queued next track
        if (!m_nextTrackId.empty() && m_nextTrackId != trackId) {
            FB2K_console_print("[JIT Queue] WARNING: Overwriting next track '", m_nextTrackId.c_str(), 
                               "' with '", trackId.c_str(), "'");
        }
        m_nextTrackId = trackId;
        m_nextTitle = title;
        m_nextJitUrl = url;  // Save URL for JIT detection
        m_needNextRequested = false;
    }
    
    // Transition: WaitingNext → Active (response received)
    m_state = State::Active;
    
    // Route based on URL type. replaceBufferedNext=true so a re-enqueue (e.g. the
    // frontend re-requesting next after a replay/"previous") physically replaces any
    // previously buffered next instead of appending after it — otherwise the stale
    // next is left as an orphan between current and the new next, which native
    // prev/next can land on, escaping the [current, next] window and killing the queue.
    if (isLocal) {
        AddLocalFileToPlaylistAsync(url, trackId, title, false, false, true);
    } else {
        AddUrlToPlaylistAsync(url, trackId, title, false, false, true);
    }
    
    return true;
}

bool QueueManager::Skip() {
    if (m_state.load() == State::Idle) {
        return false;
    }
    
    auto pc = playback_control::get();
    pc->next();
    
    return true;
}

void QueueManager::Stop(bool clearBuffer) {
    auto pc = playback_control::get();
    pc->stop();
    
    if (clearBuffer) {
        // Clear() internally calls RestoreUserContext() and sets State::Idle
        Clear();
    } else {
        // Only restore here when not clearing (Clear already handles it)
        RestoreUserContext();
        m_state = State::Idle;
    }
}

void QueueManager::Clear() {
    // Restore user context before taking lock (calls playlist_manager SDK)
    // RestoreUserContext is idempotent (-1 guard), safe if called multiple times
    RestoreUserContext();
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto plm = playlist_manager::get();
    // S134 fix: merge two conditions to reduce nesting from 4 to 3
    if (m_shadowPlaylistIndex != pfc::infinite_size &&
        m_shadowPlaylistIndex < plm->get_playlist_count()) {
        // Clear shadow playlist content
        plm->playlist_clear(m_shadowPlaylistIndex);
        
        // Restore user's active playlist instead of hardcoding 0
        if (plm->get_active_playlist() == m_shadowPlaylistIndex) {
            if (m_savedActivePlaylist < plm->get_playlist_count()) {
                plm->set_active_playlist(m_savedActivePlaylist);
                FB2K_console_print("[JIT Queue] Restored active playlist to ", m_savedActivePlaylist);
            } else if (plm->get_playlist_count() > 0) {
                plm->set_active_playlist(0);
                FB2K_console_print("[JIT Queue] Restored active playlist to 0 (saved invalid)");
            }
        }
    }
    
    m_currentTrackId.clear();
    m_nextTrackId.clear();
    m_currentTitle.clear();
    m_nextTitle.clear();
    m_currentJitUrl.clear();
    m_nextJitUrl.clear();
    m_needNextRequested = false;
    m_state = State::Idle;
    
    FB2K_console_print("[JIT Queue] Buffer cleared");
}

//==============================================================================
// Batch Preload
//==============================================================================

QueueManager::PreloadResult QueueManager::PreloadBatch(
    const std::vector<std::string>& urls, size_t startIndex, bool replace) {
    
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
        BridgeCore::GetInstance().EmitEvent("jitQueue:preloadComplete", {
            {"count", allHandles.get_count()},
            {"startIndex", startIndex},
            {"replace", replace}
        });
        
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
// State Queries
//==============================================================================

std::string QueueManager::GetCurrentTrackId() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentTrackId;
}

std::string QueueManager::GetNextTrackId() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_nextTrackId;
}

size_t QueueManager::GetBufferSize() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_shadowPlaylistIndex == pfc::infinite_size) {
        return 0;
    }
    
    auto plm = playlist_manager::get();
    if (m_shadowPlaylistIndex >= plm->get_playlist_count()) {
        return 0;
    }
    
    return plm->playlist_get_item_count(m_shadowPlaylistIndex);
}

size_t QueueManager::GetShadowPlaylistIndex() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_shadowPlaylistIndex;
}

bool QueueManager::IsActive() const {
    return m_state.load() != State::Idle;
}

//==============================================================================
// Playback Callbacks
//==============================================================================

void QueueManager::OnPlaybackNewTrack(const metadb_handle_ptr& track) {
    FB2K_console_print("[JIT Queue] OnPlaybackNewTrack called");
    
    // Check if playing from our shadow playlist
    bool isFromShadow = IsPlayingFromShadowPlaylist();
    FB2K_console_print("[JIT Queue] IsPlayingFromShadowPlaylist: ", isFromShadow ? "true" : "false");
    
    if (!isFromShadow) {
        // Not our track, deactivate and clear JIT URLs
        if (m_state.load() != State::Idle) {
            m_state = State::Idle;
            RestoreUserContext();
            std::lock_guard<std::mutex> lock(m_mutex);
            m_currentJitUrl.clear();
            m_nextJitUrl.clear();
            FB2K_console_print("[JIT Queue] Playback switched away from JIT queue");
        }
        return;
    }
    
    m_state = State::Active;
    FB2K_console_print("[JIT Queue] JIT Queue is now active");
    
    // Capture the actually-playing track path BEFORE shifting. on_playback_new_track
    // also fires for "previous" / replay-current, where the playing track still
    // matches m_currentJitUrl (not m_nextJitUrl). An unconditional shift would then
    // consume the buffered next track and break the FM queue, so we must distinguish
    // a real forward advance from a replay first.
    const char* playingPath = track.is_empty() ? nullptr : track->get_path();
    
    // Shift track IDs and URLs: next becomes current — only when we actually
    // advanced to the buffered next track.
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        FB2K_console_print("[JIT Queue] Current trackId: ", m_currentTrackId.c_str(), ", Next trackId: ", m_nextTrackId.c_str());
        
        // Match the playing path against the pre-shift buffered URLs using the same
        // strcmp method as IsPlayingFromShadowPlaylist (covers both local-file URLs
        // and stream proxy URLs).
        bool advancedToNext = playingPath != nullptr && !m_nextJitUrl.empty() &&
                              strcmp(playingPath, m_nextJitUrl.c_str()) == 0;
        bool replayedCurrent = playingPath != nullptr && !m_currentJitUrl.empty() &&
                               strcmp(playingPath, m_currentJitUrl.c_str()) == 0;
        
        if (!m_nextTrackId.empty() && advancedToNext) {
            // Real forward advance to the buffered next track → shift next -> current
            m_currentTrackId = m_nextTrackId;
            m_currentTitle = m_nextTitle;
            m_currentJitUrl = m_nextJitUrl;  // Shift URL too
            m_nextTrackId.clear();
            m_nextTitle.clear();
            m_nextJitUrl.clear();
            FB2K_console_print("[JIT Queue] Shifted: next -> current, new currentId: ", m_currentTrackId.c_str());
        } else if (replayedCurrent) {
            // "Previous" / replay of the current track → keep current and next intact
            // so the buffered next stays available and the FM queue remains usable.
            FB2K_console_print("[JIT Queue] Replay/previous detected (playing == current), buffer kept intact");
        } else {
            // First play via playNow() (next buffer empty), or path matched neither URL
            // (e.g. PreloadBatch playlist-index path) → keep currentTrackId as is.
            FB2K_console_print("[JIT Queue] No shift (first play / unmatched path)");
        }
        
        // Reset the needNext flag so we can request again
        m_needNextRequested = false;
    }
    
    // Clean up played tracks (pass the authoritative now-playing handle so the
    // current track is reliably pinned to index 0; see CleanupPlayedTracks).
    CleanupPlayedTracks(track);
    
    // Emit track changed event
    std::string currentId = GetCurrentTrackId();
    std::string currentTitle;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        currentTitle = m_currentTitle;
    }
    
    FB2K_console_print("[JIT Queue] Emitting jitQueue:trackChanged, trackId: ", currentId.c_str());
    BridgeCore::GetInstance().EmitEvent("jitQueue:trackChanged", {
        {"trackId", currentId},
        {"title", currentTitle}
    });
    
    // Only request next if buffer is empty (don't re-request if next is already queued)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_nextTrackId.empty()) {
            // Unlock before calling RequestNextTrack to avoid lock ordering issues
        } else {
            FB2K_console_print("[JIT Queue] Next track already buffered, skipping needNext request");
            return;
        }
    }
    RequestNextTrack();
}

void QueueManager::OnPlaybackStop(play_control::t_stop_reason reason) {
    FB2K_console_print("[JIT Queue] OnPlaybackStop called, reason: ", static_cast<int>(reason), ", state: ", static_cast<int>(m_state.load()));
    
    if (m_state.load() == State::Idle) {
        return;
    }
    
    // Handle different stop reasons
    if (reason == play_control::stop_reason_eof) {
        FB2K_console_print("[JIT Queue] Playback stopped - EOF");
        
        size_t bufferSize = GetBufferSize();
        FB2K_console_print("[JIT Queue] Buffer size: ", bufferSize);
        
        if (bufferSize > 0) {
            // There are more tracks in buffer, foobar should auto-play
            // But if it doesn't, we might need to trigger it
            FB2K_console_print("[JIT Queue] Buffer has tracks, waiting for auto-play");
        } else {
            // Buffer exhausted, notify frontend
            FB2K_console_print("[JIT Queue] Buffer empty, notifying frontend");
            NotifyListExhausted();
        }
    } else if (reason == play_control::stop_reason_starting_another) {
        // This is normal when switching tracks, do nothing
        FB2K_console_print("[JIT Queue] Stop reason: starting another track");
    } else if (reason == play_control::stop_reason_user) {
        FB2K_console_print("[JIT Queue] Playback stopped by user");
        m_state = State::Idle;
        RestoreUserContext();
    }
}

void QueueManager::OnPlaybackTime(double time, double duration) {
    if (m_state.load() == State::Idle) {
        return;
    }
    
    // Check if we should prefetch (when near end of track and no next track buffered)
    double remaining = duration - time;
    
    if (remaining <= PREFETCH_THRESHOLD_SECONDS) {
        bool shouldRequest = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            
            // Request next track if we don't have one and haven't already requested
            if (m_nextTrackId.empty() && !m_needNextRequested) {
                m_needNextRequested = true;
                shouldRequest = true;
                FB2K_console_print("[JIT Queue] Prefetch trigger: ", remaining, "s remaining");
            }
        }
        
        // Use RequestNextTrack to go through state machine (WaitingNext transition)
        if (shouldRequest) {
            fb2k::inMainThread([this]() {
                RequestNextTrack();
            });
        }
    }
}

//==============================================================================
// User Context Save/Restore
//==============================================================================

void QueueManager::RestoreUserContext() {
    // Read saved values under lock to prevent data race
    int savedOrder;
    size_t savedPlaylist;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        savedOrder = m_savedPlaybackOrder;
        savedPlaylist = m_savedActivePlaylist;
    }
    
    auto plm = playlist_manager::get();
    
    if (savedOrder >= 0) {
        int currentOrder = static_cast<int>(plm->playback_order_get_active());
        if (currentOrder != savedOrder) {
            plm->playback_order_set_active(static_cast<t_size>(savedOrder));
            FB2K_console_print("[JIT Queue] Restored playbackOrder to ", savedOrder);
        }
        std::lock_guard<std::mutex> lock(m_mutex);
        m_savedPlaybackOrder = -1;
    }
    
    // Also restore active playlist if currently pointing at shadow
    size_t shadowIdx = m_shadowPlaylistIndex.load();
    if (plm->get_active_playlist() == shadowIdx && savedPlaylist < plm->get_playlist_count()) {
        plm->set_active_playlist(savedPlaylist);
        FB2K_console_print("[JIT Queue] Restored active playlist to ", savedPlaylist);
    }
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
    auto mm = metadb::get();
    metadb_handle_list allHandles;
    
    for (const auto& url : urls) {
        if (url.empty()) continue;
        
        auto [pathStr, subsongIndex] = ParseSubsongUrl(url);
        metadb_handle_ptr handle = mm->handle_create(pathStr.get_ptr(), subsongIndex);
        if (handle.is_valid()) {
            allHandles.add_item(handle);
        } else {
            FB2K_console_print("[JIT Queue] Failed to create handle for: ", url.c_str());
        }
    }
    
    return allHandles;
}

void QueueManager::ActivatePlaybackOnShadow(size_t playlistIndex) {
    auto pc = playback_control::get();
    auto plm = playlist_manager::get();
    
    plm->set_active_playlist(playlistIndex);
    plm->set_playing_playlist(playlistIndex);
    pc->start(playback_control::track_command_play, false);
    
    // Confirm Active state only if not already stopped/cleared
    if (m_state.load() != State::Idle) {
        m_state = State::Active;
    }
}

void QueueManager::HandleResolvedItems(metadb_handle_list_cref items, const std::string& trackId,
                                        const std::string& sourceUrl, size_t playlistIndex, bool startPlayback,
                                        bool replaceBufferedNext) {
    if (items.get_count() == 0) {
        FB2K_console_print("[JIT Queue] Error: Failed to resolve URL: ", sourceUrl.c_str());
        BridgeCore::GetInstance().EmitEvent("jitQueue:error", {
            {"trackId", trackId},
            {"error", "Failed to resolve URL"},
            {"url", sourceUrl}
        });
        if (startPlayback) m_pendingOperation = false;
        return;
    }
    
    // Drop any stale buffered next first so this insert lands right after current,
    // keeping the shadow playlist equal to the [current, next] window.
    if (replaceBufferedNext) {
        RemoveBufferedNextFromShadow(playlistIndex);
    }
    
    auto plm = playlist_manager::get();
    if (playlistIndex < plm->get_playlist_count()) {
        size_t currentCount = plm->playlist_get_item_count(playlistIndex);
        plm->playlist_insert_items(playlistIndex, currentCount, items, bit_array_false());
        FB2K_console_print("[JIT Queue] Added to buffer: ", trackId.c_str(), " at position ", currentCount);
    }
    
    if (startPlayback) {
        ActivatePlaybackOnShadow(playlistIndex);
        FB2K_console_print("[JIT Queue] Playback started");
    }
    
    if (startPlayback) m_pendingOperation = false;
}

//==============================================================================
// Internal Methods
//==============================================================================

size_t QueueManager::GetOrCreateShadowPlaylist() {
    auto plm = playlist_manager::get();
    size_t count = plm->get_playlist_count();
    
    // Look for existing shadow playlist
    for (size_t i = 0; i < count; i++) {
        pfc::string8 name;
        plm->playlist_get_name(i, name);
        if (strcmp(name.c_str(), SHADOW_PLAYLIST_NAME) == 0) {
            // Ensure lock is installed (may be missing if found from previous session)
            InstallPlaylistLock(i);
            return i;
        }
    }
    
    // Create new shadow playlist at the end
    size_t newIndex = plm->create_playlist(SHADOW_PLAYLIST_NAME, pfc::infinite_size, pfc::infinite_size);
    
    // Install lock for index tracking and protection
    InstallPlaylistLock(newIndex);
    
    FB2K_console_print("[JIT Queue] Created shadow playlist: ", SHADOW_PLAYLIST_NAME);
    
    return newIndex;
}

bool QueueManager::IsPlayingFromShadowPlaylist() const {
    // Get expected JIT URL
    std::string expectedUrl;
    std::string nextUrl;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        expectedUrl = m_currentJitUrl;
        nextUrl = m_nextJitUrl;
    }
    
    if (expectedUrl.empty() && nextUrl.empty()) {
        // PreloadBatch path — no JIT URL set, fall back to playlist-index matching
        if (m_state.load() != State::Idle && m_shadowPlaylistIndex.load() != pfc::infinite_size) {
            size_t playingPl = pfc::infinite_size, playingItem = pfc::infinite_size;
            auto plm = playlist_manager::get();
            if (plm->get_playing_item_location(&playingPl, &playingItem)) {
                bool match = (playingPl == m_shadowPlaylistIndex.load());
                FB2K_console_print("[JIT Queue] IsPlayingFromShadowPlaylist: Playlist-index fallback, match=", match ? "true" : "false");
                return match;
            }
        }
        FB2K_console_print("[JIT Queue] IsPlayingFromShadowPlaylist: No JIT URL set, state=Idle");
        return false;
    }
    
    // Get currently playing track using playback_control
    auto pc = playback_control::get();
    metadb_handle_ptr track;
    
    if (!pc->get_now_playing(track) || track.is_empty()) {
        FB2K_console_print("[JIT Queue] IsPlayingFromShadowPlaylist: No track playing (get_now_playing failed)");
        return false;
    }
    
    // Get the path of the currently playing track
    const char* playingPath = track->get_path();
    FB2K_console_print("[JIT Queue] Playing path: ", playingPath);
    FB2K_console_print("[JIT Queue] Expected current URL: ", expectedUrl.c_str());
    if (!nextUrl.empty()) {
        FB2K_console_print("[JIT Queue] Expected next URL: ", nextUrl.c_str());
    }
    
    // Compare paths - check both current and next URL
    bool result = (strcmp(playingPath, expectedUrl.c_str()) == 0) || 
                  (!nextUrl.empty() && strcmp(playingPath, nextUrl.c_str()) == 0);
    
    FB2K_console_print("[JIT Queue] IsPlayingFromShadowPlaylist: ", result ? "true" : "false");
    
    return result;
}

void QueueManager::CleanupPlayedTracks(const metadb_handle_ptr& playingTrack) {
    // Read shadow playlist index under lock, then release lock before SDK calls
    // to prevent deadlock (SDK callbacks may re-enter QueueManager and attempt to acquire m_mutex)
    size_t shadowIdx;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        shadowIdx = m_shadowPlaylistIndex;
    }
    
    if (shadowIdx == pfc::infinite_size) {
        return;
    }
    
    auto plm = playlist_manager::get();
    
    if (shadowIdx >= plm->get_playlist_count()) {
        return;
    }
    
    // Locate the currently playing track's index authoritatively by matching the
    // now-playing handle against the shadow playlist contents. We cannot rely on
    // get_playing_item_location() here: during on_playback_new_track on EOF
    // auto-advance it reports the PREVIOUS item's index, so cleanup would lag by
    // one and leave a stale "played" track in front of current. That stale item
    // is exactly what native "previous" steps back onto, escaping the
    // [current, next] window and silently deactivating the JIT queue.
    size_t playingItem = pfc::infinite_size;
    if (playingTrack.is_valid()) {
        size_t count = plm->playlist_get_item_count(shadowIdx);
        for (size_t i = 0; i < count; ++i) {
            metadb_handle_ptr h;
            // metadb handles are interned, so pointer equality identifies the track.
            if (plm->playlist_get_item_handle(h, shadowIdx, i) && h == playingTrack) {
                playingItem = i;
                break;
            }
        }
    }
    
    // Fallback to the playing-item-location query if the handle wasn't found
    // (e.g. PreloadBatch where the playing item may not be a JIT-managed handle).
    if (playingItem == pfc::infinite_size) {
        size_t playingPlaylist = pfc::infinite_size;
        if (!plm->get_playing_item_location(&playingPlaylist, &playingItem) ||
            playingPlaylist != shadowIdx) {
            return;
        }
    }
    
    // Remove all items before the currently playing one so current stays at index 0.
    if (playingItem != pfc::infinite_size && playingItem > 0) {
        // Create bit array for items to remove (indices 0 to playingItem-1)
        pfc::bit_array_range mask(0, playingItem);
        plm->playlist_remove_items(shadowIdx, mask);
        
        FB2K_console_print("[JIT Queue] Cleaned up ", playingItem, " played tracks (current pinned to index 0)");
    }
}

void QueueManager::RemoveBufferedNextFromShadow(size_t playlistIndex) {
    // Locate the current track by its JIT URL and drop everything after it, so a
    // re-enqueued next replaces (not appends after) any previously buffered next.
    // Must run on the main thread (caller guarantees this — invoked from the add
    // lambdas / HandleResolvedItems right before insert).
    std::string currentUrl;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        currentUrl = m_currentJitUrl;
    }
    
    // No current URL window (e.g. PreloadBatch mode) → keep legacy append behavior.
    if (currentUrl.empty()) {
        return;
    }
    
    auto plm = playlist_manager::get();
    if (playlistIndex >= plm->get_playlist_count()) {
        return;
    }
    
    size_t count = plm->playlist_get_item_count(playlistIndex);
    size_t currentItem = pfc::infinite_size;
    for (size_t i = 0; i < count; ++i) {
        metadb_handle_ptr h;
        if (plm->playlist_get_item_handle(h, playlistIndex, i) && h.is_valid() &&
            strcmp(h->get_path(), currentUrl.c_str()) == 0) {
            currentItem = i;
            break;
        }
    }
    
    // Current not found in shadow playlist → don't risk removing the wrong items.
    if (currentItem == pfc::infinite_size) {
        return;
    }
    
    size_t firstAfter = currentItem + 1;
    if (firstAfter < count) {
        pfc::bit_array_range mask(firstAfter, count - firstAfter);
        plm->playlist_remove_items(playlistIndex, mask);
        FB2K_console_print("[JIT Queue] Replaced buffered next: removed ", count - firstAfter,
                           " stale track(s) after current before enqueue");
    }
}

void QueueManager::RequestNextTrack() {
    std::string currentId;
    std::string nextId;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        currentId = m_currentTrackId;
        nextId = m_nextTrackId;
    }
    
    // Don't request if we already have a next track buffered
    if (!nextId.empty()) {
        FB2K_console_print("[JIT Queue] Skip request - next track already buffered: ", nextId.c_str());
        return;
    }
    
    FB2K_console_print("[JIT Queue] Emitting jitQueue:needNext for current: ", currentId.c_str());
    
    // CAS guard — only transition Active→WaitingNext, prevent duplicate needNext
    State expected = State::Active;
    if (!m_state.compare_exchange_strong(expected, State::WaitingNext)) {
        FB2K_console_print("[JIT Queue] Skip needNext - state already ", StateToString(expected));
        return;
    }
    
    BridgeCore::GetInstance().EmitEvent("jitQueue:needNext", {
        {"currentTrackId", currentId},
        {"reason", "trackChange"}
    });
}

void QueueManager::NotifyListExhausted() {
    m_state = State::Exhausted;
    FB2K_console_print("[JIT Queue] List exhausted - notifying frontend");
    
    BridgeCore::GetInstance().EmitEvent("jitQueue:listExhausted", {
        {"lastTrackId", GetCurrentTrackId()}
    });
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
                BridgeCore::GetInstance().EmitEvent("jitQueue:error", {
                    {"trackId", trackIdCopy}, {"error", "Shadow playlist no longer exists"}, {"url", urlCopy}
                });
                return;
            }
            
            if (clearFirst) {
                plm->playlist_clear(playlistIndex);
            }

            // Fast path: 流媒体直链 / 本地文件路径直接 metadb::handle_create 同步生成 handle,
            // 完全绕开 process_locations_async 的 fb2k 进度对话框 (零弹窗、瞬间返回)。
            // Slow path: .pls / .m3u / .cue 等 wrapper 必须走 process_locations_async 展开。
            if (!LooksLikePlaylistWrapper(urlCopy)) {
                auto mdb = metadb::get();
                metadb_handle_ptr handle = mdb->handle_create(urlCopy.c_str(), 0);
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
            auto notify = process_locations_notify::create(
                [this, trackIdCopy, urlCopy, playlistIndex, startPlayback, replaceBufferedNext](metadb_handle_list_cref items) {
                    HandleResolvedItems(items, trackIdCopy, urlCopy, playlistIndex, startPlayback, replaceBufferedNext);
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
            BridgeCore::GetInstance().EmitEvent("jitQueue:error", {
                {"trackId", trackIdCopy}, {"error", e.what()}, {"url", urlCopy}
            });
        } catch (...) {
            FB2K_console_print("[JIT Queue] Unknown exception in AddUrlToPlaylistAsync");
            if (startPlayback) m_pendingOperation = false;
            BridgeCore::GetInstance().EmitEvent("jitQueue:error", {
                {"trackId", trackIdCopy}, {"error", "Unknown error"}, {"url", urlCopy}
            });
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
                BridgeCore::GetInstance().EmitEvent("jitQueue:error", {
                    {"trackId", trackIdCopy}, {"error", "Shadow playlist no longer exists"}, {"path", pathCopy}
                });
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
                BridgeCore::GetInstance().EmitEvent("jitQueue:error", {
                    {"trackId", trackIdCopy}, {"error", "Failed to resolve local file path"}, {"path", pathCopy}
                });
                if (startPlayback) m_pendingOperation = false;
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
            BridgeCore::GetInstance().EmitEvent("jitQueue:error", {
                {"trackId", trackIdCopy}, {"error", e.what()}, {"path", pathCopy}
            });
        } catch (...) {
            FB2K_console_print("[JIT Queue] Unknown exception in AddLocalFileToPlaylistAsync");
            if (startPlayback) m_pendingOperation = false;
            BridgeCore::GetInstance().EmitEvent("jitQueue:error", {
                {"trackId", trackIdCopy}, {"error", "Unknown error"}, {"path", pathCopy}
            });
        }
    });
}
