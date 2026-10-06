// ============================================
// PreferencesPage.cpp - WebView2 UI Preferences Page Implementation
// Preferences → Display → WebView2 UI
// ============================================

#include "pch.h"
#include "prefs/PreferencesPage.h"
#include "prefs/PreferencesPageInternal.h"
#include "prefs/PreferencesFields.h"
#include "prefs/PreferencesZoom.h"
#include "prefs/PreferencesCdpPort.h"
#include "webview/WebViewHost.h"
#include "settings/AdvancedConfig.h"
#include "settings/SecurityConfig.h"
#include "ui/UserInterface.h"
#include "ui/BackgroundService.h"
#include "core/WebViewContext.h"
#include "core/WebViewPanel.h"
#include "panels/PanelConfig.h"
#include "window/MainWindow.h"
#include "window/WindowManager.h"
#include <CommCtrl.h>
#include <filesystem>
#include <cstring>
#include <fstream>
#include <algorithm>
#include <utility>
#include "utils/I18n.h"

namespace fs = std::filesystem;

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

// ============================================
// 注册 Preferences Page
// ============================================

static preferences_page_factory_t<WebViewPreferencesPage> g_prefs_page_factory;

} // namespace webview_prefs
