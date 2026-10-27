#include "pch.h"
#include "core/QueueManager.h"
#include "core/QueueSessionInternal.h"
#include "api/EventEmit.h"
#include "window/MainWindow.h"
#include "window/WindowManager.h"

#include <random>

namespace js = api::jitSession;
namespace protocol = jit_session_runtime;

std::string protocol::NewId() {
    std::random_device random;
    constexpr char hex[] = "0123456789abcdef";
    std::string result(32, '0');
    for (size_t i = 0; i < 4; ++i) {
        const std::uint32_t value = random();
        for (size_t digit = 0; digit < 8; ++digit) result[i * 8 + digit] = hex[(value >> (digit * 4)) & 15];
    }
    return result;
}

QueueManager::SessionEventScope::~SessionEventScope() {
    try { owner.FlushSessionEvents(); }
    catch (...) { FB2K_console_print("[JIT Session] Event delivery failed; read the session snapshot to recover"); }
}

void QueueManager::FlushSessionEvents() {
    std::deque<protocol::Delivery> pending;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_protocol) pending.swap(m_protocol->events);
    }
    for (const auto& delivery : pending) {
        auto* host = WebViewContext::GetInstance().GetWebViewHost(reinterpret_cast<HWND>(delivery.document.hwnd));
        if (!host || !host->IsCurrentDocument(protocol::Stamp(delivery.document)) || !host->IsCurrentDocumentTrusted()) continue;
        std::visit([host](const auto& payload) {
            using P = std::decay_t<decltype(payload)>;
            if constexpr (std::is_same_v<P, js::StateChangedPayload>) api::emit::Post<js::events::StateChanged>(*host, payload);
            else if constexpr (std::is_same_v<P, js::NeedNextPayload>) api::emit::Post<js::events::NeedNext>(*host, payload);
            else if constexpr (std::is_same_v<P, js::OperationCompletedPayload>) api::emit::Post<js::events::OperationCompleted>(*host, payload);
            else if constexpr (std::is_same_v<P, js::AttemptFinishedPayload>) api::emit::Post<js::events::AttemptFinished>(*host, payload);
            else if constexpr (std::is_same_v<P, js::AttemptRecheckedPayload>) api::emit::Post<js::events::AttemptRechecked>(*host, payload);
            else if constexpr (std::is_same_v<P, js::ControlChangedPayload>) api::emit::Post<js::events::ControlChanged>(*host, payload);
        }, delivery.payload);
    }
    UpdateBackgroundHold();
}

void QueueManager::UpdateBackgroundHold() {
    std::uintptr_t owner = 0;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_protocol && m_protocol->phase == "active" && m_protocol->ownerConnected) owner = m_protocol->owner.hwnd;
    }
    if (owner == m_backgroundHold) return;
    m_backgroundHold = owner;
    // 只有独立主窗口会在隐藏时深度挂起页面；面板与弹窗从不挂起，不需要持有。
    // ponytail: 暂停中的会话也持有，页面隐藏期间一直不挂起；嫌内存时再按播放状态细分。
    auto* window = WindowManager::GetInstance().GetMainWindow();
    if (!window) return;
    const HWND root = owner ? ::GetAncestor(reinterpret_cast<HWND>(owner), GA_ROOT) : nullptr;
    window->SetSessionKeepsRunning(root && root == window->GetShellHwnd());
}

bool QueueManager::HasProtocolSession() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return ProtocolSchedules();
}

bool QueueManager::ProtocolSchedules() const {
    return m_protocol && m_protocol->phase != "released" && m_protocol->phaseReason != "legacyTakeover";
}

void QueueManager::RefreshSessionOwner() {
    std::optional<protocol::Document> owner;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_protocol && m_protocol->Expired(std::chrono::steady_clock::now())) m_protocol.reset();
        if (m_protocol && m_protocol->phase != "released") owner = m_protocol->owner;
    }
    if (!owner) return;
    const bool alive = protocol::Alive(*owner);
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_protocol && m_protocol->owner == *owner && m_protocol->ownerConnected != alive) {
        m_protocol->ownerConnected = alive;
        ++m_protocol->revision;
        m_protocol->PublishState();
    }
}

QueueManager::SessionGuard QueueManager::CheckSessionWrite(
    const std::string& sessionId, const std::string& token, std::int64_t revision,
    const std::string& requestId, const std::string& method, const nlohmann::json& payload,
    const protocol::Document& caller, bool allowReleased) {
    SessionGuard result;
    if (!m_protocol || m_protocol->id != sessionId) {
        result.failure = api::Fail("The JIT session was not found", ApiErrorCode::NOT_FOUND);
        return result;
    }
    auto& session = *m_protocol;
    const auto checked = jit_session::CheckWrite(session.token, static_cast<std::uint64_t>(session.revision),
                                                token, static_cast<std::uint64_t>(revision));
    if (caller != session.owner || checked == jit_session::WriteCheck::NotOwner ||
        (session.phase == "released" && !allowReleased)) {
        result.failure = api::Fail("This document does not control the JIT session", ApiErrorCode::NOT_OWNER);
        return result;
    }
    result.key = "owner:" + std::to_string(session.ownerEpoch) + ":" + requestId;
    result.digest = protocol::Digest(method, payload);
    std::string response;
    const auto hit = session.requests.Check(result.key, result.digest, &response);
    if (hit == jit_session::RequestLog::Hit::Replay) result.replay = std::move(response);
    else if (hit == jit_session::RequestLog::Hit::Conflict) {
        result.failure = api::Fail("The request ID was already used or has expired", ApiErrorCode::CONFLICT);
    } else if (session.phase == "released") {
        result.failure = api::Fail("The JIT session has been released", ApiErrorCode::NOT_OWNER);
    } else if (checked == jit_session::WriteCheck::Conflict) {
        result.failure = api::Fail("The JIT session revision changed", ApiErrorCode::CONFLICT,
                                    {{"details", {{"revision", session.revision}}}});
    }
    return result;
}

api::Result<js::CreateResult> QueueManager::SessionCreate(const js::CreateParams& params, const CallerContext& caller) {
    SessionEventScope flush{*this};
    const auto document = protocol::Capture(caller);
    if (!document) return api::Fail("A trusted document is required", ApiErrorCode::NOT_OWNER);
    RefreshSessionOwner();
    const std::string key = protocol::DocumentRequest(*document, params.requestId);
    const auto digest = protocol::Digest("create", nlohmann::json::object());
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_protocol && m_protocol->phase != "released") {
        std::string replay;
        if (m_protocol->owner == *document && m_protocol->requests.Check(key, digest, &replay) == jit_session::RequestLog::Hit::Replay) {
            return protocol::Replay<js::CreateResult>(replay);
        }
        return api::Fail("A JIT session already exists", ApiErrorCode::CONFLICT,
                         {{"details", {{"sessionId", m_protocol->id}}}});
    }
    m_protocol.emplace();
    m_native.reset();
    m_nativeTicket.reset();
    auto& session = *m_protocol;
    session.id = protocol::NewId();
    session.token = protocol::NewId();
    session.owner = *document;
    const js::CreateResult result{session.id, session.token, session.ownerEpoch, session.revision};
    session.requests.Record(key, digest, js::ToJson(result).dump());
    session.PublishState();
    return result;
}

api::Result<js::DiscoverResult> QueueManager::SessionDiscover(const CallerContext& caller) {
    SessionEventScope flush{*this};
    const auto document = protocol::Capture(caller);
    if (!document) return api::Fail("A trusted document is required", ApiErrorCode::NOT_OWNER);
    RefreshSessionOwner();
    std::lock_guard<std::mutex> lock(m_mutex);
    js::DiscoverResult result;
    if (m_protocol && m_protocol->phase != "released") {
        const auto& session = *m_protocol;
        result.session = js::JitSessionDiscovery{session.id, session.ownerConnected ? "connected" : "disconnected",
                                                 session.ownerEpoch, !session.ownerConnected && protocol::SameWindow(*document, session.owner)};
    }
    return result;
}

api::Result<js::GetStateResult> QueueManager::SessionGetState(const js::GetStateParams& params) {
    SessionEventScope flush{*this};
    RefreshSessionOwner();
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_protocol || m_protocol->id != params.sessionId) return api::Fail("The JIT session was not found", ApiErrorCode::NOT_FOUND);
    const auto& session = *m_protocol;
    RefreshNativeValidity();
    return js::GetStateResult{session.Snapshot()};
}

api::Result<js::ClaimControlResult> QueueManager::SessionClaim(const js::ClaimControlParams& params, const CallerContext& caller) {
    SessionEventScope flush{*this};
    const auto document = protocol::Capture(caller);
    if (!document) return api::Fail("A trusted document is required", ApiErrorCode::NOT_OWNER);
    RefreshSessionOwner();
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_protocol || m_protocol->id != params.sessionId || m_protocol->phase == "released") {
        return api::Fail("The JIT session was not found", ApiErrorCode::NOT_FOUND);
    }
    auto& session = *m_protocol;
    const auto key = protocol::DocumentRequest(*document, params.requestId);
    nlohmann::json payload{{"mode", params.mode}};
    api::results::Put(payload, "expectedOwnerEpoch", params.expectedOwnerEpoch);
    const auto digest = protocol::Digest("claimControl", payload);
    std::string replay;
    const auto hit = session.requests.Check(key, digest, &replay);
    if (hit == jit_session::RequestLog::Hit::Replay && session.owner == *document) return protocol::Replay<js::ClaimControlResult>(replay);
    if (hit != jit_session::RequestLog::Hit::New) return api::Fail("The control request is no longer applicable", ApiErrorCode::CONFLICT);
    const auto check = jit_session::CheckClaim(params.mode == "resume" ? jit_session::ClaimMode::Resume : jit_session::ClaimMode::Handover,
                                               protocol::SameWindow(*document, session.owner), session.ownerConnected,
                                               params.expectedOwnerEpoch.has_value(), static_cast<std::uint64_t>(params.expectedOwnerEpoch.value_or(0)),
                                               static_cast<std::uint64_t>(session.ownerEpoch));
    if (check != jit_session::ClaimCheck::Ok) {
        return api::Fail("The requested control transfer is not allowed",
                         check == jit_session::ClaimCheck::NotOwner ? ApiErrorCode::NOT_OWNER : ApiErrorCode::CONFLICT);
    }
    const auto previous = session.owner;
    session.owner = *document;
    session.ownerConnected = true;
    session.token = protocol::NewId();
    ++session.ownerEpoch;
    ++session.revision;
    DiscardPreparedNative();
    const js::ClaimControlResult result{session.token, session.ownerEpoch, session.revision};
    session.requests.Record(key, digest, js::ToJson(result).dump());
    session.ControlChanged("claimed", previous);
    session.PublishState();
    // 未答复请求会保留在快照；用新事件序号向新控制文档补发同一请求。
    if (session.neededAfterAttemptId && session.neededRequestKey) {
        js::NeedNextPayload event;
        event.afterAttemptId = *session.neededAfterAttemptId;
        event.requestKey = *session.neededRequestKey;
        session.Emit(std::move(event));
    }
    return result;
}

api::Result<js::GetOperationResult> QueueManager::SessionGetOperation(const js::GetOperationParams& params) {
    SessionEventScope flush{*this};
    RefreshSessionOwner();
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_protocol || m_protocol->id != params.sessionId) return api::Fail("The JIT session was not found", ApiErrorCode::NOT_FOUND);
    const auto* operation = m_protocol->operations.Find(params.operationId);
    if (!operation) return api::Fail("The operation was not found or has expired", ApiErrorCode::NOT_FOUND);
    return js::GetOperationResult{protocol::Session::Operation(*operation)};
}
