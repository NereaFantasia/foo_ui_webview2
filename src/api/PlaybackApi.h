#pragma once
#include "pch.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class IPlaybackService;

// ============================================
// Playback API Registration
// ============================================
//
// Registered APIs (29):
//   playback.play
//   playback.pause
//   playback.stop
//   playback.playPause
//   playback.playOrPause
//   playback.next
//   playback.previous
//   playback.random
//   playback.getState
//   playback.getPosition
//   playback.setPosition
//   playback.getVolume
//   playback.setVolume
//   playback.volumeUp
//   playback.volumeDown
//   playback.mute
//   playback.toggleMute
//   playback.getCurrentTrack
//   playback.getPlaybackOrder
//   playback.setPlaybackOrder
//   playback.getStopAfterCurrent
//   playback.setStopAfterCurrent
//   playback.toggleStopAfterCurrent
//   playback.playPath
//   playback.playPaths
//   playback.getCurrentTrackIndex
//   playback.getPlayingPlaylist
//   test.echo
//   test.ping
//

// Register all playback-related APIs with BridgeCore
/** @brief Register the playback.* API handlers and playback:* events. */
void RegisterPlaybackApi();

// Service injection for testability
// Set/get the IPlaybackService used by interfaced handlers.
// Default is Fb2kPlaybackService (production). Pass nullptr to reset to default.
void SetPlaybackService(IPlaybackService* service);
IPlaybackService* GetPlaybackService();

// Helper: Get track info as JSON
json GetTrackInfo(metadb_handle_ptr track);
