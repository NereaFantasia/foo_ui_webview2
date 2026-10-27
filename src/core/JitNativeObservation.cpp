#include "pch.h"
#include "core/JitNativeObservation.h"

// ============================================
// JitNativeObservation.cpp - 把 foobar2000 回调接到原生观察版本
//
// 播放与配置回调都在主线程，各自调一次这里的 hooks；队列与播放列表的计数在
// ContentVersions.cpp。回调里只读设置，不写列表。
// ============================================

namespace jit_native {
namespace {

std::optional<Settings> ReadSettings() {
    try {
        Settings value;
        value.order = playlist_manager::get()->playback_order_get_active();
        config_object::g_get_data_bool(standard_config_objects::bool_cursor_follows_playback, value.cursorFollowsPlayback);
        config_object::g_get_data_bool(standard_config_objects::bool_playback_follows_cursor, value.playbackFollowsCursor);
        config_object::g_get_data_bool(standard_config_objects::bool_playlist_stop_after_current, value.stopAfterCurrent);
        return value;
    } catch (...) {
        return std::nullopt;
    }
}

}  // namespace

Observer& NativeObserver() {
    static Observer observer(content_version::Shared());
    return observer;
}

std::optional<Settings> SampleSettings() {
    auto value = ReadSettings();
    NativeObserver().SettingsRead(value);
    return value;
}

namespace hooks {

void PlaybackStarting() { NativeObserver().Starting(); }
void PlaybackNewTrack() { NativeObserver().TrackStarted(); }
void PlaybackStopped(bool user) { NativeObserver().Stopped(user); }

void SettingsChanged() { SampleSettings(); }

}  // namespace hooks
}  // namespace jit_native
