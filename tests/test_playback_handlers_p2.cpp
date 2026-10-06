// test_playback_handlers_p2.cpp - PlaybackApi current-track, playing-playlist and play-path handlers
// Validates PlaybackApi handler logic using MockPlaybackService.
// Uses reimpl pattern: handler functions mirror product code but accept
// IPlaybackService* instead of calling playback_control::get().
#include "pch.h"
#include "mocks/MockPlaybackService.h"
#include "harness/PlaybackHandlerTestSupport.h"

using json = nlohmann::json;

// ==========================================================================
// P2a: Current Track / Track Index / Playing Playlist reimpl handlers
// ==========================================================================
namespace reimpl_p2a {

json PlaybackGetCurrentTrack(IPlaybackService* svc, const json& /*params*/) {
    auto track = svc->get_current_track();
    json result = { {"success", true}, {"found", track.has_value()} };
    if (track) {
        result["track"] = api::common::ToJson(*track);
    }
    return result;
}

json PlaybackGetCurrentTrackIndex(IPlaybackService* svc, const json& params) {
    auto loc = svc->get_playing_item_location();

    if (!loc.found) {
        return {
            {"success", true},
            {"found", false},
            {"playlist", nullptr},
            {"index", nullptr}
        };
    }

    bool includeTrackInfo = params.value("includeTrackInfo", false);
    json result = {
        {"success", true},
        {"found", true},
        {"playlist", loc.playlist},
        {"index", loc.index}
    };

    if (includeTrackInfo) {
        auto track = svc->get_track_at(loc.playlist, loc.index);
        if (track) {
            result["track"] = api::common::ToJson(*track);
        }
    }

    return result;
}

json PlaybackGetPlayingPlaylist(IPlaybackService* svc, const json& /*params*/) {
    size_t playlist = svc->get_playing_playlist();

    if (playlist == SIZE_MAX) {
        return {{"success", true}, {"playlist", nullptr}, {"found", false}};
    }

    std::string name = svc->get_playlist_name(playlist);

    return {
        {"success", true},
        {"playlist", playlist},
        {"name", name},
        {"found", true}
    };
}

} // namespace reimpl_p2a

// ==========================================================================
// P2a: playback.getCurrentTrack
// ==========================================================================

TEST_F(PlaybackHandlerTest, GetCurrentTrack_NoTrackPlaying_ReturnsNotFound) {
    // currentTrack default is empty
    auto result = reimpl_p2a::PlaybackGetCurrentTrack(&mock, emptyParams);
    EXPECT_TRUE(result["success"].get<bool>());
    EXPECT_FALSE(result["found"].get<bool>());
    EXPECT_FALSE(result.contains("track"));
    EXPECT_EQ(mock.getCurrentTrackCallCount, 1);
}

TEST_F(PlaybackHandlerTest, GetCurrentTrack_WithTrack_ReturnsTrackRow) {
    api::common::Track track;
    track.handle = "/music/test.mp3";
    track.title = "Test Song";
    track.artist = "Test Artist";
    track.album = "Test Album";
    track.duration = 240.5;
    mock.currentTrack = track;
    auto result = reimpl_p2a::PlaybackGetCurrentTrack(&mock, emptyParams);
    EXPECT_TRUE(result["found"].get<bool>());
    EXPECT_EQ(result["track"]["handle"].get<std::string>(), "/music/test.mp3");
    EXPECT_EQ(result["track"]["title"].get<std::string>(), "Test Song");
    EXPECT_EQ(result["track"]["artist"].get<std::string>(), "Test Artist");
    EXPECT_EQ(result["track"]["album"].get<std::string>(), "Test Album");
    EXPECT_DOUBLE_EQ(result["track"]["duration"].get<double>(), 240.5);
    // The row carries every declared key, the ones without a value included.
    EXPECT_EQ(result["track"].size(), api::common::Track::kFields.size());
    EXPECT_EQ(mock.getCurrentTrackCallCount, 1);
}

TEST_F(PlaybackHandlerTest, GetCurrentTrack_WithSubsong_ReturnsSubsongHandle) {
    api::common::Track track;
    track.handle = "/music/album.cue|subsong:3";
    track.title = "Track 3";
    track.subsong = 3;
    mock.currentTrack = track;
    auto result = reimpl_p2a::PlaybackGetCurrentTrack(&mock, emptyParams);
    EXPECT_EQ(result["track"]["subsong"].get<int>(), 3);
    EXPECT_EQ(result["track"]["handle"].get<std::string>(), "/music/album.cue|subsong:3");
}

// ==========================================================================
// P2a: playback.getCurrentTrackIndex
// ==========================================================================

TEST_F(PlaybackHandlerTest, GetCurrentTrackIndex_NoTrackPlaying_ReturnsNotFound) {
    // playingItemLocation default: found=false
    auto result = reimpl_p2a::PlaybackGetCurrentTrackIndex(&mock, emptyParams);
    EXPECT_TRUE(result["success"].get<bool>());
    EXPECT_FALSE(result["found"].get<bool>());
    EXPECT_TRUE(result["playlist"].is_null());
    EXPECT_TRUE(result["index"].is_null());
    EXPECT_EQ(mock.getPlayingItemLocationCallCount, 1);
}

TEST_F(PlaybackHandlerTest, GetCurrentTrackIndex_WithTrack_ReturnsPlaylistAndIndex) {
    mock.playingItemLocation = {true, 2, 15};
    auto result = reimpl_p2a::PlaybackGetCurrentTrackIndex(&mock, emptyParams);
    EXPECT_TRUE(result["success"].get<bool>());
    EXPECT_TRUE(result["found"].get<bool>());
    EXPECT_EQ(result["playlist"].get<size_t>(), 2u);
    EXPECT_EQ(result["index"].get<size_t>(), 15u);
    EXPECT_EQ(mock.getPlayingItemLocationCallCount, 1);
}

TEST_F(PlaybackHandlerTest, GetCurrentTrackIndex_WithoutTrackInfo_NoTrackField) {
    mock.playingItemLocation = {true, 0, 5};
    // includeTrackInfo not set (default false)
    auto result = reimpl_p2a::PlaybackGetCurrentTrackIndex(&mock, emptyParams);
    EXPECT_TRUE(result["found"].get<bool>());
    EXPECT_FALSE(result.contains("track"));
    EXPECT_EQ(mock.getTrackAtCallCount, 0);
}

TEST_F(PlaybackHandlerTest, GetCurrentTrackIndex_WithTrackInfo_IncludesTrack) {
    mock.playingItemLocation = {true, 1, 7};
    api::common::Track track;
    track.title = "Included Track";
    track.artist = "Someone";
    mock.trackAt = track;
    json params = {{"includeTrackInfo", true}};
    auto result = reimpl_p2a::PlaybackGetCurrentTrackIndex(&mock, params);
    EXPECT_TRUE(result["found"].get<bool>());
    EXPECT_TRUE(result.contains("track"));
    EXPECT_EQ(result["track"]["title"].get<std::string>(), "Included Track");
    EXPECT_EQ(mock.getTrackAtCallCount, 1);
    EXPECT_EQ(mock.lastTrackAtPlaylist, 1u);
    EXPECT_EQ(mock.lastTrackAtIndex, 7u);
}

TEST_F(PlaybackHandlerTest, GetCurrentTrackIndex_TrackMissing_NoTrackField) {
    mock.playingItemLocation = {true, 0, 3};
    mock.trackAt.reset();  // the row is gone
    json params = {{"includeTrackInfo", true}};
    auto result = reimpl_p2a::PlaybackGetCurrentTrackIndex(&mock, params);
    EXPECT_TRUE(result["found"].get<bool>());
    EXPECT_FALSE(result.contains("track"));
    EXPECT_EQ(mock.getTrackAtCallCount, 1);
}

// ==========================================================================
// P2a: playback.getPlayingPlaylist
// ==========================================================================

TEST_F(PlaybackHandlerTest, GetPlayingPlaylist_NoPlaylist_ReturnsNotFound) {
    // playingPlaylist default is SIZE_MAX
    auto result = reimpl_p2a::PlaybackGetPlayingPlaylist(&mock, emptyParams);
    EXPECT_TRUE(result["success"].get<bool>());
    EXPECT_FALSE(result["found"].get<bool>());
    EXPECT_TRUE(result["playlist"].is_null());
    EXPECT_EQ(mock.getPlayingPlaylistCallCount, 1);
}

TEST_F(PlaybackHandlerTest, GetPlayingPlaylist_WithPlaylist_ReturnsIndexAndName) {
    mock.playingPlaylist = 3;
    mock.playingPlaylistName = "My Favorites";
    auto result = reimpl_p2a::PlaybackGetPlayingPlaylist(&mock, emptyParams);
    EXPECT_TRUE(result["success"].get<bool>());
    EXPECT_TRUE(result["found"].get<bool>());
    EXPECT_EQ(result["playlist"].get<size_t>(), 3u);
    EXPECT_EQ(result["name"].get<std::string>(), "My Favorites");
    EXPECT_EQ(mock.getPlayingPlaylistCallCount, 1);
    EXPECT_EQ(mock.getPlaylistNameCallCount, 1);
}

TEST_F(PlaybackHandlerTest, GetPlayingPlaylist_FirstPlaylist_ReturnsZeroIndex) {
    mock.playingPlaylist = 0;
    mock.playingPlaylistName = "Default";
    auto result = reimpl_p2a::PlaybackGetPlayingPlaylist(&mock, emptyParams);
    EXPECT_TRUE(result["found"].get<bool>());
    EXPECT_EQ(result["playlist"].get<size_t>(), 0u);
    EXPECT_EQ(result["name"].get<std::string>(), "Default");
}

TEST_F(PlaybackHandlerTest, GetPlayingPlaylist_EmptyName_ReturnsEmptyString) {
    mock.playingPlaylist = 1;
    mock.playingPlaylistName = "";
    auto result = reimpl_p2a::PlaybackGetPlayingPlaylist(&mock, emptyParams);
    EXPECT_TRUE(result["found"].get<bool>());
    EXPECT_EQ(result["name"].get<std::string>(), "");
}

// ==========================================================================
// P2b: Path Playback reimpl handlers
// ==========================================================================
namespace reimpl_p2b {

// Helper: parse subsong path (mirrors SubsongUtils::ParseSubsongPath)
static std::pair<std::string, int> ParseSubsongPath(const std::string& path) {
    std::string filePath = path;
    int subsongIndex = 0;
    size_t pos = path.find("|subsong:");
    if (pos != std::string::npos) {
        filePath = path.substr(0, pos);
        try {
            subsongIndex = std::stoi(path.substr(pos + 9));
        } catch (...) {
            subsongIndex = 0;
        }
    }
    return { filePath, subsongIndex };
}

json PlaybackPlayPath(IPlaybackService* svc, const json& params) {
    std::string path = params.value("path", std::string(""));
    if (path.empty()) {
        return {{"success", false}, {"error", "path is required"}};
    }

    try {
        auto [filePath, subsongIndex] = ParseSubsongPath(path);
        bool subsongRequested = (path.find("|subsong:") != std::string::npos);

        auto result = svc->play_single_path(filePath, subsongIndex, subsongRequested);

        if (!result.success) {
            return {
                {"success", false},
                {"error", result.error},
                {"path", filePath},
                {"subsong", subsongIndex}
            };
        }

        return {
            {"success", true},
            {"tracksAdded", result.tracksAdded},
            {"path", filePath},
            {"subsong", result.resolvedSubsong}
        };
    } catch (const std::exception& e) {
        return {{"success", false}, {"error", e.what()}};
    } catch (...) {
        return {{"success", false}, {"error", "Unknown error"}};
    }
}

json PlaybackPlayPaths(IPlaybackService* svc, const json& params) {
    if (!params.contains("paths") || !params["paths"].is_array()) {
        return {{"success", false}, {"error", "paths array is required"}};
    }

    size_t startIndex = params.value("startIndex", static_cast<size_t>(0));

    try {
        std::vector<std::string> paths;
        for (const auto& pathJson : params["paths"]) {
            paths.push_back(pathJson.get<std::string>());
        }

        auto result = svc->play_multiple_paths(paths, startIndex);

        if (!result.success) {
            return {{"success", false}, {"error", result.error}};
        }

        return {
            {"success", true},
            {"tracksAdded", result.tracksAdded},
            {"startedAt", result.startedAt}
        };
    } catch (const std::exception& e) {
        return {{"success", false}, {"error", e.what()}};
    }
}

} // namespace reimpl_p2b

// ==========================================================================
// P2b: playback.playPath
// ==========================================================================

TEST_F(PlaybackHandlerTest, PlayPath_EmptyPath_ReturnsError) {
    json params = {{"path", ""}};
    auto result = reimpl_p2b::PlaybackPlayPath(&mock, params);
    EXPECT_FALSE(result["success"].get<bool>());
    EXPECT_EQ(result["error"].get<std::string>(), "path is required");
    EXPECT_EQ(mock.playSinglePathCallCount, 0);
}

TEST_F(PlaybackHandlerTest, PlayPath_MissingPath_ReturnsError) {
    auto result = reimpl_p2b::PlaybackPlayPath(&mock, emptyParams);
    EXPECT_FALSE(result["success"].get<bool>());
    EXPECT_EQ(mock.playSinglePathCallCount, 0);
}

TEST_F(PlaybackHandlerTest, PlayPath_Success_ReturnsTracksAdded) {
    mock.singlePathResult = {true, "", 1, 0};
    json params = {{"path", "/music/test.mp3"}};
    auto result = reimpl_p2b::PlaybackPlayPath(&mock, params);
    EXPECT_TRUE(result["success"].get<bool>());
    EXPECT_EQ(result["tracksAdded"].get<int>(), 1);
    EXPECT_EQ(result["path"].get<std::string>(), "/music/test.mp3");
    EXPECT_EQ(result["subsong"].get<int>(), 0);
    EXPECT_EQ(mock.playSinglePathCallCount, 1);
    EXPECT_EQ(mock.lastPlaySinglePath, "/music/test.mp3");
    EXPECT_EQ(mock.lastPlaySingleSubsong, 0);
    EXPECT_FALSE(mock.lastPlaySingleSubsongRequested);
}

TEST_F(PlaybackHandlerTest, PlayPath_WithSubsong_ParsesCorrectly) {
    mock.singlePathResult = {true, "", 1, 3};
    json params = {{"path", "/music/album.cue|subsong:3"}};
    auto result = reimpl_p2b::PlaybackPlayPath(&mock, params);
    EXPECT_TRUE(result["success"].get<bool>());
    EXPECT_EQ(result["subsong"].get<int>(), 3);
    EXPECT_EQ(mock.lastPlaySinglePath, "/music/album.cue");
    EXPECT_EQ(mock.lastPlaySingleSubsong, 3);
    EXPECT_TRUE(mock.lastPlaySingleSubsongRequested);
}

TEST_F(PlaybackHandlerTest, PlayPath_ServiceFails_ReturnsError) {
    mock.singlePathResult = {false, "Could not resolve path to playable content", 0, 0};
    json params = {{"path", "/music/missing.mp3"}};
    auto result = reimpl_p2b::PlaybackPlayPath(&mock, params);
    EXPECT_FALSE(result["success"].get<bool>());
    EXPECT_EQ(result["error"].get<std::string>(), "Could not resolve path to playable content");
    EXPECT_EQ(result["path"].get<std::string>(), "/music/missing.mp3");
}

// ==========================================================================
// P2b: playback.playPaths
// ==========================================================================

TEST_F(PlaybackHandlerTest, PlayPaths_NoPaths_ReturnsError) {
    auto result = reimpl_p2b::PlaybackPlayPaths(&mock, emptyParams);
    EXPECT_FALSE(result["success"].get<bool>());
    EXPECT_EQ(result["error"].get<std::string>(), "paths array is required");
    EXPECT_EQ(mock.playMultiplePathsCallCount, 0);
}

TEST_F(PlaybackHandlerTest, PlayPaths_InvalidPaths_ReturnsError) {
    json params = {{"paths", "not-an-array"}};
    auto result = reimpl_p2b::PlaybackPlayPaths(&mock, params);
    EXPECT_FALSE(result["success"].get<bool>());
    EXPECT_EQ(mock.playMultiplePathsCallCount, 0);
}

TEST_F(PlaybackHandlerTest, PlayPaths_Success_ReturnsTracksAdded) {
    mock.multiPathResult = {true, "", 3, 0};
    json params = {
        {"paths", json::array({"/a.mp3", "/b.mp3", "/c.mp3"})}
    };
    auto result = reimpl_p2b::PlaybackPlayPaths(&mock, params);
    EXPECT_TRUE(result["success"].get<bool>());
    EXPECT_EQ(result["tracksAdded"].get<size_t>(), 3u);
    EXPECT_EQ(result["startedAt"].get<size_t>(), 0u);
    EXPECT_EQ(mock.playMultiplePathsCallCount, 1);
    EXPECT_EQ(mock.lastPlayMultiplePaths.size(), 3u);
    EXPECT_EQ(mock.lastPlayMultiplePaths[0], "/a.mp3");
    EXPECT_EQ(mock.lastPlayMultiplePaths[2], "/c.mp3");
    EXPECT_EQ(mock.lastPlayMultipleStartIndex, 0u);
}

TEST_F(PlaybackHandlerTest, PlayPaths_WithStartIndex_PassesThrough) {
    mock.multiPathResult = {true, "", 2, 1};
    json params = {
        {"paths", json::array({"/a.mp3", "/b.mp3"})},
        {"startIndex", 1}
    };
    auto result = reimpl_p2b::PlaybackPlayPaths(&mock, params);
    EXPECT_TRUE(result["success"].get<bool>());
    EXPECT_EQ(result["startedAt"].get<size_t>(), 1u);
    EXPECT_EQ(mock.lastPlayMultipleStartIndex, 1u);
}

TEST_F(PlaybackHandlerTest, PlayPaths_ServiceFails_ReturnsError) {
    mock.multiPathResult = {false, "No playable content found", 0, 0};
    json params = {
        {"paths", json::array({"/missing.mp3"})}
    };
    auto result = reimpl_p2b::PlaybackPlayPaths(&mock, params);
    EXPECT_FALSE(result["success"].get<bool>());
    EXPECT_EQ(result["error"].get<std::string>(), "No playable content found");
}
