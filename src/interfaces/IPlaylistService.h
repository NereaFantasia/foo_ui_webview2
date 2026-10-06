#pragma once
#include <map>
#include <string>
#include <vector>
#include <functional>
#include <guiddef.h>
#include <nlohmann/json.hpp>

#include "api/generated/PlaylistSchema.h"

// fields 投影的解析结果（TrackWireSnapshot.h）。接口签名只按引用传递，
// 前向声明让本头不把 ErrorEnvelope 链带进每个消费方；要读成员的定义方
// （PlaylistApi.cpp、MockPlaylistService.h）自己含 TrackWireSnapshot.h。
struct TrackFieldSelection;

// Abstract interface for PlaylistApi handler operations.
// Production: Fb2kPlaylistService (delegates to playlist_manager / autoplaylist_manager).
// Testing:    MockPlaylistService (in-memory state + call counters).
class IPlaylistService {
public:
    virtual ~IPlaylistService() = default;

    // -- Playlist lifecycle (CRUD) ------------------------------

    /** @brief Total number of playlists. */
    virtual size_t get_playlist_count() const = 0;
    /** @brief Index of the active (focused) playlist. */
    virtual size_t get_active_playlist() const = 0;
    /** @brief Make the playlist at the given index active. */
    virtual void set_active_playlist(size_t index) = 0;
    /** @brief Index of the playlist that owns the now-playing item. */
    virtual size_t get_playing_playlist() const = 0;

    /** @brief Create a playlist with the given name at position; returns its index. */
    virtual size_t create_playlist(const std::string& name, size_t position) = 0;
    /** @brief Remove the playlist at the given index. */
    virtual bool remove_playlist(size_t index) = 0;
    /** @brief Rename the playlist at the given index. */
    virtual bool playlist_rename(size_t index, const std::string& name) = 0;
    /** @brief Remove all tracks from the playlist. */
    virtual void playlist_clear(size_t index) = 0;
    /** @brief Snapshot the playlist so the next edit can be undone. */
    virtual void playlist_undo_backup(size_t index) = 0;
    /** @brief Number of tracks in the playlist. */
    virtual size_t playlist_get_item_count(size_t index) const = 0;
    /** @brief Write the playlist's name to out; false if the index is invalid. */
    virtual bool playlist_get_name(size_t index, std::string& out) const = 0;

    // -- Identity -----------------------------------------------

    /** @brief GUID of the playlist at the given index; it stays with the playlist when others
        are added, removed or reordered. */
    virtual GUID get_playlist_guid(size_t index) const = 0;
    /** @brief Current index of the playlist with this GUID. An index not below
        get_playlist_count() means no playlist has it. */
    virtual size_t find_playlist_by_guid(const GUID& guid) const = 0;

    // -- Lock queries -------------------------------------------

    /** @brief Whether the playlist currently has an editing lock. */
    virtual bool playlist_lock_is_present(size_t index) const = 0;
    /** @brief Write the lock owner's name to out; false if unlocked. */
    virtual bool playlist_lock_query_name(size_t index, std::string& out) const = 0;

    // -- Autoplaylist (lightweight check) -----------------------

    /** @brief Whether the playlist is backed by an autoplaylist client. */
    virtual bool autoplaylist_is_client_present(size_t index) const = 0;

    // -- High-level aggregators ---------------------------------

    // PlaylistGetAll: returns summary for every playlist
    struct PlaylistInfo {
        size_t index = 0;
        std::string name;
        size_t trackCount = 0;
        bool isActive = false;
        bool isPlaying = false;
        bool isLocked = false;
        bool isAutoplaylist = false;
        GUID guid{};
    };
    /** @brief Summary (name, counts, flags) of every playlist. */
    virtual std::vector<PlaylistInfo> get_all_playlists() const = 0;

    // PlaylistGetActive / PlaylistGetPlaying: detailed single-playlist query
    struct PlaylistDetail {
        bool found = false;
        size_t index = 0;
        std::string name;
        size_t trackCount = 0;
        bool isActive = false;
        bool isPlaying = false;
        bool isLocked = false;
        double duration = 0.0;
        GUID guid{};
    };
    /** @brief Detailed info for one playlist (optionally including total duration). */
    virtual PlaylistDetail get_playlist_detail(size_t index, bool includeDuration) const = 0;

    // -- Selection management -----------------------------------

    /** @brief Indices of the selected tracks in the playlist. */
    virtual std::vector<size_t> get_selection_indices(size_t playlist) const = 0;
    /** @brief Select the given track indices (optionally clearing the prior selection). */
    virtual void set_selection(size_t playlist, const std::vector<size_t>& indices, bool clearOthers) = 0;
    /** @brief Select every track in the playlist. */
    virtual void select_all(size_t playlist) = 0;
    /** @brief Clear the playlist's selection. */
    virtual void deselect_all(size_t playlist) = 0;

    // -- Focus item ---------------------------------------------

    /** @brief Index of the focused track (SIZE_MAX if none). */
    virtual size_t get_focus_item(size_t playlist) const = 0;  // SIZE_MAX if none
    /** @brief Set the focused track by index. */
    virtual void set_focus_item(size_t playlist, size_t trackIndex) = 0;

    // -- Track removal ------------------------------------------

    /** @brief Remove the given track indices from the playlist. */
    virtual void remove_tracks(size_t playlist, const std::vector<size_t>& indices) = 0;
    /** @brief Remove the currently selected tracks. */
    virtual void remove_selection(size_t playlist) = 0;

    // -- Track movement + playback ------------------------------

    /** @brief Move the selected tracks by delta positions (negative = up). */
    virtual void move_selection(size_t playlist, int delta) = 0;
    /** @brief Trigger the track's default action (usually start playback). */
    virtual void execute_default_action(size_t playlist, size_t trackIndex) = 0;

    // -- Track insert (involves metadb resolution) -------------

    struct InsertTracksResult {
        bool success = false;
        std::string error;
        size_t addedCount = 0;
        size_t invalidCount = 0;
        size_t countBefore = 0;
        size_t totalCount = 0;
    };
    /** @brief Resolve handle specs and insert the tracks at position. */
    virtual InsertTracksResult insert_tracks(size_t playlist, size_t position, const nlohmann::json& handles) = 0;

    // -- Track info retrieval (involves metadb + titleformat) --

    /** @brief A page of playlist rows, in the declared shape of playlist.getTracks.
        `fields` is the parsed projection; projected=false gives whole rows with the play
        statistics. `formats` maps column names to Title Formatting patterns. */
    virtual api::playlist::GetTracksResult get_tracks(size_t playlist, size_t start, size_t count,
                                                      const std::map<std::string, std::string>& formats,
                                                      const TrackFieldSelection& fields) const = 0;
    /** @brief The given rows, in the declared shape of playlist.getTracksAt: in the order given,
        rows past the last one skipped, a row given twice returned twice. `formats` and `fields` as
        in get_tracks. */
    virtual api::playlist::GetTracksAtResult get_tracks_at(size_t playlist, const std::vector<size_t>& rows,
                                                          const std::map<std::string, std::string>& formats,
                                                          const TrackFieldSelection& fields) const = 0;
    /** @brief The selected rows, in the declared shape of playlist.getSelectedTracks. */
    virtual api::playlist::GetSelectedTracksResult get_selected_tracks(size_t playlist) const = 0;

    // -- Playlist duplication -----------------------------------

    // PlaylistDuplicate: copy a playlist with all tracks
    struct DuplicateResult {
        bool success = false;
        std::string error;
        size_t newIndex = 0;
        size_t sourceIndex = 0;
        std::string name;
        size_t trackCount = 0;
    };
    /** @brief Copy a playlist (with all its tracks) under a new name. */
    virtual DuplicateResult duplicate_playlist(size_t sourceIndex, const std::string& newName) = 0;

    // -- Undo / Redo --------------------------------------------

    /** @brief Restore the playlist's previous state (undo). */
    virtual bool undo_restore(size_t playlist) = 0;
    /** @brief Re-apply an undone change (redo). */
    virtual bool redo_restore(size_t playlist) = 0;

    // -- Reorder / Sort -----------------------------------------

    /** @brief Reorder tracks by an explicit index permutation. */
    virtual void reorder_items(size_t playlist, const size_t* order, size_t count) = 0;
    /** @brief Sort tracks by a titleformat pattern (optionally selected only). */
    virtual void sort_by_format(size_t playlist, const char* pattern, bool selectedOnly) = 0;

    // -- Autoplaylist management --------------------------------

    struct AutoplaylistDetectResult {
        bool isAutoplaylist = false;
        bool isDuiAutoplaylist = false;
        std::string lockName;
    };
    /** @brief Detect whether (and which kind of) autoplaylist backs the playlist. */
    virtual AutoplaylistDetectResult detect_autoplaylist(size_t playlist) const = 0;
    /** @brief Convert the playlist into an autoplaylist driven by a query/sort. */
    virtual void add_autoplaylist_client(size_t playlist, const char* query, const char* sort, uint32_t flags) = 0;
    /** @brief Detach the autoplaylist client, leaving a normal playlist. */
    virtual void remove_autoplaylist_client(size_t playlist) = 0;
    /** @brief Current autoplaylist flag bits for the playlist. */
    virtual uint32_t get_autoplaylist_flags(size_t playlist) const = 0;

    // -- Playlist-level reorder ---------------------------------

    /** @brief Reorder the playlists themselves by an index permutation. */
    virtual bool reorder_playlists(const size_t* order, size_t count) = 0;

    // -- Path-based insertion -----------------------------------

    // foobar2000 resolves the paths before anything is inserted. process_locations runs a modal
    // progress dialog with its own message loop and the async add expands in the background, so
    // playlists can be reordered, removed or locked in between. The methods that resolve paths
    // look the target up again by its GUID before inserting and report the outcome here.
    enum class PathInsertOutcome {
        Ok,               // inserted, or nothing resolved and no insert was tried
        PlaylistRemoved,  // the target playlist was removed while the paths were resolved
        Refused,          // playlist_insert_items refused the whole batch, as a lock does
    };

    struct AddPathsResult {
        size_t addedCount = 0;
        size_t invalidCount = 0;
        size_t countBefore = 0;
        size_t totalCount = 0;
        PathInsertOutcome outcome = PathInsertOutcome::Ok;
        size_t playlist = 0;  // index of the target when the tracks went in
    };

    /** @brief Resolve paths via incoming_item_filter and append to the playlist. */
    virtual AddPathsResult add_paths(size_t playlist, const nlohmann::json& paths) = 0;

    /** @brief Parse handle specs ({path,subsong} or "path|subsong:N") and append. */
    virtual AddPathsResult add_handles(size_t playlist, const nlohmann::json& handles) = 0;

    // Sequential path add: resolve individually, preserving insertion order.
    struct AddPathsSequentialResult {
        size_t addedCount = 0;
        nlohmann::json order;  // array of insertion positions
        PathInsertOutcome outcome = PathInsertOutcome::Ok;
        size_t playlist = 0;  // index of the target when the tracks went in
    };
    /** @brief Resolve and append paths one by one, preserving insertion order. */
    virtual AddPathsSequentialResult add_paths_sequential(size_t playlist, const nlohmann::json& paths) = 0;

    // Async path add: resolve in background, call onComplete when done.
    struct AsyncAddPathsInfo {
        size_t validPathCount = 0;
        size_t invalidCount = 0;
    };
    /** @brief Resolve paths in the background; invoke onComplete when finished. Tracks expanded
        after the target was removed or locked are not inserted and not counted in addedCount. */
    virtual AsyncAddPathsInfo start_add_paths_async(
        size_t playlist, const nlohmann::json& paths,
        const std::string& operationId,
        std::function<void(size_t addedCount, size_t totalCount)> onComplete) = 0;

    // Clear playlist and re-add from paths.
    struct ReplaceAllResult {
        size_t clearedCount = 0;
        size_t addedCount = 0;
        size_t invalidCount = 0;
        size_t totalCount = 0;
        PathInsertOutcome outcome = PathInsertOutcome::Ok;
        size_t playlist = 0;  // index of the target when the tracks went in
    };
    /** @brief Clear the playlist and re-add tracks from the given paths. */
    virtual ReplaceAllResult replace_all(size_t playlist, const nlohmann::json& paths) = 0;
};
