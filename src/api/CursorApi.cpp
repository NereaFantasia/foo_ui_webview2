// CursorApi.cpp — Cursor visibility control for Visual Hosting mode
//
// 详细背景见 CursorApi.h 顶部注释。本文件实现:
//   cursor.setHidden  显式设置客户区光标隐藏/显示, 立即生效。
//   cursor.isHidden   查询当前隐藏状态。
//
// 实现要点:
//   - 通过 CallerContext 路由到调用窗口对应的 WebViewHost (多窗口/多面板友好)
//   - 标志位写入 WebViewHost::SetCursorHidden, 后续所有 WM_SETCURSOR 拦截
//     都会读取此标志决定 SetCursor(nullptr) 还是 SetCursor(currentCursor_)
//   - 不持有 BridgeCore 实例, 也不广播事件 (光标状态属于单窗口内部 UI 状态)
// 形状由 src/api/schema/cursor.ts 声明，结构体与参数解析来自生成的 CursorSchema.h。

#include "pch.h"
#include "api/CursorApi.h"
#include "api/BridgeCore.h"
#include "api/CallerContext.h"
#include "api/EventEmit.h"
#include "api/TypedApi.h"
#include "api/generated/CursorSchema.h"
#include "core/WebViewContext.h"
#include "webview/WebViewHost.h"

namespace {
    namespace cursor = api::cursor;

    // 解析调用方窗口到对应的 WebViewHost。返回 nullptr 时调用方应回退到错误响应。
    WebViewHost* ResolveHost(const CallerContext& caller) {
        if (!caller.callerHwnd || !IsWindow(caller.callerHwnd)) {
            return nullptr;
        }
        return WebViewContext::GetInstance().GetHostByHwnd(caller.callerHwnd);
    }

    // ---------------------------------------------------------------
    // cursor.setHidden — 设置客户区光标隐藏/显示
    //
    // 状态实际发生变化时,向调用窗口路由 cursor:hiddenChanged 事件。
    // 同窗口内多个组件可监听此事件协同 (典型用法: 引用计数式隐藏请求)。
    // ---------------------------------------------------------------
    api::Result<cursor::SetHiddenResult> CursorSetHidden(const cursor::SetHiddenParams& p, const CallerContext& caller) {
        WebViewHost* host = ResolveHost(caller);
        if (!host) {
            return api::Fail("caller window not found", ApiErrorCode::OPERATION_FAILED);
        }
        cursor::SetHiddenResult result;
        result.changed = host->SetCursorHidden(p.hidden);
        if (result.changed) {
            cursor::HiddenChangedPayload payload;
            payload.hidden = p.hidden;
            api::emit::EmitTo<cursor::events::HiddenChanged>(caller, payload);
        }
        return result;
    }

    // ---------------------------------------------------------------
    // cursor.isHidden — 查询当前隐藏状态
    // ---------------------------------------------------------------
    api::Result<cursor::IsHiddenResult> CursorIsHidden(const cursor::IsHiddenParams&, const CallerContext& caller) {
        WebViewHost* host = ResolveHost(caller);
        cursor::IsHiddenResult result;
        result.hidden = host ? host->IsCursorHidden() : false;
        return result;
    }
}  // namespace

void RegisterCursorApi() {
    api::RegisterApi("cursor.setHidden", CursorSetHidden);
    api::RegisterApi("cursor.isHidden", CursorIsHidden);
    LOG("Cursor API registered (2 APIs)");
}
