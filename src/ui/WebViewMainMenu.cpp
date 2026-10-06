// ============================================
// WebViewMainMenu.cpp - View 菜单下的「WebView2 UI」分组与显示/隐藏窗口命令
// ============================================

#include "pch.h"
#include "ui/BackgroundService.h"
#include "ui/UserInterface.h"
#include "window/MainWindow.h"
#include "prefs/PreferencesPage.h"
#include "core/WebViewContext.h"
#include "utils/I18n.h"

// ============================================
// Main menu command to toggle background window
// ============================================
static const GUID guid_mainmenu_group_webview = 
    { 0xc8d9e0f1, 0x2a3b, 0x4c5d, { 0x6e, 0x7f, 0x80, 0x91, 0xa2, 0xb3, 0xc4, 0xd5 } };

static const GUID guid_cmd_toggle_window = 
    { 0xc8d9e0f2, 0x2a3b, 0x4c5d, { 0x6e, 0x7f, 0x80, 0x91, 0xa2, 0xb3, 0xc4, 0xd6 } };

static mainmenu_group_popup_factory g_mainmenu_group(
    guid_mainmenu_group_webview,
    mainmenu_groups::view,
    mainmenu_commands::sort_priority_dontcare,
    "WebView2 UI"
);

class MainMenuCommands : public mainmenu_commands {
public:
    t_uint32 get_command_count() override {
        return 1;
    }
    
    GUID get_command(t_uint32 index) override {
        switch (index) {
            case 0: return guid_cmd_toggle_window;
            default: return pfc::guid_null;
        }
    }
    
    void get_name(t_uint32 index, pfc::string_base& out) override {
        switch (index) {
            case 0: out = TRU("Show/Hide Window", "显示/隐藏窗口"); break;
            default: break;
        }
    }
    
    bool get_description(t_uint32 index, pfc::string_base& out) override {
        switch (index) {
            case 0: 
                out = TRU("Toggle visibility of WebView2 UI window", "切换 WebView2 UI 窗口的可见性");
                return true;
            default:
                return false;
        }
    }
    
    GUID get_parent() override {
        return guid_mainmenu_group_webview;
    }
    
    void execute(t_uint32 index, service_ptr_t<service_base> callback) override {
        switch (index) {
            case 0:
                // Check if WebView2 UI is active as main interface
                if (WebViewUI::GetInstance()) {
                    WebViewUI::GetInstance()->activate();
                } else if (background_service::HasBackgroundWindow()) {
                    background_service::ToggleWindow();
                } else if (WebViewContext::GetInstance().GetInstanceCount() > 0) {
                    // 面板模式 - 没有独立窗口可切换
                    if (background_service::IsBackgroundModeEnabled()) {
                        // 后台模式已启用但窗口尚未创建，重新初始化
                        background_service::Initialize();
                        if (background_service::HasBackgroundWindow()) {
                            background_service::ShowWindow();
                        } else {
                            // 初始化失败，打开偏好设置页
                            ui_control::get()->show_preferences(webview_prefs::GetPreferencesPageGuid());
                        }
                    } else {
                        // 后台模式未启用，直接打开偏好设置页
                        ui_control::get()->show_preferences(webview_prefs::GetPreferencesPageGuid());
                    }
                } else {
                    // 无窗口也无面板，打开偏好设置页
                    ui_control::get()->show_preferences(webview_prefs::GetPreferencesPageGuid());
                }
                break;
            default: break;
        }
    }
    
    bool get_display(t_uint32 index, pfc::string_base& text, t_uint32& flags) override {
        flags = 0;
        
        if (index == 0) {
            if (WebViewUI::GetInstance()) {
                // 独立 UI 模式
                text = TRU("Show/Hide Window", "显示/隐藏窗口");
                if (WebViewUI::GetInstance()->is_visible()) flags |= flag_checked;
            } else if (background_service::HasBackgroundWindow()) {
                // 后台窗口模式
                text = TRU("Show/Hide Window", "显示/隐藏窗口");
                HWND hwnd = background_service::GetBackgroundWindow()->GetHwnd();
                if (hwnd && IsWindowVisible(hwnd)) flags |= flag_checked;
            } else {
                // 面板模式或无实例 - 显示为偏好设置入口
                text = TRU("Preferences...", "偏好设置...");
            }
        }
        
        return true;
    }
};

static mainmenu_commands_factory_t<MainMenuCommands> g_mainmenu_commands_factory;
