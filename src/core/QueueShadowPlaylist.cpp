/**
 * QueueShadowPlaylist.cpp - JIT 队列的影子播放列表
 *
 * 影子列表的锁（ShadowPlaylistLock）与锁回调、查找或创建影子列表、判断是否正从影子列表播放、
 * 清理已播曲目、移除过期的缓冲下一首，以及在影子列表上启动播放。
 */

#include "pch.h"
#include "core/QueueManager.h"

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
