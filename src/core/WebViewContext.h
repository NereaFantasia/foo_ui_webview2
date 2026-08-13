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
    // 单例访问 (管理器本身是单例)
    static WebViewContext& GetInstance();
    
    // Non-copyable
    WebViewContext(const WebViewContext&) = delete;
    WebViewContext& operator=(const WebViewContext&) = delete;
    
    // ============================================
    // 实例管理
    // ============================================
    
    // 注册 WebView 实例
    // @param hwnd 窗口句柄 (作为唯一标识)
    // @param host WebViewHost 指针
    // @param bridge BridgeCore 指针
    // @param windowId 可选窗口 ID（多窗口系统用）
    void RegisterInstance(HWND hwnd, WebViewHost* host, BridgeCore* bridge);
    void RegisterInstance(HWND hwnd, WebViewHost* host, BridgeCore* bridge, const std::string& windowId);
    void RegisterInstance(HWND hwnd, WebViewHost* host, BridgeCore* bridge, const std::string& windowId, WebViewPanel* panel);
    
    // 注销 WebView 实例
    void UnregisterInstance(HWND hwnd);
    
    // 检查实例是否已注册
    bool HasInstance(HWND hwnd) const;
    
    // 获取实例数量
    size_t GetInstanceCount() const;
    
    // ============================================
    // 实例查询
    // ============================================
    
    // 根据 HWND 获取 BridgeCore
    BridgeCore* GetBridge(HWND hwnd) const;
    
    // 根据 HWND 获取 WebViewHost
    WebViewHost* GetWebViewHost(HWND hwnd) const;

    // 指针存活校验：host 是否仍是当前已注册实例之一。
    // 供延迟执行的响应路径（BridgeCore::SendResponse/SendError 捕获的裸指针）
    // 在解引用前校验——WebView 崩溃重建会销毁旧 host，在途响应必须丢弃而非回投。
    bool IsLiveHost(const WebViewHost* host) const;
    
    // 获取主实例的 BridgeCore (用于向后兼容)
    // 返回第一个注册的实例，如果没有则返回 nullptr
    BridgeCore* GetPrimaryBridge() const;
    
    // 获取所有已注册的 HWND
    std::vector<HWND> GetAllInstances() const;
    
    // 根据 HWND 查找对应的 WebViewHost（支持子窗口→顶级窗口查找）
    // 若 hwnd 本身未注册，会尝试 GetAncestor(hwnd, GA_ROOT) 获取顶级窗口再查找
    WebViewHost* GetHostByHwnd(HWND hwnd) const;
    
    // 根据 HWND 查找对应的 WebViewPanel
    WebViewPanel* GetPanelByHwnd(HWND hwnd) const;
    
    // ============================================
    // 事件广播
    // ============================================
    
    // 广播事件到所有 WebView 实例
    void BroadcastEvent(const std::string& event, const json& data);
    
    // 广播事件到指定实例 (排除某个实例)
    void BroadcastEventExcept(const std::string& event, const json& data, HWND excludeHwnd);

    // 是否存在任一页面可见的实例。
    // 供可再生流（每拍重发全量新值）的生产者在构造 payload 之前早退使用：
    // 全部实例 hidden 时该拍无消费者，跳过采样/序列化即可，恢复可见后下一拍
    // 自然出值，无需补偿。仅可用于可再生流——丢失一拍不影响正确性的事件；
    // 携带一次性事实或增量语义的事件必须照常投递。
    bool HasVisibleInstance() const;

    // ============================================
    // 窗口 ID 查询（多窗口系统用）
    // ============================================
    
    // 通过窗口 ID 发送定向事件
    bool SendEventTo(const std::string& windowId, const std::string& event, const json& data);
    
    // 通过窗口 ID 获取 BridgeCore
    BridgeCore* GetBridgeByWindowId(const std::string& windowId) const;
    
    // 通过窗口 ID 获取 HWND
    HWND GetHwndByWindowId(const std::string& windowId) const;
    
    // 通过 HWND 获取 windowId（反查）
    std::string GetWindowIdByHwnd(HWND hwnd) const;

private:
    WebViewContext() = default;
    
    struct InstanceInfo {
        WebViewHost* host = nullptr;
        BridgeCore* bridge = nullptr;
        std::string windowId;
        WebViewPanel* panel = nullptr;  // v2: 用于 panel.getConfig/setConfig API
    };
    
    std::unordered_map<HWND, InstanceInfo> instances_;
    mutable std::mutex mutex_;
    
    // 第一个注册的实例 (主实例)
    HWND primaryHwnd_ = nullptr;
};
