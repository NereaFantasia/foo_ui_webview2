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
