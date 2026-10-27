import type {
  ContentVersion,
  ExpectedContentVersion,
  Int,
  PlaylistGuid,
  ReportedPlaylistGuid,
  Track,
} from './common.js';

export interface Api {
  /**
   * Read the whole playback queue, in play order. No paging: every entry comes back.
   * @zh 按播放顺序读取完整待播队列；不分页一次性返回全量条目
   * @effect read
   */
  get(): GetResult;

  /**
   * Report how many entries the queue holds.
   * @zh 获取待播队列当前容纳的条目总数
   */
  getCount(): GetCountResult;

  /**
   * Queue one or more tracks by their position in a playlist. Positions past the last row are
   * skipped; when none is in range the call fails with `INVALID_INDEX`. A negative position fails
   * with `INVALID_PARAMS`.
   * @zh 按播放列表行号将一首或多首曲目追加至待播队列；超出列表范围的索引自动忽略，无任一有效索引时以 `INVALID_INDEX` 失败，负数索引以 `INVALID_PARAMS` 失败
   * @effect write
   */
  add(params: AddParams): AddResult;

  /**
   * Queue tracks by path. Queueing needs playlist membership, so the paths are appended to a
   * playlist first: by default a dedicated `[WebView Queue]` playlist, created when missing. A
   * locked target playlist fails with `LOCKED`, and nothing is added or queued. The target is
   * looked up again after the paths are resolved, as in `playlist.addPaths`: moved meanwhile, it
   * still gets the tracks and `playlist` in the result is its new index; removed meanwhile, the
   * call fails with `OPERATION_FAILED` and nothing is queued. A path the media-root check refuses
   * fails the whole call with `PERMISSION_DENIED`. Paths that resolve to nothing are dropped;
   * when none resolves the call fails with `NOT_FOUND`, the failure carrying `invalidCount`.
   * @zh 按文件路径或 URL 将曲目加入待播队列；宿主会先将曲目追加至播放列表（默认专用 `[WebView Queue]` 列表，不存在则自动创建）后再入队；目标列表已锁定时以 `LOCKED` 失败且不执行追加与入队；路径解析后重新定位目标列表，若期间被删除则以 `OPERATION_FAILED` 失败；未通过媒体根目录安全检查时整次调用以 `PERMISSION_DENIED` 失败；解析失败的无效路径自动丢弃，无任一有效曲目时以 `NOT_FOUND` 失败并在 `invalidCount` 中注明
   * @effect write
   */
  addPaths(params: AddPathsParams): AddPathsResult;

  /**
   * Remove one entry by `index`, or several by `indices`. An empty queue fails with `NOT_FOUND`
   * whatever is given. An `index` past the end fails with `INVALID_INDEX`. In a batch, duplicate
   * indices and those past the end of the queue are skipped; when none is in range the call fails
   * with `INVALID_INDEX`, the failure carrying `removedCount` `0` and `queueCount`. Giving neither
   * key, or a negative index, fails with `INVALID_PARAMS`.
   * @zh 按队列索引 `index` 或索引列表 `indices` 从待播队列中移除条目；队列为空时一律以 `NOT_FOUND` 失败；单项 `index` 越界以 `INVALID_INDEX` 失败；批量移除时跳过重复或越界索引，无任一有效索引时以 `INVALID_INDEX` 失败（错误详情附带 `removedCount: 0` 与 `queueCount`）；未提供索引或传入负数时以 `INVALID_PARAMS` 失败
   * @effect destructive
   */
  remove(params: RemoveParams): RemoveResult;

  /**
   * Empty the queue.
   * @zh 清空待播队列中的所有条目
   * @effect destructive
   * @idempotent
   */
  clear(): ClearResult;

  /**
   * Empty the queue; the same operation as `queue.clear` under its older name.
   * @zh 清空待播队列中的所有条目（旧名别名，功能同 `queue.clear`）
   */
  flush(): FlushResult;

  /**
   * Move a queued entry to the front so it plays next. Only queue order changes; the relative
   * order of the other entries is kept. An `index` of `0`, one past the end, and any index while
   * the queue is empty fail with `INVALID_INDEX`. When the queue cannot be rebuilt the call fails
   * with `OPERATION_FAILED`, the failure carrying `queueCount`.
   * @zh 将指定队列位置的条目移动至队首以便下一首播放；仅调整队列排序，保持其他条目的相对顺序；传入 `index: 0`、越界索引或队列为空时均以 `INVALID_INDEX` 失败；队列重建失败时以 `OPERATION_FAILED` 失败（附带 `queueCount`）
   * @effect write
   */
  moveToTop(params: MoveToTopParams): MoveToTopResult;

  /**
   * Replace the whole queue with an ordered list of references. One unusable reference fails the
   * whole call before anything is written. An empty list clears the queue. A playlist index is
   * read as the call arrives, so one taken before the playlist list changed queues a row of
   * another playlist; `playlistGuid` keeps naming the playlist it was read from. An entry giving
   * both `playlist` and `playlistGuid`, or a malformed GUID, fails with `INVALID_PARAMS`, a GUID
   * no playlist has with `NOT_FOUND`.
   * @zh 使用指定的有序引用列表全量替换当前待播队列；任一引用项校验失败整次调用立即终止且不写入变更；传入空列表等同清空队列；播放列表索引以调用到达时的状态为准，使用 `playlistGuid` 可稳定引用目标列表；单项同时传入 `playlist` 与 `playlistGuid` 或 GUID 格式非法时以 `INVALID_PARAMS` 失败，GUID 未匹配到列表时以 `NOT_FOUND` 失败
   * @effect destructive
   * @idempotent
   */
  setContents(params: SetContentsParams): SetContentsResult;

  /**
   * Insert tracks so they play next, ahead of everything already queued. A track that is already
   * queued is moved instead of queued twice. `items` land before `paths`; each block keeps its own
   * order. A call with neither `paths` nor `items` fails with `INVALID_PARAMS`. Every entry of
   * `items` is checked before any path is resolved, and one bad entry fails the whole call with
   * nothing written: an entry naming its playlist by neither or both of `playlist` and
   * `playlistGuid`, or by a malformed GUID, fails with `INVALID_PARAMS`, a GUID no playlist has
   * with `NOT_FOUND`, a playlist index or row out of range with `INVALID_INDEX`. A path the
   * media-root check refuses fails the whole call with `PERMISSION_DENIED`; a path that resolves
   * to nothing is dropped and counted in `invalidCount`, and when nothing at all is left to
   * insert the call fails with `NOT_FOUND`. A playlist row that changed while the paths were
   * resolved fails the call with `OPERATION_FAILED`, and so does a queue that cannot be rebuilt.
   * `expectedVersion` is compared when the queue is written, after the paths are resolved.
   * @zh 将指定曲目插入待播队列前端以便下一首播放；已存在于队列中的曲目直接前移而不重复入队；`items` 列表排在 `paths` 列表之前，各自保持输入顺序；未提供 `paths` 与 `items` 时以 `INVALID_PARAMS` 失败；在解析路径前预检所有 `items` 引用，任一项非法立即失败且不写入变更；路径未通过媒体根目录安全检查以 `PERMISSION_DENIED` 失败；无效路径自动丢弃并计入 `invalidCount`，无任一有效曲目插入时以 `NOT_FOUND` 失败；路径解析期间播放列表发生变动或队列重建失败以 `OPERATION_FAILED` 失败；在路径解析完成后写入队列前校验 `expectedVersion`
   * @effect write
   */
  insertNext(params: InsertNextParams): InsertNextResult;

  /**
   * Play the queue entry at `index` now, moving it to the front first when it is not already
   * there. An empty queue fails with `NOT_FOUND`, an `index` past the end with `INVALID_INDEX`.
   * When the entry has to be moved and the queue cannot be rebuilt, the call fails with
   * `OPERATION_FAILED` and playback is not started.
   * @zh 立即起播队列中指定索引的条目；若目标不在队首则先行移至队首；队列为空时以 `NOT_FOUND` 失败，索引越界以 `INVALID_INDEX` 失败；条目移动后队列重建失败时以 `OPERATION_FAILED` 失败且不起播
   * @effect write
   */
  playNow(params: PlayNowParams): PlayNowResult;
}

/**
 * Act only while the playlist that the rows belong to still has this `contentVersion`, as its
 * reads report it. A different version changes nothing and fails with `STALE_TARGET`, the current
 * version carried as `details.current`. Omitted, no check is made.
 * @zh 播放列表预期内容版本号；仅在行所属播放列表的版本与此匹配时执行变更，版本冲突时不修改并以 `STALE_TARGET` 失败（`details.current` 返回当前最新版本），省略时不校验
 */
type ExpectedPlaylistVersion = string;

/**
 * One queue entry: the shared track row plus its queue position and, when it has one, the
 * playlist position it was queued from.
 * @zh 待播队列条目信息（包含通用曲目数据、队列序号及来源播放列表位置）
 */
interface QueueItem extends Track {
  /**
   * Position in the queue, from `0`.
   * @zh 在待播队列中的顺序索引（从 `0` 开始）
   */
  queueIndex: Int;
  /**
   * Playlist the entry was queued from; `null` when it carries no playlist position: queued by
   * path, or the position foobar2000 keeps for it no longer holds this track (the row or the
   * playlist was removed, or the rows moved).
   * @zh 条目入队时的来源播放列表索引；通过路径入队或原列表位置已失效时为 `null`
   */
  playlist: Int | null;
  /**
   * GUID of that playlist, as `playlistGuid` takes it; `null` exactly when `playlist` is `null`.
   * @zh 来源播放列表的持久唯一标识；当且仅当 `playlist` 为 `null` 时返回 `null`
   */
  playlistGuid: ReportedPlaylistGuid | null;
  /**
   * Row in that playlist; `null` exactly when `playlist` is `null`.
   * @zh 条目在来源播放列表中的行号索引；当且仅当 `playlist` 为 `null` 时返回 `null`
   */
  playlistItem: Int | null;
}

/**
 * A reference `queue.setContents` accepts: `{ queueIndex }` keeps a slot the queue already
 * holds, `{ playlist, item }` or `{ playlistGuid, item }` adds a playlist row. Exactly one of the
 * forms per entry.
 * @zh `queue.setContents` 接受的队列条目引用形态：通过 `{ queueIndex }` 保留已有条目，或通过 `{ playlist, item }` / `{ playlistGuid, item }` 引入播放列表行
 */
interface QueueContentRef {
  /**
   * Position of an entry already in the queue.
   * @zh 现有待播队列中待保留条目的位置索引
   * @minimum 0
   */
  queueIndex?: Int;
  /**
   * Playlist of the row to add; needs `item` as well.
   * @zh 待引入曲目所在的来源播放列表索引；须与 `item` 同时提供
   * @minimum 0
   */
  playlist?: Int;
  /**
   * The playlist of the row to add by its `guid`, instead of `playlist`; needs `item` as well.
   * @zh 待引入曲目所在播放列表的 `guid`；须与 `item` 同时提供
   */
  playlistGuid?: PlaylistGuid;
  /**
   * Row in that playlist; needs `playlist` or `playlistGuid` as well.
   * @zh 待引入曲目在目标播放列表中的行号索引；须与 `playlist` 或 `playlistGuid` 同时提供
   * @minimum 0
   */
  item?: Int;
  /**
   * Only with a playlist row; given with `queueIndex`, the call fails with `INVALID_PARAMS`.
   * @zh 目标播放列表预期内容版本号；仅在引入播放列表行时有效，与 `queueIndex` 一同传入时以 `INVALID_PARAMS` 失败
   */
  expectedPlaylistVersion?: ExpectedPlaylistVersion;
}

/**
 * A playlist row, addressed by the playlist's index or `guid` and the row.
 * @zh 播放列表行引用（通过播放列表序号或 `guid` 及行索引定位）
 */
interface QueueListRef {
  /**
   * Playlist index; give this or `playlistGuid`.
   * @zh 目标播放列表的索引序号；与 `playlistGuid` 二选一传入
   * @minimum 0
   */
  playlist?: Int;
  /**
   * The playlist's `guid`, instead of `playlist`.
   * @zh 目标播放列表的唯一标识 `guid`，替代易变序号
   */
  playlistGuid?: PlaylistGuid;
  /**
   * Row in that playlist.
   * @zh 目标播放列表中的行号索引
   * @minimum 0
   */
  item: Int;
  expectedPlaylistVersion?: ExpectedPlaylistVersion;
}

interface GetResult {
  /**
   * The entries, in play order.
   * @zh 队列条目列表，按播放先后顺序排列
   */
  items: QueueItem[];
  /**
   * Number of entries.
   * @zh 待播队列当前包含的条目总数
   */
  count: Int;
  /**
   * Pass it as `expectedVersion` so that a queue changed meanwhile fails the call instead of
   * hitting other entries.
   * @zh 队列当前内容版本号；后续位置操作可作为 `expectedVersion` 传入以避免并发冲突
   */
  contentVersion: ContentVersion;
}

interface GetCountResult {
  /**
   * Number of entries.
   * @zh 待播队列当前包含的条目总数
   */
  count: Int;
  /**
   * Whether the queue holds anything; the same as `count > 0`.
   * @zh 待播队列是否非空（等同于 `count > 0`）
   */
  hasItems: boolean;
}

interface AddParams {
  /**
   * Playlist the positions refer to; the active playlist when omitted. An index past the last
   * playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails
   * with `NO_ACTIVE_ITEM`.
   * @zh 索引所引用的来源播放列表；省略时默认操作活动播放列表；越界以 `INVALID_INDEX` 失败，缺省且无活动列表时以 `NO_ACTIVE_ITEM` 失败
   * @minimum 0
   */
  playlist?: Int;
  playlistGuid?: PlaylistGuid;
  /**
   * Rows to queue, in order. Takes precedence over `track`.
   * @zh 待入队的播放列表行号数组，按传入顺序排列；优先级高于 `track`
   * @minimum 0
   */
  tracks?: Int[];
  /**
   * A single row to queue; read only when `tracks` is absent.
   * @zh 待入队的单个播放列表行号；仅在未提供 `tracks` 时生效
   * @minimum 0
   */
  track?: Int;
  expectedPlaylistVersion?: ExpectedPlaylistVersion;
}

interface AddResult {
  /**
   * How many rows were queued.
   * @zh 成功加入待播队列的曲目行数
   */
  addedCount: Int;
  /**
   * Queue length afterwards.
   * @zh 本次操作完成后待播队列的总长度
   */
  queueCount: Int;
}

interface AddPathsParams {
  /**
   * File paths or URLs, each optionally with a `|subsong:N` suffix. Entries longer than 2048
   * characters are dropped and counted in `invalidCount`.
   * @zh 待追加的文件路径或 URL 列表，单项可附加 `|subsong:N` 后缀；超过 2048 字符的条目将被丢弃并计入 `invalidCount`
   * @minItems 1
   * @security MediaRead
   */
  paths: string[];
  /**
   * Append to the dedicated `[WebView Queue]` playlist, creating it when missing. `false` uses
   * `playlist`, or the active playlist when that is omitted too.
   * @zh 是否追加至专用的 `[WebView Queue]` 播放列表（不存在则自动创建）；设为 `false` 时使用 `playlist` 或当前活动列表
   * @default true
   */
  useQueuePlaylist?: boolean;
  /**
   * Target playlist; read only when `useQueuePlaylist` is `false`. An index past the last playlist
   * fails with `INVALID_INDEX`; with this and `playlistGuid` omitted and no active playlist the
   * call fails with `NO_ACTIVE_ITEM`.
   * @zh 目标播放列表索引；仅在 `useQueuePlaylist: false` 时读取；越界以 `INVALID_INDEX` 失败，缺省且无活动列表时以 `NO_ACTIVE_ITEM` 失败
   * @minimum 0
   */
  playlist?: Int;
  /**
   * The target playlist's `guid`, instead of `playlist`; like `playlist` it is read only when
   * `useQueuePlaylist` is `false`, and not checked at all otherwise.
   * @zh 目标播放列表的唯一标识 `guid`；仅在 `useQueuePlaylist: false` 时读取
   */
  playlistGuid?: PlaylistGuid;
}

interface AddPathsResult {
  /**
   * How many tracks were appended and queued.
   * @zh 成功追加并加入待播队列的曲目总数
   */
  addedCount: Int;
  /**
   * Entries of `paths` dropped: empty ones, ones longer than 2048 characters, a `|subsong:N` no
   * track could be made for, and the plain paths when none of them resolved. A plain path that
   * resolves to nothing while another plain path of the call resolves is not counted, so a call
   * that succeeds can have dropped more than this.
   * @zh 丢弃的无效路径数量（包含空路径、超长路径、无对应曲目的 subsong 或全量解析失败项）
   */
  invalidCount: Int;
  /**
   * The playlist the tracks were appended to.
   * @zh 曲目最终追加到的目标播放列表索引
   */
  playlist: Int;
  playlistGuid: ReportedPlaylistGuid;
  /**
   * Queue length afterwards.
   * @zh 本次操作完成后待播队列的总长度
   */
  queueCount: Int;
}

interface RemoveParams {
  /**
   * One queue position to remove. Takes precedence over `indices`.
   * @zh 待移除的单条队列位置索引；优先级高于 `indices`
   * @minimum 0
   */
  index?: Int;
  /**
   * Queue positions to remove; read only when `index` is absent.
   * @zh 待批量移除的队列位置索引数组；仅在未提供 `index` 时读取
   * @minimum 0
   */
  indices?: Int[];
  expectedVersion?: ExpectedContentVersion;
}

interface RemoveResult {
  /**
   * The position removed; only when `index` was given.
   * @zh 成功移除的队列位置索引；仅在传入 `index` 时返回
   */
  removedIndex?: Int;
  /**
   * How many entries were removed; only when `indices` was given.
   * @zh 成功移除的队列条目总数；仅在传入 `indices` 时返回
   */
  removedCount?: Int;
  /**
   * Queue length afterwards.
   * @zh 本次操作完成后待播队列的总长度
   */
  queueCount: Int;
  /**
   * Already includes this change; pass it as `expectedVersion` to the next call.
   * @zh 变更后的队列最新内容版本号；可作为后续调用的 `expectedVersion`
   */
  contentVersion: ContentVersion;
}

interface ClearResult {
  /**
   * How many entries the queue held.
   * @zh 清空前队列中原有的条目总数
   */
  clearedCount: Int;
}

interface FlushResult {
  /**
   * How many entries the queue held.
   * @zh 清空前队列中原有的条目总数
   */
  clearedCount: Int;
}

interface MoveToTopParams {
  /**
   * Position of the entry to promote; `0` is already the front and is refused.
   * @zh 待提升至队首的队列条目位置索引；`0` 已在队首，传入将直接拒绝
   * @minimum 0
   */
  index: Int;
  expectedVersion?: ExpectedContentVersion;
}

interface MoveToTopResult {
  /**
   * The position the entry came from.
   * @zh 被移动条目在调整前的原始位置索引
   */
  movedIndex: Int;
  /**
   * Queue length afterwards.
   * @zh 本次操作完成后待播队列的总长度
   */
  queueCount: Int;
  /**
   * Already includes this change; pass it as `expectedVersion` to the next call.
   * @zh 变更后的队列最新内容版本号；可作为后续调用的 `expectedVersion`
   */
  contentVersion: ContentVersion;
}

interface SetContentsParams {
  /**
   * The new queue, in order. At most `max(256, current queue length)` entries; more fail with `INVALID_PARAMS`.
   * @zh 新的队列条目有序列表；最多支持 `max(256, 当前队列长度)` 项，超出上限以 `INVALID_PARAMS` 失败
   */
  items: QueueContentRef[];
  expectedVersion?: ExpectedContentVersion;
}

interface SetContentsResult {
  /**
   * Queue length afterwards.
   * @zh 本次操作完成后待播队列的总长度
   */
  queueCount: Int;
  /**
   * Already includes this change; pass it as `expectedVersion` to the next call.
   * @zh 变更后的队列最新内容版本号；可作为后续调用的 `expectedVersion`
   */
  contentVersion: ContentVersion;
}

interface InsertNextParams {
  /**
   * File paths or URLs, each optionally with a `|subsong:N` suffix. The entries they produce carry
   * no playlist position.
   * @zh 待插入的文件路径或 URL 列表，可附加 `|subsong:N` 后缀；由此入队的条目不携带播放列表位置信息
   * @security MediaRead
   */
  paths?: string[];
  /**
   * Playlist rows; the entries they produce carry that position, so the playback cursor follows
   * them. At most `max(256, current queue length)` entries; more fail with `INVALID_PARAMS`.
   * @zh 待插入的播放列表行引用列表；由此入队的条目附带列表位置且播放游标会跟随；最多支持 `max(256, 当前队列长度)` 项
   */
  items?: QueueListRef[];
  /**
   * Where to insert, counted after any moved entries are taken out; `0` is the front.
   * @zh 插入的目标队列位置（按移出既有移动条目后的队列计算）；`0` 表示队首
   * @minimum 0
   * @default 0
   */
  position?: Int;
  expectedVersion?: ExpectedContentVersion;
}

interface InsertNextResult {
  /**
   * Tracks newly queued.
   * @zh 全新加入待播队列的曲目总数
   */
  insertedCount: Int;
  /**
   * Entries that were already queued and moved.
   * @zh 既已存在于队列中并前移位置的条目总数
   */
  movedCount: Int;
  /**
   * Queue length afterwards.
   * @zh 本次操作完成后待播队列的总长度
   */
  queueCount: Int;
  /**
   * Input path count minus the tracks resolved from paths, floored at zero. A folder or playlist
   * file resolves to several tracks, so this is not a count of the paths that failed.
   * @zh 输入路径数减去解析得出的曲目数（最小截断至 0）；文件夹展开等多曲目解析不反映为精确失败数
   */
  invalidCount: Int;
  /**
   * Already includes this change; pass it as `expectedVersion` to the next call.
   * @zh 变更后的队列最新内容版本号；可作为后续调用的 `expectedVersion`
   */
  contentVersion: ContentVersion;
}

interface PlayNowParams {
  /**
   * Queue position to play.
   * @zh 待起播的队列条目位置索引
   * @minimum 0
   * @default 0
   */
  index?: Int;
  expectedVersion?: ExpectedContentVersion;
}

interface PlayNowResult {
  /**
   * The position that was played.
   * @zh 实际执行起播的队列条目位置索引
   */
  playedIndex: Int;
  /**
   * Queue length read right after playback started; the host may consume the played entry before
   * or after that read.
   * @zh 起播后立即读取的队列长度；宿主消费已播条目可能发生在读取之前或之后
   */
  queueCount: Int;
  /**
   * Read together with `queueCount`.
   * @zh 与 `queueCount` 同时读取的队列内容版本号
   */
  contentVersion: ContentVersion;
}
