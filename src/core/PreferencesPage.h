#pragma once

// ============================================
// PreferencesPage.h - WebView2 UI Preferences Page
// Preferences → Display → WebView2 UI
// ============================================

#include <string>
#include <vector>
#include <functional>
#include <foobar2000/SDK/coreDarkMode.h>

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

bool GetAutoHideWithFoobar();
void SetAutoHideWithFoobar(bool value);

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

// 开发者选项
bool GetDevToolsEnabled();
bool GetLocalNetworkAllowed();
bool GetInsecureHttpAllowed();
bool GetInsecureTlsAllowed();

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
// Preferences 页面实例类
// ============================================

class WebViewPreferencesInstance : public preferences_page_instance {
public:
    WebViewPreferencesInstance(HWND parent, preferences_page_callback::ptr callback);
    ~WebViewPreferencesInstance();
    
    // preferences_page_instance interface
    t_uint32 get_state() override;
    fb2k::hwnd_t get_wnd() override;
    void apply() override;
    void reset() override;
    
private:
    // Dialog procedure
    static INT_PTR CALLBACK DialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    INT_PTR HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    
    // UI 更新
    void InitializeControls(HWND hwnd);
    void RefreshTemplateList(HWND hwnd);
    void UpdateState();
    void OnTemplateSelectionChanged(HWND hwnd);
    
    // 模板操作
    void OnCreateTemplate(HWND hwnd);
    void OnRenameTemplate(HWND hwnd);
    void OnDeleteTemplate(HWND hwnd);
    void OnOpenTemplateFolder(HWND hwnd);
    
    // API 列表查看
    void OnShowApiList(HWND hwnd);
    
    HWND hwnd_ = nullptr;
    HFONT hFont_ = nullptr;
    preferences_page_callback::ptr callback_;
    
    // 深色模式支持
    fb2k::CCoreDarkModeHooks darkMode_;
    
    // 当前设置值（用于 UI 同步，设置更改时会立即保存到 cfg_var）
    std::string pendingTemplate_;
    bool pendingStartWithFoobar_ = false;
    bool pendingRememberPosition_ = true;
    bool pendingAutoHide_ = false;
    BackdropEffect pendingBackdrop_ = BackdropEffect::Mica;
    bool hasChanges_ = false;  // 跟踪是否有未应用的更改
};

} // namespace webview_prefs
