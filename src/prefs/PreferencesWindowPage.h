#pragma once

// ============================================
// PreferencesWindowPage.h - WebView2 UI › Window 子页
// Preferences → Display → WebView2 UI → Window
//
// 七个开关分三组：窗口（记住位置、后台模式、启动时恢复后台窗口可见状态）、
// 托盘（最小化 / 关闭到托盘）、任务栏（缩略图播放按钮、播放进度）。
// 后台模式与缩略图按钮在启动期读取，改动后要求重启；其余 Apply 即写入并立即生效。
// ============================================

#include <string>
#include <vector>

#include "prefs/PreferencesPageBase.h"

namespace webview_prefs {

class WebViewPrefsWindowPage : public preferences_page_v3 {
public:
    const char* get_name() override;
    GUID get_guid() override;
    GUID get_parent_guid() override;
    double get_sort_priority() override;
    bool get_help_url(pfc::string_base& p_out) override;
    preferences_page_instance::ptr instantiate(fb2k::hwnd_t parent, preferences_page_callback::ptr callback) override;
};

class WebViewPrefsWindowInstance : public PreferencesPageBase {
public:
    WebViewPrefsWindowInstance(HWND parent, preferences_page_callback::ptr callback);

protected:
    // PreferencesPageBase
    prefs_draft::Snapshot ReadSnapshotFromConfig() const override;
    prefs_draft::Snapshot StartupSnapshot() const override;
    bool WriteField(size_t field, const prefs_draft::Snapshot& value) override;
    void CreateControls(HWND hwnd) override;
    void LayoutContent(prefs_layout::PageLayoutBuilder& builder) override;
    void SyncControlsFromDraft() override;
    std::wstring FieldDisplayName(size_t field) const override;
    void AfterApply(const std::vector<size_t>& changed) override;
    INT_PTR OnMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override;

private:
    // 「启动时恢复后台窗口可见状态」只在草稿里勾了后台模式时可编辑；说明行随之切换。
    void RefreshRestoreVisibilityState();
    // 托盘图标由主题通过 tray API 创建；没有图标时两个托盘开关禁用并说明原因，值保留。
    void RefreshTrayState();

    // 本进程启动时影响重启的两个字段的取值：只有本页写它们，首个实例读到的值就是启动值。
    bool startupBackgroundMode_ = false;
    bool startupTaskbarButtons_ = true;
};

} // namespace webview_prefs
