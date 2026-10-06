#pragma once
#include "pch.h"

// Forward declaration
class MainWindow;

// ============================================
// WebView2 UI - user_interface implementation
// ============================================
class WebViewUI : public user_interface {
public:
    // GUID - must be unique for each component
    static const GUID guid;
    
    // === user_interface interface implementation ===
    const char* get_name() override { return "Webview2 UI"; }
    fb2k::hwnd_t init(HookProc_t hook) override;
    void shutdown() override;
    void activate() override;
    void hide() override;
    bool is_visible() override;
    GUID get_guid() override { return guid; }
    void override_statusbar_text(const char* p_text) override { /* Not used — WebView2 manages its own status display */ }
    void revert_statusbar_text() override { /* Not used — WebView2 manages its own status display */ }
    void show_now_playing() override { /* Not used — now-playing handled via Bridge events */ }
    
    // === Public methods ===
    
    // Get singleton instance
    static WebViewUI* GetInstance() { return s_instance; }
    
    // Get main window
    MainWindow* GetMainWindow() const { return mainWindow_.get(); }

private:
    static WebViewUI* s_instance;
    
    fb2k::hwnd_t mainHwnd_ = nullptr;
    HookProc_t hookProc_ = nullptr;
    std::unique_ptr<MainWindow> mainWindow_;
};
