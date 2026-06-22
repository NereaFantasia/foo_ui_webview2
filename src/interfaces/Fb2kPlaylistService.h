#pragma once
#include "interfaces/IPlaylistService.h"
#include "api/PlaylistApi.h"   // for GetPlaylistInfo, GetPlaylistTrackInfo

// Production implementation of IPlaylistService.
// All methods delegate to playlist_manager::get() / autoplaylist_manager::get().
// Header-only — included only by PlaylistApi.cpp.
class Fb2kPlaylistService : public IPlaylistService {
public:

    // -- Playlist lifecycle ------------------------------------------

    size_t get_playlist_count() const override {
        return playlist_manager::get()->get_playlist_count();
    }

    size_t get_active_playlist() const override {
        return playlist_manager::get()->get_active_playlist();
    }

    void set_active_playlist(size_t index) override {
        playlist_manager::get()->set_active_playlist(index);
    }

    size_t get_playing_playlist() const override {
        return playlist_manager::get()->get_playing_playlist();
    }

    size_t create_playlist(const std::string& name, size_t position) override {
        return playlist_manager::get()->create_playlist(name.c_str(), pfc::infinite_size, position);
    }

    bool remove_playlist(size_t index) override {
        return playlist_manager::get()->remove_playlist(index);
    }

    bool playlist_rename(size_t index, const std::string& name) override {
        return playlist_manager::get()->playlist_rename(index, name.c_str(), pfc::infinite_size);
    }

    void playlist_clear(size_t index) override {
        playlist_manager::get()->playlist_clear(index);
    }

    void playlist_undo_backup(size_t index) override {
        playlist_manager::get()->playlist_undo_backup(index);
    }

    size_t playlist_get_item_count(size_t index) const override {
        return playlist_manager::get()->playlist_get_item_count(index);
    }

    bool playlist_get_name(size_t index, std::string& out) const override {
        pfc::string8 name;
        bool ok = playlist_manager::get()->playlist_get_name(index, name);
        if (ok) out = name.c_str();
        return ok;
    }

    // -- Lock queries ------------------------------------------------

    bool playlist_lock_is_present(size_t index) const override {
        return playlist_manager::get()->playlist_lock_is_present(index);
    }

    bool playlist_lock_query_name(size_t index, std::string& out) const override {
        pfc::string8 lockName;
        bool ok = playlist_manager::get()->playlist_lock_query_name(index, lockName);
        if (ok) out = lockName.c_str();
        return ok;
    }

    // -- Autoplaylist ------------------------------------------------

    bool autoplaylist_is_client_present(size_t index) const override {
        return autoplaylist_manager::get()->is_client_present(index);
    }

    // -- High-level aggregators --------------------------------------

    // -- Cache for get_all_playlists (主线程专用，无需同步) ----------

    static void InvalidatePlaylistCache() { s_cacheValid_ = false; }

    std::vector<PlaylistInfo> get_all_playlists() const override {
        if (s_cacheValid_) {
            return s_cachedPlaylists_;
        }

        auto plm = playlist_manager::get();
        auto apm = autoplaylist_manager::get();
        size_t count = plm->get_playlist_count();
        size_t activeIdx = plm->get_active_playlist();
        size_t playingIdx = plm->get_playing_playlist();

        s_cachedPlaylists_.clear();
        s_cachedPlaylists_.reserve(count);

        for (size_t i = 0; i < count; i++) {
            pfc::string8 name;
            plm->playlist_get_name(i, name);

            bool isLocked = plm->playlist_lock_is_present(i);
            bool isAuto = apm->is_client_present(i);
            if (!isAuto && isLocked) {
                pfc::string8 lockName;
                if (plm->playlist_lock_query_name(i, lockName)) {
                    std::string ln = lockName.c_str();
                    if (ln == "Autoplaylist" || ln.find("Auto") != std::string::npos) {
                        isAuto = true;
                    }
                }
            }

            s_cachedPlaylists_.push_back({
                i,
                std::string(name.c_str()),
                plm->playlist_get_item_count(i),
                (i == activeIdx),
                (i == playingIdx),
                isLocked,
                isAuto
            });
        }
        s_cacheValid_ = true;
        return s_cachedPlaylists_;
    }

private:
    static inline bool s_cacheValid_ = false;
    static inline std::vector<PlaylistInfo> s_cachedPlaylists_;

public:

    PlaylistDetail get_playlist_detail(size_t index, bool includeDuration) const override {
        auto plm = playlist_manager::get();
        if (index >= plm->get_playlist_count()) {
            return { false };
        }

        pfc::string8 name;
        plm->playlist_get_name(index, name);
        size_t trackCount = plm->playlist_get_item_count(index);

        PlaylistDetail d;
        d.found = true;
        d.index = index;
        d.name = name.c_str();
        d.trackCount = trackCount;
        d.isActive = (index == plm->get_active_playlist());
        d.isPlaying = (index == plm->get_playing_playlist());
        d.isLocked = plm->playlist_lock_is_present(index);
        d.duration = 0.0;

        if (includeDuration) {
            metadb_handle_list items;
            plm->playlist_get_all_items(index, items);
            double total = 0.0;
            for (size_t i = 0; i < items.get_count(); i++) {
                if (items[i].is_valid()) total += items[i]->get_length();
            }
            d.duration = total;
        }
        return d;
    }

    DuplicateResult duplicate_playlist(size_t sourceIndex, const std::string& newName) override {
        auto plm = playlist_manager::get();

        // Get all tracks from source
        metadb_handle_list items;
        plm->playlist_get_all_items(sourceIndex, items);

        // Create new playlist next to source
        size_t newIndex = plm->create_playlist(newName.c_str(), pfc::infinite_size, sourceIndex + 1);
        if (newIndex == pfc::infinite_size) {
            return { false, "Failed to create playlist", 0, sourceIndex, newName, 0 };
        }

        // Undo backup + insert tracks
        plm->playlist_undo_backup(newIndex);
        plm->playlist_insert_items(newIndex, 0, items, bit_array_false());

        return { true, "", newIndex, sourceIndex, newName, items.get_count() };
    }

    // -- Selection management -----------------------------------

    std::vector<size_t> get_selection_indices(size_t playlist) const override {
        auto plm = playlist_manager::get();
        size_t count = plm->playlist_get_item_count(playlist);
        pfc::bit_array_bittable selection(count);
        plm->playlist_get_selection_mask(playlist, selection);
        std::vector<size_t> result;
        for (size_t i = 0; i < count; i++) {
            if (selection.get(i)) result.push_back(i);
        }
        return result;
    }

    void set_selection(size_t playlist, const std::vector<size_t>& indices, bool clearOthers) override {
        auto plm = playlist_manager::get();
        size_t count = plm->playlist_get_item_count(playlist);
        pfc::bit_array_bittable mask(count);
        for (size_t i : indices) {
            if (i < count) mask.set(i, true);
        }
        if (clearOthers) {
            plm->playlist_set_selection(playlist, pfc::bit_array_true(), mask);
        } else {
            plm->playlist_set_selection(playlist, mask, pfc::bit_array_true());
        }
    }

    void select_all(size_t playlist) override {
        playlist_manager::get()->playlist_set_selection(playlist, pfc::bit_array_true(), pfc::bit_array_true());
    }

    void deselect_all(size_t playlist) override {
        playlist_manager::get()->playlist_set_selection(playlist, pfc::bit_array_true(), pfc::bit_array_false());
    }

    // -- Focus item ---------------------------------------------

    size_t get_focus_item(size_t playlist) const override {
        return playlist_manager::get()->playlist_get_focus_item(playlist);
    }

    void set_focus_item(size_t playlist, size_t trackIndex) override {
        playlist_manager::get()->playlist_set_focus_item(playlist, trackIndex);
    }

    // -- Track removal ------------------------------------------

    void remove_tracks(size_t playlist, const std::vector<size_t>& indices) override {
        auto plm = playlist_manager::get();
        size_t count = plm->playlist_get_item_count(playlist);
        pfc::bit_array_bittable mask(count);
        for (size_t i : indices) {
            if (i < count) mask.set(i, true);
        }
        plm->playlist_undo_backup(playlist);
        plm->playlist_remove_items(playlist, mask);
    }

    void remove_selection(size_t playlist) override {
        auto plm = playlist_manager::get();
        plm->playlist_undo_backup(playlist);
        plm->playlist_remove_selection(playlist, false);
    }

    // -- Track movement + playback ------------------------------

    void move_selection(size_t playlist, int delta) override {
        playlist_manager::get()->playlist_move_selection(playlist, delta);
    }

    void execute_default_action(size_t playlist, size_t trackIndex) override {
        playlist_manager::get()->playlist_execute_default_action(playlist, trackIndex);
    }

    // -- Complex methods (defined in PlaylistApi.cpp) ----------

    InsertTracksResult insert_tracks(size_t playlist, size_t position, const nlohmann::json& handles) override;
    nlohmann::json get_tracks_json(size_t playlist, size_t start, size_t count, const nlohmann::json& formats) const override;
    nlohmann::json get_selected_tracks_json(size_t playlist) const override;

    // -- Undo / Redo ---------------------------------------------

    bool undo_restore(size_t playlist) override {
        return playlist_manager::get()->playlist_undo_restore(playlist);
    }

    bool redo_restore(size_t playlist) override {
        return playlist_manager::get()->playlist_redo_restore(playlist);
    }

    // -- Reorder / Sort ------------------------------------------

    void reorder_items(size_t playlist, const size_t* order, size_t count) override {
        playlist_manager::get()->playlist_reorder_items(playlist, order, count);
    }

    void sort_by_format(size_t playlist, const char* pattern, bool selectedOnly) override {
        playlist_manager::get()->playlist_sort_by_format(playlist, pattern, selectedOnly);
    }

    // -- Autoplaylist management ---------------------------------

    AutoplaylistDetectResult detect_autoplaylist(size_t playlist) const override {
        AutoplaylistDetectResult result;
        auto apm = autoplaylist_manager::get();
        auto plm = playlist_manager::get();
        result.isAutoplaylist = apm->is_client_present(playlist);
        if (!result.isAutoplaylist && plm->playlist_lock_is_present(playlist)) {
            pfc::string8 lockNameBuf;
            if (plm->playlist_lock_query_name(playlist, lockNameBuf)) {
                result.lockName = lockNameBuf.c_str();
                if (result.lockName == "Autoplaylist" || result.lockName.find("Auto") != std::string::npos) {
                    result.isAutoplaylist = true;
                    result.isDuiAutoplaylist = true;
                }
            }
        }
        return result;
    }

    void add_autoplaylist_client(size_t playlist, const char* query, const char* sort, uint32_t flags) override {
        autoplaylist_manager::get()->add_client_simple(query, sort, playlist, flags);
    }

    void remove_autoplaylist_client(size_t playlist) override {
        autoplaylist_manager::get()->remove_client(playlist);
    }

    uint32_t get_autoplaylist_flags(size_t playlist) const override {
        auto apm2 = autoplaylist_manager_v2::tryGet();
        if (apm2.is_valid()) return apm2->get_client_flags(playlist);
        return 0;
    }

    // -- Playlist-level reorder ----------------------------------

    bool reorder_playlists(const size_t* order, size_t count) override {
        return playlist_manager::get()->reorder(order, count);
    }

    // -- Complex path-based methods (defined in PlaylistApi.cpp) -

    AddPathsResult add_paths(size_t playlist, const nlohmann::json& paths) override;
    AddPathsResult add_handles(size_t playlist, const nlohmann::json& handles) override;
    AddPathsSequentialResult add_paths_sequential(size_t playlist, const nlohmann::json& paths) override;
    AsyncAddPathsInfo start_add_paths_async(
        size_t playlist, const nlohmann::json& paths,
        const std::string& operationId,
        std::function<void(size_t addedCount, size_t totalCount)> onComplete) override;
    ReplaceAllResult replace_all(size_t playlist, const nlohmann::json& paths) override;
};
