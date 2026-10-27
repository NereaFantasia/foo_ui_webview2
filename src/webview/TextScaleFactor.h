// TextScaleFactor.h - Windows「文本大小」设置（设置 → 辅助功能 → 文本大小）。
//
// 宿主把它与窗口 DPI 一起写进每个 WebView 的 RasterizationScale，页面整体随之放大，
// 与 WinUI 的 WebView2 控件、Chromium 的做法一致。只在主线程调用。
#pragma once

namespace text_scale {

// 当前的文本大小倍数，1 到 2.25；系统不提供时为 1。首次调用时开始监听设置变化，
// 变化后让所有已登记的 WebView 重设 RasterizationScale。
double Current();

}  // namespace text_scale
