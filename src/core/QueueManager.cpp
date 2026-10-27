/**
 * QueueManager.cpp - legacy jitQueue commands, playback callbacks and restoring the user's context
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
    if (m_initialized) return;
    m_initialized = true;
    
    // The shadow playlist is created on first use, not at startup.
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
    m_rows.Reset();
    m_currentEntry = 0;
    m_nextEntry = 0;
    m_generations.EndSession();

    m_protocol.reset();
    m_initialized = false;

    FB2K_console_print("[JIT Queue] Shutdown");
}

//==============================================================================
// Core API Methods
//==============================================================================

void QueueManager::PlayNow(const std::string& trackId, const std::string& title, const std::string& url,
                           const JitSessionCaller& caller) {
    LegacyTakeover();
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_sessionCaller = caller;
    }
    StartCurrentEntry(trackId, title, url);
}

void QueueManager::StartCurrentEntry(const std::string& trackId, const std::string& title, const std::string& url) {
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
    
    jit_shadow::EntrySerial entry;
    jit_shadow::Ticket ticket;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        // 新的当前条目取代全部旧添加；事件接收方由调用入口保存。
        m_generations.EndSession();
        ticket = m_generations.TakeCurrent();
        m_currentTrackId = trackId;
        m_currentTitle = title;
        m_nextTrackId.clear();
        m_nextTitle.clear();
        m_needNextRequested = false;
        entry = ++m_lastEntry;
        m_currentEntry = entry;
        m_nextEntry = 0;
        BindSessionEntry(entry, jit_shadow::Slot::Current);
    }
    
    m_state = State::Active;
    
    if (isLocal) {
        AddLocalFileToPlaylistAsync(url, trackId, title, entry, ticket);
    } else {
        AddUrlToPlaylistAsync(url, trackId, title, entry, ticket);
    }
}

bool QueueManager::EnqueueNext(const std::string& trackId, const std::string& title, const std::string& url) {
    State currentState = m_state.load();
    if (currentState != State::Active && currentState != State::WaitingNext) {
        FB2K_console_print("[JIT Queue] EnqueueNext rejected - state=", StateToString(currentState));
        return false;
    }

    LegacyTakeover();
    BufferNextEntry(trackId, title, url);
    return true;
}

void QueueManager::BufferNextEntry(const std::string& trackId, const std::string& title, const std::string& url) {
    
    bool isLocal = IsLocalFilePath(url);
    FB2K_console_print("[JIT Queue] EnqueueNext: ", trackId.c_str(), " - ", title.c_str());
    FB2K_console_print("[JIT Queue] URL: ", url.c_str(), " (", isLocal ? "LOCAL" : "STREAM", ")");
    
    jit_shadow::EntrySerial entry;
    jit_shadow::Ticket ticket;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        // Drops the add of a previously requested next, never the current track's
        ticket = m_generations.TakeNext();
        if (!m_nextTrackId.empty() && m_nextTrackId != trackId) {
            FB2K_console_print("[JIT Queue] WARNING: Overwriting next track '", m_nextTrackId.c_str(), 
                               "' with '", trackId.c_str(), "'");
        }
        m_nextTrackId = trackId;
        m_nextTitle = title;
        m_needNextRequested = false;
        entry = ++m_lastEntry;
        m_nextEntry = entry;
        BindSessionEntry(entry, jit_shadow::Slot::Next);
    }
    
    // Transition: WaitingNext → Active (response received)
    m_state = State::Active;
    
    if (isLocal) {
        AddLocalFileToPlaylistAsync(url, trackId, title, entry, ticket);
    } else {
        AddUrlToPlaylistAsync(url, trackId, title, entry, ticket);
    }
    
}

bool QueueManager::Skip() {
    if (m_state.load() == State::Idle) {
        return false;
    }

    LegacyTakeover();
    return SkipInternal();
}

bool QueueManager::SkipInternal() {

    // Play the shadow row after the playing one: playback_control::next() would play the head
    // of the native playback queue first. Falls back to next() when that row is not known.
    auto plm = playlist_manager::get();
    const size_t shadowIdx = m_shadowPlaylistIndex.load();
    size_t playingPlaylist = pfc::infinite_size;
    size_t playingRow = pfc::infinite_size;
    if (shadowIdx != pfc::infinite_size && plm->get_playing_item_location(&playingPlaylist, &playingRow) &&
        playingPlaylist == shadowIdx && playingRow + 1 < plm->playlist_get_item_count(shadowIdx)) {
        ActivatePlaybackOnShadow(shadowIdx, playingRow + 1);
    } else {
        playback_control::get()->next();
    }

    return true;
}

void QueueManager::Stop(bool clearBuffer) {
    LegacyTakeover();
    StopInternal(clearBuffer);
}

void QueueManager::StopInternal(bool clearBuffer) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_generations.EndSession();
    }
    auto pc = playback_control::get();
    pc->stop();
    
    if (clearBuffer) {
        // ClearInternal 会恢复上下文并将内核设为空闲。
        ClearInternal();
    } else {
        // 保留缓冲时由这里恢复上下文。
        RestoreUserContext();
        m_state = State::Idle;
    }
}

void QueueManager::Clear() {
    LegacyTakeover();
    ClearInternal();
}

void QueueManager::ClearInternal() {
    // Before taking the lock: it calls playlist_manager.
    RestoreUserContext();
    
    auto plm = playlist_manager::get();
    if (m_shadowPlaylistIndex != pfc::infinite_size &&
        m_shadowPlaylistIndex < plm->get_playlist_count()) {
        plm->playlist_clear(m_shadowPlaylistIndex);
        
        // Do not leave the shadow playlist active.
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
    
    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentTrackId.clear();
    m_nextTrackId.clear();
    m_currentTitle.clear();
    m_nextTitle.clear();
    m_rows.Reset();
    m_currentEntry = 0;
    m_nextEntry = 0;
    m_generations.EndSession();
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
    std::uint64_t trackSeq;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        trackSeq = m_generations.TrackStarted();
    }
    // Attribute after the callback returns: inside it get_playing_item_location() can still report
    // the previous row, and removing rows here leaves the location fb2k reports afterwards invalid.
    fb2k::inMainThread([this, trackSeq, track]() { AttributeNewTrack(trackSeq, track); });
}

void QueueManager::AttributeNewTrack(std::uint64_t trackSeq, const metadb_handle_ptr& track) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        // A later track has started, or a command replaced the session since this one started;
        // that later step or the new session decides
        if (!m_generations.IsLatestTrack(trackSeq)) {
            return;
        }
    }
    metadb_handle_ptr playing;
    if (!playback_control::get()->get_now_playing(playing) || playing != track) {
        return;
    }

    auto plm = playlist_manager::get();
    const size_t shadowIdx = m_shadowPlaylistIndex.load();
    size_t playingPlaylist = pfc::infinite_size;
    jit_shadow::PlayingLocation location;
    location.located = plm->get_playing_item_location(&playingPlaylist, &location.row);
    location.inShadow = location.located && shadowIdx != pfc::infinite_size && playingPlaylist == shadowIdx;
    if (location.inShadow) {
        VerifyRows(shadowIdx);
    }

    jit_shadow::Attribution attribution;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        attribution = jit_shadow::Attribute(m_rows, location, m_currentEntry, m_nextEntry,
                                            m_state.load() == State::Idle);
    }

    if (attribution == jit_shadow::Attribution::Release) {
        if (!location.located) {
            // The deferred step is meant to always find the playing row; seeing this while the
            // session plays its own rows means that assumption does not hold.
            FB2K_console_print("[JIT Queue] Attribution: playing location unavailable after track change");
        }
        ReleaseSession(!location.located   ? "playing location unavailable"
                       : !location.inShadow ? "playing from another playlist"
                                            : "playing row not owned by the session");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (attribution == jit_shadow::Attribution::Advance) {
            // 下一条已经起播，旧当前条目尚未完成的解析不能再插入或抢播。
            m_generations.EndSession();
            // A replay of the current entry or a batch row keeps both IDs.
            m_currentTrackId = m_nextTrackId;
            m_currentTitle = m_nextTitle;
            m_currentEntry = m_nextEntry;
            m_nextTrackId.clear();
            m_nextTitle.clear();
            m_nextEntry = 0;
            FB2K_console_print("[JIT Queue] Shifted: next -> current, new currentId: ", m_currentTrackId.c_str());
        }

        m_needNextRequested = false;
    }

    m_state = State::Active;

    RemovePlayedRows(shadowIdx, location.row);
    SessionTrackStarted(attribution);
    
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

void QueueManager::ReleaseSession(const char* reason) {
    if (m_state.load() == State::Idle) {
        return;
    }
    SessionPlaybackExternal();
    m_state = State::Idle;
    RestoreUserContext();
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        // The rows left in the shadow playlist become ownerless at the next comparison
        m_rows.Reset();
        m_generations.EndSession();
    }
    FB2K_console_print("[JIT Queue] Playback switched away from JIT queue: ", reason);
}

bool QueueManager::AddIsLive(const jit_shadow::Ticket& ticket, const std::string& trackId, const char* stage) const {
    bool live;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        live = m_generations.IsLive(ticket);
    }
    if (!live) {
        FB2K_console_print("[JIT Queue] Dropped superseded add of ", trackId.c_str(), " (", stage, ")");
    }
    return live;
}

void QueueManager::OnPlaybackStop(play_control::t_stop_reason reason) {
    if (SessionPlaybackStopped(reason)) return;
    FB2K_console_print("[JIT Queue] OnPlaybackStop called, reason: ", static_cast<int>(reason), ", state: ", static_cast<int>(m_state.load()));
    
    if (m_state.load() == State::Idle) {
        return;
    }
    
    if (reason == play_control::stop_reason_eof) {
        FB2K_console_print("[JIT Queue] Playback stopped - EOF");
        
        size_t bufferSize = GetBufferSize();
        FB2K_console_print("[JIT Queue] Buffer size: ", bufferSize);
        
        if (bufferSize > 0) {
            FB2K_console_print("[JIT Queue] Buffer has tracks, waiting for auto-play");
        } else {
            FB2K_console_print("[JIT Queue] Buffer empty, notifying frontend");
            NotifyListExhausted();
        }
    } else if (reason == play_control::stop_reason_starting_another) {
        FB2K_console_print("[JIT Queue] Stop reason: starting another track");
    } else if (reason == play_control::stop_reason_user) {
        FB2K_console_print("[JIT Queue] Playback stopped by user");
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_generations.EndSession();
        }
        m_state = State::Idle;
        RestoreUserContext();
    }
}

void QueueManager::OnPlaybackTime(double time, double duration) {
    SessionPlaybackTime(time, duration);
    if (HasProtocolSession()) return;
    if (m_state.load() == State::Idle) {
        return;
    }
    
    double remaining = duration - time;
    
    if (remaining <= PREFETCH_THRESHOLD_SECONDS) {
        bool shouldRequest = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            
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
    
    size_t shadowIdx = m_shadowPlaylistIndex.load();
    if (plm->get_active_playlist() == shadowIdx && savedPlaylist < plm->get_playlist_count()) {
        plm->set_active_playlist(savedPlaylist);
        FB2K_console_print("[JIT Queue] Restored active playlist to ", savedPlaylist);
    }
}

void QueueManager::RequestNextTrack() {
    if (HasProtocolSession()) return;
    std::string currentId;
    std::string nextId;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        currentId = m_currentTrackId;
        nextId = m_nextTrackId;
    }
    
    if (!nextId.empty()) {
        FB2K_console_print("[JIT Queue] Skip request - next track already buffered: ", nextId.c_str());
        return;
    }
    
    FB2K_console_print("[JIT Queue] Emitting jitQueue:needNext for current: ", currentId.c_str());
    
    // Only one needNext per wait.
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
    if (HasProtocolSession()) return;
    m_state = State::Exhausted;
    FB2K_console_print("[JIT Queue] List exhausted - notifying frontend");
    
    jitq::ListExhaustedPayload exhausted;
    exhausted.lastTrackId = GetCurrentTrackId();
    Announce<jitq::events::ListExhausted>(GetSessionCaller(), exhausted);
}
