/**
 * Artwork API - Album art and lyrics functionality
 * Cover art and lyrics
 * 
 * Provides APIs for:
 * - Getting album art (front cover, back cover, disc, artist)
 * - Getting current playing track's album art
 * - Getting lyrics from metadata
 */

#include "pch.h"
#include "api/ArtworkApi.h"
#include "api/BridgeCore.h"
#include <foobar2000/SDK/album_art.h>
#include <foobar2000/SDK/album_art_helpers.h>
#include <sstream>
#include <iomanip>
#include <array>
#include "utils/Base64.h"

using json = nlohmann::json;

// Detect image MIME type from magic bytes
static const char* DetectMimeType(const uint8_t* data, size_t len) {
    if (len < 2) return "application/octet-stream";
    
    const unsigned char* bytes = data;
    
    // BMP: 42 4D (only needs 2 bytes, check first)
    if (bytes[0] == 0x42 && bytes[1] == 0x4D) {
        return "image/bmp";
    }
    
    if (len < 4) return "application/octet-stream";
    
    // JPEG: FF D8 FF
    if (bytes[0] == 0xFF && bytes[1] == 0xD8 && bytes[2] == 0xFF) {
        return "image/jpeg";
    }
    
    // PNG: 89 50 4E 47
    if (bytes[0] == 0x89 && bytes[1] == 0x50 && bytes[2] == 0x4E && bytes[3] == 0x47) {
        return "image/png";
    }
    
    // GIF: 47 49 46 38
    if (bytes[0] == 0x47 && bytes[1] == 0x49 && bytes[2] == 0x46 && bytes[3] == 0x38) {
        return "image/gif";
    }
    
    // WebP: 52 49 46 46 ... 57 45 42 50
    if (len >= 12 && bytes[0] == 0x52 && bytes[1] == 0x49 && bytes[2] == 0x46 && bytes[3] == 0x46 &&
        bytes[8] == 0x57 && bytes[9] == 0x45 && bytes[10] == 0x42 && bytes[11] == 0x50) {
        return "image/webp";
    }
    
    return "application/octet-stream";
}

// ==========================================================================
// URL Encoding helper for fb2k:// protocol
// Encodes ALL special characters including path separators (/, \, :)
// This ensures consistent encoding like: E%3A%5COST%5Cvoid%5C...
// The UrlDecode function in WebViewHost.cpp will decode everything correctly
// ==========================================================================
static std::string UrlEncode(const std::string& str) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;
    escaped << std::uppercase;

    for (size_t i = 0; i < str.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(str[i]);
        
        // Keep only alphanumeric, hyphen, underscore, period, tilde
        // Everything else including /, \, : must be encoded
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << static_cast<char>(c);
        } else {
            // Encode everything else including: / \ : # ? & % space and all non-ASCII
            escaped << '%' << std::setw(2) << static_cast<int>(c);
        }
    }

    return escaped.str();
}


// Convert GUID string to album_art_ids GUID
// Returns cover_front as default; sets outValid=false for unrecognized types
static GUID StringToArtType(const std::string& type, bool* outValid = nullptr) {
    if (outValid) *outValid = true;
    if (type == "front" || type == "cover_front" || type.empty()) return album_art_ids::cover_front;
    if (type == "back" || type == "cover_back") return album_art_ids::cover_back;
    if (type == "disc") return album_art_ids::disc;
    if (type == "icon") return album_art_ids::icon;
    if (type == "artist") return album_art_ids::artist;
    if (outValid) *outValid = false;
    return album_art_ids::cover_front; // Default fallback for callers that don't check outValid
}

// Validate artwork type string; returns error JSON if invalid, or null JSON if valid
static json ValidateArtworkType(const std::string& type) {
    if (type.empty() || type == "front" || type == "cover_front" ||
        type == "back" || type == "cover_back" ||
        type == "disc" || type == "icon" || type == "artist") {
        return nullptr;  // valid
    }
    return {{"available", false}, {"error", "Unknown artwork type: " + type}, {"code", "INVALID_PARAMS"}};
}

namespace {
struct BinaryArtworkInternal {
    std::vector<uint8_t> bytes;
    std::string mimeType;
};

static void AppendCanonicalArtworkSourcePaths(
    const album_art_path_list::ptr& pathList,
    std::vector<std::string>& outPaths)
{
    if (!pathList.is_valid()) {
        return;
    }

    for (t_size i = 0; i < pathList->get_count(); ++i) {
        const char* rawPath = pathList->get_path(i);
        if (!rawPath || !rawPath[0]) {
            continue;
        }

        try {
            pfc::string8 canonicalPath;
            filesystem::g_get_canonical_path(rawPath, canonicalPath);
            outPaths.emplace_back(canonicalPath.c_str());
        } catch (...) {
            outPaths.emplace_back(rawPath);
        }
    }
}

static bool GetArtworkSourcePathsForMetadbHandle(
    const metadb_handle_ptr& track,
    const GUID& artType,
    std::vector<std::string>& outPaths)
{
    if (!track.is_valid()) {
        return false;
    }

    try {
        abort_callback_dummy abort;
        auto artManager = album_art_manager_v2::get();

        metadb_handle_list items;
        items.add_item(track);

        pfc::list_t<GUID> ids;
        ids.add_item(artType);

        auto extractor = artManager->open(items, ids, abort);
        if (!extractor.is_valid()) {
            return false;
        }

        AppendCanonicalArtworkSourcePaths(extractor->query_paths(artType, abort), outPaths);
        return !outPaths.empty();
    } catch (...) {
        return false;
    }
}

} // namespace

static bool GetArtworkBinaryForMetadbHandle(metadb_handle_ptr track, const GUID& artType, BinaryArtworkInternal& out) {
    if (!track.is_valid()) return false;

    // 1) now_playing_album_art_notify_manager (fast, cached) - only for cover_front
    // IMPORTANT: This cache is ONLY valid for the currently playing track and ONLY holds cover_front!
    if (artType == album_art_ids::cover_front) {
        try {
            auto pc = playback_control::get();
            metadb_handle_ptr nowPlaying;
        
            // Only use now_playing_manager cache if the requested track is actually the current playing track
            if (pc->get_now_playing(nowPlaying) && nowPlaying.is_valid() && nowPlaying == track) {
                auto manager = now_playing_album_art_notify_manager::get();
                auto data = manager->current();
                if (data.is_valid() && data->get_size() > 0) {
                    const uint8_t* ptr = static_cast<const uint8_t*>(data->data());
                    const size_t size = data->get_size();
                    out.bytes.assign(ptr, ptr + size);
                    out.mimeType = DetectMimeType(ptr, size);
                    return true;
                }
            }
        } catch (...) {
            // ignore
        }
    }

    // 2) album_art_manager_v2
    try {
        abort_callback_dummy abort;
        auto artManager = album_art_manager_v2::get();

        metadb_handle_list items;
        items.add_item(track);

        pfc::list_t<GUID> ids;
        ids.add_item(artType);

        auto extractor = artManager->open(items, ids, abort);
        if (extractor.is_valid()) {
            album_art_data::ptr artData;
            if (extractor->query(artType, artData, abort) && artData.is_valid() && artData->get_size() > 0) {
                const uint8_t* ptr = static_cast<const uint8_t*>(artData->data());
                const size_t size = artData->get_size();
                out.bytes.assign(ptr, ptr + size);
                out.mimeType = DetectMimeType(ptr, size);
                return true;
            }
        }
    } catch (...) {
        // ignore
    }

    return false;
}

// Parse subsong path: "path|subsong:N" -> { filePath, subsongIndex }
static std::pair<std::string, t_uint32> ArtworkParseSubsongPath(const std::string& path) {
    std::string filePath = path;
    t_uint32 subsongIndex = 0;
    size_t pos = path.find("|subsong:");
    if (pos != std::string::npos) {
        filePath = path.substr(0, pos);
        try {
            subsongIndex = static_cast<t_uint32>(std::stoul(path.substr(pos + 9)));
        } catch (...) {
            // Invalid subsong suffix — fall back to 0 with warning
            subsongIndex = 0;
            FB2K_console_formatter() << "foo_ui_webview2: invalid subsong suffix in path: " << path.c_str();
        }
    }
    return { filePath, subsongIndex };
}

// Classify path kind for hook evidence
static const char* ArtworkClassifyPathKind(const std::string& path) {
    if (path.find("file-relative://") == 0) return "file-relative://";
    if (path.find("file://") == 0) return "file://";
    if (path.find("|subsong:") != std::string::npos) return "path|subsong";
    return "native";
}

// Shared helper: canonicalize path and create handle for artwork/lyrics sibling APIs
static metadb_handle_ptr ArtworkResolveHandle(const std::string& path, const char* api) {
    auto [filePath, subsong] = ArtworkParseSubsongPath(path);
    pfc::string8 canonicalPath;
    filesystem::g_get_canonical_path(filePath.c_str(), canonicalPath);

    auto mdb = metadb::get();
    metadb_handle_ptr track = mdb->handle_create(canonicalPath.c_str(), subsong);

    LOG("[PATH_HOOK] api=%s inputPath=%s pathKind=%s canonicalPath=%s sink=handle_create usedNowPlayingShortcut=false resultKind=%s",
        api, path.c_str(), ArtworkClassifyPathKind(path), canonicalPath.c_str(),
        track.is_valid() ? "handle-valid" : "handle-invalid");

    return track;
}

namespace artwork_internal {

bool GetCurrentArtworkBinary(const std::string& type, BinaryArtwork& out) {
    GUID artType = StringToArtType(type);

    try {
        auto pc = playback_control::get();
        metadb_handle_ptr track;
        if (!pc->get_now_playing(track) || !track.is_valid()) {
            return false;
        }

        BinaryArtworkInternal tmp;
        if (!GetArtworkBinaryForMetadbHandle(track, artType, tmp)) {
            return false;
        }

        out.bytes = std::move(tmp.bytes);
        out.mimeType = std::move(tmp.mimeType);
        return true;
    } catch (...) {
        return false;
    }
}

bool GetArtworkBinaryForPath(const std::string& path, const std::string& type, BinaryArtwork& out) {
    if (path.empty()) return false;

    GUID artType = StringToArtType(type);

    try {
        abort_callback_dummy abort;

        // Parse subsong from path (e.g. "file.cue|subsong:3")
        auto [filePath, subsong] = ArtworkParseSubsongPath(path);

        pfc::string8 canonicalPath;
        filesystem::g_get_canonical_path(filePath.c_str(), canonicalPath);

        auto mdb = metadb::get();
        metadb_handle_ptr track = mdb->handle_create(canonicalPath.c_str(), subsong);

        BinaryArtworkInternal tmp;
        if (track.is_valid()) {
            // For non-current track requests, skip now_playing_manager cache by going straight to v2+extractor
            // We'll reuse helper but it may hit now_playing_manager; that's acceptable (still correct).
            if (!GetArtworkBinaryForMetadbHandle(track, artType, tmp)) {
                return false;
            }
        } else {
            // track handle is invalid, can't get artwork
            return false;
        }

        out.bytes = std::move(tmp.bytes);
        out.mimeType = std::move(tmp.mimeType);
        return true;
    } catch (...) {
        return false;
    }
}

bool TryGetArtworkSourcePathsForPath(
    const std::string& path,
    const std::string& type,
    std::vector<std::string>& outPaths)
{
    outPaths.clear();
    if (path.empty()) {
        return false;
    }

    GUID artType = StringToArtType(type);

    try {
        auto [filePath, subsong] = ArtworkParseSubsongPath(path);

        pfc::string8 canonicalPath;
        filesystem::g_get_canonical_path(filePath.c_str(), canonicalPath);

        auto mdb = metadb::get();
        metadb_handle_ptr track = mdb->handle_create(canonicalPath.c_str(), subsong);
        return GetArtworkSourcePathsForMetadbHandle(track, artType, outPaths);
    } catch (...) {
        outPaths.clear();
        return false;
    }
}

} // namespace artwork_internal



// ==========================================================================
// Artwork API handler functions
// ==========================================================================
namespace {


// ========== Current Playing Artwork ==========

// Get album art for currently playing track
json ArtworkGetCurrent(const json& params) {
    std::string type = params.value("type", "front");
    auto typeErr = ValidateArtworkType(type);
    if (!typeErr.is_null()) return typeErr;
    GUID artType = StringToArtType(type);
    
    try {
        // First, check if there's a track playing
        auto pc = playback_control::get();
        metadb_handle_ptr track;
        if (!pc->get_now_playing(track) || !track.is_valid()) {
            LOG("artwork.getCurrent: No track playing");
            return {
                {"available", false},
                {"type", type},
                {"reason", "no_track"},
            };
        }
        
        LOG("artwork.getCurrent: %s", track->get_path());
        
        // Cache the path for later use (avoids nodiscard warnings)
        const char* trackPath = track->get_path();
        
        // Method 1: Try now_playing_album_art_notify_manager (fastest, cached)
        // NOTE: This cache only holds cover_front. Skip for other art types.
        if (artType == album_art_ids::cover_front) {
            auto manager = now_playing_album_art_notify_manager::get();
            auto data = manager->current();
        
            if (data.is_valid() && data->get_size() > 0) {
                const uint8_t* ptr = static_cast<const uint8_t*>(data->data());
                size_t size = data->get_size();
            
                std::string base64 = utils::Base64Encode(ptr, size);
                const char* mimeType = DetectMimeType(ptr, size);
            
                LOG("artwork.getCurrent: Found via now_playing_manager (%zu bytes)", size);
                return {
                    {"available", true},
                    {"type", type},
                    {"source", "now_playing_manager"},
                    {"mimeType", mimeType},
                    {"size", (int64_t)size},
                    {"dataUrl", std::string("data:") + mimeType + ";base64," + base64},
                };
            }
        }
        
        // Method 2: Try album_art_manager_v2 (more comprehensive)
        LOG("artwork.getCurrent: now_playing_manager returned no data, trying album_art_manager_v2");
        abort_callback_dummy abort;
        auto artManager = album_art_manager_v2::get();
        
        metadb_handle_list items;
        items.add_item(track);
        
        pfc::list_t<GUID> ids;
        ids.add_item(artType);
        
        auto extractor = artManager->open(items, ids, abort);
        if (extractor.is_valid()) {
            album_art_data::ptr artData;
            if (extractor->query(artType, artData, abort) && artData.is_valid() && artData->get_size() > 0) {
                const uint8_t* ptr = static_cast<const uint8_t*>(artData->data());
                size_t size = artData->get_size();
                
                std::string base64 = utils::Base64Encode(ptr, size);
                const char* mimeType = DetectMimeType(ptr, size);
                
                LOG("artwork.getCurrent: Found via album_art_manager_v2 (%zu bytes)", size);
                return {
                    {"available", true},
                    {"type", type},
                    {"source", "album_art_manager_v2"},
                    {"mimeType", mimeType},
                    {"size", (int64_t)size},
                    {"dataUrl", std::string("data:") + mimeType + ";base64," + base64},
                };
            }
        }
        
        // Method 3: Try album_art_extractor directly
        LOG("artwork.getCurrent: album_art_manager_v2 returned no data, trying extractor directly");
        auto directExtractor = album_art_extractor::g_open(nullptr, trackPath, abort);
        if (directExtractor.is_valid()) {
            album_art_data::ptr artData;
            if (directExtractor->query(artType, artData, abort) && artData.is_valid() && artData->get_size() > 0) {
                const uint8_t* ptr = static_cast<const uint8_t*>(artData->data());
                size_t size = artData->get_size();
                
                std::string base64 = utils::Base64Encode(ptr, size);
                const char* mimeType = DetectMimeType(ptr, size);
                
                LOG("artwork.getCurrent: Found via direct extractor (%zu bytes)", size);
                return {
                    {"available", true},
                    {"type", type},
                    {"source", "extractor"},
                    {"mimeType", mimeType},
                    {"size", (int64_t)size},
                    {"dataUrl", std::string("data:") + mimeType + ";base64," + base64},
                };
            }
        }
        
        LOG("artwork.getCurrent: No artwork found for %s", trackPath);
        return {
            {"available", false},
            {"type", type},
            {"reason", "not_found"},
            {"path", trackPath},
        };
    } catch (const std::exception& e) {
        LOG("artwork.getCurrent error: %s", e.what());
        return {
            {"available", false},
            {"type", type},
            {"error", e.what()},
        };
    }
}


// Get album art for a specific track by path — extractor-path contract
// Supported path forms: native path, file://, path|subsong:N (subsong stripped for extractor)
// Rejected: file-relative:// (requires playlist context, use artwork.getByPlaylistItem)
json ArtworkGetByPath(const json& params) {
    std::string path = params.value("path", "");
    std::string type = params.value("type", "front");
    auto typeErr = ValidateArtworkType(type);
    if (!typeErr.is_null()) return typeErr;
    
    if (path.empty()) {
        return {{"available", false}, {"error", "path is required"}};
    }

    // Reject file-relative:// — extractor sink does not support playlist-relative paths
    if (path.find("file-relative://") == 0) {
        LOG("[PATH_HOOK] api=artwork.getByPath inputPath=%s pathKind=file-relative:// canonicalPath=null sink=extractor usedNowPlayingShortcut=false resultKind=extractor-rejected",
            path.c_str());
        return {
            {"available", false},
            {"type", type},
            {"path", path},
            {"error", "file-relative:// paths require playlist context. Use artwork.getByPlaylistItem instead."},
        };
    }

    // Strip subsong (extractor operates on file level, not subsong)
    auto [filePath, subsong] = ArtworkParseSubsongPath(path);

    // Canonicalize path before passing to extractor
    pfc::string8 canonicalPath;
    filesystem::g_get_canonical_path(filePath.c_str(), canonicalPath);
    
    GUID artType = StringToArtType(type);

    try {
        abort_callback_dummy abort;
        
        // Open album art extractor with canonical path
        auto extractor = album_art_extractor::g_open(nullptr, canonicalPath.c_str(), abort);
        if (!extractor.is_valid()) {
            LOG("[PATH_HOOK] api=artwork.getByPath resultKind=extractor-invalid (no extractor for path)");
            return {
                {"available", false},
                {"type", type},
                {"path", path},
            };
        }
        
        // Query the album art
        album_art_data::ptr data;
        if (!extractor->query(artType, data, abort) || !data.is_valid()) {
            LOG("[PATH_HOOK] api=artwork.getByPath resultKind=extractor-invalid (art type not found)");
            return {
                {"available", false},
                {"type", type},
                {"path", path},
            };
        }
        
        const uint8_t* ptr = static_cast<const uint8_t*>(data->data());
        size_t size = data->get_size();
        
        std::string base64 = utils::Base64Encode(ptr, size);
        const char* mimeType = DetectMimeType(ptr, size);

        LOG("[PATH_HOOK] api=artwork.getByPath resultKind=extractor-valid size=%zu", size);
        
        return {
            {"available", true},
            {"type", type},
            {"path", path},
            {"mimeType", mimeType},
            {"size", (int64_t)size},
            {"dataUrl", std::string("data:") + mimeType + ";base64," + base64},
        };
    } catch (const exception_album_art_not_found&) {
        LOG("[PATH_HOOK] api=artwork.getByPath resultKind=extractor-invalid (not found exception)");
        return {
            {"available", false},
            {"type", type},
            {"path", path},
        };
    } catch (const std::exception& e) {
        LOG("[PATH_HOOK] api=artwork.getByPath resultKind=extractor-invalid error=%s", e.what());
        return {
            {"available", false},
            {"type", type},
            {"path", path},
            {"error", e.what()},
        };
    }
}


// Get album art for track by path - uses album_art_manager_v2 for better compatibility
// Supports: file://, normal paths, path|subsong:N. For file-relative:// use artwork.getByPlaylistItem
json ArtworkGetForTrack(const json& params) {
    std::string path = params.value("path", "");
    std::string type = params.value("type", "front");
    auto typeErr = ValidateArtworkType(type);
    if (!typeErr.is_null()) return typeErr;
    
    if (path.empty()) {
        return {{"available", false}, {"error", "path is required"}};
    }
    
    // Check for file-relative:// paths
    if (path.find("file-relative://") == 0) {
        return {
            {"available", false},
            {"type", type},
            {"path", path},
            {"error", "file-relative:// paths require playlist context. Use artwork.getByPlaylistItem instead."},
        };
    }
    
    GUID artType = StringToArtType(type);
    
    try {
        abort_callback_dummy abort;
        
        // Use shared path resolution (handles subsong, canonical path)
        metadb_handle_ptr track = ArtworkResolveHandle(path, "artwork.getForTrack");
        
        if (!track.is_valid()) {
            return {
                {"available", false},
                {"type", type},
                {"path", path},
            };
        }
        
        // Use album_art_manager_v2 for better album art lookup
        auto manager = album_art_manager_v2::get();
        
        metadb_handle_list items;
        items.add_item(track);
        
        pfc::list_t<GUID> ids;
        ids.add_item(artType);
        
        auto extractor = manager->open(items, ids, abort);
        if (!extractor.is_valid()) {
            return {
                {"available", false},
                {"type", type},
                {"path", path},
            };
        }
        
        album_art_data::ptr data;
        if (!extractor->query(artType, data, abort) || !data.is_valid()) {
            return {
                {"available", false},
                {"type", type},
                {"path", path},
            };
        }
        
        const uint8_t* ptr = static_cast<const uint8_t*>(data->data());
        size_t size = data->get_size();
        
        std::string base64 = utils::Base64Encode(ptr, size);
        const char* mimeType = DetectMimeType(ptr, size);
        
        // Get image dimensions (basic detection for common formats)
        int width = 0, height = 0;
        if (size > 24 && ptr[0] == 0x89 && ptr[1] == 'P' && ptr[2] == 'N' && ptr[3] == 'G') {
            // PNG: width at offset 16, height at offset 20 (big-endian)
            width = (ptr[16] << 24) | (ptr[17] << 16) | (ptr[18] << 8) | ptr[19];
            height = (ptr[20] << 24) | (ptr[21] << 16) | (ptr[22] << 8) | ptr[23];
        }
        
        return {
            {"available", true},
            {"type", type},
            {"mimeType", mimeType},
            {"width", width},
            {"height", height},
            {"size", (int64_t)size},
            {"dataUrl", std::string("data:") + mimeType + ";base64," + base64},
        };
    } catch (const exception_album_art_not_found&) {
        return {
            {"available", false},
            {"type", type},
            {"path", path},
        };
    } catch (const std::exception& e) {
        return {
            {"available", false},
            {"type", type},
            {"path", path},
            {"error", e.what()},
        };
    }
}


// Get album art for track by playlist/index
json ArtworkGetByPlaylistItem(const json& params) {
    int playlist = params.value("playlist", -1);
    int index = params.value("index", -1);
    std::string type = params.value("type", "front");
    auto typeErr = ValidateArtworkType(type);
    if (!typeErr.is_null()) return typeErr;
    
    auto plm = playlist_manager::get();
    
    // Default to active playlist
    size_t playlistIndex = (playlist >= 0) ? (size_t)playlist : plm->get_active_playlist();
    if (playlistIndex == pfc::infinite_size) {
        return {{"available", false}, {"error", "No active playlist"}};
    }
    
    size_t itemIndex = (index >= 0) ? (size_t)index : 0;
    size_t count = plm->playlist_get_item_count(playlistIndex);
    
    if (itemIndex >= count) {
        return {{"available", false}, {"error", "Index out of range"}};
    }
    
    // Get track handle
    metadb_handle_ptr track;
    if (!plm->playlist_get_item_handle(track, playlistIndex, itemIndex)) {
        return {{"available", false}, {"error", "Failed to get track handle"}};
    }
    
    GUID artType = StringToArtType(type);
    
    try {
        abort_callback_dummy abort;
        
        // Use album_art_manager_v2 for better album art lookup
        auto manager = album_art_manager_v2::get();
        
        metadb_handle_list items;
        items.add_item(track);
        
        pfc::list_t<GUID> ids;
        ids.add_item(artType);
        
        auto extractor = manager->open(items, ids, abort);
        if (!extractor.is_valid()) {
            return {
                {"available", false},   // Standard field
    
                {"type", type},
                {"playlist", (int)playlistIndex},
                {"index", (int)itemIndex},
            };
        }
        
        album_art_data::ptr data;
        if (!extractor->query(artType, data, abort) || !data.is_valid()) {
            return {
                {"available", false},   // Standard field
    
                {"type", type},
                {"playlist", (int)playlistIndex},
                {"index", (int)itemIndex},
            };
        }
        
        const uint8_t* ptr = static_cast<const uint8_t*>(data->data());
        size_t size = data->get_size();
        
        std::string base64 = utils::Base64Encode(ptr, size);
        const char* mimeType = DetectMimeType(ptr, size);
        
        return {
            {"available", true},    // Standard field
 
            {"type", type},
            {"playlist", (int)playlistIndex},
            {"index", (int)itemIndex},
            {"mimeType", mimeType},
            {"size", (int64_t)size},
            {"dataUrl", std::string("data:") + mimeType + ";base64," + base64},
        };
    } catch (const exception_album_art_not_found&) {
        return {
            {"available", false},   // Standard field

            {"type", type},
            {"playlist", (int)playlistIndex},
            {"index", (int)itemIndex},
        };
    } catch (const std::exception& e) {
        LOG("getByPlaylistItem error: %s", e.what());
        return {
            {"available", false},   // Standard field

            {"type", type},
            {"playlist", (int)playlistIndex},
            {"index", (int)itemIndex},
            {"error", e.what()},
        };
    }
}


// Get list of available art types for a track
json ArtworkGetAvailableTypes(const json& params) {
    std::string path = params.value("path", "");
    
    json types = json::array();
    
    try {
        abort_callback_dummy abort;
        
        // If no path provided, use current playing track
        if (path.empty()) {
            auto pc = playback_control::get();
            metadb_handle_ptr track;
            if (pc->get_now_playing(track)) {
                const char* trackPath = track->get_path();
                path = trackPath;
            } else {
                return {
                    {"types", types},
                    {"error", "No track specified and nothing playing"},
                };
            }
        }
        
        auto extractor = album_art_extractor::g_open_allowempty(nullptr, path.c_str(), abort);
        if (!extractor.is_valid()) {
            return {{"success", true}, {"types", types}};
        }
        
        // Check each type
        struct { const char* name; GUID id; } artTypes[] = {
            {"front", album_art_ids::cover_front},
            {"back", album_art_ids::cover_back},
            {"disc", album_art_ids::disc},
            {"icon", album_art_ids::icon},
            {"artist", album_art_ids::artist},
        };
        
        for (const auto& at : artTypes) {
            if (extractor->have_entry(at.id, abort)) {
                types.push_back(at.name);
            }
        }
    } catch (const std::exception& e) {
        LOG("getAvailableTypes error: %s", e.what());
    }
    
    return {{"success", true}, {"types", types}};
}


// ========== fb2k:// Protocol URL helpers ==========

// Return a fb2k://artwork/... URL for current playing track.
// The WebView2 protocol handler will fetch and (optionally) scale the artwork.
json ArtworkGetFb2kUrl(const json& params) {
    std::string type = params.value("type", "front");
    auto typeErr = ValidateArtworkType(type);
    if (!typeErr.is_null()) return typeErr;
    int maxSize = params.value("maxSize", 0);

    try {
        auto pc = playback_control::get();
        metadb_handle_ptr track;
        if (!pc->get_now_playing(track) || !track.is_valid()) {
            LOG("[fb2k://] getFb2kUrl: No playing track");
            return {
                {"available", false},
                {"type", type},
                {"reason", "no_track"},
            };
        }

        const char* trackPath = track->get_path();
        LOG("[fb2k://] getFb2kUrl: trackPath=%s", trackPath);

        // Path placed in query parameter (not URL path) to avoid
        // Chromium URL normalization of %5C/%2F in path segments
        std::ostringstream oss;
        oss << "fb2k://artwork/?path=" << UrlEncode(trackPath) << "&type=" << type;
        if (maxSize > 0) {
            oss << "&maxSize=" << maxSize;
        }

        std::string url = oss.str();
        LOG("[fb2k://] getFb2kUrl: url=%s", url.c_str());

        return {
            {"available", true},
            {"type", type},
            {"dataUrl", url},
        };
    } catch (const std::exception& e) {
        LOG("[fb2k://] getFb2kUrl error: %s", e.what());
        return {
            {"available", false},
            {"type", type},
            {"error", e.what()},
        };
    }
}


// Return a fb2k://artwork/... URL for a given track path.
json ArtworkGetFb2kUrlByPath(const json& params) {
    std::string path = params.value("path", "");
    std::string type = params.value("type", "front");
    auto typeErr = ValidateArtworkType(type);
    if (!typeErr.is_null()) return typeErr;
    int maxSize = params.value("maxSize", 0);

    if (path.empty()) {
        return {
            {"available", false},
            {"type", type},
            {"error", "path is required"},
        };
    }

    // Path placed in query parameter (not URL path) to avoid
    // Chromium URL normalization of %5C/%2F in path segments
    std::ostringstream oss;
    oss << "fb2k://artwork/?path=" << UrlEncode(path) << "&type=" << type;
    if (maxSize > 0) {
        oss << "&maxSize=" << maxSize;
    }

    std::string url = oss.str();
    return {
        {"available", true},
        {"type", type},
        {"path", path},
        {"dataUrl", url},
    };
}


// ========== Lyrics ==========

// Get lyrics from current playing track's metadata
json ArtworkGetLyrics(const json& params) {
    std::string path = params.value("path", "");
    
    try {
        metadb_handle_ptr track;
        
        if (path.empty()) {
            // Use current playing track
            auto pc = playback_control::get();
            if (!pc->get_now_playing(track)) {
                return {
                    {"available", false},
                    {"error", "No track playing"},
                };
            }
            LOG("[PATH_HOOK] api=artwork.getLyrics usedNowPlayingShortcut=true resultKind=%s",
                track.is_valid() ? "handle-valid" : "handle-invalid");
        } else {
            // Canonicalize path before handle_create
            track = ArtworkResolveHandle(path, "artwork.getLyrics");
            if (!track.is_valid()) {
                return {
                    {"available", false},
                    {"error", "Failed to create track handle"},
                };
            }
        }
        
        // 使用 get_info_ref() 替代已弃用的 get_info()
        metadb_info_container::ptr infoContainer = track->get_info_ref();
        const file_info& info = infoContainer->info();
        
        // Try different lyrics tags
        const char* lyricsTags[] = {
            "LYRICS",
            "UNSYNCED LYRICS",
            "UNSYNCEDLYRICS",
            "SYNCEDLYRICS",
            "SYNCED LYRICS",
        };
        
        for (const char* tag : lyricsTags) {
            const char* value = info.meta_get(tag, 0);
            if (value && value[0] != '\0') {
                return {
                    {"available", true},
                    {"tag", tag},
                    {"lyrics", value},
                    {"synced", strstr(tag, "SYNC") != nullptr && strstr(tag, "UNSYNC") == nullptr},
                };
            }
        }
        
        return {
            {"available", false},
        };
    } catch (const std::exception& e) {
        LOG("getLyrics error: %s", e.what());
        return {
            {"available", false},
            {"error", e.what()},
        };
    }
}


// Get all artwork-related metadata
json ArtworkGetMetadata(const json& params) {
    std::string path = params.value("path", "");
    
    try {
        metadb_handle_ptr track;
        
        if (path.empty()) {
            auto pc = playback_control::get();
            if (!pc->get_now_playing(track)) {
                return {{"available", false}, {"error", "No track playing"}};
            }
            LOG("[PATH_HOOK] api=artwork.getMetadata usedNowPlayingShortcut=true resultKind=%s",
                track.is_valid() ? "handle-valid" : "handle-invalid");
        } else {
            // Canonicalize path before handle_create
            track = ArtworkResolveHandle(path, "artwork.getMetadata");
            if (!track.is_valid()) {
                return {{"available", false}, {"error", "Failed to create track handle"}};
            }
        }
        
        // 使用 get_info_ref() 替代已弃用的 get_info()
        metadb_info_container::ptr infoContainer = track->get_info_ref();
        const file_info& info = infoContainer->info();
        
        json result;
        result["available"] = true;
        
        // Album info
        result["album"] = info.meta_get("ALBUM", 0) ? info.meta_get("ALBUM", 0) : "";
        result["artist"] = info.meta_get("ARTIST", 0) ? info.meta_get("ARTIST", 0) : "";
        result["albumArtist"] = info.meta_get("ALBUM ARTIST", 0) ? info.meta_get("ALBUM ARTIST", 0) : "";
        result["title"] = info.meta_get("TITLE", 0) ? info.meta_get("TITLE", 0) : "";
        result["year"] = info.meta_get("DATE", 0) ? info.meta_get("DATE", 0) : "";
        result["genre"] = info.meta_get("GENRE", 0) ? info.meta_get("GENRE", 0) : "";
        result["trackNumber"] = info.meta_get("TRACKNUMBER", 0) ? info.meta_get("TRACKNUMBER", 0) : "";
        result["discNumber"] = info.meta_get("DISCNUMBER", 0) ? info.meta_get("DISCNUMBER", 0) : "";
        
        // Check for embedded artwork (check all standard types, not just cover_front)
        bool hasEmbedded = false;
        try {
            abort_callback_dummy abort;
            const char* trackPath = track->get_path();
            auto extractor = album_art_extractor::g_open(nullptr, trackPath, abort);
            if (extractor.is_valid()) {
                static const GUID embeddedTypes[] = {
                    album_art_ids::cover_front, album_art_ids::cover_back,
                    album_art_ids::disc, album_art_ids::icon, album_art_ids::artist,
                };
                for (const auto& artId : embeddedTypes) {
                    album_art_data::ptr artData;
                    try {
                        if (extractor->query(artId, artData, abort) && artData.is_valid() && artData->get_size() > 0) {
                            hasEmbedded = true;
                            break;
                        }
                    } catch (...) {}
                }
            }
        } catch (...) {}
        result["hasEmbedded"] = hasEmbedded;
        
        // Check for lyrics (check all known lyrics tags + fallback scan)
        {
            bool foundLyrics = false;
            static constexpr std::array lyricsTags = {
                "LYRICS", "UNSYNCED LYRICS", "UNSYNCEDLYRICS",
                "SYNCEDLYRICS", "SYNCED LYRICS",
            };
            for (const char* tag : lyricsTags) {
                if (info.meta_get(tag, 0)) { foundLyrics = true; break; }
            }
            if (!foundLyrics) {
                // Fallback: scan all meta fields for any name containing "lyric"
                for (t_size li = 0, ln = info.meta_get_count(); li < ln && !foundLyrics; ++li) {
                    const char* fname = info.meta_enum_name(li);
                    if (!fname) continue;
                    std::string lower(fname);
                    std::transform(lower.begin(), lower.end(), lower.begin(),
                        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                    if (lower.find("lyric") != std::string::npos) {
                        foundLyrics = true;
                    }
                }
            }
            result["hasLyrics"] = foundLyrics;
        }
        
        return result;
    } catch (const std::exception& e) {
        LOG("getMetadata error: %s", e.what());
        return {{"available", false}, {"error", e.what()}};
    }
}


// ========== Extended Artwork APIs ==========

// artwork.getBatch - Get artwork for multiple tracks
json ArtworkGetBatch(const json& params) {
    if (!params.contains("paths") || !params["paths"].is_array()) {
        return {{"success", false}, {"error", "paths array is required"}};
    }
    
    std::string type = params.value("type", "front");
    auto typeErr = ValidateArtworkType(type);
    if (!typeErr.is_null()) return typeErr;
    GUID artType = StringToArtType(type);
    json results = json::array();
    
    for (const auto& pathJson : params["paths"]) {
        std::string path = pathJson.get<std::string>();
        json item;
        item["path"] = path;
        item["available"] = false;
        
        try {
            abort_callback_dummy abort;
            
            // Use shared path resolution for subsong support
            auto [filePath, subsong] = ArtworkParseSubsongPath(path);
            auto extractor = album_art_extractor::g_open(nullptr, filePath.c_str(), abort);
            
            if (extractor.is_valid()) {
                album_art_data::ptr data;
                if (extractor->query(artType, data, abort) && data.is_valid()) {
                    const uint8_t* ptr = static_cast<const uint8_t*>(data->data());
                    size_t size = data->get_size();
                    
                    std::string base64 = utils::Base64Encode(ptr, size);
                    const char* mimeType = DetectMimeType(ptr, size);
                    
                    item["available"] = true;
                    item["mimeType"] = mimeType;
                    item["size"] = static_cast<int64_t>(size);
                    item["dataUrl"] = std::string("data:") + mimeType + ";base64," + base64;
                }
            }
        } catch (...) {
            // Continue with next item
        }
        
        results.push_back(item);
    }
    
    return {{"success", true}, {"artworks", results}};
}


// artwork.getAvailableArtwork - Get all available artwork types for a track
json ArtworkGetAvailableArtwork(const json& params) {
    std::string path = params.value("path", "");
    
    if (path.empty()) {
        return {{"success", false}, {"error", "path is required"}};
    }
    
    // Use shared path resolution for subsong support
    auto [filePath, subsong] = ArtworkParseSubsongPath(path);
    
    json artworks = json::array();
    json sources = json::array();
    
    try {
        abort_callback_dummy abort;
        auto extractor = album_art_extractor::g_open(nullptr, filePath.c_str(), abort);
        
        if (extractor.is_valid()) {
            static const struct { const char* name; GUID id; } artTypes[] = {
                {"front", album_art_ids::cover_front},
                {"back", album_art_ids::cover_back},
                {"disc", album_art_ids::disc},
                {"icon", album_art_ids::icon},
                {"artist", album_art_ids::artist},
            };
            
            bool hasAnyEmbedded = false;
            for (const auto& at : artTypes) {
                album_art_data::ptr data;
                try {
                    if (extractor->query(at.id, data, abort) && data.is_valid()) {
                        hasAnyEmbedded = true;
                        artworks.push_back({
                            {"type", at.name},
                            {"source", "embedded"}
                        });
                    }
                } catch (...) {}
            }
            
            if (hasAnyEmbedded) {
                sources.push_back("embedded");
            }
        }
        
        // Check for external artwork files in the directory
        size_t lastSlash = filePath.rfind('\\');
        if (lastSlash == std::string::npos) lastSlash = filePath.rfind('/');
        
        if (lastSlash != std::string::npos) {
            std::string dir = filePath.substr(0, lastSlash);
            std::wstring wdir = Utf8ToWide(dir);
            
            static const wchar_t* coverFiles[] = {
                L"cover.jpg", L"cover.png", L"folder.jpg", L"folder.png",
                L"front.jpg", L"front.png", L"album.jpg", L"album.png"
            };
            
            for (const auto& coverFile : coverFiles) {
                std::wstring fullPath = wdir + L"\\" + coverFile;
                if (GetFileAttributesW(fullPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
                    std::string sourceName = "folder:" + WideToUtf8(coverFile);
                    sources.push_back(sourceName);
                }
            }
        }
    } catch (...) {}
    
    // Collect types list from artworks
    json types = json::array();
    for (const auto& aw : artworks) {
        types.push_back(aw["type"]);
    }
    
    return {
        {"success", true},
        {"available", !artworks.empty()},
        {"artworks", artworks},
        {"sources", sources}
    };
}


// artwork.getFb2kUrlByPathBatch - Batch generate fb2k:// URLs for multiple tracks
json ArtworkGetFb2kUrlByPathBatch(const json& params) {
    const json* pathList = nullptr;
    if (params.contains("paths") && params["paths"].is_array()) {
        pathList = &params["paths"];
    } else if (params.contains("items") && params["items"].is_array()) {
        pathList = &params["items"];
    }

    if (pathList == nullptr) {
        return {{"success", false}, {"error", "paths or items array is required"}};
    }
    
    std::string type = params.value("type", "front");
    auto typeErr = ValidateArtworkType(type);
    if (!typeErr.is_null()) return typeErr;
    int maxSize = params.value("maxSize", 0);
    
    json artworks = json::array();
    
    for (const auto& pathJson : *pathList) {
        std::string path;
        if (pathJson.is_string()) {
            path = pathJson.get<std::string>();
        } else if (pathJson.is_object()) {
            path = pathJson.value("path", "");
        }

        if (path.empty()) {
            artworks.push_back({{"available", false}, {"error", "invalid path"}});
            continue;
        }
        
        // Path placed in query parameter (not URL path) to avoid
        // Chromium URL normalization of %5C/%2F in path segments
        std::ostringstream oss;
        oss << "fb2k://artwork/?path=" << UrlEncode(path) << "&type=" << type;
        if (maxSize > 0) {
            oss << "&maxSize=" << maxSize;
        }
        
        artworks.push_back({
            {"path", path},
            {"available", true},
            {"type", type},
            {"dataUrl", oss.str()},
        });
    }
    
    return {{"success", true}, {"artworks", artworks}};
}


// artwork.getFolderImages - Get all images in a folder
json ArtworkGetFolderImages(const json& params) {
    std::string directory = params.value("directory", "");
    
    if (directory.empty()) {
        return {{"success", false}, {"error", "directory is required"}};
    }
    
    std::wstring wdir = Utf8ToWide(directory);
    json images = json::array();
    
    static const wchar_t* imageExtensions[] = {L".jpg", L".jpeg", L".png", L".gif", L".bmp", L".webp"};
    
    WIN32_FIND_DATAW findData;
    std::wstring searchPath = wdir + L"\\*";
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);
    
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            
            std::wstring filename = findData.cFileName;
            std::wstring lowerFilename = filename;
            std::transform(lowerFilename.begin(), lowerFilename.end(), 
                lowerFilename.begin(), ::towlower);
            
            bool isImage = false;
            for (const auto& ext : imageExtensions) {
                if (lowerFilename.length() > wcslen(ext) && 
                    lowerFilename.substr(lowerFilename.length() - wcslen(ext)) == ext) {
                    isImage = true;
                    break;
                }
            }
            
            if (isImage) {
                LARGE_INTEGER fileSize;
                fileSize.LowPart = findData.nFileSizeLow;
                fileSize.HighPart = findData.nFileSizeHigh;
                
                std::wstring fullPath = wdir + L"\\" + filename;
                
                images.push_back({
                    {"name", WideToUtf8(filename)},
                    {"path", WideToUtf8(fullPath)},
                    {"size", static_cast<int64_t>(fileSize.QuadPart)}
                });
            }
        } while (FindNextFileW(hFind, &findData));
        
        FindClose(hFind);
    }
    
    return {{"success", true}, {"images", images}};
}

} // namespace

void RegisterArtworkApi() {
    auto& bridge = BridgeCore::GetInstance();

    // Current Playing Artwork
    bridge.RegisterApi("artwork.getCurrent", ArtworkGetCurrent);
    // Track Artwork by Path
    bridge.RegisterApi("artwork.getByPath", ArtworkGetByPath, {{"path", SecurityLevel::MediaRead}});
    bridge.RegisterApi("artwork.getForTrack", ArtworkGetForTrack, {{"path", SecurityLevel::MediaRead}});
    bridge.RegisterApi("artwork.getByPlaylistItem", ArtworkGetByPlaylistItem);
    bridge.RegisterApi("artwork.getAvailableTypes", ArtworkGetAvailableTypes, {{"path", SecurityLevel::MediaRead}});
    // fb2k:// Protocol URL helpers
    bridge.RegisterApi("artwork.getFb2kUrl", ArtworkGetFb2kUrl);
    bridge.RegisterApi("artwork.getFb2kUrlByPath", ArtworkGetFb2kUrlByPath, {{"path", SecurityLevel::MediaRead}});
    // Lyrics
    bridge.RegisterApi("artwork.getLyrics", ArtworkGetLyrics, {{"path", SecurityLevel::MediaRead}});
    // Metadata
    bridge.RegisterApi("artwork.getMetadata", ArtworkGetMetadata, {{"path", SecurityLevel::MediaRead}});
    // Extended Artwork APIs
    bridge.RegisterApi("artwork.getBatch", ArtworkGetBatch, {{"paths", SecurityLevel::MediaRead, true}});
    bridge.RegisterApi("artwork.getAvailableArtwork", ArtworkGetAvailableArtwork, {{"path", SecurityLevel::MediaRead}});
    bridge.RegisterApi("artwork.getFb2kUrlByPathBatch", ArtworkGetFb2kUrlByPathBatch, {{"paths", SecurityLevel::MediaRead, true}});
    bridge.RegisterApi("artwork.getFolderImages", ArtworkGetFolderImages, {{"directory", SecurityLevel::Read}});

    LOG("Artwork API registered (13 APIs)");
}
