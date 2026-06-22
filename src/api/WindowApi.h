#pragma once
#include "pch.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// ============================================
// Window API Registration
// ============================================
//
// Registered APIs (87):
//   -- window.* (81) --
//   window.minimize
//   window.maximize
//   window.restore
//   window.close
//   window.setPosition
//   window.getPosition
//   window.setSize
//   window.getSize
//   window.center
//   window.setAlwaysOnTop
//   window.isAlwaysOnTop
//   window.setFullscreen
//   window.isFullscreen
//   window.getState
//   window.setBounds
//   window.getBounds
//   window.startDrag
//   window.setMinSize
//   window.setMaxSize
//   window.setResizable
//   window.isResizable
//   window.setTitle
//   window.getTitle
//   window.showPopup
//   window.closePopup
//   window.listPopups
//   window.postMessage
//   window.sendMessage
//   window.broadcastMessage
//   window.getWindowId
//   window.isMainWindow
//   window.isPopupWindow
//   window.flash
//   window.flashTaskbar
//   window.clearFlash
//   window.setIcon
//   window.setProgressBar
//   window.clearProgressBar
//   window.getDesktopSize
//   window.bringToFront
//   window.setForeground
//   window.setClickThrough
//   window.isClickThrough
//   window.enableResize
//   window.disableResize
//   window.getMiniWindowState
//   window.setMiniWindowState
//   window.setShape
//   window.clearShape
//   window.setDwmEffect
//   window.getDwmEffect
//   window.clearDwmEffect
//   window.setBackdropColor
//   window.getBackdropColor
//   window.getDPI
//   window.setOpacity
//   window.getOpacity
//   window.setSkipTaskbar
//   window.isSkipTaskbar
//   window.showTrayIcon
//   window.hideTrayIcon
//   window.setTrayTooltip
//   window.isTrayVisible
//   window.showTrayBalloon
//   window.removeTray
//   window.registerHotkey
//   window.unregisterHotkey
//   window.listHotkeys
//   window.clearHotkeys
//   window.getMonitorInfo
//   window.getAllMonitors
//   window.openDevTools
//   window.setTransparentBackground
//   window.enableDragDrop
//   window.disableDragDrop
//   window.isDragDropEnabled
//   window.captureScreenshot
//   window.saveScreenshot
//   window.print
//   window.printToPdf
//   window.setSnapping
//   window.isSnapping
//   -- cross-namespace (6) --
//   ui.showContextMenu
//   system.getTheme
//   system.getDPI
//   system.getLocale
//   panel.getConfig
//   panel.setConfig
//

// Register all window-related APIs with BridgeCore
/** @brief Register the window.* API handlers and window:* events. */
void RegisterWindowApi();

// ============================================
// Fullscreen State Query (供 MainWindow 使用)
// ============================================

// 查询当前是否处于全屏状态
bool IsWindowFullscreen();

// 若指定窗口处于全屏，则同步退出全屏并返回 true；否则返回 false。
// 用途：MainWindow / PopupWindow / API handler 在 maximize / restore / 双击标题栏 /
// WM_SYSCOMMAND 等场景下，参考 Electron / Tauri 行为，将这些操作路由到先退出全屏。
// 同时支持 main 与 popup（通过 WindowTargetResolver::ResolveByCallerHwnd 解析 shell）。
bool ExitFullscreenIfActive(HWND hwnd);
