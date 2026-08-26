#include "pch.h"
#include "core/WebViewContext.h"
#include "api/BridgeCore.h"
#include "api/ErrorEnvelope.h"
#include "utils/JsonWriter.h"
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
                // 参数形状/类型错与路径安全拒绝分派到不同 code：前者是调用方写错了
                // 参数，后者才是权限层级不足，页面据此分支处理。
                return ApiEnvelope::MakeError(
                    r.errorMsg.c_str(),
                    r.shapeError ? ApiErrorCode::INVALID_PARAMS : ApiErrorCode::PERMISSION_DENIED);
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

// deferred 注册表与同步表分开存放：RegisterApi 的行为、签名与 handlers_ 的形状全不
// 变动，分发侧只多一次查表。两表同名时同步表优先（见 HandleMessage 查表顺序），此处
// 只记警告不覆盖 —— 静默择一会让某个 API 实际走哪条路取决于注册顺序。
void BridgeCore::RegisterApiDeferred(const std::string& method, DeferredApiHandler handler) {
    std::lock_guard lock(mutex_);
    if (handlers_.contains(method)) {
        console::printf("[Bridge] RegisterApiDeferred: '%s' is already a synchronous API; "
                        "dispatch keeps using the synchronous handler", method.c_str());
    }
    deferredHandlers_[method] = std::move(handler);
}

void BridgeCore::UnregisterApi(const std::string& method) {
    std::lock_guard lock(mutex_);
    handlers_.erase(method);
    deferredHandlers_.erase(method);
    securitySpecs_.erase(method);
}

// 注册面查询对 deferred 与同步注册一视同仁。deferred 若在此不可见，把一个方法迁到
// deferred 就等于：偏好页 API 清单（PreferencesPage.cpp:1690）与 PluginRegistry 的
// API 清单（PluginRegistry.cpp:308）里凭空少一条，且插件能覆盖该内建 API
// （PluginRegistry.cpp:177 的冲突检查以 HasApi 为口径）。
bool BridgeCore::HasApi(const std::string& method) const {
    std::lock_guard lock(mutex_);
    return handlers_.contains(method) || deferredHandlers_.contains(method);
}

std::vector<std::string> BridgeCore::GetRegisteredApiNames() const {
    std::lock_guard lock(mutex_);
    std::vector<std::string> names;
    names.reserve(handlers_.size() + deferredHandlers_.size());
    for (const auto& [name, _] : handlers_) {
        names.push_back(name);
    }
    for (const auto& [name, _] : deferredHandlers_) {
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
        
        // Find handler —— 同步表优先，未命中再查 deferred 表
        ApiHandler handler;
        DeferredApiHandler deferredHandler;
        bool methodFound = false;
        {
            std::lock_guard lock(mutex_);
            auto it = handlers_.find(method);
            if (it != handlers_.end()) {
                handler = it->second;
                methodFound = true;
            } else if (auto deferredIt = deferredHandlers_.find(method);
                       deferredIt != deferredHandlers_.end()) {
                deferredHandler = deferredIt->second;
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
        
        // 延迟响应 API：handler 不返回 json、也不在此回包，回包时机交给 responder。
        if (deferredHandler) {
            // responder 在此构造（而非投递的 lambda 内）：句柄自带 bridge/id/target，
            // lambda 因此无需捕获 this。句柄在被 Send 之前是惰性的，构造时机不影响语义。
            DeferredResponder responder(this, id, responseTarget);
            fb2k::inMainThread([method, deferredHandler, params, responder]() {
                DispatchDeferredApiCall(method, deferredHandler, params, responder);
            });
            return;
        }

        // Execute on main thread (foobar2000 SDK requirement)
        fb2k::inMainThread([this, id, method, handler, params, responseTarget]() {
            DispatchApiCall(id, method, handler, params, responseTarget);
        });

    } catch (const json::exception& e) {
        // JSON parse error
        console::printf("Bridge: JSON parse error: %s", e.what());
    }
}

// 同步 API 的主线程分发段：执行 handler 并就地回包。
// ApiPerformanceTracker 覆盖整段（析构时出耗时日志），异常一律折成 INTERNAL_ERROR
// 错误信封；id 为空（通知型调用）时成败都不回包。
void BridgeCore::DispatchApiCall(const std::string& id, const std::string& method,
                                 const ApiHandler& handler, const json& params,
                                 WebViewHost* responseTarget) {
    try {
        ApiPerformanceTracker tracker(method);
        json result = handler(params);
        if (!id.empty()) {
            // move：大结果集响应体（数万曲 tracks 数组）不再多一次深拷贝
            SendResponse(id, std::move(result), responseTarget);
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
}

// 延迟响应 API 的主线程分发段：与同步段同构（同一个 tracker 覆盖范围、同一形状的
// INTERNAL_ERROR 兜底），差别是回包交给 responder —— 因此兜底必须经 responder 的
// 恰好一次闸门（handler 完全可能先回过包才抛，那一包要被挡掉），且 id 为空时由闸门
// 负责静默丢弃。worker 段的耗时与异常由 handler 自己兜（漏响应即页面侧超时假死）。
// 不访问任何实例成员，故为 static：投递的 lambda 无需捕获 this。
void BridgeCore::DispatchDeferredApiCall(const std::string& method,
                                         const DeferredApiHandler& handler,
                                         const json& params,
                                         const DeferredResponder& responder) {
    try {
        ApiPerformanceTracker tracker(method);
        handler(params, responder);
    } catch (const std::exception& e) {
        console::printf("[Bridge] Handler error: %s", e.what());
        responder.SendError(e.what(), ApiErrorCode::INTERNAL_ERROR, method);
    } catch (...) {
        console::printf("[Bridge] Handler error: unknown exception");
        responder.SendError("Unknown internal error", ApiErrorCode::INTERNAL_ERROR, method);
    }
}

// id 归一化 —— 响应通道（SendResponse / SendResponseRaw）共用单点。
//
// JS 侧 _callbacks 是以 ++_callId 数值为键的 Map（WebViewHost.cpp:773-774、795），
// 按值匹配：数值 id 必须以 JSON number 回传，写成 string 即 Map miss，页面落到
// 30s 超时兜底（WebViewHost.cpp:784-789）表现为假死。两条通道共用本函数即保证
// 二态判定与取值逐字节同源，不会各写一份而漂移。
//
// stoi 的宽松前缀语义在此原样保留（"007" -> 7、"12abc" -> 12）：这是既有 wire
// 行为，页面侧 id 由自增计数器产生不会命中这些形态，收紧反而是可观测契约变化。
json BridgeCore::NormalizeResponseId(const std::string& id) {
    try {
        return json(std::stoi(id));
    } catch (...) {
        return json(id);
    }
}

void BridgeCore::SendResponse(const std::string& id, json result, WebViewHost* target) {
    json response;
    response["type"] = "response";
    response["id"] = NormalizeResponseId(id);
    // move 而非拷贝：形参按值接过所有权后，此处再拷贝就等于前面白让一场
    response["result"] = std::move(result);

    // 如果指定了 target，直接发送到该 WebView；否则使用默认的 webView_
    if (target) {
        // 存活校验：target 是 HandleMessage 时捕获的裸指针，经主线程回调延迟执行。
        // WebView 崩溃重建（browserProcessExited）会在请求与响应之间销毁旧 host，
        // 悬垂指针解引用曾致 AV（failure_00000195）。目标页面已死，响应丢弃即可。
        if (!WebViewContext::GetInstance().IsLiveHost(target)) {
            console::printf("[Bridge] SendResponse dropped: target WebViewHost no longer live (id=%s)", id.c_str());
            return;
        }
        std::string jsonStr = response.dump();
        std::wstring wideStr = Utf8ToWide(jsonStr);
        target->PostMessage(wideStr);
    } else {
        SendToWeb(response);
    }
}

// 直写响应通道 —— 信封字符串拼接，result 段原样嵌入不二次转义。
//
// 与 SendResponse 的三键信封（type/id/result）逐字段一致，仅省掉「建 DOM → dump」：
// 数万条曲目的 result 串在调用方（worker 段）已构建完毕，此处只做 O(1) 次拼接。
void BridgeCore::SendResponseRaw(const std::string& id, const std::string& resultJsonUtf8,
                                 WebViewHost* target) {
    std::string envelope;
    envelope.reserve(resultJsonUtf8.size() + 64);  // 信封固定开销远小于 64 字节
    envelope.append("{\"type\":\"response\",\"id\":");

    const json normalizedId = NormalizeResponseId(id);
    if (normalizedId.is_number_integer()) {
        JsonWriter::AppendJsonInt(envelope, normalizedId.get<int64_t>());
    } else {
        JsonWriter::AppendJsonString(envelope, normalizedId.get_ref<const std::string&>());
    }

    envelope.append(",\"result\":");
    envelope.append(resultJsonUtf8);
    envelope.push_back('}');

    // 如果指定了 target，直接发送到该 WebView；否则使用默认的 webView_
    if (target) {
        // 与 SendResponse 同一守卫：deferred 路径的 target 在请求与响应之间可能随
        // WebView 崩溃重建被销毁，IsLiveHost 只做指针相等判定不解引用（悬垂必判负）。
        if (!WebViewContext::GetInstance().IsLiveHost(target)) {
            console::printf("[Bridge] SendResponseRaw dropped: target WebViewHost no longer live (id=%s)", id.c_str());
            return;
        }
        std::wstring wideStr = Utf8ToWide(envelope);
        target->PostMessage(wideStr);
    } else {
        SendRawToWeb(envelope);
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
        // 与 SendResponse 同一守卫：目标 host 已随 WebView 重建销毁时丢弃错误响应。
        if (!WebViewContext::GetInstance().IsLiveHost(target)) {
            console::printf("[Bridge] SendError dropped: target WebViewHost no longer live (id=%s)", id.c_str());
            return;
        }
        std::string jsonStr = response.dump();
        std::wstring wideStr = Utf8ToWide(jsonStr);
        target->PostMessage(wideStr);
    } else {
        SendToWeb(response);
    }
}

// ============================================
// DeferredResponder — 延迟响应句柄的实现
// ============================================
//
// 状态放共享块、句柄自己只是一个 shared_ptr：句柄要被 handler 拷进 worker lambda、
// 再拷进回主线程的 lambda，闸门必须是这些副本共享的同一份 —— 各副本各自"只回一次"
// 合起来就回了多次。
//
// bridge 裸指针的生命周期：消息分发走 BridgeCore 单例（WebViewPanel.cpp:366-368
// "API 都注册在单例上"，只把本实例的 host 当 responseTarget），函数内静态对象随进程
// 存活；panel 自己的 bridge_（WebViewPanel.h:253）只用于 EmitEvent，不进本路径。
// 即便将来出现非单例 bridge 分发，PostGated 里 target 存活守卫先于 bridge 解引用
// 执行 —— target 还在注册表里意味着其所属 panel 活着，panel 持有的 bridge 亦然。
DeferredResponder::DeferredResponder(BridgeCore* bridge, std::string id, WebViewHost* target)
    : state_(std::make_shared<State>()) {
    state_->bridge = bridge;
    state_->id = std::move(id);
    state_->target = target;
}

bool DeferredResponder::PassGate(State& state) {
    if (state.id.empty()) {
        return false;  // 通知型调用：同步路径同样不回包（HandleMessage 的 id.empty 分支）
    }

    bool expected = false;
    if (!state.responded.compare_exchange_strong(expected, true)) {
        console::printf("[Bridge] Deferred response dropped: already responded (id=%s)",
                        state.id.c_str());
        return false;
    }

    // 目标页面已随 WebView 崩溃重建被销毁（failure_00000195 修复模式）：丢弃即可。
    // 这一步先于 bridge / target 解引用，SendResponse* 内部的同名守卫是第二道。
    if (state.target && !WebViewContext::GetInstance().IsLiveHost(state.target)) {
        console::printf("[Bridge] Deferred response dropped: target WebViewHost no longer live (id=%s)",
                        state.id.c_str());
        return false;
    }

    return true;
}

void DeferredResponder::PostGated(std::function<void(const State&)> dispatch) const {
    // 取 inMainThread2（声明 threadsLite.h:22-23，实现 main_thread_callback.cpp:41-47：
    // 主线程调用即同步执行 f()，其余线程退回 inMainThread 入队）。不取无条件入队的
    // inMainThread：handler 在主线程段内的同步回包本该与同步 handler 逐拍一致，无条件
    // 入队会让这类响应晚一个 pump turn 发出，跨请求的投递顺序随之可观测地变化。
    // 从 worker 调用时不阻塞等待主线程，两分支都不引入死锁面。
    //
    // 闸门与守卫的判定一律在回调体内，且两分支都落在主线程（立即分支就在当前主线程栈
    // 上，入队分支由主线程回调派发）⇒ 判定始终主线程串行，多个 worker 同时回包仍只有
    // 一个能穿过；判定通过后紧接着送达，中间没有可被抢跑的窗口。
    fb2k::inMainThread2([state = state_, dispatch = std::move(dispatch)]() {
        if (!PassGate(*state)) {
            return;
        }
        try {
            dispatch(*state);
        } catch (const std::exception& e) {
            // 送达段自身能抛的是 dump 遇非法 UTF-8（type_error.316）与 bad_alloc 一类。
            // 此时闸门已消耗，不补发第二包（重复响应是同等坏的语义），页面侧落到超时
            // 兜底；异常若逸出到主线程回调派发器则是整体崩溃，更坏。
            console::printf("[Bridge] Deferred response send failed (id=%s): %s",
                            state->id.c_str(), e.what());
        } catch (...) {
            console::printf("[Bridge] Deferred response send failed (id=%s): unknown exception",
                            state->id.c_str());
        }
    });
}

void DeferredResponder::SendRaw(std::string resultJsonUtf8) const {
    PostGated([payload = std::move(resultJsonUtf8)](const State& s) {
        s.bridge->SendResponseRaw(s.id, payload, s.target);
    });
}

void DeferredResponder::SendJson(json result) const {
    // mutable：std::function 以非 const 方式调用目标，捕获的 payload 才能真的 move 出去。
    // 少了它，捕获项是 const，std::move 静默退化成整棵树的深拷贝且编译无警告。
    PostGated([payload = std::move(result)](const State& s) mutable {
        s.bridge->SendResponse(s.id, std::move(payload), s.target);
    });
}

void DeferredResponder::SendError(std::string message, const char* errorCode,
                                  std::string method) const {
    PostGated([msg = std::move(message), errorCode, methodName = std::move(method)](const State& s) {
        s.bridge->SendError(s.id, -1, msg, s.target, errorCode, methodName);
    });
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

    // 事件经可见性门控发送；invoke 响应仍走 SendToWeb 直发。
    WebViewHost* webView = nullptr;
    {
        std::lock_guard lock(mutex_);
        webView = webView_;
    }
    if (!webView) return;

    webView->PostEventMessage(event, Utf8ToWide(message.dump()));
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

// SendToWeb 的 raw 版：消息已是 UTF-8 JSON 串，跳过 dump。路由（默认 webView_
// 的加锁读取 + 空判）与 SendToWeb 逐句一致。
void BridgeCore::SendRawToWeb(const std::string& messageUtf8) {
    WebViewHost* webView = nullptr;
    {
        std::lock_guard lock(mutex_);
        webView = webView_;
    }

    if (!webView) return;

    std::wstring wideStr = Utf8ToWide(messageUtf8);

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
        case SecurityLevel::FileWrite:
            return ps.ValidateFileWriteAccess(expanded, errorMsg);
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
        result.shapeError = true;
        result.errorMsg = methodName + ": param '" + spec.paramKey + "' must be an array";
        return result;
    }
    for (size_t i = 0; i < val.size(); ++i) {
        if (!val[i].is_object()) {
            continue;  // 非对象元素跳过（由业务 handler 自行处理格式校验）
        }
        if (!val[i].contains(spec.nestedKey)) {
            result.success = false;
            result.shapeError = true;
            result.errorMsg = methodName + ": " + spec.paramKey + "[" + std::to_string(i) + "] missing required key '" + spec.nestedKey + "'";
            return result;
        }
        if (!val[i][spec.nestedKey].is_string()) {
            result.success = false;
            result.shapeError = true;
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
        result.shapeError = true;
        result.errorMsg = methodName + ": param '" + spec.paramKey + "' must be an array";
        return result;
    }
    for (size_t i = 0; i < val.size(); ++i) {
        if (!val[i].is_string()) {
            if (spec.skipInvalid) { result.skippedCount++; continue; }
            result.success = false;
            result.shapeError = true;
            result.errorMsg = methodName + ": " + spec.paramKey + "[" + std::to_string(i) + "] must be a string";
            return result;
        }
        std::string pathUtf8 = val[i].get<std::string>();
        std::wstring wpath = Utf8ToWide(pathUtf8);
        std::wstring errorMsg;
        if (!ValidateSinglePath(wpath, spec.level, errorMsg)) {
            if (spec.skipInvalid) { result.skippedCount++; continue; }
            // 只回显参数位置与拒因，不含路径本体（路径不得写入错误 payload）
            result.success = false;
            result.errorMsg = methodName + ": path security denied for " + spec.paramKey
                + "[" + std::to_string(i) + "]: " + WideToUtf8(errorMsg);
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
        result.shapeError = true;
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
