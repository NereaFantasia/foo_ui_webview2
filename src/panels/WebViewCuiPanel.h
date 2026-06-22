/**
 * WebViewCuiPanel - CUI panel implementation
 * 
 * Provides WebView2 panel support for Columns UI.
 * Implements uie::window interface.
 */

#pragma once
#include "pch.h"
#include "core/WebViewPanel.h"
#include "panels/PanelConfig.h"
#include "ui_extension.h"
#include "utils/I18n.h"

// ============================================
// GUID definition
// ============================================

// {A1B2C3D4-5E6F-7A8B-9C0D-1E2F3A4B5C6D}
static constexpr GUID g_webview_cui_panel_guid = 
    { 0xa1b2c3d4, 0x5e6f, 0x7a8b, { 0x9c, 0x0d, 0x1e, 0x2f, 0x3a, 0x4b, 0x5c, 0x6d } };

// ============================================
// WebViewCuiPanel - CUI panel implementation
// ============================================

class WebViewCuiPanel : public uie::window, public WebViewPanel {
public:
    WebViewCuiPanel();
    ~WebViewCuiPanel();
    
    // ========== uie::extension_base interface ==========
    
    const GUID& get_extension_guid() const override { return g_webview_cui_panel_guid; }
    void get_name(pfc::string_base& out) const override { out = "WebView2 Panel"; }
    void get_category(pfc::string_base& out) const override { out = "Panels"; }
    
    // ========== uie::window interface ==========
    
    unsigned get_type() const override { return uie::type_panel; }
    bool is_available(const uie::window_host_ptr& p_host) const override { return true; }
    
    HWND create_or_transfer_window(
        HWND wnd_parent, 
        const uie::window_host_ptr& p_host,
        const ui_helpers::window_position_t& p_position) override;
    
    void destroy_window() override;
    HWND get_wnd() const override { return m_hostWnd; }
    
    // ========== 面板配置接口 ==========
    
    bool have_config_popup() const override { return true; }
    bool show_config_popup(HWND wnd_parent) override;
    void get_config(stream_writer* p_writer, abort_callback& p_abort) const override;
    void set_config(stream_reader* p_reader, t_size p_size, abort_callback& p_abort) override;
    
    bool get_description(pfc::string_base& out) const override {
        out = TRU("WebView2-based panel for custom web interfaces in Columns UI.",
                  "Columns UI 中用于自定义 Web 界面的 WebView2 面板。");
        return true;
    }
    
protected:
    // ========== WebViewPanel virtual overrides ==========
    
    void OnWebViewReady() override;
    
private:
    static constexpr wchar_t CLASS_NAME[] = L"WebViewCuiPanel";
    
    static bool RegisterWindowClass();
    static bool s_classRegistered;
    
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    
    uie::window_host_ptr m_host;
    HWND m_hostWnd = nullptr;
    // 注意：面板配置使用基类 WebViewPanel::panelConfig_
};
