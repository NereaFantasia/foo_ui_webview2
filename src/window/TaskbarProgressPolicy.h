#pragma once

// ============================================
// TaskbarProgressPolicy.h - 播放状态到任务栏进度条的映射（纯逻辑）
//
// Window 子页的「任务栏播放进度」开着时，TaskbarIntegration 按这里的规则驱动
// ITaskbarList3::SetProgressState / SetProgressValue。本头文件不依赖 Windows.h 与
// foobar2000 SDK，让 GoogleTest 直接覆盖真实生产符号。
//
// 规则：
//   - 播放中：normal，按已播比例；暂停：paused，比例照旧；
//   - 停止，或时长未知（<= 0，例如网络流）：noprogress；
//   - 比例按千分位取整并钳到 [0, 1000]，避免位置略超时长时进度条回绕。
// ============================================

#include <algorithm>
#include <string>

namespace taskbar_progress {

enum class Mode {
    NoProgress,
    Normal,
    Paused,
};

struct Decision {
    Mode mode = Mode::NoProgress;
    unsigned long long completed = 0;  // 千分位，[0, kTotal]
    static constexpr unsigned long long kTotal = 1000;

    bool operator==(const Decision& other) const {
        return mode == other.mode && completed == other.completed;
    }
};

// state 为 PlaybackCallback 广播的状态名："playing" / "paused" / "stopped"；未知名字按停止处理。
inline Decision Decide(const std::string& state, double positionSec, double lengthSec) {
    Decision d;
    if (state != "playing" && state != "paused") return d;
    if (!(lengthSec > 0.0)) return d;
    const double ratio = positionSec / lengthSec;
    const double clamped = (std::min)(1.0, (std::max)(0.0, ratio));
    d.mode = state == "playing" ? Mode::Normal : Mode::Paused;
    d.completed = static_cast<unsigned long long>(clamped * static_cast<double>(Decision::kTotal) + 0.5);
    return d;
}

}  // namespace taskbar_progress
