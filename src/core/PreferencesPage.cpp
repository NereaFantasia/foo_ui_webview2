// ============================================
// PreferencesPage.cpp - WebView2 UI Preferences Page Implementation
// Preferences → Display → WebView2 UI
// ============================================

#include "pch.h"
#include "core/PreferencesPage.h"
#include "core/PreferencesFields.h"
#include "core/PreferencesTemplateName.h"
#include "core/PreferencesZoom.h"
#include "core/PreferencesCdpPort.h"
#include "webview/WebViewHost.h"
#include "core/AdvconfigI18n.h"
#include "core/SecurityConfig.h"
#include "api/BridgeCore.h"
#include "core/UserInterface.h"
#include "core/BackgroundService.h"
#include "core/WebViewContext.h"
#include "core/WebViewPanel.h"
#include "panels/PanelConfig.h"
#include "window/MainWindow.h"
#include "window/WindowManager.h"
#include <CommCtrl.h>
#include <shlobj.h>
#include <uxtheme.h>
#include <filesystem>
#include <cstring>
#include <fstream>
#include <algorithm>
#include <utility>
#include "utils/I18n.h"
#include "utils/GuidUtils.h"

namespace fs = std::filesystem;

// ============================================
// 安全例外 (从 main.cpp 引用 advconfig) - 全局命名空间
// ============================================
extern advconfig_i18n::CheckboxFactory g_cfg_local_network;
extern advconfig_i18n::CheckboxFactory g_cfg_allow_insecure;
extern advconfig_i18n::CheckboxFactory g_cfg_allow_insecure_tls;

namespace webview_prefs {

// 总览页字段表的下标与枚举上限。
namespace overview = prefs_fields::overview;

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

// Window 子页的托盘与任务栏开关
// {689D06FC-5D6C-4750-895A-C866F734781A}
static constexpr GUID guid_cfg_minimize_to_tray =
    { 0x689d06fc, 0x5d6c, 0x4750, { 0x89, 0x5a, 0xc8, 0x66, 0xf7, 0x34, 0x78, 0x1a } };

// {381E99C6-24AA-444C-B3AD-1D1877657B2A}
static constexpr GUID guid_cfg_close_to_tray =
    { 0x381e99c6, 0x24aa, 0x444c, { 0xb3, 0xad, 0x1d, 0x18, 0x77, 0x65, 0x7b, 0x2a } };

// {8E440D98-9492-41ED-A6CE-8764D36BEEB4}
static constexpr GUID guid_cfg_taskbar_buttons =
    { 0x8e440d98, 0x9492, 0x41ed, { 0xa6, 0xce, 0x87, 0x64, 0xd3, 0x6b, 0xee, 0xb4 } };

// {9E4060D0-2DC2-477F-AA97-0B73AAD9F71E}
static constexpr GUID guid_cfg_taskbar_progress =
    { 0x9e4060d0, 0x2dc2, 0x477f, { 0xaa, 0x97, 0x0b, 0x73, 0xaa, 0xd9, 0xf7, 0x1e } };

// Performance 子页的预热开关与默认缩放。深度挂起的开关在 main.cpp（security_config），不在这里。
// {FB43CF51-7672-4E15-BD55-9E617E5A8CFA}
static constexpr GUID guid_cfg_preheat =
    { 0xfb43cf51, 0x7672, 0x4e15, { 0xbd, 0x55, 0x9e, 0x61, 0x7e, 0x5a, 0x8c, 0xfa } };

// {D8131D0F-3A4C-42FA-A9EB-96B008A7BB43}
static constexpr GUID guid_cfg_default_zoom_percent =
    { 0xd8131d0f, 0x3a4c, 0x42fa, { 0xa9, 0xeb, 0x96, 0xb0, 0x08, 0xa7, 0xbb, 0x43 } };

// Developer 子页的 CDP 端口。开关本身在 main.cpp（security_config）。
// {CC74C4AB-34C3-4FF4-A78F-05BC248F9C1C}
static constexpr GUID guid_cfg_cdp_port =
    { 0xcc74c4ab, 0x34c3, 0x4ff4, { 0xa7, 0x8f, 0x05, 0xbc, 0x24, 0x8f, 0x9c, 0x1c } };

// ============================================
// 配置变量
// ============================================

static cfg_string cfg_active_template(guid_cfg_active_template, "default");
static cfg_var_modern::cfg_bool cfg_start_with_foobar(guid_cfg_start_with_foobar, true);
static cfg_var_modern::cfg_bool cfg_remember_position(guid_cfg_remember_position, true);
static cfg_var_modern::cfg_bool cfg_auto_hide(guid_cfg_auto_hide, false);
static cfg_var_modern::cfg_int cfg_backdrop_effect(guid_cfg_backdrop_effect, static_cast<int>(BackdropEffect::Mica));
static cfg_var_modern::cfg_bool cfg_minimize_to_tray(guid_cfg_minimize_to_tray, false);
static cfg_var_modern::cfg_bool cfg_close_to_tray(guid_cfg_close_to_tray, false);
static cfg_var_modern::cfg_bool cfg_taskbar_buttons(guid_cfg_taskbar_buttons, true);
static cfg_var_modern::cfg_bool cfg_taskbar_progress(guid_cfg_taskbar_progress, false);
static cfg_var_modern::cfg_bool cfg_preheat(guid_cfg_preheat, true);
static cfg_var_modern::cfg_int cfg_default_zoom_percent(guid_cfg_default_zoom_percent, prefs_zoom::kDefaultPercent);
static cfg_var_modern::cfg_int cfg_cdp_port(guid_cfg_cdp_port, prefs_cdp::kDefaultPort);

// ============================================
// Web 资源目录管理
// ============================================

std::wstring GetWebResourcesBaseDir() {
    // get_profile_path() 可能带 file:// 前缀，去掉后才是磁盘路径。
    pfc::string8 profilePath = core_api::get_profile_path();
    if (profilePath.startsWith("file://")) {
        profilePath = profilePath.subString(7);
    }
    
    std::wstring basePath = pfc::stringcvt::string_wide_from_utf8(profilePath.c_str()).get_ptr();
    
    // WebView UI 资源根目录: profile/webview-ui/
    basePath += L"\\webview-ui";
    
    // 根目录缺失时在这里建出来：模板枚举与打开文件夹都以它存在为前提；模板子目录不在此建。
    if (!fs::exists(basePath)) {
        fs::create_directories(basePath);
    }
    
    return basePath;
}

std::wstring GetActiveWebResourcesDir() {
    std::wstring baseDir = GetWebResourcesBaseDir();
    // cfg_string::get() 按值返回；先落到具名对象再取指针，避免指向已析构的临时量。
    const pfc::string8 templateName = cfg_active_template.get();
    
    std::string templateStr = templateName.is_empty() ? "default" : templateName.c_str();
    
    std::wstring templateDir = baseDir + L"\\" + 
        pfc::stringcvt::string_wide_from_utf8(templateStr.c_str()).get_ptr();
    
    // 如果活动模板目录不存在，返回空（让调用者处理）
    if (!fs::exists(templateDir)) {
        return L"";
    }
    
    return templateDir;
}

std::wstring GetActiveTemplateName() {
    const pfc::string8 name = cfg_active_template.get();
    std::string nameStr = name.is_empty() ? "default" : name.c_str();
    return pfc::stringcvt::string_wide_from_utf8(nameStr.c_str()).get_ptr();
}

void SetActiveTemplateName(const std::string& name) {
    // 名字会拼进目录路径：只放行字母、数字、连字符、下划线（与 prefs_draft::IsValidTemplateName
    // 同一规则），其余静默不写；调用方要读回比对，不能把调用成功当作已写入。
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
    
    // 列表为空就照实返回：页面显示“default（不存在）”并拒绝 Apply，
    // 模板只由用户通过 Manage 创建（DESIGN D3）。
    return templates;
}

bool CreateTemplate(const std::string& name) {
    if (!prefs_draft::IsValidTemplateName(name) || TemplateExists(name)) {
        return false;
    }
    
    std::wstring baseDir = GetWebResourcesBaseDir();
    std::wstring templateDir = baseDir + L"\\" + 
        pfc::stringcvt::string_wide_from_utf8(name.c_str()).get_ptr();
    
    // 目录、index.html 写入与关闭三步都成功才算成功。
    // 中途失败不回滚：已建出的目录留在磁盘上，由调用方向用户如实报告。
    std::error_code ec;
    fs::create_directories(templateDir, ec);
    if (ec) {
        return false;
    }
    
    std::wstring indexPath = templateDir + L"\\index.html";
    std::ofstream indexFile(indexPath, std::ios::binary);
    if (!indexFile.is_open()) {
        return false;
    }
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
    // close() 失败置 failbit，写入失败置 badbit：任一为真都不宣称成功。
    return !indexFile.fail();
}

bool RenameTemplate(const std::string& oldName, const std::string& newName) {
    if (oldName.empty() || newName.empty() || !TemplateExists(oldName) || TemplateExists(newName)) {
        return false;
    }
    
    // 新名字的字符集与 prefs_draft::IsValidTemplateName 同一规则。
    // unsigned char 转型：非 ASCII 输入的字节为负值时 isalnum 是 UB（Debug CRT 断言）
    for (char c : newName) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-') {
            return false;
        }
    }
    
    std::wstring baseDir = GetWebResourcesBaseDir();
    std::wstring oldDir = baseDir + L"\\" + 
        pfc::stringcvt::string_wide_from_utf8(oldName.c_str()).get_ptr();
    std::wstring newDir = baseDir + L"\\" + 
        pfc::stringcvt::string_wide_from_utf8(newName.c_str()).get_ptr();
    
    // 只改目录，不碰 cfg_active_template：活动模板的改名在页面层已被拒绝，
    // 这里若替调用方改写配置，就等于绕过那条保护。
    std::error_code ec;
    fs::rename(oldDir, newDir, ec);
    return !ec;
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
// 托盘与任务栏（Window 子页）
// ============================================

bool GetMinimizeToTrayPreference() {
    return cfg_minimize_to_tray.get();
}

void SetMinimizeToTrayPreference(bool value) {
    cfg_minimize_to_tray.set(value);
}

bool GetCloseToTrayPreference() {
    return cfg_close_to_tray.get();
}

void SetCloseToTrayPreference(bool value) {
    cfg_close_to_tray.set(value);
}

bool GetTaskbarButtonsEnabled() {
    return cfg_taskbar_buttons.get();
}

void SetTaskbarButtonsEnabled(bool value) {
    cfg_taskbar_buttons.set(value);
}

bool GetTaskbarProgressEnabled() {
    return cfg_taskbar_progress.get();
}

void SetTaskbarProgressEnabled(bool value) {
    cfg_taskbar_progress.set(value);
}

// ============================================
// 性能（Performance 子页）
// ============================================

bool GetPreheatEnabled() {
    return cfg_preheat.get();
}

void SetPreheatEnabled(bool value) {
    cfg_preheat.set(value);
}

int GetDefaultZoomPercent() {
    // 存储值可能来自旧版本或被外部改写：读出时就钳到下拉框范围，UI 与新窗口用同一个值。
    return prefs_zoom::SanitizePercent(static_cast<int>(cfg_default_zoom_percent.get()));
}

void SetDefaultZoomPercent(int percent) {
    cfg_default_zoom_percent.set(prefs_zoom::SanitizePercent(percent));
}

void ApplyDefaultZoomToFollowingHosts() {
    const double factor = prefs_zoom::FactorFromPercent(GetDefaultZoomPercent());
    auto& context = WebViewContext::GetInstance();
    for (HWND hwnd : context.GetAllInstances()) {
        if (!IsWindow(hwnd)) continue;
        WebViewHost* host = context.GetHostByHwnd(hwnd);
        if (!host || host->IsZoomOverriddenByTheme()) continue;
        host->SetZoomFactor(factor);
    }
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
// 开发者选项与安全例外访问函数
// ============================================

bool GetDevToolsEnabled() {
    return security_config::GetDevToolsSetting();
}

int GetCdpPort() {
    return prefs_cdp::SanitizePort(static_cast<int>(cfg_cdp_port.get()));
}

void SetCdpPort(int port) {
    cfg_cdp_port.set(prefs_cdp::SanitizePort(port));
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

// 上面三项安全例外的写入口在 Advanced Preferences（Tools > WebView2 UI），这里只读取。

// ============================================
// 开发服务器配置访问函数
// ============================================

bool UseDevServer() {
    return security_config::UseDevServer();
}

std::string GetDevServerUrl() {
    // security_config 交出的是静态缓冲的指针，下一次调用就会被覆盖，所以复制一份给调用方。
    const char* url = security_config::GetDevServerUrl();
    return std::string(url ? url : "");
}

// ============================================
// Dialog Control IDs
// ============================================

enum ControlIds {
    // Web template 组
    IDC_GROUP_TEMPLATE = 1000,
    IDC_STATIC_TEMPLATE = 1001,
    IDC_COMBO_TEMPLATE = 1002,
    IDC_BTN_MANAGE = 1003,
    IDC_STATIC_PATH = 1004,
    IDC_EDIT_PATH = 1005,
    IDC_BTN_OPEN_FOLDER = 1006,

    // Appearance 组
    IDC_GROUP_APPEARANCE = 1020,
    IDC_STATIC_LANGUAGE = 1021,
    IDC_COMBO_LANGUAGE = 1022,
    IDC_STATIC_BACKDROP = 1023,
    IDC_COMBO_BACKDROP = 1024,
    IDC_STATIC_LANGUAGE_NOTE = 1025,

    // 1010–1013 不复用。

    // Tools 组
    IDC_GROUP_TOOLS = 1050,
    IDC_STATIC_DEV_SERVER = 1051,
    IDC_STATIC_DEV_SERVER_STATUS = 1052,
    IDC_BTN_ADVANCED = 1053,
    IDC_BTN_SHOW_API_LIST = 1054,
    IDC_STATIC_TOOLS_NOTE = 1055,
};

// Manage... 弹出菜单的命令 ID
enum ManageMenuIds {
    IDM_TEMPLATE_CREATE = 2001,
    IDM_TEMPLATE_RENAME = 2002,
    IDM_TEMPLATE_DELETE = 2003,
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
    : PreferencesPageBase(std::move(callback), overview::Table(&WebViewPreferencesInstance::TemplateIsUsable)) {
    
    // 语言覆盖只由本页写入，所以首个实例读到的值就是本进程启动值。
    static const int s_startupLanguage = static_cast<int>(i18n::GetLanguageOverride());
    startupLanguage_ = s_startupLanguage;
    
    CreatePageWindow(parent);
}

// ============================================
// 字段读写
// ============================================

std::wstring WebViewPreferencesInstance::FieldDisplayName(size_t field) const {
    switch (field) {
    case overview::Template: return TR("Template", "模板");
    case overview::Backdrop: return TR("Default window backdrop", "默认窗口背景效果");
    case overview::Language: return TR("Component language", "组件语言");
    default:
        break;
    }
    return {};
}

std::wstring WebViewPreferencesInstance::ValidationMessage(const prefs_draft::ValidationResult& result) const {
    if (result.field == overview::Template) {
        if (result.error == prefs_draft::ValidationError::Invalid) {
            return TR("The template name may only contain letters, numbers, hyphens and underscores.",
                      "模板名只能包含字母、数字、连字符和下划线。");
        }
        if (result.error == prefs_draft::ValidationError::Missing) {
            return TR("The selected template folder does not exist or has no index.html.\n"
                      "Create it with Manage... or choose another template, then apply again.",
                      "所选模板目录不存在或没有 index.html。\n"
                      "请用“管理...”创建，或改选其他模板后再应用。");
        }
    }
    return PreferencesPageBase::ValidationMessage(result);
}

prefs_draft::Snapshot WebViewPreferencesInstance::ReadSnapshotFromConfig() const {
    prefs_draft::Snapshot s = prefs_draft::Defaults(fields());
    const pfc::string8 templateStr = cfg_active_template.get();
    s.SetString(overview::Template, templateStr.is_empty() ? "default" : templateStr.c_str());
    // 未知存储值在 UI 中按默认值显示；只有 Apply 时才会把值写回。
    s.SetInt(overview::Backdrop, prefs_draft::SanitizeEnum(static_cast<int>(cfg_backdrop_effect.get()),
        overview::kBackdropCount, static_cast<int>(BackdropEffect::Mica)));
    s.SetInt(overview::Language, prefs_draft::SanitizeEnum(static_cast<int>(i18n::GetLanguageOverride()),
        overview::kLanguageCount, static_cast<int>(i18n::LanguageOverride::Auto)));
    return s;
}

prefs_draft::Snapshot WebViewPreferencesInstance::StartupSnapshot() const {
    // 只有语言影响重启：宿主缓存的菜单 / 面板描述不会因 Apply 更新。其余字段取什么值都不参与判定。
    prefs_draft::Snapshot s = prefs_draft::Defaults(fields());
    s.SetInt(overview::Language, startupLanguage_);
    return s;
}

bool WebViewPreferencesInstance::TemplateIsUsable(const std::string& name) {
    if (!prefs_draft::IsValidTemplateName(name) || !TemplateExists(name)) return false;
    std::wstring indexPath = GetWebResourcesBaseDir() + L"\\" +
        pfc::stringcvt::string_wide_from_utf8(name.c_str()).get_ptr() + L"\\index.html";
    std::error_code ec;
    return fs::is_regular_file(indexPath, ec) && fs::file_size(indexPath, ec) > 0 && !ec;
}

bool WebViewPreferencesInstance::WriteField(size_t field, const prefs_draft::Snapshot& value) {
    try {
        switch (field) {
        case overview::Template: {
            // SetActiveTemplateName 对非法名字静默不写，所以要读回比对而不是信任调用成功。
            const std::string& name = value.GetString(overview::Template);
            SetActiveTemplateName(name);
            return name == cfg_active_template.get().c_str();
        }
        case overview::Backdrop: {
            const int backdrop = value.GetInt(overview::Backdrop);
            SetBackdropEffect(static_cast<BackdropEffect>(backdrop));
            return cfg_backdrop_effect.get() == backdrop;
        }
        case overview::Language: {
            // 内部会清语言缓存；本页已创建的控件文案保持原语言，新建的组件对话框用新语言。
            const int language = value.GetInt(overview::Language);
            i18n::SetLanguageOverride(static_cast<i18n::LanguageOverride>(language));
            return static_cast<int>(i18n::GetLanguageOverride()) == language;
        }
        default:
            break;
        }
    } catch (...) {
    }
    return false;
}

void WebViewPreferencesInstance::AfterApply(const std::vector<size_t>& changed) {
    // 导航是异步的，走既有加载与回退路径。
    const auto has = [&changed](size_t field) {
        return std::find(changed.begin(), changed.end(), field) != changed.end();
    };
    if (has(overview::Template)) ReloadFrontendsForTemplateChange();
    if (has(overview::Backdrop)) RefreshChromeForBackdropChange();
}

namespace {

// 主窗口与后台窗口可能是同一个实例，也可能都不存在；不存在的窗口不算保存失败。
std::vector<MainWindow*> CollectMainWindows() {
    std::vector<MainWindow*> mainWindows;
    if (auto* ui = WebViewUI::GetInstance()) {
        if (ui->GetMainWindow()) mainWindows.push_back(ui->GetMainWindow());
    }
    if (MainWindow* background = background_service::GetBackgroundWindow()) {
        if (std::find(mainWindows.begin(), mainWindows.end(), background) == mainWindows.end()) {
            mainWindows.push_back(background);
        }
    }
    return mainWindows;
}

// 面板：先取句柄快照，再逐个解析活实例；显式模板或 URL 覆盖的面板不跟随全局模板。
void ReloadPanelsFollowingGlobalTemplate() {
    auto& context = WebViewContext::GetInstance();
    for (HWND hwnd : context.GetAllInstances()) {
        if (!IsWindow(hwnd)) continue;
        WebViewPanel* panel = context.GetPanelByHwnd(hwnd);
        if (!panel) continue;
        const PanelConfig& cfg = panel->GetPanelConfig();
        if (!cfg.templateName.empty() || !cfg.urlOverride.empty()) continue;
        panel->ReloadFrontend();
    }
}

}  // namespace

void ReloadFrontendsForDevServerChange() {
    // 主窗口走完整的前端加载路径：它会重新判断开关与 URL，开发服务器不可达时回退本地模板。
    for (MainWindow* window : CollectMainWindows()) {
        window->ReloadFrontend();
    }
    ReloadPanelsFollowingGlobalTemplate();
    console::printf("[WebView2 UI] Development server settings applied; reloading frontends that follow the global template");
}

void WebViewPreferencesInstance::ReloadFrontendsForTemplateChange() {
    // 开发服务器覆盖开启时默认模板只供之后的本地加载使用，不抢占当前开发页面。
    if (security_config::UseDevServer()) {
        console::printf("[WebView2 UI] Template changed; development server override is active, not reloading");
        return;
    }
    
    for (MainWindow* window : CollectMainWindows()) {
        window->ReloadFrontendForTemplateChange();
    }
    ReloadPanelsFollowingGlobalTemplate();
}

void WebViewPreferencesInstance::RefreshChromeForBackdropChange() {
    // 只刷新已存在的 MainWindow 与 Popup：重新解析继承，不写显式策略；
    // miniPlayer / desktopLyrics 等自带策略的窗口在解析里保持优先。DUI/CUI 面板没有 DWM 外壳。
    std::vector<WindowShellBase*> shells;
    if (auto* ui = WebViewUI::GetInstance()) {
        if (ui->GetMainWindow()) shells.push_back(ui->GetMainWindow());
    }
    if (MainWindow* background = background_service::GetBackgroundWindow()) {
        if (std::find(shells.begin(), shells.end(), background) == shells.end()) shells.push_back(background);
    }
    auto& manager = WindowManager::GetInstance();
    for (const std::string& id : manager.GetAllWindowIds()) {
        if (PopupWindow* popup = manager.GetPopup(id)) shells.push_back(popup);
    }
    for (WindowShellBase* shell : shells) {
        shell->RefreshChrome();
    }
}

void WebViewPreferencesInstance::SyncControlsFromDraft() {
    if (!hwnd()) return;
    
    HWND hComboTemplate = GetDlgItem(hwnd(), IDC_COMBO_TEMPLATE);
    if (hComboTemplate) {
        const std::wstring wanted =
            pfc::stringcvt::string_wide_from_utf8(draft().GetString(overview::Template).c_str()).get_ptr();
        const std::wstring missingSuffix = TR(" (missing)", "（不存在）");
        auto itemText = [hComboTemplate](int index) {
            const int length = static_cast<int>(SendMessageW(hComboTemplate, CB_GETLBTEXTLEN, index, 0));
            if (length < 0) return std::wstring();
            std::wstring item(static_cast<size_t>(length) + 1, L'\0');
            SendMessageW(hComboTemplate, CB_GETLBTEXT, index, reinterpret_cast<LPARAM>(item.data()));
            item.resize(static_cast<size_t>(length));
            return item;
        };
        // 标注项只由本函数追加，不对应磁盘上的目录。每次同步先清掉旧标注，
        // 这样 Reset / 冲突 / 写入失败路径反复调用也不会堆出重复项或过期项。
        for (int i = static_cast<int>(SendMessageW(hComboTemplate, CB_GETCOUNT, 0, 0)) - 1; i >= 0; i--) {
            const std::wstring item = itemText(i);
            if (item.size() > missingSuffix.size() && item.ends_with(missingSuffix)) {
                SendMessageW(hComboTemplate, CB_DELETESTRING, static_cast<WPARAM>(i), 0);
            }
        }
        const int count = static_cast<int>(SendMessageW(hComboTemplate, CB_GETCOUNT, 0, 0));
        int found = -1;
        for (int i = 0; i < count; i++) {
            if (itemText(i) == wanted) {
                found = i;
                break;
            }
        }
        if (found < 0) {
            // 草稿指向不存在的模板（例如 default 缺失）：列出来并标注，不偷偷建也不另选。
            const std::wstring label = wanted + missingSuffix;
            found = static_cast<int>(SendMessageW(hComboTemplate, CB_ADDSTRING, 0,
                reinterpret_cast<LPARAM>(label.c_str())));
        }
        SendMessageW(hComboTemplate, CB_SETCURSEL, found, 0);
    }
    
    HWND hComboLanguage = GetDlgItem(hwnd(), IDC_COMBO_LANGUAGE);
    if (hComboLanguage) {
        SendMessageW(hComboLanguage, CB_SETCURSEL, static_cast<WPARAM>(draft().GetInt(overview::Language)), 0);
    }
    
    HWND hComboBackdrop = GetDlgItem(hwnd(), IDC_COMBO_BACKDROP);
    if (hComboBackdrop) {
        SendMessageW(hComboBackdrop, CB_SETCURSEL, static_cast<WPARAM>(draft().GetInt(overview::Backdrop)), 0);
    }
    
    RefreshTemplateFolderText();
}

// 控件获得焦点时的滚入可见区由基类处理；这里只接命令与选择变化。
INT_PTR WebViewPreferencesInstance::OnMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    (void)lParam;
    if (msg != WM_COMMAND) return FALSE;
    const WORD code = HIWORD(wParam);
    
    switch (LOWORD(wParam)) {
    case IDC_COMBO_TEMPLATE:
        if (code == CBN_SELCHANGE) OnTemplateSelectionChanged(hwnd);
        return TRUE;
        
    case IDC_BTN_MANAGE:
        if (code == BN_CLICKED) OnManageTemplates(hwnd);
        return TRUE;
        
    case IDM_TEMPLATE_CREATE:
        OnCreateTemplate(hwnd);
        return TRUE;
        
    case IDM_TEMPLATE_RENAME:
        OnRenameTemplate(hwnd);
        return TRUE;
        
    case IDM_TEMPLATE_DELETE:
        OnDeleteTemplate(hwnd);
        return TRUE;
        
    case IDC_BTN_OPEN_FOLDER:
        if (code == BN_CLICKED) OnOpenTemplateFolder(hwnd);
        return TRUE;
        
    case IDC_EDIT_PATH:
        return TRUE;
        
    case IDC_COMBO_BACKDROP:
        if (code != CBN_SELCHANGE) return TRUE;
        {
            // 只改草稿：背景效果不做“选中即预览”，否则 Cancel 无法真正取消。
            HWND hCombo = GetDlgItem(hwnd, IDC_COMBO_BACKDROP);
            const int sel = static_cast<int>(SendMessageW(hCombo, CB_GETCURSEL, 0, 0));
            if (sel >= 0) draft().SetInt(overview::Backdrop, sel);
            UpdateState();
        }
        return TRUE;
        
    case IDC_COMBO_LANGUAGE:
        if (code != CBN_SELCHANGE) return TRUE;
        {
            // 下拉项顺序与 LanguageOverride 数值一致，越界一律回落 Auto。
            // 语言只进草稿，Apply 才调用 SetLanguageOverride。
            HWND hCombo = GetDlgItem(hwnd, IDC_COMBO_LANGUAGE);
            const int sel = static_cast<int>(SendMessageW(hCombo, CB_GETCURSEL, 0, 0));
            draft().SetInt(overview::Language, prefs_draft::SanitizeEnum(sel, overview::kLanguageCount,
                static_cast<int>(i18n::LanguageOverride::Auto)));
            UpdateState();
        }
        return TRUE;
        
    case IDC_BTN_ADVANCED:
        if (code == BN_CLICKED) OnOpenAdvancedPreferences();
        return TRUE;
        
    case IDC_BTN_SHOW_API_LIST:
        if (code == BN_CLICKED) OnShowApiList(hwnd);
        return TRUE;
        
    default:
        break;
    }
    return FALSE;
}

// ============================================
// 控件创建
// ============================================

void WebViewPreferencesInstance::CreateControls(HWND hwnd) {
    HINSTANCE hInst = core_api::get_my_instance();
    const HFONT pageFont = font();
    
    // 分组框先创建，排在 Z 序底部，避免盖住组内控件。
    auto createGroup = [&](int id, const wchar_t* title) {
        HWND h = CreateWindowExW(WS_EX_TRANSPARENT, L"BUTTON", title,
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | BS_GROUPBOX,
            0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), hInst, nullptr);
        SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(pageFont), FALSE);
        return h;
    };
    auto createStatic = [&](int id, const wchar_t* text, DWORD extraStyle) {
        HWND h = CreateWindowExW(0, L"STATIC", text,
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | SS_LEFT | SS_NOPREFIX | extraStyle,
            0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), hInst, nullptr);
        SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(pageFont), FALSE);
        return h;
    };
    auto createCombo = [&](int id) {
        HWND h = CreateWindowExW(0, L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST,
            0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), hInst, nullptr);
        SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(pageFont), FALSE);
        return h;
    };
    auto createButton = [&](int id, const wchar_t* text, DWORD style) {
        // BS_NOTIFY 让按钮与复选框在获得焦点时发 BN_SETFOCUS，滚动区据此把它滚入可见范围。
        HWND h = CreateWindowExW(0, L"BUTTON", text,
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP | BS_NOTIFY | style,
            0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), hInst, nullptr);
        SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(pageFont), FALSE);
        return h;
    };
    auto createReadOnlyEdit = [&](int id, DWORD exStyle, DWORD style) {
        HWND h = CreateWindowExW(exStyle, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | ES_LEFT | ES_READONLY | ES_AUTOHSCROLL | style,
            0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), hInst, nullptr);
        SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(pageFont), FALSE);
        return h;
    };
    
    createGroup(IDC_GROUP_TEMPLATE, TR("Web template", "Web 模板"));
    createGroup(IDC_GROUP_APPEARANCE, TR("Appearance", "外观"));
    createGroup(IDC_GROUP_TOOLS, TR("Tools", "工具"));
    
    // ---- Web template ----
    createStatic(IDC_STATIC_TEMPLATE, TR("Template", "模板"), 0);
    createCombo(IDC_COMBO_TEMPLATE);
    createButton(IDC_BTN_MANAGE, TR("Manage...", "管理..."), BS_PUSHBUTTON);
    createStatic(IDC_STATIC_PATH, TR("Template folder", "模板文件夹"), 0);
    // 只读但可选中、可横向滚动、可复制：完整路径留在控件里，不写省略串。
    createReadOnlyEdit(IDC_EDIT_PATH, WS_EX_CLIENTEDGE, WS_TABSTOP);
    createButton(IDC_BTN_OPEN_FOLDER, TR("Open...", "打开..."), BS_PUSHBUTTON);
    
    // ---- Appearance ----
    createStatic(IDC_STATIC_LANGUAGE, TR("Component language", "组件语言"), 0);
    HWND hComboLanguage = createCombo(IDC_COMBO_LANGUAGE);
    // 顺序必须与 i18n::LanguageOverride 的数值一一对应（Auto=0 / English=1 / Chinese=2）。
    SendMessageW(hComboLanguage, CB_ADDSTRING, 0,
        reinterpret_cast<LPARAM>(TR("Auto (follow foobar2000)", "自动（跟随 foobar2000）")));
    SendMessageW(hComboLanguage, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"English"));
    SendMessageW(hComboLanguage, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"中文"));
    SendMessageW(hComboLanguage, CB_SETCURSEL, static_cast<WPARAM>(draft().GetInt(overview::Language)), 0);
    
    // 说明行只讲语言，排在语言下拉框之下、背景效果之上；背景效果 Apply 后即时刷新，不需要重启。
    // 语言切换不会重绘已创建的控件：TR/TRU 在控件创建时求值一次。
    createStatic(IDC_STATIC_LANGUAGE_NOTE,
        TR("Component language: applies to newly opened component dialogs; menus and panel descriptions update after a restart.",
           "组件语言：对新打开的组件对话框生效；菜单与面板描述在重启后更新。"), 0);
    
    createStatic(IDC_STATIC_BACKDROP, TR("Default window backdrop", "默认窗口背景效果"), 0);
    HWND hComboBackdrop = createCombo(IDC_COMBO_BACKDROP);
    // 顺序与 BackdropEffect 枚举值一致。
    SendMessageW(hComboBackdrop, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(TR("None", "无")));
    SendMessageW(hComboBackdrop, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Mica"));
    SendMessageW(hComboBackdrop, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Mica Alt"));
    SendMessageW(hComboBackdrop, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Acrylic"));
    SendMessageW(hComboBackdrop, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Tabbed"));
    SendMessageW(hComboBackdrop, CB_SETCURSEL, static_cast<WPARAM>(draft().GetInt(overview::Backdrop)), 0);
    
    // ---- Tools ----
    createStatic(IDC_STATIC_DEV_SERVER, TR("Development server", "开发服务器"), 0);
    // 状态只读：开关与 URL 在 Developer 子页编辑，这里不进草稿。无边框、无 Tab 停靠，鼠标可选中复制。
    createReadOnlyEdit(IDC_STATIC_DEV_SERVER_STATUS, 0, 0);
    RefreshDevServerStatus();
    createButton(IDC_BTN_ADVANCED, TR("Advanced preferences...", "高级首选项..."), BS_PUSHBUTTON);
    createButton(IDC_BTN_SHOW_API_LIST, TR("API and services...", "API 与服务..."), BS_PUSHBUTTON);
    createStatic(IDC_STATIC_TOOLS_NOTE,
        TR("Window, performance and developer settings are on the sub-pages in the tree on the left; "
           "security exceptions are in Advanced Preferences under Tools > WebView2 UI.",
           "窗口、性能、开发者设置在左侧子页；安全例外在 高级首选项 > Tools > WebView2 UI。"), 0);
    
    // 模板列表与路径
    RefreshTemplateList(hwnd);
}

// ============================================
// 布局描述
// ============================================

// 三个分组的行序即 Tab 序，也是控件矩形数值表的输出顺序；铺排、滚动与重绘由基类完成。
void WebViewPreferencesInstance::LayoutContent(prefs_layout::PageLayoutBuilder& builder) {
    const int comboWindowHeight = builder.comboWindowHeight();
    
    // ---- Web template ----
    builder.BeginGroup(IDC_GROUP_TEMPLATE, {IDC_STATIC_TEMPLATE, IDC_STATIC_PATH});
    builder.LabelFieldButton(IDC_STATIC_TEMPLATE, IDC_COMBO_TEMPLATE, IDC_BTN_MANAGE, comboWindowHeight);
    builder.LabelFieldButton(IDC_STATIC_PATH, IDC_EDIT_PATH, IDC_BTN_OPEN_FOLDER);
    builder.EndGroup();
    
    // ---- Appearance ----
    builder.BeginGroup(IDC_GROUP_APPEARANCE, {IDC_STATIC_LANGUAGE, IDC_STATIC_BACKDROP});
    builder.LabelControl(IDC_STATIC_LANGUAGE, IDC_COMBO_LANGUAGE, comboWindowHeight);
    // 语言说明紧跟语言行，免得被读成背景效果的说明。
    builder.Note(IDC_STATIC_LANGUAGE_NOTE);
    builder.LabelControl(IDC_STATIC_BACKDROP, IDC_COMBO_BACKDROP, comboWindowHeight);
    builder.EndGroup();
    
    // ---- Tools ----
    builder.BeginGroup(IDC_GROUP_TOOLS, {IDC_STATIC_DEV_SERVER});
    // 无边框只读编辑框：文字与标签基线对齐，高度按一行文字。
    builder.LabelInlineText(IDC_STATIC_DEV_SERVER, IDC_STATIC_DEV_SERVER_STATUS);
    builder.Buttons({IDC_BTN_ADVANCED, IDC_BTN_SHOW_API_LIST});
    builder.Note(IDC_STATIC_TOOLS_NOTE);
    builder.EndGroup();
}

// ============================================
// 状态刷新
// ============================================

void WebViewPreferencesInstance::RefreshTemplateFolderText() {
    if (!hwnd()) return;
    std::wstring path = GetWebResourcesBaseDir();
    const std::string& templateName = draft().GetString(overview::Template);
    if (!templateName.empty()) {
        path += L"\\";
        path += pfc::stringcvt::string_wide_from_utf8(templateName.c_str()).get_ptr();
    }
    HWND hEdit = GetDlgItem(hwnd(), IDC_EDIT_PATH);
    if (hEdit) SetWindowTextW(hEdit, path.c_str());
    
    // Open... 只对存在的目录可用，不替用户建目录。
    HWND hOpen = GetDlgItem(hwnd(), IDC_BTN_OPEN_FOLDER);
    if (hOpen) {
        std::error_code ec;
        EnableWindow(hOpen, fs::is_directory(path, ec) ? TRUE : FALSE);
    }
}

void WebViewPreferencesInstance::RefreshDevServerStatus() {
    if (!hwnd()) return;
    HWND hStatus = GetDlgItem(hwnd(), IDC_STATIC_DEV_SERVER_STATUS);
    if (!hStatus) return;
    std::wstring text;
    // 开关与 URL 在每次加载前端时读取，改动在下一次加载页面时生效，不需要重启。
    if (UseDevServer()) {
        const std::string url = GetDevServerUrl();
        if (!url.empty()) {
            text = pfc::stringcvt::string_wide_from_utf8(url.c_str()).get_ptr();
            text += TR(" - enabled, applies on next page load", " - 已启用，下次加载页面时生效");
        } else {
            text = TR("Enabled, but no URL is set", "已启用但未填写 URL");
        }
    } else {
        text = TR("Off", "关闭");
    }
    SetWindowTextW(hStatus, text.c_str());
}

void WebViewPreferencesInstance::RefreshTemplateList(HWND hwnd) {
    HWND hCombo = GetDlgItem(hwnd, IDC_COMBO_TEMPLATE);
    if (!hCombo) return;
    
    SendMessageW(hCombo, CB_RESETCONTENT, 0, 0);
    
    std::vector<std::string> templates = GetTemplateList();
    for (const std::string& name : templates) {
        std::wstring wname = pfc::stringcvt::string_wide_from_utf8(name.c_str()).get_ptr();
        SendMessageW(hCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wname.c_str()));
    }
    
    // 选中项跟草稿走；草稿指向的模板不在磁盘上时由 SyncControlsFromDraft 补一条“（不存在）”。
    SyncControlsFromDraft();
}

namespace {

// 模板下拉框当前选中项对应的名字（UTF-8）。列表里的“（不存在）”标注项不是真名字，
// 剥掉后缀再返回；没有选中项时返回空串。
std::string SelectedTemplateName(HWND hwnd) {
    HWND hCombo = GetDlgItem(hwnd, IDC_COMBO_TEMPLATE);
    if (!hCombo) return {};
    const int sel = static_cast<int>(SendMessageW(hCombo, CB_GETCURSEL, 0, 0));
    if (sel < 0) return {};
    const int length = static_cast<int>(SendMessageW(hCombo, CB_GETLBTEXTLEN, sel, 0));
    if (length < 0) return {};
    std::wstring item(static_cast<size_t>(length) + 1, L'\0');
    SendMessageW(hCombo, CB_GETLBTEXT, sel, reinterpret_cast<LPARAM>(item.data()));
    item.resize(static_cast<size_t>(length));
    const std::wstring missingSuffix = TR(" (missing)", "（不存在）");
    if (item.size() > missingSuffix.size() && item.ends_with(missingSuffix)) {
        item.resize(item.size() - missingSuffix.size());
    }
    return pfc::stringcvt::string_utf8_from_wide(item.c_str()).get_ptr();
}

// 已加载面板里显式指定了该模板的个数。跟随全局模板的面板不在此列，它们由
// “活动模板不能改名 / 删除”那条保护覆盖。只看 WebViewContext 注册表里活着的实例，
// 不去翻未加载布局的存档。
size_t CountLoadedPanelsUsingTemplate(const std::string& name) {
    size_t count = 0;
    auto& context = WebViewContext::GetInstance();
    for (HWND hwnd : context.GetAllInstances()) {
        if (!IsWindow(hwnd)) continue;
        WebViewPanel* panel = context.GetPanelByHwnd(hwnd);
        if (!panel) continue;
        if (prefs_template::EqualsIgnoreCase(panel->GetPanelConfig().templateName, name)) ++count;
    }
    return count;
}

// 起名对话框的输入与结果。DialogBoxIndirectParamW 的 lParam 把它带进 WM_INITDIALOG，
// 之后挂在 DWLP_USER 上，确定时把通过校验的名字写回 accepted。
struct TemplateNamePrompt {
    const wchar_t* title = L"";
    std::wstring label;                 // 编辑框上方的一行说明
    std::wstring hint;                  // 编辑框下方的规则提示；校验失败时换成原因
    std::wstring value;                 // 初始文本
    std::vector<std::string> existing;  // 现有模板名，用于重名判定
    std::string accepted;               // 输出：通过校验的名字（UTF-8）
};

constexpr int IDC_PROMPT_LABEL = 1000;
constexpr int IDC_PROMPT_EDIT = 1001;
constexpr int IDC_PROMPT_HINT = 1002;

const wchar_t* TemplateNameErrorText(prefs_template::NameError error) {
    switch (error) {
    case prefs_template::NameError::Empty:
        return TR("Enter a name.", "请输入名称。");
    case prefs_template::NameError::InvalidChars:
        return TR("Only letters, numbers, hyphens and underscores are allowed.",
                  "只能使用字母、数字、连字符和下划线。");
    case prefs_template::NameError::Duplicate:
        return TR("A template with this name already exists (names are not case-sensitive).",
                  "已存在同名模板（名称不区分大小写）。");
    case prefs_template::NameError::None:
        break;
    }
    return L"";
}

void InitTemplateNameDialog(HWND hDlg, const TemplateNamePrompt& prompt) {
    SetWindowTextW(hDlg, prompt.title);
    SetDlgItemTextW(hDlg, IDC_PROMPT_LABEL, prompt.label.c_str());
    SetDlgItemTextW(hDlg, IDC_PROMPT_HINT, prompt.hint.c_str());
    // 按钮文案在此设置：DLGITEMTEMPLATE 的 titleArray 是定长内联数组，
    // 放不下更长的译文，故模板里只留 ASCII 兜底文本。
    SetDlgItemTextW(hDlg, IDOK, TR("OK", "确定"));
    SetDlgItemTextW(hDlg, IDCANCEL, TR("Cancel", "取消"));
    HWND hEdit = GetDlgItem(hDlg, IDC_PROMPT_EDIT);
    if (hEdit) {
        SetWindowTextW(hEdit, prompt.value.c_str());
        SendMessageW(hEdit, EM_SETSEL, 0, -1);
        SetFocus(hEdit);
    }
}

// 确定键：校验通过才关对话框；不通过就把原因写进提示行、焦点留在编辑框，不落盘。
void AcceptTemplateNameIfValid(HWND hDlg, TemplateNamePrompt& prompt) {
    HWND hEdit = GetDlgItem(hDlg, IDC_PROMPT_EDIT);
    const int length = hEdit ? GetWindowTextLengthW(hEdit) : 0;
    std::wstring text(static_cast<size_t>(length) + 1, L'\0');
    if (hEdit) GetWindowTextW(hEdit, text.data(), length + 1);
    text.resize(static_cast<size_t>(length));
    
    const std::string candidate =
        prefs_template::Trim(pfc::stringcvt::string_utf8_from_wide(text.c_str()).get_ptr());
    const prefs_template::NameError error = prefs_template::CheckName(candidate, prompt.existing);
    if (error != prefs_template::NameError::None) {
        SetDlgItemTextW(hDlg, IDC_PROMPT_HINT, TemplateNameErrorText(error));
        if (hEdit) {
            SendMessageW(hEdit, EM_SETSEL, 0, -1);
            SetFocus(hEdit);
        }
        return;
    }
    prompt.accepted = candidate;
    EndDialog(hDlg, IDOK);
}

INT_PTR CALLBACK TemplateNameDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* prompt = reinterpret_cast<TemplateNamePrompt*>(GetWindowLongPtrW(hDlg, DWLP_USER));
    switch (msg) {
    case WM_INITDIALOG:
        prompt = reinterpret_cast<TemplateNamePrompt*>(lParam);
        SetWindowLongPtrW(hDlg, DWLP_USER, static_cast<LONG_PTR>(lParam));
        InitTemplateNameDialog(hDlg, *prompt);
        return FALSE;  // 焦点已在 InitTemplateNameDialog 里交给编辑框
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK && prompt) {
            AcceptTemplateNameIfValid(hDlg, *prompt);
            return TRUE;
        }
        if (LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    case WM_CLOSE:
        EndDialog(hDlg, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}

// Win32 没有现成的“带输入框的消息框”：TaskDialog 不支持文本输入，
// 因此在内存中构建最小 DLGTEMPLATE。布局（对话框单位）：
//   [说明标签] / [编辑框] / [规则提示或错误原因] / [确定] [取消]
// DS_SETFONT 指定 MS Shell Dlg，系统会映射到当前 UI 字体，各控件尺寸随之按 DPI 换算。
struct alignas(DWORD) TemplateNameDialogTemplate {
    DLGTEMPLATE tmpl;
    WORD menuArray[1];      // 无菜单
    WORD classArray[1];     // 默认类
    WORD titleArray[1];     // 空标题，WM_INITDIALOG 再设
    WORD pointSize;
    WCHAR typeface[13];     // "MS Shell Dlg" + nul
    struct alignas(DWORD) {
        DLGITEMTEMPLATE item;
        WORD classArray[2]; // 0xFFFF, 0x0082 = STATIC
        WORD titleArray[1];
        WORD extraBytes;
    } label;
    struct alignas(DWORD) {
        DLGITEMTEMPLATE item;
        WORD classArray[2]; // 0xFFFF, 0x0081 = EDIT
        WORD titleArray[1];
        WORD extraBytes;
    } edit;
    struct alignas(DWORD) {
        DLGITEMTEMPLATE item;
        WORD classArray[2]; // 0xFFFF, 0x0082 = STATIC
        WORD titleArray[1];
        WORD extraBytes;
    } hint;
    struct alignas(DWORD) {
        DLGITEMTEMPLATE item;
        WORD classArray[2]; // 0xFFFF, 0x0080 = BUTTON
        WCHAR titleArray[3]; // "OK" + nul
        WORD extraBytes;
    } ok;
    struct alignas(DWORD) {
        DLGITEMTEMPLATE item;
        WORD classArray[2];
        WCHAR titleArray[7]; // "Cancel" + nul
        WORD extraBytes;
    } cancel;
};

TemplateNameDialogTemplate BuildTemplateNameDialogTemplate() {
    TemplateNameDialogTemplate dlg = {};
    dlg.tmpl.style = DS_MODALFRAME | DS_CENTER | DS_SETFONT | WS_POPUP | WS_CAPTION | WS_SYSMENU;
    dlg.tmpl.cdit = 5;
    dlg.tmpl.cx = 260; dlg.tmpl.cy = 96;
    dlg.pointSize = 8;
    wcscpy_s(dlg.typeface, L"MS Shell Dlg");
    
    dlg.label.item = {WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 7, 7, 246, 10, IDC_PROMPT_LABEL};
    dlg.label.classArray[0] = 0xFFFF; dlg.label.classArray[1] = 0x0082;
    
    dlg.edit.item = {WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, 0, 7, 20, 246, 14,
                     IDC_PROMPT_EDIT};
    dlg.edit.classArray[0] = 0xFFFF; dlg.edit.classArray[1] = 0x0081;
    
    // 提示行给三行高度：错误原因与“未加载布局不迁移”的说明在英文下也放得下。
    dlg.hint.item = {WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX, 0, 7, 38, 246, 30, IDC_PROMPT_HINT};
    dlg.hint.classArray[0] = 0xFFFF; dlg.hint.classArray[1] = 0x0082;
    
    dlg.ok.item = {WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP, 0, 149, 75, 50, 14, IDOK};
    dlg.ok.classArray[0] = 0xFFFF; dlg.ok.classArray[1] = 0x0080;
    wcscpy_s(dlg.ok.titleArray, L"OK");
    
    dlg.cancel.item = {WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP, 0, 203, 75, 50, 14, IDCANCEL};
    dlg.cancel.classArray[0] = 0xFFFF; dlg.cancel.classArray[1] = 0x0080;
    wcscpy_s(dlg.cancel.titleArray, L"Cancel");
    return dlg;
}

// Create 与 Rename 共用的起名对话框。返回 true 且 prompt.accepted 为通过校验的名字；
// 取消或关闭返回 false。模态期间会重入消息循环，调用方要自己持有实例引用。
bool PromptTemplateName(HWND owner, TemplateNamePrompt& prompt) {
    TemplateNameDialogTemplate dlg = BuildTemplateNameDialogTemplate();
    const INT_PTR result = DialogBoxIndirectParamW(nullptr, &dlg.tmpl, owner, TemplateNameDlgProc,
                                                   reinterpret_cast<LPARAM>(&prompt));
    return result == IDOK && !prompt.accepted.empty();
}

std::wstring Utf8ToWide(const std::string& text) {
    return pfc::stringcvt::string_wide_from_utf8(text.c_str()).get_ptr();
}

}  // namespace

void WebViewPreferencesInstance::OnTemplateSelectionChanged(HWND hwnd) {
    // 选中项只进草稿；持久化与导航都等 Apply。
    const std::string newTemplate = SelectedTemplateName(hwnd);
    if (newTemplate.empty() || newTemplate == draft().GetString(overview::Template)) return;
    draft().SetString(overview::Template, newTemplate);
    RefreshTemplateFolderText();
    UpdateState();
}

void WebViewPreferencesInstance::OnCreateTemplate(HWND hwnd) {
    // 模态期间宿主可能关掉本页并释放实例：先持有自引用，返回后再看窗口是否还在。
    service_ptr_t<WebViewPreferencesInstance> keepAlive(this);
    
    TemplateNamePrompt prompt;
    prompt.title = TR("Create Template", "新建模板");
    prompt.label = TR("Name for the new template:", "新模板的名称：");
    prompt.hint = TR("Letters, numbers, hyphens and underscores only. A folder with an index.html is created "
                     "under webview-ui.",
                     "只能使用字母、数字、连字符和下划线。将在 webview-ui 下创建带 index.html 的文件夹。");
    prompt.existing = GetTemplateList();
    if (!PromptTemplateName(hwnd, prompt) || !get_wnd()) return;
    
    const std::string& name = prompt.accepted;
    if (CreateTemplate(name)) {
        // 新模板只选入草稿：不激活、不导航，Apply 才写配置。
        draft().SetString(overview::Template, name);
        RefreshTemplateList(hwnd);
        UpdateState();
        console::printf("[WebView2 UI] Created template: %s", name.c_str());
        return;
    }
    
    // 失败：如实说明磁盘上留下了什么，不假装回滚；列表按磁盘现状重新枚举。
    std::wstring message = TR("Failed to create the template \"", "创建模板 \"");
    message += Utf8ToWide(name);
    message += TR("\".", "\" 失败。");
    if (TemplateExists(name)) {
        message += TR("\n\nThe folder was created but index.html could not be written. "
                      "The folder is left in place:\n",
                      "\n\n文件夹已创建，但 index.html 未能写入。文件夹保留在：\n");
        message += GetWebResourcesBaseDir() + L"\\" + Utf8ToWide(name);
    } else {
        message += TR("\n\nThe folder could not be created. Check the permissions of the webview-ui folder.",
                      "\n\n无法创建文件夹。请检查 webview-ui 文件夹的权限。");
    }
    RefreshTemplateList(hwnd);
    MessageBoxW(hwnd, message.c_str(), L"WebView2 UI", MB_OK | MB_ICONERROR);
}

void WebViewPreferencesInstance::OnRenameTemplate(HWND hwnd) {
    service_ptr_t<WebViewPreferencesInstance> keepAlive(this);
    
    const std::string oldName = SelectedTemplateName(hwnd);
    if (oldName.empty()) return;
    if (!TemplateExists(oldName)) {
        // 选中的是“（不存在）”标注项，或目录在本页打开后被外部删掉了。
        RefreshTemplateList(hwnd);
        MessageBoxW(hwnd, TR("This template folder does not exist, so it cannot be renamed.",
                             "该模板文件夹不存在，无法重命名。"),
            L"WebView2 UI", MB_OK | MB_ICONINFORMATION);
        return;
    }
    
    // 已提交的活动模板不能改名：配置里还指着旧名字，改了就等于把当前主界面指向一个不存在的目录。
    // 同时对照当前配置，本页打开期间被别处切换的活动模板也算。
    if (oldName == initial().GetString(overview::Template) ||
        oldName == ReadSnapshotFromConfig().GetString(overview::Template)) {
        MessageBoxW(hwnd,
            TR("This template is the active one. Switch to another template and apply first, then rename it.",
               "这是当前活动模板。请先切换到其他模板并应用，再重命名。"),
            L"WebView2 UI", MB_OK | MB_ICONINFORMATION);
        return;
    }
    
    // 已加载面板显式引用的模板也不能改名：面板配置里存的是名字，改了它就找不到目录。
    if (const size_t panels = CountLoadedPanelsUsingTemplate(oldName); panels > 0) {
        std::wstring message = std::to_wstring(panels);
        message += TR(" loaded panel(s) use this template explicitly. Change their template in the panel "
                      "settings first, then rename it.",
                      " 个已加载的面板显式使用此模板。请先在面板设置里改用其他模板，再重命名。");
        MessageBoxW(hwnd, message.c_str(), L"WebView2 UI", MB_OK | MB_ICONINFORMATION);
        return;
    }
    
    TemplateNamePrompt prompt;
    prompt.title = TR("Rename Template", "重命名模板");
    // 前后缀成对翻译：英文用 "name": 收尾，中文用括号包裹，避免任一语言出现括号不配对。
    prompt.label = TR("New name for \"", "新名称 (\"");
    prompt.label += Utf8ToWide(oldName);
    prompt.label += TR("\":", "\"):");
    prompt.hint = TR("Renames the folder immediately; Cancel on this page does not undo it. "
                     "Panels in layouts that are not loaded keep the old name.",
                     "文件夹会立即重命名，本页的取消不会撤销。未加载布局中的面板仍指向旧名称，不会被更新。");
    prompt.value = Utf8ToWide(oldName);
    prompt.existing = GetTemplateList();  // 含旧名本身：改回旧名或只改大小写都按重名拒绝
    if (!PromptTemplateName(hwnd, prompt) || !get_wnd()) return;
    
    const std::string& newName = prompt.accepted;
    if (!RenameTemplate(oldName, newName)) {
        // 对话框已排除非法名与重名，走到这里多半是目录被占用或权限不足；列表按磁盘现状重枚举。
        RefreshTemplateList(hwnd);
        std::wstring message = TR("Failed to rename the template folder \"", "重命名模板文件夹 \"");
        message += Utf8ToWide(oldName);
        message += TR("\".\n\nThe folder may be in use or you may not have permission. "
                      "Close programs that might be accessing the template files and try again.",
                      "\" 失败。\n\n文件夹可能正在被使用或没有权限。请关闭可能正在访问模板文件的程序后重试。");
        MessageBoxW(hwnd, message.c_str(), TR("Rename Failed", "重命名失败"), MB_OK | MB_ICONERROR);
        return;
    }
    
    // 草稿里若选的是旧名，跟着改成新名；其余字段的未提交编辑不变。
    if (draft().GetString(overview::Template) == oldName) draft().SetString(overview::Template, newName);
    RefreshTemplateList(hwnd);
    UpdateState();
    console::printf("[WebView2 UI] Renamed template: %s -> %s", oldName.c_str(), newName.c_str());
}

void WebViewPreferencesInstance::OnDeleteTemplate(HWND hwnd) {
    service_ptr_t<WebViewPreferencesInstance> keepAlive(this);
    
    const std::string templateName = SelectedTemplateName(hwnd);
    if (templateName.empty()) return;
    if (!TemplateExists(templateName)) {
        RefreshTemplateList(hwnd);
        MessageBoxW(hwnd, TR("This template folder does not exist; there is nothing to delete.",
                             "该模板文件夹不存在，没有可删除的内容。"),
            L"WebView2 UI", MB_OK | MB_ICONINFORMATION);
        return;
    }
    
    // 列表可以删空：页面会显示“default（不存在）”并拒绝 Apply，不为此保留最后一个。
    // 已提交的活动模板不能删：用户须先切换并应用，不能靠“先改配置再删除”绕过保护。
    // 同时对照当前配置，本页打开期间被别处切换的活动模板也算。
    if (templateName == initial().GetString(overview::Template) ||
        templateName == ReadSnapshotFromConfig().GetString(overview::Template)) {
        MessageBoxW(hwnd,
            TR("This template is the active one. Switch to another template and apply first, then delete it.",
               "这是当前活动模板。请先切换到其他模板并应用，再删除。"),
            L"WebView2 UI", MB_OK | MB_ICONINFORMATION);
        return;
    }
    
    // 已加载面板显式引用的模板不能删；未加载布局里的引用查不到，只在确认文案里说明。
    if (const size_t panels = CountLoadedPanelsUsingTemplate(templateName); panels > 0) {
        std::wstring message = std::to_wstring(panels);
        message += TR(" loaded panel(s) use this template explicitly. Change their template in the panel "
                      "settings first, then delete it.",
                      " 个已加载的面板显式使用此模板。请先在面板设置里改用其他模板，再删除。");
        MessageBoxW(hwnd, message.c_str(), L"WebView2 UI", MB_OK | MB_ICONINFORMATION);
        return;
    }
    
    // 构建确认消息：文件操作立即执行，外层 Cancel 不撤销。
    std::wstring msg = TR("Are you sure you want to delete template \"", "确定要删除模板 \"");
    msg += Utf8ToWide(templateName);
    msg += L"\"?";
    msg += TR("\n\nThe folder is deleted immediately; Cancel on this page does not undo it.\n"
              "Panels in layouts that are not loaded may still reference this name and will fall back to "
              "the missing-template behavior when opened.",
              "\n\n文件夹会立即删除；本页的取消不会撤销此操作。\n"
              "未加载布局中的面板可能仍引用此名称，重开时会按模板缺失处理。");
    
    if (MessageBoxW(hwnd, msg.c_str(), L"WebView2 UI", MB_YESNO | MB_ICONWARNING) != IDYES || !get_wnd())
        return;
    
    if (DeleteTemplate(templateName)) {
        // 删的是草稿选择项时，草稿回到已提交的活动模板；其他字段的未提交编辑不变。
        if (draft().GetString(overview::Template) == templateName) {
            draft().SetString(overview::Template, initial().GetString(overview::Template));
        }
        RefreshTemplateList(hwnd);
        UpdateState();
        console::printf("[WebView2 UI] Deleted template: %s", templateName.c_str());
        return;
    }
    
    // 删除失败：remove_all 可能只删了一部分，按磁盘现状重枚举并报告实际状态。
    RefreshTemplateList(hwnd);
    if (!TemplateExists(templateName)) {
        MessageBoxW(hwnd, TR("The template folder is already gone; the list has been refreshed.",
                             "模板文件夹已不存在，列表已刷新。"),
            L"WebView2 UI", MB_OK | MB_ICONINFORMATION);
        return;
    }
    std::wstring message = TR("Failed to delete the template \"", "删除模板 \"");
    message += Utf8ToWide(templateName);
    message += TR("\".\n\nSome files may have been removed already; the folder still exists at:\n",
                  "\" 失败。\n\n部分文件可能已被删除；文件夹仍在：\n");
    message += GetWebResourcesBaseDir() + L"\\" + Utf8ToWide(templateName);
    message += TR("\n\nThe folder may be in use or you may not have permission. "
                  "Close programs that might be accessing the template files and try again.",
                  "\n\n文件夹可能正在被使用或没有权限。请关闭可能正在访问模板文件的程序后重试。");
    MessageBoxW(hwnd, message.c_str(), L"WebView2 UI", MB_OK | MB_ICONERROR);
}

void WebViewPreferencesInstance::OnOpenTemplateFolder(HWND hwnd) const {
    std::wstring path = GetWebResourcesBaseDir();
    
    const std::string& templateName = draft().GetString(overview::Template);
    if (!templateName.empty()) {
        path += L"\\";
        path += pfc::stringcvt::string_wide_from_utf8(templateName.c_str()).get_ptr();
    }
    
    // 只打开存在的目录，不替用户建；按钮在目录缺失时已禁用，这里是兜底(fallback)。
    std::error_code ec;
    if (!fs::is_directory(path, ec)) {
        MessageBoxW(hwnd, TR("The template folder does not exist.", "模板文件夹不存在。"),
            L"WebView2 UI", MB_OK | MB_ICONINFORMATION);
        return;
    }
    
    static auto pShellExec = &::ShellExecuteW;
    if (pShellExec)
        pShellExec(nullptr, L"explore", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void WebViewPreferencesInstance::OnManageTemplates(HWND hwnd) {
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    // 选中的是“（不存在）”标注项或列表为空时，Rename / Delete 没有对象，置灰而不是点开再报错。
    const std::string selected = SelectedTemplateName(hwnd);
    const UINT targetFlags = (!selected.empty() && TemplateExists(selected)) ? MF_STRING : (MF_STRING | MF_GRAYED);
    AppendMenuW(menu, MF_STRING, IDM_TEMPLATE_CREATE, TR("Create...", "新建..."));
    AppendMenuW(menu, targetFlags, IDM_TEMPLATE_RENAME, TR("Rename...", "重命名..."));
    AppendMenuW(menu, targetFlags, IDM_TEMPLATE_DELETE, TR("Delete...", "删除..."));
    
    RECT rc{};
    HWND hButton = GetDlgItem(hwnd, IDC_BTN_MANAGE);
    if (hButton) GetWindowRect(hButton, &rc);
    
    // 弹出菜单会重入消息循环：期间宿主可能关掉本页并释放实例，
    // 先持有一份自引用，返回后再看窗口是否还在。
    service_ptr_t<WebViewPreferencesInstance> keepAlive(this);
    const UINT cmd = static_cast<UINT>(TrackPopupMenuEx(menu,
        TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD | TPM_NONOTIFY,
        rc.left, rc.bottom, hwnd, nullptr));
    DestroyMenu(menu);
    if (!cmd || !get_wnd()) return;
    SendMessageW(get_wnd(), WM_COMMAND, MAKEWPARAM(cmd, 0), 0);
}

void WebViewPreferencesInstance::OnOpenAdvancedPreferences() {
    // 目标是宿主的 Advanced 页，不是本组件的 advconfig 分支 GUID；
    // 宿主不保证自动展开分支，控件旁的说明给出 Tools > WebView2 UI 位置。
    try {
        static_api_ptr_t<ui_control>()->show_preferences(preferences_page::guid_advanced);
    } catch (...) {
        console::printf("[WebView2 UI] Failed to open Advanced Preferences");
    }
}

// ============================================
// API List Dialog
// ============================================

namespace {

constexpr DWORD DWMWA_USE_IMMERSIVE_DARK_MODE_V = 20;
constexpr int IDC_API_LIST_EDIT = 1001;

// 每窗口状态：字体与背景画刷统一在 WM_DESTROY 释放
struct ApiListState {
    HFONT monoFont = nullptr;
    HBRUSH bkBrush = nullptr;
    COLORREF bkColor = 0;
    COLORREF textColor = 0;
    bool isDark = false;
};

bool IsFb2kDarkMode() {
    auto api = ui_config_manager::tryGet();
    return api.is_valid() && api->is_dark_mode();
}

std::wstring WideFromUtf8(const char* s) {
    return pfc::stringcvt::string_wide_from_utf8(s ? s : "").get_ptr();
}

std::wstring GuidToWide(const GUID& guid) {
    return WideFromUtf8(GuidUtils::GuidToString(guid).c_str());
}

// 按当前 fb2k 深浅色重建配色，并同步标题栏与滚动条主题
void ApplyApiListTheme(HWND hwnd, ApiListState* state) {
    if (!state) return;

    state->isDark = IsFb2kDarkMode();
    // 本对话框不经 CCoreDarkModeHooks，深色用固定的深灰底与浅灰字自绘；浅色沿用系统 Window 配色
    state->bkColor = state->isDark ? RGB(32, 32, 32) : GetSysColor(COLOR_WINDOW);
    state->textColor = state->isDark ? RGB(222, 222, 222) : GetSysColor(COLOR_WINDOWTEXT);

    if (state->bkBrush) DeleteObject(state->bkBrush);
    state->bkBrush = CreateSolidBrush(state->bkColor);

    BOOL darkTitleBar = state->isDark ? TRUE : FALSE;
    ::DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE_V,
        &darkTitleBar, sizeof(darkTitleBar));

    // EDIT 的非客户区滚动条只能靠 UxTheme 切换，WM_CTLCOLOR 管不到
    HWND hEdit = GetDlgItem(hwnd, IDC_API_LIST_EDIT);
    if (hEdit) {
        SetWindowTheme(hEdit, state->isDark ? L"DarkMode_Explorer" : nullptr, nullptr);
    }

    InvalidateRect(hwnd, nullptr, TRUE);
    if (hEdit) InvalidateRect(hEdit, nullptr, TRUE);
}

// mainmenu_commands_v2 组件（ESLyric 等）只注册一个父槽位，真正的子命令由
// dynamic_instantiate() 在运行时构建。不展开这棵树，枚举结果里只剩下不可执行
// 的容器 GUID。语义与 DiscoveryApi::CollectDynamicMenuNodes 保持一致。
constexpr int kMaxDynamicMenuDepth = 16;

void AppendDynamicMenuNodes(const mainmenu_node::ptr& node,
                            const std::wstring& pathPrefix,
                            const std::wstring& ownerGuid,
                            std::wstring& out,
                            int& count,
                            int depth) {
    if (!node.is_valid() || depth > kMaxDynamicMenuDepth) return;

    t_uint32 type = mainmenu_node::type_separator;
    try { type = node->get_type(); } catch (...) { return; }
    if (type == mainmenu_node::type_separator) return;

    pfc::string8 display;
    t_uint32 flags = 0;
    try { node->get_display(display, flags); } catch (...) {
        // 标签取不到时继续遍历子树，不整棵丢弃
    }

    std::wstring label = WideFromUtf8(display.get_ptr());
    std::wstring path = pathPrefix;
    // 部分组件把动态子树根节点的标签写成与静态父槽位同名，再拼一次会得到
    // "桌面歌词/桌面歌词/显示" 这样的重复路径
    const bool duplicatesOwnerLabel = (depth == 0 && label == pathPrefix);
    if (!label.empty() && !duplicatesOwnerLabel) {
        if (!path.empty()) path += L'/';
        path += label;
    }

    if (type == mainmenu_node::type_group) {
        t_size childCount = 0;
        try { childCount = node->get_children_count(); } catch (...) { return; }
        for (t_size i = 0; i < childCount; i++) {
            mainmenu_node::ptr child;
            try { child = node->get_child(i); } catch (...) { continue; }
            AppendDynamicMenuNodes(child, path, ownerGuid, out, count, depth + 1);
        }
        return;
    }

    GUID subGuid = pfc::guid_null;
    try { subGuid = node->get_guid(); } catch (...) {}
    if (subGuid == pfc::guid_null) return;  // 无法定址，无法执行

    out += L"    { path: \"" + path + L"\", guid: \"" + ownerGuid +
           L"\", subGuid: \"" + GuidToWide(subGuid) + L"\" },\r\n";
    count++;
}

} // anonymous namespace

// API 列表对话框窗口过程
static LRESULT CALLBACK ApiListWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<ApiListState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_CREATE:
        {
            RECT rc;
            GetClientRect(hwnd, &rc);

            state = new ApiListState();
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));

            // 创建只读编辑框
            HWND hEdit = CreateWindowExW(
                0,
                L"EDIT",
                L"",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | 
                ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_AUTOHSCROLL,
                0, 0, rc.right, rc.bottom,
                hwnd,
                reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_API_LIST_EDIT)),
                (HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE),
                nullptr);
            
            // 列表按等宽字体对齐各列；首选 Cascadia Code，其次 Consolas。
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
            
            // GetObjectW 读回的是创建时请求的 LOGFONT，不是实际映射到的字体：字体缺失时 CreateFontW
            // 仍返回句柄并由 GDI 代换，这个分支只在 CreateFontW 本身失败时才切到 Consolas。
            LOGFONTW lf = {};
            if (!GetObjectW(hMonoFont, sizeof(lf), &lf) || wcscmp(lf.lfFaceName, L"Cascadia Code") != 0) {
                DeleteObject(hMonoFont);
                hMonoFont = CreateFontW(
                    -16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
            }
            SendMessageW(hEdit, WM_SETFONT, (WPARAM)hMonoFont, TRUE);
            state->monoFont = hMonoFont;

            ApplyApiListTheme(hwnd, state);
            
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
            text += L"//\r\n";
            text += L"// GUIDs are compile-time constants inside each component, so they are\r\n";
            text += L"// identical on English and localized foobar2000 installs. Only the 'name'\r\n";
            text += L"// field follows the UI language - never match on it across machines.\r\n";
            text += L"//\r\n";
            text += L"// Entries with dynamic: true are parent slots created by\r\n";
            text += L"// mainmenu_commands_v2 (ESLyric and most SMP-era components). The parent\r\n";
            text += L"// GUID is only a container and is NOT executable on its own - see the\r\n";
            text += L"// dynamicMenuCommands list below for the executable children.\r\n";
            text += L"//------------------------------------------------------------------------------\r\n\r\n";
            
            {
                service_enum_t<mainmenu_commands> e;
                service_ptr_t<mainmenu_commands> ptr;
                int cmdCount = 0;
                int dynamicParentCount = 0;
                // 动态子命令单独成表，避免与静态命令混在一起被误当作可直接执行
                std::wstring dynamicText;
                int dynamicCount = 0;

                text += L"const externalMenuCommands = [\r\n";
                while (e.next(ptr)) {
                    t_uint32 count = 0;
                    try { count = ptr->get_command_count(); } catch (...) { continue; }

                    service_ptr_t<mainmenu_commands_v2> v2;
                    const bool hasV2 = ptr->service_query_t(v2);

                    for (t_uint32 i = 0; i < count; i++) {
                        pfc::string8 name;
                        GUID cmdGuid = pfc::guid_null;
                        try { ptr->get_name(i, name); } catch (...) {}
                        try { cmdGuid = ptr->get_command(i); } catch (...) {}

                        std::wstring wname = WideFromUtf8(name.get_ptr());
                        std::wstring wguid = GuidToWide(cmdGuid);

                        bool isDynamic = false;
                        if (hasV2) {
                            try { isDynamic = v2->is_command_dynamic(i); } catch (...) {}
                        }

                        text += L"    { name: \"" + wname + L"\", guid: \"" + wguid + L"\"";
                        if (isDynamic) text += L", dynamic: true";
                        text += L" },\r\n";
                        cmdCount++;

                        if (!isDynamic) continue;
                        dynamicParentCount++;

                        mainmenu_node::ptr root;
                        try { root = v2->dynamic_instantiate(i); } catch (...) { continue; }
                        if (!root.is_valid()) continue;

                        AppendDynamicMenuNodes(root, wname, wguid, dynamicText, dynamicCount, 0);
                    }
                }
                text += L"];  // " + std::to_wstring(cmdCount) + L" commands, " +
                        std::to_wstring(dynamicParentCount) + L" dynamic parent slots\r\n\r\n";

                text += L"//------------------------------------------------------------------------------\r\n";
                text += L"// SECTION 2b: Dynamic Submenu Commands (expanded at runtime)\r\n";
                text += L"// Use: discovery.executeMainMenuCommand({ guid: '...', subGuid: '...' })\r\n";
                text += L"//\r\n";
                text += L"// These are the executable leaves of the dynamic: true parents above.\r\n";
                text += L"// Both guid and subGuid are required. subGuid may change between\r\n";
                text += L"// component versions, so resolve it at runtime via\r\n";
                text += L"// discovery.getMainMenuCommands() instead of hardcoding it.\r\n";
                text += L"//------------------------------------------------------------------------------\r\n\r\n";
                text += L"const dynamicMenuCommands = [\r\n";
                text += dynamicText;
                text += L"];  // " + std::to_wstring(dynamicCount) + L" dynamic commands\r\n\r\n";
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
            text += L"// Discovery - execute a static main menu command:\r\n";
            text += L"// window.fb2k.invoke('discovery.executeMainMenuCommand', { guid: '{...}' })\r\n\r\n";
            text += L"// Discovery - execute a dynamic child command (ESLyric etc.):\r\n";
            text += L"// Both guid and subGuid are required; the parent slot alone is not executable.\r\n";
            text += L"// window.fb2k.invoke('discovery.executeMainMenuCommand', { guid: '{...}', subGuid: '{...}' })\r\n\r\n";
            text += L"// Resolve commands at runtime instead of hardcoding GUIDs from this dump.\r\n";
            text += L"// GUIDs are locale-independent, but they change across plugin versions:\r\n";
            text += L"// const { commands } = await fb2k.invoke('discovery.getMainMenuCommands', {});\r\n";
            text += L"// const cmd = commands.find(c => c.isDynamic && /lyric/i.test(c.path));\r\n";
            text += L"// await fb2k.invoke('discovery.executeMainMenuCommand', { guid: cmd.guid, subGuid: cmd.subGuid });\r\n\r\n";
            text += L"// Discovery - execute external plugin context menu command:\r\n";
            text += L"// window.fb2k.invoke('discovery.executeContextMenuCommand', { guid: '{...}' })\r\n";
            text += L"// Note: Context menu commands operate on currently playing or selected tracks\r\n\r\n";
            text += L"// Get all available context menu commands:\r\n";
            text += L"// window.fb2k.invoke('discovery.getContextMenuCommands')\r\n";
            
            SetWindowTextW(hEdit, text.c_str());
            return 0;
        }
        
    case WM_SIZE:
        {
            HWND hEdit = GetDlgItem(hwnd, IDC_API_LIST_EDIT);
            if (hEdit) {
                RECT rc;
                GetClientRect(hwnd, &rc);
                MoveWindow(hEdit, 0, 0, rc.right, rc.bottom, TRUE);
            }
            return 0;
        }
        
    // 系统主题切换时（浅色 <-> 深色）重新应用配色
    case WM_SETTINGCHANGE:
        if (state && lParam &&
            _wcsicmp(reinterpret_cast<const wchar_t*>(lParam), L"ImmersiveColorSet") == 0) {
            ApplyApiListTheme(hwnd, state);
        }
        break;
        
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
        if (state && state->bkBrush) {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, state->textColor);
            SetBkColor(hdc, state->bkColor);
            return (LRESULT)state->bkBrush;
        }
        break;
        
    case WM_ERASEBKGND:
        if (state && state->bkBrush) {
            HDC hdc = (HDC)wParam;
            RECT rc;
            GetClientRect(hwnd, &rc);
            FillRect(hdc, &rc, state->bkBrush);
            return TRUE;
        }
        break;
        
    case WM_DESTROY:
        {
            if (state) {
                if (state->monoFont) DeleteObject(state->monoFont);
                if (state->bkBrush) DeleteObject(state->bkBrush);
                delete state;
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
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
        // 背景由 WM_ERASEBKGND 按当前深浅色自绘，避免深色模式下闪白
        wc.hbrBackground = nullptr;
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
