/**
 * QueueManager.h - JIT queue for streaming media
 *
 * The frontend keeps the logical queue; the host keeps a shadow playlist holding only the
 * current and next track and asks for the next one through jitQueue:needNext, so stream URLs
 * are resolved just in time.
 */

#pragma once
#include "pch.h"
#include "core/JitNativeContext.h"
#include "core/JitNativeObservation.h"
#include "core/JitShadowRows.h"
#include "core/QueueSessionState.h"
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <optional>

// The page that started the current JIT session. The jitQueue:* events go back to it through
// api::emit::ToCaller: by windowId, then by handle, and else to the main window's page.
struct JitSessionCaller {
    std::string windowId;
    HWND hwnd = nullptr;
};

struct CallerContext;

class QueueManager {
public:
    static QueueManager& GetInstance();
    
    QueueManager(const QueueManager&) = delete;
    QueueManager& operator=(const QueueManager&) = delete;
    
    //==========================================================================
    // State Machine
    //==========================================================================
    
    enum class State {
        Idle,           // Not active
        Active,         // Playing from shadow playlist
        WaitingNext,    // needNext emitted, waiting for enqueueNext
        Exhausted,      // No more tracks, waiting for PlayNow or clear
    };
    
    State GetState() const { return m_state.load(); }
    
    static const char* StateToString(State s) {
        switch (s) {
            case State::Idle: return "Idle";
            case State::Active: return "Active";
            case State::WaitingNext: return "WaitingNext";
            case State::Exhausted: return "Exhausted";
            default: return "Unknown";
        }
    }
    
    //==========================================================================
    // Initialization
    //==========================================================================
    
    // Creates or finds the shadow playlist.
    void Initialize();
    void Shutdown();

    // 原生接管交还用的专用单曲列表：启动时清空不在用的首个列表、删掉不在用的多余列表，并重新加锁。
    // 在主线程、不持有 m_mutex 时调用。
    void TidyNativeRestoreLists();
    
    //==========================================================================
    // Core API Methods
    //==========================================================================
    
    // Clears the buffer and plays url now; a previous playNow still being resolved is neither
    // added nor started. caller owns the new session.
    void PlayNow(const std::string& trackId, const std::string& title, const std::string& url,
                 const JitSessionCaller& caller);

    // Appends url to the shadow playlist as the next track for gapless playback. Returns
    // whether the request was accepted.
    bool EnqueueNext(const std::string& trackId, const std::string& title, const std::string& url);

    struct PreloadResult {
        bool success = false;
        size_t tracksAdded = 0;
        std::string error;
    };
    // Inserts resolved URLs (HTTP or local) into the shadow playlist without a needNext round
    // trip; playback starts from the 0-based startIndex, and replace clears the playlist first.
    // caller owns the session only when the batch starts playback, not when it is appended to a
    // playing session.
    PreloadResult PreloadBatch(const std::vector<std::string>& urls, size_t startIndex, bool replace,
                               const JitSessionCaller& caller);
    
    // Plays the next shadow row, or the player's next track when that row is unknown. Returns
    // false only when idle.
    bool Skip();

    // Stops playback and restores the user context; clearBuffer also empties the shadow playlist.
    void Stop(bool clearBuffer = true);

    // Empties the shadow playlist and restores the user context.
    void Clear();

    api::Result<api::jitSession::CreateResult> SessionCreate(const api::jitSession::CreateParams&, const CallerContext&);
    api::Result<api::jitSession::DiscoverResult> SessionDiscover(const CallerContext&);
    api::Result<api::jitSession::GetStateResult> SessionGetState(const api::jitSession::GetStateParams&);
    api::Result<api::jitSession::ClaimControlResult> SessionClaim(const api::jitSession::ClaimControlParams&, const CallerContext&);
    api::Result<api::jitSession::PlayResult> SessionPlay(const api::jitSession::PlayParams&, const CallerContext&);
    api::Result<api::jitSession::SetNextResult> SessionSetNext(const api::jitSession::SetNextParams&, const CallerContext&);
    api::Result<api::jitSession::SkipResult> SessionSkip(const api::jitSession::SkipParams&, const CallerContext&);
    api::Result<api::jitSession::StopResult> SessionStop(const api::jitSession::StopParams&, const CallerContext&);
    api::Result<api::jitSession::GetOperationResult> SessionGetOperation(const api::jitSession::GetOperationParams&);
    api::Result<void> SessionRelease(const api::jitSession::ReleaseParams&, const CallerContext&);
    api::Result<api::jitSession::PrepareAdoptionResult> SessionPrepareAdoption(const api::jitSession::PrepareAdoptionParams&, const CallerContext&);
    api::Result<api::jitSession::GetNativeContextResult> SessionGetNativeContext(const api::jitSession::GetNativeContextParams&);
    api::Result<api::jitSession::AdoptNativeResult> SessionAdoptNative(const api::jitSession::AdoptNativeParams&, const CallerContext&);
    api::Result<api::jitSession::PlayNativeResult> SessionPlayNative(const api::jitSession::PlayNativeParams&, const CallerContext&);
    api::Result<api::jitSession::SetNativeNextResult> SessionSetNativeNext(const api::jitSession::SetNativeNextParams&, const CallerContext&);
    api::Result<api::jitSession::SetNativeQueueResult> SessionSetNativeQueue(const api::jitSession::SetNativeQueueParams&, const CallerContext&);

    // 协议会话仍在调度播放：存在、未释放，也没有因旧入口接管转入只剩交还资格的 stopped。
    // 旧 jitQueue 的事件、预取与列表耗尽通知只在没有这样的会话时发出。
    bool HasProtocolSession() const;
    void NotifyEmpty();
    // 控制台里 fb2k 记录的播放失败（任意线程调用），供失败复查判断引擎是否报告过该资源。
    void NoteEngineFailure(const std::string& path);
    
    //==========================================================================
    // State Queries
    //==========================================================================
    
    // Track IDs are the frontend's.
    std::string GetCurrentTrackId() const;
    std::string GetNextTrackId() const;
    // Number of tracks in the shadow playlist.
    size_t GetBufferSize() const;
    size_t GetShadowPlaylistIndex() const;
    // Whether playback is from the shadow playlist.
    bool IsActive() const;
    void NotifyListExhausted();
    
    //==========================================================================
    // Playback Callbacks (called from PlaybackCallback)
    //==========================================================================
    
    void OnPlaybackNewTrack(const metadb_handle_ptr& track);
    void OnPlaybackStop(play_control::t_stop_reason reason);
    // Once a second; requests the next track near the end.
    void OnPlaybackTime(double time, double duration);

    //==========================================================================
    // Playlist Lock Callbacks (called by ShadowPlaylistLock)
    //==========================================================================

    // The shadow playlist moved because playlists were added, removed or reordered.
    void OnShadowPlaylistIndexChanged(size_t newIndex);
    // Someone else removed the shadow playlist.
    void OnShadowPlaylistRemoved();

private:
    QueueManager() = default;

    // API 返回或回调结束时统一出锁投递，事件不会跨过下一次命令才被发出。
    struct SessionEventScope {
        QueueManager& owner;
        ~SessionEventScope();
    };

    struct SessionGuard {
        std::optional<api::Failure> failure;
        std::optional<std::string> replay;
        std::string key;
        std::uint64_t digest = 0;
    };
    
    //==========================================================================
    // Constants
    //==========================================================================
    
    // Startup finds an existing shadow playlist by this name; keep it stable.
    static constexpr const char* SHADOW_PLAYLIST_NAME = "__webview_buffer__";
    
    // Request the next track when this many seconds remain.
    static constexpr double PREFETCH_THRESHOLD_SECONDS = 30.0;
    
    //==========================================================================
    // State
    //==========================================================================
    
    std::atomic<State> m_state{State::Idle};
    std::atomic<size_t> m_shadowPlaylistIndex{pfc::infinite_size};
    
    // Saved for RestoreUserContext; -1 means no playback order is saved.
    int m_savedPlaybackOrder = -1;
    size_t m_savedActivePlaylist = 0;
    
    std::string m_currentTrackId;
    std::string m_nextTrackId;
    std::string m_currentTitle;
    std::string m_nextTitle;
    
    // Which entry each shadow-playlist row belongs to. A track belongs to the session by the row it
    // plays from, not by its path: the same file can be the current and the next track, or play
    // from another playlist.
    jit_shadow::RowTable m_rows;
    jit_shadow::EntrySerial m_lastEntry = 0;
    jit_shadow::EntrySerial m_currentEntry = 0;
    jit_shadow::EntrySerial m_nextEntry = 0;

    // Every add takes a ticket when accepted and is dropped without side effects once a later
    // command has superseded it; only the latest track's deferred attribution step acts
    jit_shadow::Generations m_generations;

    std::atomic<bool> m_needNextRequested{false};

    // Where the jitQueue:* events go. Kept after stop and clear, so a shadow playlist the user
    // plays again still reports to the page that filled it; replaced when a session starts.
    JitSessionCaller m_sessionCaller;
    
    mutable std::mutex m_mutex;
    bool m_initialized = false;
    std::optional<jit_session_runtime::Session> m_protocol;
    // UpdateBackgroundHold 上次交给主窗口的控制页面句柄；0 表示未持有。只在主线程读写。
    std::uintptr_t m_backgroundHold = 0;

    // 原生接管的上下文：prepareAdoption 记下的快照与出现账。manual 与账里的出现按下标对齐，
    // 保存每个出现的原生事实；playlistGuid 为空表示该队列项没有播放列表位置。
    struct NativeItem {
        metadb_handle_ptr handle;
        std::string playlistGuid;
        std::optional<size_t> row;
    };
    struct NativeAdoption {
        NativeAdoption(std::uint64_t sourceCount, std::uint64_t currentRow, std::vector<jit_native::Occurrence> occurrences)
            : ledger(sourceCount, currentRow, std::move(occurrences)) {}

        std::string contextId;
        std::string snapshotToken;
        std::chrono::steady_clock::time_point expires{};
        jit_native::Stamp stamp;
        jit_native::NativeContext ledger;
        NativeItem current;
        std::string currentRef;
        std::vector<NativeItem> manual;
        std::string sourceGuid;
        std::optional<std::string> reason;
        // 快照失效等不经出现账的变化另计，与账的版本合成 contextRevision
        std::int64_t extraRevisions = 0;

        std::int64_t Revision() const { return static_cast<std::int64_t>(ledger.Revision()) + extraRevisions; }
    };
    std::optional<NativeAdoption> m_native;

    // 原生当前曲目之后的首次衔接：影子里的下一条另排一个带影子坐标的原生队列项，原生当前
    // 播完后由它把播放带进影子列表。记下行与 handle，撤回时只删这一项，不动别人的队列项。
    struct NativeTicket {
        metadb_handle_ptr handle;
        size_t row = 0;
        // 下一条是手动出现时为它的标识，队列项在原生队列里期间它是 queued
        std::string occurrenceId;
    };
    std::optional<NativeTicket> m_nativeTicket;
    
    //==========================================================================
    // Internal Methods
    //==========================================================================
    
    // Copy of m_sessionCaller taken under m_mutex; call without holding it
    JitSessionCaller GetSessionCaller() const;

    void StartCurrentEntry(const std::string& trackId, const std::string& title, const std::string& url);
    void BufferNextEntry(const std::string& trackId, const std::string& title, const std::string& url);
    bool SkipInternal();
    void StopInternal(bool clearBuffer);
    void ClearInternal();
    void WithdrawNextEntry();
    void LegacyTakeover();
    void RefreshSessionOwner();
    void FlushSessionEvents();
    // 活动会话的控制页面在独立主窗口里时，让主窗口隐藏期间不深度挂起它。主线程、不持锁调用。
    void UpdateBackgroundHold();
    SessionGuard CheckSessionWrite(const std::string& sessionId, const std::string& token, std::int64_t revision,
                                   const std::string& requestId, const std::string& method,
                                   const nlohmann::json& payload, const jit_session_runtime::Document& caller,
                                   bool allowReleased = false);
    // 以下四个都要求持有 m_mutex、在主线程调用。
    // 接管准入：会话从未播放、没有未结束的接管上下文、旧 jitQueue 不在使用。
    std::optional<api::Failure> CheckAdoptionAdmission();
    // 未接纳的快照过期或原生事实已变时记下原因，增加两个版本并发出状态。
    void RefreshNativeValidity();
    void SyncNativeSummary();
    // play、stop、release、认领与旧入口接管使未接纳的快照作废。
    void DiscardPreparedNative();
    // 已接纳且尚未结束的上下文（managed、restoring 或 conflicted）。
    bool NativeOpen() const;
    // HasProtocolSession 的不加锁版本，调用方持有 m_mutex。
    bool ProtocolSchedules() const;
    // 交还已开始或已冲突：只允许 release，不再接受调度命令。
    bool NativeRecovering() const;
    // fb2k 可能已选定已缓冲的下一条时，撤销它不再生效，返回 nextPreselected 冲突。调用方持有 m_mutex。
    std::optional<api::Failure> CheckNextWithdrawable(bool mayBePreselected) const;
    // 原生供给的前提：上下文存在且仍受管。
    std::optional<api::Failure> CheckNativeSupply(const std::string& contextId) const;
    struct NativeTarget {
        metadb_handle_ptr handle;
        std::string occurrenceId;
    };
    // 解析本上下文里的原生引用；来源行须仍与快照一致。失败时 failure 给出拒绝原因。
    std::optional<NativeTarget> ResolveNativeRef(const std::string& ref, std::optional<api::Failure>& failure);
    // 影子里属于 entry 的条目交给播放器时，把它对应的手动出现记为已提交。
    void MarkNativeSubmitted(jit_shadow::EntrySerial entry);

    // 以下在主线程调用、不持有 m_mutex：其中的写入会触发会回调 QueueManager 的播放列表回调。
    // 与 StartCurrentEntry、BufferNextEntry 相同的状态更新，只把解析换成已知的 handle。
    void StartNativeEntry(const metadb_handle_ptr& handle, const std::string& trackId, const std::string& title);
    void BufferNativeEntry(const metadb_handle_ptr& handle, const std::string& trackId, const std::string& title);
    void AddNativeHandleAsync(const metadb_handle_ptr& handle, const std::string& trackId, jit_shadow::EntrySerial entry,
                              jit_shadow::Ticket ticket);
    // 下一条加入影子后，原生当前不在影子里时为它排入首次衔接的原生队列项。
    void QueueNativeTicket(jit_shadow::EntrySerial entry);
    void WithdrawNativeTicket();
    // 交还接管上下文：detach 只结束上下文；否则撤回自有队列项，按出现账交还未消费的出现、
    // 接上来源后继。成功或无事可做时返回空；部分完成时保留恢复记录，可用新请求继续。
    std::optional<api::Failure> RestoreNativeContext(bool detach);
    void BindSessionEntry(jit_shadow::EntrySerial entry, jit_shadow::Slot slot);
    bool SessionOwnsEntry(jit_shadow::EntrySerial entry) const;
    bool ReportSessionAdd(jit_shadow::EntrySerial entry, bool added, const std::string& reason = {});
    void FailSessionOperation(const std::string& operationId, const std::string& reason);
    void SessionTrackStarted(jit_shadow::Attribution attribution);
    void SessionPlaybackExternal();
    bool SessionPlaybackStopped(play_control::t_stop_reason reason);
    void SessionPlaybackTime(double position, double duration);
    // 以 unknown 结束且未播满 1 秒的尝试：在后台重开它的资源，结果以 attemptRechecked 发出。
    struct Recheck {
        std::string sessionId;
        std::string attemptId;
        std::string entryId;
        std::string location;
        jit_shadow::EntrySerial serial = 0;
    };
    void RecheckAttempt(const Recheck& job);
    bool EngineReported(const std::string& path) const;
    // 最近的引擎失败记录：比对用的路径与记录时刻，只留最近 32 条。
    mutable std::mutex m_engineFailuresMutex;
    std::deque<std::pair<std::chrono::steady_clock::time_point, std::string>> m_engineFailures;

    size_t GetOrCreateShadowPlaylist();
    
    // Main-thread step queued by OnPlaybackNewTrack: decides from the playing row whether the
    // track advances the session, stays in the current entry or leaves the session, then removes
    // the rows before it. Gives up when a later track has started meanwhile.
    void AttributeNewTrack(std::uint64_t trackSeq, const metadb_handle_ptr& track);

    // Playback left the session: go Idle and stop treating the shadow rows as the session's.
    // Does nothing when already Idle.
    void ReleaseSession(const char* reason);

    // Whether an add can still take effect; logs the dropped add when it cannot
    bool AddIsLive(const jit_shadow::Ticket& ticket, const std::string& trackId, const char* stage) const;

    // Handle pointer values of the shadow playlist rows, in order
    static std::vector<jit_shadow::HandleKey> ReadRowHandles(size_t playlistIndex);

    // Compare m_rows with the playlist; a difference means someone else edited it, and the rows
    // are no longer used to attribute or remove anything. Returns whether m_rows can be used.
    bool VerifyRows(size_t playlistIndex);

    // Remove the rows before the playing one, so the playing track sits at index 0. Runs in the
    // attribution step: removing rows inside on_playback_new_track leaves the playing location
    // fb2k reports afterwards invalid.
    void RemovePlayedRows(size_t playlistIndex, size_t playingRow);

    // Removes the shadow rows after the current entry, so a replaced next cannot linger between
    // current and the new next where native prev/next would reach it. Keeps the rows when m_rows
    // cannot be trusted.
    void RemoveBufferedNextFromShadow(size_t playlistIndex);

    void RequestNextTrack();

    // Resolves url asynchronously into the rows of `entry`. A current-slot add clears the shadow
    // playlist and starts playback; a next-slot add replaces the buffered next, so the playlist
    // stays [current, next].
    void AddUrlToPlaylistAsync(const std::string& url, const std::string& trackId,
                                const std::string& title, jit_shadow::EntrySerial entry,
                                jit_shadow::Ticket ticket);

    // Local-file counterpart of AddUrlToPlaylistAsync.
    void AddLocalFileToPlaylistAsync(const std::string& path, const std::string& trackId,
                                      const std::string& title, jit_shadow::EntrySerial entry,
                                      jit_shadow::Ticket ticket);
    
    // Check if a URL is a local file path (not HTTP/HTTPS)
    static bool IsLocalFilePath(const std::string& url);
    
    // Restores the playback order and active playlist saved at activation; safe to call again.
    void RestoreUserContext();

    // Install playlist lock on shadow playlist for index tracking & protection
    void InstallPlaylistLock(size_t playlistIndex);

    // Parse "path|subsong:N" format -> (path, subsongIndex)
    static std::pair<pfc::string8, t_uint32> ParseSubsongUrl(const std::string& url);

    // Converts URL strings to metadb handles.
    metadb_handle_list BuildHandlesFromUrls(const std::vector<std::string>& urls);

    // Inserts the resolved items as the rows of `entry`: current rows go first and start
    // playback, next rows replace the buffered next. Does nothing once the ticket is superseded.
    // locationIsPath reports a failure in the `path` field of jitQueue:error instead of `url`.
    void HandleResolvedItems(metadb_handle_list_cref items, const std::string& trackId,
                             const std::string& location, bool locationIsPath, jit_shadow::EntrySerial entry,
                             jit_shadow::Ticket ticket);

    // Starts playback on a row of the shadow playlist.
    void ActivatePlaybackOnShadow(size_t playlistIndex, size_t row);

    // Playlist lock instance (tracks index changes, blocks rename/delete)
    service_ptr_t<playlist_lock> m_playlistLock;
};

#define g_QueueManager QueueManager::GetInstance()
