#pragma once

// ============================================
// PreferencesDeveloperPage.h - WebView2 UI › Developer 子页
// Preferences → Display → WebView2 UI → Developer
//
// 三组：开发者工具 (F12)、开发服务器（开关 + URL 输入框）、CDP 远程调试（开关 + 端口输入框）。
// 开发服务器两项 Apply 后重载跟随全局模板的前端即生效；开发者工具、CDP 开关与端口在启动期 /
// controller 创建期读取，改动后要求重启。存储经 security_config::* 与 webview_prefs::* 读写；
// 五项都只在本页编辑，Advanced 里不再有对应条目。
// ============================================

#include <string>
#include <vector>

#include "prefs/PreferencesPageBase.h"

namespace webview_prefs {

class WebViewPrefsDeveloperPage : public preferences_page_v3 {
public:
    const char* get_name() override;
    GUID get_guid() override;
    GUID get_parent_guid() override;
    double get_sort_priority() override;
    bool get_help_url(pfc::string_base& p_out) override;
    preferences_page_instance::ptr instantiate(fb2k::hwnd_t parent, preferences_page_callback::ptr callback) override;
};

class WebViewPrefsDeveloperInstance : public PreferencesPageBase {
public:
    WebViewPrefsDeveloperInstance(HWND parent, preferences_page_callback::ptr callback);

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
    // 输入框回写草稿时会再收到 EN_CHANGE；置位期间忽略，免得把刚同步的文字当成编辑。
    bool syncingControls_ = false;

    // 本进程启动时影响重启的三个字段的取值：只有本页写它们，首个实例读到的值就是启动值。
    bool startupDevTools_ = false;
    bool startupCdpRemote_ = false;
    int startupCdpPort_ = 0;
};

} // namespace webview_prefs
