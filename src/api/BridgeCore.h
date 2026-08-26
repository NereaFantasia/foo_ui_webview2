#pragma once
#include "pch.h"
#include <nlohmann/json.hpp>
#include <atomic>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>
#include <mutex>

using json = nlohmann::json;

class WebViewHost;
class BridgeCore;

// API handler function type
using ApiHandler = std::function<json(const json& params)>;

// ============================================
// 路径安全层级 — 统一权限架构
// ============================================
enum class SecurityLevel {
    None,       // 无路径参数 / 不需要路径校验
    Read,       // ValidatePath (文件系统只读)
    Write,      // ValidateWritePath (严格写白名单: profile/temp)
    MediaRead,  // ValidateMediaAccess (读 + 媒体库/播放列表上下文信任)
    MediaWrite, // ValidateMediaWriteAccess (写受信媒体上下文; 无非系统盘放行)
    FileWrite   // ValidateFileWriteAccess (通用文件写; 保留非系统盘放行, 待决)
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
    bool shapeError = false;       // true 表示参数形状/类型失败而非路径安全拒绝，wrapper 据此选错误码
};

// 校验单个路径参数（按 spec 规格提取参数、展开变量、调用对应 PathSecurity 层级）
// 实现在 BridgeCore.cpp 中
ValidationResult ValidatePathParam(const json& params,
                                   const PathSecuritySpec& spec,
                                   const std::string& methodName);

// ============================================
// 延迟响应管线 — DeferredResponder
// ============================================
//
// 常规 handler 在主线程内返回 json，HandleMessage 就地回包。大结果集查询要把重活
// 挪到 CPU worker，回包时机因此晚于 handler 返回：deferred handler 不返回结果，而是
// 收下一个可拷贝的轻句柄，在任意线程、任意时刻回包一次。
//
// 不变量 —— 每请求恰好一次响应：
//   · 漏响应 = 页面侧落到 30s 超时兜底（WebViewHost.cpp 的 Request timeout），表现为
//     假死并掩盖真因；worker 段的未捕获异常会被线程池静默吞掉，故 handler 的 worker
//     段必须自带顶层 try/catch。
//   · 重复响应 = 第二包带同一 id 回到已 resolve 的 Promise，静默丢失。
// 三条通道共用一道 CAS 闸门，判定点恒在主线程（投递取 inMainThread2，选型与两分支
// 语义见 PostGated 实现处注释），故多个 worker 线程同时回包也只有一个能穿过。
class DeferredResponder {
public:
    // 直写通道：resultJsonUtf8 必须是调用方已序列化好的合法 JSON 值文本
    // （约定同 BridgeCore::SendResponseRaw）。
    void SendRaw(std::string resultJsonUtf8) const;

    // DOM 通道：语义与同步 handler 的返回值等价，move 进信封。
    void SendJson(json result) const;

    // 框架级错误信封（type/id/error/code）。API 自身的业务失败**不**走这里——那应当
    // 用 SendJson 返回 {success:false,...} 正常响应体，与同步路径形状一致；本通道只
    // 用于框架兜底（handler 未捕获异常等）。
    // @param errorCode 必须是静态存储期字符串（ApiErrorCode:: 常量或字面量）：句柄跨
    //                  线程只带指针，临时 std::string 的 c_str() 会在投递途中失效。
    void SendError(std::string message, const char* errorCode = "",
                   std::string method = "") const;

private:
    friend class BridgeCore;

    struct State {
        BridgeCore* bridge = nullptr;   // 分发本请求的 bridge（生命周期见 .cpp 注释）
        std::string id;                 // 空串 = 通知型调用，三条通道一律静默丢弃
        WebViewHost* target = nullptr;  // nullptr = 走 bridge 默认 webView_
        std::atomic<bool> responded{false};
    };

    DeferredResponder(BridgeCore* bridge, std::string id, WebViewHost* target);

    // 三条通道共用的投递骨架：主线程执行点上过 PassGate，通过才执行 dispatch。
    void PostGated(std::function<void(const State&)> dispatch) const;

    // 主线程执行点上的三关判定「通知型丢弃 → 恰好一次闸门 → target 存活守卫」；
    // 返回 false = 本次回包丢弃。
    static bool PassGate(State& state);

    std::shared_ptr<State> state_;
};

// Deferred API handler：不返回结果，通过 responder 回包（可跨线程、可延迟）
using DeferredApiHandler = std::function<void(const json& params, DeferredResponder responder)>;

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

    // 注册延迟响应 API：handler 收 responder 而非返回 json，回包时机由 handler 决定。
    // 与 RegisterApi 分表存放（互不覆盖），但在 HasApi / GetRegisteredApiNames /
    // UnregisterApi 三个查询面与同步注册等价可见 —— 见实现处注释。
    void RegisterApiDeferred(const std::string& method, DeferredApiHandler handler);

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
    // @param result 按值接收：大结果集响应体在此让出所有权（见 SendResponse 实现），
    //               const json& 形态下 response["result"] = result 会再深拷贝整棵树
    // @param target (optional) WebView to send response to; if nullptr, uses webView_
    void SendResponse(const std::string& id, json result, WebViewHost* target = nullptr);

    // 直写响应通道：result 段已是调用方序列化好的 UTF-8 JSON 串，不再经 DOM 往返。
    // 信封字段集与 id 归一化与 SendResponse 完全一致（共用 NormalizeResponseId）。
    // @param resultJsonUtf8 必须是合法 JSON 值文本（对象/数组/标量皆可）——整串交给
    //                       PostWebMessageAsJson，坏 JSON 在 WebView2 层就投递失败
    void SendResponseRaw(const std::string& id, const std::string& resultJsonUtf8,
                         WebViewHost* target = nullptr);

    void SendError(const std::string& id, int numericCode, const std::string& message,
                   WebViewHost* target = nullptr, const char* errorCode = "",
                   const std::string& method = "");

private:
    WebViewHost* webView_ = nullptr;
    std::unordered_map<std::string, ApiHandler, std::hash<std::string>, std::equal_to<>> handlers_;
    std::unordered_map<std::string, DeferredApiHandler, std::hash<std::string>, std::equal_to<>> deferredHandlers_;
    std::unordered_map<std::string, std::vector<PathSecuritySpec>, std::hash<std::string>, std::equal_to<>> securitySpecs_;
    mutable std::mutex mutex_;

    // 主线程分发段 —— HandleMessage 投递的 lambda 只做一行转调，段体在此具名。
    // 两条路径参数与回包时机不同，分成两个方法而非一个带判别标记的方法：
    // 同步段自己回包（含 id 为空则不回包），deferred 段把回包交给 responder。
    void DispatchApiCall(const std::string& id, const std::string& method,
                         const ApiHandler& handler, const json& params,
                         WebViewHost* responseTarget);
    static void DispatchDeferredApiCall(const std::string& method,
                                        const DeferredApiHandler& handler,
                                        const json& params,
                                        const DeferredResponder& responder);

    // id 归一化（number/string 二态）——响应通道共用单点，见实现处注释
    static json NormalizeResponseId(const std::string& id);

    void SendToWeb(const json& message);
    void SendRawToWeb(const std::string& messageUtf8);
};

// Utility functions for string conversion
std::string WideToUtf8(const std::wstring& wide);
std::wstring Utf8ToWide(const std::string& utf8);
