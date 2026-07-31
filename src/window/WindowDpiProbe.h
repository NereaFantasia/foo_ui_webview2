#pragma once

// ============================================
// WindowDpiProbe.h - 窗口创建前的 DPI 探测
//
// 两个 shell 都需要在 `CreateWindowExW` **之前**知道 DPI：
//   - MainWindow::RestoreWindowPosition 要用 DPI 把 DIP 约束换算成物理像素，
//     才能与「保存的物理尺寸」比较；
//   - PopupWindow::Create 要用 DPI 把 wire 传来的物理约束换算成 DIP 存储。
//
// 此时尚无 HWND，`GetDpiForWindow` 不可用，故只能从「目标矩形落在哪个
// 显示器」推导。本头文件把该逻辑集中一处，避免两个 shell 各自复制一份
// shcore 动态解析代码（那会成为漂移源）。
// ============================================

#include "pch.h"

namespace window_dpi_probe {

// 推导给定屏幕矩形所在显示器的有效 DPI。
//
// 取不到时返回 window_geometry::kBaselineDpi（96），等价于「不缩放」——
// 即本探测引入之前的既有行为，故失败是安全退化而非错误。
//
// 注意：`GetDpiForMonitor` 属 shcore.dll（Win8.1+），项目其余 DPI 调用走
// user32 的 `GetDpiForWindow`，故此处动态解析。
int GetDpiForScreenRect(int x, int y, int width, int height);

}  // namespace window_dpi_probe
