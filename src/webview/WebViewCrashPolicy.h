#pragma once

// ==========================================================================
// WebView2 进程崩溃处置策略（纯函数层）
//
// WebViewHost 的 ProcessFailed 处理需要按崩溃进程类型分级，但那段代码位于
// COM 回调内、依赖 WebView2 头文件，无法在测试工程编译。此处把"分级 → 处置"
// 的判定抽成不依赖 WebView2 的纯函数，与 background_suspend_policy 同构，
// 使消费端契约（是否已自愈 / 是否需标记僵尸 / 是否需作废共享环境）可被单测覆盖。
//
// 背景：浏览器进程退出后所有 COM 指针仍非空，但调用一律返回
// ERROR_INVALID_STATE (0x8007139F)。若不标记僵尸态，门卫会继续放行到死对象；
// 若不作废进程级共享 environment，后续重建会复用已失效的 environment。
// ==========================================================================

namespace webview_crash_policy {

// 崩溃进程分类。调用方负责把 COREWEBVIEW2_PROCESS_FAILED_KIND 映射到此处，
// 使本层与 WebView2 头文件解耦。
enum class FailureClass {
    // 渲染进程类（renderer 退出 / 无响应 / frame renderer 退出）：
    // controller 与 DComp 连接仍有效，Reload() 可自愈。
    Render,
    // 浏览器主进程退出：整个 WebView 及其 environment 失效。
    Browser,
    // GPU / utility / sandbox helper 等：运行时通常自愈，仅记录。
    Other,
};

struct Disposition {
    // 宿主是否已自行恢复（决定上报给消费端的 recovered 字段）。
    bool recovered;
    // 是否需把面板标记为僵尸态，让门卫拦住后续 COM 调用。
    bool markPanelDead;
    // 是否需作废进程级共享 environment，避免重建复用已失效对象。
    bool invalidateEnvironment;
};

// 把 WebView2 的 kind 枚举判定结果映射到本层分类。调用方在 COM 回调里完成
// kind 比较（避免本头文件依赖 WebView2.h），此处只做归类。
constexpr FailureClass ClassifyFailedKind(bool isRenderKind, bool isBrowserKind) {
    if (isRenderKind) return FailureClass::Render;
    if (isBrowserKind) return FailureClass::Browser;
    return FailureClass::Other;
}

// ==========================================================================
// 僵尸态整窗重建的节流判定
//
// markPanelDead 之后必须有人真正发起重建，否则窗口会永久停留在空壳态
// （实测：浏览器进程被第三方注入钩子打崩后，宿主带着僵尸 controller 运行
// 了 10 小时）。但重建不能无界重试：若崩溃源是每次创建后必然复现的外部
// 因素，无界重试会退化成"重建→崩溃→重建"死循环，持续重启浏览器进程并
// 重新导航。故按"时间窗内最多 N 次"限流，窗口过期后重新计数。
// ==========================================================================

// 时间窗内允许的重建次数上限。
constexpr unsigned kBrowserRebuildAttemptLimit = 3;
// 限流时间窗长度（毫秒）。窗口过期即视为新一轮故障，重新计数。
constexpr unsigned long long kBrowserRebuildWindowMs = 600000;  // 10 分钟

struct BrowserRebuildDecision {
    // 是否发起本次重建。
    bool shouldRebuild;
    // 是否因窗口内次数用尽而放弃（调用方据此提示用户手动干预）。
    bool limitExhausted;
    // 调用方应保存的窗口内累计次数。
    unsigned nextAttempts;
    // 调用方应保存的窗口起点（毫秒 tick）。
    unsigned long long nextWindowStartMs;
};

// attempts / windowStartMs 是调用方持有的限流状态，nowMs 取单调递增的
// GetTickCount64()。rebuildInFlight 为真表示已有重建在途，本次不重复发起。
// 返回值同时给出"是否重建"与"调用方应保存的新状态"，使调用方无需自行推导。
constexpr BrowserRebuildDecision DecideBrowserExitRebuild(
    unsigned attempts,
    unsigned long long windowStartMs,
    unsigned long long nowMs,
    bool rebuildInFlight,
    unsigned limit = kBrowserRebuildAttemptLimit,
    unsigned long long windowMs = kBrowserRebuildWindowMs) {
    if (rebuildInFlight) {
        // 重建在途期间的二次崩溃事件不占配额：本次不发起，状态原样保留。
        return BrowserRebuildDecision{false, false, attempts, windowStartMs};
    }
    // attempts == 0 表示尚无记录；否则按窗口是否过期决定是重新计数还是累加。
    const bool windowExpired = attempts == 0 || (nowMs - windowStartMs) >= windowMs;
    if (windowExpired) {
        return BrowserRebuildDecision{true, false, 1u, nowMs};
    }
    if (attempts < limit) {
        return BrowserRebuildDecision{true, false, attempts + 1u, windowStartMs};
    }
    // 配额用尽：保留窗口状态，等窗口自然过期后才允许再次重建。
    return BrowserRebuildDecision{false, true, attempts, windowStartMs};
}

// reloadSucceeded 仅在 Render 类有意义（其他分类不会发起 Reload）。
constexpr Disposition Decide(FailureClass failure, bool reloadSucceeded) {
    switch (failure) {
        case FailureClass::Render:
            // Reload 成功即视为自愈；失败则与浏览器进程退出同等对待——
            // 页面已不可用，必须让门卫拦住并允许上层重建。
            return reloadSucceeded
                ? Disposition{true, false, false}
                : Disposition{false, true, false};
        case FailureClass::Browser:
            // environment 与浏览器进程同生共死，必须一并作废。
            return Disposition{false, true, true};
        case FailureClass::Other:
        default:
            // GPU / utility 进程由运行时自愈，WebView 本体仍可用。
            return Disposition{true, false, false};
    }
}

}  // namespace webview_crash_policy
