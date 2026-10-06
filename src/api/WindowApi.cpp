#include "pch.h"
#include "api/WindowApi.h"
#include "api/BridgeCore.h"
#include "api/ApiConstants.h"
#include "api/CallerContext.h"
#include "api/WindowCallerIdentity.h"
#include "api/TypedApi.h"
#include "api/generated/PanelSchema.h"
#include "api/generated/SystemSchema.h"
#include "api/generated/UiSchema.h"
#include "api/generated/WindowSchema.h"
#include "ui/UserInterface.h"
#include "core/WebViewContext.h"
#include "window/MainWindow.h"
#include "window/PopupWindow.h"
#include "window/WindowManager.h"
#include "window/WindowTargetResolver.h"
#include "window/WindowShellBase.h"
#include "window/WindowGeometryMath.h"
#include "webview/WebViewHost.h"
#include "settings/SecurityConfig.h"
#include "settings/WindowStateConfig.h"
#include "core/WebViewPanel.h"  // panel.getConfig/setConfig API

#include <algorithm>
#include <limits>
#include <optional>

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
// ============================================

// 面板的调用方：面板 HWND 可能是子窗口，取顶级窗口；没有调用方时回退到主窗口。
static HWND CallerTopLevelHwnd(const CallerContext& caller) {
    if (caller.callerHwnd && IsWindow(caller.callerHwnd)) {
        HWND topLevel = ::GetAncestor(caller.callerHwnd, GA_ROOT);
        return topLevel ? topLevel : caller.callerHwnd;
    }
    return GetMainHwnd();
}

// ============================================
// Helper: 通过调用者 HWND 获取窗口 ID
// ============================================

static window_caller::Identity GetCallerWindowIdentity(const CallerContext& caller) {
    return window_caller::Resolve(caller.callerHwnd, WindowManager::GetInstance(),
        WebViewContext::GetInstance(), IsPanelMode());
}

static std::string GetCallerWindowId(const CallerContext& caller) {
    return GetCallerWindowIdentity(caller).CurrentWindowId();
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

namespace wn = api::window;

// 像素参数声明为 number，按旧读法（value<int>）直接截掉小数；超出 int 的值钳到边界，
// 免得 double 转 int 越界。
int PixelArg(double value) {
    return static_cast<int>(std::clamp(value, static_cast<double>(std::numeric_limits<int>::min()),
                                       static_cast<double>(std::numeric_limits<int>::max())));
}

// 调用方没有可操作的窗口（既没有有效的调用方句柄，也没有主窗口）。
api::Failure WindowNotFound(nlohmann::json::object_t extra = {}) {
    return api::Fail(ApiError::WINDOW_NOT_FOUND, ApiErrorCode::NOT_FOUND, std::move(extra));
}

// 窗口目标解析失败：面板调用方按面板模式失败，其余是找不到窗口。
api::Failure TargetFailure(const WindowTargetResult& target, std::string_view method) {
    if (target.panelCaller) return api::PanelModeUnsupported(method);
    return api::Fail(target.error, ApiErrorCode::NOT_FOUND);
}

// 调用方窗口自己没有 WebView（面板的顶级窗口是 foobar2000 主框架，也没有）。
api::Failure WebViewNotAvailable() {
    return api::Fail("WebView not available", ApiErrorCode::NOT_FOUND);
}

// 设置已写下、但这次没生效的失败：迁移前这些失败就带着回包字段，照旧放进失败信封。
template <class R>
api::Failure FailWithFields(std::string error, const R& fields) {
    return api::Fail(std::move(error), ApiErrorCode::OPERATION_FAILED,
                     ToJson(fields).template get<nlohmann::json::object_t>());
}

// DWM 类设置先存后画：窗口画不出来（例如启动时仍隐藏）时设置已保存，只是这次没生效。
constexpr const char* kBackdropNotApplied = "The backdrop setting was stored but not applied";
constexpr const char* kZoomNotApplied = "WebView2 did not accept the zoom factor";

// 窗口层以 json 交出策略与窗口快照（GetBackdropPolicyInfo、GetAllWindowsInfo 按固定键构造），
// 这里换成声明的结构体；缺键取默认值，不再校验。popup 行为的回包不经 json，由
// PopupWindow::FillPopupBehavior 直接填。
std::map<std::string, json> ToOverrides(const json& overrides) {
    std::map<std::string, json> out;
    if (!overrides.is_object()) return out;
    for (auto it = overrides.begin(); it != overrides.end(); ++it) {
        out[it.key()] = it.value();
    }
    return out;
}

json OverridesToJson(const std::map<std::string, json>& overrides) {
    json out = json::object();
    for (const auto& [key, value] : overrides) {
        out[key] = value;
    }
    return out;
}

wn::WindowBackdropPolicyState ToBackdropState(const json& resolved) {
    wn::WindowBackdropPolicyState state;
    if (!resolved.is_object()) return state;
    state.activeEffect = resolved.value("activeEffect", std::string());
    state.inactiveEffect = resolved.value("inactiveEffect", std::string());
    state.darkMode = resolved.value("darkMode", false);
    state.reapplyOnActivate = resolved.value("reapplyOnActivate", false);
    return state;
}

wn::WindowPopupBehaviorState ToBehaviorState(const json& resolved) {
    wn::WindowPopupBehaviorState state;
    if (!resolved.is_object()) return state;
    state.showInTaskbar = resolved.value("showInTaskbar", false);
    state.showInAltTab = resolved.value("showInAltTab", false);
    state.keepVisibleOnShowDesktop = resolved.value("keepVisibleOnShowDesktop", false);
    state.allowMinimize = resolved.value("allowMinimize", false);
    state.owner = resolved.value("owner", std::string());
    state.noActivate = resolved.value("noActivate", false);
    return state;
}

wn::WindowInfo ToWindowInfo(const json& item) {
    wn::WindowInfo info;
    info.windowId = item.value("windowId", std::string());
    info.isMain = item.value("isMain", false);
    info.title = item.value("title", std::string());
    if (!info.isMain) {
        info.url = item.value("url", std::string());
        info.profile = item.value("profile", std::string());
        info.behavior = ToOverrides(item.value("behavior", json::object()));
        info.resolvedBehavior = ToBehaviorState(item.value("resolvedBehavior", json::object()));
    }
    info.backdropPolicy = ToOverrides(item.value("backdropPolicy", json::object()));
    info.resolvedBackdropPolicy = ToBackdropState(item.value("resolvedBackdropPolicy", json::object()));

    const json caps = item.value("capabilities", json::object());
    info.capabilities.supportsBackdropPolicy = caps.value("supportsBackdropPolicy", false);
    info.capabilities.supportsFrameless = caps.value("supportsFrameless", false);
    info.capabilities.supportsCornerPreference = caps.value("supportsCornerPreference", false);
    info.capabilities.supportsPopupBehavior = caps.value("supportsPopupBehavior", false);
    info.capabilities.supportsMicaAlt = caps.value("supportsMicaAlt", false);
    info.capabilities.supportsFullscreen = caps.value("supportsFullscreen", false);
    // 以下三项只有 popup 报
    if (caps.contains("supportsOwnerPolicy")) {
        info.capabilities.supportsOwnerPolicy = caps.value("supportsOwnerPolicy", false);
    }
    if (caps.contains("supportsNoActivate")) {
        info.capabilities.supportsNoActivate = caps.value("supportsNoActivate", false);
    }
    if (caps.contains("supportsBeforeClose")) {
        info.capabilities.supportsBeforeClose = caps.value("supportsBeforeClose", false);
    }

    const json bounds = item.value("bounds", json::object());
    info.bounds.x = bounds.value("x", std::int64_t{0});
    info.bounds.y = bounds.value("y", std::int64_t{0});
    info.bounds.width = bounds.value("width", std::int64_t{0});
    info.bounds.height = bounds.value("height", std::int64_t{0});

    info.shell = item.value("shell", json::object());
    return info;
}

// getPopupBehavior 与 setPopupBehavior 回包同形（各一份结构体）。
template <class R>
R PopupBehaviorResultOf(const PopupWindow& popup, const std::string& windowId) {
    R result;
    result.windowId = windowId;
    popup.FillPopupBehavior(result);
    return result;
}

// getBackdropPolicy 与 setBackdropPolicy 回包同形（各一份结构体）。
template <class R>
R BackdropPolicyResultOf(const WindowShellBase& shell, const std::string& windowId) {
    const json info = shell.GetBackdropPolicyInfo();
    R result;
    result.windowId = windowId;
    result.backdropPolicy = ToOverrides(info.value("backdropPolicy", json::object()));
    result.resolvedBackdropPolicy = ToBackdropState(info.value("resolvedBackdropPolicy", json::object()));
    return result;
}

// popup 行为的目标：显式 windowId 为 main 时不支持，其余按 id 找 popup；省略时取调用方 popup。
// targetId 回填实际的 popup id。
std::variant<PopupWindow*, api::Failure> FindBehaviorPopup(const std::optional<std::string>& windowId,
                                                          const CallerContext& caller, std::string_view method,
                                                          std::string& targetId) {
    targetId = windowId.value_or("");
    PopupWindow* popup = nullptr;
    if (!targetId.empty()) {
        if (targetId == "main") {
            return api::Fail(std::string(method) + " does not support main window", ApiErrorCode::NOT_SUPPORTED);
        }
        popup = WindowManager::GetInstance().GetPopup(targetId);
    } else {
        popup = FindPopupByCallerHwnd(CallerTopLevelHwnd(caller), &targetId);
    }
    if (!popup) return WindowNotFound();
    return popup;
}

// 鼠标穿透的目标：显式 windowId，省略时取调用方窗口；只有 popup 能穿透，其余返回 nullptr。
PopupWindow* FindClickThroughPopup(const std::optional<std::string>& windowId, const CallerContext& caller,
                                   std::string& targetId) {
    targetId = windowId.value_or("");
    if (targetId.empty()) {
        targetId = GetCallerWindowId(caller);
    }
    return WindowManager::GetInstance().GetPopup(targetId);
}


// ========== Basic Window Controls ==========
// 使用 WM_SYSCOMMAND 而不是 ShowWindow，以触发 Windows 原生动画效果

api::Result<void> WindowMinimize(const wn::MinimizeParams&, const CallerContext& caller) {
    if (IsPanelMode()) return api::PanelModeUnsupported("window.minimize");
    HWND hwnd = CallerTopLevelHwnd(caller);
    if (!hwnd) return WindowNotFound();

    // 使用 WM_SYSCOMMAND 触发系统动画
    PostMessage(hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
    return api::Ok();
}


api::Result<void> WindowMaximize(const wn::MaximizeParams&, const CallerContext& caller) {
    if (IsPanelMode()) return api::PanelModeUnsupported("window.maximize");
    HWND hwnd = CallerTopLevelHwnd(caller);
    if (!hwnd) return WindowNotFound();

    // 参考 Electron / Tauri Windows 行为：全屏时 maximize 应先退出全屏。
    // ExitFullscreenMode 会按 saved info 决定是否自动恢复 maximize；
    // 之后下面的 PostMessage SC_MAXIMIZE 在已 maximize 时是 no-op，未 maximize 时补 maximize。
    ExitFullscreenIfActive(hwnd);

    // 使用 WM_SYSCOMMAND 触发系统动画
    PostMessage(hwnd, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
    return api::Ok();
}


api::Result<void> WindowRestore(const wn::RestoreParams&, const CallerContext& caller) {
    if (IsPanelMode()) return api::PanelModeUnsupported("window.restore");
    HWND hwnd = CallerTopLevelHwnd(caller);
    if (!hwnd) return WindowNotFound();

    // 参考 Electron / Tauri：全屏状态下 restore 语义为“回到正常状态”，
    // ExitFullscreenMode 会恢复全屏前的 saved info（normal 或 maximized）。
    if (ExitFullscreenIfActive(hwnd)) {
        return api::Ok();
    }

    // 使用 WM_SYSCOMMAND 触发系统动画
    PostMessage(hwnd, WM_SYSCOMMAND, SC_RESTORE, 0);
    return api::Ok();
}


api::Result<void> WindowClose(const wn::CloseParams&, const CallerContext& caller) {
    if (IsPanelMode()) return api::PanelModeUnsupported("window.close");
    HWND hwnd = CallerTopLevelHwnd(caller);
    if (hwnd) {
        // 使用 WM_SYSCOMMAND 触发系统关闭动画
        PostMessage(hwnd, WM_SYSCOMMAND, SC_CLOSE, 0);
    }
    return api::Ok();
}


api::Result<wn::ToggleMaximizeResult> WindowToggleMaximize(const wn::ToggleMaximizeParams&,
                                                           const CallerContext& caller) {
    if (IsPanelMode()) return api::PanelModeUnsupported("window.toggleMaximize");
    HWND hwnd = CallerTopLevelHwnd(caller);
    if (!hwnd) return WindowNotFound({{"maximized", false}});

    wn::ToggleMaximizeResult result;
    // 参考 Electron / Tauri：全屏状态下 toggle 优先退出全屏，
    // 不再进一步在 maximize 与 normal 之间切换。
    if (ExitFullscreenIfActive(hwnd)) {
        result.maximized = IsZoomed(hwnd) != FALSE;
        return result;
    }

    bool wasMaximized = IsZoomed(hwnd) != FALSE;
    // 使用 WM_SYSCOMMAND 触发系统动画
    PostMessage(hwnd, WM_SYSCOMMAND, wasMaximized ? SC_RESTORE : SC_MAXIMIZE, 0);

    result.maximized = !wasMaximized;
    return result;
}


// ========== State Query ==========

api::Result<wn::IsMaximizedResult> WindowIsMaximized(const wn::IsMaximizedParams&, const CallerContext& caller) {
    HWND hwnd = CallerTopLevelHwnd(caller);
    wn::IsMaximizedResult result;
    result.maximized = hwnd && IsZoomed(hwnd) != FALSE;
    result.isMaximized = result.maximized;
    return result;
}


api::Result<wn::IsMinimizedResult> WindowIsMinimized(const wn::IsMinimizedParams&, const CallerContext& caller) {
    HWND hwnd = CallerTopLevelHwnd(caller);
    wn::IsMinimizedResult result;
    result.minimized = hwnd && IsIconic(hwnd) != FALSE;
    return result;
}


api::Result<wn::IsFullscreenResult> WindowIsFullscreen(const wn::IsFullscreenParams& p, const CallerContext& caller) {
    auto target = WindowTargetResolver::ResolveForObservation(p.windowId, caller.callerHwnd);
    // 解析失败时返回错误信封，而不是 fullscreen:false。
    //
    // 旧实现把「解析失败」与「窗口确实不在全屏」压成同一个 false，调用方
    // 无从分辨——正是 Q7-1 要消灭的静默错值形态。取消 observation 的主窗口
    // 回退后，面板调用方与已销毁的 caller 都会走到这里，故必须显式失败。
    if (!target.Success()) return TargetFailure(target, "window.isFullscreen");

    wn::IsFullscreenResult result;
    result.fullscreen = target.shell->IsFullscreen();
    result.isFullscreen = result.fullscreen;
    result.windowId = target.windowId;
    return result;
}


api::Result<wn::GetStateResult> WindowGetState(const wn::GetStateParams&, const CallerContext& caller) {
    wn::GetStateResult result;  // 没有窗口时标志全为 false、矩形全为 0
    HWND hwnd = CallerTopLevelHwnd(caller);
    if (!hwnd) return result;

    DWORD exStyle = GetWindowLongW(hwnd, GWL_EXSTYLE);
    bool alwaysOnTop = (exStyle & WS_EX_TOPMOST) != 0;

    bool isMax = IsZoomed(hwnd) != FALSE;
    bool isMin = IsIconic(hwnd) != FALSE;
    auto shellTarget = WindowTargetResolver::ResolveByCallerHwnd(hwnd);
    bool isFull = shellTarget.Success() ? shellTarget.shell->IsFullscreen() : false;
    bool isFocus = GetForegroundWindow() == hwnd;

    RECT rc{};
    GetWindowRect(hwnd, &rc);

    result.maximized = isMax;
    result.minimized = isMin;
    result.fullscreen = isFull;
    result.alwaysOnTop = alwaysOnTop;
    result.focused = isFocus;
    result.isMaximized = isMax;
    result.isMinimized = isMin;
    result.isFullscreen = isFull;
    result.isAlwaysOnTop = alwaysOnTop;
    result.isFocused = isFocus;
    result.width = rc.right - rc.left;
    result.height = rc.bottom - rc.top;
    result.x = rc.left;
    result.y = rc.top;
    return result;
}


// ========== Drag ==========

api::Result<void> WindowStartDrag(const wn::StartDragParams&, const CallerContext& caller) {
    if (IsPanelMode()) return api::PanelModeUnsupported("window.startDrag");
    HWND hwnd = CallerTopLevelHwnd(caller);
    if (!hwnd) return WindowNotFound();

    // Release mouse capture and send drag message
    ReleaseCapture();
    SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
    return api::Ok();
}


api::Result<void> WindowStartResize(const wn::StartResizeParams& p, const CallerContext& caller) {
    if (IsPanelMode()) return api::PanelModeUnsupported("window.startResize");
    HWND hwnd = CallerTopLevelHwnd(caller);
    if (!hwnd) return WindowNotFound();

    // 认不出的取值按右下角处理。
    const std::string& edge = p.edge;
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
    return api::Ok();
}


// ========== Always On Top ==========

api::Result<void> WindowSetAlwaysOnTop(const wn::SetAlwaysOnTopParams& p, const CallerContext& caller) {
    if (IsPanelMode()) return api::PanelModeUnsupported("window.setAlwaysOnTop");
    HWND hwnd = CallerTopLevelHwnd(caller);
    if (!hwnd) return WindowNotFound();

    const bool enabled = p.enabled;

    // 主窗口期望置顶状态登记：用户显式 alwaysOnTop 意图是该状态的唯一改写来源，
    // 供 MainWindow 的 WM_WINDOWPOSCHANGED 守护矫正参照（popup 不参与守护）。
    // 必须先于 SetWindowPos：SetWindowPos 会同步派发 WM_WINDOWPOSCHANGED，
    // 守护在该消息里按 expectedTopmost_ 矫正实际 z-order。若期望值仍是旧值
    // （冷启动首次为 false），守护会立即把刚设上的置顶撤销，置顶随即丢失。
    {
        auto* mainWin = WindowManager::GetInstance().GetMainWindow();
        if (mainWin && mainWin->GetShellHwnd() == hwnd) {
            mainWin->SetExpectedTopmost(enabled);
        }
    }

    SetWindowPos(
        hwnd,
        enabled ? HWND_TOPMOST : HWND_NOTOPMOST,
        0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE
    );
    return api::Ok();
}


api::Result<wn::IsAlwaysOnTopResult> WindowIsAlwaysOnTop(const wn::IsAlwaysOnTopParams&,
                                                         const CallerContext& caller) {
    wn::IsAlwaysOnTopResult result;  // 没有窗口时为 false
    HWND hwnd = CallerTopLevelHwnd(caller);
    if (hwnd) {
        DWORD exStyle = GetWindowLongW(hwnd, GWL_EXSTYLE);
        result.enabled = (exStyle & WS_EX_TOPMOST) != 0;
        result.isAlwaysOnTop = result.enabled;
    }
    return result;
}


api::Result<wn::ToggleAlwaysOnTopResult> WindowToggleAlwaysOnTop(const wn::ToggleAlwaysOnTopParams&,
                                                                 const CallerContext& caller) {
    if (IsPanelMode()) return api::PanelModeUnsupported("window.toggleAlwaysOnTop");
    HWND hwnd = CallerTopLevelHwnd(caller);
    if (!hwnd) return WindowNotFound({{"enabled", false}});

    DWORD exStyle = GetWindowLongW(hwnd, GWL_EXSTYLE);
    bool wasOnTop = (exStyle & WS_EX_TOPMOST) != 0;

    // 主窗口期望置顶状态登记（同 WindowSetAlwaysOnTop，守护矫正参照）
    // 同样必须先于 SetWindowPos，理由见 WindowSetAlwaysOnTop 的说明。
    {
        auto* mainWin = WindowManager::GetInstance().GetMainWindow();
        if (mainWin && mainWin->GetShellHwnd() == hwnd) {
            mainWin->SetExpectedTopmost(!wasOnTop);
        }
    }

    SetWindowPos(
        hwnd,
        wasOnTop ? HWND_NOTOPMOST : HWND_TOPMOST,
        0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE
    );

    wn::ToggleAlwaysOnTopResult result;
    result.enabled = !wasOnTop;
    return result;
}


// ========== Bounds ==========

api::Result<wn::GetBoundsResult> WindowGetBounds(const wn::GetBoundsParams&, const CallerContext& caller) {
    wn::GetBoundsResult result;  // 没有窗口时矩形全为 0
    HWND hwnd = CallerTopLevelHwnd(caller);
    if (!hwnd) return result;

    RECT rc{};
    GetWindowRect(hwnd, &rc);
    result.x = rc.left;
    result.y = rc.top;
    result.width = rc.right - rc.left;
    result.height = rc.bottom - rc.top;
    return result;
}


api::Result<void> WindowSetBounds(const wn::SetBoundsParams& p, const CallerContext& caller) {
    if (IsPanelMode()) return api::PanelModeUnsupported("window.setBounds");
    HWND hwnd = CallerTopLevelHwnd(caller);
    if (!hwnd) return WindowNotFound();

    RECT currentRect{};
    GetWindowRect(hwnd, &currentRect);

    // 省略的键保持当前值
    int x = p.x ? PixelArg(*p.x) : currentRect.left;
    int y = p.y ? PixelArg(*p.y) : currentRect.top;
    int width = p.width ? PixelArg(*p.width) : (currentRect.right - currentRect.left);
    int height = p.height ? PixelArg(*p.height) : (currentRect.bottom - currentRect.top);

    UINT flags = SWP_NOZORDER;
    if (!p.x && !p.y) flags |= SWP_NOMOVE;
    if (!p.width && !p.height) flags |= SWP_NOSIZE;

    SetWindowPos(hwnd, nullptr, x, y, width, height, flags);
    return api::Ok();
}


api::Result<void> WindowCenter(const wn::CenterParams&, const CallerContext& caller) {
    if (IsPanelMode()) return api::PanelModeUnsupported("window.center");
    HWND hwnd = CallerTopLevelHwnd(caller);
    if (!hwnd) return WindowNotFound();

    RECT windowRect{};
    GetWindowRect(hwnd, &windowRect);
    int width = windowRect.right - windowRect.left;
    int height = windowRect.bottom - windowRect.top;

    // Get monitor info
    MONITORINFO mi = { sizeof(MONITORINFO) };
    GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi);

    int x = mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - width) / 2;
    int y = mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - height) / 2;

    SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
    return api::Ok();
}


// ========== Size Constraints ==========

// 取目标 shell 当前 DPI。
//
// 尺寸约束的 wire 单位是**物理像素**（bridge 契约不随存储单位改动），而 shell
// 存储单位是 DIP，故 handler 层需要按目标窗口的 DPI 换算。
//
// 换算基准必须取**目标窗口**的 DPI，而不是 caller 的：多显示器混合缩放下
// 两者可能不同，用错会引入与 D2 同类的量纲错误。
static int GetShellDpi(WindowShellBase* shell) {
    HWND hwnd = shell ? shell->GetShellHwnd() : nullptr;
    if (!hwnd || !IsWindow(hwnd)) return window_geometry::kBaselineDpi;
    return GetDpiForWindow(hwnd);
}

api::Result<wn::SetMinSizeResult> WindowSetMinSize(const wn::SetMinSizeParams& p, const CallerContext& caller) {
    if (IsPanelMode()) return api::PanelModeUnsupported("window.setMinSize");
    auto target = WindowTargetResolver::ResolveForMutation(p.windowId, caller.callerHwnd);
    if (!target.Success()) return TargetFailure(target, "window.setMinSize");

    const int dpi = GetShellDpi(target.shell);
    target.shell->SetMinSizeDip(
        window_geometry::PhysicalToDip(PixelArg(p.width), dpi),
        window_geometry::PhysicalToDip(PixelArg(p.height), dpi));

    wn::SetMinSizeResult result;
    result.windowId = target.windowId;
    return result;
}


api::Result<wn::GetMinSizeResult> WindowGetMinSize(const wn::GetMinSizeParams& p, const CallerContext& caller) {
    auto target = WindowTargetResolver::ResolveForObservation(p.windowId, caller.callerHwnd);
    if (!target.Success()) return TargetFailure(target, "window.getMinSize");

    int widthDip = 0;
    int heightDip = 0;
    target.shell->GetMinSizeDip(widthDip, heightDip);

    // 返回**请求值**（换算回 wire 单位）而非生效值，保证 set 后 get 的往返
    // 语义稳定（Q9-b）。setSize 的响应并不带生效值或钳制信息。
    const int dpi = GetShellDpi(target.shell);
    wn::GetMinSizeResult result;
    result.width = window_geometry::DipToPhysical(widthDip, dpi);
    result.height = window_geometry::DipToPhysical(heightDip, dpi);
    result.windowId = target.windowId;
    return result;
}


api::Result<wn::SetMaxSizeResult> WindowSetMaxSize(const wn::SetMaxSizeParams& p, const CallerContext& caller) {
    if (IsPanelMode()) return api::PanelModeUnsupported("window.setMaxSize");
    auto target = WindowTargetResolver::ResolveForMutation(p.windowId, caller.callerHwnd);
    if (!target.Success()) return TargetFailure(target, "window.setMaxSize");

    const int dpi = GetShellDpi(target.shell);
    target.shell->SetMaxSizeDip(
        window_geometry::PhysicalToDip(PixelArg(p.width), dpi),
        window_geometry::PhysicalToDip(PixelArg(p.height), dpi));

    wn::SetMaxSizeResult result;
    result.windowId = target.windowId;
    return result;
}


api::Result<wn::GetMaxSizeResult> WindowGetMaxSize(const wn::GetMaxSizeParams& p, const CallerContext& caller) {
    auto target = WindowTargetResolver::ResolveForObservation(p.windowId, caller.callerHwnd);
    if (!target.Success()) return TargetFailure(target, "window.getMaxSize");

    int widthDip = 0;
    int heightDip = 0;
    target.shell->GetMaxSizeDip(widthDip, heightDip);

    const int dpi = GetShellDpi(target.shell);
    wn::GetMaxSizeResult result;
    result.width = window_geometry::DipToPhysical(widthDip, dpi);
    result.height = window_geometry::DipToPhysical(heightDip, dpi);
    result.windowId = target.windowId;
    return result;
}


api::Result<wn::SetResizableResult> WindowSetResizable(const wn::SetResizableParams& p, const CallerContext& caller) {
    if (IsPanelMode()) return api::PanelModeUnsupported("window.setResizable");
    auto target = WindowTargetResolver::ResolveForMutation(p.windowId, caller.callerHwnd);
    if (!target.Success()) return TargetFailure(target, "window.setResizable");

    // SetResizableShell 只在 Win32 调用**真实失败**时返回 false（样式写入或
    // 帧刷新失败）。幂等设值返回 true——Q10 约束①：「无变化」不得报失败。
    // 所有 shell 形态都支持运行时切换，故不存在「能力不支持」这一失败类别。
    if (!target.shell->SetResizableShell(p.resizable)) {
        return api::Fail("Failed to apply the resizable window style", ApiErrorCode::OPERATION_FAILED,
                         {{"windowId", target.windowId}});
    }

    wn::SetResizableResult result;
    result.windowId = target.windowId;
    return result;
}


api::Result<wn::IsResizableResult> WindowIsResizable(const wn::IsResizableParams& p, const CallerContext& caller) {
    auto target = WindowTargetResolver::ResolveForObservation(p.windowId, caller.callerHwnd);
    if (!target.Success()) return TargetFailure(target, "window.isResizable");

    wn::IsResizableResult result;
    result.resizable = target.shell->IsResizableShell();
    result.windowId = target.windowId;
    return result;
}


// ========== Fullscreen ==========

api::Result<wn::SetFullscreenResult> WindowSetFullscreen(const wn::SetFullscreenParams& p,
                                                         const CallerContext& caller) {
    if (IsPanelMode()) return api::PanelModeUnsupported("window.setFullscreen");
    auto target = WindowTargetResolver::ResolveForMutation(p.windowId, caller.callerHwnd);
    if (!target.Success()) return TargetFailure(target, "window.setFullscreen");
    if (!target.shell->GetCapabilities().supportsFullscreen) {
        return api::Fail("this window does not support fullscreen", ApiErrorCode::NOT_SUPPORTED,
                         {{"fullscreen", target.shell->IsFullscreen()}});
    }

    bool currentlyFull = target.shell->IsFullscreen();
    if (p.enabled && !currentlyFull) {
        EnterFullscreenMode(target.shell->GetShellHwnd(), target.shell);
    }
    else if (!p.enabled && currentlyFull) {
        ExitFullscreenMode(target.shell->GetShellHwnd(), target.shell);
    }

    wn::SetFullscreenResult result;
    result.fullscreen = target.shell->IsFullscreen();
    return result;
}


api::Result<wn::ToggleFullscreenResult> WindowToggleFullscreen(const wn::ToggleFullscreenParams& p,
                                                               const CallerContext& caller) {
    if (IsPanelMode()) return api::PanelModeUnsupported("window.toggleFullscreen");
    auto target = WindowTargetResolver::ResolveForMutation(p.windowId, caller.callerHwnd);
    if (!target.Success()) return TargetFailure(target, "window.toggleFullscreen");
    if (!target.shell->GetCapabilities().supportsFullscreen) {
        return api::Fail("this window does not support fullscreen", ApiErrorCode::NOT_SUPPORTED,
                         {{"fullscreen", target.shell->IsFullscreen()}});
    }

    bool currentlyFull = target.shell->IsFullscreen();
    if (!currentlyFull) {
        EnterFullscreenMode(target.shell->GetShellHwnd(), target.shell);
    } else {
        ExitFullscreenMode(target.shell->GetShellHwnd(), target.shell);
    }

    wn::ToggleFullscreenResult result;
    result.fullscreen = target.shell->IsFullscreen();
    return result;
}


// ========== Focus ==========

api::Result<void> WindowFocus(const wn::FocusParams& p, const CallerContext& caller) {
    // 支持可选 windowId 参数，用于从主窗口聚焦指定弹窗；空串与省略等价
    const std::string targetId = p.windowId.value_or("");
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
        hwnd = CallerTopLevelHwnd(caller);
        if (hwnd) {
            auto* candidate = WindowManager::GetInstance().GetMainWindow();
            if (candidate && candidate->GetShellHwnd() == hwnd) {
                targetMainWin = candidate;
            }
        }
    }

    if (!hwnd) return WindowNotFound();

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

    return api::Ok();
}


api::Result<void> WindowBlur(const wn::BlurParams&, const CallerContext& caller) {
    // 没法可靠地让窗口失焦，改为激活 Z 序里的下一个窗口
    HWND hwnd = CallerTopLevelHwnd(caller);
    if (!hwnd) return WindowNotFound();

    HWND nextWindow = GetNextWindow(hwnd, GW_HWNDNEXT);
    if (nextWindow) {
        SetForegroundWindow(nextWindow);
    }
    return api::Ok();
}


// ========== Title ==========

api::Result<void> WindowSetTitle(const wn::SetTitleParams& p, const CallerContext& caller) {
    if (IsPanelMode()) return api::PanelModeUnsupported("window.setTitle");
    HWND hwnd = CallerTopLevelHwnd(caller);
    if (!hwnd) return WindowNotFound();

    std::wstring wideTitle = Utf8ToWide(p.title);
    SetWindowTextW(hwnd, wideTitle.c_str());
    return api::Ok();
}


api::Result<wn::GetTitleResult> WindowGetTitle(const wn::GetTitleParams&, const CallerContext& caller) {
    wn::GetTitleResult result;  // 没有窗口时为空串
    HWND hwnd = CallerTopLevelHwnd(caller);
    if (!hwnd) return result;

    wchar_t title[256];
    GetWindowTextW(hwnd, title, 256);
    result.title = WideToUtf8(title);
    return result;
}


// ========== Flash ==========

api::Result<void> WindowFlash(const wn::FlashParams& p, const CallerContext& caller) {
    if (IsPanelMode()) return api::PanelModeUnsupported("window.flash");
    HWND hwnd = CallerTopLevelHwnd(caller);
    if (!hwnd) return WindowNotFound();

    FLASHWINFO fi = { sizeof(FLASHWINFO) };
    fi.hwnd = hwnd;
    fi.dwFlags = p.enabled ? (FLASHW_ALL | FLASHW_TIMERNOFG) : FLASHW_STOP;
    fi.uCount = static_cast<UINT>(p.count);
    fi.dwTimeout = 0;

    FlashWindowEx(&fi);
    return api::Ok();
}


// ========== System Menu ==========
// 显示系统菜单（最小化/最大化/关闭等）
// 支持传入触发元素的矩形区域，避免菜单遮挡按钮

api::Result<void> WindowShowSystemMenu(const wn::ShowSystemMenuParams& p, const CallerContext& caller) {
    if (IsPanelMode()) return api::PanelModeUnsupported("window.showSystemMenu");
    HWND hwnd = CallerTopLevelHwnd(caller);
    if (!hwnd) return WindowNotFound();

    const int x = PixelArg(p.x);
    const int y = PixelArg(p.y);
    const int w = PixelArg(p.w);
    const int h = PixelArg(p.h);

    HMENU sysMenu = GetSystemMenu(hwnd, FALSE);
    if (!sysMenu) {
        return api::Fail("System menu not available", ApiErrorCode::OPERATION_FAILED);
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
    return api::Ok();
}


// ============================================
// 标题栏相关 API (客户区扩展支持)
// ============================================

api::Result<wn::GetTitlebarHeightResult> WindowGetTitlebarHeight(const wn::GetTitlebarHeightParams&,
                                                                 const CallerContext& caller) {
    HWND callerHwnd = CallerTopLevelHwnd(caller);
    wn::GetTitlebarHeightResult result;
    if (auto* mainWnd = FindMainByCallerHwnd(callerHwnd)) {
        result.height = mainWnd->GetTitlebarHeight();
    } else if (auto* popupWnd = FindPopupByCallerHwnd(callerHwnd)) {
        result.height = popupWnd->GetTitlebarHeight();
    } else {
        result.height = MainWindow::DEFAULT_TITLEBAR_HEIGHT;
    }
    return result;
}


api::Result<wn::SetTitlebarHeightResult> WindowSetTitlebarHeight(const wn::SetTitlebarHeightParams& p,
                                                                 const CallerContext& caller) {
    if (IsPanelMode()) return api::PanelModeUnsupported("window.setTitlebarHeight");
    // 范围按截掉小数之后的整数判，与迁移前 value<int> 的读法一致
    const int height = PixelArg(p.height);
    if (height < 24 || height > 100) {
        return api::Fail("Height must be between 24 and 100", ApiErrorCode::INVALID_PARAMS);
    }

    HWND callerHwnd = CallerTopLevelHwnd(caller);
    wn::SetTitlebarHeightResult result;
    result.height = height;
    if (auto* mainWnd = FindMainByCallerHwnd(callerHwnd)) {
        mainWnd->SetTitlebarHeight(height);
        return result;
    }
    if (auto* popupWnd = FindPopupByCallerHwnd(callerHwnd)) {
        popupWnd->SetTitlebarHeight(height);
        return result;
    }

    return WindowNotFound();
}


api::Result<wn::GetCaptionButtonsWidthResult> WindowGetCaptionButtonsWidth(const wn::GetCaptionButtonsWidthParams&) {
    wn::GetCaptionButtonsWidthResult result;
    auto* ui = WebViewUI::GetInstance();
    if (!ui || !ui->GetMainWindow()) {
        result.width = 138;
        result.buttonWidth = 46;
        return result;
    }

    auto* mainWnd = ui->GetMainWindow();
    result.width = mainWnd->GetCaptionButtonsWidth();
    result.buttonWidth = mainWnd->GetCaptionButtonWidth();
    return result;
}


// 页面给的 CSS 像素矩形换成物理像素；宽或高不为正的矩形丢掉。先截小数再乘缩放，与迁移前一致。
std::vector<TitlebarDragRegion> ToPhysicalRegions(const std::optional<std::vector<wn::WindowRegion>>& regions,
                                                   double dpiScale) {
    std::vector<TitlebarDragRegion> physical;
    if (!regions) return physical;
    for (const auto& r : *regions) {
        TitlebarDragRegion region;
        region.x = static_cast<int>(PixelArg(r.x) * dpiScale);
        region.y = static_cast<int>(PixelArg(r.y) * dpiScale);
        region.width = static_cast<int>(PixelArg(r.width) * dpiScale);
        region.height = static_cast<int>(PixelArg(r.height) * dpiScale);

        if (region.width <= 0 || region.height <= 0)
            continue;
        physical.push_back(region);
    }
    return physical;
}


api::Result<wn::SetDragRegionsResult> WindowSetDragRegions(const wn::SetDragRegionsParams& p,
                                                           const CallerContext& caller) {
    if (IsPanelMode()) return api::PanelModeUnsupported("window.setDragRegions");
    HWND callerHwnd = CallerTopLevelHwnd(caller);
    if (!callerHwnd) return WindowNotFound();

    // 获取 DPI 缩放比例 (CSS 像素 -> 物理像素)
    const double dpiScale = GetDpiForWindow(callerHwnd) / 96.0;
    std::vector<TitlebarDragRegion> regions = ToPhysicalRegions(p.regions, dpiScale);

    if (auto* mainWnd = FindMainByCallerHwnd(callerHwnd)) {
        mainWnd->SetDragRegions(regions);
    } else if (auto* popupWnd = FindPopupByCallerHwnd(callerHwnd)) {
        popupWnd->SetDragRegions(ConvertDragRegionsForPopup(regions));
    } else {
        return WindowNotFound();
    }

    wn::SetDragRegionsResult result;
    result.count = static_cast<std::int64_t>(regions.size());
    result.dpiScale = dpiScale;
    return result;
}


api::Result<void> WindowClearDragRegions(const wn::ClearDragRegionsParams&, const CallerContext& caller) {
    if (IsPanelMode()) return api::PanelModeUnsupported("window.clearDragRegions");
    HWND callerHwnd = CallerTopLevelHwnd(caller);
    if (auto* mainWnd = FindMainByCallerHwnd(callerHwnd)) {
        mainWnd->ClearDragRegions();
        return api::Ok();
    }
    if (auto* popupWnd = FindPopupByCallerHwnd(callerHwnd)) {
        popupWnd->ClearDragRegions();
        return api::Ok();
    }
    return WindowNotFound();
}


api::Result<wn::SetNoDragRegionsResult> WindowSetNoDragRegions(const wn::SetNoDragRegionsParams& p,
                                                               const CallerContext& caller) {
    if (IsPanelMode()) return api::PanelModeUnsupported("window.setNoDragRegions");
    HWND callerHwnd = CallerTopLevelHwnd(caller);
    if (!callerHwnd) return WindowNotFound();

    // 获取 DPI 缩放比例 (CSS 像素 -> 物理像素)
    const double dpiScale = GetDpiForWindow(callerHwnd) / 96.0;
    std::vector<TitlebarDragRegion> regions = ToPhysicalRegions(p.regions, dpiScale);

    if (auto* mainWnd = FindMainByCallerHwnd(callerHwnd)) {
        mainWnd->SetNoDragRegions(regions);
    } else if (auto* popupWnd = FindPopupByCallerHwnd(callerHwnd)) {
        popupWnd->SetNoDragRegions(ConvertDragRegionsForPopup(regions));
    } else {
        return WindowNotFound();
    }

    wn::SetNoDragRegionsResult result;
    result.count = static_cast<std::int64_t>(regions.size());
    result.dpiScale = dpiScale;
    return result;
}


api::Result<void> WindowClearNoDragRegions(const wn::ClearNoDragRegionsParams&, const CallerContext& caller) {
    if (IsPanelMode()) return api::PanelModeUnsupported("window.clearNoDragRegions");
    HWND callerHwnd = CallerTopLevelHwnd(caller);
    if (auto* mainWnd = FindMainByCallerHwnd(callerHwnd)) {
        mainWnd->ClearNoDragRegions();
        return api::Ok();
    }
    if (auto* popupWnd = FindPopupByCallerHwnd(callerHwnd)) {
        popupWnd->ClearNoDragRegions();
        return api::Ok();
    }
    return WindowNotFound();
}


// 页面自绘的最大化键。只收主窗口：popup 的命中测试从不答非客户区码，
// 贴靠布局也只对主窗口有意义。
api::Result<wn::SetMaximizeButtonRegionResult> WindowSetMaximizeButtonRegion(
    const wn::SetMaximizeButtonRegionParams& p, const CallerContext& caller) {
    if (IsPanelMode()) return api::PanelModeUnsupported("window.setMaximizeButtonRegion");
    HWND callerHwnd = CallerTopLevelHwnd(caller);
    if (!callerHwnd) return WindowNotFound();

    auto* mainWnd = FindMainByCallerHwnd(callerHwnd);
    if (!mainWnd) {
        if (FindPopupByCallerHwnd(callerHwnd)) {
            return api::Fail("Only the main window answers a maximize button region.",
                             ApiErrorCode::NOT_SUPPORTED);
        }
        return WindowNotFound();
    }

    // 与页面的 devicePixelRatio 同一个系数：拖动矩形只乘 DPI，但最大化键的位置
    // 错一点贴靠浮层就不出，页面缩放不能漏。
    double zoom = 1.0;
    if (WebViewHost* host = WebViewContext::GetInstance().GetWebViewHost(callerHwnd)) {
        const double factor = host->GetZoomFactor();
        if (factor > 0.0) zoom = factor;
    }
    const double scale = GetDpiForWindow(callerHwnd) / 96.0 * zoom;

    std::optional<maximize_button_region::Rect> region;
    if (p.region) {
        region = maximize_button_region::ToPhysical(PixelArg(p.region->x), PixelArg(p.region->y),
                                                    PixelArg(p.region->width),
                                                    PixelArg(p.region->height), scale);
    }
    mainWnd->SetMaximizeButtonRegion(region);

    wn::SetMaximizeButtonRegionResult result;
    result.hasRegion = region.has_value();
    result.snapLayouts = maximize_button_region::SnapLayoutsSupported();
    result.scale = scale;
    return result;
}


api::Result<wn::GetTitlebarInfoResult> WindowGetTitlebarInfo(const wn::GetTitlebarInfoParams&) {
    wn::GetTitlebarInfoResult result;
    auto* ui = WebViewUI::GetInstance();
    if (!ui || !ui->GetMainWindow()) {
        result.height = 32;
        result.captionButtonsWidth = 138;
        result.captionButtonWidth = 46;
        result.isMaximized = false;
        return result;
    }

    auto* mainWnd = ui->GetMainWindow();
    HWND hwnd = mainWnd->GetHwnd();

    result.height = mainWnd->GetTitlebarHeight();
    result.captionButtonsWidth = mainWnd->GetCaptionButtonsWidth();
    result.captionButtonWidth = mainWnd->GetCaptionButtonWidth();
    result.isMaximized = IsZoomed(hwnd) != FALSE;
    return result;
}


// ========== UI Context Menu ==========
// 显示 foobar2000 风格的上下文菜单

api::Result<void> UiShowContextMenu(const api::ui::ShowContextMenuParams& p) {
    auto* ui = WebViewUI::GetInstance();
    if (!ui || !ui->GetMainWindow()) {
        return api::Fail("Window not found", ApiErrorCode::OPERATION_FAILED);
    }
    
    int x = static_cast<int>(p.x);
    int y = static_cast<int>(p.y);
    
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
    
    return api::Ok();
}


// ========== System Theme API ==========

// system.getTheme - Get system theme information
api::Result<api::system::GetThemeResult> SystemGetTheme(const api::system::GetThemeParams&) {
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
    
    api::system::GetThemeResult result;
    result.darkMode = darkMode;
    result.isDark = darkMode;
    result.accentColor = accentColor;
    result.transparency = transparency;
    return result;
}


// system.getDPI - Get DPI scaling information
api::Result<api::system::GetDPIResult> SystemGetDPI(const api::system::GetDPIParams&, const CallerContext& caller) {
    HWND hwnd = CallerTopLevelHwnd(caller);
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
    
    api::system::GetDPIResult result;
    result.dpi = dpi;
    result.scale = static_cast<double>(dpi) / 96.0;
    return result;
}


// ========== Corner Preference (Windows 11+) ==========

api::Result<void> WindowSetCornerPreference(const wn::SetCornerPreferenceParams& p) {
    if (IsPanelMode()) return api::PanelModeUnsupported("window.setCornerPreference");
    MainWindow* mainWindow = GetMainWindow();
    if (!mainWindow) return WindowNotFound();

    mainWindow->SetCornerPreference(p.mode);
    return api::Ok();
}


api::Result<wn::GetCornerPreferenceResult> WindowGetCornerPreference(const wn::GetCornerPreferenceParams&) {
    wn::GetCornerPreferenceResult result;
    MainWindow* mainWindow = GetMainWindow();
    const std::string pref = mainWindow ? mainWindow->GetCornerPreference() : "default";
    result.mode = pref;
    result.preference = pref;
    return result;
}


// system.getLocale - Get system locale information
api::Result<api::system::GetLocaleResult> SystemGetLocale(const api::system::GetLocaleParams&) {
    wchar_t localeName[LOCALE_NAME_MAX_LENGTH];
    std::string locale = "en-US";  // Default
    
    if (GetUserDefaultLocaleName(localeName, LOCALE_NAME_MAX_LENGTH)) {
        locale = WideToUtf8(localeName);
    }
    
    // Get language name
    wchar_t langName[256];
    std::string language;
    if (GetLocaleInfoEx(localeName, LOCALE_SLOCALIZEDLANGUAGENAME, langName, 256)) {
        language = WideToUtf8(langName);
    }
    
    // Get country name
    wchar_t countryName[256];
    std::string country;
    if (GetLocaleInfoEx(localeName, LOCALE_SLOCALIZEDCOUNTRYNAME, countryName, 256)) {
        country = WideToUtf8(countryName);
    }
    
    api::system::GetLocaleResult result;
    result.locale = locale;
    result.language = language;
    result.country = country;
    return result;
}


// ========== Additional Window APIs ==========

api::Result<void> WindowSetPosition(const wn::SetPositionParams& p, const CallerContext& caller) {
    if (IsPanelMode()) return api::PanelModeUnsupported("window.setPosition");
    HWND hwnd = CallerTopLevelHwnd(caller);
    if (!hwnd) return WindowNotFound();
    SetWindowPos(hwnd, nullptr, PixelArg(p.x), PixelArg(p.y), 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    return api::Ok();
}


api::Result<void> WindowSetSize(const wn::SetSizeParams& p, const CallerContext& caller) {
    if (IsPanelMode()) return api::PanelModeUnsupported("window.setSize");
    HWND hwnd = CallerTopLevelHwnd(caller);
    if (!hwnd) return WindowNotFound();
    SetWindowPos(hwnd, nullptr, 0, 0, PixelArg(p.width), PixelArg(p.height), SWP_NOMOVE | SWP_NOZORDER);
    return api::Ok();
}


api::Result<wn::GetDpiScaleResult> WindowGetDpiScale(const wn::GetDpiScaleParams&, const CallerContext& caller) {
    HWND hwnd = CallerTopLevelHwnd(caller);
    int dpi = 96;
    if (hwnd) {
        HDC hdc = GetDC(hwnd);
        if (hdc) {
            dpi = GetDeviceCaps(hdc, LOGPIXELSX);
            ReleaseDC(hwnd, hdc);
        }
    }
    wn::GetDpiScaleResult result;
    result.dpi = dpi;
    result.scale = dpi / 96.0;
    return result;
}


api::Result<void> WindowFlashTaskbar(const wn::FlashTaskbarParams& p, const CallerContext& caller) {
    if (IsPanelMode()) return api::PanelModeUnsupported("window.flashTaskbar");
    HWND hwnd = CallerTopLevelHwnd(caller);
    if (!hwnd) return WindowNotFound();
    FLASHWINFO fi = {sizeof(FLASHWINFO), hwnd, FLASHW_ALL, static_cast<UINT>(p.count), 0};
    FlashWindowEx(&fi);
    return api::Ok();
}


api::Result<wn::EnterFullscreenResult> WindowEnterFullscreen(const wn::EnterFullscreenParams& p,
                                                             const CallerContext& caller) {
    if (IsPanelMode()) return api::PanelModeUnsupported("window.enterFullscreen");
    auto target = WindowTargetResolver::ResolveForMutation(p.windowId, caller.callerHwnd);
    if (!target.Success()) return TargetFailure(target, "window.enterFullscreen");
    if (!target.shell->GetCapabilities().supportsFullscreen) {
        return api::Fail("this window does not support fullscreen", ApiErrorCode::NOT_SUPPORTED,
                         {{"isFullscreen", target.shell->IsFullscreen()}});
    }
    if (target.shell->IsFullscreen()) {
        return api::Fail("Window is already fullscreen", ApiErrorCode::OPERATION_FAILED);
    }

    // fullscreen implementation still in WindowApi.cpp (stealth DWM)
    EnterFullscreenMode(target.shell->GetShellHwnd(), target.shell);
    wn::EnterFullscreenResult result;
    result.isFullscreen = true;
    return result;
}


api::Result<wn::ExitFullscreenResult> WindowExitFullscreen(const wn::ExitFullscreenParams& p,
                                                           const CallerContext& caller) {
    if (IsPanelMode()) return api::PanelModeUnsupported("window.exitFullscreen");
    auto target = WindowTargetResolver::ResolveForMutation(p.windowId, caller.callerHwnd);
    if (!target.Success()) return TargetFailure(target, "window.exitFullscreen");
    if (!target.shell->GetCapabilities().supportsFullscreen) {
        return api::Fail("this window does not support fullscreen", ApiErrorCode::NOT_SUPPORTED,
                         {{"isFullscreen", target.shell->IsFullscreen()}});
    }
    if (!target.shell->IsFullscreen()) {
        return api::Fail("Window is not fullscreen", ApiErrorCode::OPERATION_FAILED);
    }

    // fullscreen implementation still in WindowApi.cpp (stealth DWM)
    ExitFullscreenMode(target.shell->GetShellHwnd(), target.shell);
    wn::ExitFullscreenResult result;
    result.isFullscreen = false;
    return result;
}


// setMica 与 setMicaEffect 同形；两个方法各有一份参数与结果结构体（kMethod 不同）。
template <class R, class P>
api::Result<R> SetMicaEffectImpl(const P& p, const CallerContext& caller, std::string_view method) {
    if (IsPanelMode()) return api::PanelModeUnsupported(method);
    auto target = WindowTargetResolver::ResolveForMutation(p.windowId, caller.callerHwnd);
    if (!target.Success()) return TargetFailure(target, method);

    // "mica" or "mica-alt"
    const std::string variant = p.variant == "mica-alt" ? "mica-alt" : "mica";

    // darkMode 参数: true=深色, false=浅色, 不设置则不改变
    // 这决定了 Mica 效果显示深色还是浅色背景
    const bool success = target.shell->PatchCompatibilityBackdrop(
        p.enabled ? std::optional<std::string>(variant) : std::optional<std::string>(),
        p.darkMode,
        p.enabled);

    R result;
    result.enabled = p.enabled;
    result.variant = variant;
    result.darkMode = p.darkMode;
    if (!success) return FailWithFields(kBackdropNotApplied, result);
    return result;
}


api::Result<wn::SetMicaResult> WindowSetMica(const wn::SetMicaParams& p, const CallerContext& caller) {
    return SetMicaEffectImpl<wn::SetMicaResult>(p, caller, "window.setMica");
}


// 别名: window.setMicaEffect -> window.setMica (兼容性)
api::Result<wn::SetMicaEffectResult> WindowSetMicaEffect(const wn::SetMicaEffectParams& p, const CallerContext& caller) {
    return SetMicaEffectImpl<wn::SetMicaEffectResult>(p, caller, "window.setMicaEffect");
}


api::Result<wn::SetBlurResult> WindowSetBlur(const wn::SetBlurParams& p, const CallerContext& caller) {
    if (IsPanelMode()) return api::PanelModeUnsupported("window.setBlur");
    auto target = WindowTargetResolver::ResolveForMutation(p.windowId, caller.callerHwnd);
    if (!target.Success()) return TargetFailure(target, "window.setBlur");

    const bool success = target.shell->PatchCompatibilityBlur(p.enabled);
    wn::SetBlurResult result;
    result.enabled = p.enabled;
    if (!success) return FailWithFields(kBackdropNotApplied, result);
    return result;
}


api::Result<wn::SetAcrylicResult> WindowSetAcrylic(const wn::SetAcrylicParams& p, const CallerContext& caller) {
    if (IsPanelMode()) return api::PanelModeUnsupported("window.setAcrylic");
    auto target = WindowTargetResolver::ResolveForMutation(p.windowId, caller.callerHwnd);
    if (!target.Success()) return TargetFailure(target, "window.setAcrylic");

    // darkMode 参数: true=深色, false=浅色
    const bool success = target.shell->PatchCompatibilityBackdrop(
        p.enabled ? std::optional<std::string>("acrylic") : std::optional<std::string>(),
        p.darkMode,
        p.enabled);

    wn::SetAcrylicResult result;
    result.enabled = p.enabled;
    result.darkMode = p.darkMode;
    if (!success) return FailWithFields(kBackdropNotApplied, result);
    return result;
}


api::Result<wn::SetDarkModeResult> WindowSetDarkMode(const wn::SetDarkModeParams& p, const CallerContext& caller) {
    if (IsPanelMode()) return api::PanelModeUnsupported("window.setDarkMode");
    auto target = WindowTargetResolver::ResolveForMutation(p.windowId, caller.callerHwnd);
    if (!target.Success()) return TargetFailure(target, "window.setDarkMode");

    const bool success = target.shell->PatchCompatibilityDarkMode(p.enabled);
    wn::SetDarkModeResult result;
    result.enabled = p.enabled;
    if (!success) return FailWithFields(kBackdropNotApplied, result);
    return result;
}


// ============================================
// WebView 背景透明度控制 - 用于 DWM 效果穿透
// ============================================

api::Result<wn::SetBackgroundTransparencyResult> WindowSetBackgroundTransparency(
    const wn::SetBackgroundTransparencyParams& p, const CallerContext& caller) {
    auto target = WindowTargetResolver::ResolveForMutation(p.windowId, caller.callerHwnd);
    if (!target.Success()) return TargetFailure(target, "window.setBackgroundTransparency");

    const bool transparent = p.transparent;
    bool compatibilitySuccess = target.shell->PatchCompatibilityTransparency(transparent);

    // WebView host 透明度也需要同步设置
    auto* host = WebViewContext::GetInstance().GetHostByHwnd(target.shell->GetShellHwnd());
    HRESULT hr = E_FAIL;
    if (host) {
        hr = host->SetBackgroundTransparent(transparent);
    }
    if (!host && !compatibilitySuccess) {
        return api::Fail("WebView not available and the window did not apply the change",
                         ApiErrorCode::OPERATION_FAILED);
    }

    wn::SetBackgroundTransparencyResult result;
    result.transparent = transparent;
    result.description = transparent
        ? "WebView background is now transparent - DWM effects will show through"
        : "WebView background is now opaque";
    if (FAILED(hr) && !compatibilitySuccess) {
        return FailWithFields("Neither the window nor its WebView applied the change", result);
    }
    return result;
}


// 刷新 WebView 渲染（DWM 效果后调用）
api::Result<void> WindowRefreshWebView(const wn::RefreshWebViewParams&, const CallerContext& caller) {
    HWND callerHwnd = CallerTopLevelHwnd(caller);
    auto* host = WebViewContext::GetInstance().GetHostByHwnd(callerHwnd);
    if (!host) return WebViewNotAvailable();

    host->RefreshForDwmEffect();
    return api::Ok();
}


// 重新加载 WebView 页面（开发调试用）
api::Result<void> WindowReload(const wn::ReloadParams&, const CallerContext& caller) {
    HWND callerHwnd = CallerTopLevelHwnd(caller);
    auto* host = WebViewContext::GetInstance().GetHostByHwnd(callerHwnd);
    if (!host) return WebViewNotAvailable();

    host->Reload();
    return api::Ok();
}


// ============================================
// HMR 开发服务器配置 API
// ============================================

api::Result<wn::GetDevServerConfigResult> WindowGetDevServerConfig(const wn::GetDevServerConfigParams&) {
    wn::GetDevServerConfigResult result;
    result.useDevServer = security_config::UseDevServer();
    result.devServerUrl = security_config::GetDevServerUrl();
    return result;
}


api::Result<wn::SetDevServerConfigResult> WindowSetDevServerConfig(const wn::SetDevServerConfigParams& p) {
    // 两个字段各自独立写入: 省略 devServerUrl 曾经会把已存的地址擦成空串,
    // 于是「只想打开开关」的调用把下次启动要用的地址一起丢掉了.
    if (p.useDevServer) {
        security_config::SetUseDevServer(*p.useDevServer);
    }
    if (p.devServerUrl) {
        security_config::SetDevServerUrl(p.devServerUrl->c_str());
    }

    // 回读而非回显请求值: 部分更新时未给的那个字段仍要报出真实的当前值.
    wn::SetDevServerConfigResult result;
    result.useDevServer = security_config::UseDevServer();
    result.devServerUrl = security_config::GetDevServerUrl();
    return result;
}


// ============================================
// 检查是否有保存的窗口位置
// 前端可以使用此 API 来决定是否设置默认窗口大小
// ============================================
api::Result<wn::HasSavedBoundsResult> WindowHasSavedBounds(const wn::HasSavedBoundsParams&) {
    wn::HasSavedBoundsResult result;
    result.hasSavedBounds = window_config::HasSavedPosition();
    result.description = result.hasSavedBounds
        ? "Window has saved position from previous session"
        : "No saved window position, this may be first launch";
    return result;
}



// ============================================
// 缩放控制 API (DPI 适配)
// ============================================

// 设置缩放比例
api::Result<wn::SetZoomResult> WindowSetZoom(const wn::SetZoomParams& p, const CallerContext& caller) {
    HWND callerHwnd = CallerTopLevelHwnd(caller);
    auto* host = WebViewContext::GetInstance().GetHostByHwnd(callerHwnd);
    if (!host) return WebViewNotAvailable();

    // 主题接管了这个 WebView 的缩放：偏好页的默认缩放此后不再覆盖它（本进程内有效）。
    host->MarkZoomOverriddenByTheme();
    HRESULT hr = host->SetZoomFactor(p.zoom);

    wn::SetZoomResult result;
    result.zoom = host->GetZoomFactor();
    if (FAILED(hr)) return FailWithFields(kZoomNotApplied, result);
    return result;
}


// 获取当前缩放比例
api::Result<wn::GetZoomResult> WindowGetZoom(const wn::GetZoomParams&, const CallerContext& caller) {
    HWND callerHwnd = CallerTopLevelHwnd(caller);
    auto* host = WebViewContext::GetInstance().GetHostByHwnd(callerHwnd);
    wn::GetZoomResult result;
    if (!host) {
        // 没有 WebView 时只报默认倍数，不带 DPI
        result.zoom = 1.0;
        return result;
    }

    result.zoom = host->GetZoomFactor();

    // 获取当前 DPI 信息
    int dpi = 96;
    if (callerHwnd) {
        dpi = GetDpiForWindow(callerHwnd);
    }
    result.dpi = dpi;
    result.dpiScale = dpi / 96.0;
    return result;
}


// 重置缩放为默认
api::Result<wn::ResetZoomResult> WindowResetZoom(const wn::ResetZoomParams&, const CallerContext& caller) {
    HWND callerHwnd = CallerTopLevelHwnd(caller);
    auto* host = WebViewContext::GetInstance().GetHostByHwnd(callerHwnd);
    if (!host) return WebViewNotAvailable();

    // 主题显式要 1.0，同样算接管：偏好里的默认缩放不再推给这个 WebView。
    host->MarkZoomOverriddenByTheme();
    HRESULT hr = host->SetZoomFactor(1.0);

    wn::ResetZoomResult result;
    result.zoom = 1.0;
    if (FAILED(hr)) return FailWithFields(kZoomNotApplied, result);
    return result;
}


// 根据 DPI 自动设置缩放
api::Result<wn::SetZoomForDpiResult> WindowSetZoomForDpi(const wn::SetZoomForDpiParams& p, const CallerContext& caller) {
    HWND callerHwnd = CallerTopLevelHwnd(caller);
    auto* host = WebViewContext::GetInstance().GetHostByHwnd(callerHwnd);
    if (!host) return WebViewNotAvailable();

    int dpi = static_cast<int>(std::clamp<std::int64_t>(p.dpi, std::numeric_limits<int>::min(),
                                                        std::numeric_limits<int>::max()));

    // 如果未指定 DPI，使用当前窗口 DPI
    if (dpi <= 0 && callerHwnd) {
        dpi = GetDpiForWindow(callerHwnd);
    }
    if (dpi <= 0) dpi = 96;

    host->MarkZoomOverriddenByTheme();
    HRESULT hr = host->SetZoomForDpi(dpi);

    wn::SetZoomForDpiResult result;
    result.dpi = dpi;
    result.zoom = host->GetZoomFactor();
    if (FAILED(hr)) return FailWithFields(kZoomNotApplied, result);
    return result;
}


// ============================================
// 多窗口 API
// ============================================

// 创建弹出窗口
api::Result<wn::CreatePopupResult> WindowCreatePopup(const wn::CreatePopupParams& p, const CallerContext& caller) {
    // 惰性初始化：面板模式下自动初始化 WindowManager
    auto& wm = WindowManager::GetInstance();
    if (!wm.IsInitialized()) {
        wm.InitializeForPanel();
    }

    PopupWindow::CreateParams cp;
    cp.url = p.url;
    // 规则：弹窗只继承打开者已有的信任。远程网站可以打开，但拿不到桥。
    if (WebViewHost* opener = WebViewContext::GetInstance().GetWebViewHost(caller.callerHwnd)) {
        cp.trustAbsoluteUrl = opener->IsTrustedOrigin(Utf8ToWide(p.url));
    }
    cp.width = PixelArg(p.width);
    cp.height = PixelArg(p.height);
    cp.x = p.x ? PixelArg(*p.x) : static_cast<int>(CW_USEDEFAULT);
    cp.y = p.y ? PixelArg(*p.y) : static_cast<int>(CW_USEDEFAULT);
    cp.title = p.title;
    cp.resizable = p.resizable;
    cp.alwaysOnTop = p.alwaysOnTop;
    cp.showInTaskbar = p.showInTaskbar.value_or(false);
    cp.hasShowInTaskbar = p.showInTaskbar.has_value();
    cp.minWidth = PixelArg(p.minWidth);
    cp.minHeight = PixelArg(p.minHeight);
    cp.maxWidth = PixelArg(p.maxWidth);
    cp.maxHeight = PixelArg(p.maxHeight);
    cp.frame = p.frame;
    cp.transparent = p.transparent;
    cp.beforeClose = p.beforeClose;
    cp.clickThrough = p.clickThrough;
    cp.profile = p.profile.value_or("");
    cp.hasProfile = p.profile.has_value();
    if (p.behavior) {
        cp.behavior = OverridesToJson(*p.behavior);
        cp.hasBehavior = true;
    }
    if (p.backdropPolicy) {
        cp.backdropPolicy = OverridesToJson(*p.backdropPolicy);
        cp.hasBackdropPolicy = true;
    }

    if (static_cast<int>(wm.GetPopupCount()) >= WindowManager::MAX_POPUPS) {
        return api::Fail(ApiError::MAX_POPUPS_REACHED, ApiErrorCode::OPERATION_FAILED);
    }

    std::string windowId = wm.CreatePopup(cp);
    if (windowId.empty()) {
        return api::Fail(ApiError::POPUP_CREATE_FAILED, ApiErrorCode::OPERATION_FAILED);
    }

    wn::CreatePopupResult result;
    result.windowId = std::move(windowId);
    return result;
}


// 关闭弹出窗口
api::Result<void> WindowClosePopup(const wn::ClosePopupParams& p) {
    if (p.windowId == "main") {
        return api::Fail(ApiError::CANNOT_CLOSE_MAIN, ApiErrorCode::INVALID_PARAMS);
    }

    if (!WindowManager::GetInstance().ClosePopup(p.windowId)) {
        return WindowNotFound();
    }
    return api::Ok();
}


// 关闭所有弹出窗口
api::Result<void> WindowCloseAllPopups(const wn::CloseAllPopupsParams&) {
    WindowManager::GetInstance().CloseAllPopups();
    return api::Ok();
}


// 获取所有窗口信息
api::Result<wn::GetAllWindowsResult> WindowGetAllWindows(const wn::GetAllWindowsParams&) {
    wn::GetAllWindowsResult result;
    for (const auto& item : WindowManager::GetInstance().GetAllWindowsInfo()) {
        result.items.push_back(ToWindowInfo(item));
    }
    return result;
}


// 获取当前窗口 ID
api::Result<wn::GetCurrentWindowIdResult> WindowGetCurrentWindowId(const wn::GetCurrentWindowIdParams&,
                                                                   const CallerContext& caller) {
    // 主窗口 → popup → 面板实例，都对不上时回退 main
    wn::GetCurrentWindowIdResult result;
    result.windowId = GetCallerWindowId(caller);
    return result;
}


// 获取弹出窗口行为策略
api::Result<wn::GetPopupBehaviorResult> WindowGetPopupBehavior(const wn::GetPopupBehaviorParams& p,
                                                               const CallerContext& caller) {
    std::string targetId;
    auto found = FindBehaviorPopup(p.windowId, caller, "window.getPopupBehavior", targetId);
    if (auto* failure = std::get_if<api::Failure>(&found)) return std::move(*failure);

    return PopupBehaviorResultOf<wn::GetPopupBehaviorResult>(*std::get<PopupWindow*>(found), targetId);
}


// 运行时更新弹出窗口行为策略
api::Result<wn::SetPopupBehaviorResult> WindowSetPopupBehavior(const wn::SetPopupBehaviorParams& p,
                                                               const CallerContext& caller) {
    std::string targetId;
    auto found = FindBehaviorPopup(p.windowId, caller, "window.setPopupBehavior", targetId);
    if (auto* failure = std::get_if<api::Failure>(&found)) return std::move(*failure);
    PopupWindow* popup = std::get<PopupWindow*>(found);

    // profile 与 behavior 各自独立：只传一个不动另一个
    const json behaviorPatch = p.behavior ? OverridesToJson(*p.behavior) : json::object();
    std::string error;
    if (!popup->UpdatePopupBehavior(p.profile.value_or(""), p.profile.has_value(), behaviorPatch,
                                    p.behavior.has_value(), error)) {
        return api::Fail(error.empty() ? "failed to update popup behavior" : error, ApiErrorCode::INVALID_PARAMS);
    }

    return PopupBehaviorResultOf<wn::SetPopupBehaviorResult>(*popup, targetId);
}


// 获取 DWM 背景策略
api::Result<wn::GetBackdropPolicyResult> WindowGetBackdropPolicy(const wn::GetBackdropPolicyParams& p,
                                                                 const CallerContext& caller) {
    auto target = WindowTargetResolver::ResolveForObservation(p.windowId, caller.callerHwnd);
    if (!target.Success()) return TargetFailure(target, "window.getBackdropPolicy");

    return BackdropPolicyResultOf<wn::GetBackdropPolicyResult>(*target.shell, target.windowId);
}


// 运行时更新 DWM 背景策略
api::Result<wn::SetBackdropPolicyResult> WindowSetBackdropPolicy(const wn::SetBackdropPolicyParams& p,
                                                                 const CallerContext& caller) {
    auto target = WindowTargetResolver::ResolveForMutation(p.windowId, caller.callerHwnd);
    if (!target.Success()) return TargetFailure(target, "window.setBackdropPolicy");

    // 覆盖在应用之前就已合并；返回 false 多是窗口暂时画不出来（例如启动时仍隐藏）
    std::string error;
    if (!target.shell->PatchBackdropPolicy(OverridesToJson(p.backdropPolicy), error)) {
        return api::Fail(error.empty() ? kBackdropNotApplied : error, ApiErrorCode::OPERATION_FAILED);
    }

    return BackdropPolicyResultOf<wn::SetBackdropPolicyResult>(*target.shell, target.windowId);
}


// 取消关闭（前端调用）
api::Result<void> WindowCancelClose(const wn::CancelCloseParams&, const CallerContext& caller) {
    // 查找对应的 PopupWindow
    if (auto* popup = FindPopupByCallerHwnd(CallerTopLevelHwnd(caller))) {
        popup->CancelClose();
        return api::Ok();
    }
    return WindowNotFound();
}


// 确认关闭（前端调用）
api::Result<void> WindowConfirmClose(const wn::ConfirmCloseParams&, const CallerContext& caller) {
    // 查找对应的 PopupWindow
    if (auto* popup = FindPopupByCallerHwnd(CallerTopLevelHwnd(caller))) {
        popup->ConfirmClose();
        return api::Ok();
    }
    return WindowNotFound();
}


// 动态切换窗口标题栏（插件端自动判断窗口类型，开发者无需关心）
api::Result<wn::SetFramelessResult> WindowSetFrameless(const wn::SetFramelessParams& p, const CallerContext& caller) {
    auto target = WindowTargetResolver::ResolveForMutation(p.windowId, caller.callerHwnd);
    if (!target.Success()) {
        if (IsPanelMode()) return api::PanelModeUnsupported("window.setFrameless");
        return TargetFailure(target, "window.setFrameless");
    }

    target.shell->PatchFrameless(p.frameless);
    wn::SetFramelessResult result;
    result.frameless = target.shell->IsFrameless();
    return result;
}


// 设置弹出窗口鼠标穿透
api::Result<wn::SetClickThroughResult> WindowSetClickThrough(const wn::SetClickThroughParams& p,
                                                             const CallerContext& caller) {
    std::string windowId;
    auto* popup = FindClickThroughPopup(p.windowId, caller, windowId);
    if (!popup) return WindowNotFound();

    popup->SetClickThrough(p.enabled);
    wn::SetClickThroughResult result;
    result.clickThrough = popup->IsClickThrough();
    return result;
}


// 查询弹出窗口鼠标穿透状态
api::Result<wn::IsClickThroughResult> WindowIsClickThrough(const wn::IsClickThroughParams& p,
                                                           const CallerContext& caller) {
    std::string windowId;
    auto* popup = FindClickThroughPopup(p.windowId, caller, windowId);
    if (!popup) return WindowNotFound();

    wn::IsClickThroughResult result;
    result.clickThrough = popup->IsClickThrough();
    return result;
}


// 设置弹出窗口 click-through 交互热区
api::Result<wn::SetClickThroughExcludeRegionsResult> WindowSetClickThroughExcludeRegions(
    const wn::SetClickThroughExcludeRegionsParams& p, const CallerContext& caller) {
    std::string windowId;
    auto* popup = FindClickThroughPopup(p.windowId, caller, windowId);
    if (!popup) return WindowNotFound();

    // 获取 DPI 缩放比例（CSS 像素 → 物理像素）
    double dpiScale = 1.0;
    HWND hwnd = popup->GetHwnd();
    if (hwnd) {
        int dpi = GetDpiForWindow(hwnd);
        dpiScale = dpi / 96.0;
    }

    const std::vector<wn::WindowRegion> none;
    const auto& regions = p.regions ? *p.regions : none;

    constexpr size_t MAX_REGIONS = 32;
    bool truncated = regions.size() > MAX_REGIONS;

    std::vector<RECT> physicalRects;
    physicalRects.reserve(std::min(regions.size(), MAX_REGIONS));

    // 先截小数再乘缩放，与迁移前一致；只数保留下来的矩形
    for (const auto& r : regions) {
        if (physicalRects.size() >= MAX_REGIONS) break;
        int w = static_cast<int>(PixelArg(r.width) * dpiScale);
        int h = static_cast<int>(PixelArg(r.height) * dpiScale);
        if (w <= 0 || h <= 0) continue;

        RECT rc;
        rc.left   = static_cast<int>(PixelArg(r.x) * dpiScale);
        rc.top    = static_cast<int>(PixelArg(r.y) * dpiScale);
        rc.right  = rc.left + w;
        rc.bottom = rc.top + h;
        physicalRects.push_back(rc);
    }

    popup->SetClickThroughExcludeRegions(physicalRects);

    wn::SetClickThroughExcludeRegionsResult result;
    result.windowId = windowId;
    result.count = static_cast<std::int64_t>(physicalRects.size());
    result.dpiScale = dpiScale;
    if (truncated) {
        result.warning = "regions truncated to 32";
    }
    return result;
}


// 清除弹出窗口 click-through 交互热区
api::Result<wn::ClearClickThroughExcludeRegionsResult> WindowClearClickThroughExcludeRegions(
    const wn::ClearClickThroughExcludeRegionsParams& p, const CallerContext& caller) {
    std::string windowId;
    auto* popup = FindClickThroughPopup(p.windowId, caller, windowId);
    if (!popup) return WindowNotFound();

    popup->ClearClickThroughExcludeRegions();
    wn::ClearClickThroughExcludeRegionsResult result;
    result.windowId = windowId;
    return result;
}


// ============================================
// 自定义跨窗口消息 API
// ============================================

// 定向发送消息到指定窗口
api::Result<void> WindowSendMessage(const wn::SendMessageParams& p, const CallerContext& caller) {
    const std::string sourceId = GetCallerWindowId(caller);
    if (!WindowManager::GetInstance().SendWindowMessage(sourceId, p.targetWindowId, p.message)) {
        return api::Fail(ApiError::TARGET_WINDOW_NOT_FOUND, ApiErrorCode::NOT_FOUND);
    }
    return api::Ok();
}


// 广播消息到除发送者外的所有窗口
api::Result<void> WindowBroadcast(const wn::BroadcastParams& p, const CallerContext& caller) {
    WindowManager::GetInstance().BroadcastMessage(GetCallerWindowId(caller), p.message);
    return api::Ok();
}


// ============================================
// 面板配置 API
// ============================================

// window.getMode - 获取当前面板模式
api::Result<wn::GetModeResult> WindowGetMode(const wn::GetModeParams&, const CallerContext& caller) {
    const auto identity = GetCallerWindowIdentity(caller);
    wn::GetModeResult result;
    result.mode = identity.mode;
    result.panelMode = identity.panelMode;
    result.windowId = identity.ModeWindowId();
    return result;
}


// panel.getConfig - 获取面板配置
api::Result<api::panel::GetConfigResult> PanelGetConfig(const api::panel::GetConfigParams&, const CallerContext& caller) {
    const auto identity = GetCallerWindowIdentity(caller);
    auto* panel = identity.panelHwnd ? WebViewContext::GetInstance().GetPanelByHwnd(identity.panelHwnd) : nullptr;
    if (panel) {
        const PanelConfig& cfg = panel->GetConfig();
        api::panel::GetConfigResult result;
        FillPanelConfig(cfg, result.config);
        return result;
    }
    
    return api::Fail("Panel not found", ApiErrorCode::NOT_FOUND);
}


// panel.setConfig - 设置面板配置（安全字段白名单）
api::Result<api::panel::SetConfigResult> PanelSetConfig(const api::panel::SetConfigParams& p, const CallerContext& caller) {
    const auto identity = GetCallerWindowIdentity(caller);
    auto* panel = identity.panelHwnd ? WebViewContext::GetInstance().GetPanelByHwnd(identity.panelHwnd) : nullptr;
    if (panel) {
        PanelConfig oldConfig = panel->GetConfig();
        PanelConfig newConfig = oldConfig;
        
        // 安全白名单：声明里只有这四个键
        if (p.panelName) newConfig.panelName = *p.panelName;
        if (p.transparentBackground) newConfig.transparentBackground = *p.transparentBackground;
        if (p.grabFocus) newConfig.grabFocus = *p.grabFocus;
        if (p.enableDragDrop) newConfig.enableDragDrop = *p.enableDragDrop;
        
        // 禁止从 JS 修改的字段：enableDevTools, urlOverride, templateName
        // 这些字段只能通过配置对话框修改
        
        api::panel::SetConfigResult result;
        result.changed = newConfig.HasChanged(oldConfig);
        if (result.changed) {
            panel->ApplyConfig(oldConfig, newConfig);
        }
        return result;
    }
    
    return api::Fail("Panel not found", ApiErrorCode::NOT_FOUND);
}

} // namespace

void RegisterWindowApi() {
    // Parameters and results come from src/api/schema/window.ts through the generated types.
    api::RegisterApi("window.minimize", WindowMinimize);
    api::RegisterApi("window.maximize", WindowMaximize);
    api::RegisterApi("window.restore", WindowRestore);
    api::RegisterApi("window.close", WindowClose);
    api::RegisterApi("window.toggleMaximize", WindowToggleMaximize);
    api::RegisterApi("window.isMaximized", WindowIsMaximized);
    api::RegisterApi("window.isMinimized", WindowIsMinimized);
    api::RegisterApi("window.isFullscreen", WindowIsFullscreen);
    api::RegisterApi("window.getState", WindowGetState);
    api::RegisterApi("window.startDrag", WindowStartDrag);
    api::RegisterApi("window.startResize", WindowStartResize);
    api::RegisterApi("window.setAlwaysOnTop", WindowSetAlwaysOnTop);
    api::RegisterApi("window.isAlwaysOnTop", WindowIsAlwaysOnTop);
    api::RegisterApi("window.toggleAlwaysOnTop", WindowToggleAlwaysOnTop);
    api::RegisterApi("window.getBounds", WindowGetBounds);
    api::RegisterApi("window.setBounds", WindowSetBounds);
    api::RegisterApi("window.center", WindowCenter);
    api::RegisterApi("window.setMinSize", WindowSetMinSize);
    api::RegisterApi("window.getMinSize", WindowGetMinSize);
    api::RegisterApi("window.setMaxSize", WindowSetMaxSize);
    api::RegisterApi("window.getMaxSize", WindowGetMaxSize);
    api::RegisterApi("window.setResizable", WindowSetResizable);
    api::RegisterApi("window.isResizable", WindowIsResizable);
    api::RegisterApi("window.setFullscreen", WindowSetFullscreen);
    api::RegisterApi("window.toggleFullscreen", WindowToggleFullscreen);
    api::RegisterApi("window.focus", WindowFocus);
    api::RegisterApi("window.blur", WindowBlur);
    api::RegisterApi("window.setTitle", WindowSetTitle);
    api::RegisterApi("window.getTitle", WindowGetTitle);
    api::RegisterApi("window.flash", WindowFlash);
    api::RegisterApi("window.showSystemMenu", WindowShowSystemMenu);
    api::RegisterApi("window.getTitlebarHeight", WindowGetTitlebarHeight);
    api::RegisterApi("window.setTitlebarHeight", WindowSetTitlebarHeight);
    api::RegisterApi("window.getCaptionButtonsWidth", WindowGetCaptionButtonsWidth);
    api::RegisterApi("window.setDragRegions", WindowSetDragRegions);
    api::RegisterApi("window.clearDragRegions", WindowClearDragRegions);
    api::RegisterApi("window.setNoDragRegions", WindowSetNoDragRegions);
    api::RegisterApi("window.clearNoDragRegions", WindowClearNoDragRegions);
    api::RegisterApi("window.setMaximizeButtonRegion", WindowSetMaximizeButtonRegion);
    api::RegisterApi("window.getTitlebarInfo", WindowGetTitlebarInfo);
    api::RegisterApi("ui.showContextMenu", UiShowContextMenu);
    api::RegisterApi("system.getTheme", SystemGetTheme);
    api::RegisterApi("system.getDPI", SystemGetDPI);
    api::RegisterApi("window.setCornerPreference", WindowSetCornerPreference);
    api::RegisterApi("window.getCornerPreference", WindowGetCornerPreference);
    api::RegisterApi("system.getLocale", SystemGetLocale);
    api::RegisterApi("window.setPosition", WindowSetPosition);
    api::RegisterApi("window.setSize", WindowSetSize);
    api::RegisterApi("window.getDpiScale", WindowGetDpiScale);
    api::RegisterApi("window.flashTaskbar", WindowFlashTaskbar);
    api::RegisterApi("window.enterFullscreen", WindowEnterFullscreen);
    api::RegisterApi("window.exitFullscreen", WindowExitFullscreen);
    api::RegisterApi("window.setMica", WindowSetMica);
    api::RegisterApi("window.setMicaEffect", WindowSetMicaEffect);
    api::RegisterApi("window.setBlur", WindowSetBlur);
    api::RegisterApi("window.setAcrylic", WindowSetAcrylic);
    api::RegisterApi("window.setDarkMode", WindowSetDarkMode);
    api::RegisterApi("window.setBackgroundTransparency", WindowSetBackgroundTransparency);
    api::RegisterApi("window.refreshWebView", WindowRefreshWebView);
    api::RegisterApi("window.reload", WindowReload);
    api::RegisterApi("window.getDevServerConfig", WindowGetDevServerConfig);
    api::RegisterApi("window.setDevServerConfig", WindowSetDevServerConfig);
    api::RegisterApi("window.hasSavedBounds", WindowHasSavedBounds);
    api::RegisterApi("window.setZoom", WindowSetZoom);
    api::RegisterApi("window.getZoom", WindowGetZoom);
    api::RegisterApi("window.resetZoom", WindowResetZoom);
    api::RegisterApi("window.setZoomForDpi", WindowSetZoomForDpi);
    api::RegisterApi("window.createPopup", WindowCreatePopup);
    api::RegisterApi("window.closePopup", WindowClosePopup);
    api::RegisterApi("window.closeAllPopups", WindowCloseAllPopups);
    api::RegisterApi("window.getAllWindows", WindowGetAllWindows);
    api::RegisterApi("window.getCurrentWindowId", WindowGetCurrentWindowId);
    api::RegisterApi("window.getPopupBehavior", WindowGetPopupBehavior);
    api::RegisterApi("window.setPopupBehavior", WindowSetPopupBehavior);
    api::RegisterApi("window.getBackdropPolicy", WindowGetBackdropPolicy);
    api::RegisterApi("window.setBackdropPolicy", WindowSetBackdropPolicy);
    api::RegisterApi("window.cancelClose", WindowCancelClose);
    api::RegisterApi("window.confirmClose", WindowConfirmClose);
    api::RegisterApi("window.setFrameless", WindowSetFrameless);
    api::RegisterApi("window.setClickThrough", WindowSetClickThrough);
    api::RegisterApi("window.isClickThrough", WindowIsClickThrough);
    api::RegisterApi("window.setClickThroughExcludeRegions", WindowSetClickThroughExcludeRegions);
    api::RegisterApi("window.clearClickThroughExcludeRegions", WindowClearClickThroughExcludeRegions);
    api::RegisterApi("window.sendMessage", WindowSendMessage);
    api::RegisterApi("window.broadcast", WindowBroadcast);
    api::RegisterApi("window.getMode", WindowGetMode);
    api::RegisterApi("panel.getConfig", PanelGetConfig);
    api::RegisterApi("panel.setConfig", PanelSetConfig);
}
