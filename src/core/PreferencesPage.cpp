// ============================================
// PreferencesPage.cpp - WebView2 UI Preferences Page Implementation
// Preferences → Display → WebView2 UI
// ============================================

#include "pch.h"
#include "core/PreferencesPage.h"
#include "core/SecurityConfig.h"
#include "api/BridgeCore.h"
#include "core/UserInterface.h"
#include "window/MainWindow.h"
#include <CommCtrl.h>
#include <shlobj.h>
#include <filesystem>
#include <cstring>
#include <fstream>
#include <algorithm>
#include <utility>
#include "utils/I18n.h"

namespace fs = std::filesystem;

// ============================================
// 开发者选项 (从 main.cpp 引用 advconfig) - 全局命名空间
// ============================================
extern advconfig_checkbox_factory g_cfg_devtools;
extern advconfig_checkbox_factory g_cfg_local_network;
extern advconfig_checkbox_factory g_cfg_allow_insecure;
extern advconfig_checkbox_factory g_cfg_allow_insecure_tls;
extern advconfig_checkbox_factory g_cfg_background_mode;
extern advconfig_string_factory g_cfg_dev_server_url;
extern advconfig_checkbox_factory g_cfg_use_dev_server;

namespace webview_prefs {

// ============================================
// GUIDs
// ============================================

// Preferences Page GUID
// {B7E8F3A1-4C5D-2E9F-8A1B-6C7D8E9F0A2B}
static constexpr GUID guid_prefs_page = 
    { 0xb7e8f3a1, 0x4c5d, 0x2e9f, { 0x8a, 0x1b, 0x6c, 0x7d, 0x8e, 0x9f, 0x0a, 0x2b } };

// 暴露偏好设置页 GUID 供外部模块使用
const GUID& GetPreferencesPageGuid() {
    return guid_prefs_page;
}

// Configuration Variable GUIDs
// {B7E8F3A2-4C5D-2E9F-8A1B-6C7D8E9F0A2C}
static constexpr GUID guid_cfg_active_template = 
    { 0xb7e8f3a2, 0x4c5d, 0x2e9f, { 0x8a, 0x1b, 0x6c, 0x7d, 0x8e, 0x9f, 0x0a, 0x2c } };

// {B7E8F3A3-4C5D-2E9F-8A1B-6C7D8E9F0A2D}
static constexpr GUID guid_cfg_start_with_foobar = 
    { 0xb7e8f3a3, 0x4c5d, 0x2e9f, { 0x8a, 0x1b, 0x6c, 0x7d, 0x8e, 0x9f, 0x0a, 0x2d } };

// {B7E8F3A4-4C5D-2E9F-8A1B-6C7D8E9F0A2E}
static constexpr GUID guid_cfg_remember_position = 
    { 0xb7e8f3a4, 0x4c5d, 0x2e9f, { 0x8a, 0x1b, 0x6c, 0x7d, 0x8e, 0x9f, 0x0a, 0x2e } };

// {B7E8F3A5-4C5D-2E9F-8A1B-6C7D8E9F0A2F}
static constexpr GUID guid_cfg_auto_hide = 
    { 0xb7e8f3a5, 0x4c5d, 0x2e9f, { 0x8a, 0x1b, 0x6c, 0x7d, 0x8e, 0x9f, 0x0a, 0x2f } };

// {B7E8F3A6-4C5D-2E9F-8A1B-6C7D8E9F0A30}
static constexpr GUID guid_cfg_backdrop_effect = 
    { 0xb7e8f3a6, 0x4c5d, 0x2e9f, { 0x8a, 0x1b, 0x6c, 0x7d, 0x8e, 0x9f, 0x0a, 0x30 } };

// ============================================
// 配置变量
// ============================================

static cfg_string cfg_active_template(guid_cfg_active_template, "default");
static cfg_var_modern::cfg_bool cfg_start_with_foobar(guid_cfg_start_with_foobar, true);
static cfg_var_modern::cfg_bool cfg_remember_position(guid_cfg_remember_position, true);
static cfg_var_modern::cfg_bool cfg_auto_hide(guid_cfg_auto_hide, false);
static cfg_var_modern::cfg_int cfg_backdrop_effect(guid_cfg_backdrop_effect, static_cast<int>(BackdropEffect::Mica));

// ============================================
// Web 资源目录管理
// ============================================

std::wstring GetWebResourcesBaseDir() {
    // 获取 foobar2000 profile 目录
    pfc::string8 profilePath = core_api::get_profile_path();
    if (profilePath.startsWith("file://")) {
        profilePath = profilePath.subString(7);
    }
    
    std::wstring basePath = pfc::stringcvt::string_wide_from_utf8(profilePath.c_str()).get_ptr();
    
    // WebView UI 资源根目录: profile/webview-ui/
    basePath += L"\\webview-ui";
    
    // 确保目录存在
    if (!fs::exists(basePath)) {
        fs::create_directories(basePath);
    }
    
    return basePath;
}

std::wstring GetActiveWebResourcesDir() {
    std::wstring baseDir = GetWebResourcesBaseDir();
    const char* templateName = cfg_active_template.get();
    
    std::string templateStr = (templateName && templateName[0]) ? templateName : "default";
    
    std::wstring templateDir = baseDir + L"\\" + 
        pfc::stringcvt::string_wide_from_utf8(templateStr.c_str()).get_ptr();
    
    // 如果活动模板目录不存在，返回空（让调用者处理）
    if (!fs::exists(templateDir)) {
        return L"";
    }
    
    return templateDir;
}

std::wstring GetActiveTemplateName() {
    const char* name = cfg_active_template.get();
    std::string nameStr = name ? name : "default";
    return pfc::stringcvt::string_wide_from_utf8(nameStr.c_str()).get_ptr();
}

void SetActiveTemplateName(const std::string& name) {
    // Validate template name to prevent directory traversal
    if (name.empty()) return;
    for (char c : name) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-') {
            return;
        }
    }
    cfg_active_template.set(name.c_str());
}

// ============================================
// 模板管理
// ============================================

std::vector<std::string> GetTemplateList() {
    std::vector<std::string> templates;
    std::wstring baseDir = GetWebResourcesBaseDir();
    
    try {
        for (const auto& entry : fs::directory_iterator(baseDir)) {
            if (entry.is_directory()) {
                std::wstring dirName = entry.path().filename().wstring();
                
                // 过滤掉以点开头的隐藏目录（如.cache, .git等）
                if (!dirName.empty() && dirName[0] == L'.') {
                    continue;
                }
                
                std::string name = pfc::stringcvt::string_utf8_from_wide(dirName.c_str()).get_ptr();
                templates.push_back(name);
            }
        }
    } catch (...) {
        // 目录不存在或无法访问
    }
    
    // 如果没有模板，创建默认模板
    if (templates.empty()) {
        CreateTemplate("default");
        templates.emplace_back("default");
    }
    
    return templates;
}

bool CreateTemplate(const std::string& name) {
    if (name.empty() || TemplateExists(name)) {
        return false;
    }
    
    // 验证名称（只允许字母数字和下划线连字符）
    for (char c : name) {
        if (!std::isalnum(c) && c != '_' && c != '-') {
            return false;
        }
    }
    
    std::wstring baseDir = GetWebResourcesBaseDir();
    std::wstring templateDir = baseDir + L"\\" + 
        pfc::stringcvt::string_wide_from_utf8(name.c_str()).get_ptr();
    
    try {
        fs::create_directories(templateDir);
        
        // 创建基本的 index.html
        std::wstring indexPath = templateDir + L"\\index.html";
        std::ofstream indexFile(indexPath);
        if (indexFile.is_open()) {
            indexFile << R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>WebView2 UI - )" << name << R"(</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'Segoe UI', sans-serif;
            background: transparent;
            color: white;
            display: flex;
            align-items: center;
            justify-content: center;
            min-height: 100vh;
        }
        .container {
            text-align: center;
            padding: 40px;
        }
        h1 { font-size: 2rem; margin-bottom: 1rem; }
        p { opacity: 0.7; }
    </style>
</head>
<body>
    <div class="container">
        <h1>)" << name << R"(</h1>
        <p>Edit this template in: profile/webview-ui/)" << name << R"(/</p>
    </div>
</body>
</html>
)";
            indexFile.close();
        }
        
        return true;
    } catch (...) {
        return false;
    }
}

bool RenameTemplate(const std::string& oldName, const std::string& newName) {
    if (oldName.empty() || newName.empty() || !TemplateExists(oldName) || TemplateExists(newName)) {
        return false;
    }
    
    // 验证新名称
    for (char c : newName) {
        if (!std::isalnum(c) && c != '_' && c != '-') {
            return false;
        }
    }
    
    std::wstring baseDir = GetWebResourcesBaseDir();
    std::wstring oldDir = baseDir + L"\\" + 
        pfc::stringcvt::string_wide_from_utf8(oldName.c_str()).get_ptr();
    std::wstring newDir = baseDir + L"\\" + 
        pfc::stringcvt::string_wide_from_utf8(newName.c_str()).get_ptr();
    
    try {
        fs::rename(oldDir, newDir);
        
        // 如果重命名的是当前活动模板，更新配置
        const char* currentTemplate = cfg_active_template.get();
        if (currentTemplate && strcmp(currentTemplate, oldName.c_str()) == 0) {
            cfg_active_template.set(newName.c_str());
        }
        
        return true;
    } catch (...) {
        return false;
    }
}

bool DeleteTemplate(const std::string& name) {
    if (name.empty() || !TemplateExists(name)) {
        return false;
    }
    
    // 不允许删除当前活动模板
    const char* currentTemplate = cfg_active_template.get();
    if (currentTemplate && strcmp(currentTemplate, name.c_str()) == 0) {
        return false;
    }
    
    std::wstring baseDir = GetWebResourcesBaseDir();
    std::wstring templateDir = baseDir + L"\\" + 
        pfc::stringcvt::string_wide_from_utf8(name.c_str()).get_ptr();
    
    try {
        fs::remove_all(templateDir);
        return true;
    } catch (...) {
        return false;
    }
}

bool TemplateExists(const std::string& name) {
    std::wstring baseDir = GetWebResourcesBaseDir();
    std::wstring templateDir = baseDir + L"\\" + 
        pfc::stringcvt::string_wide_from_utf8(name.c_str()).get_ptr();
    
    return fs::exists(templateDir) && fs::is_directory(templateDir);
}

// ============================================
// 窗口设置
// ============================================

bool GetStartWithFoobar() {
    return cfg_start_with_foobar.get();
}

void SetStartWithFoobar(bool value) {
    cfg_start_with_foobar.set(value);
}

bool GetRememberWindowPosition() {
    return cfg_remember_position.get();
}

void SetRememberWindowPosition(bool value) {
    cfg_remember_position.set(value);
}

bool GetAutoHideWithFoobar() {
    return cfg_auto_hide.get();
}

void SetAutoHideWithFoobar(bool value) {
    cfg_auto_hide.set(value);
}

// ============================================
// DWM 背景效果
// ============================================

BackdropEffect GetBackdropEffect() {
    return static_cast<BackdropEffect>(cfg_backdrop_effect.get());
}

void SetBackdropEffect(BackdropEffect effect) {
    cfg_backdrop_effect.set(static_cast<int>(effect));
}

// ============================================
// 开发者选项访问函数
// ============================================

bool GetDevToolsEnabled() {
    return g_cfg_devtools.get();
}

bool GetLocalNetworkAllowed() {
    return g_cfg_local_network.get();
}

bool GetInsecureHttpAllowed() {
    return g_cfg_allow_insecure.get();
}

bool GetInsecureTlsAllowed() {
    return g_cfg_allow_insecure_tls.get();
}

bool GetBackgroundModeEnabled() {
    return g_cfg_background_mode.get();
}

void SetDevToolsEnabled(bool value) {
    g_cfg_devtools.set(value);
}

void SetLocalNetworkAllowed(bool value) {
    g_cfg_local_network.set(value);
}

void SetInsecureHttpAllowed(bool value) {
    g_cfg_allow_insecure.set(value);
}

void SetInsecureTlsAllowed(bool value) {
    g_cfg_allow_insecure_tls.set(value);
}

void SetBackgroundModeEnabled(bool value) {
    g_cfg_background_mode.set(value);
}

// ============================================
// 开发服务器配置访问函数
// ============================================

bool UseDevServer() {
    return g_cfg_use_dev_server.get();
}

const char* GetDevServerUrl() {
    return g_cfg_dev_server_url.get();
}

void SetDevServerUrl(const char* url) {
    g_cfg_dev_server_url.set(url);
}

void SetUseDevServer(bool value) {
    g_cfg_use_dev_server.set(value);
}

// ============================================
// Dialog Control IDs
// ============================================

enum ControlIds {
    IDC_STATIC_TEMPLATE = 1001,
    IDC_COMBO_TEMPLATE = 1002,
    IDC_BTN_CREATE = 1003,
    IDC_BTN_RENAME = 1004,
    IDC_BTN_DELETE = 1005,
    IDC_BTN_OPEN_FOLDER = 1006,
    
    IDC_STATIC_WINDOW = 1010,
    IDC_CHK_START_WITH_FOOBAR = 1011,
    IDC_CHK_REMEMBER_POSITION = 1012,
    IDC_CHK_AUTO_HIDE = 1013,
    
    IDC_STATIC_BACKDROP = 1020,
    IDC_COMBO_BACKDROP = 1021,
    
    IDC_STATIC_DEV = 1030,
    IDC_CHK_DEVTOOLS = 1031,
    IDC_CHK_LOCAL_NETWORK = 1032,
    IDC_CHK_INSECURE_HTTP = 1033,
    IDC_CHK_BACKGROUND_MODE = 1034,
    IDC_STATIC_DEV_NOTE = 1035,
    IDC_CHK_USE_DEV_SERVER = 1036,
    IDC_EDIT_DEV_SERVER_URL = 1037,
    IDC_STATIC_DEV_SERVER = 1038,
    IDC_CHK_INSECURE_TLS = 1039,
    
    IDC_STATIC_PATH = 1040,
    IDC_EDIT_PATH = 1041,
    
    IDC_BTN_SHOW_API_LIST = 1050,
};

// ============================================
// WebViewPreferencesPage Implementation
// ============================================

const char* WebViewPreferencesPage::get_name() {
    return "WebView2 UI";
}

GUID WebViewPreferencesPage::get_guid() {
    return guid_prefs_page;
}

GUID WebViewPreferencesPage::get_parent_guid() {
    return preferences_page::guid_display;
}

double WebViewPreferencesPage::get_sort_priority() {
    return 0.0;
}

bool WebViewPreferencesPage::get_help_url(pfc::string_base& p_out) {
    p_out = "https://github.com/user/foo_ui_webview2";
    return true;
}

preferences_page_instance::ptr WebViewPreferencesPage::instantiate(
    fb2k::hwnd_t parent, 
    preferences_page_callback::ptr callback) {
    return fb2k::service_new<WebViewPreferencesInstance>(parent, callback);
}

// ============================================
// WebViewPreferencesInstance Implementation
// ============================================

WebViewPreferencesInstance::WebViewPreferencesInstance(
    HWND parent, 
    preferences_page_callback::ptr callback)
    : callback_(std::move(callback)) {
    
    // 初始化待处理值
    const char* templateStr = cfg_active_template.get();
    pendingTemplate_ = templateStr ? templateStr : "default";
    pendingStartWithFoobar_ = cfg_start_with_foobar.get();
    pendingRememberPosition_ = cfg_remember_position.get();
    pendingAutoHide_ = cfg_auto_hide.get();
    pendingBackdrop_ = static_cast<BackdropEffect>(cfg_backdrop_effect.get());
    
    // 获取父窗口尺寸
    RECT rcParent;
    GetClientRect(parent, &rcParent);
    int width = rcParent.right - rcParent.left;
    int height = rcParent.bottom - rcParent.top;
    if (width < 550) width = 550;
    if (height < 600) height = 600;  // 增加最小高度以容纳所有控件
    
    // 创建对话框窗口 (无模板，手动创建控件)
    hwnd_ = CreateWindowExW(
        0,
        L"STATIC",
        L"",
        WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
        0, 0, width, height,
        parent,
        nullptr,
        core_api::get_my_instance(),
        nullptr);
    
    if (hwnd_) {
        SetWindowLongPtrW(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        
        // 子类化窗口以处理消息
        SetWindowLongPtrW(hwnd_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(DialogProc));
        
        InitializeControls(hwnd_);
    }
}

WebViewPreferencesInstance::~WebViewPreferencesInstance() {
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    if (hFont_) {
        DeleteObject(hFont_);
        hFont_ = nullptr;
    }
}

t_uint32 WebViewPreferencesInstance::get_state() {
    t_uint32 state = preferences_state::dark_mode_supported | preferences_state::resettable;
    if (hasChanges_) {
        state |= preferences_state::changed;
    }
    return state;
}

fb2k::hwnd_t WebViewPreferencesInstance::get_wnd() {
    return hwnd_;
}

void WebViewPreferencesInstance::apply() {
    // 应用所有设置并立即重新加载 WebView
    hasChanges_ = false;
    if (callback_.is_valid()) {
        callback_->on_state_changed();
    }
    
    // 立即重新加载 WebView 以应用开发服务器配置
    auto* ui = WebViewUI::GetInstance();
    if (ui && ui->GetMainWindow()) {
        auto* mainWnd = ui->GetMainWindow();
        if (mainWnd && mainWnd->GetWebView()) {
            console::printf("[WebView2 UI] Applying settings and reloading WebView...");
            
            // 发送自定义命令 9999 触发重新加载（与模板更改相同）
            PostMessage(mainWnd->GetHwnd(), WM_COMMAND, MAKEWPARAM(9999, 0), 0);
        }
    }
}

void WebViewPreferencesInstance::reset() {
    // 重置为默认值并立即保存
    pendingTemplate_ = "default";
    pendingStartWithFoobar_ = true;
    pendingRememberPosition_ = true;
    pendingAutoHide_ = false;
    pendingBackdrop_ = BackdropEffect::Mica;
    
    // 立即保存默认值到配置
    SetActiveTemplateName(pendingTemplate_);
    SetStartWithFoobar(pendingStartWithFoobar_);
    SetRememberWindowPosition(pendingRememberPosition_);
    SetAutoHideWithFoobar(pendingAutoHide_);
    SetBackdropEffect(pendingBackdrop_);
    
    // 刷新 UI
    if (!hwnd_) return;

    HWND hComboTemplate = GetDlgItem(hwnd_, IDC_COMBO_TEMPLATE);
    if (hComboTemplate) {
        int count = (int)SendMessageW(hComboTemplate, CB_GETCOUNT, 0, 0);
        for (int i = 0; i < count; i++) {
            wchar_t buf[256];
            SendMessageW(hComboTemplate, CB_GETLBTEXT, i, (LPARAM)buf);
            if (wcscmp(buf, L"default") == 0) {
                SendMessageW(hComboTemplate, CB_SETCURSEL, i, 0);
                break;
            }
        }
    }
    
    CheckDlgButton(hwnd_, IDC_CHK_START_WITH_FOOBAR, BST_CHECKED);
    CheckDlgButton(hwnd_, IDC_CHK_REMEMBER_POSITION, BST_CHECKED);
    CheckDlgButton(hwnd_, IDC_CHK_AUTO_HIDE, BST_UNCHECKED);
    
    HWND hComboBackdrop = GetDlgItem(hwnd_, IDC_COMBO_BACKDROP);
    if (hComboBackdrop) {
        SendMessageW(hComboBackdrop, CB_SETCURSEL, 1, 0); // Mica
    }
}

INT_PTR CALLBACK WebViewPreferencesInstance::DialogProc(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    WebViewPreferencesInstance* self = reinterpret_cast<WebViewPreferencesInstance*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    
    if (self) {
        return self->HandleMessage(hwnd, msg, wParam, lParam);
    }
    
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

INT_PTR WebViewPreferencesInstance::HandleMessage(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    
    switch (msg) {
    case WM_ERASEBKGND:
        // 自绘背景 - 使用深色或浅色
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
        
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_COMBO_TEMPLATE:
            if (HIWORD(wParam) == CBN_SELCHANGE) {
                OnTemplateSelectionChanged(hwnd);
            }
            break;
            
        case IDC_BTN_CREATE:
            OnCreateTemplate(hwnd);
            break;
            
        case IDC_BTN_RENAME:
            OnRenameTemplate(hwnd);
            break;
            
        case IDC_BTN_DELETE:
            OnDeleteTemplate(hwnd);
            break;
            
        case IDC_BTN_OPEN_FOLDER:
            OnOpenTemplateFolder(hwnd);
            break;
            
        case IDC_CHK_START_WITH_FOOBAR:
            // 立即保存到配置
            pendingStartWithFoobar_ = (IsDlgButtonChecked(hwnd, IDC_CHK_START_WITH_FOOBAR) == BST_CHECKED);
            SetStartWithFoobar(pendingStartWithFoobar_);
            hasChanges_ = true;
            if (callback_.is_valid()) callback_->on_state_changed();
            break;
            
        case IDC_CHK_REMEMBER_POSITION:
            // 立即保存到配置
            pendingRememberPosition_ = (IsDlgButtonChecked(hwnd, IDC_CHK_REMEMBER_POSITION) == BST_CHECKED);
            SetRememberWindowPosition(pendingRememberPosition_);
            hasChanges_ = true;
            if (callback_.is_valid()) callback_->on_state_changed();
            break;
            
        case IDC_CHK_AUTO_HIDE:
            // 立即保存到配置
            pendingAutoHide_ = (IsDlgButtonChecked(hwnd, IDC_CHK_AUTO_HIDE) == BST_CHECKED);
            SetAutoHideWithFoobar(pendingAutoHide_);
            hasChanges_ = true;
            if (callback_.is_valid()) callback_->on_state_changed();
            break;
            
        case IDC_COMBO_BACKDROP:
            if (HIWORD(wParam) != CBN_SELCHANGE) break;
            {
                HWND hCombo = GetDlgItem(hwnd, IDC_COMBO_BACKDROP);
                int sel = (int)SendMessageW(hCombo, CB_GETCURSEL, 0, 0);
                pendingBackdrop_ = static_cast<BackdropEffect>(sel);
                // 立即保存到配置
                SetBackdropEffect(pendingBackdrop_);
                hasChanges_ = true;
                if (callback_.is_valid()) callback_->on_state_changed();
                
                // 热重载 DWM 背景效果
                auto* ui = WebViewUI::GetInstance();
                if (ui && ui->GetMainWindow()) {
                    ui->GetMainWindow()->RefreshBackdropEffect();
                }
            }
            break;
            
        // Developer Options - 直接保存到 advconfig
        case IDC_CHK_DEVTOOLS:
            SetDevToolsEnabled(IsDlgButtonChecked(hwnd, IDC_CHK_DEVTOOLS) == BST_CHECKED);
            hasChanges_ = true;
            if (callback_.is_valid()) callback_->on_state_changed();
            break;
            
        case IDC_CHK_LOCAL_NETWORK:
            SetLocalNetworkAllowed(IsDlgButtonChecked(hwnd, IDC_CHK_LOCAL_NETWORK) == BST_CHECKED);
            hasChanges_ = true;
            if (callback_.is_valid()) callback_->on_state_changed();
            break;
            
        case IDC_CHK_INSECURE_HTTP:
            SetInsecureHttpAllowed(IsDlgButtonChecked(hwnd, IDC_CHK_INSECURE_HTTP) == BST_CHECKED);
            hasChanges_ = true;
            if (callback_.is_valid()) callback_->on_state_changed();
            break;

        case IDC_CHK_INSECURE_TLS:
            SetInsecureTlsAllowed(IsDlgButtonChecked(hwnd, IDC_CHK_INSECURE_TLS) == BST_CHECKED);
            hasChanges_ = true;
            if (callback_.is_valid()) callback_->on_state_changed();
            break;
            
        case IDC_CHK_BACKGROUND_MODE:
            SetBackgroundModeEnabled(IsDlgButtonChecked(hwnd, IDC_CHK_BACKGROUND_MODE) == BST_CHECKED);
            hasChanges_ = true;
            if (callback_.is_valid()) callback_->on_state_changed();
            break;
            
        case IDC_CHK_USE_DEV_SERVER:
            SetUseDevServer(IsDlgButtonChecked(hwnd, IDC_CHK_USE_DEV_SERVER) == BST_CHECKED);
            hasChanges_ = true;
            if (callback_.is_valid()) callback_->on_state_changed();
            break;
            
        case IDC_EDIT_DEV_SERVER_URL:
            // 当编辑框失去焦点时保存 URL
            if (HIWORD(wParam) != EN_KILLFOCUS) break;
            {
                HWND hEdit = GetDlgItem(hwnd, IDC_EDIT_DEV_SERVER_URL);
                wchar_t buf[512];
                GetWindowTextW(hEdit, buf, 512);
                std::string url = pfc::stringcvt::string_utf8_from_wide(buf).get_ptr();
                SetDevServerUrl(url.c_str());
                hasChanges_ = true;
                if (callback_.is_valid()) callback_->on_state_changed();
            }
            break;
            
        case IDC_BTN_SHOW_API_LIST:
            OnShowApiList(hwnd);
            break;
        }
        break;
        
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
    case WM_CTLCOLOREDIT:
        // 深色模式背景处理
        {
            HDC hdc = (HDC)wParam;
            auto api = ui_config_manager::tryGet();
            if (api.is_valid() && api->is_dark_mode()) {
                SetTextColor(hdc, RGB(222, 222, 222));  // 浅色文字
                SetBkColor(hdc, RGB(32, 32, 32));       // 深色背景
                static HBRUSH hDarkBrush = CreateSolidBrush(RGB(32, 32, 32));
                return (INT_PTR)hDarkBrush;
            }
        }
        break;
    }
    
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ============================================
// DPI 缩放辅助函数
// ============================================

static int GetDpiForWindowSafe(HWND hwnd) {
    // 尝试使用 Windows 10 1607+ API
    using GetDpiForWindowFunc = UINT (WINAPI *)(HWND);
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
    
    // 回退到 DC 方式
    HDC hdc = GetDC(hwnd);
    int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(hwnd, hdc);
    return dpi > 0 ? dpi : 96;
}

static int DpiScale(int value, int dpi) {
    return MulDiv(value, dpi, 96);
}

void WebViewPreferencesInstance::InitializeControls(HWND hwnd) {
    HINSTANCE hInst = core_api::get_my_instance();
    
    // 获取 DPI 缩放比例
    int dpi = GetDpiForWindowSafe(hwnd);
    
    // 创建 DPI 感知字体
    NONCLIENTMETRICSW ncm = {};
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    ncm.lfMessageFont.lfHeight = DpiScale(-12, dpi);  // 12pt 字体
    hFont_ = CreateFontIndirectW(&ncm.lfMessageFont);
    HFONT hFont = hFont_;
    
    // 获取父窗口尺寸
    RECT rcParent;
    GetClientRect(hwnd, &rcParent);
    int parentWidth = rcParent.right - rcParent.left;
    
    // DPI 缩放常量 - 压缩布局
    int y = DpiScale(8, dpi);
    const int MARGIN = DpiScale(14, dpi);
    const int LABEL_HEIGHT = DpiScale(18, dpi);
    const int CONTROL_HEIGHT = DpiScale(22, dpi);  // 减小控件高度
    const int BUTTON_WIDTH = DpiScale(75, dpi);
    const int CHECKBOX_WIDTH = DpiScale(420, dpi);  // 增加宽度以显示完整文本
    const int SPACING = DpiScale(4, dpi);  // 减小间距
    const int SECTION_SPACING = DpiScale(6, dpi);  // 区块间距
    // 内容宽度基于父窗口宽度计算，最小 500
    const int CONTENT_WIDTH = (std::max)(DpiScale(500, dpi), parentWidth - MARGIN * 2 - DpiScale(20, dpi));
    
    // ============================================
    // Web 模板区域
    // ============================================
    
    // "Web" 分组标签
    HWND hLabel = CreateWindowExW(0, L"STATIC", L"Web",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        MARGIN, y, DpiScale(100, dpi), LABEL_HEIGHT,
        hwnd, (HMENU)IDC_STATIC_TEMPLATE, hInst, nullptr);
    SendMessageW(hLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
    y += LABEL_HEIGHT + DpiScale(2, dpi);
    
    // 模板下拉框
    HWND hComboTemplate = CreateWindowExW(0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP,
        MARGIN, y, DpiScale(160, dpi), DpiScale(200, dpi),
        hwnd, (HMENU)IDC_COMBO_TEMPLATE, hInst, nullptr);
    SendMessageW(hComboTemplate, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    // 按钮行
    int btnX = MARGIN + DpiScale(170, dpi);
    
    HWND hBtnCreate = CreateWindowExW(0, L"BUTTON", TR("Create", "创建"),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        btnX, y, BUTTON_WIDTH, CONTROL_HEIGHT,
        hwnd, (HMENU)IDC_BTN_CREATE, hInst, nullptr);
    SendMessageW(hBtnCreate, WM_SETFONT, (WPARAM)hFont, TRUE);
    btnX += BUTTON_WIDTH + DpiScale(6, dpi);
    
    HWND hBtnRename = CreateWindowExW(0, L"BUTTON", TR("Rename", "重命名"),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        btnX, y, BUTTON_WIDTH, CONTROL_HEIGHT,
        hwnd, (HMENU)IDC_BTN_RENAME, hInst, nullptr);
    SendMessageW(hBtnRename, WM_SETFONT, (WPARAM)hFont, TRUE);
    btnX += BUTTON_WIDTH + DpiScale(6, dpi);
    
    HWND hBtnDelete = CreateWindowExW(0, L"BUTTON", TR("Delete", "删除"),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        btnX, y, BUTTON_WIDTH, CONTROL_HEIGHT,
        hwnd, (HMENU)IDC_BTN_DELETE, hInst, nullptr);
    SendMessageW(hBtnDelete, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    y += CONTROL_HEIGHT + DpiScale(2, dpi);
    
    // Open Folder 按钮
    HWND hBtnOpen = CreateWindowExW(0, L"BUTTON", TR("Open Folder", "打开文件夹"),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        MARGIN, y, DpiScale(110, dpi), CONTROL_HEIGHT,
        hwnd, (HMENU)IDC_BTN_OPEN_FOLDER, hInst, nullptr);
    SendMessageW(hBtnOpen, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    y += CONTROL_HEIGHT + SECTION_SPACING;
    
    // ============================================
    // 窗口设置区域
    // ============================================
    
    hLabel = CreateWindowExW(0, L"STATIC", TR("Window Settings:", "窗口设置:"),
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        MARGIN, y, DpiScale(200, dpi), LABEL_HEIGHT,
        hwnd, (HMENU)IDC_STATIC_WINDOW, hInst, nullptr);
    SendMessageW(hLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
    y += LABEL_HEIGHT + DpiScale(2, dpi);
    
    // 复选框: Background mode - 用于后台运行（使用其他UI时保持WebView活跃）
    HWND hChk = CreateWindowExW(0, L"BUTTON", TR("Keep WebView active when using other UIs (background mode)", "使用其他 UI 时保持 WebView 活跃 (后台模式)"),
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
        MARGIN + DpiScale(10, dpi), y, CHECKBOX_WIDTH, CONTROL_HEIGHT,
        hwnd, (HMENU)IDC_CHK_BACKGROUND_MODE, hInst, nullptr);
    SendMessageW(hChk, WM_SETFONT, (WPARAM)hFont, TRUE);
    if (GetBackgroundModeEnabled()) {
        CheckDlgButton(hwnd, IDC_CHK_BACKGROUND_MODE, BST_CHECKED);
    }
    y += CONTROL_HEIGHT + SECTION_SPACING;
    
    // ============================================
    // 背景效果区域
    // ============================================
    
    hLabel = CreateWindowExW(0, L"STATIC", TR("Window Backdrop:", "窗口背景效果:"),
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        MARGIN, y, DpiScale(200, dpi), LABEL_HEIGHT,
        hwnd, (HMENU)IDC_STATIC_BACKDROP, hInst, nullptr);
    SendMessageW(hLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
    y += LABEL_HEIGHT + DpiScale(2, dpi);
    
    HWND hComboBackdrop = CreateWindowExW(0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP,
        MARGIN + DpiScale(10, dpi), y, DpiScale(180, dpi), DpiScale(200, dpi),
        hwnd, (HMENU)IDC_COMBO_BACKDROP, hInst, nullptr);
    SendMessageW(hComboBackdrop, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    // 添加背景效果选项
    SendMessageW(hComboBackdrop, CB_ADDSTRING, 0, (LPARAM)TR("None", "无"));
    SendMessageW(hComboBackdrop, CB_ADDSTRING, 0, (LPARAM)L"Mica");
    SendMessageW(hComboBackdrop, CB_ADDSTRING, 0, (LPARAM)L"Mica Alt");
    SendMessageW(hComboBackdrop, CB_ADDSTRING, 0, (LPARAM)L"Acrylic");
    SendMessageW(hComboBackdrop, CB_ADDSTRING, 0, (LPARAM)L"Tabbed");
    SendMessageW(hComboBackdrop, CB_SETCURSEL, static_cast<int>(pendingBackdrop_), 0);
    
    y += CONTROL_HEIGHT + SECTION_SPACING;
    
    // ============================================
    // 开发者选项区域 (可编辑复选框)
    // ============================================
    
    // 加宽以显示完整文本，基于父窗口宽度动态计算
    const int DEV_CHECKBOX_WIDTH = (std::max)(DpiScale(480, dpi), parentWidth - MARGIN * 2 - DpiScale(30, dpi));
    
    hLabel = CreateWindowExW(0, L"STATIC", TR("Developer Options (requires restart):", "开发者选项 (需要重启):"),
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        MARGIN, y, DpiScale(350, dpi), LABEL_HEIGHT,
        hwnd, (HMENU)IDC_STATIC_DEV, hInst, nullptr);
    SendMessageW(hLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
    y += LABEL_HEIGHT + DpiScale(2, dpi);
    
    // DevTools 复选框
    hChk = CreateWindowExW(0, L"BUTTON", TR("Enable Developer Tools (F12)", "启用开发者工具 (F12)"),
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
        MARGIN + DpiScale(10, dpi), y, DEV_CHECKBOX_WIDTH, CONTROL_HEIGHT,
        hwnd, (HMENU)IDC_CHK_DEVTOOLS, hInst, nullptr);
    SendMessageW(hChk, WM_SETFONT, (WPARAM)hFont, TRUE);
    CheckDlgButton(hwnd, IDC_CHK_DEVTOOLS, GetDevToolsEnabled() ? BST_CHECKED : BST_UNCHECKED);
    y += CONTROL_HEIGHT + DpiScale(1, dpi);
    
    // Local Network 复选框
    hChk = CreateWindowExW(0, L"BUTTON", TR("Allow HTTP access to local network (127.0.0.1, 192.168.x.x)", "允许 HTTP 访问本地网络 (127.0.0.1, 192.168.x.x)"),
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
        MARGIN + DpiScale(10, dpi), y, DEV_CHECKBOX_WIDTH, CONTROL_HEIGHT,
        hwnd, (HMENU)IDC_CHK_LOCAL_NETWORK, hInst, nullptr);
    SendMessageW(hChk, WM_SETFONT, (WPARAM)hFont, TRUE);
    if (GetLocalNetworkAllowed()) {
        CheckDlgButton(hwnd, IDC_CHK_LOCAL_NETWORK, BST_CHECKED);
    }
    y += CONTROL_HEIGHT + DpiScale(1, dpi);
    
    // Insecure HTTP 复选框
    hChk = CreateWindowExW(0, L"BUTTON", TR("Allow insecure HTTP connections (disable HSTS)", "允许不安全的 HTTP 连接 (禁用 HSTS)"),
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
        MARGIN + DpiScale(10, dpi), y, DEV_CHECKBOX_WIDTH, CONTROL_HEIGHT,
        hwnd, (HMENU)IDC_CHK_INSECURE_HTTP, hInst, nullptr);
    SendMessageW(hChk, WM_SETFONT, (WPARAM)hFont, TRUE);
    if (GetInsecureHttpAllowed()) {
        CheckDlgButton(hwnd, IDC_CHK_INSECURE_HTTP, BST_CHECKED);
    }
    y += CONTROL_HEIGHT + DpiScale(1, dpi);

    // Insecure TLS 复选框 (允许自签 / 无效证书,fb.http.* 用,每请求还需 opt-in)
    hChk = CreateWindowExW(0, L"BUTTON", TR("Allow self-signed / invalid TLS certs (per-request opt-in)", "允许自签 / 无效 TLS 证书 (每请求需 opt-in)"),
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
        MARGIN + DpiScale(10, dpi), y, DEV_CHECKBOX_WIDTH, CONTROL_HEIGHT,
        hwnd, (HMENU)IDC_CHK_INSECURE_TLS, hInst, nullptr);
    SendMessageW(hChk, WM_SETFONT, (WPARAM)hFont, TRUE);
    if (GetInsecureTlsAllowed()) {
        CheckDlgButton(hwnd, IDC_CHK_INSECURE_TLS, BST_CHECKED);
    }
    y += CONTROL_HEIGHT + SECTION_SPACING;
    
    // ============================================
    // 开发服务器配置 (HMR 热重载)
    // ============================================
    
    hLabel = CreateWindowExW(0, L"STATIC", TR("Development Server (HMR Hot Reload):", "开发服务器 (HMR 热重载):"),
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        MARGIN, y, DpiScale(350, dpi), LABEL_HEIGHT,
        hwnd, (HMENU)IDC_STATIC_DEV_SERVER, hInst, nullptr);
    SendMessageW(hLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
    y += LABEL_HEIGHT + DpiScale(2, dpi);
    
    // 使用开发服务器复选框
    hChk = CreateWindowExW(0, L"BUTTON", TR("Use development server (e.g., Vite dev server) - requires restart", "使用开发服务器 (如 Vite) - 需要重启"),
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
        MARGIN + DpiScale(10, dpi), y, DEV_CHECKBOX_WIDTH, CONTROL_HEIGHT,
        hwnd, (HMENU)IDC_CHK_USE_DEV_SERVER, hInst, nullptr);
    SendMessageW(hChk, WM_SETFONT, (WPARAM)hFont, TRUE);
    if (security_config::UseDevServer()) {
        CheckDlgButton(hwnd, IDC_CHK_USE_DEV_SERVER, BST_CHECKED);
    }
    y += CONTROL_HEIGHT + DpiScale(1, dpi);
    
    // 开发服务器 URL 输入框
    HWND hEditDevUrl = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_TABSTOP,
        MARGIN + DpiScale(10, dpi), y, CONTENT_WIDTH, CONTROL_HEIGHT,
        hwnd, (HMENU)IDC_EDIT_DEV_SERVER_URL, hInst, nullptr);
    SendMessageW(hEditDevUrl, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    // 设置默认值
    const char* devUrl = security_config::GetDevServerUrl();
    std::wstring wDevUrl = pfc::stringcvt::string_wide_from_utf8(devUrl).get_ptr();
    SetWindowTextW(hEditDevUrl, wDevUrl.c_str());
    
    y += CONTROL_HEIGHT + SECTION_SPACING;
    
    // ============================================
    // 资源路径显示
    // ============================================
    
    hLabel = CreateWindowExW(0, L"STATIC", TR("Resources Path:", "资源路径:"),
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        MARGIN, y, DpiScale(150, dpi), LABEL_HEIGHT,
        hwnd, (HMENU)IDC_STATIC_PATH, hInst, nullptr);
    SendMessageW(hLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
    y += LABEL_HEIGHT + DpiScale(4, dpi);
    
    std::wstring pathInfo = GetWebResourcesBaseDir();
    HWND hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", pathInfo.c_str(),
        WS_CHILD | WS_VISIBLE | ES_READONLY | ES_AUTOHSCROLL,
        MARGIN, y, CONTENT_WIDTH, CONTROL_HEIGHT,
        hwnd, (HMENU)IDC_EDIT_PATH, hInst, nullptr);
    SendMessageW(hEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    y += CONTROL_HEIGHT + SECTION_SPACING;
    
    // ============================================
    // API 列表按钮 - 加宽以显示完整文字
    // ============================================
    
    HWND hBtnApiList = CreateWindowExW(0, L"BUTTON", TR("Show API && Services", "查看 API && 服务"),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        MARGIN, y, DpiScale(170, dpi), CONTROL_HEIGHT,
        hwnd, (HMENU)IDC_BTN_SHOW_API_LIST, hInst, nullptr);
    SendMessageW(hBtnApiList, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    // ============================================
    // 填充模板列表
    // ============================================
    RefreshTemplateList(hwnd);
    
    // ============================================
    // 应用深色模式主题
    // ============================================
    darkMode_.AddDialogWithControls(hwnd);
}

void WebViewPreferencesInstance::RefreshTemplateList(HWND hwnd) {
    HWND hCombo = GetDlgItem(hwnd, IDC_COMBO_TEMPLATE);
    if (!hCombo) return;
    
    SendMessageW(hCombo, CB_RESETCONTENT, 0, 0);
    
    std::vector<std::string> templates = GetTemplateList();
    int activeIndex = 0;
    
    for (size_t i = 0; i < templates.size(); i++) {
        std::wstring wname = pfc::stringcvt::string_wide_from_utf8(templates[i].c_str()).get_ptr();
        SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)wname.c_str());
        
        if (templates[i] == pendingTemplate_) {
            activeIndex = (int)i;
        }
    }
    
    SendMessageW(hCombo, CB_SETCURSEL, activeIndex, 0);
}

void WebViewPreferencesInstance::UpdateState() {
    if (callback_.is_valid()) {
        callback_->on_state_changed();
    }
}

void WebViewPreferencesInstance::OnTemplateSelectionChanged(HWND hwnd) {
    HWND hCombo = GetDlgItem(hwnd, IDC_COMBO_TEMPLATE);
    int sel = (int)SendMessageW(hCombo, CB_GETCURSEL, 0, 0);
    if (sel >= 0) {
        wchar_t buf[256];
        SendMessageW(hCombo, CB_GETLBTEXT, sel, (LPARAM)buf);
        std::string newTemplate = pfc::stringcvt::string_utf8_from_wide(buf).get_ptr();
        
        // 检查模板是否确实改变
        if (newTemplate != pendingTemplate_) {
            pendingTemplate_ = newTemplate;
            // 立即保存到配置
            SetActiveTemplateName(pendingTemplate_);
            
            // 通知主窗口重新加载前端以立即生效
            auto* ui = WebViewUI::GetInstance();
            if (ui && ui->GetMainWindow()) {
                console::printf("[WebView2 UI] Template changed to: %s, reloading frontend...", pendingTemplate_.c_str());
                PostMessage(ui->GetMainWindow()->GetHwnd(), WM_COMMAND, MAKEWPARAM(9999, 0), 0);  // 自定义命令触发重载
            }
        }
    }
}

void WebViewPreferencesInstance::OnCreateTemplate(HWND hwnd) {
    // 简单的输入对话框
    wchar_t name[256] = L"";
    
    // 使用 InputBox 风格的对话框
    // 这里简化处理，使用固定名称模式
    std::wstring newName = L"template_";
    auto templates = GetTemplateList();
    newName += std::to_wstring(templates.size() + 1);
    
    std::string utf8Name = pfc::stringcvt::string_utf8_from_wide(newName.c_str()).get_ptr();
    
    if (CreateTemplate(utf8Name)) {
        RefreshTemplateList(hwnd);
        
        // 选中新创建的模板
        HWND hCombo = GetDlgItem(hwnd, IDC_COMBO_TEMPLATE);
        int count = (int)SendMessageW(hCombo, CB_GETCOUNT, 0, 0);
        for (int i = 0; i < count; i++) {
            wchar_t buf[256];
            SendMessageW(hCombo, CB_GETLBTEXT, i, (LPARAM)buf);
            if (wcscmp(buf, newName.c_str()) == 0) {
                SendMessageW(hCombo, CB_SETCURSEL, i, 0);
                pendingTemplate_ = utf8Name;
                // 立即保存到配置
                SetActiveTemplateName(pendingTemplate_);
                break;
            }
        }
        
        console::printf("[WebView2 UI] Created template: %s", utf8Name.c_str());
    } else {
        MessageBoxW(hwnd, TR("Failed to create template.", "创建模板失败。"), L"WebView2 UI", MB_OK | MB_ICONERROR);
    }
}

void WebViewPreferencesInstance::OnRenameTemplate(HWND hwnd) {
    HWND hCombo = GetDlgItem(hwnd, IDC_COMBO_TEMPLATE);
    int sel = (int)SendMessageW(hCombo, CB_GETCURSEL, 0, 0);
    if (sel < 0) return;
    
    wchar_t oldNameW[256];
    SendMessageW(hCombo, CB_GETLBTEXT, sel, (LPARAM)oldNameW);
    std::string oldName = pfc::stringcvt::string_utf8_from_wide(oldNameW).get_ptr();
    
    // 使用简单的输入循环实现重命名（替代半成品动态对话框）
    // 避免复杂的 DLGTEMPLATE 构建，使用 GetSaveFileName 技巧或 prompt 组合
    // 这里采用最简方案：循环 prompt 直到用户给出合法名称或取消
    
    std::wstring promptMsg = TR(
        "Enter new name for template \"", "请输入模板 \"");
    promptMsg += oldNameW;
    promptMsg += TR(
        "\":\n\n"
        "(Only letters, numbers, hyphens and underscores allowed)\n\n"
        "Click Cancel to abort.",
        "\" 的新名称:\n\n"
        "(只允许字母、数字、连字符和下划线)\n\n"
        "点击取消以中止。");
    
    // 使用 TaskDialog 带输入框（需要 CommCtrl v6），回退到简化方案
    // 简化方案：使用 FindText 技巧创建一个带编辑框的对话框
    // 最终方案：组合 MessageBox 提示 + 剪贴板约定，过于 hacky
    // 实际方案：直接构建最小 DLGTEMPLATE（3 个控件）

    // 构建内存中的对话框模板
    // 布局: [Label] [Edit] [OK] [Cancel]
    struct alignas(DWORD) {
        DLGTEMPLATE tmpl;
        WORD menuArray[1];      // 无菜单
        WORD classArray[1];     // 默认类
        WORD titleArray[1];     // 空标题
        // Item 1: Static label
        struct alignas(DWORD) {
            DLGITEMTEMPLATE item;
            WORD classArray[2]; // 0xFFFF, 0x0082 = STATIC
            WORD titleArray[2]; // ":" + nul (placeholder)
            WORD extraBytes;
        } label;
        // Item 2: Edit box
        struct alignas(DWORD) {
            DLGITEMTEMPLATE item;
            WORD classArray[2]; // 0xFFFF, 0x0081 = EDIT
            WORD titleArray[1]; // empty
            WORD extraBytes;
        } edit;
        // Item 3: OK button
        struct alignas(DWORD) {
            DLGITEMTEMPLATE item;
            WORD classArray[2]; // 0xFFFF, 0x0080 = BUTTON
            WORD titleArray[3]; // "OK" + nul
            WORD extraBytes;
        } ok;
        // Item 4: Cancel button
        struct alignas(DWORD) {
            DLGITEMTEMPLATE item;
            WORD classArray[2];
            WORD titleArray[7]; // "Cancel" + nul
            WORD extraBytes;
        } cancel;
    } dlg = {};
    
    dlg.tmpl.style = DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU;
    dlg.tmpl.cdit = 4;
    dlg.tmpl.cx = 220; dlg.tmpl.cy = 70;
    
    // Label
    dlg.label.item = {WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 7, 7, 200, 12, 1000};
    dlg.label.classArray[0] = 0xFFFF; dlg.label.classArray[1] = 0x0082;
    dlg.label.titleArray[0] = L':'; dlg.label.titleArray[1] = 0;
    
    // Edit
    dlg.edit.item = {WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, 0, 7, 22, 206, 14, 1001};
    dlg.edit.classArray[0] = 0xFFFF; dlg.edit.classArray[1] = 0x0081;
    dlg.edit.titleArray[0] = 0;
    
    // OK
    dlg.ok.item = {WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP, 0, 110, 44, 50, 14, IDOK};
    dlg.ok.classArray[0] = 0xFFFF; dlg.ok.classArray[1] = 0x0080;
    dlg.ok.titleArray[0] = L'O'; dlg.ok.titleArray[1] = L'K'; dlg.ok.titleArray[2] = 0;
    
    // Cancel
    dlg.cancel.item = {WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP, 0, 163, 44, 50, 14, IDCANCEL};
    dlg.cancel.classArray[0] = 0xFFFF; dlg.cancel.classArray[1] = 0x0080;
    dlg.cancel.titleArray[0] = L'C'; dlg.cancel.titleArray[1] = L'a'; dlg.cancel.titleArray[2] = L'n';
    dlg.cancel.titleArray[3] = L'c'; dlg.cancel.titleArray[4] = L'e'; dlg.cancel.titleArray[5] = L'l';
    dlg.cancel.titleArray[6] = 0;
    
    // 共享状态
    static std::wstring s_renameOldName;
    static std::wstring s_renameNewName;
    s_renameOldName = oldNameW;
    s_renameNewName = oldNameW;
    
    auto DlgProc = [](HWND hDlg, UINT msg, WPARAM wParam, LPARAM) -> INT_PTR {
        switch (msg) {
        case WM_INITDIALOG: {
            SetWindowTextW(hDlg, TR("Rename Template", "\u91CD\u547D\u540D\u6A21\u677F"));
            HWND hLabel = GetDlgItem(hDlg, 1000);
            if (hLabel) {
                std::wstring labelText = TR("New name for \"", "\u65B0\u540D\u79F0 (\"");
                labelText += s_renameOldName + L"\"):";
                SetWindowTextW(hLabel, labelText.c_str());
            }
            HWND hEdit = GetDlgItem(hDlg, 1001);
            if (hEdit) {
                SetWindowTextW(hEdit, s_renameNewName.c_str());
                SendMessageW(hEdit, EM_SETSEL, 0, -1);
                SetFocus(hEdit);
            }
            return FALSE;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                wchar_t buf[256] = {};
                GetDlgItemTextW(hDlg, 1001, buf, 256);
                s_renameNewName = buf;
                EndDialog(hDlg, IDOK);
                return TRUE;
            } else if (LOWORD(wParam) == IDCANCEL) {
                EndDialog(hDlg, IDCANCEL);
                return TRUE;
            }
            break;
        case WM_CLOSE:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        return FALSE;
    };
    
    INT_PTR result = DialogBoxIndirectParamW(
        nullptr, &dlg.tmpl, hwnd,
        static_cast<DLGPROC>(DlgProc), 0);
    
    if (result != IDOK) return;
    
    std::string newName = pfc::stringcvt::string_utf8_from_wide(s_renameNewName.c_str()).get_ptr();
    
    // 验证
    if (newName.empty() || newName == oldName) return;
    
    if (!RenameTemplate(oldName, newName)) {
        MessageBoxW(hwnd,
            TR("Failed to rename template. Make sure:\n"
               "- The name only contains letters, numbers, hyphens and underscores\n"
               "- A template with the new name doesn't already exist",
               "\u91CD\u547D\u540D\u5931\u8D25\u3002\u8BF7\u786E\u4FDD:\n"
               "- \u540D\u79F0\u53EA\u5305\u542B\u5B57\u6BCD\u3001\u6570\u5B57\u3001\u8FDE\u5B57\u7B26\u548C\u4E0B\u5212\u7EBF\n"
               "- \u4E0D\u5B58\u5728\u540C\u540D\u6A21\u677F"),
            TR("Rename Failed", "\u91CD\u547D\u540D\u5931\u8D25"), MB_OK | MB_ICONERROR);
        return;
    }
    
    // 刷新 ComboBox
    RefreshTemplateList(hwnd);
}

void WebViewPreferencesInstance::OnDeleteTemplate(HWND hwnd) {
    HWND hCombo = GetDlgItem(hwnd, IDC_COMBO_TEMPLATE);
    int sel = (int)SendMessageW(hCombo, CB_GETCURSEL, 0, 0);
    if (sel < 0) return;
    
    wchar_t buf[256];
    SendMessageW(hCombo, CB_GETLBTEXT, sel, (LPARAM)buf);
    std::string templateName = pfc::stringcvt::string_utf8_from_wide(buf).get_ptr();
    
    // 获取模板列表
    auto templates = GetTemplateList();
    
    // 检查是否只有一个模板
    if (templates.size() <= 1) {
        MessageBoxW(hwnd, TR("Cannot delete the only template.\n\nAt least one template must exist.", "无法删除唯一的模板。\n\n至少需要保留一个模板。"), L"WebView2 UI", MB_OK | MB_ICONERROR);
        return;
    }
    
    // 检查是否是当前活动模板
    const char* currentTemplate = cfg_active_template.get();
    bool isActive = (currentTemplate && strcmp(currentTemplate, templateName.c_str()) == 0);
    
    // 构建确认消息
    std::wstring msg = TR("Are you sure you want to delete template \"", "确定要删除模板 \"");
    msg += buf;
    msg += L"\"?";
    
    if (isActive) {
        msg += TR("\n\nThis is the currently active template. "
                  "After deletion, another template will be activated automatically.",
                  "\n\n这是当前活动模板。删除后将自动激活另一个模板。");
    }
    
    msg += TR("\n\nThis action cannot be undone.", "\n\n此操作无法撤销。");
    
    if (MessageBoxW(hwnd, msg.c_str(), L"WebView2 UI", MB_YESNO | MB_ICONWARNING) != IDYES)
        return;

    // 如果是活动模板，先切换到其他模板
    if (isActive) {
        std::string newActiveTemplate;
        for (const auto& t : templates) {
            if (t != templateName) {
                newActiveTemplate = t;
                break;
            }
        }
        
        if (!newActiveTemplate.empty()) {
            SetActiveTemplateName(newActiveTemplate);
            pendingTemplate_ = newActiveTemplate;
            console::printf("[WebView2 UI] Switched to template: %s (before deleting %s)", 
                newActiveTemplate.c_str(), templateName.c_str());
        }
    }
    
    // 现在删除模板
    if (DeleteTemplate(templateName)) {
        RefreshTemplateList(hwnd);
        
        // 选中当前活动模板
        int count = (int)SendMessageW(hCombo, CB_GETCOUNT, 0, 0);
        for (int i = 0; i < count; i++) {
            wchar_t itemBuf[256];
            SendMessageW(hCombo, CB_GETLBTEXT, i, (LPARAM)itemBuf);
            std::string itemName = pfc::stringcvt::string_utf8_from_wide(itemBuf).get_ptr();
            if (itemName == pendingTemplate_) {
                SendMessageW(hCombo, CB_SETCURSEL, i, 0);
                break;
            }
        }
        
        // 通知主窗口重新加载前端
        auto* ui = WebViewUI::GetInstance();
        if (ui && ui->GetMainWindow()) {
            PostMessage(ui->GetMainWindow()->GetHwnd(), WM_COMMAND, MAKEWPARAM(9999, 0), 0);
        }
        
        hasChanges_ = true;
        if (callback_.is_valid()) callback_->on_state_changed();
        
        console::printf("[WebView2 UI] Deleted template: %s", templateName.c_str());
    } else {
        // 删除失败
        if (!TemplateExists(templateName)) {
            MessageBoxW(hwnd, TR("Template does not exist or has been deleted.", "模板不存在或已被删除。"), L"WebView2 UI", MB_OK | MB_ICONERROR);
        } else {
            MessageBoxW(hwnd, TR("Failed to delete template.\n\nThe folder may be in use or you may not have permission.\nTry closing all programs that might be accessing the template files.", "删除模板失败。\n\n文件夹可能正在被使用或没有权限。\n请尝试关闭所有可能正在访问模板文件的程序。"), L"WebView2 UI", MB_OK | MB_ICONERROR);
        }
    }
}

void WebViewPreferencesInstance::OnOpenTemplateFolder(HWND hwnd) {
    std::wstring path = GetWebResourcesBaseDir();
    
    if (!pendingTemplate_.empty()) {
        path += L"\\";
        path += pfc::stringcvt::string_wide_from_utf8(pendingTemplate_.c_str()).get_ptr();
    }
    
    // 确保目录存在
    if (!fs::exists(path)) {
        fs::create_directories(path);
    }
    
    static auto pShellExec = &::ShellExecuteW;
    if (pShellExec)
        pShellExec(nullptr, L"explore", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

// ============================================
// API List Dialog
// ============================================

// API 列表对话框窗口过程
static LRESULT CALLBACK ApiListWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        {
            HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            RECT rc;
            GetClientRect(hwnd, &rc);
            
            // 创建只读编辑框
            HWND hEdit = CreateWindowExW(
                WS_EX_CLIENTEDGE,
                L"EDIT",
                L"",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | 
                ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_AUTOHSCROLL,
                0, 0, rc.right, rc.bottom,
                hwnd,
                (HMENU)1001,
                (HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE),
                nullptr);
            
            // 使用等宽字体 - 美化版本，更大更清晰
            // 优先使用 Cascadia Code (Windows 11/Terminal默认)，回退到 Consolas
            HFONT hMonoFont = CreateFontW(
                -16,        // 字体高度 (负值表示字符高度，正值表示单元格高度)
                0,          // 字体宽度 (0=自动)
                0, 0,       // 倾斜和旋转角度
                FW_NORMAL,  // 字体粗细
                FALSE,      // 斜体
                FALSE,      // 下划线
                FALSE,      // 删除线
                DEFAULT_CHARSET,
                OUT_TT_PRECIS,      // 优先TrueType字体
                CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY,  // ClearType渲染，更清晰
                FIXED_PITCH | FF_MODERN,
                L"Cascadia Code");  // 优先使用Cascadia Code
            
            // 如果Cascadia Code不可用，回退到Consolas
            LOGFONTW lf = {};
            if (!GetObjectW(hMonoFont, sizeof(lf), &lf) || wcscmp(lf.lfFaceName, L"Cascadia Code") != 0) {
                DeleteObject(hMonoFont);
                hMonoFont = CreateFontW(
                    -16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
            }
            SendMessageW(hEdit, WM_SETFONT, (WPARAM)hMonoFont, TRUE);
            
            // 获取所有注册的 API
            auto apiNames = BridgeCore::GetInstance().GetRegisteredApiNames();
            
            // 按字母顺序排序
            std::sort(apiNames.begin(), apiNames.end());
            
            // 构建显示文本
            std::wstring text;
            text += L"//==============================================================================\r\n";
            text += L"// WebView2 UI - API & External Plugin Services Reference\r\n";
            text += L"// Usage: window.fb2k.invoke('method', {params})\r\n";
            text += L"//==============================================================================\r\n\r\n";
            
            // Section 1: Registered APIs
            text += L"//------------------------------------------------------------------------------\r\n";
            text += L"// SECTION 1: Registered APIs (" + std::to_wstring(apiNames.size()) + L" APIs)\r\n";
            text += L"//------------------------------------------------------------------------------\r\n\r\n";
            text += L"const fb2kApis = [\r\n";
            
            for (size_t i = 0; i < apiNames.size(); i++) {
                std::wstring wname = pfc::stringcvt::string_wide_from_utf8(apiNames[i].c_str()).get_ptr();
                text += L"    \"" + wname + L"\"";
                if (i < apiNames.size() - 1) {
                    text += L",";
                }
                text += L"\r\n";
            }
            text += L"];\r\n\r\n";
            
            // Section 2: Main Menu Commands from external plugins
            text += L"//------------------------------------------------------------------------------\r\n";
            text += L"// SECTION 2: External Plugin Menu Commands\r\n";
            text += L"// Use: discovery.executeMainMenuCommand({ guid: '...' })\r\n";
            text += L"//------------------------------------------------------------------------------\r\n\r\n";
            
            {
                service_enum_t<mainmenu_commands> e;
                service_ptr_t<mainmenu_commands> ptr;
                int cmdCount = 0;
                text += L"const externalMenuCommands = [\r\n";
                while (e.next(ptr)) {
                    t_uint32 count = ptr->get_command_count();
                    for (t_uint32 i = 0; i < count; i++) {
                        pfc::string8 name, desc;
                        ptr->get_name(i, name);
                        ptr->get_description(i, desc);
                        GUID cmdGuid = ptr->get_command(i);
                        
                        char guidBuf[64];
                        sprintf_s(guidBuf, "{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
                            cmdGuid.Data1, cmdGuid.Data2, cmdGuid.Data3,
                            cmdGuid.Data4[0], cmdGuid.Data4[1], cmdGuid.Data4[2], cmdGuid.Data4[3],
                            cmdGuid.Data4[4], cmdGuid.Data4[5], cmdGuid.Data4[6], cmdGuid.Data4[7]);
                        
                        std::wstring wname = pfc::stringcvt::string_wide_from_utf8(name.get_ptr()).get_ptr();
                        std::wstring wguid = pfc::stringcvt::string_wide_from_utf8(guidBuf).get_ptr();
                        
                        text += L"    { name: \"" + wname + L"\", guid: \"" + wguid + L"\" },\r\n";
                        cmdCount++;
                    }
                }
                text += L"];  // " + std::to_wstring(cmdCount) + L" commands\r\n\r\n";
            }
            
            // Section 3: Installed Components
            text += L"//------------------------------------------------------------------------------\r\n";
            text += L"// SECTION 3: Installed Components\r\n";
            text += L"// Use: discovery.getComponents()\r\n";
            text += L"//------------------------------------------------------------------------------\r\n\r\n";
            
            {
                service_enum_t<componentversion> e;
                service_ptr_t<componentversion> ptr;
                text += L"const installedComponents = [\r\n";
                int compCount = 0;
                while (e.next(ptr)) {
                    pfc::string8 name, version;
                    ptr->get_component_name(name);
                    ptr->get_component_version(version);
                    
                    std::wstring wname = pfc::stringcvt::string_wide_from_utf8(name.get_ptr()).get_ptr();
                    std::wstring wver = pfc::stringcvt::string_wide_from_utf8(version.get_ptr()).get_ptr();
                    
                    text += L"    { name: \"" + wname + L"\", version: \"" + wver + L"\" },\r\n";
                    compCount++;
                }
                text += L"];  // " + std::to_wstring(compCount) + L" components\r\n\r\n";
            }
            
            // Section 4: Input Formats
            text += L"//------------------------------------------------------------------------------\r\n";
            text += L"// SECTION 4: Supported Input Formats\r\n";
            text += L"// Use: discovery.getInputFormats()\r\n";
            text += L"//------------------------------------------------------------------------------\r\n\r\n";
            
            {
                service_enum_t<input_file_type> eft;
                service_ptr_t<input_file_type> pft;
                text += L"const inputFormats = [\r\n";
                int formatCount = 0;
                while (eft.next(pft)) {
                    t_uint32 count = pft->get_count();
                    for (t_uint32 i = 0; i < count; i++) {
                        pfc::string8 name, mask;
                        pft->get_name(i, name);
                        pft->get_mask(i, mask);
                        
                        std::wstring wname = pfc::stringcvt::string_wide_from_utf8(name.get_ptr()).get_ptr();
                        std::wstring wmask = pfc::stringcvt::string_wide_from_utf8(mask.get_ptr()).get_ptr();
                        
                        text += L"    { name: \"" + wname + L"\", mask: \"" + wmask + L"\" },\r\n";
                        formatCount++;
                    }
                }
                text += L"];  // " + std::to_wstring(formatCount) + L" formats\r\n\r\n";
            }
            
            // Section 5: Context Menu Commands (external plugins)
            text += L"//------------------------------------------------------------------------------\r\n";
            text += L"// SECTION 5: External Plugin Context Menu Commands\r\n";
            text += L"// Use: discovery.executeContextMenuCommand({ guid: '...' })\r\n";
            text += L"// Note: These commands operate on currently playing or selected tracks\r\n";
            text += L"//------------------------------------------------------------------------------\r\n\r\n";
            
            {
                service_enum_t<contextmenu_item> e;
                service_ptr_t<contextmenu_item> ptr;
                int cmdCount = 0;
                text += L"const contextMenuCommands = [\r\n";
                while (e.next(ptr)) {
                    t_uint32 count = ptr->get_num_items();
                    for (t_uint32 i = 0; i < count; i++) {
                        pfc::string8 name, desc;
                        ptr->get_item_name(i, name);
                        ptr->get_item_description(i, desc);
                        GUID cmdGuid = ptr->get_item_guid(i);
                        
                        char guidBuf[64];
                        sprintf_s(guidBuf, "{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
                            cmdGuid.Data1, cmdGuid.Data2, cmdGuid.Data3,
                            cmdGuid.Data4[0], cmdGuid.Data4[1], cmdGuid.Data4[2], cmdGuid.Data4[3],
                            cmdGuid.Data4[4], cmdGuid.Data4[5], cmdGuid.Data4[6], cmdGuid.Data4[7]);
                        
                        std::wstring wname = pfc::stringcvt::string_wide_from_utf8(name.get_ptr()).get_ptr();
                        std::wstring wdesc = pfc::stringcvt::string_wide_from_utf8(desc.get_ptr()).get_ptr();
                        std::wstring wguid = pfc::stringcvt::string_wide_from_utf8(guidBuf).get_ptr();
                        
                        text += L"    { name: \"" + wname + L"\", desc: \"" + wdesc + L"\", guid: \"" + wguid + L"\" },\r\n";
                        cmdCount++;
                    }
                }
                text += L"];  // " + std::to_wstring(cmdCount) + L" context commands\r\n\r\n";
            }
            
            // Section 6: DSP Entries
            text += L"//------------------------------------------------------------------------------\r\n";
            text += L"// SECTION 6: DSP Processors\r\n";
            text += L"// Use: discovery.getDspEntries()\r\n";
            text += L"//------------------------------------------------------------------------------\r\n\r\n";
            
            {
                service_enum_t<dsp_entry> e;
                service_ptr_t<dsp_entry> ptr;
                text += L"const dspProcessors = [\r\n";
                int dspCount = 0;
                while (e.next(ptr)) {
                    pfc::string8 name;
                    ptr->get_name(name);
                    GUID guid = ptr->get_guid();
                    
                    char guidBuf[64];
                    sprintf_s(guidBuf, "{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
                        guid.Data1, guid.Data2, guid.Data3,
                        guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
                        guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
                    
                    std::wstring wname = pfc::stringcvt::string_wide_from_utf8(name.get_ptr()).get_ptr();
                    std::wstring wguid = pfc::stringcvt::string_wide_from_utf8(guidBuf).get_ptr();
                    
                    text += L"    { name: \"" + wname + L"\", guid: \"" + wguid + L"\" },\r\n";
                    dspCount++;
                }
                text += L"];  // " + std::to_wstring(dspCount) + L" DSPs\r\n\r\n";
            }
            
            // Example calls
            text += L"//------------------------------------------------------------------------------\r\n";
            text += L"// EXAMPLE CALLS\r\n";
            text += L"//------------------------------------------------------------------------------\r\n\r\n";
            text += L"// Basic playback:\r\n";
            text += L"// window.fb2k.invoke('playback.getState')\r\n";
            text += L"// window.fb2k.invoke('playback.play')\r\n";
            text += L"// window.fb2k.invoke('playback.pause')\r\n\r\n";
            text += L"// Playlist operations:\r\n";
            text += L"// window.fb2k.invoke('playlist.getItems', { playlistIndex: 0 })\r\n";
            text += L"// window.fb2k.invoke('playlist.getAll')\r\n\r\n";
            text += L"// Library search:\r\n";
            text += L"// window.fb2k.invoke('library.search', { query: 'artist:Beatles' })\r\n\r\n";
            text += L"// Discovery - execute external plugin main menu command:\r\n";
            text += L"// window.fb2k.invoke('discovery.executeMainMenuCommand', { guid: '{...}' })\r\n\r\n";
            text += L"// Discovery - execute external plugin context menu command:\r\n";
            text += L"// window.fb2k.invoke('discovery.executeContextMenuCommand', { guid: '{...}' })\r\n";
            text += L"// Note: Context menu commands operate on currently playing or selected tracks\r\n\r\n";
            text += L"// Get all available context menu commands:\r\n";
            text += L"// window.fb2k.invoke('discovery.getContextMenuCommands')\r\n";
            
            SetWindowTextW(hEdit, text.c_str());
            
            // 保存字体句柄以便稍后释放
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)hMonoFont);
            return 0;
        }
        
    case WM_SIZE:
        {
            HWND hEdit = GetDlgItem(hwnd, 1001);
            if (hEdit) {
                RECT rc;
                GetClientRect(hwnd, &rc);
                MoveWindow(hEdit, 0, 0, rc.right, rc.bottom, TRUE);
            }
            return 0;
        }
        
    case WM_DESTROY:
        {
            HFONT hFont = (HFONT)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
            if (hFont) {
                DeleteObject(hFont);
            }
            return 0;
        }
        
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    }
    
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void WebViewPreferencesInstance::OnShowApiList(HWND hwnd) {
    // 注册窗口类
    static bool classRegistered = false;
    const wchar_t* className = L"WebViewUIApiListWindow";
    
    if (!classRegistered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = ApiListWndProc;
        wc.hInstance = core_api::get_my_instance();
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = className;
        wc.hIcon = nullptr;
        wc.hIconSm = nullptr;
        try { static_api_ptr_t<ui_control> fb_ui; wc.hIcon = fb_ui->get_main_icon(); wc.hIconSm = fb_ui->get_main_icon(); } catch (...) {}
        
        if (RegisterClassExW(&wc)) {
            classRegistered = true;
        } else {
            MessageBoxW(hwnd, TR("Failed to register window class.", "注册窗口类失败。"), TR("Error", "错误"), MB_OK | MB_ICONERROR);
            return;
        }
    }
    
    // 计算窗口位置 (居中于屏幕)
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int width = 900;
    int height = 700;
    int x = (screenWidth - width) / 2;
    int y = (screenHeight - height) / 2;
    
    // 创建窗口
    HWND hApiWnd = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        className,
        TR("WebView2 UI - API && External Plugin Services", "WebView2 UI - API && 外部插件服务"),
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        x, y, width, height,
        nullptr,  // 无父窗口，独立窗口
        nullptr,
        core_api::get_my_instance(),
        nullptr);
    
    if (!hApiWnd) {
        MessageBoxW(hwnd, TR("Failed to create API list window.", "创建 API 列表窗口失败。"), TR("Error", "错误"), MB_OK | MB_ICONERROR);
    }
}

// ============================================
// 注册 Preferences Page
// ============================================

static preferences_page_factory_t<WebViewPreferencesPage> g_prefs_page_factory;

} // namespace webview_prefs
