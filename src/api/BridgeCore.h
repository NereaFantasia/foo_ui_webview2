#pragma once
#include "pch.h"
#include <nlohmann/json.hpp>
#include <functional>
#include <unordered_map>
#include <vector>
#include <mutex>

using json = nlohmann::json;

class WebViewHost;

// API handler function type
using ApiHandler = std::function<json(const json& params)>;

// ============================================
// 路径安全层级 — 统一权限架构
// ============================================
enum class SecurityLevel {
    None,       // 无路径参数 / 不需要路径校验
    Read,       // ValidatePath (文件系统只读)
    Write,      // ValidateWritePath (严格写白名单)
    MediaRead,  // ValidateMediaAccess (读 + 媒体库/播放列表上下文信任)
    MediaWrite  // ValidateMediaWriteAccess (写 + 媒体库/播放列表上下文信任, 无非系统盘放行)
};

// 路径参数校验规格
struct PathSecuritySpec {
    std::string paramKey;          // JSON params 中的 key (e.g. "path", "saveTo")
    SecurityLevel level;           // 校验层级
    bool isArray = false;          // 参数是否为数组 (e.g. "paths": [...])
    std::string nestedKey;         // 嵌套 key (e.g. items[].path → nestedKey = "path")
    bool skipInvalid = false;      // 数组模式: 跳过无效路径而非 fail-fast (逐条校验用)
};

// 路径校验结果
struct ValidationResult {
    bool success = true;
    std::string errorMsg;
    size_t skippedCount = 0;       // skipInvalid 模式: 被跳过的路径数量
};

// 校验单个路径参数（按 spec 规格提取参数、展开变量、调用对应 PathSecurity 层级）
// 实现在 BridgeCore.cpp 中
ValidationResult ValidatePathParam(const json& params,
                                   const PathSecuritySpec& spec,
                                   const std::string& methodName);

// ============================================
// Bridge Core - C++ <-> JavaScript communication
// Supports both singleton (backward compatible) and per-instance mode
// ============================================
class BridgeCore {
public:
    // Singleton access (for backward compatibility with standalone window)
    // For new panel instances, create BridgeCore directly and register with WebViewContext
    static BridgeCore& GetInstance();
    
    // Public constructor for per-instance usage (DUI/CUI panels)
    BridgeCore() = default;
    
    // Non-copyable
    BridgeCore(const BridgeCore&) = delete;
    BridgeCore& operator=(const BridgeCore&) = delete;
    
    // Set WebView for sending messages
    void SetWebView(WebViewHost* webView);
    
    // Register API handler
    void RegisterApi(const std::string& method, ApiHandler handler);
    
    // Register API handler with path security specs (decorator pattern)
    void RegisterApi(const std::string& method, ApiHandler handler,
                     std::vector<PathSecuritySpec> specs);
    
    // Unregister API handler
    void UnregisterApi(const std::string& method);
    
    // Check if API is registered
    bool HasApi(const std::string& method) const;
    
    // Get all registered API names
    std::vector<std::string> GetRegisteredApiNames() const;
    
    // Get security specs for a registered API (audit/debug visibility)
    std::vector<PathSecuritySpec> GetSecuritySpecs(const std::string& method) const;
    
    // Handle message from JavaScript
    // @param message JSON message from WebView
    // @param responseTarget (optional) WebView to send response to; if nullptr, uses webView_
    // @param callerHwnd (optional) HWND of the calling window, injected as _callerHwnd into params
    void HandleMessage(const std::wstring& message);
    void HandleMessage(const std::wstring& message, WebViewHost* responseTarget);
    void HandleMessage(const std::wstring& message, WebViewHost* responseTarget, HWND callerHwnd);
    
    // Send event to JavaScript
    void EmitEvent(const std::string& event, const json& data);
    
    // Send response to JavaScript
    // @param target (optional) WebView to send response to; if nullptr, uses webView_
    void SendResponse(const std::string& id, const json& result, WebViewHost* target = nullptr);
    void SendError(const std::string& id, int numericCode, const std::string& message,
                   WebViewHost* target = nullptr, const char* errorCode = "",
                   const std::string& method = "");

private:
    WebViewHost* webView_ = nullptr;
    std::unordered_map<std::string, ApiHandler, std::hash<std::string>, std::equal_to<>> handlers_;
    std::unordered_map<std::string, std::vector<PathSecuritySpec>, std::hash<std::string>, std::equal_to<>> securitySpecs_;
    mutable std::mutex mutex_;
    
    void SendToWeb(const json& message);
};

// Utility functions for string conversion
std::string WideToUtf8(const std::wstring& wide);
std::wstring Utf8ToWide(const std::string& utf8);
