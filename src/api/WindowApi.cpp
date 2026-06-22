#include "pch.h"
#include "api/WindowApi.h"
#include "api/BridgeCore.h"
#include "api/ApiConstants.h"
#include "core/UserInterface.h"
#include "core/WebViewContext.h"
#include "window/MainWindow.h"
#include "window/PopupWindow.h"
#include "window/WindowManager.h"
#include "window/WindowTargetResolver.h"
#include "window/WindowShellBase.h"
#include "webview/WebViewHost.h"
#include "core/SecurityConfig.h"
#include "core/WebViewPanel.h"  // panel.getConfig/setConfig API

// DWM wrappers
static HRESULT S_DwmSetWindowAttribute(HWND h, DWORD a, LPCVOID d, DWORD s) {
    return ::DwmSetWindowAttribute(h, a, d, s);
}
static HRESULT S_DwmExtendFrameIntoClientArea(HWND h, const MARGINS* m) {
    return ::DwmExtendFrameIntoClientArea(h, m);
}
static HRESULT S_DwmGetWindowAttribute(HWND h, DWORD a, PVOID d, DWORD s) {
    return ::DwmGetWindowAttribute(h, a, d, s);
}
static HRESULT S_DwmEnableBlurBehindWindow(HWND h, const DWM_BLURBEHIND* b) {
    return ::DwmEnableBlurBehindWindow(h, b);
}
static HRESULT S_DwmGetColorizationColor(DWORD* c, BOOL* o) {
    return ::DwmGetColorizationColor(c, o);
}

// 从 main.cpp 导入窗口配置函数
namespace window_config {
    bool HasSavedPosition();
}

// ============================================
// 面板模式检测
// ============================================

/**
 * 检测当前是否处于面板模式
 * 面板模式：没有有效的独立窗口 MainWindow，但有其他 WebView 实例
 */
static bool IsPanelMode() {
    auto* ui = WebViewUI::GetInstance();
    // 如果有有效的 MainWindow，则不是面板模式
    if (ui && ui->GetMainWindow() && ui->GetMainWindow()->GetHwnd()) {
        return false;
    }
    // 如果有任何 WebView 实例注册，则是面板模式
    return WebViewContext::GetInstance().GetInstanceCount() > 0;
}

/**
 * 返回面板模式不支持的标准响应
 * @param apiName API 名称（用于错误消息）
 */
static json PanelModeUnsupported(const char* apiName) {
    return {
        {"success", false},
        {"supported", false},
        {"panelMode", true},
        {"error", std::string(apiName) + " is not supported in panel mode"}
    };
}

// 宏：检查面板模式并返回不支持响应
#define PANEL_MODE_UNSUPPORTED(api_name) \
    if (IsPanelMode()) return PanelModeUnsupported(api_name)

// ============================================
// Helper function to get main window handle
// ============================================

static HWND GetMainHwnd() {
    auto* ui = WebViewUI::GetInstance();
    if (!ui || !ui->GetMainWindow()) return nullptr;
    return ui->GetMainWindow()->GetHwnd();
}

static MainWindow* GetMainWindow() {
    auto* ui = WebViewUI::GetInstance();
    if (!ui) return nullptr;
    return ui->GetMainWindow();
}

// ============================================
// Helper: 获取调用者窗口句柄
// 优先使用 params 中注入的 _callerHwnd，回退到主窗口
// ============================================

static HWND GetCallerHwnd(const json& params) {
    if (params.contains("_callerHwnd")) {
        auto hwnd = reinterpret_cast<HWND>(params["_callerHwnd"].get<intptr_t>());
        if (hwnd && IsWindow(hwnd)) {
            // WebViewPanel.hwnd_ 可能是子窗口，获取顶级窗口
            HWND topLevel = ::GetAncestor(hwnd, GA_ROOT);
            return topLevel ? topLevel : hwnd;
        }
    }
    return GetMainHwnd();  // 向后兼容
}

// ============================================
// Helper: 通过调用者 HWND 获取窗口 ID
// ============================================

static std::string GetCallerWindowId(const json& params) {
    HWND callerHwnd = GetCallerHwnd(params);
    auto& wm = WindowManager::GetInstance();
    
    // 检查主窗口
    auto* mainWin = wm.GetMainWindow();
    if (mainWin && mainWin->GetHwnd() == callerHwnd) return "main";
    
    // 检查弹出窗口
    for (const auto& id : wm.GetAllWindowIds()) {
        if (id == "main") continue;
        auto* popup = wm.GetPopup(id);
        if (popup && popup->GetHwnd() == callerHwnd) return id;
    }
    
    // 面板模式: 通过 WebViewContext 反查 windowId
    // panel HWND 是子窗口，callerHwnd 是其顶级窗口，需遍历匹配
    auto& ctx = WebViewContext::GetInstance();
    for (auto instanceHwnd : ctx.GetAllInstances()) {
        if (instanceHwnd == callerHwnd ||
            ::GetAncestor(instanceHwnd, GA_ROOT) == callerHwnd) {
            std::string wid = ctx.GetWindowIdByHwnd(instanceHwnd);
            if (!wid.empty()) return wid;
        }
    }
    
    return "main";  // 最终回退
}

static PopupWindow* FindPopupByCallerHwnd(HWND callerHwnd, std::string* outWindowId = nullptr) {
    auto& wm = WindowManager::GetInstance();
    for (const auto& id : wm.GetAllWindowIds()) {
        if (id == "main") continue;
        auto* popup = wm.GetPopup(id);
        if (popup && popup->GetHwnd() == callerHwnd) {
            if (outWindowId) *outWindowId = id;
            return popup;
        }
    }
    return nullptr;
}

static MainWindow* FindMainByCallerHwnd(HWND callerHwnd) {
    auto* mainWindow = WindowManager::GetInstance().GetMainWindow();
    if (mainWindow && mainWindow->GetHwnd() == callerHwnd) {
        return mainWindow;
    }
    return nullptr;
}

static bool UpdateLegacyBackdropEffectForCaller(HWND callerHwnd,
                                                const std::optional<std::string>& effect,
                                                const std::optional<bool>& darkMode,
                                                bool clearBlur) {
    if (auto* mainWindow = FindMainByCallerHwnd(callerHwnd)) {
        return mainWindow->UpdateCompatibilityBackdropEffect(effect, darkMode, clearBlur);
    }
    if (auto* popup = FindPopupByCallerHwnd(callerHwnd)) {
        return popup->UpdateCompatibilityBackdropEffect(effect, darkMode, clearBlur);
    }
    return false;
}

static bool UpdateLegacyBlurForCaller(HWND callerHwnd, bool enabled) {
    if (auto* mainWindow = FindMainByCallerHwnd(callerHwnd)) {
        return mainWindow->UpdateCompatibilityBlur(enabled);
    }
    if (auto* popup = FindPopupByCallerHwnd(callerHwnd)) {
        return popup->UpdateCompatibilityBlur(enabled);
    }
    return false;
}

static bool UpdateLegacyDarkModeForCaller(HWND callerHwnd, bool enabled) {
    if (auto* mainWindow = FindMainByCallerHwnd(callerHwnd)) {
        return mainWindow->UpdateCompatibilityDarkMode(enabled);
    }
    if (auto* popup = FindPopupByCallerHwnd(callerHwnd)) {
        return popup->UpdateCompatibilityDarkMode(enabled);
    }
    return false;
}

static bool UpdateLegacyTransparencyForCaller(HWND callerHwnd, bool transparent) {
    if (auto* mainWindow = FindMainByCallerHwnd(callerHwnd)) {
        return mainWindow->UpdateCompatibilityTransparentBackground(transparent);
    }
    if (auto* popup = FindPopupByCallerHwnd(callerHwnd)) {
        return popup->UpdateCompatibilityTransparentBackground(transparent);
    }
    return false;
}

static void ReapplyUnifiedChromeAfterFullscreenChange(HWND callerHwnd) {
    if (auto* mainWindow = FindMainByCallerHwnd(callerHwnd)) {
        mainWindow->RefreshBackdropEffect();
    }
}

static bool SupportsFullscreenForCaller(HWND callerHwnd) {
    return FindMainByCallerHwnd(callerHwnd) != nullptr;
}

static std::vector<PopupWindow::DragRegion> ConvertDragRegionsForPopup(
    const std::vector<TitlebarDragRegion>& regions) {
    std::vector<PopupWindow::DragRegion> popupRegions;
    popupRegions.reserve(regions.size());
    for (const auto& region : regions) {
        popupRegions.push_back(PopupWindow::DragRegion{
            region.x, region.y, region.width, region.height
        });
    }
    return popupRegions;
}

// ============================================
// Helper: 刷新 WebView 以解决 DWM 效果后渲染问题
// ============================================

static void RefreshWebViewAfterDwmChange(HWND hwnd) {
    // 方法 1: 通过 WebViewContext 查找对应的 WebViewHost 刷新
    auto* host = WebViewContext::GetInstance().GetHostByHwnd(hwnd);
    if (host) {
        host->RefreshForDwmEffect();
        return;
    }
    
    // 方法 2: 回退方案 - 直接操作窗口
    RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
    
    RECT rect;
    if (GetWindowRect(hwnd, &rect)) {
        int width = rect.right - rect.left;
        int height = rect.bottom - rect.top;
        
        SetWindowPos(hwnd, nullptr, 0, 0, width - 1, height, 
            SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        SetWindowPos(hwnd, nullptr, 0, 0, width, height, 
            SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

// ============================================
// Helper: 确保 WebView 背景透明以支持 DWM 效果
// ============================================

static void EnsureWebViewTransparent(HWND hwnd) {
    auto* host = WebViewContext::GetInstance().GetHostByHwnd(hwnd);
    if (host) {
        host->SetBackgroundTransparent(true);
    }
}

// ============================================
// Mica Effect Helper
// ============================================

static json SetMicaEffectImpl(const json& params) {
    PANEL_MODE_UNSUPPORTED("window.setMica");
    auto target = WindowTargetResolver::ResolveForMutation(params);
    if (!target.Success()) return target.ErrorResponse();
    
    bool enabled = params.value("enabled", true);
    std::string variant = params.value("variant", "mica");  // "mica" or "mica-alt"
    if (variant != "mica-alt") {
        variant = "mica";
    }
    
    // darkMode 参数: true=深色, false=浅色, 不设置则不改变
    // 这决定了 Mica 效果显示深色还是浅色背景
    bool hasDarkModeParam = params.contains("darkMode");
    bool darkMode = params.value("darkMode", true);
    const bool success = target.shell->PatchCompatibilityBackdrop(
        enabled ? std::optional<std::string>(variant) : std::optional<std::string>(),
        hasDarkModeParam ? std::optional<bool>(darkMode) : std::optional<bool>(),
        enabled);
    
    json result = {{"success", success}, {"enabled", enabled}, {"variant", variant}};
    if (hasDarkModeParam) {
        result["darkMode"] = darkMode;
    }
    return result;
}

// ============================================
// Fullscreen state tracking
// ============================================

// SavedWindowInfo 现在是 WindowShellBase 的 per-window 成员（savedWindowInfo_）。
// 全局 g_savedWindowInfo / g_isFullscreen 已移除，各窗口独立管理全屏状态。

// ============================================
// Fullscreen Helper: Chromium 风格的全屏实现
// 参考: chromium/src/ui/views/win/fullscreen_handler.cc
// ============================================

// RAII 类：临时隐藏窗口以避免闪烁
class ScopedFullscreenVisibility {
public:
    explicit ScopedFullscreenVisibility(HWND hwnd) : hwnd_(hwnd) {
        // 保存当前可见性状态
        was_visible_ = (GetWindowLongPtrW(hwnd_, GWL_STYLE) & WS_VISIBLE) != 0;
        if (was_visible_) {
            // 临时隐藏窗口（通过移除 WS_VISIBLE 样式）
            SetWindowLongPtrW(hwnd_, GWL_STYLE, 
                GetWindowLongPtrW(hwnd_, GWL_STYLE) & ~WS_VISIBLE);
        }
    }
    
    ~ScopedFullscreenVisibility() {
        if (was_visible_) {
            // 恢复窗口可见性
            SetWindowLongPtrW(hwnd_, GWL_STYLE, 
                GetWindowLongPtrW(hwnd_, GWL_STYLE) | WS_VISIBLE);
        }
    }
    
private:
    HWND hwnd_;
    bool was_visible_;
};

static void EnterFullscreenMode(HWND hwnd, WindowShellBase* shell) {
    if (!shell) return;
    {
        // 使用 RAII 临时隐藏窗口以避免闪烁
        ScopedFullscreenVisibility visibility(hwnd);

        // 1. 保存当前窗口信息到 per-window 成员
        // Chromium 注释: "We force the window into restored mode before going
        // fullscreen because Windows doesn't seem to hide the taskbar if the
        // window is in the maximized state."
        WindowShellBase::SavedWindowInfo info;
        info.maximized = !!IsZoomed(hwnd);

        // 如果窗口处于最大化状态，先还原
        if (info.maximized) {
            SendMessage(hwnd, WM_SYSCOMMAND, SC_RESTORE, 0);
        }

        // 保存样式和位置（在还原之后保存样式，因为样式可能因最大化而改变）
        info.style = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_STYLE));
        info.ex_style = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_EXSTYLE));
        GetWindowRect(hwnd, &info.window_rect);

        // 2. 保存并禁用窗口圆角（Windows 11）
        S_DwmGetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE,
            &info.corner_preference, sizeof(info.corner_preference));
        DWORD noRound = DWMWCP_DONOTROUND;
        S_DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &noRound, sizeof(noRound));

        // 3. 重置 DWM 边距（消除 DWM 边框效果）
        MARGINS margins = { 0, 0, 0, 0 };
        S_DwmExtendFrameIntoClientArea(hwnd, &margins);

        // 4. 设置新的窗口样式（移除边框和标题栏）
        SetWindowLongPtrW(hwnd, GWL_STYLE,
            info.style & ~(WS_CAPTION | WS_THICKFRAME));
        SetWindowLongPtrW(hwnd, GWL_EXSTYLE,
            info.ex_style & ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE |
                                            WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));

        // 5. 获取显示器信息并设置窗口大小
        MONITORINFO monitor_info = { sizeof(MONITORINFO) };
        GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor_info);

        // 必须在 SetWindowPos(FRAMECHANGED) 之前设置全屏标志
        // savedWindowInfo_ 赋值即表示全屏模式激活（has_value() == true）
        // FRAMECHANGED 会同步触发 WM_NCCALCSIZE；若 IsFullscreen() 仍为 false，
        // 从最大化进入全屏时 IsZoomed() 可能仍返回 true，导致
        // WM_NCCALCSIZE 走最大化路径（将客户区剪裁为工作区）而非全屏路径
        shell->savedWindowInfo_ = info;
        shell->SetFullscreenFlag(true);

        SetWindowPos(hwnd, HWND_TOPMOST,   // HWND_TOPMOST 覆盖任务栏（SWP_NOZORDER 已移除）
            monitor_info.rcMonitor.left, monitor_info.rcMonitor.top,
            monitor_info.rcMonitor.right - monitor_info.rcMonitor.left,
            monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top,
            SWP_NOACTIVATE | SWP_FRAMECHANGED);  // 不含 SWP_NOZORDER，必须设为 HWND_TOPMOST 才能覆盖任务栏
    }   // ~ScopedFullscreenVisibility(): 恢复 WS_VISIBLE，窗口重新可见

    // 窗口已可见后再触发 NotifyFullscreenChanged（类比 ExitFullscreenMode 第 6 步）
    // 若在窗口隐藏时调用，IsWindowVisible()==FALSE 可能导致 WebView2 刷新异常
    shell->NotifyFullscreenChanged(true);
}

static void ExitFullscreenMode(HWND hwnd, WindowShellBase* shell) {
    if (!shell || !shell->savedWindowInfo_.has_value()) return;

    // 先将保存的信息提取到局部变量，确保后续 reset 不影响读取
    const auto info = *shell->savedWindowInfo_;

    // 使用 RAII 临时隐藏窗口以避免闪烁
    {
        ScopedFullscreenVisibility visibility(hwnd);

        // 1. 恢复窗口样式
        SetWindowLongPtrW(hwnd, GWL_STYLE, static_cast<LONG_PTR>(info.style));
        SetWindowLongPtrW(hwnd, GWL_EXSTYLE, static_cast<LONG_PTR>(info.ex_style));

        // 2. 恢复窗口圆角偏好
        S_DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE,
            &info.corner_preference, sizeof(info.corner_preference));

        // 3. 恢复 DWM 边距（支持 Mica 效果）
        MARGINS margins = { -1, -1, -1, -1 };
        S_DwmExtendFrameIntoClientArea(hwnd, &margins);

        // 4. 恢复窗口位置和大小，同时恢复全屏前的 z-order
        // 兩种情况：如果入全屏前已是 alwaysOnTop（WS_EX_TOPMOST）则恢复为 HWND_TOPMOST，
        // 否则设为 HWND_NOTOPMOST 辞压全屏柟 HWND_TOPMOST
        {
            bool was_topmost = (info.ex_style & WS_EX_TOPMOST) != 0;
            HWND zOrderHint = was_topmost ? HWND_TOPMOST : HWND_NOTOPMOST;
            SetWindowPos(hwnd, zOrderHint,
                info.window_rect.left, info.window_rect.top,
                info.window_rect.right - info.window_rect.left,
                info.window_rect.bottom - info.window_rect.top,
                SWP_NOACTIVATE | SWP_FRAMECHANGED);  // 不含 SWP_NOZORDER，必须设置 z-order 辞压全屏柟
        }

        // 在 SC_MAXIMIZE 之前必须清除全屏标志（不触发 RefreshBackdropEffect）。
        // SC_MAXIMIZE 会同步触发 WM_NCCALCSIZE：若 IsFullscreen() 仍为 true，
        // WM_NCCALCSIZE 走全屏路径（return 0 无边距调整）而非最大化路径
        // （params->rgrc[0] = mi.rcWork），导致右侧三大键被裁切。
        // savedWindowInfo_.reset() 清除全屏标志；SetFullscreenFlag 做额外的
        //    per-window 标志同步（如 MainWindow::isFullscreen_）。
        shell->savedWindowInfo_.reset();
        shell->SetFullscreenFlag(false);

        // 5. 如果之前是最大化状态，恢复最大化
        if (info.maximized) {
            SendMessage(hwnd, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
        }
    }   // ~ScopedFullscreenVisibility(): SetWindowLongPtrW 恢复 WS_VISIBLE，窗口重新可见

    // 6. 窗口已可见后再触发 chrome/WebView2 可见性刷新。
    shell->NotifyFullscreenChanged(false);
}

// ============================================
// Fullscreen State Query (供 MainWindow 使用)
// ============================================

bool IsWindowFullscreen() {
    // 查询 MainWindow shell 的全屏状态（savedWindowInfo_.has_value()）
    auto& wm = WindowManager::GetInstance();
    auto* mainWin = wm.GetMainWindow();
    if (mainWin) {
        return mainWin->IsFullscreen();
    }
    return false;
}

bool ExitFullscreenIfActive(HWND hwnd) {
    if (!hwnd) return false;
    auto target = WindowTargetResolver::ResolveByCallerHwnd(hwnd);
    if (!target.Success() || !target.shell || !target.shell->IsFullscreen()) {
        return false;
    }
    ExitFullscreenMode(target.shell->GetShellHwnd(), target.shell);
    return true;
}

// ============================================
// API Registration
// ============================================


// ==========================================================================
// Window API handler functions
// ==========================================================================
namespace {


// ========== Basic Window Controls ==========
// 使用 WM_SYSCOMMAND 而不是 ShowWindow，以触发 Windows 原生动画效果

json WindowMinimize(const json& params) {
    PANEL_MODE_UNSUPPORTED("window.minimize");
    HWND hwnd = GetCallerHwnd(params);
    if (!hwnd) return { {"success", false}, {"error", "Window not found"} };
    
    // 使用 WM_SYSCOMMAND 触发系统动画
    PostMessage(hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
    return { {"success", true} };
}


json WindowMaximize(const json& params) {
    PANEL_MODE_UNSUPPORTED("window.maximize");
    HWND hwnd = GetCallerHwnd(params);
    if (!hwnd) return { {"success", false}, {"error", "Window not found"} };
    
    // 参考 Electron / Tauri Windows 行为：全屏时 maximize 应先退出全屏。
    // ExitFullscreenMode 会按 saved info 决定是否自动恢复 maximize；
    // 之后下面的 PostMessage SC_MAXIMIZE 在已 maximize 时是 no-op，未 maximize 时补 maximize。
    ExitFullscreenIfActive(hwnd);
    
    // 使用 WM_SYSCOMMAND 触发系统动画
    PostMessage(hwnd, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
    return { {"success", true} };
}


json WindowRestore(const json& params) {
    PANEL_MODE_UNSUPPORTED("window.restore");
    HWND hwnd = GetCallerHwnd(params);
    if (!hwnd) return { {"success", false}, {"error", "Window not found"} };
    
    // 参考 Electron / Tauri：全屏状态下 restore 语义为“回到正常状态”，
    // ExitFullscreenMode 会恢复全屏前的 saved info（normal 或 maximized）。
    if (ExitFullscreenIfActive(hwnd)) {
        return { {"success", true} };
    }
    
    // 使用 WM_SYSCOMMAND 触发系统动画
    PostMessage(hwnd, WM_SYSCOMMAND, SC_RESTORE, 0);
    return { {"success", true} };
}


json WindowClose(const json& params) {
    PANEL_MODE_UNSUPPORTED("window.close");
    HWND hwnd = GetCallerHwnd(params);
    if (hwnd) {
        // 使用 WM_SYSCOMMAND 触发系统关闭动画
        PostMessage(hwnd, WM_SYSCOMMAND, SC_CLOSE, 0);
    }
    return { {"success", true} };
}


json WindowToggleMaximize(const json& params) {
    PANEL_MODE_UNSUPPORTED("window.toggleMaximize");
    HWND hwnd = GetCallerHwnd(params);
    if (!hwnd) return { {"success", false}, {"maximized", false} };
    
    // 参考 Electron / Tauri：全屏状态下 toggle 优先退出全屏，
    // 不再进一步在 maximize 与 normal 之间切换。
    if (ExitFullscreenIfActive(hwnd)) {
        return { {"success", true}, {"maximized", IsZoomed(hwnd) != FALSE} };
    }
    
    bool wasMaximized = IsZoomed(hwnd) != FALSE;
    // 使用 WM_SYSCOMMAND 触发系统动画
    PostMessage(hwnd, WM_SYSCOMMAND, wasMaximized ? SC_RESTORE : SC_MAXIMIZE, 0);
    
    return {
        {"success", true},
        {"maximized", !wasMaximized},
    };
}


// ========== State Query ==========

json WindowIsMaximized(const json& params) {
    HWND hwnd = GetCallerHwnd(params);
    bool val = hwnd && IsZoomed(hwnd) != FALSE;
    return { {"maximized", val}, {"isMaximized", val} };
}


json WindowIsMinimized(const json& params) {
    HWND hwnd = GetCallerHwnd(params);
    return { {"minimized", hwnd && IsIconic(hwnd) != FALSE} };
}


json WindowIsFullscreen(const json& params) {
    auto target = WindowTargetResolver::ResolveForObservation(params);
    bool fs = target.Success() ? target.shell->IsFullscreen() : false;
    return { {"fullscreen", fs}, {"isFullscreen", fs} };
}


json WindowGetState(const json& params) {
    HWND hwnd = GetCallerHwnd(params);
    if (!hwnd) {
        return {
            {"maximized", false},
            {"minimized", false},
            {"fullscreen", false},
            {"alwaysOnTop", false},
            {"focused", false},
            {"width", 0},
            {"height", 0},
            {"x", 0},
            {"y", 0},
        };
    }
    
    DWORD exStyle = GetWindowLongW(hwnd, GWL_EXSTYLE);
    bool alwaysOnTop = (exStyle & WS_EX_TOPMOST) != 0;
    
    bool isMax = IsZoomed(hwnd) != FALSE;
    bool isMin = IsIconic(hwnd) != FALSE;
    auto shellTarget = WindowTargetResolver::ResolveByCallerHwnd(hwnd);
    bool isFull = shellTarget.Success() ? shellTarget.shell->IsFullscreen() : false;
    bool isFocus = GetForegroundWindow() == hwnd;
    
    RECT rc;
    GetWindowRect(hwnd, &rc);
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;
    
    return {
        {"maximized", isMax},
        {"minimized", isMin},
        {"fullscreen", isFull},
        {"alwaysOnTop", alwaysOnTop},
        {"focused", isFocus},
        {"isMaximized", isMax},
        {"isMinimized", isMin},
        {"isFullscreen", isFull},
        {"isAlwaysOnTop", alwaysOnTop},
        {"isFocused", isFocus},
        {"width", width},
        {"height", height},
        {"x", rc.left},
        {"y", rc.top},
    };
}


// ========== Drag ==========

json WindowStartDrag(const json& params) {
    PANEL_MODE_UNSUPPORTED("window.startDrag");
    HWND hwnd = GetCallerHwnd(params);
    if (!hwnd) return { {"success", false}, {"error", "Window not found"} };
    
    // Release mouse capture and send drag message
    ReleaseCapture();
    SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
    
    return { {"success", true} };
}


json WindowStartResize(const json& params) {
    PANEL_MODE_UNSUPPORTED("window.startResize");
    HWND hwnd = GetCallerHwnd(params);
    if (!hwnd) return { {"success", false}, {"error", "Window not found"} };
    
    std::string edge = params.value("edge", "bottomright");
    
    WPARAM hitTest = HTBOTTOMRIGHT;
    if (edge == "left") hitTest = HTLEFT;
    else if (edge == "right") hitTest = HTRIGHT;
    else if (edge == "top") hitTest = HTTOP;
    else if (edge == "bottom") hitTest = HTBOTTOM;
    else if (edge == "topleft") hitTest = HTTOPLEFT;
    else if (edge == "topright") hitTest = HTTOPRIGHT;
    else if (edge == "bottomleft") hitTest = HTBOTTOMLEFT;
    else if (edge == "bottomright") hitTest = HTBOTTOMRIGHT;
    
    ReleaseCapture();
    SendMessage(hwnd, WM_NCLBUTTONDOWN, hitTest, 0);
    
    return { {"success", true} };
}


// ========== Always On Top ==========

json WindowSetAlwaysOnTop(const json& params) {
    PANEL_MODE_UNSUPPORTED("window.setAlwaysOnTop");
    HWND hwnd = GetCallerHwnd(params);
    if (!hwnd) return { {"success", false}, {"error", "Window not found"} };
    
    bool enabled = params.value("enabled", true);
    
    SetWindowPos(
        hwnd,
        enabled ? HWND_TOPMOST : HWND_NOTOPMOST,
        0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE
    );

    // 主窗口期望置顶状态登记：用户显式 alwaysOnTop 意图是该状态的唯一改写来源，
    // 供 MainWindow 的 WM_WINDOWPOSCHANGED 守护矫正参照（popup 不参与守护）。
    {
        auto* mainWin = WindowManager::GetInstance().GetMainWindow();
        if (mainWin && mainWin->GetShellHwnd() == hwnd) {
            mainWin->SetExpectedTopmost(enabled);
        }
    }
    
    return { {"success", true} };
}


json WindowIsAlwaysOnTop(const json& params) {
    HWND hwnd = GetCallerHwnd(params);
    if (!hwnd) return { {"enabled", false}, {"isAlwaysOnTop", false} };
    
    DWORD exStyle = GetWindowLongW(hwnd, GWL_EXSTYLE);
    bool val = (exStyle & WS_EX_TOPMOST) != 0;
    return { {"enabled", val}, {"isAlwaysOnTop", val} };
}


json WindowToggleAlwaysOnTop(const json& params) {
    PANEL_MODE_UNSUPPORTED("window.toggleAlwaysOnTop");
    HWND hwnd = GetCallerHwnd(params);
    if (!hwnd) return { {"success", false}, {"enabled", false} };
    
    DWORD exStyle = GetWindowLongW(hwnd, GWL_EXSTYLE);
    bool wasOnTop = (exStyle & WS_EX_TOPMOST) != 0;
    
    SetWindowPos(
        hwnd,
        wasOnTop ? HWND_NOTOPMOST : HWND_TOPMOST,
        0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE
    );

    // 主窗口期望置顶状态登记（同 WindowSetAlwaysOnTop，守护矫正参照）
    {
        auto* mainWin = WindowManager::GetInstance().GetMainWindow();
        if (mainWin && mainWin->GetShellHwnd() == hwnd) {
            mainWin->SetExpectedTopmost(!wasOnTop);
        }
    }
    
    return {
        {"success", true},
        {"enabled", !wasOnTop},
    };
}


// ========== Bounds ==========

json WindowGetBounds(const json& params) {
    HWND hwnd = GetCallerHwnd(params);
    if (!hwnd) {
        return { {"x", 0}, {"y", 0}, {"width", 0}, {"height", 0} };
    }
    
    RECT rc;
    GetWindowRect(hwnd, &rc);
    
    return {
        {"x", rc.left},
        {"y", rc.top},
        {"width", rc.right - rc.left},
        {"height", rc.bottom - rc.top},
    };
}


json WindowSetBounds(const json& params) {
    PANEL_MODE_UNSUPPORTED("window.setBounds");
    HWND hwnd = GetCallerHwnd(params);
    if (!hwnd) return { {"success", false}, {"error", "Window not found"} };
    
    bool hasX = params.contains("x");
    bool hasY = params.contains("y");
    bool hasWidth = params.contains("width");
    bool hasHeight = params.contains("height");
    
    RECT currentRect;
    GetWindowRect(hwnd, &currentRect);
    
    int x = hasX ? params["x"].get<int>() : currentRect.left;
    int y = hasY ? params["y"].get<int>() : currentRect.top;
    int width = hasWidth ? params["width"].get<int>() : (currentRect.right - currentRect.left);
    int height = hasHeight ? params["height"].get<int>() : (currentRect.bottom - currentRect.top);
    
    UINT flags = SWP_NOZORDER;
    if (!hasX && !hasY) flags |= SWP_NOMOVE;
    if (!hasWidth && !hasHeight) flags |= SWP_NOSIZE;
    
    SetWindowPos(hwnd, nullptr, x, y, width, height, flags);
    
    return { {"success", true} };
}


json WindowCenter(const json& params) {
    PANEL_MODE_UNSUPPORTED("window.center");
    HWND hwnd = GetCallerHwnd(params);
    if (!hwnd) return { {"success", false}, {"error", "Window not found"} };
    
    RECT windowRect;
    GetWindowRect(hwnd, &windowRect);
    int width = windowRect.right - windowRect.left;
    int height = windowRect.bottom - windowRect.top;
    
    // Get monitor info
    MONITORINFO mi = { sizeof(MONITORINFO) };
    GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi);
    
    int x = mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - width) / 2;
    int y = mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - height) / 2;
    
    SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
    
    return { {"success", true} };
}


// ========== Size Constraints ==========

json WindowSetMinSize(const json& params) {
    PANEL_MODE_UNSUPPORTED("window.setMinSize");
    auto* ui = WebViewUI::GetInstance();
    if (!ui || !ui->GetMainWindow()) {
        return { {"success", false}, {"error", "Window not found"} };
    }
    
    int width = params.value("width", 0);
    int height = params.value("height", 0);
    
    ui->GetMainWindow()->SetMinSize(width, height);
    
    return { {"success", true} };
}


json WindowGetMinSize(const json& params) {
    auto* ui = WebViewUI::GetInstance();
    if (!ui || !ui->GetMainWindow()) {
        return { {"width", 0}, {"height", 0} };
    }
    
    int width, height;
    ui->GetMainWindow()->GetMinSize(width, height);
    
    return { {"width", width}, {"height", height} };
}


json WindowSetMaxSize(const json& params) {
    PANEL_MODE_UNSUPPORTED("window.setMaxSize");
    auto* ui = WebViewUI::GetInstance();
    if (!ui || !ui->GetMainWindow()) {
        return { {"success", false}, {"error", "Window not found"} };
    }
    
    int width = params.value("width", 0);
    int height = params.value("height", 0);
    
    ui->GetMainWindow()->SetMaxSize(width, height);
    
    return { {"success", true} };
}


json WindowGetMaxSize(const json& params) {
    auto* ui = WebViewUI::GetInstance();
    if (!ui || !ui->GetMainWindow()) {
        return { {"width", 0}, {"height", 0} };
    }
    
    int width, height;
    ui->GetMainWindow()->GetMaxSize(width, height);
    
    return { {"width", width}, {"height", height} };
}


json WindowSetResizable(const json& params) {
    PANEL_MODE_UNSUPPORTED("window.setResizable");
    auto* ui = WebViewUI::GetInstance();
    if (!ui || !ui->GetMainWindow()) {
        return { {"success", false}, {"error", "Window not found"} };
    }
    
    bool resizable = params.value("resizable", true);
    ui->GetMainWindow()->SetResizable(resizable);
    
    return { {"success", true} };
}


json WindowIsResizable(const json& params) {
    auto* ui = WebViewUI::GetInstance();
    if (!ui || !ui->GetMainWindow()) {
        return { {"resizable", true} };
    }
    
    return { {"resizable", ui->GetMainWindow()->IsResizable()} };
}


// ========== Fullscreen ==========

json WindowSetFullscreen(const json& params) {
    PANEL_MODE_UNSUPPORTED("window.setFullscreen");
    auto target = WindowTargetResolver::ResolveForMutation(params);
    if (!target.Success()) return target.ErrorResponse();
    if (!target.shell->GetCapabilities().supportsFullscreen) {
        return {{"success", false}, {"fullscreen", target.shell->IsFullscreen()},
            {"error", "this window does not support fullscreen"}};
    }
    
    bool enabled = params.value("enabled", true);
    bool currentlyFull = target.shell->IsFullscreen();
    
    if (enabled && !currentlyFull) {
        EnterFullscreenMode(target.shell->GetShellHwnd(), target.shell);
    }
    else if (!enabled && currentlyFull) {
        ExitFullscreenMode(target.shell->GetShellHwnd(), target.shell);
    }
    
    return { {"success", true}, {"fullscreen", target.shell->IsFullscreen()} };
}


json WindowToggleFullscreen(const json& params) {
    PANEL_MODE_UNSUPPORTED("window.toggleFullscreen");
    auto target = WindowTargetResolver::ResolveForMutation(params);
    if (!target.Success()) return target.ErrorResponse();
    if (!target.shell->GetCapabilities().supportsFullscreen) {
        return {{"success", false}, {"fullscreen", target.shell->IsFullscreen()},
            {"error", "this window does not support fullscreen"}};
    }
    
    bool currentlyFull = target.shell->IsFullscreen();
    if (!currentlyFull) {
        EnterFullscreenMode(target.shell->GetShellHwnd(), target.shell);
    } else {
        ExitFullscreenMode(target.shell->GetShellHwnd(), target.shell);
    }
    
    return {
        {"success", true},
        {"fullscreen", target.shell->IsFullscreen()},
    };
}


// ========== Focus ==========

json WindowFocus(const json& params) {
    // 支持可选 windowId 参数，用于从主窗口聚焦指定弹窗
    std::string targetId = params.value("windowId", "");
    HWND hwnd = nullptr;
    MainWindow* targetMainWin = nullptr;

    if (!targetId.empty()) {
        auto& wm = WindowManager::GetInstance();
        if (targetId == "main") {
            targetMainWin = wm.GetMainWindow();
            if (targetMainWin) hwnd = targetMainWin->GetShellHwnd();
        } else {
            auto* popup = wm.GetPopup(targetId);
            if (popup) hwnd = popup->GetShellHwnd();
        }
    } else {
        hwnd = GetCallerHwnd(params);
        if (hwnd) {
            auto* candidate = WindowManager::GetInstance().GetMainWindow();
            if (candidate && candidate->GetShellHwnd() == hwnd) {
                targetMainWin = candidate;
            }
        }
    }

    if (!hwnd) return { {"success", false}, {"error", "Window not found"} };

    // SWP_SHOWWINDOW will reveal a hidden window, so snapshot the hidden state
    // up front and reuse RestoreSurfaceAfterHidden to converge the WebView
    // surface, matching WebViewUI::activate / background_service::ShowWindow.
    const bool wasHidden = !IsWindowVisible(hwnd);
    
    // 先恢复最小化窗口
    if (IsIconic(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
    }
    
    // 绕过 Windows 前台锁超时 (ForegroundLockTimeout)
    // 当 fb2k 不是前台进程时，SetForegroundWindow 会被系统延迟 200ms~5s，
    // 导致 window.focus 偶发慢响应。AttachThreadInput 临时将当前线程附着到
    // 前台线程，使 SetForegroundWindow 视为同进程调用，绕过限制。
    DWORD foregroundTid = GetWindowThreadProcessId(GetForegroundWindow(), nullptr);
    DWORD currentTid = GetCurrentThreadId();
    bool attached = (foregroundTid != 0 && foregroundTid != currentTid)
        ? (AttachThreadInput(currentTid, foregroundTid, TRUE) != 0) : false;
    
    // 使用 SetWindowPos 确保窗口在 Z 序最前，然后激活
    SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    SetForegroundWindow(hwnd);
    BringWindowToTop(hwnd);
    
    if (attached) {
        AttachThreadInput(currentTid, foregroundTid, FALSE);
    }

    if (wasHidden && targetMainWin) {
        targetMainWin->RestoreSurfaceAfterHidden("window-focus");
    }

    return { {"success", true} };
}


json WindowBlur(const json& params) {
    // Can't reliably blur, just minimize
    HWND hwnd = GetCallerHwnd(params);
    if (!hwnd) return { {"success", false}, {"error", "Window not found"} };
    
    // Find next window and activate it
    HWND nextWindow = GetNextWindow(hwnd, GW_HWNDNEXT);
    if (nextWindow) {
        SetForegroundWindow(nextWindow);
    }
    
    return { {"success", true} };
}


// ========== Title ==========

json WindowSetTitle(const json& params) {
    PANEL_MODE_UNSUPPORTED("window.setTitle");
    HWND hwnd = GetCallerHwnd(params);
    if (!hwnd) return { {"success", false}, {"error", "Window not found"} };
    
    std::string title = params.value("title", "foobar2000");
    std::wstring wideTitle = Utf8ToWide(title);
    
    SetWindowTextW(hwnd, wideTitle.c_str());
    return { {"success", true} };
}


json WindowGetTitle(const json& params) {
    HWND hwnd = GetCallerHwnd(params);
    if (!hwnd) return { {"title", ""} };
    
    wchar_t title[256];
    GetWindowTextW(hwnd, title, 256);
    std::string utf8Title = WideToUtf8(title);
    
    return { {"title", utf8Title} };
}


// ========== Flash ==========

json WindowFlash(const json& params) {
    PANEL_MODE_UNSUPPORTED("window.flash");
    HWND hwnd = GetCallerHwnd(params);
    if (!hwnd) return { {"success", false}, {"error", "Window not found"} };
    
    bool enabled = params.value("enabled", true);
    int count = params.value("count", 3);
    
    FLASHWINFO fi = { sizeof(FLASHWINFO) };
    fi.hwnd = hwnd;
    fi.dwFlags = enabled ? (FLASHW_ALL | FLASHW_TIMERNOFG) : FLASHW_STOP;
    fi.uCount = count;
    fi.dwTimeout = 0;
    
    FlashWindowEx(&fi);
    
    return { {"success", true} };
}


// ========== System Menu ==========
// 显示系统菜单（最小化/最大化/关闭等）
// 支持传入触发元素的矩形区域，避免菜单遮挡按钮

json WindowShowSystemMenu(const json& params) {
    PANEL_MODE_UNSUPPORTED("window.showSystemMenu");
    HWND hwnd = GetCallerHwnd(params);
    if (!hwnd) return { {"success", false}, {"error", "Window not found"} };
    
    int x = params.value("x", 0);
    int y = params.value("y", 0);
    int w = params.value("w", 0);
    int h = params.value("h", 0);
    
    HMENU sysMenu = GetSystemMenu(hwnd, FALSE);
    if (!sysMenu) {
        return { {"success", false}, {"error", "System menu not available"} };
    }
    
    UINT cmd = 0;
    if (w > 0 && h > 0) {
        // 使用 TrackPopupMenuEx 支持排除区域
        // 菜单会避开按钮区域，不会遮挡
        TPMPARAMS tpm = { sizeof(TPMPARAMS) };
        tpm.rcExclude.left = x;
        tpm.rcExclude.top = y;
        tpm.rcExclude.right = x + w;
        tpm.rcExclude.bottom = y + h;
        
        // TPM_VERTICAL: 优先垂直定位（上下弹出）
        cmd = TrackPopupMenuEx(
            sysMenu,
            TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD | TPM_VERTICAL,
            x, y + h,  // 默认在按钮下方弹出
            hwnd, &tpm
        );
    } else {
        cmd = TrackPopupMenu(
            sysMenu,
            TPM_LEFTBUTTON | TPM_RETURNCMD,
            x, y,
            0, hwnd, nullptr
        );
    }
    
    if (cmd) {
        PostMessage(hwnd, WM_SYSCOMMAND, cmd, 0);
    }
    
    return { {"success", true} };
}


// ============================================
// 标题栏相关 API (客户区扩展支持)
// ============================================

json WindowGetTitlebarHeight(const json& params) {
    HWND callerHwnd = GetCallerHwnd(params);
    if (auto* mainWnd = FindMainByCallerHwnd(callerHwnd)) {
        return { {"height", mainWnd->GetTitlebarHeight()} };
    }
    if (auto* popupWnd = FindPopupByCallerHwnd(callerHwnd)) {
        return { {"height", popupWnd->GetTitlebarHeight()} };
    }
    return { {"height", MainWindow::DEFAULT_TITLEBAR_HEIGHT} };
}


json WindowSetTitlebarHeight(const json& params) {
    PANEL_MODE_UNSUPPORTED("window.setTitlebarHeight");
    int height = params.value("height", 32);
    if (height < 24 || height > 100) {
        return { {"success", false}, {"error", "Height must be between 24 and 100"} };
    }

    HWND callerHwnd = GetCallerHwnd(params);
    if (auto* mainWnd = FindMainByCallerHwnd(callerHwnd)) {
        mainWnd->SetTitlebarHeight(height);
        return { {"success", true}, {"height", height} };
    }
    if (auto* popupWnd = FindPopupByCallerHwnd(callerHwnd)) {
        popupWnd->SetTitlebarHeight(height);
        return { {"success", true}, {"height", height} };
    }

    return { {"success", false}, {"error", "Window not found"} };
}


json WindowGetCaptionButtonsWidth(const json& params) {
    auto* ui = WebViewUI::GetInstance();
    if (!ui || !ui->GetMainWindow()) {
        return { {"width", 138}, {"buttonWidth", 46} };
    }

    auto* mainWnd = ui->GetMainWindow();
    return {
        {"width", mainWnd->GetCaptionButtonsWidth()},
        {"buttonWidth", mainWnd->GetCaptionButtonWidth()}
    };
}


json WindowSetDragRegions(const json& params) {
    PANEL_MODE_UNSUPPORTED("window.setDragRegions");
    HWND callerHwnd = GetCallerHwnd(params);
    if (!callerHwnd) {
        return { {"success", false}, {"error", "Window not found"} };
    }
    
    // 获取 DPI 缩放比例 (CSS 像素 -> 物理像素)
    HWND hwnd = callerHwnd;
    double dpiScale = 1.0;
    if (hwnd) {
        int dpi = GetDpiForWindow(hwnd);
        dpiScale = dpi / 96.0;
    }
    
    std::vector<TitlebarDragRegion> regions;
    
    const auto& regionsArr = params.contains("regions") && params["regions"].is_array()
        ? params["regions"] : json::array();
    for (const auto& r : regionsArr) {
        TitlebarDragRegion region;
        // 将 CSS 像素转换为物理像素
        region.x = static_cast<int>(r.value("x", 0) * dpiScale);
        region.y = static_cast<int>(r.value("y", 0) * dpiScale);
        region.width = static_cast<int>(r.value("width", 0) * dpiScale);
        region.height = static_cast<int>(r.value("height", 0) * dpiScale);
        
        if (region.width <= 0 || region.height <= 0)
            continue;
        regions.push_back(region);
    }

    if (auto* mainWnd = FindMainByCallerHwnd(callerHwnd)) {
        mainWnd->SetDragRegions(regions);
    } else if (auto* popupWnd = FindPopupByCallerHwnd(callerHwnd)) {
        popupWnd->SetDragRegions(ConvertDragRegionsForPopup(regions));
    } else {
        return { {"success", false}, {"error", "Window not found"} };
    }

    return { {"success", true}, {"count", regions.size()}, {"dpiScale", dpiScale} };
}


json WindowClearDragRegions(const json& params) {
    PANEL_MODE_UNSUPPORTED("window.clearDragRegions");
    HWND callerHwnd = GetCallerHwnd(params);
    if (auto* mainWnd = FindMainByCallerHwnd(callerHwnd)) {
        mainWnd->ClearDragRegions();
        return { {"success", true} };
    }
    if (auto* popupWnd = FindPopupByCallerHwnd(callerHwnd)) {
        popupWnd->ClearDragRegions();
        return { {"success", true} };
    }
    return { {"success", false}, {"error", "Window not found"} };
}


json WindowSetNoDragRegions(const json& params) {
    PANEL_MODE_UNSUPPORTED("window.setNoDragRegions");
    HWND callerHwnd = GetCallerHwnd(params);
    if (!callerHwnd) {
        return { {"success", false}, {"error", "Window not found"} };
    }
    
    // 获取 DPI 缩放比例 (CSS 像素 -> 物理像素)
    HWND hwnd = callerHwnd;
    double dpiScale = 1.0;
    if (hwnd) {
        int dpi = GetDpiForWindow(hwnd);
        dpiScale = dpi / 96.0;
    }
    
    std::vector<TitlebarDragRegion> regions;
    
    const auto& noDragArr = params.contains("regions") && params["regions"].is_array()
        ? params["regions"] : json::array();
    for (const auto& r : noDragArr) {
        TitlebarDragRegion region;
        // 将 CSS 像素转换为物理像素
        region.x = static_cast<int>(r.value("x", 0) * dpiScale);
        region.y = static_cast<int>(r.value("y", 0) * dpiScale);
        region.width = static_cast<int>(r.value("width", 0) * dpiScale);
        region.height = static_cast<int>(r.value("height", 0) * dpiScale);
        
        if (region.width <= 0 || region.height <= 0)
            continue;
        regions.push_back(region);
    }

    if (auto* mainWnd = FindMainByCallerHwnd(callerHwnd)) {
        mainWnd->SetNoDragRegions(regions);
    } else if (auto* popupWnd = FindPopupByCallerHwnd(callerHwnd)) {
        popupWnd->SetNoDragRegions(ConvertDragRegionsForPopup(regions));
    } else {
        return { {"success", false}, {"error", "Window not found"} };
    }

    return { {"success", true}, {"count", regions.size()}, {"dpiScale", dpiScale} };
}


json WindowClearNoDragRegions(const json& params) {
    PANEL_MODE_UNSUPPORTED("window.clearNoDragRegions");
    HWND callerHwnd = GetCallerHwnd(params);
    if (auto* mainWnd = FindMainByCallerHwnd(callerHwnd)) {
        mainWnd->ClearNoDragRegions();
        return { {"success", true} };
    }
    if (auto* popupWnd = FindPopupByCallerHwnd(callerHwnd)) {
        popupWnd->ClearNoDragRegions();
        return { {"success", true} };
    }
    return { {"success", false}, {"error", "Window not found"} };
}


json WindowGetTitlebarInfo(const json& params) {
    auto* ui = WebViewUI::GetInstance();
    if (!ui || !ui->GetMainWindow()) {
        return {
            {"height", 32},
            {"captionButtonsWidth", 138},
            {"captionButtonWidth", 46},
            {"isMaximized", false}
        };
    }
    
    auto* mainWnd = ui->GetMainWindow();
    HWND hwnd = mainWnd->GetHwnd();
    
    return {
        {"height", mainWnd->GetTitlebarHeight()},
        {"captionButtonsWidth", mainWnd->GetCaptionButtonsWidth()},
        {"captionButtonWidth", mainWnd->GetCaptionButtonWidth()},
        {"isMaximized", IsZoomed(hwnd) != FALSE}
    };
}


// ========== UI Context Menu ==========
// 显示 foobar2000 风格的上下文菜单

json UiShowContextMenu(const json& params) {
    auto* ui = WebViewUI::GetInstance();
    if (!ui || !ui->GetMainWindow()) {
        return { {"success", false}, {"error", "Window not found"} };
    }
    
    int x = params.value("x", -1);
    int y = params.value("y", -1);
    
    // 如果没有传递有效坐标，使用当前鼠标位置
    // DPI 校正：对比实际鼠标位置与 JS 传递的坐标
    POINT cursorPt;
    GetCursorPos(&cursorPt);
    
    // Invalid coords (<=0) or DPI-scaling drift (>50px) both fall back to the
    // actual cursor position; the legacy two-branch form had identical bodies.
    if (x <= 0 || y <= 0 ||
        abs(cursorPt.x - x) > 50 || abs(cursorPt.y - y) > 50) {
        x = cursorPt.x;
        y = cursorPt.y;
    }
    
    // 调用 MainWindow 的上下文菜单
    ui->GetMainWindow()->ShowContextMenu(x, y);
    
    return { {"success", true} };
}


// ========== System Theme API ==========

// system.getTheme - Get system theme information
json SystemGetTheme(const json& params) {
    bool darkMode = false;
    std::string accentColor = "#0078D4";  // Default Windows blue
    bool transparency = true;
    
    // Check dark mode setting
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        
        DWORD value = 1, size = sizeof(DWORD);
        if (RegQueryValueExW(hKey, L"AppsUseLightTheme", nullptr, nullptr,
            reinterpret_cast<LPBYTE>(&value), &size) == ERROR_SUCCESS) {
            darkMode = (value == 0);
        }
        
        // Check transparency setting
        DWORD transValue = 1;
        size = sizeof(DWORD);
        if (RegQueryValueExW(hKey, L"EnableTransparency", nullptr, nullptr,
            reinterpret_cast<LPBYTE>(&transValue), &size) == ERROR_SUCCESS) {
            transparency = (transValue != 0);
        }
        
        RegCloseKey(hKey);
    }
    
    // Get accent color from DWM
    DWORD color = 0;
    BOOL opaque = FALSE;
    if (SUCCEEDED(S_DwmGetColorizationColor(&color, &opaque))) {
        // Convert ARGB to hex string
        char hex[8];
        sprintf_s(hex, "#%02X%02X%02X", 
            (color >> 16) & 0xFF,  // R
            (color >> 8) & 0xFF,   // G
            color & 0xFF);          // B
        accentColor = hex;
    }
    
    return {
        {"darkMode", darkMode},
        {"isDark", darkMode},  // alias for tests
        {"accentColor", accentColor},
        {"transparency", transparency}
    };
}


// system.getDPI - Get DPI scaling information
json SystemGetDPI(const json& params) {
    HWND hwnd = GetCallerHwnd(params);
    UINT dpi = 96;  // Default DPI
    
    if (hwnd) {
        // Try GetDpiForWindow (Windows 10 1607+)
        using GetDpiForWindowFunc = UINT (WINAPI *)(HWND);
        static auto pGetDpiForWindow = reinterpret_cast<GetDpiForWindowFunc>(
            GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));
        
        if (pGetDpiForWindow) {
            dpi = pGetDpiForWindow(hwnd);
        } else {
            // Fallback: use DC DPI
            HDC hdc = GetDC(hwnd);
            if (hdc) {
                dpi = GetDeviceCaps(hdc, LOGPIXELSX);
                ReleaseDC(hwnd, hdc);
            }
        }
    }
    
    double scale = static_cast<double>(dpi) / 96.0;
    
    return {
        {"dpi", dpi},
        {"scale", scale}
    };
}


// ========== Corner Preference (Windows 11+) ==========

json WindowSetCornerPreference(const json& params) {
    PANEL_MODE_UNSUPPORTED("window.setCornerPreference");
    MainWindow* mainWindow = GetMainWindow();
    if (!mainWindow) return { {"success", false}, {"error", "Window not found"} };
    
    std::string mode = params.value("mode", "default");
    mainWindow->SetCornerPreference(mode);
    
    return { {"success", true} };
}


json WindowGetCornerPreference(const json& params) {
    MainWindow* mainWindow = GetMainWindow();
    if (!mainWindow) return { {"mode", "default"}, {"preference", "default"} };
    
    std::string pref = mainWindow->GetCornerPreference();
    return { {"mode", pref}, {"preference", pref} };
}


// system.getLocale - Get system locale information
json SystemGetLocale(const json& params) {
    wchar_t localeName[LOCALE_NAME_MAX_LENGTH];
    std::string locale = "en-US";  // Default
    
    if (GetUserDefaultLocaleName(localeName, LOCALE_NAME_MAX_LENGTH)) {
        locale = WideToUtf8(localeName);
    }
    
    // Get language name
    wchar_t langName[256];
    std::string language = "";
    if (GetLocaleInfoEx(localeName, LOCALE_SLOCALIZEDLANGUAGENAME, langName, 256)) {
        language = WideToUtf8(langName);
    }
    
    // Get country name
    wchar_t countryName[256];
    std::string country = "";
    if (GetLocaleInfoEx(localeName, LOCALE_SLOCALIZEDCOUNTRYNAME, countryName, 256)) {
        country = WideToUtf8(countryName);
    }
    
    return {
        {"locale", locale},
        {"language", language},
        {"country", country}
    };
}


// ========== Additional Window APIs ==========

json WindowSetPosition(const json& params) {
    PANEL_MODE_UNSUPPORTED("window.setPosition");
    HWND hwnd = GetCallerHwnd(params);
    if (!hwnd) return {{"success", false}};
    int x = params.value("x", 0);
    int y = params.value("y", 0);
    SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    return {{"success", true}};
}


json WindowSetSize(const json& params) {
    PANEL_MODE_UNSUPPORTED("window.setSize");
    HWND hwnd = GetCallerHwnd(params);
    if (!hwnd) return {{"success", false}};
    int width = params.value("width", 800);
    int height = params.value("height", 600);
    SetWindowPos(hwnd, nullptr, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER);
    return {{"success", true}};
}


json WindowGetDpiScale(const json& params) {
    HWND hwnd = GetCallerHwnd(params);
    int dpi = 96;
    if (hwnd) {
        HDC hdc = GetDC(hwnd);
        if (hdc) {
            dpi = GetDeviceCaps(hdc, LOGPIXELSX);
            ReleaseDC(hwnd, hdc);
        }
    }
    return {{"success", true}, {"dpi", dpi}, {"scale", dpi / 96.0}};
}


json WindowFlashTaskbar(const json& params) {
    PANEL_MODE_UNSUPPORTED("window.flashTaskbar");
    HWND hwnd = GetCallerHwnd(params);
    if (!hwnd) return {{"success", false}};
    int count = params.value("count", 3);
    FLASHWINFO fi = {sizeof(FLASHWINFO), hwnd, FLASHW_ALL, (UINT)count, 0};
    FlashWindowEx(&fi);
    return {{"success", true}};
}


json WindowEnterFullscreen(const json& params) {
    PANEL_MODE_UNSUPPORTED("window.enterFullscreen");
    auto target = WindowTargetResolver::ResolveForMutation(params);
    if (!target.Success()) return target.ErrorResponse();
    if (!target.shell->GetCapabilities().supportsFullscreen) {
        return {{"success", false}, {"isFullscreen", target.shell->IsFullscreen()},
            {"error", "this window does not support fullscreen"}};
    }
    if (target.shell->IsFullscreen()) return {{"success", false}};
    
    // fullscreen implementation still in WindowApi.cpp (stealth DWM)
    EnterFullscreenMode(target.shell->GetShellHwnd(), target.shell);
    return {{"success", true}, {"isFullscreen", true}};
}


json WindowExitFullscreen(const json& params) {
    PANEL_MODE_UNSUPPORTED("window.exitFullscreen");
    auto target = WindowTargetResolver::ResolveForMutation(params);
    if (!target.Success()) return target.ErrorResponse();
    if (!target.shell->GetCapabilities().supportsFullscreen) {
        return {{"success", false}, {"isFullscreen", target.shell->IsFullscreen()},
            {"error", "this window does not support fullscreen"}};
    }
    if (!target.shell->IsFullscreen()) return {{"success", false}};
    
    // fullscreen implementation still in WindowApi.cpp (stealth DWM)
    ExitFullscreenMode(target.shell->GetShellHwnd(), target.shell);
    return {{"success", true}, {"isFullscreen", false}};
}


// Mica 效果 - 使用公共实现
json WindowSetMica(const json& params) {
    return SetMicaEffectImpl(params);
}


// 别名: window.setMicaEffect -> window.setMica (兼容性)
json WindowSetMicaEffect(const json& params) {
    return SetMicaEffectImpl(params);
}


json WindowSetBlur(const json& params) {
    PANEL_MODE_UNSUPPORTED("window.setBlur");
    auto target = WindowTargetResolver::ResolveForMutation(params);
    if (!target.Success()) return target.ErrorResponse();
    bool enabled = params.value("enabled", true);

    const bool success = target.shell->PatchCompatibilityBlur(enabled);
    return {{"success", success}, {"enabled", enabled}};
}


json WindowSetAcrylic(const json& params) {
    PANEL_MODE_UNSUPPORTED("window.setAcrylic");
    auto target = WindowTargetResolver::ResolveForMutation(params);
    if (!target.Success()) return target.ErrorResponse();
    
    bool enabled = params.value("enabled", true);
    
    // darkMode 参数: true=深色, false=浅色
    bool hasDarkModeParam = params.contains("darkMode");
    bool darkMode = params.value("darkMode", true);

    const bool success = target.shell->PatchCompatibilityBackdrop(
        enabled ? std::optional<std::string>("acrylic") : std::optional<std::string>(),
        hasDarkModeParam ? std::optional<bool>(darkMode) : std::optional<bool>(),
        enabled);

    json result = {{"success", success}, {"enabled", enabled}};
    if (hasDarkModeParam) {
        result["darkMode"] = darkMode;
    }
    return result;
}


json WindowSetDarkMode(const json& params) {
    PANEL_MODE_UNSUPPORTED("window.setDarkMode");
    auto target = WindowTargetResolver::ResolveForMutation(params);
    if (!target.Success()) return target.ErrorResponse();
    bool enabled = params.value("enabled", true);
    bool success = target.shell->PatchCompatibilityDarkMode(enabled);
    return {{"success", success}, {"enabled", enabled}};
}


// ============================================
// WebView 背景透明度控制 - 用于 DWM 效果穿透
// ============================================

json WindowSetBackgroundTransparency(const json& params) {
    bool transparent = params.value("transparent", true);
    auto target = WindowTargetResolver::ResolveForMutation(params);
    if (!target.Success()) return target.ErrorResponse();
    
    bool compatibilitySuccess = target.shell->PatchCompatibilityTransparency(transparent);
    
    // WebView host 透明度也需要同步设置
    auto* host = WebViewContext::GetInstance().GetHostByHwnd(target.shell->GetShellHwnd());
    HRESULT hr = E_FAIL;
    if (host) {
        hr = host->SetBackgroundTransparent(transparent);
    }
    if (!host && !compatibilitySuccess) {
        return {{"success", false}, {"error", "WebView not available"}};
    }
    
    return {
        {"success", SUCCEEDED(hr) || compatibilitySuccess},
        {"transparent", transparent},
        {"description", transparent 
            ? "WebView background is now transparent - DWM effects will show through" 
            : "WebView background is now opaque"}
    };
}


// 刷新 WebView 渲染（DWM 效果后调用）
json WindowRefreshWebView(const json& params) {
    HWND callerHwnd = GetCallerHwnd(params);
    auto* host = WebViewContext::GetInstance().GetHostByHwnd(callerHwnd);
    if (!host) {
        return {{"success", false}, {"error", "WebView not available"}};
    }
    
    host->RefreshForDwmEffect();
    return {{"success", true}};
}


// 重新加载 WebView 页面（开发调试用）
json WindowReload(const json& params) {
    HWND callerHwnd = GetCallerHwnd(params);
    auto* host = WebViewContext::GetInstance().GetHostByHwnd(callerHwnd);
    if (!host) {
        return {{"success", false}, {"error", "WebView not available"}};
    }
    
    host->Reload();
    return {{"success", true}};
}


// ============================================
// HMR 开发服务器配置 API
// ============================================

json WindowGetDevServerConfig(const json& params) {
    return {
        {"success", true},
        {"useDevServer", security_config::UseDevServer()},
        {"devServerUrl", security_config::GetDevServerUrl()}
    };
}


json WindowSetDevServerConfig(const json& params) {
    bool useDevServer = params.value("useDevServer", false);
    std::string devServerUrl = params.value("devServerUrl", "");
    
    security_config::SetUseDevServer(useDevServer);
    security_config::SetDevServerUrl(devServerUrl.c_str());
    
    return {
        {"success", true},
        {"useDevServer", useDevServer},
        {"devServerUrl", devServerUrl}
    };
}


// ============================================
// 检查是否有保存的窗口位置
// 前端可以使用此 API 来决定是否设置默认窗口大小
// ============================================
json WindowHasSavedBounds(const json& params) {
    bool hasSaved = window_config::HasSavedPosition();
    return {
        {"hasSavedBounds", hasSaved},
        {"description", hasSaved 
            ? "Window has saved position from previous session" 
            : "No saved window position, this may be first launch"}
    };
}



// ============================================
// 缩放控制 API (DPI 适配)
// ============================================

// 设置缩放比例
json WindowSetZoom(const json& params) {
    HWND callerHwnd = GetCallerHwnd(params);
    auto* host = WebViewContext::GetInstance().GetHostByHwnd(callerHwnd);
    if (!host) {
        return {{"success", false}, {"error", "WebView not available"}};
    }
    
    double zoomFactor = params.value("zoom", 1.0);
    HRESULT hr = host->SetZoomFactor(zoomFactor);
    
    return {
        {"success", SUCCEEDED(hr)},
        {"zoom", host->GetZoomFactor()}
    };
}


// 获取当前缩放比例
json WindowGetZoom(const json& params) {
    HWND callerHwnd = GetCallerHwnd(params);
    auto* host = WebViewContext::GetInstance().GetHostByHwnd(callerHwnd);
    if (!host) {
        return {{"success", true}, {"zoom", 1.0}};
    }
    
    double zoomFactor = host->GetZoomFactor();
    
    // 获取当前 DPI 信息
    int dpi = 96;
    if (callerHwnd) {
        dpi = GetDpiForWindow(callerHwnd);
    }
    
    return {
        {"success", true},
        {"zoom", zoomFactor},
        {"dpi", dpi},
        {"dpiScale", dpi / 96.0}
    };
}


// 重置缩放为默认
json WindowResetZoom(const json& params) {
    HWND callerHwnd = GetCallerHwnd(params);
    auto* host = WebViewContext::GetInstance().GetHostByHwnd(callerHwnd);
    if (!host) {
        return {{"success", false}, {"error", "WebView not available"}};
    }
    
    HRESULT hr = host->SetZoomFactor(1.0);
    
    return {
        {"success", SUCCEEDED(hr)},
        {"zoom", 1.0}
    };
}


// 根据 DPI 自动设置缩放
json WindowSetZoomForDpi(const json& params) {
    HWND callerHwnd = GetCallerHwnd(params);
    auto* host = WebViewContext::GetInstance().GetHostByHwnd(callerHwnd);
    if (!host) {
        return {{"success", false}, {"error", "WebView not available"}};
    }
    
    int dpi = params.value("dpi", 0);
    
    // 如果未指定 DPI，使用当前窗口 DPI
    if (dpi <= 0 && callerHwnd) {
        dpi = GetDpiForWindow(callerHwnd);
    }
    if (dpi <= 0) dpi = 96;
    
    HRESULT hr = host->SetZoomForDpi(dpi);
    
    return {
        {"success", SUCCEEDED(hr)},
        {"dpi", dpi},
        {"zoom", host->GetZoomFactor()}
    };
}


// ============================================
// 多窗口 API
// ============================================

// 创建弹出窗口
json WindowCreatePopup(const json& params) {
    // 惰性初始化：面板模式下自动初始化 WindowManager
    auto& wm = WindowManager::GetInstance();
    if (!wm.IsInitialized()) {
        wm.InitializeForPanel();
    }
    
    PopupWindow::CreateParams cp;
    cp.url = params.value("url", "");
    cp.width = params.value("width", 400);
    cp.height = params.value("height", 300);
    cp.x = params.value("x", (int)CW_USEDEFAULT);
    cp.y = params.value("y", (int)CW_USEDEFAULT);
    cp.title = params.value("title", "");
    cp.resizable = params.value("resizable", true);
    cp.alwaysOnTop = params.value("alwaysOnTop", false);
    cp.showInTaskbar = params.value("showInTaskbar", false);
    cp.hasShowInTaskbar = params.contains("showInTaskbar");
    cp.minWidth = params.value("minWidth", 200);
    cp.minHeight = params.value("minHeight", 150);
    cp.maxWidth = params.value("maxWidth", 0);
    cp.maxHeight = params.value("maxHeight", 0);
    cp.frame = params.value("frame", true);
    cp.transparent = params.value("transparent", false);
    cp.beforeClose = params.value("beforeClose", false);
    cp.clickThrough = params.value("clickThrough", false);
    cp.profile = params.value("profile", "");
    cp.hasProfile = params.contains("profile");
    if (params.contains("behavior") && !params["behavior"].is_object()) {
        return {{"success", false}, {"error", "behavior must be an object"}};
    }
    if (params.contains("behavior") && params["behavior"].is_object()) {
        cp.behavior = params["behavior"];
        cp.hasBehavior = true;
    }
    if (params.contains("backdropPolicy") && !params["backdropPolicy"].is_object()) {
        return {{"success", false}, {"error", "backdropPolicy must be an object"}};
    }
    if (params.contains("backdropPolicy") && params["backdropPolicy"].is_object()) {
        cp.backdropPolicy = params["backdropPolicy"];
        cp.hasBackdropPolicy = true;
    }
    
    if (static_cast<int>(wm.GetPopupCount()) >= WindowManager::MAX_POPUPS) {
        return {{"success", false}, {"error", ApiError::MAX_POPUPS_REACHED}};
    }
    
    std::string windowId = wm.CreatePopup(cp);
    if (windowId.empty()) {
        return {{"success", false}, {"error", ApiError::POPUP_CREATE_FAILED}};
    }
    
    return {{"success", true}, {"windowId", windowId}};
}


// 关闭弹出窗口
json WindowClosePopup(const json& params) {
    
    std::string windowId = params.value("windowId", "");
    if (windowId.empty()) {
        return {{"success", false}, {"error", ApiError::WINDOW_ID_REQUIRED}};
    }
    if (windowId == "main") {
        return {{"success", false}, {"error", ApiError::CANNOT_CLOSE_MAIN}};
    }
    
    bool ok = WindowManager::GetInstance().ClosePopup(windowId);
    if (!ok) {
        return {{"success", false}, {"error", ApiError::WINDOW_NOT_FOUND}};
    }
    return {{"success", true}};
}


// 关闭所有弹出窗口
json WindowCloseAllPopups(const json& params) {
    WindowManager::GetInstance().CloseAllPopups();
    return {{"success", true}};
}


// 获取所有窗口信息
json WindowGetAllWindows(const json& params) {
    return {{"success", true}, {"items", WindowManager::GetInstance().GetAllWindowsInfo()}};
}


// 获取当前窗口 ID
json WindowGetCurrentWindowId(const json& params) {
    HWND callerHwnd = GetCallerHwnd(params);
    auto& wm = WindowManager::GetInstance();
    
    // 检查主窗口
    auto* mainWin = wm.GetMainWindow();
    if (mainWin && mainWin->GetHwnd() == callerHwnd) {
        return {{"success", true}, {"windowId", "main"}};
    }
    
    // 检查弹出窗口
    auto ids = wm.GetAllWindowIds();
    for (const auto& id : ids) {
        if (id == "main") continue;
        auto* popup = wm.GetPopup(id);
        if (popup && popup->GetHwnd() == callerHwnd) {
            return {{"success", true}, {"windowId", id}};
        }
    }
    
    // 面板模式: 通过 WebViewContext 反查 windowId
    auto& ctx = WebViewContext::GetInstance();
    for (auto instanceHwnd : ctx.GetAllInstances()) {
        if (instanceHwnd == callerHwnd ||
            ::GetAncestor(instanceHwnd, GA_ROOT) == callerHwnd) {
            std::string wid = ctx.GetWindowIdByHwnd(instanceHwnd);
            if (!wid.empty()) {
                return {{"success", true}, {"windowId", wid}};
            }
        }
    }
    
    // 最终回退
    return {{"success", true}, {"windowId", "main"}};
}


// 获取弹出窗口行为策略
json WindowGetPopupBehavior(const json& params) {
    std::string targetId = params.value("windowId", "");
    PopupWindow* popup = nullptr;

    if (!targetId.empty()) {
        if (targetId == "main") {
            return {{"success", false}, {"error", "window.getPopupBehavior does not support main window"}};
        }
        popup = WindowManager::GetInstance().GetPopup(targetId);
    } else {
        HWND callerHwnd = GetCallerHwnd(params);
        popup = FindPopupByCallerHwnd(callerHwnd, &targetId);
    }

    if (!popup) {
        return {{"success", false}, {"error", ApiError::WINDOW_NOT_FOUND}};
    }

    json info = popup->GetPopupBehaviorInfo();
    info["success"] = true;
    info["windowId"] = targetId;
    return info;
}


// 运行时更新弹出窗口行为策略
json WindowSetPopupBehavior(const json& params) {
    std::string targetId = params.value("windowId", "");
    PopupWindow* popup = nullptr;

    if (!targetId.empty()) {
        if (targetId == "main") {
            return {{"success", false}, {"error", "window.setPopupBehavior does not support main window"}};
        }
        popup = WindowManager::GetInstance().GetPopup(targetId);
    } else {
        HWND callerHwnd = GetCallerHwnd(params);
        popup = FindPopupByCallerHwnd(callerHwnd, &targetId);
    }

    if (!popup) {
        return {{"success", false}, {"error", ApiError::WINDOW_NOT_FOUND}};
    }

    bool hasProfile = params.contains("profile");
    bool hasBehavior = params.contains("behavior");
    std::string profile = params.value("profile", "");
    json behaviorPatch = hasBehavior ? params["behavior"] : json::object();

    std::string error;
    if (!popup->UpdatePopupBehavior(profile, hasProfile, behaviorPatch, hasBehavior, error)) {
        return {{"success", false}, {"error", error.empty() ? "failed to update popup behavior" : error}};
    }

    json info = popup->GetPopupBehaviorInfo();
    info["success"] = true;
    info["windowId"] = targetId;
    return info;
}


// 获取 DWM 背景策略
json WindowGetBackdropPolicy(const json& params) {
    auto target = WindowTargetResolver::ResolveForObservation(params);
    if (!target.Success()) return target.ErrorResponse();

    json info = target.shell->GetBackdropPolicyInfo();
    info["success"] = true;
    info["windowId"] = target.windowId;
    return info;
}


// 运行时更新 DWM 背景策略
json WindowSetBackdropPolicy(const json& params) {
    if (!params.contains("backdropPolicy")) {
        return {{"success", false}, {"error", "backdropPolicy is required"}};
    }
    if (!params["backdropPolicy"].is_object()) {
        return {{"success", false}, {"error", "backdropPolicy must be an object"}};
    }

    auto target = WindowTargetResolver::ResolveForMutation(params);
    if (!target.Success()) return target.ErrorResponse();

    std::string error;
    if (!target.shell->PatchBackdropPolicy(params["backdropPolicy"], error)) {
        return {{"success", false}, {"error", error.empty() ? "failed to update backdrop policy" : error}};
    }

    json info = target.shell->GetBackdropPolicyInfo();
    info["success"] = true;
    info["windowId"] = target.windowId;
    return info;
}


// 取消关闭（前端调用）
json WindowCancelClose(const json& params) {
    HWND callerHwnd = GetCallerHwnd(params);
    
    // 查找对应的 PopupWindow
    auto ids = WindowManager::GetInstance().GetAllWindowIds();
    for (const auto& id : ids) {
        if (id == "main") continue;
        auto* popup = WindowManager::GetInstance().GetPopup(id);
        if (popup && popup->GetHwnd() == callerHwnd) {
            popup->CancelClose();
            return {{"success", true}};
        }
    }
    
    return {{"success", false}, {"error", ApiError::WINDOW_NOT_FOUND}};
}


// 确认关闭（前端调用）
json WindowConfirmClose(const json& params) {
    HWND callerHwnd = GetCallerHwnd(params);
    
    // 查找对应的 PopupWindow
    auto ids = WindowManager::GetInstance().GetAllWindowIds();
    for (const auto& id : ids) {
        if (id == "main") continue;
        auto* popup = WindowManager::GetInstance().GetPopup(id);
        if (popup && popup->GetHwnd() == callerHwnd) {
            popup->ConfirmClose();
            return {{"success", true}};
        }
    }
    
    return {{"success", false}, {"error", ApiError::WINDOW_NOT_FOUND}};
}


// 动态切换窗口标题栏（插件端自动判断窗口类型，开发者无需关心）
json WindowSetFrameless(const json& params) {
    bool frameless = params.value("frameless", true);
    auto target = WindowTargetResolver::ResolveForMutation(params);
    if (!target.Success()) {
        if (IsPanelMode()) {
            return {{"success", false}, {"error", "not supported in panel mode"}};
        }
        return target.ErrorResponse();
    }
    
    target.shell->PatchFrameless(frameless);
    return {{"success", true}, {"frameless", target.shell->IsFrameless()}};
}


// 设置弹出窗口鼠标穿透
json WindowSetClickThrough(const json& params) {
    bool enabled = params.value("enabled", true);
    std::string windowId = params.value("windowId", "");
    if (windowId.empty()) {
        windowId = GetCallerWindowId(params);
    }
    
    auto& wm = WindowManager::GetInstance();
    auto* popup = wm.GetPopup(windowId);
    if (!popup) {
        return {{"success", false}, {"error", ApiError::WINDOW_NOT_FOUND}};
    }
    
    popup->SetClickThrough(enabled);
    return {{"success", true}, {"clickThrough", popup->IsClickThrough()}};
}


// 查询弹出窗口鼠标穿透状态
json WindowIsClickThrough(const json& params) {
    std::string windowId = params.value("windowId", "");
    if (windowId.empty()) {
        windowId = GetCallerWindowId(params);
    }
    
    auto& wm = WindowManager::GetInstance();
    auto* popup = wm.GetPopup(windowId);
    if (!popup) {
        return {{"success", false}, {"error", ApiError::WINDOW_NOT_FOUND}};
    }
    
    return {{"success", true}, {"clickThrough", popup->IsClickThrough()}};
}


// 设置弹出窗口 click-through 交互热区
json WindowSetClickThroughExcludeRegions(const json& params) {
    std::string windowId = params.value("windowId", "");
    if (windowId.empty()) {
        windowId = GetCallerWindowId(params);
    }

    auto& wm = WindowManager::GetInstance();
    auto* popup = wm.GetPopup(windowId);
    if (!popup) {
        return {{"success", false}, {"error", ApiError::WINDOW_NOT_FOUND}};
    }

    // 获取 DPI 缩放比例（CSS 像素 → 物理像素）
    double dpiScale = 1.0;
    HWND hwnd = popup->GetHwnd();
    if (hwnd) {
        int dpi = GetDpiForWindow(hwnd);
        dpiScale = dpi / 96.0;
    }

    const auto& regionsArr = params.contains("regions") && params["regions"].is_array()
        ? params["regions"] : json::array();

    constexpr size_t MAX_REGIONS = 32;
    bool truncated = regionsArr.size() > MAX_REGIONS;

    std::vector<RECT> physicalRects;
    physicalRects.reserve(std::min(regionsArr.size(), MAX_REGIONS));

    size_t count = 0;
    for (const auto& r : regionsArr) {
        if (count >= MAX_REGIONS) break;
        int w = static_cast<int>(r.value("width", 0) * dpiScale);
        int h = static_cast<int>(r.value("height", 0) * dpiScale);
        if (w <= 0 || h <= 0) continue;

        RECT rc;
        rc.left   = static_cast<int>(r.value("x", 0) * dpiScale);
        rc.top    = static_cast<int>(r.value("y", 0) * dpiScale);
        rc.right  = rc.left + w;
        rc.bottom = rc.top + h;
        physicalRects.push_back(rc);
        ++count;
    }

    popup->SetClickThroughExcludeRegions(physicalRects);

    json result = {
        {"success", true},
        {"windowId", windowId},
        {"count", physicalRects.size()},
        {"dpiScale", dpiScale}
    };
    if (truncated) {
        result["warning"] = "regions truncated to 32";
    }
    return result;
}


// 清除弹出窗口 click-through 交互热区
json WindowClearClickThroughExcludeRegions(const json& params) {
    std::string windowId = params.value("windowId", "");
    if (windowId.empty()) {
        windowId = GetCallerWindowId(params);
    }

    auto& wm = WindowManager::GetInstance();
    auto* popup = wm.GetPopup(windowId);
    if (!popup) {
        return {{"success", false}, {"error", ApiError::WINDOW_NOT_FOUND}};
    }

    popup->ClearClickThroughExcludeRegions();
    return {{"success", true}, {"windowId", windowId}};
}


// ============================================
// 自定义跨窗口消息 API
// ============================================

// 定向发送消息到指定窗口
json WindowSendMessage(const json& params) {
    std::string targetId = params.value("targetWindowId", "");
    if (targetId.empty()) {
        return {{"success", false}, {"error", ApiError::WINDOW_ID_REQUIRED}};
    }
    
    if (!params.contains("message")) {
        return {{"success", false}, {"error", ApiError::REQUIRED_PARAM_MISSING}};
    }
    
    std::string sourceId = GetCallerWindowId(params);
    const auto& message = params["message"];
    
    bool ok = WindowManager::GetInstance().SendWindowMessage(sourceId, targetId, message);
    if (!ok) {
        return {{"success", false}, {"error", ApiError::TARGET_WINDOW_NOT_FOUND}};
    }
    return {{"success", true}};
}


// 广播消息到除发送者外的所有窗口
json WindowBroadcast(const json& params) {
    if (!params.contains("message")) {
        return {{"success", false}, {"error", ApiError::REQUIRED_PARAM_MISSING}};
    }
    
    std::string sourceId = GetCallerWindowId(params);
    const auto& message = params["message"];
    
    WindowManager::GetInstance().BroadcastMessage(sourceId, message);
    return {{"success", true}};
}


// ============================================
// 面板配置 API
// ============================================

// window.getMode - 获取当前面板模式
json WindowGetMode(const json& params) {
    HWND callerHwnd = GetCallerHwnd(params);
    auto& ctx = WebViewContext::GetInstance();
    
    // 遍历查找匹配的实例
    for (auto instHwnd : ctx.GetAllInstances()) {
        if (instHwnd != callerHwnd &&
            ::GetAncestor(instHwnd, GA_ROOT) != callerHwnd)
            continue;
        auto* panel = ctx.GetPanelByHwnd(instHwnd);
        if (!panel)
            continue;

        std::string mode;
        switch (panel->GetMode()) {
            case WebViewPanelMode::Standalone: mode = "standalone"; break;
            case WebViewPanelMode::DuiPanel:   mode = "dui"; break;
            case WebViewPanelMode::CuiPanel:   mode = "cui"; break;
            default: mode = "unknown"; break;
        }
        std::string windowId = ctx.GetWindowIdByHwnd(instHwnd);
        return {
            {"mode", mode},
            {"panelMode", panel->IsPanelMode()},
            {"windowId", windowId.empty() ? "panel" : windowId}
        };
    }
    
    // 回退：无面板实例，可能是独立窗口
    return {
        {"mode", IsPanelMode() ? "panel" : "standalone"},
        {"panelMode", IsPanelMode()},
        {"windowId", "main"}
    };
}


// panel.getConfig - 获取面板配置
json PanelGetConfig(const json& params) {
    HWND callerHwnd = GetCallerHwnd(params);
    auto& ctx = WebViewContext::GetInstance();
    
    // 查找面板实例
    for (auto instHwnd : ctx.GetAllInstances()) {
        if (instHwnd != callerHwnd &&
            ::GetAncestor(instHwnd, GA_ROOT) != callerHwnd)
            continue;
        auto* panel = ctx.GetPanelByHwnd(instHwnd);
        if (!panel)
            continue;

        const PanelConfig& cfg = panel->GetConfig();
        return {
            {"success", true},
            {"config", {
                {"panelName", cfg.panelName},
                {"templateName", cfg.templateName},
                {"edgeStyle", cfg.edgeStyle},
                {"urlOverride", cfg.urlOverride},
                {"transparentBackground", cfg.transparentBackground},
                {"grabFocus", cfg.grabFocus},
                {"enableDragDrop", cfg.enableDragDrop},
                {"enableDevTools", cfg.enableDevTools}
            }}
        };
    }
    
    return {{"success", false}, {"error", "Panel not found"}};
}


// panel.setConfig - 设置面板配置（安全字段白名单）
json PanelSetConfig(const json& params) {
    HWND callerHwnd = GetCallerHwnd(params);
    auto& ctx = WebViewContext::GetInstance();
    
    // 查找面板实例
    for (auto instHwnd : ctx.GetAllInstances()) {
        if (instHwnd != callerHwnd &&
            ::GetAncestor(instHwnd, GA_ROOT) != callerHwnd)
            continue;
        auto* panel = ctx.GetPanelByHwnd(instHwnd);
        if (!panel)
            continue;

        PanelConfig oldConfig = panel->GetConfig();
        PanelConfig newConfig = oldConfig;
        
        // 安全白名单：只允许修改这些字段
        if (params.contains("panelName")) {
            newConfig.panelName = params["panelName"].get<std::string>();
        }
        if (params.contains("transparentBackground")) {
            newConfig.transparentBackground = params["transparentBackground"].get<bool>();
        }
        if (params.contains("grabFocus")) {
            newConfig.grabFocus = params["grabFocus"].get<bool>();
        }
        if (params.contains("enableDragDrop")) {
            newConfig.enableDragDrop = params["enableDragDrop"].get<bool>();
        }
        
        // 禁止从 JS 修改的字段：enableDevTools, urlOverride, templateName
        // 这些字段只能通过配置对话框修改
        
        if (newConfig.HasChanged(oldConfig)) {
            panel->ApplyConfig(oldConfig, newConfig);
            return {{"success", true}, {"changed", true}};
        }
        
        return {{"success", true}, {"changed", false}};
    }
    
    return {{"success", false}, {"error", "Panel not found"}};
}

} // namespace

void RegisterWindowApi() {
    auto& bridge = BridgeCore::GetInstance();

    bridge.RegisterApi("window.minimize", WindowMinimize);
    bridge.RegisterApi("window.maximize", WindowMaximize);
    bridge.RegisterApi("window.restore", WindowRestore);
    bridge.RegisterApi("window.close", WindowClose);
    bridge.RegisterApi("window.toggleMaximize", WindowToggleMaximize);
    bridge.RegisterApi("window.isMaximized", WindowIsMaximized);
    bridge.RegisterApi("window.isMinimized", WindowIsMinimized);
    bridge.RegisterApi("window.isFullscreen", WindowIsFullscreen);
    bridge.RegisterApi("window.getState", WindowGetState);
    bridge.RegisterApi("window.startDrag", WindowStartDrag);
    bridge.RegisterApi("window.startResize", WindowStartResize);
    bridge.RegisterApi("window.setAlwaysOnTop", WindowSetAlwaysOnTop);
    bridge.RegisterApi("window.isAlwaysOnTop", WindowIsAlwaysOnTop);
    bridge.RegisterApi("window.toggleAlwaysOnTop", WindowToggleAlwaysOnTop);
    bridge.RegisterApi("window.getBounds", WindowGetBounds);
    bridge.RegisterApi("window.setBounds", WindowSetBounds);
    bridge.RegisterApi("window.center", WindowCenter);
    bridge.RegisterApi("window.setMinSize", WindowSetMinSize);
    bridge.RegisterApi("window.getMinSize", WindowGetMinSize);
    bridge.RegisterApi("window.setMaxSize", WindowSetMaxSize);
    bridge.RegisterApi("window.getMaxSize", WindowGetMaxSize);
    bridge.RegisterApi("window.setResizable", WindowSetResizable);
    bridge.RegisterApi("window.isResizable", WindowIsResizable);
    bridge.RegisterApi("window.setFullscreen", WindowSetFullscreen);
    bridge.RegisterApi("window.toggleFullscreen", WindowToggleFullscreen);
    bridge.RegisterApi("window.focus", WindowFocus);
    bridge.RegisterApi("window.blur", WindowBlur);
    bridge.RegisterApi("window.setTitle", WindowSetTitle);
    bridge.RegisterApi("window.getTitle", WindowGetTitle);
    bridge.RegisterApi("window.flash", WindowFlash);
    bridge.RegisterApi("window.showSystemMenu", WindowShowSystemMenu);
    bridge.RegisterApi("window.getTitlebarHeight", WindowGetTitlebarHeight);
    bridge.RegisterApi("window.setTitlebarHeight", WindowSetTitlebarHeight);
    bridge.RegisterApi("window.getCaptionButtonsWidth", WindowGetCaptionButtonsWidth);
    bridge.RegisterApi("window.setDragRegions", WindowSetDragRegions);
    bridge.RegisterApi("window.clearDragRegions", WindowClearDragRegions);
    bridge.RegisterApi("window.setNoDragRegions", WindowSetNoDragRegions);
    bridge.RegisterApi("window.clearNoDragRegions", WindowClearNoDragRegions);
    bridge.RegisterApi("window.getTitlebarInfo", WindowGetTitlebarInfo);
    bridge.RegisterApi("ui.showContextMenu", UiShowContextMenu);
    bridge.RegisterApi("system.getTheme", SystemGetTheme);
    bridge.RegisterApi("system.getDPI", SystemGetDPI);
    bridge.RegisterApi("window.setCornerPreference", WindowSetCornerPreference);
    bridge.RegisterApi("window.getCornerPreference", WindowGetCornerPreference);
    bridge.RegisterApi("system.getLocale", SystemGetLocale);
    bridge.RegisterApi("window.setPosition", WindowSetPosition);
    bridge.RegisterApi("window.setSize", WindowSetSize);
    bridge.RegisterApi("window.getDpiScale", WindowGetDpiScale);
    bridge.RegisterApi("window.flashTaskbar", WindowFlashTaskbar);
    bridge.RegisterApi("window.enterFullscreen", WindowEnterFullscreen);
    bridge.RegisterApi("window.exitFullscreen", WindowExitFullscreen);
    bridge.RegisterApi("window.setMica", WindowSetMica);
    bridge.RegisterApi("window.setMicaEffect", WindowSetMicaEffect);
    bridge.RegisterApi("window.setBlur", WindowSetBlur);
    bridge.RegisterApi("window.setAcrylic", WindowSetAcrylic);
    bridge.RegisterApi("window.setDarkMode", WindowSetDarkMode);
    bridge.RegisterApi("window.setBackgroundTransparency", WindowSetBackgroundTransparency);
    bridge.RegisterApi("window.refreshWebView", WindowRefreshWebView);
    bridge.RegisterApi("window.reload", WindowReload);
    bridge.RegisterApi("window.getDevServerConfig", WindowGetDevServerConfig);
    bridge.RegisterApi("window.setDevServerConfig", WindowSetDevServerConfig);
    bridge.RegisterApi("window.hasSavedBounds", WindowHasSavedBounds);
    bridge.RegisterApi("window.setZoom", WindowSetZoom);
    bridge.RegisterApi("window.getZoom", WindowGetZoom);
    bridge.RegisterApi("window.resetZoom", WindowResetZoom);
    bridge.RegisterApi("window.setZoomForDpi", WindowSetZoomForDpi);
    bridge.RegisterApi("window.createPopup", WindowCreatePopup);
    bridge.RegisterApi("window.closePopup", WindowClosePopup);
    bridge.RegisterApi("window.closeAllPopups", WindowCloseAllPopups);
    bridge.RegisterApi("window.getAllWindows", WindowGetAllWindows);
    bridge.RegisterApi("window.getCurrentWindowId", WindowGetCurrentWindowId);
    bridge.RegisterApi("window.getPopupBehavior", WindowGetPopupBehavior);
    bridge.RegisterApi("window.setPopupBehavior", WindowSetPopupBehavior);
    bridge.RegisterApi("window.getBackdropPolicy", WindowGetBackdropPolicy);
    bridge.RegisterApi("window.setBackdropPolicy", WindowSetBackdropPolicy);
    bridge.RegisterApi("window.cancelClose", WindowCancelClose);
    bridge.RegisterApi("window.confirmClose", WindowConfirmClose);
    bridge.RegisterApi("window.setFrameless", WindowSetFrameless);
    bridge.RegisterApi("window.setClickThrough", WindowSetClickThrough);
    bridge.RegisterApi("window.isClickThrough", WindowIsClickThrough);
    bridge.RegisterApi("window.setClickThroughExcludeRegions", WindowSetClickThroughExcludeRegions);
    bridge.RegisterApi("window.clearClickThroughExcludeRegions", WindowClearClickThroughExcludeRegions);
    bridge.RegisterApi("window.sendMessage", WindowSendMessage);
    bridge.RegisterApi("window.broadcast", WindowBroadcast);
    bridge.RegisterApi("window.getMode", WindowGetMode);
    bridge.RegisterApi("panel.getConfig", PanelGetConfig);
    bridge.RegisterApi("panel.setConfig", PanelSetConfig);
}
