// CursorApi.cpp — Cursor visibility control for Visual Hosting mode
//
// 详细背景见 CursorApi.h 顶部注释。本文件实现:
//   cursor.setHidden  显式设置客户区光标隐藏/显示, 立即生效。
//   cursor.isHidden   查询当前隐藏状态。
//
// 实现要点:
//   - 通过 _callerHwnd 路由到调用窗口对应的 WebViewHost (多窗口/多面板友好)
//   - 标志位写入 WebViewHost::SetCursorHidden, 后续所有 WM_SETCURSOR 拦截
//     都会读取此标志决定 SetCursor(nullptr) 还是 SetCursor(currentCursor_)
//   - 不持有 BridgeCore 实例, 也不广播事件 (光标状态属于单窗口内部 UI 状态)

#include "pch.h"
#include "api/CursorApi.h"
#include "api/BridgeCore.h"
#include "api/CallerContext.h"
#include "core/WebViewContext.h"
#include "webview/WebViewHost.h"

namespace {
    using json = nlohmann::json;

    // 解析 _callerHwnd 到对应的 WebViewHost。
    // 返回 nullptr 时调用方应回退到错误响应。
    WebViewHost* ResolveHostFromParams(const json& params) {
        if (!params.contains("_callerHwnd")) {
            return nullptr;
        }
        auto hwnd = reinterpret_cast<HWND>(params["_callerHwnd"].get<intptr_t>());
        if (!hwnd || !IsWindow(hwnd)) {
            return nullptr;
        }
        return WebViewContext::GetInstance().GetHostByHwnd(hwnd);
    }

    // ---------------------------------------------------------------
    // cursor.setHidden — 设置客户区光标隐藏/显示
    //
    // 状态实际发生变化时,向调用窗口路由 cursor:hiddenChanged 事件。
    // 同窗口内多个组件可监听此事件协同 (典型用法: 引用计数式隐藏请求)。
    // ---------------------------------------------------------------
    json CursorSetHidden(const json& params) {
        if (!params.contains("hidden") || !params["hidden"].is_boolean()) {
            return {{"success", false},
                    {"error", "hidden (boolean) is required"}};
        }
        WebViewHost* host = ResolveHostFromParams(params);
        if (!host) {
            return {{"success", false},
                    {"error", "caller window not found"}};
        }
        const bool hidden = params["hidden"].get<bool>();
        const bool changed = host->SetCursorHidden(hidden);
        if (changed) {
            auto caller = CallerContext::FromParams(params);
            caller.EmitEvent("cursor:hiddenChanged", {{"hidden", hidden}});
        }
        return {{"success", true}, {"changed", changed}};
    }

    // ---------------------------------------------------------------
    // cursor.isHidden — 查询当前隐藏状态
    // ---------------------------------------------------------------
    json CursorIsHidden(const json& params) {
        WebViewHost* host = ResolveHostFromParams(params);
        if (!host) {
            return {{"hidden", false}};
        }
        return {{"hidden", host->IsCursorHidden()}};
    }
}  // namespace

void RegisterCursorApi() {
    auto& bridge = BridgeCore::GetInstance();
    bridge.RegisterApi("cursor.setHidden", CursorSetHidden);
    bridge.RegisterApi("cursor.isHidden", CursorIsHidden);
    LOG("Cursor API registered (2 APIs)");
}
