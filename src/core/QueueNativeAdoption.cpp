#include "pch.h"
#include "core/QueueManager.h"
#include "core/QueueSessionInternal.h"
#include "utils/GuidUtils.h"
#include "utils/StringUtils.h"

#include <map>
#include <unordered_set>

// ============================================
// QueueNativeAdoption.cpp - jitSession 原生接管：快照、接纳、首次衔接与交还
//
// prepareAdoption 在一次主线程调用里读完当前曲目、原生手动队列、来源与设置，不写任何原生
// 状态；之后的失效靠观察版本比对发现。adoptNative 按取证验证过的顺序先清空原生队列，再把
// playing 切到空影子；交还时先把 playing 切回来源，再按出现账排回手动项并排入来源后继。
// 旧 jitQueue 入口介入时会话转为只剩交还资格的 stopped（见 LegacyTakeover）。
// ============================================

namespace js = api::jitSession;
namespace protocol = jit_session_runtime;

namespace {

constexpr size_t kMaxManual = 1024;
constexpr auto kSnapshotLifetime = std::chrono::seconds(30);

api::Failure NotSupported(const char* reason) {
    return api::Fail("The native playback cannot be adopted", ApiErrorCode::NOT_SUPPORTED,
                     {{"details", {{"reason", reason}}}});
}

size_t FindList(const std::string& guid) {
    GUID value{};
    return GuidUtils::StringToGuid(guid, value) ? playlist_manager_v5::get()->find_playlist_by_guid(value)
                                                : pfc::infinite_size;
}

std::string ListGuid(size_t index) {
    auto manager = playlist_manager_v5::get();
    return index < manager->get_playlist_count() ? GuidUtils::GuidToString(manager->playlist_get_guid(index)) : std::string();
}

std::string TitleOf(const metadb_handle_ptr& handle) {
    metadb_info_container::ptr info;
    if (handle->get_info_ref(info)) {
        if (const char* title = info->info().meta_get("title", 0); title && *title) return StringUtils::SafeUtf8(title);
    }
    const pfc::string8 name = pfc::string_filename_ext(handle->get_path());
    return StringUtils::SafeUtf8(name.get_ptr());
}

const char* StatusName(jit_native::Status status) {
    switch (status) {
        case jit_native::Status::Prepared: return "prepared";
        case jit_native::Status::Managed: return "managed";
        case jit_native::Status::Restoring: return "restoring";
        case jit_native::Status::Restored: return "restored";
        case jit_native::Status::Conflicted: return "conflicted";
        case jit_native::Status::Detached: return "detached";
    }
    return "conflicted";
}

const char* DispositionName(jit_native::Disposition disposition) {
    switch (disposition) {
        case jit_native::Disposition::Pending: return "pending";
        case jit_native::Disposition::Queued: return "queued";
        case jit_native::Disposition::Submitted: return "submitted";
        case jit_native::Disposition::Consumed: return "consumed";
        case jit_native::Disposition::Discarded: return "discarded";
        case jit_native::Disposition::Indeterminate: return "indeterminate";
        case jit_native::Disposition::Restored: return "restored";
    }
    return "indeterminate";
}

std::int64_t UnixMilliseconds(std::chrono::system_clock::time_point at) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(at.time_since_epoch()).count();
}

// 只读取宿主自己保存的应答，不解析页面输入。
js::PrepareAdoptionResult ReplayPrepare(const std::string& response) {
    const auto value = nlohmann::json::parse(response);
    const auto& current = value.at("current");
    js::JitNativeTrack track{current.at("nativeRef").get<std::string>(), current.at("path").get<std::string>(),
                             current.at("subsong").get<std::int64_t>(), current.at("title").get<std::string>(), {}, {}};
    if (!current.at("playlistGuid").is_null()) track.playlistGuid = current.at("playlistGuid").get<std::string>();
    if (!current.at("row").is_null()) track.row = current.at("row").get<std::int64_t>();
    return {value.at("contextId").get<std::string>(), value.at("snapshotToken").get<std::string>(),
            value.at("expiresAt").get<double>(), value.at("revision").get<std::int64_t>(),
            value.at("contextRevision").get<std::int64_t>(), std::move(track),
            value.at("manualCount").get<std::int64_t>(), value.at("sourceCount").get<std::int64_t>(),
            value.at("sourceNextOffset").get<std::int64_t>()};
}

struct NativeSample {
    metadb_handle_ptr current;
    size_t sourceIndex = 0;
    size_t currentRow = 0;
    size_t sourceCount = 0;
    std::string sourceGuid;
    std::vector<metadb_handle_ptr> manualHandles;
    std::vector<std::string> manualGuids;
    std::vector<std::optional<size_t>> manualRows;
};

// 一次主线程调用里读完接管需要的原生事实；任一条件不满足即返回 NOT_SUPPORTED，原因按检查顺序。
std::optional<api::Failure> ReadNativeSample(NativeSample& sample, size_t shadowIndex) {
    auto playback = playback_control::get();
    if (!playback->is_playing()) return NotSupported("notPlaying");
    if (playback->is_paused()) return NotSupported("paused");
    const auto settings = jit_native::SampleSettings();
    if (!settings) return NotSupported("settingsUnreadable");
    if (!settings->Supported()) return NotSupported("unsupportedSettings");

    auto manager = playlist_manager::get();
    size_t list = pfc::infinite_size;
    size_t row = pfc::infinite_size;
    metadb_handle_ptr atRow;
    if (!playback->get_now_playing(sample.current) || !manager->get_playing_item_location(&list, &row) ||
        manager->get_playing_playlist() != list || !manager->playlist_get_item_handle(atRow, list, row) ||
        atRow != sample.current) {
        return NotSupported("noPlaylistPosition");
    }
    if (list == shadowIndex || manager->playlist_lock_is_present(list)) return NotSupported("dynamicSource");
    sample.sourceIndex = list;
    sample.currentRow = row;
    sample.sourceCount = manager->playlist_get_item_count(list);
    sample.sourceGuid = ListGuid(list);

    pfc::list_t<t_playback_queue_item> queue;
    manager->queue_get_contents(queue);
    if (queue.get_count() > kMaxManual) return NotSupported("queueTooLarge");
    for (size_t i = 0; i < queue.get_count(); ++i) {
        const auto& item = queue[i];
        if (!item.m_handle.is_valid()) return NotSupported("queueItemMismatch");
        std::optional<size_t> itemRow;
        std::string guid;
        if (item.m_playlist != pfc::infinite_size || item.m_item != pfc::infinite_size) {
            metadb_handle_ptr listed;
            if (!manager->playlist_get_item_handle(listed, item.m_playlist, item.m_item) || listed != item.m_handle) {
                return NotSupported("queueItemMismatch");
            }
            itemRow = item.m_item;
            guid = ListGuid(item.m_playlist);
        }
        sample.manualHandles.push_back(item.m_handle);
        sample.manualGuids.push_back(std::move(guid));
        sample.manualRows.push_back(itemRow);
    }
    return std::nullopt;
}

js::JitNativeTrack TrackOf(const metadb_handle_ptr& handle, const std::string& ref, const std::string& guid,
                           std::optional<size_t> row) {
    js::JitNativeTrack track;
    track.nativeRef = ref;
    track.path = StringUtils::SafeUtf8(handle->get_path());
    track.subsong = handle->get_subsong_index();
    track.title = TitleOf(handle);
    if (!guid.empty()) track.playlistGuid = guid;
    if (row) track.row = static_cast<std::int64_t>(*row);
    return track;
}

js::JitNativeEntry EntryOf(js::JitNativeTrack track) {
    js::JitNativeEntry entry;
    entry.nativeRef = std::move(track.nativeRef);
    entry.path = std::move(track.path);
    entry.subsong = track.subsong;
    entry.title = std::move(track.title);
    entry.playlistGuid = std::move(track.playlistGuid);
    entry.row = track.row;
    return entry;
}

}  // namespace

std::optional<api::Failure> QueueManager::CheckAdoptionAdmission() {
    const auto& session = *m_protocol;
    if (session.started || session.current || session.next || session.phase != "active") {
        return api::Fail("The session has already played", ApiErrorCode::CONFLICT, {{"details", {{"reason", "sessionStarted"}}}});
    }
    if (m_native && m_native->ledger.GetStatus() != jit_native::Status::Prepared &&
        m_native->ledger.GetStatus() != jit_native::Status::Restored &&
        m_native->ledger.GetStatus() != jit_native::Status::Detached) {
        return api::Fail("An adopted native context is still open", ApiErrorCode::CONFLICT, {{"details", {{"reason", "contextOpen"}}}});
    }
    const size_t shadow = m_shadowPlaylistIndex.load();
    auto manager = playlist_manager::get();
    if (m_state != State::Idle || (shadow < manager->get_playlist_count() && manager->playlist_get_item_count(shadow) > 0)) {
        return api::Fail("The jitQueue buffer is in use", ApiErrorCode::CONFLICT, {{"details", {{"reason", "legacyQueueActive"}}}});
    }
    return std::nullopt;
}

namespace {

template <class Adoption>
js::JitNativeContextSummary SummaryOf(const Adoption& native) {
    const auto status = native.ledger.GetStatus();
    return {native.contextId, native.Revision(), StatusName(status), native.reason,
            status == jit_native::Status::Conflicted || status == jit_native::Status::Restoring ? "recovery" : "playback",
            static_cast<std::int64_t>(native.ledger.SourceNext())};
}

}  // namespace

void QueueManager::SyncNativeSummary() {
    if (!m_protocol) return;
    if (!m_native) m_protocol->nativeContext.reset();
    else m_protocol->nativeContext = SummaryOf(*m_native);
}

bool QueueManager::NativeRecovering() const {
    if (!m_native) return false;
    const auto status = m_native->ledger.GetStatus();
    return status == jit_native::Status::Restoring || status == jit_native::Status::Conflicted;
}

bool QueueManager::NativeOpen() const {
    if (!m_native) return false;
    const auto status = m_native->ledger.GetStatus();
    return status == jit_native::Status::Managed || status == jit_native::Status::Restoring ||
           status == jit_native::Status::Conflicted;
}

void QueueManager::RefreshNativeValidity() {
    if (!m_protocol || !m_native || m_native->reason || m_native->ledger.GetStatus() != jit_native::Status::Prepared) return;
    auto& native = *m_native;
    if (!jit_native::NativeObserver().Unchanged(native.stamp)) native.reason = "nativeChanged";
    else if (std::chrono::steady_clock::now() >= native.expires) native.reason = "snapshotExpired";
    else return;
    native.snapshotToken.clear();
    ++native.extraRevisions;
    auto& session = *m_protocol;
    ++session.revision;
    SyncNativeSummary();
    session.PublishState();
}

void QueueManager::DiscardPreparedNative() {
    if (!m_native || m_native->ledger.GetStatus() != jit_native::Status::Prepared) return;
    m_native.reset();
    SyncNativeSummary();
}

api::Result<js::PrepareAdoptionResult> QueueManager::SessionPrepareAdoption(const js::PrepareAdoptionParams& params,
                                                                           const CallerContext& caller) {
    SessionEventScope flush{*this};
    const auto document = protocol::Capture(caller);
    if (!document) return api::Fail("A trusted document is required", ApiErrorCode::NOT_OWNER);
    RefreshSessionOwner();
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto guard = CheckSessionWrite(params.sessionId, params.controlToken, params.expectedRevision,
                                         params.requestId, "prepareAdoption", nlohmann::json::object(), *document);
    if (guard.failure) return *guard.failure;
    if (guard.replay) return ReplayPrepare(*guard.replay);
    if (auto failure = CheckAdoptionAdmission()) return *failure;
    NativeSample sample;
    if (auto failure = ReadNativeSample(sample, m_shadowPlaylistIndex.load())) return *failure;

    std::vector<jit_native::Occurrence> occurrences;
    occurrences.reserve(sample.manualHandles.size());
    for (size_t i = 0; i < sample.manualHandles.size(); ++i) occurrences.push_back({protocol::NewId(), protocol::NewId()});
    auto& native = m_native.emplace(sample.sourceCount, sample.currentRow, std::move(occurrences));
    native.contextId = protocol::NewId();
    native.snapshotToken = protocol::NewId();
    native.expires = std::chrono::steady_clock::now() + kSnapshotLifetime;
    native.current = {sample.current, sample.sourceGuid, sample.currentRow};
    native.currentRef = protocol::NewId();
    native.ledger.RegisterSource(sample.currentRow, native.currentRef);
    native.sourceGuid = sample.sourceGuid;
    std::vector<std::string> watched{sample.sourceGuid};
    std::unordered_set<std::string> seen{sample.sourceGuid};
    for (size_t i = 0; i < sample.manualHandles.size(); ++i) {
        native.manual.push_back({sample.manualHandles[i], sample.manualGuids[i], sample.manualRows[i]});
        if (!sample.manualGuids[i].empty() && seen.insert(sample.manualGuids[i]).second) watched.push_back(sample.manualGuids[i]);
    }
    native.stamp = jit_native::NativeObserver().Capture(watched);

    auto& session = *m_protocol;
    ++session.revision;
    SyncNativeSummary();
    js::PrepareAdoptionResult result{
        native.contextId, native.snapshotToken,
        static_cast<double>(UnixMilliseconds(std::chrono::system_clock::now() + kSnapshotLifetime)),
        session.revision, native.Revision(),
        TrackOf(sample.current, native.currentRef, sample.sourceGuid, sample.currentRow),
        static_cast<std::int64_t>(native.manual.size()), static_cast<std::int64_t>(sample.sourceCount),
        static_cast<std::int64_t>(native.ledger.SourceNext())};
    session.requests.Record(guard.key, guard.digest, js::ToJson(result).dump());
    session.PublishState();
    return result;
}

api::Result<js::GetNativeContextResult> QueueManager::SessionGetNativeContext(const js::GetNativeContextParams& params) {
    SessionEventScope flush{*this};
    RefreshSessionOwner();
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_protocol || m_protocol->id != params.sessionId || !m_native || m_native->contextId != params.contextId) {
        return api::Fail("The native context was not found", ApiErrorCode::NOT_FOUND);
    }
    RefreshNativeValidity();
    auto& native = *m_native;
    if (params.expectedContextRevision != native.Revision()) {
        return api::Fail("The native context changed", ApiErrorCode::CONFLICT,
                         {{"details", {{"contextRevision", native.Revision()}}}});
    }
    js::GetNativeContextResult result;
    const auto summary = SummaryOf(native);
    result.context = {summary.contextId, summary.contextRevision, summary.status, summary.reason, summary.controlMode,
                      summary.sourceNextOffset,
                      TrackOf(native.current.handle, native.currentRef, native.current.playlistGuid, native.current.row),
                      static_cast<std::int64_t>(native.manual.size()), static_cast<std::int64_t>(native.ledger.SourceCount())};
    const auto offset = static_cast<size_t>(params.offset);
    const auto limit = static_cast<size_t>(params.limit);

    if (params.collection == "manual") {
        const auto& occurrences = native.ledger.Manual();
        const size_t total = occurrences.size();
        for (size_t i = offset; i < total && i < offset + limit; ++i) {
            const auto& occurrence = occurrences[i];
            const auto& item = native.manual[i];
            auto entry = EntryOf(TrackOf(item.handle, occurrence.nativeRef, item.playlistGuid, item.row));
            entry.entryId = native.ledger.EntryOf(occurrence.nativeRef);
            entry.occurrenceId = occurrence.id;
            entry.disposition = DispositionName(occurrence.disposition);
            if (const auto rank = native.ledger.RestorationRank(occurrence.id)) entry.restorationRank = static_cast<std::int64_t>(*rank);
            result.items.push_back(std::move(entry));
        }
        result.total = static_cast<std::int64_t>(total);
        if (offset + limit < total) result.nextOffset = static_cast<std::int64_t>(offset + limit);
        return result;
    }

    // 来源行按快照时的内容给出；内容一变就拒绝，不把变更后的新行混进同一个上下文。
    const size_t list = FindList(native.sourceGuid);
    if (native.reason || list == pfc::infinite_size ||
        jit_native::NativeObserver().ListVersion(native.sourceGuid) != native.stamp.lists.front().second) {
        return api::Fail("The source playlist changed since the snapshot", ApiErrorCode::CONFLICT,
                         {{"details", {{"reason", "sourceChanged"}}}});
    }
    auto manager = playlist_manager::get();
    const size_t total = static_cast<size_t>(native.ledger.SourceCount());
    for (size_t row = offset; row < total && row < offset + limit; ++row) {
        metadb_handle_ptr handle;
        if (!manager->playlist_get_item_handle(handle, list, row)) {
            return api::Fail("The source playlist changed since the snapshot", ApiErrorCode::CONFLICT,
                             {{"details", {{"reason", "sourceChanged"}}}});
        }
        auto ref = native.ledger.SourceRef(row);
        if (!ref) {
            ref = protocol::NewId();
            if (!native.ledger.RegisterSource(row, *ref)) {
                return api::Fail("The native context no longer reads its source", ApiErrorCode::CONFLICT,
                                 {{"details", {{"reason", "sourceChanged"}}}});
            }
        }
        auto entry = EntryOf(TrackOf(handle, *ref, native.sourceGuid, row));
        entry.entryId = native.ledger.EntryOf(*ref);
        result.items.push_back(std::move(entry));
    }
    result.total = static_cast<std::int64_t>(total);
    if (offset + limit < total) result.nextOffset = static_cast<std::int64_t>(offset + limit);
    return result;
}


namespace {

js::AdoptNativeResult ReplayAdopt(const std::string& response) {
    const auto value = nlohmann::json::parse(response);
    return {value.at("contextId").get<std::string>(), value.at("attemptId").get<std::string>(),
            value.at("revision").get<std::int64_t>(), value.at("contextRevision").get<std::int64_t>()};
}

api::Failure NativeConflict(const char* message, const char* reason) {
    return api::Fail(message, ApiErrorCode::CONFLICT, {{"details", {{"reason", reason}}}});
}

// 来源耗尽时最后一个待交还的出现放进只有它一行的专用列表：它播完即停，不沿原列表继续。
// 列表由宿主按 GUID 持有，不按名字认领；首个是复用的那个，它正被占用时才临时多建，之后在下一次
// 交还或启动时删掉不在用的多余列表。锁住改名与删除，与影子列表相同。
constexpr GUID kTerminalListsGuid = {0x6de64df8, 0xdf08, 0x4040, {0x9e, 0x77, 0xe3, 0xcd, 0x61, 0xbd, 0x34, 0xb3}};
cfg_string g_terminalLists(kTerminalListsGuid, "");
std::map<std::string, service_ptr_t<playlist_lock>> g_terminalLocks;

class TerminalListLock : public playlist_lock {
public:
    bool query_items_add(t_size, const pfc::list_base_const_t<metadb_handle_ptr>&, const bit_array&) override { return true; }
    bool query_items_reorder(const t_size*, t_size) override { return true; }
    bool query_items_remove(const bit_array&, bool) override { return true; }
    bool query_item_replace(t_size, const metadb_handle_ptr&, const metadb_handle_ptr&) override { return true; }
    bool query_playlist_rename(const char*, t_size) override { return false; }
    bool query_playlist_remove() override { return false; }
    bool execute_default_action(t_size) override { return false; }
    void on_playlist_index_change(t_size) override {}
    void on_playlist_remove() override {}
    void get_lock_name(pfc::string_base& out) override { out = "JIT Restore"; }
    void show_ui() override {}
    t_uint32 get_filter_mask() override { return filter_rename | filter_remove_playlist; }
};

std::vector<std::string> StoredTerminalLists() {
    std::vector<std::string> guids;
    const std::string text = g_terminalLists.get().c_str();
    for (size_t start = 0; start < text.size();) {
        const size_t end = std::min(text.find(';', start), text.size());
        if (end > start) guids.push_back(text.substr(start, end - start));
        start = end + 1;
    }
    return guids;
}

void StoreTerminalLists(const std::vector<std::string>& guids) {
    std::string text;
    for (const auto& guid : guids) text += (text.empty() ? "" : ";") + guid;
    g_terminalLists.set(text.c_str());
}

void LockTerminalList(size_t index) {
    auto manager = playlist_manager::get();
    const std::string guid = ListGuid(index);
    if (manager->playlist_lock_is_present(index) || guid.empty()) return;
    service_ptr_t<playlist_lock> lock = new service_impl_t<TerminalListLock>();
    if (manager->playlist_lock_install(index, lock)) g_terminalLocks[guid] = lock;
}

// 列表在用：正在从它播放，或原生队列里有项引用它。
bool TerminalListInUse(size_t index) {
    auto manager = playlist_manager::get();
    size_t list = pfc::infinite_size;
    size_t row = pfc::infinite_size;
    if (playback_control::get()->is_playing() && manager->get_playing_item_location(&list, &row) && list == index) return true;
    pfc::list_t<t_playback_queue_item> queued;
    manager->queue_get_contents(queued);
    for (size_t i = 0; i < queued.get_count(); ++i) {
        if (queued[i].m_playlist == index) return true;
    }
    return false;
}

size_t CreateTerminalList(std::vector<std::string>& guids, bool primary) {
    const size_t index = playlist_manager::get()->create_playlist("__webview_restore__", pfc::infinite_size, pfc::infinite_size);
    if (index == pfc::infinite_size) return index;
    if (primary) guids.insert(guids.begin(), ListGuid(index));
    else guids.push_back(ListGuid(index));
    StoreTerminalLists(guids);
    LockTerminalList(index);
    return index;
}

// 清空不在用的首个列表，删掉不在用的多余列表；已不存在的从记录里去掉。
void TidyTerminalLists() {
    auto manager = playlist_manager::get();
    auto guids = StoredTerminalLists();
    std::vector<std::string> kept;
    for (size_t i = 0; i < guids.size(); ++i) {
        const size_t index = FindList(guids[i]);
        if (index == pfc::infinite_size) continue;
        if (i == 0 || TerminalListInUse(index)) {
            if (i == 0 && !TerminalListInUse(index) && manager->playlist_get_item_count(index) != 0) manager->playlist_clear(index);
            LockTerminalList(index);
            kept.push_back(guids[i]);
            continue;
        }
        if (const auto lock = g_terminalLocks.find(guids[i]); lock != g_terminalLocks.end()) {
            manager->playlist_lock_uninstall(index, lock->second);
            g_terminalLocks.erase(lock);
        }
        if (!manager->remove_playlist(index)) kept.push_back(guids[i]);
    }
    if (kept != guids) StoreTerminalLists(kept);
}

bool QueueTerminal(const metadb_handle_ptr& handle) {
    auto manager = playlist_manager::get();
    TidyTerminalLists();
    auto guids = StoredTerminalLists();
    size_t list = guids.empty() ? pfc::infinite_size : FindList(guids.front());
    if (list == pfc::infinite_size) list = CreateTerminalList(guids, true);
    else if (TerminalListInUse(list)) list = CreateTerminalList(guids, false);
    if (list == pfc::infinite_size) return false;
    if (manager->playlist_get_item_count(list) != 0) manager->playlist_clear(list);
    metadb_handle_list items;
    items.add_item(handle);
    if (!manager->playlist_add_items(list, items, pfc::bit_array_false())) return false;
    manager->queue_add_item_playlist(list, 0);
    return true;
}

}  // namespace

void QueueManager::TidyNativeRestoreLists() {
    try { TidyTerminalLists(); } catch (...) {}
}

api::Result<js::AdoptNativeResult> QueueManager::SessionAdoptNative(const js::AdoptNativeParams& params,
                                                                   const CallerContext& caller) {
    SessionEventScope flush{*this};
    const auto document = protocol::Capture(caller);
    if (!document) return api::Fail("A trusted document is required", ApiErrorCode::NOT_OWNER);
    RefreshSessionOwner();
    // 快照令牌是凭据，不进入请求摘要。
    const nlohmann::json payload{{"contextId", params.contextId}, {"currentEntryId", params.currentEntryId}};
    SessionGuard guard;
    NativeItem current;
    std::string sourceGuid;
    size_t manualCount = 0;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        guard = CheckSessionWrite(params.sessionId, params.controlToken, params.expectedRevision, params.requestId,
                                  "adoptNative", payload, *document);
        if (guard.failure) return *guard.failure;
        if (guard.replay) return ReplayAdopt(*guard.replay);
        RefreshNativeValidity();
        if (!m_native || m_native->contextId != params.contextId) {
            return api::Fail("The native context was not found", ApiErrorCode::NOT_FOUND);
        }
        const auto& native = *m_native;
        if (native.ledger.GetStatus() != jit_native::Status::Prepared || native.snapshotToken.empty() ||
            native.snapshotToken != params.snapshotToken) {
            return NativeConflict("The snapshot can no longer be adopted", native.reason ? native.reason->c_str() : "snapshotUsed");
        }
        const auto& session = *m_protocol;
        if (session.started || session.current || session.next || session.phase != "active" || m_state != State::Idle) {
            return NativeConflict("The session has already played", "sessionStarted");
        }
        current = native.current;
        sourceGuid = native.sourceGuid;
        manualCount = native.manual.size();
    }

    // 写入前重读，不信任快照之后的任何事实；任何不符都不写。
    auto playback = playback_control::get();
    auto manager = playlist_manager::get();
    const auto settings = jit_native::SampleSettings();
    if (!settings || !settings->Supported()) return NotSupported("unsupportedSettings");
    metadb_handle_ptr playing;
    size_t list = pfc::infinite_size;
    size_t row = pfc::infinite_size;
    const bool same = playback->is_playing() && !playback->is_paused() && playback->get_now_playing(playing) &&
                      playing == current.handle && manager->get_playing_item_location(&list, &row) &&
                      list == FindList(sourceGuid) && current.row && row == *current.row &&
                      manager->queue_get_count() == manualCount;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        RefreshNativeValidity();
        if (!m_native || m_native->contextId != params.contextId || m_native->reason || !same) {
            return NativeConflict("Native playback changed since the snapshot", "nativeChanged");
        }
    }
    const double position = playback->playback_get_position();
    const double duration = playback->playback_get_length();
    const size_t activeBefore = manager->get_active_playlist();

    // 与取证的顺序一致：先清空原生队列（出现已记在账里），再把 playing 切到空影子并读回。
    const size_t shadow = GetOrCreateShadowPlaylist();
    m_shadowPlaylistIndex = shadow;
    if (manager->playlist_get_item_count(shadow) != 0) manager->playlist_clear(shadow);
    manager->queue_flush();
    manager->set_playing_playlist(shadow);
    metadb_handle_ptr after;
    const bool switched = manager->get_playing_playlist() == shadow && manager->queue_get_count() == 0 &&
                          playback->get_now_playing(after) && after == current.handle;
    if (!switched) {
        // 读回不符：把队列与 playing 放回原样，不发布接纳成功。
        std::vector<NativeItem> manual;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_native) manual = m_native->manual;
        }
        const size_t source = FindList(sourceGuid);
        if (source != pfc::infinite_size) manager->set_playing_playlist(source);
        for (const auto& item : manual) {
            const size_t itemList = item.row ? FindList(item.playlistGuid) : pfc::infinite_size;
            if (itemList != pfc::infinite_size) manager->queue_add_item_playlist(itemList, *item.row);
            else manager->queue_add_item(item.handle);
        }
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_native && m_native->contextId == params.contextId) {
            m_native->reason = "nativeChanged";
            m_native->snapshotToken.clear();
            ++m_native->extraRevisions;
            SyncNativeSummary();
        }
        return api::Fail("Native playback could not be adopted", ApiErrorCode::OPERATION_FAILED,
                         {{"details", {{"reason", "switchReadbackFailed"}, {"contextId", params.contextId}}}});
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_native || !m_protocol) return api::Fail("The JIT session was not found", ApiErrorCode::NOT_FOUND);
    auto& native = *m_native;
    auto& session = *m_protocol;
    native.ledger.Adopt(native.currentRef, params.currentEntryId);
    native.snapshotToken.clear();
    // 与 jitQueue 首次起播相同：记下用户的顺序与活动列表，交还时据此恢复。
    m_savedPlaybackOrder = static_cast<int>(settings->order);
    m_savedActivePlaylist = activeBefore;
    m_rows.Reset();
    m_currentEntry = 0;
    m_nextEntry = 0;
    m_generations.EndSession();
    m_currentTrackId = params.currentEntryId;
    m_currentTitle = TitleOf(current.handle);
    m_nextTrackId.clear();
    m_nextTitle.clear();
    m_needNextRequested = false;
    m_state = State::Active;
    ++session.revision;
    session.started = true;
    const std::string attemptId = protocol::NewId();
    protocol::Entry adopted{params.currentEntryId, m_currentTitle, StringUtils::SafeUtf8(current.handle->get_path()),
                            {}, attemptId, {}};
    adopted.status = "playing";
    adopted.execution = protocol::ExecutionStage::Playing;
    adopted.nativeRef = native.currentRef;
    session.current = std::move(adopted);
    session.position = position;
    session.duration = duration;
    session.endOfQueue = false;
    session.ClearRequest();
    SyncNativeSummary();
    const js::AdoptNativeResult result{native.contextId, attemptId, session.revision, native.Revision()};
    session.requests.Record(guard.key, guard.digest, js::ToJson(result).dump());
    session.RequestNext();
    session.PublishState();
    return result;
}

void QueueManager::QueueNativeTicket(jit_shadow::EntrySerial entry) {
    auto manager = playlist_manager::get();
    const size_t shadow = m_shadowPlaylistIndex.load();
    size_t list = pfc::infinite_size;
    size_t row = pfc::infinite_size;
    // 当前已在影子列表里播放时，影子的下一行照常接续，不需要队列项。
    if (!playback_control::get()->is_playing() || shadow >= manager->get_playlist_count() ||
        (manager->get_playing_item_location(&list, &row) && list == shadow) || !VerifyRows(shadow)) {
        return;
    }
    size_t first;
    std::string occurrenceId;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_native || m_native->ledger.GetStatus() != jit_native::Status::Managed) return;
        first = m_rows.FirstRowOf(entry);
        if (m_protocol && m_protocol->next && m_protocol->next->serial == entry) {
            if (const auto* occurrence = m_native->ledger.OccurrenceOfRef(m_protocol->next->nativeRef)) occurrenceId = occurrence->id;
        }
    }
    metadb_handle_ptr handle;
    if (first == jit_shadow::kNoRow || !manager->playlist_get_item_handle(handle, shadow, first)) return;
    WithdrawNativeTicket();
    manager->queue_add_item_playlist(shadow, first);
    std::lock_guard<std::mutex> lock(m_mutex);
    m_nativeTicket = NativeTicket{handle, first, occurrenceId};
    if (!occurrenceId.empty() && m_native && m_native->ledger.Queue(occurrenceId)) SyncNativeSummary();
}

void QueueManager::WithdrawNativeTicket() {
    std::optional<NativeTicket> ticket;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        ticket.swap(m_nativeTicket);
    }
    if (!ticket) return;
    auto manager = playlist_manager::get();
    const size_t shadow = m_shadowPlaylistIndex.load();
    pfc::list_t<t_playback_queue_item> queued;
    manager->queue_get_contents(queued);
    bool withdrawn = false;
    for (size_t i = 0; i < queued.get_count() && !withdrawn; ++i) {
        const auto& item = queued[i];
        if (item.m_playlist == shadow && item.m_item == ticket->row && item.m_handle == ticket->handle) {
            manager->queue_remove_mask(pfc::bit_array_one(i));
            withdrawn = true;
        }
    }
    if (ticket->occurrenceId.empty()) return;
    // 撤回成功的出现回到 pending；队列项已不在说明原生取走了它，起播确认之前不能当作未播。
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_native) return;
    const bool changed = withdrawn ? m_native->ledger.WithdrawQueued(ticket->occurrenceId)
                                   : m_native->ledger.Submit(ticket->occurrenceId);
    if (changed) SyncNativeSummary();
}

std::optional<api::Failure> QueueManager::RestoreNativeContext(bool detach) {
    WithdrawNativeTicket();
    std::optional<jit_native::RestorePlan> plan;
    std::vector<NativeItem> manual;
    std::vector<std::string> ids;
    std::string sourceGuid;
    std::string contextId;
    size_t restoredBefore = 0;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!NativeOpen()) return std::nullopt;
        auto& native = *m_native;
        contextId = native.contextId;
        if (detach) {
            native.ledger.Detach();
            SyncNativeSummary();
            return std::nullopt;
        }
        if (native.ledger.GetStatus() == jit_native::Status::Conflicted) {
            return api::Fail("The native context conflicts with later changes; release it with detach",
                             ApiErrorCode::CONFLICT, {{"details", {{"reason", "contextConflicted"}, {"contextId", contextId}}}});
        }
        // 自有写入会改变队列与影子列表的版本，这里只比对设置与快照记录的列表。
        const auto& observer = jit_native::NativeObserver();
        bool changed = observer.Current().settings != native.stamp.versions.settings;
        for (const auto& [guid, version] : native.stamp.lists) changed = changed || observer.ListVersion(guid) != version;
        if (changed) {
            native.ledger.Conflict();
            SyncNativeSummary();
            return api::Fail("Native settings or playlists changed since adoption", ApiErrorCode::CONFLICT,
                             {{"details", {{"reason", "nativeChanged"}, {"contextId", contextId}}}});
        }
        plan = native.ledger.PlanRestore(native.ledger.RecoveryId().empty() ? protocol::NewId() : native.ledger.RecoveryId());
        if (!plan) {
            return api::Fail("Some occurrences were handed to playback without a known outcome", ApiErrorCode::CONFLICT,
                             {{"details", {{"reason", "unresolvedOccurrences"}, {"contextId", contextId}}}});
        }
        manual = native.manual;
        for (const auto& occurrence : native.ledger.Manual()) {
            ids.push_back(occurrence.id);
            if (occurrence.disposition == jit_native::Disposition::Restored) ++restoredBefore;
        }
        sourceGuid = native.sourceGuid;
        SyncNativeSummary();
    }

    const auto partial = [&](const char* reason) {
        std::lock_guard<std::mutex> lock(m_mutex);
        SyncNativeSummary();
        return api::Fail("The native context was only partly given back", ApiErrorCode::OPERATION_FAILED,
                         {{"details", {{"reason", reason}, {"contextId", contextId}, {"recoveryId", plan->recoveryId}}}});
    };
    const auto conflict = [&](const char* reason) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_native) m_native->ledger.Conflict();
        SyncNativeSummary();
        return api::Fail("Native playback changed while the context was given back", ApiErrorCode::CONFLICT,
                         {{"details", {{"reason", reason}, {"contextId", contextId}, {"recoveryId", plan->recoveryId}}}});
    };

    auto manager = playlist_manager::get();
    // 队列里只应有本次恢复已交还的项；别人的内容一律不 flush，按冲突报告。
    if (manager->queue_get_count() != restoredBefore) return conflict("queueChanged");
    const size_t source = FindList(sourceGuid);
    if (plan->sourceRow) {
        if (source == pfc::infinite_size || *plan->sourceRow >= manager->playlist_get_item_count(source)) {
            return conflict("sourceChanged");
        }
        manager->set_playing_playlist(source);
        if (manager->get_playing_playlist() != source) return partial("playingSourceNotSet");
    }
    for (size_t k = plan->completed; k < plan->occurrences.size(); ++k) {
        const auto& id = plan->occurrences[k];
        const size_t index = static_cast<size_t>(std::find(ids.begin(), ids.end(), id) - ids.begin());
        if (index >= manual.size()) return partial("ledgerChanged");
        const auto& item = manual[index];
        if (plan->terminalOccurrence == id) {
            if (!QueueTerminal(item.handle)) return partial("terminalListFailed");
        } else if (item.row) {
            const size_t list = FindList(item.playlistGuid);
            metadb_handle_ptr listed;
            if (list == pfc::infinite_size || !manager->playlist_get_item_handle(listed, list, *item.row) ||
                listed != item.handle) {
                return conflict("queueItemMoved");
            }
            manager->queue_add_item_playlist(list, *item.row);
        } else {
            manager->queue_add_item(item.handle);
        }
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_native || !m_native->ledger.Restored(*plan, id)) return api::Fail(
            "The native context changed while it was given back", ApiErrorCode::CONFLICT,
            {{"details", {{"reason", "ledgerChanged"}, {"contextId", contextId}}}});
    }
    // 行号已在上面与来源行数比过，32 位下的收窄不会截断。
    if (plan->sourceRow) manager->queue_add_item_playlist(source, static_cast<size_t>(*plan->sourceRow));
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_native) m_native->ledger.FinishRestore(*plan);
    SyncNativeSummary();
    return std::nullopt;
}


namespace {

// 总带子曲目后缀：HandleResolvedItems 对不带后缀的 .cue 地址按“须指定子曲目”拒绝。
std::string LocationOf(const metadb_handle_ptr& handle) {
    return StringUtils::SafeUtf8(handle->get_path()) + "|subsong:" + std::to_string(handle->get_subsong_index());
}

js::SetNativeQueueResult ReplayQueue(const std::string& response) {
    const auto value = nlohmann::json::parse(response);
    return {value.at("revision").get<std::int64_t>(), value.at("contextRevision").get<std::int64_t>()};
}

}  // namespace

std::optional<api::Failure> QueueManager::CheckNativeSupply(const std::string& contextId) const {
    if (!m_native || m_native->contextId != contextId) return api::Fail("The native context was not found", ApiErrorCode::NOT_FOUND);
    const auto status = m_native->ledger.GetStatus();
    if (status == jit_native::Status::Managed) return std::nullopt;
    return NativeConflict("The native context does not take supplies now",
                          status == jit_native::Status::Prepared ? "notAdopted" : "recoveryPending");
}

std::optional<QueueManager::NativeTarget> QueueManager::ResolveNativeRef(const std::string& ref,
                                                                         std::optional<api::Failure>& failure) {
    if (!m_native) {
        failure = api::Fail("The native context was not found", ApiErrorCode::NOT_FOUND);
        return std::nullopt;
    }
    auto& native = *m_native;
    const auto& occurrences = native.ledger.Manual();
    for (size_t i = 0; i < occurrences.size(); ++i) {
        if (occurrences[i].nativeRef == ref) return NativeTarget{native.manual[i].handle, occurrences[i].id};
    }
    const auto row = native.ledger.SourceRowOf(ref);
    if (!row) {
        failure = api::Fail("The native reference does not belong to this context", ApiErrorCode::INVALID_PARAMS);
        return std::nullopt;
    }
    const size_t list = FindList(native.sourceGuid);
    metadb_handle_ptr handle;
    if (list == pfc::infinite_size ||
        jit_native::NativeObserver().ListVersion(native.sourceGuid) != native.stamp.lists.front().second ||
        !playlist_manager::get()->playlist_get_item_handle(handle, list, static_cast<size_t>(*row))) {
        failure = NativeConflict("The source playlist changed since the snapshot", "sourceChanged");
        return std::nullopt;
    }
    return NativeTarget{handle, {}};
}

void QueueManager::MarkNativeSubmitted(jit_shadow::EntrySerial entry) {
    if (!m_protocol || !m_native || m_native->ledger.GetStatus() != jit_native::Status::Managed) return;
    const auto& session = *m_protocol;
    std::string ref;
    if (session.current && session.current->serial == entry) ref = session.current->nativeRef;
    else if (session.next && session.next->serial == entry) ref = session.next->nativeRef;
    if (ref.empty()) return;
    if (const auto* occurrence = m_native->ledger.OccurrenceOfRef(ref)) {
        const std::string id = occurrence->id;
        if (m_native->ledger.Submit(id)) SyncNativeSummary();
    }
}

void QueueManager::StartNativeEntry(const metadb_handle_ptr& handle, const std::string& trackId, const std::string& title) {
    {
        auto plm = playlist_manager::get();
        const int order = static_cast<int>(plm->playback_order_get_active());
        const size_t active = plm->get_active_playlist();
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_state.load() == State::Idle) {
            m_savedPlaybackOrder = order;
            m_savedActivePlaylist = active;
        }
    }
    jit_shadow::EntrySerial entry;
    jit_shadow::Ticket ticket;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_generations.EndSession();
        ticket = m_generations.TakeCurrent();
        m_currentTrackId = trackId;
        m_currentTitle = title;
        m_nextTrackId.clear();
        m_nextTitle.clear();
        m_needNextRequested = false;
        entry = ++m_lastEntry;
        m_currentEntry = entry;
        m_nextEntry = 0;
        BindSessionEntry(entry, jit_shadow::Slot::Current);
    }
    m_state = State::Active;
    AddNativeHandleAsync(handle, trackId, entry, ticket);
}

void QueueManager::BufferNativeEntry(const metadb_handle_ptr& handle, const std::string& trackId, const std::string& title) {
    jit_shadow::EntrySerial entry;
    jit_shadow::Ticket ticket;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        ticket = m_generations.TakeNext();
        m_nextTrackId = trackId;
        m_nextTitle = title;
        m_needNextRequested = false;
        entry = ++m_lastEntry;
        m_nextEntry = entry;
        BindSessionEntry(entry, jit_shadow::Slot::Next);
    }
    m_state = State::Active;
    AddNativeHandleAsync(handle, trackId, entry, ticket);
}

void QueueManager::AddNativeHandleAsync(const metadb_handle_ptr& handle, const std::string& trackId,
                                        jit_shadow::EntrySerial entry, jit_shadow::Ticket ticket) {
    if (m_shadowPlaylistIndex == pfc::infinite_size) m_shadowPlaylistIndex = GetOrCreateShadowPlaylist();
    // 与资源地址的添加一样在之后的主线程步骤里写影子列表，应答先于事件送达。
    // handle、trackId 用初始化捕获：直接捕获 const 引用参数时闭包移动要复制它们，可能抛出。
    fb2k::inMainThread([this, handle = handle, trackId = trackId, entry, ticket]() {
        try {
            if (!AddIsLive(ticket, trackId, "queued")) return;
            const size_t playlistIndex = m_shadowPlaylistIndex.load();
            auto plm = playlist_manager::get();
            if (playlistIndex >= plm->get_playlist_count()) {
                ReportSessionAdd(entry, false, "shadowPlaylistMissing");
                return;
            }
            if (ticket.slot == jit_shadow::Slot::Current) {
                plm->playlist_clear(playlistIndex);
                std::lock_guard<std::mutex> lock(m_mutex);
                m_rows.Reset();
            }
            metadb_handle_list items;
            items.add_item(handle);
            HandleResolvedItems(items, trackId, LocationOf(handle), true, entry, ticket);
        } catch (...) {
            try {
                FB2K_console_print("[JIT Session] Adding a native entry failed: ", trackId.c_str());
                if (AddIsLive(ticket, trackId, "exception")) ReportSessionAdd(entry, false, "insertFailed");
            } catch (...) {
                // 报告本身失败时只能放弃：回调不能把异常抛回 fb2k
            }
        }
    });
}

api::Result<js::PlayNativeResult> QueueManager::SessionPlayNative(const js::PlayNativeParams& params,
                                                                 const CallerContext& caller) {
    SessionEventScope flush{*this};
    const auto document = protocol::Capture(caller);
    if (!document) return api::Fail("A trusted document is required", ApiErrorCode::NOT_OWNER);
    RefreshSessionOwner();
    nlohmann::json payload{{"contextId", params.contextId}, {"nativeRef", params.nativeRef}, {"entryId", params.entryId}};
    api::results::Put(payload, "title", params.title);
    js::PlayNativeResult result;
    metadb_handle_ptr handle;
    std::string title;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const auto guard = CheckSessionWrite(params.sessionId, params.controlToken, params.expectedRevision,
                                             params.requestId, "playNative", payload, *document);
        if (guard.failure) return *guard.failure;
        if (guard.replay) return protocol::Replay<js::PlayNativeResult>(*guard.replay);
        if (auto failure = CheckNativeSupply(params.contextId)) return *failure;
        std::optional<api::Failure> failure;
        const auto target = ResolveNativeRef(params.nativeRef, failure);
        if (!target) return failure.value_or(api::Fail("The native reference could not be resolved", ApiErrorCode::CONFLICT));
        if (!m_native || !m_native->ledger.Bind(params.nativeRef, params.entryId)) {
            return NativeConflict("The reference or entry is bound to another one", "entryBound");
        }
        handle = target->handle;
        title = params.title.value_or(TitleOf(handle));
        auto& session = *m_protocol;
        ++session.revision;
        session.SupersedePending("play");
        session.EndCurrent("superseded");
        protocol::Entry entry{params.entryId, title, StringUtils::SafeUtf8(handle->get_path()), {}, protocol::NewId(), protocol::NewId()};
        entry.nativeRef = params.nativeRef;
        result = {entry.operationId, entry.attemptId, session.revision};
        session.operations.Begin({entry.operationId, jit_session::OpKind::Play, entry.id, entry.attemptId});
        session.current = std::move(entry);
        session.next.reset();
        session.nextReady = false;
        session.endOfQueue = false;
        session.ClearRequest();
        session.position.reset();
        session.duration.reset();
        session.phase = "active";
        session.phaseReason.reset();
        session.started = true;
        session.requests.Record(guard.key, guard.digest, js::ToJson(result).dump());
        SyncNativeSummary();
        session.PublishState();
    }
    try {
        WithdrawNativeTicket();
        StartNativeEntry(handle, params.entryId, title);
    } catch (...) {
        FailSessionOperation(result.operationId, "startFailed");
    }
    return result;
}

api::Result<js::SetNativeNextResult> QueueManager::SessionSetNativeNext(const js::SetNativeNextParams& params,
                                                                       const CallerContext& caller) {
    SessionEventScope flush{*this};
    const auto document = protocol::Capture(caller);
    if (!document) return api::Fail("A trusted document is required", ApiErrorCode::NOT_OWNER);
    RefreshSessionOwner();
    nlohmann::json payload{{"contextId", params.contextId}, {"afterAttemptId", params.afterAttemptId},
                           {"nativeRef", params.nativeRef}, {"entryId", params.entryId}};
    api::results::Put(payload, "title", params.title);
    js::SetNativeNextResult result;
    metadb_handle_ptr handle;
    std::string title;
    const bool mayBePreselected = protocol::NextMayBePreselected();
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const auto guard = CheckSessionWrite(params.sessionId, params.controlToken, params.expectedRevision,
                                             params.requestId, "setNativeNext", payload, *document);
        if (guard.failure) return *guard.failure;
        if (guard.replay) return protocol::Replay<js::SetNativeNextResult>(*guard.replay);
        if (auto failure = CheckNativeSupply(params.contextId)) return *failure;
        auto& session = *m_protocol;
        if (session.phase != "active" || !session.current) return api::Fail("The session has no active attempt", ApiErrorCode::NO_ACTIVE_ITEM);
        if (session.current->attemptId != params.afterAttemptId) return api::Fail("The current attempt changed", ApiErrorCode::CONFLICT);
        if (auto preselected = CheckNextWithdrawable(mayBePreselected)) return *preselected;
        std::optional<api::Failure> failure;
        const auto target = ResolveNativeRef(params.nativeRef, failure);
        if (!target) return failure.value_or(api::Fail("The native reference could not be resolved", ApiErrorCode::CONFLICT));
        if (!m_native || !m_native->ledger.Bind(params.nativeRef, params.entryId)) {
            return NativeConflict("The reference or entry is bound to another one", "entryBound");
        }
        handle = target->handle;
        title = params.title.value_or(TitleOf(handle));
        ++session.revision;
        if (session.next) session.FinishOperation(session.next->operationId, jit_session::OpStatus::Superseded, "setNext");
        session.nextReady = false;
        session.endOfQueue = false;
        session.ClearRequest();
        protocol::Entry entry{params.entryId, title, StringUtils::SafeUtf8(handle->get_path()), {}, {}, protocol::NewId()};
        entry.nativeRef = params.nativeRef;
        result.operationId = entry.operationId;
        result.revision = session.revision;
        session.operations.Begin({entry.operationId, jit_session::OpKind::SetNext, entry.id, {}});
        session.next = std::move(entry);
        session.requests.Record(guard.key, guard.digest, js::ToJson(result).dump());
        SyncNativeSummary();
        session.PublishState();
    }
    try {
        WithdrawNextEntry();
        BufferNativeEntry(handle, params.entryId, title);
    } catch (...) {
        if (result.operationId) FailSessionOperation(*result.operationId, "bufferUpdateFailed");
    }
    return result;
}

api::Result<js::SetNativeQueueResult> QueueManager::SessionSetNativeQueue(const js::SetNativeQueueParams& params,
                                                                         const CallerContext& caller) {
    SessionEventScope flush{*this};
    const auto document = protocol::Capture(caller);
    if (!document) return api::Fail("A trusted document is required", ApiErrorCode::NOT_OWNER);
    RefreshSessionOwner();
    const nlohmann::json payload{{"contextId", params.contextId}, {"expectedContextRevision", params.expectedContextRevision},
                                 {"remainingOccurrenceIds", params.remainingOccurrenceIds}};
    SessionGuard guard;
    bool withdrawNext = false;
    const bool mayBePreselected = protocol::NextMayBePreselected();
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        guard = CheckSessionWrite(params.sessionId, params.controlToken, params.expectedRevision, params.requestId,
                                  "setNativeQueue", payload, *document);
        if (guard.failure) return *guard.failure;
        if (guard.replay) return ReplayQueue(*guard.replay);
        if (auto failure = CheckNativeSupply(params.contextId)) return *failure;
        auto& native = *m_native;
        if (params.expectedContextRevision != native.Revision()) {
            return api::Fail("The native context changed", ApiErrorCode::CONFLICT, {{"details", {{"contextRevision", native.Revision()}}}});
        }
        std::unordered_set<std::string> seen;
        for (const auto& id : params.remainingOccurrenceIds) {
            if (!seen.insert(id).second || !native.ledger.Find(id)) {
                return api::Fail("Occurrence identifiers must be known and listed once", ApiErrorCode::INVALID_PARAMS);
            }
        }
        if (!native.ledger.PlanEdit(params.remainingOccurrenceIds)) {
            return NativeConflict("A listed occurrence was handed to playback", "occurrencesChanged");
        }
        // 被删除的出现正是下一条：先从缓冲撤销它，当前继续播放。
        auto& session = *m_protocol;
        if (session.next) {
            const auto* occurrence = native.ledger.OccurrenceOfRef(session.next->nativeRef);
            withdrawNext = occurrence && !seen.contains(occurrence->id) &&
                           (occurrence->disposition == jit_native::Disposition::Pending ||
                            occurrence->disposition == jit_native::Disposition::Queued);
        }
        if (withdrawNext) {
            if (auto failure = CheckNextWithdrawable(mayBePreselected)) return *failure;
            ++session.revision;
            session.FinishOperation(session.next->operationId, jit_session::OpStatus::Superseded, "setNativeQueue");
            session.next.reset();
            session.nextReady = false;
            session.ClearRequest();
        }
    }
    if (withdrawNext) {
        WithdrawNextEntry();
        m_state = State::WaitingNext;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_native || !m_protocol) return api::Fail("The JIT session was not found", ApiErrorCode::NOT_FOUND);
    auto& native = *m_native;
    auto& session = *m_protocol;
    // 撤回之后重新校验：撤回期间被原生取走的项不能当作已删除。
    const auto plan = native.ledger.PlanEdit(params.remainingOccurrenceIds);
    if (!plan || !plan->withdrawals.empty() || !native.ledger.CommitEdit(*plan)) {
        SyncNativeSummary();
        session.PublishState();
        return NativeConflict("A removed occurrence was handed to playback while it was withdrawn", "occurrencesChanged");
    }
    ++session.revision;
    SyncNativeSummary();
    const js::SetNativeQueueResult result{session.revision, native.Revision()};
    session.requests.Record(guard.key, guard.digest, js::ToJson(result).dump());
    session.RequestNext();
    session.PublishState();
    return result;
}
