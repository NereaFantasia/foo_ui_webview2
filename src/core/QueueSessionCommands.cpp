#include "pch.h"
#include "core/QueueManager.h"
#include "core/QueueSessionInternal.h"
#include "api/ApiConstants.h"

namespace js = api::jitSession;
namespace protocol = jit_session_runtime;

api::Result<js::PlayResult> QueueManager::SessionPlay(const js::PlayParams& params, const CallerContext& caller) {
    SessionEventScope flush{*this};
    const auto document = protocol::Capture(caller);
    if (!document) return api::Fail("A trusted document is required", ApiErrorCode::NOT_OWNER);
    RefreshSessionOwner();
    nlohmann::json payload{{"entryId", params.entryId}, {"title", params.title}, {"location", params.location}};
    api::results::Put(payload, "expiresAt", params.expiresAt);
    js::PlayResult result;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const auto guard = CheckSessionWrite(params.sessionId, params.controlToken, params.expectedRevision,
                                              params.requestId, "play", payload, *document);
        if (guard.failure) return *guard.failure;
        if (guard.replay) return protocol::Replay<js::PlayResult>(*guard.replay);
        if (params.location.size() > ApiLimits::MAX_STREAM_URL_LENGTH) return api::Fail(ApiError::URL_TOO_LONG, ApiErrorCode::INVALID_PARAMS);
        if (NativeRecovering()) {
            return api::Fail("Release the native context before playing", ApiErrorCode::CONFLICT,
                             {{"details", {{"reason", "recoveryPending"}}}});
        }
        if (m_native && !m_native->ledger.EntryAvailableForLocation(params.entryId)) {
            return api::Fail("The entry is bound to a native reference", ApiErrorCode::CONFLICT, {{"details", {{"reason", "entryBound"}}}});
        }
        auto& session = *m_protocol;
        ++session.revision;
        session.SupersedePending("play");
        session.EndCurrent("superseded");
        session.current = protocol::Entry{params.entryId, params.title, params.location, params.expiresAt,
                                          protocol::NewId(), protocol::NewId()};
        session.next.reset();
        session.nextReady = false;
        session.endOfQueue = false;
        session.ClearRequest();
        session.position.reset();
        session.duration.reset();
        session.phase = "active";
        session.phaseReason.reset();
        session.started = true;
        DiscardPreparedNative();
        const auto& entry = *session.current;
        session.operations.Begin({entry.operationId, jit_session::OpKind::Play, entry.id, entry.attemptId});
        result = {entry.operationId, entry.attemptId, session.revision};
        session.requests.Record(guard.key, guard.digest, js::ToJson(result).dump());
        session.PublishState();
    }
    try {
        WithdrawNativeTicket();
        StartCurrentEntry(params.entryId, params.title, params.location);
    } catch (...) {
        FailSessionOperation(result.operationId, "startFailed");
    }
    return result;
}

api::Result<js::SetNextResult> QueueManager::SessionSetNext(const js::SetNextParams& params, const CallerContext& caller) {
    SessionEventScope flush{*this};
    const auto document = protocol::Capture(caller);
    if (!document) return api::Fail("A trusted document is required", ApiErrorCode::NOT_OWNER);
    RefreshSessionOwner();
    nlohmann::json payload{{"afterAttemptId", params.afterAttemptId}, {"endOfQueue", params.endOfQueue}};
    api::results::Put(payload, "entryId", params.entryId);
    api::results::Put(payload, "title", params.title);
    api::results::Put(payload, "location", params.location);
    api::results::Put(payload, "expiresAt", params.expiresAt);
    js::SetNextResult result;
    const bool mayBePreselected = protocol::NextMayBePreselected();
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const auto guard = CheckSessionWrite(params.sessionId, params.controlToken, params.expectedRevision,
                                              params.requestId, "setNext", payload, *document);
        if (guard.failure) return *guard.failure;
        if (guard.replay) return protocol::Replay<js::SetNextResult>(*guard.replay);
        if (params.entryId && (params.entryId->empty() || !params.location || params.location->empty() ||
                               params.location->size() > ApiLimits::MAX_STREAM_URL_LENGTH || params.endOfQueue)) {
            return api::Fail("A next entry needs an identifier and a location, without endOfQueue", ApiErrorCode::INVALID_PARAMS);
        }
        if (!params.entryId && (params.location || params.title || params.expiresAt)) {
            return api::Fail("Withdrawing the next entry does not accept resource fields", ApiErrorCode::INVALID_PARAMS);
        }
        if (params.entryId && m_native && !m_native->ledger.EntryAvailableForLocation(*params.entryId)) {
            return api::Fail("The entry is bound to a native reference", ApiErrorCode::CONFLICT, {{"details", {{"reason", "entryBound"}}}});
        }
        if (NativeRecovering()) {
            return api::Fail("Release the native context first", ApiErrorCode::CONFLICT, {{"details", {{"reason", "recoveryPending"}}}});
        }
        auto& session = *m_protocol;
        if (session.phase != "active" || !session.current) return api::Fail("The session has no active attempt", ApiErrorCode::NO_ACTIVE_ITEM);
        if (session.current->attemptId != params.afterAttemptId) return api::Fail("The current attempt changed", ApiErrorCode::CONFLICT);
        if (auto failure = CheckNextWithdrawable(mayBePreselected)) return *failure;
        ++session.revision;
        if (session.next) session.FinishOperation(session.next->operationId, jit_session::OpStatus::Superseded, "setNext");
        session.next.reset();
        session.nextReady = false;
        session.endOfQueue = params.endOfQueue;
        session.ClearRequest();
        if (params.entryId) {
            session.next = protocol::Entry{*params.entryId, params.title.value_or(""), *params.location,
                                           params.expiresAt, {}, protocol::NewId()};
            const auto& entry = *session.next;
            session.operations.Begin({entry.operationId, jit_session::OpKind::SetNext, entry.id, {}});
            result.operationId = entry.operationId;
        }
        result.revision = session.revision;
        session.requests.Record(guard.key, guard.digest, js::ToJson(result).dump());
        session.RequestNext();
        session.PublishState();
    }
    try {
        WithdrawNextEntry();
        if (params.entryId) BufferNextEntry(*params.entryId, params.title.value_or(""), *params.location);
        else if (params.endOfQueue) m_state = State::Exhausted;
        else m_state = State::WaitingNext;
    } catch (...) {
        if (result.operationId) FailSessionOperation(*result.operationId, "bufferUpdateFailed");
        else return api::Fail("The next buffer could not be withdrawn", ApiErrorCode::OPERATION_FAILED);
    }
    return result;
}

api::Result<js::SkipResult> QueueManager::SessionSkip(const js::SkipParams& params, const CallerContext& caller) {
    SessionEventScope flush{*this};
    const auto document = protocol::Capture(caller);
    if (!document) return api::Fail("A trusted document is required", ApiErrorCode::NOT_OWNER);
    RefreshSessionOwner();
    const auto playlistIndex = m_shadowPlaylistIndex.load();
    const bool rowsValid = playlistIndex < playlist_manager::get()->get_playlist_count() && VerifyRows(playlistIndex);
    size_t row = jit_shadow::kNoRow;
    js::SkipResult result;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const auto guard = CheckSessionWrite(params.sessionId, params.controlToken, params.expectedRevision,
                                              params.requestId, "skip", {{"attemptId", params.attemptId}}, *document);
        if (guard.failure) return *guard.failure;
        if (guard.replay) return protocol::Replay<js::SkipResult>(*guard.replay);
        if (NativeRecovering()) {
            return api::Fail("Release the native context first", ApiErrorCode::CONFLICT, {{"details", {{"reason", "recoveryPending"}}}});
        }
        auto& session = *m_protocol;
        if (!session.current || session.current->attemptId != params.attemptId) return api::Fail("The current attempt changed", ApiErrorCode::CONFLICT);
        if (session.phase != "active" || session.current->status != "playing" || !session.next || !session.nextReady || !rowsValid) {
            return api::Fail("There is no ready next entry", ApiErrorCode::NO_ACTIVE_ITEM);
        }
        row = m_rows.FirstRowOf(session.next->serial);
        if (row == jit_shadow::kNoRow) return api::Fail("There is no ready next entry", ApiErrorCode::NO_ACTIVE_ITEM);
        ++session.revision;
        session.EndCurrent("superseded");
        session.current = std::move(session.next);
        session.next.reset();
        auto& current = *session.current;
        current.attemptId = protocol::NewId();
        current.operationId = protocol::NewId();
        current.status = "preparing";
        session.operations.Begin({current.operationId, jit_session::OpKind::Skip, current.id, current.attemptId});
        session.opBySerial[current.serial] = current.operationId;
        session.nextReady = false;
        session.ClearRequest();
        session.position.reset();
        session.duration.reset();
        result = {current.operationId, session.revision};
        session.requests.Record(guard.key, guard.digest, js::ToJson(result).dump());
        session.PublishState();
    }
    try {
        // 起播会保存并放回原生队列；首次衔接的自有队列项先撤回，免得下一条被再播一次。
        WithdrawNativeTicket();
        ActivatePlaybackOnShadow(playlistIndex, row);
    } catch (...) {
        FailSessionOperation(result.operationId, "startFailed");
    }
    return result;
}

api::Result<js::StopResult> QueueManager::SessionStop(const js::StopParams& params, const CallerContext& caller) {
    SessionEventScope flush{*this};
    const auto document = protocol::Capture(caller);
    if (!document) return api::Fail("A trusted document is required", ApiErrorCode::NOT_OWNER);
    RefreshSessionOwner();
    js::StopResult result;
    bool stopPlayback = false;
    bool clearBuffer = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const auto guard = CheckSessionWrite(params.sessionId, params.controlToken, params.expectedRevision,
                                              params.requestId, "stop", nlohmann::json::object(), *document);
        if (guard.failure) return *guard.failure;
        if (guard.replay) return protocol::Replay<js::StopResult>(*guard.replay);
        if (NativeRecovering()) {
            return api::Fail("Release the native context first", ApiErrorCode::CONFLICT, {{"details", {{"reason", "recoveryPending"}}}});
        }
        auto& session = *m_protocol;
        stopPlayback = session.started && session.phase == "active";
        clearBuffer = session.started;
        ++session.revision;
        session.phase = "stopped";
        session.phaseReason = "command";
        session.SupersedePending("stop");
        session.EndCurrent("stopped");
        session.next.reset();
        session.nextReady = false;
        session.ClearRequest();
        DiscardPreparedNative();
        result.revision = session.revision;
        session.requests.Record(guard.key, guard.digest, js::ToJson(result).dump());
        session.PublishState();
    }
    if (stopPlayback) StopInternal(true);
    else if (clearBuffer) ClearInternal();
    bool native;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        native = NativeOpen();
    }
    if (native) {
        // 交还失败只记在上下文里，停止本身照常成功。
        RestoreNativeContext(false);
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_protocol) {
            ++m_protocol->revision;
            m_protocol->PublishState();
        }
    }
    return result;
}

namespace {

// release 的应答在动作完成之后才记录：成功记 {}，失败记错误信封，同一 requestId 重试原样
// 重放，不再执行，也不会把失败重放成成功。
std::string ReleaseRecord(const api::Failure& failure) {
    return nlohmann::json{{"error", failure.error}, {"code", failure.code}, {"extra", failure.extra}}.dump();
}

api::Result<void> ReplayRelease(const std::string& response) {
    const auto value = nlohmann::json::parse(response);
    if (!value.contains("code")) return api::Ok();
    return api::Fail(value.at("error").get<std::string>(), value.at("code").get<std::string>(),
                     value.value("extra", nlohmann::json::object()).get<nlohmann::json::object_t>());
}

}  // namespace

api::Result<void> QueueManager::SessionRelease(const js::ReleaseParams& params, const CallerContext& caller) {
    SessionEventScope flush{*this};
    const auto document = protocol::Capture(caller);
    if (!document) return api::Fail("A trusted document is required", ApiErrorCode::NOT_OWNER);
    RefreshSessionOwner();
    bool clearBuffer = false;
    nlohmann::json payload = nlohmann::json::object();
    api::results::Put(payload, "nativeDisposition", params.nativeDisposition);
    bool native = false;
    std::string key;
    std::uint64_t digest = 0;
    const bool mayBePreselected = protocol::NextMayBePreselected();
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const auto guard = CheckSessionWrite(params.sessionId, params.controlToken, params.expectedRevision,
                                              params.requestId, "release", payload, *document, true);
        if (guard.failure) return *guard.failure;
        if (guard.replay) return ReplayRelease(*guard.replay);
        key = guard.key;
        digest = guard.digest;
        native = NativeOpen();
        if (params.nativeDisposition && (!m_native || m_native->ledger.GetStatus() == jit_native::Status::Prepared)) {
            return api::Fail("Only a session that adopted native playback takes nativeDisposition", ApiErrorCode::INVALID_PARAMS);
        }
        // 已选定的下一条会照常起播，交还时再把它放回原生队列就会播两次。
        if (native && params.nativeDisposition.value_or("restore") != "detach") {
            if (auto failure = CheckNextWithdrawable(mayBePreselected)) return *failure;
        }
    }
    const auto recordFailure = [&](api::Failure failure) -> api::Result<void> {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_protocol) {
            m_protocol->requests.Record(key, digest, ReleaseRecord(failure));
            ++m_protocol->revision;
            m_protocol->PublishState();
        }
        return failure;
    };
    if (native) {
        // 交还失败时会话保留；新的 requestId 可以从中断处继续。
        if (auto failure = RestoreNativeContext(params.nativeDisposition.value_or("restore") == "detach")) {
            return recordFailure(std::move(*failure));
        }
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_protocol) return api::Fail("The JIT session was not found", ApiErrorCode::NOT_FOUND);
        auto& session = *m_protocol;
        // 旧入口接管后影子缓冲归 jitQueue 所用，释放不清它。
        const bool legacyOwnsBuffer = session.phaseReason == "legacyTakeover";
        ++session.revision;
        session.SupersedePending("release");
        session.EndCurrent("released");
        session.phase = "released";
        session.phaseReason = "command";
        session.releasedAt = std::chrono::steady_clock::now();
        session.next.reset();
        session.nextReady = false;
        session.ClearRequest();
        DiscardPreparedNative();
        session.ControlChanged("released");
        session.PublishState();
        clearBuffer = session.started && !legacyOwnsBuffer;
    }
    if (clearBuffer) {
        try {
            ClearInternal();
        } catch (...) {
            return recordFailure(api::Fail("The buffer could not be cleared", ApiErrorCode::OPERATION_FAILED));
        }
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_protocol) m_protocol->requests.Record(key, digest, "{}");
    return api::Ok();
}

void QueueManager::LegacyTakeover() {
    SessionEventScope flush{*this};
    // 旧调用要清空或重写影子列表：首次衔接的自有队列项先撤回，免得它指向的行被换成别的曲目。
    WithdrawNativeTicket();
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_protocol || !ProtocolSchedules()) return;
    auto& session = *m_protocol;
    ++session.revision;
    session.SupersedePending("legacyTakeover");
    session.EndCurrent("superseded");
    if (m_native && NativeOpen()) {
        // 接管的上下文还没交还：旧调用照常执行，会话转为只剩交还资格的 stopped。控制者保留令牌，
        // 之后只能 release（restore 或 detach）；旧动作改变的事实不在后台拿旧快照覆盖。
        m_native->ledger.Conflict();
        SyncNativeSummary();
        session.phase = "stopped";
        session.phaseReason = "legacyTakeover";
        session.next.reset();
        session.nextReady = false;
        session.ClearRequest();
        m_generations.EndSession();
        session.PublishState();
        return;
    }
    session.phase = "released";
    session.phaseReason = "legacyTakeover";
    session.token.clear();
    session.releasedAt = std::chrono::steady_clock::now();
    session.ClearRequest();
    m_generations.EndSession();
    DiscardPreparedNative();
    session.ControlChanged("legacyTakeover");
    session.PublishState();
}

std::optional<api::Failure> QueueManager::CheckNextWithdrawable(bool mayBePreselected) const {
    if (!mayBePreselected || !m_nextEntry || m_rows.FirstRowOf(m_nextEntry) == jit_shadow::kNoRow) return std::nullopt;
    return api::Fail("The buffered next entry may already be picked to follow the current track", ApiErrorCode::CONFLICT,
                     {{"details", {{"reason", "nextPreselected"}}}});
}

void QueueManager::WithdrawNextEntry() {
    WithdrawNativeTicket();
    jit_shadow::EntrySerial entry;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        entry = m_nextEntry;
        m_generations.TakeNext();
        m_nextEntry = 0;
        m_nextTrackId.clear();
        m_nextTitle.clear();
        m_needNextRequested = false;
    }
    const auto playlistIndex = m_shadowPlaylistIndex.load();
    if (!entry || playlistIndex >= playlist_manager::get()->get_playlist_count() || !VerifyRows(playlistIndex)) return;
    size_t first;
    size_t last;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        first = m_rows.FirstRowOf(entry);
        last = m_rows.LastRowOf(entry);
    }
    if (first == jit_shadow::kNoRow || last == jit_shadow::kNoRow) return;
    playlist_manager::get()->playlist_remove_items(playlistIndex, pfc::bit_array_range(first, last - first + 1));
    std::lock_guard<std::mutex> lock(m_mutex);
    m_rows.Remove(first, last - first + 1);
}

void QueueManager::NotifyEmpty() {
    LegacyTakeover();
    NotifyListExhausted();
}
