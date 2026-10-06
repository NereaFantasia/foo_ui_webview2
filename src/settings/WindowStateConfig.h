#pragma once

// ============================================
// 主窗口位置与后台窗口可见性的持久化
// ============================================
// 这些函数在 settings/WindowStateConfig.cpp 中定义。

namespace window_config {
    void GetWindowPosition(int& x, int& y, int& width, int& height, bool& maximized);
    void SetWindowPosition(int x, int y, int width, int height, bool maximized);
    bool HasSavedPosition();

    // 后台窗口上次关闭时的可见性，DUI 菜单 Show/Hide Window 持久化用
    bool IsBackgroundWindowVisible();
    void SetBackgroundWindowVisible(bool visible);
}
