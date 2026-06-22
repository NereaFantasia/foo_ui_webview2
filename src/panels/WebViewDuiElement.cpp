/**
 * WebViewDuiElement - DUI 面板实现
 */

#include "pch.h"
#include "panels/WebViewDuiElement.h"
#include "panels/PanelConfigDialog.h"
#include "core/WebViewContext.h"
#include <windowsx.h>  // GET_X_LPARAM, GET_Y_LPARAM
#include "webview/WebViewHost.h"
#include "api/BridgeCore.h"
#include "window/WindowManager.h"

// ============================================
// 静态成员初始化
// ============================================

bool WebViewDuiElementInstance::s_classRegistered = false;

// ============================================
// WebViewDuiElementInstance 实现
// ============================================

WebViewDuiElementInstance::WebViewDuiElementInstance(
    ui_element_config::ptr cfg, 
    ui_element_instance_callback::ptr callback)
    : m_callback(callback)
{
    // 解析配置数据为 PanelConfig
    if (cfg.is_valid() && cfg->get_data_size() > 0) {
        panelConfig_ = PanelConfig::Deserialize(
            static_cast<const uint8_t*>(cfg->get_data()), cfg->get_data_size());
    }
}

WebViewDuiElementInstance::~WebViewDuiElementInstance() {
    // 注销面板引用计数（仅在 OnWebViewReady 中注册成功后才注销）
    if (IsWebViewReady()) {
        WindowManager::GetInstance().UnregisterPanel();
    }
    
    // 销毁 WebView
    DestroyWebView();
    
    // 销毁窗口
    if (m_hostWnd && IsWindow(m_hostWnd)) {
        DestroyWindow(m_hostWnd);
        m_hostWnd = nullptr;
    }
}

void WebViewDuiElementInstance::Initialize(HWND parent) {
    // 注册窗口类
    if (!RegisterWindowClass()) {
        LOG("ERROR: Failed to register DUI element window class");
        return;
    }
    
    // 获取父窗口客户区大小
    RECT parentRect;
    GetClientRect(parent, &parentRect);
    
    // 创建子窗口 - 必须使用 WS_CHILD 才能嵌入 DUI 布局
    // 不设 WS_CHILD 会创建 owned 顶级窗口，导致独立窗口而非嵌入面板
    m_hostWnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"WebView DUI Panel",
        WS_CHILD | WS_CLIPCHILDREN,
        0, 0,
        0, 0,
        parent,
        nullptr,
        core_api::get_my_instance(),
        this  // 传递 this 指针
    );
    
    if (!m_hostWnd) {
        LOG("ERROR: Failed to create DUI element window, error: ", GetLastError());
        return;
    }
    
    // 在初始化 WebView 前，panelConfig_ 已在构造函数中设置
    if (panelConfig_.HasTemplateOverride()) {
        LOG("WebViewDuiElement: Using panel template: ", panelConfig_.templateName.c_str());
    }
    
    // 初始化 WebView (使用 DUI 模式)
    if (!InitializeWebView(m_hostWnd, WebViewPanelMode::DuiPanel)) {
        LOG("ERROR: Failed to initialize WebView for DUI panel");
    }
}

bool WebViewDuiElementInstance::RegisterWindowClass() {
    if (s_classRegistered) return true;
    
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = sizeof(void*);
    wc.hInstance = core_api::get_my_instance();
    wc.hIcon = nullptr;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;  // 透明背景
    wc.lpszMenuName = nullptr;
    wc.lpszClassName = CLASS_NAME;
    wc.hIconSm = nullptr;
    
    if (RegisterClassExW(&wc)) {
        s_classRegistered = true;
        return true;
    }
    
    // 类可能已存在
    DWORD error = GetLastError();
    if (error == ERROR_CLASS_ALREADY_EXISTS) {
        s_classRegistered = true;
        return true;
    }
    
    return false;
}

LRESULT CALLBACK WebViewDuiElementInstance::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    WebViewDuiElementInstance* self = nullptr;
    
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        self = static_cast<WebViewDuiElementInstance*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        // 保存 hwnd，因为此时 CreateWindowExW 还未返回
        self->m_hostWnd = hwnd;
    } else {
        self = reinterpret_cast<WebViewDuiElementInstance*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    
    if (self) {
        return self->HandleMessage(hwnd, msg, wParam, lParam);
    }
    
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT WebViewDuiElementInstance::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_SIZE: {
            // 编辑模式下不调整 WebView bounds，保持隐藏状态
            // 否则 ResizeWebView 会将 bounds 恢复为完整客户区，
            // 撤销 SetBoundsVisible(false) 的效果
            if (!m_editMode) {
                ResizeWebView();
            }
            return 0;
        }
        
        case WM_ERASEBKGND: {
            // 不擦除背景，避免闪烁
            return 1;
        }
        
        case WM_PAINT: {
            // 编辑模式下绘制占位符（WebView2 已隐藏）
            if (m_editMode) {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                RECT rc;
                GetClientRect(hwnd, &rc);
                PaintEditModePlaceholder(hdc, rc);
                EndPaint(hwnd, &ps);
                return 0;
            }
            break;  // 非编辑模式，交给 DefWindowProc
        }
        
        case WM_DESTROY: {
            // 清理
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            return 0;
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
        
        // ============================================
        // 右键菜单（编辑模式下显示配置选项）
        // ============================================
        case WM_CONTEXTMENU: {
            // 编辑模式下右键弹出配置菜单
            if (m_editMode) {
                POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                
                HMENU hMenu = CreatePopupMenu();
                if (hMenu) {
                    AppendMenuW(hMenu, MF_STRING, 1, L"Configure...");
                    
                    int cmd = TrackPopupMenu(hMenu, 
                        TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY,
                        pt.x, pt.y, 0, hwnd, nullptr);
                    
                    DestroyMenu(hMenu);
                    
                    if (cmd == 1) {
                        ShowConfigDialog();
                    }
                }
            }
            // 非编辑模式: 吞掉消息，防止冒泡到 DUI 父窗口弹出 fb2k 主菜单
            // WebView2 JS 层通过 DOM contextmenu 事件自行处理右键菜单
            return 0;
        }
        
        // ============================================
        // Visual Hosting 模式：鼠标消息转发
        // 编辑模式下不转发，让 m_hostWnd 正常接收鼠标事件
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
            if (!m_editMode && GetWebView() && GetWebView()->GetCompositionController() && GetWebView()->HandleMouseMessage(msg, wParam, lParam)) {
                return 0;  // 已处理
            }
            break;
    }
    
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void WebViewDuiElementInstance::set_configuration(ui_element_config::ptr data) {
    PanelConfig oldConfig = panelConfig_;
    PanelConfig newConfig;
    
    if (data.is_valid() && data->get_data_size() > 0) {
        newConfig = PanelConfig::Deserialize(
            static_cast<const uint8_t*>(data->get_data()), data->get_data_size());
    }
    
    panelConfig_ = newConfig;
    
    // 使用热重载机制应用配置变更
    if (oldConfig.HasChanged(newConfig) && IsWebViewReady()) {
        ApplyConfig(oldConfig, newConfig);
        
        // 编辑模式下保持 WebView 隐藏
        if (m_editMode && GetWebView()) {
            GetWebView()->SetBoundsVisible(false);
        }
    }
}

ui_element_config::ptr WebViewDuiElementInstance::get_configuration() {
    auto data = PanelConfig::Serialize(panelConfig_);
    return ui_element_config::g_create(
        g_webview_dui_element_guid,
        data.data(),
        data.size()
    );
}

void WebViewDuiElementInstance::notify(const GUID& p_what, t_size p_param1, 
                                        const void* p_param2, t_size p_param2size) {
    if (p_what == ui_element_notify_colors_changed) {
        // Colors changed notification
        if (IsWebViewReady() && GetBridge()) {
            bool isDark = m_callback->is_dark_mode();
            json data = {{"darkMode", isDark}};
            GetBridge()->EmitEvent("system:themeChanged", data);
            // Also emit ui:coloursChanged for SMP compatibility
            GetBridge()->EmitEvent("ui:coloursChanged", json::object());
        }
    }
    else if (p_what == ui_element_notify_font_changed) {
        // Font changed notification
        if (IsWebViewReady() && GetBridge()) {
            GetBridge()->EmitEvent("ui:fontChanged", json::object());
        }
    }
    else if (p_what == ui_element_notify_visibility_changed) {
        bool visible = (p_param1 != 0);
        if (IsWebViewReady() && GetBridge()) {
            json data = {{"visible", visible}};
            GetBridge()->EmitEvent("panel:visibilityChanged", data);
        }
    }
    else if (p_what == ui_element_notify_edit_mode_changed) {
        // 编辑模式变化
        bool wasEditMode = m_editMode;
        m_editMode = m_callback.is_valid() && m_callback->is_edit_mode_enabled();
        
        if (m_editMode != wasEditMode) {
            if (IsWebViewReady() && GetWebView()) {
                if (m_editMode) {
                    // 编辑模式 ON：隐藏 WebView2，显示占位符
                    GetWebView()->SetBoundsVisible(false);
                    LOG("WebViewDuiElement: Edit mode ON - hiding WebView2");
                } else {
                    // 编辑模式 OFF：恢复 WebView2
                    GetWebView()->SetBoundsVisible(true);
                    LOG("WebViewDuiElement: Edit mode OFF - showing WebView2");
                }
            }
            // 触发重绘（显示/隐藏占位符）
            if (m_hostWnd) {
                InvalidateRect(m_hostWnd, nullptr, TRUE);
            }
        }
    }
}

void WebViewDuiElementInstance::PaintEditModePlaceholder(HDC hdc, const RECT& rc) {
    // 检测深色模式
    bool isDark = m_callback.is_valid() && m_callback->is_dark_mode();
    
    // 背景
    HBRUSH hBgBrush = CreateSolidBrush(isDark ? RGB(40, 40, 40) : RGB(240, 240, 240));
    FillRect(hdc, &rc, hBgBrush);
    DeleteObject(hBgBrush);
    
    // 边框（虚线风格，指示可编辑区域）
    HPEN hPen = CreatePen(PS_DASH, 1, isDark ? RGB(100, 100, 100) : RGB(180, 180, 180));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, rc.left + 2, rc.top + 2, rc.right - 2, rc.bottom - 2);
    SelectObject(hdc, hOldBrush);
    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);
    
    // 文字
    SetBkMode(hdc, TRANSPARENT);
    
    // 标题字体
    HFONT hTitleFont = CreateFontW(-16, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    HFONT hOldFont = (HFONT)SelectObject(hdc, hTitleFont);
    
    // 绘制元素名称
    SetTextColor(hdc, isDark ? RGB(200, 200, 200) : RGB(60, 60, 60));
    RECT rcTitle = rc;
    rcTitle.bottom = rc.top + (rc.bottom - rc.top) / 2;
    DrawTextW(hdc, L"WebView2 Panel", -1, &rcTitle, 
        DT_CENTER | DT_BOTTOM | DT_SINGLELINE | DT_NOPREFIX);
    
    // 提示字体
    HFONT hHintFont = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    SelectObject(hdc, hHintFont);
    
    // 绘制提示文字
    SetTextColor(hdc, isDark ? RGB(140, 140, 140) : RGB(120, 120, 120));
    RECT rcHint = rc;
    rcHint.top = rc.top + (rc.bottom - rc.top) / 2 + 4;
    DrawTextW(hdc, L"Right-click to configure", -1, &rcHint, 
        DT_CENTER | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
    
    // 清理
    SelectObject(hdc, hOldFont);
    DeleteObject(hTitleFont);
    DeleteObject(hHintFont);
}

void WebViewDuiElementInstance::ShowConfigDialog() {
    PanelConfig oldConfig = panelConfig_;
    PanelConfig newConfig = panelConfig_;
    
    // 使用主窗口作为对话框的 owner，而非 m_hostWnd
    // 原因：DUI 编辑模式下更改布局/样式会触发元素重建，
    // 导致 m_hostWnd 被销毁，其 owned 的对话框也被自动销毁，
    // 在消息循环中引发 state 为 null 的访问违例崩溃
    HWND dialogParent = core_api::get_main_window();
    if (ShowPanelConfigDialog(dialogParent, newConfig)) {
        panelConfig_ = newConfig;
        
        // 通知 DUI 布局配置变化
        if (m_callback.is_valid()) {
            m_callback->on_min_max_info_change();
        }
        
        // 使用热重载机制应用配置变更
        if (IsWebViewReady()) {
            ApplyConfig(oldConfig, newConfig);
        }
        
        // 编辑模式下保持 WebView 隐藏（ApplyConfig 可能触发了导航或 resize）
        if (m_editMode && IsWebViewReady() && GetWebView()) {
            GetWebView()->SetBoundsVisible(false);
        }
        
        LOG("WebViewDuiElement: Config changed, template: ", 
            newConfig.templateName.empty() ? "(global)" : newConfig.templateName.c_str());
    }
}

ui_element_min_max_info WebViewDuiElementInstance::get_min_max_info() {
    ui_element_min_max_info info;
    info.m_min_width = 100;   // 最小宽度 100px
    info.m_min_height = 50;   // 最小高度 50px
    info.m_max_width = UINT32_MAX;
    info.m_max_height = UINT32_MAX;
    return info;
}

void WebViewDuiElementInstance::OnWebViewReady() {
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
    
    // 调用基类实现
    WebViewPanel::OnWebViewReady();
    
    LOG("WebViewDuiElementInstance::OnWebViewReady - DUI panel ready");
    
    // 通知前端当前是面板模式
    if (GetBridge()) {
        json data = {
            {"mode", "dui"},
            {"panelMode", true},
            {"windowId", panelId}
        };
        GetBridge()->EmitEvent("panel:initialized", data);
    }
}

// ============================================
// WebViewDuiElement 工厂实现
// ============================================

ui_element_instance_ptr WebViewDuiElement::instantiate(
    fb2k::hwnd_t parent, 
    ui_element_config::ptr cfg, 
    ui_element_instance_callback_ptr callback) 
{
    // 创建实例
    auto instance = fb2k::service_new<WebViewDuiElementInstance>(cfg, callback);
    
    // 初始化窗口
    instance->Initialize(parent);
    
    return instance;
}

ui_element_config::ptr WebViewDuiElement::get_default_configuration() {
    // 返回空配置
    return ui_element_config::g_create_empty(g_webview_dui_element_guid);
}

// ============================================
// 服务工厂注册
// ============================================

static service_factory_single_t<WebViewDuiElement> g_webview_dui_element_factory;

