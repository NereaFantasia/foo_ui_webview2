#include "pch.h"
#include "api/PlaybackApi.h"
#include "api/BridgeCore.h"
#include "interfaces/IPlaybackService.h"
#include "interfaces/Fb2kPlaybackService.h"
#include "utils/SubsongUtils.h"

// ============================================
// Helper Functions
// ============================================

using SubsongUtils::ParseSubsongPath;

json GetTrackInfo(metadb_handle_ptr track) {
    if (!track.is_valid()) return nullptr;

    std::string rawPath = track->get_path();
    auto [rawBasePath, rawPathSubsong] = ParseSubsongPath(rawPath);

    // 输出规范化：
    // absolutePath = 纯文件路径，不带 subsong 后缀
    // fullPath = absolutePath + "|subsong:N"（仅 subsong>0）
    pfc::string8 nativePath;
    filesystem::g_get_native_path(rawBasePath.c_str(), nativePath);
    std::string absolutePath = nativePath.get_ptr();
    if (absolutePath.empty()) {
        absolutePath = rawBasePath;
    }

    t_uint32 subsongIndex = track->get_subsong_index();
    if (subsongIndex == 0 && rawPathSubsong > 0) {
        subsongIndex = rawPathSubsong;
    }

    std::string fullPath = absolutePath;
    if (subsongIndex > 0 && !absolutePath.empty()) {
        fullPath = absolutePath + "|subsong:" + std::to_string(subsongIndex);
    }

    // 保持 id 唯一性：优先使用 fullPath（含 subsong）
    std::string idPath = fullPath.empty() ? rawPath : fullPath;
    
    // Use get_info_ref() instead of deprecated get_info()
    // get_info_ref() is the recommended method since foobar2000 SDK 1.3
    metadb_info_container::ptr infoContainer = track->get_info_ref();
    if (!infoContainer.is_valid()) {
        // Fallback: return minimal info
        return {
            {"id", idPath},              // Use full path as ID (含 subsong 后缀)
            {"path", rawPath},           // Original foobar path (may be relative)
            {"absolutePath", absolutePath},  // Normalized absolute path (no subsong suffix)
            {"fullPath", fullPath},      // Absolute path with optional subsong suffix
            {"subsong", subsongIndex},
            {"duration", track->get_length()},
        };
    }
    
    const file_info& info = infoContainer->info();
    
    auto getMeta = [&](const char* name) -> std::string {
        const char* value = info.meta_get(name, 0);
        return value ? value : "";
    };
    
    auto getMetaInt = [&](const char* name) -> int {
        const char* value = info.meta_get(name, 0);
        return value ? atoi(value) : 0;
    };
    
    // Get codec info
    const char* codecValue = info.info_get("codec");
    std::string codec = codecValue ? codecValue : "";
    
    return {
        {"id", idPath},                  // Use full path as ID (含 subsong 后缀)
        {"title", getMeta("title")},
        {"artist", getMeta("artist")},
        {"album", getMeta("album")},
        {"albumArtist", getMeta("album artist")},
        {"genre", getMeta("genre")},
        {"date", getMeta("date")},
        {"trackNumber", getMetaInt("tracknumber")},
        {"discNumber", getMetaInt("discnumber")},
        {"duration", info.get_length()},
        {"path", rawPath},               // Original foobar path
        {"absolutePath", absolutePath},  // Normalized absolute path (no subsong suffix)
        {"fullPath", fullPath},          // Absolute path with optional subsong suffix
        {"subsong", subsongIndex},       // Subsong index (0 = normal file, >0 = CUE subsong)
        {"fileSize", static_cast<int64_t>(track->get_filesize())},
        {"bitrate", static_cast<int>(info.info_get_bitrate())},
        {"sampleRate", static_cast<int>(info.info_get_int("samplerate"))},
        {"channels", static_cast<int>(info.info_get_int("channels"))},
        {"codec", codec},
    };
}

// ============================================
// Playback Order Helpers
// ============================================

namespace {

enum PlaybackOrder {
    ORDER_DEFAULT = 0,
    ORDER_REPEAT_PLAYLIST = 1,
    ORDER_REPEAT_TRACK = 2,
    ORDER_RANDOM = 3,
    ORDER_SHUFFLE_TRACKS = 4,
    ORDER_SHUFFLE_ALBUMS = 5,
    ORDER_SHUFFLE_FOLDERS = 6,
};

std::string OrderToString(int order) {
    switch (order) {
        case ORDER_DEFAULT: return "default";
        case ORDER_REPEAT_PLAYLIST: return "repeat-playlist";
        case ORDER_REPEAT_TRACK: return "repeat-track";
        case ORDER_RANDOM: return "random";
        case ORDER_SHUFFLE_TRACKS: return "shuffle-tracks";
        case ORDER_SHUFFLE_ALBUMS: return "shuffle-albums";
        case ORDER_SHUFFLE_FOLDERS: return "shuffle-folders";
        default: return "default";
    }
}

int StringToOrder(const std::string& str) {
    if (str == "repeat-playlist") return ORDER_REPEAT_PLAYLIST;
    if (str == "repeat-track") return ORDER_REPEAT_TRACK;
    if (str == "random") return ORDER_RANDOM;
    if (str == "shuffle-tracks") return ORDER_SHUFFLE_TRACKS;
    if (str == "shuffle-albums") return ORDER_SHUFFLE_ALBUMS;
    if (str == "shuffle-folders") return ORDER_SHUFFLE_FOLDERS;
    return ORDER_DEFAULT;
}

} // namespace

// ============================================
// API Registration
// ============================================


// ==========================================================================
// Playback API handler functions
// ==========================================================================
namespace {

// Service injection point for testability
// Production code uses Fb2kPlaybackService; tests can swap in a mock.
// Thread safety: Set/Get must only be called during single-threaded init
// (component_init / test SetUp) before any handler is invoked.
static Fb2kPlaybackService s_defaultPlaybackService;
static IPlaybackService* s_playbackService = &s_defaultPlaybackService;

// ========== Basic Controls ==========

json PlaybackPlay(const json& params) {
    s_playbackService->play_or_unpause();
    return { {"success", true} };
}


json PlaybackPause(const json& params) {
    s_playbackService->pause(true);
    return { {"success", true} };
}


json PlaybackStop(const json& params) {
    s_playbackService->stop();
    return { {"success", true} };
}


json PlaybackPlayPause(const json& params) {
    auto* svc = s_playbackService;
    bool wasPlaying = svc->is_playing() && !svc->is_paused();
    svc->play_or_pause();
    return {
        {"success", true},
        {"isPlaying", !wasPlaying},
    };
}


// 别名，与前端 API 名称一致
json PlaybackPlayOrPause(const json& params) {
    auto* svc = s_playbackService;
    bool wasPlaying = svc->is_playing() && !svc->is_paused();
    svc->play_or_pause();
    return {
        {"success", true},
        {"isPlaying", !wasPlaying},
    };
}


json PlaybackNext(const json& params) {
    s_playbackService->next();
    return { {"success", true} };
}


json PlaybackPrevious(const json& params) {
    s_playbackService->previous();
    return { {"success", true} };
}


json PlaybackRandom(const json& params) {
    s_playbackService->start(5 /*track_command_rand*/);
    return { {"success", true} };
}


// ========== State Query ==========

json PlaybackGetState(const json& params) {
    auto* svc = s_playbackService;
    
    std::string state = "stopped";
    if (svc->is_playing()) {
        state = svc->is_paused() ? "paused" : "playing";
    }
    
    return {
        {"state", state},
        {"canSeek", svc->playback_can_seek()},
        {"canPause", true},
    };
}


json PlaybackGetPosition(const json& params) {
    auto* svc = s_playbackService;
    
    double position = svc->playback_get_position();
    auto info = svc->get_now_playing_info();
    
    return {
        {"position", position},
        {"duration", info.duration},
        {"subsong", info.subsong},
        {"path", info.path},
    };
}


json PlaybackSetPosition(const json& params) {
    // 支持 position 和 seconds 两种参数名
    double seconds = params.contains("position") ? params.value("position", 0.0) : params.value("seconds", 0.0);
    auto* svc = s_playbackService;
    
    if (!svc->playback_can_seek()) {
        return {
            {"success", false},
            {"error", "Cannot seek in current track"},
        };
    }
    
    // 获取当前曲目信息用于边界检查
    auto info = svc->get_now_playing_info();
    double duration = info.duration;
    unsigned subsong = info.subsong;
    
    // 边界检查：确保 position 在有效范围内
    if (seconds < 0) {
        seconds = 0;
    }
    if (duration > 0 && seconds > duration) {
        seconds = duration - 0.1;  // 留一点余量避免跳到下一曲
        if (seconds < 0) seconds = 0;
    }
    
    double oldPosition = svc->playback_get_position();
    svc->playback_seek(seconds);
    double newPosition = svc->playback_get_position();
    
    return { 
        {"success", true},
        {"requestedPosition", params.contains("position") ? params.value("position", 0.0) : params.value("seconds", 0.0)},
        {"actualPosition", seconds},
        {"newPosition", newPosition},
        {"oldPosition", oldPosition},
        {"duration", duration},
        {"subsong", subsong}
    };
}


// ========== Volume Control ==========

json PlaybackGetVolume(const json& params) {
    auto* svc = s_playbackService;
    float db = svc->get_volume();
    
    // dB 转换为 0-100 线性百分比（对数逆转换）
    // volume = 100 * 10^(dB/20)
    float volume;
    if (db <= -100.0f) {
        volume = 0.0f;
    } else {
        volume = 100.0f * std::pow(10.0f, db / 20.0f);
        volume = std::max(0.0f, std::min(100.0f, volume));
    }
    bool muted = svc->is_muted();
    
    return {
        {"volume", volume},
        {"volumeDb", db},
        {"muted", muted},
        {"isMuted", muted},
    };
}


json PlaybackSetVolume(const json& params) {
    auto* svc = s_playbackService;
    float volume = params.value("volume", 100.0f);
    
    // 0-100 线性百分比转换为 dB（对数刻度）
    // 使用对数转换：dB = 20 * log10(volume / 100)
    float db;
    if (volume <= 0.0f) {
        db = -100.0f;  // 静音
    } else {
        db = 20.0f * std::log10(volume / 100.0f);
        db = std::max(-100.0f, std::min(0.0f, db));
    }
    
    svc->set_volume(db);
    return { {"success", true} };
}


json PlaybackVolumeUp(const json& params) {
    s_playbackService->volume_up();
    return { {"success", true} };
}


json PlaybackVolumeDown(const json& params) {
    s_playbackService->volume_down();
    return { {"success", true} };
}


json PlaybackMute(const json& params) {
    auto* svc = s_playbackService;
    bool muted = params.value("muted", true);
    bool currentMuted = svc->is_muted();
    
    // 如果需要静音且当前未静音，或需要取消静音且当前已静音
    if (muted != currentMuted) {
        svc->volume_mute_toggle();
    }
    return { {"success", true} };
}


json PlaybackToggleMute(const json& params) {
    auto* svc = s_playbackService;
    bool currentMuted = svc->is_muted();
    svc->volume_mute_toggle();
    return { 
        {"success", true},
        {"muted", !currentMuted},
    };
}


// ========== Current Track ==========

json PlaybackGetCurrentTrack(const json& params) {
    auto trackJson = s_playbackService->get_current_track_json();
    if (trackJson.is_null()) {
        return { {"success", true}, {"found", false}, {"playing", false} };
    }
    return trackJson;
}


// ========== Playback Order ==========

json PlaybackGetPlaybackOrder(const json& params) {
    int order = s_playbackService->playback_order_get_active();
    std::string orderName = OrderToString(order);
    
    return {
        {"order", order},
        {"orderName", orderName},
        {"name", orderName},  // alias for tests
        {"orderIndex", order},
    };
}


json PlaybackSetPlaybackOrder(const json& params) {
    int order = 0;
    
    // Support both number and string order
    if (params.contains("order")) {
        auto& orderParam = params["order"];
        if (orderParam.is_number()) {
            order = orderParam.get<int>();
        } else if (orderParam.is_string()) {
            order = StringToOrder(orderParam.get<std::string>());
        }
    }
    
    s_playbackService->playback_order_set_active(order);
    
    return {
        {"success", true},
        {"order", order},
        {"orderName", OrderToString(order)},
    };
}


// ========== Stop After Current ==========

json PlaybackGetStopAfterCurrent(const json& params) {
    return {
        {"enabled", s_playbackService->get_stop_after_current()},
    };
}


json PlaybackSetStopAfterCurrent(const json& params) {
    bool enabled = params.value("enabled", false);
    s_playbackService->set_stop_after_current(enabled);
    return {
        {"success", true},
        {"enabled", enabled},
    };
}


json PlaybackToggleStopAfterCurrent(const json& params) {
    auto* svc = s_playbackService;
    svc->toggle_stop_after_current();
    return {
        {"enabled", svc->get_stop_after_current()},
    };
}


// ========== Test/Echo API ==========

json TestEcho(const json& params) {
    // Return the input message wrapped in echo field for test compatibility
    if (params.contains("message")) {
        return {{"success", true}, {"echo", params["message"]}, {"input", params}};
    }
    return {{"success", true}, {"echo", params}, {"input", params}};
}


json TestPing(const json& params) {
    return {
        {"pong", true},
        {"timestamp", static_cast<int64_t>(time(nullptr))},
    };
}

static metadb_handle_ptr CreateExplicitSubsongHandle(const std::string& filePath,
                                                     int subsongIndex) {
    auto mm = metadb::get();
    return mm->handle_create(filePath.c_str(), subsongIndex);
}

static metadb_handle_ptr ResolvePlaybackHandle(const metadb_handle_list& handles,
                                               const std::string& filePath,
                                               int subsongIndex,
                                               bool subsongRequested) {
    metadb_handle_ptr handle;

    if (handles.get_count() == 0) {
        return handle;
    }

    if (!subsongRequested) {
        return handles[0];
    }

    if (handles.get_count() > 1) {
        FB2K_console_print("[PlaybackApi] Multiple subsongs found (", handles.get_count(), "), looking for index ", subsongIndex);
        for (size_t index = 0; index < handles.get_count(); index++) {
            if (handles[index]->get_subsong_index() == subsongIndex) {
                FB2K_console_print("[PlaybackApi] Found matching subsong at position ", index);
                return handles[index];
            }
        }

        if (subsongIndex < handles.get_count()) {
            FB2K_console_print("[PlaybackApi] Using subsong by list position");
            return handles[subsongIndex];
        }

        FB2K_console_print("[PlaybackApi] Subsong not found, using first track");
        handle = handles[0];
    } else {
        handle = handles[0];
    }

    if (!handle.is_valid() || handle->get_subsong_index() != subsongIndex) {
        metadb_handle_ptr explicitHandle =
            CreateExplicitSubsongHandle(filePath, subsongIndex);
        if (explicitHandle.is_valid()) {
            FB2K_console_print("[PlaybackApi] handle_create fallback resolved subsong:",
                               std::to_string(subsongIndex).c_str());
            return explicitHandle;
        }
    }

    return handle.is_valid() ? handle : handles[0];
}

static t_size EnsureActivePlaylist(playlist_manager& pm) {
    t_size playlist = pm.get_active_playlist();
    if (playlist == pfc::infinite_size) {
        playlist = pm.create_playlist_autoname(0);
        pm.set_active_playlist(playlist);
    }
    return playlist;
}

static void InsertAndPlayHandle(playlist_manager& pm,
                                t_size playlist,
                                const metadb_handle_ptr& handle) {
    metadb_handle_list playHandles;
    playHandles.add_item(handle);

    pm.playlist_undo_backup(playlist);

    const t_size base = pm.playlist_get_item_count(playlist);
    pm.playlist_insert_items(playlist, base, playHandles, pfc::bit_array_true());
    pm.playlist_execute_default_action(playlist, base);
}


// ========== Direct Path Playback ==========

// playback.playPath - Play a single file path directly
// 支持 subsong 格式: "path/to/file.flac|subsong:2"
json PlaybackPlayPath(const json& params) {
    std::string path = params.value("path", "");
    if (path.empty()) {
        return {{"success", false}, {"error", "path is required"}};
    }

    try {
        // 解析路径和 subsong 索引
        auto [filePath, subsongIndex] = ParseSubsongPath(path);
        bool subsongRequested = (path.find("|subsong:") != std::string::npos);

        FB2K_console_print("[PlaybackApi] playPath: ", filePath.c_str(), " subsong:", std::to_string(subsongIndex).c_str());

        auto result = s_playbackService->play_single_path(filePath, subsongIndex, subsongRequested);

        if (!result.success) {
            return {
                {"success", false},
                {"error", result.error},
                {"path", filePath},
                {"subsong", subsongIndex}
            };
        }

        return {
            {"success", true},
            {"tracksAdded", result.tracksAdded},
            {"path", filePath},
            {"subsong", result.resolvedSubsong}
        };
    } catch (const std::exception& e) {
        return {{"success", false}, {"error", e.what()}};
    } catch (...) {
        return {{"success", false}, {"error", "Unknown error"}};
    }
}


// playback.playPaths - Play multiple file paths
// 支持 subsong 格式: "path/to/file.flac|subsong:2"
json PlaybackPlayPaths(const json& params) {
    if (!params.contains("paths") || !params["paths"].is_array()) {
        return {{"success", false}, {"error", "paths array is required"}};
    }

    size_t startIndex = params.value("startIndex", static_cast<size_t>(0));
    bool replace = params.value("replace", false);

    try {
        std::vector<std::string> paths;
        for (const auto& pathJson : params["paths"]) {
            paths.push_back(pathJson.get<std::string>());
        }

        auto result = s_playbackService->play_multiple_paths(paths, startIndex, replace);

        if (!result.success) {
            return {{"success", false}, {"error", result.error}};
        }

        return {
            {"success", true},
            {"tracksAdded", result.tracksAdded},
            {"startedAt", result.startedAt}
        };
    } catch (const std::exception& e) {
        return {{"success", false}, {"error", e.what()}};
    }
}


// ========== playback.getCurrentTrackIndex - 获取当前播放曲目的位置 ==========
json PlaybackGetCurrentTrackIndex(const json& params) {
    auto loc = s_playbackService->get_playing_item_location();

    if (!loc.found) {
        return {
            {"success", true},
            {"found", false},
            {"playlist", nullptr},
            {"index", nullptr}
        };
    }

    // Also get track info if requested
    bool includeTrackInfo = params.value("includeTrackInfo", false);
    json result = {
        {"success", true},
        {"found", true},
        {"playlist", loc.playlist},
        {"index", loc.index}
    };

    if (includeTrackInfo) {
        auto trackJson = s_playbackService->get_track_info_at(loc.playlist, loc.index);
        if (!trackJson.is_null()) {
            result["track"] = trackJson;
        }
    }

    return result;
}


// ========== playback.getPlayingPlaylist - 获取正在播放的播放列表 ==========
json PlaybackGetPlayingPlaylist(const json& params) {
    size_t playlist = s_playbackService->get_playing_playlist();

    if (playlist == SIZE_MAX) {
        return {{"success", true}, {"playlist", nullptr}, {"found", false}};
    }

    std::string name = s_playbackService->get_playlist_name(playlist);

    return {
        {"success", true},
        {"playlist", playlist},
        {"name", name},
        {"found", true}
    };
}

} // namespace

void RegisterPlaybackApi() {
    auto& bridge = BridgeCore::GetInstance();

    bridge.RegisterApi("playback.play", PlaybackPlay);
    bridge.RegisterApi("playback.pause", PlaybackPause);
    bridge.RegisterApi("playback.stop", PlaybackStop);
    bridge.RegisterApi("playback.playPause", PlaybackPlayPause);
    bridge.RegisterApi("playback.playOrPause", PlaybackPlayOrPause);
    bridge.RegisterApi("playback.next", PlaybackNext);
    bridge.RegisterApi("playback.previous", PlaybackPrevious);
    bridge.RegisterApi("playback.random", PlaybackRandom);
    bridge.RegisterApi("playback.getState", PlaybackGetState);
    bridge.RegisterApi("playback.getPosition", PlaybackGetPosition);
    bridge.RegisterApi("playback.setPosition", PlaybackSetPosition);
    bridge.RegisterApi("playback.getVolume", PlaybackGetVolume);
    bridge.RegisterApi("playback.setVolume", PlaybackSetVolume);
    bridge.RegisterApi("playback.volumeUp", PlaybackVolumeUp);
    bridge.RegisterApi("playback.volumeDown", PlaybackVolumeDown);
    bridge.RegisterApi("playback.mute", PlaybackMute);
    bridge.RegisterApi("playback.toggleMute", PlaybackToggleMute);
    bridge.RegisterApi("playback.getCurrentTrack", PlaybackGetCurrentTrack);
    bridge.RegisterApi("playback.getPlaybackOrder", PlaybackGetPlaybackOrder);
    bridge.RegisterApi("playback.setPlaybackOrder", PlaybackSetPlaybackOrder);
    bridge.RegisterApi("playback.getStopAfterCurrent", PlaybackGetStopAfterCurrent);
    bridge.RegisterApi("playback.setStopAfterCurrent", PlaybackSetStopAfterCurrent);
    bridge.RegisterApi("playback.toggleStopAfterCurrent", PlaybackToggleStopAfterCurrent);
    bridge.RegisterApi("test.echo", TestEcho);
    bridge.RegisterApi("test.ping", TestPing);
    bridge.RegisterApi("playback.playPath", PlaybackPlayPath, {{"path", SecurityLevel::MediaRead}});
    bridge.RegisterApi("playback.playPaths", PlaybackPlayPaths, 
        {{"paths", SecurityLevel::MediaRead, true, "", true}});
    bridge.RegisterApi("playback.getCurrentTrackIndex", PlaybackGetCurrentTrackIndex);
    bridge.RegisterApi("playback.getPlayingPlaylist", PlaybackGetPlayingPlaylist);

    LOG("Playback API registered");
}

// Service injection accessors
void SetPlaybackService(IPlaybackService* service) {
    s_playbackService = service ? service : &s_defaultPlaybackService;
}

IPlaybackService* GetPlaybackService() {
    return s_playbackService;
}
