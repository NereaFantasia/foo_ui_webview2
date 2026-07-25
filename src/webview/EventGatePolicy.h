#pragma once
// ============================================
// 事件可见性门控策略：页面 hidden 期间按事件名决定直通 / 丢弃 / 缓存最新。
//
// 纯 std 头文件（无 Windows/COM 依赖），供 WebViewHost 与 GoogleTest 共用。
// 线程安全由调用方负责（WebViewHost 以 eventGateMutex_ 保护 LatestBuffer）。
// ============================================
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace event_gate {

// 页面 hidden 期间对一条事件消息的处置
enum class Action {
    Pass,    // 直通：丢失/延迟会破坏正确性的控制面事件（会唤醒挂起页面，属预期语义）
    Drop,    // 丢弃：每帧/每秒全量重发的可再生流，恢复后下一拍自然到达
    Latest,  // 缓存最新：快照/通知型，恢复时按最后到达顺序重放最新 payload
};

// 事件名 → 处置分类。未知事件名默认 Latest（有界、无损）。
inline Action Classify(std::string_view event) {
    // 前缀直通规则
    if (event.rfind("menu:", 0) == 0) return Action::Pass;      // overlay 菜单控制面
    if (event.rfind("jitQueue:", 0) == 0) return Action::Pass;  // 播放接续控制面，扣发即断播
    if (event.rfind("tray:", 0) == 0) return Action::Pass;      // 托盘交互（前端靠它恢复窗口）
    if (event.rfind("port:", 0) == 0) return Action::Pass;      // 跨窗口消息通道

    static const std::unordered_map<std::string_view, Action> kExact = {
        // ---- Pass：控制面 / 生命周期 / 跨窗口一致性 ----
        {"taskbar:buttonClicked", Action::Pass},
        {"keyboard:hotkey", Action::Pass},
        {"app:beforeQuit", Action::Pass},
        {"window:beforeClose", Action::Pass},
        {"webview:processFailed", Action::Pass},
        {"window:message", Action::Pass},
        {"state:changed", Action::Pass},
        {"state:deleted", Action::Pass},
        {"ui:menuItemClicked", Action::Pass},
        // ---- Drop：可再生流 / hidden 下无意义的视觉态 ----
        {"audio:spectrum", Action::Drop},
        {"playback:time", Action::Drop},
        {"playback:timeHighRes", Action::Drop},
        {"window:hoverStateChanged", Action::Drop},
        {"cursor:hiddenChanged", Action::Drop},
    };

    if (auto it = kExact.find(event); it != kExact.end()) {
        return it->second;
    }
    return Action::Latest;
}

// hidden 期间的 Latest 缓冲：每个事件名只保留最新 payload，
// 重放顺序 = 各事件最后一次到达的全局顺序。
// 容量天然有界：条目数 ≤ 出现过的不同事件名数。
class LatestBuffer {
public:
    struct Entry {
        std::string event;
        std::wstring payload;
    };

    void Put(std::string_view event, std::wstring payload) {
        for (auto it = entries_.begin(); it != entries_.end(); ++it) {
            if (it->event == event) {
                entries_.erase(it);
                break;
            }
        }
        entries_.push_back(Entry{std::string(event), std::move(payload)});
    }

    // 取走全部条目并清空缓冲（按重放顺序）
    std::vector<Entry> Flush() {
        std::vector<Entry> out;
        out.swap(entries_);
        return out;
    }

    size_t Size() const { return entries_.size(); }
    bool Empty() const { return entries_.empty(); }

private:
    std::vector<Entry> entries_;
};

}  // namespace event_gate
