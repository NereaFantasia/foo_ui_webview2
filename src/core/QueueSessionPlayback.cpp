#include "pch.h"
#include "core/QueueManager.h"
#include "core/QueueSessionInternal.h"
#include "utils/SubsongUtils.h"

#include <cstring>

namespace protocol = jit_session_runtime;

void QueueManager::BindSessionEntry(jit_shadow::EntrySerial entry, jit_shadow::Slot slot) {
    if (!m_protocol || !ProtocolSchedules()) return;
    auto& target = slot == jit_shadow::Slot::Current ? m_protocol->current : m_protocol->next;
    if (!target) return;
    target->serial = entry;
    m_protocol->opBySerial[entry] = target->operationId;
}

bool QueueManager::SessionOwnsEntry(jit_shadow::EntrySerial entry) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_protocol && m_protocol->opBySerial.contains(entry);
}

void QueueManager::FailSessionOperation(const std::string& operationId, const std::string& reason) {
    SessionEventScope flush{*this};
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_protocol) return;
    auto& session = *m_protocol;
    const auto* operation = session.operations.Find(operationId);
    if (!operation || operation->status != jit_session::OpStatus::Pending) return;
    ++session.revision;
    // 已交给播放器的手动出现等不到起播：结局不明，不再当作未播交还。
    for (const auto* entry : {session.current ? &*session.current : nullptr, session.next ? &*session.next : nullptr}) {
        if (!entry || entry->operationId != operationId || entry->nativeRef.empty() || !m_native) continue;
        const auto* occurrence = m_native->ledger.OccurrenceOfRef(entry->nativeRef);
        if (occurrence && occurrence->disposition == jit_native::Disposition::Submitted) {
            const std::string id = occurrence->id;
            m_native->ledger.Indeterminate(id);
            SyncNativeSummary();
        }
    }
    session.FinishOperation(operationId, jit_session::OpStatus::Failed, reason);
    if (session.current && session.current->operationId == operationId) session.EndCurrent("unknown");
    if (session.next && session.next->operationId == operationId) {
        session.next.reset();
        session.nextReady = false;
        session.ClearRequest();
    }
    session.RequestNext();
    session.PublishState();
}

bool QueueManager::ReportSessionAdd(jit_shadow::EntrySerial entry, bool added, const std::string& reason) {
    SessionEventScope flush{*this};
    std::string operationId;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_protocol) return false;
        const auto found = m_protocol->opBySerial.find(entry);
        if (found == m_protocol->opBySerial.end()) return false;
        operationId = found->second;
    }
    if (!added) {
        FailSessionOperation(operationId, reason);
        return true;
    }
    bool startNext = false;
    bool ticket = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto& session = *m_protocol;
        const auto* operation = session.operations.Find(operationId);
        if (!operation || operation->status != jit_session::OpStatus::Pending || session.phase != "active") return true;
        // 当前条目等 new_track 确认起播后才完成；预载下一条只需确认插入。
        if (session.next && session.next->serial == entry) {
            ++session.revision;
            session.nextReady = true;
            session.FinishOperation(operationId, jit_session::OpStatus::Completed);
            startNext = session.current && session.current->status == "ended";
            ticket = !startNext && m_native && m_native->ledger.GetStatus() == jit_native::Status::Managed;
            session.PublishState();
        }
    }
    if (ticket) QueueNativeTicket(entry);
    if (startNext) {
        const auto playlistIndex = m_shadowPlaylistIndex.load();
        size_t row = jit_shadow::kNoRow;
        if (playlistIndex < playlist_manager::get()->get_playlist_count() && VerifyRows(playlistIndex)) {
            std::lock_guard<std::mutex> lock(m_mutex);
            row = m_rows.FirstRowOf(entry);
        }
        if (row != jit_shadow::kNoRow) ActivatePlaybackOnShadow(playlistIndex, row);
        else SessionPlaybackExternal();
    }
    return true;
}

void QueueManager::SessionTrackStarted(jit_shadow::Attribution attribution) {
    SessionEventScope flush{*this};
    RefreshSessionOwner();
    const auto playback = playback_control::get();
    const double position = playback->playback_get_position();
    const double duration = playback->playback_get_length();
    std::lock_guard<std::mutex> lock(m_mutex);
    // 新曲目已开始：首次衔接的队列项已被取走或不再适用。
    m_nativeTicket.reset();
    if (!m_protocol || !ProtocolSchedules()) return;
    auto& session = *m_protocol;
    if (!session.current) return;
    auto& current = *session.current;
    if (current.serial != m_currentEntry && (!session.next || session.next->serial != m_currentEntry)) return;
    ++session.revision;
    if (current.serial != m_currentEntry) {
        session.FinishOperation(current.operationId, jit_session::OpStatus::Superseded, "advanced");
        session.EndCurrent("superseded", current.observedStopReason);
        current = std::move(*session.next);
        session.next.reset();
        current.attemptId = protocol::NewId();
    } else if (current.status != "preparing") {
        session.EndCurrent("superseded", current.observedStopReason);
        current.attemptId = protocol::NewId();
        current.operationId.clear();
    }
    current.status = "playing";
    current.execution = protocol::ExecutionStage::Playing;
    // 原生引用起播：手动出现记为已消费，来源行移动续播位置。
    if (!current.nativeRef.empty() && m_native && m_native->ledger.Started(current.nativeRef)) SyncNativeSummary();
    current.observedStopReason.reset();
    session.phase = "active";
    session.phaseReason.reset();
    session.position = position;
    session.duration = duration;
    if (attribution == jit_shadow::Attribution::Advance) session.nextReady = false;
    session.ClearRequest();
    if (!current.operationId.empty()) {
        session.FinishOperation(current.operationId, jit_session::OpStatus::Completed);
    }
    session.RequestNext();
    session.PublishState();
}

void QueueManager::SessionPlaybackExternal() {
    SessionEventScope flush{*this};
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_protocol || m_protocol->phase != "active") return;
    auto& session = *m_protocol;
    if (!session.current) return;
    ++session.revision;
    session.phase = "stopped";
    session.phaseReason = "external";
    session.EndCurrent("superseded", session.current->observedStopReason);
    session.SupersedePending("external");
    session.next.reset();
    session.nextReady = false;
    session.ClearRequest();
    // 外部改播：接管的账不再推进，也不自动交还，等调用方 release。
    if (m_native && m_native->ledger.GetStatus() == jit_native::Status::Managed) {
        m_native->ledger.Conflict();
        SyncNativeSummary();
    }
    session.PublishState();
}

bool QueueManager::SessionPlaybackStopped(play_control::t_stop_reason reason) {
    SessionEventScope flush{*this};
    bool restore = false;
    std::optional<Recheck> recheck;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_protocol || !ProtocolSchedules()) return false;
        auto& session = *m_protocol;
        if (!session.current || !session.started) return false;
        if (session.phase != "active") return true;
        if (reason == play_control::stop_reason_starting_another) {
            if (session.current->status == "playing") session.current->observedStopReason = "startingAnother";
            return true;
        }
        if (reason == play_control::stop_reason_eof && !session.CurrentPlaybackSubmitted()) return true;
        if (reason == play_control::stop_reason_eof && session.current->status == "ended") return true;
        ++session.revision;
        if (reason == play_control::stop_reason_eof) {
            if (session.current->status == "preparing") {
                session.FinishOperation(session.current->operationId, jit_session::OpStatus::Failed, "playbackEndedBeforeStart");
            }
            const auto outcome = jit_session::ClassifyStop(jit_session::StopKind::EndOfFile,
                                                           session.position.value_or(0), session.duration.value_or(0));
            session.EndCurrent(protocol::OutcomeName(outcome), "eof");
            if (outcome == jit_session::Outcome::Unknown && session.position.value_or(0) < jit_session::kRecheckBefore) {
                const auto& ended = *session.current;
                recheck = Recheck{session.id, ended.attemptId, ended.id, ended.location, ended.serial};
            }
            m_state = session.endOfQueue ? State::Exhausted : State::WaitingNext;
            session.RequestNext();
        } else {
            const bool user = reason == play_control::stop_reason_user;
            session.phase = "stopped";
            session.phaseReason = user ? "user" : "external";
            session.EndCurrent(user ? "stopped" : "unknown", user ? "user" : "shuttingDown");
            session.SupersedePending(user ? "user" : "shutdown");
            if (!session.nextReady) session.next.reset();
            session.ClearRequest();
            m_generations.EndSession();
            m_state = State::Idle;
            restore = user;
        }
        session.PublishState();
    }
    if (recheck) RecheckAttempt(*recheck);
    if (restore) {
        RestoreUserContext();
        // 用户停止：在回调之外的主线程步骤里交还未来的出现，绝不起播。
        fb2k::inMainThread([this] {
            SessionEventScope flush{*this};
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (!m_protocol || m_protocol->phase != "stopped" || !NativeOpen()) return;
            }
            RestoreNativeContext(false);
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_protocol) {
                ++m_protocol->revision;
                m_protocol->PublishState();
            }
        });
    }
    return true;
}

void QueueManager::SessionPlaybackTime(double position, double duration) {
    SessionEventScope flush{*this};
    RefreshSessionOwner();
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_protocol || m_protocol->phase != "active" || !m_protocol->current || m_protocol->current->status != "playing") return;
    m_protocol->position = position;
    m_protocol->duration = duration;
}

namespace {

struct RecheckResult {
    bool aborted = false;
    bool reproduced = false;
    std::string reason;
    std::string stage;
};

// 内核抛出的异常对象没有 RTTI（typeid 会再抛），只能按 SDK 声明的类型逐级 catch。
// exception_io_net_credentials 派生自 exception_io_denied，归拒绝访问；not_found 与 data 是兄弟。
const char* RecheckReason(const std::exception_ptr& error) {
    try { std::rethrow_exception(error); }
    catch (const exception_io_denied&) { return "accessDenied"; }
    catch (const exception_io_not_found&) { return "notFound"; }
    catch (const exception_io_data&) { return "unsupportedContent"; }
    catch (const exception_io_net&) { return "network"; }
    catch (const exception_io_network_not_reachable&) { return "network"; }
    catch (const exception_io_timeout&) { return "network"; }
    catch (...) { return "other"; }
}

// 按播放引擎的顺序重开：打开、初始化、解码第一块。没解出任何一块也算重现。
RecheckResult RecheckResource(const char* path, t_uint32 subsong, abort_callback& abort) {
    RecheckResult result;
    const char* stage = "open";
    try {
        input_decoder::ptr decoder;
        input_entry::g_open_for_decoding(decoder, nullptr, path, abort);
        stage = "initialize";
        decoder->initialize(subsong, input_flag_playback, abort);
        stage = "decode";
        audio_chunk_impl chunk;
        if (decoder->run(chunk, abort)) return result;
        result.reproduced = true;
        result.reason = "other";
        result.stage = stage;
    } catch (const exception_aborted&) {
        result.aborted = true;
    } catch (...) {
        result.reproduced = true;
        result.reason = RecheckReason(std::current_exception());
        result.stage = stage;
    }
    return result;
}

class EngineFailureWatch : public console_receiver {
public:
    void print(const char* message, t_size length) override {
        try {
            const std::string_view text(message, length == SIZE_MAX ? std::strlen(message) : length);
            if (const auto path = jit_session::EngineFailurePath(text)) g_QueueManager.NoteEngineFailure(*path);
        } catch (...) {}
    }
};

FB2K_SERVICE_FACTORY(EngineFailureWatch);

}  // namespace

void QueueManager::NoteEngineFailure(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_engineFailuresMutex);
    m_engineFailures.emplace_back(std::chrono::steady_clock::now(), jit_session::ComparableResourcePath(path));
    if (m_engineFailures.size() > 32) m_engineFailures.pop_front();
}

bool QueueManager::EngineReported(const std::string& path) const {
    // 失败行在停止前写入；复查网络错误最长约十秒，一分钟内的记录都算这次尝试的。
    const auto since = std::chrono::steady_clock::now() - std::chrono::minutes(1);
    const auto key = jit_session::ComparableResourcePath(path);
    std::lock_guard<std::mutex> lock(m_engineFailuresMutex);
    return std::any_of(m_engineFailures.begin(), m_engineFailures.end(),
                       [&](const auto& item) { return item.first >= since && item.second == key; });
}

void QueueManager::RecheckAttempt(const Recheck& job) {
    // 优先复查这次实际播放的那一行；行已不在（接纳的原生曲目、已清理）时退回调用方给的地址。
    std::string path;
    t_uint32 subsong = 0;
    size_t row = jit_shadow::kNoRow;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (job.serial) row = m_rows.FirstRowOf(job.serial);
    }
    metadb_handle_ptr handle;
    const size_t shadow = m_shadowPlaylistIndex.load();
    if (row != jit_shadow::kNoRow && shadow < playlist_manager::get()->get_playlist_count() &&
        playlist_manager::get()->playlist_get_item_handle(handle, shadow, row)) {
        path = handle->get_path();
        subsong = handle->get_subsong_index();
    } else {
        std::tie(path, subsong) = SubsongUtils::ParseSubsongPath(job.location);
    }
    // 后台线程的入口不接异常，逃出即终止进程；两段回调出错都放弃这次复查，不发事件。
    // job、result 用初始化捕获：直接捕获 const 对象时闭包移动要复制字符串，可能抛出。
    fb2k::splitTask([this, job = job, path, subsong] {
        try {
            pfc::string8 canonical = path.c_str();
            try { filesystem::g_get_canonical_path(path.c_str(), canonical); } catch (...) {}
            const auto result = RecheckResource(canonical.c_str(), subsong, fb2k::mainAborter());
            if (result.aborted) return;
            fb2k::inMainThread([this, job, path, result = result] {
                try {
                    SessionEventScope flush{*this};
                    const bool reported = EngineReported(path);
                    std::lock_guard<std::mutex> lock(m_mutex);
                    if (!m_protocol || m_protocol->id != job.sessionId) return;
                    api::jitSession::AttemptRecheckedPayload event;
                    event.attemptId = job.attemptId;
                    event.entryId = job.entryId;
                    event.result = result.reproduced ? "reproduced" : "notReproduced";
                    if (result.reproduced) {
                        event.reason = result.reason;
                        event.stage = result.stage;
                    }
                    event.engineReported = reported;
                    m_protocol->Emit(std::move(event));
                } catch (...) {}
            });
        } catch (...) {}
    });
}
