/**
 * WebViewCuiPanel implementation
 */

#include "pch.h"
#include "WebViewCuiPanel.h"
#include "panels/PanelConfigDialog.h"
#include "core/WebViewContext.h"
#include "settings/SecurityConfig.h"
#include "api/BridgeCore.h"
#include "api/EventEmit.h"
#include "api/generated/PanelSchema.h"
#include "webview/HostShortcutRelay.h"
#include "webview/WebViewHost.h"
#include "window/WindowManager.h"

// ============================================
// Static member initialization
// ============================================

bool WebViewCuiPanel::s_classRegistered = false;

// Factory instance - multi-instance panel (must be static)
static uie::window_factory<WebViewCuiPanel> g_webview_cui_panel_factory;

// ============================================
// Constructor/Destructor
// ============================================

WebViewCuiPanel::WebViewCuiPanel() {
    FB2K_console_print("[WebViewCuiPanel] Construct");
}

WebViewCuiPanel::~WebViewCuiPanel() {
    FB2K_console_print("[WebViewCuiPanel] Destruct");
    UnregisterColourListener();
}

// ============================================
// Columns UI 颜色
// ============================================

COLORREF WebViewCuiPanel::QueryHostBackground() {
    // helper 取不到 Columns UI 服务时回退系统色。每次现建：只在擦除与换色时调用。
    return cui::colours::helper().get_colour(cui::colours::colour_background);
}

void WebViewCuiPanel::ColourListener::on_colour_changed(uint32_t changedItemsMask) const {
    if (changedItemsMask & cui::colours::colour_flag_background) {
        m_owner.OnHostColoursChanged();
    }
}

void WebViewCuiPanel::ColourListener::on_bool_changed(uint32_t changedItemsMask) const {
    // 切换深色模式会整套换色，通知里不一定带 colour_flag_background。
    if (changedItemsMask & cui::colours::bool_flag_dark_mode_enabled) {
        m_owner.OnHostColoursChanged();
    }
}

void WebViewCuiPanel::OnHostColoursChanged() {
    if (m_hostWnd && IsWindow(m_hostWnd)) {
        InvalidateRect(m_hostWnd, nullptr, TRUE);
    }
    RefreshHostBackground();
}

void WebViewCuiPanel::RegisterColourListener() {
    if (m_colourListener) return;
    cui::colours::manager::ptr api;
    if (!fb2k::std_api_try_get(api)) return;
    m_colourListener = std::make_unique<ColourListener>(*this);
    m_colourManager = api;
    // 旧版 Columns UI 的 register_common_callback 是空实现，不会收到通知；
    // 此时擦除仍现取颜色，只是换色后要等下一次重绘才更新。
    m_colourManager->register_common_callback(m_colourListener.get());
}

void WebViewCuiPanel::UnregisterColourListener() {
    if (m_colourListener && m_colourManager.is_valid()) {
        m_colourManager->deregister_common_callback(m_colourListener.get());
    }
    m_colourListener.reset();
    m_colourManager.reset();
}

// ============================================
// Window class registration
// ============================================

bool WebViewCuiPanel::RegisterWindowClass() {
    if (s_classRegistered) return true;
    
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = sizeof(void*);
    wc.hInstance = core_api::get_my_instance();
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;  // 背景由 WM_ERASEBKGND 按 Columns UI 配色画
    wc.lpszClassName = CLASS_NAME;
    
    if (RegisterClassExW(&wc) == 0) {
        FB2K_console_print("[WebViewCuiPanel] RegisterWindowClass failed");
        return false;
    }
    
    s_classRegistered = true;
    return true;
}

// ============================================
// uie::window interface implementation
// ============================================

HWND WebViewCuiPanel::create_or_transfer_window(
    HWND wnd_parent, 
    const uie::window_host_ptr& p_host,
    const ui_helpers::window_position_t& p_position) 
{
    FB2K_console_print("[WebViewCuiPanel] create_or_transfer_window");
    
    // Already have window, return it
    if (m_hostWnd != nullptr) {
        SetParent(m_hostWnd, wnd_parent);
        SetWindowPos(m_hostWnd, nullptr,
            p_position.x, p_position.y,
            p_position.cx, p_position.cy,
            SWP_NOZORDER);
        m_host = p_host;
        return m_hostWnd;
    }
    
    // Register window class
    if (!RegisterWindowClass()) {
        return nullptr;
    }
    
    // Save host
    m_host = p_host;
    
    // Create window (no WS_VISIBLE - host will show when ready)
    //
    // 不带 WS_CLIPCHILDREN：WebView 的子窗口盖满整个客户区，带了它，宿主窗口的背景就画不到
    // 子窗口下面，页面透明处露出的像素换色后没人重画（见 WebViewDuiElement 的同一处注释）。
    m_hostWnd = CreateWindowExW(
        WS_EX_CONTROLPARENT,
        CLASS_NAME,
        L"WebView2 CUI Panel",
        WS_CHILD | WS_CLIPSIBLINGS,
        p_position.x, p_position.y,
        p_position.cx, p_position.cy,
        wnd_parent,
        nullptr,
        core_api::get_my_instance(),
        this
    );
    
    if (m_hostWnd == nullptr) {
        FB2K_console_print("[WebViewCuiPanel] CreateWindow failed");
        return nullptr;
    }
    
    FB2K_console_print("[WebViewCuiPanel] Window created");

    RegisterColourListener();

    // 在初始化 WebView 前应用已恢复的面板配置
    if (panelConfig_.HasTemplateOverride()) {
        FB2K_console_print("[WebViewCuiPanel] Using panel template: ", panelConfig_.templateName.c_str());
    }
    
    // Initialize WebView
    InitializeWebView(m_hostWnd, WebViewPanelMode::CuiPanel);
    
    return m_hostWnd;
}

void WebViewCuiPanel::destroy_window() {
    FB2K_console_print("[WebViewCuiPanel] destroy_window");

    UnregisterColourListener();

    // 注销面板引用计数（仅在 OnWebViewReady 中注册成功后才注销）
    if (IsWebViewReady()) {
        WindowManager::GetInstance().UnregisterPanel();
    }
    DestroyWebView();
    
    if (m_hostWnd != nullptr) {
        DestroyWindow(m_hostWnd);
        m_hostWnd = nullptr;
    }
    
    m_host.reset();
}

// ============================================
// WebViewPanel virtual functions
// ============================================

void WebViewCuiPanel::OnWebViewReady() {
    // 通过 WindowManager 统一分配 panelId（DUI/CUI 共用计数器）
    auto& wm = WindowManager::GetInstance();
    if (!wm.IsInitialized()) {
        wm.InitializeForPanel();
    }
    std::string panelId = wm.GeneratePanelId();
    SetWindowId(panelId);
    
    // 重新注册到 WebViewContext（带 windowId 和 panel 指针）
    WebViewContext::GetInstance().UnregisterInstance(GetHwnd());
    WebViewContext::GetInstance().RegisterInstance(GetHwnd(), GetWebView(), GetBridge(), panelId, this);
    
    // WindowManager 引用计数
    wm.RegisterPanel();
    
    // Call base implementation (register APIs, setup callbacks, load frontend)
    WebViewPanel::OnWebViewReady();
    
    FB2K_console_print("[WebViewCuiPanel] WebView ready");
    
    // Notify frontend about CUI panel mode
    if (GetBridge()) {
        api::panel::InitializedPayload initialized;
        initialized.mode = "cui";
        initialized.panelMode = true;
        initialized.windowId = panelId;
        api::emit::Emit<api::panel::events::Initialized>(*GetBridge(), initialized);
    }
}

// ============================================
// 面板配置接口实现
// ============================================

bool WebViewCuiPanel::show_config_popup(HWND wnd_parent) {
    PanelConfig oldConfig = panelConfig_;
    PanelConfig newConfig = panelConfig_;
    
    if (ShowPanelConfigDialog(wnd_parent, newConfig)) {
        panelConfig_ = newConfig;
        
        // 如果 WebView 已就绪，使用热重载机制
        if (IsWebViewReady()) {
            ApplyConfig(oldConfig, newConfig);
        }
        
        FB2K_console_print("[WebViewCuiPanel] Config changed, template: ", 
            newConfig.templateName.empty() ? "(global)" : newConfig.templateName.c_str());
        return true;
    }
    
    return false;
}

void WebViewCuiPanel::get_config(stream_writer* p_writer, abort_callback& p_abort) const {
    PanelConfig::Write(p_writer, panelConfig_, p_abort);
}

void WebViewCuiPanel::set_config(stream_reader* p_reader, t_size p_size, abort_callback& p_abort) {
    panelConfig_ = PanelConfig::Read(p_reader, p_size, p_abort);
    
    // 注意：set_config 在 create_or_transfer_window 之前被 CUI host 调用
    // 此时窗口尚未创建，配置会在 create_or_transfer_window 中应用
    FB2K_console_print("[WebViewCuiPanel] Config restored, template: ", 
        panelConfig_.templateName.empty() ? "(global)" : panelConfig_.templateName.c_str());
}

// ============================================
// Window procedure
// ============================================

LRESULT CALLBACK WebViewCuiPanel::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    WebViewCuiPanel* pThis = nullptr;
    
    if (msg == WM_NCCREATE) {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        pThis = reinterpret_cast<WebViewCuiPanel*>(pCreate->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    } else {
        pThis = reinterpret_cast<WebViewCuiPanel*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    
    if (pThis) {
        return pThis->HandleMessage(hwnd, msg, wParam, lParam);
    }
    
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT WebViewCuiPanel::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_SIZE: {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            
            if (auto* webView = GetWebView()) {
                if (auto* controller = webView->GetController()) {
                    webView->SyncRasterizationScale();
                    RECT bounds = {0, 0, width, height};
                    controller->put_Bounds(bounds);
                }
            }
            return 0;
        }
        
        // 用 Columns UI 当前的背景色填满客户区，包括 WebView 子窗口下面：页面透明处露出的就是它。
        case WM_ERASEBKGND: {
            RECT rc;
            GetClientRect(hwnd, &rc);
            if (HBRUSH brush = CreateSolidBrush(QueryHostBackground())) {
                FillRect(reinterpret_cast<HDC>(wParam), &rc, brush);
                DeleteObject(brush);
            }
            return 1;
        }

        // ============================================
        // 焦点管理（用于 Selection API）
        // ============================================
        case WM_SETFOCUS: {
            OnSetFocus();
            return 0;
        }
        
        case WM_KILLFOCUS: {
            OnKillFocus();
            return 0;
        }
        
        // Keyboard shortcuts
        // 键盘焦点在面板宿主窗口时按键才到这里；WebView 有焦点时走 AcceleratorKeyPressed（规格 §5.3）。
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            // 输入法开着时 wParam 多为 VK_PROCESSKEY，先还原真实键码。
            const UINT vk = host_shortcut_relay::HostWindowKey(hwnd, wParam);
            const bool repeat = (lParam & (1 << 30)) != 0;
            // F12 to open DevTools (only in dev server mode)
            if (msg == WM_KEYDOWN && vk == VK_F12 && GetWebView() && GetWebView()->IsReady()) {
                if (security_config::UseDevServer()) {
                    GetWebView()->OpenDevTools();
                    return 0;
                }
            }
            // F5 to reload
            else if (msg == WM_KEYDOWN && vk == VK_F5 && GetWebView() && GetWebView()->IsReady()) {
                GetWebView()->Reload();
                return 0;
            }
            // 其余按键按转发规则交给 foobar2000 快捷键；执行后本面板可能已销毁，直接返回。
            else if (host_shortcut_relay::TryRelayFromHostWindow(hwnd, wParam, vk, host_shortcut_relay::CurrentModifiers(), repeat)) {
                return 0;
            }
            break;
        }
        
        // Mouse message forwarding (needed for Visual Hosting mode)
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
            if (GetWebView() && GetWebView()->GetCompositionController() && GetWebView()->HandleMouseMessage(msg, wParam, lParam)) {
                return 0;  // Handled
            }
            break;
        
        // 吞掉 WM_CONTEXTMENU，防止冒泡到 CUI 父窗口弹出 fb2k 主菜单
        // WebView2 JS 层通过 DOM contextmenu 事件自行处理右键菜单
        case WM_CONTEXTMENU:
            return 0;
        
        case WM_DESTROY:
            DestroyWebView();
            return 0;
    }
    
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
