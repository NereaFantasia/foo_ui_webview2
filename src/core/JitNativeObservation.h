#pragma once

// ============================================
// JitNativeObservation.h - 原生接管的观察版本
//
// 回调只给出“发生了什么”，不带调用方身份，也没有 playing 列表的专用回调；所以这里把起播、
// 换曲、用户停止、播放顺序与相关设置分别计成单调版本，队列与各播放列表内容读共享的
// content_version::Counters。快照时记下一组版本，提交前比对，任何一项变了都算事实已变。
// 设置按有效值计：同值的重复通知不算变化。
//
// Observer 不依赖 foobar2000 SDK，只在主线程使用。回调接入与读取设置见
// JitNativeObservation.cpp 的 hooks。
// ============================================

#include "core/ContentVersions.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace jit_native {

// 接管只支持这组设置：Default 顺序，三项跟随与“播完当前停止”全部关闭。
struct Settings {
    std::size_t order = 0;
    bool cursorFollowsPlayback = false;
    bool playbackFollowsCursor = false;
    bool stopAfterCurrent = false;

    bool operator==(const Settings&) const = default;

    // Default 是播放顺序列表的第 0 项。
    bool Supported() const { return order == 0 && !cursorFollowsPlayback && !playbackFollowsCursor && !stopAfterCurrent; }
};

struct Versions {
    std::uint64_t starts = 0;
    std::uint64_t tracks = 0;
    std::uint64_t userStops = 0;
    std::uint64_t settings = 0;

    bool operator==(const Versions&) const = default;
};

// 一次快照记下的版本；lists 只含快照关心的列表。
struct Stamp {
    Versions versions;
    std::uint64_t queue = 0;
    std::vector<std::pair<std::string, std::uint64_t>> lists;
};

class Observer {
public:
    explicit Observer(const content_version::Counters& content) : m_content(content) {}

    void Starting() { ++m_versions.starts; }
    void TrackStarted() { ++m_versions.tracks; }
    void Stopped(bool user) {
        if (user) ++m_versions.userStops;
    }

    // 记下读到的设置；读不到时传空。只有有效值与上次不同才增加版本，第一次只建立基线。
    void SettingsRead(const std::optional<Settings>& value) {
        if (m_settingsKnown && value == m_settings) return;
        if (m_settingsKnown) ++m_versions.settings;
        m_settingsKnown = true;
        m_settings = value;
    }

    const std::optional<Settings>& LastSettings() const { return m_settings; }
    const Versions& Current() const { return m_versions; }

    std::uint64_t ListVersion(const std::string& guid) const { return m_content.List(guid); }

    Stamp Capture(const std::vector<std::string>& guids) const {
        Stamp stamp{m_versions, m_content.Queue(), {}};
        for (const std::string& guid : guids) stamp.lists.emplace_back(guid, ListVersion(guid));
        return stamp;
    }

    bool Unchanged(const Stamp& stamp) const {
        if (!(stamp.versions == m_versions) || stamp.queue != m_content.Queue()) return false;
        for (const auto& [guid, version] : stamp.lists) {
            if (ListVersion(guid) != version) return false;
        }
        return true;
    }

private:
    const content_version::Counters& m_content;
    Versions m_versions;
    bool m_settingsKnown = false;
    std::optional<Settings> m_settings;
};

// 进程内唯一的观察者；只在主线程访问。
Observer& NativeObserver();

// 读取当前有效设置并记入观察者，读不到时返回空。快照经它取设置，保证比对前已有基线。
std::optional<Settings> SampleSettings();

// 回调接入点：只记事实，不在回调里写列表或做完整的播放控制读写。
namespace hooks {
void PlaybackStarting();
void PlaybackNewTrack();
void PlaybackStopped(bool user);
// 播放顺序或相关设置可能变了；重新读取有效值。
void SettingsChanged();
}  // namespace hooks

}  // namespace jit_native
