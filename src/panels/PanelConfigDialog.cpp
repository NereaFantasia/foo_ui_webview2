/**
 * PanelConfigDialog - 面板配置对话框实现
 * 
 * 手动创建模态对话框，风格与 PreferencesPage 一致。
 * v2: 完整支持所有 PanelConfig 字段，含 i18n。
 */

#include "pch.h"
#include "panels/PanelConfigDialog.h"
#include "core/PreferencesPage.h"
#include "utils/I18n.h"
#include <foobar2000/SDK/coreDarkMode.h>
#include <CommCtrl.h>

// ============================================
// 对话框内部状态
// ============================================

struct PanelConfigDialogState {
    PanelConfig config;            // 当前编辑中的配置
    PanelConfig originalConfig;    // 进入对话框时的原始配置
    bool confirmed = false;        // 用户是否点击了 OK
    
    fb2k::CCoreDarkModeHooks darkMode;  // 深色模式支持
};

// ============================================
// 控件 ID（v2 扩展）
// ============================================

enum PanelDlgControlIds {
    IDC_PD_STATIC_NAME      = 2001,
    IDC_PD_EDIT_NAME        = 2002,
    IDC_PD_STATIC_TEMPLATE  = 2003,
    IDC_PD_COMBO_TEMPLATE   = 2004,
    IDC_PD_STATIC_EDGE      = 2005,
    IDC_PD_COMBO_EDGE       = 2006,
    IDC_PD_STATIC_URL       = 2007,
    IDC_PD_EDIT_URL         = 2008,
    IDC_PD_CHECK_TRANSPARENT = 2009,
    IDC_PD_CHECK_FOCUS      = 2010,
    IDC_PD_CHECK_DRAGDROP   = 2011,
    IDC_PD_CHECK_DEVTOOLS   = 2012,
    IDC_PD_STATIC_PATH      = 2013,
    IDC_PD_EDIT_PATH        = 2014,
    IDC_PD_BTN_OK           = 2015,
    IDC_PD_BTN_CANCEL       = 2016,
};

// ============================================
// DPI 辅助函数
// ============================================

static int PanelDlgGetDpi(HWND hwnd) {
    typedef UINT (WINAPI *GetDpiForWindowFunc)(HWND);
    static GetDpiForWindowFunc pGetDpiForWindow = nullptr;
    static bool tried = false;
    
    if (!tried) {
        tried = true;
        HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
        if (hUser32) {
            pGetDpiForWindow = (GetDpiForWindowFunc)GetProcAddress(hUser32, "GetDpiForWindow");
        }
    }
    
    if (pGetDpiForWindow && hwnd) {
        UINT dpi = pGetDpiForWindow(hwnd);
        if (dpi > 0) return (int)dpi;
    }
    
    HDC hdc = GetDC(hwnd);
    int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(hwnd, hdc);
    return dpi > 0 ? dpi : 96;
}

static int PanelDlgDpiScale(int value, int dpi) {
    return MulDiv(value, dpi, 96);
}

// ============================================
// 更新资源路径显示
// ============================================

static void UpdatePathDisplay(HWND hwnd, const std::string& templateName, const std::string& urlOverride) {
    HWND hEditPath = GetDlgItem(hwnd, IDC_PD_EDIT_PATH);
    if (!hEditPath) return;
    
    std::wstring path;
    
    // URL 覆盖优先
    if (!urlOverride.empty()) {
        path = pfc::stringcvt::string_wide_from_utf8(urlOverride.c_str()).get_ptr();
    }
    else if (templateName.empty()) {
        // 跟随全局
        path = webview_prefs::GetActiveWebResourcesDir();
        if (path.empty()) {
            path = TR("(global template not found)", "(未找到全局模板)");
        }
    } else {
        // 复用运行时模板查找逻辑 — 检查 index.html 是否存在
        std::wstring baseDir = webview_prefs::GetWebResourcesBaseDir();
        std::wstring panelDir = baseDir + L"\\" + 
            pfc::stringcvt::string_wide_from_utf8(templateName.c_str()).get_ptr();
        std::wstring indexPath = panelDir + L"\\index.html";
        DWORD attrs = GetFileAttributesW(indexPath.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            path = panelDir;
        } else {
            // 模板目录缺失或不完整，显示回退后的全局路径
            path = webview_prefs::GetActiveWebResourcesDir();
            if (path.empty()) {
                path = panelDir + L" " + TR("(not found, will use global)", "(未找到，将使用全局模板)");
            } else {
                path = path + L" " + TR("(fallback from: ", "(回退自: ") + 
                    pfc::stringcvt::string_wide_from_utf8(templateName.c_str()).get_ptr() + L")";
            }
        }
    }
    
    SetWindowTextW(hEditPath, path.c_str());
}

// ============================================
// 对话框窗口过程
// ============================================

static LRESULT CALLBACK PanelConfigDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    PanelConfigDialogState* state = reinterpret_cast<PanelConfigDialogState*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    
    switch (msg) {
    case WM_CREATE:
    {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        state = static_cast<PanelConfigDialogState*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        
        HINSTANCE hInst = core_api::get_my_instance();
        int dpi = PanelDlgGetDpi(hwnd);
        
        // 创建 DPI 感知字体
        NONCLIENTMETRICSW ncm = {};
        ncm.cbSize = sizeof(ncm);
        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
        ncm.lfMessageFont.lfHeight = PanelDlgDpiScale(-12, dpi);
        HFONT hFont = CreateFontIndirectW(&ncm.lfMessageFont);
        
        int y = PanelDlgDpiScale(10, dpi);
        const int MARGIN = PanelDlgDpiScale(12, dpi);
        const int LABEL_H = PanelDlgDpiScale(16, dpi);
        const int CTRL_H = PanelDlgDpiScale(22, dpi);
        const int CHECK_H = PanelDlgDpiScale(20, dpi);
        const int GAP = PanelDlgDpiScale(6, dpi);
        
        RECT rc;
        GetClientRect(hwnd, &rc);
        int contentW = rc.right - MARGIN * 2;
        int halfW = (contentW - GAP) / 2;
        
        // ========== 第一行：面板名称 ==========
        HWND hLabel = CreateWindowExW(0, L"STATIC", TR("Panel Name:", "面板名称:"),
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            MARGIN, y, PanelDlgDpiScale(100, dpi), LABEL_H,
            hwnd, (HMENU)IDC_PD_STATIC_NAME, hInst, nullptr);
        SendMessageW(hLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
        y += LABEL_H + PanelDlgDpiScale(2, dpi);
        
        HWND hEditName = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_TABSTOP,
            MARGIN, y, contentW, CTRL_H,
            hwnd, (HMENU)IDC_PD_EDIT_NAME, hInst, nullptr);
        SendMessageW(hEditName, WM_SETFONT, (WPARAM)hFont, TRUE);
        if (!state->config.panelName.empty()) {
            SetWindowTextW(hEditName, pfc::stringcvt::string_wide_from_utf8(state->config.panelName.c_str()).get_ptr());
        }
        y += CTRL_H + GAP;
        
        // ========== 第二行：模板选择 + 边框样式 ==========
        hLabel = CreateWindowExW(0, L"STATIC", TR("Template:", "模板:"),
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            MARGIN, y, halfW, LABEL_H,
            hwnd, (HMENU)IDC_PD_STATIC_TEMPLATE, hInst, nullptr);
        SendMessageW(hLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
        
        HWND hLabelEdge = CreateWindowExW(0, L"STATIC", TR("Edge Style:", "边框样式:"),
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            MARGIN + halfW + GAP, y, halfW, LABEL_H,
            hwnd, (HMENU)IDC_PD_STATIC_EDGE, hInst, nullptr);
        SendMessageW(hLabelEdge, WM_SETFONT, (WPARAM)hFont, TRUE);
        y += LABEL_H + PanelDlgDpiScale(2, dpi);
        
        // 模板下拉框
        HWND hCombo = CreateWindowExW(0, L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP,
            MARGIN, y, halfW, PanelDlgDpiScale(200, dpi),
            hwnd, (HMENU)IDC_PD_COMBO_TEMPLATE, hInst, nullptr);
        SendMessageW(hCombo, WM_SETFONT, (WPARAM)hFont, TRUE);
        
        // 填充模板选项
        SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)TR("(Follow global)", "(跟随全局)"));
        auto templates = webview_prefs::GetTemplateList();
        int selectedIndex = 0;
        for (size_t i = 0; i < templates.size(); i++) {
            std::wstring wname = pfc::stringcvt::string_wide_from_utf8(templates[i].c_str()).get_ptr();
            SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)wname.c_str());
            if (!state->config.templateName.empty() && templates[i] == state->config.templateName) {
                selectedIndex = (int)(i + 1);
            }
        }
        SendMessageW(hCombo, CB_SETCURSEL, selectedIndex, 0);
        
        // 边框样式下拉框
        HWND hComboEdge = CreateWindowExW(0, L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_TABSTOP,
            MARGIN + halfW + GAP, y, halfW, PanelDlgDpiScale(120, dpi),
            hwnd, (HMENU)IDC_PD_COMBO_EDGE, hInst, nullptr);
        SendMessageW(hComboEdge, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hComboEdge, CB_ADDSTRING, 0, (LPARAM)TR("None", "无"));
        SendMessageW(hComboEdge, CB_ADDSTRING, 0, (LPARAM)TR("Sunken", "凹陷"));
        SendMessageW(hComboEdge, CB_ADDSTRING, 0, (LPARAM)TR("Grey", "灰色"));
        SendMessageW(hComboEdge, CB_SETCURSEL, state->config.edgeStyle, 0);
        y += CTRL_H + GAP;
        
        // ========== 第三行：URL 覆盖 ==========
        hLabel = CreateWindowExW(0, L"STATIC", TR("URL Override (optional):", "URL 覆盖 (可选):"),
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            MARGIN, y, contentW, LABEL_H,
            hwnd, (HMENU)IDC_PD_STATIC_URL, hInst, nullptr);
        SendMessageW(hLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
        y += LABEL_H + PanelDlgDpiScale(2, dpi);
        
        HWND hEditUrl = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_TABSTOP,
            MARGIN, y, contentW, CTRL_H,
            hwnd, (HMENU)IDC_PD_EDIT_URL, hInst, nullptr);
        SendMessageW(hEditUrl, WM_SETFONT, (WPARAM)hFont, TRUE);
        if (!state->config.urlOverride.empty()) {
            SetWindowTextW(hEditUrl, pfc::stringcvt::string_wide_from_utf8(state->config.urlOverride.c_str()).get_ptr());
        }
        y += CTRL_H + GAP;
        
        // ========== 第四行：复选框 (2x2) ==========
        HWND hCheck1 = CreateWindowExW(0, L"BUTTON", TR("Transparent Background", "透明背景"),
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
            MARGIN, y, halfW, CHECK_H,
            hwnd, (HMENU)IDC_PD_CHECK_TRANSPARENT, hInst, nullptr);
        SendMessageW(hCheck1, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hCheck1, BM_SETCHECK, state->config.transparentBackground ? BST_CHECKED : BST_UNCHECKED, 0);
        
        HWND hCheck2 = CreateWindowExW(0, L"BUTTON", TR("Grab Focus", "获取焦点"),
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
            MARGIN + halfW + GAP, y, halfW, CHECK_H,
            hwnd, (HMENU)IDC_PD_CHECK_FOCUS, hInst, nullptr);
        SendMessageW(hCheck2, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hCheck2, BM_SETCHECK, state->config.grabFocus ? BST_CHECKED : BST_UNCHECKED, 0);
        y += CHECK_H + PanelDlgDpiScale(2, dpi);
        
        HWND hCheck3 = CreateWindowExW(0, L"BUTTON", TR("Enable Drag && Drop", "启用拖放"),
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
            MARGIN, y, halfW, CHECK_H,
            hwnd, (HMENU)IDC_PD_CHECK_DRAGDROP, hInst, nullptr);
        SendMessageW(hCheck3, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hCheck3, BM_SETCHECK, state->config.enableDragDrop ? BST_CHECKED : BST_UNCHECKED, 0);
        
        HWND hCheck4 = CreateWindowExW(0, L"BUTTON", TR("Enable DevTools", "启用开发者工具"),
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
            MARGIN + halfW + GAP, y, halfW, CHECK_H,
            hwnd, (HMENU)IDC_PD_CHECK_DEVTOOLS, hInst, nullptr);
        SendMessageW(hCheck4, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hCheck4, BM_SETCHECK, state->config.enableDevTools ? BST_CHECKED : BST_UNCHECKED, 0);
        y += CHECK_H + GAP;
        
        // ========== 第五行：资源路径（只读） ==========
        hLabel = CreateWindowExW(0, L"STATIC", TR("Resources path:", "资源路径:"),
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            MARGIN, y, contentW, LABEL_H,
            hwnd, (HMENU)IDC_PD_STATIC_PATH, hInst, nullptr);
        SendMessageW(hLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
        y += LABEL_H + PanelDlgDpiScale(2, dpi);
        
        HWND hEditPath = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_READONLY | ES_AUTOHSCROLL,
            MARGIN, y, contentW, CTRL_H,
            hwnd, (HMENU)IDC_PD_EDIT_PATH, hInst, nullptr);
        SendMessageW(hEditPath, WM_SETFONT, (WPARAM)hFont, TRUE);
        y += CTRL_H + PanelDlgDpiScale(10, dpi);
        
        // ========== 第六行：OK / Cancel 按钮 ==========
        int btnW = PanelDlgDpiScale(80, dpi);
        int btnH = PanelDlgDpiScale(26, dpi);
        int btnX = rc.right - MARGIN - btnW;
        
        HWND hBtnCancel = CreateWindowExW(0, L"BUTTON", TR("Cancel", "取消"),
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
            btnX, y, btnW, btnH,
            hwnd, (HMENU)IDC_PD_BTN_CANCEL, hInst, nullptr);
        SendMessageW(hBtnCancel, WM_SETFONT, (WPARAM)hFont, TRUE);
        
        btnX -= btnW + PanelDlgDpiScale(8, dpi);
        HWND hBtnOK = CreateWindowExW(0, L"BUTTON", TR("OK", "确定"),
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP,
            btnX, y, btnW, btnH,
            hwnd, (HMENU)IDC_PD_BTN_OK, hInst, nullptr);
        SendMessageW(hBtnOK, WM_SETFONT, (WPARAM)hFont, TRUE);
        
        // 初始化路径显示
        UpdatePathDisplay(hwnd, state->config.templateName, state->config.urlOverride);
        
        // 深色模式
        state->darkMode.AddDialogWithControls(hwnd);
        
        return 0;
    }
    
    case WM_ERASEBKGND:
    {
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hwnd, &rc);
        
        auto api = ui_config_manager::tryGet();
        bool isDark = api.is_valid() && api->is_dark_mode();
        
        HBRUSH hBrush = CreateSolidBrush(isDark ? RGB(32, 32, 32) : GetSysColor(COLOR_3DFACE));
        FillRect(hdc, &rc, hBrush);
        DeleteObject(hBrush);
        return TRUE;
    }
    
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
    case WM_CTLCOLOREDIT:
    {
        HDC hdc = (HDC)wParam;
        auto api = ui_config_manager::tryGet();
        if (api.is_valid() && api->is_dark_mode()) {
            SetTextColor(hdc, RGB(222, 222, 222));
            SetBkColor(hdc, RGB(32, 32, 32));
            static HBRUSH hDarkBrush = CreateSolidBrush(RGB(32, 32, 32));
            return (LRESULT)hDarkBrush;
        }
        break;
    }
    
    case WM_COMMAND:
    {
        switch (LOWORD(wParam)) {
        case IDC_PD_COMBO_TEMPLATE:
            if (HIWORD(wParam) == CBN_SELCHANGE && state) {
                HWND hCombo = GetDlgItem(hwnd, IDC_PD_COMBO_TEMPLATE);
                int sel = (int)SendMessageW(hCombo, CB_GETCURSEL, 0, 0);
                
                if (sel == 0) {
                    state->config.templateName.clear();
                } else {
                    // 动态分配防缓冲溢出
                    int textLen = (int)SendMessageW(hCombo, CB_GETLBTEXTLEN, sel, 0);
                    std::wstring buf(static_cast<size_t>(textLen) + 1, L'\0');
                    SendMessageW(hCombo, CB_GETLBTEXT, sel, (LPARAM)buf.data());
                    state->config.templateName = pfc::stringcvt::string_utf8_from_wide(buf.c_str()).get_ptr();
                }
                
                UpdatePathDisplay(hwnd, state->config.templateName, state->config.urlOverride);
            }
            break;
        
        case IDC_PD_COMBO_EDGE:
            if (HIWORD(wParam) == CBN_SELCHANGE && state) {
                HWND hCombo = GetDlgItem(hwnd, IDC_PD_COMBO_EDGE);
                state->config.edgeStyle = (uint8_t)SendMessageW(hCombo, CB_GETCURSEL, 0, 0);
            }
            break;
        
        case IDC_PD_EDIT_URL:
            if (HIWORD(wParam) == EN_CHANGE && state) {
                int urlLen = GetWindowTextLengthW(GetDlgItem(hwnd, IDC_PD_EDIT_URL));
                std::wstring urlBuf(static_cast<size_t>(urlLen) + 1, L'\0');
                GetDlgItemTextW(hwnd, IDC_PD_EDIT_URL, urlBuf.data(), urlLen + 1);
                state->config.urlOverride = pfc::stringcvt::string_utf8_from_wide(urlBuf.c_str()).get_ptr();
                UpdatePathDisplay(hwnd, state->config.templateName, state->config.urlOverride);
            }
            break;
        
        case IDC_PD_CHECK_TRANSPARENT:
            if (state) {
                state->config.transparentBackground = 
                    (SendDlgItemMessageW(hwnd, IDC_PD_CHECK_TRANSPARENT, BM_GETCHECK, 0, 0) == BST_CHECKED);
            }
            break;
        
        case IDC_PD_CHECK_FOCUS:
            if (state) {
                state->config.grabFocus = 
                    (SendDlgItemMessageW(hwnd, IDC_PD_CHECK_FOCUS, BM_GETCHECK, 0, 0) == BST_CHECKED);
            }
            break;
        
        case IDC_PD_CHECK_DRAGDROP:
            if (state) {
                state->config.enableDragDrop = 
                    (SendDlgItemMessageW(hwnd, IDC_PD_CHECK_DRAGDROP, BM_GETCHECK, 0, 0) == BST_CHECKED);
            }
            break;
        
        case IDC_PD_CHECK_DEVTOOLS:
            if (state) {
                state->config.enableDevTools = 
                    (SendDlgItemMessageW(hwnd, IDC_PD_CHECK_DEVTOOLS, BM_GETCHECK, 0, 0) == BST_CHECKED);
            }
            break;
            
        case IDC_PD_BTN_OK:
            if (state) {
                // 读取面板名称（动态缓冲防截断）
                int nameLen = GetWindowTextLengthW(GetDlgItem(hwnd, IDC_PD_EDIT_NAME));
                std::wstring nameBuf(static_cast<size_t>(nameLen) + 1, L'\0');
                GetDlgItemTextW(hwnd, IDC_PD_EDIT_NAME, nameBuf.data(), nameLen + 1);
                state->config.panelName = pfc::stringcvt::string_utf8_from_wide(nameBuf.c_str()).get_ptr();
                
                state->confirmed = true;
            }
            DestroyWindow(hwnd);
            return 0;
            
        case IDC_PD_BTN_CANCEL:
            if (state) {
                state->confirmed = false;
            }
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    }
    
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            if (state) state->confirmed = false;
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    
    case WM_CLOSE:
        if (state) state->confirmed = false;
        DestroyWindow(hwnd);
        return 0;
        
    case WM_DESTROY:
        // 不使用 PostQuitMessage(0) —— 它会干扰 WebView2 的 COM STA 消息泵，
        // 导致待处理的 COM 回调丢失，引发 windows.storage.dll 崩溃。
        // 改用 PostThreadMessage 发送 WM_NULL 以唤醒 GetMessage，
        // 然后由消息循环的 IsWindow 检查退出。
        PostThreadMessage(GetCurrentThreadId(), WM_NULL, 0, 0);
        return 0;
    
    case WM_NCDESTROY:
        // 窗口即将被彻底销毁，清除 state 指针防止悬垂引用
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        return 0;
    }
    
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ============================================
// 公开接口
// ============================================

bool ShowPanelConfigDialog(HWND parent, PanelConfig& config) {
    // 注册窗口类
    static bool s_classRegistered = false;
    const wchar_t* className = L"WebViewPanelConfigDialog";
    
    if (!s_classRegistered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = PanelConfigDlgProc;
        wc.hInstance = core_api::get_my_instance();
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_3DFACE + 1);
        wc.lpszClassName = className;
        
        if (RegisterClassExW(&wc) || GetLastError() == ERROR_CLASS_ALREADY_EXISTS) {
            s_classRegistered = true;
        } else {
            return false;
        }
    }
    
    // 准备对话框状态
    PanelConfigDialogState state;
    state.config = config;
    state.originalConfig = config;
    state.confirmed = false;
    
    // 计算窗口大小和位置（居中于父窗口）
    int dpi = PanelDlgGetDpi(parent);
    int dlgW = PanelDlgDpiScale(400, dpi);   // 加宽以容纳更多控件
    int dlgH = PanelDlgDpiScale(320, dpi);   // 加高以容纳所有控件
    
    RECT parentRect = {};
    if (parent && IsWindow(parent)) {
        GetWindowRect(parent, &parentRect);
    } else {
        parentRect.left = 0;
        parentRect.top = 0;
        parentRect.right = GetSystemMetrics(SM_CXSCREEN);
        parentRect.bottom = GetSystemMetrics(SM_CYSCREEN);
    }
    
    int x = parentRect.left + (parentRect.right - parentRect.left - dlgW) / 2;
    int y = parentRect.top + (parentRect.bottom - parentRect.top - dlgH) / 2;
    
    // 创建模态风格的弹出窗口
    HWND hDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOOLWINDOW,
        className,
        TR("Panel Configuration", "面板配置"),
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN,
        x, y, dlgW, dlgH,
        parent,
        nullptr,
        core_api::get_my_instance(),
        &state);
    
    if (!hDlg) {
        return false;
    }
    
    // 禁用父窗口（模拟模态）
    if (parent && IsWindow(parent)) {
        EnableWindow(parent, FALSE);
    }
    
    ShowWindow(hDlg, SW_SHOW);
    UpdateWindow(hDlg);
    
    // 模态消息循环
    // 注意：DUI 编辑模式下宿主窗口可能被重建，导致 owned dialog 被自动销毁
    // 必须在每次迭代检查对话框是否仍然有效
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (!IsWindow(hDlg)) {
            // 对话框已被外部销毁（如 owner 窗口被 DUI 重建）
            break;
        }
        if (!IsDialogMessage(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    
    // 恢复父窗口
    if (parent && IsWindow(parent)) {
        EnableWindow(parent, TRUE);
        SetForegroundWindow(parent);
    }
    
    // 判断结果：用户确认且配置有变化
    if (state.confirmed && state.config.HasChanged(state.originalConfig)) {
        config = state.config;
        return true;
    }
    
    return false;
}
