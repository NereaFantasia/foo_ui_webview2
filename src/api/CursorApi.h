#pragma once

#include "BridgeCore.h"

// ============================================
// CursorApi — explicit client-area cursor control (CompositionController mode)
//
// 背景:
//   Visual Hosting (CompositionController) 模式下 WebView2 不拥有任何 HWND,
//   所有 WM_SETCURSOR 由父窗口处理。Chromium 的 CursorChanged 事件不能反映
//   CSS `cursor: none` 状态变化 (实测仅在 hover 元素的 cursor 类型切换时触发),
//   因此前端无法仅靠 CSS 完成光标隐藏。
//
//   本 API 提供前端到 C++ 的显式通知通道, 用于全屏播放器 idle 隐藏等场景。
//
// 注册的方法:
//   cursor.setHidden  { hidden: boolean } -> { success: boolean, changed: boolean }
//   cursor.isHidden   {} -> { hidden: boolean }
//
// 发射的事件:
//   cursor:hiddenChanged  { hidden: boolean }
//     仅在 setHidden 真正改变状态时触发, 用于多组件协同
//     (典型实现: 前端用引用计数追踪多个独立的隐藏请求源)。
// ============================================
/** @brief Register the cursor.* API handlers. */
void RegisterCursorApi();
