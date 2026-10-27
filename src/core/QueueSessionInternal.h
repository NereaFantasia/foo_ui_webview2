#pragma once

#include "api/CallerContext.h"
#include "core/QueueSessionState.h"
#include "core/WebViewContext.h"
#include "webview/WebViewHost.h"

namespace jit_session_runtime {

inline WebViewHost::DocumentStamp Stamp(const Document& document) {
    return {document.hostSerial, document.navigationGeneration, document.hostGeneration};
}

inline bool Alive(const Document& document) {
    auto& context = WebViewContext::GetInstance();
    const auto hwnd = reinterpret_cast<HWND>(document.hwnd);
    auto* host = context.GetWebViewHost(hwnd);
    return host && context.GetWindowIdByHwnd(hwnd) == document.windowId &&
           host->IsCurrentDocument(Stamp(document)) && host->IsCurrentDocumentTrusted();
}

inline std::optional<Document> Capture(const CallerContext& caller) {
    auto& context = WebViewContext::GetInstance();
    auto* host = context.GetWebViewHost(caller.callerHwnd);
    if (!host || !host->IsCurrentDocumentTrusted()) return {};
    const auto stamp = host->CaptureDocument();
    if (!host->IsCurrentDocument(stamp)) return {};
    const auto windowId = context.GetWindowIdByHwnd(caller.callerHwnd);
    if (windowId.empty()) return {};
    return Document{windowId, reinterpret_cast<std::uintptr_t>(caller.callerHwnd),
                    stamp.hostSerial, stamp.navigationGeneration, stamp.hostGeneration};
}

inline bool SameWindow(const Document& left, const Document& right) {
    return !left.windowId.empty() && left.windowId == right.windowId && left.hwnd == right.hwnd;
}

inline std::string DocumentRequest(const Document& document, const std::string& requestId) {
    return "document:" + nlohmann::json::array({document.hostSerial, document.navigationGeneration,
                                                document.hostGeneration, requestId}).dump();
}

inline std::uint64_t Digest(const std::string& method, const nlohmann::json& payload) {
    const std::string value = nlohmann::json::array({method, payload}).dump();
    std::uint64_t hash = 14695981039346656037ULL;
    for (const unsigned char byte : value) {
        hash ^= byte;
        hash *= 1099511628211ULL;
    }
    return hash;
}

// 只读取宿主自己保存的成功应答，不解析页面输入。
template <class R>
R Replay(const std::string& response) {
    const auto value = nlohmann::json::parse(response);
    R result;
    if constexpr (requires { result.sessionId; }) result.sessionId = value.at("sessionId").get<std::string>();
    if constexpr (requires { result.controlToken; }) result.controlToken = value.at("controlToken").get<std::string>();
    if constexpr (requires { result.ownerEpoch; }) result.ownerEpoch = value.at("ownerEpoch").get<std::int64_t>();
    if constexpr (requires { result.revision; }) result.revision = value.at("revision").get<std::int64_t>();
    if constexpr (requires { result.attemptId; }) result.attemptId = value.at("attemptId").get<std::string>();
    if constexpr (requires { result.operationId; }) {
        if constexpr (std::is_same_v<decltype(result.operationId), std::optional<std::string>>) {
            if (value.contains("operationId")) result.operationId = value.at("operationId").get<std::string>();
        } else {
            result.operationId = value.at("operationId").get<std::string>();
        }
    }
    return result;
}

// fb2k 在当前曲目真正结束之前就选定下一首，之后删掉队列项或影子列表行都撤不回。解码最多领先
// 输出一个缓冲长度，另留 1 秒给输出与 DSP 的延迟。主线程调用。
// ponytail: 只按核心输出缓冲估计；交叉淡化这类提前读取更多的 DSP 不在估计内，遇到再按实测放宽。
inline bool NextMayBePreselected() {
    auto playback = playback_control::get();
    if (!playback->is_playing()) return false;
    const double length = playback->playback_get_length();
    if (length <= 0) return false;
    const double buffer = output_manager::get()->getCoreConfig().m_buffer_length;
    return length - playback->playback_get_position() <= buffer + 1.0;
}

inline const char* OutcomeName(jit_session::Outcome outcome) {
    switch (outcome) {
        case jit_session::Outcome::Completed: return "completed";
        case jit_session::Outcome::Stopped: return "stopped";
        case jit_session::Outcome::Superseded: return "superseded";
        case jit_session::Outcome::Released: return "released";
        case jit_session::Outcome::Unknown: return "unknown";
    }
    return "unknown";
}

}  // namespace jit_session_runtime
