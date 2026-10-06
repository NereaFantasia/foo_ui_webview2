#include "pch.h"
#include "api/LibraryApi.h"
#include "api/AlbumIdentity.h"
#include "api/BridgeCore.h"
#include "api/CallerContext.h"
#include "api/ErrorEnvelope.h"
#include "api/EventEmit.h"
#include "api/MetaAccess.h"
#include "api/PlaylistApi.h"
#include "api/PlaylistLock.h"
#include "api/PlaylistTarget.h"
#include "api/TrackRow.h"
#include "api/TrackWireSnapshot.h"
#include "api/TypedApi.h"
#include "api/generated/LibrarySchema.h"
#include "domain/library/LibraryCache.h"
#include "domain/library/LibraryTreeIndex.h"
#include "core/WebViewContext.h"
#include <foobar2000/SDK/album_art.h>
#include <foobar2000/SDK/album_art_helpers.h>
#include <algorithm>
#include <atomic>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <random>
#include <tuple>
#include "utils/Base64.h"
#include "utils/JsonWriter.h"
#include "utils/StringUtils.h"
#include "utils/SubsongUtils.h"


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

    // path 是专辑首曲的 get_path()，不带子曲目后缀；上游聚合时已不分子曲目，
    // 按第一首取封面。
    metadb_handle_ptr track = SubsongUtils::CreateCanonicalHandle(path, 0);

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

// 一张专辑在归组结果里的一项：整张专辑的折叠（不带曲目列表）与它的曲目，
// 曲目按媒体库顺序。
struct AlbumEntry {
  AlbumData data;
  std::vector<metadb_handle_ptr> tracks;
};

// 键是 AlbumKey。用有序映射：getAlbums 排序前按键序遍历，排序的输入顺序不随
// 哈希变化。
using AlbumIndex = std::map<std::string, AlbumEntry>;

// 一首曲目的专辑身份，口径见 AlbumIdentity.h。album artist 在时不读 artist。
// 返回的指针指向 info 内部，info 存活期间有效。
static std::optional<AlbumIdentity> AlbumIdentityOf(const file_info &info) {
  const char *album = info.meta_get("album", 0);
  const char *albumArtist = info.meta_get("album artist", 0);
  const char *artist = albumArtist ? nullptr : info.meta_get("artist", 0);
  return ResolveAlbumIdentity(album, albumArtist, artist);
}

// ============================================
// API Registration
// ============================================


// ==========================================================================
// Library API handler functions
// ==========================================================================
namespace {

// Not `lib`: the handlers name their library_manager pointer that.
namespace lb = api::library;

// Declared counts are non-negative 64-bit; on a 32-bit build a count beyond
// size_t means "no limit" rather than wrapping.
size_t ToSize(std::int64_t value) {
  if (value <= 0) return 0;
  if (static_cast<std::uint64_t>(value) > std::numeric_limits<size_t>::max())
    return std::numeric_limits<size_t>::max();
  return static_cast<size_t>(value);
}

// Declared results kept between calls. They live here rather than in
// domain/library/LibraryCache so that domain does not depend on the generated types;
// LibraryCache stays the one place that knows when the library changed, and
// once its generation moves everything kept here is dropped. The handlers and
// the library callbacks that invalidate all run on the main thread.
struct KeptResults {
  uint64_t generation = 0;
  // library.getAlbums complete lists, by query, sort and includeCover.
  std::map<std::tuple<std::string, std::string, bool>, std::vector<lb::AlbumInfo>> albums;
  // Every album of the library with its tracks, read by library.getAlbums and
  // library.getAlbumTracks. Shared so that a handler keeps its copy alive even
  // if the generation moves while it runs.
  std::shared_ptr<const AlbumIndex> albumIndex;
  // library.getArtists scan, unsorted and with every row's albums filled.
  std::optional<std::vector<lb::ArtistInfo>> artists;
  std::optional<lb::GetStatusResult> status;
  // library.getAll full list and the library size it was read from; rows of
  // invalid handles are left out, so the two can differ. Shared so that the
  // async path can hand over the list its worker built without a copy.
  std::shared_ptr<const std::vector<lb::LibraryTrack>> allTracks;
  std::int64_t allTracksTotal = 0;
};

KeptResults &Kept() {
  static KeptResults kept;
  const uint64_t generation = g_LibraryCache.GetGeneration();
  if (kept.generation != generation) {
    kept = KeptResults{};
    kept.generation = generation;
  }
  return kept;
}

void KeepAllTracks(std::shared_ptr<const std::vector<lb::LibraryTrack>> rows, std::int64_t total) {
  KeptResults &kept = Kept();
  kept.allTracks = std::move(rows);
  kept.allTracksTotal = total;
  g_LibraryCache.NoteKept();
}

// A library list row. `track` must be valid; what `index` counts is up to the
// list, as its declaration says.
lb::LibraryTrack LibraryTrackRow(const metadb_handle_ptr &track, size_t index) {
  lb::LibraryTrack row;
  static_cast<api::common::Track &>(row) = BuildTrackRow(track);
  row.index = static_cast<std::int64_t>(index);
  return row;
}

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


// ========== Library Status ==========

api::Result<lb::IsEnabledResult> LibraryIsEnabled(const lb::IsEnabledParams& /*params*/) {
  lb::IsEnabledResult result;
  result.enabled = library_manager::get()->is_library_enabled();
  return result;
}


api::Result<lb::GetStatsResult> LibraryGetStats(const lb::GetStatsParams& /*params*/) {
  auto lib = library_manager::get();

  lb::GetStatsResult result;
  if (!lib->is_library_enabled()) {
    result.cacheValid = g_LibraryCache.IsValid();
    result.lastModified = g_LibraryCache.GetLastModified();
    return result;
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
      // 专辑按 getAlbums 的分组键计，totalAlbums 才与 getAlbums.total 一致
      if (const auto album = AlbumIdentityOf(info))
        albums.insert(AlbumKey(*album));
      // totalArtists 按参与艺术家计：多值 artist 的每个值各计一次，
      // 与 getArtists 的条目数口径一致。同字段查两次是刻意的，本函数
      // 不在送达热路径上
      for (const auto &artistValue : MetaValues(info, "artist"))
        artists.insert(artistValue);
    }
  }

  result.totalTracks = static_cast<std::int64_t>(items.get_count());
  result.totalAlbums = static_cast<std::int64_t>(albums.size());
  result.totalArtists = static_cast<std::int64_t>(artists.size());
  result.totalDuration = totalDuration;
  result.totalSize = static_cast<std::int64_t>(totalSize);
  result.cacheValid = g_LibraryCache.IsValid();
  result.lastModified = g_LibraryCache.GetLastModified();
  return result;
}


// ========== Cache Control ==========

api::Result<lb::InvalidateCacheResult> LibraryInvalidateCache(
    const lb::InvalidateCacheParams& /*params*/) {
  g_LibraryCache.Invalidate();
  g_LibraryTreeIndex.Invalidate();
  lb::InvalidateCacheResult result;
  result.timestamp = g_LibraryCache.GetLastModified();
  return result;
}


api::Result<lb::GetCacheStatsResult> LibraryGetCacheStats(const lb::GetCacheStatsParams& /*params*/) {
  const json cache = g_LibraryCache.GetStats();
  const json tree = g_LibraryTreeIndex.GetStats();
  const KeptResults &kept = Kept();

  lb::GetCacheStatsResult result;
  result.valid = cache.value("valid", false);
  result.lastModified = cache.value("lastModified", std::int64_t{0});
  result.albumsCacheEntries = static_cast<std::int64_t>(kept.albums.size());
  result.tracksCached = kept.allTracks != nullptr;
  result.artistsCached = kept.artists.has_value();
  result.genresCached = false;  // 流派结果不保留
  result.statsCached = kept.status.has_value();
  // 封面不保留，三个封面字段恒为 0
  result.coversCached = 0;
  result.coverCacheBytes = 0;
  result.coverCacheMB = 0.0;
  result.cacheHits = cache.value("cacheHits", std::int64_t{0});
  result.cacheMisses = cache.value("cacheMisses", std::int64_t{0});
  // 目录树索引统计字段
  result.treeIndexValid = tree.value("treeIndexValid", false);
  result.rootsCached = tree.value("rootsCached", std::int64_t{0});
  result.treeIndexedTracks = tree.value("treeIndexedTracks", std::int64_t{0});
  result.treeSkippedTracks = tree.value("treeSkippedTracks", std::int64_t{0});
  result.treeLastBuilt = tree.value("treeLastBuilt", std::int64_t{0});
  return result;
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
//             取 rec → rating（仅当被请求）→ 直写 UTF-8（省略 fields 出声明的
//             LibraryTrack 全部键，传 fields 出投影键集）→ SendRaw
//
// fields 投影是纯收窄：掩码由主线程解析一次随请求下传，捕获侧、rating、序列化侧
// 三处各自按位门控。省略 fields 时掩码为全集，三处判定恒真，与投影前逐字等价。
//
// 查询段为什么仍留主线程：search_index 的 search() 可在任意线程跑，但它的输出是
// 内部索引序，与现契约的库序不等价（实测四组查询集合全等、顺序全不等），换引擎
// 即改变 tracks 顺序这一可观测面，故只把后段（批量元数据 + 序列化）下 worker。
//
// 每曲标量的线程放置依据：
//   · handle / path / absolutePath 留主线程。get_path() 本身可跨线程（location 随
//     handle 不可变：SDK 对 metadb_handle::get_location() 注明 "valid till the object
//     is released"，metadb.h 也注明"一个位置只有一个 handle"），但 absolutePath 要过
//     filesystem::g_get_native_path —— 它对非 file:// 路径转派
//     filesystem_v3::getNativePath（实现方可以是第三方
//     插件）且 SDK 无线程标注，本仓库现有调用点全在主线程，无跨线程先例。
//     handle 由 absolutePath 拼出，取值规则与 BuildTrackRow 共用 FillTrackIdentity。
//   · fileSize 走 get_filestats()（"最近一次"文件状态，是可变态），照 library.getAll
//     async 路径的快照先例在主线程取；subsong 同批取，省一次遍历。
//   · rating、元数据、排序键全在 worker：rating 用 metadb_v2::formatTitle_v2
//     （免数据库访问），排序用 SDK 自己的 sort_by_format_get_order。
//
// 每请求恰好一次响应：主线程段的错误路径直接经 responder 回**正常**的失败信封
// （不能落成框架错误信封）；worker 段顶层 try/catch 同样回失败信封 ——
// cpuThreadPool 会静默吞掉未捕获异常，漏响应即页面侧 30s 超时假死。

// 主线程为每曲捕获的标量。输出数组内的 index 不入表：它由序列化侧的
// baseIndex + i 决定（query 从 0 起、search 从 offset 起）。
struct QueryTrackCapture {
  std::string handle;
  std::string path;
  std::string absolutePath;
  int64_t fileSize = 0;
  uint32_t subsong = 0;
  bool valid = false;  // handle 无效时为 false，序列化侧跳过这一行
};

// fieldMask 逐项门控：投影模式下未被请求的字段一次 SDK 调用都不做。absolutePath 是
// 唯一可能转派进第三方实现的（g_get_native_path 对非 file:// 路径走
// filesystem_v3::getNativePath），只要标签不要路径的查询由此完全绕开它；handle 由它
// 拼出，要 handle 也得走一遍。
QueryTrackCapture CaptureQueryTrack(const metadb_handle_ptr& track, uint32_t fieldMask) {
  QueryTrackCapture cap;
  if (!track.is_valid()) {
    return cap;
  }
  if (fieldMask & (TrackField::kHandle | TrackField::kAbsolutePath)) {
    api::common::Track identity;
    FillTrackIdentity(identity, track);
    cap.handle = std::move(identity.handle);
    cap.path = std::move(identity.path);
    cap.absolutePath = std::move(identity.absolutePath);
    cap.fileSize = identity.fileSize;
    cap.subsong = static_cast<uint32_t>(identity.subsong);
  } else {
    if (fieldMask & TrackField::kPath) {
      cap.path = track->get_path();
    }
    if (fieldMask & TrackField::kFileSize) {
      cap.fileSize = static_cast<int64_t>(track->get_filesize());
    }
    if (fieldMask & TrackField::kSubsong) {
      cap.subsong = track->get_subsong_index();
    }
  }
  cap.valid = true;
  return cap;
}

// 两个 API 的信封与错误体形状不同，不得混写。
enum class QueryWireApi { Query, Search };

// 查询过滤建起来之后的失败（取曲目、序列化等）。search 的失败一直带空的 tracks 与
// total，照旧带出。
json MakeQueryWireErrorBody(QueryWireApi api, const char* message) {
  if (api == QueryWireApi::Query) {
    return api::results::FailureToJson(
        api::Fail(message ? message : "Query failed", ApiErrorCode::OPERATION_FAILED));
  }
  return api::results::FailureToJson(api::Fail(message ? message : "Search failed",
                                               ApiErrorCode::OPERATION_FAILED,
                                               {{"tracks", json::array()}, {"total", 0}}));
}

// create_ex 拒绝查询串（语法错，或 query 带了 SORT BY）。details.param 与 playlist.getMatchingRows
// 同形，页面凭它把查询写错与别的 INVALID_PARAMS 分开；报错文字是宿主语言的原文，不能拿来判断。
json MakeQuerySyntaxErrorBody(QueryWireApi api, const char* message) {
  const json details{{"param", "query"}};
  if (api == QueryWireApi::Query) {
    return api::results::FailureToJson(
        api::Fail("Invalid query syntax", ApiErrorCode::INVALID_PARAMS, {{"details", details}}));
  }
  return api::results::FailureToJson(
      api::Fail(message && *message ? message : "Invalid query syntax",
                ApiErrorCode::INVALID_PARAMS,
                {{"tracks", json::array()}, {"total", 0}, {"details", details}}));
}

// fields 校验失败的回包（MakeTrackFieldsErrorBody）在 TrackWireSnapshot.h：playlist.getTracks
// 要用同一份失败，匿名 namespace 的内部链接够不着它。

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

// rating 的 worker 版：候选值与取舍同主线程的 ResolveTrackRating（RatingResolve.h），
// 只把 %rating% 的求值通道从 format_title 换成 formatTitle_v2（用预取的 rec，免数据库
// 访问）。不在此吞异常 —— 抛出即触发调用方的整批主线程兜底。
int ResolveTrackRatingFromRec(const metadb_v2::ptr& mdb, const metadb_handle_ptr& track,
                              const metadb_v2::rec_t& rec, const file_info& info,
                              const titleformat_object::ptr& script) {
  pfc::string8 result;
  mdb->formatTitle_v2(track, rec, nullptr, result, script, nullptr);
  return SelectRating(StatsRatingFromText(result), TagRatingOf(info)).value;
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
  bool first = true;
  for (size_t i = 0; i < count; ++i) {
    // handle 无效的行没有可写的身份，整行跳过；index 仍按它在命中里的位置算。
    if (!req.caps[i].valid) continue;
    if (!first) out.push_back(',');
    first = false;

    // 以下逐字段的 mask 判定在全字段路径上恒真（mask == kAll）；投影时未被请求的
    // 字段连取值都不做，省掉的是每行的 meta_get / info_get 与随后的 SafeUtf8 拷贝，
    // 不只是几个字节的输出。取自句柄的字段不论有没有 info 都照写。
    snap.index = req.baseIndex + i;
    if (mask & TrackField::kHandle) {
      snap.handle = req.caps[i].handle;
    }
    if (mask & TrackField::kPath) {
      snap.path = StringUtils::SafeUtf8(req.caps[i].path);
    }
    if (mask & TrackField::kAbsolutePath) {
      snap.absolutePath = req.caps[i].absolutePath;
    }
    if (mask & TrackField::kFileSize) snap.fileSize = req.caps[i].fileSize;
    if (mask & TrackField::kSubsong) snap.subsong = req.caps[i].subsong;

    const metadb_info_container::ptr& infoHolder = req.infos[i];
    if (!infoHolder.is_valid()) {
      // info 容器无效：取自 info 的字段归零，行仍出全部请求键。duration 取 0.0 ——
      // BuildTrackRow 这一支写的是 track->get_length()，而 get_length() 内部就是
      // get_info_ref()->info().get_length()，容器为空时它自己就会解空指针，故该
      // 取值不可复现；ratings[i] 恒为 0（rating 循环跳过无 info 的曲目）。
      ResetInfoFields(snap);
      snap.rating = req.ratings[i];
      if (req.projected) {
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
    // albumArtists 与 albumArtist 的关系同上面的 artists 与 artist。
    if (mask & TrackField::kAlbumArtists) {
      snap.albumArtists = MetaValuesRaw(info, "album artist");
      if (mask & TrackField::kAlbumArtist) snap.albumArtist = JoinMetaValues(snap.albumArtists, ", ");
    } else if (mask & TrackField::kAlbumArtist) {
      snap.albumArtist = MetaJoined(info, "album artist");
    }
    if (mask & TrackField::kGenre) snap.genre = MetaJoined(info, "genre");
    if (mask & TrackField::kDate) snap.date = getMeta("date");
    if (mask & TrackField::kTrackNumber) snap.trackNumber = getMetaInt("tracknumber");
    if (mask & TrackField::kDiscNumber) snap.discNumber = getMetaInt("discnumber");
    if (mask & TrackField::kDuration) snap.duration = info.get_length();
    if (mask & TrackField::kBitrate) snap.bitrate = static_cast<int>(info.info_get_bitrate());
    if (mask & TrackField::kSampleRate) {
      snap.sampleRate = static_cast<int>(info.info_get_int("samplerate"));
    }
    if (mask & TrackField::kChannels) {
      snap.channels = static_cast<int>(info.info_get_int("channels"));
    }
    if (mask & TrackField::kCodec) snap.codec = StringUtils::SafeUtf8(info.info_get("codec"));
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
    //    （见 SDK 的 sort_by_format_get_order_v3 实现），自算键无法复现同一顺序，而顺序是
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
    //    且无锁语义（见 SDK 对 get_info_ref 两个重载的注释），同步版走的就是它，
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
          req->ratings[i] = ResolveTrackRatingFromRec(mdb, req->tracks[i], req->recs[i],
                                                      req->infos[i]->info(), req->ratingScript);
        }
      } catch (...) {
        console::printf("[Library] worker rating failed, falling back to main thread batch");
        fb2k::inMainThread([req, responder]() {
          try {
            const size_t n = req->tracks.get_count();
            for (size_t i = 0; i < n; ++i) {
              if (!req->infos[i].is_valid()) continue;
              req->ratings[i] = ResolveTrackRating(req->tracks[i], &req->infos[i]->info()).value;
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
// 见下面的 LibrarySearchDeferred。


// library.search 的延迟响应版：主线程只做过滤与标量捕获，元数据读取与序列化下
// CPU worker。成功回包由直写器写出（形状见 src/api/schema/library.ts 的 SearchResult，
// 行的键集由单测对着生成的 kFields 核对），失败回包走声明的失败信封。
void LibrarySearchDeferred(const lb::SearchParams& p, const DeferredResponder& responder) {
  const std::string &query = p.query;
  const size_t offset = ToSize(p.offset);
  const size_t limit = ToSize(p.limit);

  // fields 校验排在一切之前：名字错的请求不该先跑一遍全库扫描再被拒，也不该在
  // 空 query 这类分支上"成功"返回 —— fail-closed。
  const TrackFieldSelection fields = ParseTrackFieldSelection(p.fields);
  if (!fields.valid) {
    responder.SendJson(MakeTrackFieldsErrorBody(fields));
    return;
  }

  // 空 query 与库未启用都回空页
  auto lib = library_manager::get();
  if (query.empty() || !lib->is_library_enabled()) {
    lb::SearchResult empty;
    empty.offset = p.offset;
    empty.limit = p.limit;
    api::Send(responder, api::Result<lb::SearchResult>(std::move(empty)));
    return;
  }

  // filter 的 flags：search 带 AllowSort，query 不带 —— 同一条带 SORT BY 的串在两个
  // API 下成败不同，flags 属可观测面，不得对齐。
  search_filter_v2::ptr filter;
  try {
    filter = search_filter_manager_v2::get()->create_ex(
        query.c_str(), fb2k::service_new<completion_notify_dummy>(),
        search_filter_manager_v2::KFlagSuppressNotify |
            search_filter_manager_v2::KFlagAllowSort);
  } catch (const std::exception& e) {
    responder.SendJson(MakeQuerySyntaxErrorBody(QueryWireApi::Search, e.what()));
    return;
  } catch (...) {
    responder.SendJson(MakeQuerySyntaxErrorBody(QueryWireApi::Search, nullptr));
    return;
  }

  try {
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
    req->baseIndex = offset;
    req->total = totalMatched;
    req->fieldMask = fields.mask;
    req->projected = fields.projected;

    // 空页（零命中 / offset 越界 / limit=0）就地回包，不为零行付线程跳变的税。
    // 信封与 tracks 段（"[]"）都与字段集无关，投影不改这一支。
    if (SendEmptyQueryWireResultIfNoRows(req, responder)) {
      return;
    }

    // rating 未被请求就连脚本都不编译
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

// 下面三个单元由 library.getAlbums 与 library.getArtistAlbums 共用。分组键归
// 各自的调用点（前者按专辑名加 album artist 的复合键，后者按专辑名），逐轨折叠、
// 行序列化与排序谓词三件事共用同一份实现，两个端点的行形状因此不会各自漂移。

// 把一条曲目折叠进它所属的专辑聚合。albumName 与 albumArtistResolved 由调用点
// 传入：调用点构造分组键时已经取过这两个值，传进来才不会让字段查找翻倍。
// 首轨写入的字段保持"先到先得"，本函数不做任何规范化。
static void FoldTrackIntoAlbum(const file_info &info,
                               const metadb_handle_ptr &item,
                               const char *albumName,
                               const char *albumArtistResolved,
                               bool includeTracks, AlbumData &album) {
  album.name = albumName;
  album.trackCount++;
  album.duration += info.get_length();

  // First track path (for cover art)
  if (album.firstTrackPath.empty()) {
    album.firstTrackPath = item->get_path();
  }

  // Album artist
  if (album.albumArtist.empty() && albumArtistResolved) {
    album.albumArtist = albumArtistResolved;
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
    album.tracks.push_back({trackNum ? atoi(trackNum) : 0, item->get_path()});
  }
}

// 专辑行的 JSON 形态。coverDataUrl 不在此处出键：只有 getAlbums 需要它，且抽取
// 成本高，由调用点在 firstTrackPath 非空时自行叠加。
// g_get_native_path 刻意留在本函数（即每个输出行一次），不要挪进逐轨折叠 ——
// 那会从"每页至多 limit 次"变成"每张专辑一次"。
static lb::AlbumInfo BuildAlbumRow(const AlbumData &data, bool includeTracks) {
  lb::AlbumInfo row;
  row.name = data.name;
  row.artist = data.albumArtist.empty() ? data.artist : data.albumArtist;
  row.albumArtist = data.albumArtist;
  row.trackCount = static_cast<std::int64_t>(data.trackCount);
  row.discCount = static_cast<std::int64_t>(data.discs.empty() ? 1 : data.discs.size());
  row.duration = data.duration;
  row.year = data.year;
  row.genre = data.genre;
  row.label = data.label;
  row.firstTrackPath = data.firstTrackPath; // For cover art retrieval

  // Add absolute path for firstTrackPath
  if (!data.firstTrackPath.empty()) {
    pfc::string8 nativePath;
    filesystem::g_get_native_path(data.firstTrackPath.c_str(), nativePath);
    row.firstTrackAbsolutePath = std::string(nativePath.get_ptr());
  }

  if (includeTracks) {
    std::vector<lb::AlbumTrackRef> trackList;
    auto sortedTracks = data.tracks;
    std::sort(sortedTracks.begin(), sortedTracks.end());
    trackList.reserve(sortedTracks.size());
    for (const auto &[num, path] : sortedTracks) {
      pfc::string8 trackNativePath;
      filesystem::g_get_native_path(path.c_str(), trackNativePath);
      lb::AlbumTrackRef ref;
      ref.trackNumber = static_cast<std::int64_t>(num);
      ref.path = path;
      ref.absolutePath = std::string(trackNativePath.get_ptr());
      trackList.push_back(std::move(ref));
    }
    row.tracks = std::move(trackList);
  }

  return row;
}

// library.getAlbums still answers with json; it writes the same row through
// the declared writer so the two endpoints cannot drift apart.
static json BuildAlbumRowJson(const AlbumData &data, bool includeTracks) {
  return api::results::Value(BuildAlbumRow(data, includeTracks));
}

// 排序谓词。两个端点共用，故写成命名函数，两处调用点各留一行转发 lambda。
// 未知 sortBy 落到最后一支，与按名排序等价。
static bool AlbumLess(const AlbumData &a, const AlbumData &b,
                      const std::string &sortBy) {
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
}

// 读一遍媒体库，把每首属于专辑的曲目折进它的专辑，并记下句柄。折叠不带曲目
// 列表：includeTracks 的行由 FoldWithTrackList 按句柄现折。
AlbumIndex ScanAlbums() {
  metadb_handle_list items;
  library_manager::get()->get_all_items(items);

  AlbumIndex index;
  for (size_t i = 0; i < items.get_count(); i++) {
    const metadb_handle_ptr &item = items[i];
    if (!item.is_valid())
      continue;

    metadb_info_container::ptr infoContainer = item->get_info_ref();
    if (!infoContainer.is_valid())
      continue;
    const file_info &info = infoContainer->info();

    const std::optional<AlbumIdentity> identity = AlbumIdentityOf(info);
    if (!identity)
      continue;

    AlbumEntry &entry = index[AlbumKey(*identity)];
    FoldTrackIntoAlbum(info, item, identity->name, identity->albumArtist, false, entry.data);
    entry.tracks.push_back(item);
  }
  return index;
}

// 保留的专辑归组。reuse 为假（getAlbums 的 useCache: false）或还没有保留时，
// 读一遍媒体库重建并保留。reused 非空时写入这次是否用上了保留的那份。
std::shared_ptr<const AlbumIndex> KeptAlbumIndex(bool reuse, bool *reused = nullptr) {
  KeptResults &kept = Kept();
  const bool hit = reuse && kept.albumIndex != nullptr;
  if (reused) *reused = hit;
  if (!hit) {
    kept.albumIndex = std::make_shared<const AlbumIndex>(ScanAlbums());
    g_LibraryCache.NoteKept();
  }
  return kept.albumIndex;
}

// 按保留的句柄把一张专辑重新折叠一遍，这次带上曲目列表。句柄与折叠顺序都与
// 扫描时相同，所以除 tracks 外与 entry.data 一致。
AlbumData FoldWithTrackList(const AlbumEntry &entry) {
  AlbumData data;
  for (const metadb_handle_ptr &track : entry.tracks) {
    metadb_info_container::ptr infoContainer = track->get_info_ref();
    if (!infoContainer.is_valid())
      continue;
    FoldTrackIntoAlbum(infoContainer->info(), track, entry.data.name.c_str(),
                       entry.data.albumArtist.c_str(), true, data);
  }
  return data;
}

api::Result<lb::GetAlbumsResult> LibraryGetAlbums(const lb::GetAlbumsParams& p) {
  const std::string &sortBy = p.sort;
  const std::string &filterQuery = p.query;
  const size_t offset = ToSize(p.offset);
  const size_t limit = ToSize(p.limit);
  const bool includeTracks = p.includeTracks;
  const bool includeCover = p.includeCover;
  const int coverMaxSize = static_cast<int>(std::clamp<std::int64_t>(
      p.coverMaxSize, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()));

  lb::GetAlbumsResult result;
  result.offset = p.offset;
  result.limit = p.limit;
  result.includeCover = includeCover;

  if (!library_manager::get()->is_library_enabled()) {
    return result;
  }

  // Only complete lists are kept (see the end of this function), so only a
  // request from the first album without track lists can be answered from one.
  const auto keptKey = std::make_tuple(filterQuery, sortBy, includeCover);
  if (p.useCache && offset == 0 && !includeTracks) {
    const auto &keptAlbums = Kept().albums;
    const auto it = keptAlbums.find(keptKey);
    g_LibraryCache.RecordLookup(it != keptAlbums.end());
    if (it != keptAlbums.end()) {
      const std::vector<lb::AlbumInfo> &all = it->second;
      const size_t endIdx = std::min(limit, all.size());
      result.albums.assign(all.begin(), all.begin() + static_cast<std::ptrdiff_t>(endIdx));
      result.total = static_cast<std::int64_t>(all.size());
      result.hasMore = endIdx < all.size();
      result.fromCache = true;
      return result;
    }
  }

  // The grouping is kept apart from the lists: it serves any query, sort and
  // page, and library.getAlbumTracks reads the same one.
  const std::shared_ptr<const AlbumIndex> index = KeptAlbumIndex(p.useCache);

  // Filter by query if provided
  std::vector<const AlbumEntry *> filteredAlbums;
  std::string lowerQuery = filterQuery;
  std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(),
                 ::tolower);

  for (const auto &[key, entry] : *index) {
    const AlbumData &data = entry.data;
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
    filteredAlbums.push_back(&entry);
  }

  // Sort
  std::sort(filteredAlbums.begin(), filteredAlbums.end(),
            [&sortBy](const AlbumEntry *a, const AlbumEntry *b) {
              return AlbumLess(a->data, b->data, sortBy);
            });

  // Pagination
  const size_t total = filteredAlbums.size();
  const size_t endIdx = offset >= total ? total : offset + std::min(limit, total - offset);

  for (size_t i = offset; i < endIdx; i++) {
    const AlbumData &data = filteredAlbums[i]->data;
    lb::AlbumInfo row = includeTracks
                            ? BuildAlbumRow(FoldWithTrackList(*filteredAlbums[i]), true)
                            : BuildAlbumRow(data, false);
    if (includeCover && !data.firstTrackPath.empty()) {
      std::string coverDataUrl = GetCoverDataUrl(data.firstTrackPath, coverMaxSize);
      if (!coverDataUrl.empty()) {
        row.coverDataUrl = std::move(coverDataUrl);
      }
    }
    result.albums.push_back(std::move(row));
  }
  result.total = static_cast<std::int64_t>(total);
  result.hasMore = endIdx < total;

  // Keep only a complete list: a later request can then be answered by
  // cutting it to its own limit.
  if (offset == 0 && !includeTracks && endIdx == total) {
    Kept().albums[keptKey] = result.albums;
    g_LibraryCache.NoteKept();
  }

  return result;
}


// ========== Artists ==========

api::Result<lb::GetArtistsResult> LibraryGetArtists(const lb::GetArtistsParams& p) {
  auto lib = library_manager::get();
  if (!lib->is_library_enabled()) {
    return api::Fail("Library not enabled", ApiErrorCode::LIBRARY_DISABLED,
                     {{"items", json::array()}, {"count", 0}});
  }

  const std::string &sortBy = p.sort;
  const size_t limit = ToSize(p.limit);

  // 扫描一次、保留一份带 albums 的完整结果；includeAlbums 为假时在出口处去掉
  // albums，所以两种形态出自同一次扫描。limit 只截艺术家条目，不截每位的 albums。
  auto &kept = Kept().artists;
  g_LibraryCache.RecordLookup(kept.has_value());
  if (!kept) {
    metadb_handle_list items;
    lib->get_all_items(items);

    struct ArtistData {
      std::string name;
      std::set<std::string> albums;
      // 专辑身份 (专辑名, 专辑艺术家)，即 getAlbums 的分组键（AlbumIdentity.h）。
      // 同名不同艺术家的两张专辑在这里是两条，在 albums（只按名去重，供
      // albumCount）里是一条。
      std::set<std::pair<std::string, std::string>> albumKeys;
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

      // 专辑身份按曲目取一次，再折进每位署名艺术家。不属于任何专辑的曲目不计
      // 专辑，与 getAlbums 跳过它们一致。
      const std::optional<AlbumIdentity> album = AlbumIdentityOf(info);

      // 按值遍历：多值 artist 的每个值各成一个条目，曲目计进每一位参与者
      for (const auto &artistName : MetaValues(info, "artist")) {
        auto &artist = artistMap[artistName];
        artist.name = artistName;
        artist.trackCount++;
        artist.totalDuration += info.get_length();

        if (album) {
          artist.albums.emplace(album->name);
          artist.albumKeys.emplace(album->name, album->albumArtist);
        }
      }
    }

    std::vector<lb::ArtistInfo> rows;
    rows.reserve(artistMap.size());
    for (const auto &[key, data] : artistMap) {
      lb::ArtistInfo row;
      row.name = data.name;
      row.albumCount = static_cast<std::int64_t>(data.albums.size());
      row.trackCount = static_cast<std::int64_t>(data.trackCount);
      row.totalDuration = data.totalDuration;
      // std::set 按 (name, artist) 字节序遍历，albums 的顺序因此可复现
      std::vector<lb::ArtistAlbumRef> albumRows;
      albumRows.reserve(data.albumKeys.size());
      for (const auto &[albumName, albumArtistName] : data.albumKeys) {
        lb::ArtistAlbumRef ref;
        ref.name = albumName;
        ref.artist = albumArtistName;
        albumRows.push_back(std::move(ref));
      }
      row.albums = std::move(albumRows);
      rows.push_back(std::move(row));
    }
    kept = std::move(rows);
    g_LibraryCache.NoteKept();
  }

  std::vector<lb::ArtistInfo> artists = *kept;
  if (!p.includeAlbums) {
    for (auto &artist : artists) artist.albums.reset();
  }

  // Sort
  if (sortBy == "name") {
    std::sort(artists.begin(), artists.end(),
              [](const lb::ArtistInfo &a, const lb::ArtistInfo &b) { return a.name < b.name; });
  } else if (sortBy == "trackCount") {
    std::sort(artists.begin(), artists.end(),
              [](const lb::ArtistInfo &a, const lb::ArtistInfo &b) {
                return a.trackCount > b.trackCount;
              });
  } else if (sortBy == "albumCount") {
    std::sort(artists.begin(), artists.end(),
              [](const lb::ArtistInfo &a, const lb::ArtistInfo &b) {
                return a.albumCount > b.albumCount;
              });
  }

  // Limit results
  if (artists.size() > limit) {
    artists.resize(limit);
  }

  lb::GetArtistsResult result;
  result.count = static_cast<std::int64_t>(artists.size());
  result.items = std::move(artists);
  return result;
}


// ========== 精确匹配的两个共享单元 ==========
//
// 宿主的 search_filter 不能单独用来做精确匹配。实机测得两类偏差，方向相反：
//
//   过宽 —— 星号与问号是通配符（"Camelli?" 会命中 Camellia），且查询里的小写
//           字符可匹配标签任意大小写、大写字符才要求同位大写（查 "alinut" 会
//           带回 "Alinut" 的曲目）。这部分由 MatchesAtomicValue 在命中行上收回。
//
//   过窄 —— 双引号在查询串里无法表达。把引号翻倍的写法宿主不接受，含引号的
//           名字一条都命中不了，后置校验也就无从收起，只能整个跳过预筛、让
//           全库进后置校验。
//
// 两者都不能靠改转义解决：查询语法没有任何转义手段，反斜杠、脱字符、方括号、
// 单引号实测全部零命中。查询因此降级为纯性能预筛，判据以后置校验为准。

// 精确匹配的唯一判据：某个原子标签值与目标逐字节相等。刻意与 getArtists 的
// 条目口径同源（同样走 MetaValues），保证那边给出的名字回传后语义闭合。
static bool MatchesAtomicValue(const file_info& info, const char* field,
                               const std::string& wanted) {
  const std::vector<std::string> values = MetaValues(info, field);
  return std::find(values.begin(), values.end(), wanted) != values.end();
}

// 能安全嵌进查询串的值才回内容，含双引号的回空——调用方据此跳过预筛。
// field 由调用方给出，需要引号的字段名（如 "album artist"）自带引号传入。
static std::optional<std::string> BuildValueQuery(const char* field,
                                                  const char* op,
                                                  const std::string& value) {
  if (value.find('"') != std::string::npos) return std::nullopt;
  return std::string(field) + " " + op + " \"" + value + "\"";
}

// ========== Album Tracks ==========

// 按 getAlbums 的分组键查保留的归组，只为这一张专辑生成曲目行。归组还没有时
// 读一遍媒体库建立；专辑名为空时不可能命中，不为它读媒体库。
api::Result<lb::GetAlbumTracksResult> LibraryGetAlbumTracks(const lb::GetAlbumTracksParams& p) {
  lb::GetAlbumTracksResult result;
  result.album = p.album;
  result.albumArtist = p.albumArtist;

  if (p.album.empty() || !library_manager::get()->is_library_enabled()) {
    return result;
  }

  try {
    bool reused = false;
    const std::shared_ptr<const AlbumIndex> index = KeptAlbumIndex(true, &reused);
    g_LibraryCache.RecordLookup(reused);

    const auto it = index->find(AlbumKey(p.album, p.albumArtist));
    if (it == index->end()) {
      return result;
    }
    const AlbumEntry &entry = it->second;
    result.row = BuildAlbumRow(entry.data, false);

    result.tracks.reserve(entry.tracks.size());
    for (const metadb_handle_ptr &track : entry.tracks) {
      result.tracks.push_back(LibraryTrackRow(track, 0));
    }
    // 按行里给出的 discNumber、trackNumber 排，相等的保持媒体库顺序
    std::stable_sort(result.tracks.begin(), result.tracks.end(),
                     [](const lb::LibraryTrack &a, const lb::LibraryTrack &b) {
                       if (a.discNumber != b.discNumber) return a.discNumber < b.discNumber;
                       return a.trackNumber < b.trackNumber;
                     });
    for (size_t i = 0; i < result.tracks.size(); i++) {
      result.tracks[i].index = static_cast<std::int64_t>(i);
    }
    result.items = result.tracks;
    result.total = static_cast<std::int64_t>(result.tracks.size());
    return result;
  } catch (...) {
    return api::Fail("Failed to read album tracks", ApiErrorCode::OPERATION_FAILED,
                     {{"items", json::array()},
                      {"tracks", json::array()},
                      {"total", 0},
                      {"album", p.album},
                      {"albumArtist", p.albumArtist}});
  }
}


// ========== Artist Tracks ==========

api::Result<lb::GetArtistTracksResult> LibraryGetArtistTracks(const lb::GetArtistTracksParams& p) {
  const std::string &artistName = p.artist;
  const size_t limit = ToSize(p.limit);

  lb::GetArtistTracksResult result;
  result.artist = artistName;

  auto lib = library_manager::get();
  if (artistName.empty() || !lib->is_library_enabled()) {
    return result;
  }

  try {
    // 查询只作性能预筛，判据在下面的后置校验。
    const std::optional<std::string> query =
        BuildValueQuery("artist", "IS", artistName);

    metadb_handle_list allItems;
    lib->get_all_items(allItems);

    pfc::array_t<bool> mask;
    mask.set_size(allItems.get_count());
    if (query) {
      search_filter_v2::ptr filter = search_filter_manager_v2::get()->create_ex(
          query->c_str(), fb2k::service_new<completion_notify_dummy>(),
          search_filter_manager_v2::KFlagSuppressNotify);
      filter->test_multi(allItems, mask.get_ptr());
    } else {
      for (size_t i = 0; i < allItems.get_count(); i++) mask[i] = true;
    }

    for (size_t i = 0; i < allItems.get_count() && result.tracks.size() < limit; i++) {
      if (!mask[i]) continue;

      // 取不到 file_info 的曲目一并排除：那种情形下只能发一行 artist 为空串的
      // 降级信息，放进"精确匹配"的结果里名不副实。
      metadb_info_container::ptr infoContainer = allItems[i]->get_info_ref();
      if (!infoContainer.is_valid()) continue;
      if (!MatchesAtomicValue(infoContainer->info(), "artist", artistName))
        continue;

      result.tracks.push_back(LibraryTrackRow(allItems[i], result.tracks.size()));
    }

    result.items = result.tracks;
    result.count = static_cast<std::int64_t>(result.tracks.size());
    result.total = result.count;
    return result;
  } catch (...) {
    return api::Fail("Failed to read artist tracks", ApiErrorCode::OPERATION_FAILED,
                     {{"items", json::array()},
                      {"tracks", json::array()},
                      {"total", 0},
                      {"count", 0},
                      {"artist", artistName}});
  }
}


// ========== Random Tracks ==========

api::Result<lb::GetRandomTracksResult> LibraryGetRandomTracks(const lb::GetRandomTracksParams& p) {
  const size_t reqCount = ToSize(p.count);

  lb::GetRandomTracksResult result;

  auto lib = library_manager::get();
  if (!lib->is_library_enabled()) {
    return result;
  }

  metadb_handle_list items;
  lib->get_all_items(items);

  if (items.get_count() == 0) {
    return result;
  }

  // Generate random indices
  std::vector<size_t> indices;
  indices.reserve(items.get_count());
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

  result.tracks.reserve(count);
  for (size_t i = 0; i < count; i++) {
    const metadb_handle_ptr &track = items[indices[i]];
    if (!track.is_valid()) continue;
    result.tracks.push_back(LibraryTrackRow(track, result.tracks.size()));
  }
  result.count = static_cast<std::int64_t>(result.tracks.size());
  return result;
}


// ========== Library Operations ==========

api::Result<void> LibraryRescan(const lb::RescanParams& /*params*/) {
  library_manager::get()->rescan();
  return api::Ok();
}


api::Result<lb::AddToPlaylistResult> LibraryAddToPlaylist(const lb::AddToPlaylistParams& p) {
  auto plm = playlist_manager::get();

  size_t playlistIndex = 0;
  if (auto failure = api::ResolvePlaylistTarget(*GetPlaylistService(), p.playlist, p.playlistGuid,
                                                playlistIndex)) {
    return std::move(*failure);
  }

  // 与 playlist.addPaths 等写入端点同一口径：有锁就拒绝，不先留撤销点。
  if (plm->playlist_lock_is_present(playlistIndex)) {
    return PlaylistLocked(playlistIndex, "library.addToPlaylist");
  }

  // Resolve paths to handles. Accepts the repo-wide `path|subsong:N` spelling
  // and canonicalizes, so items land with the same identity the library /
  // playlist views already report for them.
  metadb_handle_list handles;

  for (const std::string &path : p.paths) {
    auto [filePath, subsong] = SubsongUtils::ParseSubsongPath(path);
    metadb_handle_ptr handle = SubsongUtils::CreateCanonicalHandle(filePath, subsong);
    if (handle.is_valid()) {
      handles.add_item(handle);
    }
  }

  if (handles.get_count() == 0) {
    return api::Fail("No valid tracks", ApiErrorCode::OPERATION_FAILED);
  }

  // Undo backup before modification
  plm->playlist_undo_backup(playlistIndex);

  // 整批插入或整批拒绝，拒绝时返回 SIZE_MAX；插进去时条数就是 handles 的条数。
  if (plm->playlist_insert_items(playlistIndex, pfc::infinite_size, handles,
                                 pfc::bit_array_false()) == SIZE_MAX) {
    if (plm->playlist_lock_is_present(playlistIndex)) {
      return PlaylistLocked(playlistIndex, "library.addToPlaylist");
    }
    return api::Fail("Failed to add tracks to the playlist", ApiErrorCode::OPERATION_FAILED);
  }

  lb::AddToPlaylistResult result;
  result.added = static_cast<std::int64_t>(handles.get_count());
  return result;
}


// ========== Extended Library APIs ==========

// library.getArtistAlbums - Get all albums for a specific artist
//
// 行形状与 library.getAlbums 逐键同构（共用 BuildAlbumRow），差别有三处，
// 都是刻意保留的：分组键是纯专辑名而非「专辑名 + album artist」复合键，故同名
// 不同艺术家的专辑在本端点会合并；album 标签缺失的曲目归入 "(Unknown Album)"
// 而 getAlbums 直接跳过；album 标签存在但值为空串时本端点归入名为空串的分组，
// getAlbums 同样跳过。
//
// 另有一处同名不同义：行内的 trackCount / duration / discCount 只累加该艺术家
// 参与的曲目（折叠只对 mask 命中项调用），而 getAlbums 遍历全库、算的是整张
// 专辑。消费方拿本端点的行当专辑卡渲染会显示偏小的数字，故双语文档与 SDK
// JSDoc 都必须点明这条。
//
// 本函数自己的失败（未启用、查询出错）仍带空的 albums：旧版 SDK 类型把它
// 声明为必填，按必填读取的调用方拿到失败时也不至于解构出 undefined。参数
// 错误由生成的 Reader 在进入本函数之前报出，不带 albums。
api::Result<lb::GetArtistAlbumsResult> LibraryGetArtistAlbums(const lb::GetArtistAlbumsParams& p) {
  const std::string &artist = p.artist;
  const size_t limit = ToSize(p.limit);
  const std::string &sortBy = p.sort;

  auto lib = library_manager::get();
  if (!lib->is_library_enabled()) {
    return api::Fail("Library not enabled", ApiErrorCode::LIBRARY_DISABLED,
                     {{"albums", json::array()}});
  }

  try {
    // 默认精确匹配：fb2k 的查询引擎按原子值比较，所以 getArtists 给出的条目名
    // 直接就能命中多值 artist 的非首位值。match: 'substring' 走 HAS 的子串匹配，
    // 保留旧行为，代价是短名与互为子串的艺术家名会串台。
    const bool exact = (p.match != "substring");
    const std::optional<std::string> query =
        BuildValueQuery("artist", exact ? "IS" : "HAS", artist);

    // substring 分支没有后置校验，跳过预筛就等于返回全库。含双引号的名字在这
    // 条路径上表达不出来，只能照旧回空集——与改动前的可观察行为一致。
    if (!query && !exact) {
      lb::GetArtistAlbumsResult empty;
      empty.artist = artist;
      return empty;
    }

    metadb_handle_list allItems;
    lib->get_all_items(allItems);

    pfc::array_t<bool> mask;
    mask.set_size(allItems.get_count());
    if (query) {
      search_filter_v2::ptr filter = search_filter_manager_v2::get()->create_ex(
          query->c_str(), fb2k::service_new<completion_notify_dummy>(),
          search_filter_manager_v2::KFlagSuppressNotify);
      filter->test_multi(allItems, mask.get_ptr());
    } else {
      for (size_t i = 0; i < allItems.get_count(); i++) mask[i] = true;
    }

    // 对匹配结果按 album 分组。全量聚合完再截断到 limit —— 中途 break 会让已收
    // 集专辑的 trackCount 停在半路，触顶时系统性偏低。
    std::map<std::string, AlbumData> albumMap;

    for (size_t i = 0; i < allItems.get_count(); i++) {
      if (!mask[i]) continue;

      metadb_info_container::ptr infoContainer = allItems[i]->get_info_ref();
      if (!infoContainer.is_valid()) continue;
      const file_info& info = infoContainer->info();

      // 后置精确校验只加在 exact 分支上；substring 的语义就是宿主的包含匹配，
      // 收严会把它变成另一个契约。
      if (exact && !MatchesAtomicValue(info, "artist", artist)) continue;

      const char* album = info.meta_get("album", 0);
      std::string albumName = album ? album : "(Unknown Album)";

      const char* albumArtist = info.meta_get("album artist", 0);
      if (!albumArtist)
        albumArtist = info.meta_get("artist", 0);

      FoldTrackIntoAlbum(info, allItems[i], albumName.c_str(), albumArtist,
                         false, albumMap[albumName]);
    }

    std::vector<AlbumData> rows;
    rows.reserve(albumMap.size());
    for (const auto& [name, data] : albumMap) {
      rows.push_back(data);
    }

    std::sort(rows.begin(), rows.end(),
              [&sortBy](const AlbumData &a, const AlbumData &b) {
                return AlbumLess(a, b, sortBy);
              });

    const size_t total = rows.size();
    const size_t endIdx = std::min(limit, total);

    lb::GetArtistAlbumsResult result;
    result.artist = artist;
    result.albums.reserve(endIdx);
    for (size_t i = 0; i < endIdx; i++) {
      result.albums.push_back(BuildAlbumRow(rows[i], false));
    }
    result.total = static_cast<std::int64_t>(total);
    result.hasMore = endIdx < total;
    return result;
  } catch (...) {
    return api::Fail("Search failed", ApiErrorCode::OPERATION_FAILED,
                     {{"albums", json::array()}});
  }
}


// library.getGenres - Get all genres with track counts
api::Result<lb::GetGenresResult> LibraryGetGenres(const lb::GetGenresParams& /*params*/) {
  auto lib = library_manager::get();
  if (!lib->is_library_enabled()) {
    return api::Fail("Library not enabled", ApiErrorCode::LIBRARY_DISABLED);
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

  lb::GetGenresResult result;
  result.genres.reserve(genreCount.size());
  for (const auto &[name, count] : genreCount) {
    lb::LibraryValueCount row;
    row.name = name;
    row.trackCount = count;
    result.genres.push_back(std::move(row));
  }
  return result;
}


// ========== Field Values (Tag System) ==========
// library.getFieldValues - Aggregate unique values for any metadata field
// Generalized version of getGenres: supports multi-value fields and separator splitting
api::Result<lb::GetFieldValuesResult> LibraryGetFieldValues(const lb::GetFieldValuesParams& p) {
  const std::string &field = p.field;
  const std::string &separator = p.separator;
  const size_t limit = ToSize(p.limit);

  auto lib = library_manager::get();
  if (!lib->is_library_enabled()) {
    return api::Fail("Library not enabled", ApiErrorCode::LIBRARY_DISABLED,
                     {{"values", json::array()}});
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

  lb::GetFieldValuesResult result;
  result.values.reserve(sorted.size());
  for (auto &entry : sorted) {
    lb::LibraryValueCount row;
    row.name = std::move(entry.name);
    row.trackCount = entry.count;
    result.values.push_back(std::move(row));
  }
  result.total = static_cast<std::int64_t>(valueCount.size());
  result.field = field;
  return result;
}


// library.query 的延迟响应版：主线程只做过滤与标量捕获，排序、元数据读取与序列化
// 下 CPU worker。排序发生在截 limit 之前，total 是截断前的全命中数。成功回包由直写器
// 写出（形状见 src/api/schema/library.ts 的 QueryResult），失败回包走声明的失败信封。
void LibraryQueryDeferred(const lb::QueryParams& p, const DeferredResponder& responder) {
  const std::string &query = p.query;
  const size_t limit = ToSize(p.limit);
  const std::string &sortBy = p.sort;

  // fields 校验排在一切之前：名字错的请求不该先跑一遍全库扫描再被拒 —— fail-closed
  const TrackFieldSelection fields = ParseTrackFieldSelection(p.fields);
  if (!fields.valid) {
    responder.SendJson(MakeTrackFieldsErrorBody(fields));
    return;
  }

  auto lib = library_manager::get();
  if (!lib->is_library_enabled()) {
    responder.SendJson(api::results::FailureToJson(
        api::Fail("Library not enabled", ApiErrorCode::LIBRARY_DISABLED)));
    return;
  }

  // query 只带 SuppressNotify，不带 AllowSort —— 带 SORT BY 的串在此会被 create_ex
  // 拒掉，与语法错一样回 "Invalid query syntax"。
  search_filter_v2::ptr filter;
  try {
    filter = search_filter_manager_v2::get()->create_ex(
        query.c_str(), fb2k::service_new<completion_notify_dummy>(),
        search_filter_manager_v2::KFlagSuppressNotify);
  } catch (...) {
    responder.SendJson(MakeQuerySyntaxErrorBody(QueryWireApi::Query, nullptr));
    return;
  }

  try {
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

    // 脚本一律在主线程编译（titleformat 编译的主线程口径见 SDK
    // titleformat_object_cache 里的断言），worker 只负责求值。compile 失败 = 忽略
    // 排序继续，与同步版一致。sort 与 fields 互不相干：排序键是调用方给的
    // titleformat 串，不受投影字段集影响（排序仍在截 limit 之前）。
    if (!sortBy.empty()) {
      static_api_ptr_t<titleformat_compiler> compiler;
      titleformat_object::ptr script;
      if (compiler->compile(script, sortBy.c_str())) {
        req->sortScript = script;
      }
    }
    // rating 未被请求就连脚本都不编译
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
  } catch (const std::exception& e) {
    responder.SendJson(MakeQueryWireErrorBody(QueryWireApi::Query, e.what()));
  } catch (...) {
    responder.SendJson(MakeQueryWireErrorBody(QueryWireApi::Query, nullptr));
  }
}


// library.browseDirectory - Browse media library by directory
// ========== Root/Tree API ==========

// The tree index answers in json because core does not see the generated
// types; its keys are fixed by LibraryTreeIndex::GetRootsJson,
// GetBrowseTreeJson and DirectoryNodeToJson.
lb::LibraryRootInfo RootFromJson(const json &root) {
  lb::LibraryRootInfo row;
  row.id = root.value("id", std::string{});
  row.displayName = root.value("displayName", std::string{});
  row.rawPath = root.value("rawPath", std::string{});
  row.absolutePath = root.value("absolutePath", std::string{});
  row.trackCount = root.value("trackCount", std::int64_t{0});
  return row;
}

lb::LibraryDirectoryNodeInfo DirectoryNodeFromJson(const json &node) {
  lb::LibraryDirectoryNodeInfo row;
  row.id = node.value("id", std::string{});
  row.rootId = node.value("rootId", std::string{});
  row.pathId = node.value("pathId", std::string{});
  row.parentPathId = node.value("parentPathId", std::string{});
  row.name = node.value("name", std::string{});
  row.displayName = node.value("displayName", std::string{});
  row.rawPath = node.value("rawPath", std::string{});
  row.absolutePath = node.value("absolutePath", std::string{});
  row.relativePath = node.value("relativePath", std::string{});
  row.depth = node.value("depth", std::int64_t{0});
  row.trackCount = node.value("trackCount", std::int64_t{0});
  row.childDirectoryCount = node.value("childDirectoryCount", std::int64_t{0});
  row.hasChildren = node.value("hasChildren", false);
  return row;
}

api::Result<lb::GetRootsResult> LibraryGetRoots(const lb::GetRootsParams& /*params*/) {
  // 先捕获调用前索引是否已存在，用于判定 fromCache
  const bool wasValid = g_LibraryTreeIndex.IsValid();
  const json index = g_LibraryTreeIndex.GetRootsJson();

  if (!index.value("success", false)) {
    // 索引构建失败：索引给出的零值统计键照旧随失败带出
    json::object_t extra;
    for (const auto &[key, value] : index.items()) {
      if (key != "success" && key != "error") extra[key] = value;
    }
    return api::Fail(index.value("error", std::string("Failed to build library root index")),
                     ApiErrorCode::OPERATION_FAILED, std::move(extra));
  }

  lb::GetRootsResult result;
  result.enabled = index.value("enabled", false);
  result.total = index.value("total", std::int64_t{0});
  result.indexedTracks = index.value("indexedTracks", std::int64_t{0});
  result.skippedTracks = index.value("skippedTracks", std::int64_t{0});
  // 仅当调用前索引已存在时才标记为缓存命中
  result.fromCache = wasValid || index.value("fromCache", false);
  for (const auto &root : index.value("roots", json::array())) {
    result.roots.push_back(RootFromJson(root));
  }
  return result;
}


// ========== Typed Tree API ==========

api::Result<lb::BrowseTreeResult> LibraryBrowseTree(const lb::BrowseTreeParams& p) {
  // includeFiles 为 false 时忽略 recursiveFiles
  const bool includeFiles = p.includeFiles;
  const bool recursiveFiles = includeFiles && p.recursiveFiles;

  // 索引只给目录结构；files 的整行在下面按句柄构建。
  const json tree = g_LibraryTreeIndex.GetBrowseTreeJson(p.rootId, p.pathId);
  if (!tree.value("success", false)) {
    // 索引没建起来是宿主侧失败；建起来了还失败，就是根目录或路径不存在。
    const char *code =
        g_LibraryTreeIndex.IsValid() ? ApiErrorCode::NOT_FOUND : ApiErrorCode::OPERATION_FAILED;
    return api::Fail(tree.value("error", std::string("Failed to build library tree index")), code);
  }

  lb::BrowseTreeResult result;
  result.root = RootFromJson(tree.value("root", json::object()));
  result.pathId = tree.value("pathId", std::string{});
  result.absolutePath = tree.value("absolutePath", std::string{});
  result.fromCache = tree.value("fromCache", false);
  for (const auto &node : tree.value("directories", json::array())) {
    result.directories.push_back(DirectoryNodeFromJson(node));
  }

  if (includeFiles) {
    metadb_handle_list fileHandles;
    std::vector<size_t> globalIndices;
    g_LibraryTreeIndex.GetDirectoryFileHandles(p.rootId, p.pathId, recursiveFiles, fileHandles,
                                               globalIndices);
    result.files.reserve(fileHandles.get_count());
    for (size_t i = 0; i < fileHandles.get_count(); ++i) {
      if (!fileHandles[i].is_valid()) continue;
      result.files.push_back(LibraryTrackRow(fileHandles[i], globalIndices[i]));
    }
  }

  return result;
}


// ========== Legacy Directory API ==========

api::Result<lb::BrowseDirectoryResult> LibraryBrowseDirectory(const lb::BrowseDirectoryParams& p) {
  const std::string &pathStr = p.path;
  const bool includeFiles = p.includeFiles;

  auto lib = library_manager::get();
  if (!lib->is_library_enabled()) {
    return api::Fail("Library not enabled", ApiErrorCode::LIBRARY_DISABLED);
  }

  metadb_handle_list items;
  lib->get_all_items(items);

  std::set<std::string> directories;
  lb::BrowseDirectoryResult result;

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

      if (!lowerPath.starts_with(lowerFilter))
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
      result.files.push_back(LibraryTrackRow(item, i));
    }
  }

  result.directories.assign(directories.begin(), directories.end());
  result.items = result.directories;
  return result;
}


// Additional Library APIs for test compatibility

api::Result<lb::GetStatusResult> LibraryGetStatus(const lb::GetStatusParams& /*params*/) {
  auto lib = library_manager::get();
  const bool enabled = lib->is_library_enabled();

  auto &kept = Kept().status;
  g_LibraryCache.RecordLookup(kept.has_value());
  if (kept) {
    return *kept;
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

  lb::GetStatusResult result;
  result.enabled = enabled;
  result.initialized = enabled;
  result.scanning = false;
  result.itemCount = static_cast<std::int64_t>(count);
  result.count = static_cast<std::int64_t>(count);

  if (enabled) {
    kept = result;
    g_LibraryCache.NoteKept();
  }
  return result;
}


api::Result<lb::GetCountResult> LibraryGetCount(const lb::GetCountParams& /*params*/) {
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
  lb::GetCountResult result;
  result.count = static_cast<std::int64_t>(count);
  return result;
}


// Main-thread half of one row of the async library.getAll: identity fields,
// fileSize and rating are read here, the tags and technical fields from `info`
// on the worker.
struct PendingTrackRow {
  lb::LibraryTrack row;
  metadb_info_container::ptr info;
};

// Builds the whole library off the main thread and delivers it as the
// library:getAllResult event on the calling window. The list is kept only when
// the library did not change while the worker ran.
lb::GetAllResult StartAsyncGetAll(const metadb_handle_list &all, const lb::GetAllParams &p,
                                  const CallerContext &caller) {
  auto pending = std::make_shared<std::vector<PendingTrackRow>>();
  pending->reserve(all.get_count());
  for (size_t i = 0; i < all.get_count(); i++) {
    const auto &track = all[i];
    if (!track.is_valid())
      continue;

    PendingTrackRow item;
    FillTrackIdentity(item.row, track);
    item.info = track->get_info_ref();
    if (item.info.is_valid()) {
      item.row.rating = ResolveTrackRating(track, &item.info->info()).value;
    } else {
      item.row.duration = track->get_length();
    }
    item.row.index = static_cast<std::int64_t>(i);
    pending->push_back(std::move(item));
  }

  // The strings the lambdas below capture are not const: a const capture makes
  // the closure's move constructor copy it, and that copy can throw.
  std::string requestId = GenerateLibraryRequestId();
  const auto total = static_cast<std::int64_t>(all.get_count());
  const uint64_t generation = g_LibraryCache.GetGeneration();
  const std::int64_t offset = p.offset;
  const std::int64_t limit = p.limit;
  const HWND callerHwnd = caller.callerHwnd;
  std::string callerWindowId = caller.windowId;

  fb2k::inCpuWorkerThread([pending, requestId, total, generation, offset, limit, callerHwnd,
                           callerWindowId]() {
    try {
      auto rows = std::make_shared<std::vector<lb::LibraryTrack>>();
      rows->reserve(pending->size());
      for (auto &item : *pending) {
        if (item.info.is_valid())
          FillTrackMetadata(item.row, item.info->info());
        rows->push_back(std::move(item.row));
      }

      // The whole library is tens of thousands of rows, so the payload turns into
      // JSON here rather than on the main thread (api::emit::Prepared).
      lb::GetAllResultPayload payload;
      payload.requestId = requestId;
      payload.tracks = *rows;
      payload.items = *rows;
      payload.total = total;
      payload.offset = offset;
      payload.limit = limit;
      payload.fromCache = false;
      api::emit::Prepared<lb::events::GetAllResult> result(payload);

      // Delivered and kept on the main thread: PostWebMessageAsJson is
      // UI-thread, and the kept results are main-thread only.
      std::shared_ptr<const std::vector<lb::LibraryTrack>> kept = std::move(rows);
      fb2k::inMainThread([result = std::move(result), callerHwnd, callerWindowId, kept, total,
                          generation]() {
        if (g_LibraryCache.GetGeneration() == generation) {
          KeepAllTracks(kept, total);
        }
        api::emit::ToCaller<lb::events::GetAllResult>(callerWindowId, callerHwnd, result);
      });
    } catch (const std::exception &e) {
      std::string errMsg = e.what();
      fb2k::inMainThread([callerHwnd, callerWindowId, requestId, offset, limit, errMsg]() {
        lb::GetAllResultPayload payload;
        payload.requestId = requestId;
        payload.total = 0;
        payload.offset = offset;
        payload.limit = limit;
        payload.fromCache = false;
        payload.error = errMsg;
        api::emit::ToCaller<lb::events::GetAllResult>(callerWindowId, callerHwnd, payload);
      });
    }
  });

  lb::GetAllResult receipt;
  receipt.pending = true;
  receipt.requestId = requestId;
  return receipt;
}

api::Result<lb::GetAllResult> LibraryGetAll(const lb::GetAllParams& p, const CallerContext& caller) {
  const size_t offset = ToSize(p.offset);
  const size_t limit = ToSize(p.limit);

  lb::GetAllResult result;
  result.offset = p.offset;
  result.limit = p.limit;

  if (p.useCache && offset == 0) {
    const auto kept = Kept().allTracks;
    const std::int64_t keptTotal = Kept().allTracksTotal;
    g_LibraryCache.RecordLookup(kept != nullptr);
    if (kept) {
      const size_t end = std::min(limit, kept->size());
      result.tracks.emplace(kept->begin(), kept->begin() + static_cast<std::ptrdiff_t>(end));
      result.items = result.tracks;
      result.total = keptTotal;
      result.fromCache = true;
      return result;
    }
  }

  metadb_handle_list all;
  library_manager::get()->get_all_items(all);
  const size_t count = all.get_count();
  const size_t first = std::min(offset, count);
  const size_t end = first + std::min(limit, count - first);
  const bool complete = offset == 0 && end == count;

  // A full-library request serializes every track; built on the main thread it
  // holds up the invokes queued behind it, so on request it goes to a worker.
  if (p.asyncResult && p.useCache && complete) {
    return StartAsyncGetAll(all, p, caller);
  }

  std::vector<lb::LibraryTrack> rows;
  rows.reserve(end - first);
  for (size_t i = first; i < end; i++) {
    const auto &track = all[i];
    if (!track.is_valid())
      continue;
    rows.push_back(LibraryTrackRow(track, i));
  }

  if (complete) {
    KeepAllTracks(std::make_shared<const std::vector<lb::LibraryTrack>>(rows),
                  static_cast<std::int64_t>(count));
  }

  result.total = static_cast<std::int64_t>(count);
  result.fromCache = false;
  result.tracks = std::move(rows);
  result.items = result.tracks;
  return result;
}


api::Result<lb::GetByPathResult> LibraryGetByPath(const lb::GetByPathParams& p) {
  const std::string &path = p.path;

  // O(log n) handle creation: 规范化后直接建 handle。声明写明按第一个子曲目查找、
  // 不认 "|subsong:N" 后缀，所以序号固定为 0，不走 CreateTrackHandle。
  metadb_handle_ptr handle = SubsongUtils::CreateCanonicalHandle(path, 0);
  
  // O(1) hash lookup: 验证 handle 是否在库中
  lb::GetByPathResult result;
  if (!handle.is_valid() || !library_manager::get()->is_item_in_library(handle)) {
    result.found = false;
    result.path = path;
    return result;
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

  result.found = true;
  result.path = handle->get_path();
  result.absolutePath = std::string(nativePath.get_ptr());
  result.title = getMeta("title");
  result.artist = JoinMetaValues(artistValues, ", ");
  result.artists = artistValues;
  result.album = getMeta("album");
  result.duration = info.get_length();
  result.trackNumber = getMeta("tracknumber");
  result.genre = MetaJoined(info, "genre");
  result.date = getMeta("date");
  return result;
}


// library.getRecentlyAdded - Get recently added tracks
// sortBy: "added" (requires foo_playcount, auto-fallback) or "modified" (SDK native)
// Whether a %added% value is a time rather than a placeholder for "none".
bool HasAddedTime(const std::string &value) {
  return !value.empty() && value != "?" && value != "N/A";
}

api::Result<lb::GetRecentlyAddedResult> LibraryGetRecentlyAdded(const lb::GetRecentlyAddedParams& p) {
  const size_t limit = ToSize(p.limit);

  lb::GetRecentlyAddedResult result;
  result.limit = p.limit;
  result.sortBy = p.sortBy;
  result.fallback = false;

  metadb_handle_list all;
  library_manager::get()->get_all_items(all);
  const size_t total = all.get_count();
  result.total = static_cast<std::int64_t>(total);

  if (total == 0) {
    return result;
  }

  // --- sortBy=="added": use %added% titleformat (foo_playcount) ---
  if (p.sortBy == "added") {
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
      if (HasAddedTime(ts)) {
        validCount++;
      }
      entries.push_back({i, ts});
    }

    // If foo_playcount not available (all "?"), fallback to "modified"
    if (validCount == 0) {
      result.sortBy = "modified";
      result.fallback = true;
    } else {
      // Sort by added timestamp descending (lexicographic works for ISO dates)
      std::sort(entries.begin(), entries.end(), [](const IndexedTime& a, const IndexedTime& b) {
        // Invalid timestamps sort to the end
        const bool aValid = HasAddedTime(a.timeStr);
        const bool bValid = HasAddedTime(b.timeStr);
        if (aValid != bValid) return aValid > bValid;
        return a.timeStr > b.timeStr;
      });

      const size_t count = std::min(limit, entries.size());
      result.tracks.reserve(count);
      for (size_t i = 0; i < count; i++) {
        lb::RecentLibraryTrack row;
        static_cast<lb::LibraryTrack &>(row) = LibraryTrackRow(all[entries[i].index], entries[i].index);
        if (HasAddedTime(entries[i].timeStr)) {
          row.added = entries[i].timeStr;
        }
        result.tracks.push_back(std::move(row));
      }
      return result;
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

  const size_t count = std::min(limit, entries.size());
  result.tracks.reserve(count);
  for (size_t i = 0; i < count; i++) {
    lb::RecentLibraryTrack row;
    static_cast<lb::LibraryTrack &>(row) = LibraryTrackRow(all[entries[i].index], entries[i].index);
    // Convert Windows FILETIME (100ns since 1601) to Unix timestamp (seconds since 1970)
    if (entries[i].ts != filetimestamp_invalid) {
      row.modified = static_cast<std::int64_t>((entries[i].ts - 116444736000000000ULL) / 10000000ULL);
    }
    result.tracks.push_back(std::move(row));
  }
  return result;
}


api::Result<void> LibraryRefresh(const lb::RefreshParams& /*params*/) {
  library_manager::get()->rescan();
  return api::Ok();
}

} // namespace

void RegisterLibraryApi() {
    // 全部方法已在 src/api/schema/library.ts 声明，参数、结果与路径安全级别都来自
    // 生成的类型。

    api::RegisterApi("library.isEnabled", LibraryIsEnabled);
    api::RegisterApi("library.getStats", LibraryGetStats);
    api::RegisterApi("library.invalidateCache", LibraryInvalidateCache);
    api::RegisterApi("library.getCacheStats", LibraryGetCacheStats);
    // query / search 走 deferred：主线程只出过滤结果，序列化在 CPU worker 上做。
    // 注册名不变，两条路径不得同名并存 —— 同步表优先，留着同步注册等于改动作废。
    api::RegisterApiDeferred("library.search", LibrarySearchDeferred);
    api::RegisterApi("library.getAlbums", LibraryGetAlbums);
    api::RegisterApi("library.getArtists", LibraryGetArtists);
    api::RegisterApi("library.getGenres", LibraryGetGenres);
    api::RegisterApi("library.getAlbumTracks", LibraryGetAlbumTracks);
    api::RegisterApi("library.getArtistTracks", LibraryGetArtistTracks);
    api::RegisterApi("library.getRandomTracks", LibraryGetRandomTracks);
    api::RegisterApi("library.rescan", LibraryRescan);
    api::RegisterApi("library.addToPlaylist", LibraryAddToPlaylist);
    api::RegisterApi("library.getArtistAlbums", LibraryGetArtistAlbums);
    api::RegisterApi("library.getFieldValues", LibraryGetFieldValues);
    api::RegisterApiDeferred("library.query", LibraryQueryDeferred);
    api::RegisterApi("library.getRoots", LibraryGetRoots);
    api::RegisterApi("library.browseTree", LibraryBrowseTree);
    api::RegisterApi("library.browseDirectory", LibraryBrowseDirectory);
    api::RegisterApi("library.getStatus", LibraryGetStatus);
    api::RegisterApi("library.getCount", LibraryGetCount);
    api::RegisterApi("library.getAll", LibraryGetAll);
    api::RegisterApi("library.getByPath", LibraryGetByPath);
    api::RegisterApi("library.getRecentlyAdded", LibraryGetRecentlyAdded);
    api::RegisterApi("library.refresh", LibraryRefresh);
}
