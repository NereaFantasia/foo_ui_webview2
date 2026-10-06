#pragma once

// ============================================
// PreferencesPerformancePage.h - WebView2 UI › Performance 子页
// Preferences → Display → WebView2 UI → Performance
//
// 三项：启动时预热 WebView2 环境（启动期读取，改动后重启生效）、隐藏时深度挂起
//（每次可见性策略计算时读取，Apply 即生效，只对主窗口路径起作用）、默认内容缩放
//（整数百分比下拉框；Apply 后写到所有未被主题 window.setZoom 覆盖过的 WebView，新建的
// WebView 在 controller 就绪时自己读）。深度挂起经 security_config 读写，其余两项经 webview_prefs。
// ============================================

#include <string>
#include <vector>

#include "prefs/PreferencesPageBase.h"

namespace webview_prefs {

class WebViewPrefsPerformancePage : public preferences_page_v3 {
public:
    const char* get_name() override;
    GUID get_guid() override;
    GUID get_parent_guid() override;
    double get_sort_priority() override;
    bool get_help_url(pfc::string_base& p_out) override;
    preferences_page_instance::ptr instantiate(fb2k::hwnd_t parent, preferences_page_callback::ptr callback) override;
};

class WebViewPrefsPerformanceInstance : public PreferencesPageBase {
public:
    WebViewPrefsPerformanceInstance(HWND parent, preferences_page_callback::ptr callback);

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
    // 本进程启动时的预热开关取值：只有本页写它，首个实例读到的值就是启动值。
    bool startupPreheat_ = true;
};

} // namespace webview_prefs
