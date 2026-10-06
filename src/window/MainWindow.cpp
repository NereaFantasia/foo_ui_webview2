#include "pch.h"
#include <wtsapi32.h>            // 会话锁定/解锁通知（锁屏即释放内存）
#pragma comment(lib, "wtsapi32.lib")
#include "window/MainWindow.h"
#include "window/ChromeController.h"
#include "window/MenuOverlayHost.h"
#include "window/WindowBehaviorTrace.h"
#include "window/WindowChromeResolver.h"
#include "window/WindowChromeTrace.h"
#include "utils/I18n.h"
#include "webview/WebViewHost.h"
#include "webview/WebViewCrashPolicy.h"
#include "settings/SecurityConfig.h"
#include "settings/WindowStateConfig.h"
#include "api/BridgeCore.h"
#include "api/EventEmit.h"
#include "api/generated/WindowSchema.h"
#include "core/WebViewContext.h"
#include "api/PlaybackApi.h"
#include "api/ConfigApi.h"
#include "api/PlaylistApi.h"
#include "api/LibraryApi.h"
#include "api/WindowApi.h"
#include "api/ArtworkApi.h"
#include "api/PluginRegistry.h"
// New universal APIs
#include "api/FileApi.h"
#include "api/DialogApi.h"
#include "api/ClipboardApi.h"
#include "api/ShellApi.h"
#include "api/HttpApi.h"
#include "api/KeyboardApi.h"
#include "api/UiApi.h"
#include "api/LyricsApi.h"
#include "api/MetadataApi.h"
#include "api/AudioApi.h"
#include "api/ConsoleApi.h"
#include "api/DndApi.h"
#include "api/QueueApi.h"
#include "api/DiscoveryApi.h"
#include "api/ReplayGainApi.h"
#include "api/PlaycountApi.h"
#include "api/TitleformatApi.h"
#include "callbacks/PlaybackCallback.h"
#include "callbacks/PlaylistCallback.h"
#include "callbacks/LibraryCallback.h"
#include "callbacks/MetadbCallback.h"
#include "window/TrayIcon.h"
#include "window/TaskbarIntegration.h"
#include <windowsx.h>  // For GET_X_LPARAM, GET_Y_LPARAM
#include <foobar2000/SDK/menu_helpers.h>  // For standard_commands
#include <uxtheme.h>  // For SetWindowTheme
#include <shellapi.h>  // For ShellExecuteW
#include <fstream>
#include <sstream>
#include <iomanip>
#include <array>
#include <algorithm>
#include <cctype>
#include "utils/WindowUtils.h"
#include "prefs/PreferencesPage.h"
#include "window/WindowManager.h"
#include "window/WindowGeometryMath.h"
#include "window/WindowDpiProbe.h"
#include "window/MainWindowInternal.h"

#pragma comment(lib, "uxtheme.lib")

using namespace mainwindow_detail;

bool MainWindow::classRegistered_ = false;

MainWindow::MainWindow() = default;

MainWindow::~MainWindow() {
    Destroy();
}

bool MainWindow::RegisterWindowClass() {
    if (classRegistered_) return true;
    
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = sizeof(MainWindow*);
    wc.hInstance = core_api::get_my_instance();
        try { static_api_ptr_t<ui_control> fb_ui; wc.hIcon = fb_ui->get_main_icon(); wc.hIconSm = fb_ui->get_main_icon(); } catch (...) {}
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        // 设置为 NULL 让 DWM 处理窗口背景（Mica/Acrylic 效果穿透）
        wc.hbrBackground = nullptr;
        wc.lpszMenuName = nullptr;
        wc.lpszClassName = CLASS_NAME;
    
    if (!RegisterClassExW(&wc)) {
        DWORD error = GetLastError();
        if (error != ERROR_CLASS_ALREADY_EXISTS) {
            LOG("Failed to register window class, error: ", error);
            return false;
        }
    }
    
    classRegistered_ = true;
    return true;
}

HWND MainWindow::Create(HWND parent) {
    if (!RegisterWindowClass()) {
        return nullptr;
    }
    
    parentHwnd_ = parent;
    
    // 把物理口径的默认尺寸下限迁移为 DIP 存储。
    // 必须先于 RestoreWindowPosition——后者会用这些约束钳制保存的尺寸。
    SeedDefaultConstraintsFromInitialDpi();
    
    // 恢复保存的窗口位置，或使用默认值
    int x, y, width, height, showCmd;
    RestoreWindowPosition(x, y, width, height, showCmd);
    
    LOG("Create() - Restored position: x=", x, " y=", y, " w=", width, " h=", height, " showCmd=", showCmd);
    
    // 保存恢复的 showCmd，因为创建时需要用 SW_HIDE
    int savedShowCmd = showCmd;
    isMaximized_ = (savedShowCmd == SW_SHOWMAXIMIZED);
    isMinimized_ = (savedShowCmd == SW_SHOWMINIMIZED ||
        savedShowCmd == SW_SHOWMINNOACTIVE ||
        savedShowCmd == SW_MINIMIZE);
    savedShowCmd_ = savedShowCmd;
    startupPresentationCoordinator_.Reset();
    SyncStartupRevealProjection();
    startupSurfaceAuthorityPending_ = true;
    startupSurfaceCommitPending_ = true;
    pendingStartupSurfaceBounds_ = RECT{0, 0, 0, 0};
    hasPendingStartupSurfaceBounds_ = false;
    startupSurfaceFirstPresentCommitted_ = false;
    
    // Create top-level window (foobar2000 user_interface expects this)
    // 创建时使用 WS_OVERLAPPEDWINDOW 保持 DWM 标准窗口动画和 backdrop 合成初始化。
    // OnCreate 中移除 WS_SYSMENU 消除 DWM 三大键按钮 overlay，
    // 保留 WS_CAPTION（动画）和 WS_MINIMIZEBOX/WS_MAXIMIZEBOX（Shell 行为）。
    DWORD style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
    // WS_EX_NOREDIRECTIONBITMAP: 关键！让窗口不渲染到重定向表面
    // 这样 DWM 效果 (Mica/Acrylic) 可以正确穿透 WebView2 显示
    // 即使在窗口 resize 时也能保持透明效果
    DWORD exStyle = WS_EX_APPWINDOW | WS_EX_NOREDIRECTIONBITMAP;
    
    startupImmediateShowMode_ = false;  // cold-start reveal：延迟显示直到 WebView 就绪或短超时

    hwnd_ = CreateWindowExW(
        exStyle,                            // Extended style
        CLASS_NAME,                         // Window class
        L"foobar2000",                      // Window title
        style,                              // Style
        x, y,                               // Position
        width, height,                      // Size
        nullptr,                            // Parent (nullptr for top-level)
        nullptr,                            // Menu
        core_api::get_my_instance(),        // Instance
        this                                // Pass this pointer
    );
    
    if (!hwnd_) {
        LOG("Failed to create window, error: ", GetLastError());
        return nullptr;
    }

    S_LogStartupProbeIdentity("MainWindow::Create.afterCreateWindowEx", hwnd_);
    
    // 强制重新计算非客户区，应用客户区扩展
    SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0,
        SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    
    // 保留 maximized placement 语义：仅存储 rcNormalPosition（用户取消最大化时的恢复位置），
    // 不显示窗口。showCmd 使用 SW_HIDE 防止 SetWindowPlacement 使窗口可见。
    // 实际最大化和显示由 TryCommitStartupReveal 的 ShowWindow(SW_SHOWMAXIMIZED) 完成。
    if (savedShowCmd == SW_SHOWMAXIMIZED) {
        WINDOWPLACEMENT wp = { sizeof(WINDOWPLACEMENT) };
        GetWindowPlacement(hwnd_, &wp);
        wp.rcNormalPosition = { x, y, x + width, y + height };
        wp.showCmd = SW_HIDE;
        wp.flags = 0;
        SetWindowPlacement(hwnd_, &wp);
    }

    // Cold-start reveal：不在 Create 中 ShowWindow。
    // 窗口保持隐藏，等待 WebView 首帧就绪或短超时后由 TryCommitStartupReveal 显示。
    // foobar2000 消息泵持续驱动，user_interface::create 同步返回 HWND。
    // 启动短超时 timer：WebView 在超时内就绪则首帧即内容；否则退化为空 Mica 窗口。
    if (hwnd_ && IsWindow(hwnd_)) {
        SetTimer(hwnd_, SHORT_REVEAL_TIMER_ID, SHORT_REVEAL_TIMEOUT_MS, nullptr);
        LOG("MainWindow: Short reveal timer armed (", SHORT_REVEAL_TIMEOUT_MS, "ms)");
    }

    LOG("Window created with cold-start reveal, savedShowCmd=", savedShowCmd,
        " isMaximized=", isMaximized_);
    
    return hwnd_;
}

void MainWindow::Destroy() {
    if (webView_) {
        webView_.reset();
    }
    
    if (hwnd_ && IsWindow(hwnd_)) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

LRESULT CALLBACK MainWindow::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    MainWindow* pThis = nullptr;
    
    if (msg == WM_NCCREATE) {
        CREATESTRUCTW* pCreate = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = reinterpret_cast<MainWindow*>(pCreate->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        pThis->hwnd_ = hwnd;
    } else {
        pThis = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    
    if (pThis) {
        return pThis->HandleMessage(msg, wParam, lParam);
    }
    
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT MainWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    // TaskbarButtonCreated 在 Explorer 重启或首次出现时初始化 ITaskbarList3
    {
        UINT tbcMsg = TaskbarIntegration::GetInstance().GetTaskbarCreatedMsg();
        if (tbcMsg != 0 && msg == tbcMsg) {
            TaskbarIntegration::GetInstance().Initialize(hwnd_);
            TrayIcon::GetInstance().RecreateAfterShellRestart();
            return 0;
        }
    }

    // 页面自绘最大化键上的非客户区鼠标消息先于 DwmDefWindowProc 处理：它们要转给
    // 页面，按下也不能交给默认处理。只在无标题栏时：有系统标题栏时 HTMAXBUTTON
    // 是 Windows 自己的按钮。
    if (frameless_) {
        LRESULT maximizeButtonResult = 0;
        if (HandleMaximizeButtonMessage(msg, wParam, lParam, maximizeButtonResult)) {
            return maximizeButtonResult;
        }
    }

    // [Chrome Reconciliation] DwmDefWindowProc 分阶段路由。
    // Bootstrap 阶段：全量调用（WM_NCHITTEST 除外），帮助 backdrop 初始化。
    // SteadyState 阶段：过滤 NC visual 消息，防止三大键复发。
    if (frameless_ && ShouldRouteToDwmDefWindowProc(msg)) {
        LRESULT dwmResult;
        if (DwmDefWindowProc(hwnd_, msg, wParam, lParam, &dwmResult)) {
            return dwmResult;
        }
    }

    switch (msg) {
        // ---- 托盘图标回调 ----
        case TrayIcon::kCallbackMessage:
            return TrayIcon::GetInstance().HandleTrayCallback(wParam, lParam);

        case WM_NCCALCSIZE:
            // 无标题栏模式：完全移除非客户区（标题栏和边框）
            if (frameless_ && wParam == TRUE) {
                // 全屏模式下不需要任何边距调整
                if (IsWindowFullscreen()) {
                    return 0;
                }
                
                auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
                
                if (IsZoomed(hwnd_)) {
                    // [最大化裁切修复] 将客户区精确对齐到监视器工作区。
                    // WM_GETMINMAXINFO 把窗口尺寸向四周扩展了 frame 厚度，
                    // 使 WS_THICKFRAME 边框被推到屏幕外不可见。
                    // 此处把客户区收回到工作区，excess frame 成为 off-screen NC 区。
                    // 三大键消除由 WS_SYSMENU 移除 + DwmDefWindowProc 路由保证，
                    // 不依赖 NC=0。（Chromium 同款做法）
                    HMONITOR hMon = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
                    if (hMon) {
                        MONITORINFO mi = { sizeof(MONITORINFO) };
                        if (GetMonitorInfo(hMon, &mi)) {
                            params->rgrc[0] = mi.rcWork;

                            auto hasAutoHideBar = [&](UINT edge) -> bool {
                                APPBARDATA abd = { sizeof(APPBARDATA) };
                                abd.uEdge = edge;
                                abd.rc = mi.rcMonitor;
                                return reinterpret_cast<HWND>(
                                    SHAppBarMessage(ABM_GETAUTOHIDEBAREX, &abd)) != nullptr;
                            };
                            constexpr int kAutoHideGap = 2;
                            if (hasAutoHideBar(ABE_TOP))    params->rgrc[0].top    += kAutoHideGap;
                            if (hasAutoHideBar(ABE_BOTTOM)) params->rgrc[0].bottom -= kAutoHideGap;
                            if (hasAutoHideBar(ABE_LEFT))   params->rgrc[0].left   += kAutoHideGap;
                            if (hasAutoHideBar(ABE_RIGHT))  params->rgrc[0].right  -= kAutoHideGap;
                        }
                    }
                }
                return 0;
            }
            // 有标题栏模式：交给 DefWindowProc 绘制系统标题栏
            if (!frameless_) break;
            // 无标题栏且 wParam 为 FALSE：lParam 指向的矩形原样留作客户区，与上面
            // 非客户区为 0 的处理一致。必须在这里返回：往下会落进 WM_NCHITTEST，
            // 把矩形指针当成光标坐标，再把命中码当作本消息的返回值。
            return 0;

        case WM_NCHITTEST: {
            // 有标题栏模式：交给 DefWindowProc 处理系统标题栏点击测试
            if (!frameless_) break;
            
            // 使用窗口坐标（不是客户区坐标）
            // 这样即使 WebView 覆盖整个客户区，WM_NCHITTEST 仍会在消息分发前被处理
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            
            // 获取窗口矩形（屏幕坐标）
            RECT rcWindow;
            GetWindowRect(hwnd_, &rcWindow);
            
            // 边缘和角落的判定范围（视觉上仍为 1-2px，但点击判定范围更大）
            // CORNER_HIT_WIDTH: 角落判定范围，更大便于用户点击
            // EDGE_HIT_WIDTH: 边缘判定范围，与 WebView 内缩一致
            // isFullscreen_ 时禁用 resize hit-zone，否则 EnterFullscreenMode 移除
            // WS_THICKFRAME 后系统 hit-test 不再触发 SC_SIZE，但本自定义路径
            // 仍会返回 HT* 边缘代码导致 Windows 启动 SC_SIZE 模态 loop
            const bool canResize = resizable_ && !isMaximized_ && !isFullscreen_;
            const int CORNER_HIT_WIDTH = canResize ? 8 : 0;
            const int EDGE_HIT_WIDTH = canResize ? 4 : 0;
            
            // 判断鼠标位置是否在角落区域（使用更大的判定范围）
            bool cornerTop    = pt.y < rcWindow.top + CORNER_HIT_WIDTH;
            bool cornerBottom = pt.y >= rcWindow.bottom - CORNER_HIT_WIDTH;
            bool cornerLeft   = pt.x < rcWindow.left + CORNER_HIT_WIDTH;
            bool cornerRight  = pt.x >= rcWindow.right - CORNER_HIT_WIDTH;
            
            // 1. 处理四个角（优先级最高，使用更大的判定范围）
            if (cornerTop && cornerLeft)     return HTTOPLEFT;
            if (cornerTop && cornerRight)    return HTTOPRIGHT;
            if (cornerBottom && cornerLeft)  return HTBOTTOMLEFT;
            if (cornerBottom && cornerRight) return HTBOTTOMRIGHT;
            
            // 判断鼠标位置是否在边缘区域（使用较小的判定范围）
            bool edgeTop    = pt.y < rcWindow.top + EDGE_HIT_WIDTH;
            bool edgeBottom = pt.y >= rcWindow.bottom - EDGE_HIT_WIDTH;
            bool edgeLeft   = pt.x < rcWindow.left + EDGE_HIT_WIDTH;
            bool edgeRight  = pt.x >= rcWindow.right - EDGE_HIT_WIDTH;
            
            // 2. 处理四条边
            if (edgeTop)    return HTTOP;
            if (edgeBottom) return HTBOTTOM;
            if (edgeLeft)   return HTLEFT;
            if (edgeRight)  return HTRIGHT;
            
            // 3. 标题栏区域（转换为客户区坐标检测）
            POINT clientPt = pt;
            ScreenToClient(hwnd_, &clientPt);
            
            RECT clientRect;
            GetClientRect(hwnd_, &clientRect);
            int width = clientRect.right;

            // 页面自绘的最大化键：答 HTMAXBUTTON，Windows 11 悬停时才弹贴靠布局。
            // 排在 CSS 区域查询之前，因为按钮通常在 no-drag 区里，那条路会答 HTCLIENT。
            if (IsPointInMaximizeButton(clientPt.x, clientPt.y)) {
                return HTMAXBUTTON;
            }
            
            // ============================================
            // 使用 WebView2 的 CSS app-region 查询 (v1.1.5+)
            // 这让 -webkit-app-region: drag/no-drag 真正生效
            // 特别解决 Teleport 到 body 的元素无法被父级 DOM 树检测的问题
            // ============================================
            if (webView_ && webView_->IsReady()) {
                int regionKind = webView_->GetNonClientRegionAtPoint(clientPt);
                if (regionKind == 1) {
                    // CSS -webkit-app-region: drag → 可拖拽
                    return HTCAPTION;
                } else if (regionKind == 0 && clientPt.y < titlebarHeight_) {
                    // CSS -webkit-app-region: no-drag 或无设置 → 检查是否在标题栏区域
                    // 如果在标题栏高度内但 WebView 说是客户区，则让 WebView 处理（按钮可点击）
                    return HTCLIENT;
                }
                // regionKind == 2 表示不在 WebView 内，继续默认逻辑
            }

            // 检查是否在自定义非拖拽区域（如标题栏按钮）
            if (IsPointInNoDragRegion(clientPt.x, clientPt.y)) {
                return HTCLIENT;
            }
            
            // 检查是否在自定义拖拽区域
            if (IsPointInDragRegion(clientPt.x, clientPt.y)) {
                return HTCAPTION;
            }
            
            // 检查是否有自定义拖拽区域设置
            bool hasCustomDragRegions = false;
            {
                std::lock_guard<std::mutex> lock(regionsMutex_);
                hasCustomDragRegions = !dragRegions_.empty();
            }
            
            // 标题栏区域（高度 titlebarHeight_）
            // 只有在没有自定义拖拽区域时，才使用默认标题栏拖拽逻辑
            if (!hasCustomDragRegions && clientPt.y < titlebarHeight_) {
                // 检查是否在系统按钮区域（右侧 138px = 3*46）
                int captionButtonsWidth = captionButtonWidth_ * 3;
                if (clientPt.x >= width - captionButtonsWidth) {
                    // 让 WebView 处理按钮点击
                    return HTCLIENT;
                }
                // 可拖拽区域
                return HTCAPTION;
            }
            
            // 4. 其余区域交给 WebView (HTCLIENT)
            return HTCLIENT;
        }
        
        // 非客户区按下左键：全屏时先退出全屏再继续 move/resize。
        // 覆盖路径：
        //   - HTCAPTION 拖动（系统标题栏 / -webkit-app-region: drag / Visual Hosting / window.startDrag API）
        //   - HTLEFT/HTRIGHT/HTTOP/HTBOTTOM/HT*CORNER 边缘 resize（window.startResize API、第三方 SendMessage）
        // 全屏时 ExitFullscreenMode 必须被显式调用：
        //   - 恢复 savedWindowInfo_（清 WS_EX_TOPMOST 等）
        //   - 触发 NotifyFullscreenChanged(false) 广播 window:stateChanged
        //   - 否则窗口虽然视觉上 resize 走出全屏尺寸，但 isFullscreen_ / TOPMOST 残留。
        // 参考 Chrome / Edge / Electron / Tauri 行为。
        case WM_NCLBUTTONDOWN:
            if (IsWindowFullscreen()) {
                switch (wParam) {
                    case HTCAPTION:
                    case HTLEFT: case HTRIGHT: case HTTOP: case HTBOTTOM:
                    case HTTOPLEFT: case HTTOPRIGHT:
                    case HTBOTTOMLEFT: case HTBOTTOMRIGHT:
                        ExitFullscreenIfActive(hwnd_);
                        // 不 return 0：让 DefWindowProc 继续处理对应 modal loop
                        // (HTCAPTION → SC_MOVE, HT*Edge → SC_SIZE)。
                        // ExitFullscreen 已恢复 WS_THICKFRAME，size loop 可正常接管。
                        break;
                    default:
                        break;
                }
            }
            break;
        
        // 双击标题栏最大化/还原（仅无标题栏模式）
        case WM_NCLBUTTONDBLCLK:
            if (frameless_ && wParam == HTCAPTION) {
                // 全屏模式下双击标题栏退出全屏（参考 Electron / Tauri 行为）
                if (IsWindowFullscreen()) {
                    ExitFullscreenIfActive(hwnd_);
                    return 0;
                }

                // setResizable(false) 的窗口没有最大化语义（与 WS_MAXIMIZEBOX 被移除一致，
                // 亦与 PopupWindow::HandleDragAreaMouse 对齐）。HTCAPTION 可能来自 CSS
                // -webkit-app-region: drag，前端拦不住非客户区消息，必须在此吞掉，
                // 否则 ShowWindow(SW_MAXIMIZE) 会无视样式位直接放大固定尺寸的窗口。
                if (!resizable_) {
                    return 0;
                }

                if (isMaximized_) {
                    ShowWindow(hwnd_, SW_RESTORE);
                } else {
                    ShowWindow(hwnd_, SW_MAXIMIZE);
                }
                return 0;
            }
            break;
        
        // 系统菜单 / 键盘快捷键路径的 maximize/restore：全屏时先退出全屏
        // 覆盖 Alt+Space 系统菜单、frame=true 模式 DefWindowProc 双击标题栏、
        // 任何 PostMessage(WM_SYSCOMMAND, SC_MAXIMIZE/SC_RESTORE) 等路径。
        // API handler 层（WindowMaximize/Restore/ToggleMaximize）已在 PostMessage 之前
        // 调用 ExitFullscreenIfActive，到达此处时已不在全屏，本拦截器不会重复触发。
        case WM_SYSCOMMAND: {
            const UINT cmd = static_cast<UINT>(wParam) & 0xFFF0;
            // 最小化/关闭到托盘
            if (cmd == SC_MINIMIZE && TrayIcon::GetInstance().GetMinimizeToTray()) {
                HideWindowToTray();
                return 0;
            }
            if (cmd == SC_CLOSE && TrayIcon::GetInstance().GetCloseToTray()) {
                HideWindowToTray();
                return 0;
            }
            if ((cmd == SC_MAXIMIZE || cmd == SC_RESTORE) && IsWindowFullscreen()) {
                ExitFullscreenIfActive(hwnd_);
                if (cmd == SC_RESTORE) {
                    // restore 语义已由 ExitFullscreen 满足，阻断后续 SC_RESTORE
                    return 0;
                }
                // SC_MAXIMIZE 让 DefWindowProc 继续处理：若 ExitFullscreen 已恢复 maximize 则 no-op
                break;
            }
            break;
        }
        
        // 右键标题栏显示系统菜单
        case WM_NCRBUTTONUP:
            if (wParam == HTCAPTION) {
                POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                ShowSystemMenu(pt.x, pt.y);
                return 0;
            }
            break;
            
        case WM_CREATE:
            OnCreate();
            // 注册会话锁定/解锁通知：锁屏=必然不可见，借此 SetVisible(false)+深挂起
            //（TrySuspend，回退 Low）省内存，解锁复原。零误判风险（锁屏期间窗口绝不可见）。
            WTSRegisterSessionNotification(hwnd_, NOTIFY_FOR_THIS_SESSION);
            return 0;
            
        case WM_CLOSE:
            // closeToTray 时，任何关闭路径（含任务栏右键"关闭窗口"、缩略图关闭等
            // 直接发 WM_CLOSE 的来源）都应隐藏到托盘而非退出 foobar2000，与 SC_CLOSE
            // 分支语义一致。真正的"退出"由托盘 Exit 菜单 / foobar File→Exit 直接执行
            // guid_main_exit（不经此路），故不会误把退出拦成隐藏。
            if (TrayIcon::GetInstance().GetCloseToTray()) {
                HideWindowToTray();
                return 0;
            }
            // When window is closed, exit foobar2000 properly
            OnClose();
            return 0;
            
        case WM_DESTROY:
            LogWebViewLifecycle("window.destroy");
            WTSUnRegisterSessionNotification(hwnd_);
            OnDestroy();
            return 0;

        case WM_WTSSESSION_CHANGE:
            // 锁屏 → 隐藏 WebView 释放内存；解锁 → 复原。
            if (wParam == WTS_SESSION_LOCK) {
                LogWebViewLifecycle("session.lock");
                SetBgSuspend(kBgSuspendLocked, true, "session-lock");
            } else if (wParam == WTS_SESSION_UNLOCK) {
                LogWebViewLifecycle("session.unlock");
                SetBgSuspend(kBgSuspendLocked, false, "session-unlock");
            }
            return 0;

        case WM_SHOWWINDOW: {
            std::string extra = std::string("show=") + WindowChromeTrace::BoolText(wParam != FALSE) +
                " status=" + std::to_string((long long)lParam);
            TraceWindowPhase("WM_SHOWWINDOW", nullptr, nullptr, nullptr, nullptr, extra.c_str());
            // 物理隐藏的唯一通用信号：最小化/最大化不发本消息，只有 ShowWindow(SW_HIDE)
            // 会到达这里，故无需枚举各隐藏调用方。
            if (wParam == FALSE) {
                dwmCompositionResetPending_ = true;
            }
            if (wParam != FALSE) {
                // 托盘恢复的统一入口：所有重新显示窗口的路径（WebViewUI::activate /
                // window.focus / BackgroundService::ShowWindow / 任务栏）都经过这里，
                // 清 kBgSuspendTrayHidden 并按投影恢复页面（挂起态经 put_IsVisible(TRUE)
                // 自动 resume）。未设置该 reason 时是 no-op：背景窗口 show、启动 reveal
                // 不受影响。此刻窗口尚未真正可见，RestoreSurfaceAfterHidden 可能拒绝，
                // SetBgSuspend 内部会兜底 SetVisible(true)；完整 chrome/surface 收敛由
                // 各恢复调用方既有的 RestoreSurfaceAfterHidden 后置调用完成。
                SetBgSuspend(kBgSuspendTrayHidden, false, "window-shown");
            }
            if (wParam != FALSE && !isMinimized_ &&
                !startupRevealPending_ && startupRevealCommitted_ && !startupRevealSettling_) {
                ApplyResolvedChrome(true);
            }
            break;
        }

        case WM_ENTERSIZEMOVE: {
            interactiveSizingActive_ = true;
            ++interactiveSizingSessionId_;
            hasPendingInteractiveResize_ = false;
            lastInteractiveResizeQpc_ = 0;
            const std::string extra = std::string("session=") + std::to_string(interactiveSizingSessionId_) +
                " interactiveSizingActive=" + WindowChromeTrace::BoolText(interactiveSizingActive_);
            TraceWindowPhase("WM_ENTERSIZEMOVE", nullptr, nullptr, nullptr, nullptr, extra.c_str());
            LogSurfaceDiagnostics("WM_ENTERSIZEMOVE");
            EmitInteractiveResizeEvidence("WM_ENTERSIZEMOVE", "enter", nullptr, nullptr);
            CaptureRuntimeDomProbe("WM_ENTERSIZEMOVE/+0ms", 0);
            break;
        }

        case WM_SIZING: {
            // 本 case 全部内容都是诊断：拖拽期间逐帧触发，trace 关闭时连 extra
            // 字符串都不应构造（TraceWindowPhase 内部门控在链路末端，不足以省下拼装）。
            if (WindowChromeTrace::AuxiliaryTraceEnabled()) {
                const RECT* sizingRect = reinterpret_cast<const RECT*>(lParam);
                const char* edgeName = S_GetSizingEdgeName(wParam);
                const std::string extra = std::string("edge=") + edgeName +
                    " session=" + std::to_string(interactiveSizingSessionId_) +
                    " sizingRect=" + (sizingRect ? S_FormatNativeRect(*sizingRect) : std::string("N/A"));
                TraceWindowPhase("WM_SIZING", nullptr, nullptr, nullptr, nullptr, extra.c_str());
                EmitInteractiveResizeEvidence("WM_SIZING", edgeName, sizingRect, nullptr);
            }
            break;
        }

        case WM_WINDOWPOSCHANGING: {
            // 与 WM_SIZING 同理：整条 case 都是诊断，且本消息在拖拽/移动期间
            // 频率高于 WM_SIZING，故把 extra 构造一并纳入门控。
            if (WindowChromeTrace::AuxiliaryTraceEnabled()) {
                const WINDOWPOS* windowPos = reinterpret_cast<const WINDOWPOS*>(lParam);
                std::string extra;
                if (windowPos) {
                    const std::string flagsHex = S_FormatHex32((uint32_t)windowPos->flags);
                    extra = std::string("session=") + std::to_string(interactiveSizingSessionId_) +
                        " interactiveSizingActive=" + WindowChromeTrace::BoolText(interactiveSizingActive_) +
                        " flags=0x" + flagsHex +
                        " flagsText=" + S_FormatWindowPosFlags(windowPos->flags) +
                        " x=" + std::to_string(windowPos->x) +
                        " y=" + std::to_string(windowPos->y) +
                        " cx=" + std::to_string(windowPos->cx) +
                        " cy=" + std::to_string(windowPos->cy);
                } else {
                    extra = std::string("session=") + std::to_string(interactiveSizingSessionId_) +
                        " interactiveSizingActive=" + WindowChromeTrace::BoolText(interactiveSizingActive_) +
                        " windowPos=null";
                }
                TraceWindowPhase("WM_WINDOWPOSCHANGING", nullptr, nullptr, nullptr, nullptr, extra.c_str());
                EmitInteractiveResizeEvidence("WM_WINDOWPOSCHANGING", "window-pos", nullptr, windowPos);
            }
            break;
        }

        case WM_WINDOWPOSCHANGED: {
            const WINDOWPOS* windowPos = reinterpret_cast<const WINDOWPOS*>(lParam);
            // 仅诊断段落纳入门控：本 case 后续的 NotifyParentWindowPositionChanged
            // 与置顶状态守护是功能性代码，必须无条件执行，故此处不能提前 return。
            if (WindowChromeTrace::AuxiliaryTraceEnabled()) {
                std::string extra;
                if (windowPos) {
                    const std::string flagsHex = S_FormatHex32((uint32_t)windowPos->flags);
                    extra = std::string("session=") + std::to_string(interactiveSizingSessionId_) +
                        " interactiveSizingActive=" + WindowChromeTrace::BoolText(interactiveSizingActive_) +
                        " flags=0x" + flagsHex +
                        " flagsText=" + S_FormatWindowPosFlags(windowPos->flags) +
                        " x=" + std::to_string(windowPos->x) +
                        " y=" + std::to_string(windowPos->y) +
                        " cx=" + std::to_string(windowPos->cx) +
                        " cy=" + std::to_string(windowPos->cy);
                } else {
                    extra = std::string("session=") + std::to_string(interactiveSizingSessionId_) +
                        " interactiveSizingActive=" + WindowChromeTrace::BoolText(interactiveSizingActive_) +
                        " windowPos=null";
                }
                TraceWindowPhase("WM_WINDOWPOSCHANGED", nullptr, nullptr, nullptr, nullptr, extra.c_str());
                EmitInteractiveResizeEvidence("WM_WINDOWPOSCHANGED", "window-pos", nullptr, windowPos);
            }
            
            // WebView2 Composition 模式要求: 父窗口位置变化时通知控制器，
            // 否则 WebView2 内部输入窗口不跟随移动，产生「幽灵窗口」阻断桌面交互。
            if (webView_ && webView_->GetController()) {
                window_behavior_trace::Com(hwnd_, "controller.NotifyParentWindowPositionChanged");
                webView_->GetController()->NotifyParentWindowPositionChanged();
            }

            // -- 期望置顶状态守护 --
            // 任何 z-order 路径（现有或未来新增）若让主窗口的 WS_EX_TOPMOST 偏离期望
            // ——升段 = 被相对插入隐式授予置顶（意外覆盖全桌面），
            //   降段 = 用户显式置顶被撤销路径悄悄剥除——在此统一矫正。
            // 期望值唯一由 SetExpectedTopmost 改写（用户显式 alwaysOnTop 意图）；
            // 全屏期间合法持有 HWND_TOPMOST，暂停矫正（进入路径先置 isFullscreen_，
            // 退出路径在恢复 z-order 之后才清标志，两侧均被正确豁免）。
            // 矫正触发的下一次 WM_WINDOWPOSCHANGED 中实际位已与期望一致，自然收敛。
            if (!IsFullscreen() && windowPos && !(windowPos->flags & SWP_NOZORDER)) {
                const bool actualTopmost =
                    (GetWindowLongW(hwnd_, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
                if (actualTopmost != expectedTopmost_) {
                    {
                        std::ostringstream stream;
                        stream << "[ActivationEvidence] source=MainWindow::TopmostGuard"
                               << " t=" << WindowChromeTrace::RelativeMs() << "ms"
                               << " actualTopmost=" << WindowChromeTrace::BoolText(actualTopmost)
                               << " expectedTopmost=" << WindowChromeTrace::BoolText(expectedTopmost_)
                               << " correction=" << (expectedTopmost_ ? "HWND_TOPMOST" : "HWND_NOTOPMOST");
                        S_EmitEvidenceLine(stream.str());
                    }
                    SetWindowPos(hwnd_, expectedTopmost_ ? HWND_TOPMOST : HWND_NOTOPMOST,
                        0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                }
            }
            // z-order/位置变化可能改变“是否被完全覆盖”，防抖后重评估
            ScheduleCoverReevaluation();
            break;
        }
        
        case WM_TIMER:
            if (wParam == STARTUP_REVEAL_TIMER_ID) {
                KillTimer(hwnd_, STARTUP_REVEAL_TIMER_ID);
                LOG("MainWindow: Startup reveal fallback timer fired");
                if (!startupRevealCommitted_) {
                    ApplyStartupPresentationDecision(
                        startupPresentationCoordinator_.OnFallbackTimerFired(),
                        "StartupFallbackTimer");
                }
                return 0;
            }
            if (wParam == SHORT_REVEAL_TIMER_ID) {
                KillTimer(hwnd_, SHORT_REVEAL_TIMER_ID);
                LOG("MainWindow: Short reveal timer fired (", SHORT_REVEAL_TIMEOUT_MS, "ms)");
                if (!startupRevealCommitted_) {
                    // 短超时触发：WebView 首帧 authority 未在 250ms 内齐备，
                    // 退化为空 Mica 窗口，行为与之前 immediate-show 一致。
                    // 使用 ForceFallbackReveal 而非 OnFallbackTimerFired，
                    // 因为短超时时导航可能尚未完成。
                    ApplyStartupPresentationDecision(
                        startupPresentationCoordinator_.ForceFallbackReveal(),
                        "ShortRevealTimer");
                }
                return 0;
            }
            if (wParam == STARTUP_SETTLEMENT_PROBE_T100_TIMER_ID) {
                KillTimer(hwnd_, STARTUP_SETTLEMENT_PROBE_T100_TIMER_ID);
                CaptureRuntimeDomProbe("FinalizeStartupRevealSettlement/+100ms", 100);
                return 0;
            }
            if (wParam == STARTUP_SETTLEMENT_PROBE_T250_TIMER_ID) {
                KillTimer(hwnd_, STARTUP_SETTLEMENT_PROBE_T250_TIMER_ID);
                CaptureRuntimeDomProbe("FinalizeStartupRevealSettlement/+250ms", 250);
                return 0;
            }
            if (wParam == STARTUP_SETTLEMENT_PROBE_T500_TIMER_ID) {
                KillTimer(hwnd_, STARTUP_SETTLEMENT_PROBE_T500_TIMER_ID);
                CaptureRuntimeDomProbe("FinalizeStartupRevealSettlement/+500ms", 500);
                return 0;
            }
            if (wParam == SIZE_MAXIMIZED_PROBE_T100_TIMER_ID) {
                KillTimer(hwnd_, SIZE_MAXIMIZED_PROBE_T100_TIMER_ID);
                CaptureRuntimeDomProbe("WM_SIZE.sizeType=maximized/+100ms", 100);
                return 0;
            }
            if (wParam == SIZE_RESTORED_PROBE_T100_TIMER_ID) {
                KillTimer(hwnd_, SIZE_RESTORED_PROBE_T100_TIMER_ID);
                CaptureRuntimeDomProbe("WM_SIZE.sizeType=restored/+100ms", 100);
                return 0;
            }
            if (wParam == RESIZE_COALESCE_TIMER_ID) {
                FlushPendingInteractiveResize();
                return 0;
            }
            if (wParam == RESTORE_SURFACE_CONVERGE_TIMER_ID) {
                KillTimer(hwnd_, RESTORE_SURFACE_CONVERGE_TIMER_ID);
                // 后台恢复后延迟重复完整 Resume/visible/surface 收敛。安全桌面切换
                // 期间即使首次 put_IsVisible(TRUE) 成功，DComp 首帧仍可能尚未提交。
                if (!isMinimized_) {
                    RestoreSurfaceAfterHidden("deferred-background-resume");
                }
                return 0;
            }
            if (wParam == COVER_REEVAL_TIMER_ID) {
                KillTimer(hwnd_, COVER_REEVAL_TIMER_ID);
                EvaluateCoverState();
                return 0;
            }
            if (wParam == COVER_POLL_TIMER_ID) {
                // 覆盖态周期重检：不再被完全覆盖即恢复并停轮询（在 EvaluateCoverState 内处理）
                EvaluateCoverState();
                return 0;
            }
            if (wParam == BG_RESUME_RETRY_TIMER_ID) {
                KillTimer(hwnd_, BG_RESUME_RETRY_TIMER_ID);
                SetBgSuspend(0, false, "bg-resume-retry");
                return 0;
            }
            if (wParam == PAGE_HEALTH_RETRY_TIMER_ID) {
                KillTimer(hwnd_, PAGE_HEALTH_RETRY_TIMER_ID);
                if (pageHealthProbePending_) {
                    LogWebViewLifecycle("health.timeout");
                    pageHealthProbePending_ = false;
                    ++pageHealthProbeGeneration_;
                    HandleRestoredPageHealthResult(false, "page-health-timeout");
                    return 0;
                }
                ProbeRestoredPageHealth("page-health-retry");
                return 0;
            }
            if (wParam == PROCESS_DEAD_REBUILD_TIMER_ID) {
                KillTimer(hwnd_, PROCESS_DEAD_REBUILD_TIMER_ID);
                // 复检僵尸标记：调度与执行之间若已有其他路径完成重建
                //（例如面板重挂载），此处不得再销毁一个健康的 WebView。
                if (IsWebViewProcessDead()) {
                    RebuildWebViewAfterHealthFailure("browser-process-exited");
                }
                return 0;
            }
            if (wParam == DEFERRED_BACKDROP_TIMER_ID) {
                KillTimer(hwnd_, DEFERRED_BACKDROP_TIMER_ID);
                deferredBackdropPending_ = false;
                if (needsAuthoritativeChromeReapply_ && !isMinimized_ && IsWindowVisible(hwnd_)) {
                    needsAuthoritativeChromeReapply_ = false;
                    TraceWindowPhase("DeferredBackdropTimer.enter");
                    // Chrome 层 reapply（仅在 WebView 就绪后触发）
                    ApplyResolvedChrome(true);
                    TraceWindowPhase("DeferredBackdropTimer.done");
                } else {
                    needsAuthoritativeChromeReapply_ = false;
                }
                return 0;
            }
            if (wParam == BACKDROP_KICK_EXIT_TIMER_ID) {
                // 当前无 backdrop kick 序列使用该计时器，仅做安全清理
                KillTimer(hwnd_, BACKDROP_KICK_EXIT_TIMER_ID);
                return 0;
            }
            return 0;

        // WM_DEFERRED_CHROME_REAPPLY: 保留为 legacy fallback，
        // 当前主路径使用 DEFERRED_BACKDROP_TIMER_ID。
        case WM_DEFERRED_CHROME_REAPPLY:
            needsAuthoritativeChromeReapply_ = false;
            return 0;

        // WM_DEFERRED_SURFACE_CONVERGE: startup-only one-shot ? legacy fallback
        case WM_DEFERRED_SURFACE_CONVERGE: {
            const bool isStartupOneShot = startupDeferredSurfaceConvergePending_;
            startupDeferredSurfaceConvergePending_ = false;
            const char* reason = isStartupOneShot
                ? "startup-post-first-present" : "deferred-surface-converge";
            LogSurfaceDiagnostics("WM_DEFERRED_SURFACE_CONVERGE.enter");
            {
                std::ostringstream stream;
                stream << "[DeferredSurfaceConverge]"
                       << " t=" << WindowChromeTrace::RelativeMs() << "ms"
                       << " reason=" << reason
                       << " isStartupOneShot=" << WindowChromeTrace::BoolText(isStartupOneShot)
                       << " firstPresentCommitted=" << WindowChromeTrace::BoolText(startupSurfaceFirstPresentCommitted_)
                       << " isMinimized=" << WindowChromeTrace::BoolText(isMinimized_);
                S_EmitEvidenceLine(stream.str());
            }
            EnsureSurfaceConvergedAfterNativeFrame(reason);
            CaptureRuntimeDomProbe("WM_DEFERRED_SURFACE_CONVERGE.done");
            FinalizeStartupRevealSettlement();
            return 0;
        }

        case WM_EXITSIZEMOVE: {
            const unsigned int sessionId = interactiveSizingSessionId_;
            interactiveSizingActive_ = false;
            // 必须在清 interactiveSizingActive_ 之后：此时 CoalesceInteractiveResize
            // 已不再拦截，最后一个尺寸保证以真实 put_Bounds 落地。
            FlushPendingInteractiveResize();
            const std::string extra = std::string("session=") + std::to_string(sessionId) +
                " interactiveSizingActive=" + WindowChromeTrace::BoolText(interactiveSizingActive_);
            TraceWindowPhase("WM_EXITSIZEMOVE", nullptr, nullptr, nullptr, nullptr, extra.c_str());
            LogSurfaceDiagnostics("WM_EXITSIZEMOVE");
            EmitInteractiveResizeEvidence("WM_EXITSIZEMOVE", "exit", nullptr, nullptr);
            CaptureRuntimeDomProbe("WM_EXITSIZEMOVE/+0ms", 0);
            // 窗口移动或调整大小结束后保存位置
            SaveWindowPosition();
            return 0;
        }
            
        case WM_SIZE: {
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
            
            // 无标题栏模式下，最大化状态改变时重新计算非客户区
            if (frameless_ && wasMaximized != isMaximized_) {
                SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0,
                    SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            }

            if (wParam == SIZE_MINIMIZED) {
                BroadcastWindowStateChangedIfNeeded();
                // 最小化统一走 background_suspend_policy 投影（不再直调
                // SetVisible(false)+SetMemoryUsageLow(true)）。投影决定：
                // put_IsVisible(FALSE)（页面 visibilityState=hidden，事件门控随之生效）
                // → TrySuspend 深挂起（advconfig 关闭时回退 Low）；CDP 自动化
                // keep-alive veto 在投影内（截图需持续出帧）。
                // 关键经验（CDP 实测）：仅 Low 是净零——occlusion 已禁用时页面保持
                // visible、rAF/定时器不停，释放的内存立刻被回占；必须先 hidden。
                // 仍不使用 SetBoundsVisible(false)（零视口会导致 HMR 导航失败）。
                ClearCoverSuspend();  // 进入最小化：清覆盖记账+停轮询，恢复后会重新评估
                SetBgSuspend(kBgSuspendMinimized, true, "minimize");
                return 0;
            }

            // 从最小化恢复：统一恢复序列——SetBgSuspend 清 kMinimized
            // 后经 RestoreSurfaceAfterHidden 执行 put_IsVisible(TRUE)（挂起态自动
            // resume）+ chrome/surface 收敛，再按当前 reasons 重投影内存等级；DComp
            // 表面随后仍由 RESTORE_SURFACE_CONVERGE nudge 兜底强制重呈现，防空窗。
            if (wasMinimized) {
                SetBgSuspend(kBgSuspendMinimized, false, "restore-from-minimize");
                // "隐藏+最小化"的窗口被 SW_RESTORE / SC_RESTORE 一步恢复到可见时，系统
                // 不发 WM_SHOWWINDOW(TRUE)（DefWindowProc 内部按 SW_SHOWNORMAL 显示，
                // 该命令被文档明确排除在 WM_SHOWWINDOW 之外），WM_SHOWWINDOW 里的
                // kTrayHidden 清除到不了，reasons 会一直挂着 0x8 让页面在可见状态下
                // 继续被投影成 Low。托盘隐藏现在总是先最小化（见 HideWindowToTray），
                // 这条已是常规路径，在此补清。窗口尚未真正可见时留给 WM_SHOWWINDOW。
                if ((bgSuspendReasons_ & kBgSuspendTrayHidden) != 0 && IsWindowVisible(hwnd_)) {
                    SetBgSuspend(kBgSuspendTrayHidden, false, "restore-from-tray");
                }
            }

            OnSize(LOWORD(lParam), HIWORD(lParam));

            // 补做最小化期间被推迟的约束校正（见 RevalidateSizeAgainstConstraints
            // 的 iconic 分支）。此时窗口已回到还原态，几何可安全改写。
            if (pendingConstraintRevalidate_ && !isMinimized_ && !IsIconic(hwnd_)) {
                pendingConstraintRevalidate_ = false;
                RevalidateSizeAgainstConstraints();
            }
            if (wasMinimized && !startupRevealPending_ && startupRevealCommitted_ && !startupRevealSettling_) {
                ApplyResolvedChrome(true);
                // 兜底：恢复后延迟一拍强制一次真实 surface transition（nudge），防 DComp 表面留空。
                // 最小化时投影已把页面隐藏（SetVisible(false)+TrySuspend/Low），上方
                // SetBgSuspend 恢复分支执行统一恢复序列，此 nudge 为其既有兜底
                //（已验证可靠，非历史“无法恢复”场景）。
                if (hwnd_ && IsWindow(hwnd_)) {
                    KillTimer(hwnd_, RESTORE_SURFACE_CONVERGE_TIMER_ID);
                    SetTimer(hwnd_, RESTORE_SURFACE_CONVERGE_TIMER_ID,
                             RESTORE_SURFACE_CONVERGE_DELAY_MS, nullptr);
                }
            }
            BroadcastWindowStateChangedIfNeeded();
            if (!startupRevealPending_ && startupRevealCommitted_ &&
                (sizeType == "maximized" || sizeType == "restored")) {
                ScheduleManualResizeRuntimeProbes(sizeType.c_str());
            }
            return 0;
        }
            
        case WM_PAINT:
            OnPaint();
            return 0;
            
        case WM_ERASEBKGND: {
            // 始终返回 FALSE，让 DWM 处理背景绘制
            // 这样 Mica/Acrylic 效果才能正确显示
            return FALSE;
        }
            
        case WM_NCACTIVATE:
            // 自绘菜单持有前台期间，向 DefWindowProc 谎报激活态：DWM 对 mica/acrylic
            // 的原生调暗跟随 NCACTIVATE 状态，谎报 TRUE 保住宿主材质的激活观感
            //（经典 owner-popup 模式，原生菜单语义：菜单弹出不使宿主变灰）。
            if (wParam == FALSE && MenuOverlayHost::GetInstance().ShouldHoldOwnerActivation(
                    reinterpret_cast<HWND>(lParam))) {
                return DefWindowProcW(hwnd_, WM_NCACTIVATE, TRUE, lParam);
            }
            break;

        case WM_ACTIVATE:
            OnActivate(wParam, lParam);
            // 覆盖检测：失活 → 防抖后评估是否被完全覆盖；激活 → 立即解除覆盖挂起（获焦必然未被覆盖）
            if (LOWORD(wParam) == WA_INACTIVE) {
                ScheduleCoverReevaluation();
            } else {
                SetBgSuspend(kBgSuspendCovered, false, "activated");
                KillTimer(hwnd_, COVER_POLL_TIMER_ID);
                KillTimer(hwnd_, COVER_REEVAL_TIMER_ID);
            }
            break;

        case WM_ACTIVATEAPP:
            OnActivateApp(wParam != FALSE);
            if (wParam != FALSE) {
                SetBgSuspend(kBgSuspendCovered, false, "app-activated");
                KillTimer(hwnd_, COVER_POLL_TIMER_ID);
                KillTimer(hwnd_, COVER_REEVAL_TIMER_ID);
            } else {
                ScheduleCoverReevaluation();
            }
            return 0;
            
        case WM_SETFOCUS: {
            // Visual Hosting (CompositionController) 模式下 WebView2 没有独立子 HWND，
            // Alt+Tab / 任务栏切回窗口时系统把键盘焦点交给顶层 HWND，
            // 若不主动把焦点转交给 Chromium，页面收不到键盘输入（必须点击一次才恢复）。
            // 这里在宿主窗口拿到焦点时调用 MoveFocus 把焦点路由进 WebView 内容。
            {
                std::ostringstream stream;
                stream << "[ActivationEvidence] source=WM_SETFOCUS"
                       << " t=" << WindowChromeTrace::RelativeMs() << "ms"
                       << " prevFocusHwnd=0x" << pfc::format_hex((size_t)(HWND)wParam)
                       << " webViewReady=" << WindowChromeTrace::BoolText(webView_ && webView_->IsReady());
                S_EmitEvidenceLine(stream.str());
            }
            if (webView_ && webView_->IsReady() && !isMinimized_) {
                webView_->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
            }
            break;  // 交给 DefWindowProc 维持默认焦点语义
        }
            
        case WM_DPICHANGED:
            OnDpiChanged(wParam, lParam);
            return 0;
            
        case WM_GETMINMAXINFO: {
            auto* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
            
            // 约束以 DIP 存储，此处换算为物理像素。
            // 这是唯一把约束值交给 Win32 的地方，也是唯一天然知道当前 DPI
            // 的时刻，因此换算落在这里（Q9）。DPI 变更后系统下次询问会自动
            // 用新 DPI 换算，无需额外的重算逻辑。
            //
            // 注意：hwnd_ 在 WM_NCCREATE 前尚未赋值，而 WM_GETMINMAXINFO
            // 可能早于它到达（CreateWindowEx 内部），故 dpi 取用需容错。
            const int dpi = (hwnd_ && IsWindow(hwnd_))
                ? GetDpiForWindow(hwnd_)
                : window_geometry::kBaselineDpi;
            
            // 最小尺寸
            mmi->ptMinTrackSize.x = window_geometry::DipToPhysical(minWidthDip_, dpi);
            mmi->ptMinTrackSize.y = window_geometry::DipToPhysical(minHeightDip_, dpi);
            
            // 最大尺寸 (0 表示无限制)
            if (maxWidthDip_ > 0) {
                mmi->ptMaxTrackSize.x = window_geometry::DipToPhysical(maxWidthDip_, dpi);
            }
            if (maxHeightDip_ > 0) {
                mmi->ptMaxTrackSize.y = window_geometry::DipToPhysical(maxHeightDip_, dpi);
            }

            // frameless 最大化时，通过 ptMaxPosition/ptMaxSize 将窗口向四周扩展
            // frame 厚度，使 WS_THICKFRAME 边框被推到屏幕外（不可见）。
            // WM_NCCALCSIZE 再将客户区收回到工作区，excess frame 成为 off-screen NC 区。
            // 这是 Chromium 风格的无边框最大化做法。
            if (frameless_) {
                HMONITOR hMon = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
                if (hMon) {
                    MONITORINFO mi = { sizeof(MONITORINFO) };
                    if (GetMonitorInfo(hMon, &mi)) {
                        int frameX = GetSystemMetrics(SM_CXSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
                        int frameY = GetSystemMetrics(SM_CYSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
                        mmi->ptMaxPosition.x = mi.rcWork.left - mi.rcMonitor.left - frameX;
                        mmi->ptMaxPosition.y = mi.rcWork.top  - mi.rcMonitor.top  - frameY;
                        mmi->ptMaxSize.x = (mi.rcWork.right  - mi.rcWork.left) + 2 * frameX;
                        mmi->ptMaxSize.y = (mi.rcWork.bottom - mi.rcWork.top)  + 2 * frameY;
                    }
                }
            }
            
            break;  // 让 DefWindowProc 也处理
        }
            
        case WM_CONTEXTMENU:
            // 不自动显示右键菜单，让前端通过 ui.showContextMenu API 控制
            // 前端可以使用 e.preventDefault() 阻止默认菜单
            // 需要菜单时调用 fb2k.invoke('ui.showContextMenu', {x, y})
            return 0;
            
        case WM_COMMAND:
            // 缩略图按钮点击
            if (HIWORD(wParam) == THBN_CLICKED) {
                TaskbarIntegration::GetInstance().HandleButtonClicked(LOWORD(wParam));
                return 0;
            }
            OnCommand(LOWORD(wParam));
            return 0;
            
        case WM_KEYDOWN:
            // F12 to open DevTools (仅开发服务器模式允许)
            if (wParam == VK_F12 && webView_ && webView_->IsReady()) {
                if (security_config::UseDevServer()) {
                    webView_->OpenDevTools();
                }
            }
            // F5 to reload
            else if (wParam == VK_F5 && webView_ && webView_->IsReady()) {
                webView_->Reload();
            }
            // Check if this key is bound to Preferences command
            else {
                // Get the GUID for Preferences command
                GUID prefsGuid = standard_commands::guid_main_preferences;
                
                // Check common keyboard shortcuts for Preferences
                bool isCtrlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                bool isShiftDown = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                bool isAltDown = (GetKeyState(VK_MENU) & 0x8000) != 0;
                
                // Default shortcut for Preferences is Ctrl+P
                if (wParam == 'P' && isCtrlDown && !isShiftDown && !isAltDown) {
                    LOG("Preferences shortcut detected (Ctrl+P), opening preferences");
                    standard_commands::run_main(prefsGuid);
                    return 0;  // Prevent WebView from handling this key
                }
            }
            break;
            
        // ============================================
        // Visual Hosting 模式：客户区光标同步
        // CompositionController 模式下 WebView2 不拥有任何 HWND, 所以所有
        // WM_SETCURSOR 都到父窗口。我们必须在 HTCLIENT 时主动 SetCursor
        // 为 Chromium 当前希望显示的光标 (或 cursor.setHidden(true) 时 nullptr),
        // 否则 DefWindowProc 会用 GCLP_HCURSOR (类光标) 兜底, 在 idle 隐藏
        // 期间会错误地显示箭头光标 (问题 B), 在 hover 切换时又会落后于
        // Chromium 的 CursorChanged (问题 C)。
        //
        // 边缘 / 标题栏 / 系统按钮 (HTCAPTION / HTLEFT / HTBOTTOMRIGHT 等)
        // 仍由 DefWindowProc 处理, 以便系统正确显示 resize / move 光标。
        // ============================================
        case WM_SETCURSOR:
            // HTMAXBUTTON 只会是页面自绘的最大化键：鼠标移动已转给页面，光标也用页面的。
            if ((LOWORD(lParam) == HTCLIENT || (frameless_ && LOWORD(lParam) == HTMAXBUTTON)) &&
                webView_ && webView_->ApplyCurrentCursor()) {
                return TRUE;
            }
            break;

        // ============================================
        // Visual Hosting 模式：鼠标消息转发
        // ============================================
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
            if (webView_ && webView_->GetCompositionController()) {
                POINT pt;
                POINTSTOPOINT(pt, lParam);
                
                // ============================================
                // 拖拽区域检测：不转发拖拽区域的鼠标事件
                // 让系统处理窗口拖拽
                // ============================================
                bool inDragRegion = IsPointInDragRegion(pt.x, pt.y) && !IsPointInNoDragRegion(pt.x, pt.y);
                
                if (inDragRegion) {
                    if (msg == WM_LBUTTONDOWN) {
                        // 在拖拽区域内单击，不转发给 WebView
                        // 通过模拟非客户区消息来实现拖拽
                        ReleaseCapture();
                        SendMessageW(hwnd_, WM_NCLBUTTONDOWN, HTCAPTION, lParam);
                        return 0;
                    }
                    else if (msg == WM_LBUTTONDBLCLK) {
                        // 在拖拽区域内双击，切换最大化状态（全屏时禁用；
                        // 不可调整大小的窗口同样没有最大化语义，与 WM_NCLBUTTONDBLCLK 分支一致）
                        if (IsWindowFullscreen() || !resizable_) {
                            return 0;
                        }
                        if (isMaximized_) {
                            ShowWindow(hwnd_, SW_RESTORE);
                        } else {
                            ShowWindow(hwnd_, SW_MAXIMIZE);
                        }
                        return 0;
                    }
                    else if (msg == WM_RBUTTONUP) {
                        // 在拖拽区域内右键，显示系统菜单
                        POINT screenPt = pt;
                        ClientToScreen(hwnd_, &screenPt);
                        ShowSystemMenu(screenPt.x, screenPt.y);
                        return 0;
                    }
                }
                
                if (webView_->HandleMouseMessage(msg, wParam, lParam)) {
                    return 0;  // 已处理
                }
            }
            break;
    }
    
    return DefWindowProcW(hwnd_, msg, wParam, lParam);
}

void MainWindow::OnCreate() {
    LOG("MainWindow::OnCreate");
    
    // OnCreate 阶段只做最小 DWM 调用序列：
    // 不调用 Chrome 统一层（ApplyResolvedChrome / WindowChromeApplier），
    // 不写 corner preference、BlurBehind、MICA_EFFECT=FALSE 等额外属性，
    // 不触发额外的 SWP_FRAMECHANGED / RedrawWindow。
    // Chrome 层延迟到 OnWebViewReady 后首次激活。

    // 1. 设置应用级深色模式 (影响右键菜单等)
    bool isDark = IsFoobar2000DarkMode();
    SetAppDarkMode(isDark);
    AllowDarkModeForWindow(hwnd_, isDark);
    
    // 2. 扩展客户区到整个窗口（包括边框）
    MARGINS margins = { -1, -1, -1, -1 };
    S_DwmExtendFrameIntoClientArea(hwnd_, &margins);

    // 2.1 [frameless 模式] 移除按钮使能位防止 DWM 渲染三大键。
    // 根因：DwmExtendFrameIntoClientArea({-1,-1,-1,-1}) + transparent WebView +
    // WS_SYSMENU = DWM 三大键按钮作为玻璃框架 sub-layer 透过透明内容可见。
    // NC面积=0 无法消除此行为——按钮不在 NC 区域，而是 DWM 合成层装饰。
    // 关键：保留 WS_CAPTION 使 DWM 标准动画（最小化/恢复/最大化）正常工作，
    // 仅移除 WS_SYSMENU 以消除全部三大键按钮渲染。
    // DWM 按钮 overlay 渲染完全依赖 WS_SYSMENU；WS_MINIMIZEBOX/WS_MAXIMIZEBOX
    // 只在 WS_SYSMENU 存在时控制哪些按钮 enabled/disabled，不单独触发渲染。
    // 保留 WS_MINIMIZEBOX/WS_MAXIMIZEBOX 确保 Shell 任务栏点击最小化/恢复切换
    // 以及任务栏右键菜单中的最小化/最大化功能正常工作。
    // 恢复 frameless→non-frameless 时由 ApplyNativeFrameStrategy::Standard 重新添加。
    if (frameless_) {
        LONG_PTR curStyle = GetWindowLongPtrW(hwnd_, GWL_STYLE);
        SetWindowLongPtrW(hwnd_, GWL_STYLE,
            curStyle & ~WS_SYSMENU);
    }

    // 2.5 确保 Windows 11 圆角矩形：显式写 DWMWCP_ROUND，
    // 避免 CreateWindowEx → ShowWindow 之间出现直角首帧
    {
        constexpr DWORD DWMWA_WINDOW_CORNER_PREFERENCE_V = 33;
        constexpr DWORD DWMWCP_ROUND = 2;
        DWORD cornerPref = DWMWCP_ROUND;
        S_DwmSetWindowAttribute(hwnd_, DWMWA_WINDOW_CORNER_PREFERENCE_V,
            &cornerPref, sizeof(cornerPref));
    }

    // 显式启用 DWM 窗口过渡动画。
    // CaptionlessOverlapped 窗口依赖此属性保持标准 DWM 动画。
    {
        constexpr DWORD DWMWA_TRANSITIONS_FORCEDISABLED = 3;
        BOOL transDisabled = FALSE;
        S_DwmSetWindowAttribute(hwnd_, DWMWA_TRANSITIONS_FORCEDISABLED,
            &transDisabled, sizeof(transDisabled));
    }

    isActive_ = (::GetForegroundWindow() == hwnd_);
    chromeBackdropBroadcastReady_ = false;

    // 3. 直接写 DWM 属性（精确复刻 v1.1.19 EnableMicaEffect）
    //    顺序: dark mode → backdrop type → (fallback mica)
    //    不写 corner pref、不写 BlurBehind、不写 MICA_EFFECT=FALSE
    EnableMicaEffect();
    
    // 4. 设置主窗口 ID
    SetWindowId("main");
    
    // 5. 初始化 WebView2（DComp target 在此创建）
    const bool webViewInitStarted = InitializeWebView(hwnd_, WebViewPanelMode::Standalone);

    if (!webViewInitStarted) {
        LOG("ERROR: Failed to start WebView2 initialization");
        ApplyStartupPresentationDecision(
            startupPresentationCoordinator_.ForceFallbackReveal(),
            "InitializeWebViewFailed");
        MessageBoxW(hwnd_, 
            TR("Failed to initialize WebView2.\n\nPlease ensure WebView2 Runtime is installed:\nhttps://go.microsoft.com/fwlink/p/?LinkId=2124703",
               "WebView2 初始化失败。\n\n请确认已安装 WebView2 运行时:\nhttps://go.microsoft.com/fwlink/p/?LinkId=2124703"),
            TR("WebView2 UI Error", "WebView2 UI 错误"),
            MB_OK | MB_ICONERROR);
    }
}

void MainWindow::OnDestroy() {
    LOG("MainWindow::OnDestroy - saving position");
    KillTimer(hwnd_, STARTUP_REVEAL_TIMER_ID);
    KillTimer(hwnd_, SHORT_REVEAL_TIMER_ID);
    KillTimer(hwnd_, STARTUP_SETTLEMENT_PROBE_T100_TIMER_ID);
    KillTimer(hwnd_, STARTUP_SETTLEMENT_PROBE_T250_TIMER_ID);
    KillTimer(hwnd_, STARTUP_SETTLEMENT_PROBE_T500_TIMER_ID);
    KillTimer(hwnd_, SIZE_MAXIMIZED_PROBE_T100_TIMER_ID);
    KillTimer(hwnd_, SIZE_RESTORED_PROBE_T100_TIMER_ID);
    KillTimer(hwnd_, RESTORE_SURFACE_CONVERGE_TIMER_ID);
    KillTimer(hwnd_, BG_RESUME_RETRY_TIMER_ID);
    KillTimer(hwnd_, PAGE_HEALTH_RETRY_TIMER_ID);
    KillTimer(hwnd_, PROCESS_DEAD_REBUILD_TIMER_ID);
    KillTimer(hwnd_, BACKDROP_KICK_EXIT_TIMER_ID);
    KillTimer(hwnd_, RESIZE_COALESCE_TIMER_ID);
    hasPendingInteractiveResize_ = false;
    startupPresentationCoordinator_.MarkClosed();
    SyncStartupRevealProjection();
    startupSurfaceAuthorityPending_ = false;
    startupSurfaceCommitPending_ = false;
    hasPendingStartupSurfaceBounds_ = false;
    startupSurfaceFirstPresentCommitted_ = false;
    startupDeferredSurfaceConvergePending_ = false;
    // 确保在任何退出方式下都保存窗口位置
    SaveWindowPosition();
    chromeBackdropBroadcastReady_ = false;
    // 使用基类方法销毁 WebView
    DestroyWebView();
}

void MainWindow::OnClose() {
    LOG("MainWindow closing - saving position and exiting foobar2000");

    // 在退出前保存窗口位置
    SaveWindowPosition();

    // Use standard command to exit foobar2000 properly
    standard_commands::run_main(standard_commands::guid_main_exit);
}

void MainWindow::HideWindowToTray() {
    // 托盘隐藏统一走 background_suspend_policy 投影（不再直调
    // SetVisible(false)+SetMemoryUsageLow(true)）。投影在 SW_HIDE 前
    // 应用（仅 SW_HIDE 不会让页面 visibilityState=hidden，因 IsVisible 仍 TRUE +
    // occlusion 已禁用）：put_IsVisible(FALSE) → TrySuspend 深挂起（advconfig 关闭
    // 时回退 Low）；CDP 自动化 keep-alive veto 在投影内（托盘期间 CDP 工具须持续可用）。
    // WM_SHOWWINDOW(TRUE) 或从最小化恢复的 WM_SIZE 分支清除 kBgSuspendTrayHidden；
    // 各恢复调用方随后通过 RestoreSurfaceAfterHidden 同步 surface 状态。
    ClearCoverSuspend();
    SetBgSuspend(kBgSuspendTrayHidden, true, "tray-hide");
    // 投影被否决（CDP keep-alive）时页面保持 IsVisible=TRUE，Visual Hosting 的渲染/输入
    // 顶层窗（msedgewebview2 的 Chrome_WidgetWin_1）不随宿主 SW_HIDE 消失，会留在原矩形
    // 拦截桌面鼠标输入。先最小化，让 Chromium 把该窗同步停泊到
    // (-32000,-32000)，再隐藏；停泊期间仍出帧，CDP 截图不受影响。收 bounds 不可取：
    // 零视口会停帧，而 keep-alive 存在的目的正是保持出帧。三条恢复路径
    //（WebViewUI::activate / window.focus / window.restore）都先按 IsIconic 做
    // SW_RESTORE，能处理"最小化+隐藏"复合态；SaveWindowPosition 按
    // WPF_RESTORETOMAXIMIZED 保住最大化事实。
    // 过渡动画临时禁用：隐藏到托盘不该出现"缩进任务栏"的最小化动画。
    if (!bgSuspendPageHidden_ && webView_ && webView_->IsReady() && !IsIconic(hwnd_)) {
        constexpr DWORD DWMWA_TRANSITIONS_FORCEDISABLED = 3;
        BOOL transitionsDisabled = TRUE;
        S_DwmSetWindowAttribute(hwnd_, DWMWA_TRANSITIONS_FORCEDISABLED,
            &transitionsDisabled, sizeof(transitionsDisabled));
        ShowWindow(hwnd_, SW_MINIMIZE);
        ShowWindow(hwnd_, SW_HIDE);
        transitionsDisabled = FALSE;
        S_DwmSetWindowAttribute(hwnd_, DWMWA_TRANSITIONS_FORCEDISABLED,
            &transitionsDisabled, sizeof(transitionsDisabled));
        LogSurfaceDiagnostics("HideWindowToTray.parked");
        return;
    }
    ShowWindow(hwnd_, SW_HIDE);
}


void MainWindow::OnSize(int width, int height) {
    if (!webView_ || !webView_->IsReady())
        return;

    RECT clientRect{};
    GetClientRect(hwnd_, &clientRect);

    if (isMinimized_) {
        // 不手动隐藏 WebView — WebView2 运行时自动处理
        return;
    }

    if (startupSurfaceAuthorityPending_) {
        pendingStartupSurfaceBounds_ = clientRect;
        hasPendingStartupSurfaceBounds_ = true;

        const std::string cachedBoundsText = S_FormatNativeRect(clientRect);
        std::ostringstream stream;
        stream << "[StartupSurfaceAuthority]"
               << " t=" << WindowChromeTrace::RelativeMs() << "ms"
               << " owner=OnSize"
               << " action=cache-only"
               << " startupSurfaceAuthorityPending=" << WindowChromeTrace::BoolText(startupSurfaceAuthorityPending_)
               << " startupSurfaceCommitPending=" << WindowChromeTrace::BoolText(startupSurfaceCommitPending_)
               << " width=" << width
               << " height=" << height
               << " cachedBounds=" << cachedBoundsText.c_str();
        S_EmitEvidenceLine(stream.str());

        std::string extra = std::string("width=") + std::to_string(width) +
            " height=" + std::to_string(height) +
            " cachedBounds=" + cachedBoundsText +
            " startupSurfaceAuthorityPending=" + WindowChromeTrace::BoolText(startupSurfaceAuthorityPending_);
        TraceWindowPhase("OnSize.startup-cache-only", nullptr, nullptr, nullptr, nullptr, extra.c_str());
        return;
    }
    
    // WebView 覆盖整个客户区，让 DWM 效果完全穿透
    // 不再内缩，窗口调整由 WM_NCHITTEST 处理
    if (CoalesceInteractiveResize(clientRect)) {
        return;
    }
    webView_->Resize(clientRect);
}

bool MainWindow::CoalesceInteractiveResize(const RECT& clientRect) {
    if (!interactiveSizingActive_ || !hwnd_ || !IsWindow(hwnd_)) {
        return false;
    }

    static const double qpcMsPerTick = [] {
        LARGE_INTEGER freq{};
        return QueryPerformanceFrequency(&freq) && freq.QuadPart
            ? 1000.0 / static_cast<double>(freq.QuadPart)
            : 0.0;
    }();

    LARGE_INTEGER now{};
    if (qpcMsPerTick == 0.0 || !QueryPerformanceCounter(&now)) {
        return false;  // 无高精度计时器：退回逐条直通，不引入未知行为
    }

    const double sinceLastMs = lastInteractiveResizeQpc_ == 0
        ? RESIZE_COALESCE_MIN_INTERVAL_MS
        : static_cast<double>(now.QuadPart - lastInteractiveResizeQpc_) * qpcMsPerTick;

    if (sinceLastMs >= RESIZE_COALESCE_MIN_INTERVAL_MS) {
        lastInteractiveResizeQpc_ = now.QuadPart;
        hasPendingInteractiveResize_ = false;
        KillTimer(hwnd_, RESIZE_COALESCE_TIMER_ID);
        return false;
    }

    // 距上次落地过近：只记住最新尺寸。尾随定时器保证「按住不动」时它仍会落地，
    // 而模态循环会分发 WM_TIMER，故无需外部消息泵配合。
    pendingInteractiveResizeBounds_ = clientRect;
    if (!hasPendingInteractiveResize_) {
        hasPendingInteractiveResize_ = true;
        SetTimer(hwnd_, RESIZE_COALESCE_TIMER_ID, RESIZE_COALESCE_TRAILING_MS, nullptr);
    }
    return true;
}

void MainWindow::FlushPendingInteractiveResize() {
    if (hwnd_ && IsWindow(hwnd_)) {
        KillTimer(hwnd_, RESIZE_COALESCE_TIMER_ID);
    }
    if (!hasPendingInteractiveResize_) {
        return;
    }
    hasPendingInteractiveResize_ = false;

    if (!webView_ || !webView_->IsReady() || isMinimized_ || startupSurfaceAuthorityPending_) {
        return;
    }

    LARGE_INTEGER now{};
    if (QueryPerformanceCounter(&now)) {
        lastInteractiveResizeQpc_ = now.QuadPart;
    }
    webView_->Resize(pendingInteractiveResizeBounds_);
}

bool MainWindow::BroadcastWindowStateChangedIfNeeded(bool force) {
    if (!webView_ || !webView_->IsReady() || !chromeBackdropBroadcastReady_) {
        return false;
    }

    if (!force && hasBroadcastWindowState_ &&
        lastBroadcastIsMaximized_ == isMaximized_ &&
        lastBroadcastIsMinimized_ == isMinimized_ &&
        lastBroadcastIsActive_    == isActive_ &&
        lastBroadcastIsFullscreen_ == isFullscreen_) {
        return false;
    }

    hasBroadcastWindowState_ = true;
    lastBroadcastIsMaximized_  = isMaximized_;
    lastBroadcastIsMinimized_  = isMinimized_;
    lastBroadcastIsActive_     = isActive_;
    lastBroadcastIsFullscreen_ = isFullscreen_;

    api::window::StateChangedPayload state;
    // 事件发给所有窗口，windowId（主窗口为 "main"）让各页面分清是不是自己的窗口变了。
    state.windowId = GetWindowId();
    state.isMaximized = state.maximized = isMaximized_;
    state.isMinimized = state.minimized = isMinimized_;
    state.isActive = state.active = isActive_;
    state.isFullscreen = state.fullscreen = isFullscreen_;
    api::emit::Broadcast<api::window::events::StateChanged>(state);

    {
        std::ostringstream stream;
        stream << "[ActivationEvidence] source=BroadcastWindowStateChanged"
               << " t=" << WindowChromeTrace::RelativeMs() << "ms"
               << " force=" << WindowChromeTrace::BoolText(force)
               << " active=" << WindowChromeTrace::BoolText(isActive_)
               << " isActive=" << WindowChromeTrace::BoolText(isActive_)
               << " maximized=" << WindowChromeTrace::BoolText(isMaximized_)
               << " minimized=" << WindowChromeTrace::BoolText(isMinimized_)
               << " fullscreen=" << WindowChromeTrace::BoolText(isFullscreen_);
        S_EmitEvidenceLine(stream.str());
    }

    return true;
}

void MainWindow::OnPaint() {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd_, &ps);
    
    // 完全不绘制任何内容
    // 窗口背景由 DWM Mica/Acrylic 效果处理
    // WebView 的透明背景会显示 DWM 效果
    // "Loading" 状态由前端 HTML 处理
    
    EndPaint(hwnd_, &ps);
}

// Test page HTML moved to separate file for maintainability
#include "window/TestPageHtml.inl"

void MainWindow::OnWebViewReady() {
    LOG("MainWindow::OnWebViewReady");
    webViewRebuildInProgress_ = false;
    LogWebViewLifecycle("window.webview-ready");
    
    // 初始化 WindowManager（windowId 已在 OnCreate 中设置）
    WindowManager::GetInstance().Initialize(this);
    
    // 强制整个窗口重绘（MainWindow 特有，DWM 效果需要）
    RedrawWindow(hwnd_, nullptr, nullptr, 
                 RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    
    // 调用基类实现（API 注册、回调初始化、页面加载）
    WebViewPanel::OnWebViewReady();
    
    // 为了向后兼容，同时绑定到 BridgeCore 单例
    // 这确保现有使用 BridgeCore::GetInstance() 的代码仍然正常工作
    BridgeCore::GetInstance().SetWebView(webView_.get());
    
    // 触发重新布局
    RECT rect;
    GetClientRect(hwnd_, &rect);
    hasBroadcastWindowState_ = true;
    lastBroadcastIsMaximized_ = isMaximized_;
    lastBroadcastIsMinimized_ = isMinimized_;
    lastBroadcastIsActive_ = isActive_;
    OnSize(rect.right, rect.bottom);
    LogWebViewLifecycle("window.webview-ready-sized");

    // cold-start reveal 路径下该标志始终为 false（immediate-show 才会置位）。
    startupChromeLayerSuppressed_ = false;

    // [Chrome Reconciliation] 不再在 OnWebViewReady 中切换 WS_POPUP。
    // 稳态 native frame strategy 延迟到 FinalizeStartupRevealSettlement 中
    // 由 shared chrome apply 统一处理。

    chromeBackdropBroadcastReady_ = false;
    ApplyResolvedChrome(true);

    // Cold-start reveal：WebView 就绪后取消短超时 timer（若尚未触发）
    if (hwnd_ && IsWindow(hwnd_)) {
        KillTimer(hwnd_, SHORT_REVEAL_TIMER_ID);
    }

    MaybeCommitStartupRevealAfterChromeReady("OnWebViewReady");

    // Cold-start reveal 路径：若 reveal 已在 TryCommitStartupReveal 中完成但
    // settlement 尚未执行，补做一次（幂等）。
    if (startupRevealCommitted_) {
        FinalizeStartupRevealSettlement();
    }
    
    LOG("MainWindow::OnWebViewReady completed");
}

// ============================================
// 启动 reveal 状态机
// ============================================

void MainWindow::OnNavigationCompleted(bool success) {
    LogWebViewLifecycle("navigation.completed",
        std::string("success=") + (success ? "1" : "0"));
    ApplyStartupPresentationDecision(
        startupPresentationCoordinator_.OnNavigationCompleted(success, IsStartupRevealChromeReady()),
        success ? "OnNavigationCompleted" : "OnNavigationCompletedFailed");
}

void MainWindow::OnWindowReadySignal(const std::string& source) {
    ApplyStartupPresentationDecision(
        startupPresentationCoordinator_.OnWindowReadySignal(source, IsStartupRevealChromeReady()),
        "OnWindowReadySignal");
}

void MainWindow::OnVisualReadySignal(const std::string& source) {
    ApplyStartupPresentationDecision(
        startupPresentationCoordinator_.OnVisualReadySignal(source, IsStartupRevealChromeReady()),
        "OnVisualReadySignal");
}

void MainWindow::OnSetFocus() {
    {
        std::ostringstream stream;
        stream << "[ActivationEvidence] source=MainWindow::OnSetFocus"
               << " t=" << WindowChromeTrace::RelativeMs() << "ms"
               << " foregroundOwned=" << WindowChromeTrace::BoolText(IsForegroundOwnedByMainWindow())
               << " foregroundHwnd=0x" << pfc::format_hex((size_t)::GetForegroundWindow())
               << " isActive=" << WindowChromeTrace::BoolText(isActive_);
        S_EmitEvidenceLine(stream.str());
    }
    // Only propagate panel-level focus; window activation is solely decided by WM_ACTIVATE/WM_ACTIVATEAPP.
    WebViewPanel::OnSetFocus();
}

void MainWindow::OnKillFocus() {
    {
        std::ostringstream stream;
        stream << "[ActivationEvidence] source=MainWindow::OnKillFocus"
               << " t=" << WindowChromeTrace::RelativeMs() << "ms"
               << " foregroundOwned=" << WindowChromeTrace::BoolText(IsForegroundOwnedByMainWindow())
               << " foregroundHwnd=0x" << pfc::format_hex((size_t)::GetForegroundWindow())
               << " isActive=" << WindowChromeTrace::BoolText(isActive_);
        S_EmitEvidenceLine(stream.str());
    }
    // Only propagate panel-level focus; window activation is solely decided by WM_ACTIVATE/WM_ACTIVATEAPP.
    WebViewPanel::OnKillFocus();
}

void MainWindow::OnWebViewProcessFailed(COREWEBVIEW2_PROCESS_FAILED_KIND failedKind, bool recovered) {
    // Forward to the base class first so `webview:processFailed` reaches every
    // window front-end before MainWindow records its window-state evidence.
    WebViewPanel::OnWebViewProcessFailed(failedKind, recovered);

    // WebViewHost 已写入 webview_crash.log 并按崩溃类型分级处理。
    // 此处补一条 ActivationEvidence 便于把崩溃与窗口状态（最小化/前台/可见）关联定位。
    {
        std::ostringstream stream;
        stream << "[ActivationEvidence] source=MainWindow::OnWebViewProcessFailed"
               << " t=" << WindowChromeTrace::RelativeMs() << "ms"
               << " failedKind=" << static_cast<int>(failedKind)
               << " recovered=" << WindowChromeTrace::BoolText(recovered)
               << " isMinimized=" << WindowChromeTrace::BoolText(isMinimized_)
               << " isActive=" << WindowChromeTrace::BoolText(isActive_)
               << " foregroundOwned=" << WindowChromeTrace::BoolText(IsForegroundOwnedByMainWindow());
        S_EmitEvidenceLine(stream.str());
    }

    // 渲染进程类崩溃：WebViewHost 已自行 Reload，恢复后重新把焦点路由进页面，
    // 避免"恢复但键盘失灵"。
    if (recovered && webView_ && webView_->IsReady() && !isMinimized_) {
        webView_->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
        return;
    }

    // 不可恢复（浏览器进程退出 / Reload 失败）：controller 已成僵尸，所有
    // put_* 调用都会返回 ERROR_INVALID_STATE，窗口退化为空壳。健康探针只在
    // 恢复成功后才装载，此路径够不到它，故必须由这里主动发起整窗重建，否则
    // 空窗口会无限期存在（实测可持续 10 小时）。
    if (!recovered) {
        ScheduleProcessDeadRebuild("browser-process-exited");
    }
}

void MainWindow::OnWebViewInitFailed() {
    // InitializeWebView 只报告"是否成功启动"，controller 创建失败发生在异步
    // 回调里。若不在此处释放重建锁，RebuildWebViewAfterHealthFailure 会返回
    // true 而锁永不清零，后续所有重建（含健康探针路径）被永久堵死。
    if (webViewRebuildInProgress_) {
        webViewRebuildInProgress_ = false;
        LogWebViewLifecycle("rebuild.failed", "reason=controller-create-failed");
        console::print("[WebView2 UI] WebView rebuild failed during controller creation");
    }
    WebViewPanel::OnWebViewInitFailed();
}

bool MainWindow::IsStartupRevealChromeReady() const {
    const bool hasPluginEffect =
        S_IsPluginManagedBackdropEffect(resolvedChromeState_.effectiveActiveEffect) ||
        S_IsPluginManagedBackdropEffect(resolvedChromeState_.effectiveInactiveEffect);
    if (!hasPluginEffect && !resolvedChromeState_.transparentBackground) {
        return false;
    }
    // chrome ready 语义不弱于一次真实 apply：WebView 必须已就绪，
    // 这样 transparent background 和 RefreshForDwmEffect 才真正生效
    return webView_ && webView_->IsReady();
}

bool MainWindow::IsForegroundOwnedByMainWindow() const {
    if (!hwnd_ || !IsWindow(hwnd_)) {
        return false;
    }

    HWND foreground = ::GetForegroundWindow();
    if (!foreground) {
        return false;
    }

    return foreground == hwnd_ || IsChild(hwnd_, foreground) != FALSE;
}

void MainWindow::MaybeCommitStartupRevealAfterChromeReady(const char* reason) {
    const auto startupPresentation = startupPresentationCoordinator_.GetSnapshot();
    if (!startupPresentation.revealPending || startupPresentation.revealCommitted) {
        return;
    }
    if (!IsStartupRevealChromeReady()) {
        return;
    }

    LOG("MainWindow: Chrome prepared during startup reveal, reason=", reason,
        " activeEffect=", resolvedChromeState_.effectiveActiveEffect.c_str(),
        " inactiveEffect=", resolvedChromeState_.effectiveInactiveEffect.c_str(),
        " transparent=", resolvedChromeState_.transparentBackground ? "true" : "false",
        " windowReadySignaled=", startupPresentation.windowReadySignaled ? "true" : "false",
        " visualReadySignaled=", startupPresentation.visualReadySignaled ? "true" : "false");

    ApplyStartupPresentationDecision(startupPresentationCoordinator_.OnChromeReady(), reason);
}

void MainWindow::SyncStartupRevealProjection() {
    const auto startupPresentation = startupPresentationCoordinator_.GetSnapshot();
    startupRevealPending_ = startupPresentation.revealPending;
    startupNavigationCompleted_ = startupPresentation.navigationCompleted;
    startupReadySignaled_ = startupPresentation.windowReadySignaled;
    startupRevealCommitted_ = startupPresentation.revealCommitted;
    startupRevealSettling_ = startupPresentation.revealSettling;
}

void MainWindow::BypassStartupRevealForImmediateShow() {
    // [DEPRECATED] cold-start reveal 路径下已不再调用此方法；保留方法体仅用于回退诊断。
    startupPresentationCoordinator_.MarkRevealStarted(false);
    startupPresentationCoordinator_.MarkRevealSettled();
    SyncStartupRevealProjection();

    startupSurfaceAuthorityPending_ = false;
    startupSurfaceCommitPending_ = false;
    pendingStartupSurfaceBounds_ = RECT{0, 0, 0, 0};
    hasPendingStartupSurfaceBounds_ = false;
    startupSurfaceFirstPresentCommitted_ = false;
    startupDeferredSurfaceConvergePending_ = false;
    deferredBackdropPending_ = false;

    // immediate show 模式下也启动 deferred backdrop timer，以触发 NRB 翻转，
    // 迫使 DWM 重建 visual tree。
    if (hwnd_ && IsWindow(hwnd_) && !isMinimized_) {
        needsAuthoritativeChromeReapply_ = true;
        SetTimer(hwnd_, DEFERRED_BACKDROP_TIMER_ID, DEFERRED_BACKDROP_DELAY_MS, nullptr);
    } else {
        needsAuthoritativeChromeReapply_ = false;
    }

    FB2K_console_formatter()
        << "[StartupRevealBypass]"
        << " t=" << WindowChromeTrace::RelativeMs() << "ms"
        << " mode=immediate-show"
        << " startupRevealPending=" << WindowChromeTrace::BoolText(startupRevealPending_)
        << " startupRevealCommitted=" << WindowChromeTrace::BoolText(startupRevealCommitted_)
        << " startupRevealSettling=" << WindowChromeTrace::BoolText(startupRevealSettling_);
}

void MainWindow::ApplyStartupPresentationDecision(const StartupPresentationCoordinatorDecision& decision,
                                                  const char* reason) {
    if (decision.cancelFallbackTimer && hwnd_ && IsWindow(hwnd_)) {
        KillTimer(hwnd_, STARTUP_REVEAL_TIMER_ID);
    }

    if (decision.armFallbackTimer) {
        ArmStartupFallbackTimer();
    }

    if (!decision.commitReveal) {
        SyncStartupRevealProjection();
        return;
    }

    LOG("MainWindow: Startup presentation commit requested, reason=", reason,
        " fallback=", decision.fallbackUsed ? "true" : "false");
    TryCommitStartupReveal();
}

bool MainWindow::RefreshStartupRevealSurface() {
    if (!hwnd_ || !IsWindow(hwnd_) || !webView_ || !webView_->IsReady() || isMinimized_) {
        return false;
    }

    RECT clientRect{};
    GetClientRect(hwnd_, &clientRect);
    if (!webView_->SetVisible(true)) {
        return false;
    }
    webView_->Resize(clientRect);
    return true;
}

bool MainWindow::EnsureSurfaceConvergedAfterNativeFrame(const char* reason) {
    if (!hwnd_ || !IsWindow(hwnd_) || !webView_ || !webView_->IsReady() || isMinimized_) {
        return false;
    }

    auto* controller = webView_->GetController();
    if (!controller) return false;

    RECT clientRect{};
    GetClientRect(hwnd_, &clientRect);

    RECT currentBounds{};
    controller->get_Bounds(&currentBounds);

    // 此处历史上直连 FB2K_console_formatter，绕开了 S_EmitEvidenceLine，
    // 因此辅助 trace 门控对其无效。改为显式门控；boundsMatch 只服务于该行日志，
    // 一并纳入门控内以免留下未使用变量。下方 nudge 是功能性收敛，不受门控影响。
    if (WindowChromeTrace::AuxiliaryTraceEnabled()) {
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
            << " match=" << WindowChromeTrace::BoolText(boundsMatch);
    }

    // bounded nudge: right-1 ? finalRect
    // 即使 bounds 相同，也需通过尺寸抖动迫使 DComp surface 重新合成；
    // 若 bounds 不同，则同时完成收敛。
    RECT nudged = clientRect;
    nudged.right = (nudged.right > 0) ? nudged.right - 1 : 0;
    webView_->Resize(nudged);
    webView_->Resize(clientRect);

    LogSurfaceDiagnostics((std::string("SurfaceConverge.done/") + reason).c_str());
    return true;
}

void MainWindow::FinalizeStartupRevealSettlement() {
    SyncStartupRevealProjection();
    if (!startupRevealCommitted_ || isMinimized_) {
        return;
    }

    if (startupRevealSettling_) {
        startupPresentationCoordinator_.MarkRevealSettled();
        SyncStartupRevealProjection();
    } else if (chromeBackdropBroadcastReady_) {
        return;
    }

    // [Chrome Reconciliation] 启动期到稳态的相位切换：
    // 在 startup first-present + settlement 完成后，将 chromePhase_ 切到 SteadyState。
    // SteadyState 后 shared chrome apply 会通过 applyNativeFrameStrategy hook
    // 把窗口切为 CaptionlessOverlapped，代替旧的 WS_POPUP 策略。
    chromePhase_ = MainChromePhase::SteadyState;

    chromeBackdropBroadcastReady_ = true;

    // 此处需短暂解除 deferredBackdropPending_ 门控，使 ApplyResolvedChrome
    // 能穿透 ApplyBackdropPolicyForActivation 中的延迟写入守卫。
    // 根因：deferredBackdropPending_ 的职责是阻止 ShowWindow 后同步消息触发的
    // DWM backdrop 重复写入，但它同时也阻止了 NativeFrameStrategy（style bits）
    // 和 applyFrameless（DWM frame extension）的应用，导致三大键在 ShowWindow
    // 后 ~200ms 内持续可见，直到 DEFERRED_BACKDROP_TIMER 触发才消失。
    const bool savedDeferredBackdrop = deferredBackdropPending_;
    deferredBackdropPending_ = false;
    ApplyResolvedChrome(true);
    deferredBackdropPending_ = savedDeferredBackdrop;

    // [Cold-start reveal] WS_CAPTION / WS_MINIMIZEBOX / WS_MAXIMIZEBOX 始终保留，
    // 三大键通过移除 WS_SYSMENU 消除（DWM 按钮 overlay 完全依赖此位），
    // 无需在 settlement 阶段恢复任何样式位。

    BroadcastWindowStateChangedIfNeeded(true);
    CaptureRuntimeDomProbe("FinalizeStartupRevealSettlement/+0ms", 0);
    ScheduleFinalizeStartupRuntimeProbes();
}

bool MainWindow::EnsureAuthoritativeNativeChrome(const char* reason) {
    TraceWindowPhase("EnsureAuthoritativeNativeChrome.enter", nullptr, "true",
        nullptr, nullptr, reason);

    if (!hwnd_ || !IsWindow(hwnd_) || isMinimized_) {
        TraceWindowPhase("EnsureAuthoritativeNativeChrome.skip", nullptr, "true",
            nullptr, nullptr, "reason=invalid-hwnd-or-minimized");
        return false;
    }

    LOG("MainWindow: Authoritative native chrome reapply, reason=", reason,
        " activeEffect=", resolvedChromeState_.effectiveActiveEffect.c_str(),
        " inactiveEffect=", resolvedChromeState_.effectiveInactiveEffect.c_str(),
        " transparent=", resolvedChromeState_.transparentBackground ? "true" : "false");

    // 仅在窗口真被 SW_HIDE 隐藏过时重置：DWM 对隐藏窗口写入 SYSTEMBACKDROP_TYPE 不会
    // 初始化合成管线，重设相同值被视为 no-op，必须 NONE→目标值制造真实状态变更。
    // 最小化不销毁合成状态（任务栏预览仍出图），此时重置只会让 acrylic 断掉一帧。
    // 门控留到窗口真正可见才消费：隐藏期的写入同样被 DWM 丢弃。
    const bool resetDwmComposition = dwmCompositionResetPending_ && IsWindowVisible(hwnd_);

    if (resetDwmComposition) {
        dwmCompositionResetPending_ = false;

        int resetType = DWMSBT_NONE_V;
        S_DwmSetWindowAttribute(hwnd_, DWMWA_SYSTEMBACKDROP_TYPE_V,
            &resetType, sizeof(resetType));

        // 重新扩展帧到客户区，确保 hidden→visible 后帧边距仍然生效
        MARGINS margins = { -1, -1, -1, -1 };
        S_DwmExtendFrameIntoClientArea(hwnd_, &margins);
    }

    ApplyResolvedChrome(true);
    // SWP_FRAMECHANGED 会让 DWM 重新评估帧合成，同样只在真重置后才需要。
    if (resetDwmComposition) {
        ChromeController::RefreshNativeFrame(hwnd_);
    }
    const std::string exitExtra = std::string(reason) +
        " dwmCompositionReset=" + WindowChromeTrace::BoolText(resetDwmComposition);
    LogSurfaceDiagnostics((std::string("EnsureAuthoritativeNativeChrome.exit/") + reason).c_str());
    CaptureRuntimeDomProbe(std::string("EnsureAuthoritativeNativeChrome.exit/") + reason);
    TraceWindowPhase("EnsureAuthoritativeNativeChrome.exit", nullptr, "true",
        nullptr, nullptr, exitExtra.c_str());
    return true;
}

bool MainWindow::ReapplyNativeChromeAfterStartupReveal() {
    return EnsureAuthoritativeNativeChrome("startup-reveal");
}

void MainWindow::TryCommitStartupReveal() {
    SyncStartupRevealProjection();
    const auto startupPresentation = startupPresentationCoordinator_.GetSnapshot();
    const bool fallbackUsed = startupPresentation.fallbackUsed;
    const bool visualReadySignaled = startupPresentation.visualReadySignaled;
    const std::string revealReason = fallbackUsed ? "fallback" : (visualReadySignaled ? "visual" : "ready");
    std::string enterExtra = std::string("startupNavigationCompleted=") +
        WindowChromeTrace::BoolText(startupNavigationCompleted_) +
        " startupReadySignaled=" + WindowChromeTrace::BoolText(startupReadySignaled_) +
        " savedShowCmd=" + std::to_string(savedShowCmd_) +
        " revealReason=" + revealReason;
    TraceWindowPhase("TryCommitStartupReveal.enter", nullptr, nullptr, nullptr, nullptr,
        enterExtra.c_str());

    if (startupRevealCommitted_) return;

    if (!hwnd_ || !IsWindow(hwnd_)) {
        startupPresentationCoordinator_.MarkClosed();
        SyncStartupRevealProjection();
        TraceWindowPhase("TryCommitStartupReveal.invalid-hwnd");
        return;
    }

    startupPresentationCoordinator_.MarkRevealStarted(fallbackUsed);
    SyncStartupRevealProjection();

    KillTimer(hwnd_, STARTUP_REVEAL_TIMER_ID);

    const bool canRefreshSurface = webView_ && webView_->IsReady();
    startupSurfaceAuthorityPending_ = canRefreshSurface && !isMinimized_;
    startupSurfaceCommitPending_ = startupSurfaceAuthorityPending_;
    pendingStartupSurfaceBounds_ = RECT{0, 0, 0, 0};
    hasPendingStartupSurfaceBounds_ = false;
    startupSurfaceFirstPresentCommitted_ = false;

    // 不在 ShowWindow 前让 WebView 进入最终 visible/bounds 状态，
    // 将 controller 置为 hidden + zero bounds，确保 post-show 的
    // SetVisible(true) + Resize(finalBounds) 是第一笔真实的 surface transition。
    // 实测已证明：仅有 visibility delta 不够，
    // bounds 也必须是真实 transition（从 zero 到 final）。
    if (canRefreshSurface) {
        webView_->SetVisible(false);
        webView_->Resize(RECT{0, 0, 0, 0});
    }

    int showCmd = savedShowCmd_;
    if (isMinimized_) {
        showCmd = SW_SHOWMINIMIZED;
    } else if (isMaximized_) {
        showCmd = SW_SHOWMAXIMIZED;
    } else if (showCmd == SW_SHOWMAXIMIZED ||
               showCmd == SW_SHOWMINIMIZED ||
               showCmd == SW_SHOWMINNOACTIVE ||
               showCmd == SW_MINIMIZE) {
        showCmd = SW_SHOW;
    }

    // 延迟 DWM 写入策略：不在 ShowWindow 之前写入 backdrop，
    // 而是在 ShowWindow 之后、窗口已可见时首次写入。
    // DWM 对隐藏窗口的 SYSTEMBACKDROP_TYPE 写入不会初始化合成管线，
    // 且 ShowWindow 后重写相同值被 DWM 视为 no-op。
    // 只 resolve state，供 ShowWindow 后立即使用。
    if (!isMinimized_ && showCmd != SW_SHOWMINIMIZED) {
        ResolveBackdropPolicy();
    }

    // 设置延迟 backdrop 门控：阻止 ShowWindow 后所有同步 DWM 写入，
    // 直到 DEFERRED_BACKDROP_TIMER 触发（约 200ms 后）。
    // 这包括 FinalizeStartupRevealSettlement → ApplyResolvedChrome 的写入。
    if (!isMinimized_ && showCmd != SW_SHOWMINIMIZED) {
        deferredBackdropPending_ = true;
    }

    // [首帧即内容] Reveal 前 DWM 管线重置。
    // DWM 对隐藏窗口设置的 SYSTEMBACKDROP_TYPE 不会初始化合成管线，
    // ShowWindow 后重设相同值被视为 no-op。必须先 reset 为 NONE，
    // 再写回正确值，迫使 DWM 看到真正的状态变更。
    if (!isMinimized_ && showCmd != SW_SHOWMINIMIZED) {
        // 1. DWM SYSTEMBACKDROP_TYPE reset ? NONE
        int resetType = DWMSBT_NONE_V;
        S_DwmSetWindowAttribute(hwnd_, DWMWA_SYSTEMBACKDROP_TYPE_V,
            &resetType, sizeof(resetType));

        // 2. 重新扩展帧到客户区
        MARGINS margins = { -1, -1, -1, -1 };
        S_DwmExtendFrameIntoClientArea(hwnd_, &margins);

        // 3. 仅重写 Mica/Acrylic backdrop 属性，不调用 ApplyResolvedChrome/RefreshNativeFrame。
        // 根因：ApplyResolvedChrome 在隐藏窗口上触发 ApplyFramelessState 的
        // SWP_FRAMECHANGED + RedrawWindow(RDW_FRAME)，再叠加 RefreshNativeFrame 的
        // 第二次 SWP_FRAMECHANGED + RedrawWindow(RDW_ERASE|RDW_ALLCHILDREN)，
        // 造成 DWM 帧合成管线不一致 → ShowWindow 后三大键 overlay 复现。
        // 完整 Chrome 层在 post-show FinalizeStartupRevealSettlement 中应用。
        EnableMicaEffect();
        TraceWindowPhase("PreShowDwmReset.micaRestore", nullptr, nullptr,
            nullptr, nullptr,
            canRefreshSurface ? "webview-ready=true" : "webview-ready=false");
    }

    // BackgroundService 协同：startupVisibilityHint_=false 时强制 SW_HIDE，
    // 让窗口从 cold-start reveal 起全程不可见，避免外部 SW_SHOWMINNOACTIVE +
    // 延迟隐藏序列与 reveal 流程争抢 ShowWindow 调用导致物理状态闪现。
    // chrome / backdrop 等 DWM 状态已在前面 ResolveBackdropPolicy / EnableMicaEffect
    // 中设置好，下次用户主动 Show 窗口时会立刻应用，不会丢失。
    const int finalShowCmd = startupVisibilityHint_ ? showCmd : SW_HIDE;
    std::string showExtra = std::string("showCmd=") + std::to_string(finalShowCmd) +
        " reason=" + revealReason +
        " visibilityHint=" + (startupVisibilityHint_ ? "visible" : "hidden");
    TraceWindowPhase("ShowWindow.before", nullptr, nullptr, nullptr, nullptr, showExtra.c_str());

    ShowWindow(hwnd_, finalShowCmd);

    // [Critical] ShowWindow 后立即触发 WM_NCCALCSIZE 以设置零 NC 面积。
    // Create()/OnCreate() 中的 SWP_FRAMECHANGED 在窗口隐藏 ~1.5s 后已失效，
    // DWM 在首次 ShowWindow 时重新初始化帧合成。此处仅用 SWP_FRAMECHANGED
    // 触发 WM_NCCALCSIZE（返回 0），即时消除 NC 面积。
    // 配合 startupShowWindowGuard_ 拦截 ShowWindow 内部的 WM_NCPAINT，
    // 双重保证三大键 overlay 不会在任何窗口状态（普通/最大化）下闪现。
    // 置 framelessDwmApplied_=true 短路后续 ApplyFramelessState 的
    // RedrawWindow(RDW_FRAME)——该组合操作在 cold-start 路径上会令 DWM
    // 重新评估帧装饰，复现三大键（a177ad2 证实）。
    SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0,
        SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    framelessDwmApplied_ = true;
    // 记下这次等效于「已按当前 frameless_ 应用过」，供 ApplyFramelessState 的
    // 守卫比较；不记的话后续真正的 frameless 切换会被误判成重复调用。
    lastFramelessDwmValue_ = frameless_;

    LogSurfaceDiagnostics("ShowWindow.after");
    CaptureRuntimeDomProbe("ShowWindow.after");
    TraceWindowPhase("ShowWindow.after", nullptr, nullptr, nullptr, nullptr, showExtra.c_str());

    // 不在 ShowWindow 同步调用中写 DWM backdrop。
    // DWM 的 backdrop 合成管线只在窗口实际呈现后才建立。
    // 在 ShowWindow 的同一消息处理中写入 SYSTEMBACKDROP_TYPE，DWM 尚未完成
    // 首次合成，写入被 DWM 视为对未呈现窗口的操作，不会初始化 backdrop 层。
    // 改为 PostMessage 延迟到下一消息循环迭代，让 DWM 有时间完成首次合成。

    UpdateWindow(hwnd_);

    bool postShowSurfaceFirstPresent = false;
    bool postShowNativeChromeReapplied = false;

    // post-show first-present surface commit.
    // pre-show 已将 WebView controller 置为 hidden + zero bounds。
    // 此处的 SetVisible(true) + Resize(finalBounds) 是 DComp surface 的
    // 第一笔真实 transition，确保 DWM 合成管线被激活。
    // 实测已证明：DwmFlush alone 不是充分条件。
    // 实测已证明：reset->reapply alone 也不是充分条件。
    // 真正缺少的是 post-show 的 first-present surface delta。
    if (canRefreshSurface && !isMinimized_ && startupSurfaceCommitPending_) {
        RECT targetBounds{};
        if (hasPendingStartupSurfaceBounds_) {
            targetBounds = pendingStartupSurfaceBounds_;
        } else {
            GetClientRect(hwnd_, &targetBounds);
        }

        const std::string pendingBoundsText = hasPendingStartupSurfaceBounds_
            ? S_FormatNativeRect(pendingStartupSurfaceBounds_)
            : "N/A";

        {
            RECT preBounds{};
            bool hasPre = false;
            if (webView_->GetController()) {
                hasPre = SUCCEEDED(webView_->GetController()->get_Bounds(&preBounds));
            }
            std::ostringstream surfaceStream;
            surfaceStream << "[FirstPresentSurface] t=" << WindowChromeTrace::RelativeMs() << "ms"
                          << " phase=before"
                          << " owner=TryCommitStartupReveal"
                          << " startupSurfaceAuthorityPending=" << WindowChromeTrace::BoolText(startupSurfaceAuthorityPending_)
                          << " startupSurfaceCommitPending=" << WindowChromeTrace::BoolText(startupSurfaceCommitPending_)
                          << " hasPendingBounds=" << WindowChromeTrace::BoolText(hasPendingStartupSurfaceBounds_)
                          << " pendingBounds=" << pendingBoundsText.c_str()
                          << " preBounds=" << (hasPre
                              ? (std::string("(") + std::to_string(preBounds.left) + ","
                                 + std::to_string(preBounds.top) + ","
                                 + std::to_string(preBounds.right) + ","
                                 + std::to_string(preBounds.bottom) + ")").c_str()
                              : "N/A")
                          << " targetBounds=(" << targetBounds.left << "," << targetBounds.top
                          << "," << targetBounds.right << "," << targetBounds.bottom << ")"
                          << " showCmd=" << showCmd;
            S_EmitEvidenceLine(surfaceStream.str());
        }

        webView_->SetVisible(true);
        webView_->Resize(targetBounds);
        postShowSurfaceFirstPresent = true;
        startupSurfaceFirstPresentCommitted_ = true;

        const RECT committedBounds = targetBounds;
        const RECT deferredBounds = pendingStartupSurfaceBounds_;
        const bool hasDeferredSync = hasPendingStartupSurfaceBounds_ &&
            (deferredBounds.left != committedBounds.left ||
             deferredBounds.top != committedBounds.top ||
             deferredBounds.right != committedBounds.right ||
             deferredBounds.bottom != committedBounds.bottom);

        startupSurfaceCommitPending_ = false;
        startupSurfaceAuthorityPending_ = false;
        hasPendingStartupSurfaceBounds_ = false;

        if (hasDeferredSync) {
            const std::string deferredBoundsText = S_FormatNativeRect(deferredBounds);
            webView_->Resize(deferredBounds);

            std::ostringstream stream;
            stream << "[StartupSurfaceAuthority]"
                   << " t=" << WindowChromeTrace::RelativeMs() << "ms"
                   << " owner=TryCommitStartupReveal"
                   << " action=post-commit-sync"
                   << " bounds=" << deferredBoundsText.c_str();
            S_EmitEvidenceLine(stream.str());
            TraceWindowPhase("PostShowFirstPresent.post-commit-sync", nullptr, nullptr,
                nullptr, nullptr, deferredBoundsText.c_str());
        }

        {
            std::ostringstream surfaceStream;
            surfaceStream << "[FirstPresentSurface] t=" << WindowChromeTrace::RelativeMs() << "ms"
                          << " phase=after"
                          << " owner=TryCommitStartupReveal"
                          << " postShowSurfaceFirstPresent=true"
                          << " showCmd=" << showCmd;
            S_EmitEvidenceLine(surfaceStream.str());
        }
        TraceWindowPhase("PostShowFirstPresent.done");

        LOG("MainWindow: First-present surface commit after startup reveal, showCmd=", showCmd,
            " reason=", revealReason.c_str());
    }

    if (startupSurfaceAuthorityPending_) {
        std::ostringstream stream;
        stream << "[StartupSurfaceAuthority]"
               << " t=" << WindowChromeTrace::RelativeMs() << "ms"
               << " owner=TryCommitStartupReveal"
               << " action=release-without-commit"
               << " startupSurfaceCommitPending=" << WindowChromeTrace::BoolText(startupSurfaceCommitPending_)
               << " canRefreshSurface=" << WindowChromeTrace::BoolText(canRefreshSurface)
               << " isMinimized=" << WindowChromeTrace::BoolText(isMinimized_);
        S_EmitEvidenceLine(stream.str());
        startupSurfaceAuthorityPending_ = false;
        startupSurfaceCommitPending_ = false;
        hasPendingStartupSurfaceBounds_ = false;
    }

    if (!isMinimized_) {
        FinalizeStartupRevealSettlement();
    }

    // startup-only deferred surface converge: first-present + settlement 已完成，
    // 投递到下一消息周期做一次 bounded nudge，模拟"用户手动 resize"的 DComp 收敛。
    // 门控条件：cold startup reveal + first-present 已提交 + 非最小化 + 仅一次
    if (startupSurfaceFirstPresentCommitted_ && !isMinimized_ && hwnd_ && IsWindow(hwnd_)) {
        startupDeferredSurfaceConvergePending_ = true;
        PostMessage(hwnd_, WM_DEFERRED_SURFACE_CONVERGE, 0, 0);
        LOG("MainWindow: Scheduled startup-only deferred surface converge");
    }

    // 延迟首次 DWM backdrop 写入。使用 SetTimer 而非 PostMessage，
    // 确保 DWM 有足够时间完成窗口首次合成（PostMessage 仅延迟 ~16ms，
    // 常在同一 DWM 合成帧内执行）。200ms 约 12 帧(60Hz)，给 DWM 充分初始化时间。
    if (!isMinimized_ && hwnd_ && IsWindow(hwnd_)) {
        needsAuthoritativeChromeReapply_ = true;
        SetTimer(hwnd_, DEFERRED_BACKDROP_TIMER_ID, DEFERRED_BACKDROP_DELAY_MS, nullptr);
        LOG("MainWindow: Scheduled deferred backdrop apply (",
            DEFERRED_BACKDROP_DELAY_MS, "ms delay)");
    } else {
        needsAuthoritativeChromeReapply_ = false;
    }
    if (isMinimized_) {
        startupPresentationCoordinator_.MarkRevealSettled();
        SyncStartupRevealProjection();
    }

    LOG("MainWindow: Startup reveal committed, reason=",
        revealReason.c_str(), " showCmd=", showCmd,
        " postShowSurfaceFirstPresent=", postShowSurfaceFirstPresent ? "true" : "false",
        " postShowNativeChromeReapplied=", postShowNativeChromeReapplied ? "true" : "false",
        " startupSurfaceFirstPresentCommitted=", startupSurfaceFirstPresentCommitted_ ? "true" : "false");

    std::string exitExtra = std::string("showCmd=") + std::to_string(showCmd) +
        " reason=" + revealReason +
        " postShowSurfaceFirstPresent=" + WindowChromeTrace::BoolText(postShowSurfaceFirstPresent) +
        " postShowNativeChromeReapplied=" + WindowChromeTrace::BoolText(postShowNativeChromeReapplied) +
        " startupSurfaceFirstPresentCommitted=" + WindowChromeTrace::BoolText(startupSurfaceFirstPresentCommitted_);
    TraceWindowPhase("TryCommitStartupReveal.exit", nullptr, nullptr, nullptr, nullptr, exitExtra.c_str());
}

void MainWindow::ArmStartupFallbackTimer() {
    if (startupRevealCommitted_) return;
    SetTimer(hwnd_, STARTUP_REVEAL_TIMER_ID, STARTUP_REVEAL_TIMEOUT_MS, nullptr);
    LOG("MainWindow: Startup fallback timer armed (", STARTUP_REVEAL_TIMEOUT_MS, "ms)");
}
