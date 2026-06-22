#pragma once
#include "pch.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class IPlaylistService;
class IPlaybackService;

// ============================================
// Playlist API Registration
// ============================================
//
// Registered APIs (47):
//   playlist.getCount
//   playlist.getAll
//   playlist.getActive
//   playlist.setActive
//   playlist.getPlaying
//   playlist.create
//   playlist.remove
//   playlist.rename
//   playlist.clear
//   playlist.insertTracks
//   playlist.getTrackCount
//   playlist.getTracks
//   playlist.getSelectedTracks
//   playlist.setSelection
//   playlist.removeTracks
//   playlist.removeSelectedTracks
//   playlist.moveTracks
//   playlist.playTrack
//   playlist.focusTrack
//   playlist.getFocusTrack
//   playlist.sort
//   playlist.shuffle
//   playlist.undo
//   playlist.redo
//   playlist.isAutoplaylist
//   playlist.createAutoplaylist
//   playlist.convertToAutoplaylist
//   playlist.removeAutoplaylist
//   playlist.getAutoplaylistInfo
//   playlist.getAutoplaylistQuery
//   playlist.duplicate
//   playlist.addPaths
//   playlist.addHandles
//   playlist.getLockInfo
//   playlist.isLocked
//   playlist.getSelection
//   playlist.selectAll
//   playlist.deselectAll
//   playlist.getFocusedTrack
//   playlist.setFocusedTrack
//   playlist.reverse
//   playlist.reorder
//   playlist.addPathsSequential
//   playlist.addPathsAsync
//   playlist.replaceAllAndPlay
//   playlist.reorderPlaylists
//   playlist.getAvailableColumns
//

// Register all playlist-related APIs with BridgeCore
/** @brief Register the playlist.* API handlers and playlist:* events. */
void RegisterPlaylistApi();

// Service injection for testability
// Set/get the IPlaylistService used by interfaced handlers.
// Default is Fb2kPlaylistService (production). Pass nullptr to reset to default.
void SetPlaylistService(IPlaylistService* service);
IPlaylistService* GetPlaylistService();

// Playback service injection (used by replaceAllAndPlay)
void SetPlaylistApiPlaybackService(IPlaybackService* service);
IPlaybackService* GetPlaylistApiPlaybackService();

// Helper: Get playlist info as JSON
json GetPlaylistInfo(size_t index, bool includeDuration = false);

// Helper: Get track info from playlist as JSON
json GetPlaylistTrackInfo(const metadb_handle_ptr& track, size_t index);

