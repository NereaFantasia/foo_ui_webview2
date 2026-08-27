#include "pch.h"
#include "api/LibraryApi.h"
#include "api/BridgeCore.h"
#include "api/CallerContext.h"
#include "api/ErrorEnvelope.h"
#include "api/MetaAccess.h"
#include "api/TrackWireSnapshot.h"
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
#include "utils/JsonWriter.h"
#include "utils/StringUtils.h"


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

  // artist 与 artists 共用一次字段查找：拼接串必须是数组按 ", " join 的结果
  // （artists.join(", ") === artist 是对外契约），两边各查一次既多一次 meta_find，
  // 也留下两侧取值漂移的口子。键集必须与 wire 版 WriteTrackJson 一致，否则
  // getAll(DOM) 与 query(wire) 形状分叉。
  const std::vector<std::string> artistValues = MetaValuesRaw(info, "artist");

  return {
      {"index", snap.index},
      {"title", getMeta("title")},
      {"artist", JoinMetaValues(artistValues, ", ")},
      {"artists", artistValues},
      {"album", getMeta("album")},
      {"albumArtist", MetaJoined(info, "album artist")},
      {"genre", MetaJoined(info, "genre")},
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
    if (!infoContainer.is_valid())
      continue;
    const file_info& info = infoContainer->info();
    {
      const char *album = info.meta_get("album", 0);
      const char *artist = info.meta_get("artist", 0);
      const char *albumArtist = info.meta_get("album artist", 0);

      // 使用 album + album artist 组合作为唯一标识（与 getAlbums 保持一致）
      // albumKey 的回退项刻意仍取首值：改成全值会让本函数的 totalAlbums
      // 与 getAlbums.total 失配
      if (album && strlen(album) > 0) {
        std::string albumKey = album;
        albumKey += '\0';
        albumKey += (albumArtist ? albumArtist : (artist ? artist : ""));
        albums.insert(albumKey);
      }
      // totalArtists 按参与艺术家计：多值 artist 的每个值各计一次，
      // 与 getArtists 的条目数口径一致。同字段查两次是刻意的，本函数
      // 不在送达热路径上
      for (const auto &artistValue : MetaValues(info, "artist"))
        artists.insert(artistValue);
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


// ============================================================================
// library.query / library.search 的延迟响应直写管线
// ============================================================================
//
// 线程模型：
//   主线程段：参数解析 → 前置校验（含 fields 白名单）→ create_ex →
//             get_all_items + test_multi → 收集命中 handle（query 收全量或前
//             limit 条，search 只收 [offset, offset+limit) 这一页）→ 每曲标量
//             捕获（按 fields 掩码逐项门控）→ 编译 titleformat 脚本 → 把请求派给
//             CPU worker
//   worker 段：（query 请求排序时）算排序序并截 limit → queryMultiParallel_ 批量
//             取 rec → rating（仅当被请求）→ 直写 UTF-8（省略 fields 出全 20 键，
//             传 fields 出投影键集）→ SendRaw
//
// fields 投影是纯收窄：掩码由主线程解析一次随请求下传，捕获侧、rating、序列化侧
// 三处各自按位门控。省略 fields 时掩码为全集，三处判定恒真，与投影前逐字等价。
//
// 查询段为什么仍留主线程：search_index 的 search() 可在任意线程跑，但它的输出是
// 内部索引序，与现契约的库序不等价（实测四组查询集合全等、顺序全不等），换引擎
// 即改变 tracks 顺序这一可观测面，故只把后段（批量元数据 + 序列化）下 worker。
//
// 每曲标量的线程放置依据：
//   · path / absolutePath 留主线程。get_path() 本身可跨线程（location 随 handle
//     不可变：metadb_handle.h:42-43 的 "valid till the object is released" 加
//     metadb.h:298 的"一个位置只有一个 handle"），但 absolutePath 要过
//     filesystem::g_get_native_path —— 它对非 file:// 路径转派
//     filesystem_v3::getNativePath（filesystem.cpp:136-156，实现方可以是第三方
//     插件）且 SDK 无线程标注，本仓库现有调用点全在主线程，无跨线程先例。
//     absolutePath 既然留主线程，path 顺手一起捕获不额外要钱。
//   · fileSize 走 get_filestats()（"最近一次"文件状态，是可变态），照 library.getAll
//     async 路径的快照先例在主线程取；subsong 同批取，省一次遍历。
//   · rating、元数据、排序键全在 worker：rating 用 metadb_v2::formatTitle_v2
//     （免数据库访问），排序用 SDK 自己的 sort_by_format_get_order。
//
// 每请求恰好一次响应：主线程段的错误路径直接经 responder 回**正常**响应体（形状
// 与同步版逐字一致，不能落成框架错误信封）；worker 段顶层 try/catch 同样回正常
// 响应体 —— cpuThreadPool 会静默吞掉未捕获异常，漏响应即页面侧 30s 超时假死。

// 主线程为每曲捕获的标量。输出数组内的 index 不入表：它由序列化侧的
// baseIndex + i 决定（query 从 0 起、search 从 offset 起，与同步版
// GetLibraryTrackInfo 的传参口径一致）。
struct QueryTrackCapture {
  std::string path;
  std::string absolutePath;
  int64_t fileSize = 0;
  uint32_t subsong = 0;
  bool valid = false;  // handle 无效时为 false，序列化侧照同步版输出 JSON null
};

// fieldMask 逐项门控：投影模式下未被请求的字段一次 SDK 调用都不做。四项里
// absolutePath 是唯一可能转派进第三方实现的（g_get_native_path 对非 file:// 路径
// 走 filesystem_v3::getNativePath），只要标签不要路径的查询由此完全绕开它。
// handle 无效判定不受掩码影响：那决定的是数组里出 null 还是出对象，属形状面。
QueryTrackCapture CaptureQueryTrack(const metadb_handle_ptr& track, uint32_t fieldMask) {
  QueryTrackCapture cap;
  if (!track.is_valid()) {
    return cap;  // 同步版 GetLibraryTrackInfo 对无效 handle 直接返回 null
  }
  if (fieldMask & TrackField::kPath) {
    cap.path = track->get_path();
  }
  if (fieldMask & TrackField::kAbsolutePath) {
    pfc::string8 nativePath;
    filesystem::g_get_native_path(track->get_path(), nativePath);
    cap.absolutePath = nativePath.get_ptr();
  }
  if (fieldMask & TrackField::kFileSize) {
    cap.fileSize = static_cast<int64_t>(track->get_filesize());
  }
  if (fieldMask & TrackField::kSubsong) {
    cap.subsong = track->get_subsong_index();
  }
  cap.valid = true;
  return cap;
}

// 两个 API 的信封与错误体形状不同，不得混写。
enum class QueryWireApi { Query, Search };

// 失败响应体：形状与各自同步版的 catch 分支逐字一致。query 的同步版把一切异常
// 折成同一个固定串，故此处忽略 message。
json MakeQueryWireErrorBody(QueryWireApi api, const char* message) {
  if (api == QueryWireApi::Query) {
    return {{"success", false}, {"error", "Invalid query syntax"}};
  }
  return {{"success", false},
          {"error", message ? message : "Search failed"},
          {"tracks", json::array()},
          {"total", 0}};
}

// fields 校验失败的响应体。这条路径随 fields 参数新增，不在"错误形状不变"的回归面
// 内（那管的是 query 语法错与 search 失败两条既有路径），故两个 API 共用同一形状：
// ApiEnvelope::MakeError 产出的 success:false 正常响应体 + 机器可读 code，与其余
// API 的参数错一致。
//
// 不用 DeferredResponder::SendError：那是框架错误信封通道（BridgeCore.h:78-82 明文
// 只给框架兜底用），页面侧收到 error 字段会把 Promise reject 掉
// （WebViewHost.cpp:794-799），而本仓库所有参数校验失败都是 resolve 出 success:false。
json MakeTrackFieldsErrorBody(const TrackFieldSelection& fields) {
  if (fields.unknownFields.empty()) {
    return ApiEnvelope::MakeError(fields.errorMessage, ApiErrorCode::INVALID_PARAMS);
  }
  // 未知名回给调用方：拼写错误不静默丢字段，也不用逐个试
  json unknown = json::array();
  for (const std::string& name : fields.unknownFields) {
    unknown.push_back(name);
  }
  return ApiEnvelope::MakeError(fields.errorMessage, ApiErrorCode::INVALID_PARAMS,
                                {{"unknownFields", unknown}});
}

// 一次请求在 worker 段要用的全部输入。用 shared_ptr 传递：rating 兜底会在
// worker → 主线程 → worker 之间多跳一次，各段必须共享同一份状态。
struct QueryWireRequest {
  QueryWireApi api = QueryWireApi::Query;
  const char* method = "";                  // 静态串，只用于耗时日志
  metadb_handle_list tracks;                // 待序列化的命中集
  std::vector<QueryTrackCapture> caps;      // 与 tracks 同序同长
  std::vector<metadb_v2::rec_t> recs;       // 批量取回的 info 记录，按槽位写入
  std::vector<metadb_info_container::ptr> infos;  // 逐曲最终生效的 info 容器
  std::vector<int> ratings;
  titleformat_object::ptr ratingScript;     // 主线程编译；空 = 未请求 rating
  titleformat_object::ptr sortScript;       // 主线程编译；空 = 不排序
  uint32_t fieldMask = TrackField::kAll;    // 请求的字段集；省略 fields 时为全集
  bool projected = false;                   // true = 显式传了 fields，走投影写法
  size_t limit = 0;                         // 信封回显值，同时是截断上界
  size_t offset = 0;                        // search 信封字段
  size_t baseIndex = 0;                     // 输出 index 起点
  size_t total = 0;                         // 信封 total（截断前的命中总数）
  std::chrono::steady_clock::time_point workerStart;
};

// rating 的 worker 版：解析、回退、clamp 与主线程版 ComputeTrackRating 逐步一致，
// 只把取值通道从 format_title 换成 formatTitle_v2（用预取的 rec，免数据库访问）。
// 不在此吞异常 —— 抛出即触发调用方的整批主线程兜底。
int ComputeTrackRatingFromRec(const metadb_v2::ptr& mdb, const metadb_handle_ptr& track,
                              const metadb_v2::rec_t& rec, const file_info& info,
                              const titleformat_object::ptr& script) {
  int rating = 0;
  pfc::string8 result;
  mdb->formatTitle_v2(track, rec, nullptr, result, script, nullptr);
  if (result.get_length() > 0 && result[0] != '?') {
    rating = atoi(result.get_ptr());
  }
  // Fallback to file tag if foo_playcount not available
  if (rating == 0) {
    const char* tagValue = info.meta_get("rating", 0);
    rating = tagValue ? atoi(tagValue) : 0;
  }
  if (rating < 0) rating = 0;
  if (rating > 5) rating = 5;
  return rating;
}

// tracks 数组文本（含首尾方括号），query 与 search 的信封各拼接一次。
std::string BuildTracksArrayJson(const QueryWireRequest& req) {
  const size_t count = req.tracks.get_count();
  const uint32_t mask = req.fieldMask;

  std::string out;
  // 单曲全字段实测均值 582 字节，按 600 一次备足，避免增长期的多次搬运。投影时按
  // 请求字段数缩放（上限仍是全字段那一档）：八万行一律按 600 备足会在 x86 宿主上
  // 白占几十 MB，而这条路径的存在意义正是压掉那几十 MB。
  const size_t perRow =
      req.projected ? std::min<size_t>(600, 32 + 64 * TrackField::Count(mask)) : 600;
  out.reserve(count * perRow + 2);
  out.push_back('[');

  TrackWireSnapshot snap;  // 循环外复用：字符串成员保住容量，省掉每曲重新分配
  for (size_t i = 0; i < count; ++i) {
    if (i > 0) out.push_back(',');

    if (!req.caps[i].valid) {
      // handle 无效：同步版 GetLibraryTrackInfo 在此返回 JSON null，数组里就是一个
      // null 元素。此形状原样保留（投影也不例外：null 是整行形态，不是字段集）。
      out.append("null");
      continue;
    }

    // 以下逐字段的 mask 判定在全字段路径上恒真（mask == kAll），取值与写入次序
    // 因此与投影前逐字不变；投影时未被请求的字段连取值都不做，省掉的是每行的
    // meta_get / info_get 与随后的 SafeUtf8 拷贝，不只是几个字节的输出。
    snap.index = req.baseIndex + i;
    if (mask & TrackField::kPath) {
      snap.path = StringUtils::SafeUtf8(req.caps[i].path);
    }
    if (mask & TrackField::kAbsolutePath) {
      snap.absolutePath = StringUtils::SafeUtf8(req.caps[i].absolutePath);
    }

    const metadb_info_container::ptr& infoHolder = req.infos[i];
    if (!infoHolder.is_valid()) {
      // info 容器无效：同步版走 8 键 fallback，键集在此保持不变。duration 取 0.0
      // —— 同步版这一支写的是 track->get_length()，而 get_length() 内部就是
      // get_info_ref()->info().get_length()，容器为空时它自己就会解空指针，故该
      // 取值不可复现；rating 同步版硬写 0，此处 ratings[i] 也恒为 0（rating 循环
      // 跳过无 info 的曲目）。
      snap.hasInfo = false;
      snap.title.clear();
      snap.artist.clear();
      snap.album.clear();
      snap.duration = 0.0;
      snap.rating = req.ratings[i];
      if (req.projected) {
        // 投影下损坏条目同样出全部请求键，fallback 不产出的字段取类型默认
        ResetFieldsAbsentFromFallback(snap);
        WriteTrackJsonProjected(out, snap, mask);
      } else {
        WriteTrackJson(out, snap);
      }
      continue;
    }

    const file_info& info = infoHolder->info();
    auto getMeta = [&](const char* name) -> std::string {
      return StringUtils::SafeUtf8(info.meta_get(name, 0));
    };
    auto getMetaInt = [&](const char* name) -> int {
      const char* value = info.meta_get(name, 0);
      return value ? atoi(value) : 0;
    };

    snap.hasInfo = true;
    if (mask & TrackField::kTitle) snap.title = getMeta("title");
    // artists 被请求时，artist 从同一次枚举结果 join 出来（保住 artists.join(", ")
    // === artist），字段查找不翻倍；只要 artist 不要 artists 时仍走 MetaJoined 的
    // 单值快路径（count == 1 不分配 vector）。
    if (mask & TrackField::kArtists) {
      auto raw = MetaValuesRaw(info, "artist");
      snap.artists = raw;
      if (mask & TrackField::kArtist) snap.artist = JoinMetaValues(raw, ", ");
    } else if (mask & TrackField::kArtist) {
      snap.artist = MetaJoined(info, "artist");
    }
    if (mask & TrackField::kAlbum) snap.album = getMeta("album");
    if (mask & TrackField::kAlbumArtist) snap.albumArtist = MetaJoined(info, "album artist");
    if (mask & TrackField::kGenre) snap.genre = MetaJoined(info, "genre");
    if (mask & TrackField::kDate) snap.date = getMeta("date");
    if (mask & TrackField::kTrackNumber) snap.trackNumber = getMetaInt("tracknumber");
    if (mask & TrackField::kDiscNumber) snap.discNumber = getMetaInt("discnumber");
    if (mask & TrackField::kDuration) snap.duration = info.get_length();
    if (mask & TrackField::kFileSize) snap.fileSize = req.caps[i].fileSize;
    if (mask & TrackField::kBitrate) snap.bitrate = static_cast<int>(info.info_get_bitrate());
    if (mask & TrackField::kSampleRate) {
      snap.sampleRate = static_cast<int>(info.info_get_int("samplerate"));
    }
    if (mask & TrackField::kChannels) {
      snap.channels = static_cast<int>(info.info_get_int("channels"));
    }
    if (mask & TrackField::kCodec) snap.codec = StringUtils::SafeUtf8(info.info_get("codec"));
    if (mask & TrackField::kSubsong) snap.subsong = req.caps[i].subsong;
    if (mask & TrackField::kRating) snap.rating = req.ratings[i];

    if (req.projected) {
      WriteTrackJsonProjected(out, snap, mask);
    } else {
      WriteTrackJson(out, snap);
    }
  }

  out.push_back(']');
  return out;
}

// 信封字符串：字段集与各自同步版逐字段一致（键序不同，契约只承诺语义等价）。
std::string BuildQueryWireEnvelope(const QueryWireRequest& req, const std::string& tracksJson) {
  std::string envelope;

  if (req.api == QueryWireApi::Query) {
    envelope.reserve(tracksJson.size() + 64);
    envelope.append("{\"success\":true,\"tracks\":");
    envelope.append(tracksJson);
    envelope.append(",\"total\":");
    JsonWriter::AppendJsonInt(envelope, static_cast<int64_t>(req.total));
    envelope.push_back('}');
    return envelope;
  }

  envelope.reserve(tracksJson.size() + 128);
  envelope.append("{\"success\":true,\"tracks\":");
  envelope.append(tracksJson);
  envelope.append(",\"total\":");
  JsonWriter::AppendJsonInt(envelope, static_cast<int64_t>(req.total));
  envelope.append(",\"offset\":");
  JsonWriter::AppendJsonInt(envelope, static_cast<int64_t>(req.offset));
  envelope.append(",\"limit\":");
  JsonWriter::AppendJsonInt(envelope, static_cast<int64_t>(req.limit));
  envelope.append(",\"hasMore\":");
  JsonWriter::AppendJsonBool(envelope, req.offset + req.tracks.get_count() < req.total);
  envelope.push_back('}');
  return envelope;
}

// 零行短路：待序列化行数为 0 时就地回包，不派 worker。返回 true = 已回包。
//
// 动机：零命中查询本身只有零点几毫秒，deferred 的两次线程跳变反而让它涨到 6ms
// 级、越过"小结果集不得回归"的门槛；而零行没有任何可卸载的重活，卸载只剩纯税。
// 主线程调用 responder 时 inMainThread2 就地执行，回包时机与同步版同拍。
//
// 形状一致性不靠"另写一份对齐"，而是复用 worker 路径同一个信封构造函数、tracks
// 段固定 "[]"（BuildTracksArrayJson 在 0 行时的产物就是它）→ 逐字节相同。total
// 仍取真实全命中数：行数为 0 不只有零命中一种成因（limit=0、search 的 offset
// 越界翻页都会给出空页而 total 非零），写死 0 会篡改分页语义。
bool SendEmptyQueryWireResultIfNoRows(const std::shared_ptr<QueryWireRequest>& req,
                                      const DeferredResponder& responder) {
  if (req->tracks.get_count() != 0) {
    return false;
  }
  responder.SendRaw(BuildQueryWireEnvelope(*req, "[]"));
  return true;
}

// worker 段末尾：序列化 + 拼信封 + 耗时日志 + 回包。ratings 必须已就绪。
void FinishQueryWireOnWorker(const std::shared_ptr<QueryWireRequest>& req,
                             const DeferredResponder& responder) {
  try {
    std::string payload = BuildQueryWireEnvelope(*req, BuildTracksArrayJson(*req));

    // deferred 后 ApiPerformanceTracker 只覆盖主线程段，worker 段的耗时若不在此
    // 补一条，慢查询就从性能日志里消失。阈值取与 ApiPerformanceTracker 的 INFO
    // 同一档（50ms），免得小查询刷屏。console::printf 实为 pfc printf，只认
    // %s/%i/%d/%u/%x/%c（没有 %f/%llu），所以数字先拼进串、单 %s 送出。
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now() - req->workerStart)
                             .count();
    if (elapsed >= 50) {
      std::string line = "[Perf] ";
      line += req->method;
      line += " worker: rows=" + std::to_string(req->tracks.get_count());
      line += " bytes=" + std::to_string(payload.size());
      line += " took=" + std::to_string(elapsed) + "ms";
      console::printf("%s", line.c_str());
    }

    responder.SendRaw(std::move(payload));
  } catch (const std::exception& e) {
    responder.SendJson(MakeQueryWireErrorBody(req->api, e.what()));
  } catch (...) {
    responder.SendJson(MakeQueryWireErrorBody(req->api, "Search failed"));
  }
}

// worker 段主体。顶层 try/catch 把一切异常折成该 API 现状形状的正常响应体。
void RunQueryWireWorker(const std::shared_ptr<QueryWireRequest>& req,
                        const DeferredResponder& responder) {
  try {
    req->workerStart = std::chrono::steady_clock::now();

    // 1. 排序（仅 query 且脚本编译成功）。用 SDK 自己的 sort_by_format_get_order
    //    而不是自算排序键 + std::stable_sort：SDK 的排序键要过 tfhook_sort 与
    //    fb2k::makeSortString、比较走 fb2k::sortStringCompare 且以原下标兜平局
    //    （metadb_handle_list.cpp:348-487），自算键无法复现同一顺序，而顺序是
    //    可观测契约。取 get_order 而非就地 reorder，是为了把同一置换套到捕获的
    //    标量上；该函数内部即 queryMultiParallelEx_ + formatTitle_v2，与本管线
    //    其余调用同属已验证可在 worker 跑的那一组。
    if (req->sortScript.is_valid() && req->tracks.get_count() > 1) {
      const size_t count = req->tracks.get_count();
      pfc::array_t<t_size> order;
      order.set_size(count);
      req->tracks.sort_by_format_get_order(order.get_ptr(), req->sortScript, nullptr);

      const size_t keep = std::min(req->limit, count);
      metadb_handle_list ordered;
      std::vector<QueryTrackCapture> orderedCaps;
      orderedCaps.reserve(keep);
      for (size_t i = 0; i < keep; ++i) {
        const t_size src = order[i];  // reorder 的语义：新 [i] = 旧 [order[i]]
        ordered.add_item(req->tracks[src]);
        orderedCaps.push_back(std::move(req->caps[src]));
      }
      req->tracks = std::move(ordered);
      req->caps = std::move(orderedCaps);
    }

    const size_t count = req->tracks.get_count();

    // 2. 批量取 info 记录。回调会被多线程并发调用，故预分配 count 槽、按 idx 写
    //    对应槽位，全程不扩容、不碰共享可变态。
    req->recs.assign(count, metadb_v2::rec_t{});
    auto mdb = metadb_v2::get();
    if (count > 0) {
      mdb->queryMultiParallel_(req->tracks, [&req](size_t idx, const metadb_v2::rec_t& rec) {
        if (idx < req->recs.size()) {
          req->recs[idx] = rec;
        }
      });
    }

    // 3. 逐曲定下最终生效的 info 容器。rec 里的 info 可能为空（该曲信息未知），
    //    此时退回 get_info_ref()：SDK 明文该简化版永不返回空、可在任意上下文调用
    //    且无锁语义（metadb_handle.h:126-127 与 108-110），同步版走的就是它，
    //    形状因此不变。
    req->infos.assign(count, metadb_info_container::ptr());
    for (size_t i = 0; i < count; ++i) {
      if (!req->caps[i].valid) continue;
      req->infos[i] = req->recs[i].info.is_valid() ? req->recs[i].info
                                                   : req->tracks[i]->get_info_ref();
    }

    // 4. rating。只在被请求（或省略 fields）时才算：未请求时整轮 formatTitle_v2
    //    连同它的 provider 链求值全部免掉，ratings 保持全零且无人读取 —— 这是
    //    F4「无论调用方要不要都算一次 %rating%」的按需化。ratingScript 也只在该
    //    情形下编译（见两个 handler 的主线程段），此处的门控与它同一个判据。
    //    任一曲抛异常 = 该请求整批改走主线程兜底（第三方 rating provider 违规
    //    假设主线程的防线），不让单曲异常打断整个请求。
    req->ratings.assign(count, 0);
    if (req->fieldMask & TrackField::kRating) {
      try {
        for (size_t i = 0; i < count; ++i) {
          if (!req->infos[i].is_valid()) continue;
          req->ratings[i] = ComputeTrackRatingFromRec(mdb, req->tracks[i], req->recs[i],
                                                      req->infos[i]->info(), req->ratingScript);
        }
      } catch (...) {
        console::printf("[Library] worker rating failed, falling back to main thread batch");
        fb2k::inMainThread([req, responder]() {
          try {
            const size_t n = req->tracks.get_count();
            for (size_t i = 0; i < n; ++i) {
              if (!req->infos[i].is_valid()) continue;
              req->ratings[i] = ComputeTrackRating(req->tracks[i], req->infos[i]->info());
            }
            fb2k::inCpuWorkerThread([req, responder]() {
              FinishQueryWireOnWorker(req, responder);
            });
          } catch (const std::exception& e) {
            responder.SendJson(MakeQueryWireErrorBody(req->api, e.what()));
          } catch (...) {
            responder.SendJson(MakeQueryWireErrorBody(req->api, "Search failed"));
          }
        });
        return;  // 回包交给续段，本段到此为止
      }
    }

    // 5. 序列化 + 回包
    FinishQueryWireOnWorker(req, responder);
  } catch (const std::exception& e) {
    responder.SendJson(MakeQueryWireErrorBody(req->api, e.what()));
  } catch (...) {
    responder.SendJson(MakeQueryWireErrorBody(req->api, "Search failed"));
  }
}


// ========== Search ==========
//
// 过滤走全库 get_all_items + search_filter_v2::test_multi 并按库序收集，**不是**
// search_index：索引查询虽可在任意线程执行，但返回内部索引序，与本 API 现契约的
// 库序不等价，换过去即改变 tracks 顺序。元数据读取与序列化在 CPU worker 上做，
// 见 LibrarySearchDeferred；本同步版保留为形状基准，已不在注册面上。

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
            {"tracks", tracks},
            {"total", totalMatched},
            {"offset", offset},
            {"limit", limit},
            {"hasMore", offset + matchedItems.get_count() < totalMatched}};
  } catch (const std::exception &e) {
    return {{"success", false},
            {"error", e.what()},
            {"tracks", json::array()},
            {"total", 0}};
  } catch (...) {
    return {{"success", false},
            {"error", "Search failed"},
            {"tracks", json::array()},
            {"total", 0}};
  }
}


// library.search 的延迟响应版：主线程只做过滤与标量捕获，元数据读取与序列化下
// CPU worker。响应形状、错误形状、分页语义与 tracks 顺序与上面的同步版逐项一致。
void LibrarySearchDeferred(const json& params, const DeferredResponder& responder) {
  std::string query = params.value("query", "");
  size_t offset = params.value("offset", static_cast<size_t>(0));
  size_t limit = params.value("limit", static_cast<size_t>(100));

  // fields 校验排在一切之前：形状错的请求不该先跑一遍全库扫描再被拒，也不该在
  // 空 query 这类分支上"成功"返回 —— fail-closed。
  const TrackFieldSelection fields = ParseTrackFieldSelection(params);
  if (!fields.valid) {
    responder.SendJson(MakeTrackFieldsErrorBody(fields));
    return;
  }

  // 空 query 与库未启用都是同步版的"成功空集"形状（无 hasMore 键）
  if (query.empty()) {
    responder.SendJson({{"success", true},
                        {"tracks", json::array()},
                        {"total", 0},
                        {"offset", offset},
                        {"limit", limit}});
    return;
  }

  auto lib = library_manager::get();
  if (!lib->is_library_enabled()) {
    responder.SendJson({{"success", true},
                        {"tracks", json::array()},
                        {"total", 0},
                        {"offset", offset},
                        {"limit", limit}});
    return;
  }

  try {
    // filter 的 flags 与同步版一致：search 带 AllowSort，query 不带 —— 同一条
    // 带 SORT BY 的串在两个 API 下成败不同，flags 属可观测面，不得对齐。
    search_filter_v2::ptr filter = search_filter_manager_v2::get()->create_ex(
        query.c_str(), fb2k::service_new<completion_notify_dummy>(),
        search_filter_manager_v2::KFlagSuppressNotify |
            search_filter_manager_v2::KFlagAllowSort);

    metadb_handle_list allItems;
    lib->get_all_items(allItems);

    pfc::array_t<bool> mask;
    mask.set_size(allItems.get_count());
    filter->test_multi(allItems, mask.get_ptr());

    // 计全数、只收 [offset, offset+limit) 这一页，与同步版同一个循环形状
    auto req = std::make_shared<QueryWireRequest>();
    size_t totalMatched = 0;
    for (size_t i = 0; i < allItems.get_count(); i++) {
      if (mask[i]) {
        if (totalMatched >= offset && req->tracks.get_count() < limit) {
          req->tracks.add_item(allItems[i]);
        }
        totalMatched++;
      }
    }

    req->api = QueryWireApi::Search;
    req->method = "library.search";
    req->limit = limit;
    req->offset = offset;
    req->baseIndex = offset;  // 同步版传给 GetLibraryTrackInfo 的是 offset + i
    req->total = totalMatched;
    req->fieldMask = fields.mask;
    req->projected = fields.projected;

    // 空页（零命中 / offset 越界 / limit=0）就地回包，不为零行付线程跳变的税。
    // 信封与 tracks 段（"[]"）都与字段集无关，投影不改这一支。
    if (SendEmptyQueryWireResultIfNoRows(req, responder)) {
      return;
    }

    // rating 未被请求就连脚本都不编译（spec §3.1 语义 3 的按需化）
    if (req->fieldMask & TrackField::kRating) {
      static_api_ptr_t<titleformat_compiler>()->compile_safe(req->ratingScript, "%rating%");
    }

    req->caps.reserve(req->tracks.get_count());
    for (size_t i = 0; i < req->tracks.get_count(); i++) {
      req->caps.push_back(CaptureQueryTrack(req->tracks[i], req->fieldMask));
    }

    fb2k::inCpuWorkerThread([req, responder]() { RunQueryWireWorker(req, responder); });
  } catch (const std::exception& e) {
    responder.SendJson(MakeQueryWireErrorBody(QueryWireApi::Search, e.what()));
  } catch (...) {
    responder.SendJson(MakeQueryWireErrorBody(QueryWireApi::Search, "Search failed"));
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

      // 按值遍历：多值 artist 的每个值各成一个条目，曲目计进每一位参与者
      for (const auto &artistName : MetaValues(info, "artist")) {
        auto &artist = artistMap[artistName];
        artist.name = artistName;
        artist.trackCount++;
        artist.totalDuration += info.get_length();

        const char *album = info.meta_get("album", 0);
        if (album && strlen(album) > 0) {
          artist.albums.insert(album);
        }
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
// 注意: 此函数全仓零注册，是保留的死代码 —— library.getGenres 实际注册的是
// 下方的 LibraryGetGenres_2（带 trackCount 的版本）。删除需先征求用户同意。
// 返回 {success, items[{name}], count}
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

    // 按值遍历：多值 genre 的每个值各成一个条目，曲目计进每一个值
    for (const auto &genre : MetaValues(info, "genre")) {
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


// library.query 的延迟响应版：主线程只做过滤与标量捕获，排序、元数据读取与序列化
// 下 CPU worker。响应形状、错误形状、total 口径、排序语义与截断次序与上面的同步版
// 逐项一致（排序仍发生在截 limit 之前，total 是截断前的全命中数）。
void LibraryQueryDeferred(const json& params, const DeferredResponder& responder) {
  std::string query = params.value("query", "");
  size_t limit = params.value("limit", static_cast<size_t>(100));
  std::string sortBy = params.value("sort", "");

  // fields 校验排在一切之前：形状错的请求不该先跑一遍全库扫描再被拒 —— fail-closed
  const TrackFieldSelection fields = ParseTrackFieldSelection(params);
  if (!fields.valid) {
    responder.SendJson(MakeTrackFieldsErrorBody(fields));
    return;
  }

  if (query.empty()) {
    responder.SendJson({{"success", false}, {"error", "query is required"}});
    return;
  }

  auto lib = library_manager::get();
  if (!lib->is_library_enabled()) {
    responder.SendJson({{"success", false}, {"error", "Library not enabled"}});
    return;
  }

  try {
    // flags 与同步版一致：query 只带 SuppressNotify，不带 AllowSort —— 带 SORT BY
    // 的串在此会被 create_ex 拒掉，落到下面的 catch 出 "Invalid query syntax"。
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

    auto req = std::make_shared<QueryWireRequest>();
    req->api = QueryWireApi::Query;
    req->method = "library.query";
    req->limit = limit;
    req->baseIndex = 0;
    req->total = results.get_count();
    req->fieldMask = fields.mask;
    req->projected = fields.projected;

    // 脚本一律在主线程编译（titleformat 编译的主线程口径见 titleformat.h:245-253
    // 的 titleformat_object_cache 断言），worker 只负责求值。compile 失败 = 忽略
    // 排序继续，与同步版一致。sort 与 fields 互不相干：排序键是调用方给的
    // titleformat 串，不受投影字段集影响（排序仍在截 limit 之前）。
    if (!sortBy.empty()) {
      static_api_ptr_t<titleformat_compiler> compiler;
      titleformat_object::ptr script;
      if (compiler->compile(script, sortBy.c_str())) {
        req->sortScript = script;
      }
    }
    // rating 未被请求就连脚本都不编译（spec §3.1 语义 3 的按需化）
    if (req->fieldMask & TrackField::kRating) {
      static_api_ptr_t<titleformat_compiler>()->compile_safe(req->ratingScript, "%rating%");
    }

    // 不排序时只有前 limit 条会被序列化，标量就只捕获这一段（省下为丢弃的命中付
    // 主线程开销）；要排序时留哪几条得等 worker 排完，只能整批捕获。
    const size_t captureCount = req->sortScript.is_valid()
                                    ? results.get_count()
                                    : std::min(limit, results.get_count());
    if (captureCount == results.get_count()) {
      req->tracks = std::move(results);
    } else {
      for (size_t i = 0; i < captureCount; i++) {
        req->tracks.add_item(results[i]);
      }
    }

    // 零行（零命中 / limit=0）就地回包，不为零行付线程跳变的税。判据要等 tracks
    // 填完才成立，故排在脚本编译之后 —— 零行时那次 %rating% 编译是微秒级冗余，
    // 不值得为它把语句顺序重排（sortScript 又是 captureCount 的输入，动不了）。
    // 信封与 tracks 段（"[]"）都与字段集无关，投影不改这一支。
    if (SendEmptyQueryWireResultIfNoRows(req, responder)) {
      return;
    }

    req->caps.reserve(req->tracks.get_count());
    for (size_t i = 0; i < req->tracks.get_count(); i++) {
      req->caps.push_back(CaptureQueryTrack(req->tracks[i], req->fieldMask));
    }

    fb2k::inCpuWorkerThread([req, responder]() { RunQueryWireWorker(req, responder); });
  } catch (...) {
    responder.SendJson(MakeQueryWireErrorBody(QueryWireApi::Query, nullptr));
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

  // 与 DOM 版同口径：artist / artists 同源于一次字段查找。本 API 是扁平对象，
  // 不返回 albumArtist / composer，故只多这一个数组键。
  const std::vector<std::string> artistValues = MetaValuesRaw(info, "artist");

  return {{"success", true},
          {"found", true},
          {"path", handle->get_path()},
          {"absolutePath", std::string(nativePath.get_ptr())},
          {"title", getMeta("title")},
          {"artist", JoinMetaValues(artistValues, ", ")},
          {"artists", artistValues},
          {"album", getMeta("album")},
          {"duration", info.get_length()},
          {"trackNumber", getMeta("tracknumber")},
          {"genre", MetaJoined(info, "genre")},
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
    // query / search 走 deferred：主线程只出过滤结果，序列化在 CPU worker 上做。
    // 注册名不变，两条路径不得同名并存 —— 同步表优先，留着同步注册等于改动作废。
    bridge.RegisterApiDeferred("library.search", LibrarySearchDeferred);
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
    bridge.RegisterApiDeferred("library.query", LibraryQueryDeferred);
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
