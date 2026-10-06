// test_playlist_handlers_p5b.cpp - PlaylistApi add-paths, add-handles and replace-all handlers
// Validates PlaylistApi handler logic using MockPlaylistService.
// Uses reimpl pattern: handler functions mirror product code but accept
// IPlaylistService* instead of calling playlist_manager::get().
#include "pch.h"
#include "mocks/MockPlaylistService.h"
#include "mocks/MockPlaybackService.h"

using json = nlohmann::json;

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
