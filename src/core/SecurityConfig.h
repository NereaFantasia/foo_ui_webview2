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

    // CDP 自动化 keep-alive：生效时托盘/最小化/锁屏不得将页面挂起
    // (visibilityState 保持 visible)。生效条件 = 本进程实际开启了 CDP 端口
    // (环境创建期快照) && keep-alive 子开关。
    bool IsAutomationKeepAliveActive();

    // 由 WebViewEnvironment 在注入 --remote-debugging-port 的同一分支调用,
    // 一次性置位本进程 CDP 端口快照。advconfig 值运行期可改但端口需重启,
    // keep-alive 判定必须绑定真实端口状态而非配置实时值。
    void NoteCdpPortOpenedThisProcess();

    // 深度挂起（TrySuspend）开关：隐藏路径（最小化/托盘/锁屏）把内存回收手段
    // 从 MemoryUsageTargetLevel=Low 升级为真正冻结 renderer 的 TrySuspend。
    // 关闭时整条 deepSuspend 投影退化为现状 Low 路径（回退开关，实时读取）。
    bool IsDeepSuspendEnabled();
    
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
