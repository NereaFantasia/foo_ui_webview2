#pragma once
// MockPlaybackService.h - Test mock for IPlaybackService
// Phase 4: Enables offline testing of PlaybackApi handler logic.

#include "../../src/interfaces/IPlaybackService.h"
#include <string>
#include <nlohmann/json.hpp>

class MockPlaybackService : public IPlaybackService {
public:
    // Configurable state
    bool playing = false;
    bool paused = false;
    bool canSeek = true;
    bool muted = false;
    float volumeDb = 0.0f;  // 0 dB = full volume

    // Call tracking
    int playCallCount = 0;
    int pauseCallCount = 0;
    int stopCallCount = 0;
    int playOrPauseCallCount = 0;
    int nextCallCount = 0;
    int previousCallCount = 0;
    int startRandomCallCount = 0;
    bool lastPauseArg = false;

    // P1a: Volume call tracking
    int setVolumeCallCount = 0;
    int volumeUpCallCount = 0;
    int volumeDownCallCount = 0;
    int muteToggleCallCount = 0;
    float lastSetVolumeDb = 0.0f;

    // P1d: Stop after current state
    bool stopAfterCurrent = false;
    int setStopAfterCurrentCallCount = 0;
    int toggleStopAfterCurrentCallCount = 0;

    // P1b: Position/seek state
    double position = 0.0;
    double seekTarget = -1.0;
    int seekCallCount = 0;
    mutable int getNowPlayingInfoCallCount = 0;
    NowPlayingInfo nowPlayingInfo;  // configurable by test

    // P2: Current track JSON (configurable by test)
    nlohmann::json currentTrackJson = nullptr;
    mutable int getCurrentTrackJsonCallCount = 0;

    // P2: Playing item location (configurable by test)
    PlayingItemLocation playingItemLocation;
    mutable int getPlayingItemLocationCallCount = 0;

    // P2: Track info at specific position (configurable by test)
    nlohmann::json trackInfoAtResult = nullptr;
    mutable int getTrackInfoAtCallCount = 0;
    mutable size_t lastTrackInfoAtPlaylist = SIZE_MAX;
    mutable size_t lastTrackInfoAtIndex = SIZE_MAX;

    // P2: Playing playlist (configurable by test)
    size_t playingPlaylist = SIZE_MAX;
    std::string playingPlaylistName;
    mutable int getPlayingPlaylistCallCount = 0;
    mutable int getPlaylistNameCallCount = 0;

    // P2b: Path playback (configurable by test)
    PathPlayResult singlePathResult;
    int playSinglePathCallCount = 0;
    std::string lastPlaySinglePath;
    int lastPlaySingleSubsong = 0;
    bool lastPlaySingleSubsongRequested = false;

    MultiPathPlayResult multiPathResult;
    int playMultiplePathsCallCount = 0;
    std::vector<std::string> lastPlayMultiplePaths;
    size_t lastPlayMultipleStartIndex = 0;

    void Reset() {
        playing = false;
        paused = false;
        canSeek = true;
        muted = false;
        volumeDb = 0.0f;
        playCallCount = 0;
        pauseCallCount = 0;
        stopCallCount = 0;
        playOrPauseCallCount = 0;
        nextCallCount = 0;
        previousCallCount = 0;
        startRandomCallCount = 0;
        lastPauseArg = false;
        setVolumeCallCount = 0;
        volumeUpCallCount = 0;
        volumeDownCallCount = 0;
        muteToggleCallCount = 0;
        lastSetVolumeDb = 0.0f;
        stopAfterCurrent = false;
        setStopAfterCurrentCallCount = 0;
        toggleStopAfterCurrentCallCount = 0;
        position = 0.0;
        seekTarget = -1.0;
        seekCallCount = 0;
        getNowPlayingInfoCallCount = 0;
        nowPlayingInfo = NowPlayingInfo{};
        activePlaybackOrder = 0;
        setPlaybackOrderCallCount = 0;
        lastSetPlaybackOrder = -1;
        // P2
        currentTrackJson = nullptr;
        getCurrentTrackJsonCallCount = 0;
        playingItemLocation = PlayingItemLocation{};
        getPlayingItemLocationCallCount = 0;
        trackInfoAtResult = nullptr;
        getTrackInfoAtCallCount = 0;
        lastTrackInfoAtPlaylist = SIZE_MAX;
        lastTrackInfoAtIndex = SIZE_MAX;
        playingPlaylist = SIZE_MAX;
        playingPlaylistName.clear();
        getPlayingPlaylistCallCount = 0;
        getPlaylistNameCallCount = 0;
        // P2b
        singlePathResult = PathPlayResult{};
        playSinglePathCallCount = 0;
        lastPlaySinglePath.clear();
        lastPlaySingleSubsong = 0;
        lastPlaySingleSubsongRequested = false;
        multiPathResult = MultiPathPlayResult{};
        playMultiplePathsCallCount = 0;
        lastPlayMultiplePaths.clear();
        lastPlayMultipleStartIndex = 0;
    }

    // IPlaybackService implementation
    void play_or_unpause() override {
        playCallCount++;
        playing = true;
        paused = false;
    }

    void pause(bool p) override {
        pauseCallCount++;
        lastPauseArg = p;
        if (p) paused = true;
        else paused = false;
    }

    void stop() override {
        stopCallCount++;
        playing = false;
        paused = false;
    }

    bool is_playing() const override { return playing; }
    bool is_paused() const override { return paused; }
    bool playback_can_seek() const override { return canSeek; }
    float get_volume() const override { return volumeDb; }
    bool is_muted() const override { return muted; }

    // P0-ext: Transport extensions
    void play_or_pause() override {
        playOrPauseCallCount++;
        // Toggle: if playing → pause, if paused/stopped → play
        if (playing && !paused) {
            paused = true;
        } else {
            playing = true;
            paused = false;
        }
    }

    void next() override {
        nextCallCount++;
    }

    void previous() override {
        previousCallCount++;
    }

    void start(unsigned /*command*/, bool p = false) override {
        startRandomCallCount++;
        playing = true;
        paused = p;
    }

    // P1a: Volume control
    void set_volume(float db) override {
        setVolumeCallCount++;
        lastSetVolumeDb = db;
        volumeDb = db;
    }

    void volume_up() override {
        volumeUpCallCount++;
    }

    void volume_down() override {
        volumeDownCallCount++;
    }

    void volume_mute_toggle() override {
        muteToggleCallCount++;
        muted = !muted;
    }

    // P1d: Stop after current
    bool get_stop_after_current() const override {
        return stopAfterCurrent;
    }

    void set_stop_after_current(bool enabled) override {
        setStopAfterCurrentCallCount++;
        stopAfterCurrent = enabled;
    }

    void toggle_stop_after_current() override {
        toggleStopAfterCurrentCallCount++;
        stopAfterCurrent = !stopAfterCurrent;
    }

    // P1c: Playback order
    int activePlaybackOrder = 0;
    int setPlaybackOrderCallCount = 0;
    int lastSetPlaybackOrder = -1;

    int playback_order_get_active() const override {
        return activePlaybackOrder;
    }

    void playback_order_set_active(int order) override {
        setPlaybackOrderCallCount++;
        lastSetPlaybackOrder = order;
        activePlaybackOrder = order;
    }

    // P1b: Position/seek
    double playback_get_position() const override {
        return position;
    }

    void playback_seek(double seconds) override {
        seekCallCount++;
        seekTarget = seconds;
        position = seconds;  // mock: seek immediately updates position
    }

    NowPlayingInfo get_now_playing_info() const override {
        getNowPlayingInfoCallCount++;
        return nowPlayingInfo;
    }

    // P2: Current track JSON
    nlohmann::json get_current_track_json() const override {
        getCurrentTrackJsonCallCount++;
        return currentTrackJson;
    }

    // P2: Playing item location
    PlayingItemLocation get_playing_item_location() const override {
        getPlayingItemLocationCallCount++;
        return playingItemLocation;
    }

    nlohmann::json get_track_info_at(size_t playlist, size_t index) const override {
        getTrackInfoAtCallCount++;
        lastTrackInfoAtPlaylist = playlist;
        lastTrackInfoAtIndex = index;
        return trackInfoAtResult;
    }

    // P2: Playing playlist
    size_t get_playing_playlist() const override {
        getPlayingPlaylistCallCount++;
        return playingPlaylist;
    }

    std::string get_playlist_name(size_t playlist) const override {
        getPlaylistNameCallCount++;
        return playingPlaylistName;
    }

    // P2b: Path playback
    PathPlayResult play_single_path(const std::string& filePath, int subsongIndex, bool subsongRequested) override {
        playSinglePathCallCount++;
        lastPlaySinglePath = filePath;
        lastPlaySingleSubsong = subsongIndex;
        lastPlaySingleSubsongRequested = subsongRequested;
        return singlePathResult;
    }

    MultiPathPlayResult play_multiple_paths(const std::vector<std::string>& paths, size_t startIndex, bool replace = false) override {
        playMultiplePathsCallCount++;
        lastPlayMultiplePaths = paths;
        lastPlayMultipleStartIndex = startIndex;
        return multiPathResult;
    }
};
