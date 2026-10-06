#include "pch.h"
#include "callbacks/PlaylistCallback.h"
#include "api/EventEmit.h"
#include "api/generated/PlaybackSchema.h"
#include "api/generated/PlaylistSchema.h"
#include "core/WebViewContext.h"
#include "api/PlaylistApi.h"
#include "api/adapters/Fb2kPlaylistService.h"
#include "core/QueueManager.h"
#include "domain/PathSecurity.h"
#include "utils/GuidUtils.h"
#include "utils/StringUtils.h"
#include <vector>

// ============================================
// PlaylistCallback Implementation
// ============================================
// Handles foobar2000 playlist events and sends them to JavaScript via Bridge

// Helper — check if a playlist index is the JIT Queue shadow playlist
namespace pl = api::playlist;

static std::int64_t IndexOrNone(size_t index) {
    return index == pfc::infinite_size ? -1 : static_cast<std::int64_t>(index);
}

static bool IsShadowPlaylist(size_t p_playlist) {
    size_t shadowIdx = QueueManager::GetInstance().GetShadowPlaylistIndex();
    return shadowIdx != pfc::infinite_size && p_playlist == shadowIdx;
}

// 载荷里的 GUID：回调给的序号在回调期间指向这张列表，页面按 GUID 认列表就不必再拿序号反查
// getAll（反查与下一次增删之间有竞态）。SDK 没写越界序号时 playlist_get_guid 返回什么，不问它，
// 回空串，与 PlaylistGuidOf 的写法一致。
static std::string GuidOfPlaylist(size_t index) {
    auto plm = playlist_manager_v5::get();
    if (index >= plm->get_playlist_count()) return {};
    return GuidUtils::GuidToString(plm->playlist_get_guid(index));
}

// Invalidate the caches that derive from playlist membership.
//
// Must run before any IsShadowPlaylist early-return: shadow playlist entries are
// real playlist_manager items, so PathSecurity indexes them and treats them as
// trusted media context. Skipping invalidation for the shadow playlist would
// leave the membership index permanently stale.
//
// Only for events that change which tracks a playlist holds. Pure reordering
// keeps the membership set intact and must not trigger an index rebuild.
static void InvalidatePlaylistMembershipCaches() {
    Fb2kPlaylistService::InvalidatePlaylistCache();

    // Touching the singleton before services are up would construct it with an
    // empty whitelist (its core_api probes swallow failures and return ""), and
    // a function-local static caches that degraded state for the whole session.
    // Skipping is safe: an unconstructed index is already invalid, so the first
    // query after startup builds it from live playlist state.
    if (!core_api::are_services_available()) {
        return;
    }
    PathSecurity::Instance().InvalidatePlaylistIndex();
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
            InvalidatePlaylistMembershipCaches();
            if (IsShadowPlaylist(p_playlist)) return;  // suppress JIT internal events
            pl::ItemsAddedPayload payload;
            payload.playlist = static_cast<std::int64_t>(p_playlist);
            payload.playlistGuid = GuidOfPlaylist(p_playlist);
            payload.start = static_cast<std::int64_t>(p_start);
            payload.count = static_cast<std::int64_t>(p_data.get_count());
            api::emit::Broadcast<pl::events::ItemsAdded>(payload);
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
            InvalidatePlaylistMembershipCaches();
            if (IsShadowPlaylist(p_playlist)) return;  // suppress JIT internal events
            pl::ItemsRemovedPayload payload;
            payload.playlist = static_cast<std::int64_t>(p_playlist);
            payload.playlistGuid = GuidOfPlaylist(p_playlist);
            payload.oldCount = static_cast<std::int64_t>(p_old_count);
            payload.newCount = static_cast<std::int64_t>(p_new_count);
            api::emit::Broadcast<pl::events::ItemsRemoved>(payload);
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
            pl::ItemsReorderedPayload payload;
            payload.playlist = static_cast<std::int64_t>(p_playlist);
            payload.playlistGuid = GuidOfPlaylist(p_playlist);
            payload.count = static_cast<std::int64_t>(p_count);
            api::emit::Broadcast<pl::events::ItemsReordered>(payload);
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
            pl::SelectionChangedPayload payload;
            payload.playlist = static_cast<std::int64_t>(p_playlist);
            payload.playlistGuid = GuidOfPlaylist(p_playlist);
            api::emit::Broadcast<pl::events::SelectionChanged>(payload);
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
            pl::FocusChangedPayload payload;
            payload.playlist = static_cast<std::int64_t>(p_playlist);
            payload.playlistGuid = GuidOfPlaylist(p_playlist);
            payload.from = IndexOrNone(p_from);
            payload.to = IndexOrNone(p_to);
            api::emit::Broadcast<pl::events::FocusChanged>(payload);
        } catch (...) {}
    }
    
    // Called when items are replaced (e.g., metadata update)
    void on_items_replaced(
        size_t p_playlist,
        const bit_array& p_mask,
        const pfc::list_base_const_t<t_on_items_replaced_entry>& p_data
    ) override {
        try {
            // Replacement swaps in different tracks, so paths change even though
            // the item count does not.
            InvalidatePlaylistMembershipCaches();
            if (IsShadowPlaylist(p_playlist)) return;  // suppress JIT internal events
            pl::ItemsReplacedPayload payload;
            payload.playlist = static_cast<std::int64_t>(p_playlist);
            payload.playlistGuid = GuidOfPlaylist(p_playlist);
            payload.count = static_cast<std::int64_t>(p_data.get_count());
            api::emit::Broadcast<pl::events::ItemsReplaced>(payload);
        } catch (...) {}
    }
    
    // Called when a playlist is created
    void on_playlist_created(
        size_t p_index,
        const char* p_name,
        size_t p_name_len
    ) override {
        try {
            // p_name_len may be a "length unknown" sentinel; pfc clamps it to the
            // real NUL-terminated length. Constructing std::string directly from
            // the raw pair would copy the sentinel count verbatim.
            pfc::string8 name;
            name.set_string(p_name, p_name_len);
            Fb2kPlaylistService::InvalidatePlaylistCache();
            pl::CreatedPayload payload;
            payload.index = static_cast<std::int64_t>(p_index);
            payload.guid = GuidOfPlaylist(p_index);
            // 名字由调用 playlist_manager 的组件给出，按字节长度截断时可能切断多字节序列；
            // 非法 UTF-8 会让 EmitEvent 的 dump 抛异常、事件发不出去。
            payload.name = StringUtils::SafeUtf8(name.get_ptr());
            api::emit::Broadcast<pl::events::Created>(payload);
        } catch (...) {}
    }
    
    // 移除之前：被移除的列表还在，这时记下它们的 GUID，留给紧接着的 on_playlists_removed。
    // 移除之后它们已不在，按旧序号去问只会问到别的列表。
    void on_playlists_removing(
        const bit_array& p_mask,
        size_t p_old_count,
        size_t /*p_new_count*/
    ) override {
        try {
            m_removing.clear();
            m_removingOldCount = p_old_count;
            for (size_t i = 0; i < p_old_count; i++) {
                if (p_mask.get(i)) m_removing.push_back(GuidOfPlaylist(i));
            }
        } catch (...) {
            m_removing.clear();
            m_removingOldCount = SIZE_MAX;
        }
    }

    // Called when playlists are removed
    void on_playlists_removed(
        const bit_array& p_mask,
        size_t p_old_count,
        size_t p_new_count
    ) override {
        try {
            InvalidatePlaylistMembershipCaches();
            pl::RemovedPayload payload;
            payload.oldCount = static_cast<std::int64_t>(p_old_count);
            payload.newCount = static_cast<std::int64_t>(p_new_count);
            for (size_t i = 0; i < p_old_count; i++) {
                if (p_mask.get(i)) payload.indices.push_back(static_cast<std::int64_t>(i));
            }
            // 预告与这次移除对得上才用记下的 GUID；对不上时宁可不给，也不给错的。
            if (m_removingOldCount == p_old_count && m_removing.size() == payload.indices.size()) {
                payload.guids = std::move(m_removing);
            } else {
                console::print("[Playlist] playlists removed without a matching removing notice; "
                               "playlist:removed carries no GUIDs");
            }
            m_removing.clear();
            m_removingOldCount = SIZE_MAX;
            api::emit::Broadcast<pl::events::Removed>(payload);
        } catch (...) {}
    }
    
    // Called when playlists are reordered
    void on_playlists_reorder(
        const size_t* /*p_order*/,
        size_t p_count
    ) override {
        try {
            Fb2kPlaylistService::InvalidatePlaylistCache();
            pl::ReorderedPayload payload;
            payload.count = static_cast<std::int64_t>(p_count);
            // 重排之后逐位读 GUID：页面拿它对照自己记的顺序，就知道谁挪到了哪里。
            payload.guids.reserve(p_count);
            for (size_t i = 0; i < p_count; i++) payload.guids.push_back(GuidOfPlaylist(i));
            api::emit::Broadcast<pl::events::Reordered>(payload);
        } catch (...) {}
    }
    
    // Called when active playlist changes
    void on_playlist_activate(
        size_t p_old,
        size_t p_new
    ) override {
        try {
            Fb2kPlaylistService::InvalidatePlaylistCache();
            pl::ActivatedPayload payload;
            payload.oldIndex = IndexOrNone(p_old);
            payload.newIndex = IndexOrNone(p_new);
            // 旧的活动列表可能正是刚删掉的那张，它的序号现在指向别的列表，所以只给新列表的 GUID。
            if (p_new != pfc::infinite_size) {
                if (std::string guid = GuidOfPlaylist(p_new); !guid.empty()) payload.newGuid = std::move(guid);
            }
            api::emit::Broadcast<pl::events::Activated>(payload);
        } catch (...) {}
    }
    
    // Called when a playlist is renamed
    void on_playlist_renamed(
        size_t p_index,
        const char* p_new_name,
        size_t p_new_name_len
    ) override {
        try {
            // Same sentinel-length caveat as on_playlist_created.
            pfc::string8 newName;
            newName.set_string(p_new_name, p_new_name_len);
            Fb2kPlaylistService::InvalidatePlaylistCache();
            pl::RenamedPayload payload;
            payload.index = static_cast<std::int64_t>(p_index);
            payload.guid = GuidOfPlaylist(p_index);
            // 同 on_playlist_created：名字先过 SafeUtf8。
            payload.name = StringUtils::SafeUtf8(newName.get_ptr());
            api::emit::Broadcast<pl::events::Renamed>(payload);
        } catch (...) {}
    }
    
    // Called when playlist lock status changes
    void on_playlist_locked(
        size_t p_playlist,
        bool p_locked
    ) override {
        try {
            Fb2kPlaylistService::InvalidatePlaylistCache();
            pl::LockChangedPayload payload;
            payload.playlist = static_cast<std::int64_t>(p_playlist);
            payload.playlistGuid = GuidOfPlaylist(p_playlist);
            payload.locked = p_locked;
            api::emit::Broadcast<pl::events::LockChanged>(payload);
        } catch (...) {}
    }
    
    // Called when default format changes
    void on_default_format_changed() override {
        try {
            api::emit::Broadcast<pl::events::DefaultFormatChanged>({});
        } catch (...) {}
    }
    
    // Called when playback order changes
    void on_playback_order_changed(size_t p_new_index) override {
        try {
            api::playback::OrderChangedPayload payload;
            payload.orderIndex = static_cast<std::int64_t>(p_new_index);
            payload.order = payload.orderIndex;  // alias: match playback.getPlaybackOrder response
            api::emit::Broadcast<api::playback::events::OrderChanged>(payload);
        } catch (...) {}
    }
    
    // Stub implementations for callbacks we don't need to handle
    void on_items_removing(size_t p_playlist, const bit_array& p_mask, size_t p_old_count, size_t p_new_count) override { /* SDK stub — not needed */ }
    void on_items_modified(size_t p_playlist, const bit_array& p_mask) override { /* SDK stub — not needed */ }
    void on_items_modified_fromplayback(size_t p_playlist, const bit_array& p_mask, play_control::t_display_level p_level) override { /* SDK stub — not needed */ }
    void on_item_ensure_visible(size_t p_playlist, size_t p_idx) override { /* SDK stub — not needed */ }

private:
    // on_playlists_removing 记下的被移除列表的 GUID（按序号升序）与当时的列表个数；
    // 回调都在主线程上，一次移除的两个回调之间不会插进另一次。
    std::vector<std::string> m_removing;
    size_t m_removingOldCount = SIZE_MAX;
};

// Static factory for automatic registration
static service_factory_single_t<PlaylistCallbackImpl> g_playlistCallback;

void InitPlaylistCallbacks() {
    // The callback is automatically registered by the static factory
    console::print("[WebView2 UI] Playlist callbacks initialized");
}

