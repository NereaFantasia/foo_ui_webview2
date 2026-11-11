/**
 * WebViewDuiElement - DUI 面板实现
 */

#include "pch.h"
#include "panels/WebViewDuiElement.h"
#include "panels/PanelConfigDialog.h"
#include "panels/PanelDpi.h"
#include "core/FrontendDirectoryResolver.h"
#include "core/WebViewContext.h"
#include <windowsx.h>  // GET_X_LPARAM, GET_Y_LPARAM
#include "webview/WebViewHost.h"
#include "api/BridgeCore.h"
#include "api/EventEmit.h"
#include "api/generated/PanelSchema.h"
#include "api/generated/SystemSchema.h"
#include "api/generated/UiSchema.h"
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
    //
    // 不带 WS_CLIPCHILDREN：WebView 的子窗口盖满整个客户区，带了它，宿主窗口的背景就画不到
    // 子窗口下面。页面透明处露出的正是子窗口下面的像素，DUI 换色后没人重画，会一直停在旧颜色。
    m_hostWnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"WebView DUI Panel",
        WS_CHILD | WS_CLIPSIBLINGS,
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
    
    // 编辑模式已开启时新建的面板（如从「替换 UI 元素」选入）不会再收到切换通知，初值要自己读，
    // 否则 WebView 满尺寸显示，右键落到页面上，进不了配置。
    m_editMode = m_callback.is_valid() && m_callback->is_edit_mode_enabled();

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
    wc.hbrBackground = nullptr;  // 背景由 WM_ERASEBKGND 按 DUI 配色画
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
            // 编辑模式的占位符自己会填满整个客户区，这里不必先擦一遍。
            if (m_editMode) return 1;
            // 用 DUI 当前的背景色填满客户区，包括 WebView 子窗口下面：页面透明处露出的就是它。
            RECT rc;
            GetClientRect(hwnd, &rc);
            if (HBRUSH brush = CreateSolidBrush(QueryHostBackground())) {
                FillRect(reinterpret_cast<HDC>(wParam), &rc, brush);
                DeleteObject(brush);
            }
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
        // 右键菜单
        // ============================================
        case WM_CONTEXTMENU: {
            // 编辑模式：交给默认处理，它把消息转给父窗口，由宿主弹出 DUI 的标准编辑菜单
            // （替换、剪切、复制、粘贴），本面板的「配置...」经 edit_mode_context_menu_* 并入其中。
            if (m_editMode) break;
            // 非编辑模式: 吞掉消息，防止冒泡到 DUI 父窗口弹出 fb2k 主菜单
            // WebView2 JS 层通过 DOM contextmenu 事件自行处理右键菜单
            return 0;
        }

        // 编辑模式下双击直接打开配置；其余情况与下面的鼠标消息一起转发
        case WM_LBUTTONDBLCLK:
            if (m_editMode) {
                ShowConfigDialog();
                return 0;
            }
            [[fallthrough]];

        // ============================================
        // Visual Hosting 模式：鼠标消息转发
        // 编辑模式下不转发，让 m_hostWnd 正常接收鼠标事件
        // ============================================
        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
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

    // 占位符上的面板名、页面来源与选项随配置变化
    if (m_editMode && oldConfig.HasChanged(newConfig) && m_hostWnd && IsWindow(m_hostWnd)) {
        InvalidateRect(m_hostWnd, nullptr, TRUE);
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
        // 宿主窗口的背景要跟着换，不取决于页面是否就绪；面板在隐藏的页签里时，下次显示照样重画。
        if (m_hostWnd && IsWindow(m_hostWnd)) {
            InvalidateRect(m_hostWnd, nullptr, TRUE);
        }
        RefreshHostBackground();

        // Colors changed notification
        if (IsWebViewReady() && GetBridge()) {
            api::system::ThemeChangedPayload theme;
            // 与 PaintEditModePlaceholder 同一判法：回调无效时按浅色处理，不解引用空指针。
            theme.darkMode = m_callback.is_valid() && m_callback->is_dark_mode();
            api::emit::Emit<api::system::events::ThemeChanged>(*GetBridge(), theme);
            // Also emit ui:coloursChanged for SMP compatibility
            api::emit::Emit<api::ui::events::ColoursChanged>(*GetBridge(), {});
        }
    }
    else if (p_what == ui_element_notify_font_changed) {
        // 占位符用 DUI 的字体，编辑模式下要重画
        if (m_editMode && m_hostWnd && IsWindow(m_hostWnd)) {
            InvalidateRect(m_hostWnd, nullptr, TRUE);
        }
        // Font changed notification
        if (IsWebViewReady() && GetBridge()) {
            api::emit::Emit<api::ui::events::FontChanged>(*GetBridge(), {});
        }
    }
    else if (p_what == ui_element_notify_visibility_changed) {
        bool visible = (p_param1 != 0);
        if (IsWebViewReady() && GetBridge()) {
            api::panel::VisibilityChangedPayload payload;
            payload.visible = visible;
            api::emit::Emit<api::panel::events::VisibilityChanged>(*GetBridge(), payload);
        }
    }
    else if (p_what == ui_element_notify_edit_mode_changed) {
        // 编辑模式变化
        bool wasEditMode = m_editMode;
        m_editMode = m_callback.is_valid() && m_callback->is_edit_mode_enabled();
        
        if (m_editMode != wasEditMode) {
            // 每次进入编辑模式都重新解析页面来源：模板目录可能在编辑模式之外被增删
            m_placeholder.valid = false;
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

namespace {

// 按 fgPercent 把前景色混进背景色。
COLORREF BlendColour(COLORREF fg, COLORREF bg, int fgPercent) {
    auto mix = [fgPercent](int a, int b) { return (a * fgPercent + b * (100 - fgPercent)) / 100; };
    return RGB(mix(GetRValue(fg), GetRValue(bg)), mix(GetGValue(fg), GetGValue(bg)),
               mix(GetBValue(fg), GetBValue(bg)));
}

std::wstring WidenUtf8(const std::string& utf8) {
    return pfc::stringcvt::string_wide_from_utf8(utf8.c_str()).get_ptr();
}

} // namespace

COLORREF WebViewDuiElementInstance::QueryHostText() {
    if (m_callback.is_valid()) {
        return m_callback->query_std_color(ui_color_text);
    }
    return GetSysColor(COLOR_WINDOWTEXT);
}

void WebViewDuiElementInstance::UpdatePlaceholderText() {
    const PanelConfig& cfg = panelConfig_;
    m_placeholder.forConfig = cfg;
    m_placeholder.valid = true;

    m_placeholder.title = cfg.panelName.empty() ? std::wstring(TR("WebView2 Panel", "WebView2 面板"))
                                                : WidenUtf8(cfg.panelName);

    // 页面来源：与配置对话框、面板加载用同一个解析器，显示的就是面板实际会加载的页面。
    const std::wstring pagePrefix = TR("Page: ", "页面：");
    if (cfg.HasUrlOverride()) {
        m_placeholder.source = pagePrefix + WidenUtf8(cfg.urlOverride);
    } else {
        using Source = frontend_directory_policy::Source;
        const auto resolution = frontend_directory::Resolve(cfg.templateName);
        const std::wstring templateLabel = pagePrefix + std::wstring(TR("template ", "模板 ")) +
                                           WidenUtf8(cfg.templateName);
        if (resolution.source == Source::None) {
            m_placeholder.source = pagePrefix + std::wstring(TR("no template with an index.html was found",
                                                                "未找到含 index.html 的模板"));
        } else if (cfg.templateName.empty()) {
            m_placeholder.source = pagePrefix + std::wstring(TR("follows the global setting", "跟随全局设置"));
        } else if (resolution.source == Source::PanelTemplate) {
            m_placeholder.source = templateLabel;
        } else {
            m_placeholder.source = templateLabel + std::wstring(TR(" not found, fell back", " 未找到，已回退"));
        }
    }

    // 已开启的选项，顺序固定；一个都没开则这一行为空，不显示。
    std::wstring options;
    auto add = [&options](bool on, const wchar_t* label) {
        if (!on) return;
        if (!options.empty()) options += L" · ";
        options += label;
    };
    add(cfg.transparentBackground, TR("Transparent Background", "透明背景"));
    add(cfg.grabFocus, TR("Grab Focus", "获取焦点"));
    add(cfg.enableDragDrop, TR("Drag & Drop", "拖放"));
    add(cfg.enableDevTools, TR("DevTools", "开发者工具"));
    m_placeholder.options = std::move(options);

    m_placeholder.hint = TR("Right-click or double-click to configure", "右键或双击进行配置");
}

void WebViewDuiElementInstance::PaintEditModePlaceholder(HDC hdc, const RECT& rc) {
    if (!m_placeholder.valid || m_placeholder.forConfig != panelConfig_) {
        UpdatePlaceholderText();
    }

    const int dpi = panel_dpi::GetDpi(m_hostWnd);
    const COLORREF bg = QueryHostBackground();
    const COLORREF text = QueryHostText();
    const COLORREF dim = BlendColour(text, bg, 60);
    const COLORREF border = BlendColour(text, bg, 40);

    // 背景：与不在编辑模式时页面透明处同色
    if (HBRUSH brush = CreateSolidBrush(bg)) {
        FillRect(hdc, &rc, brush);
        DeleteObject(brush);
    }

    // 虚线框指示可编辑区域，按 DPI 内缩
    const int inset = panel_dpi::Scale(4, dpi);
    if (HPEN pen = CreatePen(PS_DASH, 1, border)) {
        HPEN oldPen = (HPEN)SelectObject(hdc, pen);
        HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, rc.left + inset, rc.top + inset, rc.right - inset, rc.bottom - inset);
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(pen);
    }

    // 字体：借用 DUI 的默认字体（归 DUI 所有，不删）；取不到时退回系统消息字体。标题在其基础上放大加粗。
    HFONT hostFont = m_callback.is_valid() ? m_callback->query_font_ex(ui_font_default) : nullptr;
    LOGFONTW lf = {};
    if (!hostFont || GetObjectW(hostFont, sizeof(lf), &lf) != sizeof(lf)) {
        NONCLIENTMETRICSW ncm = {};
        ncm.cbSize = sizeof(ncm);
        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
        lf = ncm.lfMessageFont;
        lf.lfHeight = panel_dpi::Scale(-12, dpi);
        hostFont = nullptr;
    }
    HFONT ownedBody = hostFont ? nullptr : CreateFontIndirectW(&lf);
    HFONT bodyFont = hostFont ? hostFont : ownedBody;
    LOGFONTW titleLf = lf;
    titleLf.lfHeight = MulDiv(lf.lfHeight, 125, 100);
    titleLf.lfWeight = FW_SEMIBOLD;
    HFONT titleFont = CreateFontIndirectW(&titleLf);

    // 行：标题、页面来源、选项、提示。空间不够时按 选项、页面来源、提示 的顺序丢，最后只剩标题。
    struct Line {
        const std::wstring* text;
        HFONT font;
        COLORREF colour;
        int height;
        bool show;
    };
    Line lines[4] = {
        {&m_placeholder.title, titleFont, text, 0, true},
        {&m_placeholder.source, bodyFont, dim, 0, !m_placeholder.source.empty()},
        {&m_placeholder.options, bodyFont, dim, 0, !m_placeholder.options.empty()},
        {&m_placeholder.hint, bodyFont, dim, 0, !m_placeholder.hint.empty()},
    };
    HFONT oldFont = (HFONT)GetCurrentObject(hdc, OBJ_FONT);
    for (Line& line : lines) {
        TEXTMETRICW tm = {};
        SelectObject(hdc, line.font);
        GetTextMetricsW(hdc, &tm);
        line.height = tm.tmHeight;
    }

    const int gap = panel_dpi::Scale(4, dpi);
    const int margin = inset + panel_dpi::Scale(12, dpi);
    const int available = (rc.bottom - rc.top) - 2 * margin;
    auto totalHeight = [&]() {
        int total = 0;
        int shown = 0;
        for (const Line& line : lines) {
            if (!line.show) continue;
            total += line.height;
            ++shown;
        }
        return shown > 0 ? total + gap * (shown - 1) : 0;
    };
    for (int drop : {2, 1, 3}) {
        if (totalHeight() <= available) break;
        lines[drop].show = false;
    }

    SetBkMode(hdc, TRANSPARENT);
    int y = rc.top + ((rc.bottom - rc.top) - totalHeight()) / 2;
    for (const Line& line : lines) {
        if (!line.show) continue;
        RECT lineRect = {rc.left + margin, y, rc.right - margin, y + line.height};
        SelectObject(hdc, line.font);
        SetTextColor(hdc, line.colour);
        DrawTextW(hdc, line.text->c_str(), -1, &lineRect,
                  DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
        y += line.height + gap;
    }

    SelectObject(hdc, oldFont);
    DeleteObject(titleFont);
    if (ownedBody) DeleteObject(ownedBody);
}

// ============================================
// 编辑模式菜单：把「配置...」并入 DUI 的标准编辑菜单
// ============================================

bool WebViewDuiElementInstance::edit_mode_context_menu_test(const POINT&, bool) {
    return true;
}

void WebViewDuiElementInstance::edit_mode_context_menu_build(const POINT&, bool, HMENU p_menu, unsigned p_id_base) {
    AppendMenuW(p_menu, MF_STRING, p_id_base, TR("Configure...", "配置..."));
}

void WebViewDuiElementInstance::edit_mode_context_menu_command(const POINT&, bool, unsigned p_id, unsigned p_id_base) {
    if (p_id == p_id_base) {
        ShowConfigDialog();
    }
}

bool WebViewDuiElementInstance::edit_mode_context_menu_get_focus_point(POINT& p_point) {
    RECT rc;
    if (!m_hostWnd || !GetWindowRect(m_hostWnd, &rc)) return false;
    p_point = {(rc.left + rc.right) / 2, (rc.top + rc.bottom) / 2};
    return true;
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

        // 占位符上的面板名、页面来源与选项随配置变化
        if (m_editMode && m_hostWnd && IsWindow(m_hostWnd)) {
            InvalidateRect(m_hostWnd, nullptr, TRUE);
        }

        LOG("WebViewDuiElement: Config changed, template: ", 
            newConfig.templateName.empty() ? "(global)" : newConfig.templateName.c_str());
    }
}

COLORREF WebViewDuiElementInstance::QueryHostBackground() {
    // 回调取不到颜色时 SDK 自己回退到系统色；回调本身无效时在这里回退。
    if (m_callback.is_valid()) {
        return m_callback->query_std_color(ui_color_background);
    }
    return GetSysColor(COLOR_WINDOW);
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

    // 基类把 WebView 设成了满尺寸；编辑模式下要收起，让占位符接管鼠标。
    if (m_editMode && GetWebView()) {
        GetWebView()->SetBoundsVisible(false);
    }

    LOG("WebViewDuiElementInstance::OnWebViewReady - DUI panel ready");
    
    // 通知前端当前是面板模式
    if (GetBridge()) {
        api::panel::InitializedPayload initialized;
        initialized.mode = "dui";
        initialized.panelMode = true;
        initialized.windowId = panelId;
        api::emit::Emit<api::panel::events::Initialized>(*GetBridge(), initialized);
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

