/**
 * PanelConfigDialog - 面板配置对话框实现
 *
 * 手动创建模态对话框，风格与 PreferencesPage 一致。
 * v2: 完整支持所有 PanelConfig 字段，含 i18n。
 * v3: 分「页面」「行为」两组；模板旁可新建、打开文件夹；用不上的控件不占位置，窗口高度随布局调整。
 */

#include "pch.h"
#include "panels/PanelConfigDialog.h"
#include "panels/PanelDpi.h"
#include "core/FrontendDirectoryResolver.h"
#include "prefs/PreferencesPage.h"
#include "prefs/TemplateNamePrompt.h"
#include "utils/I18n.h"
#include "webview/OriginPolicy.h"
#include <foobar2000/SDK/coreDarkMode.h>
#include <CommCtrl.h>

// ============================================
// 对话框内部状态
// ============================================

struct PanelConfigDialogState {
    PanelConfig config;            // 当前编辑中的配置
    PanelConfig originalConfig;    // 进入对话框时的原始配置
    bool confirmed = false;        // 用户是否点击了 OK
    bool allowUrlVisible = false;  // 「允许此网站……」是否显示：只有填了外部网址才需要
    HFONT font = nullptr;          // 控件字体
    HFONT boldFont = nullptr;      // 分组标题字体

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
    IDC_PD_CHECK_ALLOW_URL  = 2017,
    IDC_PD_BTN_NEW_TEMPLATE = 2018,
    IDC_PD_BTN_OPEN_FOLDER  = 2019,
    IDC_PD_HEAD_PAGE        = 2020,
    IDC_PD_LINE_PAGE        = 2021,
    IDC_PD_HEAD_BEHAVIOR    = 2022,
    IDC_PD_LINE_BEHAVIOR    = 2023,
};

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
    } else {
        // 与面板加载时用同一个解析器，显示的就是面板实际会加载的目录
        const auto resolution = frontend_directory::Resolve(templateName);
        if (resolution.source == frontend_directory_policy::Source::None) {
            path = TR("(no template with an index.html was found)", "(未找到含 index.html 的模板)");
        } else {
            path = resolution.directory;
            if (!templateName.empty() &&
                resolution.source != frontend_directory_policy::Source::PanelTemplate) {
                path = path + L" " + TR("(fallback from: ", "(回退自: ") +
                    pfc::stringcvt::string_wide_from_utf8(templateName.c_str()).get_ptr() + L")";
            }
        }
    }

    SetWindowTextW(hEditPath, path.c_str());
}

// 「允许此网站……」只在填了外部网址时需要：本机与内置地址自动允许、没有可选的，不显示；
// 其他网址只在来源等于已允许的来源时勾选，换了网站就显示未允许。
static void UpdateAllowUrlCheck(HWND hwnd, PanelConfigDialogState& state) {
    HWND hCheck = GetDlgItem(hwnd, IDC_PD_CHECK_ALLOW_URL);
    if (!hCheck) return;

    const PanelConfig& config = state.config;
    const std::wstring url = pfc::stringcvt::string_wide_from_utf8(config.urlOverride.c_str()).get_ptr();
    const bool hasOrigin = !origin_policy::OriginOf(url).empty();
    const bool automatic = !origin_policy::PanelUrlTrustedOrigin(url, L"").empty();
    const bool approved = hasOrigin && config.UrlOrigin() == config.approvedUrlOrigin;

    state.allowUrlVisible = hasOrigin && !automatic;
    EnableWindow(hCheck, state.allowUrlVisible);
    SendMessageW(hCheck, BM_SETCHECK, (automatic || approved) ? BST_CHECKED : BST_UNCHECKED, 0);
}

// 重新读磁盘上的模板列表填进下拉框，选中配置里的模板；找不到时选「(跟随全局)」。
static void PopulateTemplateCombo(HWND hwnd, const PanelConfig& config) {
    HWND hCombo = GetDlgItem(hwnd, IDC_PD_COMBO_TEMPLATE);
    if (!hCombo) return;

    SendMessageW(hCombo, CB_RESETCONTENT, 0, 0);
    SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)TR("(Follow global)", "(跟随全局)"));
    const auto templates = webview_prefs::GetTemplateList();
    int selectedIndex = 0;
    for (size_t i = 0; i < templates.size(); i++) {
        std::wstring wname = pfc::stringcvt::string_wide_from_utf8(templates[i].c_str()).get_ptr();
        SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)wname.c_str());
        if (!config.templateName.empty() && templates[i] == config.templateName) {
            selectedIndex = (int)(i + 1);
        }
    }
    SendMessageW(hCombo, CB_SETCURSEL, selectedIndex, 0);
}

// 「打开文件夹」要有目录可开：URL 覆盖优先时没有，具体模板要在磁盘上，「跟随全局」要能解析出目录。
static void UpdateTemplateButtons(HWND hwnd, const PanelConfig& config) {
    bool canOpen = false;
    if (config.urlOverride.empty()) {
        canOpen = config.templateName.empty()
            ? frontend_directory::Resolve(config.templateName).source != frontend_directory_policy::Source::None
            : webview_prefs::TemplateExists(config.templateName);
    }
    EnableWindow(GetDlgItem(hwnd, IDC_PD_BTN_OPEN_FOLDER), canOpen);
}

// ============================================
// 布局
//
// 控件在 WM_CREATE 里只创建、不定位；位置全由这里按当前状态算出：隐藏的控件不占位置，
// 窗口高度随之调整。所有尺寸按 96 DPI 给出，再按窗口 DPI 换算。
// ============================================

// 把客户区高度调到 clientH，窗口保持垂直居中不动。
static void FitWindowToClient(HWND hwnd, int clientH) {
    RECT client;
    GetClientRect(hwnd, &client);
    if (client.bottom == clientH) return;

    RECT want = {0, 0, client.right, clientH};
    AdjustWindowRectEx(&want, (DWORD)GetWindowLongPtrW(hwnd, GWL_STYLE), FALSE,
                       (DWORD)GetWindowLongPtrW(hwnd, GWL_EXSTYLE));
    RECT window;
    GetWindowRect(hwnd, &window);
    const int newH = want.bottom - want.top;
    const int oldH = window.bottom - window.top;
    SetWindowPos(hwnd, nullptr, window.left, window.top - (newH - oldH) / 2, window.right - window.left, newH,
                 SWP_NOZORDER | SWP_NOACTIVATE);
}

static void LayoutDialog(HWND hwnd, const PanelConfigDialogState& state) {
    using panel_dpi::Scale;
    const int dpi = panel_dpi::GetDpi(hwnd);
    const int margin = Scale(14, dpi);
    const int labelH = Scale(16, dpi);
    const int ctrlH = Scale(22, dpi);
    const int checkH = Scale(20, dpi);
    const int gap = Scale(6, dpi);
    const int tight = Scale(2, dpi);
    const int sectionGap = Scale(12, dpi);

    RECT client;
    GetClientRect(hwnd, &client);
    const int contentW = client.right - margin * 2;
    const int halfW = (contentW - gap) / 2;

    auto place = [hwnd](int id, int x, int y, int w, int h) {
        if (HWND control = GetDlgItem(hwnd, id)) MoveWindow(control, x, y, w, h, TRUE);
    };

    // 分组标题：文字加一条延伸到右边的细线
    HDC dc = GetDC(hwnd);
    auto heading = [&](int headId, int lineId, int y) {
        HWND head = GetDlgItem(hwnd, headId);
        wchar_t text[64] = {};
        GetWindowTextW(head, text, 64);
        HFONT old = (HFONT)SelectObject(dc, state.boldFont);
        SIZE size = {};
        GetTextExtentPoint32W(dc, text, (int)wcslen(text), &size);
        SelectObject(dc, old);
        const int textW = size.cx + Scale(4, dpi);
        place(headId, margin, y, textW, labelH);
        place(lineId, margin + textW + gap, y + labelH / 2, contentW - textW - gap, 2);
    };

    int y = Scale(12, dpi);

    // ===== 「页面」组 =====
    heading(IDC_PD_HEAD_PAGE, IDC_PD_LINE_PAGE, y);
    y += labelH + gap;

    place(IDC_PD_STATIC_NAME, margin, y, contentW, labelH);
    y += labelH + tight;
    place(IDC_PD_EDIT_NAME, margin, y, contentW, ctrlH);
    y += ctrlH + gap;

    place(IDC_PD_STATIC_TEMPLATE, margin, y, contentW, labelH);
    y += labelH + tight;
    const int newW = Scale(76, dpi);
    const int openW = Scale(100, dpi);
    const int comboW = contentW - newW - openW - gap * 2;
    place(IDC_PD_COMBO_TEMPLATE, margin, y, comboW, Scale(200, dpi));   // 下拉框的高度含展开的列表
    place(IDC_PD_BTN_NEW_TEMPLATE, margin + comboW + gap, y, newW, ctrlH);
    place(IDC_PD_BTN_OPEN_FOLDER, margin + comboW + gap + newW + gap, y, openW, ctrlH);
    y += ctrlH + gap;

    place(IDC_PD_STATIC_URL, margin, y, contentW, labelH);
    y += labelH + tight;
    place(IDC_PD_EDIT_URL, margin, y, contentW, ctrlH);
    y += ctrlH + tight;

    // 括号前手动换行，免得按宽度断在词中间；留两行高度。不显示时一点位置都不占。
    const int allowUrlH = Scale(32, dpi);
    ShowWindow(GetDlgItem(hwnd, IDC_PD_CHECK_ALLOW_URL), state.allowUrlVisible ? SW_SHOW : SW_HIDE);
    if (state.allowUrlVisible) {
        place(IDC_PD_CHECK_ALLOW_URL, margin, y, contentW, allowUrlH);
        y += allowUrlH;
    }
    y += gap;

    place(IDC_PD_STATIC_PATH, margin, y, contentW, labelH);
    y += labelH + tight;
    place(IDC_PD_EDIT_PATH, margin, y, contentW, ctrlH);
    y += ctrlH + sectionGap;

    // ===== 「行为」组 =====
    heading(IDC_PD_HEAD_BEHAVIOR, IDC_PD_LINE_BEHAVIOR, y);
    y += labelH + gap;

    // 边框样式：标签在左，下拉框在右，一行
    const int edgeLabelW = Scale(100, dpi);
    place(IDC_PD_STATIC_EDGE, margin, y + (ctrlH - labelH) / 2, edgeLabelW, labelH);
    place(IDC_PD_COMBO_EDGE, margin + edgeLabelW, y, Scale(140, dpi), Scale(120, dpi));
    y += ctrlH + gap;

    place(IDC_PD_CHECK_TRANSPARENT, margin, y, halfW, checkH);
    place(IDC_PD_CHECK_FOCUS, margin + halfW + gap, y, halfW, checkH);
    y += checkH + tight;
    place(IDC_PD_CHECK_DRAGDROP, margin, y, halfW, checkH);
    place(IDC_PD_CHECK_DEVTOOLS, margin + halfW + gap, y, halfW, checkH);
    y += checkH + sectionGap;

    // ===== 按钮 =====
    const int btnW = Scale(80, dpi);
    const int btnH = Scale(26, dpi);
    int btnX = client.right - margin - btnW;
    place(IDC_PD_BTN_CANCEL, btnX, y, btnW, btnH);
    btnX -= btnW + Scale(8, dpi);
    place(IDC_PD_BTN_OK, btnX, y, btnW, btnH);
    y += btnH + margin;

    ReleaseDC(hwnd, dc);
    FitWindowToClient(hwnd, y);
}

// 页面来源相关的显示一起刷新：路径、「允许此网站……」、模板按钮，再重排。
static void RefreshPageState(HWND hwnd, PanelConfigDialogState& state) {
    UpdatePathDisplay(hwnd, state.config.templateName, state.config.urlOverride);
    UpdateAllowUrlCheck(hwnd, state);
    UpdateTemplateButtons(hwnd, state.config);
    LayoutDialog(hwnd, state);
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
        int dpi = panel_dpi::GetDpi(hwnd);

        // 创建 DPI 感知字体
        NONCLIENTMETRICSW ncm = {};
        ncm.cbSize = sizeof(ncm);
        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
        ncm.lfMessageFont.lfHeight = panel_dpi::Scale(-12, dpi);
        state->font = CreateFontIndirectW(&ncm.lfMessageFont);
        ncm.lfMessageFont.lfWeight = FW_SEMIBOLD;
        state->boldFont = CreateFontIndirectW(&ncm.lfMessageFont);
        HFONT hFont = state->font;

        // 创建控件：不带位置，位置由 LayoutDialog 统一算。创建顺序即 Tab 顺序。
        auto make = [&](DWORD exStyle, const wchar_t* cls, const wchar_t* text, DWORD style, int id,
                        HFONT font) {
            HWND control = CreateWindowExW(exStyle, cls, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 0, 0, hwnd,
                                           (HMENU)(INT_PTR)id, hInst, nullptr);
            SendMessageW(control, WM_SETFONT, (WPARAM)font, TRUE);
            return control;
        };

        // ===== 「页面」组 =====
        make(0, L"STATIC", TR("Page", "页面"), SS_LEFT | SS_NOPREFIX, IDC_PD_HEAD_PAGE, state->boldFont);
        make(0, L"STATIC", L"", SS_ETCHEDHORZ, IDC_PD_LINE_PAGE, hFont);

        make(0, L"STATIC", TR("Panel Name:", "面板名称:"), SS_LEFT, IDC_PD_STATIC_NAME, hFont);
        HWND hEditName = make(WS_EX_CLIENTEDGE, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, IDC_PD_EDIT_NAME, hFont);
        if (!state->config.panelName.empty()) {
            SetWindowTextW(hEditName, pfc::stringcvt::string_wide_from_utf8(state->config.panelName.c_str()).get_ptr());
        }

        make(0, L"STATIC", TR("Template:", "模板:"), SS_LEFT, IDC_PD_STATIC_TEMPLATE, hFont);
        make(0, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, IDC_PD_COMBO_TEMPLATE, hFont);
        PopulateTemplateCombo(hwnd, state->config);
        make(0, L"BUTTON", TR("New...", "新建..."), BS_PUSHBUTTON | WS_TABSTOP, IDC_PD_BTN_NEW_TEMPLATE, hFont);
        make(0, L"BUTTON", TR("Open Folder", "打开文件夹"), BS_PUSHBUTTON | WS_TABSTOP, IDC_PD_BTN_OPEN_FOLDER, hFont);

        make(0, L"STATIC", TR("URL Override (optional):", "URL 覆盖 (可选):"), SS_LEFT, IDC_PD_STATIC_URL, hFont);
        HWND hEditUrl = make(WS_EX_CLIENTEDGE, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, IDC_PD_EDIT_URL, hFont);
        if (!state->config.urlOverride.empty()) {
            SetWindowTextW(hEditUrl, pfc::stringcvt::string_wide_from_utf8(state->config.urlOverride.c_str()).get_ptr());
        }

        make(0, L"BUTTON",
             TR("Allow this site to access foobar2000 and this computer\n(it can run programs and read or write files)",
                "允许此网站访问 foobar2000 和这台电脑\n（可运行程序、读写文件）"),
             BS_AUTOCHECKBOX | BS_MULTILINE | WS_TABSTOP, IDC_PD_CHECK_ALLOW_URL, hFont);

        make(0, L"STATIC", TR("Resources path:", "资源路径:"), SS_LEFT, IDC_PD_STATIC_PATH, hFont);
        make(WS_EX_CLIENTEDGE, L"EDIT", L"", ES_READONLY | ES_AUTOHSCROLL, IDC_PD_EDIT_PATH, hFont);

        // ===== 「行为」组 =====
        make(0, L"STATIC", TR("Behavior", "行为"), SS_LEFT | SS_NOPREFIX, IDC_PD_HEAD_BEHAVIOR, state->boldFont);
        make(0, L"STATIC", L"", SS_ETCHEDHORZ, IDC_PD_LINE_BEHAVIOR, hFont);

        make(0, L"STATIC", TR("Edge Style:", "边框样式:"), SS_LEFT, IDC_PD_STATIC_EDGE, hFont);
        HWND hComboEdge = make(0, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, IDC_PD_COMBO_EDGE, hFont);
        SendMessageW(hComboEdge, CB_ADDSTRING, 0, (LPARAM)TR("None", "无"));
        SendMessageW(hComboEdge, CB_ADDSTRING, 0, (LPARAM)TR("Sunken", "凹陷"));
        SendMessageW(hComboEdge, CB_ADDSTRING, 0, (LPARAM)TR("Grey", "灰色"));
        SendMessageW(hComboEdge, CB_SETCURSEL, state->config.edgeStyle, 0);

        HWND hCheck1 = make(0, L"BUTTON", TR("Transparent Background", "透明背景"), BS_AUTOCHECKBOX | WS_TABSTOP,
                            IDC_PD_CHECK_TRANSPARENT, hFont);
        SendMessageW(hCheck1, BM_SETCHECK, state->config.transparentBackground ? BST_CHECKED : BST_UNCHECKED, 0);
        HWND hCheck2 = make(0, L"BUTTON", TR("Grab Focus", "获取焦点"), BS_AUTOCHECKBOX | WS_TABSTOP,
                            IDC_PD_CHECK_FOCUS, hFont);
        SendMessageW(hCheck2, BM_SETCHECK, state->config.grabFocus ? BST_CHECKED : BST_UNCHECKED, 0);
        HWND hCheck3 = make(0, L"BUTTON", TR("Enable Drag && Drop", "启用拖放"), BS_AUTOCHECKBOX | WS_TABSTOP,
                            IDC_PD_CHECK_DRAGDROP, hFont);
        SendMessageW(hCheck3, BM_SETCHECK, state->config.enableDragDrop ? BST_CHECKED : BST_UNCHECKED, 0);
        HWND hCheck4 = make(0, L"BUTTON", TR("Enable DevTools", "启用开发者工具"), BS_AUTOCHECKBOX | WS_TABSTOP,
                            IDC_PD_CHECK_DEVTOOLS, hFont);
        SendMessageW(hCheck4, BM_SETCHECK, state->config.enableDevTools ? BST_CHECKED : BST_UNCHECKED, 0);

        // ===== 按钮 =====
        make(0, L"BUTTON", TR("OK", "确定"), BS_DEFPUSHBUTTON | WS_TABSTOP, IDC_PD_BTN_OK, hFont);
        make(0, L"BUTTON", TR("Cancel", "取消"), BS_PUSHBUTTON | WS_TABSTOP, IDC_PD_BTN_CANCEL, hFont);

        // 初始化显示并排版
        RefreshPageState(hwnd, *state);

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

                RefreshPageState(hwnd, *state);
            }
            break;

        case IDC_PD_BTN_NEW_TEMPLATE:
            if (state) {
                // 起名对话框是嵌套的模态窗口：返回后先确认本对话框还在
                webview_prefs::TemplateNamePrompt prompt;
                webview_prefs::FillCreateTemplatePrompt(prompt);
                if (!webview_prefs::PromptTemplateName(hwnd, prompt) || !IsWindow(hwnd)) break;

                std::wstring failure;
                const bool created = webview_prefs::CreateTemplateWithReport(prompt.accepted, failure);
                if (created) {
                    // 选中只进草稿，点「确定」才写入面板配置；取消也不回滚已建好的目录
                    state->config.templateName = prompt.accepted;
                }
                PopulateTemplateCombo(hwnd, state->config);   // 失败时按磁盘现状刷新
                RefreshPageState(hwnd, *state);
                if (!created) {
                    MessageBoxW(hwnd, failure.c_str(), L"WebView2 UI", MB_OK | MB_ICONERROR);
                }
            }
            break;

        case IDC_PD_BTN_OPEN_FOLDER:
            if (state) {
                if (!state->config.templateName.empty()) {
                    webview_prefs::OpenTemplateFolder(hwnd, state->config.templateName);
                } else {
                    // 「跟随全局」：打开解析出来的目录
                    const auto resolution = frontend_directory::Resolve(state->config.templateName);
                    if (resolution.source != frontend_directory_policy::Source::None) {
                        webview_prefs::OpenFolderPath(hwnd, resolution.directory);
                    }
                }
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
                RefreshPageState(hwnd, *state);
            }
            break;

        case IDC_PD_CHECK_ALLOW_URL:
            if (state) {
                const bool allow =
                    SendDlgItemMessageW(hwnd, IDC_PD_CHECK_ALLOW_URL, BM_GETCHECK, 0, 0) == BST_CHECKED;
                state->config.approvedUrlOrigin = allow ? state->config.UrlOrigin() : std::string();
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

                // 只保留当前网址的允许，免得改回以前的网址时旧的允许悄悄生效
                if (state->config.approvedUrlOrigin != state->config.UrlOrigin()) {
                    state->config.approvedUrlOrigin.clear();
                }

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
        // 窗口即将被彻底销毁：释放字体，清除 state 指针防止悬垂引用
        if (state) {
            if (state->font) DeleteObject(state->font);
            if (state->boldFont) DeleteObject(state->boldFont);
            state->font = nullptr;
            state->boldFont = nullptr;
        }
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

    // 计算窗口大小和位置（居中于父窗口）。高度只是初值，创建后由布局按内容调整。
    int dpi = panel_dpi::GetDpi(parent);
    int dlgW = panel_dpi::Scale(420, dpi);
    int dlgH = panel_dpi::Scale(420, dpi);

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
