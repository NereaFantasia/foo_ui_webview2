/**
 * QueueShadowPlaylist.cpp - JIT 队列的影子播放列表
 *
 * 影子列表的锁（ShadowPlaylistLock）与锁回调、查找或创建影子列表、归属表与列表的比对、
 * 删除已播的行、移除过期的缓冲下一首，以及在影子列表的指定行上启动播放（保留原生播放队列）。
 */

#include "pch.h"
#include "core/QueueManager.h"
#include "core/QueueManagerInternal.h"
#include "callbacks/QueueCallback.h"

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

    bool query_playlist_rename(const char*, t_size) override { return false; }
    bool query_playlist_remove() override { return false; }

    // Fall through to default action (start playback)
    bool execute_default_action(t_size) override { return false; }

    void on_playlist_index_change(t_size p_new_index) override {
        m_owner.OnShadowPlaylistIndexChanged(p_new_index);
    }

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
    SessionPlaybackExternal();
    m_shadowPlaylistIndex = pfc::infinite_size;
    m_state = State::Idle;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_rows.Reset();
        m_generations.EndSession();
    }
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

void QueueManager::ActivatePlaybackOnShadow(size_t playlistIndex, size_t row) {
    auto plm = playlist_manager::get();

    plm->set_active_playlist(playlistIndex);
    plm->set_playing_playlist(playlistIndex);

    // Play this row itself: start(track_command_play) plays the head of the native playback
    // queue first and consumes it. Activating a row empties the whole queue instead, as a native
    // double-click does, so the queue is put back as it was and still plays first once this
    // track ends.
    pfc::list_t<t_playback_queue_item> queued;
    plm->queue_get_contents(queued);
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_protocol && playlistIndex == m_shadowPlaylistIndex.load() && !m_rows.IsStale() && row < m_rows.Size()) {
            m_protocol->PlaybackSubmitted(m_rows.At(row).entry);
            MarkNativeSubmitted(m_rows.At(row).entry);
        }
    }
    if (queued.get_count() == 0) {
        plm->playlist_execute_default_action(playlistIndex, row);
    } else {
        QueueBroadcastSuppressor suppressor("unknown");
        plm->playlist_execute_default_action(playlistIndex, row);
        if (plm->queue_get_count() != queued.get_count()) {
            plm->queue_flush();
            // Same rule as queue.setContents: an item keeps its playlist coordinates when it had
            // them, a handle-only item goes back as a handle
            for (size_t i = 0; i < queued.get_count(); ++i) {
                const t_playback_queue_item& item = queued[i];
                if (item.m_playlist != pfc::infinite_size && item.m_item != pfc::infinite_size) {
                    plm->queue_add_item_playlist(item.m_playlist, item.m_item);
                } else {
                    plm->queue_add_item(item.m_handle);
                }
            }
        }
    }

    // A stop or clear meanwhile keeps the queue idle.
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
    
    for (size_t i = 0; i < count; i++) {
        pfc::string8 name;
        plm->playlist_get_name(i, name);
        if (strcmp(name.c_str(), SHADOW_PLAYLIST_NAME) == 0) {
            // Ensure lock is installed (may be missing if found from previous session)
            InstallPlaylistLock(i);
            return i;
        }
    }
    
    size_t newIndex = plm->create_playlist(SHADOW_PLAYLIST_NAME, pfc::infinite_size, pfc::infinite_size);
    
    InstallPlaylistLock(newIndex);
    
    FB2K_console_print("[JIT Queue] Created shadow playlist: ", SHADOW_PLAYLIST_NAME);
    
    return newIndex;
}

std::vector<jit_shadow::HandleKey> QueueManager::ReadRowHandles(size_t playlistIndex) {
    auto plm = playlist_manager::get();
    if (playlistIndex >= plm->get_playlist_count()) {
        return {};
    }
    metadb_handle_list items;
    plm->playlist_get_all_items(playlistIndex, items);
    return jit_queue_detail::HandleKeys(items);
}

bool QueueManager::VerifyRows(size_t playlistIndex) {
    // Read the playlist before taking the lock: SDK calls may call back into QueueManager.
    const std::vector<jit_shadow::HandleKey> actual = ReadRowHandles(playlistIndex);
    std::lock_guard<std::mutex> lock(m_mutex);
    const bool wasStale = m_rows.IsStale();
    const bool usable = m_rows.Verify(actual);
    if (!usable && !wasStale) {
        FB2K_console_print("[JIT Queue] Shadow playlist was edited externally; its rows are no longer attributed");
    }
    return usable;
}

void QueueManager::RemovePlayedRows(size_t playlistIndex, size_t playingRow) {
    if (playingRow == 0 || playingRow == pfc::infinite_size) {
        return;
    }
    // Rows that cannot be attributed are never removed
    if (!VerifyRows(playlistIndex)) {
        return;
    }

    auto plm = playlist_manager::get();
    plm->playlist_remove_items(playlistIndex, pfc::bit_array_range(0, playingRow));
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_rows.Remove(0, playingRow);
    }
    FB2K_console_print("[JIT Queue] Cleaned up ", playingRow, " played tracks (current pinned to index 0)");
}

void QueueManager::RemoveBufferedNextFromShadow(size_t playlistIndex) {
    // Main thread only.
    if (!VerifyRows(playlistIndex)) {
        return;
    }

    size_t lastCurrentRow;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        lastCurrentRow = m_rows.LastRowOf(m_currentEntry);
    }
    // Without the current entry's rows (a batch, or played away) nothing is removed: the rows to
    // drop cannot be told apart.
    if (lastCurrentRow == jit_shadow::kNoRow) {
        return;
    }

    auto plm = playlist_manager::get();
    size_t count = plm->playlist_get_item_count(playlistIndex);
    size_t firstAfter = lastCurrentRow + 1;
    if (firstAfter < count) {
        plm->playlist_remove_items(playlistIndex, pfc::bit_array_range(firstAfter, count - firstAfter));
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_rows.Remove(firstAfter, count - firstAfter);
        }
        FB2K_console_print("[JIT Queue] Replaced buffered next: removed ", count - firstAfter,
                           " stale track(s) after current before enqueue");
    }
}
