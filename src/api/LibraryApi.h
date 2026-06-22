#pragma once
#include "pch.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// ============================================
// Library API Registration
// ============================================
//
// Registered APIs (25):
//   library.isEnabled
//   library.getStats
//   library.invalidateCache
//   library.getCacheStats
//   library.search
//   library.getAlbums
//   library.getArtists
//   library.getGenres
//   library.getAlbumTracks
//   library.getArtistTracks
//   library.getRandomTracks
//   library.rescan
//   library.addToPlaylist
//   library.getArtistAlbums
//   library.getFieldValues
//   library.query
//   library.getRoots
//   library.browseTree
//   library.browseDirectory
//   library.getStatus
//   library.getCount
//   library.getAll
//   library.getByPath
//   library.getRecentlyAdded
//   library.refresh
//

// Register all library-related APIs with BridgeCore
/** @brief Register the library.* (media library) API handlers. */
void RegisterLibraryApi();

// Helper: Get track info from library as JSON
json GetLibraryTrackInfo(metadb_handle_ptr track, size_t index);

