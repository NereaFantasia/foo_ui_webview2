#include "pch.h"
#include "api/LibraryApi.h"
#include "api/BridgeCore.h"
#include "api/CallerContext.h"
#include "api/ErrorEnvelope.h"
#include "core/LibraryCache.h"
#include "core/LibraryTreeIndex.h"
#include "core/WebViewContext.h"
#include <foobar2000/SDK/album_art.h>
#include <foobar2000/SDK/album_art_helpers.h>
#include <atomic>
#include <map>
#include <set>
#include <random>
#include "utils/Base64.h"


// ============================================
// Helper: 按分隔符拆分字符串并累计计数（trim 前后空白）
// ============================================
static void SplitAndCount(const char *val, const std::string &separator,
                          std::map<std::string, int> &valueCount) {
  if (separator.empty()) {
    valueCount[val]++;
    return;
  }
  std::string strVal(val);
  size_t start = 0;
  while (start < strVal.size()) {
    size_t pos = strVal.find(separator, start);
    if (pos == std::string::npos) pos = strVal.size();
    size_t begin = start;
    size_t end = pos;
    while (begin < end && (strVal[begin] == ' ' || strVal[begin] == '\t')) begin++;
    while (end > begin && (strVal[end - 1] == ' ' || strVal[end - 1] == '\t')) end--;
    if (end > begin) {
      valueCount[strVal.substr(begin, end - begin)]++;
    }
    start = pos + separator.size();
  }
}

// Helper: 从单条 track 的 file_info 中收集指定字段的值
static void CollectFieldValues(const file_info &info, const std::string &field,
                               const std::string &separator,
                               std::map<std::string, int> &valueCount) {
  t_size valCount = info.meta_get_count_by_name(field.c_str());
  for (t_size j = 0; j < valCount; j++) {
    const char *val = info.meta_get(field.c_str(), j);
    if (!val || !*val) continue;
    SplitAndCount(val, separator, valueCount);
  }
}

// ============================================
// Helper Structures
// ============================================


static const char *DetectMimeType(const uint8_t *data, size_t len) {
  if (len < 4)
    return "image/jpeg";
  const unsigned char *bytes = data;

  if (bytes[0] == 0xFF && bytes[1] == 0xD8 && bytes[2] == 0xFF)
    return "image/jpeg";
  if (bytes[0] == 0x89 && bytes[1] == 0x50 && bytes[2] == 0x4E &&
      bytes[3] == 0x47)
    return "image/png";
  if (bytes[0] == 0x47 && bytes[1] == 0x49 && bytes[2] == 0x46 &&
      bytes[3] == 0x38)
    return "image/gif";
  if (len >= 12 && bytes[0] == 0x52 && bytes[1] == 0x49 && bytes[8] == 0x57 &&
      bytes[9] == 0x45)
    return "image/webp";
  return "image/jpeg";
}

// Get cover art data URL for a track path
// Uses album_art_manager_v2 to support both embedded and external cover art
// (cover.jpg, folder.jpg, etc.)
static std::string GetCoverDataUrl(const std::string &path, int maxSize = 0) {
  if (path.empty())
    return "";

  try {
    abort_callback_dummy abort;

    // Convert path to canonical form
    pfc::string8 canonicalPath;
    filesystem::g_get_canonical_path(path.c_str(), canonicalPath);

    // Create metadb handle for the path
    auto mdb = metadb::get();
    metadb_handle_ptr track = mdb->handle_create(canonicalPath.c_str(), 0);

    if (track.is_valid()) {
      // Use album_art_manager_v2 for best compatibility (supports embedded +
      // external covers)
      auto manager = album_art_manager_v2::get();

      metadb_handle_list items;
      items.add_item(track);

      pfc::list_t<GUID> ids;
      ids.add_item(album_art_ids::cover_front);

      auto extractor = manager->open(items, ids, abort);
      if (extractor.is_valid()) {
        album_art_data::ptr data;
        if (extractor->query(album_art_ids::cover_front, data, abort) &&
            data.is_valid()) {
          const uint8_t *ptr = static_cast<const uint8_t *>(data->data());
          size_t size = data->get_size();

          // Skip if too large (optional size limit)
          if (maxSize > 0 && size > static_cast<size_t>(maxSize) * 1024) {
            return ""; // Too large, skip
          }

          std::string base64 = utils::Base64Encode(ptr, size);
          const char *mimeType = DetectMimeType(ptr, size);
          return std::string("data:") + mimeType + ";base64," + base64;
        }
      }
    }

    // Fallback: try using album_art_extractor directly (for non-library files)
    auto extractor = album_art_extractor::g_open(nullptr, path.c_str(), abort);
    if (extractor.is_valid()) {
      album_art_data::ptr data;
      if (extractor->query(album_art_ids::cover_front, data, abort) &&
          data.is_valid()) {
        const uint8_t *ptr = static_cast<const uint8_t *>(data->data());
        size_t size = data->get_size();

        if (maxSize > 0 && size > static_cast<size_t>(maxSize) * 1024) {
          return "";
        }

        std::string base64 = utils::Base64Encode(ptr, size);
        const char *mimeType = DetectMimeType(ptr, size);
        return std::string("data:") + mimeType + ";base64," + base64;
      }
    }
  } catch (...) {
    // Silently ignore — returns empty string below
  }

  return "";
}

struct AlbumData {
  std::string name;
  std::string artist;
  std::string albumArtist;
  std::string year;
  std::string genre;
  std::string label;
  std::string firstTrackPath; // For cover art retrieval
  size_t trackCount = 0;
  size_t discCount = 0;
  double duration = 0;
  std::set<size_t> discs; // Track unique disc numbers
  std::vector<std::pair<size_t, std::string>>
      tracks; // (trackNum, path) for track list

  AlbumData() = default;
  AlbumData(const AlbumData&) = default;
  AlbumData& operator=(const AlbumData&) = default;
  AlbumData(AlbumData&&) noexcept = default;
  AlbumData& operator=(AlbumData&&) noexcept = default;
};

// ============================================
// Helper Functions
// ============================================

// Worker-safe per-track snapshot for the async library.getAll path.
//
// The main thread captures these fields per track; the heavy JSON
// construction then runs on a CPU worker thread using only the
// snapshot. Both members are thread-safe to carry across threads:
//   - metadb_info_container::ptr returns an immutable snapshot via
//     get_info_ref() (metadb_handle.h:104-115), readable from any thread.
//   - rating is precomputed on the main thread because format_title("%rating%")
//     is main-thread-preferred (metadb_handle.h:69-73).
struct TrackSnapshot {
  metadb_info_container::ptr info; // immutable info snapshot (any-thread read)
  std::string path;                // track->get_path() (logical path)
  std::string absolutePath;        // native filesystem path
  int64_t fileSize = 0;            // track->get_filesize()
  uint32_t subsong = 0;            // track->get_subsong_index()
  int rating = 0;                  // precomputed on main thread (0-5)
  size_t index = 0;                // position in the library list
};

// Compute a track's rating (0-5) on the main thread.
//
// format_title("%rating%") is main-thread-preferred (metadb_handle.h:69-73),
// so this MUST run on the main thread. Falls back to the file's RATING tag
// when foo_playcount is unavailable. Logic is lifted verbatim from the former
// inline body of GetLibraryTrackInfo to preserve behavior for all callers.
static int ComputeTrackRating(metadb_handle_ptr track, const file_info& info) {
  int rating = 0;
  try {
    static titleformat_object::ptr script;
    if (!script.is_valid()) {
      static_api_ptr_t<titleformat_compiler>()->compile_safe(script, "%rating%");
    }
    pfc::string8 result;
    track->format_title(nullptr, result, script, nullptr);
    if (result.get_length() > 0 && result[0] != '?') {
      rating = atoi(result.get_ptr());
    }
  } catch (...) {
    // Silently ignore — falls through to tag fallback
  }
  // Fallback to file tag if foo_playcount not available
  if (rating == 0) {
    const char* tagValue = info.meta_get("rating", 0);
    rating = tagValue ? atoi(tagValue) : 0;
  }
  // Clamp to valid range
  if (rating < 0)
    rating = 0;
  if (rating > 5)
    rating = 5;
  return rating;
}

// Worker-safe JSON builder shared by GetLibraryTrackInfo (main thread) and the
// async library.getAll worker phase. Reads only the immutable info snapshot
// plus the already-captured scalar fields, so it is safe on any thread.
// The field set is identical to GetLibraryTrackInfo's full return shape.
static json BuildTrackJsonFromSnapshot(const TrackSnapshot& snap) {
  if (!snap.info.is_valid()) {
    // Fallback: minimal info (mirrors GetLibraryTrackInfo's invalid-container
    // branch shape).
    return {
        {"index", snap.index},
        {"title", ""},
        {"artist", ""},
        {"album", ""},
        {"duration", 0.0},
        {"path", snap.path},
        {"absolutePath", snap.absolutePath},
        {"rating", snap.rating},
    };
  }

  const file_info& info = snap.info->info();

  auto getMeta = [&](const char* name) -> std::string {
    const char* value = info.meta_get(name, 0);
    return value ? value : "";
  };

  auto getMetaInt = [&](const char* name) -> int {
    const char* value = info.meta_get(name, 0);
    return value ? atoi(value) : 0;
  };

  const char* codecValue = info.info_get("codec");
  std::string codec = codecValue ? codecValue : "";

  return {
      {"index", snap.index},
      {"title", getMeta("title")},
      {"artist", getMeta("artist")},
      {"album", getMeta("album")},
      {"albumArtist", getMeta("album artist")},
      {"genre", getMeta("genre")},
      {"date", getMeta("date")},
      {"trackNumber", getMetaInt("tracknumber")},
      {"discNumber", getMetaInt("discnumber")},
      {"duration", info.get_length()},
      {"path", snap.path},
      {"absolutePath", snap.absolutePath},
      {"fileSize", snap.fileSize},
      {"bitrate", static_cast<int>(info.info_get_bitrate())},
      {"sampleRate", static_cast<int>(info.info_get_int("samplerate"))},
      {"channels", static_cast<int>(info.info_get_int("channels"))},
      {"codec", codec},
      {"subsong", snap.subsong},
      {"rating", snap.rating},
  };
}

json GetLibraryTrackInfo(metadb_handle_ptr track, size_t index) {
  if (!track.is_valid())
    return nullptr;

  // Get native filesystem path
  pfc::string8 nativePath;
  filesystem::g_get_native_path(track->get_path(), nativePath);
  std::string absolutePath = nativePath.get_ptr();

  // Use get_info_ref() instead of deprecated get_info()
  metadb_info_container::ptr infoContainer = track->get_info_ref();
  if (!infoContainer.is_valid()) {
    // Fallback: return minimal info
    return {
        {"index", index},
        {"title", ""},
        {"artist", ""},
        {"album", ""},
        {"duration", track->get_length()},
        {"path", std::string(track->get_path())},
        {"absolutePath", absolutePath},
        {"rating", 0},
    };
  }

  // Rating is main-thread-preferred (format_title), so compute it here and
  // reuse the worker-safe JSON builder for the remaining fields.
  TrackSnapshot snap;
  snap.info = infoContainer;
  snap.path = std::string(track->get_path());
  snap.absolutePath = absolutePath;
  snap.fileSize = static_cast<int64_t>(track->get_filesize());
  snap.subsong = track->get_subsong_index();
  snap.rating = ComputeTrackRating(track, infoContainer->info());
  snap.index = index;

  return BuildTrackJsonFromSnapshot(snap);
}

// ============================================
// API Registration
// ============================================


// ==========================================================================
// Library API handler functions
// ==========================================================================
namespace {

// -- Async library.getAll plumbing ------------------------------------------

// Monotonic request-id generator for the async library.getAll path. Mirrors
// AudioApi's GenerateTaskId pattern.
std::atomic<int> g_libraryRequestIdCounter{0};

std::string GenerateLibraryRequestId() {
  int id = g_libraryRequestIdCounter.fetch_add(1);
  char buf[64];
  sprintf_s(buf, "libraryGetAll_%d", id);
  return buf;
}

// Panel-mode fallback: locate a BridgeCore instance under the same top-level
// window as `hwnd`. Mirrors AudioApi.cpp's FindBridgeByTopLevelAncestor (kept
// file-local; the two namespaces do not share this helper).
BridgeCore* FindBridgeByTopLevelAncestor(WebViewContext& wvc, HWND hwnd) {
  HWND top = ::GetAncestor(hwnd, GA_ROOT);
  if (!top) return nullptr;
  for (auto ih : wvc.GetAllInstances()) {
    if (::GetAncestor(ih, GA_ROOT) == top) {
      if (auto* bridge = wvc.GetBridge(ih)) {
        return bridge;
      }
    }
  }
  return nullptr;
}


// ========== Library Status ==========

json LibraryIsEnabled(const json& params) {
  return {{"success", true}, {"enabled", library_manager::get()->is_library_enabled()}};
}


json LibraryGetStats(const json& params) {
  auto lib = library_manager::get();

  if (!lib->is_library_enabled()) {
    return {
        {"totalTracks", 0},
        {"totalAlbums", 0},
        {"totalArtists", 0},
        {"totalDuration", 0.0},
        {"totalSize", static_cast<int64_t>(0)},
    };
  }

  // Get all items
  metadb_handle_list items;
  lib->get_all_items(items);

  double totalDuration = 0;
  uint64_t totalSize = 0;
  std::set<std::string> albums; // album + album artist 组合
  std::set<std::string> artists;

  for (size_t i = 0; i < items.get_count(); i++) {
    auto &item = items[i];
    if (!item.is_valid())
      continue;

    totalDuration += item->get_length();
    totalSize += item->get_filesize();

    // 使用 get_info_ref() 替代已弃用的 get_info()，避免每轨的 file_info_impl 值拷贝开销
    metadb_info_container::ptr infoContainer = item->get_info_ref();
    const file_info& info = infoContainer->info();
    {
      const char *album = info.meta_get("album", 0);
      const char *artist = info.meta_get("artist", 0);
      const char *albumArtist = info.meta_get("album artist", 0);

      // 使用 album + album artist 组合作为唯一标识（与 getAlbums 保持一致）
      if (album && strlen(album) > 0) {
        std::string albumKey = album;
        albumKey += '\0';
        albumKey += (albumArtist ? albumArtist : (artist ? artist : ""));
        albums.insert(albumKey);
      }
      if (artist && strlen(artist) > 0)
        artists.insert(artist);
    }
  }

  return {
      {"totalTracks", items.get_count()},
      {"totalAlbums", albums.size()},
      {"totalArtists", artists.size()},
      {"totalDuration", totalDuration},
      {"totalSize", static_cast<int64_t>(totalSize)},
      {"cacheValid", g_LibraryCache.IsValid()},
      {"lastModified", g_LibraryCache.GetLastModified()},
  };
}


// ========== Cache Control ==========

json LibraryInvalidateCache(const json& params) {
  g_LibraryCache.Invalidate();
  g_LibraryTreeIndex.Invalidate();
  return {
      {"success", true},
      {"timestamp", g_LibraryCache.GetLastModified()},
  };
}


json LibraryGetCacheStats(const json& params) {
  json stats = g_LibraryCache.GetStats();
  // 合并目录树索引统计字段
  json treeStats = g_LibraryTreeIndex.GetStats();
  for (auto& [key, value] : treeStats.items()) {
      stats[key] = value;
  }
  return stats;
}


// ========== Search (Optimized with search_index) ==========

json LibrarySearch(const json& params) {
  std::string query = params.value("query", "");
  size_t offset = params.value("offset", static_cast<size_t>(0));
  size_t limit = params.value("limit", static_cast<size_t>(100));

  if (query.empty()) {
    return {{"success", true},
            {"tracks", json::array()},
            {"total", 0},
            {"offset", offset},
            {"limit", limit}};
  }

  auto lib = library_manager::get();
  if (!lib->is_library_enabled()) {
    return {{"success", true},
            {"tracks", json::array()},
            {"total", 0},
            {"offset", offset},
            {"limit", limit}};
  }

  try {
    // Create search filter with foobar2000 native query syntax
    search_filter_v2::ptr filter = search_filter_manager_v2::get()->create_ex(
        query.c_str(), fb2k::service_new<completion_notify_dummy>(),
        search_filter_manager_v2::KFlagSuppressNotify |
            search_filter_manager_v2::KFlagAllowSort);

    // Use traditional filter method with pagination
    metadb_handle_list allItems;
    lib->get_all_items(allItems);

    pfc::array_t<bool> mask;
    mask.set_size(allItems.get_count());
    filter->test_multi(allItems, mask.get_ptr());

    // Count total matches and collect paginated results
    metadb_handle_list matchedItems;
    size_t totalMatched = 0;

    for (size_t i = 0; i < allItems.get_count(); i++) {
      if (mask[i]) {
        if (totalMatched >= offset && matchedItems.get_count() < limit) {
          matchedItems.add_item(allItems[i]);
        }
        totalMatched++;
      }
    }

    // Build response
    json tracks = json::array();
    for (size_t i = 0; i < matchedItems.get_count(); i++) {
      tracks.push_back(GetLibraryTrackInfo(matchedItems[i], offset + i));
    }

    return {{"success", true},
            {"items", tracks},  // Standard field
            {"tracks", tracks}, // Legacy field (deprecated)
            {"total", totalMatched},
            {"offset", offset},
            {"limit", limit},
            {"hasMore", offset + matchedItems.get_count() < totalMatched}};
  } catch (const std::exception &e) {
    return {{"success", false},
            {"error", e.what()},
            {"items", json::array()},
            {"tracks", json::array()},
            {"total", 0}};
  } catch (...) {
    return {{"success", false},
            {"error", "Search failed"},
            {"items", json::array()},
            {"tracks", json::array()},
            {"total", 0}};
  }
}


// ========== Albums (Enhanced with full metadata + optional cover + caching)
// ==========

json LibraryGetAlbums(const json& params) {
  auto lib = library_manager::get();
  if (!lib->is_library_enabled()) {
    return {{"success", true}, {"albums", json::array()}, {"total", 0}};
  }

  std::string sortBy = params.value("sort", "name");
  std::string filterQuery = params.value("query", "");
  size_t offset = params.value("offset", static_cast<size_t>(0));
  size_t limit = params.value("limit", static_cast<size_t>(100));
  bool includeTracks = params.value("includeTracks", false);
  bool includeCover = params.value("includeCover", false);
  int coverMaxSize = params.value("coverMaxSize", 500);
  bool useCache = params.value("useCache", true); // NEW: Use cache by default

  // Check cache first (only for full queries without pagination)
  if (useCache && offset == 0 && !includeTracks) {
    auto cached =
        g_LibraryCache.GetCachedAlbums(filterQuery, sortBy, includeCover);
    if (cached.has_value()) {
      json result = cached.value();
      // Apply pagination to cached result
      size_t total = result["albums"].size();
      if (limit < total) {
        json pagedAlbums = json::array();
        for (size_t i = 0; i < std::min(limit, total); i++) {
          pagedAlbums.push_back(result["albums"][i]);
        }
        result["albums"] = pagedAlbums;
        result["hasMore"] = true;
      }
      result["fromCache"] = true;
      return result;
    }
  }

  // Collect album information
  metadb_handle_list items;
  lib->get_all_items(items);

  std::map<std::string, AlbumData> albumMap;

  for (size_t i = 0; i < items.get_count(); i++) {
    auto &item = items[i];
    if (!item.is_valid())
      continue;

    metadb_info_container::ptr infoContainer = item->get_info_ref();
    if (!infoContainer.is_valid())
      continue;
    const file_info& info = infoContainer->info();

    const char *albumName = info.meta_get("album", 0);
    if (!albumName || strlen(albumName) == 0)
      continue;

    // Create unique key: album + album artist (to distinguish same-named
    // albums)
    const char *albumArtist = info.meta_get("album artist", 0);
    if (!albumArtist)
      albumArtist = info.meta_get("artist", 0);
    std::string key = albumName;
    key += '\0';
    key += (albumArtist ? albumArtist : "");

    auto &album = albumMap[key];
    album.name = albumName;
    album.trackCount++;
    album.duration += info.get_length();

    // First track path (for cover art)
    if (album.firstTrackPath.empty()) {
      album.firstTrackPath = item->get_path();
    }

    // Album artist
    if (album.albumArtist.empty() && albumArtist) {
      album.albumArtist = albumArtist;
    }

    // Artist (track artist, may differ from album artist)
    if (album.artist.empty()) {
      const char *artist = info.meta_get("artist", 0);
      if (artist)
        album.artist = artist;
    }

    // Year
    if (album.year.empty()) {
      const char *date = info.meta_get("date", 0);
      if (date)
        album.year = date;
    }

    // Genre
    if (album.genre.empty()) {
      const char *genre = info.meta_get("genre", 0);
      if (genre)
        album.genre = genre;
    }

    // Label
    if (album.label.empty()) {
      const char *label = info.meta_get("publisher", 0);
      if (!label)
        label = info.meta_get("label", 0);
      if (label)
        album.label = label;
    }

    // Disc numbers
    const char *discNum = info.meta_get("discnumber", 0);
    if (discNum) {
      album.discs.insert(atoi(discNum));
    }

    // Track list (optional)
    if (includeTracks) {
      const char *trackNum = info.meta_get("tracknumber", 0);
      album.tracks.push_back(
          {trackNum ? atoi(trackNum) : 0, item->get_path()});
    }
  }

  // Filter by query if provided
  std::vector<AlbumData> filteredAlbums;
  std::string lowerQuery = filterQuery;
  std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(),
                 ::tolower);

  for (const auto &[key, data] : albumMap) {
    if (!filterQuery.empty()) {
      std::string lowerName = data.name;
      std::string lowerArtist =
          data.albumArtist.empty() ? data.artist : data.albumArtist;
      std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                     ::tolower);
      std::transform(lowerArtist.begin(), lowerArtist.end(),
                     lowerArtist.begin(), ::tolower);

      if (lowerName.find(lowerQuery) == std::string::npos &&
          lowerArtist.find(lowerQuery) == std::string::npos) {
        continue;
      }
    }
    filteredAlbums.push_back(data);
  }

  // Sort
  auto sortFunc = [&sortBy](const AlbumData &a, const AlbumData &b) -> bool {
    if (sortBy == "artist") {
      std::string aArtist = a.albumArtist.empty() ? a.artist : a.albumArtist;
      std::string bArtist = b.albumArtist.empty() ? b.artist : b.albumArtist;
      return aArtist < bArtist;
    } else if (sortBy == "year") {
      return a.year > b.year; // Newest first
    } else if (sortBy == "trackCount") {
      return a.trackCount > b.trackCount;
    } else { // name (default)
      return a.name < b.name;
    }
  };
  std::sort(filteredAlbums.begin(), filteredAlbums.end(), sortFunc);

  // Pagination
  size_t total = filteredAlbums.size();
  size_t endIdx = std::min(offset + limit, total);

  // Convert to JSON array
  json albums = json::array();
  for (size_t i = offset; i < endIdx; i++) {
    const auto &data = filteredAlbums[i];
    json albumJson = {
        {"name", data.name},
        {"artist", data.albumArtist.empty() ? data.artist : data.albumArtist},
        {"albumArtist", data.albumArtist},
        {"trackCount", data.trackCount},
        {"discCount", data.discs.empty() ? 1 : data.discs.size()},
        {"duration", data.duration},
        {"year", data.year},
        {"genre", data.genre},
        {"label", data.label},
        {"firstTrackPath", data.firstTrackPath}, // For cover art retrieval
    };

    // Add absolute path for firstTrackPath
    if (!data.firstTrackPath.empty()) {
      pfc::string8 nativePath;
      filesystem::g_get_native_path(data.firstTrackPath.c_str(), nativePath);
      albumJson["firstTrackAbsolutePath"] = std::string(nativePath.get_ptr());

      // NEW: Include cover art data URL if requested
      if (includeCover) {
        std::string coverDataUrl =
            GetCoverDataUrl(data.firstTrackPath, coverMaxSize);
        if (!coverDataUrl.empty()) {
          albumJson["coverDataUrl"] = coverDataUrl;
        }
      }
    }

    if (includeTracks) {
      json trackList = json::array();
      auto sortedTracks = data.tracks;
      std::sort(sortedTracks.begin(), sortedTracks.end());
      for (const auto &[num, path] : sortedTracks) {
        pfc::string8 trackNativePath;
        filesystem::g_get_native_path(path.c_str(), trackNativePath);
        trackList.push_back(
            {{"trackNumber", num},
             {"path", path},
             {"absolutePath", std::string(trackNativePath.get_ptr())}});
      }
      albumJson["tracks"] = trackList;
    }

    albums.push_back(albumJson);
  }

  json result = {{"albums", albums},          {"total", total},
                 {"offset", offset},          {"limit", limit},
                 {"hasMore", endIdx < total}, {"includeCover", includeCover},
                 {"fromCache", false}};

  // Cache the full result (only if this is a complete query)
  if (offset == 0 && !includeTracks && endIdx == total) {
    g_LibraryCache.SetCachedAlbums(filterQuery, sortBy, includeCover, result);
  }

  return result;
}


// ========== Artists ==========

json LibraryGetArtists(const json& params) {
  auto lib = library_manager::get();
  if (!lib->is_library_enabled()) {
    return {{"success", false}, {"error", "Library not enabled"}, {"items", json::array()}, {"count", 0}};
  }

  std::string sortBy = params.value("sort", "name");
  size_t limit = params.value("limit", static_cast<size_t>(1000));

  // 尝试缓存
  auto cached = g_LibraryCache.GetCachedArtists();
  json artists;
  if (cached.has_value()) {
    artists = cached.value();
  } else {
    metadb_handle_list items;
    lib->get_all_items(items);

    struct ArtistData {
      std::string name;
      std::set<std::string> albums;
      size_t trackCount = 0;
      double totalDuration = 0;
    };

    std::map<std::string, ArtistData> artistMap;

    for (size_t i = 0; i < items.get_count(); i++) {
      auto &item = items[i];
      if (!item.is_valid())
        continue;

      metadb_info_container::ptr infoContainer = item->get_info_ref();
      if (!infoContainer.is_valid())
        continue;
      const file_info& info = infoContainer->info();

      const char *artistName = info.meta_get("artist", 0);
      if (!artistName || strlen(artistName) == 0)
        continue;

      auto &artist = artistMap[artistName];
      artist.name = artistName;
      artist.trackCount++;
      artist.totalDuration += info.get_length();

      const char *album = info.meta_get("album", 0);
      if (album && strlen(album) > 0) {
        artist.albums.insert(album);
      }
    }

    artists = json::array();
    for (const auto &[key, data] : artistMap) {
      artists.push_back({
          {"name", data.name},
          {"albumCount", data.albums.size()},
          {"trackCount", data.trackCount},
          {"totalDuration", data.totalDuration},
      });
    }

    // 缓存未排序的完整结果
    g_LibraryCache.SetCachedArtists(artists);
  }

  // Sort
  if (sortBy == "name") {
    std::sort(
        artists.begin(), artists.end(), [](const json &a, const json &b) {
          return a["name"].get<std::string>() < b["name"].get<std::string>();
        });
  } else if (sortBy == "trackCount") {
    std::sort(artists.begin(), artists.end(),
              [](const json &a, const json &b) {
                return a["trackCount"].get<size_t>() >
                       b["trackCount"].get<size_t>();
              });
  } else if (sortBy == "albumCount") {
    std::sort(artists.begin(), artists.end(),
              [](const json &a, const json &b) {
                return a["albumCount"].get<size_t>() >
                       b["albumCount"].get<size_t>();
              });
  }

  // Limit results
  if (artists.size() > limit) {
    artists = json(artists.begin(), artists.begin() + limit);
  }

  return {
      {"success", true},
      {"items", artists},
      {"count", artists.size()}
  };
}


// ========== Genres ==========
// 注意: 此 API 与下方 library.getGenres（带 trackCount 版本）重复注册
// 后注册版本会覆盖此版本，但为遵守仓库规则保留原始代码
// 返回 {success, genres[{name, trackCount}]}
json LibraryGetGenres(const json& params) {
  auto lib = library_manager::get();
  if (!lib->is_library_enabled()) {
    return {{"success", false}, {"error", "Library not enabled"}, {"items", json::array()}, {"count", 0}};
  }

  // 尝试缓存
  auto cached = g_LibraryCache.GetCachedGenres();
  if (cached.has_value()) {
    return cached.value();
  }

  metadb_handle_list items;
  lib->get_all_items(items);

  std::set<std::string> genres;

  for (size_t i = 0; i < items.get_count(); i++) {
    auto &item = items[i];
    if (!item.is_valid())
      continue;

    metadb_info_container::ptr infoContainer = item->get_info_ref();
    if (!infoContainer.is_valid())
      continue;
    const file_info& info = infoContainer->info();

    for (size_t j = 0; j < info.meta_get_count_by_name("genre"); j++) {
      const char *genre = info.meta_get("genre", j);
      if (genre && strlen(genre) > 0) {
        genres.insert(genre);
      }
    }
  }

  json genreItems = json::array();
  for (const auto &genre : genres) {
    genreItems.push_back({{"name", genre}});
  }

  json result = {
      {"success", true},
      {"items", genreItems},
      {"count", genreItems.size()}
  };

  g_LibraryCache.SetCachedGenres(result);
  return result;
}


// ========== Album Tracks ==========

json LibraryGetAlbumTracks(const json& params) {
  std::string albumName = params.value("album", "");
  std::string artistName = params.value("artist", "");

  if (albumName.empty()) {
    return {{"success", true},
            {"items", json::array()},
            {"tracks", json::array()},
            {"total", 0},
            {"album", ""}};
  }

  auto lib = library_manager::get();
  if (!lib->is_library_enabled()) {
    return {{"success", true},
            {"items", json::array()},
            {"tracks", json::array()},
            {"total", 0},
            {"album", albumName}};
  }

  try {
    // 使用 search_filter 替代全量逐条遍历
    auto escapeQuery = [](const std::string& s) -> std::string {
      std::string result;
      result.reserve(s.size());
      for (char c : s) {
        if (c == '"') result += "\"\"";
        else result += c;
      }
      return result;
    };

    std::string query = "album IS \"" + escapeQuery(albumName) + "\"";
    if (!artistName.empty()) {
      query += " AND (\"album artist\" IS \"" + escapeQuery(artistName)
             + "\" OR artist IS \"" + escapeQuery(artistName) + "\")";
    }

    search_filter_v2::ptr filter = search_filter_manager_v2::get()->create_ex(
        query.c_str(), fb2k::service_new<completion_notify_dummy>(),
        search_filter_manager_v2::KFlagSuppressNotify);

    metadb_handle_list allItems;
    lib->get_all_items(allItems);

    pfc::array_t<bool> mask;
    mask.set_size(allItems.get_count());
    filter->test_multi(allItems, mask.get_ptr());

    // 收集匹配的曲目并按曲目号排序
    std::vector<std::pair<metadb_handle_ptr, int>> matchingTracks;

    for (size_t i = 0; i < allItems.get_count(); i++) {
      if (!mask[i]) continue;

      int trackNum = 0;
      metadb_info_container::ptr infoContainer = allItems[i]->get_info_ref();
      if (infoContainer.is_valid()) {
        const char* trackNumStr = infoContainer->info().meta_get("tracknumber", 0);
        if (trackNumStr) trackNum = atoi(trackNumStr);
      }
      matchingTracks.push_back({allItems[i], trackNum});
    }

    std::sort(matchingTracks.begin(), matchingTracks.end(),
              [](const auto &a, const auto &b) { return a.second < b.second; });

    json tracks = json::array();
    for (size_t i = 0; i < matchingTracks.size(); i++) {
      tracks.push_back(GetLibraryTrackInfo(matchingTracks[i].first, i));
    }

    return {{"success", true},
            {"items", tracks},
            {"tracks", tracks},
            {"total", tracks.size()},
            {"album", albumName},
            {"artist", artistName}};
  } catch (...) {
    return {{"items", json::array()},
            {"tracks", json::array()},
            {"total", 0},
            {"album", albumName},
            {"artist", artistName}};
  }
}


// ========== Artist Tracks ==========

json LibraryGetArtistTracks(const json& params) {
  std::string artistName = params.value("artist", "");
  size_t limit = params.value("limit", static_cast<size_t>(500));

  if (artistName.empty()) {
    return {{"success", true}, {"tracks", json::array()}, {"count", 0}, {"artist", ""}};
  }

  auto lib = library_manager::get();
  if (!lib->is_library_enabled()) {
    return {{"success", true}, {"tracks", json::array()}, {"count", 0}, {"artist", artistName}};
  }

  try {
    // 使用 search_filter 替代全量逐条遍历
    auto escapeQuery = [](const std::string& s) -> std::string {
      std::string result;
      result.reserve(s.size());
      for (char c : s) {
        if (c == '"') result += "\"\"";
        else result += c;
      }
      return result;
    };

    std::string query = "artist IS \"" + escapeQuery(artistName) + "\"";

    search_filter_v2::ptr filter = search_filter_manager_v2::get()->create_ex(
        query.c_str(), fb2k::service_new<completion_notify_dummy>(),
        search_filter_manager_v2::KFlagSuppressNotify);

    metadb_handle_list allItems;
    lib->get_all_items(allItems);

    pfc::array_t<bool> mask;
    mask.set_size(allItems.get_count());
    filter->test_multi(allItems, mask.get_ptr());

    json tracks = json::array();
    size_t count = 0;
    for (size_t i = 0; i < allItems.get_count() && count < limit; i++) {
      if (!mask[i]) continue;
      tracks.push_back(GetLibraryTrackInfo(allItems[i], count));
      count++;
    }

    return {{"success", true},
            {"items", tracks},
            {"tracks", tracks},
            {"total", count},
            {"count", count},
            {"artist", artistName}};
  } catch (...) {
    return {{"items", json::array()},
            {"tracks", json::array()},
            {"total", 0},
            {"count", 0},
            {"artist", artistName}};
  }
}


// ========== Random Tracks ==========

json LibraryGetRandomTracks(const json& params) {
  size_t reqCount = params.value("count", static_cast<size_t>(10));

  auto lib = library_manager::get();
  if (!lib->is_library_enabled()) {
    return {{"success", true}, {"tracks", json::array()}, {"count", 0}};
  }

  metadb_handle_list items;
  lib->get_all_items(items);

  if (items.get_count() == 0) {
    return {{"success", true}, {"tracks", json::array()}, {"count", 0}};
  }

  // Generate random indices
  std::vector<size_t> indices;
  for (size_t i = 0; i < items.get_count(); i++) {
    indices.push_back(i);
  }

  // Shuffle
  {
    std::mt19937 rng(std::random_device{}());
    for (size_t i = indices.size() - 1; i > 0; i--) {
      std::uniform_int_distribution<size_t> dist(0, i);
      size_t j = dist(rng);
      std::swap(indices[i], indices[j]);
    }
  }

  // Take first 'count' items
  size_t count = std::min(reqCount, indices.size());

  json tracks = json::array();
  for (size_t i = 0; i < count; i++) {
    tracks.push_back(GetLibraryTrackInfo(items[indices[i]], i));
  }

  return {{"success", true}, {"tracks", tracks}, {"count", tracks.size()}};
}


// ========== Library Operations ==========

json LibraryRescan(const json& params) {
  auto lib = library_manager::get();
  lib->rescan();
  return {{"success", true}};
}


json LibraryAddToPlaylist(const json& params) {
  auto paths = params.value("paths", json::array());
  size_t playlistIndex = params.value("playlist", pfc::infinite_size);

  if (paths.empty()) {
    return {{"success", false}, {"error", "No paths specified"}};
  }

  auto plm = playlist_manager::get();

  if (playlistIndex == pfc::infinite_size) {
    playlistIndex = plm->get_active_playlist();
  }

  if (playlistIndex >= plm->get_playlist_count()) {
    return {{"success", false}, {"error", "Invalid playlist index"}};
  }

  // Resolve paths to handles
  metadb_handle_list handles;
  auto metadb = metadb::get();

  for (const auto &pathJson : paths) {
    std::string path = pathJson.get<std::string>();
    metadb_handle_ptr handle;
    metadb->handle_create(handle, make_playable_location(path.c_str(), 0));
    if (handle.is_valid()) {
      handles.add_item(handle);
    }
  }

  if (handles.get_count() == 0) {
    return {{"success", false}, {"error", "No valid tracks"}};
  }

  // Undo backup before modification
  plm->playlist_undo_backup(playlistIndex);

  // Add to playlist
  plm->playlist_insert_items(playlistIndex, pfc::infinite_size, handles,
                             pfc::bit_array_false());

  return {{"success", true}, {"added", handles.get_count()}};
}


// ========== Extended Library APIs ==========

// library.getArtistAlbums - Get all albums for a specific artist
json LibraryGetArtistAlbums(const json& params) {
  std::string artist = params.value("artist", "");
  size_t limit = params.value("limit", static_cast<size_t>(100));

  if (artist.empty()) {
    return {{"success", false}, {"error", "artist is required"}};
  }

  auto lib = library_manager::get();
  if (!lib->is_library_enabled()) {
    return {{"success", false}, {"error", "Library not enabled"}};
  }

  try {
    // 使用 search_filter 预筛选，减少手动遍历量
    auto escapeQuery = [](const std::string& s) -> std::string {
      std::string result;
      result.reserve(s.size());
      for (char c : s) {
        if (c == '"') result += "\"\"";
        else result += c;
      }
      return result;
    };

    // HAS 做子串匹配，与原代码 find() 语义一致
    std::string query = "artist HAS \"" + escapeQuery(artist) + "\"";

    search_filter_v2::ptr filter = search_filter_manager_v2::get()->create_ex(
        query.c_str(), fb2k::service_new<completion_notify_dummy>(),
        search_filter_manager_v2::KFlagSuppressNotify);

    metadb_handle_list allItems;
    lib->get_all_items(allItems);

    pfc::array_t<bool> mask;
    mask.set_size(allItems.get_count());
    filter->test_multi(allItems, mask.get_ptr());

    // 对匹配结果按 album 分组
    std::map<std::string, json> albumMap;

    for (size_t i = 0; i < allItems.get_count(); i++) {
      if (!mask[i]) continue;

      metadb_info_container::ptr infoContainer = allItems[i]->get_info_ref();
      if (!infoContainer.is_valid()) continue;
      const file_info& info = infoContainer->info();

      const char* trackArtist = info.meta_get("artist", 0);
      const char* album = info.meta_get("album", 0);
      std::string albumName = album ? album : "(Unknown Album)";

      if (albumMap.find(albumName) == albumMap.end()) {
        const char* year = info.meta_get("date", 0);
        albumMap[albumName] = {{"name", albumName},
                               {"artist", trackArtist ? trackArtist : ""},
                               {"year", year ? year : ""},
                               {"trackCount", 1}};
      } else {
        albumMap[albumName]["trackCount"] =
            albumMap[albumName]["trackCount"].get<int>() + 1;
      }

      if (albumMap.size() >= limit) break;
    }

    json albums = json::array();
    for (auto& [name, album] : albumMap) {
      albums.push_back(album);
    }

    return {{"success", true}, {"albums", albums}};
  } catch (...) {
    return {{"success", false}, {"error", "Search failed"}};
  }
}


// library.getGenres - Get all genres with track counts
json LibraryGetGenres_2(const json& params) {
  auto lib = library_manager::get();
  if (!lib->is_library_enabled()) {
    return {{"success", false}, {"error", "Library not enabled"}};
  }

  metadb_handle_list items;
  lib->get_all_items(items);

  std::map<std::string, int> genreCount;

  for (size_t i = 0; i < items.get_count(); i++) {
    auto &item = items[i];
    if (!item.is_valid())
      continue;

    metadb_info_container::ptr infoContainer = item->get_info_ref();
    if (!infoContainer.is_valid())
      continue;
    const file_info& info = infoContainer->info();

    const char *genre = info.meta_get("genre", 0);
    if (genre && strlen(genre) > 0) {
      genreCount[genre]++;
    }
  }

  json genres = json::array();
  for (auto &[name, count] : genreCount) {
    genres.push_back({{"name", name}, {"trackCount", count}});
  }

  return {{"success", true}, {"genres", genres}};
}


// ========== Field Values (Tag System) ==========
// library.getFieldValues - Aggregate unique values for any metadata field
// Generalized version of getGenres: supports multi-value fields and separator splitting
json LibraryGetFieldValues(const json& params) {
  std::string field = params.value("field", "");
  std::string separator = params.value("separator", "");
  size_t limit = params.value("limit", static_cast<size_t>(5000));

  if (field.empty()) {
    return {{"success", false}, {"error", "field is required"}};
  }

  auto lib = library_manager::get();
  if (!lib->is_library_enabled()) {
    return {{"success", false}, {"error", "Library not enabled"}, {"values", json::array()}};
  }

  metadb_handle_list items;
  lib->get_all_items(items);

  std::map<std::string, int> valueCount;

  for (size_t i = 0; i < items.get_count(); i++) {
    auto &item = items[i];
    if (!item.is_valid()) continue;

    // 使用 get_info_ref() 避免 file_info_impl 值拷贝，大库性能显著更好
    metadb_info_container::ptr infoContainer = item->get_info_ref();
    if (!infoContainer.is_valid()) continue;

    CollectFieldValues(infoContainer->info(), field, separator, valueCount);
  }

  // 按 trackCount 降序排列
  struct ValueEntry {
    std::string name;
    int count;
  };
  std::vector<ValueEntry> sorted;
  sorted.reserve(valueCount.size());
  for (auto &[name, count] : valueCount) {
    sorted.push_back({name, count});
  }
  std::sort(sorted.begin(), sorted.end(),
            [](const ValueEntry &a, const ValueEntry &b) {
              return a.count > b.count;
            });

  // 截断到 limit
  if (sorted.size() > limit) sorted.resize(limit);

  json values = json::array();
  for (auto &entry : sorted) {
    values.push_back({{"name", entry.name}, {"trackCount", entry.count}});
  }

  return {{"success", true},
          {"values", values},
          {"total", valueCount.size()},
          {"field", field}};
}


// library.query - Advanced query with TitleFormat
json LibraryQuery(const json& params) {
  std::string query = params.value("query", "");
  size_t limit = params.value("limit", static_cast<size_t>(100));
  std::string sortBy = params.value("sort", "");

  if (query.empty()) {
    return {{"success", false}, {"error", "query is required"}};
  }

  auto lib = library_manager::get();
  if (!lib->is_library_enabled()) {
    return {{"success", false}, {"error", "Library not enabled"}};
  }

  try {
    search_filter_v2::ptr filter;
    filter = search_filter_manager_v2::get()->create_ex(
        query.c_str(), fb2k::service_new<completion_notify_dummy>(),
        search_filter_manager_v2::KFlagSuppressNotify);

    metadb_handle_list allItems;
    lib->get_all_items(allItems);

    pfc::array_t<bool> mask;
    mask.set_size(allItems.get_count());
    filter->test_multi(allItems, mask.get_ptr());

    metadb_handle_list results;
    for (size_t i = 0; i < allItems.get_count(); i++) {
      if (mask[i]) {
        results.add_item(allItems[i]);
      }
    }

    // Sort if requested
    if (!sortBy.empty()) {
      static_api_ptr_t<titleformat_compiler> compiler;
      titleformat_object::ptr script;
      if (compiler->compile(script, sortBy.c_str())) {
        results.sort_by_format(script, nullptr);
      }
    }

    json tracks = json::array();
    for (size_t i = 0; i < results.get_count() && i < limit; i++) {
      tracks.push_back(GetLibraryTrackInfo(results[i], i));
    }

    return {{"success", true},
            {"tracks", tracks},
            {"total", results.get_count()}};
  } catch (...) {
    return {{"success", false}, {"error", "Invalid query syntax"}};
  }
}


// library.browseDirectory - Browse media library by directory
// ========== Root/Tree API ==========

json LibraryGetRoots(const json& params) {
  // 先捕获调用前索引是否已存在，用于判定 fromCache
  bool wasValid = g_LibraryTreeIndex.IsValid();
  json result = g_LibraryTreeIndex.GetRootsJson();
  // 仅当调用前索引已存在时才标记为缓存命中
  if (wasValid && result.value("success", false)) {
      result["fromCache"] = true;
  }
  return result;
}


// ========== Typed Tree API ==========

json LibraryBrowseTree(const json& params) {
  // rootId 必填
  if (!params.contains("rootId") || !params["rootId"].is_string() ||
      params["rootId"].get<std::string>().empty()) {
      FailureHook::LogSync("library.browseTree", ApiErrorCode::REQUIRED_PARAM,
                           "rootId is required");
      return ApiEnvelope::MakeError("rootId is required", ApiErrorCode::REQUIRED_PARAM);
  }

  std::string rootId = params["rootId"].get<std::string>();
  std::string pathId = params.value("pathId", "");
  bool includeFiles = params.value("includeFiles", false);
  bool recursiveFiles = params.value("recursiveFiles", false);

  // includeFiles === false 时忽略 recursiveFiles
  if (!includeFiles) {
      recursiveFiles = false;
  }

  // 获取目录树结构（files 为空数组）
  json result = g_LibraryTreeIndex.GetBrowseTreeJson(rootId, pathId, includeFiles, recursiveFiles);

  if (!result.value("success", false)) {
      return result;
  }

  // 填充 files 数组
  if (includeFiles) {
      metadb_handle_list fileHandles;
      std::vector<size_t> globalIndices;
      g_LibraryTreeIndex.GetDirectoryFileHandles(rootId, pathId, recursiveFiles, fileHandles, globalIndices);

      json filesArray = json::array();
      for (size_t i = 0; i < fileHandles.get_count(); ++i) {
          filesArray.push_back(GetLibraryTrackInfo(fileHandles[i], globalIndices[i]));
      }
      result["files"] = filesArray;
  }

  return result;
}


// ========== Legacy Directory API ==========

json LibraryBrowseDirectory(const json& params) {
  std::string pathStr = params.value("path", "");
  bool includeFiles = params.value("includeFiles", true);

  auto lib = library_manager::get();
  if (!lib->is_library_enabled()) {
    return {{"success", false}, {"error", "Library not enabled"}};
  }

  metadb_handle_list items;
  lib->get_all_items(items);

  std::set<std::string> directories;
  json files = json::array();

  for (size_t i = 0; i < items.get_count(); i++) {
    auto &item = items[i];
    if (!item.is_valid())
      continue;

    std::string fullPath = item->get_path();

    // If path filter specified, check if matches
    if (!pathStr.empty()) {
      // Convert both to lowercase for comparison
      std::string lowerPath = fullPath;
      std::string lowerFilter = pathStr;
      std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(),
                     ::tolower);
      std::transform(lowerFilter.begin(), lowerFilter.end(),
                     lowerFilter.begin(), ::tolower);

      if (lowerPath.find(lowerFilter) != 0)
        continue;
    }

    // Extract directory
    size_t lastSlash = fullPath.rfind('\\');
    if (lastSlash == std::string::npos) {
      lastSlash = fullPath.rfind('/');
    }

    if (lastSlash != std::string::npos) {
      std::string dir = fullPath.substr(0, lastSlash);

      if (pathStr.empty() || dir.length() > pathStr.length()) {
        // Get next level directory
        size_t nextSlash = dir.find_first_of("\\/", pathStr.length() + 1);
        if (nextSlash != std::string::npos) {
          directories.insert(dir.substr(0, nextSlash));
        } else if (dir.length() > pathStr.length()) {
          directories.insert(dir);
        }
      }
    }

    if (includeFiles) {
      files.push_back(GetLibraryTrackInfo(item, i));
    }
  }

  json dirs = json::array();
  for (const auto &dir : directories) {
    dirs.push_back(dir);
  }

  return {
      {"success", true},
      {"directories", dirs},
      {"files", files},
      {"items", dirs} // alias for test compatibility
  };
}


// Additional Library APIs for test compatibility

json LibraryGetStatus(const json& params) {
  auto lib = library_manager::get();
  bool enabled = lib->is_library_enabled();

  // 尝试缓存
  auto cached = g_LibraryCache.GetCachedStats();
  if (cached.has_value()) {
    return cached.value();
  }

  // 使用 enum_items 计数，避免分配完整 metadb_handle_list
  size_t count = 0;
  if (enabled) {
    class counter : public library_manager::enum_callback {
    public:
      size_t& m_count;
      explicit counter(size_t& c) : m_count(c) {}
      bool on_item(const metadb_handle_ptr&) override {
        ++m_count;
        return true;
      }
    };
    counter cb(count);
    lib->enum_items(cb);
  }

  json result = {
      {"enabled", enabled}, {"initialized", enabled},
      {"scanning", false},
      {"itemCount", count}, {"count", count},
  };

  if (enabled) {
    g_LibraryCache.SetCachedStats(result);
  }
  return result;
}


json LibraryGetCount(const json& params) {
  // 使用 enum_items() 遍历计数，避免分配 metadb_handle_list 内存开销
  size_t count = 0;
  class counter : public library_manager::enum_callback {
  public:
      size_t& m_count;
      explicit counter(size_t& c) : m_count(c) {}
      bool on_item(const metadb_handle_ptr&) override {
          ++m_count;
          return true; // continue enumeration
      }
  };
  counter cb(count);
  library_manager::get()->enum_items(cb);
  return {{"success", true}, {"count", count}};
}


json LibraryGetAll(const json& params) {
  // Support both 'offset'/'limit' and 'start'/'count' parameter names
  int offset = params.contains("start") ? params.value("start", 0)
                                        : params.value("offset", 0);
  int limit = params.contains("count") ? params.value("count", 100)
                                       : params.value("limit", 100);
  bool useCache = params.value("useCache", true); // NEW: Use cache by default
  // Opt-in: offload the full-library serialization to a CPU worker thread and
  // deliver the result via the `library:getAllResult` event. Only the SDK
  // `library.getAll` wrapper sets this flag; raw/paginated callers
  // (enumerateTracks, GetLibraryItems, web fb2k.ts) omit it and keep the
  // synchronous contract intact.
  bool asyncResult = params.value("asyncResult", false);

  // Check cache first (only for offset=0 and large requests)
  if (useCache && offset == 0) {
    // Zero-copy cache hit: hold a refcounted immutable handle instead of
    // deep-copying the entire library payload. We only copy the page of
    // tracks the caller actually asked for.
    auto cached = g_LibraryCache.GetCachedTracksShared();
    if (cached) {
      const json& full = *cached;
      size_t total = full["total"].get<size_t>();
      const json& cachedTracks = full["tracks"];

      json pagedTracks = json::array();
      size_t pageEnd = std::min(static_cast<size_t>(limit), cachedTracks.size());
      pagedTracks.get_ref<json::array_t&>().reserve(pageEnd);
      for (size_t i = 0; i < pageEnd; i++) {
        pagedTracks.push_back(cachedTracks[i]);
      }

      return json{{"tracks", pagedTracks},
                  {"items", pagedTracks},
                  {"total", total},
                  {"offset", offset},
                  {"limit", limit},
                  {"fromCache", true}};
    }
  }

  metadb_handle_list all;
  library_manager::get()->get_all_items(all);

  size_t end = std::min((size_t)(offset + limit), all.get_count());

  // -- Async cache-warm path (offloaded serialization) --------------------
  // Trigger only when: (1) caller opted in, (2) cache is being used,
  // (3) this is the full-library query (offset 0 covering every track) — the
  // exact condition that previously serialized ~all tracks on the main thread
  // and blocked subsequent invokes. The main thread snapshots the immutable
  // info containers + rating; a CPU worker thread builds JSON; the main thread
  // then emits the result to the originating WebView instance.
  if (asyncResult && useCache && offset == 0 && end == all.get_count()) {
    // Capture worker-safe snapshots on the main thread.
    auto snapshots = std::make_shared<std::vector<TrackSnapshot>>();
    snapshots->reserve(all.get_count());
    for (size_t i = 0; i < all.get_count(); i++) {
      const auto& track = all[i];
      if (!track.is_valid())
        continue;

      TrackSnapshot snap;
      snap.info = track->get_info_ref(); // immutable snapshot, any-thread safe
      snap.path = std::string(track->get_path());
      pfc::string8 nativePath;
      filesystem::g_get_native_path(track->get_path(), nativePath);
      snap.absolutePath = nativePath.get_ptr();
      snap.fileSize = static_cast<int64_t>(track->get_filesize());
      snap.subsong = track->get_subsong_index();
      // rating uses format_title (main-thread-preferred), compute here.
      snap.rating = snap.info.is_valid()
                        ? ComputeTrackRating(track, snap.info->info())
                        : 0;
      snap.index = i;
      snapshots->push_back(std::move(snap));
    }

    std::string requestId = GenerateLibraryRequestId();
    size_t total = all.get_count();

    // Capture caller routing context while still on the main thread.
    auto caller = CallerContext::FromParams(params);
    HWND callerHwnd = caller.callerHwnd;
    std::string callerWindowId = caller.windowId;

    // Build JSON on a CPU worker thread.
    fb2k::inCpuWorkerThread([snapshots, requestId, total, offset, limit,
                             callerHwnd, callerWindowId]() {
      // Route the result event back to the originating WebView instance.
      // Mirrors AudioApi::ExecuteWaveformGeneration's emitToCaller lambda.
      auto emitToCaller = [callerHwnd, callerWindowId](const std::string& event,
                                                       const json& data) {
        auto& wvc = WebViewContext::GetInstance();
        if (!callerWindowId.empty() && wvc.SendEventTo(callerWindowId, event, data))
          return;
        if (callerHwnd) {
          if (auto* bridge = wvc.GetBridge(callerHwnd)) {
            bridge->EmitEvent(event, data);
            return;
          }
          if (auto* bridge = FindBridgeByTopLevelAncestor(wvc, callerHwnd)) {
            bridge->EmitEvent(event, data);
            return;
          }
        }
        BridgeCore::GetInstance().EmitEvent(event, data);
      };

      try {
        json tracks = json::array();
        tracks.get_ref<json::array_t&>().reserve(snapshots->size());
        for (const auto& snap : *snapshots) {
          tracks.push_back(BuildTrackJsonFromSnapshot(snap));
        }

        json result = {{"requestId", requestId},
                       {"tracks", tracks},
                       {"items", tracks},
                       {"total", total},
                       {"offset", offset},
                       {"limit", limit},
                       {"fromCache", false}};

        // LibraryCache is shared_mutex-protected; writing from the worker is
        // safe and warms the cache for subsequent (synchronous) cache hits.
        g_LibraryCache.SetCachedTracks(result);

        // Deliver on the main thread (PostWebMessageAsJson is UI-thread).
        fb2k::inMainThread([emitToCaller, result]() {
          emitToCaller("library:getAllResult", result);
        });
      } catch (const std::exception& e) {
        std::string errMsg = e.what();
        fb2k::inMainThread([emitToCaller, requestId, offset, limit, errMsg]() {
          json result = {{"requestId", requestId}, {"tracks", json::array()},
                         {"items", json::array()}, {"total", 0},
                         {"offset", offset},        {"limit", limit},
                         {"fromCache", false},      {"error", errMsg}};
          emitToCaller("library:getAllResult", result);
        });
      }
    });

    // Handler returns synchronously; the full result arrives via the event.
    return json{{"pending", true}, {"requestId", requestId}};
  }

  // -- Synchronous path (unchanged) ---------------------------------------
  json tracks = json::array();
  for (size_t i = offset; i < end; i++) {
    const auto& track = all[i];
    if (!track.is_valid())
      continue;

    // Use the standard GetLibraryTrackInfo helper (includes absolutePath)
    tracks.push_back(GetLibraryTrackInfo(track, i));
  }

  json result = {{"tracks", tracks},         {"items", tracks},
                 {"total", all.get_count()}, {"offset", offset},
                 {"limit", limit},           {"fromCache", false}};

  // Cache the full result if this is a complete query (all tracks)
  if (offset == 0 && end == all.get_count()) {
    g_LibraryCache.SetCachedTracks(result);
  }

  return result;
}


json LibraryGetByPath(const json& params) {
  std::string path = params.value("path", "");
  if (path.empty())
    return {{"success", false}, {"error", "path is required"}};

  // O(log n) handle creation: 使用 canonical path 直接创建 handle
  pfc::string8 canonicalPath;
  filesystem::g_get_canonical_path(path.c_str(), canonicalPath);
  
  auto mdb = metadb::get();
  metadb_handle_ptr handle = mdb->handle_create(canonicalPath.c_str(), 0);
  
  // O(1) hash lookup: 验证 handle 是否在库中
  if (!handle.is_valid() || !library_manager::get()->is_item_in_library(handle)) {
    return {{"success", true}, {"found", false}, {"path", path}};
  }
  
  // 使用 get_info_ref() 避免值拷贝
  metadb_info_container::ptr infoContainer = handle->get_info_ref();
  const file_info& info = infoContainer->info();
  
  pfc::string8 nativePath;
  filesystem::g_get_native_path(handle->get_path(), nativePath);
  
  auto getMeta = [&](const char *name) -> std::string {
    const char *v = info.meta_get(name, 0);
    return v ? v : "";
  };

  return {{"success", true},
          {"found", true},
          {"path", handle->get_path()},
          {"absolutePath", std::string(nativePath.get_ptr())},
          {"title", getMeta("title")},
          {"artist", getMeta("artist")},
          {"album", getMeta("album")},
          {"duration", info.get_length()},
          {"trackNumber", getMeta("tracknumber")},
          {"genre", getMeta("genre")},
          {"date", getMeta("date")}};
}


// library.getRecentlyAdded - Get recently added tracks
// sortBy: "added" (requires foo_playcount, auto-fallback) or "modified" (SDK native)
json LibraryGetRecentlyAdded(const json& params) {
  int limit = params.value("limit", 50);
  std::string sortBy = params.value("sortBy", "added");
  bool fallback = false;

  metadb_handle_list all;
  library_manager::get()->get_all_items(all);
  size_t total = all.get_count();

  if (total == 0) {
    return {
      {"success", true}, {"tracks", json::array()},
      {"total", 0}, {"limit", limit},
      {"sortBy", sortBy}, {"fallback", false}
    };
  }

  // --- sortBy=="added": use %added% titleformat (foo_playcount) ---
  if (sortBy == "added") {
    static titleformat_object::ptr tfAdded;
    if (!tfAdded.is_valid()) {
      static_api_ptr_t<titleformat_compiler>()->compile_safe(tfAdded, "%added%");
    }

    // Collect (index, added_string) pairs
    struct IndexedTime {
      size_t index;
      std::string timeStr;
    };
    std::vector<IndexedTime> entries;
    entries.reserve(total);
    int validCount = 0;

    for (size_t i = 0; i < total; i++) {
      pfc::string8 formatted;
      all[i]->format_title(nullptr, formatted, tfAdded, nullptr);
      std::string ts = formatted.c_str();
      if (!ts.empty() && ts != "?" && ts != "N/A") {
        validCount++;
      }
      entries.push_back({i, ts});
    }

    // If foo_playcount not available (all "?"), fallback to "modified"
    if (validCount == 0) {
      sortBy = "modified";
      fallback = true;
    } else {
      // Sort by added timestamp descending (lexicographic works for ISO dates)
      std::sort(entries.begin(), entries.end(), [](const IndexedTime& a, const IndexedTime& b) {
        // Invalid timestamps sort to the end
        bool aValid = !a.timeStr.empty() && a.timeStr != "?" && a.timeStr != "N/A";
        bool bValid = !b.timeStr.empty() && b.timeStr != "?" && b.timeStr != "N/A";
        if (aValid != bValid) return aValid > bValid;
        return a.timeStr > b.timeStr;
      });

      size_t count = std::min(static_cast<size_t>(limit), entries.size());
      json tracks = json::array();
      for (size_t i = 0; i < count; i++) {
        json item = GetLibraryTrackInfo(all[entries[i].index], entries[i].index);
        item["added"] = entries[i].timeStr;
        tracks.push_back(item);
      }

      return {
        {"success", true}, {"tracks", tracks},
        {"total", total}, {"limit", limit},
        {"sortBy", "added"}, {"fallback", false}
      };
    }
  }

  // --- sortBy=="modified": use file modification timestamp (SDK native) ---
  struct IndexedTimestamp {
    size_t index;
    t_filetimestamp ts;
  };
  std::vector<IndexedTimestamp> entries;
  entries.reserve(total);

  for (size_t i = 0; i < total; i++) {
    t_filestats stats = all[i]->get_filestats();
    entries.push_back({i, stats.m_timestamp});
  }

  // Sort by timestamp descending
  std::sort(entries.begin(), entries.end(), [](const IndexedTimestamp& a, const IndexedTimestamp& b) {
    return a.ts > b.ts;
  });

  size_t count = std::min(static_cast<size_t>(limit), entries.size());
  json tracks = json::array();
  for (size_t i = 0; i < count; i++) {
    json item = GetLibraryTrackInfo(all[entries[i].index], entries[i].index);
    // Convert Windows FILETIME (100ns since 1601) to Unix timestamp (seconds since 1970)
    if (entries[i].ts != filetimestamp_invalid) {
      item["modified"] = static_cast<int64_t>((entries[i].ts - 116444736000000000ULL) / 10000000ULL);
    }
    tracks.push_back(item);
  }

  return {
    {"success", true}, {"tracks", tracks},
    {"total", total}, {"limit", limit},
    {"sortBy", sortBy}, {"fallback", fallback}
  };
}


json LibraryRefresh(const json& params) {
  library_manager::get()->rescan();
  return {{"success", true}};
}

} // namespace

void RegisterLibraryApi() {
    auto& bridge = BridgeCore::GetInstance();

    bridge.RegisterApi("library.isEnabled", LibraryIsEnabled);
    bridge.RegisterApi("library.getStats", LibraryGetStats);
    bridge.RegisterApi("library.invalidateCache", LibraryInvalidateCache);
    bridge.RegisterApi("library.getCacheStats", LibraryGetCacheStats);
    bridge.RegisterApi("library.search", LibrarySearch);
    bridge.RegisterApi("library.getAlbums", LibraryGetAlbums);
    bridge.RegisterApi("library.getArtists", LibraryGetArtists);
    bridge.RegisterApi("library.getGenres", LibraryGetGenres_2);
    bridge.RegisterApi("library.getAlbumTracks", LibraryGetAlbumTracks);
    bridge.RegisterApi("library.getArtistTracks", LibraryGetArtistTracks);
    bridge.RegisterApi("library.getRandomTracks", LibraryGetRandomTracks);
    bridge.RegisterApi("library.rescan", LibraryRescan);
    bridge.RegisterApi("library.addToPlaylist", LibraryAddToPlaylist);
    bridge.RegisterApi("library.getArtistAlbums", LibraryGetArtistAlbums);
    bridge.RegisterApi("library.getFieldValues", LibraryGetFieldValues);
    bridge.RegisterApi("library.query", LibraryQuery);
    bridge.RegisterApi("library.getRoots", LibraryGetRoots);
    bridge.RegisterApi("library.browseTree", LibraryBrowseTree);
    bridge.RegisterApi("library.browseDirectory", LibraryBrowseDirectory);
    bridge.RegisterApi("library.getStatus", LibraryGetStatus);
    bridge.RegisterApi("library.getCount", LibraryGetCount);
    bridge.RegisterApi("library.getAll", LibraryGetAll);
    bridge.RegisterApi("library.getByPath", LibraryGetByPath, {{"path", SecurityLevel::MediaRead}});
    bridge.RegisterApi("library.getRecentlyAdded", LibraryGetRecentlyAdded);
    bridge.RegisterApi("library.refresh", LibraryRefresh);
}
