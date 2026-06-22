#include "pch.h"
#include "callbacks/PlaylistCallback.h"
#include "core/WebViewContext.h"
#include "api/PlaylistApi.h"
#include "interfaces/Fb2kPlaylistService.h"
#include "core/QueueManager.h"

// ============================================
// PlaylistCallback Implementation
// ============================================
// Handles foobar2000 playlist events and sends them to JavaScript via Bridge

// Helper — check if a playlist index is the JIT Queue shadow playlist
static bool IsShadowPlaylist(size_t p_playlist) {
    size_t shadowIdx = QueueManager::GetInstance().GetShadowPlaylistIndex();
    return shadowIdx != pfc::infinite_size && p_playlist == shadowIdx;
}

// Use playlist_callback_static for static registration with service factory
class PlaylistCallbackImpl : public playlist_callback_static {
public:
    // Return flags for all events we want to receive
    unsigned get_flags() override {
        return flag_all;
    }
    
    // Called when items are added to a playlist
    void on_items_added(
        size_t p_playlist,
        size_t p_start,
        metadb_handle_list_cref p_data,
        const bit_array& p_selection
    ) override {
        try {
            Fb2kPlaylistService::InvalidatePlaylistCache();
            if (IsShadowPlaylist(p_playlist)) return;  // suppress JIT internal events
            WebViewContext::GetInstance().BroadcastEvent("playlist:itemsAdded", {
                {"playlist", p_playlist},
                {"start", p_start},
                {"count", p_data.get_count()},
            });
        } catch (...) {}
    }
    
    // Called when items are removed from a playlist
    void on_items_removed(
        size_t p_playlist,
        const bit_array& p_mask,
        size_t p_old_count,
        size_t p_new_count
    ) override {
        try {
            Fb2kPlaylistService::InvalidatePlaylistCache();
            if (IsShadowPlaylist(p_playlist)) return;  // suppress JIT internal events
            WebViewContext::GetInstance().BroadcastEvent("playlist:itemsRemoved", {
                {"playlist", p_playlist},
                {"oldCount", p_old_count},
                {"newCount", p_new_count},
            });
        } catch (...) {}
    }
    
    // Called when items are reordered in a playlist
    void on_items_reordered(
        size_t p_playlist,
        const size_t* p_order,
        size_t p_count
    ) override {
        try {
            Fb2kPlaylistService::InvalidatePlaylistCache();
            if (IsShadowPlaylist(p_playlist)) return;  // suppress JIT internal events
            WebViewContext::GetInstance().BroadcastEvent("playlist:itemsReordered", {
                {"playlist", p_playlist},
                {"count", p_count},
            });
        } catch (...) {}
    }
    
    // Called when selection changes in a playlist
    void on_items_selection_change(
        size_t p_playlist,
        const bit_array& p_affected,
        const bit_array& p_state
    ) override {
        try {
            if (IsShadowPlaylist(p_playlist)) return;  // suppress JIT internal events
            WebViewContext::GetInstance().BroadcastEvent("playlist:selectionChanged", {
                {"playlist", p_playlist},
            });
        } catch (...) {}
    }
    
    // Called when item focus changes
    void on_item_focus_change(
        size_t p_playlist,
        size_t p_from,
        size_t p_to
    ) override {
        try {
            if (IsShadowPlaylist(p_playlist)) return;  // suppress JIT internal events
            WebViewContext::GetInstance().BroadcastEvent("playlist:focusChanged", {
                {"playlist", p_playlist},
                {"from", p_from == pfc::infinite_size ? -1 : static_cast<int64_t>(p_from)},
                {"to", p_to == pfc::infinite_size ? -1 : static_cast<int64_t>(p_to)},
            });
        } catch (...) {}
    }
    
    // Called when items are replaced (e.g., metadata update)
    void on_items_replaced(
        size_t p_playlist,
        const bit_array& p_mask,
        const pfc::list_base_const_t<t_on_items_replaced_entry>& p_data
    ) override {
        try {
            if (IsShadowPlaylist(p_playlist)) return;  // suppress JIT internal events
            WebViewContext::GetInstance().BroadcastEvent("playlist:itemsReplaced", {
                {"playlist", p_playlist},
                {"count", p_data.get_count()},
            });
        } catch (...) {}
    }
    
    // Called when a playlist is created
    void on_playlist_created(
        size_t p_index,
        const char* p_name,
        size_t p_name_len
    ) override {
        try {
            Fb2kPlaylistService::InvalidatePlaylistCache();
            WebViewContext::GetInstance().BroadcastEvent("playlist:created", {
                {"index", p_index},
                {"name", std::string(p_name, p_name_len)},
            });
        } catch (...) {}
    }
    
    // Called when playlists are removed
    void on_playlists_removed(
        const bit_array& p_mask,
        size_t p_old_count,
        size_t p_new_count
    ) override {
        try {
            Fb2kPlaylistService::InvalidatePlaylistCache();
            WebViewContext::GetInstance().BroadcastEvent("playlist:removed", {
                {"oldCount", p_old_count},
                {"newCount", p_new_count},
            });
        } catch (...) {}
    }
    
    // Called when playlists are reordered
    void on_playlists_reorder(
        const size_t* p_order,
        size_t p_count
    ) override {
        try {
            Fb2kPlaylistService::InvalidatePlaylistCache();
            WebViewContext::GetInstance().BroadcastEvent("playlist:reordered", {
                {"count", p_count},
            });
        } catch (...) {}
    }
    
    // Called when active playlist changes
    void on_playlist_activate(
        size_t p_old,
        size_t p_new
    ) override {
        try {
            Fb2kPlaylistService::InvalidatePlaylistCache();
            WebViewContext::GetInstance().BroadcastEvent("playlist:activated", {
                {"oldIndex", p_old == pfc::infinite_size ? -1 : static_cast<int64_t>(p_old)},
                {"newIndex", p_new == pfc::infinite_size ? -1 : static_cast<int64_t>(p_new)},
            });
        } catch (...) {}
    }
    
    // Called when a playlist is renamed
    void on_playlist_renamed(
        size_t p_index,
        const char* p_new_name,
        size_t p_new_name_len
    ) override {
        try {
            Fb2kPlaylistService::InvalidatePlaylistCache();
            WebViewContext::GetInstance().BroadcastEvent("playlist:renamed", {
                {"index", p_index},
                {"name", std::string(p_new_name, p_new_name_len)},
            });
        } catch (...) {}
    }
    
    // Called when playlist lock status changes
    void on_playlist_locked(
        size_t p_playlist,
        bool p_locked
    ) override {
        try {
            Fb2kPlaylistService::InvalidatePlaylistCache();
            WebViewContext::GetInstance().BroadcastEvent("playlist:lockChanged", {
                {"playlist", p_playlist},
                {"locked", p_locked},
            });
        } catch (...) {}
    }
    
    // Called when default format changes
    void on_default_format_changed() override {
        try {
            WebViewContext::GetInstance().BroadcastEvent("playlist:defaultFormatChanged", {});
        } catch (...) {}
    }
    
    // Called when playback order changes
    void on_playback_order_changed(size_t p_new_index) override {
        try {
            WebViewContext::GetInstance().BroadcastEvent("playback:orderChanged", {
                {"orderIndex", p_new_index},
                {"order", p_new_index},  // alias: match playback.getPlaybackOrder response
            });
        } catch (...) {}
    }
    
    // Stub implementations for callbacks we don't need to handle
    void on_items_removing(size_t p_playlist, const bit_array& p_mask, size_t p_old_count, size_t p_new_count) override { /* SDK stub — not needed */ }
    void on_items_modified(size_t p_playlist, const bit_array& p_mask) override { /* SDK stub — not needed */ }
    void on_items_modified_fromplayback(size_t p_playlist, const bit_array& p_mask, play_control::t_display_level p_level) override { /* SDK stub — not needed */ }
    void on_item_ensure_visible(size_t p_playlist, size_t p_idx) override { /* SDK stub — not needed */ }
    void on_playlists_removing(const bit_array& p_mask, size_t p_old_count, size_t p_new_count) override { /* SDK stub — not needed */ }
};

// Static factory for automatic registration
static service_factory_single_t<PlaylistCallbackImpl> g_playlistCallback;

void InitPlaylistCallbacks() {
    // The callback is automatically registered by the static factory
    console::print("[WebView2 UI] Playlist callbacks initialized");
}

