#include "pch.h"
#include "api/PlaylistApi.h"
#include "api/ApiConstants.h"
#include "api/AsyncOperationRegistry.h"
#include "api/BridgeCore.h"
#include "api/ErrorEnvelope.h"
#include "api/EventEmit.h"
#include "api/GroupRunPlan.h"
#include "api/MetaAccess.h"
#include "api/PlaylistLock.h"
#include "api/PlaylistTarget.h"
#include "api/RatingResolve.h"
#include "api/TrackRow.h"
#include "api/TrackWireSnapshot.h"
#include "api/TypedApi.h"
#include "api/generated/PlaylistSchema.h"
#include "core/WebViewContext.h"
#include "api/adapters/Fb2kPlaylistService.h"
#include "api/adapters/Fb2kPlaybackService.h"
#include "utils/GuidUtils.h"
#include "utils/PlaylistFormatUtils.h"
#include "utils/SubsongUtils.h"
#include <atomic>
#include <random>

// ============================================
// Helper Functions
// ============================================

struct ParsedPlayablePath {
    std::string path;
    t_uint32 subsong = 0;
    bool hasSubsong = false;
};

// 解析 path|subsong:N，返回基础路径与 subsong 信息
static ParsedPlayablePath ParsePlayablePath(const std::string& input) {
    ParsedPlayablePath result;
    result.path = input;

    size_t pos = input.find("|subsong:");
    if (pos == std::string::npos) {
        return result;
    }

    result.hasSubsong = true;
    result.path = input.substr(0, pos);
    try {
        result.subsong = static_cast<t_uint32>(std::stoul(input.substr(pos + 9)));
    } catch (...) {
        result.subsong = 0;
    }
    return result;
}

// 将前端 paths 解析为 metadb_handle，优先处理 path|subsong:N，普通路径批量走 process_locations
// 修复：批量调用 process_locations 避免逐条调用导致弹窗风暴
//
// 顺序语义（playlist.addPaths / replaceAllAndPlay 共用）：普通路径段传 p_filter=true，
// 即 fb2k 原生"添加文件"的 incoming item filter——按指针排序去重后再按用户配置的
// incoming sorter 排序（见 SDK playlist.h 对 incoming item filter 的说明）。因此输出顺序
// **不是**输入顺序，也会去掉重复项；这是已发布行为，保持不变（queue.addPaths 取同一做法）。
// 需要严格保持输入顺序的调用方走 add_paths_sequential。
static void ResolvePathsToHandles(const json& paths, metadb_handle_list& outItems, size_t& invalidCount) {
    auto piif = playlist_incoming_item_filter::get();

    // 第一遍：分离 subsong 路径（直接建 handle）和普通路径（收集后批量处理）
    pfc::string_list_impl batchPaths;
    // 记录每条路径在 outItems 中的插入顺序：
    // type=0 表示 subsong handle（已直接加入 subsongHandles），type=1 表示普通路径（待批量解析）
    struct PathEntry { int type; size_t index; };
    std::vector<PathEntry> order;
    metadb_handle_list subsongHandles;
    size_t batchIndex = 0;

    for (const auto& pathValue : paths) {
        if (!pathValue.is_string()) {
            invalidCount++;
            continue;
        }

        ParsedPlayablePath parsed = ParsePlayablePath(pathValue.get<std::string>());
        if (parsed.path.empty()) {
            invalidCount++;
            continue;
        }

        // 拒绝超长 URL：foo_httpstream 内部缓冲区有限，超过 2048 会抛
        // ERROR_BUFFER_OVERFLOW 触发 fb2k 原生弹窗。
        if (parsed.path.length() > ApiLimits::MAX_STREAM_URL_LENGTH) {
            invalidCount++;
            continue;
        }

        // subsong 指定时直接建 handle（经规范化），避免 process_locations 把后缀当成路径文本
        if (parsed.hasSubsong) {
            metadb_handle_ptr handle = SubsongUtils::CreateCanonicalHandle(parsed.path, parsed.subsong);
            if (handle.is_valid()) {
                order.push_back({0, subsongHandles.get_count()});
                subsongHandles.add_item(handle);
            } else {
                invalidCount++;
            }
            continue;
        }

        order.push_back({1, batchIndex++});
        batchPaths.add_item(parsed.path.c_str());
    }

    // 第二遍：一次性调用 process_locations 解析所有普通路径
    metadb_handle_list batchResolved;
    if (batchPaths.get_count() > 0) {
        piif->process_locations(batchPaths, batchResolved, true, nullptr, nullptr, core_api::get_main_window());
    }

    // 如果没有 subsong 混合，直接输出批量结果（快速路径）
    if (subsongHandles.get_count() == 0) {
        if (batchResolved.get_count() == 0 && batchPaths.get_count() > 0) {
            invalidCount += batchPaths.get_count();
        }
        outItems += batchResolved;
        return;
    }

    // 有 subsong 混合时：显式 subsong 项按输入顺序前置，批量解析结果整体追加。
    // 批量段内部的顺序由 p_filter=true 的 filter_items 决定（见函数头注释），
    // 不与输入对齐；process_locations 也不报告每条输入展开出的 handle 边界，
    // 无法把批量结果按输入位置切回去。跨类型混排时的输入顺序在此不保证。
    for (const auto& entry : order) {
        if (entry.type == 0) {
            outItems.add_item(subsongHandles[entry.index]);
        }
    }
    if (batchResolved.get_count() > 0) {
        outItems += batchResolved;
    } else if (batchPaths.get_count() > 0) {
        invalidCount += batchPaths.get_count();
    }
}

// 将 JSON handles 数组（支持 object {path,subsong} 和 string 两种格式）解析为 metadb_handle_list
static void ParseJsonHandlesToList(const json& handles, metadb_handle_list& outItems, size_t& invalidCount) {
    for (const auto& h : handles) {
        std::string path;
        t_uint32 subsong = 0;

        if (h.is_object()) {
            path = h.value("path", "");
            subsong = h.value("subsong", 0);
        } else if (h.is_string()) {
            ParsedPlayablePath parsed = ParsePlayablePath(h.get<std::string>());
            path = parsed.path;
            subsong = parsed.subsong;
        } else {
            invalidCount++;
            continue;
        }

        if (path.empty()) {
            invalidCount++;
            continue;
        }

        if (path.length() > ApiLimits::MAX_STREAM_URL_LENGTH) {
            invalidCount++;
            continue;
        }

        metadb_handle_ptr handle = SubsongUtils::CreateCanonicalHandle(path, subsong);
        if (handle.is_valid()) {
            outItems.add_item(handle);
        } else {
            invalidCount++;
        }
    }
}

// Autoplaylist 检测结果
struct AutoplaylistDetection {
    bool isAutoplaylist = false;
    bool isDuiAutoplaylist = false;
    std::string lockName;
};

// 检测指定播放列表是否为 autoplaylist（支持 SDK 与 DUI fallback）
static AutoplaylistDetection DetectAutoplaylist(size_t playlistIndex) {
    AutoplaylistDetection result;
    auto apm = autoplaylist_manager::get();
    auto plm = playlist_manager::get();
    result.isAutoplaylist = apm->is_client_present(playlistIndex);

    if (!result.isAutoplaylist && plm->playlist_lock_is_present(playlistIndex)) {
        pfc::string8 lockNameBuf;
        if (plm->playlist_lock_query_name(playlistIndex, lockNameBuf)) {
            result.lockName = lockNameBuf.c_str();
            if (result.lockName == "Autoplaylist" || result.lockName.find("Auto") != std::string::npos) {
                result.isAutoplaylist = true;
                result.isDuiAutoplaylist = true;
            }
        }
    }
    return result;
}

// 生成唯一操作 ID
//
// 走共享的 FormatAsyncOperationId（AsyncOperationRegistry.cpp），与
// metadata.probeBatchAsync 的 probe_ 和 file.*Async 的 fileop_ 同一套格式。
// 不用「毫秒时间戳 + 6 位随机」这类格式：时间戳部分对任何页面都是可预测的，
// id 的熵只剩那 6 位。operationId 对页面是不透明句柄，格式不属于对外契约。
//
// 只在主线程的 handler 里调用，所以 mt19937_64 不需要加锁。
static std::string GenerateOperationId() {
    static std::mt19937_64 rng(std::random_device{}());
    static std::atomic<uint64_t> counter{0};
    return fb2k_api::FormatAsyncOperationId("addpaths", counter.fetch_add(1) + 1, rng());
}

json GetPlaylistInfo(size_t index, bool includeDuration) {
    auto plm = playlist_manager::get();
    
    if (index >= plm->get_playlist_count()) {
        return nullptr;
    }
    
    pfc::string8 name;
    plm->playlist_get_name(index, name);
    
    size_t trackCount = plm->playlist_get_item_count(index);
    bool isActive = (index == plm->get_active_playlist());
    bool isPlaying = (index == plm->get_playing_playlist());
    
    json result = {
        {"index", index},
        {"name", std::string(name.c_str())},
        {"trackCount", trackCount},
        {"isActive", isActive},
        {"isPlaying", isPlaying},
        {"isLocked", plm->playlist_lock_is_present(index)},
    };
    
    // 按需计算 duration，避免默认加载全部轨道
    if (includeDuration) {
        double totalDuration = 0;
        metadb_handle_list items;
        plm->playlist_get_all_items(index, items);
        for (size_t i = 0; i < items.get_count(); i++) {
            if (items[i].is_valid()) {
                totalDuration += items[i]->get_length();
            }
        }
        result["duration"] = totalDuration;
    }
    
    return result;
}

// ============================================
// Fb2kPlaylistService — out-of-line definitions
// (These methods depend on static helpers defined above)
// ============================================

IPlaylistService::InsertTracksResult Fb2kPlaylistService::insert_tracks(
    size_t playlist, size_t position, const json& handles) {
    auto plm = playlist_manager::get();
    size_t countBefore = plm->playlist_get_item_count(playlist);
    if (position > countBefore) position = countBefore;

    metadb_handle_list items;
    size_t invalidCount = 0;
    ParseJsonHandlesToList(handles, items, invalidCount);

    if (items.get_count() == 0) {
        return { false, "No valid handles created", 0, invalidCount, countBefore, countBefore };
    }

    plm->playlist_undo_backup(playlist);
    plm->playlist_insert_items(playlist, position, items, bit_array_false());
    size_t countAfter = plm->playlist_get_item_count(playlist);

    return { true, "", items.get_count(), invalidCount, countBefore, countAfter };
}

namespace {

namespace plrow = api::playlist;

// The fields that come from the file's tags and technical info, as opposed to the handle.
constexpr uint32_t kInfoFieldMask =
    TrackField::kTitle | TrackField::kArtist | TrackField::kArtists | TrackField::kAlbum |
    TrackField::kAlbumArtist | TrackField::kAlbumArtists | TrackField::kGenre | TrackField::kDate |
    TrackField::kTrackNumber | TrackField::kDiscNumber | TrackField::kDuration |
    TrackField::kBitrate | TrackField::kSampleRate | TrackField::kChannels | TrackField::kCodec;

// composer and comment, which a playlist row carries beyond the shared track fields.
void FillPlaylistTags(std::string& composer, std::string& comment, const file_info* info) {
    if (info == nullptr) return;
    composer = MetaJoined(*info, "composer");
    const char* value = info->meta_get("comment", 0);
    comment = value ? StringUtils::SafeUtf8(value) : std::string();
}

// A playlist row narrowed to `mask`, `index` always included. The values come from the builders
// of the shared track row (TrackRow.h), so a row with every field equals BuildTrackRow plus
// index. What costs is only computed when asked for: handle and absolutePath go through
// g_get_native_path, rating through %rating%; path, subsong and fileSize alone are read straight
// off the handle, as library.query does. An invalid handle gives the requested keys with type
// defaults.
plrow::PlaylistTrackPartial PlaylistRowOf(const metadb_handle_ptr& track, size_t index,
                                          uint32_t mask, const file_info* info) {
    plrow::PlaylistTrackPartial row;
    row.index = static_cast<std::int64_t>(index);

    api::common::Track full;
    if (track.is_valid()) {
        if (mask & (TrackField::kHandle | TrackField::kAbsolutePath)) {
            FillTrackIdentity(full, track);
        } else {
            if (mask & TrackField::kPath) full.path = StringUtils::SafeUtf8(track->get_path());
            if (mask & TrackField::kSubsong) full.subsong = track->get_subsong_index();
            if (mask & TrackField::kFileSize) {
                full.fileSize = static_cast<std::int64_t>(track->get_filesize());
            }
        }
        // Same split as BuildTrackRow: no info, no rating and the duration from the handle
        if (info != nullptr) {
            if (mask & kInfoFieldMask) FillTrackMetadata(full, *info);
            if (mask & TrackField::kRating) full.rating = ResolveTrackRating(track, info).value;
        } else if (mask & TrackField::kDuration) {
            full.duration = track->get_length();
        }
    }

    if (mask & TrackField::kHandle) row.handle = std::move(full.handle);
    if (mask & TrackField::kPath) row.path = std::move(full.path);
    if (mask & TrackField::kAbsolutePath) row.absolutePath = std::move(full.absolutePath);
    if (mask & TrackField::kSubsong) row.subsong = full.subsong;
    if (mask & TrackField::kTitle) row.title = std::move(full.title);
    if (mask & TrackField::kArtist) row.artist = std::move(full.artist);
    if (mask & TrackField::kArtists) row.artists = std::move(full.artists);
    if (mask & TrackField::kAlbum) row.album = std::move(full.album);
    if (mask & TrackField::kAlbumArtist) row.albumArtist = std::move(full.albumArtist);
    if (mask & TrackField::kAlbumArtists) row.albumArtists = std::move(full.albumArtists);
    if (mask & TrackField::kGenre) row.genre = std::move(full.genre);
    if (mask & TrackField::kDate) row.date = std::move(full.date);
    if (mask & TrackField::kTrackNumber) row.trackNumber = full.trackNumber;
    if (mask & TrackField::kDiscNumber) row.discNumber = full.discNumber;
    if (mask & TrackField::kDuration) row.duration = full.duration;
    if (mask & TrackField::kFileSize) row.fileSize = full.fileSize;
    if (mask & TrackField::kBitrate) row.bitrate = full.bitrate;
    if (mask & TrackField::kSampleRate) row.sampleRate = full.sampleRate;
    if (mask & TrackField::kChannels) row.channels = full.channels;
    if (mask & TrackField::kCodec) row.codec = std::move(full.codec);
    if (mask & TrackField::kRating) row.rating = full.rating;
    return row;
}

// %play_count% as an integer; nothing when the text is not one (no foo_playcount gives "?").
std::optional<std::int64_t> PlayCountOf(const std::string& text) {
    if (text.empty()) return std::nullopt;
    char* end = nullptr;
    const long long value = std::strtoll(text.c_str(), &end, 10);
    if (end == text.c_str() || *end != '\0') return std::nullopt;
    return static_cast<std::int64_t>(value);
}

// playlist.getTracks 与 playlist.getTracksAt 共用的行构造：items[i] 是第 rows[i] 行的句柄，
// 两者等长。两个方法的行因此逐键相同，只是挑哪些行不同。
std::vector<plrow::PlaylistTrackPartial> BuildPlaylistRows(
    const metadb_handle_list& items, const std::vector<size_t>& rows,
    const std::map<std::string, std::string>& formats, const TrackFieldSelection& fields) {
    std::vector<plrow::PlaylistTrackPartial> out;

    // Compile optional title format columns (foo_playcount virtual fields etc.)
    std::vector<std::pair<std::string, titleformat_object::ptr>> compiledFormats;
    if (!formats.empty()) {
        auto compiler = titleformat_compiler::get();
        for (const auto& [key, pattern] : formats) {
            titleformat_object::ptr script;
            compiler->compile_safe(script, pattern.c_str());
            compiledFormats.emplace_back(key, script);
        }
    }

    // foo_playcount virtual fields — static compilation
    // 投影态不产出这四个键（它们不在投影白名单里，要取值走 formats），
    // 只在全字段路径逐行求值——每行要多跑四次 formatTitle_v2，开销较大。
    static titleformat_object::ptr s_playCount, s_firstPlayed, s_lastPlayed, s_added;
    static bool s_fpcInit = false;
    if (!s_fpcInit) {
        auto compiler = titleformat_compiler::get();
        compiler->compile_safe(s_playCount,   "%play_count%");
        compiler->compile_safe(s_firstPlayed, "[%first_played%]");
        compiler->compile_safe(s_lastPlayed,  "[%last_played%]");
        compiler->compile_safe(s_added,       "[%added%]");
        s_fpcInit = true;
    }

    auto mdb2 = metadb_v2::get();
    // info 记录只有两处消费：全字段路径的四个 playcount 键、formats 附加列。
    // 投影且无 formats 时整批不取，省掉每页一次数据库往返。
    pfc::array_t<metadb_v2::rec_t> v2recs;
    if (!fields.projected || !compiledFormats.empty()) {
        v2recs = mdb2->queryMultiSimple(items);
    }

    const uint32_t mask = fields.projected ? fields.mask : TrackField::kAll;
    out.reserve(items.get_count());
    for (size_t i = 0; i < items.get_count(); i++) {
        const metadb_handle_ptr& track = items[i];
        metadb_info_container::ptr container;
        if (track.is_valid() && (!fields.projected || (mask & (kInfoFieldMask | TrackField::kRating)))) {
            container = track->get_info_ref();
        }
        const file_info* info = container.is_valid() ? &container->info() : nullptr;

        plrow::PlaylistTrackPartial row = PlaylistRowOf(track, rows[i], mask, info);

        auto evalV2 = [&](const titleformat_object::ptr& script) -> std::string {
            pfc::string8 buf;
            mdb2->formatTitle_v2(track, v2recs[i], nullptr, buf, script, nullptr);
            if (buf.get_length() == 0 || (buf.get_length() == 1 && buf[0] == '?')) return "";
            return std::string(buf.get_ptr());
        };

        if (!fields.projected) {
            std::string composer;
            std::string comment;
            FillPlaylistTags(composer, comment, info);
            row.composer = std::move(composer);
            row.comment = std::move(comment);
            if (track.is_valid()) {
                row.playCount = PlayCountOf(evalV2(s_playCount));
                if (std::string value = evalV2(s_firstPlayed); !value.empty()) row.firstPlayed = std::move(value);
                if (std::string value = evalV2(s_lastPlayed); !value.empty()) row.lastPlayed = std::move(value);
                if (std::string value = evalV2(s_added); !value.empty()) row.added = std::move(value);
            }
        }

        if (!compiledFormats.empty()) {
            std::map<std::string, std::string> values;
            for (const auto& [key, script] : compiledFormats) {
                pfc::string8 text;
                if (track.is_valid()) {
                    mdb2->formatTitle_v2(track, v2recs[i], nullptr, text, script, nullptr);
                }
                values[key] = StringUtils::SafeUtf8(text.get_ptr());
            }
            row.formats = std::move(values);
        }
        out.push_back(std::move(row));
    }
    return out;
}

}  // namespace

plrow::GetTracksResult Fb2kPlaylistService::get_tracks(
    size_t playlist, size_t start, size_t count, const std::map<std::string, std::string>& formats,
    const TrackFieldSelection& fields) const {
    auto plm = playlist_manager::get();
    const size_t totalCount = plm->playlist_get_item_count(playlist);

    plrow::GetTracksResult result;
    result.playlist = static_cast<std::int64_t>(playlist);
    result.playlistGuid = api::PlaylistGuidOf(*this, playlist);
    result.start = static_cast<std::int64_t>(start);
    result.total = static_cast<std::int64_t>(totalCount);
    if (start >= totalCount) {
        return result;
    }

    const size_t actualCount = std::min(count, totalCount - start);
    metadb_handle_list items;
    pfc::bit_array_range range(start, actualCount);
    plm->playlist_get_items(playlist, items, range);

    std::vector<size_t> rows(items.get_count());
    for (size_t i = 0; i < rows.size(); i++) rows[i] = start + i;
    result.tracks = BuildPlaylistRows(items, rows, formats, fields);
    result.count = static_cast<std::int64_t>(result.tracks.size());
    return result;
}

// 行号任意、可重复、可乱序，所以逐行取句柄，而不是按掩码取：playlist_get_items 按列表顺序
// 回答并把重复的行并成一行。
plrow::GetTracksAtResult Fb2kPlaylistService::get_tracks_at(
    size_t playlist, const std::vector<size_t>& rows, const std::map<std::string, std::string>& formats,
    const TrackFieldSelection& fields) const {
    auto plm = playlist_manager::get();
    const size_t totalCount = plm->playlist_get_item_count(playlist);

    plrow::GetTracksAtResult result;
    result.playlist = static_cast<std::int64_t>(playlist);
    result.playlistGuid = api::PlaylistGuidOf(*this, playlist);
    result.total = static_cast<std::int64_t>(totalCount);

    metadb_handle_list items;
    std::vector<size_t> kept;
    kept.reserve(rows.size());
    for (const size_t row : rows) {
        metadb_handle_ptr track;
        if (row >= totalCount || !plm->playlist_get_item_handle(track, playlist, row)) continue;
        items.add_item(track);
        kept.push_back(row);
    }
    result.tracks = BuildPlaylistRows(items, kept, formats, fields);
    result.count = static_cast<std::int64_t>(result.tracks.size());
    return result;
}

plrow::GetSelectedTracksResult Fb2kPlaylistService::get_selected_tracks(size_t playlist) const {
    auto plm = playlist_manager::get();
    size_t itemCount = plm->playlist_get_item_count(playlist);

    pfc::bit_array_bittable selection(itemCount);
    plm->playlist_get_selection_mask(playlist, selection);

    metadb_handle_list items;
    plm->playlist_get_selected_items(playlist, items);

    plrow::GetSelectedTracksResult result;
    result.playlist = static_cast<std::int64_t>(playlist);
    result.playlistGuid = api::PlaylistGuidOf(*this, playlist);
    size_t trackIdx = 0;
    for (size_t i = 0; i < itemCount; i++) {
        if (!selection.get(i) || trackIdx >= items.get_count()) continue;
        const metadb_handle_ptr& track = items[trackIdx++];
        // 无效句柄没有可写的身份，跳过
        if (!track.is_valid()) continue;

        metadb_info_container::ptr container = track->get_info_ref();
        const file_info* info = container.is_valid() ? &container->info() : nullptr;
        plrow::PlaylistTrack row;
        static_cast<api::common::Track&>(row) = BuildTrackRow(track, info);
        row.index = static_cast<std::int64_t>(i);
        FillPlaylistTags(row.composer, row.comment, info);
        result.tracks.push_back(std::move(row));
    }

    result.count = static_cast<std::int64_t>(result.tracks.size());
    return result;
}

// -- Path-based service methods (out-of-line) -----------------

IPlaylistService::AddPathsResult Fb2kPlaylistService::add_paths(
    size_t playlist, const json& paths) {
    auto plm = playlist_manager::get();
    const api::PinnedPlaylist target(*this, playlist);

    AddPathsResult r;
    r.playlist = playlist;
    r.countBefore = plm->playlist_get_item_count(playlist);
    r.totalCount = r.countBefore;

    metadb_handle_list items;
    ResolvePathsToHandles(paths, items, r.invalidCount);

    // 解析时开过模态进度框，列表可能已被拖动、删掉或上锁。一首都没解析出来也先找回：
    // 失败信封里的序号与曲目数要是这张列表现在的，被删掉时报删掉而不是报没有曲目。
    const auto located = target.Locate(*this);
    if (!located.has_value()) {
        r.outcome = PathInsertOutcome::PlaylistRemoved;
        return r;
    }
    r.playlist = *located;
    r.countBefore = plm->playlist_get_item_count(r.playlist);
    r.totalCount = r.countBefore;
    if (items.get_count() == 0) return r;

    plm->playlist_undo_backup(r.playlist);
    if (plm->playlist_insert_items(r.playlist, r.countBefore, items, bit_array_false()) == SIZE_MAX) {
        r.outcome = PathInsertOutcome::Refused;
        return r;
    }
    r.addedCount = items.get_count();
    r.totalCount = plm->playlist_get_item_count(r.playlist);
    return r;
}

// 句柄直接由路径建出，不经 process_locations，写入前没有让出消息循环，无需重新定位目标。
IPlaylistService::AddPathsResult Fb2kPlaylistService::add_handles(
    size_t playlist, const json& handles) {
    auto plm = playlist_manager::get();
    size_t countBefore = plm->playlist_get_item_count(playlist);

    metadb_handle_list items;
    size_t invalidCount = 0;
    ParseJsonHandlesToList(handles, items, invalidCount);

    if (items.get_count() == 0) {
        return { 0, invalidCount, countBefore, countBefore, PathInsertOutcome::Ok, playlist };
    }

    plm->playlist_undo_backup(playlist);
    size_t insertPos = plm->playlist_get_item_count(playlist);
    plm->playlist_insert_items(playlist, insertPos, items, bit_array_false());
    size_t countAfter = plm->playlist_get_item_count(playlist);

    return { items.get_count(), invalidCount, countBefore, countAfter, PathInsertOutcome::Ok, playlist };
}

// playlist.addPathsSequential 的契约是"按给定顺序逐条追加"（SDK JSDoc: one-by-one），
// 因此与 ResolvePathsToHandles 刻意不同：普通路径段传 p_filter=false 关掉
// filter_items 的按指针排序去重，段内输出顺序即输入顺序；只把"连续普通路径段"
// 合成一次 process_locations 调用（避免逐条调用的进度框风暴），段与段之间、以及
// 与 `|subsong:N` 项之间按原始出现顺序拼接。一条路径展开出的 N 个 handle
// （cue / 多 subsong 文件）天然占据该路径在段内的位置。与 QueueApi 的
// ResolveInsertNextPaths 同一做法。
IPlaylistService::AddPathsSequentialResult Fb2kPlaylistService::add_paths_sequential(
    size_t playlist, const json& paths) {
    auto plm = playlist_manager::get();
    auto piif = playlist_incoming_item_filter::get();
    const api::PinnedPlaylist target(*this, playlist);

    metadb_handle_list ordered;
    pfc::string_list_impl runBatch;
    auto flushRun = [&]() {
        if (runBatch.get_count() == 0) return;
        metadb_handle_list resolved;
        piif->process_locations(runBatch, resolved, /*p_filter=*/false, nullptr, nullptr,
                                core_api::get_main_window());
        ordered += resolved;
        runBatch.remove_all();
    };

    for (const auto& path : paths) {
        if (!path.is_string()) continue;
        ParsedPlayablePath parsed = ParsePlayablePath(path.get<std::string>());
        if (parsed.path.empty()) continue;
        if (parsed.path.length() > ApiLimits::MAX_STREAM_URL_LENGTH) continue;

        if (parsed.hasSubsong) {
            flushRun();
            metadb_handle_ptr handle = SubsongUtils::CreateCanonicalHandle(parsed.path, parsed.subsong);
            if (handle.is_valid()) {
                ordered.add_item(handle);
            }
            continue;
        }
        runBatch.add_item(parsed.path.c_str());
    }
    flushRun();

    AddPathsSequentialResult r;
    r.order = json::array();
    r.playlist = playlist;

    // 解析时开过模态进度框，列表可能已被拖动、删掉或上锁。一首都没解析出来也先找回：
    // 成功应答里的序号要是这张列表现在的，被删掉时照 addPaths 报删掉。
    const auto located = target.Locate(*this);
    if (!located.has_value()) {
        r.outcome = PathInsertOutcome::PlaylistRemoved;
        return r;
    }
    r.playlist = *located;
    if (ordered.get_count() == 0) return r;

    plm->playlist_undo_backup(r.playlist);
    const size_t insertPos = plm->playlist_get_item_count(r.playlist);
    if (plm->playlist_insert_items(r.playlist, insertPos, ordered, bit_array_false()) == SIZE_MAX) {
        r.outcome = PathInsertOutcome::Refused;
        return r;
    }
    r.addedCount = ordered.get_count();
    for (size_t j = 0; j < ordered.get_count(); j++) {
        r.order.push_back(insertPos + j);
    }
    return r;
}

IPlaylistService::AsyncAddPathsInfo Fb2kPlaylistService::start_add_paths_async(
    size_t playlist, const json& paths, const std::string& operationId,
    std::function<void(size_t addedCount, size_t totalCount)> onComplete) {
    std::vector<ParsedPlayablePath> parsedPaths;
    size_t invalidCount = 0;

    for (const auto& p : paths) {
        if (p.is_string()) {
            ParsedPlayablePath parsed = ParsePlayablePath(p.get<std::string>());
            if (parsed.path.empty()) { invalidCount++; continue; }
            if (parsed.path.length() > ApiLimits::MAX_STREAM_URL_LENGTH) {
                invalidCount++;
                continue;
            }
            parsedPaths.push_back(std::move(parsed));
        } else { invalidCount++; }
    }
    if (parsedPaths.empty()) {
        return { 0, invalidCount };
    }

    size_t totalCount = parsedPaths.size();

    // 分流:本地路径 / 流媒体直链经规范化后同步建 handle,零弹窗;
    // 仅 .pls / .m3u / .cue 等 wrapper 走 process_locations_async 展开。
    // `|subsong:N` 已由 ParsePlayablePath 拆出,快路径必须把它交给 handle
    // (此前恒传 0,cue 子曲目会被当成整轨)。
    metadb_handle_list fastHandles;
    pfc::list_t<pfc::string8> slowStrings;
    pfc::list_t<const char*> slowList;
    for (const auto& parsed : parsedPaths) {
        if (PlaylistFormatUtils::LooksLikePlaylistWrapper(parsed.path.c_str())) {
            slowStrings.add_item(pfc::string8(parsed.path.c_str()));
        } else {
            metadb_handle_ptr handle = SubsongUtils::CreateCanonicalHandle(parsed.path, parsed.subsong);
            if (handle.is_valid()) fastHandles.add_item(handle);
        }
    }
    for (size_t i = 0; i < slowStrings.get_count(); i++) {
        slowList.add_item(slowStrings[i].get_ptr());
    }

    // playlist_insert_items 整批插入或整批拒绝（锁的 query_items_add 对整批只答一次），
    // 拒绝时返回 SIZE_MAX；只把真插进去的计入 addedCount。
    auto plm = playlist_manager::get();
    size_t fastAdded = 0;
    if (fastHandles.get_count() > 0 && playlist < plm->get_playlist_count()) {
        plm->playlist_undo_backup(playlist);
        size_t insertPos = plm->playlist_get_item_count(playlist);
        if (plm->playlist_insert_items(playlist, insertPos, fastHandles, bit_array_false()) != SIZE_MAX) {
            fastAdded = fastHandles.get_count();
        }
    }

    if (slowList.get_count() == 0) {
        // 全是 fast path,立即触发回调,不走 process_locations_async (永不弹窗)。
        if (onComplete) onComplete(fastAdded, totalCount);
        return { totalCount, invalidCount };
    }

    // 还有 wrapper 路径,异步展开 (op_flag_delay_ui+background 短操作不弹)。
    const api::PinnedPlaylist target(*this, playlist);
    auto filter = playlist_incoming_item_filter_v2::get();
    auto notify = process_locations_notify::create(
        [target, totalCount, fastAdded, onComplete](metadb_handle_list_cref items) {
            // 展开期间列表可能被拖动、删掉或上锁：按 GUID 找回目标，删掉了就不插；
            // 按插入的返回值计数，而不是按展开出的条数。
            size_t slowAdded = 0;
            auto plm = playlist_manager::get();
            if (const auto located = target.Locate(Fb2kPlaylistService{});
                items.get_count() > 0 && located.has_value()) {
                plm->playlist_undo_backup(*located);
                size_t insertPos = plm->playlist_get_item_count(*located);
                if (plm->playlist_insert_items(*located, insertPos, items, bit_array_false()) != SIZE_MAX) {
                    slowAdded = items.get_count();
                }
            }
            if (onComplete) onComplete(fastAdded + slowAdded, totalCount);
        }
    );
    filter->process_locations_async(slowList,
        playlist_incoming_item_filter_v2::op_flag_delay_ui | playlist_incoming_item_filter_v2::op_flag_background,
        nullptr, nullptr, nullptr, notify);

    return { totalCount, invalidCount };
}

IPlaylistService::ReplaceAllResult Fb2kPlaylistService::replace_all(
    size_t playlist, const json& paths) {
    auto plm = playlist_manager::get();
    const api::PinnedPlaylist target(*this, playlist);

    ReplaceAllResult r;
    r.playlist = playlist;
    plm->playlist_undo_backup(playlist);
    r.clearedCount = plm->playlist_get_item_count(playlist);
    plm->playlist_clear(playlist);

    metadb_handle_list items;
    ResolvePathsToHandles(paths, items, r.invalidCount);

    // 清空之后才解析，解析时开过模态进度框，列表可能已被拖动、删掉或上锁。一首都没解析
    // 出来也先找回，被删掉时报删掉而不是报没有曲目。
    const auto located = target.Locate(*this);
    if (!located.has_value()) {
        r.outcome = PathInsertOutcome::PlaylistRemoved;
        return r;
    }
    r.playlist = *located;
    if (items.get_count() == 0) return r;

    if (plm->playlist_insert_items(r.playlist, 0, items, bit_array_false()) == SIZE_MAX) {
        r.outcome = PathInsertOutcome::Refused;
        r.totalCount = plm->playlist_get_item_count(r.playlist);
        return r;
    }
    r.addedCount = items.get_count();
    r.totalCount = plm->playlist_get_item_count(r.playlist);
    return r;
}

// ============================================
// API Registration
// ============================================

// Service injection point for testability
// Thread safety: Set/Get must only be called during single-threaded init
// (component_init / test SetUp) before any handler is invoked.
static Fb2kPlaylistService s_defaultPlaylistService;
static IPlaylistService* s_playlistService = &s_defaultPlaylistService;

void SetPlaylistService(IPlaylistService* service) {
    s_playlistService = service ? service : &s_defaultPlaylistService;
}

IPlaylistService* GetPlaylistService() {
    return s_playlistService;
}

// Playback service injection for replaceAllAndPlay
static Fb2kPlaybackService s_defaultPlaybackServiceLocal;
static IPlaybackService* s_playbackServiceLocal = &s_defaultPlaybackServiceLocal;

void SetPlaylistApiPlaybackService(IPlaybackService* service) {
    s_playbackServiceLocal = service ? service : &s_defaultPlaybackServiceLocal;
}

IPlaybackService* GetPlaylistApiPlaybackService() {
    return s_playbackServiceLocal;
}


// ==========================================================================
// Playlist API handler functions
// ==========================================================================
namespace {

namespace pl = api::playlist;

// 目标播放列表：p 的 playlist 与 playlistGuid 二选一，省略时取活动列表；失败码见 PlaylistTarget.h。
template <class P>
std::optional<api::Failure> ResolvePlaylist(IPlaylistService* svc, const P& p, size_t& index,
                                            const char* invalidMessage = "Invalid playlist index") {
    return api::ResolvePlaylistTarget(*svc, p.playlist, p.playlistGuid, index,
                                      api::PlaylistOmitted::UseActive, invalidMessage);
}

// 按路径加曲目时，路径解析期间目标被删掉或被加锁造成的失败；Ok 时返回 std::nullopt。
// 被删掉时不报序号，原序号现在可能是另一张列表。
std::optional<api::Failure> PathInsertFailure(const IPlaylistService* svc,
                                              IPlaylistService::PathInsertOutcome outcome,
                                              size_t playlist) {
    using enum IPlaylistService::PathInsertOutcome;
    switch (outcome) {
    case Ok:
        return std::nullopt;
    case PlaylistRemoved:
        return api::Fail("The playlist was removed while the paths were being resolved",
                         ApiErrorCode::OPERATION_FAILED);
    case Refused:
        if (svc->playlist_lock_is_present(playlist)) return PlaylistLocked(playlist);
        return api::Fail("Failed to add tracks to the playlist", ApiErrorCode::OPERATION_FAILED);
    }
    return std::nullopt;
}

// 列表不存在时回空结果的查询端，只把参数本身的错误当失败：两者都给、GUID 格式不对。
bool IsParamsFailure(const api::Failure& failure) {
    return failure.code == ApiErrorCode::INVALID_PARAMS;
}

// 失败信封里保留成功时的字段（迁移前这些回包以 success:false 带着同样的键）。
template <class R>
api::Failure FailWithFields(std::string error, const R& fields) {
    return api::Fail(std::move(error), ApiErrorCode::OPERATION_FAILED,
                     ToJson(fields).template get<nlohmann::json::object_t>());
}

// playlist.reorder 与 playlist.reorderPlaylists 的 newOrder：长度必须等于条目数，每项在范围内，
// 每项只出现一次（SDK 的 reorder 要的是排列，重复项的行为没有定义）。长度与越界的报错文字
// 与附带的字段沿用迁移前。
std::optional<api::Failure> ToOrder(const std::vector<std::int64_t>& newOrder, size_t count,
                                    std::vector<size_t>& order) {
    if (newOrder.size() != count) {
        return api::Fail("newOrder length mismatch", ApiErrorCode::INVALID_PARAMS,
                         {{"expected", count}, {"got", newOrder.size()}});
    }
    order.resize(count);
    std::vector<bool> seen(count, false);
    for (size_t i = 0; i < count; i++) {
        if (newOrder[i] < 0 || static_cast<std::uint64_t>(newOrder[i]) >= count) {
            return api::Fail("Index out of range", ApiErrorCode::INVALID_INDEX, {{"index", newOrder[i]}});
        }
        order[i] = static_cast<size_t>(newOrder[i]);
        if (seen[order[i]]) {
            return api::Fail("newOrder lists an entry twice", ApiErrorCode::INVALID_PARAMS,
                             {{"index", newOrder[i]}});
        }
        seen[order[i]] = true;
    }
    return std::nullopt;
}

std::vector<size_t> ReversedOrder(size_t count) {
    std::vector<size_t> order(count);
    for (size_t i = 0; i < count; i++) order[i] = count - 1 - i;
    return order;
}

// SDK：undo / redo 返回 false，是列表上锁或没有还原点。
api::Failure UndoFailure(IPlaylistService* svc, size_t index, const char* nothingMessage) {
    if (svc->playlist_lock_is_present(index)) return PlaylistLocked(index);
    return api::Fail(nothingMessage, ApiErrorCode::NOT_FOUND);
}

// 选中、移除用的行号：负数已由生成的解析器拒绝，这里仍丢掉负数，免得转成 size_t 时回绕；
// 超出末尾的由服务层按行数丢掉。
std::vector<size_t> RowsOf(const std::vector<std::int64_t>& rows) {
    std::vector<size_t> out;
    out.reserve(rows.size());
    for (const std::int64_t row : rows) {
        if (row >= 0) out.push_back(static_cast<size_t>(row));
    }
    return out;
}

// 焦点行号：省略或为负时是 infinite，SDK 约定这表示没有焦点。
size_t FocusRowOf(const std::optional<std::int64_t>& row) {
    return row && *row >= 0 ? static_cast<size_t>(*row) : pfc::infinite_size;
}

std::int64_t FocusRowToJson(size_t focus) {
    return focus == SIZE_MAX ? -1 : static_cast<std::int64_t>(focus);
}

// focusTrack 与 setFocusedTrack 共用。invalidMessage 沿用两者迁移前各自的报错文字。
template <class P>
api::Result<void> SetFocus(const P& p, const std::optional<std::int64_t>& row, const char* invalidMessage) {
    auto* svc = s_playlistService;
    size_t playlistIndex = 0;
    if (auto failure = ResolvePlaylist(svc, p, playlistIndex, invalidMessage)) return std::move(*failure);

    const size_t focus = FocusRowOf(row);
    if (focus != pfc::infinite_size && focus >= svc->playlist_get_item_count(playlistIndex)) {
        return api::Fail("Invalid track index", ApiErrorCode::INVALID_INDEX);
    }
    svc->set_focus_item(playlistIndex, focus);
    return api::Ok();
}

// getActive 与 getPlaying 共用：index 为 SIZE_MAX（没有这样的列表）时 found 为 false。
template <class R>
R DescribePlaylist(size_t index) {
    R result;
    if (index == SIZE_MAX) return result;
    const auto d = s_playlistService->get_playlist_detail(index, true);
    if (!d.found) return result;
    result.found = true;
    result.index = static_cast<std::int64_t>(d.index);
    result.guid = GuidUtils::GuidToString(d.guid);
    result.name = d.name;
    result.trackCount = static_cast<std::int64_t>(d.trackCount);
    result.isActive = d.isActive;
    result.isPlaying = d.isPlaying;
    result.isLocked = d.isLocked;
    result.duration = d.duration;
    return result;
}

// getAutoplaylistInfo 与 getAutoplaylistQuery 共用的自动播放列表状态。
template <class R>
R AutoplaylistStateOf(IPlaylistService* svc, size_t index) {
    R result;
    result.playlist = static_cast<std::int64_t>(index);
    result.playlistGuid = api::PlaylistGuidOf(*svc, index);
    const auto det = svc->detect_autoplaylist(index);
    result.isAutoplaylist = det.isAutoplaylist;
    if (!det.isAutoplaylist) return result;

    const uint32_t flags = det.isDuiAutoplaylist ? 0 : svc->get_autoplaylist_flags(index);
    result.keepSorted = (flags & autoplaylist_flag_sort) != 0;
    result.source = det.isDuiAutoplaylist ? "dui" : "sdk";
    if (!det.lockName.empty()) result.lockName = det.lockName;
    return result;
}


// ========== Playlist Management ==========

api::Result<pl::GetCountResult> PlaylistGetCount(const pl::GetCountParams&) {
    pl::GetCountResult result;
    result.count = static_cast<std::int64_t>(s_playlistService->get_playlist_count());
    return result;
}


api::Result<pl::GetAllResult> PlaylistGetAll(const pl::GetAllParams&) {
    const auto allPlaylists = s_playlistService->get_all_playlists();

    pl::GetAllResult result;
    result.playlists.reserve(allPlaylists.size());
    for (const auto& info : allPlaylists) {
        pl::PlaylistInfo item;
        item.index = static_cast<std::int64_t>(info.index);
        item.guid = GuidUtils::GuidToString(info.guid);
        item.name = info.name;
        item.trackCount = static_cast<std::int64_t>(info.trackCount);
        item.isActive = info.isActive;
        item.isPlaying = info.isPlaying;
        item.isLocked = info.isLocked;
        item.isAutoplaylist = info.isAutoplaylist;
        result.playlists.push_back(std::move(item));
    }
    result.count = static_cast<std::int64_t>(result.playlists.size());
    return result;
}


api::Result<pl::GetActiveResult> PlaylistGetActive(const pl::GetActiveParams&) {
    return DescribePlaylist<pl::GetActiveResult>(s_playlistService->get_active_playlist());
}


api::Result<void> PlaylistSetActive(const pl::SetActiveParams& p) {
    auto* svc = s_playlistService;
    size_t index = 0;
    if (auto failure = api::ResolvePlaylistTarget(*svc, p.playlist, p.playlistGuid, index,
                                                  api::PlaylistOmitted::Refuse)) {
        return std::move(*failure);
    }

    svc->set_active_playlist(index);
    return api::Ok();
}


api::Result<pl::GetPlayingResult> PlaylistGetPlaying(const pl::GetPlayingParams&) {
    return DescribePlaylist<pl::GetPlayingResult>(s_playlistService->get_playing_playlist());
}


api::Result<pl::CreateResult> PlaylistCreate(const pl::CreateParams& p) {
    // 省略 position 时传 infinite：SDK 约定放到末尾
    const size_t position = p.position ? static_cast<size_t>(*p.position) : SIZE_MAX;
    const size_t newIndex = s_playlistService->create_playlist(p.name, position);
    // SDK：返回 infinite 表示创建失败（在不允许的上下文里调用）
    if (newIndex == SIZE_MAX) return api::Fail("Failed to create playlist", ApiErrorCode::OPERATION_FAILED);

    pl::CreateResult result;
    result.index = static_cast<std::int64_t>(newIndex);
    result.guid = api::PlaylistGuidOf(*s_playlistService, newIndex);
    return result;
}


api::Result<void> PlaylistRemove(const pl::RemoveParams& p) {
    auto* svc = s_playlistService;
    size_t index = 0;
    if (auto failure = ResolvePlaylist(svc, p, index)) return std::move(*failure);

    const bool hasLock = svc->playlist_lock_is_present(index);
    if (!svc->remove_playlist(index)) {
        if (hasLock) return PlaylistLocked(index);
        return api::Fail("Failed to remove playlist", ApiErrorCode::OPERATION_FAILED);
    }
    return api::Ok();
}


api::Result<void> PlaylistRename(const pl::RenameParams& p) {
    auto* svc = s_playlistService;
    size_t index = 0;
    if (auto failure = api::ResolvePlaylistTarget(*svc, p.playlist, p.playlistGuid, index,
                                                  api::PlaylistOmitted::Refuse)) {
        return std::move(*failure);
    }

    // SDK：playlist_rename 返回 false 是因为列表上的锁拒绝了新名字
    if (!svc->playlist_rename(index, p.name)) {
        if (svc->playlist_lock_is_present(index)) return PlaylistLocked(index);
        return api::Fail("Failed to rename playlist", ApiErrorCode::OPERATION_FAILED);
    }
    return api::Ok();
}


api::Result<pl::ClearResult> PlaylistClear(const pl::ClearParams& p) {
    auto* svc = s_playlistService;
    size_t index = 0;
    if (auto failure = ResolvePlaylist(svc, p, index)) return std::move(*failure);
    if (svc->playlist_lock_is_present(index)) return PlaylistLocked(index);

    pl::ClearResult result;
    result.playlist = static_cast<std::int64_t>(index);
    result.playlistGuid = api::PlaylistGuidOf(*svc, index);
    result.clearedCount = static_cast<std::int64_t>(svc->playlist_get_item_count(index));
    svc->playlist_undo_backup(index);
    svc->playlist_clear(index);
    result.remainingCount = static_cast<std::int64_t>(svc->playlist_get_item_count(index));
    if (result.remainingCount != 0) return FailWithFields("Playlist was not cleared", result);
    return result;
}


// ========== Track Operations ==========

api::Result<pl::InsertTracksResult> PlaylistInsertTracks(const pl::InsertTracksParams& p) {
    auto* svc = s_playlistService;
    size_t index = 0;
    if (auto failure = ResolvePlaylist(svc, p, index)) return std::move(*failure);
    if (svc->playlist_lock_is_present(index)) return PlaylistLocked(index);

    const json handles = p.handles;
    const auto r = svc->insert_tracks(index, static_cast<size_t>(p.position), handles);
    if (!r.success) {
        return api::Fail(r.error, ApiErrorCode::NOT_FOUND,
                         {{"playlist", index}, {"requestedCount", p.handles.size()}, {"invalidCount", r.invalidCount}});
    }

    // insertIndex 回显请求的位置，不是截到列表末尾之后的位置
    pl::InsertTracksResult result;
    result.playlist = static_cast<std::int64_t>(index);
    result.playlistGuid = api::PlaylistGuidOf(*svc, index);
    result.insertIndex = p.position;
    result.requestedCount = static_cast<std::int64_t>(p.handles.size());
    result.addedCount = static_cast<std::int64_t>(r.addedCount);
    result.invalidCount = static_cast<std::int64_t>(r.invalidCount);
    result.countBefore = static_cast<std::int64_t>(r.countBefore);
    result.totalCount = static_cast<std::int64_t>(r.totalCount);
    return result;
}


api::Result<pl::GetTrackCountResult> PlaylistGetTrackCount(const pl::GetTrackCountParams& p) {
    auto* svc = s_playlistService;
    pl::GetTrackCountResult result;
    // 查询端：没有这个播放列表时照旧报 0，不算失败
    size_t index = 0;
    if (auto failure = ResolvePlaylist(svc, p, index)) {
        if (IsParamsFailure(*failure)) return std::move(*failure);
        return result;
    }
    result.count = static_cast<std::int64_t>(svc->playlist_get_item_count(index));
    return result;
}


api::Result<pl::GetTracksResult> PlaylistGetTracks(const pl::GetTracksParams& p) {
    auto* svc = s_playlistService;
    // fields 校验排在越界检查之前：插在后面会让
    // {playlist: 99, fields: ["bogus"]} 静默出空页而不是报 INVALID_PARAMS。
    const TrackFieldSelection fields = ParseTrackFieldSelection(p.fields);
    if (!fields.valid) {
        return TrackFieldsFailure(fields);
    }

    size_t index = 0;
    if (auto failure = ResolvePlaylist(svc, p, index)) {
        if (IsParamsFailure(*failure)) return std::move(*failure);
        // 查询端：没有这个播放列表时回空页，不当失败
        pl::GetTracksResult empty;
        empty.playlist = p.playlist.value_or(-1);
        empty.start = p.start;
        return empty;
    }
    return svc->get_tracks(index, static_cast<size_t>(p.start), static_cast<size_t>(p.count),
                           p.formats.value_or(std::map<std::string, std::string>{}), fields);
}


// 与 getTracks 同一个查询端：fields 先查，列表不存在回空结果，行形状由同一个构造单元给出。
api::Result<pl::GetTracksAtResult> PlaylistGetTracksAt(const pl::GetTracksAtParams& p) {
    auto* svc = s_playlistService;
    const TrackFieldSelection fields = ParseTrackFieldSelection(p.fields);
    if (!fields.valid) {
        return TrackFieldsFailure(fields);
    }

    size_t index = 0;
    if (auto failure = ResolvePlaylist(svc, p, index)) {
        if (IsParamsFailure(*failure)) return std::move(*failure);
        pl::GetTracksAtResult empty;
        empty.playlist = p.playlist.value_or(-1);
        return empty;
    }
    return svc->get_tracks_at(index, RowsOf(p.rows),
                              p.formats.value_or(std::map<std::string, std::string>{}), fields);
}


// 解析器拒收的查询串：与 library.search / library.query 同一个判据，details.param 让页面把它
// 与别的 INVALID_PARAMS（两者都给、GUID 格式不对）分开。报错文字是宿主语言的原文，不能拿来判断。
api::Failure QuerySyntaxFailure(const char* message) {
    return api::Fail(message && *message ? message : "Invalid query syntax", ApiErrorCode::INVALID_PARAMS,
                     {{"details", json{{"param", "query"}}}});
}


// 在主线程上对整张列表求值，与 library.search 同路：search_filter 没有线程安全说明。
// 不带 KFlagAllowSort，带 SORT BY 的串与语法错一样被 create_ex 拒掉。
api::Result<pl::GetMatchingRowsResult> PlaylistGetMatchingRows(const pl::GetMatchingRowsParams& p) {
    auto* svc = s_playlistService;
    size_t index = 0;
    if (auto failure = ResolvePlaylist(svc, p, index)) return std::move(*failure);

    search_filter_v2::ptr filter;
    try {
        filter = search_filter_manager_v2::get()->create_ex(
            p.query.c_str(), fb2k::service_new<completion_notify_dummy>(),
            search_filter_manager_v2::KFlagSuppressNotify);
    } catch (const std::exception& e) {
        return QuerySyntaxFailure(e.what());
    } catch (...) {
        return QuerySyntaxFailure(nullptr);
    }

    metadb_handle_list items;
    playlist_manager::get()->playlist_get_all_items(index, items);

    pl::GetMatchingRowsResult result;
    result.playlist = static_cast<std::int64_t>(index);
    result.playlistGuid = api::PlaylistGuidOf(*svc, index);
    result.total = static_cast<std::int64_t>(items.get_count());
    if (items.get_count() > 0) {
        pfc::array_t<bool> mask;
        mask.set_size(items.get_count());
        try {
            filter->test_multi(items, mask.get_ptr());
        } catch (const std::exception& e) {
            return api::Fail(e.what(), ApiErrorCode::OPERATION_FAILED);
        }
        for (size_t i = 0; i < items.get_count(); i++) {
            if (mask[i]) result.items.push_back(static_cast<std::int64_t>(i));
        }
    }
    result.count = static_cast<std::int64_t>(result.items.size());
    return result;
}


// ========== getGroupRuns ==========
//
// 按 Title Formatting 模式对整份播放列表做顺序游程，只返回游程边界，
// 不返回整表行数据。返回体大小随游程数量增长；没有固定的
// 游程数量或响应大小上限，调用方应按实际结果分页加载可见行。
// 以 deferred 注册——求键与序列化在 CPU worker 上跑，主线程段只做校验、编译模式、
// 取句柄列表。游程合并、ASCII 折叠与直写是纯值的，在 api/GroupRunPlan.h，测试
// 工程（不链 fb2k SDK）能构建那一半。

// 分批处理以限制单批 metadata 快照的内存占用；批大小由实现常量控制，不能据此
// 推断整次请求的固定内存上限。
constexpr size_t kGroupRunBatch = 2000;

struct GroupRunsRequest {
    size_t playlist = 0;
    std::string playlistGuid;
    size_t total = 0;
    bool twoLevel = false;
    metadb_handle_list items;
    std::vector<titleformat_object::ptr> scripts;
    fb2k_group_runs::GroupRunAccumulator acc;
    size_t nextRow = 0;  // 下一批的起点；批循环是多跳状态机，靠它在跳之间续
};

// worker 段的失败回正常体而不是 SendError（遵循 BridgeCore.h 中 DeferredResponder 注释里的不变量）。
// 形状带上 runs/total，与 library.search 的 MakeQueryWireErrorBody 同一考虑：调用方
// 不必为错误分支单独判空。
json MakeGroupRunsErrorBody(const std::string& message) {
    json body = ApiEnvelope::MakeError(message, ApiErrorCode::OPERATION_FAILED);
    body["runs"] = json::array();
    body["total"] = 0;
    return body;
}

// 跑一批：取这批的 info 记录，再在**本线程**顺序求键并并入游程。抛出的异常由调用方
// 接住、回滚该批并改走主线程。
void ProcessGroupRunBatch(GroupRunsRequest& req, size_t begin, size_t end) {
    const size_t n = end - begin;
    auto mdb = metadb_v2::get();

    // 零拷贝切片：queryMultiParallel_ 吃的是多态接口 metadb_handle_list_cref，
    // 所以切片不复制；每批新建 metadb_handle_list 逐条
    // add_item 会多一轮 n 次 refcount 与每批一次分配。
    pfc::list_partial_ref_t<metadb_handle_ptr> slice(req.items, begin, n);

    // 回调会被多线程并发调用，故预分配 n 槽、按 idx 写对应槽位，全程不扩容、不碰
    // 共享可变态（同 LibraryApi.cpp 中 RunQueryWireWorker 的先例）。
    pfc::array_t<metadb_v2::rec_t> recs;
    recs.resize(n);
    mdb->queryMultiParallel_(slice, [&recs, n](size_t idx, const metadb_v2::rec_t& rec) {
        if (idx < n) recs[idx] = rec;
    });

    // TF 留在本线程顺序跑，**不进并发回调**。两条理由缺一不可：
    // titleformat_object::run 没有任何线程安全说明，模式带
    // $puts/$get 时尤其可疑；而 SDK 对 queryMultiParallel 回调里的异常传播只字未提，从 SDK
    // 线程池的线程抛出接不住就是回包落空。先例同形（LibraryApi.cpp 的 RunQueryWireWorker）。
    for (size_t i = 0; i < n; i++) {
        pfc::string8 primary;
        mdb->formatTitle_v2(req.items[begin + i], recs[i], nullptr, primary, req.scripts[0],
                            nullptr);
        pfc::string8 sub;
        if (req.twoLevel) {
            mdb->formatTitle_v2(req.items[begin + i], recs[i], nullptr, sub, req.scripts[1],
                                nullptr);
        }
        req.acc.Push(begin + i, primary.get_ptr(), req.twoLevel ? sub.get_ptr() : "");
    }
}

void RunGroupRunsWorker(const std::shared_ptr<GroupRunsRequest>& req,
                        const DeferredResponder& responder);

// 主线程只重算失败的批次，再交回 worker 续跑。Title Formatting 模式可能调用
// 要求主线程的第三方 provider；整表重算会把其余批次也放到主线程，延长界面阻塞。
// 本函数要么回包、要么把回包交给续段，两条路径都不能落空。
void RetryGroupRunBatchOnMainThread(const std::shared_ptr<GroupRunsRequest>& req,
                                    const DeferredResponder& responder, size_t begin,
                                    size_t end) {
    fb2k::inMainThread([req, responder, begin, end]() {
        try {
            ProcessGroupRunBatch(*req, begin, end);
            req->nextRow = end;
            // 当前批次重试成功后续跑剩余批次，不能提前返回不完整结果。
            fb2k::inCpuWorkerThread([req, responder]() { RunGroupRunsWorker(req, responder); });
        } catch (const std::exception& e) {
            responder.SendJson(MakeGroupRunsErrorBody(e.what()));
        } catch (...) {
            responder.SendJson(MakeGroupRunsErrorBody("getGroupRuns failed"));
        }
    });
}

void RunGroupRunsWorker(const std::shared_ptr<GroupRunsRequest>& req,
                        const DeferredResponder& responder) {
    try {
        while (req->nextRow < req->total) {
            const size_t begin = req->nextRow;
            const size_t end = std::min(begin + kGroupRunBatch, req->total);

            // 批边界快照：游程合并是有状态的顺序累加，兜底重算前不回滚
            // 的话，已并入的行会被二次合并，各游程 count 之和就不再等于 total。
            const fb2k_group_runs::GroupRunAccumulator::Snapshot snapshot = req->acc.Capture();
            try {
                ProcessGroupRunBatch(*req, begin, end);
            } catch (...) {
                req->acc.Restore(snapshot);
                console::printf("[Playlist] getGroupRuns batch [%u,%u) failed on worker, "
                                "retrying it on main thread",
                                static_cast<unsigned>(begin), static_cast<unsigned>(end));
                RetryGroupRunBatchOnMainThread(req, responder, begin, end);
                return;  // 回包交给续段，本段到此为止
            }
            req->nextRow = end;
        }

        // 只有最后一批结束才回包（恰好一次回包要在每一跳上都成立）
        req->acc.Finish();
        std::string out;
        fb2k_group_runs::WriteGroupRunsJson(out, req->playlist, req->playlistGuid, req->total, req->acc);
        responder.SendRaw(std::move(out));
    } catch (const std::exception& e) {
        responder.SendJson(MakeGroupRunsErrorBody(e.what()));
    } catch (...) {
        responder.SendJson(MakeGroupRunsErrorBody("getGroupRuns failed"));
    }
}

void PlaylistGetGroupRuns(const pl::GetGroupRunsParams& p, const DeferredResponder& responder) {
    try {
        auto* svc = s_playlistService;

        // 形状已由声明的读取器查过；个数与空串的判定、消息与 details.pattern 仍归
        // ParseGroupRunPatterns。
        const fb2k_group_runs::PatternSelection selection =
            fb2k_group_runs::ParseGroupRunPatterns(json{{"patterns", p.patterns}});
        if (!selection.valid) {
            if (selection.errorIndex == fb2k_group_runs::kNoPatternIndex) {
                responder.SendJson(
                    ApiEnvelope::MakeError(selection.errorMessage, ApiErrorCode::INVALID_PARAMS));
            } else {
                responder.SendJson(ApiEnvelope::MakeError(selection.errorMessage,
                                                          ApiErrorCode::INVALID_PARAMS,
                                                          {{"pattern", selection.errorIndex}}));
            }
            return;
        }

        size_t playlistIndex = 0;
        if (auto failure = ResolvePlaylist(svc, p, playlistIndex)) {
            // 越界时照旧在 details.playlist 带回请求的索引
            if (p.playlist) failure->extra["details"] = json{{"playlist", *p.playlist}};
            responder.SendJson(api::results::FailureToJson(*failure));
            return;
        }

        // 用 compile 不用 compile_safe：后者的 SDK 注释是「Should never fail,
        // falls back to %filename% in case of failure」，拿它编译
        // 用户串会让「编译失败返回 INVALID_PARAMS」永不触发——patterns: ["%album"]
        // 会静默回退成 %filename%，每行一组还返回 success:true。
        auto compiler = titleformat_compiler::get();
        auto req = std::make_shared<GroupRunsRequest>();
        for (size_t i = 0; i < selection.patterns.size(); i++) {
            titleformat_object::ptr script;
            if (!compiler->compile(script, selection.patterns[i].c_str())) {
                responder.SendJson(ApiEnvelope::MakeError("patterns failed to compile",
                                                          ApiErrorCode::INVALID_PARAMS,
                                                          {{"pattern", i}}));
                return;
            }
            req->scripts.push_back(script);
        }

        req->playlist = playlistIndex;
        // 在主线程上取：回包在 worker 上写出，那时序号可能已指向别的列表，GUID 不会。
        req->playlistGuid = api::PlaylistGuidOf(*svc, playlistIndex);
        req->twoLevel = selection.patterns.size() == fb2k_group_runs::kMaxPatterns;
        req->acc = fb2k_group_runs::GroupRunAccumulator(req->twoLevel);
        playlist_manager::get()->playlist_get_all_items(playlistIndex, req->items);
        req->total = req->items.get_count();

        // 空列表就地回包，不为零行付线程跳变的税
        if (req->total == 0) {
            pl::GetGroupRunsResult empty;
            empty.playlist = static_cast<std::int64_t>(playlistIndex);
            empty.playlistGuid = req->playlistGuid;
            api::Send(responder, api::Result<pl::GetGroupRunsResult>(std::move(empty)));
            return;
        }

        fb2k::inCpuWorkerThread([req, responder]() { RunGroupRunsWorker(req, responder); });
    } catch (const std::exception& e) {
        responder.SendJson(MakeGroupRunsErrorBody(e.what()));
    } catch (...) {
        responder.SendJson(MakeGroupRunsErrorBody("getGroupRuns failed"));
    }
}


api::Result<pl::GetSelectedTracksResult> PlaylistGetSelectedTracks(
    const pl::GetSelectedTracksParams& p) {
    auto* svc = s_playlistService;
    size_t index = 0;
    if (auto failure = ResolvePlaylist(svc, p, index)) {
        // 失败照旧带空的 tracks
        failure->extra["tracks"] = json::array();
        return std::move(*failure);
    }
    return svc->get_selected_tracks(index);
}


api::Result<void> PlaylistSetSelection(const pl::SetSelectionParams& p) {
    auto* svc = s_playlistService;
    size_t index = 0;
    if (auto failure = ResolvePlaylist(svc, p, index)) return std::move(*failure);

    svc->set_selection(index, RowsOf(p.indices), p.clearOthers);
    return api::Ok();
}


api::Result<void> PlaylistRemoveTracks(const pl::RemoveTracksParams& p) {
    auto* svc = s_playlistService;
    size_t index = 0;
    if (auto failure = ResolvePlaylist(svc, p, index)) return std::move(*failure);
    if (svc->playlist_lock_is_present(index)) return PlaylistLocked(index);

    svc->remove_tracks(index, RowsOf(p.items));
    return api::Ok();
}


api::Result<void> PlaylistRemoveSelectedTracks(const pl::RemoveSelectedTracksParams& p) {
    auto* svc = s_playlistService;
    size_t index = 0;
    if (auto failure = ResolvePlaylist(svc, p, index)) return std::move(*failure);
    if (svc->playlist_lock_is_present(index)) return PlaylistLocked(index);

    svc->remove_selection(index);
    return api::Ok();
}


api::Result<void> PlaylistMoveTracks(const pl::MoveTracksParams& p) {
    auto* svc = s_playlistService;
    size_t index = 0;
    if (auto failure = ResolvePlaylist(svc, p, index)) return std::move(*failure);
    if (svc->playlist_lock_is_present(index)) return PlaylistLocked(index);

    svc->playlist_undo_backup(index);

    // 移动的是选中：给了 items 就先把选中换成它们
    if (p.items && !p.items->empty()) {
        svc->set_selection(index, RowsOf(*p.items), true);
    }
    const auto delta = std::clamp<std::int64_t>(p.delta, std::numeric_limits<int>::min(),
                                                std::numeric_limits<int>::max());
    svc->move_selection(index, static_cast<int>(delta));
    return api::Ok();
}


api::Result<void> PlaylistPlayTrack(const pl::PlayTrackParams& p) {
    auto* svc = s_playlistService;
    size_t playlistIndex = 0;
    if (auto failure = ResolvePlaylist(svc, p, playlistIndex)) return std::move(*failure);

    const auto trackIndex = static_cast<size_t>(p.index);
    if (trackIndex >= svc->playlist_get_item_count(playlistIndex)) {
        return api::Fail("Invalid track index", ApiErrorCode::INVALID_INDEX);
    }

    // 可选：播放前先静音，消除两次 IPC round-trip 之间的音频缓冲间隙
    // muted 依赖 playback_control SDK，不可通过 playlist service 抽象
    if (p.muted) {
        auto pc = playback_control::get();
        if (!pc->is_muted()) pc->volume_mute_toggle();
    }

    if (p.deferred) {
        // 延迟执行：让当前消息循环完成后再播放。deferred 路径依赖 fb2k::inMainThread SDK，
        // 不可通过 service 抽象。排队期间别的消息可能增删、挪动列表，捕获的序号那时可能已是
        // 另一张列表：钉住 GUID，执行时找回；列表没了或行数不够就不播。
        const api::PinnedPlaylist target(*svc, playlistIndex);
        fb2k::inMainThread([target, trackIndex]() {
            auto* live = s_playlistService;
            const auto located = target.Locate(*live);
            if (!located.has_value() || trackIndex >= live->playlist_get_item_count(*located)) {
                console::print("[Playlist] playTrack (deferred): the playlist was removed or "
                               "shortened before playback started; nothing played");
                return;
            }
            playlist_manager::get()->playlist_execute_default_action(*located, trackIndex);
        });
    } else {
        // 立即执行 — 走 service 接口
        svc->execute_default_action(playlistIndex, trackIndex);
    }
    return api::Ok();
}


// [DEPRECATED] 改用 playlist.setFocusedTrack；保留仅为兼容旧调用。
api::Result<void> PlaylistFocusTrack(const pl::FocusTrackParams& p) {
    FB2K_console_print("[DEPRECATED] playlist.focusTrack is deprecated, use playlist.setFocusedTrack");
    return SetFocus(p, p.index, "Invalid playlist");
}


// [DEPRECATED] 改用 playlist.getFocusedTrack；保留仅为兼容旧调用。
api::Result<pl::GetFocusTrackResult> PlaylistGetFocusTrack(const pl::GetFocusTrackParams& p) {
    FB2K_console_print("[DEPRECATED] playlist.getFocusTrack is deprecated, use playlist.getFocusedTrack");
    auto* svc = s_playlistService;
    size_t index = 0;
    if (auto failure = ResolvePlaylist(svc, p, index, "Invalid playlist")) return std::move(*failure);

    pl::GetFocusTrackResult result;
    result.playlist = static_cast<std::int64_t>(index);
    result.playlistGuid = api::PlaylistGuidOf(*svc, index);
    result.index = FocusRowToJson(svc->get_focus_item(index));
    return result;
}


api::Result<void> PlaylistSort(const pl::SortParams& p) {
    auto* svc = s_playlistService;
    size_t index = 0;
    if (auto failure = ResolvePlaylist(svc, p, index)) return std::move(*failure);
    if (svc->playlist_lock_is_present(index)) return PlaylistLocked(index);

    svc->playlist_undo_backup(index);
    svc->sort_by_format(index, p.pattern.c_str(), p.selectedOnly);

    // descending 把整张表倒过来，selectedOnly 时未选中的曲目也在内
    if (p.descending) {
        const size_t count = svc->playlist_get_item_count(index);
        const auto order = ReversedOrder(count);
        svc->reorder_items(index, order.data(), count);
    }
    return api::Ok();
}


api::Result<void> PlaylistShuffle(const pl::ShuffleParams& p) {
    auto* svc = s_playlistService;
    size_t index = 0;
    if (auto failure = ResolvePlaylist(svc, p, index)) return std::move(*failure);
    if (svc->playlist_lock_is_present(index)) return PlaylistLocked(index);

    const size_t count = svc->playlist_get_item_count(index);
    if (count <= 1) return api::Ok();

    std::vector<size_t> order(count);
    for (size_t i = 0; i < count; i++) order[i] = i;

    // Fisher-Yates shuffle
    {
        std::mt19937 rng(std::random_device{}());
        for (size_t i = count - 1; i > 0; i--) {
            std::uniform_int_distribution<size_t> dist(0, i);
            size_t j = dist(rng);
            std::swap(order[i], order[j]);
        }
    }

    svc->playlist_undo_backup(index);
    svc->reorder_items(index, order.data(), count);
    return api::Ok();
}


api::Result<void> PlaylistUndo(const pl::UndoParams& p) {
    auto* svc = s_playlistService;
    size_t index = 0;
    if (auto failure = ResolvePlaylist(svc, p, index)) return std::move(*failure);

    if (!svc->undo_restore(index)) return UndoFailure(svc, index, "Nothing to undo");
    return api::Ok();
}


api::Result<void> PlaylistRedo(const pl::RedoParams& p) {
    auto* svc = s_playlistService;
    size_t index = 0;
    if (auto failure = ResolvePlaylist(svc, p, index)) return std::move(*failure);

    if (!svc->redo_restore(index)) return UndoFailure(svc, index, "Nothing to redo");
    return api::Ok();
}


// ========== Autoplaylist (Smart Playlist) APIs ==========

// Check if a playlist is an autoplaylist
api::Result<pl::IsAutoplaylistResult> PlaylistIsAutoplaylist(const pl::IsAutoplaylistParams& p) {
    auto* svc = s_playlistService;
    size_t index = 0;
    if (auto failure = ResolvePlaylist(svc, p, index)) return std::move(*failure);

    const auto det = svc->detect_autoplaylist(index);
    pl::IsAutoplaylistResult result;
    result.playlist = static_cast<std::int64_t>(index);
    result.playlistGuid = api::PlaylistGuidOf(*svc, index);
    result.isAutoplaylist = det.isAutoplaylist;
    if (!det.lockName.empty()) result.lockName = det.lockName;
    return result;
}


// Create a simple autoplaylist (smart playlist)
api::Result<pl::CreateAutoplaylistResult> PlaylistCreateAutoplaylist(const pl::CreateAutoplaylistParams& p) {
    auto* svc = s_playlistService;
    const size_t newIndex = svc->create_playlist(p.name, SIZE_MAX);
    if (newIndex == SIZE_MAX) return api::Fail("Failed to create playlist", ApiErrorCode::OPERATION_FAILED);

    // SDK：add_client_simple 失败时抛 exception_autoplaylist；建好的空列表随之删掉
    try {
        const uint32_t flags = p.keepSorted ? autoplaylist_flag_sort : 0;
        svc->add_autoplaylist_client(newIndex, p.query.c_str(), p.sort.c_str(), flags);
    } catch (const std::exception& e) {
        svc->remove_playlist(newIndex);
        return api::Fail(std::string("Failed to create autoplaylist: ") + e.what(), ApiErrorCode::OPERATION_FAILED);
    }

    pl::CreateAutoplaylistResult result;
    result.index = static_cast<std::int64_t>(newIndex);
    result.playlist = result.index;
    result.guid = api::PlaylistGuidOf(*svc, newIndex);
    result.name = p.name;
    result.query = p.query;
    return result;
}


// Convert an existing playlist to autoplaylist
api::Result<pl::ConvertToAutoplaylistResult> PlaylistConvertToAutoplaylist(const pl::ConvertToAutoplaylistParams& p) {
    auto* svc = s_playlistService;
    size_t index = 0;
    if (auto failure = ResolvePlaylist(svc, p, index)) return std::move(*failure);

    // SDK：已是自动播放列表、上锁失败等都以 exception_autoplaylist 抛出
    try {
        const uint32_t flags = p.keepSorted ? autoplaylist_flag_sort : 0;
        svc->add_autoplaylist_client(index, p.query.c_str(), p.sort.c_str(), flags);
    } catch (const std::exception& e) {
        return api::Fail(std::string("Failed to convert: ") + e.what(), ApiErrorCode::OPERATION_FAILED);
    }

    pl::ConvertToAutoplaylistResult result;
    result.playlist = static_cast<std::int64_t>(index);
    result.playlistGuid = api::PlaylistGuidOf(*svc, index);
    return result;
}


// Remove autoplaylist status (convert back to normal playlist)
api::Result<pl::RemoveAutoplaylistResult> PlaylistRemoveAutoplaylist(const pl::RemoveAutoplaylistParams& p) {
    auto* svc = s_playlistService;
    size_t index = 0;
    if (auto failure = ResolvePlaylist(svc, p, index)) return std::move(*failure);

    pl::RemoveAutoplaylistResult result;
    result.playlist = static_cast<std::int64_t>(index);
    result.playlistGuid = api::PlaylistGuidOf(*svc, index);

    if (svc->autoplaylist_is_client_present(index)) {
        try {
            svc->remove_autoplaylist_client(index);
        } catch (const std::exception& e) {
            return api::Fail(std::string("Failed to remove autoplaylist: ") + e.what(), ApiErrorCode::OPERATION_FAILED);
        }
        result.source = "sdk";
        return result;
    }

    // 其他组件管理的自动播放列表只有它的锁，这里解不开，只如实报告
    const auto det = svc->detect_autoplaylist(index);
    if (det.isDuiAutoplaylist) {
        result.source = "dui";
        result.note = "DUI autoplaylist lock detected; proceed with playlist.remove to delete playlist";
        return result;
    }

    return api::Fail("Not an autoplaylist", ApiErrorCode::NOT_FOUND);
}


// Get autoplaylist info (flags, query if available)
api::Result<pl::GetAutoplaylistInfoResult> PlaylistGetAutoplaylistInfo(const pl::GetAutoplaylistInfoParams& p) {
    auto* svc = s_playlistService;
    size_t index = 0;
    if (auto failure = ResolvePlaylist(svc, p, index)) return std::move(*failure);

    return AutoplaylistStateOf<pl::GetAutoplaylistInfoResult>(svc, index);
}


// Get autoplaylist query string
api::Result<pl::GetAutoplaylistQueryResult> PlaylistGetAutoplaylistQuery(const pl::GetAutoplaylistQueryParams& p) {
    auto* svc = s_playlistService;
    size_t index = 0;
    if (auto failure = ResolvePlaylist(svc, p, index)) return std::move(*failure);

    // query 恒为 null：SDK 不公开自动播放列表的查询语句
    auto result = AutoplaylistStateOf<pl::GetAutoplaylistQueryResult>(svc, index);
    if (result.isAutoplaylist) result.note = "Query string not exposed by SDK";
    return result;
}


// Duplicate playlist
api::Result<pl::DuplicateResult> PlaylistDuplicate(const pl::DuplicateParams& p) {
    auto* svc = s_playlistService;
    size_t index = 0;
    if (auto failure = ResolvePlaylist(svc, p, index)) return std::move(*failure);

    // Default name: "Original (Copy)"
    std::string newName = p.name.value_or("");
    if (newName.empty()) {
        std::string origName;
        svc->playlist_get_name(index, origName);
        newName = origName + " (Copy)";
    }

    const auto dup = svc->duplicate_playlist(index, newName);
    if (!dup.success) return api::Fail(dup.error, ApiErrorCode::OPERATION_FAILED);

    pl::DuplicateResult result;
    result.index = static_cast<std::int64_t>(dup.newIndex);
    result.sourcePlaylist = static_cast<std::int64_t>(dup.sourceIndex);
    result.sourcePlaylistGuid = api::PlaylistGuidOf(*svc, dup.sourceIndex);
    result.newPlaylist = result.index;
    result.guid = api::PlaylistGuidOf(*svc, dup.newIndex);
    result.name = dup.name;
    result.trackCount = static_cast<std::int64_t>(dup.trackCount);
    return result;
}


// Add tracks from file/folder paths
api::Result<pl::AddPathsResult> PlaylistAddPaths(const pl::AddPathsParams& p) {
    auto* svc = s_playlistService;
    size_t index = 0;
    if (auto failure = ResolvePlaylist(svc, p, index)) return std::move(*failure);
    if (svc->playlist_lock_is_present(index)) return PlaylistLocked(index);

    const auto r = svc->add_paths(index, json(p.paths));
    if (auto failure = PathInsertFailure(svc, r.outcome, r.playlist)) return std::move(*failure);
    if (r.addedCount == 0) {
        return api::Fail("No valid tracks found", ApiErrorCode::NOT_FOUND,
                         {{"playlist", r.playlist},
                          {"requestedPaths", p.paths.size()},
                          {"invalidCount", r.invalidCount},
                          {"countBefore", r.countBefore}});
    }

    pl::AddPathsResult result;
    result.playlist = static_cast<std::int64_t>(r.playlist);
    result.playlistGuid = api::PlaylistGuidOf(*svc, r.playlist);
    result.requestedPaths = static_cast<std::int64_t>(p.paths.size());
    result.addedCount = static_cast<std::int64_t>(r.addedCount);
    result.invalidCount = static_cast<std::int64_t>(r.invalidCount);
    result.countBefore = static_cast<std::int64_t>(r.countBefore);
    result.totalCount = static_cast<std::int64_t>(r.totalCount);
    return result;
}


// Add tracks with explicit control (no automatic CUE expansion)
// Each handle specifies path and optional subsong index
api::Result<pl::AddHandlesResult> PlaylistAddHandles(const pl::AddHandlesParams& p) {
    auto* svc = s_playlistService;
    size_t index = 0;
    if (auto failure = ResolvePlaylist(svc, p, index)) return std::move(*failure);
    if (svc->playlist_lock_is_present(index)) return PlaylistLocked(index);

    const auto r = svc->add_handles(index, json(p.handles));
    if (r.addedCount == 0) {
        return api::Fail("No valid handles created", ApiErrorCode::NOT_FOUND,
                         {{"playlist", index}, {"requestedCount", p.handles.size()}, {"invalidCount", r.invalidCount}});
    }

    pl::AddHandlesResult result;
    result.playlist = static_cast<std::int64_t>(index);
    result.playlistGuid = api::PlaylistGuidOf(*svc, index);
    result.requestedCount = static_cast<std::int64_t>(p.handles.size());
    result.addedCount = static_cast<std::int64_t>(r.addedCount);
    result.invalidCount = static_cast<std::int64_t>(r.invalidCount);
    result.countBefore = static_cast<std::int64_t>(r.countBefore);
    result.totalCount = static_cast<std::int64_t>(r.totalCount);
    return result;
}


// Get playlist lock info
api::Result<pl::GetLockInfoResult> PlaylistGetLockInfo(const pl::GetLockInfoParams& p) {
    auto* svc = s_playlistService;
    size_t index = 0;
    if (auto failure = ResolvePlaylist(svc, p, index)) return std::move(*failure);

    pl::GetLockInfoResult result;
    result.playlist = static_cast<std::int64_t>(index);
    result.playlistGuid = api::PlaylistGuidOf(*svc, index);
    result.isLocked = svc->playlist_lock_is_present(index);
    return result;
}


// Alias: isLocked (returns just the boolean)
api::Result<pl::IsLockedResult> PlaylistIsLocked(const pl::IsLockedParams& p) {
    auto* svc = s_playlistService;
    size_t index = 0;
    // 失败也带 isLocked:false，迁移前就是这样
    if (auto failure = ResolvePlaylist(svc, p, index, "Invalid playlist")) {
        failure->extra["isLocked"] = false;
        return std::move(*failure);
    }

    pl::IsLockedResult result;
    result.isLocked = svc->playlist_lock_is_present(index);
    return result;
}


// Selection APIs
api::Result<pl::GetSelectionResult> PlaylistGetSelection(const pl::GetSelectionParams& p) {
    auto* svc = s_playlistService;
    size_t index = 0;
    if (auto failure = ResolvePlaylist(svc, p, index, "Invalid playlist")) return std::move(*failure);

    pl::GetSelectionResult result;
    for (const size_t row : svc->get_selection_indices(index)) {
        result.items.push_back(static_cast<std::int64_t>(row));
    }
    result.count = static_cast<std::int64_t>(result.items.size());
    result.playlist = static_cast<std::int64_t>(index);
    result.playlistGuid = api::PlaylistGuidOf(*svc, index);
    return result;
}


api::Result<void> PlaylistSelectAll(const pl::SelectAllParams& p) {
    auto* svc = s_playlistService;
    size_t index = 0;
    if (auto failure = ResolvePlaylist(svc, p, index)) return std::move(*failure);

    svc->select_all(index);
    return api::Ok();
}


api::Result<void> PlaylistDeselectAll(const pl::DeselectAllParams& p) {
    auto* svc = s_playlistService;
    size_t index = 0;
    if (auto failure = ResolvePlaylist(svc, p, index)) return std::move(*failure);

    svc->deselect_all(index);
    return api::Ok();
}


api::Result<pl::GetFocusedTrackResult> PlaylistGetFocusedTrack(const pl::GetFocusedTrackParams& p) {
    auto* svc = s_playlistService;
    pl::GetFocusedTrackResult result;
    result.index = -1;
    // 查询端：没有这个播放列表时照旧报 index -1、不带 playlist，不算失败
    size_t index = 0;
    if (auto failure = ResolvePlaylist(svc, p, index)) {
        if (IsParamsFailure(*failure)) return std::move(*failure);
        return result;
    }

    result.playlist = static_cast<std::int64_t>(index);
    result.playlistGuid = api::PlaylistGuidOf(*svc, index);
    result.index = FocusRowToJson(svc->get_focus_item(index));
    return result;
}


api::Result<void> PlaylistSetFocusedTrack(const pl::SetFocusedTrackParams& p) {
    return SetFocus(p, p.index, "Invalid playlist index");
}


api::Result<void> PlaylistReverse(const pl::ReverseParams& p) {
    auto* svc = s_playlistService;
    size_t index = 0;
    if (auto failure = ResolvePlaylist(svc, p, index)) return std::move(*failure);
    if (svc->playlist_lock_is_present(index)) return PlaylistLocked(index);

    const size_t count = svc->playlist_get_item_count(index);
    if (count < 2) return api::Ok();

    const auto order = ReversedOrder(count);
    svc->playlist_undo_backup(index);
    svc->reorder_items(index, order.data(), count);
    return api::Ok();
}


// ========== playlist.reorder
api::Result<pl::ReorderResult> PlaylistReorder(const pl::ReorderParams& p) {
    auto* svc = s_playlistService;
    size_t index = 0;
    if (auto failure = ResolvePlaylist(svc, p, index)) return std::move(*failure);
    if (svc->playlist_lock_is_present(index)) return PlaylistLocked(index);

    const size_t count = svc->playlist_get_item_count(index);
    std::vector<size_t> order;
    if (auto failure = ToOrder(p.newOrder, count, order)) return std::move(*failure);

    svc->playlist_undo_backup(index);
    svc->reorder_items(index, order.data(), count);

    pl::ReorderResult result;
    result.playlist = static_cast<std::int64_t>(index);
    result.playlistGuid = api::PlaylistGuidOf(*svc, index);
    result.itemCount = static_cast<std::int64_t>(count);
    return result;
}


// ========== playlist.addPathsSequential - sequential path add ==========
api::Result<pl::AddPathsSequentialResult> PlaylistAddPathsSequential(const pl::AddPathsSequentialParams& p) {
    auto* svc = s_playlistService;
    size_t index = 0;
    if (auto failure = ResolvePlaylist(svc, p, index)) return std::move(*failure);
    if (svc->playlist_lock_is_present(index)) return PlaylistLocked(index);

    const auto r = svc->add_paths_sequential(index, json(p.paths));
    if (auto failure = PathInsertFailure(svc, r.outcome, r.playlist)) return std::move(*failure);

    pl::AddPathsSequentialResult result;
    result.playlist = static_cast<std::int64_t>(r.playlist);
    result.playlistGuid = api::PlaylistGuidOf(*svc, r.playlist);
    result.addedCount = static_cast<std::int64_t>(r.addedCount);
    for (const auto& row : r.order) {
        result.order.push_back(row.get<std::int64_t>());
    }
    return result;
}


// ========== playlist.addPathsAsync - async path add ==========
api::Result<pl::AddPathsAsyncResult> PlaylistAddPathsAsync(const pl::AddPathsAsyncParams& p) {
    auto* svc = s_playlistService;
    size_t index = 0;
    if (auto failure = ResolvePlaylist(svc, p, index)) return std::move(*failure);
    if (svc->playlist_lock_is_present(index)) return PlaylistLocked(index);

    std::string operationId = GenerateOperationId();
    // 在解析出目标的这一刻取 GUID：完成事件可能在列表挪动或删掉之后才发，那时序号已不可信。
    std::string playlistGuid = api::PlaylistGuidOf(*svc, index);

    auto info = svc->start_add_paths_async(index, json(p.paths), operationId,
        [operationId, playlistGuid](size_t addedCount, size_t totalCount) {
            fb2k::inMainThread([opId = operationId, guid = playlistGuid, addedCount, totalCount]() {
                pl::AddCompletePayload payload;
                payload.operationId = opId;
                payload.playlistGuid = guid;
                payload.success = true;
                payload.addedCount = static_cast<std::int64_t>(addedCount);
                payload.totalCount = static_cast<std::int64_t>(totalCount);
                api::emit::Broadcast<pl::events::AddComplete>(payload);
            });
        }
    );

    // 一条可用的都没有时服务层直接返回，不调 onComplete，也就没有完成事件
    if (info.validPathCount == 0) {
        return api::Fail("No valid paths specified", ApiErrorCode::INVALID_PARAMS,
                         {{"invalidCount", info.invalidCount}});
    }

    pl::AddPathsAsyncResult result;
    result.operationId = std::move(operationId);
    result.playlistGuid = std::move(playlistGuid);
    result.status = "pending";
    result.totalCount = static_cast<std::int64_t>(info.validPathCount);
    result.invalidCount = static_cast<std::int64_t>(info.invalidCount);
    return result;
}


// ========== playlist.replaceAllAndPlay - atomic clear+add+play ==========
api::Result<pl::ReplaceAllAndPlayResult> PlaylistReplaceAllAndPlay(const pl::ReplaceAllAndPlayParams& p) {
    auto* svc = s_playlistService;
    auto* pbSvc = s_playbackServiceLocal;
    size_t playlistIndex = 0;
    if (auto failure = ResolvePlaylist(svc, p, playlistIndex)) return std::move(*failure);
    if (svc->playlist_lock_is_present(playlistIndex)) return PlaylistLocked(playlistIndex);

    // Step 1: stop playback
    if (p.stopFirst && pbSvc->is_playing()) {
        pbSvc->stop();
    }

    // Step 2-4: clear + add；一首都没加上时列表已经清空
    const auto r = svc->replace_all(playlistIndex, json(p.paths));
    if (auto failure = PathInsertFailure(svc, r.outcome, r.playlist)) return std::move(*failure);
    if (r.addedCount == 0) {
        return api::Fail("No valid tracks found", ApiErrorCode::NOT_FOUND,
                         {{"clearedCount", r.clearedCount}, {"invalidCount", r.invalidCount}});
    }

    // Step 5: activate + play；路径解析期间列表可能被拖动，之后都用写入时的序号
    playlistIndex = r.playlist;
    svc->set_active_playlist(playlistIndex);
    size_t playIndex = static_cast<size_t>(p.playIndex);
    if (playIndex >= r.totalCount) {
        playIndex = 0;
    }
    if (p.autoPlay) {
        svc->execute_default_action(playlistIndex, playIndex);
    } else {
        svc->set_focus_item(playlistIndex, playIndex);
    }

    pl::ReplaceAllAndPlayResult result;
    result.playlist = static_cast<std::int64_t>(playlistIndex);
    result.playlistGuid = api::PlaylistGuidOf(*svc, playlistIndex);
    result.clearedCount = static_cast<std::int64_t>(r.clearedCount);
    result.addedCount = static_cast<std::int64_t>(r.addedCount);
    result.totalCount = static_cast<std::int64_t>(r.totalCount);
    result.playIndex = static_cast<std::int64_t>(playIndex);
    return result;
}


// ========== playlist.reorderPlaylists — 重排播放列表顺序 ==========
api::Result<pl::ReorderPlaylistsResult> PlaylistReorderPlaylists(const pl::ReorderPlaylistsParams& p) {
    auto* svc = s_playlistService;
    const size_t count = svc->get_playlist_count();
    if (p.newOrder.has_value() == p.newOrderGuids.has_value()) {
        return api::Fail(p.newOrder ? "Give newOrder or newOrderGuids, not both"
                                    : "newOrder or newOrderGuids is required",
                         ApiErrorCode::INVALID_PARAMS);
    }
    std::vector<size_t> order;
    if (p.newOrder) {
        if (auto failure = ToOrder(*p.newOrder, count, order)) return std::move(*failure);
    } else if (auto failure = api::ResolvePlaylistGuidOrder(*svc, *p.newOrderGuids, order)) {
        return std::move(*failure);
    }

    pl::ReorderPlaylistsResult result;
    result.count = static_cast<std::int64_t>(count);
    // SDK：返回 false 表示在不允许的上下文里调用
    if (!svc->reorder_playlists(order.data(), count)) return FailWithFields("Failed to reorder playlists", result);
    return result;
}


// ========== playlist.getAvailableColumns — 获取 DUI 可用列定义 ==========
api::Result<pl::GetAvailableColumnsResult> PlaylistGetAvailableColumns(const pl::GetAvailableColumnsParams&) {
    pl::GetAvailableColumnsResult result;

    for (auto provider : fb2k::playlistColumnProvider::enumerate()) {
        size_t numCols = provider->numColumns();
        for (size_t i = 0; i < numCols; i++) {
            auto id = provider->columnID(i);
            auto flags = provider->columnFlags(i);

            pl::PlaylistColumnDefinition col;
            col.id = pfc::print_guid(id).c_str();
            col.name = provider->columnName(i)->c_str();
            col.pattern = provider->columnFormatSpec(i)->c_str();
            col.alignment = "left";
            if (flags & fb2k::playlistColumnProvider::flag_alignRight) col.alignment = "right";
            else if (flags & fb2k::playlistColumnProvider::flag_alignCenter) col.alignment = "center";
            col.numeric = (flags & fb2k::playlistColumnProvider::flag_numeric) != 0;

            auto sortScript = provider->columnSortScript(i);
            if (sortScript.is_valid() && sortScript->length() > 0) {
                col.sortPattern = sortScript->c_str();
            }

            result.columns.push_back(std::move(col));
        }
    }

    result.count = static_cast<std::int64_t>(result.columns.size());
    return result;
}

} // namespace

void RegisterPlaylistApi() {
    api::RegisterApi("playlist.getCount", PlaylistGetCount);
    api::RegisterApi("playlist.getAll", PlaylistGetAll);
    api::RegisterApi("playlist.getActive", PlaylistGetActive);
    api::RegisterApi("playlist.setActive", PlaylistSetActive);
    api::RegisterApi("playlist.getPlaying", PlaylistGetPlaying);
    api::RegisterApi("playlist.create", PlaylistCreate);
    api::RegisterApi("playlist.remove", PlaylistRemove);
    api::RegisterApi("playlist.rename", PlaylistRename);
    api::RegisterApi("playlist.clear", PlaylistClear);
    api::RegisterApi("playlist.insertTracks", PlaylistInsertTracks);
    api::RegisterApi("playlist.getTrackCount", PlaylistGetTrackCount);
    api::RegisterApi("playlist.getTracks", PlaylistGetTracks);
    api::RegisterApi("playlist.getTracksAt", PlaylistGetTracksAt);
    api::RegisterApi("playlist.getMatchingRows", PlaylistGetMatchingRows);
    api::RegisterApi("playlist.getSelectedTracks", PlaylistGetSelectedTracks);
    api::RegisterApi("playlist.setSelection", PlaylistSetSelection);
    api::RegisterApi("playlist.removeTracks", PlaylistRemoveTracks);
    api::RegisterApi("playlist.removeSelectedTracks", PlaylistRemoveSelectedTracks);
    api::RegisterApi("playlist.moveTracks", PlaylistMoveTracks);
    api::RegisterApi("playlist.playTrack", PlaylistPlayTrack);
    api::RegisterApi("playlist.focusTrack", PlaylistFocusTrack);
    api::RegisterApi("playlist.getFocusTrack", PlaylistGetFocusTrack);
    api::RegisterApi("playlist.sort", PlaylistSort);
    api::RegisterApi("playlist.shuffle", PlaylistShuffle);
    api::RegisterApi("playlist.undo", PlaylistUndo);
    api::RegisterApi("playlist.redo", PlaylistRedo);
    api::RegisterApi("playlist.isAutoplaylist", PlaylistIsAutoplaylist);
    api::RegisterApi("playlist.createAutoplaylist", PlaylistCreateAutoplaylist);
    api::RegisterApi("playlist.convertToAutoplaylist", PlaylistConvertToAutoplaylist);
    api::RegisterApi("playlist.removeAutoplaylist", PlaylistRemoveAutoplaylist);
    api::RegisterApi("playlist.getAutoplaylistInfo", PlaylistGetAutoplaylistInfo);
    api::RegisterApi("playlist.getAutoplaylistQuery", PlaylistGetAutoplaylistQuery);
    api::RegisterApi("playlist.duplicate", PlaylistDuplicate);
    api::RegisterApi("playlist.addPaths", PlaylistAddPaths);
    api::RegisterApi("playlist.addHandles", PlaylistAddHandles);
    api::RegisterApi("playlist.getLockInfo", PlaylistGetLockInfo);
    api::RegisterApi("playlist.isLocked", PlaylistIsLocked);
    api::RegisterApi("playlist.getSelection", PlaylistGetSelection);
    api::RegisterApi("playlist.selectAll", PlaylistSelectAll);
    api::RegisterApi("playlist.deselectAll", PlaylistDeselectAll);
    api::RegisterApi("playlist.getFocusedTrack", PlaylistGetFocusedTrack);
    api::RegisterApi("playlist.setFocusedTrack", PlaylistSetFocusedTrack);
    api::RegisterApi("playlist.reverse", PlaylistReverse);
    api::RegisterApi("playlist.reorder", PlaylistReorder);
    api::RegisterApi("playlist.addPathsSequential", PlaylistAddPathsSequential);
    api::RegisterApi("playlist.addPathsAsync", PlaylistAddPathsAsync);
    api::RegisterApi("playlist.replaceAllAndPlay", PlaylistReplaceAllAndPlay);
    api::RegisterApi("playlist.reorderPlaylists", PlaylistReorderPlaylists);
    api::RegisterApi("playlist.getAvailableColumns", PlaylistGetAvailableColumns);
    // deferred：全表求键与序列化在 CPU worker 上跑
    api::RegisterApiDeferred("playlist.getGroupRuns", PlaylistGetGroupRuns);
}
