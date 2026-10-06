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
#include "core/QueueManagerInternal.h"
#include "api/EventEmit.h"
#include "api/generated/JitQueueSchema.h"

namespace {

namespace jitq = api::jitQueue;

using jit_queue_detail::Announce;

}  // namespace

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
// Core API Methods
//==============================================================================

bool QueueManager::PlayNow(const std::string& trackId, const std::string& title, const std::string& url,
                           const JitSessionCaller& caller) {
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
        // A new session: from here on its events go to this page, a page that had the previous
        // session hears nothing more from it (there is no closing event for a replaced session).
        m_sessionCaller = caller;
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
// State Queries
//==============================================================================

JitSessionCaller QueueManager::GetSessionCaller() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_sessionCaller;
}

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
    jitq::TrackChangedPayload changed;
    changed.trackId = currentId;
    changed.title = currentTitle;
    Announce<jitq::events::TrackChanged>(GetSessionCaller(), changed);
    
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

    // 新曲目开始后允许重新请求下一首。页面重载可能丢失上一轮 needNext 的应答；
    // 将仍在等待的状态恢复为 Active，避免旧等待状态阻止 RequestNextTrack。
    State unanswered = State::WaitingNext;
    if (m_state.compare_exchange_strong(unanswered, State::Active)) {
        FB2K_console_print("[JIT Queue] Previous needNext went unanswered; recovering to Active");
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
    
    jitq::NeedNextPayload need;
    need.currentTrackId = currentId;
    need.reason = "trackChange";
    Announce<jitq::events::NeedNext>(GetSessionCaller(), need);
}

void QueueManager::NotifyListExhausted() {
    m_state = State::Exhausted;
    FB2K_console_print("[JIT Queue] List exhausted - notifying frontend");
    
    jitq::ListExhaustedPayload exhausted;
    exhausted.lastTrackId = GetCurrentTrackId();
    Announce<jitq::events::ListExhausted>(GetSessionCaller(), exhausted);
}
