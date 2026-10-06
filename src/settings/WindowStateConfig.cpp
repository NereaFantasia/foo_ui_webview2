#include "pch.h"
#include "settings/WindowStateConfig.h"

// ============================================
// Window Position Configuration
// ============================================

// GUIDs for window position storage
// {8F7E3A21-5B4C-4D2E-9A1F-6C8D7E9F0A1B}
static constexpr GUID guid_cfg_window_x = 
    { 0x8f7e3a21, 0x5b4c, 0x4d2e, { 0x9a, 0x1f, 0x6c, 0x8d, 0x7e, 0x9f, 0x0a, 0x1b } };
// {8F7E3A22-5B4C-4D2E-9A1F-6C8D7E9F0A1C}
static constexpr GUID guid_cfg_window_y = 
    { 0x8f7e3a22, 0x5b4c, 0x4d2e, { 0x9a, 0x1f, 0x6c, 0x8d, 0x7e, 0x9f, 0x0a, 0x1c } };
// {8F7E3A23-5B4C-4D2E-9A1F-6C8D7E9F0A1D}
static constexpr GUID guid_cfg_window_width = 
    { 0x8f7e3a23, 0x5b4c, 0x4d2e, { 0x9a, 0x1f, 0x6c, 0x8d, 0x7e, 0x9f, 0x0a, 0x1d } };
// {8F7E3A24-5B4C-4D2E-9A1F-6C8D7E9F0A1E}
static constexpr GUID guid_cfg_window_height = 
    { 0x8f7e3a24, 0x5b4c, 0x4d2e, { 0x9a, 0x1f, 0x6c, 0x8d, 0x7e, 0x9f, 0x0a, 0x1e } };
// {8F7E3A25-5B4C-4D2E-9A1F-6C8D7E9F0A1F}
static constexpr GUID guid_cfg_window_maximized = 
    { 0x8f7e3a25, 0x5b4c, 0x4d2e, { 0x9a, 0x1f, 0x6c, 0x8d, 0x7e, 0x9f, 0x0a, 0x1f } };
// {8F7E3A26-5B4C-4D2E-9A1F-6C8D7E9F0A20}
// DUI 后台窗口上次关闭时的可见性（菜单 Show/Hide Window 持久化）
static constexpr GUID guid_cfg_background_window_visible =
    { 0x8f7e3a26, 0x5b4c, 0x4d2e, { 0x9a, 0x1f, 0x6c, 0x8d, 0x7e, 0x9f, 0x0a, 0x20 } };

// Configuration variables for window position (using foobar2000 configStore)
// Default values: centered 1280x720
cfg_var_modern::cfg_int cfg_window_x(guid_cfg_window_x, INT_MIN);  // INT_MIN = not set
cfg_var_modern::cfg_int cfg_window_y(guid_cfg_window_y, INT_MIN);
cfg_var_modern::cfg_int cfg_window_width(guid_cfg_window_width, 1280);
cfg_var_modern::cfg_int cfg_window_height(guid_cfg_window_height, 720);
cfg_var_modern::cfg_bool cfg_window_maximized(guid_cfg_window_maximized, false);
// 默认 true：首次安装/未 toggle 过的用户保持旧行为（启动后显示后台窗口）
cfg_var_modern::cfg_bool cfg_background_window_visible(guid_cfg_background_window_visible, true);

// Export getters/setters for MainWindow to use
namespace window_config {
    void GetWindowPosition(int& x, int& y, int& width, int& height, bool& maximized) {
        x = static_cast<int>(cfg_window_x.get());
        y = static_cast<int>(cfg_window_y.get());
        width = static_cast<int>(cfg_window_width.get());
        height = static_cast<int>(cfg_window_height.get());
        maximized = cfg_window_maximized.get();
        
        // 调试日志
        console::printf("[WindowConfig] GetWindowPosition: x=%d, y=%d, w=%d, h=%d, max=%d",
            x, y, width, height, maximized ? 1 : 0);
    }
    
    void SetWindowPosition(int x, int y, int width, int height, bool maximized) {
        console::printf("[WindowConfig] SetWindowPosition: x=%d, y=%d, w=%d, h=%d, max=%d",
            x, y, width, height, maximized ? 1 : 0);
        
        cfg_window_x.set(x);
        cfg_window_y.set(y);
        cfg_window_width.set(width);
        cfg_window_height.set(height);
        cfg_window_maximized.set(maximized);
    }
    
    bool HasSavedPosition() {
        bool hasSaved = cfg_window_x.get() != INT_MIN;
        console::printf("[WindowConfig] HasSavedPosition: %s (x=%d)",
            hasSaved ? "true" : "false", static_cast<int>(cfg_window_x.get()));
        return hasSaved;
    }

    // 后台窗口上次关闭时的可见性，DUI 菜单 Show/Hide Window 持久化用
    bool IsBackgroundWindowVisible() {
        return cfg_background_window_visible.get();
    }

    void SetBackgroundWindowVisible(bool visible) {
        cfg_background_window_visible.set(visible);
        console::printf("[WindowConfig] SetBackgroundWindowVisible: %s",
            visible ? "true" : "false");
    }
}
