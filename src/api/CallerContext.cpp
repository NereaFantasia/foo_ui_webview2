#include "pch.h"
#include "api/CallerContext.h"
#include "api/BridgeCore.h"
#include "core/WebViewContext.h"

CallerContext CallerContext::FromParams(const json& params) {
    CallerContext ctx;

    if (!params.contains("_callerHwnd")) {
        ctx.bridge = &BridgeCore::GetInstance();
        return ctx;
    }

    auto hwnd = reinterpret_cast<HWND>(params["_callerHwnd"].get<intptr_t>());
    if (!hwnd || !IsWindow(hwnd)) {
        ctx.bridge = &BridgeCore::GetInstance();
        return ctx;
    }

    ctx.callerHwnd = hwnd;
    auto& wvc = WebViewContext::GetInstance();

    // 1. 直接按 _callerHwnd 匹配
    //    WebViewPanel 注册时以 hwnd_ 作 key，与 _callerHwnd 一致
    BridgeCore* directBridge = wvc.GetBridge(hwnd);
    if (directBridge) {
        ctx.bridge = directBridge;
        ctx.windowId = wvc.GetWindowIdByHwnd(hwnd);
        return ctx;
    }

    // 2. 弹窗模式 fallback：_callerHwnd 可能已被 GetAncestor 归为顶层窗口，
    //    需在所有 instance hwnd 中找与 callerHwnd 同属一个顶层窗口的实例
    HWND topLevel = ::GetAncestor(hwnd, GA_ROOT);
    if (topLevel) {
        for (auto instanceHwnd : wvc.GetAllInstances()) {
            if (instanceHwnd == hwnd) continue; // ->-> step 1 ->?
            if (::GetAncestor(instanceHwnd, GA_ROOT) == topLevel) {
                BridgeCore* b = wvc.GetBridge(instanceHwnd);
                if (b) {
                    ctx.bridge = b;
                    ctx.windowId = wvc.GetWindowIdByHwnd(instanceHwnd);
                    return ctx;
                }
            }
        }
    }

    // 3. 最终 fallback → BridgeCore 单例
    ctx.bridge = &BridgeCore::GetInstance();
    // 尝试从第一个已注册实例获取 windowId
    auto allInstances = wvc.GetAllInstances();
    if (!allInstances.empty()) {
        ctx.windowId = wvc.GetWindowIdByHwnd(allInstances.front());
    }
    return ctx;
}

void CallerContext::EmitEvent(const std::string& event, const json& data) const {
    if (bridge) {
        bridge->EmitEvent(event, data);
    } else {
        BridgeCore::GetInstance().EmitEvent(event, data);
    }
}

bool CallerContext::TryEmitEvent(const std::string& event, const json& data) const {
    if (bridge) {
        bridge->EmitEvent(event, data);
        return true;
    }
    return false;
}
