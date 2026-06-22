#include "pch.h"
#include "api/PlaylistApi.h"
#include "api/ApiConstants.h"
#include "api/BridgeCore.h"
#include "api/ErrorEnvelope.h"
#include "core/WebViewContext.h"
#include "interfaces/Fb2kPlaylistService.h"
#include "interfaces/Fb2kPlaybackService.h"
#include <atomic>
#include <random>

// ============================================
// Helper Functions
// ============================================

// 生成随机字符串用于操作ID
static std::string GenerateRandomString(size_t length) {
    static const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, sizeof(chars) - 2);
    
    std::string result;
    result.reserve(length);
    for (size_t i = 0; i < length; i++) {
        result += chars[dis(gen)];
    }
    return result;
}

struct ParsedPlayablePath {
    std::string path;
    t_uint32 subsong = 0;
    bool hasSubsong = false;
};

// 检测 URL 是否是 playlist wrapper(.pls/.m3u/.cue 等),需要展开成多个 handle。
// 非 wrapper 的本地路径 / 流媒体直链可以直接 metadb::handle_create 同步处理,
// 完全绕开 process_locations_async 的 fb2k 进度对话框。
static bool LooksLikePlaylistWrapper(const std::string& url) {
    if (url.empty()) return false;
    auto dotPos = url.find_last_of('.');
    auto sepPos = url.find_last_of("/\\");
    if (dotPos == std::string::npos) return false;
    if (sepPos != std::string::npos && dotPos < sepPos) return false;

    std::string ext = url.substr(dotPos);
    auto q = ext.find_first_of("?#");
    if (q != std::string::npos) ext = ext.substr(0, q);
    std::transform(ext.begin(), ext.end(), ext.begin(),
        [](unsigned char c) { return static_cast<char>(::tolower(c)); });

    static const char* const kWrappers[] = {
        ".pls", ".m3u", ".m3u8", ".asx", ".wpl", ".xspf", ".fpl", ".cue"
    };
    for (auto w : kWrappers) {
        if (ext == w) return true;
    }
    return false;
}

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
static void ResolvePathsToHandles(const json& paths, metadb_handle_list& outItems, size_t& invalidCount) {
    auto piif = playlist_incoming_item_filter::get();
    auto mdb = metadb::get();

    // 第一遍：分离 subsong 路径（直接 handle_create）和普通路径（收集后批量处理）
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

        // subsong 指定时直接 handle_create，避免 process_locations 把后缀当成路径文本
        if (parsed.hasSubsong) {
            metadb_handle_ptr handle = mdb->handle_create(parsed.path.c_str(), parsed.subsong);
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

    // 有 subsong 混合时，按原始顺序合并结果
    // process_locations 会保持输入路径顺序，所以我们按 batch 路径顺序逐段取
    // 注意：一条路径可能解析出多个 handle（如 CUE），所以用路径边界来分段
    // 简化处理：subsong handle 按序插入，batch 结果整体追加（保持 process_locations 内部顺序）
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
    auto mdb = metadb::get();
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

        metadb_handle_ptr handle = mdb->handle_create(path.c_str(), subsong);
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
static std::string GenerateOperationId() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    return "op_" + std::to_string(ms) + "_" + GenerateRandomString(6);
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
    
    // First try meta tag (zero cost), only fallback to format_title if needed
    int rating = getMetaInt("rating");
    // Fallback to foo_playcount virtual field (more expensive)
    if (rating == 0) {
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
            // Silently ignore — falls through to clamping
        }
    }
    // Clamp to valid range
    if (rating < 0) rating = 0;
    if (rating > 5) rating = 5;
    
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
        {"artist", getMeta("artist")},
        {"album", getMeta("album")},
        {"albumArtist", getMeta("album artist")},
        {"genre", getMeta("genre")},
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
        {"composer", getMeta("composer")},
        {"comment", getMeta("comment")},
    };
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
    size_t playlist, size_t start, size_t count, const json& formats) const {
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
    auto v2recs = mdb2->queryMultiSimple(items);

    json tracks = json::array();
    for (size_t i = 0; i < items.get_count(); i++) {
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

IPlaylistService::AddPathsSequentialResult Fb2kPlaylistService::add_paths_sequential(
    size_t playlist, const json& paths) {
    auto plm = playlist_manager::get();
    auto piif = playlist_incoming_item_filter::get();
    auto mdb = metadb::get();

    json resultIndices = json::array();
    size_t addedCount = 0;
    plm->playlist_undo_backup(playlist);

    pfc::string_list_impl batchPaths;
    struct SeqEntry { bool isSubsong; size_t batchIdx; metadb_handle_ptr handle; };
    std::vector<SeqEntry> entries;
    size_t batchIdx = 0;

    for (const auto& path : paths) {
        if (!path.is_string()) continue;
        ParsedPlayablePath parsed = ParsePlayablePath(path.get<std::string>());
        std::string pathStr = parsed.path;
        if (pathStr.empty()) continue;
        if (pathStr.length() > ApiLimits::MAX_STREAM_URL_LENGTH) continue;

        if (parsed.hasSubsong) {
            metadb_handle_ptr handle = mdb->handle_create(pathStr.c_str(), parsed.subsong);
            if (handle.is_valid()) {
                entries.push_back({true, 0, handle});
            }
        } else {
            entries.push_back({false, batchIdx++, metadb_handle_ptr()});
            batchPaths.add_item(pathStr.c_str());
        }
    }

    metadb_handle_list batchResolved;
    if (batchPaths.get_count() > 0) {
        piif->process_locations(batchPaths, batchResolved, true, nullptr, nullptr, core_api::get_main_window());
    }

    for (const auto& entry : entries) {
        if (entry.isSubsong) {
            size_t insertPos = plm->playlist_get_item_count(playlist);
            metadb_handle_list items;
            items.add_item(entry.handle);
            plm->playlist_insert_items(playlist, insertPos, items, bit_array_false());
            resultIndices.push_back(insertPos);
            addedCount++;
        }
    }
    if (batchResolved.get_count() > 0) {
        size_t insertPos = plm->playlist_get_item_count(playlist);
        plm->playlist_insert_items(playlist, insertPos, batchResolved, bit_array_false());
        for (size_t j = 0; j < batchResolved.get_count(); j++) {
            resultIndices.push_back(insertPos + j);
        }
        addedCount += batchResolved.get_count();
    }

    return { addedCount, resultIndices };
}

IPlaylistService::AsyncAddPathsInfo Fb2kPlaylistService::start_add_paths_async(
    size_t playlist, const json& paths, const std::string& operationId,
    std::function<void(size_t addedCount, size_t totalCount)> onComplete) {
    pfc::list_t<pfc::string8> pathStrings;
    pfc::list_t<const char*> urlList;
    size_t invalidCount = 0;

    for (const auto& p : paths) {
        if (p.is_string()) {
            ParsedPlayablePath parsed = ParsePlayablePath(p.get<std::string>());
            if (parsed.path.empty()) { invalidCount++; continue; }
            if (parsed.path.length() > ApiLimits::MAX_STREAM_URL_LENGTH) {
                invalidCount++;
                continue;
            }
            pathStrings.add_item(pfc::string8(parsed.path.c_str()));
        } else { invalidCount++; }
    }
    for (size_t i = 0; i < pathStrings.get_count(); i++) {
        urlList.add_item(pathStrings[i].get_ptr());
    }
    if (urlList.get_count() == 0) {
        return { 0, invalidCount };
    }

    size_t totalCount = urlList.get_count();

    // 分流:本地路径 / 流媒体直链走 metadb::handle_create 同步零弹窗;
    // 仅 .pls / .m3u / .cue 等 wrapper 走 process_locations_async 展开。
    auto mdb = metadb::get();
    metadb_handle_list fastHandles;
    pfc::list_t<pfc::string8> slowStrings;
    pfc::list_t<const char*> slowList;
    for (size_t i = 0; i < urlList.get_count(); i++) {
        const char* url = urlList[i];
        if (LooksLikePlaylistWrapper(url)) {
            slowStrings.add_item(pfc::string8(url));
        } else {
            metadb_handle_ptr handle = mdb->handle_create(url, 0);
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
    return svc->get_tracks_json(playlistIndex, start, count, extraFormats);
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
}
