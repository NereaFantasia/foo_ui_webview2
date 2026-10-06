#pragma once

// ============================================
// PreferencesPage.h - WebView2 UI Preferences Page
// Preferences → Display → WebView2 UI
// ============================================

#include <string>
#include <vector>
#include <functional>
#include "core/PreferencesPageBase.h"

// Forward declarations
class preferences_page_callback;

namespace webview_prefs {

// ============================================
// 配置变量访问接口
// ============================================

// Web 资源目录 (存储在 profile 目录下)
std::wstring GetWebResourcesBaseDir();
std::wstring GetActiveWebResourcesDir();
std::wstring GetActiveTemplateName();
void SetActiveTemplateName(const std::string& name);

// 模板管理
std::vector<std::string> GetTemplateList();
bool CreateTemplate(const std::string& name);
bool RenameTemplate(const std::string& oldName, const std::string& newName);
bool DeleteTemplate(const std::string& name);
bool TemplateExists(const std::string& name);

// 窗口设置
bool GetStartWithFoobar();
void SetStartWithFoobar(bool value);

bool GetRememberWindowPosition();
void SetRememberWindowPosition(bool value);

// 无 UI 入口、无运行时消费者；配置键按 DESIGN D5 保留：不展示、不重置、不删。
bool GetAutoHideWithFoobar();
void SetAutoHideWithFoobar(bool value);

// 托盘与任务栏（Window 子页）。偏好是启动初值与 Apply 时的即时写入；
// tray.* / taskbar.* API 的运行期调用只覆盖本进程，不写回这里。
bool GetMinimizeToTrayPreference();
void SetMinimizeToTrayPreference(bool value);
bool GetCloseToTrayPreference();
void SetCloseToTrayPreference(bool value);
// 关闭时 TaskbarIntegration 不再安装默认的缩略图播放按钮；主题用 taskbar.setButtons 装的按钮不受影响。
bool GetTaskbarButtonsEnabled();
void SetTaskbarButtonsEnabled(bool value);
// 开启时按播放状态驱动任务栏按钮上的进度条；主题调 taskbar.setProgress 后由主题接管到下次状态变化。
bool GetTaskbarProgressEnabled();
void SetTaskbarProgressEnabled(bool value);

// 性能（Performance 子页）。
// 预热：initquit::on_init 是否在启动时创建 WebView2 环境；关闭后首个窗口创建时按需创建。改动后下次启动生效。
bool GetPreheatEnabled();
void SetPreheatEnabled(bool value);
// 默认内容缩放，整数百分比（50–200，步进 25；100 = 不叠加在系统 DPI 之上）。读出时已钳到范围内。
int GetDefaultZoomPercent();
void SetDefaultZoomPercent(int percent);
// 把当前默认缩放应用到所有还没被主题 window.setZoom 覆盖过的 WebView；新建的 WebView 在 controller 就绪时自己读。
void ApplyDefaultZoomToFollowingHosts();

// DWM 背景效果
enum class BackdropEffect {
    None = 0,
    Mica = 1,
    MicaAlt = 2,
    Acrylic = 3,
    Tabbed = 4
};

BackdropEffect GetBackdropEffect();
void SetBackdropEffect(BackdropEffect effect);

// 偏好设置页 GUID（供 BackgroundService 等外部模块用）
const GUID& GetPreferencesPageGuid();

// 开发者选项（Developer 子页）
bool GetDevToolsEnabled();
// CDP 远程调试端口（Developer 子页），1024–65535，默认 9222；读出时越界值已退回默认。
// 环境创建时拼进 --remote-debugging-port，改动后重启生效。
int GetCdpPort();
void SetCdpPort(int port);
// 安全例外，写入口在 Advanced Preferences（Tools > WebView2 UI），这里只读取。
bool GetLocalNetworkAllowed();
bool GetInsecureHttpAllowed();
bool GetInsecureTlsAllowed();

// 开发服务器开关或 URL 改变后，让主窗口、后台窗口与跟随全局模板的面板按当前配置重新加载前端；
// 显式指定模板或 URL 的面板与弹出窗口不动。导航是异步的，返回不代表加载完成。
void ReloadFrontendsForDevServerChange();

// ============================================
// Preferences 页面类
// ============================================

class WebViewPreferencesPage : public preferences_page_v3 {
public:
    // preferences_page interface
    const char* get_name() override;
    GUID get_guid() override;
    GUID get_parent_guid() override;
    double get_sort_priority() override;
    bool get_help_url(pfc::string_base& p_out) override;
    
    // preferences_page_v3 interface
    preferences_page_instance::ptr instantiate(
        fb2k::hwnd_t parent, 
        preferences_page_callback::ptr callback) override;
};

// ============================================
// Preferences 页面实例类（总览页）
// ============================================

// 容器、字体、布局、滚动与草稿提交都在 PreferencesPageBase；本类只有总览页自己的
// 字段读写、控件、布局描述、模板命令与 Apply 之后的运行时刷新。
class WebViewPreferencesInstance : public PreferencesPageBase {
public:
    WebViewPreferencesInstance(HWND parent, preferences_page_callback::ptr callback);

protected:
    // PreferencesPageBase
    prefs_draft::Snapshot ReadSnapshotFromConfig() const override;
    prefs_draft::Snapshot StartupSnapshot() const override;
    bool WriteField(size_t field, const prefs_draft::Snapshot& value) override;
    void CreateControls(HWND hwnd) override;
    void LayoutContent(prefs_layout::PageLayoutBuilder& builder) override;
    void SyncControlsFromDraft() override;
    std::wstring FieldDisplayName(size_t field) const override;
    std::wstring ValidationMessage(const prefs_draft::ValidationResult& result) const override;
    void AfterApply(const std::vector<size_t>& changed) override;
    INT_PTR OnMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override;

private:
    // 模板可用：目录存在且含非空 index.html。作为字段表里模板字段的额外校验。
    static bool TemplateIsUsable(const std::string& name);

    // UI 状态刷新
    void RefreshTemplateList(HWND hwnd);
    void RefreshTemplateFolderText();
    void RefreshDevServerStatus();
    void OnTemplateSelectionChanged(HWND hwnd);

    // Apply 写成之后的运行时刷新
    void ReloadFrontendsForTemplateChange();
    void RefreshChromeForBackdropChange();

    // 模板操作
    void OnManageTemplates(HWND hwnd);
    void OnCreateTemplate(HWND hwnd);
    void OnRenameTemplate(HWND hwnd);
    void OnDeleteTemplate(HWND hwnd);
    void OnOpenTemplateFolder(HWND hwnd) const;

    // 工具
    void OnOpenAdvancedPreferences();
    void OnShowApiList(HWND hwnd);

    // 本进程启动时的语言意图：语言覆盖只由本页写入，首个实例创建时读到的值即启动值
    int startupLanguage_ = 0;
};

} // namespace webview_prefs
