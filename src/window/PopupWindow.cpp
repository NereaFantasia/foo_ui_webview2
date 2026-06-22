#include "pch.h"
#include "window/PopupWindow.h"
#include "window/WindowChromeResolver.h"
#include "window/WindowChromeTrace.h"
#include "window/WindowManager.h"
#include "core/WebViewContext.h"
#include "core/PreferencesPage.h"
#include "core/SecurityConfig.h"
#include "webview/WebViewHost.h"
#include "api/BridgeCore.h"
#include "api/HttpApi.h"
#include "api/PortHub.h"
#include "api/WindowApi.h"  // ExitFullscreenIfActive
#include "selection/SelectionWatcher.h"
#include <windowsx.h>  // For GET_X_LPARAM, GET_Y_LPARAM
#include <commctrl.h>   // For SetWindowSubclass / RemoveWindowSubclass
#include <wrl/event.h>  // For Microsoft::WRL::Callback (MoveFocusRequested handler)
#include <algorithm>
#include <cctype>
#include <sstream>  // For ostringstream (ActivationEvidence logging)
#include "utils/WindowUtils.h"

using namespace Microsoft::WRL;  // For Callback<> (MoveFocusRequested handler)

namespace {
    static BOOL S_DwmDefWindowProc(HWND h, UINT msg, WPARAM w, LPARAM l, LRESULT* r) {
        return ::DwmDefWindowProc(h, msg, w, l, r);
    }
    static HRESULT S_DwmExtendFrameIntoClientArea(HWND h, const MARGINS* m) {
        return ::DwmExtendFrameIntoClientArea(h, m);
    }
    static HRESULT S_DwmSetWindowAttribute(HWND h, DWORD a, LPCVOID d, DWORD s) {
        return ::DwmSetWindowAttribute(h, a, d, s);
    }

    // Windows 11: 窗口圆角偏好
    constexpr DWORD DWMWA_WINDOW_CORNER_PREFERENCE_V = 33;
    constexpr DWORD DWMWCP_DEFAULT_V = 0;
    constexpr DWORD DWMWCP_ROUND_V = 2;

    // Windows 11: 背景类型
    constexpr DWORD DWMWA_SYSTEMBACKDROP_TYPE_V = 38;
    constexpr DWORD DWMWA_USE_IMMERSIVE_DARK_MODE_V = 20;
    constexpr int DWMSBT_NONE_V = 1;
    constexpr int DWMSBT_MAINWINDOW_V = 2;      // Mica
    constexpr int DWMSBT_TRANSIENTWINDOW_V = 3; // Acrylic

    constexpr UINT WM_POPUP_RESTORE_FROM_MINIMIZE = WM_APP + 0x52;
    constexpr UINT WM_POPUP_UNDO_FOREGROUND = WM_APP + 0x55;

    // 别名 — 消除 PopupWindow / MainWindow / WindowChromeResolver 三文件重复
    inline std::string S_ToLower(std::string v) { return WindowUtils::ToLower(std::move(v)); }
    inline bool S_TryGetBool(const json& obj, const char* key, bool& out) {
        return WindowUtils::TryGetBool(obj, key, out);
    }

    inline std::string S_GetUserBackdropEffectString() {
        return WindowUtils::GetUserBackdropEffectString();
    }

    inline bool S_IsPluginManagedBackdropEffect(const std::string& effect) {
        return WindowUtils::IsPluginManagedBackdropEffect(effect);
    }
}

bool PopupWindow::classRegistered_ = false;

PopupWindow::PopupWindow() = default;

PopupWindow::~PopupWindow() {
    Destroy();
}

void PopupWindow::SetTitlebarHeight(int height) {
    if (height > 0) {
        titlebarHeight_ = height;
    }
}

void PopupWindow::SetDragRegions(const std::vector<DragRegion>& regions) {
    std::lock_guard<std::mutex> lock(regionsMutex_);
    dragRegions_ = regions;
}

void PopupWindow::ClearDragRegions() {
    std::lock_guard<std::mutex> lock(regionsMutex_);
    dragRegions_.clear();
}

void PopupWindow::SetNoDragRegions(const std::vector<DragRegion>& regions) {
    std::lock_guard<std::mutex> lock(regionsMutex_);
    noDragRegions_ = regions;
}

void PopupWindow::ClearNoDragRegions() {
    std::lock_guard<std::mutex> lock(regionsMutex_);
    noDragRegions_.clear();
}

bool PopupWindow::RegisterWindowClass() {
    if (classRegistered_) return true;
    
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = sizeof(PopupWindow*);
        wc.hInstance = core_api::get_my_instance();
        try { static_api_ptr_t<ui_control> fb_ui; wc.hIcon = fb_ui->get_main_icon(); wc.hIconSm = fb_ui->get_main_icon(); } catch (...) {}
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.lpszMenuName = nullptr;
        wc.lpszClassName = CLASS_NAME;
    
    if (!RegisterClassExW(&wc)) {
        DWORD error = GetLastError();
        if (error != ERROR_CLASS_ALREADY_EXISTS) {
            console::printf("[PopupWindow] Failed to register window class, error: %u", error);
            return false;
        }
    }
    
    classRegistered_ = true;
    return true;
}

HWND PopupWindow::Create(const CreateParams& params, const std::string& windowId, HWND ownerHwnd) {
    if (!RegisterWindowClass()) {
        return nullptr;
    }
    
    createParams_ = params;
    ownerHwnd_ = ownerHwnd;
    InitializePolicyState();
    
    // 设置 windowId（基类 WebViewPanel 字段）
    SetWindowId(windowId);
    
    // 计算窗口位置
    int x = params.x;
    int y = params.y;
    
    if (x == CW_USEDEFAULT || y == CW_USEDEFAULT) {
        // 居中于屏幕
        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);
        if (x == CW_USEDEFAULT) x = (screenWidth - params.width) / 2;
        if (y == CW_USEDEFAULT) y = (screenHeight - params.height) / 2;
    }
    
    // 判断是否使用 WS_POPUP 模式（frame=false + transparent + 无 DWM 效果）
    // 用于桌面歌词等需要完全无边框/无阴影的场景
    usePopupStyle_ = !params.frame && params.transparent &&
        resolvedBackdropPolicy_.activeEffect == "none" &&
        resolvedBackdropPolicy_.inactiveEffect == "none";

    // 窗口样式
    DWORD style;
    if (usePopupStyle_) {
        style = WS_POPUP | WS_CLIPCHILDREN;
    } else {
        style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
        if (!params.resizable) {
            style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
        }
    }
    
    DWORD exStyle = WS_EX_NOREDIRECTIONBITMAP;
    ApplyResolvedBehaviorToStyles(style, exStyle);
    if (params.alwaysOnTop) {
        exStyle |= WS_EX_TOPMOST;
    }
    
    // 初始化鼠标穿透状态 — 窗口创建前仅标记，窗口创建后通过 SetClickThrough 应用扩展样式
    // （不在此处设置 clickThrough_，而是在创建后调用 SetClickThrough 统一处理）
    
    // 窗口标题
    std::wstring wTitle;
    if (!params.title.empty()) {
        wTitle = pfc::stringcvt::string_wide_from_utf8(params.title.c_str()).get_ptr();
    } else {
        wTitle = L"Popup";
    }
    
    HWND owner = nullptr;
    if (resolvedBehavior_.owner == "main" && ownerHwnd_ && IsWindow(ownerHwnd_)) {
        owner = ownerHwnd_;
    } else if (resolvedBehavior_.noActivate || params.clickThrough) {
        // overlay / noActivate popup：以隐藏 activation sink 为 owner，
        // 把「拒绝激活后的回退目标」从主窗口改为不可感知的 sink，
        // 避免 WebView2 内部 SetFocus 激活本窗口后主窗口被前置覆盖外部应用。
        owner = WindowManager::GetInstance().GetActivationSinkHwnd();
    }

    // 默认无 owner：允许 popup 置于主窗口后方（行为与当前版本一致）
    hwnd_ = CreateWindowExW(
        exStyle,
        CLASS_NAME,
        wTitle.c_str(),
        style,
        x, y,
        params.width, params.height,
        owner,
        nullptr,
        core_api::get_my_instance(),
        this                 // Pass this pointer
    );
    
    if (!hwnd_) {
        console::printf("[PopupWindow] Failed to create window, error: %u", GetLastError());
        return nullptr;
    }
    
    // frame=false 时触发 SWP_FRAMECHANGED，使 WM_NCCALCSIZE 生效
    if (!params.frame) {
        SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0,
            SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
    
    // 启动隐藏：不立即显示，等 WebView 页面首帧后再 reveal
    startupRevealPending_ = true;
    startupRevealCommitted_ = false;
    
    // 应用初始鼠标穿透状态（需在窗口创建后设置 WS_EX_TRANSPARENT 扩展样式）
    if (params.clickThrough) {
        SetClickThrough(true);
    }
    
    FB2K_console_formatter() << "[PopupWindow] Created: id=" 
        << GetWindowId().c_str() << ", hwnd=0x" << pfc::format_hex((size_t)hwnd_) 
        << ", size=" << params.width << "x" << params.height;
    
    return hwnd_;
}

void PopupWindow::Destroy() {
    // 取消挂起的关闭计时器
    if (pendingClose_ && hwnd_) {
        KillTimer(hwnd_, CLOSE_TIMER_ID);
        pendingClose_ = false;
    }
    
    // 不手动 webView_.reset()，让 DestroyWindow → WM_DESTROY → OnDestroy → DestroyWebView
    // 正确处理注销+释放顺序，避免 WebViewContext 中出现悬空指针
    if (hwnd_ && IsWindow(hwnd_)) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

// ============================================
// 窗口过程
// ============================================

LRESULT CALLBACK PopupWindow::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    PopupWindow* pThis = nullptr;
    
    if (msg == WM_NCCREATE) {
        CREATESTRUCTW* pCreate = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = reinterpret_cast<PopupWindow*>(pCreate->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        pThis->hwnd_ = hwnd;
    } else {
        pThis = reinterpret_cast<PopupWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    
    if (pThis) {
        return pThis->HandleMessage(msg, wParam, lParam);
    }
    
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT PopupWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_SYSCOMMAND && resolvedBehavior_.keepVisibleOnShowDesktop && (wParam & 0xFFF0) == SC_MINIMIZE) {
        json data = {
            {"windowId", GetWindowId()},
            {"reason", "policy.keepVisibleOnShowDesktop"}
        };
        WebViewContext::GetInstance().BroadcastEvent("window:minimizeSuppressed", data);
        return 0;
    }

    // 全屏状态下的 maximize/restore 优先退出全屏（参考 Electron / Tauri）。
    // 覆盖路径：双击标题栏、Alt+Space 系统菜单、以及任何 PostMessage(WM_SYSCOMMAND, ...)。
    // API handler 已在 PostMessage 前调用 ExitFullscreenIfActive，到达此处时已不在全屏。
    if (msg == WM_SYSCOMMAND) {
        const UINT cmd = static_cast<UINT>(wParam) & 0xFFF0;
        if ((cmd == SC_MAXIMIZE || cmd == SC_RESTORE) && IsFullscreen()) {
            ExitFullscreenIfActive(hwnd_);
            if (cmd == SC_RESTORE) {
                return 0;
            }
            // SC_MAXIMIZE 让后续处理继续走：ExitFullscreen 已恢复 maximize 时为 no-op
        }
    }

    // 非客户区按下左键：全屏时先退出全屏再继续 move/resize。
    // 覆盖路径：
    //   - HTCAPTION 拖动（系统标题栏 / HandleDragAreaMouse 合成 / window.startDrag API）
    //   - HTLEFT/HTRIGHT/HTTOP/HTBOTTOM/HT*CORNER 边缘 resize（window.startResize API、第三方 SendMessage）
    // 必须显式 ExitFullscreenMode 才能清 WS_EX_TOPMOST 并广播 window:stateChanged。
    // 参考 Chrome / Edge / Electron / Tauri 行为。
    if (msg == WM_NCLBUTTONDOWN) {
        std::ostringstream extra;
        extra << "hitTest=" << wParam
              << " isFullscreen=" << WindowChromeTrace::BoolText(IsFullscreen());
        EmitActivationEvidence("WM_NCLBUTTONDOWN", extra.str());
    }
    if (msg == WM_NCLBUTTONDOWN && IsFullscreen()) {
        switch (wParam) {
            case HTCAPTION:
            case HTLEFT: case HTRIGHT: case HTTOP: case HTBOTTOM:
            case HTTOPLEFT: case HTTOPRIGHT:
            case HTBOTTOMLEFT: case HTBOTTOMRIGHT:
                ExitFullscreenIfActive(hwnd_);
                // 不 return：让后续 DefWindowProc 接管对应 modal loop
                // (HTCAPTION → SC_MOVE, HT*Edge → SC_SIZE)。
                break;
            default:
                break;
        }
    }

    // frame=false 时，阻止 DWM / UxTheme 在合成层绘制原生标题栏按钮（三大键）
    if (!createParams_.frame) {
        switch (msg) {
            case WM_NCACTIVATE: {
                EmitActivationEvidence("WM_NCACTIVATE",
                    std::string("active=") + WindowChromeTrace::BoolText(wParam != FALSE));
                // lParam = -1 告诉 DefWindowProc 仅更新激活状态，不重绘非客户区
                return DefWindowProcW(hwnd_, msg, wParam, -1);
            }
            case WM_NCPAINT:
                return 0;
            default:
                break;
        }
    }

    // frame=false 时，让 DWM 处理部分消息（但 WM_NCHITTEST 除外）
    if (!createParams_.frame && msg != WM_NCHITTEST) {
        LRESULT dwmResult;
        if (S_DwmDefWindowProc(hwnd_, msg, wParam, lParam, &dwmResult)) {
            return dwmResult;
        }
    }
    
    switch (msg) {
        case WM_NCCALCSIZE:
            if (wParam == TRUE && IsFullscreen()) {
                // 全屏: 整个窗口都是客户区，无论 frame 状态
                return 0;
            }
            // frame=false: 移除非客户区（标题栏+边框），最大化时调整边距
            if (!createParams_.frame && wParam == TRUE) {
                auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
                WINDOWPLACEMENT wp = { sizeof(wp) };
                GetWindowPlacement(hwnd_, &wp);
                if (wp.showCmd != SW_SHOWMAXIMIZED)
                    return 0;
                int frameX = GetSystemMetrics(SM_CXFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
                int frameY = GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
                params->rgrc[0].left += frameX;
                params->rgrc[0].right -= frameX;
                params->rgrc[0].top += frameY;
                params->rgrc[0].bottom -= frameY;
                return 0;
            }
            break;
            
        case WM_NCHITTEST:
            if (!createParams_.frame || clickThrough_)
                return OnNcHitTest(lParam);
            break;
        
        case WM_CREATE:
            OnCreate();
            return 0;

        case WM_ACTIVATE: {
            // noActivate / overlay 意图：WebView2 内部调用 SetForegroundWindow，
            // 完全绕过 WM_MOUSEACTIVATE → MA_NOACTIVATE 拦截。
            // WM_ACTIVATE 是通知（激活已发生），必须主动撤销。
            const WORD activationKind = LOWORD(wParam);
            const bool minimized = HIWORD(wParam) != 0;
            const HWND otherHwnd = (HWND)lParam;
            {
                std::ostringstream extra;
                extra << "wParam.kind=" << activationKind
                      << " minimized=" << WindowChromeTrace::BoolText(minimized)
                      << " otherHwnd=0x" << pfc::format_hex((size_t)otherHwnd);
                EmitActivationEvidence("WM_ACTIVATE", extra.str());
            }
            if (activationKind != WA_INACTIVE &&
                (resolvedBehavior_.noActivate || overlayIntent_)) {
                EmitActivationEvidence("WM_ACTIVATE.undoScheduled",
                    std::string("wParam.kind=") + std::to_string(activationKind));
                // 同步调 SetForegroundWindow 在 WM_ACTIVATE 内部不可靠（系统正在处理激活链）。
                // 改为 PostMessage 异步还原：等激活链完成后再调。
                PostMessage(hwnd_, WM_POPUP_UNDO_FOREGROUND, 0,
                    (LPARAM)lastForegroundBeforeClick_);
                // 不调用 OnActivate — 保持 popup 的 inactive 语义
                return 0;
            }
            // 第二波防御：WebView2 的 SetFocus 分两波完成——第二波把 popup 推到前台后
            // 立即弹回 fb2k 主窗口（实测 otherHwnd=主窗口），主窗口随之覆盖外部应用。
            // 在 popup 把激活交给主窗口时再次把激活引到不可感知的 sink。
            if (activationKind == WA_INACTIVE &&
                (resolvedBehavior_.noActivate || overlayIntent_) &&
                otherHwnd && otherHwnd == core_api::get_main_window()) {
                std::ostringstream e2;
                e2 << "otherHwnd=0x" << pfc::format_hex((size_t)otherHwnd);
                EmitActivationEvidence("WM_ACTIVATE.undoScheduled.inactiveToMain", e2.str());
                PostMessage(hwnd_, WM_POPUP_UNDO_FOREGROUND, 0,
                    (LPARAM)lastForegroundBeforeClick_);
            }
            if (menuDismissCallback_ && activationKind == WA_INACTIVE) {
                menuDismissCallback_("blur");
            }
            OnActivate(wParam);
            break;
        }

        case WM_ACTIVATEAPP: {
            std::ostringstream extra;
            extra << "active=" << WindowChromeTrace::BoolText(wParam != FALSE)
                  << " otherThreadId=" << (unsigned long)lParam;
            EmitActivationEvidence("WM_ACTIVATEAPP", extra.str());
            if (menuDismissCallback_ && wParam == FALSE) {
                menuDismissCallback_("blur");
            }
            break;  // fall through to DefWindowProc
        }

        case WM_SETFOCUS: {
            std::ostringstream extra;
            extra << "prevFocusHwnd=0x" << pfc::format_hex((size_t)(HWND)wParam);
            EmitActivationEvidence("WM_SETFOCUS", extra.str());
            break;  // fall through to DefWindowProc
        }

        case WM_KILLFOCUS: {
            std::ostringstream extra;
            extra << "nextFocusHwnd=0x" << pfc::format_hex((size_t)(HWND)wParam);
            EmitActivationEvidence("WM_KILLFOCUS", extra.str());
            break;  // fall through to DefWindowProc
        }

        case WM_POPUP_UNDO_FOREGROUND: {
            HWND hRestore = (HWND)lParam;
            HWND mainHwnd = core_api::get_main_window();
            const bool restoreValid = hRestore && IsWindow(hRestore);
            {
                std::ostringstream extra;
                extra << "restoreTo=0x" << pfc::format_hex((size_t)hRestore)
                      << " mainHwnd=0x" << pfc::format_hex((size_t)mainHwnd)
                      << " restoreValid=" << WindowChromeTrace::BoolText(restoreValid);
                EmitActivationEvidence("WM_POPUP_UNDO_FOREGROUND.enter", extra.str());
            }
            // 策略 0（主）：把激活引到不可感知的 activation sink（同进程，可靠）。
            // 实测证实：系统拒绝激活 overlay popup 后的回退目标取决于「最近活动 / z-order」，
            // 会落到 fb2k 主窗口并覆盖外部应用。先让 sink 成为最近活动窗口，
            // 使后续回退 / 二次夺焦落到 sink 而非主窗口。sink 屏幕外全透明，用户不可感知。
            HWND sink = WindowManager::GetInstance().GetActivationSinkHwnd();
            if (sink && IsWindow(sink) && sink != hwnd_) {
                BOOL sinkOk = SetForegroundWindow(sink);
                EmitActivationEvidence("WM_POPUP_UNDO_FOREGROUND.setForegroundSink",
                    std::string("setForegroundOk=") + WindowChromeTrace::BoolText(sinkOk != FALSE));
            }
            // 策略 1：直接将主窗口 z-order 压回外部窗口之后。
            // 这是对自身进程窗口的操作，不受 SetForegroundWindow 权限限制。
            // 即使主窗口已被系统激活，z-order 变更仍可正常执行。
            // 跨 band 的相对插入必然改写主窗口 WS_EX_TOPMOST，一律跳过：
            // 升段（参照 topmost、主窗口非 topmost）= 隐式授予置顶 → 意外覆盖全桌面；
            // 降段（参照非 topmost、主窗口 topmost）= 用户显式 alwaysOnTop 被悄悄剥除。
            // 同 band 内移动不改置顶状态，安全执行。升段场景下主窗口位于非 topmost
            // 段顶部，天然已排在整个 topmost 段之后，跳过即正确；其余残差由
            // MainWindow 的 WM_WINDOWPOSCHANGED 期望置顶守护兜底。
            if (mainHwnd && restoreValid
                && mainHwnd != hwnd_ && mainHwnd != hRestore) {
                const bool refTopmost =
                    (GetWindowLongW(hRestore, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
                const bool mainTopmost =
                    (GetWindowLongW(mainHwnd, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
                if (refTopmost != mainTopmost) {
                    EmitActivationEvidence(
                        "WM_POPUP_UNDO_FOREGROUND.pushMainBehind.skipCrossBandRef",
                        std::string("refTopmost=") + WindowChromeTrace::BoolText(refTopmost) +
                        " mainTopmost=" + WindowChromeTrace::BoolText(mainTopmost));
                } else {
                    BOOL pushOk = SetWindowPos(mainHwnd, hRestore, 0, 0, 0, 0,
                        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                    EmitActivationEvidence("WM_POPUP_UNDO_FOREGROUND.pushMainBehind",
                        std::string("setWindowPosOk=") + WindowChromeTrace::BoolText(pushOk != FALSE));
                }
            }
            // 策略 2：尝试将前台还给外部窗口（可能被系统拒绝，策略 1 是保底）
            if (restoreValid && hRestore != hwnd_) {
                BOOL fgOk = SetForegroundWindow(hRestore);
                EmitActivationEvidence("WM_POPUP_UNDO_FOREGROUND.setForeground",
                    std::string("setForegroundOk=") + WindowChromeTrace::BoolText(fgOk != FALSE));
            }
            // 确保 popup 保持 TOPMOST（视觉上仍置顶），但不激活
            SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            EmitActivationEvidence("WM_POPUP_UNDO_FOREGROUND.exit");
            return 0;
        }

        case WM_MOUSEACTIVATE: {
            // 保存当前前台窗口，供 WM_ACTIVATE 还原使用。
            // activation sink 不能作为还原参照：sink 是不可见的 topmost 窗口，
            // 前台落在 sink 说明上一轮撤销已把焦点停在 sink（本就无外部窗口可还）。
            // 若把 sink 当作「外部应用」参照，撤销路径的 SetWindowPos(main, sink)
            // 会把主窗口插进 topmost 段——按 Win32 z-order 规则，位置高于任何现存
            // topmost 窗口的窗口会被隐式授予 WS_EX_TOPMOST——导致主窗口意外置顶
            // （偶发置顶 bug 的 sink 反馈环）。
            {
                HWND fgBefore = GetForegroundWindow();
                HWND sinkHwnd = WindowManager::GetInstance().GetActivationSinkHwnd();
                const bool fgIsSink = fgBefore && sinkHwnd && fgBefore == sinkHwnd;
                lastForegroundBeforeClick_ = fgIsSink ? nullptr : fgBefore;
                std::ostringstream extra;
                extra << "hitTest=" << LOWORD(lParam)
                      << " mouseMsg=" << HIWORD(lParam)
                      << " topLevelHwnd=0x" << pfc::format_hex((size_t)(HWND)wParam)
                      << " fgBefore=0x" << pfc::format_hex((size_t)fgBefore)
                      << " sinkFiltered=" << WindowChromeTrace::BoolText(fgIsSink);
                EmitActivationEvidence("WM_MOUSEACTIVATE.enter", extra.str());
            }
            // 所有 popup 被点击时通知 WindowManager：WebView2 Visual Hosting 模式下，
            // Chromium 处理 mousedown 会调 SetForegroundWindow → 主窗口被无意激活。
            // 通知 WindowManager 使主窗口能识别这种"无意激活"并抑制它，
            // 把前台还给外部应用 / sink，避免主窗口覆盖用户当前操作的程序。
            // 不限于 overlay/noActivate popup——miniplayer 等普通 popup 也需要保护。
            WindowManager::GetInstance().NotifyOverlayInteraction(lastForegroundBeforeClick_);
            // 穿透模式：非热区点击不激活窗口（仅对 inactive 窗口有效的首次激活防御）
            if (clickThrough_ && !clickThroughExcludeRegions_.empty()) {
                POINT pt;
                GetCursorPos(&pt);
                ScreenToClient(hwnd_, &pt);
                if (!IsPointInExcludeRegion(pt)) {
                    EmitActivationEvidence("WM_MOUSEACTIVATE.return",
                        "result=MA_NOACTIVATEANDEAT (outside exclude region)");
                    return MA_NOACTIVATEANDEAT;
                }
                // 热区点击：消息正常投递（按钮可用），但不激活 popup，
                // 避免 owned-window 语义把 owner（主窗口）拉到前台。
                EmitActivationEvidence("WM_MOUSEACTIVATE.return",
                    "result=MA_NOACTIVATE (in exclude region)");
                return MA_NOACTIVATE;
            }
            // noActivate 模式（如 desktopLyrics profile）：返回 MA_NOACTIVATE，
            // 阻止 popup 因鼠标点击被激活，从而避免 Win32 owned-window 语义
            // 把 owner（主窗口）连带拉到前台。
            // 单独的 WS_EX_NOACTIVATE 不阻止 WM_ACTIVATE / WM_MOUSEACTIVATE，
            // 必须在此显式拦截。MA_NOACTIVATE 不会吞掉鼠标消息，
            // WM_LBUTTONDOWN 等仍正常投递，按钮可用。
            if (resolvedBehavior_.noActivate) {
                EmitActivationEvidence("WM_MOUSEACTIVATE.return",
                    "result=MA_NOACTIVATE (noActivate)");
                return MA_NOACTIVATE;
            }
            // overlay 意图：创建时指定了 clickThrough（表明是 overlay 类窗口），
            // 即使运行时临时关闭穿透（为了让按钮可用），仍不应被鼠标激活。
            // "是否激活"与"是否穿透鼠标"是正交维度（参考 Win32 overlay 工具惯例）。
            if (createParams_.clickThrough || overlayIntent_) {
                EmitActivationEvidence("WM_MOUSEACTIVATE.return",
                    "result=MA_NOACTIVATE (overlay intent)");
                return MA_NOACTIVATE;
            }
            EmitActivationEvidence("WM_MOUSEACTIVATE.return",
                "result=MA_ACTIVATE (default)");
            break;
        }
            
        case WM_CLOSE:
            OnClose();
            return 0;
            
        case WM_DESTROY:
            OnDestroy();
            return 0;

        case WM_SHOWWINDOW: {
            std::string extra = std::string("show=") + WindowChromeTrace::BoolText(wParam != FALSE) +
                " status=" + std::to_string((long long)lParam);
            TraceWindowPhase("WM_SHOWWINDOW", nullptr, nullptr, nullptr, nullptr, extra.c_str());
            if (wParam != FALSE && !isMinimized_ &&
                !startupRevealPending_ && startupRevealCommitted_ && !startupRevealSettling_) {
                ApplyResolvedChrome(true);
            }
            break;
        }

        case WM_WINDOWPOSCHANGING: {
            WINDOWPOS* wp = reinterpret_cast<WINDOWPOS*>(lParam);
            if (wp && resolvedBehavior_.owner == "main" && ownerHwnd_ && IsWindow(ownerHwnd_)) {
                // 阻止 Win32 owned-window 语义在 popup 被激活或移动时
                // 连带改变 owner（主窗口）的 z-order，
                // 避免「点击浮动窗口 → 主窗口被唤出」问题
                wp->flags |= SWP_NOOWNERZORDER;
            }
            break;
        }

        case WM_WINDOWPOSCHANGED: {
            const WINDOWPOS* windowPos = reinterpret_cast<const WINDOWPOS*>(lParam);
            std::string extra;
            if (windowPos) {
                extra = std::string("flags=") + std::to_string(windowPos->flags) +
                    " x=" + std::to_string(windowPos->x) +
                    " y=" + std::to_string(windowPos->y) +
                    " cx=" + std::to_string(windowPos->cx) +
                    " cy=" + std::to_string(windowPos->cy);
            } else {
                extra = "windowPos=null";
            }
            TraceWindowPhase("WM_WINDOWPOSCHANGED", nullptr, nullptr, nullptr, nullptr, extra.c_str());
            
            // WebView2 Composition 模式要求: 父窗口位置变化时通知控制器，
            // 否则 WebView2 内部输入窗口不跟随移动，产生「幽灵窗口」阻断桌面交互。
            if (webView_ && webView_->GetController()) {
                webView_->GetController()->NotifyParentWindowPositionChanged();
            }
            break;
        }
            
        case WM_SIZE: {
            if (wParam == SIZE_MINIMIZED && resolvedBehavior_.keepVisibleOnShowDesktop) {
                std::string extra = std::string("sizeType=minimized-suppressed width=") +
                    std::to_string(LOWORD(lParam)) +
                    " height=" + std::to_string(HIWORD(lParam));
                TraceWindowPhase("WM_SIZE", nullptr, nullptr, nullptr, nullptr, extra.c_str());
                if (!restoreFromMinimizePending_) {
                    restoreFromMinimizePending_ = true;
                    PostMessageW(hwnd_, WM_POPUP_RESTORE_FROM_MINIMIZE, 0, 0);
                }
                return 0;
            }
            const bool wasMaximized = isMaximized_;
            const bool wasMinimized = isMinimized_;
            std::string sizeType = "restored";

            switch (wParam) {
                case SIZE_MINIMIZED:
                    isMaximized_ = false;
                    isMinimized_ = true;
                    sizeType = "minimized";
                    break;
                case SIZE_MAXIMIZED:
                    isMaximized_ = true;
                    isMinimized_ = false;
                    sizeType = "maximized";
                    break;
                default:
                    isMaximized_ = false;
                    isMinimized_ = false;
                    sizeType = "restored";
                    break;
            }

            std::string extra = std::string("sizeType=") + sizeType +
                " width=" + std::to_string(LOWORD(lParam)) +
                " height=" + std::to_string(HIWORD(lParam));
            TraceWindowPhase("WM_SIZE", nullptr, nullptr, nullptr, nullptr, extra.c_str());

            if (!createParams_.frame && wasMaximized != isMaximized_) {
                SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0,
                    SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            }

            if (wParam == SIZE_MINIMIZED) {
                // 不手动隐藏 WebView — WebView2 运行时自动处理
                return 0;
            }

            // wasMinimized 恢复时不需手动恢复可见性，OnSize Resize 会触发布局更新

            OnSize(LOWORD(lParam), HIWORD(lParam));
            if (wasMinimized && !startupRevealPending_ && startupRevealCommitted_ && !startupRevealSettling_) {
                ApplyBackdropForActivation(isActive_, true);
            }
            return 0;
        }

        case WM_POPUP_RESTORE_FROM_MINIMIZE:
            restoreFromMinimizePending_ = false;
            ShowWindow(hwnd_, resolvedBehavior_.noActivate ? SW_SHOWNOACTIVATE : SW_RESTORE);
            // Win+D 恢复后重新确认 topmost z-order，
            // 避免 Show Desktop 动画导致 popup 落入 non-topmost 层
            if (GetWindowLongW(hwnd_, GWL_EXSTYLE) & WS_EX_TOPMOST) {
                SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            }
            return 0;
            
        case WM_GETMINMAXINFO: {
            auto* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
            mmi->ptMinTrackSize.x = createParams_.minWidth;
            mmi->ptMinTrackSize.y = createParams_.minHeight;
            if (createParams_.maxWidth > 0) {
                mmi->ptMaxTrackSize.x = createParams_.maxWidth;
            }
            if (createParams_.maxHeight > 0) {
                mmi->ptMaxTrackSize.y = createParams_.maxHeight;
            }
            return 0;
        }
        
        case WM_TIMER:
            if (wParam == CLOSE_TIMER_ID) {
                // beforeClose 超时，强制关闭
                KillTimer(hwnd_, CLOSE_TIMER_ID);
                pendingClose_ = false;
                console::printf("[PopupWindow] beforeClose timeout, force closing: %s",
                               GetWindowId().c_str());
                DestroyWindow(hwnd_);
            }
            else if (wParam == HOVER_TIMER_ID) {
                // 鼠标穿透 hover 检测：只在 clickThrough_ 期间轮询光标位置
                if (clickThrough_) {
                    POINT cursor;
                    GetCursorPos(&cursor);
                    RECT rc;
                    GetWindowRect(hwnd_, &rc);
                    bool hovering = PtInRect(&rc, cursor) != FALSE;
                    if (hovering != wasHovering_) {
                        wasHovering_ = hovering;
                        json data = {
                            {"windowId", GetWindowId()},
                            {"hovering", hovering}
                        };
                        WebViewContext::GetInstance().BroadcastEvent("window:hoverStateChanged", data);
                    }
                    // 刷新 exclude region 有效穿透
                    UpdateClickThroughEffective();
                } else {
                    // 已退出穿透模式，停止计时器
                    KillTimer(hwnd_, HOVER_TIMER_ID);
                    wasHovering_ = false;
                }
            }
            else if (wParam == STARTUP_REVEAL_TIMER_ID) {
                KillTimer(hwnd_, STARTUP_REVEAL_TIMER_ID);
                console::printf("[PopupWindow] Startup reveal fallback timer fired: %s",
                               GetWindowId().c_str());
                if (!startupRevealCommitted_) {
                    TryCommitStartupReveal();
                }
            }
            return 0;

        case WM_DEFERRED_CHROME_REAPPLY:
            if (needsAuthoritativeChromeReapply_ && !isMinimized_ && IsWindowVisible(hwnd_)) {
                needsAuthoritativeChromeReapply_ = false;
                EnsureAuthoritativeNativeChrome("deferred-post-reveal");
                PostMessage(hwnd_, WM_DEFERRED_SURFACE_CONVERGE, 0, 0);
            } else {
                needsAuthoritativeChromeReapply_ = false;
            }
            return 0;

        case WM_DEFERRED_SURFACE_CONVERGE:
            LogSurfaceDiagnostics("WM_DEFERRED_SURFACE_CONVERGE.enter");
            EnsureSurfaceConvergedAfterNativeFrame("deferred-surface-converge");
            return 0;
            
        case WM_ERASEBKGND:
            return FALSE;
            
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd_, &ps);
            EndPaint(hwnd_, &ps);
            return 0;
        }
            
        case WM_KEYDOWN:
            if (menuDismissCallback_ && wParam == VK_ESCAPE) {
                menuDismissCallback_("escape");
                return 0;
            }
            // F12 开发者工具（仅开发服务器模式）
            if (wParam == VK_F12 && webView_ && webView_->IsReady()) {
                if (security_config::UseDevServer()) {
                    webView_->OpenDevTools();
                }
            }
            // F5 刷新
            else if (wParam == VK_F5 && webView_ && webView_->IsReady()) {
                webView_->Reload();
            }
            break;
        
        // ============================================
        // Visual Hosting 模式：客户区光标同步
        // 与 MainWindow 同源逻辑 (详见 MainWindow.cpp 中相同 case 注释)。
        // 边缘 / 拖拽区由 OnNcHitTest 通过 HTCAPTION/HT*Edge 上报, 已经
        // 在到达此处之前由 DefWindowProc 处理掉, 不会落到 HTCLIENT 分支。
        // ============================================
        case WM_SETCURSOR:
            if (LOWORD(lParam) == HTCLIENT && webView_ &&
                webView_->ApplyCurrentCursor()) {
                return TRUE;
            }
            break;

            // ============================================
            // Visual Hosting 模式：鼠标消息转发
            // frame=false 时先拦截拖拽区域的关键事件，避免
            // HandleMouseMessage 中 SetCapture 抢占系统拖拽
            // ============================================
        case WM_ENTERSIZEMOVE: {
            EmitActivationEvidence("WM_ENTERSIZEMOVE");
            break;  // fall through to DefWindowProc
        }
        case WM_EXITSIZEMOVE: {
            EmitActivationEvidence("WM_EXITSIZEMOVE");
            break;  // fall through to DefWindowProc
        }
        case WM_CAPTURECHANGED: {
            std::ostringstream extra;
            extra << "newCaptureHwnd=0x" << pfc::format_hex((size_t)(HWND)lParam)
                  << " wasOverlayDragging=" << WindowChromeTrace::BoolText(isOverlayDragging_);
            EmitActivationEvidence("WM_CAPTURECHANGED", extra.str());
            // 自定义拖拽期间被外部抢走 capture（罕见）：兑现重置状态，
            // 避免后续 WM_MOUSEMOVE 仍当作拖拽处理造成窗口跳动。
            if (isOverlayDragging_) {
                isOverlayDragging_ = false;
            }
            break;  // fall through to DefWindowProc
        }

        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_MBUTTONDBLCLK:
        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
        case WM_XBUTTONDOWN:
        case WM_XBUTTONUP:
        case WM_XBUTTONDBLCLK:
        case WM_MOUSELEAVE:
            // -- ActivationEvidence: 仅对按键 down/up 输出，避免 WM_MOUSEMOVE 洪泛 --
            if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP ||
                msg == WM_RBUTTONDOWN || msg == WM_RBUTTONUP ||
                msg == WM_MBUTTONDOWN || msg == WM_MBUTTONUP) {
                const char* phase = "WM_BUTTON";
                switch (msg) {
                    case WM_LBUTTONDOWN: phase = "WM_LBUTTONDOWN"; break;
                    case WM_LBUTTONUP:   phase = "WM_LBUTTONUP"; break;
                    case WM_RBUTTONDOWN: phase = "WM_RBUTTONDOWN"; break;
                    case WM_RBUTTONUP:   phase = "WM_RBUTTONUP"; break;
                    case WM_MBUTTONDOWN: phase = "WM_MBUTTONDOWN"; break;
                    case WM_MBUTTONUP:   phase = "WM_MBUTTONUP"; break;
                    default: break;
                }
                std::ostringstream extra;
                extra << "x=" << GET_X_LPARAM(lParam)
                      << " y=" << GET_Y_LPARAM(lParam)
                      << " wParam=0x" << pfc::format_hex((size_t)wParam);
                EmitActivationEvidence(phase, extra.str());
            }
            // -- 穿透模式前置过滤：非热区鼠标消息一律吞掉 --
            // 自定义拖拽期间跳过此过滤：光标可能被拖出热区，
            // 但 SetCapture 保证 mouse move/up 必须达到 popup 才能继续拖拽 / 释放。
            if (clickThrough_ && !clickThroughExcludeRegions_.empty() && msg != WM_MOUSELEAVE
                && !isOverlayDragging_) {
                POINT ctPt;
                if (msg == WM_MOUSEWHEEL || msg == WM_MOUSEHWHEEL) {
                    // 滚轮消息的坐标是屏幕坐标，需要转换
                    ctPt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                    ScreenToClient(hwnd_, &ctPt);
                } else {
                    ctPt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                }
                if (!IsPointInExcludeRegion(ctPt)) {
                    // 非热区：吞掉消息，立刻尝试恢复穿透
                    UpdateClickThroughEffective();
                    return 0;
                }
            }
            // -- 正常路径 --
            if (HandleDragAreaMouse(msg, wParam, lParam))
                return 0;
            if (webView_ && webView_->GetCompositionController() && webView_->HandleMouseMessage(msg, wParam, lParam)) {
                return 0;
            }
            break;
    }
    
    return DefWindowProcW(hwnd_, msg, wParam, lParam);
}

// WM_NCHITTEST 处理（frame=false 模式）
LRESULT PopupWindow::OnNcHitTest(LPARAM lParam) {
    POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
    RECT rcWindow;
    GetWindowRect(hwnd_, &rcWindow);

    // 鼠标穿透模式：根据热区判断是否穿透
    if (clickThrough_) {
        if (!clickThroughExcludeRegions_.empty()) {
            // 有热区：检查光标是否在热区内
            POINT clientPt = pt;
            ScreenToClient(hwnd_, &clientPt);
            if (!IsPointInExcludeRegion(clientPt)) {
                // 非热区：穿透（同线程窗口兜底）
                return HTTRANSPARENT;
            }
            // 热区内：fall through 到 resize/drag 正常判定
        } else {
            // 无热区：完整穿透（保持原有行为）
            SetTimer(hwnd_, HOVER_TIMER_ID, HOVER_POLL_MS, nullptr);
            return HTTRANSPARENT;
        }
    }

    // 调整大小边缘（仅 resizable 且非最大化时）
    if (createParams_.resizable && !isMaximized_) {
        const int CORNER = 8, EDGE = 4;
        bool cT = pt.y < rcWindow.top + CORNER;
        bool cB = pt.y >= rcWindow.bottom - CORNER;
        bool cL = pt.x < rcWindow.left + CORNER;
        bool cR = pt.x >= rcWindow.right - CORNER;
        if (cT && cL) return HTTOPLEFT;
        if (cT && cR) return HTTOPRIGHT;
        if (cB && cL) return HTBOTTOMLEFT;
        if (cB && cR) return HTBOTTOMRIGHT;
        bool eT = pt.y < rcWindow.top + EDGE;
        bool eB = pt.y >= rcWindow.bottom - EDGE;
        bool eL = pt.x < rcWindow.left + EDGE;
        bool eR = pt.x >= rcWindow.right - EDGE;
        if (eT) return HTTOP;
        if (eB) return HTBOTTOM;
        if (eL) return HTLEFT;
        if (eR) return HTRIGHT;
    }

    POINT clientPt = pt;
    ScreenToClient(hwnd_, &clientPt);

    // 拖拽区域不再返回 HTCAPTION（避免 DWM 截获鼠标消息导致 WebView2 丢失 hover）
    // 统一返回 HTCLIENT，拖拽由 HandleDragAreaMouse 通过 WM_NCLBUTTONDOWN 启动
    return HTCLIENT;
}

// frame=false 拖拽区域鼠标拦截
bool PopupWindow::HandleDragAreaMouse(UINT msg, WPARAM /*wParam*/, LPARAM lParam) {
    if (!webView_ || !webView_->IsReady())
        return false;

    // -- 自定义拖拽循环（overlay popup 专用）--
    // 一旦进入 isOverlayDragging_ 状态，所有 WM_MOUSEMOVE / WM_LBUTTONUP 走此分支，
    // 完全绕开 DefWindowProc 的 SC_MOVE 路径，避免内部 SetForegroundWindow 触发激活链。
    if (isOverlayDragging_) {
        if (msg == WM_MOUSEMOVE) {
            POINT cursorScreen;
            ::GetCursorPos(&cursorScreen);
            const int dx = cursorScreen.x - overlayDragStartCursor_.x;
            const int dy = cursorScreen.y - overlayDragStartCursor_.y;
            const int newX = overlayDragStartWindow_.x + dx;
            const int newY = overlayDragStartWindow_.y + dy;
            // 关键：SWP_NOACTIVATE + SWP_NOZORDER —— 移动但不激活、不改 z-order
            ::SetWindowPos(hwnd_, nullptr, newX, newY, 0, 0,
                SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            return true;
        }
        if (msg == WM_LBUTTONUP) {
            ::ReleaseCapture();
            isOverlayDragging_ = false;
            return true;
        }
        // 其他鼠标消息（WM_RBUTTONDOWN 等）让原路径继续
    }

    if (msg != WM_LBUTTONDOWN && msg != WM_LBUTTONDBLCLK && msg != WM_RBUTTONUP)
        return false;

    POINT pt;
    POINTSTOPOINT(pt, lParam);
    int regionKind = webView_->GetNonClientRegionAtPoint(pt);
    bool inNoDragRegion = IsPointInNoDragRegion(pt.x, pt.y);
    bool inCustomDragRegion = IsPointInDragRegion(pt.x, pt.y);
    bool hasCustomDragRegions = false;
    {
        std::lock_guard<std::mutex> lock(regionsMutex_);
        hasCustomDragRegions = !dragRegions_.empty();
    }
    bool inDefaultTitlebarRegion = (!hasCustomDragRegions && pt.y < titlebarHeight_);
    bool noDragByCss = (regionKind == 0 && pt.y < titlebarHeight_);
    bool dragByCss = (regionKind == 1);
    bool shouldDrag = dragByCss || (!noDragByCss && !inNoDragRegion &&
        (inCustomDragRegion || inDefaultTitlebarRegion));
    if (!shouldDrag)
        return false;

    if (msg == WM_LBUTTONDOWN) {
        // overlay popup（noActivate / overlayIntent）：自定义拖拽循环，
        // 绕过 DefWindowProc(WM_NCLBUTTONDOWN HTCAPTION) → SC_MOVE 模态循环内部的
        // SetForegroundWindow 强制激活，避免主窗口被前置覆盖外部应用。
        // 详见 PopupWindow.h::isOverlayDragging_ 注释。
        if (resolvedBehavior_.noActivate || overlayIntent_) {
            POINT cursorScreen;
            ::GetCursorPos(&cursorScreen);
            RECT windowRect;
            ::GetWindowRect(hwnd_, &windowRect);
            overlayDragStartCursor_ = cursorScreen;
            overlayDragStartWindow_ = { windowRect.left, windowRect.top };
            // 防止 WebView2 此前抓的 capture 干扰自定义拖拽
            ::ReleaseCapture();
            ::SetCapture(hwnd_);
            isOverlayDragging_ = true;
            return true;
        }
        // 普通 popup：仍走系统标准拖拽路径（保持原行为，含 Aero Snap 等）
        ReleaseCapture();
        SendMessageW(hwnd_, WM_NCLBUTTONDOWN, HTCAPTION, lParam);
        return true;
    }
    if (msg == WM_LBUTTONDBLCLK && createParams_.resizable) {
        // 双击标题栏切换最大化/还原
        ShowWindow(hwnd_, isMaximized_ ? SW_RESTORE : SW_MAXIMIZE);
        return true;
    }
    if (msg == WM_RBUTTONUP) {
        // 右键标题栏显示系统菜单
        POINT screenPt = pt;
        ClientToScreen(hwnd_, &screenPt);
        HMENU sysMenu = GetSystemMenu(hwnd_, FALSE);
        if (sysMenu) {
            TrackPopupMenu(sysMenu, TPM_RIGHTBUTTON,
                screenPt.x, screenPt.y, 0, hwnd_, nullptr);
        }
        return true;
    }
    return false;
}

bool PopupWindow::IsPointInDragRegion(int clientX, int clientY) const {
    std::lock_guard<std::mutex> lock(regionsMutex_);
    for (const auto& region : dragRegions_) {
        if (clientX >= region.x && clientX < region.x + region.width &&
            clientY >= region.y && clientY < region.y + region.height) {
            return true;
        }
    }
    return false;
}

bool PopupWindow::IsPointInNoDragRegion(int clientX, int clientY) const {
    std::lock_guard<std::mutex> lock(regionsMutex_);
    for (const auto& region : noDragRegions_) {
        if (clientX >= region.x && clientX < region.x + region.width &&
            clientY >= region.y && clientY < region.y + region.height) {
            return true;
        }
    }
    return false;
}

// ============================================
// 消息处理
// ============================================

void PopupWindow::OnCreate() {
    console::printf("[PopupWindow] OnCreate: %s", GetWindowId().c_str());

    isActive_ = (::GetForegroundWindow() == hwnd_);
    chromeBackdropBroadcastReady_ = false;
    ApplyResolvedChrome(true);
    
    // 初始化 WebView2（基类方法）
    if (!InitializeWebView(hwnd_, WebViewPanelMode::Standalone)) {
        console::printf("[PopupWindow] ERROR: Failed to start WebView2 initialization");
        // WebView 初始化失败，立即显示窗口
        TryCommitStartupReveal();
    }
}

void PopupWindow::OnClose() {
    // 如果未启用 beforeClose，直接关闭
    if (!createParams_.beforeClose) {
        DestroyWindow(hwnd_);
        return;
    }
    
    // beforeClose 异步关闭机制
    if (pendingClose_) {
        return;  // 已在关闭流程中
    }
    
    pendingClose_ = true;
    
    // 通知前端窗口即将关闭
    if (bridge_) {
        json data = {{"windowId", GetWindowId()}};
        bridge_->EmitEvent("window:beforeClose", data);
    }
    
    // 启动超时计时器
    SetTimer(hwnd_, CLOSE_TIMER_ID, CLOSE_TIMEOUT_MS, nullptr);
    
    console::printf("[PopupWindow] beforeClose emitted, waiting for response: %s",
                   GetWindowId().c_str());
    // 不调用 DefWindowProc — 阻止立即关闭
}

// ================================================================
// WebView2 MoveFocusRequested 拦截（阻止 Chromium 焦点移交）
// Visual Hosting 模式下，Chromium 处理鼠标点击可交互元素时，
// 会触发 MoveFocusRequested 请求焦点转移，导致 SetForegroundWindow
// → 主窗口被激活。拦截并标记 Handled，阻止默认焦点移交。
// ================================================================

void PopupWindow::InstallMoveFocusGuard() {
    // 仅对 noActivate / overlay / clickThrough 类 popup 安装
    if (!resolvedBehavior_.noActivate && !overlayIntent_ && !createParams_.clickThrough) {
        return;
    }
    if (moveFocusRequestedToken_.value != 0) {
        // 已安装，跳过
        return;
    }
    if (!webView_ || !webView_->GetController()) {
        console::printf("[PopupWindow] InstallMoveFocusGuard: WebView2 controller not ready for %s",
                       GetWindowId().c_str());
        return;
    }

    webView_->GetController()->add_MoveFocusRequested(
        Callback<ICoreWebView2MoveFocusRequestedEventHandler>(
            [this](ICoreWebView2Controller* sender,
                   ICoreWebView2MoveFocusRequestedEventArgs* args) -> HRESULT {
                // 拦截 WebView2 的焦点移交请求：
                // 当用户点击可交互元素时，Chromium 内部会发起 MoveFocusRequested，
                // 默认行为是调用 SetFocus/SetForegroundWindow 导致 popup 或其 owner
                // 被激活。对 noActivate/overlay popup，标记 Handled 阻止默认行为，
                // 焦点不会转移到 popup 或主窗口，但按钮点击仍正常处理（mousedown/up
                // 不依赖焦点移交）。
                if (resolvedBehavior_.noActivate || overlayIntent_ || createParams_.clickThrough) {
                    COREWEBVIEW2_MOVE_FOCUS_REASON reason = COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC;
                    if (args) {
                        args->get_Reason(&reason);
                    }
                    std::ostringstream ev;
                    ev << "reason=" << (reason == COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC
                                        ? "programmatic" : "next")
                       << " result=Handled(true)";
                    EmitActivationEvidence("MoveFocusRequested.intercepted", ev.str());
                    if (args) {
                        args->put_Handled(TRUE);
                    }
                }
                return S_OK;
            }).Get(),
        &moveFocusRequestedToken_);

    if (moveFocusRequestedToken_.value != 0) {
        console::printf("[PopupWindow] InstallMoveFocusGuard: installed for %s",
                       GetWindowId().c_str());
    } else {
        console::printf("[PopupWindow] InstallMoveFocusGuard: FAILED for %s",
                       GetWindowId().c_str());
    }
}

void PopupWindow::RemoveMoveFocusGuard() {
    if (moveFocusRequestedToken_.value != 0 && webView_ && webView_->GetController()) {
        webView_->GetController()->remove_MoveFocusRequested(moveFocusRequestedToken_);
        console::printf("[PopupWindow] RemoveMoveFocusGuard: removed for %s",
                       GetWindowId().c_str());
    }
    moveFocusRequestedToken_ = {};
}

// ================================================================
// WebView2 子 HWND 子类化（拦截 Chromium 内部 WM_MOUSEACTIVATE）
// ================================================================

LRESULT CALLBACK PopupWindow::WebViewSubclassProc(HWND hwnd, UINT msg,
    WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    if (msg == WM_MOUSEACTIVATE) {
        // Popup 窗口的 WebView2 子 HWND 收到 WM_MOUSEACTIVATE：
        // Chromium 默认返回 MA_ACTIVATE，但 popup 的顶层 WndProc 已有拦截。
        // 问题在于 WM_MOUSEACTIVATE 发到子 HWND 时，顶层拦截器收不到。
        // 此处返回 MA_NOACTIVATE，与顶层窗口的拦截策略保持一致：
        // 不激活窗口，但允许鼠标消息正常投递（按钮仍可点击）。
        // dwRefData 携带 popup 的 HWND，用于诊断日志。
        HWND popupHwnd = (HWND)dwRefData;
        PopupWindow* self = nullptr;
        if (popupHwnd && IsWindow(popupHwnd)) {
            self = (PopupWindow*)GetWindowLongPtr(popupHwnd, GWLP_USERDATA);
        }
        if (self && (self->resolvedBehavior_.noActivate || self->overlayIntent_ || self->createParams_.clickThrough)) {
            std::ostringstream ev;
            ev << "result=MA_NOACTIVATE (webview child hwnd intercepted)"
               << " childHwnd=0x" << pfc::format_hex((size_t)hwnd);
            self->EmitActivationEvidence("WebViewSubclassProc.WM_MOUSEACTIVATE", ev.str());
            return MA_NOACTIVATE;
        }
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

void PopupWindow::InstallWebViewSubclass() {
    // 仅对 noActivate / overlay / clickThrough 类 popup 安装子类化
    if (!resolvedBehavior_.noActivate && !overlayIntent_ && !createParams_.clickThrough) {
        return;
    }
    if (webViewChildHwnd_ && webViewSubclassId_) {
        // 已安装，跳过
        return;
    }

    // Visual Hosting 模式下 WebView2 不创建子 HWND（Chromium 通过 DirectComposition
    // 直接在宿主窗口客户区渲染，输入通过 SendMouseInput 路由）。
    // 此模式下 WM_MOUSEACTIVATE 发到 popup 顶层窗口，子类化不可行也不必要。
    // 改用 MoveFocusRequested 拦截方案（见 OnWebViewReady 中的 InstallMoveFocusGuard）。
    if (webView_ && webView_->GetCompositionController()) {
        console::printf("[PopupWindow] InstallWebViewSubclass: Visual Hosting mode, "
                       "no child HWND to subclass for %s - using MoveFocusRequested guard instead",
                       GetWindowId().c_str());
        return;
    }

    // 非 Visual Hosting 模式（标准 Controller）：WebView2 创建 Chrome_WidgetWin_* 子 HWND。
    // WM_MOUSEACTIVATE 发到子 HWND 时，popup 顶层拦截器收不到。
    // 在子 HWND 层拦截返回 MA_NOACTIVATE，根除激活链。

    // 查找 WebView2 内部最外层子 HWND
    HWND child = FindWindowExW(hwnd_, nullptr, L"Chrome_WidgetWin_0", nullptr);
    if (!child) {
        // 如果 Chrome_WidgetWin_0 找不到，可能是 WebView2 尚未完全创建子 HWND。
        // 尝试查找任何 Chrome_WidgetWin_* 子窗口。
        child = FindWindowExW(hwnd_, nullptr, L"Chrome_WidgetWin_1", nullptr);
    }
    if (!child) {
        // 兜底：查找 popup 下的第一个子 HWND（如果 popup 只有 WebView2 一个子窗口）
        child = FindWindowExW(hwnd_, nullptr, nullptr, nullptr);
    }

    if (!child) {
        console::printf("[PopupWindow] InstallWebViewSubclass: no child HWND found for %s",
                       GetWindowId().c_str());
        return;
    }

    webViewChildHwnd_ = child;
    webViewSubclassId_ = (UINT_PTR)this;  // 使用 popup 对象指针作为唯一 subclass ID

    BOOL ok = SetWindowSubclass(child, WebViewSubclassProc, webViewSubclassId_,
        (DWORD_PTR)hwnd_);  // dwRefData = popup HWND，供子类回调诊断日志使用
    if (ok) {
        console::printf("[PopupWindow] InstallWebViewSubclass: installed on child HWND 0x%s for %s",
                       pfc::format_hex((size_t)child).c_str(), GetWindowId().c_str());
    } else {
        console::printf("[PopupWindow] InstallWebViewSubclass: FAILED for %s",
                       GetWindowId().c_str());
        webViewChildHwnd_ = nullptr;
        webViewSubclassId_ = 0;
    }
}

void PopupWindow::RemoveWebViewSubclass() {
    if (webViewChildHwnd_ && webViewSubclassId_ && IsWindow(webViewChildHwnd_)) {
        RemoveWindowSubclass(webViewChildHwnd_, WebViewSubclassProc, webViewSubclassId_);
        console::printf("[PopupWindow] RemoveWebViewSubclass: removed from 0x%s for %s",
                       pfc::format_hex((size_t)webViewChildHwnd_).c_str(), GetWindowId().c_str());
    }
    webViewChildHwnd_ = nullptr;
    webViewSubclassId_ = 0;
}

void PopupWindow::OnDestroy() {
    console::printf("[PopupWindow] OnDestroy: %s", GetWindowId().c_str());
    
    // 移除 WebView2 子 HWND 子类化（必须在 DestroyWebView 之前，
    // 否则子 HWND 可能已不存在导致 RemoveWindowSubclass 失败）
    RemoveWebViewSubclass();
    // 移除 MoveFocusRequested 拦截（必须在 DestroyWebView 之前，
    // 否则 controller 已释放导致 remove_MoveFocusRequested 失败）
    RemoveMoveFocusGuard();

    // 取消计时器
    KillTimer(hwnd_, CLOSE_TIMER_ID);
    KillTimer(hwnd_, STARTUP_REVEAL_TIMER_ID);
    startupRevealPending_ = false;
    startupNavigationCompleted_ = false;
    startupReadySignaled_ = false;
    pendingClose_ = false;
    
    // 销毁该窗口的所有 Port（按设计文档，不清理全局 State）
    PortHub::Instance().CleanupWindowPorts(GetWindowId());
    
    // 取消该窗口所有未完成的异步 HTTP 请求，释放并发槽位
    CancelAllHttpRequestsForWindow(GetWindowId());
    
    // 销毁 WebView（基类方法）
    chromeBackdropBroadcastReady_ = false;
    DestroyWebView();
    
    // 广播关闭事件
    json data = {{"windowId", GetWindowId()}};
    if (!suppressLifecycleBroadcast_) {
        WebViewContext::GetInstance().BroadcastEvent("window:popupClosed", data);
    }
    
    // 从 WindowManager 注销（异步，避免在 WndProc 中锁死）
    std::string id = GetWindowId();
    fb2k::inMainThread([id]() {
        WindowManager::GetInstance().RemovePopup(id);
    });
}

void PopupWindow::OnSize(int /*width*/, int /*height*/) {
    if (!webView_ || !webView_->IsReady())
        return;

    if (isMinimized_) {
        // 不手动隐藏 WebView — WebView2 运行时自动处理
        return;
    }
    
    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);
    webView_->Resize(clientRect);
}

void PopupWindow::OnActivate(WPARAM wParam) {
    const bool previousActive = isActive_;
    isActive_ = (LOWORD(wParam) != WA_INACTIVE);

    // alwaysOnTop 的 popup 被重新激活时，重新确认 TOPMOST z-order，
    // 防止被其他 topmost 窗口（如任务栏）或系统 z-order 调整覆盖
    if (isActive_ && !previousActive) {
        if (GetWindowLongW(hwnd_, GWL_EXSTYLE) & WS_EX_TOPMOST) {
            SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
    }

    const bool shouldApplyBackdrop = !startupRevealPending_ && !startupRevealSettling_;
    std::string extra = std::string("wParam=") + std::to_string((unsigned long long)wParam) +
        " applyBackdrop=" + WindowChromeTrace::BoolText(shouldApplyBackdrop) +
        " startupRevealSettling=" + WindowChromeTrace::BoolText(startupRevealSettling_);
    TraceWindowPhase("WM_ACTIVATE", WindowChromeTrace::BoolText(isActive_), nullptr,
        GetTraceEffect(isActive_), lastBackdropEffect_.empty() ? "-" : lastBackdropEffect_.c_str(),
        extra.c_str());

    // 启动 reveal 期间，直到 post-show native chrome settle 完成前都跳过 backdrop apply。
    // ShowWindow 内部的 WM_ACTIVATE 只更新 active 状态，真正的首个 authoritative apply
    // 由 ReapplyNativeChromeAfterStartupReveal 在 settle 点统一完成，避免 DWM coalesce 抢跑写入。
    if (!shouldApplyBackdrop) {
        return;
    }
    if (previousActive != isActive_) {
        ApplyBackdropForActivation(isActive_, false);
    }
}

// ============================================
// beforeClose 控制
// ============================================

void PopupWindow::CancelClose() {
    if (!pendingClose_) return;
    
    KillTimer(hwnd_, CLOSE_TIMER_ID);
    pendingClose_ = false;
    
    console::printf("[PopupWindow] Close cancelled: %s", GetWindowId().c_str());
}

void PopupWindow::ConfirmClose() {
    if (!pendingClose_) return;
    
    KillTimer(hwnd_, CLOSE_TIMER_ID);
    pendingClose_ = false;
    
    console::printf("[PopupWindow] Close confirmed: %s", GetWindowId().c_str());
    DestroyWindow(hwnd_);
}

// ============================================
// 运行时标题栏切换
// ============================================

void PopupWindow::SetFrameless(bool frameless) {
    if (!hwnd_ || !IsWindow(hwnd_)) return;
    
    bool wasFrameless = !createParams_.frame;
    if (wasFrameless == frameless) return;  // 无变化
    
    createParams_.frame = !frameless;
    ApplyResolvedChrome(true);
    
    console::printf("[PopupWindow] SetFrameless(%s): %s",
                   frameless ? "true" : "false", GetWindowId().c_str());
}

void PopupWindow::SetClickThrough(bool enabled) {
    if (clickThrough_ == enabled) return;
    clickThrough_ = enabled;
    if (enabled) overlayIntent_ = true;
    
    console::printf("[PopupWindow] SetClickThrough(%s): %s",
                   enabled ? "true" : "false", GetWindowId().c_str());
    
    if (hwnd_ && IsWindow(hwnd_)) {
        LONG_PTR exStyle = GetWindowLongPtrW(hwnd_, GWL_EXSTYLE);
        if (enabled) {
            // WS_EX_LAYERED + WS_EX_TRANSPARENT 实现完整鼠标穿透
            // 仅靠 WM_NCHITTEST 返回 HTTRANSPARENT 只能穿透到同线程窗口（owner），
            // 无法穿透到桌面或其他进程——必须使用窗口扩展样式让 DWM 层直接跳过该窗口
            if (!(exStyle & WS_EX_LAYERED)) {
                exStyle |= WS_EX_LAYERED;
            }
            exStyle |= WS_EX_TRANSPARENT;
            SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, exStyle);
            // 保持窗口完全不透明（实际透明度由 WebView2/DComp 控制）
            SetLayeredWindowAttributes(hwnd_, 0, 255, LWA_ALPHA);
            clickThroughEffective_ = true;
            
            // 启动 hover 检测计时器（穿透模式下 WM_MOUSEMOVE 不可用，改用坐标轮询）
            wasHovering_ = false;
            SetTimer(hwnd_, HOVER_TIMER_ID, HOVER_POLL_MS, nullptr);

            // 若已有 exclude regions，立即刷新有效穿透（消除 80ms 首 tick 间隙）
            if (!clickThroughExcludeRegions_.empty()) {
                UpdateClickThroughEffective();
            }
        } else {
            // 移除 WS_EX_TRANSPARENT 和 WS_EX_LAYERED
            // WS_EX_LAYERED 必须同时移除：WS_EX_NOREDIRECTIONBITMAP 窗口上
            // SetLayeredWindowAttributes 是 no-op，DWM 回退到 per-pixel alpha hit testing
            // → 背景 alpha=0 的像素无法接收鼠标事件
            exStyle &= ~WS_EX_TRANSPARENT;
            exStyle &= ~WS_EX_LAYERED;
            SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, exStyle);
            SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE);
            clickThroughEffective_ = false;
            // 注意: 不清空 clickThroughExcludeRegions_（与点击穿透解耦）
            
            // 停止计时器，通知前端 hover 结束
            wasHovering_ = false;
            KillTimer(hwnd_, HOVER_TIMER_ID);
            json data = {
                {"windowId", GetWindowId()},
                {"hovering", false}
            };
            WebViewContext::GetInstance().BroadcastEvent("window:hoverStateChanged", data);
        }
    }
}

void PopupWindow::SetClickThroughExcludeRegions(const std::vector<RECT>& regions) {
    // 截断到上限 32
    constexpr size_t MAX_EXCLUDE_REGIONS = 32;
    if (regions.size() <= MAX_EXCLUDE_REGIONS) {
        clickThroughExcludeRegions_ = regions;
    } else {
        clickThroughExcludeRegions_.assign(regions.begin(), regions.begin() + MAX_EXCLUDE_REGIONS);
    }
    console::printf("[PopupWindow] SetClickThroughExcludeRegions: %s, count=%zu",
                   GetWindowId().c_str(), clickThroughExcludeRegions_.size());
    UpdateClickThroughEffective();
}

void PopupWindow::ClearClickThroughExcludeRegions() {
    clickThroughExcludeRegions_.clear();
    console::printf("[PopupWindow] ClearClickThroughExcludeRegions: %s", GetWindowId().c_str());
    UpdateClickThroughEffective();
}

bool PopupWindow::IsPointInExcludeRegion(POINT ptPhysical) const {
    for (const auto& rc : clickThroughExcludeRegions_) {
        if (PtInRect(&rc, ptPhysical)) {
            return true;
        }
    }
    return false;
}

void PopupWindow::UpdateClickThroughEffective() {
    if (!hwnd_ || !IsWindow(hwnd_)) return;

    // 若未处于穿透请求态，不改变原生样式
    if (!clickThrough_) {
        clickThroughEffective_ = false;
        return;
    }

    // 若无热区，确保完整穿透
    if (clickThroughExcludeRegions_.empty()) {
        if (!clickThroughEffective_) {
            LONG_PTR exStyle = GetWindowLongPtrW(hwnd_, GWL_EXSTYLE);
            exStyle |= WS_EX_LAYERED;
            exStyle |= WS_EX_TRANSPARENT;
            SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, exStyle);
            SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE);
            clickThroughEffective_ = true;
        }
        return;
    }

    // 有热区：根据光标位置决定是否穿透
    POINT cursor;
    GetCursorPos(&cursor);
    ScreenToClient(hwnd_, &cursor);

    bool inExcludeRegion = IsPointInExcludeRegion(cursor);
    bool shouldBeTransparent = !inExcludeRegion;

    if (shouldBeTransparent != clickThroughEffective_) {
        LONG_PTR exStyle = GetWindowLongPtrW(hwnd_, GWL_EXSTYLE);
        if (shouldBeTransparent) {
            exStyle |= WS_EX_LAYERED;
            exStyle |= WS_EX_TRANSPARENT;
        } else {
            exStyle &= ~WS_EX_TRANSPARENT;
            exStyle &= ~WS_EX_LAYERED;
        }
        SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, exStyle);
        SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE);
        clickThroughEffective_ = shouldBeTransparent;
    }
}

void PopupWindow::ApplyFramelessState(bool frameless) {
    if (!hwnd_ || !IsWindow(hwnd_)) return;

    // frame:true 时判断当前是否需要 1px DWM 帧扩展（backdrop 效果可见性）
    bool needsFrameMarginExtension = false;
    if (!frameless) {
        needsFrameMarginExtension =
            S_IsPluginManagedBackdropEffect(resolvedChromeState_.effectiveActiveEffect) ||
            S_IsPluginManagedBackdropEffect(resolvedChromeState_.effectiveInactiveEffect);
    }

    // 短路守卫：防止 Chrome Apply 链重复触发 DWM 帧重组（与 MainWindow 对齐）
    // 额外条件：frame:true 时若帧扩展需求变化（效果从 none→acrylic 或反之），不可短路
    if (framelessDwmApplied_ && createParams_.frame == !frameless &&
        lastFrameMarginExtended_ == needsFrameMarginExtension) {
        return;
    }
    framelessDwmApplied_ = true;
    lastFrameMarginExtended_ = needsFrameMarginExtension;

    // WS_POPUP 模式：无 DWM 帧，不需要扩展/圆角（彻底消除边框和阴影）
    if (usePopupStyle_) {
        SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0,
            SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        RedrawWindow(hwnd_, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_FRAME);
        return;
    }

    if (frameless) {
        MARGINS margins = { -1, -1, -1, -1 };
        S_DwmExtendFrameIntoClientArea(hwnd_, &margins);

        DWORD cornerPref = DWMWCP_ROUND_V;
        S_DwmSetWindowAttribute(hwnd_, DWMWA_WINDOW_CORNER_PREFERENCE_V,
            &cornerPref, sizeof(cornerPref));
    } else {
        // frame:true 标准帧模式：
        // 当存在 plugin-managed DWM 效果 (mica/acrylic) 时，需要全帧扩展 {-1,-1,-1,-1}
        // 才能让 DWM 合成管线将 backdrop 材质渲染到整个客户区。
        // Mica (DWMSBT_MAINWINDOW) 有全窗口填充的特殊行为，{0,0,0,1} 即可；
        // 但 Acrylic (DWMSBT_TRANSIENTWINDOW) 仅填充 DWM 帧扩展区域，{0,0,0,1} 下
        // 客户区仅 1px 底边有效果、其余区域显灰。使用 {-1,-1,-1,-1} 统一解决。
        // MARGINS{0,0,0,0} 会导致 DWMWA_SYSTEMBACKDROP_TYPE 设置成功但材质不可见。
        MARGINS margins = needsFrameMarginExtension ? MARGINS{ -1, -1, -1, -1 } : MARGINS{ 0, 0, 0, 0 };
        S_DwmExtendFrameIntoClientArea(hwnd_, &margins);

        DWORD cornerPref = DWMWCP_DEFAULT_V;
        S_DwmSetWindowAttribute(hwnd_, DWMWA_WINDOW_CORNER_PREFERENCE_V,
            &cornerPref, sizeof(cornerPref));
    }

    SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0,
        SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    RedrawWindow(hwnd_, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_FRAME);
}

// ============================================
// WebView 就绪回调
// ============================================

void PopupWindow::OnWebViewReady() {
    console::printf("[PopupWindow] OnWebViewReady: %s", GetWindowId().c_str());
    
    // 不调用基类 OnWebViewReady()，因为它内部会调用 LoadFrontendPage() 导航到裸 index.html，
    // 然后我们又要 Navigate(BuildNavigationUrl()) 到带 query params 的 URL，造成双重导航。
    // 改为手动调用基类各步骤，跳过 LoadFrontendPage。
    ResizeWebView();
    bridge_->SetWebView(webView_.get());
    RegisterAllApis();
    InitializeCallbacks();
    webView_->SetMessageHandler([this](const std::wstring& json) {
        HandleWebMessage(json);
    });
    
    // WebView2 子 HWND 子类化：拦截 Chromium 内部的 WM_MOUSEACTIVATE，
    // 阻止点击可交互元素时绕过 popup 顶层 MA_NOACTIVATE 拦截器。
    // 仅在标准 Controller 模式下有效（Visual Hosting 模式无子 HWND）。
    InstallWebViewSubclass();
    
    // WebView2 MoveFocusRequested 拦截：阻止 Chromium 内部焦点移交，
    // 防止点击可交互元素时 SetForegroundWindow 导致主窗口被激活。
    // 在 Visual Hosting 模式下尤其关键（该模式无子 HWND，子类化拦截不可行）。
    InstallMoveFocusGuard();
    
    // transparent 背景透明处理
    if (createParams_.transparent && webView_) {
        webView_->SetBackgroundTransparent(true);
    }

    chromeBackdropBroadcastReady_ = true;
    ApplyResolvedChrome(true);
    
    // 导航到带 windowId/route 参数的目标 URL（仅一次导航）
    std::wstring url = BuildNavigationUrl();
    if (!url.empty()) {
        // 生产模式需要设置虚拟主机映射
        if (!security_config::UseDevServer()) {
            std::wstring resourcesDir = GetFrontendResourcesDir();
            if (!resourcesDir.empty()) {
                SetupVirtualHostMapping(resourcesDir);
            }
        }
        // 当 popup 直接指向第三方 http(s) URL 时, 必须把该 origin 登记进
        // WebViewHost 的可信集合, 否则 IsOriginAllowed 会静默 block 来自
        // 该页面的所有 invoke 请求, 表现为 30s 超时.
        // BuildNavigationUrl 已对 createParams_.url 做了协议识别和参数拼接,
        // 这里直接用最终 URL 抽取 origin 即可 (NormalizeToOrigin 内部会丢
        // path/query/fragment, 重复登记会去重).
        if (webView_) {
            webView_->AddTrustedOrigin(url);
        }
        console::printf("[PopupWindow] Navigating to: %s",
                       pfc::stringcvt::string_utf8_from_wide(url.c_str()).get_ptr());
        Navigate(url);
    }
    
    // 注册 SelectionWatcher
    SelectionWatcher::GetInstance().RegisterPanel(this);
    
    // 触发重新布局
    RECT rect;
    GetClientRect(hwnd_, &rect);
    OnSize(rect.right, rect.bottom);
    
    // 广播弹出窗口已创建事件
    json data = {
        {"windowId", GetWindowId()},
        {"title", createParams_.title},
        {"url", createParams_.url}
    };
    if (!suppressLifecycleBroadcast_) {
        WebViewContext::GetInstance().BroadcastEvent("window:popupOpened", data);
    }
}

// ============================================
// 启动 reveal 状态机
// ============================================

void PopupWindow::OnNavigationCompleted(bool success) {
    if (!startupRevealPending_ || startupRevealCommitted_) return;

    startupNavigationCompleted_ = true;

    if (success) {
        if (startupReadySignaled_ && IsStartupRevealChromeReady()) {
            TryCommitStartupReveal();
        } else {
            ArmStartupFallbackTimer();
        }
    } else {
        console::printf("[PopupWindow] Navigation failed, fallback reveal: %s",
                       GetWindowId().c_str());
        TryCommitStartupReveal();
    }
}

void PopupWindow::OnWindowReadySignal(const std::string& source) {
    if (!startupRevealPending_ || startupRevealCommitted_) return;

    const bool ignoreEarlyAutomaticSignal =
        source == "auto-dom-content-loaded" ||
        source == "auto-ready-state" ||
        source == "auto-ready-state-interactive";
    if (ignoreEarlyAutomaticSignal) {
        console::printf("[PopupWindow] Ignoring automatic windowReady during startup reveal, source=%s: %s",
                       source.c_str(),
                       GetWindowId().c_str());
        return;
    }

    startupReadySignaled_ = true;

    if (startupNavigationCompleted_ && IsStartupRevealChromeReady()) {
        KillTimer(hwnd_, STARTUP_REVEAL_TIMER_ID);
        TryCommitStartupReveal();
    }
}

bool PopupWindow::IsStartupRevealChromeReady() const {
    const bool hasPluginEffect =
        S_IsPluginManagedBackdropEffect(resolvedChromeState_.effectiveActiveEffect) ||
        S_IsPluginManagedBackdropEffect(resolvedChromeState_.effectiveInactiveEffect);
    if (!hasPluginEffect && !resolvedChromeState_.transparentBackground) {
        return false;
    }
    return webView_ && webView_->IsReady();
}

void PopupWindow::MaybeCommitStartupRevealAfterChromeReady(const char* reason) {
    if (!startupRevealPending_ || startupRevealCommitted_ || !startupNavigationCompleted_) {
        return;
    }
    if (!IsStartupRevealChromeReady()) {
        return;
    }

    console::printf("[PopupWindow] Chrome prepared during startup reveal, reason=%s, activeEffect=%s, inactiveEffect=%s, transparent=%s, startupReadySignaled=%s: %s",
                   reason,
                   resolvedChromeState_.effectiveActiveEffect.c_str(),
                   resolvedChromeState_.effectiveInactiveEffect.c_str(),
                   resolvedChromeState_.transparentBackground ? "true" : "false",
                   startupReadySignaled_ ? "true" : "false",
                   GetWindowId().c_str());

    if (!startupReadySignaled_) {
        return;
    }

    KillTimer(hwnd_, STARTUP_REVEAL_TIMER_ID);
    TryCommitStartupReveal();
}

void PopupWindow::RefreshStartupRevealSurface() {
    if (!hwnd_ || !IsWindow(hwnd_) || !webView_ || !webView_->IsReady() || isMinimized_) {
        return;
    }

    RECT clientRect{};
    GetClientRect(hwnd_, &clientRect);
    webView_->SetVisible(true);
    webView_->Resize(clientRect);
}

void PopupWindow::LogSurfaceDiagnostics(const char* phase) const {
    if (!hwnd_ || !IsWindow(hwnd_)) return;

    RECT clientRect{};
    GetClientRect(hwnd_, &clientRect);

    RECT controllerBounds{};
    bool hasBounds = false;
    if (webView_ && webView_->IsReady() && webView_->GetController()) {
        hasBounds = SUCCEEDED(webView_->GetController()->get_Bounds(&controllerBounds));
    }

    FB2K_console_formatter()
        << "[SurfaceDiag] phase=" << phase
        << " t=" << WindowChromeTrace::RelativeMs() << "ms"
        << " hwnd=0x" << pfc::format_hex((size_t)hwnd_)
        << " clientRect=(" << clientRect.left << "," << clientRect.top
        << "," << clientRect.right << "," << clientRect.bottom << ")"
        << " controllerBounds=" << (hasBounds
            ? (std::string("(") + std::to_string(controllerBounds.left) + ","
               + std::to_string(controllerBounds.top) + ","
               + std::to_string(controllerBounds.right) + ","
               + std::to_string(controllerBounds.bottom) + ")").c_str()
            : "N/A")
        << " visible=" << WindowChromeTrace::BoolText(IsWindowVisible(hwnd_) != FALSE)
        << " iconic=" << WindowChromeTrace::BoolText(IsIconic(hwnd_) != FALSE)
        << " zoomed=" << WindowChromeTrace::BoolText(IsZoomed(hwnd_) != FALSE)
        << " windowId=" << GetWindowId().c_str();
}

bool PopupWindow::EnsureSurfaceConvergedAfterNativeFrame(const char* reason) {
    if (!hwnd_ || !IsWindow(hwnd_) || !webView_ || !webView_->IsReady() || isMinimized_) {
        return false;
    }

    auto* controller = webView_->GetController();
    if (!controller) return false;

    RECT clientRect{};
    GetClientRect(hwnd_, &clientRect);

    RECT currentBounds{};
    controller->get_Bounds(&currentBounds);

    const bool boundsMatch = (clientRect.left == currentBounds.left &&
                              clientRect.top == currentBounds.top &&
                              clientRect.right == currentBounds.right &&
                              clientRect.bottom == currentBounds.bottom);

    FB2K_console_formatter()
        << "[SurfaceConverge] reason=" << reason
        << " t=" << WindowChromeTrace::RelativeMs() << "ms"
        << " clientRect=(" << clientRect.left << "," << clientRect.top
        << "," << clientRect.right << "," << clientRect.bottom << ")"
        << " controllerBounds=(" << currentBounds.left << "," << currentBounds.top
        << "," << currentBounds.right << "," << currentBounds.bottom << ")"
        << " match=" << WindowChromeTrace::BoolText(boundsMatch)
        << " windowId=" << GetWindowId().c_str();

    RECT nudged = clientRect;
    nudged.right = (nudged.right > 0) ? nudged.right - 1 : 0;
    webView_->Resize(nudged);
    webView_->Resize(clientRect);

    LogSurfaceDiagnostics((std::string("SurfaceConverge.done/") + reason).c_str());
    return true;
}

bool PopupWindow::EnsureAuthoritativeNativeChrome(const char* reason) {
    TraceWindowPhase("EnsureAuthoritativeNativeChrome.enter", nullptr, "true",
        nullptr, nullptr, reason);

    if (!hwnd_ || !IsWindow(hwnd_) || isMinimized_) {
        TraceWindowPhase("EnsureAuthoritativeNativeChrome.skip", nullptr, "true",
            nullptr, nullptr, "reason=invalid-hwnd-or-minimized");
        return false;
    }

    console::printf("[PopupWindow] Authoritative native chrome reapply, reason=%s, activeEffect=%s, inactiveEffect=%s, transparent=%s: %s",
                   reason,
                   resolvedChromeState_.effectiveActiveEffect.c_str(),
                   resolvedChromeState_.effectiveInactiveEffect.c_str(),
                   resolvedChromeState_.transparentBackground ? "true" : "false",
                   GetWindowId().c_str());

    // WS_POPUP 模式：无 DWM 帧合成，直接 apply chrome + refresh
    if (usePopupStyle_) {
        ApplyResolvedChrome(true);
        WindowChromeApplier::RefreshNativeFrame(hwnd_);
        return true;
    }

    // 同 MainWindow：先 reset 为 NONE 再设回正确值，迫使 DWM 重建合成管线
    int resetType = DWMSBT_NONE_V;
    S_DwmSetWindowAttribute(hwnd_, DWMWA_SYSTEMBACKDROP_TYPE_V,
        &resetType, sizeof(resetType));

    MARGINS margins = { -1, -1, -1, -1 };
    S_DwmExtendFrameIntoClientArea(hwnd_, &margins);

    ApplyResolvedChrome(true);
    WindowChromeApplier::RefreshNativeFrame(hwnd_);
    LogSurfaceDiagnostics((std::string("EnsureAuthoritativeNativeChrome.exit/") + reason).c_str());
    TraceWindowPhase("EnsureAuthoritativeNativeChrome.exit", nullptr, "true",
        nullptr, nullptr, reason);
    return true;
}

bool PopupWindow::ReapplyNativeChromeAfterStartupReveal() {
    return EnsureAuthoritativeNativeChrome("startup-reveal");
}

void PopupWindow::TryCommitStartupReveal() {
    std::string enterExtra = std::string("startupNavigationCompleted=") +
        WindowChromeTrace::BoolText(startupNavigationCompleted_) +
        " startupReadySignaled=" + WindowChromeTrace::BoolText(startupReadySignaled_) +
        " noActivate=" + WindowChromeTrace::BoolText(resolvedBehavior_.noActivate);
    TraceWindowPhase("TryCommitStartupReveal.enter", nullptr, nullptr, nullptr, nullptr,
        enterExtra.c_str());

    if (startupRevealCommitted_) return;

    if (!hwnd_ || !IsWindow(hwnd_)) {
        startupRevealPending_ = false;
        startupRevealSettling_ = false;
        TraceWindowPhase("TryCommitStartupReveal.invalid-hwnd");
        return;
    }

    startupRevealCommitted_ = true;
    startupRevealPending_ = false;
    startupRevealSettling_ = true;

    KillTimer(hwnd_, STARTUP_REVEAL_TIMER_ID);

    const bool canRefreshSurface = webView_ && webView_->IsReady();
    if (canRefreshSurface) {
        RECT clientRect;
        GetClientRect(hwnd_, &clientRect);
        webView_->SetVisible(true);
        webView_->Resize(clientRect);
    }

    int showCmd = resolvedBehavior_.noActivate ? SW_SHOWNOACTIVATE : SW_SHOW;
    if (isMinimized_) {
        showCmd = SW_SHOWMINIMIZED;
    } else if (isMaximized_) {
        showCmd = SW_SHOWMAXIMIZED;
    }

    // 在 ShowWindow 之前同步写入最终 backdrop 到 DWM。
    // ShowWindow 内部触发的 WM_ACTIVATE 会被 startupRevealSettling_ 跳过，
    // 如果不在这里预写，第一个可见帧将没有正确的 backdrop type。
    // noActivate 的 popup 不会收到 WM_ACTIVATE，用当前 isActive_ 状态。
    if (!isMinimized_ && showCmd != SW_SHOWMINIMIZED) {
        ResolvePolicies();
        const bool preShowActive = resolvedBehavior_.noActivate ? isActive_ : true;
        ApplyBackdropForActivation(preShowActive, true);
    }

    std::string showExtra = std::string("showCmd=") + std::to_string(showCmd) +
        " reason=" + (startupReadySignaled_ ? "ready" : "fallback");
    TraceWindowPhase("ShowWindow.before", nullptr, nullptr, nullptr, nullptr, showExtra.c_str());

    ShowWindow(hwnd_, showCmd);
    LogSurfaceDiagnostics("ShowWindow.after");
    TraceWindowPhase("ShowWindow.after", nullptr, nullptr, nullptr, nullptr, showExtra.c_str());

    UpdateWindow(hwnd_);

    bool postShowSurfaceSynchronized = false;
    bool postShowNativeChromeReapplied = false;
    if (canRefreshSurface && !isMinimized_) {
        console::printf("[PopupWindow] Synchronizing WebView surface after startup reveal (%s, showCmd=%d): %s",
                       startupReadySignaled_ ? "ready" : "fallback",
                       showCmd,
                       GetWindowId().c_str());
        RefreshStartupRevealSurface();
        postShowSurfaceSynchronized = true;
    }

    startupRevealSettling_ = false;
    if (!isMinimized_) {
        postShowNativeChromeReapplied = ApplyBackdropForActivation(isActive_, true);
    }

    needsAuthoritativeChromeReapply_ = false;

    console::printf("[PopupWindow] Startup reveal committed (%s, showCmd=%d, postShowSurfaceSynchronized=%s, postShowNativeChromeReapplied=%s): %s",
                   startupReadySignaled_ ? "ready" : "fallback",
                   showCmd,
                   postShowSurfaceSynchronized ? "true" : "false",
                   postShowNativeChromeReapplied ? "true" : "false",
                   GetWindowId().c_str());

    std::string exitExtra = std::string("showCmd=") + std::to_string(showCmd) +
        " reason=" + (startupReadySignaled_ ? "ready" : "fallback") +
        " postShowSurfaceSynchronized=" + WindowChromeTrace::BoolText(postShowSurfaceSynchronized) +
        " postShowNativeChromeReapplied=" + WindowChromeTrace::BoolText(postShowNativeChromeReapplied);
    TraceWindowPhase("TryCommitStartupReveal.exit", nullptr, nullptr, nullptr, nullptr, exitExtra.c_str());
}

void PopupWindow::ArmStartupFallbackTimer() {
    if (startupRevealCommitted_) return;
    SetTimer(hwnd_, STARTUP_REVEAL_TIMER_ID, STARTUP_REVEAL_TIMEOUT_MS, nullptr);
}

// ============================================
// 策略处理
// ============================================

std::string PopupWindow::NormalizeProfile(const std::string& profile) {
    std::string p = S_ToLower(profile);
    if (p == "standard") return "standard";
    if (p == "miniplayer" || p == "mini-player" || p == "mini_player") return "miniPlayer";
    if (p == "desktoplyrics" || p == "desktop-lyrics" || p == "desktop_lyrics") return "desktopLyrics";
    return "";
}

std::string PopupWindow::NormalizeEffect(const std::string& effect, bool allowSystem, const std::string& fallback) {
    return WindowChromeResolver::NormalizeBackdropEffect(effect, allowSystem, fallback);
}

WindowChromeBaseState PopupWindow::BuildChromeBaseState() const {
    WindowChromeBaseState base;
    // 与 MainWindow::BuildChromeBaseState 保持一致：inactiveEffect="inherit"
    // 让 DWM 原生调暗 mica/acrylic。reapplyOnActivate=false 避免每次 WM_ACTIVATE
    // 主动重写 DWMWA_SYSTEMBACKDROP_TYPE （该写入为瞬时无动画，会覆盖 DWM
    // 原生失焦过渡动画并在 WS_EX_NOREDIRECTIONBITMAP 下造成背景闪透明）。
    base.backdropPolicy.activeEffect = "inherit";
    base.backdropPolicy.inactiveEffect = "inherit";
    base.backdropPolicy.darkMode = true;
    base.backdropPolicy.reapplyOnActivate = false;
    base.inheritedBackdropEffect = S_GetUserBackdropEffectString();

    if (hasProfile_ && profile_ == "miniPlayer") {
        base.backdropPolicy.activeEffect = "acrylic";
        base.backdropPolicy.inactiveEffect = "acrylic";
        base.inheritedBackdropEffect = "acrylic";
    } else if (hasProfile_ && profile_ == "desktopLyrics") {
        base.backdropPolicy.activeEffect = "none";
        base.backdropPolicy.inactiveEffect = "none";
        base.inheritedBackdropEffect = "none";
    }

    base.frameless = !createParams_.frame;
    base.cornerPreference = "unsupported";
    base.transparentBackground = createParams_.transparent;
    return base;
}

WindowChromeStandardState PopupWindow::BuildChromeStandardState() const {
    WindowChromeStandardState standard;
    standard.backdropPolicy = backdropPolicyOverrides_;
    return standard;
}

WindowChromeDerivedState PopupWindow::BuildChromeDerivedState() const {
    WindowChromeDerivedState derived;
    derived.active = isActive_;
    derived.fullscreen = IsFullscreen();
    derived.supportsCornerPreference = false;
    derived.supportsBackdropPolicy = true;
    derived.supportsMicaAlt = false;
    derived.windowKind = WindowKind::Popup;
    return derived;
}

WindowChromeApplyHooks PopupWindow::BuildChromeApplyHooks() {
    WindowChromeApplyHooks hooks;
    hooks.hwnd = hwnd_;
    hooks.webView = webView_ ? webView_.get() : nullptr;
    hooks.diagnosticWindowKind = "popup";
    hooks.diagnosticWindowId = GetWindowId().empty() ? "popup" : GetWindowId().c_str();
    hooks.diagnosticStartupRevealPending = startupRevealPending_;
    hooks.diagnosticStartupRevealCommitted = startupRevealCommitted_;
    hooks.diagnosticStartupRevealSettling = startupRevealSettling_;
    hooks.diagnosticIsActive = isActive_;
    hooks.applyFrameless = [this](bool frameless) {
        ApplyFramelessState(frameless);
    };
    hooks.canBroadcastBackdropStateChanged = [this]() {
        return CanBroadcastBackdropStateChanged();
    };
    hooks.broadcastBackdropStateChanged = [this](bool active, const std::string& mode,
                                                 const std::string& effect) {
        BroadcastBackdropStateChanged(active, mode, effect);
    };
    return hooks;
}

bool PopupWindow::ApplyResolvedChrome(bool forceRefresh) {
    ResolvePolicies();
    const bool applied = ApplyBackdropForActivation(isActive_, forceRefresh);
    MaybeCommitStartupRevealAfterChromeReady("ApplyResolvedChrome");
    return applied;
}

bool PopupWindow::UpdateCompatibilityBackdropEffect(const std::optional<std::string>& effect,
                                                    const std::optional<bool>& darkMode,
                                                    bool clearBlur, bool forceRefresh) {
    chromeCompatibilityOverrides_.legacyBackdropEffect = effect;
    if (darkMode.has_value()) {
        chromeCompatibilityOverrides_.legacyDarkMode = darkMode;
    }
    if (clearBlur) {
        chromeCompatibilityOverrides_.legacyUseBlurBehind = false;
    }
    return ApplyResolvedChrome(forceRefresh);
}

bool PopupWindow::UpdateCompatibilityBlur(bool enabled, bool forceRefresh) {
    chromeCompatibilityOverrides_.legacyUseBlurBehind = enabled;
    return ApplyResolvedChrome(forceRefresh);
}

bool PopupWindow::UpdateCompatibilityDarkMode(bool enabled, bool forceRefresh) {
    chromeCompatibilityOverrides_.legacyDarkMode = enabled;
    return ApplyResolvedChrome(forceRefresh);
}

bool PopupWindow::UpdateCompatibilityTransparentBackground(bool transparent, bool forceRefresh) {
    chromeCompatibilityOverrides_.legacyTransparentBackground = transparent;
    return ApplyResolvedChrome(forceRefresh);
}

void PopupWindow::InitializePolicyState() {
    hasProfile_ = createParams_.hasProfile;
    profile_ = hasProfile_ ? NormalizeProfile(createParams_.profile) : "legacy";
    if (hasProfile_ && profile_.empty()) {
        profile_ = "standard";
    }

    behaviorOverrides_ = (createParams_.hasBehavior && createParams_.behavior.is_object())
        ? createParams_.behavior
        : json::object();
    backdropPolicyOverrides_ = (createParams_.hasBackdropPolicy && createParams_.backdropPolicy.is_object())
        ? createParams_.backdropPolicy
        : json::object();

    ResolvePolicies();
}

void PopupWindow::ResolvePolicies() {
    // 1) 兼容默认：按旧字段推导
    resolvedBehavior_.showInTaskbar = createParams_.showInTaskbar;
    resolvedBehavior_.showInAltTab = createParams_.showInTaskbar;
    resolvedBehavior_.keepVisibleOnShowDesktop = false;
    resolvedBehavior_.allowMinimize = createParams_.showInTaskbar;
    resolvedBehavior_.owner = "none";
    resolvedBehavior_.noActivate = false;

    // 与 BuildChromeBaseState 保持一致，详见该函数注释。
    resolvedBackdropPolicy_.activeEffect = "inherit";
    resolvedBackdropPolicy_.inactiveEffect = "inherit";
    resolvedBackdropPolicy_.darkMode = true;
    resolvedBackdropPolicy_.reapplyOnActivate = false;

    // 2) Profile 默认
    if (hasProfile_) {
        if (profile_ == "standard") {
            resolvedBehavior_.showInTaskbar = true;
            resolvedBehavior_.showInAltTab = true;
            resolvedBehavior_.keepVisibleOnShowDesktop = false;
            resolvedBehavior_.allowMinimize = true;
            resolvedBehavior_.owner = "main";
            resolvedBehavior_.noActivate = false;

            // standard profile 与主窗口对齐：inactiveEffect="inherit"
            // 依赖 DWM 原生失焦过渡动画，避免背景闪透明。
            resolvedBackdropPolicy_.activeEffect = "inherit";
            resolvedBackdropPolicy_.inactiveEffect = "inherit";
        } else if (profile_ == "miniPlayer") {
            resolvedBehavior_.showInTaskbar = false;
            resolvedBehavior_.showInAltTab = false;
            resolvedBehavior_.keepVisibleOnShowDesktop = true;
            resolvedBehavior_.allowMinimize = false;
            resolvedBehavior_.owner = "none";
            resolvedBehavior_.noActivate = false;

            // mini 模式默认保持失焦前后观感一致
            resolvedBackdropPolicy_.activeEffect = "acrylic";
            resolvedBackdropPolicy_.inactiveEffect = "acrylic";
        } else if (profile_ == "desktopLyrics") {
            resolvedBehavior_.showInTaskbar = false;
            resolvedBehavior_.showInAltTab = false;
            resolvedBehavior_.keepVisibleOnShowDesktop = true;
            resolvedBehavior_.allowMinimize = false;
            resolvedBehavior_.owner = "none";
            resolvedBehavior_.noActivate = true;

            // 桌面歌词：完全透明背景，不使用任何 DWM 效果
            resolvedBackdropPolicy_.activeEffect = "none";
            resolvedBackdropPolicy_.inactiveEffect = "none";
        }
    }

    // 3) 旧参数优先（仅在明确传入时覆盖 profile）
    if (createParams_.hasShowInTaskbar) {
        resolvedBehavior_.showInTaskbar = createParams_.showInTaskbar;
        resolvedBehavior_.showInAltTab = createParams_.showInTaskbar;
        resolvedBehavior_.allowMinimize = createParams_.showInTaskbar;
    }

    // 4) behavior 显式覆盖
    S_TryGetBool(behaviorOverrides_, "showInTaskbar", resolvedBehavior_.showInTaskbar);
    S_TryGetBool(behaviorOverrides_, "showInAltTab", resolvedBehavior_.showInAltTab);
    S_TryGetBool(behaviorOverrides_, "keepVisibleOnShowDesktop", resolvedBehavior_.keepVisibleOnShowDesktop);
    S_TryGetBool(behaviorOverrides_, "allowMinimize", resolvedBehavior_.allowMinimize);
    S_TryGetBool(behaviorOverrides_, "noActivate", resolvedBehavior_.noActivate);
    if (behaviorOverrides_.is_object() && behaviorOverrides_.contains("owner") &&
        behaviorOverrides_["owner"].is_string()) {
        std::string owner = S_ToLower(behaviorOverrides_["owner"].get<std::string>());
        resolvedBehavior_.owner = (owner == "main") ? "main" : "none";
    }

    // 5) backdropPolicy 显式覆盖
    if (backdropPolicyOverrides_.is_object()) {
        if (backdropPolicyOverrides_.contains("activeEffect") &&
            backdropPolicyOverrides_["activeEffect"].is_string()) {
            resolvedBackdropPolicy_.activeEffect = NormalizeEffect(
                backdropPolicyOverrides_["activeEffect"].get<std::string>(), false,
                resolvedBackdropPolicy_.activeEffect);
        }
        if (backdropPolicyOverrides_.contains("inactiveEffect") &&
            backdropPolicyOverrides_["inactiveEffect"].is_string()) {
            resolvedBackdropPolicy_.inactiveEffect = NormalizeEffect(
                backdropPolicyOverrides_["inactiveEffect"].get<std::string>(), true,
                resolvedBackdropPolicy_.inactiveEffect);
        }
        S_TryGetBool(backdropPolicyOverrides_, "darkMode", resolvedBackdropPolicy_.darkMode);
        S_TryGetBool(backdropPolicyOverrides_, "reapplyOnActivate", resolvedBackdropPolicy_.reapplyOnActivate);
    }

    // 6) 一致性收敛
    if (!resolvedBehavior_.showInAltTab) {
        resolvedBehavior_.showInTaskbar = false;
    }
    if (resolvedBehavior_.keepVisibleOnShowDesktop) {
        resolvedBehavior_.allowMinimize = false;
    }

    WindowChromeResolverSnapshot snapshot;
    snapshot.base = BuildChromeBaseState();
    snapshot.compatibility = chromeCompatibilityOverrides_;
    snapshot.standard = BuildChromeStandardState();
    snapshot.derived = BuildChromeDerivedState();

    resolvedChromeState_ = WindowChromeResolver::Resolve(snapshot);
    resolvedBackdropPolicy_.activeEffect = resolvedChromeState_.backdropPolicy.activeEffect;
    resolvedBackdropPolicy_.inactiveEffect = resolvedChromeState_.backdropPolicy.inactiveEffect;
    resolvedBackdropPolicy_.darkMode = resolvedChromeState_.backdropPolicy.darkMode;
    resolvedBackdropPolicy_.reapplyOnActivate = resolvedChromeState_.backdropPolicy.reapplyOnActivate;
}

void PopupWindow::ApplyResolvedBehaviorToStyles(DWORD& style, DWORD& exStyle) const {
    // 最小化能力
    if (!resolvedBehavior_.allowMinimize) {
        style &= ~WS_MINIMIZEBOX;
    } else {
        style |= WS_MINIMIZEBOX;
    }

    // Shell 可见性（任务栏 / Alt+Tab）
    bool hideFromShell = !resolvedBehavior_.showInTaskbar || !resolvedBehavior_.showInAltTab;
    if (hideFromShell) {
        exStyle |= WS_EX_TOOLWINDOW;
        exStyle &= ~WS_EX_APPWINDOW;
    } else {
        exStyle &= ~WS_EX_TOOLWINDOW;
        exStyle |= WS_EX_APPWINDOW;
    }

    // 激活策略
    if (resolvedBehavior_.noActivate) {
        exStyle |= WS_EX_NOACTIVATE;
    } else {
        exStyle &= ~WS_EX_NOACTIVATE;
    }
}

void PopupWindow::ApplyResolvedBehaviorRuntime() {
    if (!hwnd_ || !IsWindow(hwnd_)) return;

    DWORD style = static_cast<DWORD>(GetWindowLongPtrW(hwnd_, GWL_STYLE));
    DWORD exStyle = static_cast<DWORD>(GetWindowLongPtrW(hwnd_, GWL_EXSTYLE));
    DWORD newStyle = style;
    DWORD newExStyle = exStyle;

    ApplyResolvedBehaviorToStyles(newStyle, newExStyle);

    if (newStyle != style) {
        SetWindowLongPtrW(hwnd_, GWL_STYLE, static_cast<LONG_PTR>(newStyle));
    }
    if (newExStyle != exStyle) {
        SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, static_cast<LONG_PTR>(newExStyle));
    }

    HWND owner = nullptr;
    if (resolvedBehavior_.owner == "main" && ownerHwnd_ && IsWindow(ownerHwnd_)) {
        owner = ownerHwnd_;
    } else if (resolvedBehavior_.noActivate || overlayIntent_ || createParams_.clickThrough) {
        // 与 Create 同口径：overlay / noActivate popup 改用 activation sink 作 owner。
        owner = WindowManager::GetInstance().GetActivationSinkHwnd();
    }
    SetWindowLongPtrW(hwnd_, GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(owner));

    SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0,
        SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);

    if (!resolvedBehavior_.allowMinimize && IsIconic(hwnd_)) {
        ShowWindow(hwnd_, resolvedBehavior_.noActivate ? SW_SHOWNOACTIVATE : SW_RESTORE);
    }
}

void PopupWindow::ApplyBackdropEffect(const std::string& effect, bool forceRefresh) {
    if (!hwnd_ || !IsWindow(hwnd_)) return;

    WindowChromeResolverSnapshot snapshot;
    snapshot.base = BuildChromeBaseState();
    snapshot.compatibility = chromeCompatibilityOverrides_;
    snapshot.standard = BuildChromeStandardState();
    snapshot.standard.backdropPolicy["activeEffect"] = effect;
    snapshot.standard.backdropPolicy["inactiveEffect"] = effect;
    snapshot.derived = BuildChromeDerivedState();

    WindowChromeApplyRequest request;
    request.state = WindowChromeResolver::Resolve(snapshot);
    request.active = isActive_;
    request.forceRefresh = forceRefresh;
    request.previousMode = lastBackdropMode_;
    request.previousEffect = lastBackdropEffect_;

    WindowChromeApplyResult result = WindowChromeApplier::Apply(BuildChromeApplyHooks(), request);
    if (result.success) {
        lastBackdropMode_ = result.mode;
        lastBackdropEffect_ = result.effect;
    }
}

bool PopupWindow::ApplyBackdropForActivation(bool active, bool forceRefresh) {
    WindowChromeApplyRequest request;
    request.state = resolvedChromeState_;
    request.active = active;
    request.forceRefresh = forceRefresh;
    request.previousMode = lastBackdropMode_;
    request.previousEffect = lastBackdropEffect_;

    WindowChromeApplyResult result = WindowChromeApplier::Apply(BuildChromeApplyHooks(), request);

    if (result.success) {
        lastBackdropMode_ = result.mode;
        lastBackdropEffect_ = result.effect;
    }
    return result.success;
}

bool PopupWindow::CanBroadcastBackdropStateChanged() const {
    return chromeBackdropBroadcastReady_ &&
        hwnd_ &&
        IsWindow(hwnd_) &&
        webView_ &&
        webView_->IsReady() &&
        WebViewContext::GetInstance().HasInstance(hwnd_);
}

const char* PopupWindow::GetTraceEffect(bool active) const {
    const std::string& effect = active
        ? resolvedChromeState_.effectiveActiveEffect
        : resolvedChromeState_.effectiveInactiveEffect;
    return effect.empty() ? "-" : effect.c_str();
}

void PopupWindow::TraceWindowPhase(const char* phase,
                                   const char* active,
                                   const char* forceRefresh,
                                   const char* effect,
                                   const char* previousEffect,
                                   const char* extra) const {
    WindowChromeTrace::EmitWindowPhase({
        .phase = phase,
        .windowKind = "popup",
        .defaultWindowId = "popup",
        .windowId = GetWindowId(),
        .hwnd = hwnd_,
        .startupRevealPending = startupRevealPending_,
        .startupRevealCommitted = startupRevealCommitted_,
        .startupRevealSettling = startupRevealSettling_,
        .isActive = isActive_,
        .active = active,
        .forceRefresh = forceRefresh,
        .effect = (effect && effect[0]) ? effect : GetTraceEffect(isActive_),
        .lastBackdropEffect = lastBackdropEffect_,
        .previousEffect = previousEffect,
        .extra = extra
    });
}

// ActivationEvidence 诊断日志
// 与 MainWindow.cpp 中 S_EmitEvidenceLine 输出 "[ActivationEvidence] source=..."
// 同口径，方便对齐时间线、跟踪「桌面歌词 popup 点击 → 主窗口被前置」焦点链。
// 仅对 noActivate / overlayIntent / clickThrough 类 popup 输出，避免污染普通 popup 日志。
void PopupWindow::EmitActivationEvidence(const char* phase, const std::string& extra) const {
    if (!resolvedBehavior_.noActivate && !overlayIntent_ && !clickThrough_) {
        return;
    }
    std::ostringstream os;
    os << "[ActivationEvidence] source=PopupWindow::" << (phase ? phase : "?")
       << " t=" << WindowChromeTrace::RelativeMs() << "ms"
       << " tid=" << ::GetCurrentThreadId()
       << " windowId=" << GetWindowId().c_str()
       << " hwnd=0x" << pfc::format_hex((size_t)hwnd_)
       << " fgHwnd=0x" << pfc::format_hex((size_t)::GetForegroundWindow())
       << " focusHwnd=0x" << pfc::format_hex((size_t)::GetFocus())
       << " activeHwnd=0x" << pfc::format_hex((size_t)::GetActiveWindow())
       << " captureHwnd=0x" << pfc::format_hex((size_t)::GetCapture())
       << " ownerHwnd=0x" << pfc::format_hex((size_t)::GetWindow(hwnd_, GW_OWNER))
       << " profile=" << profile_.c_str()
       << " noActivate=" << WindowChromeTrace::BoolText(resolvedBehavior_.noActivate)
       << " overlayIntent=" << WindowChromeTrace::BoolText(overlayIntent_)
       << " clickThrough=" << WindowChromeTrace::BoolText(clickThrough_)
       << " savedForeground=0x" << pfc::format_hex((size_t)lastForegroundBeforeClick_);
    if (!extra.empty()) {
        os << " " << extra;
    }
    WindowChromeTrace::EmitAuxiliaryLine(os.str());
}

void PopupWindow::BroadcastBehaviorChanged() const {
    json data = {
        {"windowId", GetWindowId()},
        {"profile", profile_},
        {"behavior", behaviorOverrides_},
        {"resolvedBehavior", BuildResolvedBehaviorJson()}
    };
    WebViewContext::GetInstance().BroadcastEvent("window:behaviorChanged", data);
}

void PopupWindow::BroadcastBackdropStateChanged(bool active, const std::string& mode,
                                                const std::string& effect) const {
    json data = {
        {"windowId", GetWindowId()},
        {"active", active},
        {"mode", mode},
        {"effect", effect}
    };
    WebViewContext::GetInstance().BroadcastEvent("window:backdropStateChanged", data);
}

json PopupWindow::BuildResolvedBehaviorJson() const {
    return {
        {"showInTaskbar", resolvedBehavior_.showInTaskbar},
        {"showInAltTab", resolvedBehavior_.showInAltTab},
        {"keepVisibleOnShowDesktop", resolvedBehavior_.keepVisibleOnShowDesktop},
        {"allowMinimize", resolvedBehavior_.allowMinimize},
        {"owner", resolvedBehavior_.owner},
        {"noActivate", resolvedBehavior_.noActivate}
    };
}

json PopupWindow::BuildResolvedBackdropPolicyJson() const {
    return {
        {"activeEffect", resolvedBackdropPolicy_.activeEffect},
        {"inactiveEffect", resolvedBackdropPolicy_.inactiveEffect},
        {"darkMode", resolvedBackdropPolicy_.darkMode},
        {"reapplyOnActivate", resolvedBackdropPolicy_.reapplyOnActivate}
    };
}

json PopupWindow::GetPopupBehaviorInfo() const {
    return {
        {"profile", profile_},
        {"behavior", behaviorOverrides_},
        {"resolvedBehavior", BuildResolvedBehaviorJson()}
    };
}

json PopupWindow::GetBackdropPolicyInfo() const {
    return {
        {"backdropPolicy", backdropPolicyOverrides_},
        {"resolvedBackdropPolicy", BuildResolvedBackdropPolicyJson()}
    };
}

json PopupWindow::GetResolvedBehavior() const {
    return BuildResolvedBehaviorJson();
}

json PopupWindow::GetResolvedBackdropPolicy() const {
    return BuildResolvedBackdropPolicyJson();
}

bool PopupWindow::UpdatePopupBehavior(const std::string& profile, bool hasProfile,
                                      const json& behaviorPatch, bool hasBehaviorPatch,
                                      std::string& error) {
    if (hasProfile) {
        std::string normalized = NormalizeProfile(profile);
        if (normalized.empty()) {
            error = "Invalid profile, expected 'standard' or 'miniPlayer'";
            return false;
        }
        hasProfile_ = true;
        profile_ = normalized;
    }

    if (hasBehaviorPatch) {
        if (!behaviorPatch.is_object()) {
            error = "behavior must be an object";
            return false;
        }
        for (auto it = behaviorPatch.begin(); it != behaviorPatch.end(); ++it) {
            if (it.value().is_null()) {
                behaviorOverrides_.erase(it.key());
            } else {
                behaviorOverrides_[it.key()] = it.value();
            }
        }
    }

    ResolvePolicies();
    ApplyResolvedBehaviorRuntime();
    ApplyBackdropForActivation(isActive_, true);
    BroadcastBehaviorChanged();
    return true;
}

bool PopupWindow::UpdateBackdropPolicy(const json& backdropPolicyPatch, std::string& error) {
    if (!backdropPolicyPatch.is_object()) {
        error = "backdropPolicy must be an object";
        return false;
    }
    for (auto it = backdropPolicyPatch.begin(); it != backdropPolicyPatch.end(); ++it) {
        if (it.value().is_null()) {
            backdropPolicyOverrides_.erase(it.key());
        } else {
            backdropPolicyOverrides_[it.key()] = it.value();
        }
    }

    ResolvePolicies();
    return ApplyBackdropForActivation(isActive_, true);
}

// ============================================
// URL 构建
// ============================================

std::wstring PopupWindow::BuildNavigationUrl() const {
    std::string windowId = GetWindowId();
    std::string urlParam = createParams_.url;

    // 绝对 URL 直通（http/https/file/data）— 只追加 windowId query param
    auto startsWithCI = [](const std::string& s, const char* prefix) {
        size_t len = std::strlen(prefix);
        return s.size() >= len && _strnicmp(s.c_str(), prefix, len) == 0;
    };
    if (startsWithCI(urlParam, "http://") || startsWithCI(urlParam, "https://") ||
        startsWithCI(urlParam, "file:///") || startsWithCI(urlParam, "data:")) {
        // data: URI 不追加 query param（格式不支持）
        if (startsWithCI(urlParam, "data:")) {
            return pfc::stringcvt::string_wide_from_utf8(urlParam.c_str()).get_ptr();
        }
        char separator = (urlParam.find('?') != std::string::npos) ? '&' : '?';
        std::string url = urlParam + separator + "windowId=" + windowId;
        return pfc::stringcvt::string_wide_from_utf8(url.c_str()).get_ptr();
    }

    // 合并开发模式两层判断为一层 guard
    const char* devServerUrl = security_config::UseDevServer()
        ? security_config::GetDevServerUrl() : nullptr;

    if (devServerUrl && devServerUrl[0]) {
        std::string url(devServerUrl);
        bool isHtmlFile = (urlParam.find(".html") != std::string::npos);

        if (isHtmlFile) {
            if (url.back() != '/') url += '/';
            url += urlParam;
            url += "?windowId=" + windowId;
        } else {
            char separator = (url.find('?') != std::string::npos) ? '&' : '?';
            url += separator;
            url += "windowId=" + windowId;
            if (!urlParam.empty()) {
                url += "&route=" + urlParam;
            }
        }
        return pfc::stringcvt::string_wide_from_utf8(url.c_str()).get_ptr();
    }

    // 生产模式：使用虚拟主机名
    bool isHtmlFile = (urlParam.find(".html") != std::string::npos);
    std::string url;

    if (isHtmlFile) {
        url = "https://foo-ui-webview2.local/" + urlParam + "?windowId=" + windowId;
    } else {
        url = "https://foo-ui-webview2.local/index.html?windowId=" + windowId;
        if (!urlParam.empty()) {
            url += "&route=" + urlParam;
        }
    }

    return pfc::stringcvt::string_wide_from_utf8(url.c_str()).get_ptr();
}

// ============================================
// WindowShellBase 实现
// PopupWindow 作为 shell adapter 接入统一抽象
// ============================================

std::string PopupWindow::GetShellWindowId() const {
    return GetWindowId();
}

WindowKind PopupWindow::GetWindowKind() const {
    return WindowKind::Popup;
}

HWND PopupWindow::GetShellHwnd() const {
    return GetHwnd();
}

WindowShellCapabilities PopupWindow::GetCapabilities() const {
    WindowShellCapabilities caps;
    caps.supportsFullscreen = true;
    caps.supportsBackdropPolicy = true;
    caps.supportsCornerPreference = false;
    caps.supportsMicaAlt = false;
    caps.supportsOwnerPolicy = true;
    caps.supportsNoActivate = true;
    caps.participatesInAppBootstrap = false;
    caps.supportsBeforeClose = true;
    caps.windowKind = WindowKind::Popup;
    return caps;
}

WindowShellSnapshot PopupWindow::GetShellSnapshot() const {
    WindowShellSnapshot snap;
    snap.windowId = GetWindowId();
    snap.kind = WindowKind::Popup;
    snap.capabilities = GetCapabilities();

    snap.lifecycle.created = (hwnd_ != nullptr);
    snap.lifecycle.visible = hwnd_ && IsWindowVisible(hwnd_);
    snap.lifecycle.active = isActive_;
    snap.lifecycle.minimized = isMinimized_;
    snap.lifecycle.maximized = isMaximized_;
    snap.lifecycle.fullscreen = IsFullscreen();
    snap.lifecycle.pendingDestroy = pendingClose_;

    snap.startupPresentation.navigationCompleted = startupNavigationCompleted_;
    snap.startupPresentation.windowReadySignaled = startupReadySignaled_;
    snap.startupPresentation.revealCommitted = startupRevealCommitted_;

    snap.chrome.resolved = resolvedChromeState_;
    return snap;
}

bool PopupWindow::PatchBackdropPolicy(const json& policyPatch, std::string& error) {
    return UpdateBackdropPolicy(policyPatch, error);
}

void PopupWindow::PatchFrameless(bool frameless) {
    SetFrameless(frameless);
}

void PopupWindow::RefreshChrome() {
    ApplyBackdropForActivation(isActive_, true);
}

bool PopupWindow::PatchCompatibilityBackdrop(const std::optional<std::string>& effect,
                                             const std::optional<bool>& darkMode,
                                             bool clearBlur, bool forceRefresh) {
    return UpdateCompatibilityBackdropEffect(effect, darkMode, clearBlur, forceRefresh);
}

bool PopupWindow::PatchCompatibilityBlur(bool enabled, bool forceRefresh) {
    return UpdateCompatibilityBlur(enabled, forceRefresh);
}

bool PopupWindow::PatchCompatibilityDarkMode(bool enabled, bool forceRefresh) {
    return UpdateCompatibilityDarkMode(enabled, forceRefresh);
}

bool PopupWindow::PatchCompatibilityTransparency(bool transparent, bool forceRefresh) {
    return UpdateCompatibilityTransparentBackground(transparent, forceRefresh);
}

bool PopupWindow::IsFullscreen() const {
    return savedWindowInfo_.has_value();
}

void PopupWindow::NotifyFullscreenChanged(bool isFullscreen) {
    // 重新解析 chrome state 并应用（fullscreen 状态影响 WindowChromeResolver 路径选择）
    ApplyResolvedChrome(true);
    // 广播 window:stateChanged 事件通知前端
    json stateData = {
        {"isMaximized",  isMaximized_},
        {"isMinimized",  isMinimized_},
        {"maximized",    isMaximized_},
        {"minimized",    isMinimized_},
        {"isActive",     isActive_},
        {"active",       isActive_},
        {"isFullscreen", isFullscreen},
        {"fullscreen",   isFullscreen}
    };
    if (bridge_) {
        WebViewContext::GetInstance().BroadcastEvent("window:stateChanged", stateData);
    }
}

void PopupWindow::SetFullscreenFlag(bool /*isFullscreen*/) {
    // savedWindowInfo_ 已由 EnterFullscreenMode/ExitFullscreenMode 直接管理
    // 此处无需额外操作
}

bool PopupWindow::IsActive() const {
    return isActive_;
}
