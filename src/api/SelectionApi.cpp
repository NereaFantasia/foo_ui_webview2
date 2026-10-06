#include "pch.h"
#include "api/SelectionApi.h"
#include "api/PlaylistApi.h"
#include "api/PlaylistTarget.h"
#include "api/TrackRow.h"
#include "api/TypedApi.h"
#include "api/generated/SelectionSchema.h"
#include "utils/SubsongUtils.h"

// ============================================
// Selection API Implementation
// ============================================
//
// 提供选择相关的 API：
// - selection.getViewerMode    获取用户的 Selection Viewer 偏好
// - selection.getViewingTrack  获取当前应该显示的曲目（带 Fallback 逻辑）
// - selection.get              获取当前全局选择
// 形状由 src/api/schema/selection.ts 声明，结构体与参数解析来自生成的 SelectionSchema.h。

// ============================================
// Helper Functions
// ============================================

namespace {

namespace selection = api::selection;

// 获取 Selection Viewer 模式字符串
// 基于 ui_selection_manager::get_selection_type() 的返回值
std::string GetViewerModeString() {
    auto selMgr = ui_selection_manager::get();
    GUID type = selMgr->get_selection_type();

    // 比较 contextmenu_item::caller_now_playing
    // 如果类型是 now_playing，则用户偏好是 "prefer_playing"
    // 否则是 "prefer_selection"
    if (type == contextmenu_item::caller_now_playing) {
        return "prefer_playing";
    }
    return "prefer_selection";
}

// 获取选择类型的字符串表示
std::string GetSelectionTypeString(const GUID& type) {
    if (type == contextmenu_item::caller_now_playing) {
        return "now_playing";
    }
    if (type == contextmenu_item::caller_active_playlist_selection) {
        return "active_playlist_selection";
    }
    if (type == contextmenu_item::caller_active_playlist) {
        return "active_playlist";
    }
    if (type == contextmenu_item::caller_playlist_manager) {
        return "playlist_manager";
    }
    if (type == contextmenu_item::caller_media_library_viewer) {
        return "media_library_viewer";
    }
    return "unknown";
}

// 队列与选择共用的 handle 串：原生路径，仅 subsong > 0 时附加 "|subsong:N"。
std::string HandleStringOf(const metadb_handle_ptr& track) {
    pfc::string8 nativePath;
    filesystem::g_get_native_path(track->get_path(), nativePath);
    std::string handlePath = nativePath.get_ptr();
    t_uint32 subsong = track->get_subsong_index();
    if (subsong > 0) {
        handlePath += "|subsong:" + std::to_string(subsong);
    }
    return handlePath;
}

} // namespace

// ==========================================================================
// Selection API handler functions
// ==========================================================================
namespace {


// ========== selection.getViewerMode ==========
// 获取用户在 Preferences > Display > Selection Viewers 中的设置
api::Result<selection::GetViewerModeResult> SelectionGetViewerMode(const selection::GetViewerModeParams& /*params*/) {
    selection::GetViewerModeResult result;
    result.mode = GetViewerModeString();
    return result;
}


// ========== selection.getViewingTrack ==========
// 获取当前应该显示的曲目（带 Fallback 逻辑）
// 1. 如果用户偏好是 prefer_playing 且有正在播放的曲目，返回 now_playing
// 2. 否则返回当前选择的第一个曲目
// 3. 如果都没有，fallback 到 now_playing
api::Result<selection::GetViewingTrackResult> SelectionGetViewingTrack(const selection::GetViewingTrackParams& params) {
    auto selMgr = ui_selection_manager::get();
    auto pc = playback_control::get();

    GUID type = selMgr->get_selection_type();
    const std::string mode = (type == contextmenu_item::caller_now_playing)
        ? "prefer_playing"
        : "prefer_selection";

    metadb_handle_ptr track;
    std::string source;

    // Fallback 逻辑
    if (mode == "prefer_playing") {
        // 优先使用正在播放的曲目
        if (pc->get_now_playing(track) && track.is_valid()) {
            source = "now_playing";
        } else {
            // 回退到选择
            metadb_handle_list selectionList;
            selMgr->get_selection(selectionList);
            if (selectionList.get_count() > 0) {
                track = selectionList[0];
                source = "selection";
            }
        }
    } else {
        // 优先使用选择
        metadb_handle_list selectionList;
        selMgr->get_selection(selectionList);
        if (selectionList.get_count() > 0) {
            track = selectionList[0];
            source = "selection";
        } else {
            // 回退到正在播放
            if (pc->get_now_playing(track) && track.is_valid()) {
                source = "now_playing";
            }
        }
    }

    selection::GetViewingTrackResult result;
    result.mode = mode;
    if (!track.is_valid()) {
        result.found = false;
        return result;
    }

    // 获取曲目在播放列表中的位置
    auto plm = playlist_manager::get();
    size_t playlistIndex = pfc::infinite_size;
    size_t itemIndex = pfc::infinite_size;

    if (source == "now_playing") {
        // 对于正在播放的曲目，使用 get_playing_item_location。返回 false 时 SDK 没说出参写了什么，
        // 两个都按未知处理。
        if (!plm->get_playing_item_location(&playlistIndex, &itemIndex)) {
            playlistIndex = pfc::infinite_size;
            itemIndex = pfc::infinite_size;
        }
    } else {
        // 对于选择，在活动播放列表中查找
        playlistIndex = plm->get_active_playlist();
        if (playlistIndex != pfc::infinite_size) {
            size_t count = plm->playlist_get_item_count(playlistIndex);
            for (size_t i = 0; i < count; i++) {
                metadb_handle_ptr h;
                if (plm->playlist_get_item_handle(h, playlistIndex, i) && h == track) {
                    itemIndex = i;
                    break;
                }
            }
        }
    }

    result.found = true;
    result.source = source;
    result.handle = HandleStringOf(track);

    // 列表与行成对报告：选中的曲目不在活动列表里时，只报列表会被当成「在这张列表里」。
    if (playlistIndex != pfc::infinite_size && itemIndex != pfc::infinite_size
        && playlistIndex < plm->get_playlist_count()) {
        result.playlistIndex = static_cast<std::int64_t>(playlistIndex);
        result.playlistGuid = api::PlaylistGuidOf(*GetPlaylistService(), playlistIndex);
        result.itemIndex = static_cast<std::int64_t>(itemIndex);
    }

    // 如果请求包含曲目信息
    if (params.includeTrackInfo) {
        result.track = BuildTrackRow(track);
    }

    return result;
}


// ========== selection.get ==========
// 获取当前全局选择的曲目列表
api::Result<selection::GetResult> SelectionGet(const selection::GetParams& params) {
    auto selMgr = ui_selection_manager::get();

    metadb_handle_list selectionList;
    selMgr->get_selection(selectionList);

    GUID type = selMgr->get_selection_type();

    const size_t count = selectionList.get_count();

    // 分页参数：limit 省略时按 100 封顶并报告 truncated；limit 为 0 表示全部
    const size_t offset = static_cast<size_t>(params.offset);
    size_t limit = params.limit.has_value() ? static_cast<size_t>(*params.limit) : 100;
    if (limit == 0) {
        limit = count;
    }

    bool truncated = false;
    if (count > 100 && !params.limit.has_value()) {
        truncated = true;
        limit = 100;
    }

    selection::GetResult result;
    const size_t end = std::min(offset + limit, count);
    for (size_t i = offset; i < end; i++) {
        result.handles.push_back(HandleStringOf(selectionList[i]));
    }

    result.count = static_cast<std::int64_t>(count);
    result.type = GetSelectionTypeString(type);
    result.offset = static_cast<std::int64_t>(offset);
    if (truncated) {
        result.truncated = true;
        result.hasMore = true;
    } else {
        result.hasMore = (end < count);
    }
    return result;
}


// ========== selection.getType ==========
// 获取选择类型（SMP 兼容 API）
api::Result<selection::GetTypeResult> SelectionGetType(const selection::GetTypeParams& /*params*/) {
    auto selMgr = ui_selection_manager::get();
    GUID type = selMgr->get_selection_type();

    // 类型索引参考 SMP 文档
    std::int64_t typeIndex = 0;
    if (type == contextmenu_item::caller_now_playing) {
        typeIndex = 0;
    } else if (type == contextmenu_item::caller_active_playlist_selection) {
        typeIndex = 1;
    } else if (type == contextmenu_item::caller_active_playlist) {
        typeIndex = 2;
    } else if (type == contextmenu_item::caller_playlist_manager) {
        typeIndex = 3;
    } else if (type == contextmenu_item::caller_media_library_viewer) {
        typeIndex = 5;
    }

    selection::GetTypeResult result;
    result.type = typeIndex;
    result.typeName = GetSelectionTypeString(type);
    return result;
}


// ========== selection.set ==========
// 设置当前选择
api::Result<selection::SetResult> SelectionSet(const selection::SetParams& params) {
    // 解析 handles：拆 path|subsong:N，规范化后建 handle，与 playlist 里的曲目同一身份
    metadb_handle_list items;

    for (const std::string& handleString : params.handles) {
        auto [filePath, subsongIndex] = SubsongUtils::ParseSubsongPath(handleString);
        metadb_handle_ptr handle = SubsongUtils::CreateCanonicalHandle(filePath, subsongIndex);
        if (handle.is_valid()) {
            items.add_item(handle);
        }
    }

    if (items.get_count() == 0) {
        return api::Fail("No valid handles found", ApiErrorCode::INVALID_PARAMS);
    }

    // 获取 holder 并设置选择
    try {
        auto selMgr = ui_selection_manager::get();
        auto holder = selMgr->acquire();
        if (!holder.is_valid()) {
            return api::Fail("Failed to acquire selection holder", ApiErrorCode::OPERATION_FAILED);
        }
        holder->set_selection(items);
        selection::SetResult result;
        result.count = static_cast<std::int64_t>(items.get_count());
        return result;
    } catch (...) {
        return api::Fail("Exception while setting selection", ApiErrorCode::OPERATION_FAILED);
    }
}


// ========== selection.setPlaylistTracking ==========
// 设置播放列表跟踪模式；mode 的取值已由生成的解析器限定为 selection / playlist
api::Result<selection::SetPlaylistTrackingResult> SelectionSetPlaylistTracking(const selection::SetPlaylistTrackingParams& params) {
    try {
        auto selMgr = ui_selection_manager::get();
        auto holder = selMgr->acquire();

        if (!holder.is_valid()) {
            return api::Fail("Failed to acquire selection holder", ApiErrorCode::OPERATION_FAILED);
        }

        if (params.mode == "playlist") {
            holder->set_playlist_tracking();
        } else {
            holder->set_playlist_selection_tracking();
        }

        selection::SetPlaylistTrackingResult result;
        result.mode = params.mode;
        return result;
    } catch (...) {
        return api::Fail("Exception while setting tracking mode", ApiErrorCode::OPERATION_FAILED);
    }
}

} // namespace

void RegisterSelectionApi() {
    api::RegisterApi("selection.getViewerMode", SelectionGetViewerMode);
    api::RegisterApi("selection.getViewingTrack", SelectionGetViewingTrack);
    api::RegisterApi("selection.get", SelectionGet);
    api::RegisterApi("selection.getType", SelectionGetType);
    api::RegisterApi("selection.set", SelectionSet);
    api::RegisterApi("selection.setPlaylistTracking", SelectionSetPlaylistTracking);

    LOG("Selection API registered");
}
