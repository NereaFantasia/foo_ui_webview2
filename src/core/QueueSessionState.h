#pragma once

#include "api/generated/JitSessionSchema.h"
#include "core/JitSessionModel.h"

#include <chrono>
#include <deque>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <variant>

namespace jit_session_runtime {

struct Document {
    std::string windowId;
    std::uintptr_t hwnd = 0;
    std::uint64_t hostSerial = 0;
    std::uint64_t navigationGeneration = 0;
    std::uint64_t hostGeneration = 0;

    bool operator==(const Document&) const = default;
};

enum class ExecutionStage { Resolving, Submitted, Playing };

struct Entry {
    std::string id;
    std::string title;
    std::string location;
    std::optional<double> expiresAt;
    std::string attemptId;
    std::string operationId;
    std::uint64_t serial = 0;
    std::string status = "preparing";
    ExecutionStage execution = ExecutionStage::Resolving;
    std::optional<std::string> observedStopReason;
    // 接纳的当前曲目或原生供给播放的原生引用；资源地址条目为空
    std::string nativeRef;
};

using Payload = std::variant<api::jitSession::StateChangedPayload, api::jitSession::NeedNextPayload,
                             api::jitSession::OperationCompletedPayload, api::jitSession::AttemptFinishedPayload,
                             api::jitSession::AttemptRecheckedPayload, api::jitSession::ControlChangedPayload>;

struct Delivery {
    Document document;
    Payload payload;
};

// 会话记录只接收已经核实的文档和播放事实；调用方持有 QueueManager 的锁。
struct Session {
    std::string id;
    std::string token;
    Document owner;
    bool ownerConnected = true;
    std::int64_t ownerEpoch = 1;
    std::int64_t revision = 1;
    std::int64_t eventSeq = 0;
    std::string phase = "active";
    std::optional<std::string> phaseReason;
    std::optional<Entry> current;
    std::optional<Entry> next;
    bool nextReady = false;
    bool endOfQueue = false;
    bool started = false;
    std::optional<std::string> neededAfterAttemptId;
    std::optional<std::string> neededRequestKey;
    std::optional<double> position;
    std::optional<double> duration;
    std::chrono::steady_clock::time_point releasedAt{};
    jit_session::RequestLog requests;
    jit_session::OperationLog operations;
    std::unordered_map<std::uint64_t, std::string> opBySerial;
    std::deque<Delivery> events;
    // 原生上下文的摘要，由 QueueManager 在上下文变化时同步
    std::optional<api::jitSession::JitNativeContextSummary> nativeContext;

    bool Expired(std::chrono::steady_clock::time_point now) const {
        return phase == "released" && now - releasedAt >= std::chrono::minutes(10);
    }

    // 逻辑 current 可先于原生播放改变；只有实际交给播放器后，EOF 才可能属于这个目标。
    void PlaybackSubmitted(std::uint64_t serial) {
        if (phase != "active") return;
        if (current && current->serial == serial) current->execution = ExecutionStage::Submitted;
        else if (next && next->serial == serial) next->execution = ExecutionStage::Submitted;
    }

    bool CurrentPlaybackSubmitted() const {
        return current && current->execution != ExecutionStage::Resolving;
    }

    api::jitSession::JitSessionSnapshot Snapshot() const {
        api::jitSession::JitSessionSnapshot result;
        result.sessionId = id;
        result.revision = revision;
        result.lastEventSeq = eventSeq;
        result.ownerEpoch = ownerEpoch;
        result.ownerState = phase == "released" ? "none" : ownerConnected ? "connected" : "disconnected";
        result.phase = phase;
        result.phaseReason = phaseReason;
        if (current) {
            api::jitSession::JitSessionAttempt attempt{current->id, current->attemptId, current->title, current->status, {}};
            if (!current->nativeRef.empty()) attempt.nativeRef = current->nativeRef;
            result.current = std::move(attempt);
        }
        if (next) {
            api::jitSession::JitSessionNextEntry entry{next->id, next->title, next->operationId, {}};
            if (!next->nativeRef.empty()) entry.nativeRef = next->nativeRef;
            result.next = std::move(entry);
        }
        result.supplyState = endOfQueue ? "exhausted" : next ? nextReady ? "ready" : "preparing" : "needed";
        result.neededAfterAttemptId = neededAfterAttemptId;
        result.neededRequestKey = neededRequestKey;
        const auto pending = operations.PendingIds();
        result.pendingOperations.assign(pending.begin(), pending.end());
        result.nativeContext = nativeContext;
        return result;
    }

    static api::jitSession::JitSessionOperation Operation(const jit_session::Operation& op) {
        api::jitSession::JitSessionOperation result;
        result.operationId = op.id;
        result.kind = op.kind == jit_session::OpKind::Play ? "play" : op.kind == jit_session::OpKind::Skip ? "skip" : "setNext";
        result.entryId = op.entryId;
        if (!op.attemptId.empty()) result.attemptId = op.attemptId;
        switch (op.status) {
            case jit_session::OpStatus::Pending: result.status = "pending"; break;
            case jit_session::OpStatus::Completed: result.status = "completed"; break;
            case jit_session::OpStatus::Failed: result.status = "failed"; break;
            case jit_session::OpStatus::Superseded: result.status = "superseded"; break;
        }
        if (!op.reason.empty()) result.reason = op.reason;
        return result;
    }

    template <class P>
    void Emit(P payload) {
        payload.sessionId = id;
        payload.revision = revision;
        payload.eventSeq = ++eventSeq;
        if constexpr (std::is_same_v<P, api::jitSession::StateChangedPayload>) payload.state = Snapshot();
        events.push_back({owner, std::move(payload)});
    }

    void PublishState() { Emit(api::jitSession::StateChangedPayload{}); }

    bool FinishOperation(const std::string& operationId, jit_session::OpStatus status, std::string reason = {}) {
        if (!operations.Finish(operationId, status, std::move(reason))) return false;
        api::jitSession::OperationCompletedPayload event;
        event.operation = Operation(*operations.Find(operationId));
        Emit(std::move(event));
        return true;
    }

    void SupersedePending(const std::string& reason) {
        for (const auto& operationId : operations.PendingIds()) {
            FinishOperation(operationId, jit_session::OpStatus::Superseded, reason);
        }
    }

    void EndCurrent(const std::string& outcome, std::optional<std::string> stopReason = {}) {
        if (!current || current->status == "ended") return;
        current->status = "ended";
        api::jitSession::AttemptFinishedPayload event;
        event.attemptId = current->attemptId;
        event.entryId = current->id;
        event.outcome = outcome;
        event.stopReason = std::move(stopReason);
        event.position = position;
        event.duration = duration;
        Emit(std::move(event));
    }

    void ClearRequest() {
        neededAfterAttemptId.reset();
        neededRequestKey.reset();
    }

    void RequestNext() {
        if (phase != "active" || !current || next || endOfQueue || neededAfterAttemptId) return;
        neededAfterAttemptId = current->attemptId;
        neededRequestKey = id + ":" + std::to_string(eventSeq + 1);
        api::jitSession::NeedNextPayload event;
        event.afterAttemptId = *neededAfterAttemptId;
        event.requestKey = *neededRequestKey;
        Emit(std::move(event));
    }

    void ControlChanged(const std::string& reason, const std::optional<Document>& previous = {}) {
        api::jitSession::ControlChangedPayload event;
        event.ownerEpoch = ownerEpoch;
        event.reason = reason;
        Emit(std::move(event));
        if (previous && *previous != owner) events.push_back({*previous, events.back().payload});
    }
};

std::string NewId();

}  // namespace jit_session_runtime
