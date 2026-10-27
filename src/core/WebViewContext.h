#pragma once
#include "pch.h"
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <mutex>
#include <vector>

using json = nlohmann::json;

class WebViewHost;
class BridgeCore;
class WebViewPanel;

// ============================================
// WebViewContext - 多实例 WebView 管理器
// 用于支持 DUI/CUI 面板的多实例场景
// ============================================
class WebViewContext {
public:
    static WebViewContext& GetInstance();
    
    WebViewContext(const WebViewContext&) = delete;
    WebViewContext& operator=(const WebViewContext&) = delete;
    
    // ============================================
    // 实例管理
    // ============================================
    
    // hwnd 是实例的唯一标识。
    void RegisterInstance(HWND hwnd, WebViewHost* host, BridgeCore* bridge);
    void RegisterInstance(HWND hwnd, WebViewHost* host, BridgeCore* bridge, const std::string& windowId);
    void RegisterInstance(HWND hwnd, WebViewHost* host, BridgeCore* bridge, const std::string& windowId, WebViewPanel* panel);
    
    void UnregisterInstance(HWND hwnd);
    
    bool HasInstance(HWND hwnd) const;
    
    size_t GetInstanceCount() const;
    
    // ============================================
    // 实例查询
    // ============================================
    
    BridgeCore* GetBridge(HWND hwnd) const;
    
    WebViewHost* GetWebViewHost(HWND hwnd) const;

    // 指针存活校验：host 是否仍是当前已注册实例之一。
    // 供延迟执行的响应路径（BridgeCore::SendResponse/SendError 捕获的裸指针）
    // 在解引用前校验——WebView 崩溃重建会销毁旧 host，在途响应必须丢弃而非回投。
    bool IsLiveHost(const WebViewHost* host) const;
    
    // 主实例的 BridgeCore，没有实例时为 nullptr。
    BridgeCore* GetPrimaryBridge() const;
    
    std::vector<HWND> GetAllInstances() const;
    
    // hwnd 本身未注册时改按它的顶级窗口（GA_ROOT）查找；GetPanelByHwnd 相同。
    WebViewHost* GetHostByHwnd(HWND hwnd) const;
    
    WebViewPanel* GetPanelByHwnd(HWND hwnd) const;
    
    // ============================================
    // 事件广播
    // ============================================
    
    void BroadcastEvent(const std::string& event, const json& data);
    
    void BroadcastEventExcept(const std::string& event, const json& data, HWND excludeHwnd);

    // 是否有页面可见的实例。只供每拍重发全量值的流在构造载荷前早退：全部隐藏时跳过这一拍，
    // 恢复可见后下一拍照常出值。一次性事实或增量事件必须照常投递。
    bool HasVisibleInstance() const;

    // ============================================
    // 窗口 ID 查询（多窗口系统用）
    // ============================================
    
    bool SendEventTo(const std::string& windowId, const std::string& event, const json& data);
    
    BridgeCore* GetBridgeByWindowId(const std::string& windowId) const;
    
    HWND GetHwndByWindowId(const std::string& windowId) const;
    
    std::string GetWindowIdByHwnd(HWND hwnd) const;

private:
    WebViewContext() = default;
    
    struct InstanceInfo {
        WebViewHost* host = nullptr;
        BridgeCore* bridge = nullptr;
        std::string windowId;
        WebViewPanel* panel = nullptr;  // 供 panel.getConfig/setConfig 使用
    };
    
    std::unordered_map<HWND, InstanceInfo> instances_;
    mutable std::mutex mutex_;
    
    // 主实例：最先注册的实例，它注销后由任一剩余实例接替。
    HWND primaryHwnd_ = nullptr;
};
