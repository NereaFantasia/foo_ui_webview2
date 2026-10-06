import type { Int } from './common.js';

export interface Api {
  /**
   * Start a new JIT session with this track: the buffer is cleared and playback begins. The
   * host detects the URL kind itself (`http(s)://` streams, Windows or UNC paths are local).
   * @zh 用这首曲目开启新的 JIT 会话：清空缓冲并开始播放。宿主自己判断 URL 类型（`http(s)://` 是流，Windows 或 UNC 路径是本地文件）。
   */
  playNow(params: PlayNowParams): PlayNowResult;

  /**
   * Preload the next track into the buffer, in answer to `jitQueue:needNext`. Refused with
   * `NO_ACTIVE_ITEM` unless a JIT session is playing or waiting for its next track.
   * @zh 把下一首预载进缓冲，用于响应 `jitQueue:needNext`。除非有 JIT 会话正在播放或正在等下一首，否则以 `NO_ACTIVE_ITEM` 拒绝。
   */
  enqueueNext(params: EnqueueNextParams): EnqueueNextResult;

  /**
   * Skip to the next buffered track. Refused with `NO_ACTIVE_ITEM` while no JIT session is
   * active.
   * @zh 跳到缓冲中的下一首。没有活动的 JIT 会话时以 `NO_ACTIVE_ITEM` 拒绝。
   */
  skip(): SkipResult;

  /**
   * Stop playback, optionally keeping the buffer.
   * @zh 停止播放，可选保留缓冲。
   */
  stop(params: StopParams): void;

  /**
   * Empty the buffer and return the session to idle.
   * @zh 清空缓冲并让会话回到空闲。
   */
  clear(): void;

  /**
   * Report the JIT session state.
   * @zh 报告 JIT 会话的状态。
   */
  getState(): GetStateResult;

  /**
   * Tell the host the page has no more tracks to offer; the session moves to `Exhausted` and
   * `jitQueue:listExhausted` is emitted.
   * @zh 告诉宿主页面没有更多曲目可供；会话进入 `Exhausted` 并发出 `jitQueue:listExhausted`。
   */
  notifyEmpty(): void;

  /**
   * Insert a batch of tracks into the shadow playlist, starting playback from `startIndex` when
   * the session is not already playing.
   * @zh 把一批曲目插入影子播放列表，会话尚未在播放时从 `startIndex` 起播。
   */
  preloadBatch(params: PreloadBatchParams): PreloadBatchResult;
}

export interface Events {
  /**
   * A track of the shadow playlist started playing, a replay of the current track included.
   * `trackId` and `title` are the session's current track as `jitQueue.playNow` or
   * `jitQueue.enqueueNext` named it. Tracks inserted by `jitQueue.preloadBatch` have no
   * identifier: playing one leaves both unchanged, and both are empty once a batch has started
   * playback. The JIT events go to the page that started the session, with `jitQueue.playNow` or
   * with a `jitQueue.preloadBatch` that started playback; when that page is gone, to a page under
   * the same top-level window, and else to the main window's page. A page whose session another
   * page started anew hears nothing more of it. Before any session has started they go to the
   * main window's page.
   * @zh 影子播放列表里的一首曲目开始播放，重播当前曲目也算。`trackId` 与 `title` 是会话当前曲目在 `jitQueue.playNow` 或 `jitQueue.enqueueNext` 里给的标识与标题。`jitQueue.preloadBatch` 插入的曲目没有标识：播到它们时两者不变，某一批开始播放之后两者都为空。JIT 事件发给开启会话的页面，即调用 `jitQueue.playNow` 或调用了开始播放的 `jitQueue.preloadBatch` 的页面；该页面已不在时发给同一顶层窗口下的页面，再没有就发给主窗口的页面。会话被别的页面重新开启后，原来的页面不再收到它的事件。还没有开启过会话时发给主窗口的页面。
   * @delivery caller
   */
  trackChanged: TrackChangedPayload;

  /**
   * The host needs the next track: none is buffered and no earlier request is waiting for an
   * answer. It checks when a track of the shadow playlist starts, dropping a request left
   * unanswered during the previous track, and again when 30 seconds of the track remain. The
   * page answers with `jitQueue.enqueueNext`, or with `jitQueue.notifyEmpty` when it has no more
   * tracks. Delivered like `jitQueue:trackChanged`.
   * @zh 宿主需要下一首：缓冲里没有，也没有仍在等回复的请求。宿主在影子播放列表的曲目开始时检查（上一首期间没得到回复的请求就此作废），曲目剩 30 秒时再检查一次。页面以 `jitQueue.enqueueNext` 作答，没有更多曲目时以 `jitQueue.notifyEmpty` 作答。投递范围同 `jitQueue:trackChanged`。
   * @delivery caller
   */
  needNext: NeedNextPayload;

  /**
   * `jitQueue.notifyEmpty` was called, even with no session active, or playback of the shadow
   * playlist reached the end of a track while the playlist held no tracks. The session is now
   * `Exhausted`. Delivered like `jitQueue:trackChanged`.
   * @zh 调用了 `jitQueue.notifyEmpty`（没有活动会话时也发），或影子播放列表的播放在一首曲目结束时列表里已没有曲目。会话进入 `Exhausted`。投递范围同 `jitQueue:trackChanged`。
   * @delivery caller
   */
  listExhausted: ListExhaustedPayload;

  /**
   * `jitQueue.preloadBatch` inserted its tracks into the shadow playlist. Sent before the call
   * returns; a call that fails sends nothing. Delivered like `jitQueue:trackChanged`: a batch that
   * starts playback reports to the page that called it, a batch appended to a playing session
   * reports to that session's page.
   * @zh `jitQueue.preloadBatch` 把曲目插入了影子播放列表。在调用返回之前发出；调用失败时不发。投递范围同 `jitQueue:trackChanged`：开始播放的一批发给调用它的页面，追加到正在播放的会话里的一批发给该会话的页面。
   * @delivery caller
   */
  preloadComplete: PreloadCompletePayload;

  /**
   * A track given to `jitQueue.playNow` or `jitQueue.enqueueNext` could not be added to the
   * shadow playlist: its location resolved to no tracks, the shadow playlist was gone, or adding
   * it failed with an exception. The track is added after the call has already succeeded, so the
   * failure arrives only as this event. Delivered like `jitQueue:trackChanged`.
   * @zh 交给 `jitQueue.playNow` 或 `jitQueue.enqueueNext` 的曲目没能加入影子播放列表：位置解析不出曲目、影子播放列表已不存在，或加入时抛出异常。曲目在调用成功返回之后才加入，所以失败只以这个事件告知。投递范围同 `jitQueue:trackChanged`。
   * @delivery caller
   */
  error: ErrorPayload;
}

interface TrackChangedPayload {
  /**
   * Identifier of the session's current track; empty when it has none.
   * @zh 会话当前曲目的标识；没有时为空。
   */
  trackId: string;
  /**
   * Title of the session's current track; empty when it has none or none was given.
   * @zh 会话当前曲目的标题；没有当前曲目或没给标题时为空。
   */
  title: string;
}

interface NeedNextPayload {
  /**
   * Identifier of the session's current track; empty when it has none.
   * @zh 会话当前曲目的标识；没有时为空。
   */
  currentTrackId: string;
  /**
   * Always `trackChange`.
   * @zh 恒为 `trackChange`。
   */
  reason: 'trackChange';
}

interface ListExhaustedPayload {
  /**
   * Identifier of the session's current track; empty when it has none.
   * @zh 会话当前曲目的标识；没有时为空。
   */
  lastTrackId: string;
}

interface PreloadCompletePayload {
  /**
   * How many tracks were inserted; the same as `tracksAdded` of the call.
   * @zh 插入的曲目数；与调用返回的 `tracksAdded` 相同。
   */
  count: Int;
  /**
   * `startIndex` of the call.
   * @zh 调用的 `startIndex`。
   */
  startIndex: Int;
  /**
   * `replace` of the call.
   * @zh 调用的 `replace`。
   */
  replace: boolean;
}

interface ErrorPayload {
  /**
   * Identifier the call gave the track.
   * @zh 调用给该曲目的标识。
   */
  trackId: string;
  /**
   * What went wrong, in English.
   * @zh 出错原因，英文。
   */
  error: string;
  /**
   * The location, when it is not a local file path. Exactly one of `url` and `path` is present.
   * @zh 位置，不是本地文件路径时出现。`url` 与 `path` 恰好出现一个。
   */
  url?: string;
  /**
   * The location, when it is a local file path.
   * @zh 位置，是本地文件路径时出现。
   */
  path?: string;
}

/**
 * State of the JIT session.
 * @zh JIT 会话的状态。
 */
type JitQueueState = 'Idle' | 'Active' | 'WaitingNext' | 'Exhausted' | 'Unknown';

interface PlayNowParams {
  /**
   * Caller-assigned identifier of the track, echoed in the session state and events.
   * @zh 调用方给曲目的标识，会在会话状态与事件里回显。
   * @minLength 1
   */
  trackId: string;
  /**
   * Display title.
   * @zh 显示标题。
   * @default ""
   */
  title?: string;
  /**
   * Stream URL or file path. At most 2048 characters.
   * @zh 流 URL 或文件路径。最多 2048 字符。
   * @minLength 1
   * @security MediaRead
   */
  url: string;
}

interface PlayNowResult {
  /**
   * The identifier given.
   * @zh 传入的标识。
   */
  trackId: string;
  /**
   * Index of the shadow playlist, which the call creates or takes over when this session has
   * none; `-1` only if foobar2000 could not create it.
   * @zh 影子播放列表的索引；本次会话还没有时由这次调用创建或接管；只有 foobar2000 建不出来时才是 `-1`。
   */
  shadowPlaylist: Int;
}

interface EnqueueNextParams {
  /**
   * Caller-assigned identifier of the track.
   * @zh 调用方给曲目的标识。
   * @minLength 1
   */
  trackId: string;
  /**
   * Display title.
   * @zh 显示标题。
   * @default ""
   */
  title?: string;
  /**
   * Stream URL or file path. At most 2048 characters.
   * @zh 流 URL 或文件路径。最多 2048 字符。
   * @minLength 1
   * @security MediaRead
   */
  url: string;
}

interface EnqueueNextResult {
  /**
   * The identifier given.
   * @zh 传入的标识。
   */
  trackId: string;
  /**
   * Tracks in the buffer afterwards.
   * @zh 之后缓冲里的曲目数。
   */
  bufferSize: Int;
}

interface SkipResult {
  /**
   * Identifier of the track playing after the skip.
   * @zh 跳过后正在播放的曲目标识。
   */
  currentTrackId: string;
}

interface StopParams {
  /**
   * Also empty the buffer.
   * @zh 是否同时清空缓冲。
   * @default true
   */
  clearBuffer?: boolean;
}

interface GetStateResult {
  /**
   * Whether a JIT session is active.
   * @zh 是否有活动的 JIT 会话。
   */
  isActive: boolean;
  /**
   * The session state.
   * @zh 会话状态。
   */
  state: JitQueueState;
  /**
   * Identifier of the playing track; empty when none.
   * @zh 正在播放的曲目标识；没有时为空。
   */
  currentTrackId: string;
  /**
   * Identifier of the buffered next track; empty when none.
   * @zh 已缓冲的下一首标识；没有时为空。
   */
  nextTrackId: string;
  /**
   * Tracks in the buffer.
   * @zh 缓冲里的曲目数。
   */
  bufferSize: Int;
  /**
   * Index of the shadow playlist; `-1` until a `jitQueue.playNow` or `jitQueue.preloadBatch` of
   * this foobar2000 session creates or takes it over, even when one is left from an earlier
   * session, and again after the playlist is removed, which also ends the JIT session. Stopping
   * or clearing keeps the index of the emptied playlist.
   * @zh 影子播放列表的索引；在本次 foobar2000 运行中的 `jitQueue.playNow` 或 `jitQueue.preloadBatch` 创建或接管它之前为 `-1`，即便上次运行留下了一个；列表被删掉之后也回到 `-1`，同时 JIT 会话结束。停止或清空保留那张已清空列表的索引。
   */
  shadowPlaylist: Int;
}

interface PreloadBatchParams {
  /**
   * Track URLs or `path|subsong:N` values. Entries longer than 2048 characters are dropped and
   * counted in `invalidCount`; at most 10000 entries may remain.
   * @zh 曲目 URL 或 `path|subsong:N`。超过 2048 字符的条目丢弃并计入 `invalidCount`；剩余最多 10000 条。
   * @security MediaRead
   */
  urls?: string[];
  /**
   * Which entry to start playing from.
   * @zh 从第几条起播。
   * @minimum 0
   * @default 0
   */
  startIndex?: Int;
  /**
   * Empty the shadow playlist before inserting. `false` appends.
   * @zh 插入前先清空影子播放列表。`false` 为追加。
   * @default true
   */
  replace?: boolean;
}

interface PreloadBatchResult {
  /**
   * How many tracks were inserted.
   * @zh 插入的曲目数。
   */
  tracksAdded: Int;
  /**
   * How many entries were dropped for being too long.
   * @zh 因过长被丢弃的条数。
   */
  invalidCount: Int;
}
