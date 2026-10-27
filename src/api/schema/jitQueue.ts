import type { Int } from './common.js';

export interface Api {
  /**
   * Start a new JIT session with this track: the buffer is cleared and playback begins. The
   * host detects the URL kind itself (`http(s)://` streams, Windows or UNC paths are local).
   * Tracks in the native playback queue are kept: when this track ends, foobar2000 plays them
   * first and the JIT session ends.
   * @zh 基于指定曲目开启全新 JIT 会话（清空当前缓冲并立即起播；宿主自动识别 URL 协议与本地文件路径；原生播放队列保留并在本会话结束后接续播放）
   */
  playNow(params: PlayNowParams): PlayNowResult;

  /**
   * Preload the next track into the buffer, in answer to `jitQueue:needNext`. Refused with
   * `NO_ACTIVE_ITEM` unless a JIT session is playing or waiting for its next track.
   * @zh 将下一首曲目预载进播放缓冲（用于响应 jitQueue:needNext 事件；仅在存在活跃会话播放或等待下一首时受理，否则返回 NO_ACTIVE_ITEM）
   */
  enqueueNext(params: EnqueueNextParams): EnqueueNextResult;

  /**
   * Skip to the next buffered track. When a buffered track follows the playing one, it plays
   * that track and leaves the native playback queue untouched. Otherwise it acts as foobar2000's
   * Next command, which plays the first track of the native playback queue when the queue is not
   * empty. Refused with `NO_ACTIVE_ITEM` while no JIT session is active.
   * @zh 跳过当前曲目并播放缓冲中的下一首（若缓冲存在后续曲目则直接播放且不改动原生队列；否则执行原生下一首逻辑；无活跃会话时返回 NO_ACTIVE_ITEM）
   */
  skip(): SkipResult;

  /**
   * Stop playback, optionally keeping the buffer.
   * @zh 停止当前播放（支持选择是否保留已缓冲内容）
   */
  stop(params: StopParams): void;

  /**
   * Empty the buffer and return the session to idle.
   * @zh 清空当前播放缓冲并使会话恢复空闲状态
   */
  clear(): void;

  /**
   * Report the JIT session state.
   * @zh 查询 JIT 会话当前运行状态
   */
  getState(): GetStateResult;

  /**
   * Tell the host the page has no more tracks to offer; the session moves to `Exhausted` and
   * `jitQueue:listExhausted` is emitted.
   * @zh 通知宿主当前会话无更多后续曲目（会话进入 Exhausted 状态并派发 jitQueue:listExhausted 事件）
   */
  notifyEmpty(): void;

  /**
   * Insert a batch of tracks into the shadow playlist, starting playback from `startIndex` when
   * the session is not already playing.
   * @zh 将一组曲目批量写入影子播放列表（若会话尚未播放则自动从 startIndex 处开始起播）
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
   * @zh 影子播放列表曲目起播事件（涵盖重复播放当前曲目；trackId 与 title 反映 playNow 或 enqueueNext 配置的元数据；事件定向派发给启动会话的页面）
   * @delivery caller
   */
  trackChanged: TrackChangedPayload;

  /**
   * The host needs the next track: none is buffered and no earlier request is waiting for an
   * answer. It checks when a track of the shadow playlist starts, dropping a request left
   * unanswered during the previous track, and again when 30 seconds of the track remain. The
   * page answers with `jitQueue.enqueueNext`, or with `jitQueue.notifyEmpty` when it has no more
   * tracks. Delivered like `jitQueue:trackChanged`.
   * @zh 宿主请求下一首曲目事件（当缓冲为空且无进行中请求时触发，于曲目起播及剩余 30 秒时检查；页面须调用 enqueueNext 预载或 notifyEmpty 告知耗尽）
   * @delivery caller
   */
  needNext: NeedNextPayload;

  /**
   * `jitQueue.notifyEmpty` was called, even with no session active, or playback of the shadow
   * playlist reached the end of a track while the playlist held no tracks. The session is now
   * `Exhausted`. Delivered like `jitQueue:trackChanged`.
   * @zh 会话曲目列表耗尽事件（由调用 notifyEmpty 触发或影子列表播放至末尾无后续曲目触发；会话状态流转为 Exhausted）
   * @delivery caller
   */
  listExhausted: ListExhaustedPayload;

  /**
   * `jitQueue.preloadBatch` inserted its tracks into the shadow playlist. Sent before the call
   * returns; a call that fails sends nothing. Delivered like `jitQueue:trackChanged`: a batch that
   * starts playback reports to the page that called it, a batch appended to a playing session
   * reports to that session's page.
   * @zh 批量预载曲目写入影子播放列表完成事件（在 preloadBatch 调用返回前派发）
   * @delivery caller
   */
  preloadComplete: PreloadCompletePayload;

  /**
   * A track given to `jitQueue.playNow` or `jitQueue.enqueueNext` could not be added to the
   * shadow playlist: its location resolved to no tracks, the shadow playlist was gone, or adding
   * it failed with an exception. The track is added after the call has already succeeded, so the
   * failure arrives only as this event. Not sent for an add superseded before it completes: by a
   * later `jitQueue.playNow`, another `jitQueue.enqueueNext` for the next track,
   * `jitQueue.stop`, `jitQueue.clear`, a `jitQueue.preloadBatch` that starts playback, the user
   * stopping playback, or playback leaving the shadow playlist; such a track is neither added nor
   * played. Advancing to an already buffered next track also supersedes an unresolved current
   * track. Delivered like `jitQueue:trackChanged`.
   * @zh 曲目加入影子播放列表失败事件（当路径无法解析、列表被外部移除或底层抛出异常时在异步加入完成后派发；若加入前被新操作取代则不派发）
   * @delivery caller
   */
  error: ErrorPayload;
}

interface TrackChangedPayload {
  /**
   * Identifier of the session's current track; empty when it has none.
   * @zh 会话当前播放曲目的标识符 trackId（缺省时为空字符串）
   */
  trackId: string;
  /**
   * Title of the session's current track; empty when it has none or none was given.
   * @zh 会话当前播放曲目的显示标题（缺省时为空字符串）
   */
  title: string;
}

interface NeedNextPayload {
  /**
   * Identifier of the session's current track; empty when it has none.
   * @zh 会话当前播放曲目的标识符 trackId（缺省时为空字符串）
   */
  currentTrackId: string;
  /**
   * Always `trackChange`.
   * @zh 请求触发原因（固定为 trackChange）
   */
  reason: 'trackChange';
}

interface ListExhaustedPayload {
  /**
   * Identifier of the session's current track; empty when it has none.
   * @zh 会话当前播放曲目的标识符 trackId（缺省时为空字符串）
   */
  lastTrackId: string;
}

interface PreloadCompletePayload {
  /**
   * How many tracks were inserted; the same as `tracksAdded` of the call.
   * @zh 实际成功插入的曲目总数（与调用返回的 tracksAdded 保持一致）
   */
  count: Int;
  /**
   * `startIndex` of the call.
   * @zh 调用时传入的起始播放索引 startIndex
   */
  startIndex: Int;
  /**
   * `replace` of the call.
   * @zh 调用时传入的替换模式标记 replace
   */
  replace: boolean;
}

interface ErrorPayload {
  /**
   * Identifier the call gave the track.
   * @zh 添加失败曲目的原始标识符 trackId
   */
  trackId: string;
  /**
   * What went wrong, in English.
   * @zh 底层返回的错误原因描述
   */
  error: string;
  /**
   * The location, when it is not a local file path. Exactly one of `url` and `path` is present.
   * @zh 发生错误的远程 URL 地址（与 path 互斥提供）
   */
  url?: string;
  /**
   * The location, when it is a local file path.
   * @zh 发生错误的本地文件绝对路径（与 url 互斥提供）
   */
  path?: string;
}

/**
 * State of the JIT session.
 * @zh JIT 会话状态枚举
 */
type JitQueueState = 'Idle' | 'Active' | 'WaitingNext' | 'Exhausted' | 'Unknown';

interface PlayNowParams {
  /**
   * Caller-assigned identifier of the track, echoed in the session state and events.
   * @zh 调用方分配的曲目唯一标识符 trackId（在状态查询与事件中回显）
   * @minLength 1
   */
  trackId: string;
  /**
   * Display title.
   * @zh 用于界面展示的曲目标题
   * @default ""
   */
  title?: string;
  /**
   * Stream URL or file path. At most 2048 characters.
   * @zh 音频流 URL 或本地文件物理路径（最多 2048 字符）
   * @minLength 1
   * @security MediaRead
   */
  url: string;
}

interface PlayNowResult {
  /**
   * The identifier given.
   * @zh 传入的曲目标识符 trackId
   */
  trackId: string;
  /**
   * Index of the shadow playlist, which the call creates or takes over when this session has
   * none; `-1` only if foobar2000 could not create it.
   * @zh 影子播放列表在播放列表管理器中的索引（不存在时由宿主自动创建接管，创建失败返回 -1）
   */
  shadowPlaylist: Int;
}

interface EnqueueNextParams {
  /**
   * Caller-assigned identifier of the track.
   * @zh 调用方分配的曲目唯一标识符 trackId
   * @minLength 1
   */
  trackId: string;
  /**
   * Display title.
   * @zh 用于界面展示的曲目标题
   * @default ""
   */
  title?: string;
  /**
   * Stream URL or file path. At most 2048 characters.
   * @zh 音频流 URL 或本地文件物理路径（最多 2048 字符）
   * @minLength 1
   * @security MediaRead
   */
  url: string;
}

interface EnqueueNextResult {
  /**
   * The identifier given.
   * @zh 传入的曲目标识符 trackId
   */
  trackId: string;
  /**
   * Tracks in the buffer afterwards.
   * @zh 预载完成后缓冲内的曲目总数
   */
  bufferSize: Int;
}

interface SkipResult {
  /**
   * Identifier of the session's current track when the call returns. The skipped-to track has
   * not started yet at that point, so this is still the track being skipped; the new one is
   * reported by `jitQueue:trackChanged`.
   * @zh 调用返回时会话记录的曲目标识符（此时新曲目尚未起播，仍为被跳过的曲目 ID，起播后由 jitQueue:trackChanged 通知）
   */
  currentTrackId: string;
}

interface StopParams {
  /**
   * Also empty the buffer.
   * @zh 停止播放时是否同步清空已缓冲的曲目
   * @default true
   */
  clearBuffer?: boolean;
}

interface GetStateResult {
  /**
   * Whether a JIT session is active.
   * @zh 当前是否存在活跃运行的 JIT 会话
   */
  isActive: boolean;
  /**
   * The session state.
   * @zh 会话当前运行状态
   */
  state: JitQueueState;
  /**
   * Identifier of the playing track; empty when none.
   * @zh 当前正在播放的曲目标识符 trackId（缺省时为空字符串）
   */
  currentTrackId: string;
  /**
   * Identifier of the buffered next track; empty when none.
   * @zh 已预载到缓冲的下一首曲目标识符（缺省时为空字符串）
   */
  nextTrackId: string;
  /**
   * Tracks in the buffer.
   * @zh 当前缓冲队列中待播放的曲目总数
   */
  bufferSize: Int;
  /**
   * Index of the shadow playlist; `-1` until a `jitQueue.playNow` or `jitQueue.preloadBatch` of
   * this foobar2000 session creates or takes it over, even when one is left from an earlier
   * session, and again after the playlist is removed, which also ends the JIT session. Stopping
   * or clearing keeps the index of the emptied playlist.
   * @zh 影子播放列表索引（被外部删除时重置为 -1 并终止会话；停止或清空操作会保留空列表的索引）
   */
  shadowPlaylist: Int;
}

interface PreloadBatchParams {
  /**
   * Track URLs or `path|subsong:N` values. Entries longer than 2048 characters are dropped and
   * counted in `invalidCount`; at most 10000 entries may remain.
   * @zh 待批量预载的音频 URL 或「path|subsong:N」条目列表（超出 2048 字符的条目将被丢弃并计入 invalidCount；单次最多支持 10000 条）
   * @security MediaRead
   */
  urls?: string[];
  /**
   * Which entry to start playing from.
   * @zh 批量插入后起始起播的曲目索引 startIndex
   * @minimum 0
   * @default 0
   */
  startIndex?: Int;
  /**
   * Empty the shadow playlist before inserting. `false` appends.
   * @zh 插入前是否清空影子播放列表（false 为追加到末尾）
   * @default true
   */
  replace?: boolean;
}

interface PreloadBatchResult {
  /**
   * How many tracks were inserted.
   * @zh 实际成功追加或写入的曲目数量
   */
  tracksAdded: Int;
  /**
   * How many entries were dropped for being too long.
   * @zh 因长度超出限制等原因被丢弃的无效条目数
   */
  invalidCount: Int;
}
