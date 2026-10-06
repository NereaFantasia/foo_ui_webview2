#pragma once

// ============================================
// WindowBehaviorTrace.h - 窗口行为轨迹（默认关闭的取证工具）
//
// 记录窗口与 WebView 相关代码对系统产生的副作用：本 DLL 调用的窗口、DWM API，
// 本模块窗口收到的消息及返回值，WebView2 / DirectComposition 的关键调用，
// 以及发给页面的窗口类事件。用途是比对同一组操作在两个构建上的副作用是否一致，
// 例如纯搬运的重构前后。比对脚本见 scripts/window-trace/。
//
// 开启：启动 foobar2000 前设环境变量 FOO_UI_WEBVIEW2_BEHAVIOR_TRACE=1，
// 或在 profile 目录放一个 webview_behavior_trace.on 文件。轨迹写到 profile 目录的
// webview_behavior_trace.log，每次启动重写。开关只在启动时读一次。
//
// 关闭时下面的记录函数只做一次原子读，不改变任何行为。
// ============================================

#include <atomic>
#include <string>
#include <windows.h>
#include <nlohmann/json.hpp>

namespace window_behavior_trace {

namespace detail {
extern std::atomic<bool> g_active;
}  // namespace detail

// 本次进程是否在记录轨迹。
inline bool Active() noexcept {
    return detail::g_active.load(std::memory_order_relaxed);
}

// 记录一次代表窗口 owner 发出的 WebView2 或 DirectComposition 调用。
// op 用「对象.方法」的形式，例如 "controller.put_Bounds"。value 是参数里决定行为的
// 那个标量：可见性记 0 / 1，背景色记 alpha，移焦记原因的枚举值。
void Com(HWND owner, const char* op) noexcept;
void Com(HWND owner, const char* op, const RECT& bounds) noexcept;
void Com(HWND owner, const char* op, long long value) noexcept;

// 记录发给 owner 所承载页面的事件。只记窗口、面板、菜单、托盘、任务栏与应用
// 这几类事件名，播放、频谱等高频事件与窗口行为无关，不记。
void Event(HWND owner, const std::string& name) noexcept;

// 把一次桥接调用在 UI 线程上引起的副作用归到同一帧里。参数里以下划线开头的键
// （桥接层注入的调用方句柄等）不进轨迹，它们每次运行都不同。
class InvokeScope {
public:
    InvokeScope(const std::string& method, const nlohmann::json& params) noexcept;
    ~InvokeScope();
    InvokeScope(const InvokeScope&) = delete;
    InvokeScope& operator=(const InvokeScope&) = delete;

private:
    bool open_ = false;
};

}  // namespace window_behavior_trace
