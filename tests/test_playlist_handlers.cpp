// test_playlist_handlers.cpp - Phase 4 P3 offline handler logic tests
// Validates PlaylistApi handler logic using MockPlaylistService.
// Uses reimpl pattern: handler functions mirror product code but accept
// IPlaylistService* instead of calling playlist_manager::get().
#include "pch.h"
#include "mocks/MockPlaylistService.h"
#include "mocks/MockPlaybackService.h"

using json = nlohmann::json;

// ==========================================================================
// Reimplemented handler functions (mirror PlaylistApi.cpp P3 handlers)
// ==========================================================================
namespace reimpl_p3 {

// Helper: mirrors MakePlaylistLockedError (simplified for tests)
static json MakePlaylistLockedError(size_t playlistIndex) {
    return {
        {"success", false},
        {"error", "Playlist is locked"},
        {"details", {{"playlist", playlistIndex}, {"isLocked", true}}}
    };
}

json PlaylistGetCount(IPlaylistService* svc, const json& /*params*/) {
    return { {"count", svc->get_playlist_count()} };
}

json PlaylistGetAll(IPlaylistService* svc, const json& /*params*/) {
    auto allPlaylists = svc->get_all_playlists();

    json playlists = json::array();
    playlists.get_ref<json::array_t&>().reserve(allPlaylists.size());
    for (const auto& pl : allPlaylists) {
        playlists.push_back({
            {"index", pl.index},
            {"name", pl.name},
            {"trackCount", pl.trackCount},
            {"isActive", pl.isActive},
            {"isPlaying", pl.isPlaying},
            {"isLocked", pl.isLocked},
            {"isAutoplaylist", pl.isAutoplaylist},
        });
    }
    return playlists;
}

json PlaylistGetActive(IPlaylistService* svc, const json& /*params*/) {
    size_t active = svc->get_active_playlist();
    if (active == SIZE_MAX) {
        return { {"success", true}, {"found", false} };
    }
    auto d = svc->get_playlist_detail(active, true);
    if (!d.found) {
        return { {"success", true}, {"found", false} };
    }
    return {
        {"index", d.index},
        {"name", d.name},
        {"trackCount", d.trackCount},
        {"isActive", d.isActive},
        {"isPlaying", d.isPlaying},
        {"isLocked", d.isLocked},
        {"duration", d.duration},
    };
}

json PlaylistSetActive(IPlaylistService* svc, const json& params) {
    size_t playlist = params.value("playlist", SIZE_MAX);
    if (playlist >= svc->get_playlist_count()) {
        return { {"success", false}, {"error", "Invalid playlist index"} };
    }
    svc->set_active_playlist(playlist);
    return { {"success", true} };
}

json PlaylistGetPlaying(IPlaylistService* svc, const json& /*params*/) {
    size_t playing = svc->get_playing_playlist();
    if (playing == SIZE_MAX) {
        return { {"success", true}, {"found", false} };
    }
    auto d = svc->get_playlist_detail(playing, true);
    if (!d.found) {
        return { {"success", true}, {"found", false} };
    }
    return {
        {"index", d.index},
        {"name", d.name},
        {"trackCount", d.trackCount},
        {"isActive", d.isActive},
        {"isPlaying", d.isPlaying},
        {"isLocked", d.isLocked},
        {"duration", d.duration},
    };
}

json PlaylistCreate(IPlaylistService* svc, const json& params) {
    std::string name = params.value("name", "New Playlist");
    size_t insertAt = params.value("position", SIZE_MAX);
    size_t newIndex = svc->create_playlist(name, insertAt);
    return {
        {"success", true},
        {"index", newIndex},
    };
}

json PlaylistRemove(IPlaylistService* svc, const json& params) {
    size_t playlist = params.value("playlist", SIZE_MAX);
    if (playlist == SIZE_MAX) {
        playlist = svc->get_active_playlist();
    }
    if (playlist >= svc->get_playlist_count()) {
        return { {"success", false}, {"error", "Invalid playlist index"} };
    }
    bool hasLock = svc->playlist_lock_is_present(playlist);
    bool result = svc->remove_playlist(playlist);
    if (!result && hasLock) {
        return MakePlaylistLockedError(playlist);
    }
    return { {"success", result} };
}

json PlaylistRename(IPlaylistService* svc, const json& params) {
    size_t playlist = params.value("playlist", SIZE_MAX);
    std::string name = params.value("name", "");
    if (playlist >= svc->get_playlist_count()) {
        return { {"success", false}, {"error", "Invalid playlist index"} };
    }
    bool result = svc->playlist_rename(playlist, name);
    return { {"success", result} };
}

json PlaylistClear(IPlaylistService* svc, const json& params) {
    size_t playlist = params.value("playlist", SIZE_MAX);
    if (playlist == SIZE_MAX) {
        playlist = svc->get_active_playlist();
    }
    if (playlist >= svc->get_playlist_count()) {
        return { {"success", false}, {"error", "Invalid playlist index"} };
    }
    if (svc->playlist_lock_is_present(playlist)) {
        return MakePlaylistLockedError(playlist);
    }
    size_t countBefore = svc->playlist_get_item_count(playlist);
    svc->playlist_undo_backup(playlist);
    svc->playlist_clear(playlist);
    size_t countAfter = svc->playlist_get_item_count(playlist);
    return {
        {"success", countAfter == 0},
        {"playlist", playlist},
        {"clearedCount", countBefore},
        {"remainingCount", countAfter}
    };
}

json PlaylistDuplicate(IPlaylistService* svc, const json& params) {
    size_t playlistIndex = params.value("playlist", SIZE_MAX);
    std::string newName = params.value("name", "");
    if (playlistIndex == SIZE_MAX) {
        playlistIndex = svc->get_active_playlist();
    }
    if (playlistIndex >= svc->get_playlist_count()) {
        return { {"success", false}, {"error", "Invalid playlist index"} };
    }
    if (newName.empty()) {
        std::string origName;
        svc->playlist_get_name(playlistIndex, origName);
        newName = origName + " (Copy)";
    }
    auto result = svc->duplicate_playlist(playlistIndex, newName);
    if (!result.success) {
        return { {"success", false}, {"error", result.error} };
    }
    return {
        {"success", true},
        {"index", result.newIndex},
        {"sourcePlaylist", result.sourceIndex},
        {"newPlaylist", result.newIndex},
        {"name", result.name},
        {"trackCount", result.trackCount}
    };
}

} // namespace reimpl_p3


// ==========================================================================
// Test fixture
// ==========================================================================
class PlaylistHandlerTest : public ::testing::Test {
protected:
    MockPlaylistService mock;

    void SetUp() override {
        mock.Reset();
    }

    void AddPlaylists(std::initializer_list<std::pair<std::string, size_t>> items) {
        for (const auto& [name, count] : items) {
            mock.playlists.push_back({name, count, false, false, ""});
        }
    }
};


// ==========================================================================
// PlaylistGetCount tests
// ==========================================================================
TEST_F(PlaylistHandlerTest, GetCount_Empty) {
    auto r = reimpl_p3::PlaylistGetCount(&mock, {});
    EXPECT_EQ(r["count"], 0);
}

TEST_F(PlaylistHandlerTest, GetCount_Multiple) {
    AddPlaylists({{"A", 10}, {"B", 20}, {"C", 5}});
    auto r = reimpl_p3::PlaylistGetCount(&mock, {});
    EXPECT_EQ(r["count"], 3);
}


// ==========================================================================
// PlaylistGetAll tests
// ==========================================================================
TEST_F(PlaylistHandlerTest, GetAll_Empty) {
    auto r = reimpl_p3::PlaylistGetAll(&mock, {});
    EXPECT_TRUE(r.is_array());
    EXPECT_EQ(r.size(), 0u);
}

TEST_F(PlaylistHandlerTest, GetAll_MultipleWithFlags) {
    AddPlaylists({{"Default", 10}, {"Rock", 20}, {"Auto", 5}});
    mock.activePlaylist = 0;
    mock.playingPlaylist = 1;
    mock.playlists[2].isAutoplaylist = true;
    mock.playlists[1].isLocked = true;

    auto r = reimpl_p3::PlaylistGetAll(&mock, {});
    ASSERT_EQ(r.size(), 3u);

    EXPECT_EQ(r[0]["name"], "Default");
    EXPECT_EQ(r[0]["trackCount"], 10);
    EXPECT_TRUE(r[0]["isActive"].get<bool>());
    EXPECT_FALSE(r[0]["isPlaying"].get<bool>());

    EXPECT_EQ(r[1]["name"], "Rock");
    EXPECT_TRUE(r[1]["isPlaying"].get<bool>());
    EXPECT_TRUE(r[1]["isLocked"].get<bool>());

    EXPECT_TRUE(r[2]["isAutoplaylist"].get<bool>());
}


// ==========================================================================
// PlaylistGetActive tests
// ==========================================================================
TEST_F(PlaylistHandlerTest, GetActive_NoActive) {
    mock.activePlaylist = SIZE_MAX;
    auto r = reimpl_p3::PlaylistGetActive(&mock, {});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_FALSE(r["found"].get<bool>());
}

TEST_F(PlaylistHandlerTest, GetActive_Valid) {
    AddPlaylists({{"Default", 10}, {"Rock", 20}});
    mock.activePlaylist = 1;
    mock.detailDuration = 123.5;

    auto r = reimpl_p3::PlaylistGetActive(&mock, {});
    EXPECT_EQ(r["index"], 1);
    EXPECT_EQ(r["name"], "Rock");
    EXPECT_EQ(r["trackCount"], 20);
    EXPECT_DOUBLE_EQ(r["duration"].get<double>(), 123.5);
}


// ==========================================================================
// PlaylistSetActive tests
// ==========================================================================
TEST_F(PlaylistHandlerTest, SetActive_Valid) {
    AddPlaylists({{"A", 0}, {"B", 0}});
    auto r = reimpl_p3::PlaylistSetActive(&mock, {{"playlist", 1}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.lastSetActiveIndex, 1u);
}

TEST_F(PlaylistHandlerTest, SetActive_InvalidIndex) {
    AddPlaylists({{"A", 0}});
    auto r = reimpl_p3::PlaylistSetActive(&mock, {{"playlist", 5}});
    EXPECT_FALSE(r["success"].get<bool>());
    EXPECT_EQ(r["error"], "Invalid playlist index");
}


// ==========================================================================
// PlaylistGetPlaying tests
// ==========================================================================
TEST_F(PlaylistHandlerTest, GetPlaying_NoPlaying) {
    mock.playingPlaylist = SIZE_MAX;
    auto r = reimpl_p3::PlaylistGetPlaying(&mock, {});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_FALSE(r["found"].get<bool>());
}

TEST_F(PlaylistHandlerTest, GetPlaying_Valid) {
    AddPlaylists({{"Default", 10}});
    mock.playingPlaylist = 0;
    mock.detailDuration = 300.0;

    auto r = reimpl_p3::PlaylistGetPlaying(&mock, {});
    EXPECT_EQ(r["index"], 0);
    EXPECT_EQ(r["name"], "Default");
    EXPECT_TRUE(r["isPlaying"].get<bool>());
}


// ==========================================================================
// PlaylistCreate tests
// ==========================================================================
TEST_F(PlaylistHandlerTest, Create_Default) {
    mock.nextCreateIndex = 0;
    auto r = reimpl_p3::PlaylistCreate(&mock, json::object());
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(r["index"], 0);
    EXPECT_EQ(mock.lastCreateName, "New Playlist");
}

TEST_F(PlaylistHandlerTest, Create_CustomNameAndPosition) {
    mock.nextCreateIndex = 2;
    auto r = reimpl_p3::PlaylistCreate(&mock, {{"name", "My List"}, {"position", 1}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(r["index"], 2);
    EXPECT_EQ(mock.lastCreateName, "My List");
    EXPECT_EQ(mock.lastCreatePosition, 1u);
}


// ==========================================================================
// PlaylistRemove tests
// ==========================================================================
TEST_F(PlaylistHandlerTest, Remove_Valid) {
    AddPlaylists({{"A", 0}, {"B", 0}});
    auto r = reimpl_p3::PlaylistRemove(&mock, {{"playlist", 0}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.lastRemoveIndex, 0u);
}

TEST_F(PlaylistHandlerTest, Remove_DefaultsToActive) {
    AddPlaylists({{"A", 0}, {"B", 0}});
    mock.activePlaylist = 1;
    auto r = reimpl_p3::PlaylistRemove(&mock, json::object());
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.lastRemoveIndex, 1u);
}

TEST_F(PlaylistHandlerTest, Remove_InvalidIndex) {
    AddPlaylists({{"A", 0}});
    auto r = reimpl_p3::PlaylistRemove(&mock, {{"playlist", 5}});
    EXPECT_FALSE(r["success"].get<bool>());
}

TEST_F(PlaylistHandlerTest, Remove_LockedFailure) {
    AddPlaylists({{"Locked", 0}});
    mock.playlists[0].isLocked = true;
    mock.removeResult = false;
    auto r = reimpl_p3::PlaylistRemove(&mock, {{"playlist", 0}});
    EXPECT_FALSE(r["success"].get<bool>());
    // Locked error should contain details
    EXPECT_TRUE(r.contains("error") || r.contains("details"));
}


// ==========================================================================
// PlaylistRename tests
// ==========================================================================
TEST_F(PlaylistHandlerTest, Rename_Valid) {
    AddPlaylists({{"Old", 0}});
    auto r = reimpl_p3::PlaylistRename(&mock, {{"playlist", 0}, {"name", "New"}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.lastRenameName, "New");
}

TEST_F(PlaylistHandlerTest, Rename_InvalidIndex) {
    auto r = reimpl_p3::PlaylistRename(&mock, {{"playlist", 0}, {"name", "X"}});
    EXPECT_FALSE(r["success"].get<bool>());
}


// ==========================================================================
// PlaylistClear tests
// ==========================================================================
TEST_F(PlaylistHandlerTest, Clear_Valid) {
    AddPlaylists({{"A", 15}});
    auto r = reimpl_p3::PlaylistClear(&mock, {{"playlist", 0}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(r["clearedCount"], 15);
    EXPECT_EQ(r["remainingCount"], 0);
    EXPECT_EQ(mock.undoBackupCallCount, 1);
    EXPECT_EQ(mock.clearCallCount, 1);
}

TEST_F(PlaylistHandlerTest, Clear_DefaultsToActive) {
    AddPlaylists({{"A", 5}, {"B", 10}});
    mock.activePlaylist = 1;
    auto r = reimpl_p3::PlaylistClear(&mock, json::object());
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(r["playlist"], 1);
    EXPECT_EQ(mock.lastClearIndex, 1u);
}

TEST_F(PlaylistHandlerTest, Clear_Locked) {
    AddPlaylists({{"A", 5}});
    mock.playlists[0].isLocked = true;
    auto r = reimpl_p3::PlaylistClear(&mock, {{"playlist", 0}});
    EXPECT_FALSE(r["success"].get<bool>());
    EXPECT_EQ(mock.clearCallCount, 0);
}

TEST_F(PlaylistHandlerTest, Clear_InvalidIndex) {
    auto r = reimpl_p3::PlaylistClear(&mock, {{"playlist", 99}});
    EXPECT_FALSE(r["success"].get<bool>());
}


// ==========================================================================
// PlaylistDuplicate tests
// ==========================================================================
TEST_F(PlaylistHandlerTest, Duplicate_Success) {
    AddPlaylists({{"Original", 10}});
    mock.duplicateResult = {true, "", 1, 0, "Original (Copy)", 10};

    auto r = reimpl_p3::PlaylistDuplicate(&mock, {{"playlist", 0}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(r["name"], "Original (Copy)");
    EXPECT_EQ(r["trackCount"], 10);
    EXPECT_EQ(r["sourcePlaylist"], 0);
    EXPECT_EQ(mock.duplicateCallCount, 1);
}

TEST_F(PlaylistHandlerTest, Duplicate_CustomName) {
    AddPlaylists({{"Original", 5}});
    mock.duplicateResult = {true, "", 1, 0, "Custom", 5};

    auto r = reimpl_p3::PlaylistDuplicate(&mock, {{"playlist", 0}, {"name", "Custom"}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.lastDuplicateName, "Custom");
}

TEST_F(PlaylistHandlerTest, Duplicate_DefaultsToActive) {
    AddPlaylists({{"A", 3}, {"B", 7}});
    mock.activePlaylist = 1;
    mock.duplicateResult = {true, "", 2, 1, "B (Copy)", 7};

    auto r = reimpl_p3::PlaylistDuplicate(&mock, json::object());
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.lastDuplicateSource, 1u);
}

TEST_F(PlaylistHandlerTest, Duplicate_InvalidIndex) {
    AddPlaylists({{"A", 0}});
    auto r = reimpl_p3::PlaylistDuplicate(&mock, {{"playlist", 5}});
    EXPECT_FALSE(r["success"].get<bool>());
    EXPECT_EQ(mock.duplicateCallCount, 0);
}

TEST_F(PlaylistHandlerTest, Duplicate_ServiceFailure) {
    AddPlaylists({{"A", 0}});
    mock.duplicateResult = {false, "Failed to create playlist", 0, 0, "", 0};

    auto r = reimpl_p3::PlaylistDuplicate(&mock, {{"playlist", 0}});
    EXPECT_FALSE(r["success"].get<bool>());
    EXPECT_EQ(r["error"], "Failed to create playlist");
}

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
    size_t playlistIndex = params.contains("playlist") ? params.value("playlist", SIZE_MAX)
                                                        : params.value("index", SIZE_MAX);
    size_t start = params.value("start", static_cast<size_t>(0));
    size_t count = params.value("count", static_cast<size_t>(100));
    auto extraFormats = params.value("formats", json::object());
    if (playlistIndex == SIZE_MAX) playlistIndex = svc->get_active_playlist();
    if (playlistIndex >= svc->get_playlist_count())
        return { {"playlist", playlistIndex}, {"start", start}, {"count", 0}, {"total", 0}, {"tracks", json::array()} };
    return svc->get_tracks_json(playlistIndex, start, count, extraFormats);
}

// 16. PlaylistGetSelectedTracks
json PlaylistGetSelectedTracks(IPlaylistService* svc, const json& params) {
    size_t playlistIndex = params.contains("playlist") ? params.value("playlist", SIZE_MAX)
                                                        : params.value("index", SIZE_MAX);
    if (playlistIndex == SIZE_MAX) playlistIndex = svc->get_active_playlist();
    if (playlistIndex >= svc->get_playlist_count())
        return { {"success", false}, {"error", "Invalid playlist index"}, {"tracks", json::array()} };
    return svc->get_selected_tracks_json(playlistIndex);
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
    mock.tracksJsonResult = {
        {"playlist", 0}, {"start", 5}, {"count", 3}, {"total", 20},
        {"tracks", json::array({json::object({{"index", 5}}), json::object({{"index", 6}}), json::object({{"index", 7}})})}
    };
    auto r = reimpl_p4::PlaylistGetTracks(&mock, {{"playlist", 0}, {"start", 5}, {"count", 3}});
    EXPECT_EQ(r["count"], 3);
    EXPECT_EQ(r["total"], 20);
    EXPECT_EQ(mock.getTracksJsonCallCount, 1);
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
    mock.tracksJsonResult = {
        {"playlist", 0}, {"start", 0}, {"count", 10}, {"total", 10}, {"tracks", json::array()}
    };
    auto r = reimpl_p4::PlaylistGetTracks(&mock, json::object());
    EXPECT_EQ(mock.getTracksJsonCallCount, 1);
    EXPECT_EQ(mock.lastGetTracksStart, 0u);
    EXPECT_EQ(mock.lastGetTracksCount, 100u);  // default
}

// ==========================================================================
// P4b: PlaylistGetSelectedTracks tests
// ==========================================================================
TEST_F(PlaylistHandlerP4Test, GetSelectedTracks_Valid) {
    SetUpP4();
    mock.selectedTracksJsonResult = {
        {"success", true}, {"playlist", 0}, {"tracks", json::array()}, {"count", 0}
    };
    auto r = reimpl_p4::PlaylistGetSelectedTracks(&mock, {{"playlist", 0}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.getSelectedTracksJsonCallCount, 1);
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

// ==========================================================================
// P5 reimplemented handler functions (Undo/Redo/Sort/Shuffle/Lock/Autoplaylist/Reorder)
// ==========================================================================
namespace reimpl_p5 {

static json MakePlaylistLockedError(size_t playlistIndex) {
    return {
        {"success", false},
        {"error", "Playlist is locked"},
        {"details", {{"playlist", playlistIndex}, {"isLocked", true}}}
    };
}

// Helper: mirrors GetPlaylistIndexFromParams
static size_t GetPlaylistIndexFromParams(const json& params) {
    if (params.contains("playlist")) return params.value("playlist", SIZE_MAX);
    return params.value("index", SIZE_MAX);
}

// 1. PlaylistUndo
json PlaylistUndo(IPlaylistService* svc, const json& params) {
    size_t idx = params.value("playlist", SIZE_MAX);
    if (idx == SIZE_MAX) idx = svc->get_active_playlist();
    if (idx >= svc->get_playlist_count())
        return { {"success", false} };
    return { {"success", svc->undo_restore(idx)} };
}

// 2. PlaylistRedo
json PlaylistRedo(IPlaylistService* svc, const json& params) {
    size_t idx = params.value("playlist", SIZE_MAX);
    if (idx == SIZE_MAX) idx = svc->get_active_playlist();
    if (idx >= svc->get_playlist_count())
        return { {"success", false} };
    return { {"success", svc->redo_restore(idx)} };
}

// 3. PlaylistSort
json PlaylistSort(IPlaylistService* svc, const json& params) {
    size_t idx = GetPlaylistIndexFromParams(params);
    if (idx == SIZE_MAX) idx = svc->get_active_playlist();
    if (idx >= svc->get_playlist_count())
        return { {"success", false}, {"error", "Invalid playlist index"} };
    if (svc->playlist_lock_is_present(idx))
        return MakePlaylistLockedError(idx);

    std::string pattern = params.value("pattern", "%title%");
    bool descending = params.value("descending", false);
    bool selectedOnly = params.value("selectedOnly", false);

    svc->playlist_undo_backup(idx);
    svc->sort_by_format(idx, pattern.c_str(), selectedOnly);

    if (descending) {
        size_t count = svc->playlist_get_item_count(idx);
        if (count > 1) {
            std::vector<size_t> order(count);
            for (size_t i = 0; i < count; i++) order[i] = count - 1 - i;
            svc->reorder_items(idx, order.data(), count);
        }
    }

    return { {"success", true} };
}

// 4. PlaylistShuffle
json PlaylistShuffle(IPlaylistService* svc, const json& params) {
    size_t idx = GetPlaylistIndexFromParams(params);
    if (idx == SIZE_MAX) idx = svc->get_active_playlist();
    if (idx >= svc->get_playlist_count())
        return { {"success", false}, {"error", "Invalid playlist index"} };
    if (svc->playlist_lock_is_present(idx))
        return MakePlaylistLockedError(idx);

    size_t count = svc->playlist_get_item_count(idx);
    if (count <= 1) return { {"success", true} };

    svc->playlist_undo_backup(idx);
    std::vector<size_t> order(count);
    for (size_t i = 0; i < count; i++) order[i] = i;
    std::random_device rd;
    std::mt19937 gen(rd());
    for (size_t i = count - 1; i > 0; i--) {
        std::uniform_int_distribution<size_t> dist(0, i);
        std::swap(order[i], order[dist(gen)]);
    }
    svc->reorder_items(idx, order.data(), count);
    return { {"success", true} };
}

// 5. PlaylistGetLockInfo
json PlaylistGetLockInfo(IPlaylistService* svc, const json& params) {
    size_t idx = params.value("playlist", SIZE_MAX);
    if (idx == SIZE_MAX) idx = svc->get_active_playlist();
    if (idx >= svc->get_playlist_count())
        return { {"playlist", idx}, {"isLocked", false} };
    return { {"playlist", idx}, {"isLocked", svc->playlist_lock_is_present(idx)} };
}

// 6. PlaylistIsLocked
json PlaylistIsLocked(IPlaylistService* svc, const json& params) {
    size_t idx = params.value("playlist", SIZE_MAX);
    if (idx == SIZE_MAX) idx = svc->get_active_playlist();
    if (idx >= svc->get_playlist_count())
        return { {"success", false}, {"isLocked", false}, {"error", "Invalid playlist"} };
    return { {"success", true}, {"isLocked", svc->playlist_lock_is_present(idx)} };
}

// 7. PlaylistReverse
json PlaylistReverse(IPlaylistService* svc, const json& params) {
    size_t idx = params.value("playlist", SIZE_MAX);
    if (idx == SIZE_MAX) idx = svc->get_active_playlist();
    if (idx >= svc->get_playlist_count())
        return { {"success", false}, {"error", "Invalid playlist index"} };
    if (svc->playlist_lock_is_present(idx))
        return MakePlaylistLockedError(idx);
    size_t count = svc->playlist_get_item_count(idx);
    if (count < 2) return { {"success", true} };
    svc->playlist_undo_backup(idx);
    std::vector<size_t> order(count);
    for (size_t i = 0; i < count; i++) order[i] = count - 1 - i;
    svc->reorder_items(idx, order.data(), count);
    return { {"success", true} };
}

// 8. PlaylistReorder
json PlaylistReorder(IPlaylistService* svc, const json& params) {
    size_t idx = params.value("playlist", SIZE_MAX);
    if (idx == SIZE_MAX) idx = svc->get_active_playlist();
    if (idx >= svc->get_playlist_count())
        return { {"success", false}, {"error", "Invalid playlist index"} };
    if (svc->playlist_lock_is_present(idx))
        return MakePlaylistLockedError(idx);

    auto newOrder = params.value("newOrder", json::array());
    size_t count = svc->playlist_get_item_count(idx);
    if (newOrder.size() != count)
        return { {"success", false}, {"error", "newOrder length mismatch"} };

    std::vector<size_t> order(count);
    for (size_t i = 0; i < count; i++) {
        if (!newOrder[i].is_number())
            return { {"success", false}, {"error", "newOrder must contain numbers"} };
        size_t v = newOrder[i].get<size_t>();
        if (v >= count)
            return { {"success", false}, {"error", "Index out of range"} };
        order[i] = v;
    }

    svc->playlist_undo_backup(idx);
    svc->reorder_items(idx, order.data(), count);
    return { {"success", true}, {"playlist", idx}, {"itemCount", count} };
}

// 9. PlaylistIsAutoplaylist
json PlaylistIsAutoplaylist(IPlaylistService* svc, const json& params) {
    size_t idx = params.value("playlist", SIZE_MAX);
    if (idx == SIZE_MAX) idx = svc->get_active_playlist();
    if (idx >= svc->get_playlist_count())
        return { {"playlist", idx}, {"isAutoplaylist", false} };

    auto det = svc->detect_autoplaylist(idx);
    json r = { {"playlist", idx}, {"isAutoplaylist", det.isAutoplaylist} };
    if (!det.lockName.empty()) r["lockName"] = det.lockName;
    return r;
}

// 10. PlaylistCreateAutoplaylist
json PlaylistCreateAutoplaylist(IPlaylistService* svc, const json& params) {
    std::string name = params.value("name", "New Autoplaylist");
    std::string query = params.value("query", "");
    std::string sort = params.value("sort", "");
    bool keepSorted = params.value("keepSorted", false);

    if (query.empty())
        return { {"success", false}, {"error", "Query string is required"} };

    uint32_t flags = keepSorted ? 1 : 0;  // autoplaylist_flag_sort == 1
    size_t idx = svc->create_playlist(name, SIZE_MAX);
    if (idx == SIZE_MAX)
        return { {"success", false}, {"error", "Failed to create playlist"} };

    try {
        svc->add_autoplaylist_client(idx, query.c_str(), sort.c_str(), flags);
    } catch (...) {
        svc->remove_playlist(idx);
        return { {"success", false}, {"error", "Failed to configure autoplaylist"} };
    }

    return {
        {"success", true}, {"index", idx}, {"playlist", idx},
        {"name", name}, {"query", query}
    };
}

// 11. PlaylistConvertToAutoplaylist
json PlaylistConvertToAutoplaylist(IPlaylistService* svc, const json& params) {
    size_t idx = params.value("playlist", SIZE_MAX);
    if (idx == SIZE_MAX) idx = svc->get_active_playlist();
    if (idx >= svc->get_playlist_count())
        return { {"success", false}, {"error", "Invalid playlist index"} };

    std::string query = params.value("query", "");
    std::string sort = params.value("sort", "");
    bool keepSorted = params.value("keepSorted", false);
    if (query.empty())
        return { {"success", false}, {"error", "Query string is required"} };

    uint32_t flags = keepSorted ? 1 : 0;
    try {
        svc->add_autoplaylist_client(idx, query.c_str(), sort.c_str(), flags);
    } catch (...) {
        return { {"success", false}, {"error", "Failed to convert to autoplaylist"} };
    }
    return { {"success", true}, {"playlist", idx} };
}

// 12. PlaylistRemoveAutoplaylist
json PlaylistRemoveAutoplaylist(IPlaylistService* svc, const json& params) {
    size_t idx = params.value("playlist", SIZE_MAX);
    if (idx == SIZE_MAX) idx = svc->get_active_playlist();
    if (idx >= svc->get_playlist_count())
        return { {"success", false}, {"error", "Invalid playlist index"} };

    if (svc->autoplaylist_is_client_present(idx)) {
        svc->remove_autoplaylist_client(idx);
        return { {"success", true}, {"playlist", idx}, {"source", "sdk"} };
    }

    auto det = svc->detect_autoplaylist(idx);
    if (det.isDuiAutoplaylist)
        return {
            {"success", true}, {"playlist", idx}, {"source", "dui"},
            {"note", "DUI autoplaylist lock detected; proceed with playlist.remove to delete playlist"}
        };

    return { {"success", false}, {"error", "Not an autoplaylist"} };
}

// 13. PlaylistGetAutoplaylistInfo
json PlaylistGetAutoplaylistInfo(IPlaylistService* svc, const json& params) {
    size_t idx = params.value("playlist", SIZE_MAX);
    if (idx == SIZE_MAX) idx = svc->get_active_playlist();
    if (idx >= svc->get_playlist_count())
        return { {"success", false}, {"error", "Invalid playlist index"} };

    auto det = svc->detect_autoplaylist(idx);
    if (!det.isAutoplaylist)
        return { {"isAutoplaylist", false}, {"playlist", idx} };

    uint32_t flags = 0;
    if (!det.isDuiAutoplaylist)
        flags = svc->get_autoplaylist_flags(idx);

    json r = {
        {"isAutoplaylist", true}, {"playlist", idx},
        {"keepSorted", (flags & 1) != 0},
        {"source", det.isDuiAutoplaylist ? "dui" : "sdk"}
    };
    if (!det.lockName.empty()) r["lockName"] = det.lockName;
    return r;
}

// 14. PlaylistGetAutoplaylistQuery
json PlaylistGetAutoplaylistQuery(IPlaylistService* svc, const json& params) {
    size_t idx = params.value("playlist", SIZE_MAX);
    if (idx == SIZE_MAX) idx = svc->get_active_playlist();
    if (idx >= svc->get_playlist_count())
        return { {"success", false}, {"error", "Invalid playlist index"} };

    auto det = svc->detect_autoplaylist(idx);
    if (!det.isAutoplaylist)
        return { {"isAutoplaylist", false}, {"playlist", idx}, {"query", nullptr} };

    uint32_t flags = 0;
    if (!det.isDuiAutoplaylist)
        flags = svc->get_autoplaylist_flags(idx);

    json r = {
        {"isAutoplaylist", true}, {"playlist", idx}, {"query", nullptr},
        {"keepSorted", (flags & 1) != 0},
        {"source", det.isDuiAutoplaylist ? "dui" : "sdk"},
        {"note", "Query string not exposed by SDK"}
    };
    if (!det.lockName.empty()) r["lockName"] = det.lockName;
    return r;
}

// 15. PlaylistReorderPlaylists
json PlaylistReorderPlaylists(IPlaylistService* svc, const json& params) {
    auto newOrder = params.value("newOrder", json::array());
    size_t count = svc->get_playlist_count();

    if (newOrder.size() != count)
        return { {"success", false}, {"error", "newOrder length mismatch"},
                 {"expected", count}, {"got", newOrder.size()} };

    std::vector<size_t> order(count);
    for (size_t i = 0; i < count; i++) {
        if (!newOrder[i].is_number())
            return { {"success", false}, {"error", "newOrder must contain numbers"} };
        size_t v = newOrder[i].get<size_t>();
        if (v >= count)
            return { {"success", false}, {"error", "Index out of range"}, {"index", v} };
        order[i] = v;
    }

    bool ok = svc->reorder_playlists(order.data(), count);
    return { {"success", ok}, {"count", count} };
}

} // namespace reimpl_p5

// ==========================================================================
// P5 Test Fixture
// ==========================================================================

class PlaylistHandlerP5Test : public ::testing::Test {
protected:
    MockPlaylistService mock;

    void SetUpBasic() {
        mock.Reset();
        mock.playlists = {
            {"Default", 10, false, false},
            {"Rock", 5, false, false}
        };
    }

    void SetUpLocked() {
        SetUpBasic();
        mock.playlists[0].isLocked = true;
    }
};

// ==========================================================================
// P5 Tests: Undo / Redo
// ==========================================================================

TEST_F(PlaylistHandlerP5Test, Undo_Valid) {
    SetUpBasic();
    mock.undoRestoreResult = true;
    auto r = reimpl_p5::PlaylistUndo(&mock, {{"playlist", 0}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.undoRestoreCallCount, 1);
    EXPECT_EQ(mock.lastUndoRestorePlaylist, 0u);
}

TEST_F(PlaylistHandlerP5Test, Undo_Failure) {
    SetUpBasic();
    mock.undoRestoreResult = false;
    auto r = reimpl_p5::PlaylistUndo(&mock, {{"playlist", 0}});
    EXPECT_FALSE(r["success"].get<bool>());
}

TEST_F(PlaylistHandlerP5Test, Undo_DefaultActive) {
    SetUpBasic();
    auto r = reimpl_p5::PlaylistUndo(&mock, json::object());
    EXPECT_EQ(mock.lastUndoRestorePlaylist, 0u);
}

TEST_F(PlaylistHandlerP5Test, Undo_InvalidPlaylist) {
    SetUpBasic();
    auto r = reimpl_p5::PlaylistUndo(&mock, {{"playlist", 99}});
    EXPECT_FALSE(r["success"].get<bool>());
    EXPECT_EQ(mock.undoRestoreCallCount, 0);
}

TEST_F(PlaylistHandlerP5Test, Redo_Valid) {
    SetUpBasic();
    mock.redoRestoreResult = true;
    auto r = reimpl_p5::PlaylistRedo(&mock, {{"playlist", 0}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.redoRestoreCallCount, 1);
}

TEST_F(PlaylistHandlerP5Test, Redo_Failure) {
    SetUpBasic();
    mock.redoRestoreResult = false;
    auto r = reimpl_p5::PlaylistRedo(&mock, {{"playlist", 0}});
    EXPECT_FALSE(r["success"].get<bool>());
}

// ==========================================================================
// P5 Tests: Sort
// ==========================================================================

TEST_F(PlaylistHandlerP5Test, Sort_Default) {
    SetUpBasic();
    auto r = reimpl_p5::PlaylistSort(&mock, json::object());
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.sortByFormatCallCount, 1);
    EXPECT_EQ(mock.lastSortPlaylist, 0u);
    EXPECT_EQ(mock.lastSortPattern, "%title%");
    EXPECT_FALSE(mock.lastSortSelectedOnly);
}

TEST_F(PlaylistHandlerP5Test, Sort_CustomPattern) {
    SetUpBasic();
    auto r = reimpl_p5::PlaylistSort(&mock, {{"playlist", 1}, {"pattern", "%artist%"}, {"selectedOnly", true}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.lastSortPlaylist, 1u);
    EXPECT_EQ(mock.lastSortPattern, "%artist%");
    EXPECT_TRUE(mock.lastSortSelectedOnly);
}

TEST_F(PlaylistHandlerP5Test, Sort_Descending) {
    SetUpBasic();
    auto r = reimpl_p5::PlaylistSort(&mock, {{"playlist", 0}, {"descending", true}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.sortByFormatCallCount, 1);
    EXPECT_EQ(mock.reorderItemsCallCount, 1);
    // Verify reverse order: 10 items -> [9,8,7,6,5,4,3,2,1,0]
    ASSERT_EQ(mock.lastReorderItemsOrder.size(), 10u);
    EXPECT_EQ(mock.lastReorderItemsOrder[0], 9u);
    EXPECT_EQ(mock.lastReorderItemsOrder[9], 0u);
}

TEST_F(PlaylistHandlerP5Test, Sort_Locked) {
    SetUpLocked();
    auto r = reimpl_p5::PlaylistSort(&mock, {{"playlist", 0}});
    EXPECT_FALSE(r["success"].get<bool>());
    EXPECT_EQ(r["error"], "Playlist is locked");
    EXPECT_EQ(mock.sortByFormatCallCount, 0);
}

TEST_F(PlaylistHandlerP5Test, Sort_InvalidPlaylist) {
    SetUpBasic();
    auto r = reimpl_p5::PlaylistSort(&mock, {{"playlist", 99}});
    EXPECT_FALSE(r["success"].get<bool>());
}

TEST_F(PlaylistHandlerP5Test, Sort_IndexKey) {
    SetUpBasic();
    auto r = reimpl_p5::PlaylistSort(&mock, {{"index", 1}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.lastSortPlaylist, 1u);
}

// ==========================================================================
// P5 Tests: Shuffle
// ==========================================================================

TEST_F(PlaylistHandlerP5Test, Shuffle_Valid) {
    SetUpBasic();
    auto r = reimpl_p5::PlaylistShuffle(&mock, {{"playlist", 0}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.reorderItemsCallCount, 1);
    ASSERT_EQ(mock.lastReorderItemsOrder.size(), 10u);
}

TEST_F(PlaylistHandlerP5Test, Shuffle_SingleItem) {
    SetUpBasic();
    mock.playlists[0].trackCount = 1;
    auto r = reimpl_p5::PlaylistShuffle(&mock, {{"playlist", 0}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.reorderItemsCallCount, 0);
}

TEST_F(PlaylistHandlerP5Test, Shuffle_EmptyPlaylist) {
    SetUpBasic();
    mock.playlists[0].trackCount = 0;
    auto r = reimpl_p5::PlaylistShuffle(&mock, {{"playlist", 0}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.reorderItemsCallCount, 0);
}

TEST_F(PlaylistHandlerP5Test, Shuffle_Locked) {
    SetUpLocked();
    auto r = reimpl_p5::PlaylistShuffle(&mock, {{"playlist", 0}});
    EXPECT_FALSE(r["success"].get<bool>());
    EXPECT_EQ(mock.reorderItemsCallCount, 0);
}

// ==========================================================================
// P5 Tests: GetLockInfo / IsLocked
// ==========================================================================

TEST_F(PlaylistHandlerP5Test, GetLockInfo_Unlocked) {
    SetUpBasic();
    auto r = reimpl_p5::PlaylistGetLockInfo(&mock, {{"playlist", 0}});
    EXPECT_EQ(r["playlist"], 0u);
    EXPECT_FALSE(r["isLocked"].get<bool>());
}

TEST_F(PlaylistHandlerP5Test, GetLockInfo_Locked) {
    SetUpLocked();
    auto r = reimpl_p5::PlaylistGetLockInfo(&mock, {{"playlist", 0}});
    EXPECT_TRUE(r["isLocked"].get<bool>());
}

TEST_F(PlaylistHandlerP5Test, GetLockInfo_DefaultActive) {
    SetUpBasic();
    auto r = reimpl_p5::PlaylistGetLockInfo(&mock, json::object());
    EXPECT_EQ(r["playlist"], 0u);
}

TEST_F(PlaylistHandlerP5Test, IsLocked_Valid) {
    SetUpBasic();
    auto r = reimpl_p5::PlaylistIsLocked(&mock, {{"playlist", 0}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_FALSE(r["isLocked"].get<bool>());
}

TEST_F(PlaylistHandlerP5Test, IsLocked_InvalidPlaylist) {
    SetUpBasic();
    auto r = reimpl_p5::PlaylistIsLocked(&mock, {{"playlist", 99}});
    EXPECT_FALSE(r["success"].get<bool>());
}

// ==========================================================================
// P5 Tests: Reverse
// ==========================================================================

TEST_F(PlaylistHandlerP5Test, Reverse_Valid) {
    SetUpBasic();
    auto r = reimpl_p5::PlaylistReverse(&mock, {{"playlist", 0}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.reorderItemsCallCount, 1);
    // 10 items: [9,8,7,...,0]
    ASSERT_EQ(mock.lastReorderItemsOrder.size(), 10u);
    EXPECT_EQ(mock.lastReorderItemsOrder[0], 9u);
    EXPECT_EQ(mock.lastReorderItemsOrder[9], 0u);
}

TEST_F(PlaylistHandlerP5Test, Reverse_SingleItem) {
    SetUpBasic();
    mock.playlists[0].trackCount = 1;
    auto r = reimpl_p5::PlaylistReverse(&mock, {{"playlist", 0}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.reorderItemsCallCount, 0);
}

TEST_F(PlaylistHandlerP5Test, Reverse_Locked) {
    SetUpLocked();
    auto r = reimpl_p5::PlaylistReverse(&mock, {{"playlist", 0}});
    EXPECT_FALSE(r["success"].get<bool>());
    EXPECT_EQ(mock.reorderItemsCallCount, 0);
}

// ==========================================================================
// P5 Tests: Reorder (track-level)
// ==========================================================================

TEST_F(PlaylistHandlerP5Test, Reorder_Valid) {
    SetUpBasic();
    mock.playlists[0].trackCount = 3;
    auto r = reimpl_p5::PlaylistReorder(&mock, {{"playlist", 0}, {"newOrder", {2, 0, 1}}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(r["playlist"], 0u);
    EXPECT_EQ(r["itemCount"], 3u);
    ASSERT_EQ(mock.lastReorderItemsOrder, (std::vector<size_t>{2, 0, 1}));
}

TEST_F(PlaylistHandlerP5Test, Reorder_LengthMismatch) {
    SetUpBasic();
    mock.playlists[0].trackCount = 3;
    auto r = reimpl_p5::PlaylistReorder(&mock, {{"playlist", 0}, {"newOrder", {0, 1}}});
    EXPECT_FALSE(r["success"].get<bool>());
    EXPECT_EQ(mock.reorderItemsCallCount, 0);
}

TEST_F(PlaylistHandlerP5Test, Reorder_IndexOutOfRange) {
    SetUpBasic();
    mock.playlists[0].trackCount = 3;
    auto r = reimpl_p5::PlaylistReorder(&mock, {{"playlist", 0}, {"newOrder", {0, 1, 99}}});
    EXPECT_FALSE(r["success"].get<bool>());
}

TEST_F(PlaylistHandlerP5Test, Reorder_Locked) {
    SetUpLocked();
    auto r = reimpl_p5::PlaylistReorder(&mock, {{"playlist", 0}, {"newOrder", {0}}});
    EXPECT_FALSE(r["success"].get<bool>());
}

// ==========================================================================
// P5 Tests: IsAutoplaylist
// ==========================================================================

TEST_F(PlaylistHandlerP5Test, IsAutoplaylist_False) {
    SetUpBasic();
    mock.detectAutoplaylistResult = {false, false, ""};
    auto r = reimpl_p5::PlaylistIsAutoplaylist(&mock, {{"playlist", 0}});
    EXPECT_FALSE(r["isAutoplaylist"].get<bool>());
}

TEST_F(PlaylistHandlerP5Test, IsAutoplaylist_SdkTrue) {
    SetUpBasic();
    mock.detectAutoplaylistResult = {true, false, ""};
    auto r = reimpl_p5::PlaylistIsAutoplaylist(&mock, {{"playlist", 0}});
    EXPECT_TRUE(r["isAutoplaylist"].get<bool>());
}

TEST_F(PlaylistHandlerP5Test, IsAutoplaylist_DuiWithLock) {
    SetUpBasic();
    mock.detectAutoplaylistResult = {true, true, "Autoplaylist"};
    auto r = reimpl_p5::PlaylistIsAutoplaylist(&mock, {{"playlist", 0}});
    EXPECT_TRUE(r["isAutoplaylist"].get<bool>());
    EXPECT_EQ(r["lockName"], "Autoplaylist");
}

TEST_F(PlaylistHandlerP5Test, IsAutoplaylist_DefaultActive) {
    SetUpBasic();
    mock.detectAutoplaylistResult = {false, false, ""};
    auto r = reimpl_p5::PlaylistIsAutoplaylist(&mock, json::object());
    EXPECT_EQ(mock.lastDetectPlaylist, 0u);
}

// ==========================================================================
// P5 Tests: CreateAutoplaylist
// ==========================================================================

TEST_F(PlaylistHandlerP5Test, CreateAutoplaylist_Valid) {
    SetUpBasic();
    auto r = reimpl_p5::PlaylistCreateAutoplaylist(&mock, {
        {"name", "My Auto"}, {"query", "artist IS test"}, {"sort", "%title%"}, {"keepSorted", true}
    });
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(r["name"], "My Auto");
    EXPECT_EQ(r["query"], "artist IS test");
    EXPECT_EQ(mock.addAutoplaylistClientCallCount, 1);
    EXPECT_EQ(mock.lastAddAutoQuery, "artist IS test");
    EXPECT_EQ(mock.lastAddAutoSort, "%title%");
    EXPECT_EQ(mock.lastAddAutoFlags, 1u);
}

TEST_F(PlaylistHandlerP5Test, CreateAutoplaylist_EmptyQuery) {
    SetUpBasic();
    auto r = reimpl_p5::PlaylistCreateAutoplaylist(&mock, {{"name", "Test"}, {"query", ""}});
    EXPECT_FALSE(r["success"].get<bool>());
    EXPECT_EQ(r["error"], "Query string is required");
}

TEST_F(PlaylistHandlerP5Test, CreateAutoplaylist_NoKeepSorted) {
    SetUpBasic();
    auto r = reimpl_p5::PlaylistCreateAutoplaylist(&mock, {{"query", "test"}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.lastAddAutoFlags, 0u);
}

// ==========================================================================
// P5 Tests: ConvertToAutoplaylist
// ==========================================================================

TEST_F(PlaylistHandlerP5Test, ConvertAutoplaylist_Valid) {
    SetUpBasic();
    auto r = reimpl_p5::PlaylistConvertToAutoplaylist(&mock, {{"playlist", 0}, {"query", "genre IS rock"}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(r["playlist"], 0u);
    EXPECT_EQ(mock.addAutoplaylistClientCallCount, 1);
}

TEST_F(PlaylistHandlerP5Test, ConvertAutoplaylist_EmptyQuery) {
    SetUpBasic();
    auto r = reimpl_p5::PlaylistConvertToAutoplaylist(&mock, {{"playlist", 0}, {"query", ""}});
    EXPECT_FALSE(r["success"].get<bool>());
}

TEST_F(PlaylistHandlerP5Test, ConvertAutoplaylist_InvalidPlaylist) {
    SetUpBasic();
    auto r = reimpl_p5::PlaylistConvertToAutoplaylist(&mock, {{"playlist", 99}, {"query", "test"}});
    EXPECT_FALSE(r["success"].get<bool>());
}

// ==========================================================================
// P5 Tests: RemoveAutoplaylist
// ==========================================================================

TEST_F(PlaylistHandlerP5Test, RemoveAutoplaylist_SdkClient) {
    SetUpBasic();
    mock.playlists[0].isAutoplaylist = true;
    auto r = reimpl_p5::PlaylistRemoveAutoplaylist(&mock, {{"playlist", 0}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(r["source"], "sdk");
    EXPECT_EQ(mock.removeAutoplaylistClientCallCount, 1);
}

TEST_F(PlaylistHandlerP5Test, RemoveAutoplaylist_DuiFallback) {
    SetUpBasic();
    // Not an autoplaylist_manager client, but DUI autoplaylist lock
    mock.detectAutoplaylistResult = {true, true, "Autoplaylist"};
    auto r = reimpl_p5::PlaylistRemoveAutoplaylist(&mock, {{"playlist", 0}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(r["source"], "dui");
}

TEST_F(PlaylistHandlerP5Test, RemoveAutoplaylist_NotAutoplaylist) {
    SetUpBasic();
    mock.detectAutoplaylistResult = {false, false, ""};
    auto r = reimpl_p5::PlaylistRemoveAutoplaylist(&mock, {{"playlist", 0}});
    EXPECT_FALSE(r["success"].get<bool>());
    EXPECT_EQ(r["error"], "Not an autoplaylist");
}

TEST_F(PlaylistHandlerP5Test, RemoveAutoplaylist_InvalidPlaylist) {
    SetUpBasic();
    auto r = reimpl_p5::PlaylistRemoveAutoplaylist(&mock, {{"playlist", 99}});
    EXPECT_FALSE(r["success"].get<bool>());
}

// ==========================================================================
// P5 Tests: GetAutoplaylistInfo
// ==========================================================================

TEST_F(PlaylistHandlerP5Test, GetAutoplaylistInfo_SdkAutoplaylist) {
    SetUpBasic();
    mock.detectAutoplaylistResult = {true, false, ""};
    mock.autoplaylistFlagsResult = 1;  // autoplaylist_flag_sort
    auto r = reimpl_p5::PlaylistGetAutoplaylistInfo(&mock, {{"playlist", 0}});
    EXPECT_TRUE(r["isAutoplaylist"].get<bool>());
    EXPECT_TRUE(r["keepSorted"].get<bool>());
    EXPECT_EQ(r["source"], "sdk");
    EXPECT_EQ(mock.getAutoplaylistFlagsCallCount, 1);
}

TEST_F(PlaylistHandlerP5Test, GetAutoplaylistInfo_DuiAutoplaylist) {
    SetUpBasic();
    mock.detectAutoplaylistResult = {true, true, "Autoplaylist"};
    auto r = reimpl_p5::PlaylistGetAutoplaylistInfo(&mock, {{"playlist", 0}});
    EXPECT_TRUE(r["isAutoplaylist"].get<bool>());
    EXPECT_EQ(r["source"], "dui");
    EXPECT_EQ(r["lockName"], "Autoplaylist");
    EXPECT_EQ(mock.getAutoplaylistFlagsCallCount, 0);  // DUI -> skip flags
}

TEST_F(PlaylistHandlerP5Test, GetAutoplaylistInfo_NotAutoplaylist) {
    SetUpBasic();
    mock.detectAutoplaylistResult = {false, false, ""};
    auto r = reimpl_p5::PlaylistGetAutoplaylistInfo(&mock, {{"playlist", 0}});
    EXPECT_FALSE(r["isAutoplaylist"].get<bool>());
}

// ==========================================================================
// P5 Tests: GetAutoplaylistQuery
// ==========================================================================

TEST_F(PlaylistHandlerP5Test, GetAutoplaylistQuery_SdkAutoplaylist) {
    SetUpBasic();
    mock.detectAutoplaylistResult = {true, false, ""};
    mock.autoplaylistFlagsResult = 0;
    auto r = reimpl_p5::PlaylistGetAutoplaylistQuery(&mock, {{"playlist", 0}});
    EXPECT_TRUE(r["isAutoplaylist"].get<bool>());
    EXPECT_TRUE(r["query"].is_null());
    EXPECT_FALSE(r["keepSorted"].get<bool>());
    EXPECT_EQ(r["source"], "sdk");
}

TEST_F(PlaylistHandlerP5Test, GetAutoplaylistQuery_NotAutoplaylist) {
    SetUpBasic();
    mock.detectAutoplaylistResult = {false, false, ""};
    auto r = reimpl_p5::PlaylistGetAutoplaylistQuery(&mock, {{"playlist", 0}});
    EXPECT_FALSE(r["isAutoplaylist"].get<bool>());
    EXPECT_TRUE(r["query"].is_null());
}

// ==========================================================================
// P5 Tests: ReorderPlaylists (playlist-level)
// ==========================================================================

TEST_F(PlaylistHandlerP5Test, ReorderPlaylists_Valid) {
    SetUpBasic();
    auto r = reimpl_p5::PlaylistReorderPlaylists(&mock, {{"newOrder", {1, 0}}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(r["count"], 2u);
    ASSERT_EQ(mock.lastReorderPlaylistsOrder, (std::vector<size_t>{1, 0}));
}

TEST_F(PlaylistHandlerP5Test, ReorderPlaylists_LengthMismatch) {
    SetUpBasic();
    auto r = reimpl_p5::PlaylistReorderPlaylists(&mock, {{"newOrder", {0}}});
    EXPECT_FALSE(r["success"].get<bool>());
    EXPECT_EQ(r["error"], "newOrder length mismatch");
}

TEST_F(PlaylistHandlerP5Test, ReorderPlaylists_IndexOutOfRange) {
    SetUpBasic();
    auto r = reimpl_p5::PlaylistReorderPlaylists(&mock, {{"newOrder", {0, 99}}});
    EXPECT_FALSE(r["success"].get<bool>());
}

TEST_F(PlaylistHandlerP5Test, ReorderPlaylists_NonNumeric) {
    SetUpBasic();
    auto r = reimpl_p5::PlaylistReorderPlaylists(&mock, {{"newOrder", {"a", "b"}}});
    EXPECT_FALSE(r["success"].get<bool>());
}

TEST_F(PlaylistHandlerP5Test, ReorderPlaylists_Failure) {
    SetUpBasic();
    mock.reorderPlaylistsResult = false;
    auto r = reimpl_p5::PlaylistReorderPlaylists(&mock, {{"newOrder", {1, 0}}});
    EXPECT_FALSE(r["success"].get<bool>());
}

// ==========================================================================
// Reimplemented handler functions (mirror PlaylistApi.cpp P5b handlers)
// ==========================================================================
namespace reimpl_p5b {

static json MakePlaylistLockedError(size_t playlistIndex) {
    return {
        {"success", false},
        {"error", "Playlist is locked"},
        {"details", {{"playlist", playlistIndex}, {"isLocked", true}}}
    };
}

json PlaylistAddPaths(IPlaylistService* svc, const json& params) {
    size_t playlistIndex = params.value("playlist", SIZE_MAX);
    auto paths = params.value("paths", json::array());
    if (playlistIndex == SIZE_MAX) playlistIndex = svc->get_active_playlist();
    if (playlistIndex >= svc->get_playlist_count())
        return { {"success", false}, {"error", "Invalid playlist index"} };
    if (svc->playlist_lock_is_present(playlistIndex))
        return MakePlaylistLockedError(playlistIndex);
    if (paths.empty())
        return { {"success", false}, {"error", "No paths specified"} };

    auto result = svc->add_paths(playlistIndex, paths);
    if (result.addedCount == 0)
        return { {"success", false}, {"error", "No valid tracks found"},
            {"playlist", playlistIndex}, {"requestedPaths", paths.size()},
            {"invalidCount", result.invalidCount}, {"countBefore", result.countBefore} };
    return { {"success", true}, {"playlist", playlistIndex},
        {"requestedPaths", paths.size()}, {"addedCount", result.addedCount},
        {"invalidCount", result.invalidCount}, {"countBefore", result.countBefore},
        {"totalCount", result.totalCount} };
}

json PlaylistAddHandles(IPlaylistService* svc, const json& params) {
    size_t playlistIndex = params.value("playlist", SIZE_MAX);
    auto handles = params.value("handles", json::array());
    if (playlistIndex == SIZE_MAX) playlistIndex = svc->get_active_playlist();
    if (playlistIndex >= svc->get_playlist_count())
        return { {"success", false}, {"error", "Invalid playlist index"} };
    if (svc->playlist_lock_is_present(playlistIndex))
        return MakePlaylistLockedError(playlistIndex);
    if (handles.empty())
        return { {"success", false}, {"error", "No handles specified"} };

    auto result = svc->add_handles(playlistIndex, handles);
    if (result.addedCount == 0)
        return { {"success", false}, {"error", "No valid handles created"},
            {"playlist", playlistIndex}, {"requestedCount", handles.size()},
            {"invalidCount", result.invalidCount} };
    return { {"success", true}, {"playlist", playlistIndex},
        {"requestedCount", handles.size()}, {"addedCount", result.addedCount},
        {"invalidCount", result.invalidCount}, {"countBefore", result.countBefore},
        {"totalCount", result.totalCount} };
}

json PlaylistAddPathsSequential(IPlaylistService* svc, const json& params) {
    size_t playlistIndex = params.value("playlist", SIZE_MAX);
    auto paths = params.value("paths", json::array());
    if (playlistIndex == SIZE_MAX) playlistIndex = svc->get_active_playlist();
    if (playlistIndex >= svc->get_playlist_count())
        return {{"success", false}, {"error", "Invalid playlist index"}};
    if (svc->playlist_lock_is_present(playlistIndex))
        return MakePlaylistLockedError(playlistIndex);
    if (paths.empty())
        return {{"success", false}, {"error", "No paths specified"}};

    auto result = svc->add_paths_sequential(playlistIndex, paths);
    return { {"success", true}, {"playlist", playlistIndex},
        {"addedCount", result.addedCount}, {"order", result.order} };
}

json PlaylistAddPathsAsync(IPlaylistService* svc, const json& params) {
    size_t playlistIndex = params.value("playlist", SIZE_MAX);
    auto paths = params.value("paths", json::array());
    if (playlistIndex == SIZE_MAX) playlistIndex = svc->get_active_playlist();
    if (playlistIndex >= svc->get_playlist_count())
        return {{"success", false}, {"error", "Invalid playlist index"}};
    if (svc->playlist_lock_is_present(playlistIndex))
        return MakePlaylistLockedError(playlistIndex);
    if (paths.empty())
        return {{"success", false}, {"error", "No paths specified"}};

    std::string operationId = "test-op-id";
    auto info = svc->start_add_paths_async(playlistIndex, paths, operationId, nullptr);
    if (info.validPathCount == 0)
        return { {"success", false}, {"error", "No valid paths specified"},
            {"invalidCount", info.invalidCount} };
    return { {"success", true}, {"operationId", operationId},
        {"status", "pending"}, {"totalCount", info.validPathCount},
        {"invalidCount", info.invalidCount} };
}

json PlaylistReplaceAllAndPlay(IPlaylistService* svc, IPlaybackService* pbSvc, const json& params) {
    size_t playlistIndex = params.value("playlist", SIZE_MAX);
    auto paths = params.value("paths", json::array());
    size_t playIndex = params.value("playIndex", static_cast<size_t>(0));
    bool stopFirst = params.value("stopFirst", true);
    bool autoPlay = params.value("autoPlay", true);

    if (playlistIndex == SIZE_MAX) playlistIndex = svc->get_active_playlist();
    if (playlistIndex >= svc->get_playlist_count())
        return { {"success", false}, {"error", "Invalid playlist index"} };
    if (svc->playlist_lock_is_present(playlistIndex))
        return MakePlaylistLockedError(playlistIndex);
    if (paths.empty())
        return { {"success", false}, {"error", "No paths specified"} };

    if (stopFirst && pbSvc->is_playing())
        pbSvc->stop();

    auto result = svc->replace_all(playlistIndex, paths);
    if (result.addedCount == 0)
        return { {"success", false}, {"error", "No valid tracks found"},
            {"clearedCount", result.clearedCount}, {"invalidCount", result.invalidCount} };

    svc->set_active_playlist(playlistIndex);
    if (playIndex >= result.totalCount) playIndex = 0;
    if (autoPlay)
        svc->execute_default_action(playlistIndex, playIndex);
    else
        svc->set_focus_item(playlistIndex, playIndex);

    return { {"success", true}, {"playlist", playlistIndex},
        {"clearedCount", result.clearedCount}, {"addedCount", result.addedCount},
        {"totalCount", result.totalCount}, {"playIndex", playIndex} };
}

} // namespace reimpl_p5b

// ==========================================================================
// P5b Test Fixture
// ==========================================================================
class PlaylistHandlerP5bTest : public ::testing::Test {
protected:
    MockPlaylistService mock;
    MockPlaybackService pbMock;

    void SetUpBasic() {
        mock.Reset();
        mock.playlists = {
            {"Default", 10, false, false},
            {"Rock", 5, false, false}
        };
        pbMock.playing = false;
    }

    void SetUpLocked() {
        SetUpBasic();
        mock.playlists[0].isLocked = true;
    }
};

// ==========================================================================
// P5b Tests: playlist.addPaths
// ==========================================================================

TEST_F(PlaylistHandlerP5bTest, AddPaths_Success) {
    SetUpBasic();
    mock.addPathsResult = { 3, 0, 10, 13 };
    auto r = reimpl_p5b::PlaylistAddPaths(&mock, {{"playlist", 0}, {"paths", {"a.mp3", "b.mp3", "c.mp3"}}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(r["addedCount"], 3u);
    EXPECT_EQ(r["totalCount"], 13u);
    EXPECT_EQ(r["countBefore"], 10u);
    EXPECT_EQ(r["invalidCount"], 0u);
    EXPECT_EQ(mock.addPathsCallCount, 1);
    EXPECT_EQ(mock.lastAddPathsPlaylist, 0u);
}

TEST_F(PlaylistHandlerP5bTest, AddPaths_DefaultActive) {
    SetUpBasic();
    mock.addPathsResult = { 1, 0, 10, 11 };
    auto r = reimpl_p5b::PlaylistAddPaths(&mock, {{"paths", {"a.mp3"}}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.lastAddPathsPlaylist, 0u);
}

TEST_F(PlaylistHandlerP5bTest, AddPaths_InvalidPlaylist) {
    SetUpBasic();
    auto r = reimpl_p5b::PlaylistAddPaths(&mock, {{"playlist", 99}, {"paths", {"a.mp3"}}});
    EXPECT_FALSE(r["success"].get<bool>());
    EXPECT_EQ(r["error"], "Invalid playlist index");
}

TEST_F(PlaylistHandlerP5bTest, AddPaths_Locked) {
    SetUpLocked();
    auto r = reimpl_p5b::PlaylistAddPaths(&mock, {{"playlist", 0}, {"paths", {"a.mp3"}}});
    EXPECT_FALSE(r["success"].get<bool>());
    EXPECT_EQ(r["error"], "Playlist is locked");
}

TEST_F(PlaylistHandlerP5bTest, AddPaths_Empty) {
    SetUpBasic();
    auto r = reimpl_p5b::PlaylistAddPaths(&mock, {{"playlist", 0}, {"paths", json::array()}});
    EXPECT_FALSE(r["success"].get<bool>());
    EXPECT_EQ(r["error"], "No paths specified");
}

TEST_F(PlaylistHandlerP5bTest, AddPaths_NoValidTracks) {
    SetUpBasic();
    mock.addPathsResult = { 0, 2, 10, 10 };
    auto r = reimpl_p5b::PlaylistAddPaths(&mock, {{"playlist", 0}, {"paths", {"bad1", "bad2"}}});
    EXPECT_FALSE(r["success"].get<bool>());
    EXPECT_EQ(r["error"], "No valid tracks found");
    EXPECT_EQ(r["invalidCount"], 2u);
}

TEST_F(PlaylistHandlerP5bTest, AddPaths_WithInvalid) {
    SetUpBasic();
    mock.addPathsResult = { 2, 1, 5, 7 };
    auto r = reimpl_p5b::PlaylistAddPaths(&mock, {{"playlist", 1}, {"paths", {"a.mp3", "bad", "b.mp3"}}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(r["addedCount"], 2u);
    EXPECT_EQ(r["invalidCount"], 1u);
    EXPECT_EQ(r["requestedPaths"], 3u);
}

// ==========================================================================
// P5b Tests: playlist.addHandles
// ==========================================================================

TEST_F(PlaylistHandlerP5bTest, AddHandles_Success) {
    SetUpBasic();
    mock.addPathsResult = { 2, 0, 10, 12 };
    auto handles = json::array();
    handles.push_back({{"path", "a.mp3"}, {"subsong", 0}});
    handles.push_back({{"path", "b.mp3"}, {"subsong", 0}});
    auto r = reimpl_p5b::PlaylistAddHandles(&mock, {{"playlist", 0}, {"handles", handles}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(r["addedCount"], 2u);
    EXPECT_EQ(mock.addHandlesCallCount, 1);
}

TEST_F(PlaylistHandlerP5bTest, AddHandles_InvalidPlaylist) {
    SetUpBasic();
    auto r = reimpl_p5b::PlaylistAddHandles(&mock, {{"playlist", 99}, {"handles", json::array({json::object()})}});
    EXPECT_FALSE(r["success"].get<bool>());
    EXPECT_EQ(r["error"], "Invalid playlist index");
}

TEST_F(PlaylistHandlerP5bTest, AddHandles_Locked) {
    SetUpLocked();
    auto r = reimpl_p5b::PlaylistAddHandles(&mock, {{"playlist", 0}, {"handles", json::array({json::object()})}});
    EXPECT_FALSE(r["success"].get<bool>());
    EXPECT_EQ(r["error"], "Playlist is locked");
}

TEST_F(PlaylistHandlerP5bTest, AddHandles_Empty) {
    SetUpBasic();
    auto r = reimpl_p5b::PlaylistAddHandles(&mock, {{"playlist", 0}, {"handles", json::array()}});
    EXPECT_FALSE(r["success"].get<bool>());
    EXPECT_EQ(r["error"], "No handles specified");
}

TEST_F(PlaylistHandlerP5bTest, AddHandles_NoValid) {
    SetUpBasic();
    mock.addPathsResult = { 0, 1, 10, 10 };
    auto r = reimpl_p5b::PlaylistAddHandles(&mock, {{"playlist", 0}, {"handles", json::array({{{"path", "bad"}}})}});
    EXPECT_FALSE(r["success"].get<bool>());
    EXPECT_EQ(r["error"], "No valid handles created");
}

TEST_F(PlaylistHandlerP5bTest, AddHandles_DefaultActive) {
    SetUpBasic();
    mock.addPathsResult = { 1, 0, 10, 11 };
    auto r = reimpl_p5b::PlaylistAddHandles(&mock, {{"handles", json::array({{{"path","a.mp3"}}})}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.lastAddHandlesPlaylist, 0u);
}

// ==========================================================================
// P5b Tests: playlist.addPathsSequential
// ==========================================================================

TEST_F(PlaylistHandlerP5bTest, AddPathsSeq_Success) {
    SetUpBasic();
    mock.addPathsSeqResult = { 3, {10, 11, 12} };
    auto r = reimpl_p5b::PlaylistAddPathsSequential(&mock, {{"playlist", 0}, {"paths", {"a.mp3", "b.mp3", "c.mp3"}}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(r["addedCount"], 3u);
    EXPECT_EQ(r["order"].size(), 3u);
    EXPECT_EQ(mock.addPathsSeqCallCount, 1);
}

TEST_F(PlaylistHandlerP5bTest, AddPathsSeq_InvalidPlaylist) {
    SetUpBasic();
    auto r = reimpl_p5b::PlaylistAddPathsSequential(&mock, {{"playlist", 99}, {"paths", {"a.mp3"}}});
    EXPECT_FALSE(r["success"].get<bool>());
}

TEST_F(PlaylistHandlerP5bTest, AddPathsSeq_Locked) {
    SetUpLocked();
    auto r = reimpl_p5b::PlaylistAddPathsSequential(&mock, {{"playlist", 0}, {"paths", {"a.mp3"}}});
    EXPECT_FALSE(r["success"].get<bool>());
}

TEST_F(PlaylistHandlerP5bTest, AddPathsSeq_Empty) {
    SetUpBasic();
    auto r = reimpl_p5b::PlaylistAddPathsSequential(&mock, {{"playlist", 0}, {"paths", json::array()}});
    EXPECT_FALSE(r["success"].get<bool>());
}

TEST_F(PlaylistHandlerP5bTest, AddPathsSeq_DefaultActive) {
    SetUpBasic();
    mock.addPathsSeqResult = { 1, {10} };
    auto r = reimpl_p5b::PlaylistAddPathsSequential(&mock, {{"paths", {"a.mp3"}}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.lastAddPathsSeqPlaylist, 0u);
}

// ==========================================================================
// P5b Tests: playlist.addPathsAsync
// ==========================================================================

TEST_F(PlaylistHandlerP5bTest, AddPathsAsync_Success) {
    SetUpBasic();
    mock.asyncAddPathsInfo = { 3, 0 };
    auto r = reimpl_p5b::PlaylistAddPathsAsync(&mock, {{"playlist", 0}, {"paths", {"a.mp3", "b.mp3", "c.mp3"}}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(r["status"], "pending");
    EXPECT_EQ(r["totalCount"], 3u);
    EXPECT_EQ(r["invalidCount"], 0u);
    EXPECT_TRUE(r.contains("operationId"));
    EXPECT_EQ(mock.startAddPathsAsyncCallCount, 1);
}

TEST_F(PlaylistHandlerP5bTest, AddPathsAsync_InvalidPlaylist) {
    SetUpBasic();
    auto r = reimpl_p5b::PlaylistAddPathsAsync(&mock, {{"playlist", 99}, {"paths", {"a.mp3"}}});
    EXPECT_FALSE(r["success"].get<bool>());
}

TEST_F(PlaylistHandlerP5bTest, AddPathsAsync_Locked) {
    SetUpLocked();
    auto r = reimpl_p5b::PlaylistAddPathsAsync(&mock, {{"playlist", 0}, {"paths", {"a.mp3"}}});
    EXPECT_FALSE(r["success"].get<bool>());
}

TEST_F(PlaylistHandlerP5bTest, AddPathsAsync_Empty) {
    SetUpBasic();
    auto r = reimpl_p5b::PlaylistAddPathsAsync(&mock, {{"playlist", 0}, {"paths", json::array()}});
    EXPECT_FALSE(r["success"].get<bool>());
}

TEST_F(PlaylistHandlerP5bTest, AddPathsAsync_NoValid) {
    SetUpBasic();
    mock.asyncAddPathsInfo = { 0, 2 };
    auto r = reimpl_p5b::PlaylistAddPathsAsync(&mock, {{"playlist", 0}, {"paths", {"bad1", "bad2"}}});
    EXPECT_FALSE(r["success"].get<bool>());
    EXPECT_EQ(r["error"], "No valid paths specified");
    EXPECT_EQ(r["invalidCount"], 2u);
}

TEST_F(PlaylistHandlerP5bTest, AddPathsAsync_WithInvalid) {
    SetUpBasic();
    mock.asyncAddPathsInfo = { 2, 1 };
    auto r = reimpl_p5b::PlaylistAddPathsAsync(&mock, {{"playlist", 0}, {"paths", {"a", "b", "bad"}}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(r["totalCount"], 2u);
    EXPECT_EQ(r["invalidCount"], 1u);
}

TEST_F(PlaylistHandlerP5bTest, AddPathsAsync_DefaultActive) {
    SetUpBasic();
    mock.asyncAddPathsInfo = { 1, 0 };
    auto r = reimpl_p5b::PlaylistAddPathsAsync(&mock, {{"paths", {"a.mp3"}}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.lastAsyncPlaylist, 0u);
}

// ==========================================================================
// P5b Tests: playlist.replaceAllAndPlay
// ==========================================================================

TEST_F(PlaylistHandlerP5bTest, ReplaceAll_Success) {
    SetUpBasic();
    mock.replaceAllResult = { 10, 3, 0, 3 };
    auto r = reimpl_p5b::PlaylistReplaceAllAndPlay(&mock, &pbMock,
        {{"playlist", 0}, {"paths", {"a.mp3", "b.mp3", "c.mp3"}}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(r["clearedCount"], 10u);
    EXPECT_EQ(r["addedCount"], 3u);
    EXPECT_EQ(r["totalCount"], 3u);
    EXPECT_EQ(r["playIndex"], 0u);
    EXPECT_EQ(mock.replaceAllCallCount, 1);
    EXPECT_EQ(mock.setActivePlaylistCallCount, 1);
    EXPECT_EQ(mock.executeDefaultActionCallCount, 1);
}

TEST_F(PlaylistHandlerP5bTest, ReplaceAll_StopsPlayback) {
    SetUpBasic();
    pbMock.playing = true;
    mock.replaceAllResult = { 5, 2, 0, 2 };
    auto r = reimpl_p5b::PlaylistReplaceAllAndPlay(&mock, &pbMock,
        {{"playlist", 0}, {"paths", {"a.mp3", "b.mp3"}}, {"stopFirst", true}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(pbMock.stopCallCount, 1);
}

TEST_F(PlaylistHandlerP5bTest, ReplaceAll_NoStopWhenNotPlaying) {
    SetUpBasic();
    pbMock.playing = false;
    mock.replaceAllResult = { 5, 2, 0, 2 };
    auto r = reimpl_p5b::PlaylistReplaceAllAndPlay(&mock, &pbMock,
        {{"playlist", 0}, {"paths", {"a.mp3", "b.mp3"}}, {"stopFirst", true}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(pbMock.stopCallCount, 0);
}

TEST_F(PlaylistHandlerP5bTest, ReplaceAll_StopFirstFalse) {
    SetUpBasic();
    pbMock.playing = true;
    mock.replaceAllResult = { 5, 2, 0, 2 };
    auto r = reimpl_p5b::PlaylistReplaceAllAndPlay(&mock, &pbMock,
        {{"playlist", 0}, {"paths", {"a.mp3"}}, {"stopFirst", false}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(pbMock.stopCallCount, 0);
}

TEST_F(PlaylistHandlerP5bTest, ReplaceAll_AutoPlayFalse) {
    SetUpBasic();
    mock.replaceAllResult = { 5, 2, 0, 2 };
    auto r = reimpl_p5b::PlaylistReplaceAllAndPlay(&mock, &pbMock,
        {{"playlist", 0}, {"paths", {"a.mp3"}}, {"autoPlay", false}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.executeDefaultActionCallCount, 0);
    EXPECT_EQ(mock.setFocusItemCallCount, 1);
}

TEST_F(PlaylistHandlerP5bTest, ReplaceAll_PlayIndexClamped) {
    SetUpBasic();
    mock.replaceAllResult = { 5, 2, 0, 2 };
    auto r = reimpl_p5b::PlaylistReplaceAllAndPlay(&mock, &pbMock,
        {{"playlist", 0}, {"paths", {"a.mp3"}}, {"playIndex", 99}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(r["playIndex"], 0u);
}

TEST_F(PlaylistHandlerP5bTest, ReplaceAll_InvalidPlaylist) {
    SetUpBasic();
    auto r = reimpl_p5b::PlaylistReplaceAllAndPlay(&mock, &pbMock,
        {{"playlist", 99}, {"paths", {"a.mp3"}}});
    EXPECT_FALSE(r["success"].get<bool>());
}

TEST_F(PlaylistHandlerP5bTest, ReplaceAll_Locked) {
    SetUpLocked();
    auto r = reimpl_p5b::PlaylistReplaceAllAndPlay(&mock, &pbMock,
        {{"playlist", 0}, {"paths", {"a.mp3"}}});
    EXPECT_FALSE(r["success"].get<bool>());
}

TEST_F(PlaylistHandlerP5bTest, ReplaceAll_EmptyPaths) {
    SetUpBasic();
    auto r = reimpl_p5b::PlaylistReplaceAllAndPlay(&mock, &pbMock,
        {{"playlist", 0}, {"paths", json::array()}});
    EXPECT_FALSE(r["success"].get<bool>());
}

TEST_F(PlaylistHandlerP5bTest, ReplaceAll_NoValidTracks) {
    SetUpBasic();
    mock.replaceAllResult = { 10, 0, 2, 0 };
    auto r = reimpl_p5b::PlaylistReplaceAllAndPlay(&mock, &pbMock,
        {{"playlist", 0}, {"paths", {"bad1", "bad2"}}});
    EXPECT_FALSE(r["success"].get<bool>());
    EXPECT_EQ(r["error"], "No valid tracks found");
    EXPECT_EQ(r["clearedCount"], 10u);
}

TEST_F(PlaylistHandlerP5bTest, ReplaceAll_DefaultActive) {
    SetUpBasic();
    mock.replaceAllResult = { 10, 1, 0, 1 };
    auto r = reimpl_p5b::PlaylistReplaceAllAndPlay(&mock, &pbMock,
        {{"paths", {"a.mp3"}}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.lastReplaceAllPlaylist, 0u);
}
