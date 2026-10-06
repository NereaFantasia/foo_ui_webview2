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
#include "api/LyricsFilePolicy.h"
#include "api/ArtworkRequestParser.h"
#include "api/BridgeCore.h"
#include "api/PlaylistApi.h"
#include "api/PlaylistTarget.h"
#include "api/TypedApi.h"
#include "api/generated/ArtworkSchema.h"
#include "api/MetaAccess.h"
#include <foobar2000/SDK/album_art.h>
#include <foobar2000/SDK/album_art_helpers.h>
#include <sstream>
#include <iomanip>
#include <array>
#include "utils/Base64.h"
#include "utils/SubsongUtils.h"

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
// artwork_request::Parse() in api/ArtworkRequestParser.cpp decodes it on the way back
// ==========================================================================
[[maybe_unused]] static std::string UrlEncode(const std::string& str) {
    return artwork_request::UrlEncode(str);
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

// `abort` is handed to album_art_manager_v2 so extraction can be interrupted;
// exception_aborted is rethrown rather than swallowed as "no artwork".
static bool GetArtworkBinaryForMetadbHandle(const metadb_handle_ptr& track, const GUID& artType,
                                            BinaryArtworkInternal& out, abort_callback& abort) {
    if (!track.is_valid()) return false;

    // 1) now_playing_album_art_notify_manager (fast, cached) - only for cover_front
    // IMPORTANT:
    // - This cache is ONLY valid for the currently playing track and ONLY holds cover_front.
    // - playback_control is main-thread-only (SDK: "work from main app thread only").
    //   Calling get_now_playing() from fb2k::inCpuWorkerThread raises uBugCheck
    //   (failure_00000253). Worker/async artwork extraction must skip this shortcut.
    if (artType == album_art_ids::cover_front && core_api::is_main_thread()) {
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
    } catch (const exception_aborted&) {
        throw;
    } catch (...) {
        // ignore
    }

    return false;
}

// Parse subsong path: "path|subsong:N" -> { filePath, subsongIndex }. The rules and the
// console line for an unreadable index are SubsongUtils::ParseSubsongPath's.
static std::pair<std::string, t_uint32> ArtworkParseSubsongPath(const std::string& path) {
    return SubsongUtils::ParseSubsongPath(path);
}

// Classify path kind for hook evidence
static const char* ArtworkClassifyPathKind(const std::string& path) {
    if (path.starts_with("file-relative://")) return "file-relative://";
    if (path.starts_with("file://")) return "file://";
    if (path.find(fb2k_paths::kSubsongMarker) != std::string::npos) return "path|subsong";
    return "native";
}

// Shared helper: canonicalize path and create handle for artwork/lyrics sibling APIs
static metadb_handle_ptr ArtworkResolveHandle(const std::string& path, const char* api) {
    auto [filePath, subsong] = ArtworkParseSubsongPath(path);
    std::string canonicalPath;
    metadb_handle_ptr track = SubsongUtils::CreateCanonicalHandle(filePath.c_str(), subsong, canonicalPath);

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
        if (!GetArtworkBinaryForMetadbHandle(track, artType, tmp, fb2k::noAbort)) {
            return false;
        }

        out.bytes = std::move(tmp.bytes);
        out.mimeType = std::move(tmp.mimeType);
        return true;
    } catch (...) {
        return false;
    }
}

bool GetArtworkBinaryForPath(const std::string& path, const std::string& type, BinaryArtwork& out,
                             abort_callback& abort) {
    if (path.empty()) return false;

    GUID artType = StringToArtType(type);

    try {
        // Parse subsong from path (e.g. "file.cue|subsong:3")
        auto [filePath, subsong] = ArtworkParseSubsongPath(path);
        metadb_handle_ptr track = SubsongUtils::CreateCanonicalHandle(filePath, subsong);

        BinaryArtworkInternal tmp;
        if (track.is_valid()) {
            // For non-current track requests, skip now_playing_manager cache by going straight to v2+extractor
            // We'll reuse helper but it may hit now_playing_manager; that's acceptable (still correct).
            if (!GetArtworkBinaryForMetadbHandle(track, artType, tmp, abort)) {
                return false;
            }
        } else {
            // track handle is invalid, can't get artwork
            return false;
        }

        out.bytes = std::move(tmp.bytes);
        out.mimeType = std::move(tmp.mimeType);
        return true;
    } catch (const exception_aborted&) {
        throw;
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
        metadb_handle_ptr track = SubsongUtils::CreateCanonicalHandle(filePath, subsong);
        return GetArtworkSourcePathsForMetadbHandle(track, artType, outPaths);
    } catch (...) {
        outPaths.clear();
        return false;
    }
}

} // namespace artwork_internal



// ==========================================================================
// Artwork API handler functions：形状由 src/api/schema/artwork.ts 声明，参数解析
// （含 type 枚举与路径安全检查）在进入 handler 之前完成。
// ==========================================================================
namespace {

namespace art = api::artwork;

// 把一张图片的字节填进带 mimeType / size / dataUrl 三个可选字段的结果结构。
template <class R>
void PutPicture(R& out, const uint8_t* ptr, size_t size) {
    const char* mimeType = DetectMimeType(ptr, size);
    out.mimeType = mimeType;
    out.size = static_cast<std::int64_t>(size);
    out.dataUrl = std::string("data:") + mimeType + ";base64," + utils::Base64Encode(ptr, size);
}

// album_art_manager_v2 打开一个句柄并取一种图片；没有时返回 false。
bool QueryManagerArtwork(const metadb_handle_ptr& track, const GUID& artType, album_art_data::ptr& data,
                         abort_callback& abort) {
    auto manager = album_art_manager_v2::get();
    metadb_handle_list items;
    items.add_item(track);
    pfc::list_t<GUID> ids;
    ids.add_item(artType);
    auto extractor = manager->open(items, ids, abort);
    if (!extractor.is_valid()) return false;
    return extractor->query(artType, data, abort) && data.is_valid() && data->get_size() > 0;
}

api::Failure FileRelativeRefused(const std::string& path, const char* apiName) {
    LOG("[PATH_HOOK] api=%s inputPath=%s pathKind=file-relative:// canonicalPath=null sink=extractor usedNowPlayingShortcut=false resultKind=extractor-rejected",
        apiName, path.c_str());
    return api::Fail("file-relative:// paths require playlist context. Use artwork.getByPlaylistItem instead.",
                     ApiErrorCode::INVALID_PATH);
}

// ========== Current Playing Artwork ==========

api::Result<art::GetCurrentResult> ArtworkGetCurrent(const art::GetCurrentParams& p) {
    art::GetCurrentResult result;
    result.available = false;
    result.type = p.type;
    const GUID artType = StringToArtType(p.type);

    try {
        auto pc = playback_control::get();
        metadb_handle_ptr track;
        if (!pc->get_now_playing(track) || !track.is_valid()) {
            LOG("artwork.getCurrent: No track playing");
            result.reason = "no_track";
            return result;
        }

        const char* trackPath = track->get_path();
        LOG("artwork.getCurrent: %s", trackPath);

        // Method 1: now_playing_album_art_notify_manager (fastest, cached). The cache only
        // holds cover_front, so other types skip it.
        if (artType == album_art_ids::cover_front) {
            auto manager = now_playing_album_art_notify_manager::get();
            auto data = manager->current();
            if (data.is_valid() && data->get_size() > 0) {
                LOG("artwork.getCurrent: Found via now_playing_manager (%zu bytes)", data->get_size());
                PutPicture(result, static_cast<const uint8_t*>(data->data()), data->get_size());
                result.source = "now_playing_manager";
                result.available = true;
                return result;
            }
        }

        // Method 2: album_art_manager_v2 (more comprehensive)
        abort_callback_dummy abort;
        album_art_data::ptr artData;
        if (QueryManagerArtwork(track, artType, artData, abort)) {
            LOG("artwork.getCurrent: Found via album_art_manager_v2 (%zu bytes)", artData->get_size());
            PutPicture(result, static_cast<const uint8_t*>(artData->data()), artData->get_size());
            result.source = "album_art_manager_v2";
            result.available = true;
            return result;
        }

        // Method 3: album_art_extractor directly
        auto directExtractor = album_art_extractor::g_open(nullptr, trackPath, abort);
        if (directExtractor.is_valid()) {
            album_art_data::ptr direct;
            if (directExtractor->query(artType, direct, abort) && direct.is_valid() && direct->get_size() > 0) {
                LOG("artwork.getCurrent: Found via direct extractor (%zu bytes)", direct->get_size());
                PutPicture(result, static_cast<const uint8_t*>(direct->data()), direct->get_size());
                result.source = "extractor";
                result.available = true;
                return result;
            }
        }

        LOG("artwork.getCurrent: No artwork found for %s", trackPath);
        result.reason = "not_found";
        result.path = trackPath;
        return result;
    } catch (const std::exception& e) {
        LOG("artwork.getCurrent error: %s", e.what());
        return api::Fail(e.what(), ApiErrorCode::OPERATION_FAILED);
    }
}

// Extractor-path contract: native path, file://, path|subsong:N (subsong stripped, the
// extractor works per file). file-relative:// needs playlist context and is refused.
api::Result<art::GetByPathResult> ArtworkGetByPath(const art::GetByPathParams& p) {
    if (p.path.starts_with("file-relative://")) {
        return FileRelativeRefused(p.path, "artwork.getByPath");
    }

    art::GetByPathResult result;
    result.available = false;
    result.type = p.type;
    result.path = p.path;

    auto [filePath, subsong] = ArtworkParseSubsongPath(p.path);
    pfc::string8 canonicalPath;
    filesystem::g_get_canonical_path(filePath.c_str(), canonicalPath);
    const GUID artType = StringToArtType(p.type);

    try {
        abort_callback_dummy abort;
        auto extractor = album_art_extractor::g_open(nullptr, canonicalPath.c_str(), abort);
        if (!extractor.is_valid()) {
            LOG("[PATH_HOOK] api=artwork.getByPath resultKind=extractor-invalid (no extractor for path)");
            return result;
        }
        album_art_data::ptr data;
        if (!extractor->query(artType, data, abort) || !data.is_valid()) {
            LOG("[PATH_HOOK] api=artwork.getByPath resultKind=extractor-invalid (art type not found)");
            return result;
        }
        LOG("[PATH_HOOK] api=artwork.getByPath resultKind=extractor-valid size=%zu", data->get_size());
        PutPicture(result, static_cast<const uint8_t*>(data->data()), data->get_size());
        result.available = true;
        return result;
    } catch (const exception_album_art_not_found&) {
        LOG("[PATH_HOOK] api=artwork.getByPath resultKind=extractor-invalid (not found exception)");
        return result;
    } catch (const std::exception& e) {
        LOG("[PATH_HOOK] api=artwork.getByPath resultKind=extractor-invalid error=%s", e.what());
        return api::Fail(e.what(), ApiErrorCode::OPERATION_FAILED);
    }
}

// album_art_manager_v2 path: also finds folder covers, and measures PNG dimensions.
api::Result<art::GetForTrackResult> ArtworkGetForTrack(const art::GetForTrackParams& p) {
    if (p.path.starts_with("file-relative://")) {
        return FileRelativeRefused(p.path, "artwork.getForTrack");
    }

    art::GetForTrackResult result;
    result.available = false;
    result.type = p.type;
    result.path = p.path;
    const GUID artType = StringToArtType(p.type);

    try {
        abort_callback_dummy abort;
        metadb_handle_ptr track = ArtworkResolveHandle(p.path, "artwork.getForTrack");
        if (!track.is_valid()) return result;

        album_art_data::ptr data;
        if (!QueryManagerArtwork(track, artType, data, abort)) return result;

        const uint8_t* ptr = static_cast<const uint8_t*>(data->data());
        const size_t size = data->get_size();
        PutPicture(result, ptr, size);

        // Dimensions are only read out of a PNG header (offsets 16 and 20, big-endian).
        std::int64_t width = 0, height = 0;
        if (size > 24 && ptr[0] == 0x89 && ptr[1] == 'P' && ptr[2] == 'N' && ptr[3] == 'G') {
            width = (ptr[16] << 24) | (ptr[17] << 16) | (ptr[18] << 8) | ptr[19];
            height = (ptr[20] << 24) | (ptr[21] << 16) | (ptr[22] << 8) | ptr[23];
        }
        result.width = width;
        result.height = height;
        result.available = true;
        return result;
    } catch (const exception_album_art_not_found&) {
        return result;
    } catch (const std::exception& e) {
        return api::Fail(e.what(), ApiErrorCode::OPERATION_FAILED);
    }
}

api::Result<art::GetByPlaylistItemResult> ArtworkGetByPlaylistItem(const art::GetByPlaylistItemParams& p) {
    auto plm = playlist_manager::get();

    // 负的 playlist 表示活动列表，与省略同义。
    const std::optional<std::int64_t> requested =
        p.playlist >= 0 ? std::optional<std::int64_t>(p.playlist) : std::nullopt;
    size_t playlistIndex = 0;
    if (auto failure = api::ResolvePlaylistTarget(*GetPlaylistService(), requested, p.playlistGuid,
                                                  playlistIndex)) {
        return std::move(*failure);
    }

    const size_t itemIndex = (p.index >= 0) ? static_cast<size_t>(p.index) : 0;
    const size_t count = plm->playlist_get_item_count(playlistIndex);
    if (itemIndex >= count) {
        return api::Fail("Index out of range", ApiErrorCode::NOT_FOUND);
    }

    metadb_handle_ptr track;
    if (!plm->playlist_get_item_handle(track, playlistIndex, itemIndex)) {
        return api::Fail("Failed to get track handle", ApiErrorCode::OPERATION_FAILED);
    }

    art::GetByPlaylistItemResult result;
    result.available = false;
    result.type = p.type;
    result.playlist = static_cast<std::int64_t>(playlistIndex);
    result.playlistGuid = api::PlaylistGuidOf(*GetPlaylistService(), playlistIndex);
    result.index = static_cast<std::int64_t>(itemIndex);
    const GUID artType = StringToArtType(p.type);

    try {
        abort_callback_dummy abort;
        album_art_data::ptr data;
        if (!QueryManagerArtwork(track, artType, data, abort)) return result;
        PutPicture(result, static_cast<const uint8_t*>(data->data()), data->get_size());
        result.available = true;
        return result;
    } catch (const exception_album_art_not_found&) {
        return result;
    } catch (const std::exception& e) {
        LOG("getByPlaylistItem error: %s", e.what());
        return api::Fail(e.what(), ApiErrorCode::OPERATION_FAILED);
    }
}

api::Result<art::GetAvailableTypesResult> ArtworkGetAvailableTypes(const art::GetAvailableTypesParams& p) {
    art::GetAvailableTypesResult result;
    std::string path = p.path.value_or("");

    try {
        abort_callback_dummy abort;

        if (path.empty()) {
            auto pc = playback_control::get();
            metadb_handle_ptr track;
            if (!pc->get_now_playing(track)) {
                return api::Fail("No track specified and nothing playing", ApiErrorCode::NO_ACTIVE_ITEM);
            }
            path = track->get_path();
        }

        auto extractor = album_art_extractor::g_open_allowempty(nullptr, path.c_str(), abort);
        if (!extractor.is_valid()) return result;

        // The probe order is the reported order.
        static const struct { const char* name; GUID id; } artTypes[] = {
            {"front", album_art_ids::cover_front},
            {"back", album_art_ids::cover_back},
            {"disc", album_art_ids::disc},
            {"icon", album_art_ids::icon},
            {"artist", album_art_ids::artist},
        };
        for (const auto& at : artTypes) {
            if (extractor->have_entry(at.id, abort)) {
                result.types.emplace_back(at.name);
            }
        }
    } catch (const std::exception& e) {
        LOG("getAvailableTypes error: %s", e.what());
    }

    return result;
}

// ========== fb2k:// Protocol URL helpers ==========

api::Failure UrlBuildRefused(artwork_request::ParseError error) {
    return api::Fail(std::string("Invalid artwork URL parameters: ") + artwork_request::ParseErrorName(error),
                     ApiErrorCode::INVALID_PARAMS);
}

api::Result<art::GetFb2kUrlResult> ArtworkGetFb2kUrl(const art::GetFb2kUrlParams& p) {
    art::GetFb2kUrlResult result;
    result.available = false;
    result.type = p.type;

    try {
        auto pc = playback_control::get();
        metadb_handle_ptr track;
        if (!pc->get_now_playing(track) || !track.is_valid()) {
            LOG("[fb2k://] getFb2kUrl: No playing track");
            result.reason = "no_track";
            return result;
        }

        const char* trackPath = track->get_path();
        LOG("[fb2k://] getFb2kUrl: trackPath=%s", trackPath);

        // The path goes in a query parameter, not the URL path, so Chromium's normalisation
        // of %5C / %2F in path segments cannot touch it.
        const auto built = artwork_request::BuildFb2kArtworkUrl(trackPath, p.type, p.maxSize);
        if (!built.ok()) return UrlBuildRefused(built.error);
        LOG("[fb2k://] getFb2kUrl: url=%s", built.url.c_str());

        result.available = true;
        result.type = built.type;
        result.dataUrl = built.url;
        return result;
    } catch (const std::exception& e) {
        LOG("[fb2k://] getFb2kUrl error: %s", e.what());
        return api::Fail(e.what(), ApiErrorCode::OPERATION_FAILED);
    }
}

api::Result<art::GetFb2kUrlByPathResult> ArtworkGetFb2kUrlByPath(const art::GetFb2kUrlByPathParams& p) {
    const auto built = artwork_request::BuildFb2kArtworkUrl(p.path, p.type, p.maxSize);
    if (!built.ok()) return UrlBuildRefused(built.error);

    art::GetFb2kUrlByPathResult result;
    result.available = true;
    result.type = built.type;
    result.path = p.path;
    result.dataUrl = built.url;
    return result;
}

// ========== Lyrics ==========

api::Result<art::GetLyricsResult> ArtworkGetLyrics(const art::GetLyricsParams& p) {
    const std::string path = p.path.value_or("");
    try {
        metadb_handle_ptr track;
        if (path.empty()) {
            auto pc = playback_control::get();
            if (!pc->get_now_playing(track)) {
                return api::Fail("No track playing", ApiErrorCode::NO_ACTIVE_ITEM);
            }
            LOG("[PATH_HOOK] api=artwork.getLyrics usedNowPlayingShortcut=true resultKind=%s",
                track.is_valid() ? "handle-valid" : "handle-invalid");
        } else {
            track = ArtworkResolveHandle(path, "artwork.getLyrics");
            if (!track.is_valid()) {
                return api::Fail("Failed to create track handle", ApiErrorCode::NOT_FOUND);
            }
        }

        metadb_info_container::ptr infoContainer = SubsongUtils::GetInfoOrReadFile(track);
        const file_info& info = infoContainer->info();

        art::GetLyricsResult result;
        result.available = false;

        // Probe order decides: the first non-empty tag wins.
        for (const char* tag : lyrics_file_policy::kKnownLyricsTags) {
            const char* value = info.meta_get(tag, 0);
            if (value && value[0] != '\0') {
                result.available = true;
                result.tag = tag;
                result.lyrics = value;
                result.synced = strstr(tag, "SYNC") != nullptr && strstr(tag, "UNSYNC") == nullptr;
                return result;
            }
        }
        return result;
    } catch (const std::exception& e) {
        LOG("getLyrics error: %s", e.what());
        return api::Fail(e.what(), ApiErrorCode::OPERATION_FAILED);
    }
}

api::Result<art::GetMetadataResult> ArtworkGetMetadata(const art::GetMetadataParams& p) {
    const std::string path = p.path.value_or("");
    try {
        metadb_handle_ptr track;
        if (path.empty()) {
            auto pc = playback_control::get();
            if (!pc->get_now_playing(track)) {
                return api::Fail("No track playing", ApiErrorCode::NO_ACTIVE_ITEM);
            }
            LOG("[PATH_HOOK] api=artwork.getMetadata usedNowPlayingShortcut=true resultKind=%s",
                track.is_valid() ? "handle-valid" : "handle-invalid");
        } else {
            track = ArtworkResolveHandle(path, "artwork.getMetadata");
            if (!track.is_valid()) {
                return api::Fail("Failed to create track handle", ApiErrorCode::NOT_FOUND);
            }
        }

        metadb_info_container::ptr infoContainer = SubsongUtils::GetInfoOrReadFile(track);
        const file_info& info = infoContainer->info();

        const auto single = [&info](const char* name) -> std::string {
            const char* value = info.meta_get(name, 0);
            return value ? value : "";
        };

        art::GetMetadataResult result;
        result.available = true;
        result.album = single("ALBUM");
        result.artist = MetaJoined(info, "ARTIST");
        result.albumArtist = MetaJoined(info, "ALBUM ARTIST");
        result.title = single("TITLE");
        result.year = single("DATE");
        result.genre = MetaJoined(info, "GENRE");
        result.trackNumber = single("TRACKNUMBER");
        result.discNumber = single("DISCNUMBER");

        // Embedded artwork: any of the five standard types counts, not just cover_front.
        bool hasEmbedded = false;
        try {
            abort_callback_dummy abort;
            auto extractor = album_art_extractor::g_open(nullptr, track->get_path(), abort);
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
        result.hasEmbedded = hasEmbedded;

        bool foundLyrics = false;
        for (const char* tag : lyrics_file_policy::kKnownLyricsTags) {
            if (info.meta_exists(tag)) { foundLyrics = true; break; }
        }
        result.hasLyrics = foundLyrics;
        return result;
    } catch (const std::exception& e) {
        LOG("getMetadata error: %s", e.what());
        return api::Fail(e.what(), ApiErrorCode::OPERATION_FAILED);
    }
}

// ========== Extended Artwork APIs ==========

api::Result<art::GetBatchResult> ArtworkGetBatch(const art::GetBatchParams& p) {
    const GUID artType = StringToArtType(p.type);
    art::GetBatchResult result;
    result.artworks.reserve(p.paths.size());

    for (const auto& path : p.paths) {
        art::ArtworkBatchRow row;
        row.path = path;
        row.available = false;
        try {
            abort_callback_dummy abort;
            auto [filePath, subsong] = ArtworkParseSubsongPath(path);
            auto extractor = album_art_extractor::g_open(nullptr, filePath.c_str(), abort);
            if (extractor.is_valid()) {
                album_art_data::ptr data;
                if (extractor->query(artType, data, abort) && data.is_valid()) {
                    PutPicture(row, static_cast<const uint8_t*>(data->data()), data->get_size());
                    row.available = true;
                }
            }
        } catch (...) {
            // A file that cannot be opened keeps its row with available: false.
        }
        result.artworks.push_back(std::move(row));
    }
    return result;
}

api::Result<art::GetAvailableArtworkResult> ArtworkGetAvailableArtwork(const art::GetAvailableArtworkParams& p) {
    auto [filePath, subsong] = ArtworkParseSubsongPath(p.path);
    art::GetAvailableArtworkResult result;
    result.available = false;

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
            for (const auto& at : artTypes) {
                album_art_data::ptr data;
                try {
                    if (extractor->query(at.id, data, abort) && data.is_valid()) {
                        art::ArtworkAvailableEntry entry;
                        entry.type = at.name;
                        entry.source = "embedded";
                        result.artworks.push_back(std::move(entry));
                    }
                } catch (...) {}
            }
            if (!result.artworks.empty()) {
                result.sources.emplace_back("embedded");
            }
        }

        // External cover files next to the track. This branch cuts the directory out of the
        // path as a string and hands it to a Win32 file API, so it needs the native form:
        // fb2k gives "file://D:\Music\x.flac" for a local file, whose directory part never
        // matches anything on disk. The extractor above keeps receiving fb2k's own form.
        std::string scanPath;
        if (SubsongUtils::TryResolveNativeMediaPath(filePath, scanPath)) {
            size_t lastSlash = scanPath.rfind('\\');
            if (lastSlash == std::string::npos) lastSlash = scanPath.rfind('/');

            if (lastSlash != std::string::npos) {
                const std::wstring wdir = Utf8ToWide(scanPath.substr(0, lastSlash));
                static const wchar_t* coverFiles[] = {
                    L"cover.jpg", L"cover.png", L"folder.jpg", L"folder.png",
                    L"front.jpg", L"front.png", L"album.jpg", L"album.png"
                };
                for (const auto& coverFile : coverFiles) {
                    const std::wstring fullPath = wdir + L"\\" + coverFile;
                    if (GetFileAttributesW(fullPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
                        result.sources.push_back("folder:" + WideToUtf8(coverFile));
                    }
                }
            }
        }
    } catch (...) {}

    result.available = !result.artworks.empty();
    return result;
}

api::Result<art::GetFb2kUrlByPathBatchResult> ArtworkGetFb2kUrlByPathBatch(const art::GetFb2kUrlByPathBatchParams& p) {
    static constexpr size_t kMaxBatchItems = 100;
    const bool hasPaths = p.paths.has_value();
    const bool hasItems = p.items.has_value();
    if (hasPaths == hasItems) {
        return api::Fail("exactly one of paths or items is required", ApiErrorCode::INVALID_PARAMS);
    }
    const size_t count = hasPaths ? p.paths->size() : p.items->size();
    if (count > kMaxBatchItems) {
        return api::Fail("artwork batch limit exceeded", ApiErrorCode::INVALID_PARAMS);
    }

    art::GetFb2kUrlByPathBatchResult result;
    result.artworks.reserve(count);

    const auto build = [&](const std::string& path, const std::string& type, std::optional<std::int64_t> maxSize) {
        art::ArtworkUrlRow row;
        row.path = path;
        row.available = false;
        if (path.empty()) {
            row.error = "invalid path";
        } else if (const auto built = artwork_request::BuildFb2kArtworkUrl(path, type, maxSize); !built.ok()) {
            row.error = artwork_request::ParseErrorName(built.error);
        } else {
            row.available = true;
            row.type = built.type;
            row.dataUrl = built.url;
        }
        result.artworks.push_back(std::move(row));
    };

    if (hasPaths) {
        for (const auto& path : *p.paths) build(path, p.type, p.maxSize);
    } else {
        // An entry's own type and limit win over the batch-wide ones.
        for (const auto& item : *p.items) {
            build(item.path, item.type.value_or(p.type), item.maxSize.has_value() ? item.maxSize : p.maxSize);
        }
    }
    return result;
}

api::Result<art::GetFolderImagesResult> ArtworkGetFolderImages(const art::GetFolderImagesParams& p) {
    const std::wstring wdir = Utf8ToWide(p.directory);
    art::GetFolderImagesResult result;

    static const wchar_t* imageExtensions[] = {L".jpg", L".jpeg", L".png", L".gif", L".bmp", L".webp"};

    WIN32_FIND_DATAW findData;
    const std::wstring searchPath = wdir + L"\\*";
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) return result;

    do {
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;

        const std::wstring filename = findData.cFileName;
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
        if (!isImage) continue;

        LARGE_INTEGER fileSize;
        fileSize.LowPart = findData.nFileSizeLow;
        fileSize.HighPart = findData.nFileSizeHigh;

        art::ArtworkFolderImage image;
        image.name = WideToUtf8(filename);
        image.path = WideToUtf8(wdir + L"\\" + filename);
        image.size = static_cast<std::int64_t>(fileSize.QuadPart);
        result.images.push_back(std::move(image));
    } while (FindNextFileW(hFind, &findData));

    FindClose(hFind);
    return result;
}

} // namespace

void RegisterArtworkApi() {
    // 路径参数的安全级别来自声明（生成头里的 kPathParams），不再在这里登记。
    api::RegisterApi("artwork.getCurrent", ArtworkGetCurrent);
    api::RegisterApi("artwork.getByPath", ArtworkGetByPath);
    api::RegisterApi("artwork.getForTrack", ArtworkGetForTrack);
    api::RegisterApi("artwork.getByPlaylistItem", ArtworkGetByPlaylistItem);
    api::RegisterApi("artwork.getAvailableTypes", ArtworkGetAvailableTypes);
    api::RegisterApi("artwork.getFb2kUrl", ArtworkGetFb2kUrl);
    api::RegisterApi("artwork.getFb2kUrlByPath", ArtworkGetFb2kUrlByPath);
    api::RegisterApi("artwork.getLyrics", ArtworkGetLyrics);
    api::RegisterApi("artwork.getMetadata", ArtworkGetMetadata);
    api::RegisterApi("artwork.getBatch", ArtworkGetBatch);
    api::RegisterApi("artwork.getAvailableArtwork", ArtworkGetAvailableArtwork);
    api::RegisterApi("artwork.getFb2kUrlByPathBatch", ArtworkGetFb2kUrlByPathBatch);
    api::RegisterApi("artwork.getFolderImages", ArtworkGetFolderImages);

    LOG("Artwork API registered (13 APIs)");
}
