#include "pch.h"
#include "api/BridgeCore.h"
#include "api/ErrorEnvelope.h"
#include "utils/PathSecurity.h"
#include "utils/PathExpansion.h"
#include "webview/WebViewHost.h"
#include <chrono>

// ============================================
// 性能追踪器 - 检测阻塞主线程的 API 调用
// ============================================
class ApiPerformanceTracker {
public:
    explicit ApiPerformanceTracker(std::string method) 
        : method_(std::move(method)), start_(std::chrono::high_resolution_clock::now()) {
    }
    
    ~ApiPerformanceTracker() {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start_).count();
        int duration = static_cast<int>(duration_ms);  // 转换为 int 以兼容 printf %d
        
        // 超过阈值时警告
        if (duration >= WARN_THRESHOLD_MS) {
            console::printf("[Perf] !!! SLOW API: %s took %d ms (threshold: %d ms)", 
                           method_.c_str(), duration, WARN_THRESHOLD_MS);
        } else if (duration >= INFO_THRESHOLD_MS) {
            console::printf("[Perf] <<< END: %s took %d ms", method_.c_str(), duration);
        }
        
        // 超过阻塞阈值时严重警告
        if (duration >= BLOCK_THRESHOLD_MS) {
            console::printf("[Perf] *** BLOCKING API: %s BLOCKED UI for %d ms! Consider async operation.", 
                           method_.c_str(), duration);
        }
    }
    
private:
    std::string method_;
    std::chrono::high_resolution_clock::time_point start_;
    
    static constexpr int INFO_THRESHOLD_MS = 50;    // 50ms 以上记录
    static constexpr int WARN_THRESHOLD_MS = 200;   // 200ms 以上警告
    static constexpr int BLOCK_THRESHOLD_MS = 500;  // 500ms 以上视为阻塞
};

BridgeCore& BridgeCore::GetInstance() {
    static BridgeCore instance;
    return instance;
}

void BridgeCore::SetWebView(WebViewHost* webView) {
    std::lock_guard lock(mutex_);
    webView_ = webView;
}

void BridgeCore::RegisterApi(const std::string& method, ApiHandler handler) {
    std::lock_guard lock(mutex_);
    handlers_[method] = std::move(handler);
}

void BridgeCore::RegisterApi(const std::string& method, ApiHandler handler,
                             std::vector<PathSecuritySpec> specs) {
    auto wrapped = [innerHandler = std::move(handler),
                    innerSpecs = specs,
                    methodName = method](const json& params) -> json {
        size_t totalSkipped = 0;
        for (const auto& spec : innerSpecs) {
            auto r = ValidatePathParam(params, spec, methodName);
            if (!r.success) {
                return ApiEnvelope::MakeError(r.errorMsg.c_str(), ApiErrorCode::PERMISSION_DENIED);
            }
            totalSkipped += r.skippedCount;
        }
        json result = innerHandler(params);
        // 若有路径被跳过，在成功响应中附加 skippedPaths 警告
        if (totalSkipped > 0 && result.is_object() && result.value("success", false)) {
            result["skippedPaths"] = totalSkipped;
        }
        return result;
    };

    std::lock_guard lock(mutex_);
    handlers_[method] = std::move(wrapped);
    securitySpecs_[method] = std::move(specs);
}

void BridgeCore::UnregisterApi(const std::string& method) {
    std::lock_guard lock(mutex_);
    handlers_.erase(method);
    securitySpecs_.erase(method);
}

bool BridgeCore::HasApi(const std::string& method) const {
    std::lock_guard lock(mutex_);
    return handlers_.contains(method);
}

std::vector<std::string> BridgeCore::GetRegisteredApiNames() const {
    std::lock_guard lock(mutex_);
    std::vector<std::string> names;
    names.reserve(handlers_.size());
    for (const auto& [name, _] : handlers_) {
        names.push_back(name);
    }
    return names;
}

std::vector<PathSecuritySpec> BridgeCore::GetSecuritySpecs(const std::string& method) const {
    std::lock_guard lock(mutex_);
    auto it = securitySpecs_.find(method);
    if (it != securitySpecs_.end()) {
        return it->second;
    }
    return {};
}

void BridgeCore::HandleMessage(const std::wstring& message) {
    // 调用带 target 的重载，使用默认 webView_
    HandleMessage(message, nullptr);
}

void BridgeCore::HandleMessage(const std::wstring& message, WebViewHost* responseTarget) {
    // 向后兼容：无 callerHwnd 参数时传 nullptr
    HandleMessage(message, responseTarget, nullptr);
}

void BridgeCore::HandleMessage(const std::wstring& message, WebViewHost* responseTarget, HWND callerHwnd) {
    try {
        auto trimAscii = [](const std::string& in) -> std::string {
            size_t start = 0;
            while (start < in.size() && std::isspace(static_cast<unsigned char>(in[start]))) {
                ++start;
            }
            size_t end = in.size();
            while (end > start && std::isspace(static_cast<unsigned char>(in[end - 1]))) {
                --end;
            }
            return in.substr(start, end - start);
        };

        // Convert to UTF-8
        std::string utf8Message = WideToUtf8(message);
        
        // Parse JSON
        json msg = json::parse(utf8Message);
        
        // Extract fields - id can be number or string
        std::string id;
        if (msg.contains("id")) {
            if (msg["id"].is_number()) {
                id = std::to_string(msg["id"].get<int>());
            } else if (msg["id"].is_string()) {
                id = msg["id"].get<std::string>();
            }
        }
        
        std::string method = trimAscii(msg.value("method", ""));
        json params = msg.value("params", json::object());
        
        if (method.empty()) {
            if (!id.empty()) {
                SendError(id, -32600, "Invalid request: method is required", responseTarget,
                          ApiErrorCode::INVALID_REQUEST);
            }
            return;
        }
        
        // 注入调用者上下文到 params
        if (callerHwnd) {
            params["_callerHwnd"] = reinterpret_cast<intptr_t>(callerHwnd);
        }
        
        // Find handler
        ApiHandler handler;
        bool methodFound = false;
        {
            std::lock_guard lock(mutex_);
            auto it = handlers_.find(method);
            if (it != handlers_.end()) {
                handler = it->second;
                methodFound = true;
            } else {
                console::printf("[Bridge] Method not found: %s", method.c_str());
            }
        }
        
        if (!methodFound) {
            // Send error on main thread to avoid threading issues
            fb2k::inMainThread([this, id, method, responseTarget]() {
                SendError(id, -32601, "Method not found: " + method, responseTarget,
                          ApiErrorCode::METHOD_NOT_FOUND, method);
            });
            return;
        }
        
        // Execute on main thread (foobar2000 SDK requirement)
        fb2k::inMainThread([this, id, method, handler, params, responseTarget]() {
            try {
                ApiPerformanceTracker tracker(method);
                json result = handler(params);
                if (!id.empty()) {
                    SendResponse(id, result, responseTarget);
                }
            } catch (const std::exception& e) {
                console::printf("[Bridge] Handler error: %s", e.what());
                if (!id.empty()) {
                    SendError(id, -1, e.what(), responseTarget,
                              ApiErrorCode::INTERNAL_ERROR, method);
                }
            } catch (...) {
                console::printf("[Bridge] Handler error: unknown exception");
                if (!id.empty()) {
                    SendError(id, -1, "Unknown internal error", responseTarget,
                              ApiErrorCode::INTERNAL_ERROR, method);
                }
            }
        });
        
    } catch (const json::exception& e) {
        // JSON parse error
        console::printf("Bridge: JSON parse error: %s", e.what());
    }
}

void BridgeCore::SendResponse(const std::string& id, const json& result, WebViewHost* target) {
    json response;
    response["type"] = "response";
    // Convert id back to number if possible
    try {
        response["id"] = std::stoi(id);
    } catch (...) {
        response["id"] = id;
    }
    response["result"] = result;
    
    // 如果指定了 target，直接发送到该 WebView；否则使用默认的 webView_
    if (target) {
        std::string jsonStr = response.dump();
        std::wstring wideStr = Utf8ToWide(jsonStr);
        target->PostMessage(wideStr);
    } else {
        SendToWeb(response);
    }
}

void BridgeCore::SendError(const std::string& id, int /*numericCode*/, const std::string& message,
                            WebViewHost* target, const char* errorCode, const std::string& method) {
    json response;
    response["type"] = "response";
    // Convert id back to number if possible
    try {
        response["id"] = std::stoi(id);
    } catch (...) {
        response["id"] = id;
    }
    response["error"] = message;  // JS expects error as string
    // 统一 error code 字段（失败信封契约，见 ErrorEnvelope.h）
    if (errorCode && errorCode[0] != '\0') {
        response["code"] = errorCode;
    }

    // Failure sampling-log hook: log framework-level failures
    FailureHook::LogFramework(
        errorCode ? errorCode : "UNKNOWN",
        message.c_str(),
        method
    );
    
    // 如果指定了 target，直接发送到该 WebView；否则使用默认的 webView_
    if (target) {
        std::string jsonStr = response.dump();
        std::wstring wideStr = Utf8ToWide(jsonStr);
        target->PostMessage(wideStr);
    } else {
        SendToWeb(response);
    }
}

void BridgeCore::EmitEvent(const std::string& event, const json& data) {
    // Sampling log: trace event dispatch volume and payload size
    static std::atomic<uint64_t> eventCounter{0};
    uint64_t count = ++eventCounter;
    // Sample every 500th event to avoid log spam
    if (count % 500 == 1) {
        size_t payloadSize = data.dump().size();
        FB2K_console_formatter()
            << "[EventFlow] emit #" << count
            << " event=" << event.c_str()
            << " payload_bytes=" << (unsigned)payloadSize
            << " method=EmitEvent target=caller";
    }

    json message;
    message["type"] = "event";
    message["event"] = event;
    message["data"] = data;
    SendToWeb(message);
}

void BridgeCore::SendToWeb(const json& message) {
    WebViewHost* webView = nullptr;
    {
        std::lock_guard lock(mutex_);
        webView = webView_;
    }
    
    if (!webView) return;
    
    std::string jsonStr = message.dump();
    std::wstring wideStr = Utf8ToWide(jsonStr);
    
    webView->PostMessage(wideStr);
}

// ============================================
// Utility functions
// ============================================

std::string WideToUtf8(const std::wstring& wide) {
    if (wide.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) return "";
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, result.data(), size, nullptr, nullptr);
    result.pop_back();
    return result;
}

std::wstring Utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (size <= 0) return L"";
    std::wstring result(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, result.data(), size);
    result.pop_back();
    return result;
}

// ============================================
// ValidatePathParam — 统一路径参数校验
// ============================================

// 展开路径变量 — 委托给共享 PathExpansion 模块
static std::wstring ExpandPathVariables(const std::wstring& path) {
    return PathExpansion::Expand(path);
}

// 对单条路径按 SecurityLevel 调用对应 PathSecurity 方法
static bool ValidateSinglePath(const std::wstring& rawPath, SecurityLevel level, std::wstring& errorMsg) {
    std::wstring expanded = ExpandPathVariables(rawPath);
    auto& ps = PathSecurity::Instance();
    
    switch (level) {
        case SecurityLevel::None:
            return true;
        case SecurityLevel::Read:
            return ps.ValidatePath(expanded, errorMsg);
        case SecurityLevel::Write:
            return ps.ValidateWritePath(expanded, errorMsg);
        case SecurityLevel::MediaRead:
            return ps.ValidateMediaAccess(expanded, errorMsg);
        case SecurityLevel::MediaWrite:
            return ps.ValidateMediaWriteAccess(expanded, errorMsg);
    }
    errorMsg = L"Unknown security level";
    return false;
}

// 校验嵌套数组参数: items[].path 模式
static ValidationResult ValidateNestedArrayParam(const json& val,
                                                  const PathSecuritySpec& spec,
                                                  const std::string& methodName) {
    ValidationResult result;
    if (!val.is_array()) {
        result.success = false;
        result.errorMsg = methodName + ": param '" + spec.paramKey + "' must be an array";
        return result;
    }
    for (size_t i = 0; i < val.size(); ++i) {
        if (!val[i].is_object()) {
            continue;  // 非对象元素跳过（由业务 handler 自行处理格式校验）
        }
        if (!val[i].contains(spec.nestedKey)) {
            result.success = false;
            result.errorMsg = methodName + ": " + spec.paramKey + "[" + std::to_string(i) + "] missing required key '" + spec.nestedKey + "'";
            return result;
        }
        if (!val[i][spec.nestedKey].is_string()) {
            result.success = false;
            result.errorMsg = methodName + ": " + spec.paramKey + "[" + std::to_string(i) + "]." + spec.nestedKey + " must be a string";
            return result;
        }
        std::wstring wpath = Utf8ToWide(val[i][spec.nestedKey].get<std::string>());
        std::wstring errorMsg;
        if (!ValidateSinglePath(wpath, spec.level, errorMsg)) {
            result.success = false;
            result.errorMsg = methodName + ": path security denied for " + spec.paramKey + "[" + std::to_string(i) + "]." + spec.nestedKey + ": " + WideToUtf8(errorMsg);
            return result;
        }
    }
    return result;
}

// 校验简单数组参数: paths: ["a", "b"]
// skipInvalid=true 时跳过无效路径（记录 skippedCount），而非 fail-fast
static ValidationResult ValidateArrayParam(const json& val,
                                            const PathSecuritySpec& spec,
                                            const std::string& methodName) {
    ValidationResult result;
    if (!val.is_array()) {
        result.success = false;
        result.errorMsg = methodName + ": param '" + spec.paramKey + "' must be an array";
        return result;
    }
    for (size_t i = 0; i < val.size(); ++i) {
        if (!val[i].is_string()) {
            if (spec.skipInvalid) { result.skippedCount++; continue; }
            result.success = false;
            result.errorMsg = methodName + ": " + spec.paramKey + "[" + std::to_string(i) + "] must be a string";
            return result;
        }
        std::string pathUtf8 = val[i].get<std::string>();
        std::wstring wpath = Utf8ToWide(pathUtf8);
        std::wstring errorMsg;
        if (!ValidateSinglePath(wpath, spec.level, errorMsg)) {
            if (spec.skipInvalid) { result.skippedCount++; continue; }
            // 在错误消息中包含实际路径（截断到 120 字符），方便排查
            std::string pathSnippet = pathUtf8.length() > 120 ? pathUtf8.substr(0, 120) + "..." : pathUtf8;
            result.success = false;
            result.errorMsg = methodName + ": path security denied for " + spec.paramKey
                + "[" + std::to_string(i) + "]: " + WideToUtf8(errorMsg)
                + " (path: " + pathSnippet + ")";
            return result;
        }
    }
    return result;
}

// 校验单值参数: path: "xxx"
static ValidationResult ValidateScalarParam(const json& val,
                                             const PathSecuritySpec& spec,
                                             const std::string& methodName) {
    ValidationResult result;
    if (!val.is_string()) {
        if (val.is_null()) {
            return result;  // null 视为"未提供"
        }
        result.success = false;
        result.errorMsg = methodName + ": param '" + spec.paramKey + "' must be a string";
        return result;
    }
    std::wstring wpath = Utf8ToWide(val.get<std::string>());
    std::wstring errorMsg;
    if (!ValidateSinglePath(wpath, spec.level, errorMsg)) {
        result.success = false;
        result.errorMsg = methodName + ": path security denied for '" + spec.paramKey + "': " + WideToUtf8(errorMsg);
        return result;
    }
    return result;
}

ValidationResult ValidatePathParam(const json& params,
                                   const PathSecuritySpec& spec,
                                   const std::string& methodName) {
    // 可选参数: key 不存在时跳过
    if (!params.contains(spec.paramKey)) {
        return {};  // success = true
    }
    
    const auto& val = params[spec.paramKey];
    
    if (!spec.nestedKey.empty()) {
        return ValidateNestedArrayParam(val, spec, methodName);
    }
    if (spec.isArray) {
        return ValidateArrayParam(val, spec, methodName);
    }
    return ValidateScalarParam(val, spec, methodName);
}
