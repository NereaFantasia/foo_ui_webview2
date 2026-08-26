// BridgeDispatchSimulator.h - Test double for BridgeCore dispatch logic
// Mirrors the real HandleMessage / SendResponse / SendResponseRaw / SendError /
// EmitEvent / RegisterApiDeferred + DeferredResponder contract without
// foobar2000 SDK, WebView2, or Win32 dependencies.
#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <stdexcept>
#include <nlohmann/json.hpp>

// 直写信封的转义/数值格式化取真身同一实现（JsonWriter.h 只依赖 STL，
// 无 SDK/Win32 依赖，不破坏本 harness 的独立性）。
#include "utils/JsonWriter.h"

using json = nlohmann::json;

class BridgeDispatchSimulator {
public:
    using ApiHandler = std::function<json(const json& params)>;

    // 延迟响应句柄（mirrors BridgeCore 的 DeferredResponder）。
    //
    // 与真身的唯一形态差异：真身三条通道走 fb2k::inMainThread2（主线程调用即同步执行、
    // 其余线程入队），本 harness 没有主线程队列，两种调用者都就地执行 —— 主线程调用者
    // 与真身完全同形，worker 调用者是"真身入队后到主线程执行"的等价简化。三关次序
    // （通知型丢弃 → 恰好一次 CAS → target 存活）与真身逐条一致，闸门本身是真的
    // atomic CAS —— 多线程用例测的就是它。
    //
    // 由此也决定了本 harness 的消息队列线程安全边界：并发回包时只有闸门赢家会碰
    // sentMessages_，输家只碰原子计数器。这正是被测不变量本身。
    //
    // 另一处有意的不对称：真身在闸门之后把送达段包在 try/catch 里（dump 遇非法 UTF-8
    // 会抛，逸出到主线程回调派发器就是崩溃），本 harness 不包 —— 测试环境里意外异常
    // 应当把用例打红，而不是被吞。
    class DeferredResponder {
    public:
        void SendRaw(std::string resultJsonUtf8) const;
        void SendJson(json result) const;
        void SendError(std::string message, const char* errorCode = "",
                       std::string method = "") const;

    private:
        friend class BridgeDispatchSimulator;

        struct State {
            BridgeDispatchSimulator* sim = nullptr;
            std::string id;  // 空串 = 通知型调用，三条通道一律静默丢弃
            std::atomic<bool> responded{false};
        };

        DeferredResponder(BridgeDispatchSimulator* sim, std::string id)
            : state_(std::make_shared<State>()) {
            state_->sim = sim;
            state_->id = std::move(id);
        }

        // 三条通道共用的闸门；返回 false = 本次回包被丢弃
        bool PassGate() const;

        std::shared_ptr<State> state_;
    };

    using DeferredApiHandler = std::function<void(const json& params, DeferredResponder responder)>;

    // ---- Registration (mirrors BridgeCore::RegisterApi) ----
    void RegisterApi(const std::string& method, ApiHandler handler) {
        handlers_[method] = std::move(handler);
    }

    // mirrors BridgeCore::RegisterApiDeferred —— 独立注册表，不覆盖同步表；
    // 注销与两个查询面对 deferred 与同步注册一视同仁（同真身口径）。
    void RegisterApiDeferred(const std::string& method, DeferredApiHandler handler) {
        deferredHandlers_[method] = std::move(handler);
    }

    void UnregisterApi(const std::string& method) {
        handlers_.erase(method);
        deferredHandlers_.erase(method);
    }

    bool HasApi(const std::string& method) const {
        return handlers_.contains(method) || deferredHandlers_.contains(method);
    }

    std::vector<std::string> GetRegisteredApiNames() const {
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

    // target 存活谓词（mirrors WebViewContext::IsLiveHost 在闸门之后的那一道守卫）。
    // 未设置 = 恒存活。测试在派发前设置，不做并发保护。
    void SetTargetLivePredicate(std::function<bool()> predicate) {
        targetLivePredicate_ = std::move(predicate);
    }

    // ---- Message handling (mirrors BridgeCore::HandleMessage) ----
    // Takes UTF-8 JSON string directly (real BridgeCore takes wstring + WideToUtf8)
    void HandleMessage(const std::string& utf8Json) {
        try {
            json msg = json::parse(utf8Json);

            // Extract id (number or string)
            std::string id;
            if (msg.contains("id")) {
                if (msg["id"].is_number()) {
                    id = std::to_string(msg["id"].get<int>());
                } else if (msg["id"].is_string()) {
                    id = msg["id"].get<std::string>();
                }
            }

            // Extract and trim method
            std::string method = TrimAscii(msg.value("method", ""));
            json params = msg.value("params", json::object());

            if (method.empty()) {
                if (!id.empty()) {
                    SendError(id, -32600, "Invalid request: method is required",
                              "INVALID_REQUEST");
                }
                return;
            }

            // Find handler — 同步表优先，未命中再查 deferred 表（同真身查表顺序）
            auto it = handlers_.find(method);
            if (it == handlers_.end()) {
                auto deferredIt = deferredHandlers_.find(method);
                if (deferredIt == deferredHandlers_.end()) {
                    SendError(id, -32601, "Method not found: " + method,
                              "METHOD_NOT_FOUND", method);
                    return;
                }
                DispatchDeferredApiCall(id, method, deferredIt->second, params);
                return;
            }

            DispatchApiCall(id, method, it->second, params);

        } catch (const json::exception&) {
            // JSON parse error — silently dropped (mirrors real BridgeCore)
        }
    }

    // ---- Response/Error/Event formatting (mirrors BridgeCore) ----
    // result 按值接收（随真身 B3 签名），函数体内 move 入信封。
    void SendResponse(const std::string& id, json result) {
        json response;
        response["type"] = "response";
        response["id"] = NormalizeResponseId(id);
        response["result"] = std::move(result);
        sentMessages_.push_back(std::move(response));
    }

    // 直写响应通道（mirrors BridgeCore::SendResponseRaw）：result 段是已序列化好的
    // UTF-8 JSON 串，信封靠字符串拼接，id 走与 SendResponse 同一归一化。
    //
    // 真身把拼好的整串交给 PostWebMessageAsJson，页面侧 e.data 是解析后的对象；
    // 因此这里同时留存原始串（GetSentRawMessages / LastRawMessage，供字节级断言）
    // 和解析结果（入 sentMessages_，让既有信封/顺序断言面对 raw 通道同样适用）。
    // result 段非法 JSON 时此处 parse 抛出，等价于真身在 WebView2 层投递失败。
    void SendResponseRaw(const std::string& id, const std::string& resultJsonUtf8) {
        std::string envelope;
        envelope.reserve(resultJsonUtf8.size() + 64);
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

        sentRawMessages_.push_back(envelope);
        sentMessages_.push_back(json::parse(envelope));
    }

    void SendError(const std::string& id, int /*numericCode*/, const std::string& message,
                   const char* errorCode = "", const std::string& method = "") {
        json response;
        response["type"] = "response";
        try {
            response["id"] = std::stoi(id);
        } catch (...) {
            response["id"] = id;
        }
        response["error"] = message;
        if (errorCode && errorCode[0] != '\0') {
            response["code"] = errorCode;
        }
        sentMessages_.push_back(std::move(response));
    }

    void EmitEvent(const std::string& event, const json& data) {
        json message;
        message["type"] = "event";
        message["event"] = event;
        message["data"] = data;
        sentMessages_.push_back(std::move(message));
    }

    // ---- Test inspection ----
    const std::vector<json>& GetSentMessages() const { return sentMessages_; }
    size_t GetMessageCount() const { return sentMessages_.size(); }
    void ClearMessages() {
        sentMessages_.clear();
        sentRawMessages_.clear();
    }

    const json& LastMessage() const {
        if (sentMessages_.empty()) {
            throw std::runtime_error("No messages sent");
        }
        return sentMessages_.back();
    }

    // 直写通道的原始信封串（字节级断言用；只有 SendResponseRaw 会入队）
    const std::vector<std::string>& GetSentRawMessages() const { return sentRawMessages_; }

    const std::string& LastRawMessage() const {
        if (sentRawMessages_.empty()) {
            throw std::runtime_error("No raw messages sent");
        }
        return sentRawMessages_.back();
    }

    // deferred 通道的丢弃计数（真身在对应位置是两条 console 记录，形态不同、判据相同）
    size_t GetDuplicateDropCount() const { return duplicateDropCount_.load(); }
    size_t GetDeadTargetDropCount() const { return deadTargetDropCount_.load(); }

private:
    // 分发段（mirrors BridgeCore::DispatchApiCall / DispatchDeferredApiCall）：真身把
    // 这两段外提成具名方法、投递的 lambda 只留一行转调，镜像照同一形态切分以保持逐段
    // 可对照。真身两段各由 fb2k::inMainThread 包裹，此处同步执行。
    void DispatchApiCall(const std::string& id, const std::string& method,
                         const ApiHandler& handler, const json& params) {
        try {
            json result = handler(params);
            if (!id.empty()) {
                // 随真身 B3：SendResponse 按值收 result，调用点让出所有权
                SendResponse(id, std::move(result));
            }
        } catch (const std::exception& e) {
            if (!id.empty()) {
                SendError(id, -1, e.what(), "INTERNAL_ERROR", method);
            }
        } catch (...) {
            if (!id.empty()) {
                SendError(id, -1, "Unknown internal error", "INTERNAL_ERROR", method);
            }
        }
    }

    // handler 不返回结果；异常兜底必须经 responder 的闸门（handler 可能先回过包才抛），
    // id 为空时由闸门负责静默丢弃。
    void DispatchDeferredApiCall(const std::string& id, const std::string& method,
                                 const DeferredApiHandler& handler, const json& params) {
        DeferredResponder responder(this, id);
        try {
            handler(params, responder);
        } catch (const std::exception& e) {
            responder.SendError(e.what(), "INTERNAL_ERROR", method);
        } catch (...) {
            responder.SendError("Unknown internal error", "INTERNAL_ERROR", method);
        }
    }

    // Mirrors BridgeCore::NormalizeResponseId (BridgeCore.cpp) — 数值 id 以 JSON
    // number 回传、非数值以 string 回传；stoi 的前缀语义（"007"->7、"12abc"->12）
    // 原样保留。两条响应通道共用，二态写错即页面侧 _callbacks Map miss。
    static json NormalizeResponseId(const std::string& id) {
        try {
            return json(std::stoi(id));
        } catch (...) {
            return json(id);
        }
    }

    static std::string TrimAscii(const std::string& in) {
        size_t start = 0;
        while (start < in.size() && std::isspace(static_cast<unsigned char>(in[start]))) {
            ++start;
        }
        size_t end = in.size();
        while (end > start && std::isspace(static_cast<unsigned char>(in[end - 1]))) {
            --end;
        }
        return in.substr(start, end - start);
    }

    std::unordered_map<std::string, ApiHandler> handlers_;
    std::unordered_map<std::string, DeferredApiHandler> deferredHandlers_;
    std::vector<json> sentMessages_;
    std::vector<std::string> sentRawMessages_;
    std::function<bool()> targetLivePredicate_;
    std::atomic<size_t> duplicateDropCount_{0};
    std::atomic<size_t> deadTargetDropCount_{0};
};

// DeferredResponder 的成员定义放到外层类之后：闸门与三条通道都要用外层类的完整类型
// （SendResponse / SendResponseRaw / SendError 与计数器）。
inline bool BridgeDispatchSimulator::DeferredResponder::PassGate() const {
    if (state_->id.empty()) {
        return false;  // 通知型调用：同步路径同样不回包
    }

    bool expected = false;
    if (!state_->responded.compare_exchange_strong(expected, true)) {
        ++state_->sim->duplicateDropCount_;
        return false;
    }

    // 闸门之后才判存活：与真身同序，被丢弃的那一次同样已消耗掉闸门
    if (state_->sim->targetLivePredicate_ && !state_->sim->targetLivePredicate_()) {
        ++state_->sim->deadTargetDropCount_;
        return false;
    }

    return true;
}

inline void BridgeDispatchSimulator::DeferredResponder::SendRaw(std::string resultJsonUtf8) const {
    if (!PassGate()) return;
    state_->sim->SendResponseRaw(state_->id, resultJsonUtf8);
}

inline void BridgeDispatchSimulator::DeferredResponder::SendJson(json result) const {
    if (!PassGate()) return;
    state_->sim->SendResponse(state_->id, std::move(result));
}

inline void BridgeDispatchSimulator::DeferredResponder::SendError(std::string message,
                                                                  const char* errorCode,
                                                                  std::string method) const {
    if (!PassGate()) return;
    state_->sim->SendError(state_->id, -1, message, errorCode, method);
}
