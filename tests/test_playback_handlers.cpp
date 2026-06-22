// test_playback_handlers.cpp - Phase 4 offline handler logic tests
// Validates PlaybackApi handler logic using MockPlaybackService.
// Uses reimpl pattern: handler functions mirror product code but accept
// IPlaybackService* instead of calling playback_control::get().
#include "pch.h"
#include "mocks/MockPlaybackService.h"

using json = nlohmann::json;

// ==========================================================================
// Reimplemented handler functions (mirror PlaybackApi.cpp anonymous namespace)
// These must be kept in sync with the product handlers.
// ==========================================================================
namespace reimpl {

json PlaybackPlay(IPlaybackService* svc, const json& /*params*/) {
    svc->play_or_unpause();
    return { {"success", true} };
}

json PlaybackPause(IPlaybackService* svc, const json& /*params*/) {
    svc->pause(true);
    return { {"success", true} };
}

json PlaybackStop(IPlaybackService* svc, const json& /*params*/) {
    svc->stop();
    return { {"success", true} };
}

json PlaybackGetState(IPlaybackService* svc, const json& /*params*/) {
    std::string state = "stopped";
    if (svc->is_playing()) {
        state = svc->is_paused() ? "paused" : "playing";
    }
    return {
        {"state", state},
        {"canSeek", svc->playback_can_seek()},
        {"canPause", true},
    };
}

json PlaybackGetVolume(IPlaybackService* svc, const json& /*params*/) {
    float db = svc->get_volume();
    float volume;
    if (db <= -100.0f) {
        volume = 0.0f;
    } else {
        volume = 100.0f * std::pow(10.0f, db / 20.0f);
        volume = std::max(0.0f, std::min(100.0f, volume));
    }
    bool muted = svc->is_muted();
    return {
        {"volume", volume},
        {"volumeDb", db},
        {"muted", muted},
        {"isMuted", muted},
    };
}

} // namespace reimpl

// ==========================================================================
// P0-ext: Additional reimpl handlers for transport extensions
// These mirror PlaybackPlayPause, PlaybackPlayOrPause, PlaybackNext,
// PlaybackPrevious, and PlaybackRandom in PlaybackApi.cpp.
// ==========================================================================
namespace reimpl_ext {

json PlaybackPlayPause(IPlaybackService* svc, const json& /*params*/) {
    bool wasPlaying = svc->is_playing() && !svc->is_paused();
    svc->play_or_pause();
    return {
        {"success", true},
        {"isPlaying", !wasPlaying},
    };
}

json PlaybackNext(IPlaybackService* svc, const json& /*params*/) {
    svc->next();
    return { {"success", true} };
}

json PlaybackPrevious(IPlaybackService* svc, const json& /*params*/) {
    svc->previous();
    return { {"success", true} };
}

json PlaybackRandom(IPlaybackService* svc, const json& /*params*/) {
    svc->start(5 /*track_command_rand*/);
    return { {"success", true} };
}

} // namespace reimpl_ext

// ==========================================================================
// Test fixture
// ==========================================================================
class PlaybackHandlerTest : public ::testing::Test {
protected:
    MockPlaybackService mock;
    json emptyParams = json::object();

    void SetUp() override {
        mock.Reset();
    }
};

// ==========================================================================
// playback.play
// ==========================================================================

TEST_F(PlaybackHandlerTest, Play_CallsPlayOrUnpause) {
    auto result = reimpl::PlaybackPlay(&mock, emptyParams);
    EXPECT_TRUE(result["success"].get<bool>());
    EXPECT_EQ(mock.playCallCount, 1);
}

TEST_F(PlaybackHandlerTest, Play_SetsPlayingState) {
    mock.playing = false;
    reimpl::PlaybackPlay(&mock, emptyParams);
    EXPECT_TRUE(mock.playing);
    EXPECT_FALSE(mock.paused);
}

// ==========================================================================
// playback.pause
// ==========================================================================

TEST_F(PlaybackHandlerTest, Pause_CallsPauseTrue) {
    auto result = reimpl::PlaybackPause(&mock, emptyParams);
    EXPECT_TRUE(result["success"].get<bool>());
    EXPECT_EQ(mock.pauseCallCount, 1);
    EXPECT_TRUE(mock.lastPauseArg);
}

TEST_F(PlaybackHandlerTest, Pause_SetsPausedState) {
    mock.playing = true;
    reimpl::PlaybackPause(&mock, emptyParams);
    EXPECT_TRUE(mock.paused);
}

// ==========================================================================
// playback.stop
// ==========================================================================

TEST_F(PlaybackHandlerTest, Stop_CallsStop) {
    auto result = reimpl::PlaybackStop(&mock, emptyParams);
    EXPECT_TRUE(result["success"].get<bool>());
    EXPECT_EQ(mock.stopCallCount, 1);
}

TEST_F(PlaybackHandlerTest, Stop_ClearsPlayingState) {
    mock.playing = true;
    mock.paused = true;
    reimpl::PlaybackStop(&mock, emptyParams);
    EXPECT_FALSE(mock.playing);
    EXPECT_FALSE(mock.paused);
}

// ==========================================================================
// playback.getState
// ==========================================================================

TEST_F(PlaybackHandlerTest, GetState_Stopped) {
    mock.playing = false;
    auto result = reimpl::PlaybackGetState(&mock, emptyParams);
    EXPECT_EQ(result["state"], "stopped");
    EXPECT_TRUE(result["canPause"].get<bool>());
}

TEST_F(PlaybackHandlerTest, GetState_Playing) {
    mock.playing = true;
    mock.paused = false;
    auto result = reimpl::PlaybackGetState(&mock, emptyParams);
    EXPECT_EQ(result["state"], "playing");
}

TEST_F(PlaybackHandlerTest, GetState_Paused) {
    mock.playing = true;
    mock.paused = true;
    auto result = reimpl::PlaybackGetState(&mock, emptyParams);
    EXPECT_EQ(result["state"], "paused");
}

TEST_F(PlaybackHandlerTest, GetState_CanSeek) {
    mock.canSeek = true;
    auto result = reimpl::PlaybackGetState(&mock, emptyParams);
    EXPECT_TRUE(result["canSeek"].get<bool>());
}

TEST_F(PlaybackHandlerTest, GetState_CannotSeek) {
    mock.canSeek = false;
    auto result = reimpl::PlaybackGetState(&mock, emptyParams);
    EXPECT_FALSE(result["canSeek"].get<bool>());
}

// ==========================================================================
// playback.getVolume
// ==========================================================================

TEST_F(PlaybackHandlerTest, GetVolume_FullVolume) {
    mock.volumeDb = 0.0f;  // 0 dB = 100%
    auto result = reimpl::PlaybackGetVolume(&mock, emptyParams);
    EXPECT_NEAR(result["volume"].get<float>(), 100.0f, 0.1f);
    EXPECT_FLOAT_EQ(result["volumeDb"].get<float>(), 0.0f);
}

TEST_F(PlaybackHandlerTest, GetVolume_HalfVolume) {
    // -6.02 dB ≈ 50% linear
    mock.volumeDb = -6.02f;
    auto result = reimpl::PlaybackGetVolume(&mock, emptyParams);
    EXPECT_NEAR(result["volume"].get<float>(), 50.0f, 1.0f);
}

TEST_F(PlaybackHandlerTest, GetVolume_Silent) {
    mock.volumeDb = -100.0f;
    auto result = reimpl::PlaybackGetVolume(&mock, emptyParams);
    EXPECT_FLOAT_EQ(result["volume"].get<float>(), 0.0f);
}

TEST_F(PlaybackHandlerTest, GetVolume_MutedState) {
    mock.muted = true;
    auto result = reimpl::PlaybackGetVolume(&mock, emptyParams);
    EXPECT_TRUE(result["muted"].get<bool>());
    EXPECT_TRUE(result["isMuted"].get<bool>());
}

TEST_F(PlaybackHandlerTest, GetVolume_NotMuted) {
    mock.muted = false;
    auto result = reimpl::PlaybackGetVolume(&mock, emptyParams);
    EXPECT_FALSE(result["muted"].get<bool>());
}

// ==========================================================================
// P0-ext: playback.playPause / playback.playOrPause
// ==========================================================================

TEST_F(PlaybackHandlerTest, PlayPause_FromStopped_StartsPlaying) {
    mock.playing = false;
    mock.paused = false;
    auto result = reimpl_ext::PlaybackPlayPause(&mock, emptyParams);
    EXPECT_TRUE(result["success"].get<bool>());
    EXPECT_TRUE(result["isPlaying"].get<bool>());  // was stopped → now playing
    EXPECT_EQ(mock.playOrPauseCallCount, 1);
}

TEST_F(PlaybackHandlerTest, PlayPause_FromPlaying_PausesAndReturnsFalse) {
    mock.playing = true;
    mock.paused = false;
    auto result = reimpl_ext::PlaybackPlayPause(&mock, emptyParams);
    EXPECT_TRUE(result["success"].get<bool>());
    EXPECT_FALSE(result["isPlaying"].get<bool>());  // was playing → toggled to pause
    EXPECT_EQ(mock.playOrPauseCallCount, 1);
}

TEST_F(PlaybackHandlerTest, PlayPause_FromPaused_ResumesPlaying) {
    mock.playing = true;
    mock.paused = true;
    auto result = reimpl_ext::PlaybackPlayPause(&mock, emptyParams);
    EXPECT_TRUE(result["success"].get<bool>());
    EXPECT_TRUE(result["isPlaying"].get<bool>());  // was paused → now playing
}

// ==========================================================================
// P0-ext: playback.next
// ==========================================================================

TEST_F(PlaybackHandlerTest, Next_CallsNext) {
    auto result = reimpl_ext::PlaybackNext(&mock, emptyParams);
    EXPECT_TRUE(result["success"].get<bool>());
    EXPECT_EQ(mock.nextCallCount, 1);
}

// ==========================================================================
// P0-ext: playback.previous
// ==========================================================================

TEST_F(PlaybackHandlerTest, Previous_CallsPrevious) {
    auto result = reimpl_ext::PlaybackPrevious(&mock, emptyParams);
    EXPECT_TRUE(result["success"].get<bool>());
    EXPECT_EQ(mock.previousCallCount, 1);
}

// ==========================================================================
// P0-ext: playback.random
// ==========================================================================

TEST_F(PlaybackHandlerTest, Random_CallsStartWithRandomCommand) {
    auto result = reimpl_ext::PlaybackRandom(&mock, emptyParams);
    EXPECT_TRUE(result["success"].get<bool>());
    EXPECT_EQ(mock.startRandomCallCount, 1);
}

TEST_F(PlaybackHandlerTest, Random_StartsPlaying) {
    mock.playing = false;
    reimpl_ext::PlaybackRandom(&mock, emptyParams);
    EXPECT_TRUE(mock.playing);
    EXPECT_FALSE(mock.paused);  // default paused=false
}

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

// ==========================================================================
// P2a: Current Track / Track Index / Playing Playlist reimpl handlers
// ==========================================================================
namespace reimpl_p2a {

json PlaybackGetCurrentTrack(IPlaybackService* svc, const json& /*params*/) {
    auto trackJson = svc->get_current_track_json();
    if (trackJson.is_null()) {
        return { {"success", true}, {"found", false}, {"playing", false} };
    }
    return trackJson;
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
        auto trackJson = svc->get_track_info_at(loc.playlist, loc.index);
        if (!trackJson.is_null()) {
            result["track"] = trackJson;
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
    // currentTrackJson default is nullptr
    auto result = reimpl_p2a::PlaybackGetCurrentTrack(&mock, emptyParams);
    EXPECT_TRUE(result["success"].get<bool>());
    EXPECT_FALSE(result["found"].get<bool>());
    EXPECT_FALSE(result["playing"].get<bool>());
    EXPECT_EQ(mock.getCurrentTrackJsonCallCount, 1);
}

TEST_F(PlaybackHandlerTest, GetCurrentTrack_WithTrack_ReturnsTrackInfo) {
    mock.currentTrackJson = {
        {"id", "/music/test.mp3"},
        {"title", "Test Song"},
        {"artist", "Test Artist"},
        {"album", "Test Album"},
        {"duration", 240.5}
    };
    auto result = reimpl_p2a::PlaybackGetCurrentTrack(&mock, emptyParams);
    EXPECT_EQ(result["id"].get<std::string>(), "/music/test.mp3");
    EXPECT_EQ(result["title"].get<std::string>(), "Test Song");
    EXPECT_EQ(result["artist"].get<std::string>(), "Test Artist");
    EXPECT_EQ(result["album"].get<std::string>(), "Test Album");
    EXPECT_DOUBLE_EQ(result["duration"].get<double>(), 240.5);
    EXPECT_EQ(mock.getCurrentTrackJsonCallCount, 1);
}

TEST_F(PlaybackHandlerTest, GetCurrentTrack_WithSubsong_ReturnsFullPath) {
    mock.currentTrackJson = {
        {"id", "/music/album.cue|subsong:3"},
        {"title", "Track 3"},
        {"subsong", 3},
        {"fullPath", "/music/album.cue|subsong:3"}
    };
    auto result = reimpl_p2a::PlaybackGetCurrentTrack(&mock, emptyParams);
    EXPECT_EQ(result["subsong"].get<int>(), 3);
    EXPECT_EQ(result["fullPath"].get<std::string>(), "/music/album.cue|subsong:3");
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
    EXPECT_EQ(mock.getTrackInfoAtCallCount, 0);
}

TEST_F(PlaybackHandlerTest, GetCurrentTrackIndex_WithTrackInfo_IncludesTrack) {
    mock.playingItemLocation = {true, 1, 7};
    mock.trackInfoAtResult = {{"title", "Included Track"}, {"artist", "Someone"}};
    json params = {{"includeTrackInfo", true}};
    auto result = reimpl_p2a::PlaybackGetCurrentTrackIndex(&mock, params);
    EXPECT_TRUE(result["found"].get<bool>());
    EXPECT_TRUE(result.contains("track"));
    EXPECT_EQ(result["track"]["title"].get<std::string>(), "Included Track");
    EXPECT_EQ(mock.getTrackInfoAtCallCount, 1);
    EXPECT_EQ(mock.lastTrackInfoAtPlaylist, 1u);
    EXPECT_EQ(mock.lastTrackInfoAtIndex, 7u);
}

TEST_F(PlaybackHandlerTest, GetCurrentTrackIndex_TrackInfoNull_NoTrackField) {
    mock.playingItemLocation = {true, 0, 3};
    mock.trackInfoAtResult = nullptr;  // track info not available
    json params = {{"includeTrackInfo", true}};
    auto result = reimpl_p2a::PlaybackGetCurrentTrackIndex(&mock, params);
    EXPECT_TRUE(result["found"].get<bool>());
    EXPECT_FALSE(result.contains("track"));
    EXPECT_EQ(mock.getTrackInfoAtCallCount, 1);
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
