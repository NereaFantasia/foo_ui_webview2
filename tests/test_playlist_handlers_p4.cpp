// test_playlist_handlers_p4.cpp - PlaylistApi track, selection and focus handlers
// Validates PlaylistApi handler logic using MockPlaylistService.
// Uses reimpl pattern: handler functions mirror product code but accept
// IPlaylistService* instead of calling playlist_manager::get().
#include "pch.h"
#include "mocks/MockPlaylistService.h"
#include "mocks/MockPlaybackService.h"
#include "harness/PlaylistHandlerTestSupport.h"

using json = nlohmann::json;

// ==========================================================================
// P4 reimplemented handler functions (Track/Selection/Focus)
// ==========================================================================
namespace reimpl_p4 {

static json MakePlaylistLockedError(size_t playlistIndex) {
    return {
        {"success", false},
        {"error", "Playlist is locked"},
        {"details", {{"playlist", playlistIndex}, {"isLocked", true}}}
    };
}

// 1. PlaylistGetTrackCount
json PlaylistGetTrackCount(IPlaylistService* svc, const json& params) {
    size_t index = params.contains("playlist") ? params.value("playlist", SIZE_MAX)
                                                : params.value("index", SIZE_MAX);
    if (index == SIZE_MAX) index = svc->get_active_playlist();
    if (index >= svc->get_playlist_count()) return { {"count", 0} };
    return { {"count", svc->playlist_get_item_count(index)} };
}

// 2. PlaylistSetSelection
json PlaylistSetSelection(IPlaylistService* svc, const json& params) {
    size_t playlistIndex = params.contains("playlist") ? params.value("playlist", SIZE_MAX)
                                                        : params.value("index", SIZE_MAX);
    auto indices = params.value("indices", json::array());
    bool clearOthers = params.value("clearOthers", true);
    if (playlistIndex == SIZE_MAX) playlistIndex = svc->get_active_playlist();
    if (playlistIndex >= svc->get_playlist_count())
        return { {"success", false}, {"error", "Invalid playlist index"} };
    std::vector<size_t> idxVec;
    for (const auto& idx : indices) idxVec.push_back(idx.get<size_t>());
    svc->set_selection(playlistIndex, idxVec, clearOthers);
    return { {"success", true} };
}

// 3. PlaylistRemoveTracks
json PlaylistRemoveTracks(IPlaylistService* svc, const json& params) {
    size_t playlistIndex = params.contains("playlist") ? params.value("playlist", SIZE_MAX)
                                                        : params.value("index", SIZE_MAX);
    auto items = params.value("items", json::array());
    if (playlistIndex == SIZE_MAX) playlistIndex = svc->get_active_playlist();
    if (playlistIndex >= svc->get_playlist_count())
        return { {"success", false}, {"error", "Invalid playlist index"} };
    if (svc->playlist_lock_is_present(playlistIndex))
        return MakePlaylistLockedError(playlistIndex);
    std::vector<size_t> indices;
    for (const auto& item : items) indices.push_back(item.get<size_t>());
    svc->remove_tracks(playlistIndex, indices);
    return { {"success", true} };
}

// 4. PlaylistRemoveSelectedTracks
json PlaylistRemoveSelectedTracks(IPlaylistService* svc, const json& params) {
    size_t playlistIndex = params.contains("playlist") ? params.value("playlist", SIZE_MAX)
                                                        : params.value("index", SIZE_MAX);
    if (playlistIndex == SIZE_MAX) playlistIndex = svc->get_active_playlist();
    if (playlistIndex >= svc->get_playlist_count())
        return { {"success", false}, {"error", "Invalid playlist index"} };
    if (svc->playlist_lock_is_present(playlistIndex))
        return MakePlaylistLockedError(playlistIndex);
    svc->remove_selection(playlistIndex);
    return { {"success", true} };
}

// 5. PlaylistMoveTracks
json PlaylistMoveTracks(IPlaylistService* svc, const json& params) {
    size_t playlistIndex = params.contains("playlist") ? params.value("playlist", SIZE_MAX)
                                                        : params.value("index", SIZE_MAX);
    auto items = params.value("items", json::array());
    int delta = params.value("delta", 0);
    if (playlistIndex == SIZE_MAX) playlistIndex = svc->get_active_playlist();
    if (playlistIndex >= svc->get_playlist_count())
        return { {"success", false}, {"error", "Invalid playlist index"} };
    if (svc->playlist_lock_is_present(playlistIndex))
        return MakePlaylistLockedError(playlistIndex);
    svc->playlist_undo_backup(playlistIndex);
    if (!items.empty()) {
        std::vector<size_t> idxVec;
        for (const auto& item : items) idxVec.push_back(item.get<size_t>());
        svc->set_selection(playlistIndex, idxVec, true);
    }
    svc->move_selection(playlistIndex, delta);
    return { {"success", true} };
}

// 6. PlaylistPlayTrack (non-deferred only)
json PlaylistPlayTrack(IPlaylistService* svc, const json& params) {
    size_t playlistIndex = params.value("playlist", SIZE_MAX);
    size_t trackIndex = params.value("index", params.value("track", static_cast<size_t>(0)));
    if (playlistIndex == SIZE_MAX) playlistIndex = svc->get_active_playlist();
    if (playlistIndex >= svc->get_playlist_count())
        return { {"success", false}, {"error", "Invalid playlist index"} };
    size_t trackCount = svc->playlist_get_item_count(playlistIndex);
    if (trackIndex >= trackCount)
        return { {"success", false}, {"error", "Invalid track index"} };
    svc->execute_default_action(playlistIndex, trackIndex);
    return { {"success", true} };
}

// 7. PlaylistFocusTrack (deprecated)
json PlaylistFocusTrack(IPlaylistService* svc, const json& params) {
    size_t playlistIndex = params.value("playlist", SIZE_MAX);
    size_t trackIndex = params.value("index", params.value("track", SIZE_MAX));
    if (playlistIndex == SIZE_MAX) playlistIndex = svc->get_active_playlist();
    if (playlistIndex >= svc->get_playlist_count())
        return { {"success", false}, {"error", "Invalid playlist"} };
    svc->set_focus_item(playlistIndex, trackIndex);
    return { {"success", true} };
}

// 8. PlaylistGetFocusTrack (deprecated)
json PlaylistGetFocusTrack(IPlaylistService* svc, const json& params) {
    size_t playlistIndex = params.value("playlist", SIZE_MAX);
    if (playlistIndex == SIZE_MAX) playlistIndex = svc->get_active_playlist();
    if (playlistIndex >= svc->get_playlist_count())
        return { {"success", false}, {"error", "Invalid playlist"} };
    size_t focus = svc->get_focus_item(playlistIndex);
    return {{"playlist", playlistIndex}, {"index", focus == SIZE_MAX ? -1 : (int64_t)focus}};
}

// 9. PlaylistGetSelection
json PlaylistGetSelection(IPlaylistService* svc, const json& params) {
    size_t playlistIndex = params.value("playlist", SIZE_MAX);
    if (playlistIndex == SIZE_MAX) playlistIndex = svc->get_active_playlist();
    if (playlistIndex >= svc->get_playlist_count())
        return { {"success", false}, {"error", "Invalid playlist"} };
    auto indices = svc->get_selection_indices(playlistIndex);
    json items = json::array();
    for (size_t i : indices) items.push_back(i);
    return { {"success", true}, {"items", items}, {"count", items.size()}, {"playlist", playlistIndex} };
}

// 10. PlaylistSelectAll
json PlaylistSelectAll(IPlaylistService* svc, const json& params) {
    size_t playlistIndex = params.value("playlist", SIZE_MAX);
    if (playlistIndex == SIZE_MAX) playlistIndex = svc->get_active_playlist();
    if (playlistIndex >= svc->get_playlist_count()) return { {"success", false} };
    svc->select_all(playlistIndex);
    return { {"success", true} };
}

// 11. PlaylistDeselectAll
json PlaylistDeselectAll(IPlaylistService* svc, const json& params) {
    size_t playlistIndex = params.value("playlist", SIZE_MAX);
    if (playlistIndex == SIZE_MAX) playlistIndex = svc->get_active_playlist();
    if (playlistIndex >= svc->get_playlist_count()) return { {"success", false} };
    svc->deselect_all(playlistIndex);
    return { {"success", true} };
}

// 12. PlaylistGetFocusedTrack
json PlaylistGetFocusedTrack(IPlaylistService* svc, const json& params) {
    size_t playlistIndex = params.value("playlist", SIZE_MAX);
    if (playlistIndex == SIZE_MAX) playlistIndex = svc->get_active_playlist();
    if (playlistIndex >= svc->get_playlist_count()) return { {"index", -1} };
    size_t focus = svc->get_focus_item(playlistIndex);
    return {{"playlist", playlistIndex}, {"index", focus == SIZE_MAX ? -1 : (int64_t)focus}};
}

// 13. PlaylistSetFocusedTrack
json PlaylistSetFocusedTrack(IPlaylistService* svc, const json& params) {
    size_t playlistIndex = params.value("playlist", SIZE_MAX);
    size_t trackIndex = params.value("index", SIZE_MAX);
    if (playlistIndex == SIZE_MAX) playlistIndex = svc->get_active_playlist();
    if (playlistIndex >= svc->get_playlist_count()) return { {"success", false} };
    svc->set_focus_item(playlistIndex, trackIndex);
    return { {"success", true} };
}

// 14. PlaylistInsertTracks
json PlaylistInsertTracks(IPlaylistService* svc, const json& params) {
    size_t playlistIndex = params.value("playlist", SIZE_MAX);
    size_t insertIndex = params.value("position", params.value("index", static_cast<size_t>(0)));
    auto handles = params.value("handles", json::array());
    if (playlistIndex == SIZE_MAX) playlistIndex = svc->get_active_playlist();
    if (playlistIndex >= svc->get_playlist_count())
        return { {"success", false}, {"error", "Invalid playlist index"} };
    if (svc->playlist_lock_is_present(playlistIndex))
        return MakePlaylistLockedError(playlistIndex);
    if (handles.empty())
        return { {"success", false}, {"error", "No handles specified"} };
    auto r = svc->insert_tracks(playlistIndex, insertIndex, handles);
    if (!r.success)
        return { {"success", false}, {"error", r.error}, {"playlist", playlistIndex},
                 {"requestedCount", handles.size()}, {"invalidCount", r.invalidCount} };
    return { {"success", true}, {"playlist", playlistIndex}, {"insertIndex", insertIndex},
             {"requestedCount", handles.size()}, {"addedCount", r.addedCount},
             {"invalidCount", r.invalidCount}, {"countBefore", r.countBefore},
             {"totalCount", r.totalCount} };
}

// 15. PlaylistGetTracks
json PlaylistGetTracks(IPlaylistService* svc, const json& params) {
    // fields 校验排在越界检查之前（与 PlaylistApi.cpp 的 handler 同序）；形状归声明的
    // 读取器，这里只重现名字的判定
    std::optional<std::vector<std::string>> names;
    if (params.contains("fields") && params["fields"].is_array()) {
        names = params["fields"].get<std::vector<std::string>>();
    }
    const auto fields = ParseTrackFieldSelection(names);
    if (!fields.valid) return MakeTrackFieldsErrorBody(fields);
    size_t playlistIndex = params.value("playlist", SIZE_MAX);
    size_t start = params.value("start", static_cast<size_t>(0));
    size_t count = params.value("count", static_cast<size_t>(100));
    const auto formats = params.value("formats", std::map<std::string, std::string>{});
    if (playlistIndex == SIZE_MAX) playlistIndex = svc->get_active_playlist();
    if (playlistIndex >= svc->get_playlist_count())
        return { {"success", true}, {"playlist", playlistIndex}, {"start", start}, {"count", 0}, {"total", 0}, {"tracks", json::array()} };
    return api::Result<api::playlist::GetTracksResult>(
               svc->get_tracks(playlistIndex, start, count, formats, fields))
        .ToJson();
}

// 16. PlaylistGetSelectedTracks
json PlaylistGetSelectedTracks(IPlaylistService* svc, const json& params) {
    size_t playlistIndex = params.value("playlist", SIZE_MAX);
    if (playlistIndex == SIZE_MAX) playlistIndex = svc->get_active_playlist();
    if (playlistIndex >= svc->get_playlist_count())
        return { {"success", false}, {"error", "Invalid playlist index"}, {"code", "INVALID_INDEX"}, {"tracks", json::array()} };
    return api::Result<api::playlist::GetSelectedTracksResult>(svc->get_selected_tracks(playlistIndex))
        .ToJson();
}

} // namespace reimpl_p4

// ==========================================================================
// P4 helper: set up selection / focus state
// ==========================================================================
class PlaylistHandlerP4Test : public PlaylistHandlerTest {
protected:
    void SetUpP4(size_t numPlaylists = 1, size_t trackCount = 10) {
        for (size_t i = 0; i < numPlaylists; i++) {
            mock.playlists.push_back({"PL" + std::to_string(i), trackCount, false, false, ""});
            mock.selectionIndices.push_back({});
            mock.focusItems.push_back(SIZE_MAX);
        }
        mock.activePlaylist = 0;
    }
};

// ==========================================================================
// P4: PlaylistGetTrackCount tests
// ==========================================================================
TEST_F(PlaylistHandlerP4Test, GetTrackCount_Valid) {
    SetUpP4(2, 42);
    auto r = reimpl_p4::PlaylistGetTrackCount(&mock, {{"playlist", 0}});
    EXPECT_EQ(r["count"], 42);
}

TEST_F(PlaylistHandlerP4Test, GetTrackCount_DefaultActive) {
    SetUpP4(2, 15);
    mock.activePlaylist = 1;
    auto r = reimpl_p4::PlaylistGetTrackCount(&mock, json::object());
    EXPECT_EQ(r["count"], 15);
}

TEST_F(PlaylistHandlerP4Test, GetTrackCount_InvalidIndex) {
    SetUpP4();
    auto r = reimpl_p4::PlaylistGetTrackCount(&mock, {{"playlist", 99}});
    EXPECT_EQ(r["count"], 0);
}

// ==========================================================================
// P4: PlaylistSetSelection tests
// ==========================================================================
TEST_F(PlaylistHandlerP4Test, SetSelection_ClearOthers) {
    SetUpP4();
    auto r = reimpl_p4::PlaylistSetSelection(&mock, {{"playlist", 0}, {"indices", {1, 3, 5}}, {"clearOthers", true}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.setSelectionCallCount, 1);
    EXPECT_TRUE(mock.lastSetSelectionClearOthers);
    EXPECT_EQ(mock.lastSetSelectionIndices, (std::vector<size_t>{1, 3, 5}));
}

TEST_F(PlaylistHandlerP4Test, SetSelection_Additive) {
    SetUpP4();
    auto r = reimpl_p4::PlaylistSetSelection(&mock, {{"playlist", 0}, {"indices", {2}}, {"clearOthers", false}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_FALSE(mock.lastSetSelectionClearOthers);
}

TEST_F(PlaylistHandlerP4Test, SetSelection_InvalidPlaylist) {
    auto r = reimpl_p4::PlaylistSetSelection(&mock, {{"playlist", 99}, {"indices", {0}}});
    EXPECT_FALSE(r["success"].get<bool>());
}

// ==========================================================================
// P4: PlaylistRemoveTracks tests
// ==========================================================================
TEST_F(PlaylistHandlerP4Test, RemoveTracks_Valid) {
    SetUpP4();
    auto r = reimpl_p4::PlaylistRemoveTracks(&mock, {{"playlist", 0}, {"items", {1, 3}}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.removeTracksCallCount, 1);
    EXPECT_EQ(mock.lastRemoveTracksIndices, (std::vector<size_t>{1, 3}));
}

TEST_F(PlaylistHandlerP4Test, RemoveTracks_Locked) {
    SetUpP4();
    mock.playlists[0].isLocked = true;
    auto r = reimpl_p4::PlaylistRemoveTracks(&mock, {{"playlist", 0}, {"items", {0}}});
    EXPECT_FALSE(r["success"].get<bool>());
    EXPECT_EQ(mock.removeTracksCallCount, 0);
}

TEST_F(PlaylistHandlerP4Test, RemoveTracks_InvalidPlaylist) {
    auto r = reimpl_p4::PlaylistRemoveTracks(&mock, {{"playlist", 99}, {"items", {0}}});
    EXPECT_FALSE(r["success"].get<bool>());
}

// ==========================================================================
// P4: PlaylistRemoveSelectedTracks tests
// ==========================================================================
TEST_F(PlaylistHandlerP4Test, RemoveSelectedTracks_Valid) {
    SetUpP4();
    auto r = reimpl_p4::PlaylistRemoveSelectedTracks(&mock, {{"playlist", 0}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.removeSelectionCallCount, 1);
}

TEST_F(PlaylistHandlerP4Test, RemoveSelectedTracks_Locked) {
    SetUpP4();
    mock.playlists[0].isLocked = true;
    auto r = reimpl_p4::PlaylistRemoveSelectedTracks(&mock, {{"playlist", 0}});
    EXPECT_FALSE(r["success"].get<bool>());
    EXPECT_EQ(mock.removeSelectionCallCount, 0);
}

// ==========================================================================
// P4: PlaylistMoveTracks tests
// ==========================================================================
TEST_F(PlaylistHandlerP4Test, MoveTracks_WithItems) {
    SetUpP4();
    auto r = reimpl_p4::PlaylistMoveTracks(&mock, {{"playlist", 0}, {"items", {2, 3}}, {"delta", -1}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.setSelectionCallCount, 1);
    EXPECT_EQ(mock.moveSelectionCallCount, 1);
    EXPECT_EQ(mock.lastMoveSelectionDelta, -1);
}

TEST_F(PlaylistHandlerP4Test, MoveTracks_NoItems) {
    SetUpP4();
    auto r = reimpl_p4::PlaylistMoveTracks(&mock, {{"playlist", 0}, {"delta", 2}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.setSelectionCallCount, 0);
    EXPECT_EQ(mock.moveSelectionCallCount, 1);
    EXPECT_EQ(mock.lastMoveSelectionDelta, 2);
}

TEST_F(PlaylistHandlerP4Test, MoveTracks_Locked) {
    SetUpP4();
    mock.playlists[0].isLocked = true;
    auto r = reimpl_p4::PlaylistMoveTracks(&mock, {{"playlist", 0}, {"delta", 1}});
    EXPECT_FALSE(r["success"].get<bool>());
    EXPECT_EQ(mock.moveSelectionCallCount, 0);
}

// ==========================================================================
// P4: PlaylistPlayTrack tests
// ==========================================================================
TEST_F(PlaylistHandlerP4Test, PlayTrack_Valid) {
    SetUpP4(1, 5);
    auto r = reimpl_p4::PlaylistPlayTrack(&mock, {{"playlist", 0}, {"index", 3}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.executeDefaultActionCallCount, 1);
    EXPECT_EQ(mock.lastExecutePlaylist, 0u);
    EXPECT_EQ(mock.lastExecuteTrackIndex, 3u);
}

TEST_F(PlaylistHandlerP4Test, PlayTrack_InvalidIndex) {
    SetUpP4(1, 5);
    auto r = reimpl_p4::PlaylistPlayTrack(&mock, {{"playlist", 0}, {"index", 99}});
    EXPECT_FALSE(r["success"].get<bool>());
    EXPECT_EQ(mock.executeDefaultActionCallCount, 0);
}

TEST_F(PlaylistHandlerP4Test, PlayTrack_DefaultActive) {
    SetUpP4(1, 5);
    auto r = reimpl_p4::PlaylistPlayTrack(&mock, {{"index", 2}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.lastExecutePlaylist, 0u);
    EXPECT_EQ(mock.lastExecuteTrackIndex, 2u);
}

// ==========================================================================
// P4: PlaylistFocusTrack (deprecated) tests
// ==========================================================================
TEST_F(PlaylistHandlerP4Test, FocusTrack_Deprecated_Valid) {
    SetUpP4();
    auto r = reimpl_p4::PlaylistFocusTrack(&mock, {{"playlist", 0}, {"index", 5}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.setFocusItemCallCount, 1);
    EXPECT_EQ(mock.lastSetFocusIndex, 5u);
}

TEST_F(PlaylistHandlerP4Test, FocusTrack_Deprecated_OldParam) {
    SetUpP4();
    auto r = reimpl_p4::PlaylistFocusTrack(&mock, {{"playlist", 0}, {"track", 7}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.lastSetFocusIndex, 7u);
}

// ==========================================================================
// P4: PlaylistGetFocusTrack (deprecated) tests
// ==========================================================================
TEST_F(PlaylistHandlerP4Test, GetFocusTrack_Deprecated_Valid) {
    SetUpP4();
    mock.focusItems[0] = 3;
    auto r = reimpl_p4::PlaylistGetFocusTrack(&mock, {{"playlist", 0}});
    EXPECT_EQ(r["index"], 3);
    EXPECT_EQ(r["playlist"], 0);
}

TEST_F(PlaylistHandlerP4Test, GetFocusTrack_Deprecated_None) {
    SetUpP4();
    auto r = reimpl_p4::PlaylistGetFocusTrack(&mock, {{"playlist", 0}});
    EXPECT_EQ(r["index"], -1);
}

// ==========================================================================
// P4: PlaylistGetSelection tests
// ==========================================================================
TEST_F(PlaylistHandlerP4Test, GetSelection_Empty) {
    SetUpP4();
    auto r = reimpl_p4::PlaylistGetSelection(&mock, {{"playlist", 0}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(r["count"], 0);
    EXPECT_TRUE(r["items"].empty());
}

TEST_F(PlaylistHandlerP4Test, GetSelection_WithItems) {
    SetUpP4();
    mock.selectionIndices[0] = {1, 4, 7};
    auto r = reimpl_p4::PlaylistGetSelection(&mock, {{"playlist", 0}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(r["count"], 3);
    auto items = r["items"].get<std::vector<size_t>>();
    EXPECT_EQ(items, (std::vector<size_t>{1, 4, 7}));
}

// ==========================================================================
// P4: PlaylistSelectAll tests
// ==========================================================================
TEST_F(PlaylistHandlerP4Test, SelectAll_Valid) {
    SetUpP4(1, 5);
    auto r = reimpl_p4::PlaylistSelectAll(&mock, {{"playlist", 0}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.selectAllCallCount, 1);
    EXPECT_EQ(mock.selectionIndices[0], (std::vector<size_t>{0, 1, 2, 3, 4}));
}

TEST_F(PlaylistHandlerP4Test, SelectAll_Invalid) {
    auto r = reimpl_p4::PlaylistSelectAll(&mock, {{"playlist", 99}});
    EXPECT_FALSE(r["success"].get<bool>());
}

// ==========================================================================
// P4: PlaylistDeselectAll tests
// ==========================================================================
TEST_F(PlaylistHandlerP4Test, DeselectAll_Valid) {
    SetUpP4();
    mock.selectionIndices[0] = {1, 2};
    auto r = reimpl_p4::PlaylistDeselectAll(&mock, {{"playlist", 0}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.deselectAllCallCount, 1);
    EXPECT_TRUE(mock.selectionIndices[0].empty());
}

// ==========================================================================
// P4: PlaylistGetFocusedTrack tests
// ==========================================================================
TEST_F(PlaylistHandlerP4Test, GetFocusedTrack_Valid) {
    SetUpP4();
    mock.focusItems[0] = 8;
    auto r = reimpl_p4::PlaylistGetFocusedTrack(&mock, {{"playlist", 0}});
    EXPECT_EQ(r["index"], 8);
    EXPECT_EQ(r["playlist"], 0);
}

TEST_F(PlaylistHandlerP4Test, GetFocusedTrack_None) {
    SetUpP4();
    auto r = reimpl_p4::PlaylistGetFocusedTrack(&mock, {{"playlist", 0}});
    EXPECT_EQ(r["index"], -1);
}

TEST_F(PlaylistHandlerP4Test, GetFocusedTrack_Invalid) {
    auto r = reimpl_p4::PlaylistGetFocusedTrack(&mock, {{"playlist", 99}});
    EXPECT_EQ(r["index"], -1);
}

// ==========================================================================
// P4: PlaylistSetFocusedTrack tests
// ==========================================================================
TEST_F(PlaylistHandlerP4Test, SetFocusedTrack_Valid) {
    SetUpP4();
    auto r = reimpl_p4::PlaylistSetFocusedTrack(&mock, {{"playlist", 0}, {"index", 4}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.setFocusItemCallCount, 1);
    EXPECT_EQ(mock.focusItems[0], 4u);
}

TEST_F(PlaylistHandlerP4Test, SetFocusedTrack_Invalid) {
    auto r = reimpl_p4::PlaylistSetFocusedTrack(&mock, {{"playlist", 99}, {"index", 0}});
    EXPECT_FALSE(r["success"].get<bool>());
}

// ==========================================================================
// P4b: PlaylistInsertTracks tests
// ==========================================================================
TEST_F(PlaylistHandlerP4Test, InsertTracks_Success) {
    SetUpP4();
    mock.insertTracksResult = {true, "", 3, 0, 10, 13};
    auto r = reimpl_p4::PlaylistInsertTracks(&mock, {{"playlist", 0}, {"handles", {"a", "b", "c"}}, {"position", 5}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(r["addedCount"], 3);
    EXPECT_EQ(r["countBefore"], 10);
    EXPECT_EQ(r["totalCount"], 13);
    EXPECT_EQ(mock.insertTracksCallCount, 1);
}

TEST_F(PlaylistHandlerP4Test, InsertTracks_Locked) {
    SetUpP4();
    mock.playlists[0].isLocked = true;
    auto r = reimpl_p4::PlaylistInsertTracks(&mock, {{"playlist", 0}, {"handles", {"x"}}});
    EXPECT_FALSE(r["success"].get<bool>());
    EXPECT_EQ(mock.insertTracksCallCount, 0);
}

TEST_F(PlaylistHandlerP4Test, InsertTracks_EmptyHandles) {
    SetUpP4();
    auto r = reimpl_p4::PlaylistInsertTracks(&mock, {{"playlist", 0}, {"handles", json::array()}});
    EXPECT_FALSE(r["success"].get<bool>());
}

TEST_F(PlaylistHandlerP4Test, InsertTracks_ServiceFailure) {
    SetUpP4();
    mock.insertTracksResult = {false, "No valid handles created", 0, 2, 10, 10};
    auto r = reimpl_p4::PlaylistInsertTracks(&mock, {{"playlist", 0}, {"handles", {"x", "y"}}});
    EXPECT_FALSE(r["success"].get<bool>());
    EXPECT_EQ(r["error"], "No valid handles created");
}

// ==========================================================================
// P4b: PlaylistGetTracks tests
// ==========================================================================
TEST_F(PlaylistHandlerP4Test, GetTracks_Valid) {
    SetUpP4(1, 20);
    mock.tracksResult.playlist = 0;
    mock.tracksResult.start = 5;
    mock.tracksResult.count = 3;
    mock.tracksResult.total = 20;
    for (std::int64_t row : {5, 6, 7}) {
        api::playlist::PlaylistTrackPartial track;
        track.index = row;
        mock.tracksResult.tracks.push_back(track);
    }
    auto r = reimpl_p4::PlaylistGetTracks(&mock, {{"playlist", 0}, {"start", 5}, {"count", 3}});
    EXPECT_EQ(r["count"], 3);
    EXPECT_EQ(r["total"], 20);
    EXPECT_EQ(mock.getTracksCallCount, 1);
    EXPECT_EQ(mock.lastGetTracksStart, 5u);
    EXPECT_EQ(mock.lastGetTracksCount, 3u);
}

TEST_F(PlaylistHandlerP4Test, GetTracks_InvalidPlaylist) {
    auto r = reimpl_p4::PlaylistGetTracks(&mock, {{"playlist", 99}});
    EXPECT_EQ(r["count"], 0);
    EXPECT_EQ(r["total"], 0);
}

TEST_F(PlaylistHandlerP4Test, GetTracks_DefaultParams) {
    SetUpP4();
    mock.tracksResult.total = 10;
    auto r = reimpl_p4::PlaylistGetTracks(&mock, json::object());
    EXPECT_EQ(mock.getTracksCallCount, 1);
    EXPECT_EQ(mock.lastGetTracksStart, 0u);
    EXPECT_EQ(mock.lastGetTracksCount, 100u);  // default
    EXPECT_FALSE(mock.lastGetTracksProjected);  // 未传 fields 不走进投影
}

// fields 校验排在越界检查之前（SPEC §4.1）：playlist 99 越界但 fields 非法时
// 必须报 INVALID_PARAMS 而不是静默出空页。
TEST_F(PlaylistHandlerP4Test, GetTracks_InvalidFieldsBeforeRangeCheck) {
    SetUpP4(1, 20);
    auto r = reimpl_p4::PlaylistGetTracks(&mock, {{"playlist", 99}, {"fields", {"bogus"}}});
    EXPECT_FALSE(r["success"].get<bool>());
    EXPECT_EQ(r["code"], "INVALID_PARAMS");
    EXPECT_EQ(r["details"]["unknownFields"], json::array({"bogus"}));
    EXPECT_EQ(mock.getTracksCallCount, 0);
}

TEST_F(PlaylistHandlerP4Test, GetTracks_ValidFieldsForwardedAsProjection) {
    SetUpP4(1, 20);
    mock.tracksResult.count = 1;
    mock.tracksResult.total = 20;
    api::playlist::PlaylistTrackPartial track;
    track.index = 0;
    track.title = "t";
    mock.tracksResult.tracks.push_back(track);
    auto r = reimpl_p4::PlaylistGetTracks(&mock, {{"playlist", 0}, {"fields", {"title"}}});
    EXPECT_EQ(mock.getTracksCallCount, 1);
    EXPECT_TRUE(mock.lastGetTracksProjected);
    EXPECT_EQ(r["tracks"][0].size(), 2u);  // index + title
}

// ==========================================================================
// P4b: PlaylistGetSelectedTracks tests
// ==========================================================================
TEST_F(PlaylistHandlerP4Test, GetSelectedTracks_Valid) {
    SetUpP4();
    auto r = reimpl_p4::PlaylistGetSelectedTracks(&mock, {{"playlist", 0}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.getSelectedTracksCallCount, 1);
}

TEST_F(PlaylistHandlerP4Test, GetSelectedTracks_InvalidPlaylist) {
    auto r = reimpl_p4::PlaylistGetSelectedTracks(&mock, {{"playlist", 99}});
    EXPECT_FALSE(r["success"].get<bool>());
}

// ==========================================================================
// M9: P4 audit - added missing boundary tests
// ==========================================================================

TEST_F(PlaylistHandlerP4Test, RemoveTracks_DefaultActive) {
    SetUpP4();
    auto r = reimpl_p4::PlaylistRemoveTracks(&mock, {{"items", {0, 2}}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.lastRemoveTracksPlaylist, 0u);
}

TEST_F(PlaylistHandlerP4Test, RemoveSelectedTracks_InvalidPlaylist) {
    auto r = reimpl_p4::PlaylistRemoveSelectedTracks(&mock, {{"playlist", 99}});
    EXPECT_FALSE(r["success"].get<bool>());
}

TEST_F(PlaylistHandlerP4Test, MoveTracks_InvalidPlaylist) {
    auto r = reimpl_p4::PlaylistMoveTracks(&mock, {{"playlist", 99}, {"delta", 1}});
    EXPECT_FALSE(r["success"].get<bool>());
}

TEST_F(PlaylistHandlerP4Test, InsertTracks_InvalidPlaylist) {
    auto r = reimpl_p4::PlaylistInsertTracks(&mock, {{"playlist", 99}, {"handles", {"x"}}});
    EXPECT_FALSE(r["success"].get<bool>());
}

TEST_F(PlaylistHandlerP4Test, InsertTracks_DefaultActive) {
    SetUpP4();
    mock.insertTracksResult = {true, "", 1, 0, 10, 11};
    auto r = reimpl_p4::PlaylistInsertTracks(&mock, {{"handles", {"a"}}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.lastInsertTracksPlaylist, 0u);
}

TEST_F(PlaylistHandlerP4Test, GetSelection_InvalidPlaylist) {
    auto r = reimpl_p4::PlaylistGetSelection(&mock, {{"playlist", 99}});
    EXPECT_FALSE(r["success"].get<bool>());
}

TEST_F(PlaylistHandlerP4Test, DeselectAll_InvalidPlaylist) {
    auto r = reimpl_p4::PlaylistDeselectAll(&mock, {{"playlist", 99}});
    EXPECT_FALSE(r["success"].get<bool>());
}
