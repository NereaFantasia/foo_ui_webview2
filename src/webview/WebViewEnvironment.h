#pragma once
#include "pch.h"
#include <wil/com.h>
#include <WebView2.h>
#include <functional>
#include <mutex>

// ============================================
// WebViewEnvironment - WebView2 环境预热与共享
// 
// 优化策略：
// 1. 在 foobar2000 启动时预创建 WebView2 环境
// 2. 所有面板/窗口共享同一个环境实例
// 3. 大幅减少后续 WebView 创建时间
// ============================================

class WebViewEnvironment {
public:
    static WebViewEnvironment& GetInstance();
    
    // 禁止拷贝
    WebViewEnvironment(const WebViewEnvironment&) = delete;
    WebViewEnvironment& operator=(const WebViewEnvironment&) = delete;
    
    // 预热环境（在 foobar2000 启动时调用）
    // 这个函数会异步创建 WebView2 环境
    void Preheat();
    
    // 释放全局 COM 引用并重置就绪状态（在 foobar2000 关闭时调用）
    // 调用后 environment_ 被释放，后续 GetEnvironment 将重新创建
    void Shutdown();
    
    // 获取共享环境（如果未就绪则创建）
    // callback 会在环境就绪时调用
    using EnvironmentCallback = std::function<void(ICoreWebView2Environment*)>;
    void GetEnvironment(const EnvironmentCallback& callback);
    
    // 环境是否已就绪
    bool IsReady() const { return ready_; }
    
    // 获取用户数据路径
    static std::wstring GetUserDataPath();

private:
    WebViewEnvironment() = default;
    
    // Internal method - assumes creating_ flag is already set
    void CreateEnvironmentInternal();
    
    // 失败时 drain 等待队列并以 nullptr 回调
    void DrainPendingCallbacksOnFailure();
    
    wil::com_ptr<ICoreWebView2Environment> environment_;
    std::mutex mutex_;
    bool ready_ = false;
    bool creating_ = false;
    
    // 等待环境就绪的回调队列
    std::vector<EnvironmentCallback> pendingCallbacks_;
};

// ============================================
// initquit 服务声明 - 在启动时预热环境
// ============================================
void InitWebViewEnvironment();
void ShutdownWebViewEnvironment();
