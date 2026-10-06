#pragma once
// PlaylistLock.h - 往播放列表写曲目的端点在目标列表上锁时共用的失败信封。
//
// playlist.* 的写操作、library.addToPlaylist、queue.addPaths、playback.playPath 与
// playback.playPaths 都答这一个形状：code LOCKED，details 为 { playlist, isLocked: true }。
// 这些端点动手前先查 playlist_lock_is_present，有锁就拒绝，不管这把锁是否恰好放行
// 这一类写操作：同一把锁下各端点因此同进同退。
#include "api/ApiResult.h"
#include "api/ErrorEnvelope.h"

#include <cstddef>

// api 只进日志行（[FailureEnvelope] 的 api= 字段），不进信封。
inline api::Failure PlaylistLocked(size_t playlistIndex, const char* api = "playlist.*") {
    FailureHook::LogSync(api, ApiErrorCode::LOCKED, "Playlist is locked", true);
    return api::Fail("Playlist is locked", ApiErrorCode::LOCKED,
                     {{"details", nlohmann::json{{"playlist", playlistIndex}, {"isLocked", true}}}});
}
