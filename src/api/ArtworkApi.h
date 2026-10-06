#pragma once

#include "pch.h"

// Artwork API - Album art and lyrics functionality
// Cover art and lyrics
//
// Registered APIs (13):
//   artwork.getCurrent
//   artwork.getByPath
//   artwork.getForTrack
//   artwork.getByPlaylistItem
//   artwork.getAvailableTypes
//   artwork.getFb2kUrl
//   artwork.getFb2kUrlByPath
//   artwork.getLyrics
//   artwork.getMetadata
//   artwork.getBatch
//   artwork.getAvailableArtwork
//   artwork.getFb2kUrlByPathBatch
//   artwork.getFolderImages
//

/** @brief Register the artwork.* API handlers. */
void RegisterArtworkApi();

// ============================================================
// Internal helpers for high-performance binary artwork delivery
// Used by WebView2 custom protocol handlers (fb2k://artwork, artwork://)
// ============================================================
namespace artwork_internal {

struct BinaryArtwork {
    std::vector<uint8_t> bytes;
    std::string mimeType;
};

// Get artwork bytes for current playing track
bool GetCurrentArtworkBinary(const std::string& type, BinaryArtwork& out);

// Get artwork bytes for a specific file path.
// `abort` reaches the album-art extractor so a cancelled worker item stops the
// SDK call instead of running it to completion; exception_aborted propagates to
// the caller. Synchronous Bridge callers keep the default no-op aborter.
bool GetArtworkBinaryForPath(const std::string& path, const std::string& type, BinaryArtwork& out,
                             abort_callback& abort = fb2k::noAbort);

// Try to resolve stable source file paths for cache sharing.
// Returns false for embedded-only artwork or when the source paths are unavailable.
bool TryGetArtworkSourcePathsForPath(
    const std::string& path,
    const std::string& type,
    std::vector<std::string>& outPaths);

}
