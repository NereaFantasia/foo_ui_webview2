#include "pch.h"
#include "api/PlaylistApi.h"
#include "api/ApiConstants.h"
#include "api/AsyncOperationRegistry.h"
#include "api/BridgeCore.h"
#include "api/ErrorEnvelope.h"
#include "api/GroupRunPlan.h"
#include "api/MetaAccess.h"
#include "api/RatingResolve.h"
#include "api/TrackWireSnapshot.h"
#include "core/WebViewContext.h"
#include "interfaces/Fb2kPlaylistService.h"
#include "interfaces/Fb2kPlaybackService.h"
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

static json MakePlaylistLockedError(size_t playlistIndex) {
    FailureHook::LogSync("playlist.*", ApiErrorCode::LOCKED,
                         "Playlist is locked", true);
    return ApiEnvelope::MakeError("Playlist is locked", ApiErrorCode::LOCKED,
                                  {{"playlist", playlistIndex}, {"isLocked", true}});
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

// Helper: Get playlist index from params (supports both 'index' and 'playlist' parameter names)
static size_t GetPlaylistIndexFromParams(const json& params) {
    if (params.contains("playlist")) {
        return params.value("playlist", pfc::infinite_size);
    }
    return params.value("index", pfc::infinite_size);
}

json GetPlaylistTrackInfo(const metadb_handle_ptr& track, size_t index) {
    if (!track.is_valid()) return nullptr;
    
    // Get native filesystem path
    pfc::string8 nativePath;
    filesystem::g_get_native_path(track->get_path(), nativePath);
    std::string absolutePath = nativePath.get_ptr();
    
    // 使用 get_info_ref() 替代已弃用的 get_info()，避免 file_info_impl 值拷贝开销
    metadb_info_container::ptr infoContainer = track->get_info_ref();
    const file_info& info = infoContainer->info();
    
    auto getMeta = [&](const char* name) -> std::string {
        const char* value = info.meta_get(name, 0);
        return value ? value : "";
    };
    
    auto getMetaInt = [&](const char* name) -> int {
        const char* value = info.meta_get(name, 0);
        return value ? atoi(value) : 0;
    };
    
    // 取值顺序与来源判定归 RatingResolve.h，rating.get 走的是同一处
    const int rating = ResolveTrackRating(track, &info).value;

    // Get audio technical info
    std::string codec;
    int bitrate = 0;
    int sampleRate = 0;
    int channels = 0;
    
    // Try to get codec from info_get
    const char* codecVal = info.info_get("codec");
    if (codecVal) codec = codecVal;
    
    // Try to get bitrate
    const char* bitrateVal = info.info_get("bitrate");
    if (bitrateVal) bitrate = atoi(bitrateVal);
    
    // Get sample rate and channels from info
    sampleRate = static_cast<int>(info.info_get_int("samplerate"));
    channels = static_cast<int>(info.info_get_int("channels"));
    
    return {
        {"index", index},
        {"title", getMeta("title")},
        {"artist", MetaJoined(info, "artist")},
        {"album", getMeta("album")},
        {"albumArtist", MetaJoined(info, "album artist")},
        {"genre", MetaJoined(info, "genre")},
        {"date", getMeta("date")},
        {"trackNumber", getMetaInt("tracknumber")},
        {"discNumber", getMetaInt("discnumber")},
        {"duration", info.get_length()},
        {"path", std::string(track->get_path())},
        {"absolutePath", absolutePath},
        {"fileSize", static_cast<int64_t>(track->get_filesize())},
        {"subsong", track->get_subsong_index()},
        {"rating", rating},
        {"codec", codec},
        {"bitrate", bitrate},
        {"sampleRate", sampleRate},
        {"channels", channels},
        {"composer", MetaJoined(info, "composer")},
        {"comment", getMeta("comment")},
    };
}

// ============================================
// 投影分支（playlist.getTracks 的 fields 参数）
// ============================================
// 与全字段路径同名键的取值表达式逐条相同，但只算掩码选中的字段——未选
// absolutePath 不调 g_get_native_path、未选 rating 不走 %rating% 回退，这是
// 投影的性能收益所在。
// artists 是投影态独有的键（全字段路径没有）：使用 MetaValuesRaw
// （注意源标签名是 artist 不是 artists），类型是字符串数组。
// 无效句柄同样出全部请求键、值取类型默认：消费方的行校验器
// 不用为损坏行分叉。composer / comment 不在投影白名单里，不会传入这里。
json GetPlaylistTrackInfoProjected(const metadb_handle_ptr& track, size_t index,
                                   uint32_t mask) {
    json row;
    row["index"] = index;

    if (!track.is_valid()) {
        if (mask & TrackField::kTitle) row["title"] = "";
        if (mask & TrackField::kArtist) row["artist"] = "";
        if (mask & TrackField::kArtists) row["artists"] = json::array();
        if (mask & TrackField::kAlbum) row["album"] = "";
        if (mask & TrackField::kAlbumArtist) row["albumArtist"] = "";
        if (mask & TrackField::kGenre) row["genre"] = "";
        if (mask & TrackField::kDate) row["date"] = "";
        if (mask & TrackField::kTrackNumber) row["trackNumber"] = 0;
        if (mask & TrackField::kDiscNumber) row["discNumber"] = 0;
        if (mask & TrackField::kDuration) row["duration"] = 0.0;
        if (mask & TrackField::kPath) row["path"] = "";
        if (mask & TrackField::kAbsolutePath) row["absolutePath"] = "";
        if (mask & TrackField::kFileSize) row["fileSize"] = 0;
        if (mask & TrackField::kBitrate) row["bitrate"] = 0;
        if (mask & TrackField::kSampleRate) row["sampleRate"] = 0;
        if (mask & TrackField::kChannels) row["channels"] = 0;
        if (mask & TrackField::kCodec) row["codec"] = "";
        if (mask & TrackField::kSubsong) row["subsong"] = 0;
        if (mask & TrackField::kRating) row["rating"] = 0;
        return row;
    }

    // 使用 get_info_ref() 替代已弃用的 get_info()，与全字段路径同一口径
    metadb_info_container::ptr infoContainer = track->get_info_ref();
    const file_info& info = infoContainer->info();

    auto getMeta = [&](const char* name) -> std::string {
        const char* value = info.meta_get(name, 0);
        return value ? value : "";
    };
    auto getMetaInt = [&](const char* name) -> int {
        const char* value = info.meta_get(name, 0);
        return value ? atoi(value) : 0;
    };

    if (mask & TrackField::kTitle) row["title"] = getMeta("title");
    if (mask & TrackField::kArtist) row["artist"] = MetaJoined(info, "artist");
    if (mask & TrackField::kArtists) row["artists"] = MetaValuesRaw(info, "artist");
    if (mask & TrackField::kAlbum) row["album"] = getMeta("album");
    if (mask & TrackField::kAlbumArtist) row["albumArtist"] = MetaJoined(info, "album artist");
    if (mask & TrackField::kGenre) row["genre"] = MetaJoined(info, "genre");
    if (mask & TrackField::kDate) row["date"] = getMeta("date");
    if (mask & TrackField::kTrackNumber) row["trackNumber"] = getMetaInt("tracknumber");
    if (mask & TrackField::kDiscNumber) row["discNumber"] = getMetaInt("discnumber");
    if (mask & TrackField::kDuration) row["duration"] = info.get_length();
    if (mask & TrackField::kPath) row["path"] = std::string(track->get_path());
    if (mask & TrackField::kAbsolutePath) {
        pfc::string8 nativePath;
        filesystem::g_get_native_path(track->get_path(), nativePath);
        row["absolutePath"] = std::string(nativePath.get_ptr());
    }
    if (mask & TrackField::kFileSize)
        row["fileSize"] = static_cast<int64_t>(track->get_filesize());
    if (mask & TrackField::kSubsong) row["subsong"] = track->get_subsong_index();
    if (mask & TrackField::kRating) row["rating"] = ResolveTrackRating(track, &info).value;
    if (mask & TrackField::kCodec) {
        const char* value = info.info_get("codec");
        row["codec"] = value ? value : "";
    }
    if (mask & TrackField::kBitrate) {
        const char* value = info.info_get("bitrate");
        row["bitrate"] = value ? atoi(value) : 0;
    }
    if (mask & TrackField::kSampleRate)
        row["sampleRate"] = static_cast<int>(info.info_get_int("samplerate"));
    if (mask & TrackField::kChannels)
        row["channels"] = static_cast<int>(info.info_get_int("channels"));
    return row;
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

json Fb2kPlaylistService::get_tracks_json(
    size_t playlist, size_t start, size_t count, const json& formats,
    const TrackFieldSelection& fields) const {
    auto plm = playlist_manager::get();
    size_t totalCount = plm->playlist_get_item_count(playlist);

    if (start >= totalCount) {
        return {
            {"playlist", playlist}, {"start", start},
            {"count", 0}, {"total", totalCount}, {"tracks", json::array()}
        };
    }

    size_t actualCount = std::min(count, totalCount - start);
    metadb_handle_list items;
    pfc::bit_array_range range(start, actualCount);
    plm->playlist_get_items(playlist, items, range);

    // Compile optional title format columns (foo_playcount virtual fields etc.)
    std::vector<std::pair<std::string, titleformat_object::ptr>> compiledFormats;
    if (!formats.empty() && formats.is_object()) {
        auto compiler = titleformat_compiler::get();
        for (auto& [key, val] : formats.items()) {
            if (val.is_string()) {
                titleformat_object::ptr script;
                compiler->compile_safe(script, val.get<std::string>().c_str());
                compiledFormats.emplace_back(key, script);
            }
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

    json tracks = json::array();
    for (size_t i = 0; i < items.get_count(); i++) {
        if (fields.projected) {
            json trackInfo = GetPlaylistTrackInfoProjected(items[i], start + i, fields.mask);
            for (auto& [key, script] : compiledFormats) {
                pfc::string8 result;
                mdb2->formatTitle_v2(items[i], v2recs[i], nullptr, result, script, nullptr);
                trackInfo[key] = std::string(result.get_ptr());
            }
            tracks.push_back(std::move(trackInfo));
            continue;
        }

        json trackInfo = GetPlaylistTrackInfo(items[i], start + i);

        auto evalV2 = [&](const titleformat_object::ptr& script) -> std::string {
            pfc::string8 buf;
            mdb2->formatTitle_v2(items[i], v2recs[i], nullptr, buf, script, nullptr);
            if (buf.get_length() == 0 || (buf.get_length() == 1 && buf[0] == '?')) return "";
            return std::string(buf.get_ptr());
        };
        trackInfo["playCount"]   = evalV2(s_playCount);
        trackInfo["firstPlayed"] = evalV2(s_firstPlayed);
        trackInfo["lastPlayed"]  = evalV2(s_lastPlayed);
        trackInfo["added"]       = evalV2(s_added);

        for (auto& [key, script] : compiledFormats) {
            pfc::string8 result;
            mdb2->formatTitle_v2(items[i], v2recs[i], nullptr, result, script, nullptr);
            trackInfo[key] = std::string(result.get_ptr());
        }
        tracks.push_back(std::move(trackInfo));
    }

    return {
        {"playlist", playlist}, {"start", start},
        {"count", tracks.size()}, {"total", totalCount}, {"tracks", tracks}
    };
}

json Fb2kPlaylistService::get_selected_tracks_json(size_t playlist) const {
    auto plm = playlist_manager::get();
    size_t itemCount = plm->playlist_get_item_count(playlist);

    pfc::bit_array_bittable selection(itemCount);
    plm->playlist_get_selection_mask(playlist, selection);

    metadb_handle_list items;
    plm->playlist_get_selected_items(playlist, items);

    json tracks = json::array();
    size_t trackIdx = 0;
    for (size_t i = 0; i < itemCount; i++) {
        if (selection.get(i) && trackIdx < items.get_count()) {
            tracks.push_back(GetPlaylistTrackInfo(items[trackIdx], i));
            trackIdx++;
        }
    }

    return {
        {"success", true}, {"playlist", playlist},
        {"tracks", tracks}, {"count", tracks.size()}
    };
}

// -- Path-based service methods (out-of-line) -----------------

IPlaylistService::AddPathsResult Fb2kPlaylistService::add_paths(
    size_t playlist, const json& paths) {
    auto plm = playlist_manager::get();
    size_t countBefore = plm->playlist_get_item_count(playlist);

    metadb_handle_list items;
    size_t invalidCount = 0;
    ResolvePathsToHandles(paths, items, invalidCount);

    if (items.get_count() == 0) {
        return { 0, invalidCount, countBefore, countBefore };
    }

    plm->playlist_undo_backup(playlist);
    size_t insertPos = plm->playlist_get_item_count(playlist);
    plm->playlist_insert_items(playlist, insertPos, items, bit_array_false());
    size_t countAfter = plm->playlist_get_item_count(playlist);

    return { items.get_count(), invalidCount, countBefore, countAfter };
}

IPlaylistService::AddPathsResult Fb2kPlaylistService::add_handles(
    size_t playlist, const json& handles) {
    auto plm = playlist_manager::get();
    size_t countBefore = plm->playlist_get_item_count(playlist);

    metadb_handle_list items;
    size_t invalidCount = 0;
    ParseJsonHandlesToList(handles, items, invalidCount);

    if (items.get_count() == 0) {
        return { 0, invalidCount, countBefore, countBefore };
    }

    plm->playlist_undo_backup(playlist);
    size_t insertPos = plm->playlist_get_item_count(playlist);
    plm->playlist_insert_items(playlist, insertPos, items, bit_array_false());
    size_t countAfter = plm->playlist_get_item_count(playlist);

    return { items.get_count(), invalidCount, countBefore, countAfter };
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

    json resultIndices = json::array();
    if (ordered.get_count() == 0) {
        return { 0, resultIndices };
    }

    plm->playlist_undo_backup(playlist);
    size_t insertPos = plm->playlist_get_item_count(playlist);
    plm->playlist_insert_items(playlist, insertPos, ordered, bit_array_false());
    for (size_t j = 0; j < ordered.get_count(); j++) {
        resultIndices.push_back(insertPos + j);
    }

    return { ordered.get_count(), resultIndices };
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

    auto plm = playlist_manager::get();
    size_t fastAdded = 0;
    if (fastHandles.get_count() > 0 && playlist < plm->get_playlist_count()) {
        plm->playlist_undo_backup(playlist);
        size_t insertPos = plm->playlist_get_item_count(playlist);
        plm->playlist_insert_items(playlist, insertPos, fastHandles, bit_array_false());
        fastAdded = fastHandles.get_count();
    }

    if (slowList.get_count() == 0) {
        // 全是 fast path,立即触发回调,不走 process_locations_async (永不弹窗)。
        if (onComplete) onComplete(fastAdded, totalCount);
        return { totalCount, invalidCount };
    }

    // 还有 wrapper 路径,异步展开 (op_flag_delay_ui+background 短操作不弹)。
    auto filter = playlist_incoming_item_filter_v2::get();
    auto notify = process_locations_notify::create(
        [playlist, operationId, totalCount, fastAdded, onComplete](metadb_handle_list_cref items) {
            size_t slowAdded = items.get_count();
            auto plm = playlist_manager::get();
            if (slowAdded > 0 && playlist < plm->get_playlist_count()) {
                plm->playlist_undo_backup(playlist);
                size_t insertPos = plm->playlist_get_item_count(playlist);
                plm->playlist_insert_items(playlist, insertPos, items, bit_array_false());
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

    plm->playlist_undo_backup(playlist);
    size_t clearedCount = plm->playlist_get_item_count(playlist);
    plm->playlist_clear(playlist);

    metadb_handle_list items;
    size_t invalidCount = 0;
    ResolvePathsToHandles(paths, items, invalidCount);

    if (items.get_count() == 0) {
        return { clearedCount, 0, invalidCount, 0 };
    }

    plm->playlist_insert_items(playlist, 0, items, bit_array_false());
    size_t totalCount = plm->playlist_get_item_count(playlist);

    return { clearedCount, items.get_count(), invalidCount, totalCount };
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


// ========== Playlist Management ==========

json PlaylistGetCount(const json& params) {
    return { {"count", s_playlistService->get_playlist_count()} };
}


json PlaylistGetAll(const json& params) {
    auto allPlaylists = s_playlistService->get_all_playlists();

    json playlists = json::array();
    playlists.get_ref<json::array_t&>().reserve(allPlaylists.size());

    for (const auto& pl : allPlaylists) {
        playlists.push_back({
            {"index", pl.index},
            {"name", pl.name},
            {"trackCount", pl.trackCount},
            {"isActive", pl.isActive},
            {"isPlaying", pl.isPlaying},
            {"isLocked", pl.isLocked},
            {"isAutoplaylist", pl.isAutoplaylist},
        });
    }

    return playlists;
}


json PlaylistGetActive(const json& params) {
    size_t active = s_playlistService->get_active_playlist();

    if (active == SIZE_MAX) {
        return { {"success", true}, {"found", false} };
    }

    auto d = s_playlistService->get_playlist_detail(active, true);
    if (!d.found) {
        return { {"success", true}, {"found", false} };
    }

    return {
        {"index", d.index},
        {"name", d.name},
        {"trackCount", d.trackCount},
        {"isActive", d.isActive},
        {"isPlaying", d.isPlaying},
        {"isLocked", d.isLocked},
        {"duration", d.duration},
    };
}


json PlaylistSetActive(const json& params) {
    size_t playlist = params.value("playlist", SIZE_MAX);

    if (playlist >= s_playlistService->get_playlist_count()) {
        return { {"success", false}, {"error", "Invalid playlist index"} };
    }

    s_playlistService->set_active_playlist(playlist);
    return { {"success", true} };
}


json PlaylistGetPlaying(const json& params) {
    size_t playing = s_playlistService->get_playing_playlist();

    if (playing == SIZE_MAX) {
        return { {"success", true}, {"found", false} };
    }

    auto d = s_playlistService->get_playlist_detail(playing, true);
    if (!d.found) {
        return { {"success", true}, {"found", false} };
    }

    return {
        {"index", d.index},
        {"name", d.name},
        {"trackCount", d.trackCount},
        {"isActive", d.isActive},
        {"isPlaying", d.isPlaying},
        {"isLocked", d.isLocked},
        {"duration", d.duration},
    };
}


json PlaylistCreate(const json& params) {
    std::string name = params.value("name", "New Playlist");
    size_t insertAt = params.value("position", SIZE_MAX);

    size_t newIndex = s_playlistService->create_playlist(name, insertAt);

    return {
        {"success", true},
        {"index", newIndex},
    };
}


json PlaylistRemove(const json& params) {
    auto* svc = s_playlistService;
    size_t playlist = params.value("playlist", SIZE_MAX);

    if (playlist == SIZE_MAX) {
        playlist = svc->get_active_playlist();
    }

    if (playlist >= svc->get_playlist_count()) {
        return { {"success", false}, {"error", "Invalid playlist index"} };
    }

    bool hasLock = svc->playlist_lock_is_present(playlist);

    bool result = svc->remove_playlist(playlist);
    if (!result && hasLock) {
        return MakePlaylistLockedError(playlist);
    }
    return { {"success", result} };
}


json PlaylistRename(const json& params) {
    size_t playlist = params.value("playlist", SIZE_MAX);
    std::string name = params.value("name", "");

    if (playlist >= s_playlistService->get_playlist_count()) {
        return { {"success", false}, {"error", "Invalid playlist index"} };
    }

    bool result = s_playlistService->playlist_rename(playlist, name);
    return { {"success", result} };
}


json PlaylistClear(const json& params) {
    auto* svc = s_playlistService;
    size_t playlist = params.value("playlist", SIZE_MAX);

    if (playlist == SIZE_MAX) {
        playlist = svc->get_active_playlist();
    }

    if (playlist >= svc->get_playlist_count()) {
        return { {"success", false}, {"error", "Invalid playlist index"} };
    }

    if (svc->playlist_lock_is_present(playlist)) {
        return MakePlaylistLockedError(playlist);
    }

    size_t countBefore = svc->playlist_get_item_count(playlist);
    svc->playlist_undo_backup(playlist);
    svc->playlist_clear(playlist);
    size_t countAfter = svc->playlist_get_item_count(playlist);

    return {
        {"success", countAfter == 0},
        {"playlist", playlist},
        {"clearedCount", countBefore},
        {"remainingCount", countAfter}
    };
}


// ========== Track Operations ==========

json PlaylistInsertTracks(const json& params) {
    auto* svc = s_playlistService;
    size_t playlistIndex = params.value("playlist", SIZE_MAX);
    size_t insertIndex = params.value("position", params.value("index", static_cast<size_t>(0)));
    auto handles = params.value("handles", json::array());

    if (playlistIndex == SIZE_MAX) {
        playlistIndex = svc->get_active_playlist();
    }
    if (playlistIndex >= svc->get_playlist_count()) {
        return {{"success", false}, {"error", "Invalid playlist index"}};
    }
    if (svc->playlist_lock_is_present(playlistIndex)) {
        return MakePlaylistLockedError(playlistIndex);
    }
    if (handles.empty()) {
        return {{"success", false}, {"error", "No handles specified"}};
    }

    auto r = svc->insert_tracks(playlistIndex, insertIndex, handles);
    if (!r.success) {
        return {
            {"success", false}, {"error", r.error},
            {"playlist", playlistIndex}, {"requestedCount", handles.size()},
            {"invalidCount", r.invalidCount}
        };
    }
    return {
        {"success", true}, {"playlist", playlistIndex},
        {"insertIndex", insertIndex}, {"requestedCount", handles.size()},
        {"addedCount", r.addedCount}, {"invalidCount", r.invalidCount},
        {"countBefore", r.countBefore}, {"totalCount", r.totalCount}
    };
}


json PlaylistGetTrackCount(const json& params) {
    auto* svc = s_playlistService;
    size_t index = GetPlaylistIndexFromParams(params);
    if (index == SIZE_MAX) {
        index = svc->get_active_playlist();
    }
    if (index >= svc->get_playlist_count()) {
        return { {"count", 0} };
    }
    return { {"count", svc->playlist_get_item_count(index)} };
}


json PlaylistGetTracks(const json& params) {
    auto* svc = s_playlistService;
    // fields 校验排在越界检查之前：插在后面会让
    // {playlist: 99, fields: ["bogus"]} 静默出空页而不是报 INVALID_PARAMS。
    const TrackFieldSelection fields = ParseTrackFieldSelection(params);
    if (!fields.valid) {
        return MakeTrackFieldsErrorBody(fields);
    }
    size_t playlistIndex = GetPlaylistIndexFromParams(params);
    size_t start = params.value("start", static_cast<size_t>(0));
    size_t count = params.value("count", static_cast<size_t>(100));
    auto extraFormats = params.value("formats", json::object());

    if (playlistIndex == SIZE_MAX) {
        playlistIndex = svc->get_active_playlist();
    }
    if (playlistIndex >= svc->get_playlist_count()) {
        return {
            {"playlist", playlistIndex}, {"start", start},
            {"count", 0}, {"total", 0}, {"tracks", json::array()}
        };
    }
    return svc->get_tracks_json(playlistIndex, start, count, extraFormats, fields);
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
        fb2k_group_runs::WriteGroupRunsJson(out, req->playlist, req->total, req->acc);
        responder.SendRaw(std::move(out));
    } catch (const std::exception& e) {
        responder.SendJson(MakeGroupRunsErrorBody(e.what()));
    } catch (...) {
        responder.SendJson(MakeGroupRunsErrorBody("getGroupRuns failed"));
    }
}

void PlaylistGetGroupRuns(const json& params, const DeferredResponder& responder) {
    try {
        auto* svc = s_playlistService;

        const fb2k_group_runs::PatternSelection selection =
            fb2k_group_runs::ParseGroupRunPatterns(params);
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

        size_t playlistIndex = GetPlaylistIndexFromParams(params);
        if (playlistIndex == SIZE_MAX) {
            playlistIndex = svc->get_active_playlist();
        }
        // 越界只对 >= count 的正值报错：JSON 的 -1 转 size_t 回绕成 SIZE_MAX，与「缺省」
        // 哨兵同值，因此选择活动列表；不能将 -1 描述为越界错误。
        if (playlistIndex >= svc->get_playlist_count()) {
            responder.SendJson(ApiEnvelope::MakeError("Invalid playlist index",
                                                      ApiErrorCode::INVALID_PARAMS,
                                                      {{"playlist", playlistIndex}}));
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
        req->twoLevel = selection.patterns.size() == fb2k_group_runs::kMaxPatterns;
        req->acc = fb2k_group_runs::GroupRunAccumulator(req->twoLevel);
        playlist_manager::get()->playlist_get_all_items(playlistIndex, req->items);
        req->total = req->items.get_count();

        // 空列表就地回包，不为零行付线程跳变的税
        if (req->total == 0) {
            responder.SendJson({{"success", true},
                                {"playlist", playlistIndex},
                                {"total", 0},
                                {"runs", json::array()}});
            return;
        }

        fb2k::inCpuWorkerThread([req, responder]() { RunGroupRunsWorker(req, responder); });
    } catch (const std::exception& e) {
        responder.SendJson(MakeGroupRunsErrorBody(e.what()));
    } catch (...) {
        responder.SendJson(MakeGroupRunsErrorBody("getGroupRuns failed"));
    }
}


json PlaylistGetSelectedTracks(const json& params) {
    auto* svc = s_playlistService;
    size_t playlistIndex = GetPlaylistIndexFromParams(params);

    if (playlistIndex == SIZE_MAX) {
        playlistIndex = svc->get_active_playlist();
    }
    if (playlistIndex >= svc->get_playlist_count()) {
        return { {"success", false}, {"error", "Invalid playlist index"}, {"tracks", json::array()} };
    }
    return svc->get_selected_tracks_json(playlistIndex);
}


json PlaylistSetSelection(const json& params) {
    auto* svc = s_playlistService;
    size_t playlistIndex = GetPlaylistIndexFromParams(params);
    auto indices = params.value("indices", json::array());
    bool clearOthers = params.value("clearOthers", true);

    if (playlistIndex == SIZE_MAX) {
        playlistIndex = svc->get_active_playlist();
    }
    if (playlistIndex >= svc->get_playlist_count()) {
        return { {"success", false}, {"error", "Invalid playlist index"} };
    }

    std::vector<size_t> idxVec;
    for (const auto& idx : indices) {
        idxVec.push_back(idx.get<size_t>());
    }
    svc->set_selection(playlistIndex, idxVec, clearOthers);
    return { {"success", true} };
}


json PlaylistRemoveTracks(const json& params) {
    auto* svc = s_playlistService;
    size_t playlistIndex = GetPlaylistIndexFromParams(params);
    auto items = params.value("items", json::array());

    if (playlistIndex == SIZE_MAX) {
        playlistIndex = svc->get_active_playlist();
    }
    if (playlistIndex >= svc->get_playlist_count()) {
        return { {"success", false}, {"error", "Invalid playlist index"} };
    }
    if (svc->playlist_lock_is_present(playlistIndex)) {
        return MakePlaylistLockedError(playlistIndex);
    }

    std::vector<size_t> indices;
    for (const auto& item : items) {
        indices.push_back(item.get<size_t>());
    }
    svc->remove_tracks(playlistIndex, indices);
    return { {"success", true} };
}


json PlaylistRemoveSelectedTracks(const json& params) {
    auto* svc = s_playlistService;
    size_t playlistIndex = GetPlaylistIndexFromParams(params);

    if (playlistIndex == SIZE_MAX) {
        playlistIndex = svc->get_active_playlist();
    }
    if (playlistIndex >= svc->get_playlist_count()) {
        return { {"success", false}, {"error", "Invalid playlist index"} };
    }
    if (svc->playlist_lock_is_present(playlistIndex)) {
        return MakePlaylistLockedError(playlistIndex);
    }
    svc->remove_selection(playlistIndex);
    return { {"success", true} };
}


json PlaylistMoveTracks(const json& params) {
    auto* svc = s_playlistService;
    size_t playlistIndex = GetPlaylistIndexFromParams(params);
    auto items = params.value("items", json::array());
    int delta = params.value("delta", 0);

    if (playlistIndex == SIZE_MAX) {
        playlistIndex = svc->get_active_playlist();
    }
    if (playlistIndex >= svc->get_playlist_count()) {
        return { {"success", false}, {"error", "Invalid playlist index"} };
    }
    if (svc->playlist_lock_is_present(playlistIndex)) {
        return MakePlaylistLockedError(playlistIndex);
    }

    svc->playlist_undo_backup(playlistIndex);

    if (!items.empty()) {
        std::vector<size_t> idxVec;
        for (const auto& item : items) {
            idxVec.push_back(item.get<size_t>());
        }
        svc->set_selection(playlistIndex, idxVec, true);
    }
    svc->move_selection(playlistIndex, delta);
    return { {"success", true} };
}


json PlaylistPlayTrack(const json& params) {
    auto* svc = s_playlistService;
    size_t playlistIndex = params.value("playlist", SIZE_MAX);
    // 支持两种参数名：index（新）和 track（旧），优先使用 index
    size_t trackIndex = params.value("index", params.value("track", static_cast<size_t>(0)));
    bool deferred = params.value("deferred", false);  // 新增：延迟执行选项
    
    if (playlistIndex == SIZE_MAX) {
        playlistIndex = svc->get_active_playlist();
    }
    
    if (playlistIndex >= svc->get_playlist_count()) {
        return { {"success", false}, {"error", "Invalid playlist index"} };
    }
    
    size_t trackCount = svc->playlist_get_item_count(playlistIndex);
    if (trackIndex >= trackCount) {
        return { {"success", false}, {"error", "Invalid track index"} };
    }
    
    // 可选：播放前先静音，消除两次 IPC round-trip 之间的音频缓冲间隙
    // muted 依赖 playback_control SDK，不可通过 playlist service 抽象
    bool muted = params.value("muted", false);
    if (muted) {
        auto pc = playback_control::get();
        if (!pc->is_muted()) pc->volume_mute_toggle();
    }

    if (deferred) {
        // 延迟执行：让当前消息循环完成后再播放
        // deferred 路径依赖 fb2k::inMainThread SDK，不可通过 service 抽象
        size_t pIdx = playlistIndex;
        size_t tIdx = trackIndex;
        fb2k::inMainThread([pIdx, tIdx]() {
            playlist_manager::get()->playlist_execute_default_action(pIdx, tIdx);
        });
    } else {
        // 立即执行 — 走 service 接口
        svc->execute_default_action(playlistIndex, trackIndex);
    }
    
    return { {"success", true} };
}


// [DEPRECATED] 改用 playlist.setFocusedTrack；保留仅为兼容旧调用。
json PlaylistFocusTrack(const json& params) {
    FB2K_console_print("[DEPRECATED] playlist.focusTrack is deprecated, use playlist.setFocusedTrack");
    auto* svc = s_playlistService;
    size_t playlistIndex = params.value("playlist", SIZE_MAX);
    size_t trackIndex = params.value("index", params.value("track", SIZE_MAX));
    if (playlistIndex == SIZE_MAX) playlistIndex = svc->get_active_playlist();
    if (playlistIndex >= svc->get_playlist_count()) return {{"success", false}, {"error", "Invalid playlist"}};
    size_t trackCount = svc->playlist_get_item_count(playlistIndex);
    if (trackIndex != pfc::infinite_size && trackIndex >= trackCount) {
        return {{"success", false}, {"error", "Invalid track index"}};
    }
    svc->set_focus_item(playlistIndex, trackIndex);
    return {{"success", true}};
}


// [DEPRECATED] 改用 playlist.getFocusedTrack；保留仅为兼容旧调用。
json PlaylistGetFocusTrack(const json& params) {
    FB2K_console_print("[DEPRECATED] playlist.getFocusTrack is deprecated, use playlist.getFocusedTrack");
    auto* svc = s_playlistService;
    size_t playlistIndex = params.value("playlist", SIZE_MAX);
    if (playlistIndex == SIZE_MAX) playlistIndex = svc->get_active_playlist();
    if (playlistIndex >= svc->get_playlist_count()) return {{"success", false}, {"error", "Invalid playlist"}};
    size_t focus = svc->get_focus_item(playlistIndex);
    return {{"success", true}, {"playlist", playlistIndex}, {"index", focus == SIZE_MAX ? -1 : (int64_t)focus}};
}


json PlaylistSort(const json& params) {
    auto* svc = s_playlistService;
    size_t playlistIndex = GetPlaylistIndexFromParams(params);
    std::string pattern = params.value("pattern", "%title%");
    bool descending = params.value("descending", false);
    bool selectedOnly = params.value("selectedOnly", false);

    if (playlistIndex == SIZE_MAX) playlistIndex = svc->get_active_playlist();
    if (playlistIndex >= svc->get_playlist_count())
        return { {"success", false}, {"error", "Invalid playlist index"} };
    if (svc->playlist_lock_is_present(playlistIndex))
        return MakePlaylistLockedError(playlistIndex);

    svc->playlist_undo_backup(playlistIndex);
    svc->sort_by_format(playlistIndex, pattern.c_str(), selectedOnly);

    if (descending) {
        size_t count = svc->playlist_get_item_count(playlistIndex);
        std::vector<size_t> order(count);
        for (size_t i = 0; i < count; i++) order[i] = count - 1 - i;
        svc->reorder_items(playlistIndex, order.data(), count);
    }

    return { {"success", true} };
}


json PlaylistShuffle(const json& params) {
    auto* svc = s_playlistService;
    size_t playlistIndex = GetPlaylistIndexFromParams(params);

    if (playlistIndex == SIZE_MAX) playlistIndex = svc->get_active_playlist();
    if (playlistIndex >= svc->get_playlist_count())
        return { {"success", false}, {"error", "Invalid playlist index"} };
    if (svc->playlist_lock_is_present(playlistIndex))
        return MakePlaylistLockedError(playlistIndex);

    size_t count = svc->playlist_get_item_count(playlistIndex);
    if (count <= 1) return { {"success", true} };

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

    svc->playlist_undo_backup(playlistIndex);
    svc->reorder_items(playlistIndex, order.data(), count);

    return { {"success", true} };
}


json PlaylistUndo(const json& params) {
    auto* svc = s_playlistService;
    size_t playlistIndex = params.value("playlist", SIZE_MAX);
    if (playlistIndex == SIZE_MAX) playlistIndex = svc->get_active_playlist();
    if (playlistIndex >= svc->get_playlist_count())
        return { {"success", false}, {"error", "Invalid playlist index"} };
    bool result = svc->undo_restore(playlistIndex);
    return { {"success", result} };
}


json PlaylistRedo(const json& params) {
    auto* svc = s_playlistService;
    size_t playlistIndex = params.value("playlist", SIZE_MAX);
    if (playlistIndex == SIZE_MAX) playlistIndex = svc->get_active_playlist();
    if (playlistIndex >= svc->get_playlist_count())
        return { {"success", false}, {"error", "Invalid playlist index"} };
    bool result = svc->redo_restore(playlistIndex);
    return { {"success", result} };
}


// ========== Autoplaylist (Smart Playlist) APIs ==========

// Check if a playlist is an autoplaylist
json PlaylistIsAutoplaylist(const json& params) {
    auto* svc = s_playlistService;
    size_t playlistIndex = params.value("playlist", SIZE_MAX);
    if (playlistIndex == SIZE_MAX) playlistIndex = svc->get_active_playlist();
    if (playlistIndex >= svc->get_playlist_count())
        return { {"success", false}, {"error", "Invalid playlist index"} };

    auto det = svc->detect_autoplaylist(playlistIndex);
    json result = { {"playlist", playlistIndex}, {"isAutoplaylist", det.isAutoplaylist} };
    if (!det.lockName.empty()) result["lockName"] = det.lockName;
    return result;
}


// Create a simple autoplaylist (smart playlist)
json PlaylistCreateAutoplaylist(const json& params) {
    auto* svc = s_playlistService;
    std::string name = params.value("name", "New Autoplaylist");
    std::string query = params.value("query", "");
    std::string sort = params.value("sort", "");
    bool keepSorted = params.value("keepSorted", false);

    if (query.empty())
        return { {"success", false}, {"error", "Query is required"} };

    size_t newIndex = svc->create_playlist(name, SIZE_MAX);
    if (newIndex == SIZE_MAX)
        return { {"success", false}, {"error", "Failed to create playlist"} };

    try {
        uint32_t flags = keepSorted ? autoplaylist_flag_sort : 0;
        svc->add_autoplaylist_client(newIndex, query.c_str(), sort.c_str(), flags);
        return {
            {"success", true}, {"index", newIndex}, {"playlist", newIndex},
            {"name", name}, {"query", query}
        };
    } catch (const std::exception& e) {
        svc->remove_playlist(newIndex);
        return { {"success", false}, {"error", std::string("Failed to create autoplaylist: ") + e.what()} };
    }
}


// Convert an existing playlist to autoplaylist
json PlaylistConvertToAutoplaylist(const json& params) {
    auto* svc = s_playlistService;
    size_t playlistIndex = params.value("playlist", SIZE_MAX);
    std::string query = params.value("query", "");
    std::string sort = params.value("sort", "");
    bool keepSorted = params.value("keepSorted", false);

    if (playlistIndex == SIZE_MAX) playlistIndex = svc->get_active_playlist();
    if (query.empty())
        return { {"success", false}, {"error", "Query is required"} };
    if (playlistIndex >= svc->get_playlist_count())
        return { {"success", false}, {"error", "Invalid playlist index"} };

    try {
        uint32_t flags = keepSorted ? autoplaylist_flag_sort : 0;
        svc->add_autoplaylist_client(playlistIndex, query.c_str(), sort.c_str(), flags);
        return { {"success", true}, {"playlist", playlistIndex} };
    } catch (const std::exception& e) {
        return { {"success", false}, {"error", std::string("Failed to convert: ") + e.what()} };
    }
}


// Remove autoplaylist status (convert back to normal playlist)
json PlaylistRemoveAutoplaylist(const json& params) {
    auto* svc = s_playlistService;
    size_t playlistIndex = params.value("playlist", SIZE_MAX);
    if (playlistIndex == SIZE_MAX) playlistIndex = svc->get_active_playlist();
    if (playlistIndex >= svc->get_playlist_count())
        return { {"success", false}, {"error", "Invalid playlist index"} };

    if (svc->autoplaylist_is_client_present(playlistIndex)) {
        svc->remove_autoplaylist_client(playlistIndex);
        return { {"success", true}, {"playlist", playlistIndex}, {"source", "sdk"} };
    }

    // Fallback: DUI autoplaylist lock
    auto det = svc->detect_autoplaylist(playlistIndex);
    if (det.isDuiAutoplaylist) {
        return {
            {"success", true}, {"playlist", playlistIndex}, {"source", "dui"},
            {"note", "DUI autoplaylist lock detected; proceed with playlist.remove to delete playlist"}
        };
    }

    return { {"success", false}, {"error", "Not an autoplaylist"} };
}


// Get autoplaylist info (flags, query if available)
json PlaylistGetAutoplaylistInfo(const json& params) {
    auto* svc = s_playlistService;
    size_t playlistIndex = params.value("playlist", SIZE_MAX);
    if (playlistIndex == SIZE_MAX) playlistIndex = svc->get_active_playlist();
    if (playlistIndex >= svc->get_playlist_count())
        return { {"success", false}, {"error", "Invalid playlist index"} };

    auto det = svc->detect_autoplaylist(playlistIndex);
    if (!det.isAutoplaylist)
        return { {"isAutoplaylist", false}, {"playlist", playlistIndex} };

    uint32_t flags = 0;
    if (!det.isDuiAutoplaylist)
        flags = svc->get_autoplaylist_flags(playlistIndex);

    json result = {
        {"isAutoplaylist", true},
        {"playlist", playlistIndex},
        {"keepSorted", (flags & autoplaylist_flag_sort) != 0},
        {"source", det.isDuiAutoplaylist ? "dui" : "sdk"}
    };
    if (!det.lockName.empty())
        result["lockName"] = det.lockName;
    return result;
}


// Get autoplaylist query string
json PlaylistGetAutoplaylistQuery(const json& params) {
    auto* svc = s_playlistService;
    size_t playlistIndex = params.value("playlist", SIZE_MAX);
    if (playlistIndex == SIZE_MAX) playlistIndex = svc->get_active_playlist();
    if (playlistIndex >= svc->get_playlist_count())
        return { {"success", false}, {"error", "Invalid playlist index"} };

    auto det = svc->detect_autoplaylist(playlistIndex);
    if (!det.isAutoplaylist)
        return { {"isAutoplaylist", false}, {"playlist", playlistIndex}, {"query", nullptr} };

    uint32_t flags = 0;
    if (!det.isDuiAutoplaylist)
        flags = svc->get_autoplaylist_flags(playlistIndex);

    json result = {
        {"isAutoplaylist", true},
        {"playlist", playlistIndex},
        {"query", nullptr},
        {"keepSorted", (flags & autoplaylist_flag_sort) != 0},
        {"source", det.isDuiAutoplaylist ? "dui" : "sdk"},
        {"note", "Query string not exposed by SDK"}
    };
    if (!det.lockName.empty())
        result["lockName"] = det.lockName;
    return result;
}


// Duplicate playlist
json PlaylistDuplicate(const json& params) {
    auto* svc = s_playlistService;
    size_t playlistIndex = params.value("playlist", SIZE_MAX);
    std::string newName = params.value("name", "");

    if (playlistIndex == SIZE_MAX) {
        playlistIndex = svc->get_active_playlist();
    }

    if (playlistIndex >= svc->get_playlist_count()) {
        return { {"success", false}, {"error", "Invalid playlist index"} };
    }

    // Default name: "Original (Copy)"
    if (newName.empty()) {
        std::string origName;
        svc->playlist_get_name(playlistIndex, origName);
        newName = origName + " (Copy)";
    }

    auto result = svc->duplicate_playlist(playlistIndex, newName);
    if (!result.success) {
        return { {"success", false}, {"error", result.error} };
    }

    return {
        {"success", true},
        {"index", result.newIndex},
        {"sourcePlaylist", result.sourceIndex},
        {"newPlaylist", result.newIndex},
        {"name", result.name},
        {"trackCount", result.trackCount}
    };
}


// Add tracks from file/folder paths
json PlaylistAddPaths(const json& params) {
    auto svc = s_playlistService;
    size_t playlistIndex = params.value("playlist", SIZE_MAX);
    auto paths = params.value("paths", json::array());

    if (playlistIndex == SIZE_MAX) {
        playlistIndex = svc->get_active_playlist();
    }
    if (playlistIndex >= svc->get_playlist_count()) {
        return { {"success", false}, {"error", "Invalid playlist index"} };
    }
    if (svc->playlist_lock_is_present(playlistIndex)) {
        return MakePlaylistLockedError(playlistIndex);
    }
    if (paths.empty()) {
        return { {"success", false}, {"error", "No paths specified"} };
    }

    auto result = svc->add_paths(playlistIndex, paths);

    if (result.addedCount == 0) {
        return {
            {"success", false}, {"error", "No valid tracks found"},
            {"playlist", playlistIndex}, {"requestedPaths", paths.size()},
            {"invalidCount", result.invalidCount}, {"countBefore", result.countBefore}
        };
    }

    return {
        {"success", true}, {"playlist", playlistIndex},
        {"requestedPaths", paths.size()}, {"addedCount", result.addedCount},
        {"invalidCount", result.invalidCount}, {"countBefore", result.countBefore},
        {"totalCount", result.totalCount}
    };
}


// Add tracks with explicit control (no automatic CUE expansion)
// Each handle specifies path and optional subsong index
json PlaylistAddHandles(const json& params) {
    auto svc = s_playlistService;
    size_t playlistIndex = params.value("playlist", SIZE_MAX);
    auto handles = params.value("handles", json::array());

    if (playlistIndex == SIZE_MAX) {
        playlistIndex = svc->get_active_playlist();
    }
    if (playlistIndex >= svc->get_playlist_count()) {
        return { {"success", false}, {"error", "Invalid playlist index"} };
    }
    if (svc->playlist_lock_is_present(playlistIndex)) {
        return MakePlaylistLockedError(playlistIndex);
    }
    if (handles.empty()) {
        return { {"success", false}, {"error", "No handles specified"} };
    }

    auto result = svc->add_handles(playlistIndex, handles);

    if (result.addedCount == 0) {
        return {
            {"success", false}, {"error", "No valid handles created"},
            {"playlist", playlistIndex}, {"requestedCount", handles.size()},
            {"invalidCount", result.invalidCount}
        };
    }

    return {
        {"success", true}, {"playlist", playlistIndex},
        {"requestedCount", handles.size()}, {"addedCount", result.addedCount},
        {"invalidCount", result.invalidCount}, {"countBefore", result.countBefore},
        {"totalCount", result.totalCount}
    };
}


// Get playlist lock info
json PlaylistGetLockInfo(const json& params) {
    auto* svc = s_playlistService;
    size_t playlistIndex = params.value("playlist", SIZE_MAX);
    if (playlistIndex == SIZE_MAX) playlistIndex = svc->get_active_playlist();
    if (playlistIndex >= svc->get_playlist_count())
        return { {"success", false}, {"error", "Invalid playlist index"} };
    bool isLocked = svc->playlist_lock_is_present(playlistIndex);
    return { {"playlist", playlistIndex}, {"isLocked", isLocked} };
}


// Alias: isLocked (returns just the boolean)
json PlaylistIsLocked(const json& params) {
    auto* svc = s_playlistService;
    size_t playlistIndex = params.value("playlist", SIZE_MAX);
    if (playlistIndex == SIZE_MAX) playlistIndex = svc->get_active_playlist();
    if (playlistIndex >= svc->get_playlist_count()) return {{"success", false}, {"isLocked", false}, {"error", "Invalid playlist"}};
    return {{"success", true}, {"isLocked", svc->playlist_lock_is_present(playlistIndex)}};
}


// Selection APIs
json PlaylistGetSelection(const json& params) {
    auto* svc = s_playlistService;
    size_t playlistIndex = params.value("playlist", SIZE_MAX);
    if (playlistIndex == SIZE_MAX) playlistIndex = svc->get_active_playlist();
    if (playlistIndex >= svc->get_playlist_count()) return {{"success", false}, {"error", "Invalid playlist"}};

    auto indices = svc->get_selection_indices(playlistIndex);
    json items = json::array();
    for (size_t i : indices) items.push_back(i);
    return {
        {"success", true}, {"items", items},
        {"count", items.size()}, {"playlist", playlistIndex}
    };
}


json PlaylistSelectAll(const json& params) {
    auto* svc = s_playlistService;
    size_t playlistIndex = params.value("playlist", SIZE_MAX);
    if (playlistIndex == SIZE_MAX) playlistIndex = svc->get_active_playlist();
    if (playlistIndex >= svc->get_playlist_count()) return {{"success", false}};
    svc->select_all(playlistIndex);
    return {{"success", true}};
}


json PlaylistDeselectAll(const json& params) {
    auto* svc = s_playlistService;
    size_t playlistIndex = params.value("playlist", SIZE_MAX);
    if (playlistIndex == SIZE_MAX) playlistIndex = svc->get_active_playlist();
    if (playlistIndex >= svc->get_playlist_count()) return {{"success", false}};
    svc->deselect_all(playlistIndex);
    return {{"success", true}};
}


json PlaylistGetFocusedTrack(const json& params) {
    auto* svc = s_playlistService;
    size_t playlistIndex = params.value("playlist", SIZE_MAX);
    if (playlistIndex == SIZE_MAX) playlistIndex = svc->get_active_playlist();
    if (playlistIndex >= svc->get_playlist_count()) return {{"success", true}, {"index", -1}};
    size_t focus = svc->get_focus_item(playlistIndex);
    return {{"success", true}, {"playlist", playlistIndex}, {"index", focus == SIZE_MAX ? -1 : (int64_t)focus}};
}


json PlaylistSetFocusedTrack(const json& params) {
    auto* svc = s_playlistService;
    size_t playlistIndex = params.value("playlist", SIZE_MAX);
    size_t trackIndex = params.value("index", SIZE_MAX);
    if (playlistIndex == SIZE_MAX) playlistIndex = svc->get_active_playlist();
    if (playlistIndex >= svc->get_playlist_count()) return {{"success", false}, {"error", "Invalid playlist index"}};
    size_t trackCount = svc->playlist_get_item_count(playlistIndex);
    if (trackIndex != pfc::infinite_size && trackIndex >= trackCount) {
        return {{"success", false}, {"error", "Invalid track index"}};
    }
    svc->set_focus_item(playlistIndex, trackIndex);
    return {{"success", true}};
}


json PlaylistReverse(const json& params) {
    auto* svc = s_playlistService;
    size_t playlistIndex = params.value("playlist", SIZE_MAX);
    if (playlistIndex == SIZE_MAX) playlistIndex = svc->get_active_playlist();
    if (playlistIndex >= svc->get_playlist_count()) return {{"success", false}};
    if (svc->playlist_lock_is_present(playlistIndex)) return MakePlaylistLockedError(playlistIndex);

    size_t count = svc->playlist_get_item_count(playlistIndex);
    if (count < 2) return {{"success", true}};

    std::vector<size_t> order(count);
    for (size_t i = 0; i < count; i++) order[i] = count - 1 - i;
    svc->playlist_undo_backup(playlistIndex);
    svc->reorder_items(playlistIndex, order.data(), count);
    return {{"success", true}};
}


// ========== playlist.reorder
json PlaylistReorder(const json& params) {
    auto* svc = s_playlistService;
    size_t playlistIndex = params.value("playlist", SIZE_MAX);
    auto newOrder = params.value("newOrder", json::array());

    if (playlistIndex == SIZE_MAX) playlistIndex = svc->get_active_playlist();
    if (playlistIndex >= svc->get_playlist_count())
        return {{"success", false}, {"error", "Invalid playlist index"}};
    if (svc->playlist_lock_is_present(playlistIndex))
        return MakePlaylistLockedError(playlistIndex);

    size_t count = svc->playlist_get_item_count(playlistIndex);
    if (newOrder.size() != count)
        return {{"success", false}, {"error", "newOrder length mismatch"}, {"expected", count}, {"got", newOrder.size()}};

    std::vector<size_t> order(count);
    for (size_t i = 0; i < count; i++) {
        if (!newOrder[i].is_number())
            return {{"success", false}, {"error", "newOrder must contain numbers"}};
        size_t idx = newOrder[i].get<size_t>();
        if (idx >= count)
            return {{"success", false}, {"error", "Index out of range"}, {"index", idx}};
        order[i] = idx;
    }

    svc->playlist_undo_backup(playlistIndex);
    svc->reorder_items(playlistIndex, order.data(), count);
    return {{"success", true}, {"playlist", playlistIndex}, {"itemCount", count}};
}


// ========== playlist.addPathsSequential - sequential path add ==========
json PlaylistAddPathsSequential(const json& params) {
    auto svc = s_playlistService;
    size_t playlistIndex = params.value("playlist", SIZE_MAX);
    auto paths = params.value("paths", json::array());

    if (playlistIndex == SIZE_MAX) {
        playlistIndex = svc->get_active_playlist();
    }
    if (playlistIndex >= svc->get_playlist_count()) {
        return {{"success", false}, {"error", "Invalid playlist index"}};
    }
    if (svc->playlist_lock_is_present(playlistIndex)) {
        return MakePlaylistLockedError(playlistIndex);
    }
    if (paths.empty()) {
        return {{"success", false}, {"error", "No paths specified"}};
    }

    auto result = svc->add_paths_sequential(playlistIndex, paths);

    return {
        {"success", true}, {"playlist", playlistIndex},
        {"addedCount", result.addedCount}, {"order", result.order}
    };
}


// ========== playlist.addPathsAsync - async path add ==========
json PlaylistAddPathsAsync(const json& params) {
    auto svc = s_playlistService;
    size_t playlistIndex = params.value("playlist", SIZE_MAX);
    auto paths = params.value("paths", json::array());

    if (playlistIndex == SIZE_MAX) {
        playlistIndex = svc->get_active_playlist();
    }
    if (playlistIndex >= svc->get_playlist_count()) {
        return {{"success", false}, {"error", "Invalid playlist index"}};
    }
    if (svc->playlist_lock_is_present(playlistIndex)) {
        return MakePlaylistLockedError(playlistIndex);
    }
    if (paths.empty()) {
        return {{"success", false}, {"error", "No paths specified"}};
    }

    std::string operationId = GenerateOperationId();

    auto info = svc->start_add_paths_async(playlistIndex, paths, operationId,
        [operationId](size_t addedCount, size_t totalCount) {
            fb2k::inMainThread([opId = operationId, addedCount, totalCount]() {
                WebViewContext::GetInstance().BroadcastEvent("playlist:addComplete", {
                    {"operationId", opId}, {"success", true},
                    {"addedCount", addedCount}, {"totalCount", totalCount}
                });
            });
        }
    );

    if (info.validPathCount == 0) {
        return {
            {"success", false}, {"error", "No valid paths specified"},
            {"invalidCount", info.invalidCount}
        };
    }

    return {
        {"success", true}, {"operationId", operationId},
        {"status", "pending"}, {"totalCount", info.validPathCount},
        {"invalidCount", info.invalidCount}
    };
}


// ========== playlist.replaceAllAndPlay - atomic clear+add+play ==========
json PlaylistReplaceAllAndPlay(const json& params) {
    auto svc = s_playlistService;
    auto pbSvc = s_playbackServiceLocal;
    size_t playlistIndex = params.value("playlist", SIZE_MAX);
    auto paths = params.value("paths", json::array());
    size_t playIndex = params.value("playIndex", static_cast<size_t>(0));
    bool stopFirst = params.value("stopFirst", true);
    bool autoPlay = params.value("autoPlay", true);

    if (playlistIndex == SIZE_MAX) {
        playlistIndex = svc->get_active_playlist();
    }
    if (playlistIndex >= svc->get_playlist_count()) {
        return { {"success", false}, {"error", "Invalid playlist index"} };
    }
    if (svc->playlist_lock_is_present(playlistIndex)) {
        return MakePlaylistLockedError(playlistIndex);
    }
    if (paths.empty()) {
        return { {"success", false}, {"error", "No paths specified"} };
    }

    // Step 1: stop playback
    if (stopFirst && pbSvc->is_playing()) {
        pbSvc->stop();
    }

    // Step 2-4: clear + add
    auto result = svc->replace_all(playlistIndex, paths);

    if (result.addedCount == 0) {
        return {
            {"success", false}, {"error", "No valid tracks found"},
            {"clearedCount", result.clearedCount}, {"invalidCount", result.invalidCount}
        };
    }

    // Step 5: activate + play
    svc->set_active_playlist(playlistIndex);
    if (playIndex >= result.totalCount) {
        playIndex = 0;
    }
    if (autoPlay) {
        svc->execute_default_action(playlistIndex, playIndex);
    } else {
        svc->set_focus_item(playlistIndex, playIndex);
    }

    return {
        {"success", true}, {"playlist", playlistIndex},
        {"clearedCount", result.clearedCount}, {"addedCount", result.addedCount},
        {"totalCount", result.totalCount}, {"playIndex", playIndex}
    };
}


// ========== playlist.reorderPlaylists — 重排播放列表顺序 ==========
json PlaylistReorderPlaylists(const json& params) {
    auto* svc = s_playlistService;
    auto newOrder = params.value("newOrder", json::array());
    size_t count = svc->get_playlist_count();

    if (newOrder.size() != count) {
        return {
            {"success", false},
            {"error", "newOrder length mismatch"},
            {"expected", count},
            {"got", newOrder.size()}
        };
    }

    pfc::array_t<size_t> order;
    order.set_size(count);

    for (size_t i = 0; i < count; i++) {
        if (!newOrder[i].is_number()) {
            return {{"success", false}, {"error", "newOrder must contain numbers"}};
        }
        size_t idx = newOrder[i].get<size_t>();
        if (idx >= count) {
            return {{"success", false}, {"error", "Index out of range"}, {"index", idx}};
        }
        order[i] = idx;
    }

    bool ok = svc->reorder_playlists(order.get_ptr(), count);
    return {{"success", ok}, {"count", count}};
}


// ========== playlist.getAvailableColumns — 获取 DUI 可用列定义 ==========
json PlaylistGetAvailableColumns(const json& params) {
    json columns = json::array();
    
    for (auto provider : fb2k::playlistColumnProvider::enumerate()) {
        size_t numCols = provider->numColumns();
        for (size_t i = 0; i < numCols; i++) {
            auto id = provider->columnID(i);
            auto flags = provider->columnFlags(i);
            
            std::string align = "left";
            if (flags & fb2k::playlistColumnProvider::flag_alignRight) align = "right";
            else if (flags & fb2k::playlistColumnProvider::flag_alignCenter) align = "center";
            
            json col = {
                {"id", pfc::print_guid(id).c_str()},
                {"name", provider->columnName(i)->c_str()},
                {"pattern", provider->columnFormatSpec(i)->c_str()},
                {"alignment", align},
                {"numeric", (flags & fb2k::playlistColumnProvider::flag_numeric) != 0}
            };
            
            auto sortScript = provider->columnSortScript(i);
            if (sortScript.is_valid() && sortScript->length() > 0) {
                col["sortPattern"] = sortScript->c_str();
            }
            
            columns.push_back(col);
        }
    }
    
    return columns;
}

} // namespace

void RegisterPlaylistApi() {
    auto& bridge = BridgeCore::GetInstance();

    bridge.RegisterApi("playlist.getCount", PlaylistGetCount);
    bridge.RegisterApi("playlist.getAll", PlaylistGetAll);
    bridge.RegisterApi("playlist.getActive", PlaylistGetActive);
    bridge.RegisterApi("playlist.setActive", PlaylistSetActive);
    bridge.RegisterApi("playlist.getPlaying", PlaylistGetPlaying);
    bridge.RegisterApi("playlist.create", PlaylistCreate);
    bridge.RegisterApi("playlist.remove", PlaylistRemove);
    bridge.RegisterApi("playlist.rename", PlaylistRename);
    bridge.RegisterApi("playlist.clear", PlaylistClear);
    bridge.RegisterApi("playlist.insertTracks", PlaylistInsertTracks);
    bridge.RegisterApi("playlist.getTrackCount", PlaylistGetTrackCount);
    bridge.RegisterApi("playlist.getTracks", PlaylistGetTracks);
    bridge.RegisterApi("playlist.getSelectedTracks", PlaylistGetSelectedTracks);
    bridge.RegisterApi("playlist.setSelection", PlaylistSetSelection);
    bridge.RegisterApi("playlist.removeTracks", PlaylistRemoveTracks);
    bridge.RegisterApi("playlist.removeSelectedTracks", PlaylistRemoveSelectedTracks);
    bridge.RegisterApi("playlist.moveTracks", PlaylistMoveTracks);
    bridge.RegisterApi("playlist.playTrack", PlaylistPlayTrack);
    bridge.RegisterApi("playlist.focusTrack", PlaylistFocusTrack);
    bridge.RegisterApi("playlist.getFocusTrack", PlaylistGetFocusTrack);
    bridge.RegisterApi("playlist.sort", PlaylistSort);
    bridge.RegisterApi("playlist.shuffle", PlaylistShuffle);
    bridge.RegisterApi("playlist.undo", PlaylistUndo);
    bridge.RegisterApi("playlist.redo", PlaylistRedo);
    bridge.RegisterApi("playlist.isAutoplaylist", PlaylistIsAutoplaylist);
    bridge.RegisterApi("playlist.createAutoplaylist", PlaylistCreateAutoplaylist);
    bridge.RegisterApi("playlist.convertToAutoplaylist", PlaylistConvertToAutoplaylist);
    bridge.RegisterApi("playlist.removeAutoplaylist", PlaylistRemoveAutoplaylist);
    bridge.RegisterApi("playlist.getAutoplaylistInfo", PlaylistGetAutoplaylistInfo);
    bridge.RegisterApi("playlist.getAutoplaylistQuery", PlaylistGetAutoplaylistQuery);
    bridge.RegisterApi("playlist.duplicate", PlaylistDuplicate);
    bridge.RegisterApi("playlist.addPaths", PlaylistAddPaths, {{"paths", SecurityLevel::MediaRead, true}});
    bridge.RegisterApi("playlist.addHandles", PlaylistAddHandles);
    bridge.RegisterApi("playlist.getLockInfo", PlaylistGetLockInfo);
    bridge.RegisterApi("playlist.isLocked", PlaylistIsLocked);
    bridge.RegisterApi("playlist.getSelection", PlaylistGetSelection);
    bridge.RegisterApi("playlist.selectAll", PlaylistSelectAll);
    bridge.RegisterApi("playlist.deselectAll", PlaylistDeselectAll);
    bridge.RegisterApi("playlist.getFocusedTrack", PlaylistGetFocusedTrack);
    bridge.RegisterApi("playlist.setFocusedTrack", PlaylistSetFocusedTrack);
    bridge.RegisterApi("playlist.reverse", PlaylistReverse);
    bridge.RegisterApi("playlist.reorder", PlaylistReorder);
    bridge.RegisterApi("playlist.addPathsSequential", PlaylistAddPathsSequential, {{"paths", SecurityLevel::MediaRead, true}});
    bridge.RegisterApi("playlist.addPathsAsync", PlaylistAddPathsAsync, {{"paths", SecurityLevel::MediaRead, true}});
    bridge.RegisterApi("playlist.replaceAllAndPlay", PlaylistReplaceAllAndPlay, {{"paths", SecurityLevel::MediaRead, true}});
    bridge.RegisterApi("playlist.reorderPlaylists", PlaylistReorderPlaylists);
    bridge.RegisterApi("playlist.getAvailableColumns", PlaylistGetAvailableColumns);
    // deferred：全表求键与序列化在 CPU worker 上跑
    bridge.RegisterApiDeferred("playlist.getGroupRuns", PlaylistGetGroupRuns);
}
