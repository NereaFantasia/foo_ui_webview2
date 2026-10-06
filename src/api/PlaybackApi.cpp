#include "pch.h"
#include "api/PlaybackApi.h"
#include "api/BridgeCore.h"
#include "api/TypedApi.h"
#include "api/generated/PlaybackSchema.h"
#include "api/MetaAccess.h"
#include "api/PlaylistLock.h"
#include "api/PlaylistApi.h"
#include "api/PlaylistTarget.h"
#include "interfaces/IPlaybackService.h"
#include "api/adapters/Fb2kPlaybackService.h"
#include "api/VolumeScale.h"
#include "utils/HostTime.h"
#include "utils/SubsongUtils.h"

// ============================================
// Helper Functions
// ============================================

using SubsongUtils::ParseSubsongPath;

json GetTrackInfo(const metadb_handle_ptr& track) {
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
        {"artist", MetaJoined(info, "artist")},
        {"album", getMeta("album")},
        {"albumArtist", MetaJoined(info, "album artist")},
        {"genre", MetaJoined(info, "genre")},
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


// ==========================================================================
// Playback API handler functions
// 形状由 src/api/schema/playback.ts 声明，结构体与参数解析来自生成的 PlaybackSchema.h。
// ==========================================================================
namespace {

namespace playback = api::playback;

// Service injection point for testability
// Production code uses Fb2kPlaybackService; tests can swap in a mock.
// Thread safety: Set/Get must only be called during single-threaded init
// (component_init / test SetUp) before any handler is invoked.
static Fb2kPlaybackService s_defaultPlaybackService;
static IPlaybackService* s_playbackService = &s_defaultPlaybackService;

// ========== Basic Controls ==========

api::Result<void> PlaybackPlay(const playback::PlayParams&) {
    s_playbackService->play_or_unpause();
    return api::Ok();
}


api::Result<void> PlaybackPause(const playback::PauseParams&) {
    s_playbackService->pause(true);
    return api::Ok();
}


api::Result<void> PlaybackStop(const playback::StopParams&) {
    s_playbackService->stop();
    return api::Ok();
}


// playPause 与 playOrPause 是同一个操作的两个名字：各自有生成的结构体，逻辑共用。
bool TogglePlayPause() {
    auto* svc = s_playbackService;
    const bool wasPlaying = svc->is_playing() && !svc->is_paused();
    svc->play_or_pause();
    return !wasPlaying;
}

api::Result<playback::PlayPauseResult> PlaybackPlayPause(const playback::PlayPauseParams&) {
    playback::PlayPauseResult result;
    result.isPlaying = TogglePlayPause();
    return result;
}


api::Result<playback::PlayOrPauseResult> PlaybackPlayOrPause(const playback::PlayOrPauseParams&) {
    playback::PlayOrPauseResult result;
    result.isPlaying = TogglePlayPause();
    return result;
}


api::Result<void> PlaybackNext(const playback::NextParams&) {
    s_playbackService->next();
    return api::Ok();
}


api::Result<void> PlaybackPrevious(const playback::PreviousParams&) {
    s_playbackService->previous();
    return api::Ok();
}


api::Result<void> PlaybackRandom(const playback::RandomParams&) {
    s_playbackService->start(5 /*track_command_rand*/);
    return api::Ok();
}


// ========== State Query ==========

api::Result<playback::GetStateResult> PlaybackGetState(const playback::GetStateParams&) {
    auto* svc = s_playbackService;

    playback::GetStateResult result;
    result.state = "stopped";
    if (svc->is_playing()) {
        result.state = svc->is_paused() ? "paused" : "playing";
    }
    result.canSeek = svc->playback_can_seek();
    result.canPause = true;
    return result;
}


api::Result<playback::GetPositionResult> PlaybackGetPosition(const playback::GetPositionParams&) {
    auto* svc = s_playbackService;
    const auto info = svc->get_now_playing_info();

    playback::GetPositionResult result;
    result.position = svc->playback_get_position();
    result.hostTime = host_time::NowUnixMs();
    result.duration = info.duration;
    result.subsong = info.subsong;
    result.path = info.path;
    return result;
}


api::Result<playback::SetPositionResult> PlaybackSetPosition(const playback::SetPositionParams& params) {
    auto* svc = s_playbackService;

    if (!svc->is_playing()) {
        return api::Fail("Nothing is playing", ApiErrorCode::NO_ACTIVE_ITEM);
    }
    if (!svc->playback_can_seek()) {
        return api::Fail("Cannot seek in current track", ApiErrorCode::NOT_SUPPORTED);
    }

    // 边界检查：确保 position 在有效范围内
    const auto info = svc->get_now_playing_info();
    const double duration = info.duration;
    double seconds = params.position;
    if (seconds < 0) {
        seconds = 0;
    }
    if (duration > 0 && seconds > duration) {
        seconds = duration - 0.1;  // 留一点余量避免跳到下一曲
        if (seconds < 0) seconds = 0;
    }

    const double oldPosition = svc->playback_get_position();
    svc->playback_seek(seconds);
    const double newPosition = svc->playback_get_position();
    const double newPositionHostTime = host_time::NowUnixMs();

    playback::SetPositionResult result;
    result.requestedPosition = params.position;
    result.actualPosition = seconds;
    result.oldPosition = oldPosition;
    result.newPosition = newPosition;
    result.hostTime = newPositionHostTime;
    result.duration = duration;
    result.subsong = info.subsong;
    return result;
}


// ========== Volume Control ==========

api::Result<playback::GetVolumeResult> PlaybackGetVolume(const playback::GetVolumeParams&) {
    auto* svc = s_playbackService;
    const float db = svc->get_volume();
    const float volume = volume_scale::PercentFromDb(db);
    const bool muted = svc->is_muted();

    playback::GetVolumeResult result;
    result.volume = volume;
    result.volumeDb = db;
    result.muted = muted;
    result.isMuted = muted;
    return result;
}


api::Result<void> PlaybackSetVolume(const playback::SetVolumeParams& params) {
    auto* svc = s_playbackService;
    const float volume = static_cast<float>(params.volume);

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
    return api::Ok();
}


api::Result<void> PlaybackVolumeUp(const playback::VolumeUpParams&) {
    s_playbackService->volume_up();
    return api::Ok();
}


api::Result<void> PlaybackVolumeDown(const playback::VolumeDownParams&) {
    s_playbackService->volume_down();
    return api::Ok();
}


api::Result<void> PlaybackMute(const playback::MuteParams& params) {
    auto* svc = s_playbackService;
    const bool currentMuted = svc->is_muted();

    // 如果需要静音且当前未静音，或需要取消静音且当前已静音
    if (params.muted != currentMuted) {
        svc->volume_mute_toggle();
    }
    return api::Ok();
}


api::Result<playback::ToggleMuteResult> PlaybackToggleMute(const playback::ToggleMuteParams&) {
    auto* svc = s_playbackService;
    const bool currentMuted = svc->is_muted();
    svc->volume_mute_toggle();

    playback::ToggleMuteResult result;
    result.muted = !currentMuted;
    return result;
}


// ========== Current Track ==========

api::Result<playback::GetCurrentTrackResult> PlaybackGetCurrentTrack(const playback::GetCurrentTrackParams&) {
    playback::GetCurrentTrackResult result;
    auto track = s_playbackService->get_current_track();
    result.found = track.has_value();
    if (track) {
        result.track = std::move(*track);
    }
    return result;
}


// ========== Playback Order ==========

api::Result<playback::GetPlaybackOrderResult> PlaybackGetPlaybackOrder(const playback::GetPlaybackOrderParams&) {
    const int order = s_playbackService->playback_order_get_active();
    const std::string orderName = OrderToString(order);

    playback::GetPlaybackOrderResult result;
    result.order = order;
    result.orderName = orderName;
    result.name = orderName;
    result.orderIndex = order;
    return result;
}


api::Result<playback::SetPlaybackOrderResult> PlaybackSetPlaybackOrder(const playback::SetPlaybackOrderParams& params) {
    if (!params.order && !params.name) {
        return api::Fail("order or name is required", ApiErrorCode::INVALID_PARAMS);
    }
    if (params.order && params.name) {
        return api::Fail("order and name cannot both be given", ApiErrorCode::INVALID_PARAMS);
    }
    // name 的取值由声明的枚举保证，StringToOrder 认得全部七个名字。
    const int order = params.order ? static_cast<int>(*params.order) : StringToOrder(*params.name);
    s_playbackService->playback_order_set_active(order);

    // 报告生效的顺序，而不是回显输入。
    const int active = s_playbackService->playback_order_get_active();
    playback::SetPlaybackOrderResult result;
    result.order = active;
    result.orderName = OrderToString(active);
    return result;
}


// ========== Stop After Current ==========

api::Result<playback::GetStopAfterCurrentResult> PlaybackGetStopAfterCurrent(const playback::GetStopAfterCurrentParams&) {
    playback::GetStopAfterCurrentResult result;
    result.enabled = s_playbackService->get_stop_after_current();
    return result;
}


api::Result<playback::SetStopAfterCurrentResult> PlaybackSetStopAfterCurrent(const playback::SetStopAfterCurrentParams& params) {
    s_playbackService->set_stop_after_current(params.enabled);
    playback::SetStopAfterCurrentResult result;
    result.enabled = params.enabled;
    return result;
}


api::Result<playback::ToggleStopAfterCurrentResult> PlaybackToggleStopAfterCurrent(const playback::ToggleStopAfterCurrentParams&) {
    auto* svc = s_playbackService;
    svc->toggle_stop_after_current();
    playback::ToggleStopAfterCurrentResult result;
    result.enabled = svc->get_stop_after_current();
    return result;
}


// ========== Test/Echo API ==========
// test.* 不在声明范围内，经 BridgeCore::RegisterUndeclaredApi 注册。

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

// ========== Direct Path Playback ==========

// playback.playPath - Play a single file path directly
// 支持 subsong 格式: "path/to/file.flac|subsong:2"
api::Result<playback::PlayPathResult> PlaybackPlayPath(const playback::PlayPathParams& params) {
    try {
        // 解析路径和 subsong 索引
        auto [filePath, subsongIndex] = ParseSubsongPath(params.path);
        const bool subsongRequested = (params.path.find("|subsong:") != std::string::npos);

        FB2K_console_print("[PlaybackApi] playPath: ", filePath.c_str(), " subsong:", std::to_string(subsongIndex).c_str());

        const auto outcome = s_playbackService->play_single_path(filePath, subsongIndex, subsongRequested);

        if (!outcome.success) {
            if (outcome.locked) return PlaylistLocked(outcome.playlist, "playback.playPath");
            return api::Fail(outcome.error,
                             outcome.unresolved ? ApiErrorCode::NOT_FOUND : ApiErrorCode::OPERATION_FAILED,
                             {{"path", filePath}, {"subsong", subsongIndex}});
        }

        playback::PlayPathResult result;
        result.tracksAdded = outcome.tracksAdded;
        result.path = filePath;
        result.subsong = outcome.resolvedSubsong;
        return result;
    } catch (const std::exception& e) {
        return api::Fail(e.what(), ApiErrorCode::OPERATION_FAILED);
    } catch (...) {
        return api::Fail("Unknown error", ApiErrorCode::OPERATION_FAILED);
    }
}


// playback.playPaths - Play multiple file paths
// 支持 subsong 格式: "path/to/file.flac|subsong:2"
api::Result<playback::PlayPathsResult> PlaybackPlayPaths(const playback::PlayPathsParams& params) {
    try {
        const auto outcome = s_playbackService->play_multiple_paths(
            params.paths, static_cast<size_t>(params.startIndex), params.replace);

        if (!outcome.success) {
            if (outcome.locked) return PlaylistLocked(outcome.playlist, "playback.playPaths");
            return api::Fail(outcome.error,
                             outcome.unresolved ? ApiErrorCode::NOT_FOUND : ApiErrorCode::OPERATION_FAILED);
        }

        playback::PlayPathsResult result;
        result.tracksAdded = static_cast<std::int64_t>(outcome.tracksAdded);
        result.startedAt = static_cast<std::int64_t>(outcome.startedAt);
        return result;
    } catch (const std::exception& e) {
        return api::Fail(e.what(), ApiErrorCode::OPERATION_FAILED);
    }
}


// 在播位置的列表 GUID。页面拿到序号再去 getAll 反查，中间别的列表一增删就会认错；
// 序号越界时不去问 SDK（它没写越界时返回什么），回 null。
std::optional<std::string> PlaylistGuidOrNull(size_t playlist) {
    const IPlaylistService& lists = *GetPlaylistService();
    if (playlist >= lists.get_playlist_count()) return std::nullopt;
    return api::PlaylistGuidOf(lists, playlist);
}

// ========== playback.getCurrentTrackIndex - 获取当前播放曲目的位置 ==========
api::Result<playback::GetCurrentTrackIndexResult> PlaybackGetCurrentTrackIndex(const playback::GetCurrentTrackIndexParams& params) {
    auto* svc = s_playbackService;
    const auto loc = svc->get_playing_item_location();

    playback::GetCurrentTrackIndexResult result;
    result.found = loc.found;
    if (!loc.found) {
        // playlist 与 index 保持 nullopt，写出 null
        return result;
    }

    result.playlist = static_cast<std::int64_t>(loc.playlist);
    result.playlistGuid = PlaylistGuidOrNull(loc.playlist);
    result.index = static_cast<std::int64_t>(loc.index);

    if (params.includeTrackInfo) {
        auto track = svc->get_track_at(loc.playlist, loc.index);
        if (track) {
            result.track = std::move(*track);
        }
    }
    return result;
}


// ========== playback.getPlayingPlaylist - 获取正在播放的播放列表 ==========
api::Result<playback::GetPlayingPlaylistResult> PlaybackGetPlayingPlaylist(const playback::GetPlayingPlaylistParams&) {
    const size_t playlist = s_playbackService->get_playing_playlist();

    playback::GetPlayingPlaylistResult result;
    if (playlist == SIZE_MAX) {
        result.found = false;
        return result;
    }

    result.found = true;
    result.playlist = static_cast<std::int64_t>(playlist);
    result.playlistGuid = PlaylistGuidOrNull(playlist);
    result.name = s_playbackService->get_playlist_name(playlist);
    return result;
}

} // namespace

void RegisterPlaybackApi() {
    api::RegisterApi("playback.play", PlaybackPlay);
    api::RegisterApi("playback.pause", PlaybackPause);
    api::RegisterApi("playback.stop", PlaybackStop);
    api::RegisterApi("playback.playPause", PlaybackPlayPause);
    api::RegisterApi("playback.playOrPause", PlaybackPlayOrPause);
    api::RegisterApi("playback.next", PlaybackNext);
    api::RegisterApi("playback.previous", PlaybackPrevious);
    api::RegisterApi("playback.random", PlaybackRandom);
    api::RegisterApi("playback.getState", PlaybackGetState);
    api::RegisterApi("playback.getPosition", PlaybackGetPosition);
    api::RegisterApi("playback.setPosition", PlaybackSetPosition);
    api::RegisterApi("playback.getVolume", PlaybackGetVolume);
    api::RegisterApi("playback.setVolume", PlaybackSetVolume);
    api::RegisterApi("playback.volumeUp", PlaybackVolumeUp);
    api::RegisterApi("playback.volumeDown", PlaybackVolumeDown);
    api::RegisterApi("playback.mute", PlaybackMute);
    api::RegisterApi("playback.toggleMute", PlaybackToggleMute);
    api::RegisterApi("playback.getCurrentTrack", PlaybackGetCurrentTrack);
    api::RegisterApi("playback.getPlaybackOrder", PlaybackGetPlaybackOrder);
    api::RegisterApi("playback.setPlaybackOrder", PlaybackSetPlaybackOrder);
    api::RegisterApi("playback.getStopAfterCurrent", PlaybackGetStopAfterCurrent);
    api::RegisterApi("playback.setStopAfterCurrent", PlaybackSetStopAfterCurrent);
    api::RegisterApi("playback.toggleStopAfterCurrent", PlaybackToggleStopAfterCurrent);
    api::RegisterApi("playback.playPath", PlaybackPlayPath);
    api::RegisterApi("playback.playPaths", PlaybackPlayPaths);
    api::RegisterApi("playback.getCurrentTrackIndex", PlaybackGetCurrentTrackIndex);
    api::RegisterApi("playback.getPlayingPlaylist", PlaybackGetPlayingPlaylist);

    auto& bridge = BridgeCore::GetInstance();
    bridge.RegisterUndeclaredApi("test.echo", TestEcho);
    bridge.RegisterUndeclaredApi("test.ping", TestPing);

    LOG("Playback API registered");
}

// Service injection accessors
void SetPlaybackService(IPlaybackService* service) {
    s_playbackService = service ? service : &s_defaultPlaybackService;
}

IPlaybackService* GetPlaybackService() {
    return s_playbackService;
}
