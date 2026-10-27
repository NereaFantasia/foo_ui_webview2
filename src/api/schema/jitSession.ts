import type { Int } from './common.js';

export interface Api {
  /**
   * Report what this host's JIT sessions can and cannot do, so a coordinator can plan around
   * the limits instead of discovering them in playback.
   * @zh 报告本宿主的 JIT 会话能做什么、不能做什么，让协调方事先避开限制，而不是在播放中才发现
   * @experimental
   */
  getCapabilities(): GetCapabilitiesResult;

  /**
   * Create a session and take control of it. Creating changes nothing in playback. Fails with
   * `CONFLICT` while another session exists (released sessions excepted), with its `sessionId`
   * in `details`; take it over with `jitSession.claimControl` instead. A retry with the same
   * `requestId` from the same page returns the first answer.
   * @zh 创建会话并取得控制。创建本身不改变播放。已有会话（已释放的除外）时以 `CONFLICT` 失败，`details` 里给出它的 `sessionId`，应改用 `jitSession.claimControl` 接管。同一页面以同一 `requestId` 重试时返回第一次的应答
   * @experimental
   */
  create(params: CreateParams): CreateResult;

  /**
   * Find the session a page can resume or take over, without changing playback or granting
   * control. `canResume` is true for a page reloaded in the window whose previous page held
   * control.
   * @zh 查找页面可以恢复或接管的会话，不改变播放，也不授予控制。在原控制页面所在的窗口里重载后的页面，`canResume` 为 true
   * @experimental
   */
  discover(): DiscoverResult;

  /**
   * Read a consistent snapshot of a session. The snapshot never contains the control token.
   * @zh 读取会话的一致快照。快照不含控制令牌
   * @experimental
   */
  getState(params: GetStateParams): GetStateResult;

  /**
   * Take control of a session without restarting what is playing. `resume` is for a page
   * reloaded in the window whose previous page held control; `handover` is for any other page
   * and must name the current `ownerEpoch`. The previous controller's token stops working and
   * its pending operations are kept. Fails with `NOT_OWNER` when resuming is not allowed and with
   * `CONFLICT` when `expectedOwnerEpoch` is out of date.
   * @zh 不重启正在播放的内容，取得会话的控制。`resume` 用于原控制页面所在窗口里重载后的页面；`handover` 用于其他页面，必须给出当前的 `ownerEpoch`。原控制者的令牌随即失效，它已受理的操作保留。不允许恢复时以 `NOT_OWNER` 失败，`expectedOwnerEpoch` 过期时以 `CONFLICT` 失败
   * @experimental
   */
  claimControl(params: ClaimControlParams): ClaimControlResult;

  /**
   * Play an entry now as a new attempt. The answer means the command was accepted, not that the
   * resource can play: `jitSession:operationCompleted` reports whether it was added and started,
   * and `jitSession:attemptFinished` how the attempt ended. Tracks in the native playback queue
   * are kept and play first when the attempt ends. Write commands fail with `NOT_OWNER` for a
   * token that is not the current controller's and with `CONFLICT` when `expectedRevision` is
   * out of date or `requestId` was used for a different command.
   * @zh 立即把一个条目作为新尝试播放。应答表示命令已受理，不表示资源能播：是否加入并开始播放由 `jitSession:operationCompleted` 报告，尝试如何结束由 `jitSession:attemptFinished` 报告。原生播放队列里的曲目保留，这次尝试结束后先播它们。写命令在令牌不属于当前控制者时以 `NOT_OWNER` 失败，在 `expectedRevision` 过期或 `requestId` 已用于别的命令时以 `CONFLICT` 失败
   * @experimental
   */
  play(params: PlayParams): PlayResult;

  /**
   * Set, replace or withdraw the entry after the current attempt. Accepted only while
   * `afterAttemptId` is still the current attempt. A replaced or withdrawn next entry is removed
   * from the buffer and its pending operation ends as `superseded`; the current attempt keeps
   * playing. Without `entryId`, the next entry is withdrawn; `endOfQueue: true` also declares
   * that nothing follows. When the current attempt has already ended and the session is waiting
   * for its next entry, the new entry starts at once. Within the output buffer length plus one
   * second of the current track's end, foobar2000 may already have picked the buffered next entry
   * to follow it, so withdrawing or replacing a buffered entry then fails with `CONFLICT` and
   * `details.reason: "nextPreselected"`: nothing changes and the buffered entry plays next. Once it
   * starts, set what follows it against its attempt. Setting a next entry while none is buffered is
   * still accepted.
   * @zh 设置、替换或撤销当前尝试之后的条目。只在 `afterAttemptId` 仍是当前尝试时受理。被替换或撤销的下一条从缓冲中移除，它未结束的操作以 `superseded` 结束；当前尝试继续播放。不传 `entryId` 即撤销下一条；`endOfQueue: true` 同时声明之后没有更多条目。当前尝试已经结束、会话正在等下一条时，新条目立即开始播放。当前曲目距结尾不到输出缓冲长度加 1 秒时，foobar2000 可能已经选定已缓冲的下一条接着播放，此时撤销或替换它以 `CONFLICT` 失败，`details.reason` 为 `nextPreselected`：状态不变，已缓冲的下一条照常接着播放。等它开始后，再以它的尝试为准设置之后的条目。没有已缓冲的下一条时，设置下一条照常受理
   * @experimental
   */
  setNext(params: SetNextParams): SetNextResult;

  /**
   * End the current attempt and play the buffered next entry, leaving the native playback queue
   * untouched. Accepted only while `attemptId` is the current attempt. Without a ready next
   * entry, fails with `NO_ACTIVE_ITEM`: the current attempt keeps playing, the revision does
   * not change and no operation is created.
   * @zh 结束当前尝试，播放已缓冲的下一条，不动原生播放队列。只在 `attemptId` 仍是当前尝试时受理。下一条尚未就绪时以 `NO_ACTIVE_ITEM` 拒绝，当前尝试继续播放，版本号不变，也不创建操作
   * @experimental
   */
  skip(params: SkipParams): SkipResult;

  /**
   * Stop playback and supersede pending operations. The session stays, in `stopped`, for its
   * state and results to be read; a later `jitSession.play` starts a new attempt. After
   * `jitSession.adoptNative`, the remaining native queue occurrences are given back without
   * starting playback; a failure is recorded in `nativeContext`.
   * @zh 停止播放并取代未结束的操作。会话保留为 `stopped`，可以读取状态与结果；之后的 `jitSession.play` 开始新的尝试。`jitSession.adoptNative` 之后，剩余的原生队列出现会交还，但不开始播放；交还失败记录在 `nativeContext` 中
   * @experimental
   */
  stop(params: StopParams): StopResult;

  /**
   * Read an operation's status. The pending operations and the latest 64 finished ones of a
   * session can be read. After release they remain readable until the next `jitSession.create`
   * or for ten minutes, whichever comes first; an expired record fails with `NOT_FOUND`.
   * @zh 读取一个操作的状态。可以读到会话的全部未结束操作与最近 64 个已结束的操作；会话释放后保留到下一次 `jitSession.create` 或十分钟，以先到者为准。记录过期后以 `NOT_FOUND` 失败
   * @experimental
   */
  getOperation(params: GetOperationParams): GetOperationResult;

  /**
   * End the session and give the native context back: the buffer is emptied, the track playing
   * keeps decoding, and the playback order and active playlist saved when the session first
   * played are restored. After `jitSession.adoptNative`, the remaining native queue occurrences
   * are given back first (see `nativeDisposition`); when that fails the session is kept, the error
   * carries `details.contextId` and `details.recoveryId`, and a release with a new `requestId`
   * continues where it stopped. Giving the context back while foobar2000 may already have picked
   * the buffered next entry (see `jitSession.setNext`) fails with `CONFLICT` and
   * `details.reason: "nextPreselected"` and changes nothing; `nativeDisposition: "detach"` is still
   * accepted. Without an adopted context, a next entry picked that way may still play after the
   * release.
   * @zh 结束会话并归还原生上下文：清空缓冲，正在播放的曲目继续解码，恢复会话首次播放时保存的播放顺序与活动播放列表。`jitSession.adoptNative` 之后，先交还剩余的原生队列出现（见 `nativeDisposition`）；交还失败时会话保留，错误带有 `details.contextId` 与 `details.recoveryId`，以新的 `requestId` 再次释放会从中断处继续。foobar2000 可能已经选定已缓冲的下一条时（见 `jitSession.setNext`），交还上下文以 `CONFLICT` 失败，`details.reason` 为 `nextPreselected`，状态不变；`nativeDisposition: "detach"` 照常受理。没有接管上下文时，这样选定的下一条在释放后仍可能播放
   * @experimental
   */
  release(params: ReleaseParams): void;

  /**
   * Capture the native playback for adoption: the track playing, the native playback queue one
   * occurrence at a time (repeated tracks stay separate) and the playlist it plays from. Nothing in
   * playback, the queue, the playlists or the settings changes; read the occurrences and source
   * rows with `jitSession.getNativeContext`. The session must be new: `CONFLICT` once it has
   * played, while a `jitQueue` buffer is in use or while an adopted context is still open. A new
   * call replaces an unadopted snapshot. Fails with `NOT_SUPPORTED` unless playback is running
   * and not paused from a row of a normal playlist (not an autoplaylist or another locked one),
   * the playback order is Default, cursor and playback following and stop-after-current are off,
   * and the queue holds at most 1024 tracks whose positions match their playlists; `details.reason`
   * names the first failed condition. `jitSession.adoptNative` commits the snapshot.
   * @zh 为接管记录原生播放：正在播放的曲目、原生播放队列的逐次出现（重复曲目分开记录），以及播放所在的播放列表。播放、队列、播放列表与设置都不改变；出现与来源行用 `jitSession.getNativeContext` 读取。会话必须是新的：已播放过、`jitQueue` 缓冲正在使用或已接管的上下文尚未结束时以 `CONFLICT` 失败。再次调用会替换尚未接纳的快照。除非播放正在进行、未暂停、来自普通播放列表（不是自动播放列表或其他被锁定的列表）的一行，播放顺序为 Default，光标跟随、播放跟随与“播完当前停止”均关闭，且队列不超过 1024 首、各项位置与所在列表一致，否则以 `NOT_SUPPORTED` 失败，`details.reason` 指出第一个不满足的条件。快照由 `jitSession.adoptNative` 接纳
   * @experimental
   */
  prepareAdoption(params: PrepareAdoptionParams): PrepareAdoptionResult;

  /**
   * Read one page of a native context: the `manual` occurrences captured from the native queue, in
   * their order, or the `source` rows of the playlist being played. Reading changes neither
   * revision; a row read twice keeps its `nativeRef`. Fails with `CONFLICT` when
   * `expectedContextRevision` is out of date, and for `source` pages also when the playlist's
   * content changed since the snapshot; a page past the end is empty.
   * @zh 读取原生上下文的一页：从原生队列记录的 `manual` 出现（按原顺序），或正在播放的播放列表的 `source` 行。读取不改变两个版本号；同一行再次读取保持同一个 `nativeRef`。`expectedContextRevision` 过期时以 `CONFLICT` 失败；读 `source` 页时，播放列表内容自快照以来变过也以 `CONFLICT` 失败。超出末尾的页为空
   * @experimental
   */
  getNativeContext(params: GetNativeContextParams): GetNativeContextResult;

  /**
   * Adopt the native playback captured by `jitSession.prepareAdoption` without interrupting it.
   * The track playing becomes the current attempt, bound to `currentEntryId`, and keeps decoding;
   * the native playback queue is emptied, its occurrences kept by the host, and the session
   * schedules what follows (`jitSession:needNext` asks for it). Stopping or releasing the session
   * gives the remaining occurrences back to the native queue and continues the playlist after
   * the last row played from it. When no row is left after it, the last occurrence goes into a
   * single-track playlist the host keeps for this (`__webview_restore__`), so playback stops after
   * it instead of continuing the playlist it came from. A `jitQueue` call made while the context is open keeps its usual
   * behavior; the session then stays `stopped` with reason `legacyTakeover`, keeps its control
   * token and accepts only `jitSession.release`. Fails with `CONFLICT` when the snapshot was
   * replaced, expired or already used, or native playback changed since it was taken, and with
   * `NOT_SUPPORTED` when a setting no longer allows adoption; nothing changes then.
   * @zh 不打断地接纳 `jitSession.prepareAdoption` 记录的原生播放。正在播放的曲目成为当前尝试，绑定 `currentEntryId`，继续解码；原生播放队列被清空，其中的出现由宿主保存，之后播什么由会话调度（`jitSession:needNext` 会来索取）。停止或释放会话时，剩余的出现交还原生队列，并从来源中最后播放的一行之后接着播放。来源在那一行之后已无内容时，最后一个出现放进宿主为此保留的单曲列表（`__webview_restore__`），播完即停，不沿它原来所在的列表继续。上下文未结束时调用 `jitQueue` 方法，它照常执行；会话随后保持 `stopped`，原因为 `legacyTakeover`，保留控制令牌，只接受 `jitSession.release`。快照已被替换、过期或已用过，或者原生播放自快照以来有变化时以 `CONFLICT` 失败；设置已不允许接管时以 `NOT_SUPPORTED` 失败；失败时不改变任何状态
   * @experimental
   */
  adoptNative(params: AdoptNativeParams): AdoptNativeResult;

  /**
   * Play a source row or queue occurrence of the adopted context now, as `jitSession.play` does
   * for a location: the answer means accepted, and `jitSession:operationCompleted` reports the
   * start. The first supply of a `nativeRef` binds it to `entryId`; replaying it keeps that
   * `entryId`, and a different one fails with `CONFLICT`. Starting a queue occurrence consumes
   * it, so it is not given back later; replaying a consumed or removed one does not bring it back.
   * Playing a source row moves the point the playlist continues from when the context is given
   * back.
   * @zh 立即播放已接纳上下文中的一个来源行或队列出现，与 `jitSession.play` 播放资源地址相同：应答表示已受理，起播结果由 `jitSession:operationCompleted` 报告。某个 `nativeRef` 第一次供给时绑定到 `entryId`；重播沿用该 `entryId`，换用别的会以 `CONFLICT` 失败。队列出现一旦开始播放即被消费，之后不再交还；重播已消费或已删除的出现不会让它重新回到待交还之列。播放来源行会改变交还上下文时来源接续的位置
   * @experimental
   */
  playNative(params: PlayNativeParams): PlayResult;

  /**
   * Set a source row or queue occurrence of the adopted context as the entry after the current
   * attempt, as `jitSession.setNext` does for a location. Binding works as for
   * `jitSession.playNative`; use `jitSession.setNext` without `entryId` to withdraw it. Replacing a
   * buffered next entry fails with `nextPreselected` under the same condition as `jitSession.setNext`.
   * @zh 把已接纳上下文中的一个来源行或队列出现设为当前尝试之后的条目，与 `jitSession.setNext` 设置资源地址相同。绑定规则同 `jitSession.playNative`；撤销时使用不带 `entryId` 的 `jitSession.setNext`。替换已缓冲的下一条时，与 `jitSession.setNext` 在同样条件下以 `nextPreselected` 失败
   * @experimental
   */
  setNativeNext(params: SetNativeNextParams): SetNextResult;

  /**
   * Decide which queue occurrences of the adopted context are still to be given back, and in what
   * order: list every pending or queued occurrence to keep; the ones left out are removed and
   * never given back. A removed occurrence that is the next entry is withdrawn from the buffer.
   * Fails with `INVALID_PARAMS` for a repeated or unknown identifier, and with `CONFLICT` when
   * `expectedContextRevision` is out of date or a listed occurrence has meanwhile been handed to
   * playback, and with `CONFLICT` and `details.reason: "nextPreselected"` when removing the buffered
   * next entry under the condition described for `jitSession.setNext`; nothing changes then.
   * @zh 决定已接纳上下文中哪些队列出现仍待交还以及顺序：列出要保留的每个 pending 或 queued 出现；未列出的被删除，不再交还。被删除的出现若正是下一条，会从缓冲中撤销。标识重复或不认识时以 `INVALID_PARAMS` 失败；`expectedContextRevision` 过期，或列出的出现此间已交给播放时以 `CONFLICT` 失败；在 `jitSession.setNext` 所述条件下删除已缓冲的下一条，以 `CONFLICT` 失败，`details.reason` 为 `nextPreselected`；失败时不改变任何状态
   * @experimental
   */
  setNativeQueue(params: SetNativeQueueParams): SetNativeQueueResult;
}

export interface Events {
  /**
   * The session's state changed: an attempt started or ended, an operation finished, supply or
   * control changed. Carries the full snapshot. Sent to the controlling page only; a page that
   * missed events reads `jitSession.getState`.
   * @zh 会话状态变化：尝试开始或结束、操作结束、供给或控制变化。携带完整快照。只发给当前控制页面；漏掉事件的页面应读取 `jitSession.getState`
   * @delivery owner
   */
  stateChanged: StateChangedPayload;

  /**
   * The session needs its next entry: an attempt started with none buffered and no end of queue
   * declared. Answer with `jitSession.setNext`. The open request stays readable in the snapshot
   * as `neededAfterAttemptId`. Sent like `jitSession:stateChanged`.
   * @zh 会话需要下一条：一次尝试开始时缓冲里没有下一条，也没有声明队列结束。用 `jitSession.setNext` 作答。未答复的请求在快照的 `neededAfterAttemptId` 中可以读回。投递范围同 `jitSession:stateChanged`
   * @delivery owner
   */
  needNext: NeedNextPayload;

  /**
   * An operation reached its final status. Each operation sends this once. Sent like
   * `jitSession:stateChanged`.
   * @zh 一个操作到达终态。每个操作只发一次。投递范围同 `jitSession:stateChanged`
   * @delivery owner
   */
  operationCompleted: OperationCompletedPayload;

  /**
   * An attempt ended. `outcome` is `completed` only when playback reached the end of the
   * resource; an ending the host cannot attribute is `unknown`, with the facts it has. Sent like
   * `jitSession:stateChanged`.
   * @zh 一次尝试结束。只有播放到达资源末尾时 `outcome` 才是 `completed`；宿主无法确认原因的结束报 `unknown`，并附上已知的事实。投递范围同 `jitSession:stateChanged`
   * @delivery owner
   */
  attemptFinished: AttemptFinishedPayload;

  /**
   * The host reopened the resource of an attempt that ended as `unknown` within its first second,
   * and reports what that second try ran into. It describes the resource at the time of the
   * recheck, not necessarily the cause of the original failure: a transient network failure may
   * not reproduce, and a single-use address may fail differently the second time. Follows the
   * attempt's `jitSession:attemptFinished`, up to about ten seconds later for network errors; not
   * sent for other endings or when the host shuts down meanwhile. It changes no session state and
   * is not kept in the snapshot, so a page that misses it cannot read it back. Sent like
   * `jitSession:stateChanged`.
   * @zh 尝试以 `unknown` 结束且未播满 1 秒时，宿主重新打开它的资源，报告这次复查遇到的情况。它反映的是复查时资源的状态，不一定是原来那次失败的原因：瞬时的网络故障可能不再出现，一次性地址第二次打开可能以别的原因失败。在该尝试的 `jitSession:attemptFinished` 之后发出，网络错误时最多晚约十秒；其他结束方式不发，宿主在此期间退出也不发。它不改变会话状态，也不保留在快照里，页面错过就读不回来。投递范围同 `jitSession:stateChanged`
   * @delivery owner
   */
  attemptRechecked: AttemptRecheckedPayload;

  /**
   * Control of the session moved: claimed by another page, taken over by a `jitQueue` call, or
   * released. Sent to the new and the previous controlling page.
   * @zh 会话的控制发生变化：被其他页面认领、被 `jitQueue` 调用接管，或被释放。发给新的与原来的控制页面
   * @delivery owner
   */
  controlChanged: ControlChangedPayload;
}

interface GetCapabilitiesResult {
  /**
   * Version of the session protocol.
   * @zh 会话协议的版本
   */
  protocolVersion: Int;
  /**
   * Whether a session can take over native playback without interrupting it, through
   * `jitSession.prepareAdoption` and `jitSession.adoptNative`. This host supports it;
   * `jitSession.prepareAdoption` decides whether the playback at hand qualifies.
   * @zh 会话能否经 `jitSession.prepareAdoption` 与 `jitSession.adoptNative` 不打断地接管原生播放。本宿主支持；当前这次播放是否符合条件由 `jitSession.prepareAdoption` 判定
   */
  adoption: boolean;
  /**
   * Whether attempt failures are classified (network, decoding). Not available: foobar2000
   * reports no structured playback error. See `failureRecheck` for the best-effort substitute.
   * @zh 是否对尝试失败分类（网络、解码）。不支持：foobar2000 不报告结构化的播放错误。尽力而为的替代见 `failureRecheck`
   */
  preciseFailure: boolean;
  /**
   * Whether an attempt that ends as `unknown` within its first second is followed by
   * `jitSession:attemptRechecked`.
   * @zh 以 `unknown` 结束且未播满 1 秒的尝试之后，是否会发出 `jitSession:attemptRechecked`
   */
  failureRecheck?: boolean;
  /**
   * Whether the controlling page keeps answering `jitSession:needNext` while its window is
   * minimized, hidden to the tray or the screen is locked. This host supports it: while a
   * session is active, its page is hidden but not suspended. Closing or reloading the page
   * still interrupts the supply until a page claims control again.
   * @zh 窗口最小化、隐藏到托盘或锁屏期间，控制页面能否继续回答 `jitSession:needNext`。本宿主支持：会话活动期间，它的页面只隐藏、不挂起。关闭或重载页面仍会中断供给，直到有页面重新取得控制
   */
  backgroundExecution: boolean;
  /**
   * When playback started elsewhere is recognized: only once that track has started, so an
   * operation completing before then may still be added.
   * @zh 何时识别出别处开始的播放：只在那首曲目开始之后，此前完成的操作仍可能被加入
   */
  externalTakeoverDetection: 'afterTrackStart';
}

interface CreateParams {
  /**
   * Caller-chosen key that makes a retry return the first answer.
   * @zh 调用方选定的键，重试时返回第一次的应答
   * @minLength 1
   */
  requestId: string;
}

interface CreateResult {
  /**
   * The new session.
   * @zh 新会话
   */
  sessionId: string;
  /**
   * Token that write commands must carry. Keep it in the page; it is never sent in events or
   * state.
   * @zh 写命令必须携带的令牌。由页面保管，不会出现在事件或状态中
   */
  controlToken: string;
  /**
   * Control generation; changes whenever control moves.
   * @zh 控制代次，控制每转移一次就变化
   */
  ownerEpoch: Int;
  /**
   * Current state version.
   * @zh 当前状态版本
   */
  revision: Int;
}

interface DiscoverResult {
  /**
   * The session, or null when there is none.
   * @zh 会话；没有时为 null
   */
  session: JitSessionDiscovery | null;
}

interface JitSessionDiscovery {
  /**
   * The session.
   * @zh 会话
   */
  sessionId: string;
  /**
   * `connected` while the page holding control is still loaded; `disconnected` after it
   * navigated away or closed.
   * @zh 持有控制的页面仍在时为 `connected`；它导航离开或关闭后为 `disconnected`
   */
  ownerState: 'connected' | 'disconnected';
  /**
   * Current control generation, for `jitSession.claimControl`.
   * @zh 当前控制代次，供 `jitSession.claimControl` 使用
   */
  ownerEpoch: Int;
  /**
   * Whether the calling page may claim it with `mode: 'resume'`.
   * @zh 调用页面能否以 `mode: 'resume'` 认领
   */
  canResume: boolean;
}

interface GetStateParams {
  /**
   * Session to read.
   * @zh 要读取的会话
   * @minLength 1
   */
  sessionId: string;
}

interface GetStateResult {
  /**
   * Snapshot of the session.
   * @zh 会话快照
   */
  state: JitSessionSnapshot;
}

interface JitSessionSnapshot {
  /**
   * The session.
   * @zh 会话
   */
  sessionId: string;
  /**
   * State version; every change the session reports raises it.
   * @zh 状态版本；会话报告的每次变化都会使它增大
   */
  revision: Int;
  /**
   * `eventSeq` of the last event this snapshot reflects.
   * @zh 本快照已反映的最后一个事件的 `eventSeq`
   */
  lastEventSeq: Int;
  /**
   * Current control generation.
   * @zh 当前控制代次
   */
  ownerEpoch: Int;
  /**
   * Whether the controlling page is still loaded.
   * @zh 控制页面是否仍在
   */
  ownerState: 'connected' | 'disconnected' | 'none';
  /**
   * `active` while the session runs playback, `stopped` after a stop, `released` after
   * `jitSession.release` or a `jitQueue` takeover; a takeover while an adopted native context is
   * open leaves it `stopped`.
   * @zh 会话执行播放时为 `active`，停止后为 `stopped`，`jitSession.release` 或被 `jitQueue` 接管后为 `released`；已接纳的原生上下文尚未结束时被接管，会话保持 `stopped`
   */
  phase: 'active' | 'stopped' | 'released';
  /**
   * Why the session last stopped or was released, or null.
   * @zh 会话最近一次停止或释放的原因；没有时为 null
   */
  phaseReason: 'user' | 'command' | 'external' | 'legacyTakeover' | null;
  /**
   * The current attempt, or null.
   * @zh 当前尝试；没有时为 null
   */
  current: JitSessionAttempt | null;
  /**
   * The next entry, or null.
   * @zh 下一条；没有时为 null
   */
  next: JitSessionNextEntry | null;
  /**
   * `needed`: no next entry and no end declared; `preparing`: the next entry's operation is
   * pending; `ready`: the next entry is buffered; `exhausted`: the end of queue was declared.
   * @zh `needed`：没有下一条且未声明结束；`preparing`：下一条的操作未结束；`ready`：下一条已缓冲；`exhausted`：已声明队列结束
   */
  supplyState: 'needed' | 'preparing' | 'ready' | 'exhausted';
  /**
   * The attempt an unanswered `jitSession:needNext` was sent for, or null.
   * @zh 尚未答复的 `jitSession:needNext` 所针对的尝试；没有时为 null
   */
  neededAfterAttemptId: string | null;
  /**
   * Identifier of the unanswered supply request, or null.
   * @zh 尚未答复的供给请求标识；没有时为 null
   */
  neededRequestKey: string | null;
  /**
   * Pending operations, in the order they were accepted.
   * @zh 未结束的操作，按受理顺序
   */
  pendingOperations: string[];
  /**
   * The native context captured with `jitSession.prepareAdoption`; absent when there is none.
   * @zh 用 `jitSession.prepareAdoption` 记录的原生上下文；没有时不含此字段
   */
  nativeContext?: JitNativeContextSummary;
}

interface JitNativeContextSummary {
  /**
   * The context.
   * @zh 上下文
   */
  contextId: string;
  /**
   * Version of the occurrences, their bindings and the context status; every change raises it
   * together with the session `revision`.
   * @zh 出现、绑定与上下文状态的版本；每次变化都与会话的 `revision` 一起增大
   */
  contextRevision: Int;
  /**
   * `prepared` while only the snapshot exists and native playback runs on its own.
   * @zh 只有快照、原生播放照常进行时为 `prepared`
   */
  status: 'prepared' | 'managed' | 'restoring' | 'restored' | 'conflicted' | 'detached';
  /**
   * Why the context stopped applying, or null: `snapshotExpired` after 30 seconds unadopted,
   * `nativeChanged` when playback, the queue, the settings or a captured playlist changed.
   * @zh 上下文不再适用的原因；没有时为 null。未接纳超过 30 秒为 `snapshotExpired`，播放、队列、设置或记录的播放列表发生变化为 `nativeChanged`
   */
  reason: 'snapshotExpired' | 'nativeChanged' | null;
  /**
   * `playback` while the session may schedule playback; `recovery` while only release is allowed.
   * @zh 会话可以调度播放时为 `playback`；只允许释放时为 `recovery`
   */
  controlMode: 'playback' | 'recovery';
  /**
   * Source row native playback continues from when the context is given back; equals
   * `sourceCount` when the source is exhausted.
   * @zh 交还上下文时原生播放接续的来源行；来源已耗尽时等于 `sourceCount`
   */
  sourceNextOffset: Int;
}

interface JitSessionAttempt {
  /**
   * Caller's entry identifier.
   * @zh 调用方给的条目标识
   */
  entryId: string;
  /**
   * Host's identifier of this attempt; a replay of the same entry gets a new one.
   * @zh 宿主给这次尝试的标识；同一条目重播会得到新的标识
   */
  attemptId: string;
  /**
   * Display title.
   * @zh 显示标题
   */
  title: string;
  /**
   * `preparing` until playback starts, then `playing`; `ended` once the attempt finished.
   * @zh 开始播放之前为 `preparing`，之后为 `playing`；尝试结束后为 `ended`
   */
  status: 'preparing' | 'playing' | 'ended';
  /**
   * The native reference the entry plays, for an adopted current track or a native supply;
   * absent for a location.
   * @zh 条目播放的原生引用，用于接纳的当前曲目或原生供给；资源地址条目不含此字段
   */
  nativeRef?: string;
}

interface JitSessionNextEntry {
  /**
   * Caller's entry identifier.
   * @zh 调用方给的条目标识
   */
  entryId: string;
  /**
   * Display title.
   * @zh 显示标题
   */
  title: string;
  /**
   * Operation adding it to the buffer.
   * @zh 把它加入缓冲的操作
   */
  operationId: string;
  /**
   * The native reference the entry plays, for a native supply; absent for a location.
   * @zh 条目播放的原生引用，用于原生供给；资源地址条目不含此字段
   */
  nativeRef?: string;
}

interface ClaimControlParams {
  /**
   * Session to claim.
   * @zh 要认领的会话
   * @minLength 1
   */
  sessionId: string;
  /**
   * `resume` after reloading in the controlling window, `handover` from any other page.
   * @zh 在控制窗口里重载后用 `resume`，其他页面用 `handover`
   */
  mode: 'resume' | 'handover';
  /**
   * The `ownerEpoch` being taken over; required for `handover`.
   * @zh 要接管的 `ownerEpoch`；`handover` 时必填
   * @minimum 0
   */
  expectedOwnerEpoch?: Int;
  /**
   * Caller-chosen key that makes a retry return the first answer.
   * @zh 调用方选定的键，重试时返回第一次的应答
   * @minLength 1
   */
  requestId: string;
}

interface ClaimControlResult {
  /**
   * New control token; the previous controller's no longer works.
   * @zh 新的控制令牌；原控制者的令牌不再有效
   */
  controlToken: string;
  /**
   * New control generation.
   * @zh 新的控制代次
   */
  ownerEpoch: Int;
  /**
   * Current state version.
   * @zh 当前状态版本
   */
  revision: Int;
}

interface PlayParams {
  /**
   * Session to act on.
   * @zh 要操作的会话
   * @minLength 1
   */
  sessionId: string;
  /**
   * Control token from `jitSession.create` or `jitSession.claimControl`.
   * @zh 来自 `jitSession.create` 或 `jitSession.claimControl` 的控制令牌
   * @minLength 1
   */
  controlToken: string;
  /**
   * The `revision` the command is based on.
   * @zh 命令所依据的 `revision`
   * @minimum 0
   */
  expectedRevision: Int;
  /**
   * Caller-chosen key that makes a retry return the first answer instead of running again.
   * @zh 调用方选定的键，重试时返回第一次的应答而不是再执行一次
   * @minLength 1
   */
  requestId: string;
  /**
   * Caller's identifier of this occurrence in its queue; repeated resources use different ones.
   * @zh 调用方给这次出现的标识；同一资源重复入队时使用不同的标识
   * @minLength 1
   */
  entryId: string;
  /**
   * Display title.
   * @zh 显示标题
   * @default ""
   */
  title?: string;
  /**
   * Resource to play: a file path or `file://` URL, `path|subsong:N` for one subsong, or an
   * `http(s)://` stream. A playlist file that expands to more than one track fails the
   * operation. CUE locations must name a subsong explicitly. At most 2048 characters.
   * @zh 要播放的资源：文件路径或 `file://` URL、表示单个子曲目的 `path|subsong:N`，或 `http(s)://` 流。展开出多于一首曲目的播放列表文件会使操作失败，CUE 地址必须明确指定子曲目。最多 2048 字符
   * @minLength 1
   * @security MediaRead
   */
  location: string;
  /**
   * When the location stops being valid, in Unix milliseconds; recorded only.
   * @zh 资源地址的失效时刻，Unix 毫秒；只做记录
   */
  expiresAt?: number;
}

interface PlayResult {
  /**
   * Operation adding and starting the entry.
   * @zh 加入并开始播放该条目的操作
   */
  operationId: string;
  /**
   * The attempt this command starts.
   * @zh 本命令开始的尝试
   */
  attemptId: string;
  /**
   * State version after accepting the command.
   * @zh 受理命令之后的状态版本
   */
  revision: Int;
}

interface SetNextParams {
  /**
   * Session to act on.
   * @zh 要操作的会话
   * @minLength 1
   */
  sessionId: string;
  /**
   * Control token from `jitSession.create` or `jitSession.claimControl`.
   * @zh 来自 `jitSession.create` 或 `jitSession.claimControl` 的控制令牌
   * @minLength 1
   */
  controlToken: string;
  /**
   * The `revision` the command is based on.
   * @zh 命令所依据的 `revision`
   * @minimum 0
   */
  expectedRevision: Int;
  /**
   * Caller-chosen key that makes a retry return the first answer instead of running again.
   * @zh 调用方选定的键，重试时返回第一次的应答而不是再执行一次
   * @minLength 1
   */
  requestId: string;
  /**
   * The current attempt the command is meant for.
   * @zh 命令所针对的当前尝试
   * @minLength 1
   */
  afterAttemptId: string;
  /**
   * Identifier of the next entry; leave out to withdraw the next entry.
   * @zh 下一条的标识；不传即撤销下一条
   */
  entryId?: string;
  /**
   * Display title.
   * @zh 显示标题
   */
  title?: string;
  /**
   * Resource of the next entry, written as for `jitSession.play`; required with `entryId`.
   * @zh 下一条的资源，写法同 `jitSession.play`；传 `entryId` 时必填
   * @security MediaRead
   */
  location?: string;
  /**
   * When the location stops being valid, in Unix milliseconds; recorded only.
   * @zh 资源地址的失效时刻，Unix 毫秒；只做记录
   */
  expiresAt?: number;
  /**
   * Declare that nothing follows. Cannot be combined with `entryId`.
   * @zh 声明之后没有更多条目。不能与 `entryId` 同时使用
   * @default false
   */
  endOfQueue?: boolean;
}

interface SetNextResult {
  /**
   * Operation adding the next entry; absent when the next entry was withdrawn.
   * @zh 加入下一条的操作；撤销下一条时不含
   */
  operationId?: string;
  /**
   * State version after accepting the command.
   * @zh 受理命令之后的状态版本
   */
  revision: Int;
}

interface SkipParams {
  /**
   * Session to act on.
   * @zh 要操作的会话
   * @minLength 1
   */
  sessionId: string;
  /**
   * Control token from `jitSession.create` or `jitSession.claimControl`.
   * @zh 来自 `jitSession.create` 或 `jitSession.claimControl` 的控制令牌
   * @minLength 1
   */
  controlToken: string;
  /**
   * The `revision` the command is based on.
   * @zh 命令所依据的 `revision`
   * @minimum 0
   */
  expectedRevision: Int;
  /**
   * Caller-chosen key that makes a retry return the first answer instead of running again.
   * @zh 调用方选定的键，重试时返回第一次的应答而不是再执行一次
   * @minLength 1
   */
  requestId: string;
  /**
   * The current attempt to end.
   * @zh 要结束的当前尝试
   * @minLength 1
   */
  attemptId: string;
}

interface SkipResult {
  /**
   * Operation starting the buffered next entry.
   * @zh 起播已缓冲下一条的操作
   */
  operationId: string;
  /**
   * State version after accepting the command.
   * @zh 受理命令之后的状态版本
   */
  revision: Int;
}

interface StopParams {
  /**
   * Session to act on.
   * @zh 要操作的会话
   * @minLength 1
   */
  sessionId: string;
  /**
   * Control token from `jitSession.create` or `jitSession.claimControl`.
   * @zh 来自 `jitSession.create` 或 `jitSession.claimControl` 的控制令牌
   * @minLength 1
   */
  controlToken: string;
  /**
   * The `revision` the command is based on.
   * @zh 命令所依据的 `revision`
   * @minimum 0
   */
  expectedRevision: Int;
  /**
   * Caller-chosen key that makes a retry return the first answer instead of running again.
   * @zh 调用方选定的键，重试时返回第一次的应答而不是再执行一次
   * @minLength 1
   */
  requestId: string;
}

interface StopResult {
  /**
   * State version after accepting the command.
   * @zh 受理命令之后的状态版本
   */
  revision: Int;
}

interface GetOperationParams {
  /**
   * Session the operation belongs to.
   * @zh 操作所属的会话
   * @minLength 1
   */
  sessionId: string;
  /**
   * Operation to read.
   * @zh 要读取的操作
   * @minLength 1
   */
  operationId: string;
}

interface GetOperationResult {
  /**
   * The operation.
   * @zh 操作
   */
  operation: JitSessionOperation;
}

interface JitSessionOperation {
  /**
   * The operation.
   * @zh 操作
   */
  operationId: string;
  /**
   * Command that created it.
   * @zh 创建它的命令
   */
  kind: 'play' | 'setNext' | 'skip';
  /**
   * Entry it adds.
   * @zh 它加入的条目
   */
  entryId: string;
  /**
   * Attempt started by play or skip; null while preparing a next entry.
   * @zh play 或 skip 开始的尝试；准备下一条时为 null
   */
  attemptId: string | null;
  /**
   * `pending`, or the final status: `completed` (added, and started for `play`), `failed`, or
   * `superseded` by a later command.
   * @zh `pending`，或终态：`completed`（已加入，`play` 还已开始播放）、`failed`，或被之后的命令 `superseded`
   */
  status: 'pending' | 'completed' | 'failed' | 'superseded';
  /**
   * Why it failed or was superseded, or null.
   * @zh 失败或被取代的原因；没有时为 null
   */
  reason: string | null;
}

interface ReleaseParams {
  /**
   * Session to act on.
   * @zh 要操作的会话
   * @minLength 1
   */
  sessionId: string;
  /**
   * Control token from `jitSession.create` or `jitSession.claimControl`.
   * @zh 来自 `jitSession.create` 或 `jitSession.claimControl` 的控制令牌
   * @minLength 1
   */
  controlToken: string;
  /**
   * The `revision` the command is based on.
   * @zh 命令所依据的 `revision`
   * @minimum 0
   */
  expectedRevision: Int;
  /**
   * Caller-chosen key that makes a retry return the first answer instead of running again.
   * @zh 调用方选定的键，重试时返回第一次的应答而不是再执行一次
   * @minLength 1
   */
  requestId: string;
  /**
   * What to do with an adopted native context: `restore` (default) gives the remaining
   * occurrences back to the native queue and continues the playlist; `detach` leaves the native
   * queue and playlists as they are and keeps the occurrences readable. Only for a session that
   * adopted native playback.
   * @zh 如何处理已接纳的原生上下文：`restore`（默认）把剩余的出现交还原生队列并接着播放来源；`detach` 不动原生队列与播放列表，出现仍可读取。只用于接纳了原生播放的会话
   */
  nativeDisposition?: 'restore' | 'detach';
}

interface PlayNativeParams {
  /**
   * Session to act on.
   * @zh 要操作的会话
   * @minLength 1
   */
  sessionId: string;
  /**
   * Control token from `jitSession.create` or `jitSession.claimControl`.
   * @zh 来自 `jitSession.create` 或 `jitSession.claimControl` 的控制令牌
   * @minLength 1
   */
  controlToken: string;
  /**
   * The `revision` the command is based on.
   * @zh 命令所依据的 `revision`
   * @minimum 0
   */
  expectedRevision: Int;
  /**
   * Caller-chosen key that makes a retry return the first answer instead of running again.
   * @zh 调用方选定的键，重试时返回第一次的应答而不是再执行一次
   * @minLength 1
   */
  requestId: string;
  /**
   * The adopted context.
   * @zh 已接纳的上下文
   * @minLength 1
   */
  contextId: string;
  /**
   * A `nativeRef` read from `jitSession.getNativeContext`.
   * @zh 从 `jitSession.getNativeContext` 读到的 `nativeRef`
   * @minLength 1
   */
  nativeRef: string;
  /**
   * Caller's identifier for this entry.
   * @zh 调用方给这个条目的标识
   * @minLength 1
   */
  entryId: string;
  /**
   * Display title; the track's title tag when left out.
   * @zh 显示标题；不传时用曲目的标题标签
   */
  title?: string;
}

interface SetNativeNextParams {
  /**
   * Session to act on.
   * @zh 要操作的会话
   * @minLength 1
   */
  sessionId: string;
  /**
   * Control token from `jitSession.create` or `jitSession.claimControl`.
   * @zh 来自 `jitSession.create` 或 `jitSession.claimControl` 的控制令牌
   * @minLength 1
   */
  controlToken: string;
  /**
   * The `revision` the command is based on.
   * @zh 命令所依据的 `revision`
   * @minimum 0
   */
  expectedRevision: Int;
  /**
   * Caller-chosen key that makes a retry return the first answer instead of running again.
   * @zh 调用方选定的键，重试时返回第一次的应答而不是再执行一次
   * @minLength 1
   */
  requestId: string;
  /**
   * The adopted context.
   * @zh 已接纳的上下文
   * @minLength 1
   */
  contextId: string;
  /**
   * The current attempt the command is meant for.
   * @zh 命令所针对的当前尝试
   * @minLength 1
   */
  afterAttemptId: string;
  /**
   * A `nativeRef` read from `jitSession.getNativeContext`.
   * @zh 从 `jitSession.getNativeContext` 读到的 `nativeRef`
   * @minLength 1
   */
  nativeRef: string;
  /**
   * Caller's identifier for this entry.
   * @zh 调用方给这个条目的标识
   * @minLength 1
   */
  entryId: string;
  /**
   * Display title; the track's title tag when left out.
   * @zh 显示标题；不传时用曲目的标题标签
   */
  title?: string;
}

interface SetNativeQueueParams {
  /**
   * Session to act on.
   * @zh 要操作的会话
   * @minLength 1
   */
  sessionId: string;
  /**
   * Control token from `jitSession.create` or `jitSession.claimControl`.
   * @zh 来自 `jitSession.create` 或 `jitSession.claimControl` 的控制令牌
   * @minLength 1
   */
  controlToken: string;
  /**
   * The `revision` the command is based on.
   * @zh 命令所依据的 `revision`
   * @minimum 0
   */
  expectedRevision: Int;
  /**
   * Caller-chosen key that makes a retry return the first answer instead of running again.
   * @zh 调用方选定的键，重试时返回第一次的应答而不是再执行一次
   * @minLength 1
   */
  requestId: string;
  /**
   * The adopted context.
   * @zh 已接纳的上下文
   * @minLength 1
   */
  contextId: string;
  /**
   * The `contextRevision` the command is based on.
   * @zh 命令所依据的 `contextRevision`
   * @minimum 0
   */
  expectedContextRevision: Int;
  /**
   * Occurrences to keep, in the order to give them back.
   * @zh 要保留的出现，按交还的顺序
   */
  remainingOccurrenceIds: string[];
}

interface SetNativeQueueResult {
  /**
   * State version after accepting the command.
   * @zh 受理命令之后的状态版本
   */
  revision: Int;
  /**
   * Version of the context after the change.
   * @zh 变化之后的上下文版本
   */
  contextRevision: Int;
}

interface AdoptNativeParams {
  /**
   * Session to act on.
   * @zh 要操作的会话
   * @minLength 1
   */
  sessionId: string;
  /**
   * Control token from `jitSession.create` or `jitSession.claimControl`.
   * @zh 来自 `jitSession.create` 或 `jitSession.claimControl` 的控制令牌
   * @minLength 1
   */
  controlToken: string;
  /**
   * The `revision` the command is based on.
   * @zh 命令所依据的 `revision`
   * @minimum 0
   */
  expectedRevision: Int;
  /**
   * Caller-chosen key that makes a retry return the first answer instead of running again.
   * @zh 调用方选定的键，重试时返回第一次的应答而不是再执行一次
   * @minLength 1
   */
  requestId: string;
  /**
   * The context returned by `jitSession.prepareAdoption`.
   * @zh `jitSession.prepareAdoption` 返回的上下文
   * @minLength 1
   */
  contextId: string;
  /**
   * The snapshot token returned with it.
   * @zh 与之一同返回的快照令牌
   * @minLength 1
   */
  snapshotToken: string;
  /**
   * Caller's entry identifier for the track playing.
   * @zh 调用方给正在播放的曲目的条目标识
   * @minLength 1
   */
  currentEntryId: string;
}

interface AdoptNativeResult {
  /**
   * The context, now managed by the session.
   * @zh 上下文，此后由会话管理
   */
  contextId: string;
  /**
   * The attempt given to the track playing.
   * @zh 给正在播放的曲目的尝试
   */
  attemptId: string;
  /**
   * State version after accepting the command.
   * @zh 受理命令之后的状态版本
   */
  revision: Int;
  /**
   * Version of the context.
   * @zh 上下文的版本
   */
  contextRevision: Int;
}

interface PrepareAdoptionParams {
  /**
   * Session to act on.
   * @zh 要操作的会话
   * @minLength 1
   */
  sessionId: string;
  /**
   * Control token from `jitSession.create` or `jitSession.claimControl`.
   * @zh 来自 `jitSession.create` 或 `jitSession.claimControl` 的控制令牌
   * @minLength 1
   */
  controlToken: string;
  /**
   * The `revision` the command is based on.
   * @zh 命令所依据的 `revision`
   * @minimum 0
   */
  expectedRevision: Int;
  /**
   * Caller-chosen key that makes a retry return the first answer instead of running again.
   * @zh 调用方选定的键，重试时返回第一次的应答而不是再执行一次
   * @minLength 1
   */
  requestId: string;
}

interface PrepareAdoptionResult {
  /**
   * The native context.
   * @zh 原生上下文
   */
  contextId: string;
  /**
   * Authorizes adopting this snapshot once. Keep it in the page; it is never sent in events or
   * state.
   * @zh 授权接纳这次快照，只能用一次。由页面保管，不会出现在事件或状态中
   */
  snapshotToken: string;
  /**
   * When the snapshot expires unadopted, in Unix milliseconds; the host times it with its own
   * monotonic clock.
   * @zh 快照未被接纳时的过期时刻，Unix 毫秒；宿主按自己的单调时钟计时
   */
  expiresAt: number;
  /**
   * State version after accepting the command.
   * @zh 受理命令之后的状态版本
   */
  revision: Int;
  /**
   * Version of the context.
   * @zh 上下文的版本
   */
  contextRevision: Int;
  /**
   * The track playing; its `nativeRef` is the same as its source row's.
   * @zh 正在播放的曲目；它的 `nativeRef` 与所在来源行的相同
   */
  current: JitNativeTrack;
  /**
   * Occurrences captured from the native playback queue.
   * @zh 从原生播放队列记录的出现数
   */
  manualCount: Int;
  /**
   * Rows of the playlist being played.
   * @zh 正在播放的播放列表的行数
   */
  sourceCount: Int;
  /**
   * Source row after the track playing.
   * @zh 正在播放的曲目之后的来源行
   */
  sourceNextOffset: Int;
}

interface JitNativeTrack {
  /**
   * Host's reference to this occurrence or source row, valid within its context only.
   * @zh 宿主给这个出现或来源行的引用，只在所属上下文内有效
   */
  nativeRef: string;
  /**
   * foobar2000 path of the track.
   * @zh 曲目的 foobar2000 路径
   */
  path: string;
  /**
   * Subsong index within the file.
   * @zh 文件内的子曲目序号
   */
  subsong: Int;
  /**
   * Title tag, or the file name when there is none.
   * @zh 标题标签；没有时为文件名
   */
  title: string;
  /**
   * Playlist the track is in, or null for a queue entry without a playlist position.
   * @zh 曲目所在的播放列表；没有播放列表位置的队列项为 null
   */
  playlistGuid: string | null;
  /**
   * Row within that playlist, or null.
   * @zh 在该播放列表中的行；没有时为 null
   */
  row: Int | null;
}

interface GetNativeContextParams {
  /**
   * Session the context belongs to.
   * @zh 上下文所属的会话
   * @minLength 1
   */
  sessionId: string;
  /**
   * Context to read.
   * @zh 要读取的上下文
   * @minLength 1
   */
  contextId: string;
  /**
   * The `contextRevision` the page is based on.
   * @zh 页面所依据的 `contextRevision`
   * @minimum 0
   */
  expectedContextRevision: Int;
  /**
   * `manual` for the queue occurrences, `source` for the playlist rows.
   * @zh `manual` 读取队列出现，`source` 读取播放列表的行
   */
  collection: 'manual' | 'source';
  /**
   * First item: an occurrence index for `manual`, a playlist row for `source`.
   * @zh 第一项：`manual` 为出现序号，`source` 为播放列表的行
   * @minimum 0
   */
  offset: Int;
  /**
   * Items per page.
   * @zh 每页项数
   * @minimum 1
   * @maximum 128
   */
  limit: Int;
}

interface GetNativeContextResult {
  /**
   * The context, with the track captured as current and the counts.
   * @zh 上下文，附带记录时的当前曲目与各项数量
   */
  context: JitNativeContextDetail;
  /**
   * Number of items in the collection.
   * @zh 该集合的总项数
   */
  total: Int;
  /**
   * The page.
   * @zh 本页内容
   */
  items: JitNativeEntry[];
  /**
   * Offset of the next page, or null on the last page.
   * @zh 下一页的起点；最后一页为 null
   */
  nextOffset: Int | null;
}

interface JitNativeContextDetail {
  /**
   * The context.
   * @zh 上下文
   */
  contextId: string;
  /**
   * Version of the context.
   * @zh 上下文的版本
   */
  contextRevision: Int;
  /**
   * Status, as in the state snapshot.
   * @zh 状态，同状态快照
   */
  status: 'prepared' | 'managed' | 'restoring' | 'restored' | 'conflicted' | 'detached';
  /**
   * Why the context stopped applying, as in the state snapshot, or null.
   * @zh 上下文不再适用的原因，同状态快照；没有时为 null
   */
  reason: 'snapshotExpired' | 'nativeChanged' | null;
  /**
   * Control mode, as in the state snapshot.
   * @zh 控制模式，同状态快照
   */
  controlMode: 'playback' | 'recovery';
  /**
   * Source row native playback continues from when the context is given back.
   * @zh 交还上下文时原生播放接续的来源行
   */
  sourceNextOffset: Int;
  /**
   * The track playing when the snapshot was taken.
   * @zh 快照时正在播放的曲目
   */
  current: JitNativeTrack;
  /**
   * Occurrences captured from the native playback queue.
   * @zh 从原生播放队列记录的出现数
   */
  manualCount: Int;
  /**
   * Rows of the playlist being played when the snapshot was taken.
   * @zh 快照时正在播放的播放列表的行数
   */
  sourceCount: Int;
}

interface JitNativeEntry {
  /**
   * Host's reference, valid within this context only.
   * @zh 宿主给的引用，只在本上下文内有效
   */
  nativeRef: string;
  /**
   * The caller's entry bound to this reference, or null before it is supplied.
   * @zh 绑定到这个引用的调用方条目；供给之前为 null
   */
  entryId: string | null;
  /**
   * foobar2000 path of the track.
   * @zh 曲目的 foobar2000 路径
   */
  path: string;
  /**
   * Subsong index within the file.
   * @zh 文件内的子曲目序号
   */
  subsong: Int;
  /**
   * Title tag, or the file name when there is none.
   * @zh 标题标签；没有时为文件名
   */
  title: string;
  /**
   * Playlist the track is in, or null for a queue entry without a playlist position.
   * @zh 曲目所在的播放列表；没有播放列表位置的队列项为 null
   */
  playlistGuid: string | null;
  /**
   * Row within that playlist, or null.
   * @zh 在该播放列表中的行；没有时为 null
   */
  row: Int | null;
  /**
   * Occurrence identifier for `manual` items; null for source rows.
   * @zh `manual` 项的出现标识；来源行为 null
   */
  occurrenceId: string | null;
  /**
   * What happened to a `manual` occurrence: `pending` not handed to playback yet, `queued` in the
   * native queue as the next track, `submitted` taken by playback without a confirmed start,
   * `consumed` started, `discarded` removed by the caller, `indeterminate` handed over without a
   * known outcome, `restored` given back to the native queue. Null for source rows.
   * @zh `manual` 出现的去向：`pending` 尚未交给播放，`queued` 作为下一首在原生队列中，`submitted` 已被播放取走、起播未确认，`consumed` 已开始播放，`discarded` 被调用方删除，`indeterminate` 已交出但结局未知，`restored` 已交还原生队列。来源行为 null
   */
  disposition: 'pending' | 'queued' | 'submitted' | 'consumed' | 'discarded' | 'indeterminate' | 'restored' | null;
  /**
   * Position among the occurrences to give back, from 0; null when it will not be given back.
   * @zh 在待交还出现中的位置，从 0 起；不交还时为 null
   */
  restorationRank: Int | null;
}

interface StateChangedPayload {
  /**
   * The session.
   * @zh 会话
   */
  sessionId: string;
  /**
   * Order of this event within the session; consumers drop an event they have already seen.
   * @zh 本事件在会话内的顺序；消费方丢弃已处理过的事件
   */
  eventSeq: Int;
  /**
   * State version this event reflects.
   * @zh 本事件反映的状态版本
   */
  revision: Int;
  /**
   * Snapshot after the change.
   * @zh 变化之后的快照
   */
  state: JitSessionSnapshot;
}

interface NeedNextPayload {
  /**
   * The session.
   * @zh 会话
   */
  sessionId: string;
  /**
   * Order of this event within the session; consumers drop an event they have already seen.
   * @zh 本事件在会话内的顺序；消费方丢弃已处理过的事件
   */
  eventSeq: Int;
  /**
   * State version this event reflects.
   * @zh 本事件反映的状态版本
   */
  revision: Int;
  /**
   * The attempt that has no next entry.
   * @zh 没有下一条的那次尝试
   */
  afterAttemptId: string;
  /**
   * Identifier of this supply request; also readable in the snapshot.
   * @zh 本次供给请求的标识，也可以从快照读回
   */
  requestKey: string;
}

interface OperationCompletedPayload {
  /**
   * The session.
   * @zh 会话
   */
  sessionId: string;
  /**
   * Order of this event within the session; consumers drop an event they have already seen.
   * @zh 本事件在会话内的顺序；消费方丢弃已处理过的事件
   */
  eventSeq: Int;
  /**
   * State version this event reflects.
   * @zh 本事件反映的状态版本
   */
  revision: Int;
  /**
   * The operation in its final status.
   * @zh 处于终态的操作
   */
  operation: JitSessionOperation;
}

interface AttemptFinishedPayload {
  /**
   * The session.
   * @zh 会话
   */
  sessionId: string;
  /**
   * Order of this event within the session; consumers drop an event they have already seen.
   * @zh 本事件在会话内的顺序；消费方丢弃已处理过的事件
   */
  eventSeq: Int;
  /**
   * State version this event reflects.
   * @zh 本事件反映的状态版本
   */
  revision: Int;
  /**
   * The attempt that ended.
   * @zh 结束的尝试
   */
  attemptId: string;
  /**
   * Its entry.
   * @zh 它的条目
   */
  entryId: string;
  /**
   * `completed`: playback reached the end; `stopped`: stopped by the user or `jitSession.stop`;
   * `superseded`: another attempt or playback started; `released`: the session ended;
   * `unknown`: the host cannot attribute the ending.
   * @zh `completed`：播放到达末尾；`stopped`：被用户或 `jitSession.stop` 停止；`superseded`：另一次尝试或别的播放开始了；`released`：会话结束；`unknown`：宿主无法确认原因
   */
  outcome: 'completed' | 'stopped' | 'superseded' | 'released' | 'unknown';
  /**
   * Native stop reason, when one was observed; null for a command or an inferred transition.
   * @zh 已观察到的原生停止原因；由命令或归属变化结束时为 null
   */
  stopReason: 'user' | 'eof' | 'startingAnother' | 'shuttingDown' | null;
  /**
   * Last playback position read for the attempt, in seconds, or null.
   * @zh 这次尝试最后读到的播放位置，单位秒；没有时为 null
   */
  position: number | null;
  /**
   * Duration of the resource, in seconds, or null when unknown.
   * @zh 资源时长，单位秒；未知时为 null
   */
  duration: number | null;
}

interface AttemptRecheckedPayload {
  /**
   * The session.
   * @zh 会话
   */
  sessionId: string;
  /**
   * Order of this event within the session; consumers drop an event they have already seen.
   * @zh 本事件在会话内的顺序；消费方丢弃已处理过的事件
   */
  eventSeq: Int;
  /**
   * State version this event reflects.
   * @zh 本事件反映的状态版本
   */
  revision: Int;
  /**
   * The attempt whose resource was rechecked.
   * @zh 被复查资源的尝试
   */
  attemptId: string;
  /**
   * Its entry.
   * @zh 它的条目
   */
  entryId: string;
  /**
   * `reproduced`: the recheck failed too; `notReproduced`: the resource opened and decoded its
   * first block this time.
   * @zh `reproduced`：复查同样失败；`notReproduced`：这次资源能打开并解码出第一块
   */
  result: 'reproduced' | 'notReproduced';
  /**
   * Why the recheck failed, or null when it did not: `accessDenied` (for example HTTP 403),
   * `notFound` (HTTP 404 or a missing file), `unsupportedContent` (not audio, or a format no
   * installed decoder reads), `network` (other HTTP errors, refused or timed-out connections),
   * `other`.
   * @zh 复查失败的原因，未失败时为 null：`accessDenied`（如 HTTP 403）、`notFound`（HTTP 404 或文件不存在）、`unsupportedContent`（不是音频，或没有已安装的解码器能读）、`network`（其他 HTTP 错误、连接被拒或超时）、`other`
   */
  reason: 'accessDenied' | 'notFound' | 'unsupportedContent' | 'network' | 'other' | null;
  /**
   * Where the recheck failed: opening the resource, initializing the decoder, or decoding the
   * first block; null when it did not fail.
   * @zh 复查在哪一步失败：打开资源、初始化解码器或解码第一块；未失败时为 null
   */
  stage: 'open' | 'initialize' | 'decode' | null;
  /**
   * Whether foobar2000 logged a playback failure naming this resource within the last minute,
   * for example that it could not open it.
   * @zh 最近一分钟内，foobar2000 是否记录过指明该资源的播放失败，例如无法打开它
   */
  engineReported: boolean;
}

interface ControlChangedPayload {
  /**
   * The session.
   * @zh 会话
   */
  sessionId: string;
  /**
   * Order of this event within the session; consumers drop an event they have already seen.
   * @zh 本事件在会话内的顺序；消费方丢弃已处理过的事件
   */
  eventSeq: Int;
  /**
   * State version this event reflects.
   * @zh 本事件反映的状态版本
   */
  revision: Int;
  /**
   * Control generation after the change.
   * @zh 变化之后的控制代次
   */
  ownerEpoch: Int;
  /**
   * `claimed` by another page, taken over by a `jitQueue` call (`legacyTakeover`), or
   * `released`.
   * @zh 被其他页面认领（`claimed`）、被 `jitQueue` 调用接管（`legacyTakeover`），或已释放（`released`）
   */
  reason: 'claimed' | 'legacyTakeover' | 'released';
}
