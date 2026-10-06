// test_playback_handlers.cpp - PlaybackApi transport, state and volume query handlers
// Validates PlaybackApi handler logic using MockPlaybackService.
// Uses reimpl pattern: handler functions mirror product code but accept
// IPlaybackService* instead of calling playback_control::get().
#include "pch.h"
#include "mocks/MockPlaybackService.h"
#include "harness/PlaybackHandlerTestSupport.h"

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
