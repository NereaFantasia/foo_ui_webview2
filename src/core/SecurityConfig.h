#pragma once

// ============================================
// Security Configuration Access Functions
// ============================================
// 这些函数在 main.cpp 中定义，用于访问安全配置

namespace security_config {
    // DevTools 开关
    bool IsDevToolsEnabled();
    
    // CDP 远程调试开关 (MCP/AI Agent)
    bool IsCdpRemoteEnabled();
    
    // 本地网络访问开关
    bool IsLocalNetworkAccessAllowed();
    
    // 允许 HTTP 明文连接
    bool IsInsecureHttpAllowed();

    // 允许自签 / 无效 TLS 证书 (fb.http.* 用,每请求还需 opt-in)
    bool IsInsecureTlsAllowed();
    
    // 后台模式开关
    bool IsBackgroundModeEnabled();
    
    // HMR 开发服务器配置
    bool UseDevServer();
    const char* GetDevServerUrl();
    void SetDevServerUrl(const char* url);
    void SetUseDevServer(bool use);
}
