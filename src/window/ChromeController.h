#pragma once

#include "window/WindowChromeResolver.h"
#include "window/WindowChromeApplier.h"

// ============================================
// ChromeController - 统一 chrome resolve/apply 入口
//
// 包装现有 WindowChromeResolver / WindowChromeApplier，
// 不引入第二套 raw/compat/resolved schema。
// ============================================
class ChromeController {
public:
    // 从快照解析 resolved chrome 状态（委托 WindowChromeResolver）
    static ResolvedWindowChromeState Resolve(const WindowChromeResolverSnapshot& snapshot);

    // 应用 resolved chrome 到窗口（委托 WindowChromeApplier）
    static WindowChromeApplyResult Apply(const WindowChromeApplyHooks& hooks,
                                         const WindowChromeApplyRequest& request);

    // 组合 resolve + apply
    static WindowChromeApplyResult ResolveAndApply(
        const WindowChromeResolverSnapshot& snapshot,
        const WindowChromeApplyHooks& hooks,
        bool forceRefresh,
        const std::string& previousMode,
        const std::string& previousEffect);

    // 刷新 native frame（委托 WindowChromeApplier）
    static void RefreshNativeFrame(HWND hwnd);
};
