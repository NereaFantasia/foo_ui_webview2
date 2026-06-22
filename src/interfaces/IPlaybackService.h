#pragma once
// IPlaybackService.h - Abstract interface for playback operations
// Abstract interface for PlaybackApi handlers; enables offline testing.
//
// This interface wraps the subset of foobar2000 playback_control used by
// the most common API handlers. Additional methods will be added in
// subsequent batches as more handlers are interfaced.

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

class IPlaybackService {
public:
    virtual ~IPlaybackService() = default;

    // Transport controls
    /** @brief Start playback if stopped, or unpause if paused. */
    virtual void play_or_unpause() = 0;
    /** @brief Set the paused state (true = pause, false = resume). */
    virtual void pause(bool paused) = 0;
    /** @brief Stop playback and clear the now-playing item. */
    virtual void stop() = 0;

    // Transport extensions
    /** @brief Toggle between play and pause. */
    virtual void play_or_pause() = 0;		// SDK inline helper: toggle play/pause
    /** @brief Skip to the next track. */
    virtual void next() = 0;				// SDK inline helper: start(track_command_next)
    /** @brief Skip to the previous track. */
    virtual void previous() = 0;			// SDK inline helper: start(track_command_prev)
    /** @brief Issue a raw transport command, optionally starting paused. */
    virtual void start(unsigned command, bool paused = false) = 0; // command: playback_control::t_track_command enum (e.g. track_command_play=0, track_command_next=1, ...)

    // State queries
    /** @brief Whether playback is active (playing or paused). */
    virtual bool is_playing() const = 0;
    /** @brief Whether playback is currently paused. */
    virtual bool is_paused() const = 0;
    /** @brief Whether the current track supports seeking. */
    virtual bool playback_can_seek() const = 0;

    // Volume
    /** @brief Current volume in decibels (0 dB = unattenuated). */
    virtual float get_volume() const = 0;     // returns dB value
    /** @brief Whether output is currently muted. */
    virtual bool is_muted() const = 0;

    // Volume control
    /** @brief Set the volume in decibels. */
    virtual void set_volume(float db) = 0;
    /** @brief Raise the volume by one step. */
    virtual void volume_up() = 0;
    /** @brief Lower the volume by one step. */
    virtual void volume_down() = 0;
    /** @brief Toggle mute on/off. */
    virtual void volume_mute_toggle() = 0;

    // Stop after current
    /** @brief Whether "stop after current track" is armed. */
    virtual bool get_stop_after_current() const = 0;
    /** @brief Arm or disarm "stop after current track". */
    virtual void set_stop_after_current(bool enabled) = 0;
    /** @brief Toggle "stop after current track". */
    virtual void toggle_stop_after_current() = 0;

    // Playback order (delegates to playlist_manager in production)
    /** @brief Active playback-order index (default / repeat / shuffle / ...). */
    virtual int playback_order_get_active() const = 0;
    /** @brief Set the active playback-order by index. */
    virtual void playback_order_set_active(int order) = 0;

    // Position/seek
    /** @brief Current playback position in seconds. */
    virtual double playback_get_position() const = 0;
    /** @brief Seek to an absolute position in seconds. */
    virtual void playback_seek(double seconds) = 0;

    // Now playing info (simplified struct; full metadb integration deferred)
    struct NowPlayingInfo {
        bool found = false;
        double duration = 0.0;
        unsigned subsong = 0;
        std::string path;
    };
    /** @brief Lightweight now-playing summary (path, duration, subsong). */
    virtual NowPlayingInfo get_now_playing_info() const = 0;

    /** @brief Full now-playing metadata as JSON; null when nothing is playing. */
    virtual nlohmann::json get_current_track_json() const = 0;

    // Playing item location
    struct PlayingItemLocation {
        bool found = false;
        size_t playlist = SIZE_MAX;
        size_t index = SIZE_MAX;
    };
    /** @brief Playlist/index location of the now-playing item. */
    virtual PlayingItemLocation get_playing_item_location() const = 0;
    /** @brief Track metadata at the given playlist/index as JSON. */
    virtual nlohmann::json get_track_info_at(size_t playlist, size_t index) const = 0;

    // Playing playlist info
    /** @brief Index of the playlist that owns the now-playing item (SIZE_MAX if none). */
    virtual size_t get_playing_playlist() const = 0;  // SIZE_MAX if none
    /** @brief Display name of the playlist at the given index. */
    virtual std::string get_playlist_name(size_t playlist) const = 0;

    // Direct path playback
    struct PathPlayResult {
        bool success = false;
        std::string error;
        int tracksAdded = 0;
        int resolvedSubsong = 0;
    };
    /** @brief Play a single file path, optionally a specific subsong. */
    virtual PathPlayResult play_single_path(const std::string& filePath, int subsongIndex, bool subsongRequested) = 0;

    struct MultiPathPlayResult {
        bool success = false;
        std::string error;
        size_t tracksAdded = 0;
        size_t startedAt = 0;
    };
    /** @brief Play multiple paths from startIndex, optionally replacing the active playlist. */
    virtual MultiPathPlayResult play_multiple_paths(const std::vector<std::string>& paths, size_t startIndex, bool replace = false) = 0;
};
