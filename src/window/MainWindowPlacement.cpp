// MainWindowPlacement.cpp — 主窗口位置与尺寸的保存、恢复，以及按初始 DPI 播种默认尺寸约束
#include "pch.h"
#include "window/MainWindow.h"
#include "window/WindowDpiProbe.h"
#include "window/WindowGeometryMath.h"
#include "prefs/PreferencesPage.h"
#include "settings/WindowStateConfig.h"

// ============================================
// 窗口位置记忆功能
// ============================================

void MainWindow::SaveWindowPosition() {
    // 检查是否启用了记住窗口位置
    if (!webview_prefs::GetRememberWindowPosition()) {
        LOG("SaveWindowPosition: Disabled by user setting");
        return;
    }
    
    if (!hwnd_ || !IsWindow(hwnd_)) {
        return;
    }
    
    // 获取窗口位置信息（包含正常状态下的位置，即使窗口最大化）
    WINDOWPLACEMENT wp = { sizeof(WINDOWPLACEMENT) };
    if (!GetWindowPlacement(hwnd_, &wp)) {
        LOG("SaveWindowPosition: GetWindowPlacement failed");
        return;
    }
    
    // 使用 rcNormalPosition 获取非最大化状态的窗口位置
    // 这确保恢复时使用正确的正常尺寸
    int x = wp.rcNormalPosition.left;
    int y = wp.rcNormalPosition.top;
    int width = wp.rcNormalPosition.right - wp.rcNormalPosition.left;
    int height = wp.rcNormalPosition.bottom - wp.rcNormalPosition.top;
    // 最小化时 WM_SIZE(SIZE_MINIMIZED) 已把 isMaximized_ 置假、showCmd 变成
    // SW_SHOWMINIMIZED，最大化事实只剩 WPF_RESTORETOMAXIMIZED 记着。托盘隐藏现在
    // 会先最小化（见 HideWindowToTray），从托盘退出是常规路径，不读这一位会让
    // 最大化用户每次从托盘退出后回到普通尺寸。
    const bool iconic = IsIconic(hwnd_) != FALSE || wp.showCmd == SW_SHOWMINIMIZED;
    const bool restoresToMaximized = iconic && (wp.flags & WPF_RESTORETOMAXIMIZED) != 0;
    bool maximized = (wp.showCmd == SW_SHOWMAXIMIZED) || isMaximized_ || restoresToMaximized;
    
    window_config::SetWindowPosition(x, y, width, height, maximized);
    
    LOG("SaveWindowPosition: x=", x, " y=", y, " w=", width, " h=", height, " max=", maximized);
}

int MainWindow::GetDpiForSavedRect(int x, int y, int width, int height) {
    // 委托给共享探测器（两个 shell 共用，避免各自复制 shcore 解析逻辑）。
    return window_dpi_probe::GetDpiForScreenRect(x, y, width, height);
}

void MainWindow::SeedDefaultConstraintsFromInitialDpi() {
    // 历史默认下限的口径是**物理像素**（见 kDefaultMinWidthPhysical 说明）。
    // 约束存储改为 DIP 后，若把那两个字面量直接当 DIP，125% 缩放下实际下限
    // 会从 400 物理放大到 500 物理。此处按初始显示器 DPI 反向换算，使
    // WM_GETMINMAXINFO 再换算回物理时得到与改动前一致的值。
    //
    // 只迁移**默认值**：用户显式设过的值会经 SetMinSizeDip 覆盖，那条路径
    // 的 wire 单位换算已在 handler 层完成，与此无关。
    //
    // 初始 DPI 的取法：窗口尚未创建，故用「即将落到哪个显示器」推导——
    // 有保存位置时用该矩形，否则用主显示器原点。这与 RestoreWindowPosition
    // 随后对保存尺寸所用的 DPI 口径一致。
    int probeX = 0;
    int probeY = 0;
    int probeW = 1;
    int probeH = 1;

    if (webview_prefs::GetRememberWindowPosition() && window_config::HasSavedPosition()) {
        int savedX = 0, savedY = 0, savedW = 0, savedH = 0;
        bool savedMaximized = false;
        window_config::GetWindowPosition(savedX, savedY, savedW, savedH, savedMaximized);
        probeX = savedX;
        probeY = savedY;
        probeW = savedW;
        probeH = savedH;
    }

    const int initialDpi = window_dpi_probe::GetDpiForScreenRect(probeX, probeY, probeW, probeH);

    minWidthDip_ = window_geometry::PhysicalToDip(kDefaultMinWidthPhysical, initialDpi);
    minHeightDip_ = window_geometry::PhysicalToDip(kDefaultMinHeightPhysical, initialDpi);

    LOG("SeedDefaultConstraintsFromInitialDpi: dpi=", initialDpi,
        " min=", minWidthDip_, "x", minHeightDip_, " DIP",
        " (physical baseline ", kDefaultMinWidthPhysical, "x", kDefaultMinHeightPhysical, ")");
}

void MainWindow::RestoreWindowPosition(int& x, int& y, int& width, int& height, int& showCmd) {
    // 默认值
    width = 1280;
    height = 720;
    
    // 先居中
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    x = (screenWidth - width) / 2;
    y = (screenHeight - height) / 2;
    showCmd = SW_SHOW;
    
    // 检查是否启用了记住窗口位置
    if (!webview_prefs::GetRememberWindowPosition()) {
        LOG("RestoreWindowPosition: Disabled by user setting, using defaults");
        return;
    }
    
    // 检查是否有保存的位置
    if (!window_config::HasSavedPosition()) {
        LOG("RestoreWindowPosition: no saved position, using defaults");
        return;
    }
    
    bool maximized = false;
    window_config::GetWindowPosition(x, y, width, height, maximized);
    
    // 验证窗口尺寸是否合理。
    //
    // 保存值是**物理像素**（来自 GetWindowRect），而约束字段是 DIP，
    // 故须先把约束换算到物理空间再比较——直接比较会在非 100% 缩放下
    // 用错量纲。
    //
    // 此函数在 Create() 内、CreateWindowExW **之前**被调用，此时尚无
    // hwnd_ 可供 GetDpiForWindow，故用保存的窗口矩形所在显示器的 DPI。
    // 取不到时退回基准 DPI（等价于不缩放，即旧行为）。
    const int restoreDpi = GetDpiForSavedRect(x, y, width, height);
    const auto clamped = window_geometry::ClampSize(
        width, height,
        window_geometry::DipToPhysical(minWidthDip_, restoreDpi),
        window_geometry::DipToPhysical(minHeightDip_, restoreDpi),
        window_geometry::DipToPhysical(maxWidthDip_, restoreDpi),
        window_geometry::DipToPhysical(maxHeightDip_, restoreDpi));
    width = clamped.width;
    height = clamped.height;
    
    // 验证窗口位置是否在任一显示器内
    // 使用 MonitorFromRect 检查窗口是否可见
    RECT windowRect = { x, y, x + width, y + height };
    HMONITOR hMonitor = MonitorFromRect(&windowRect, MONITOR_DEFAULTTONULL);
    
    if (!hMonitor) {
        // 窗口不在任何显示器上，重新居中到主显示器
        LOG("RestoreWindowPosition: saved position off-screen, re-centering");
        x = (screenWidth - width) / 2;
        y = (screenHeight - height) / 2;
    }
    
    // 设置显示命令
    showCmd = maximized ? SW_SHOWMAXIMIZED : SW_SHOW;
    
    LOG("RestoreWindowPosition: x=", x, " y=", y, " w=", width, " h=", height, " showCmd=", showCmd);
}
