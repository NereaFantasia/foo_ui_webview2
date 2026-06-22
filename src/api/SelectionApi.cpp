#include "pch.h"
#include "api/SelectionApi.h"
#include "api/BridgeCore.h"
#include "api/PlaybackApi.h"  // 用于 GetTrackInfo()

// ============================================
// Selection API Implementation
// ============================================
// 
// 提供选择相关的 API：
// - selection.getViewerMode    获取用户的 Selection Viewer 偏好
// - selection.getViewingTrack  获取当前应该显示的曲目（带 Fallback 逻辑）
// - selection.get              获取当前全局选择

// ============================================
// Helper Functions
// ============================================

namespace {

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

} // namespace

// ============================================
// API Registration
// ============================================


// ==========================================================================
// Selection API handler functions
// ==========================================================================
namespace {


// ========== selection.getViewerMode ==========
// 获取用户在 Preferences > Display > Selection Viewers 中的设置
// 返回 "prefer_playing" 或 "prefer_selection"
json SelectionGetViewerMode(const json& params) {
    std::string mode = GetViewerModeString();
    return {
        {"mode", mode}
    };
}


// ========== selection.getViewingTrack ==========
// 获取当前应该显示的曲目（带 Fallback 逻辑）
// 1. 如果用户偏好是 prefer_playing 且有正在播放的曲目，返回 now_playing
// 2. 否则返回当前选择的第一个曲目
// 3. 如果都没有，fallback 到 now_playing
json SelectionGetViewingTrack(const json& params) {
    auto selMgr = ui_selection_manager::get();
    auto pc = playback_control::get();
    
    GUID type = selMgr->get_selection_type();
    std::string mode = (type == contextmenu_item::caller_now_playing) 
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
            metadb_handle_list selection;
            selMgr->get_selection(selection);
            if (selection.get_count() > 0) {
                track = selection[0];
                source = "selection";
            }
        }
    } else {
        // 优先使用选择
        metadb_handle_list selection;
        selMgr->get_selection(selection);
        if (selection.get_count() > 0) {
            track = selection[0];
            source = "selection";
        } else {
            // 回退到正在播放
            if (pc->get_now_playing(track) && track.is_valid()) {
                source = "now_playing";
            }
        }
    }
    
    if (!track.is_valid()) {
        return {
            {"success", true},
            {"found", false},
            {"mode", mode}
        };
    }
    
    // 获取曲目在播放列表中的位置
    auto plm = playlist_manager::get();
    size_t playlistIndex = pfc::infinite_size;
    size_t itemIndex = pfc::infinite_size;
    
    if (source == "now_playing") {
        // 对于正在播放的曲目，使用 get_playing_item_location
        plm->get_playing_item_location(&playlistIndex, &itemIndex);
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
    
    // 构建 handle 字符串
    pfc::string8 nativePath;
    filesystem::g_get_native_path(track->get_path(), nativePath);
    std::string handlePath = nativePath.get_ptr();
    t_uint32 subsong = track->get_subsong_index();
    if (subsong > 0) {
        handlePath += "|subsong:" + std::to_string(subsong);
    }
    
    json result = {
        {"success", true},
        {"found", true},
        {"mode", mode},
        {"source", source},
        {"handle", handlePath}
    };
    
    // 如果找到位置信息，添加到结果
    if (playlistIndex != pfc::infinite_size) {
        result["playlistIndex"] = playlistIndex;
    }
    if (itemIndex != pfc::infinite_size) {
        result["itemIndex"] = itemIndex;
    }
    
    // 如果请求包含曲目信息
    if (params.value("includeTrackInfo", false)) {
        result["track"] = GetTrackInfo(track);
    }
    
    return result;
}


// ========== selection.get ==========
// 获取当前全局选择的曲目列表
json SelectionGet(const json& params) {
    auto selMgr = ui_selection_manager::get();
    
    metadb_handle_list selection;
    selMgr->get_selection(selection);
    
    GUID type = selMgr->get_selection_type();
    std::string typeStr = GetSelectionTypeString(type);
    
    size_t count = selection.get_count();
    
    // 分页参数
    size_t offset = params.value("offset", static_cast<size_t>(0));
    size_t limit = params.value("limit", static_cast<size_t>(100));
    
    // 如果 limit 为 0，返回全部
    if (limit == 0) {
        limit = count;
    }
    
    // 阈值优化：如果选择数量 > 100 且没有指定 limit，只返回 count
    bool truncated = false;
    if (count > 100 && !params.contains("limit")) {
        truncated = true;
        limit = 100;
    }
    
    json handles = json::array();
    size_t end = std::min(offset + limit, count);
    
    for (size_t i = offset; i < end; i++) {
        metadb_handle_ptr h = selection[i];
        pfc::string8 nativePath;
        filesystem::g_get_native_path(h->get_path(), nativePath);
        std::string handlePath = nativePath.get_ptr();
        t_uint32 subsong = h->get_subsong_index();
        if (subsong > 0) {
            handlePath += "|subsong:" + std::to_string(subsong);
        }
        handles.push_back(handlePath);
    }
    
    json result = {
        {"count", count},
        {"type", typeStr},
        {"handles", handles},
        {"offset", offset}
    };
    
    if (truncated) {
        result["truncated"] = true;
        result["hasMore"] = true;
    } else {
        result["hasMore"] = (end < count);
    }
    
    return result;
}


// ========== selection.getType ==========
// 获取选择类型（SMP 兼容 API）
// -- (0-6) ------------------------------------------------------------
json SelectionGetType(const json& params) {
    auto selMgr = ui_selection_manager::get();
    GUID type = selMgr->get_selection_type();
    
    // 类型索引参考 SMP 文档
    int typeIndex = 0;
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
    
    return {
        {"type", typeIndex},
        {"typeName", GetSelectionTypeString(type)}
    };
}


// ========== selection.set ==========
// 设置当前选择
// 参数: { handles: ["path|subsong:N", ...] }
json SelectionSet(const json& params) {
    // -- handles ------------------------------------------------------
    if (!params.contains("handles") || !params["handles"].is_array()) {
        return {
            {"success", false},
            {"error", "handles array is required"}
        };
    }
    
    const auto& handlesJson = params["handles"];
    if (handlesJson.empty()) {
        return {
            {"success", false},
            {"error", "handles array is empty"}
        };
    }
    
    // 解析 handles
    metadb_handle_list items;
    auto mm = metadb::get();
    
    for (const auto& handleJson : handlesJson) {
        if (!handleJson.is_string()) continue;
        
        std::string path = handleJson.get<std::string>();
        
        // 解析 path|subsong:N 格式
        std::string filePath = path;
        t_uint32 subsongIndex = 0;
        
        size_t pos = path.find("|subsong:");
        if (pos != std::string::npos) {
            filePath = path.substr(0, pos);
            try {
                subsongIndex = static_cast<t_uint32>(std::stoul(path.substr(pos + 9)));
            } catch (...) {
                subsongIndex = 0;
            }
        }
        
        // 创建 handle
        pfc::string8 pathStr(filePath.c_str());
        metadb_handle_ptr handle = mm->handle_create(pathStr.get_ptr(), subsongIndex);
        if (handle.is_valid()) {
            items.add_item(handle);
        }
    }
    
    if (items.get_count() == 0) {
        return {
            {"success", false},
            {"error", "No valid handles found"}
        };
    }
    
    // 获取 holder 并设置选择
    try {
        auto selMgr = ui_selection_manager::get();
        auto holder = selMgr->acquire();
        if (holder.is_valid()) {
            holder->set_selection(items);
            return {
                {"success", true},
                {"count", items.get_count()}
            };
        } else {
            return {
                {"success", false},
                {"error", "Failed to acquire selection holder"}
            };
        }
    } catch (...) {
        return {
            {"success", false},
            {"error", "Exception while setting selection"}
        };
    }
}


// ========== selection.setPlaylistTracking ==========
// 设置播放列表跟踪模式
// 参数: { mode: "selection" | "playlist" }
json SelectionSetPlaylistTracking(const json& params) {
    std::string mode = params.value("mode", "selection");
    
    try {
        auto selMgr = ui_selection_manager::get();
        auto holder = selMgr->acquire();
        
        if (!holder.is_valid()) {
            return {
                {"success", false},
                {"error", "Failed to acquire selection holder"}
            };
        }
        
        if (mode == "playlist") {
            holder->set_playlist_tracking();
        } else {
            holder->set_playlist_selection_tracking();
        }
        
        return {
            {"success", true},
            {"mode", mode}
        };
    } catch (...) {
        return {
            {"success", false},
            {"error", "Exception while setting tracking mode"}
        };
    }
}

} // namespace

void RegisterSelectionApi() {
    auto& bridge = BridgeCore::GetInstance();

    bridge.RegisterApi("selection.getViewerMode", SelectionGetViewerMode);
    bridge.RegisterApi("selection.getViewingTrack", SelectionGetViewingTrack);
    bridge.RegisterApi("selection.get", SelectionGet);
    bridge.RegisterApi("selection.getType", SelectionGetType);
    bridge.RegisterApi("selection.set", SelectionSet);
    bridge.RegisterApi("selection.setPlaylistTracking", SelectionSetPlaylistTracking);

    LOG("Selection API registered");
}
