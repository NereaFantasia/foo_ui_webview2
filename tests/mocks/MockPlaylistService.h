#pragma once
#include "../src/interfaces/IPlaylistService.h"
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

// Phase 4 P3: Mock implementation of IPlaylistService for offline testing.
class MockPlaylistService : public IPlaylistService {
public:
    // -- Simulated state ----------------------------------------------

    // Playlist store (vector index = playlist index)
    struct MockPlaylist {
        std::string name;
        size_t trackCount = 0;
        bool isLocked = false;
        bool isAutoplaylist = false;
        std::string lockName;
    };
    std::vector<MockPlaylist> playlists;
    size_t activePlaylist = 0;
    size_t playingPlaylist = SIZE_MAX;

    // For detail queries
    double detailDuration = 0.0;

    // For create
    size_t nextCreateIndex = 0;

    // For remove
    bool removeResult = true;

    // For rename
    bool renameResult = true;

    // For duplicate
    DuplicateResult duplicateResult;

    // -- P4: Selection / Focus / Track state --------------------------

    // Per-playlist selection indices (outer vector index = playlist index)
    std::vector<std::vector<size_t>> selectionIndices;
    // Per-playlist focus item (SIZE_MAX = none)
    std::vector<size_t> focusItems;

    // For insert_tracks
    InsertTracksResult insertTracksResult;

    // For get_tracks_json / get_selected_tracks_json
    nlohmann::json tracksJsonResult = nlohmann::json::array();
    nlohmann::json selectedTracksJsonResult = nlohmann::json::object();

    // -- Call counters ------------------------------------------------

    mutable int getPlaylistCountCallCount = 0;
    mutable int getActivePlaylistCallCount = 0;
    int setActivePlaylistCallCount = 0;
    size_t lastSetActiveIndex = SIZE_MAX;
    mutable int getPlayingPlaylistCallCount = 0;
    int createPlaylistCallCount = 0;
    std::string lastCreateName;
    size_t lastCreatePosition = SIZE_MAX;
    int removePlaylistCallCount = 0;
    size_t lastRemoveIndex = SIZE_MAX;
    int renamePlaylistCallCount = 0;
    size_t lastRenameIndex = SIZE_MAX;
    std::string lastRenameName;
    int clearCallCount = 0;
    size_t lastClearIndex = SIZE_MAX;
    int undoBackupCallCount = 0;
    size_t lastUndoBackupIndex = SIZE_MAX;
    mutable int getAllPlaylistsCallCount = 0;
    mutable int getPlaylistDetailCallCount = 0;
    mutable size_t lastDetailIndex = SIZE_MAX;
    mutable bool lastDetailIncludeDuration = false;
    int duplicateCallCount = 0;
    size_t lastDuplicateSource = SIZE_MAX;
    std::string lastDuplicateName;
    mutable int getNameCallCount = 0;
    mutable int lockIsPresentCallCount = 0;
    mutable int lockQueryNameCallCount = 0;
    mutable int autoplaylistCheckCallCount = 0;
    mutable int getItemCountCallCount = 0;

    // P4 counters
    mutable int getSelectionIndicesCallCount = 0;
    mutable size_t lastGetSelectionPlaylist = SIZE_MAX;
    int setSelectionCallCount = 0;
    size_t lastSetSelectionPlaylist = SIZE_MAX;
    std::vector<size_t> lastSetSelectionIndices;
    bool lastSetSelectionClearOthers = false;
    int selectAllCallCount = 0;
    size_t lastSelectAllPlaylist = SIZE_MAX;
    int deselectAllCallCount = 0;
    size_t lastDeselectAllPlaylist = SIZE_MAX;
    mutable int getFocusItemCallCount = 0;
    int setFocusItemCallCount = 0;
    size_t lastSetFocusPlaylist = SIZE_MAX;
    size_t lastSetFocusIndex = SIZE_MAX;
    int removeTracksCallCount = 0;
    size_t lastRemoveTracksPlaylist = SIZE_MAX;
    std::vector<size_t> lastRemoveTracksIndices;
    int removeSelectionCallCount = 0;
    size_t lastRemoveSelectionPlaylist = SIZE_MAX;
    int moveSelectionCallCount = 0;
    size_t lastMoveSelectionPlaylist = SIZE_MAX;
    int lastMoveSelectionDelta = 0;
    int executeDefaultActionCallCount = 0;
    size_t lastExecutePlaylist = SIZE_MAX;
    size_t lastExecuteTrackIndex = SIZE_MAX;
    int insertTracksCallCount = 0;
    size_t lastInsertTracksPlaylist = SIZE_MAX;
    size_t lastInsertPosition = 0;
    mutable int getTracksJsonCallCount = 0;
    mutable size_t lastGetTracksPlaylist = SIZE_MAX;
    mutable size_t lastGetTracksStart = 0;
    mutable size_t lastGetTracksCount = 0;
    mutable int getSelectedTracksJsonCallCount = 0;
    mutable size_t lastGetSelectedTracksPlaylist = SIZE_MAX;

    // P5 counters
    int undoRestoreCallCount = 0;
    size_t lastUndoRestorePlaylist = SIZE_MAX;
    bool undoRestoreResult = true;
    int redoRestoreCallCount = 0;
    size_t lastRedoRestorePlaylist = SIZE_MAX;
    bool redoRestoreResult = true;
    int reorderItemsCallCount = 0;
    size_t lastReorderItemsPlaylist = SIZE_MAX;
    std::vector<size_t> lastReorderItemsOrder;
    int sortByFormatCallCount = 0;
    size_t lastSortPlaylist = SIZE_MAX;
    std::string lastSortPattern;
    bool lastSortSelectedOnly = false;
    // Autoplaylist
    mutable int detectAutoplaylistCallCount = 0;
    mutable size_t lastDetectPlaylist = SIZE_MAX;
    AutoplaylistDetectResult detectAutoplaylistResult;
    int addAutoplaylistClientCallCount = 0;
    size_t lastAddAutoPlaylist = SIZE_MAX;
    std::string lastAddAutoQuery;
    std::string lastAddAutoSort;
    uint32_t lastAddAutoFlags = 0;
    int removeAutoplaylistClientCallCount = 0;
    size_t lastRemoveAutoPlaylist = SIZE_MAX;
    mutable int getAutoplaylistFlagsCallCount = 0;
    mutable size_t lastGetAutoFlagsPlaylist = SIZE_MAX;
    uint32_t autoplaylistFlagsResult = 0;
    // Playlist-level reorder
    int reorderPlaylistsCallCount = 0;
    std::vector<size_t> lastReorderPlaylistsOrder;
    bool reorderPlaylistsResult = true;

    // -- P5b: Path?based add / replace --------------------------------
    AddPathsResult addPathsResult = { 0, 0, 0, 0 };
    int addPathsCallCount = 0;
    size_t lastAddPathsPlaylist = SIZE_MAX;
    nlohmann::json lastAddPathsPaths;

    int addHandlesCallCount = 0;
    size_t lastAddHandlesPlaylist = SIZE_MAX;
    nlohmann::json lastAddHandlesHandles;

    AddPathsSequentialResult addPathsSeqResult = { 0, {} };
    int addPathsSeqCallCount = 0;
    size_t lastAddPathsSeqPlaylist = SIZE_MAX;
    nlohmann::json lastAddPathsSeqPaths;

    AsyncAddPathsInfo asyncAddPathsInfo = { 0, 0 };
    int startAddPathsAsyncCallCount = 0;
    size_t lastAsyncPlaylist = SIZE_MAX;
    std::string lastAsyncOperationId;
    nlohmann::json lastAsyncPaths;

    ReplaceAllResult replaceAllResult = { 0, 0, 0, 0 };
    int replaceAllCallCount = 0;
    size_t lastReplaceAllPlaylist = SIZE_MAX;
    nlohmann::json lastReplaceAllPaths;

    // -- Reset --------------------------------------------------------

    void Reset() {
        playlists.clear();
        activePlaylist = 0;
        playingPlaylist = SIZE_MAX;
        detailDuration = 0.0;
        nextCreateIndex = 0;
        removeResult = true;
        renameResult = true;
        duplicateResult = {};

        getPlaylistCountCallCount = 0;
        getActivePlaylistCallCount = 0;
        setActivePlaylistCallCount = 0;
        lastSetActiveIndex = SIZE_MAX;
        getPlayingPlaylistCallCount = 0;
        createPlaylistCallCount = 0;
        lastCreateName.clear();
        lastCreatePosition = SIZE_MAX;
        removePlaylistCallCount = 0;
        lastRemoveIndex = SIZE_MAX;
        renamePlaylistCallCount = 0;
        lastRenameIndex = SIZE_MAX;
        lastRenameName.clear();
        clearCallCount = 0;
        lastClearIndex = SIZE_MAX;
        undoBackupCallCount = 0;
        lastUndoBackupIndex = SIZE_MAX;
        getAllPlaylistsCallCount = 0;
        getPlaylistDetailCallCount = 0;
        lastDetailIndex = SIZE_MAX;
        lastDetailIncludeDuration = false;
        duplicateCallCount = 0;
        lastDuplicateSource = SIZE_MAX;
        lastDuplicateName.clear();
        getNameCallCount = 0;
        lockIsPresentCallCount = 0;
        lockQueryNameCallCount = 0;
        autoplaylistCheckCallCount = 0;
        getItemCountCallCount = 0;

        // P4 state
        selectionIndices.clear();
        focusItems.clear();
        insertTracksResult = {};
        tracksJsonResult = nlohmann::json::array();
        selectedTracksJsonResult = nlohmann::json::object();

        // P4 counters
        getSelectionIndicesCallCount = 0;
        lastGetSelectionPlaylist = SIZE_MAX;
        setSelectionCallCount = 0;
        lastSetSelectionPlaylist = SIZE_MAX;
        lastSetSelectionIndices.clear();
        lastSetSelectionClearOthers = false;
        selectAllCallCount = 0;
        lastSelectAllPlaylist = SIZE_MAX;
        deselectAllCallCount = 0;
        lastDeselectAllPlaylist = SIZE_MAX;
        getFocusItemCallCount = 0;
        setFocusItemCallCount = 0;
        lastSetFocusPlaylist = SIZE_MAX;
        lastSetFocusIndex = SIZE_MAX;
        removeTracksCallCount = 0;
        lastRemoveTracksPlaylist = SIZE_MAX;
        lastRemoveTracksIndices.clear();
        removeSelectionCallCount = 0;
        lastRemoveSelectionPlaylist = SIZE_MAX;
        moveSelectionCallCount = 0;
        lastMoveSelectionPlaylist = SIZE_MAX;
        lastMoveSelectionDelta = 0;
        executeDefaultActionCallCount = 0;
        lastExecutePlaylist = SIZE_MAX;
        lastExecuteTrackIndex = SIZE_MAX;
        insertTracksCallCount = 0;
        lastInsertTracksPlaylist = SIZE_MAX;
        lastInsertPosition = 0;
        getTracksJsonCallCount = 0;
        lastGetTracksPlaylist = SIZE_MAX;
        lastGetTracksStart = 0;
        lastGetTracksCount = 0;
        getSelectedTracksJsonCallCount = 0;
        lastGetSelectedTracksPlaylist = SIZE_MAX;

        // P5 counters
        undoRestoreCallCount = 0;
        lastUndoRestorePlaylist = SIZE_MAX;
        undoRestoreResult = true;
        redoRestoreCallCount = 0;
        lastRedoRestorePlaylist = SIZE_MAX;
        redoRestoreResult = true;
        reorderItemsCallCount = 0;
        lastReorderItemsPlaylist = SIZE_MAX;
        lastReorderItemsOrder.clear();
        sortByFormatCallCount = 0;
        lastSortPlaylist = SIZE_MAX;
        lastSortPattern.clear();
        lastSortSelectedOnly = false;
        detectAutoplaylistCallCount = 0;
        lastDetectPlaylist = SIZE_MAX;
        detectAutoplaylistResult = {};
        addAutoplaylistClientCallCount = 0;
        lastAddAutoPlaylist = SIZE_MAX;
        lastAddAutoQuery.clear();
        lastAddAutoSort.clear();
        lastAddAutoFlags = 0;
        removeAutoplaylistClientCallCount = 0;
        lastRemoveAutoPlaylist = SIZE_MAX;
        getAutoplaylistFlagsCallCount = 0;
        lastGetAutoFlagsPlaylist = SIZE_MAX;
        autoplaylistFlagsResult = 0;
        reorderPlaylistsCallCount = 0;
        lastReorderPlaylistsOrder.clear();
        reorderPlaylistsResult = true;

        // P5b counters
        addPathsResult = { 0, 0, 0, 0 };
        addPathsCallCount = 0;
        lastAddPathsPlaylist = SIZE_MAX;
        lastAddPathsPaths = nlohmann::json::array();
        addHandlesCallCount = 0;
        lastAddHandlesPlaylist = SIZE_MAX;
        lastAddHandlesHandles = nlohmann::json::array();
        addPathsSeqResult = { 0, {} };
        addPathsSeqCallCount = 0;
        lastAddPathsSeqPlaylist = SIZE_MAX;
        lastAddPathsSeqPaths = nlohmann::json::array();
        asyncAddPathsInfo = { 0, 0 };
        startAddPathsAsyncCallCount = 0;
        lastAsyncPlaylist = SIZE_MAX;
        lastAsyncOperationId.clear();
        lastAsyncPaths = nlohmann::json::array();
        replaceAllResult = { 0, 0, 0, 0 };
        replaceAllCallCount = 0;
        lastReplaceAllPlaylist = SIZE_MAX;
        lastReplaceAllPaths = nlohmann::json::array();
    }

    // -- IPlaylistService implementation ------------------------------

    size_t get_playlist_count() const override {
        getPlaylistCountCallCount++;
        return playlists.size();
    }

    size_t get_active_playlist() const override {
        getActivePlaylistCallCount++;
        return activePlaylist;
    }

    void set_active_playlist(size_t index) override {
        setActivePlaylistCallCount++;
        lastSetActiveIndex = index;
        activePlaylist = index;
    }

    size_t get_playing_playlist() const override {
        getPlayingPlaylistCallCount++;
        return playingPlaylist;
    }

    size_t create_playlist(const std::string& name, size_t position) override {
        createPlaylistCallCount++;
        lastCreateName = name;
        lastCreatePosition = position;
        size_t idx = nextCreateIndex;
        playlists.insert(playlists.begin() + std::min(idx, playlists.size()),
                         MockPlaylist{name, 0, false, false, ""});
        nextCreateIndex = idx;
        return idx;
    }

    bool remove_playlist(size_t index) override {
        removePlaylistCallCount++;
        lastRemoveIndex = index;
        return removeResult;
    }

    bool playlist_rename(size_t index, const std::string& name) override {
        renamePlaylistCallCount++;
        lastRenameIndex = index;
        lastRenameName = name;
        if (renameResult && index < playlists.size()) {
            playlists[index].name = name;
        }
        return renameResult;
    }

    void playlist_clear(size_t index) override {
        clearCallCount++;
        lastClearIndex = index;
        if (index < playlists.size()) {
            playlists[index].trackCount = 0;
        }
    }

    void playlist_undo_backup(size_t index) override {
        undoBackupCallCount++;
        lastUndoBackupIndex = index;
    }

    size_t playlist_get_item_count(size_t index) const override {
        getItemCountCallCount++;
        if (index < playlists.size()) return playlists[index].trackCount;
        return 0;
    }

    bool playlist_get_name(size_t index, std::string& out) const override {
        getNameCallCount++;
        if (index < playlists.size()) {
            out = playlists[index].name;
            return true;
        }
        return false;
    }

    bool playlist_lock_is_present(size_t index) const override {
        lockIsPresentCallCount++;
        if (index < playlists.size()) return playlists[index].isLocked;
        return false;
    }

    bool playlist_lock_query_name(size_t index, std::string& out) const override {
        lockQueryNameCallCount++;
        if (index < playlists.size() && !playlists[index].lockName.empty()) {
            out = playlists[index].lockName;
            return true;
        }
        return false;
    }

    bool autoplaylist_is_client_present(size_t index) const override {
        autoplaylistCheckCallCount++;
        if (index < playlists.size()) return playlists[index].isAutoplaylist;
        return false;
    }

    std::vector<PlaylistInfo> get_all_playlists() const override {
        getAllPlaylistsCallCount++;
        std::vector<PlaylistInfo> result;
        result.reserve(playlists.size());
        for (size_t i = 0; i < playlists.size(); i++) {
            result.push_back({
                i,
                playlists[i].name,
                playlists[i].trackCount,
                (i == activePlaylist),
                (i == playingPlaylist),
                playlists[i].isLocked,
                playlists[i].isAutoplaylist
            });
        }
        return result;
    }

    PlaylistDetail get_playlist_detail(size_t index, bool includeDuration) const override {
        getPlaylistDetailCallCount++;
        lastDetailIndex = index;
        lastDetailIncludeDuration = includeDuration;
        if (index >= playlists.size()) {
            return { false };
        }
        PlaylistDetail d;
        d.found = true;
        d.index = index;
        d.name = playlists[index].name;
        d.trackCount = playlists[index].trackCount;
        d.isActive = (index == activePlaylist);
        d.isPlaying = (index == playingPlaylist);
        d.isLocked = playlists[index].isLocked;
        d.duration = includeDuration ? detailDuration : 0.0;
        return d;
    }

    DuplicateResult duplicate_playlist(size_t sourceIndex, const std::string& newName) override {
        duplicateCallCount++;
        lastDuplicateSource = sourceIndex;
        lastDuplicateName = newName;
        return duplicateResult;
    }

    // -- P4: Selection management -------------------------------------

    std::vector<size_t> get_selection_indices(size_t playlist) const override {
        getSelectionIndicesCallCount++;
        lastGetSelectionPlaylist = playlist;
        if (playlist < selectionIndices.size()) return selectionIndices[playlist];
        return {};
    }

    void set_selection(size_t playlist, const std::vector<size_t>& indices, bool clearOthers) override {
        setSelectionCallCount++;
        lastSetSelectionPlaylist = playlist;
        lastSetSelectionIndices = indices;
        lastSetSelectionClearOthers = clearOthers;
        if (playlist < selectionIndices.size()) {
            if (clearOthers) {
                selectionIndices[playlist] = indices;
            } else {
                // merge dedup, simulating bit_array mode
                auto& sel = selectionIndices[playlist];
                for (size_t i : indices) {
                    if (std::find(sel.begin(), sel.end(), i) == sel.end())
                        sel.push_back(i);
                }
            }
        }
    }

    void select_all(size_t playlist) override {
        selectAllCallCount++;
        lastSelectAllPlaylist = playlist;
        if (playlist < playlists.size() && playlist < selectionIndices.size()) {
            selectionIndices[playlist].clear();
            for (size_t i = 0; i < playlists[playlist].trackCount; i++)
                selectionIndices[playlist].push_back(i);
        }
    }

    void deselect_all(size_t playlist) override {
        deselectAllCallCount++;
        lastDeselectAllPlaylist = playlist;
        if (playlist < selectionIndices.size()) selectionIndices[playlist].clear();
    }

    // -- P4: Focus item -----------------------------------------------

    size_t get_focus_item(size_t playlist) const override {
        getFocusItemCallCount++;
        if (playlist < focusItems.size()) return focusItems[playlist];
        return SIZE_MAX;
    }

    void set_focus_item(size_t playlist, size_t trackIndex) override {
        setFocusItemCallCount++;
        lastSetFocusPlaylist = playlist;
        lastSetFocusIndex = trackIndex;
        if (playlist < focusItems.size()) focusItems[playlist] = trackIndex;
    }

    // -- P4: Track removal --------------------------------------------

    void remove_tracks(size_t playlist, const std::vector<size_t>& indices) override {
        removeTracksCallCount++;
        lastRemoveTracksPlaylist = playlist;
        lastRemoveTracksIndices = indices;
    }

    void remove_selection(size_t playlist) override {
        removeSelectionCallCount++;
        lastRemoveSelectionPlaylist = playlist;
    }

    // -- P4: Track movement + playback --------------------------------

    void move_selection(size_t playlist, int delta) override {
        moveSelectionCallCount++;
        lastMoveSelectionPlaylist = playlist;
        lastMoveSelectionDelta = delta;
    }

    void execute_default_action(size_t playlist, size_t trackIndex) override {
        executeDefaultActionCallCount++;
        lastExecutePlaylist = playlist;
        lastExecuteTrackIndex = trackIndex;
    }

    // -- P4b: Track insert --------------------------------------------

    InsertTracksResult insert_tracks(size_t playlist, size_t position, const nlohmann::json& handles) override {
        insertTracksCallCount++;
        lastInsertTracksPlaylist = playlist;
        lastInsertPosition = position;
        return insertTracksResult;
    }

    // -- P4b: Track info retrieval ------------------------------------

    nlohmann::json get_tracks_json(size_t playlist, size_t start, size_t count, const nlohmann::json& formats) const override {
        getTracksJsonCallCount++;
        lastGetTracksPlaylist = playlist;
        lastGetTracksStart = start;
        lastGetTracksCount = count;
        return tracksJsonResult;
    }

    nlohmann::json get_selected_tracks_json(size_t playlist) const override {
        getSelectedTracksJsonCallCount++;
        lastGetSelectedTracksPlaylist = playlist;
        return selectedTracksJsonResult;
    }

    // -- P5: Undo / Redo ----------------------------------------------

    bool undo_restore(size_t playlist) override {
        undoRestoreCallCount++;
        lastUndoRestorePlaylist = playlist;
        return undoRestoreResult;
    }

    bool redo_restore(size_t playlist) override {
        redoRestoreCallCount++;
        lastRedoRestorePlaylist = playlist;
        return redoRestoreResult;
    }

    // -- P5: Reorder / Sort -------------------------------------------

    void reorder_items(size_t playlist, const size_t* order, size_t count) override {
        reorderItemsCallCount++;
        lastReorderItemsPlaylist = playlist;
        lastReorderItemsOrder.assign(order, order + count);
    }

    void sort_by_format(size_t playlist, const char* pattern, bool selectedOnly) override {
        sortByFormatCallCount++;
        lastSortPlaylist = playlist;
        lastSortPattern = pattern;
        lastSortSelectedOnly = selectedOnly;
    }

    // -- P5: Autoplaylist management ----------------------------------

    AutoplaylistDetectResult detect_autoplaylist(size_t playlist) const override {
        detectAutoplaylistCallCount++;
        lastDetectPlaylist = playlist;
        return detectAutoplaylistResult;
    }

    void add_autoplaylist_client(size_t playlist, const char* query, const char* sort, uint32_t flags) override {
        addAutoplaylistClientCallCount++;
        lastAddAutoPlaylist = playlist;
        lastAddAutoQuery = query;
        lastAddAutoSort = sort;
        lastAddAutoFlags = flags;
    }

    void remove_autoplaylist_client(size_t playlist) override {
        removeAutoplaylistClientCallCount++;
        lastRemoveAutoPlaylist = playlist;
    }

    uint32_t get_autoplaylist_flags(size_t playlist) const override {
        getAutoplaylistFlagsCallCount++;
        lastGetAutoFlagsPlaylist = playlist;
        return autoplaylistFlagsResult;
    }

    // -- P5: Playlist-level reorder -----------------------------------

    bool reorder_playlists(const size_t* order, size_t count) override {
        reorderPlaylistsCallCount++;
        lastReorderPlaylistsOrder.assign(order, order + count);
        return reorderPlaylistsResult;
    }

    // -- P5b: Path-based add / replace --------------------------------

    AddPathsResult add_paths(size_t playlist, const nlohmann::json& paths) override {
        addPathsCallCount++;
        lastAddPathsPlaylist = playlist;
        lastAddPathsPaths = paths;
        return addPathsResult;
    }

    AddPathsResult add_handles(size_t playlist, const nlohmann::json& handles) override {
        addHandlesCallCount++;
        lastAddHandlesPlaylist = playlist;
        lastAddHandlesHandles = handles;
        return addPathsResult;   // reuse same result struct
    }

    AddPathsSequentialResult add_paths_sequential(size_t playlist, const nlohmann::json& paths) override {
        addPathsSeqCallCount++;
        lastAddPathsSeqPlaylist = playlist;
        lastAddPathsSeqPaths = paths;
        return addPathsSeqResult;
    }

    AsyncAddPathsInfo start_add_paths_async(size_t playlist, const nlohmann::json& paths,
        const std::string& operationId, std::function<void(size_t, size_t)> callback) override {
        startAddPathsAsyncCallCount++;
        lastAsyncPlaylist = playlist;
        lastAsyncPaths = paths;
        lastAsyncOperationId = operationId;
        // Optionally invoke callback synchronously for testing
        if (callback && asyncAddPathsInfo.validPathCount > 0) {
            callback(asyncAddPathsInfo.validPathCount, asyncAddPathsInfo.validPathCount + asyncAddPathsInfo.invalidCount);
        }
        return asyncAddPathsInfo;
    }

    ReplaceAllResult replace_all(size_t playlist, const nlohmann::json& paths) override {
        replaceAllCallCount++;
        lastReplaceAllPlaylist = playlist;
        lastReplaceAllPaths = paths;
        return replaceAllResult;
    }
};
