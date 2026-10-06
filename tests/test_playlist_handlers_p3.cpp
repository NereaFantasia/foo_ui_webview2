// test_playlist_handlers_p3.cpp - PlaylistApi playlist list, activation and CRUD handlers
// Validates PlaylistApi handler logic using MockPlaylistService.
// Uses reimpl pattern: handler functions mirror product code but accept
// IPlaylistService* instead of calling playlist_manager::get().
#include "pch.h"
#include "mocks/MockPlaylistService.h"
#include "mocks/MockPlaybackService.h"
#include "harness/PlaylistHandlerTestSupport.h"

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
