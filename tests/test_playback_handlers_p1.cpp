// test_playback_handlers_p1.cpp - PlaybackApi volume, stop-after-current, order and seek handlers
// Validates PlaybackApi handler logic using MockPlaybackService.
// Uses reimpl pattern: handler functions mirror product code but accept
// IPlaybackService* instead of calling playback_control::get().
#include "pch.h"
#include "mocks/MockPlaybackService.h"
#include "harness/PlaybackHandlerTestSupport.h"

using json = nlohmann::json;

// ==========================================================================
// P1a: Volume control reimpl handlers
// ==========================================================================
namespace reimpl_p1a {

json PlaybackSetVolume(IPlaybackService* svc, const json& params) {
    float volume = params.value("volume", 100.0f);
    float db;
    if (volume <= 0.0f) {
        db = -100.0f;
    } else {
        db = 20.0f * std::log10(volume / 100.0f);
        db = std::max(-100.0f, std::min(0.0f, db));
    }
    svc->set_volume(db);
    return { {"success", true} };
}

json PlaybackVolumeUp(IPlaybackService* svc, const json& /*params*/) {
    svc->volume_up();
    return { {"success", true} };
}

json PlaybackVolumeDown(IPlaybackService* svc, const json& /*params*/) {
    svc->volume_down();
    return { {"success", true} };
}

json PlaybackMute(IPlaybackService* svc, const json& params) {
    bool muted = params.value("muted", true);
    bool currentMuted = svc->is_muted();
    if (muted != currentMuted) {
        svc->volume_mute_toggle();
    }
    return { {"success", true} };
}

json PlaybackToggleMute(IPlaybackService* svc, const json& /*params*/) {
    bool currentMuted = svc->is_muted();
    svc->volume_mute_toggle();
    return {
        {"success", true},
        {"muted", !currentMuted},
    };
}

} // namespace reimpl_p1a

// ==========================================================================
// P1a: playback.setVolume
// ==========================================================================

TEST_F(PlaybackHandlerTest, SetVolume_100Percent_SetsTo0dB) {
    json params = { {"volume", 100.0f} };
    auto result = reimpl_p1a::PlaybackSetVolume(&mock, params);
    EXPECT_TRUE(result["success"].get<bool>());
    EXPECT_NEAR(mock.lastSetVolumeDb, 0.0f, 0.01f);
    EXPECT_EQ(mock.setVolumeCallCount, 1);
}

TEST_F(PlaybackHandlerTest, SetVolume_0Percent_SetsToMinus100dB) {
    json params = { {"volume", 0.0f} };
    reimpl_p1a::PlaybackSetVolume(&mock, params);
    EXPECT_FLOAT_EQ(mock.lastSetVolumeDb, -100.0f);
}

TEST_F(PlaybackHandlerTest, SetVolume_50Percent_SetsToAboutMinus6dB) {
    json params = { {"volume", 50.0f} };
    reimpl_p1a::PlaybackSetVolume(&mock, params);
    // 20 * log10(0.5) ≈ -6.02 dB
    EXPECT_NEAR(mock.lastSetVolumeDb, -6.02f, 0.1f);
}

TEST_F(PlaybackHandlerTest, SetVolume_NegativeValue_ClampsToMinus100dB) {
    json params = { {"volume", -10.0f} };
    reimpl_p1a::PlaybackSetVolume(&mock, params);
    EXPECT_FLOAT_EQ(mock.lastSetVolumeDb, -100.0f);
}

TEST_F(PlaybackHandlerTest, SetVolume_DefaultParamIs100) {
    // No "volume" key → default 100.0f → 0 dB
    reimpl_p1a::PlaybackSetVolume(&mock, emptyParams);
    EXPECT_NEAR(mock.lastSetVolumeDb, 0.0f, 0.01f);
}

// ==========================================================================
// P1a: playback.volumeUp
// ==========================================================================

TEST_F(PlaybackHandlerTest, VolumeUp_CallsVolumeUp) {
    auto result = reimpl_p1a::PlaybackVolumeUp(&mock, emptyParams);
    EXPECT_TRUE(result["success"].get<bool>());
    EXPECT_EQ(mock.volumeUpCallCount, 1);
}

// ==========================================================================
// P1a: playback.volumeDown
// ==========================================================================

TEST_F(PlaybackHandlerTest, VolumeDown_CallsVolumeDown) {
    auto result = reimpl_p1a::PlaybackVolumeDown(&mock, emptyParams);
    EXPECT_TRUE(result["success"].get<bool>());
    EXPECT_EQ(mock.volumeDownCallCount, 1);
}

// ==========================================================================
// P1a: playback.mute
// ==========================================================================

TEST_F(PlaybackHandlerTest, Mute_RequestMuteWhenNotMuted_Toggles) {
    mock.muted = false;
    json params = { {"muted", true} };
    reimpl_p1a::PlaybackMute(&mock, params);
    EXPECT_EQ(mock.muteToggleCallCount, 1);
    EXPECT_TRUE(mock.muted);  // toggled from false → true
}

TEST_F(PlaybackHandlerTest, Mute_RequestMuteWhenAlreadyMuted_NoToggle) {
    mock.muted = true;
    json params = { {"muted", true} };
    reimpl_p1a::PlaybackMute(&mock, params);
    EXPECT_EQ(mock.muteToggleCallCount, 0);  // already muted, no change
    EXPECT_TRUE(mock.muted);
}

TEST_F(PlaybackHandlerTest, Mute_RequestUnmuteWhenMuted_Toggles) {
    mock.muted = true;
    json params = { {"muted", false} };
    reimpl_p1a::PlaybackMute(&mock, params);
    EXPECT_EQ(mock.muteToggleCallCount, 1);
    EXPECT_FALSE(mock.muted);  // toggled from true → false
}

TEST_F(PlaybackHandlerTest, Mute_RequestUnmuteWhenNotMuted_NoToggle) {
    mock.muted = false;
    json params = { {"muted", false} };
    reimpl_p1a::PlaybackMute(&mock, params);
    EXPECT_EQ(mock.muteToggleCallCount, 0);  // already unmuted, no change
    EXPECT_FALSE(mock.muted);
}

TEST_F(PlaybackHandlerTest, Mute_DefaultParamIsMuteTrue) {
    mock.muted = false;
    // No "muted" key → default true → should toggle
    reimpl_p1a::PlaybackMute(&mock, emptyParams);
    EXPECT_EQ(mock.muteToggleCallCount, 1);
    EXPECT_TRUE(mock.muted);
}

// ==========================================================================
// P1a: playback.toggleMute
// ==========================================================================

TEST_F(PlaybackHandlerTest, ToggleMute_FromNotMuted_ReturnsMutedTrue) {
    mock.muted = false;
    auto result = reimpl_p1a::PlaybackToggleMute(&mock, emptyParams);
    EXPECT_TRUE(result["success"].get<bool>());
    EXPECT_TRUE(result["muted"].get<bool>());
    EXPECT_EQ(mock.muteToggleCallCount, 1);
    EXPECT_TRUE(mock.muted);
}

TEST_F(PlaybackHandlerTest, ToggleMute_FromMuted_ReturnsMutedFalse) {
    mock.muted = true;
    auto result = reimpl_p1a::PlaybackToggleMute(&mock, emptyParams);
    EXPECT_TRUE(result["success"].get<bool>());
    EXPECT_FALSE(result["muted"].get<bool>());
    EXPECT_EQ(mock.muteToggleCallCount, 1);
    EXPECT_FALSE(mock.muted);
}

// ==========================================================================
// P1d: StopAfterCurrent reimpl handlers
// ==========================================================================
namespace reimpl_p1d {

json PlaybackGetStopAfterCurrent(IPlaybackService* svc, const json& /*params*/) {
    return {
        {"enabled", svc->get_stop_after_current()},
    };
}

json PlaybackSetStopAfterCurrent(IPlaybackService* svc, const json& params) {
    bool enabled = params.value("enabled", false);
    svc->set_stop_after_current(enabled);
    return {
        {"success", true},
        {"enabled", enabled},
    };
}

json PlaybackToggleStopAfterCurrent(IPlaybackService* svc, const json& /*params*/) {
    svc->toggle_stop_after_current();
    return {
        {"enabled", svc->get_stop_after_current()},
    };
}

} // namespace reimpl_p1d

// ==========================================================================
// P1d: playback.getStopAfterCurrent
// ==========================================================================

TEST_F(PlaybackHandlerTest, GetStopAfterCurrent_WhenDisabled) {
    mock.stopAfterCurrent = false;
    auto result = reimpl_p1d::PlaybackGetStopAfterCurrent(&mock, emptyParams);
    EXPECT_FALSE(result["enabled"].get<bool>());
}

TEST_F(PlaybackHandlerTest, GetStopAfterCurrent_WhenEnabled) {
    mock.stopAfterCurrent = true;
    auto result = reimpl_p1d::PlaybackGetStopAfterCurrent(&mock, emptyParams);
    EXPECT_TRUE(result["enabled"].get<bool>());
}

// ==========================================================================
// P1d: playback.setStopAfterCurrent
// ==========================================================================

TEST_F(PlaybackHandlerTest, SetStopAfterCurrent_EnablesIt) {
    mock.stopAfterCurrent = false;
    json params = { {"enabled", true} };
    auto result = reimpl_p1d::PlaybackSetStopAfterCurrent(&mock, params);
    EXPECT_TRUE(result["success"].get<bool>());
    EXPECT_TRUE(result["enabled"].get<bool>());
    EXPECT_TRUE(mock.stopAfterCurrent);
    EXPECT_EQ(mock.setStopAfterCurrentCallCount, 1);
}

TEST_F(PlaybackHandlerTest, SetStopAfterCurrent_DisablesIt) {
    mock.stopAfterCurrent = true;
    json params = { {"enabled", false} };
    reimpl_p1d::PlaybackSetStopAfterCurrent(&mock, params);
    EXPECT_FALSE(mock.stopAfterCurrent);
}

TEST_F(PlaybackHandlerTest, SetStopAfterCurrent_DefaultParamIsFalse) {
    mock.stopAfterCurrent = true;
    reimpl_p1d::PlaybackSetStopAfterCurrent(&mock, emptyParams);
    EXPECT_FALSE(mock.stopAfterCurrent);  // default = false
}

// ==========================================================================
// P1d: playback.toggleStopAfterCurrent
// ==========================================================================

TEST_F(PlaybackHandlerTest, ToggleStopAfterCurrent_FromDisabled_ReturnsEnabled) {
    mock.stopAfterCurrent = false;
    auto result = reimpl_p1d::PlaybackToggleStopAfterCurrent(&mock, emptyParams);
    EXPECT_TRUE(result["enabled"].get<bool>());
    EXPECT_TRUE(mock.stopAfterCurrent);
    EXPECT_EQ(mock.toggleStopAfterCurrentCallCount, 1);
}

TEST_F(PlaybackHandlerTest, ToggleStopAfterCurrent_FromEnabled_ReturnsDisabled) {
    mock.stopAfterCurrent = true;
    auto result = reimpl_p1d::PlaybackToggleStopAfterCurrent(&mock, emptyParams);
    EXPECT_FALSE(result["enabled"].get<bool>());
    EXPECT_FALSE(mock.stopAfterCurrent);
}

// ==========================================================================
// P1c: PlaybackOrder reimpl handlers
// ==========================================================================
namespace reimpl_p1c {

std::string OrderToString(int order) {
    switch (order) {
        case 0: return "default";
        case 1: return "repeat-playlist";
        case 2: return "repeat-track";
        case 3: return "random";
        case 4: return "shuffle-tracks";
        case 5: return "shuffle-albums";
        case 6: return "shuffle-folders";
        default: return "default";
    }
}

int StringToOrder(const std::string& str) {
    if (str == "repeat-playlist") return 1;
    if (str == "repeat-track") return 2;
    if (str == "random") return 3;
    if (str == "shuffle-tracks") return 4;
    if (str == "shuffle-albums") return 5;
    if (str == "shuffle-folders") return 6;
    return 0;
}

json PlaybackGetPlaybackOrder(IPlaybackService* svc, const json& /*params*/) {
    int order = svc->playback_order_get_active();
    std::string orderName = OrderToString(order);
    return {
        {"order", order},
        {"orderName", orderName},
        {"name", orderName},
        {"orderIndex", order},
    };
}

json PlaybackSetPlaybackOrder(IPlaybackService* svc, const json& params) {
    int order = 0;
    if (params.contains("order")) {
        auto& orderParam = params["order"];
        if (orderParam.is_number()) {
            order = orderParam.get<int>();
        } else if (orderParam.is_string()) {
            order = StringToOrder(orderParam.get<std::string>());
        }
    }
    svc->playback_order_set_active(order);
    return {
        {"success", true},
        {"order", order},
        {"orderName", OrderToString(order)},
    };
}

} // namespace reimpl_p1c

// ==========================================================================
// P1c: playback.getPlaybackOrder
// ==========================================================================

TEST_F(PlaybackHandlerTest, GetPlaybackOrder_Default) {
    mock.activePlaybackOrder = 0;
    auto result = reimpl_p1c::PlaybackGetPlaybackOrder(&mock, emptyParams);
    EXPECT_EQ(result["order"].get<int>(), 0);
    EXPECT_EQ(result["orderName"].get<std::string>(), "default");
}

TEST_F(PlaybackHandlerTest, GetPlaybackOrder_RepeatPlaylist) {
    mock.activePlaybackOrder = 1;
    auto result = reimpl_p1c::PlaybackGetPlaybackOrder(&mock, emptyParams);
    EXPECT_EQ(result["order"].get<int>(), 1);
    EXPECT_EQ(result["orderName"].get<std::string>(), "repeat-playlist");
}

TEST_F(PlaybackHandlerTest, GetPlaybackOrder_ShuffleTracks) {
    mock.activePlaybackOrder = 4;
    auto result = reimpl_p1c::PlaybackGetPlaybackOrder(&mock, emptyParams);
    EXPECT_EQ(result["order"].get<int>(), 4);
    EXPECT_EQ(result["orderName"].get<std::string>(), "shuffle-tracks");
}

// ==========================================================================
// P1c: playback.setPlaybackOrder
// ==========================================================================

TEST_F(PlaybackHandlerTest, SetPlaybackOrder_ByNumber) {
    json params = { {"order", 3} };
    auto result = reimpl_p1c::PlaybackSetPlaybackOrder(&mock, params);
    EXPECT_TRUE(result["success"].get<bool>());
    EXPECT_EQ(result["order"].get<int>(), 3);
    EXPECT_EQ(result["orderName"].get<std::string>(), "random");
    EXPECT_EQ(mock.activePlaybackOrder, 3);
}

TEST_F(PlaybackHandlerTest, SetPlaybackOrder_ByString) {
    json params = { {"order", "shuffle-albums"} };
    auto result = reimpl_p1c::PlaybackSetPlaybackOrder(&mock, params);
    EXPECT_TRUE(result["success"].get<bool>());
    EXPECT_EQ(result["order"].get<int>(), 5);
    EXPECT_EQ(result["orderName"].get<std::string>(), "shuffle-albums");
    EXPECT_EQ(mock.activePlaybackOrder, 5);
}

TEST_F(PlaybackHandlerTest, SetPlaybackOrder_UnknownString_DefaultsToZero) {
    json params = { {"order", "nonexistent-order"} };
    auto result = reimpl_p1c::PlaybackSetPlaybackOrder(&mock, params);
    EXPECT_EQ(result["order"].get<int>(), 0);
    EXPECT_EQ(result["orderName"].get<std::string>(), "default");
}

TEST_F(PlaybackHandlerTest, SetPlaybackOrder_NoParam_DefaultsToZero) {
    auto result = reimpl_p1c::PlaybackSetPlaybackOrder(&mock, emptyParams);
    EXPECT_EQ(result["order"].get<int>(), 0);
}

TEST_F(PlaybackHandlerTest, GetPlaybackOrder_AllOrders) {
    // Verify all 7 known orders map correctly
    const std::pair<int, std::string> orders[] = {
        {0, "default"}, {1, "repeat-playlist"}, {2, "repeat-track"},
        {3, "random"}, {4, "shuffle-tracks"}, {5, "shuffle-albums"},
        {6, "shuffle-folders"},
    };
    for (auto& [idx, name] : orders) {
        mock.activePlaybackOrder = idx;
        auto result = reimpl_p1c::PlaybackGetPlaybackOrder(&mock, emptyParams);
        EXPECT_EQ(result["orderName"].get<std::string>(), name);
    }
}

TEST_F(PlaybackHandlerTest, SetPlaybackOrder_StringToIntRoundTrip) {
    // Set by string, then get → should match
    json params = { {"order", "repeat-track"} };
    reimpl_p1c::PlaybackSetPlaybackOrder(&mock, params);
    auto result = reimpl_p1c::PlaybackGetPlaybackOrder(&mock, emptyParams);
    EXPECT_EQ(result["order"].get<int>(), 2);
    EXPECT_EQ(result["orderName"].get<std::string>(), "repeat-track");
}

// ==========================================================================
// P1b: Position/Seek reimpl handlers
// ==========================================================================
namespace reimpl_p1b {

json PlaybackGetPosition(IPlaybackService* svc, const json& /*params*/) {
    double position = svc->playback_get_position();
    auto info = svc->get_now_playing_info();
    return {
        {"position", position},
        {"duration", info.duration},
        {"subsong", info.subsong},
        {"path", info.path},
    };
}

json PlaybackSetPosition(IPlaybackService* svc, const json& params) {
    double seconds = params.contains("position") ? params.value("position", 0.0) : params.value("seconds", 0.0);

    if (!svc->playback_can_seek()) {
        return {
            {"success", false},
            {"error", "Cannot seek in current track"},
        };
    }

    auto info = svc->get_now_playing_info();
    double duration = info.duration;
    unsigned subsong = info.subsong;

    if (seconds < 0) {
        seconds = 0;
    }
    if (duration > 0 && seconds > duration) {
        seconds = duration - 0.1;
        if (seconds < 0) seconds = 0;
    }

    double oldPosition = svc->playback_get_position();
    svc->playback_seek(seconds);
    double newPosition = svc->playback_get_position();

    return {
        {"success", true},
        {"requestedPosition", params.contains("position") ? params.value("position", 0.0) : params.value("seconds", 0.0)},
        {"actualPosition", seconds},
        {"newPosition", newPosition},
        {"oldPosition", oldPosition},
        {"duration", duration},
        {"subsong", subsong}
    };
}

} // namespace reimpl_p1b

// ==========================================================================
// P1b: playback.getPosition
// ==========================================================================

TEST_F(PlaybackHandlerTest, GetPosition_ReturnsCurrentPosition) {
    mock.position = 42.5;
    auto result = reimpl_p1b::PlaybackGetPosition(&mock, emptyParams);
    EXPECT_DOUBLE_EQ(result["position"].get<double>(), 42.5);
}

TEST_F(PlaybackHandlerTest, GetPosition_NoTrackPlaying_DurationIsZero) {
    // nowPlayingInfo default: found=false, duration=0
    mock.position = 0.0;
    auto result = reimpl_p1b::PlaybackGetPosition(&mock, emptyParams);
    EXPECT_DOUBLE_EQ(result["duration"].get<double>(), 0.0);
    EXPECT_EQ(result["subsong"].get<unsigned>(), 0u);
    EXPECT_EQ(result["path"].get<std::string>(), "");
}

TEST_F(PlaybackHandlerTest, GetPosition_WithTrackInfo) {
    mock.position = 30.0;
    mock.nowPlayingInfo = {true, 240.0, 0, "file://test.mp3"};
    auto result = reimpl_p1b::PlaybackGetPosition(&mock, emptyParams);
    EXPECT_DOUBLE_EQ(result["position"].get<double>(), 30.0);
    EXPECT_DOUBLE_EQ(result["duration"].get<double>(), 240.0);
    EXPECT_EQ(result["path"].get<std::string>(), "file://test.mp3");
}

// ==========================================================================
// P1b: playback.setPosition
// ==========================================================================

TEST_F(PlaybackHandlerTest, SetPosition_SeeksToPosition) {
    mock.position = 10.0;
    mock.nowPlayingInfo = {true, 240.0, 0, ""};
    json params = { {"position", 60.0} };
    auto result = reimpl_p1b::PlaybackSetPosition(&mock, params);
    EXPECT_TRUE(result["success"].get<bool>());
    EXPECT_DOUBLE_EQ(result["actualPosition"].get<double>(), 60.0);
    EXPECT_DOUBLE_EQ(result["oldPosition"].get<double>(), 10.0);
    EXPECT_EQ(mock.seekCallCount, 1);
    EXPECT_DOUBLE_EQ(mock.seekTarget, 60.0);
}

TEST_F(PlaybackHandlerTest, SetPosition_CannotSeek_ReturnsError) {
    mock.canSeek = false;
    json params = { {"position", 30.0} };
    auto result = reimpl_p1b::PlaybackSetPosition(&mock, params);
    EXPECT_FALSE(result["success"].get<bool>());
    EXPECT_EQ(result["error"].get<std::string>(), "Cannot seek in current track");
    EXPECT_EQ(mock.seekCallCount, 0);
}

TEST_F(PlaybackHandlerTest, SetPosition_NegativePosition_ClampsToZero) {
    mock.nowPlayingInfo = {true, 240.0, 0, ""};
    json params = { {"position", -5.0} };
    auto result = reimpl_p1b::PlaybackSetPosition(&mock, params);
    EXPECT_TRUE(result["success"].get<bool>());
    EXPECT_DOUBLE_EQ(result["actualPosition"].get<double>(), 0.0);
    EXPECT_DOUBLE_EQ(mock.seekTarget, 0.0);
}

TEST_F(PlaybackHandlerTest, SetPosition_BeyondDuration_ClampsToEnd) {
    mock.nowPlayingInfo = {true, 180.0, 0, ""};
    json params = { {"position", 999.0} };
    auto result = reimpl_p1b::PlaybackSetPosition(&mock, params);
    EXPECT_TRUE(result["success"].get<bool>());
    // clamped to duration - 0.1 = 179.9
    EXPECT_NEAR(result["actualPosition"].get<double>(), 179.9, 0.01);
}

TEST_F(PlaybackHandlerTest, SetPosition_UsesSecondsParam) {
    mock.nowPlayingInfo = {true, 240.0, 0, ""};
    json params = { {"seconds", 45.0} };
    auto result = reimpl_p1b::PlaybackSetPosition(&mock, params);
    EXPECT_TRUE(result["success"].get<bool>());
    EXPECT_DOUBLE_EQ(result["actualPosition"].get<double>(), 45.0);
}

TEST_F(PlaybackHandlerTest, SetPosition_NoTrackInfo_NoBoundaryClamping) {
    // nowPlayingInfo.found=false, duration=0 → no upper bound clamping
    mock.nowPlayingInfo = {};
    json params = { {"position", 999.0} };
    auto result = reimpl_p1b::PlaybackSetPosition(&mock, params);
    EXPECT_TRUE(result["success"].get<bool>());
    EXPECT_DOUBLE_EQ(result["actualPosition"].get<double>(), 999.0);
}

TEST_F(PlaybackHandlerTest, SetPosition_ReturnsOldAndNewPosition) {
    mock.position = 20.0;
    mock.nowPlayingInfo = {true, 300.0, 0, ""};
    json params = { {"position", 100.0} };
    auto result = reimpl_p1b::PlaybackSetPosition(&mock, params);
    EXPECT_DOUBLE_EQ(result["oldPosition"].get<double>(), 20.0);
    // mock.seek sets position = seconds, so newPosition = 100.0
    EXPECT_DOUBLE_EQ(result["newPosition"].get<double>(), 100.0);
}
