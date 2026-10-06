import type { Int, PlaylistGuid, ReportedPlaylistGuid, Track } from './common.js';

export interface Api {
  /**
   * Read the whole playback queue, in play order. No paging: every entry comes back.
   * @zh 按播放顺序读取整个播放队列。不分页，全部条目一次返回。
   * @effect read
   */
  get(): GetResult;

  /**
   * Report how many entries the queue holds.
   * @zh 报告队列里有多少条目。
   */
  getCount(): GetCountResult;

  /**
   * Queue one or more tracks by their position in a playlist. Positions past the last row are
   * skipped; when none is in range the call fails with `INVALID_INDEX`. A negative position fails
   * with `INVALID_PARAMS`.
   * @zh 按播放列表中的位置把一首或多首曲目加入队列。超出最后一行的位置跳过；一个都不在范围内时以 `INVALID_INDEX` 失败。负数位置以 `INVALID_PARAMS` 失败。
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
   * @zh 按路径把曲目加入队列。入队需要曲目先在某个播放列表里，所以路径会先追加到一个播放列表：默认是专用的 `[WebView Queue]` 列表，不存在则创建。目标播放列表已上锁时以 `LOCKED` 失败，不添加也不入队。路径解析完会重新查找目标列表，处理同 `playlist.addPaths`：期间被挪动，曲目照样加进它，结果里的 `playlist` 是它的新索引；期间被删掉，以 `OPERATION_FAILED` 失败，不入队。有一条路径没通过媒体根检查，整次调用以 `PERMISSION_DENIED` 失败。解析不出曲目的路径被丢弃；一条都解析不出时以 `NOT_FOUND` 失败，失败里带 `invalidCount`。
   * @effect write
   */
  addPaths(params: AddPathsParams): AddPathsResult;

  /**
   * Remove one entry by `index`, or several by `indices`. An empty queue fails with `NOT_FOUND`
   * whatever is given. An `index` past the end fails with `INVALID_INDEX`. In a batch, duplicate
   * indices and those past the end of the queue are skipped; when none is in range the call fails
   * with `INVALID_INDEX`, the failure carrying `removedCount` `0` and `queueCount`. Giving neither
   * key, or a negative index, fails with `INVALID_PARAMS`.
   * @zh 按 `index` 移除一条，或按 `indices` 移除多条。队列为空时不论给了什么都以 `NOT_FOUND` 失败。`index` 超出队尾以 `INVALID_INDEX` 失败。批量时重复的下标与超出队尾的下标跳过；一个都不在范围内时以 `INVALID_INDEX` 失败，失败里带 `removedCount` 为 `0` 与 `queueCount`。两个键都没给或下标为负数，以 `INVALID_PARAMS` 失败。
   * @effect destructive
   */
  remove(params: RemoveParams): RemoveResult;

  /**
   * Empty the queue.
   * @zh 清空队列。
   * @effect destructive
   * @idempotent
   */
  clear(): ClearResult;

  /**
   * Empty the queue; the same operation as `queue.clear` under its older name.
   * @zh 清空队列；与 `queue.clear` 是同一个操作，这是它的旧名字。
   */
  flush(): FlushResult;

  /**
   * Move a queued entry to the front so it plays next. Only queue order changes; the relative
   * order of the other entries is kept. An `index` of `0`, one past the end, and any index while
   * the queue is empty fail with `INVALID_INDEX`. When the queue cannot be rebuilt the call fails
   * with `OPERATION_FAILED`, the failure carrying `queueCount`.
   * @zh 把队列中的一条移到队首，使其下一首播放。只改队列顺序，其余条目的相对顺序不变。`index` 为 `0`、超出队尾，以及队列为空时的任何下标，都以 `INVALID_INDEX` 失败。队列重建不成时以 `OPERATION_FAILED` 失败，失败里带 `queueCount`。
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
   * @zh 用一份有序引用列表替换整个队列。任一条引用不可用，整次调用在写入前失败。空列表即清空队列。播放列表索引按调用到达时的清单解读，所以在列表清单变化之前取来的索引会把另一张列表的行加进队列；`playlistGuid` 始终指向读取时的那张列表。某一条同时给了 `playlist` 与 `playlistGuid`、或 GUID 格式不对，以 `INVALID_PARAMS` 失败；没有列表持有该 GUID 时以 `NOT_FOUND` 失败。
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
   * @zh 插入曲目使其下一首播放，排在已入队的所有条目之前。已在队列中的曲目被移动而不是再入队一次。`items` 块排在 `paths` 块之前，各块内部保持传入顺序。`paths` 与 `items` 都没给时以 `INVALID_PARAMS` 失败。`items` 的每一条都在解析路径之前检查，任一条不合格整次调用失败，什么也不写：一条用 `playlist` 与 `playlistGuid` 两个都没给或都给了、或 GUID 格式不对，以 `INVALID_PARAMS` 失败；没有列表持有该 GUID 时以 `NOT_FOUND` 失败；播放列表序号或行号越界以 `INVALID_INDEX` 失败。有一条路径没通过媒体根检查，整次调用以 `PERMISSION_DENIED` 失败；解析不出曲目的路径被丢弃并计入 `invalidCount`，什么也没剩下可插入时以 `NOT_FOUND` 失败。解析路径期间某个播放列表行变了，以 `OPERATION_FAILED` 失败；队列重建不成同样如此。
   * @effect write
   */
  insertNext(params: InsertNextParams): InsertNextResult;

  /**
   * Play the queue entry at `index` now, moving it to the front first when it is not already
   * there. An empty queue fails with `NOT_FOUND`, an `index` past the end with `INVALID_INDEX`.
   * When the entry has to be moved and the queue cannot be rebuilt, the call fails with
   * `OPERATION_FAILED` and playback is not started.
   * @zh 立即播放队列中 `index` 处的条目；它不在队首时先移到队首。队列为空时以 `NOT_FOUND` 失败，`index` 超出队尾以 `INVALID_INDEX` 失败。需要移动条目而队列重建不成时以 `OPERATION_FAILED` 失败，不起播。
   * @effect write
   */
  playNow(params: PlayNowParams): PlayNowResult;
}

/**
 * One queue entry: the shared track row plus its queue position and, when it has one, the
 * playlist position it was queued from.
 * @zh 一条队列条目：共享曲目行，加上队列位置，以及它入队时所在的播放列表位置（如果有）。
 */
interface QueueItem extends Track {
  /**
   * Position in the queue, from `0`.
   * @zh 在队列中的位置，从 `0` 起。
   */
  queueIndex: Int;
  /**
   * Playlist the entry was queued from; `null` when it carries no playlist position: queued by
   * path, or the position foobar2000 keeps for it no longer holds this track (the row or the
   * playlist was removed, or the rows moved).
   * @zh 条目入队时所在的播放列表；不带播放列表位置时为 `null`：按路径入队，或 foobar2000 为它记的位置上已不是这首曲目（行或列表被删掉，或各行挪动了）。
   */
  playlist: Int | null;
  /**
   * GUID of that playlist, as `playlistGuid` takes it; `null` exactly when `playlist` is `null`.
   * @zh 该播放列表的 GUID，写法与 `playlistGuid` 接受的一致；恰在 `playlist` 为 `null` 时为 `null`。
   */
  playlistGuid: ReportedPlaylistGuid | null;
  /**
   * Row in that playlist; `null` exactly when `playlist` is `null`.
   * @zh 在该播放列表中的行号；恰在 `playlist` 为 `null` 时为 `null`。
   */
  playlistItem: Int | null;
}

/**
 * A reference `queue.setContents` accepts: `{ queueIndex }` keeps a slot the queue already
 * holds, `{ playlist, item }` or `{ playlistGuid, item }` adds a playlist row. Exactly one of the
 * forms per entry.
 * @zh `queue.setContents` 接受的引用：`{ queueIndex }` 保留队列里已有的槽位，`{ playlist, item }` 或 `{ playlistGuid, item }` 加入一个播放列表行。每条只能是其中一种形态。
 */
interface QueueContentRef {
  /**
   * Position of an entry already in the queue.
   * @zh 队列中已有条目的位置。
   * @minimum 0
   */
  queueIndex?: Int;
  /**
   * Playlist of the row to add; needs `item` as well.
   * @zh 要加入的行所在的播放列表；须与 `item` 同时给出。
   * @minimum 0
   */
  playlist?: Int;
  /**
   * The playlist of the row to add by its `guid`, instead of `playlist`; needs `item` as well.
   * @zh 用 `guid` 指定要加入的行所在的播放列表，代替 `playlist`；须与 `item` 同时给出。
   */
  playlistGuid?: PlaylistGuid;
  /**
   * Row in that playlist; needs `playlist` or `playlistGuid` as well.
   * @zh 该播放列表中的行号；须与 `playlist` 或 `playlistGuid` 同时给出。
   * @minimum 0
   */
  item?: Int;
}

/**
 * A playlist row, addressed by the playlist's index or `guid` and the row.
 * @zh 一个播放列表行，按播放列表的索引或 `guid` 加行号寻址。
 */
interface QueueListRef {
  /**
   * Playlist index; give this or `playlistGuid`.
   * @zh 播放列表索引；与 `playlistGuid` 给一个。
   * @minimum 0
   */
  playlist?: Int;
  /**
   * The playlist's `guid`, instead of `playlist`.
   * @zh 播放列表的 `guid`，代替 `playlist`。
   */
  playlistGuid?: PlaylistGuid;
  /**
   * Row in that playlist.
   * @zh 该播放列表中的行号。
   * @minimum 0
   */
  item: Int;
}

interface GetResult {
  /**
   * The entries, in play order.
   * @zh 条目，按播放顺序。
   */
  items: QueueItem[];
  /**
   * Number of entries.
   * @zh 条目数。
   */
  count: Int;
}

interface GetCountResult {
  /**
   * Number of entries.
   * @zh 条目数。
   */
  count: Int;
  /**
   * Whether the queue holds anything; the same as `count > 0`.
   * @zh 队列是否非空；等于 `count > 0`。
   */
  hasItems: boolean;
}

interface AddParams {
  /**
   * Playlist the positions refer to; the active playlist when omitted. An index past the last
   * playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails
   * with `NO_ACTIVE_ITEM`.
   * @zh 位置所指的播放列表；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。
   * @minimum 0
   */
  playlist?: Int;
  playlistGuid?: PlaylistGuid;
  /**
   * Rows to queue, in order. Takes precedence over `track`.
   * @zh 要入队的行，按顺序。优先于 `track`。
   * @minimum 0
   */
  tracks?: Int[];
  /**
   * A single row to queue; read only when `tracks` is absent.
   * @zh 要入队的单个行；只在没传 `tracks` 时读取。
   * @minimum 0
   */
  track?: Int;
}

interface AddResult {
  /**
   * How many rows were queued.
   * @zh 入队的行数。
   */
  addedCount: Int;
  /**
   * Queue length afterwards.
   * @zh 之后的队列长度。
   */
  queueCount: Int;
}

interface AddPathsParams {
  /**
   * File paths or URLs, each optionally with a `|subsong:N` suffix. Entries longer than 2048
   * characters are dropped and counted in `invalidCount`.
   * @zh 文件路径或 URL，每条可带 `|subsong:N` 后缀。超过 2048 字符的条目丢弃并计入 `invalidCount`。
   * @minItems 1
   * @security MediaRead
   */
  paths: string[];
  /**
   * Append to the dedicated `[WebView Queue]` playlist, creating it when missing. `false` uses
   * `playlist`, or the active playlist when that is omitted too.
   * @zh 追加到专用的 `[WebView Queue]` 播放列表，不存在则创建。`false` 时用 `playlist`，它也省略时用活动播放列表。
   * @default true
   */
  useQueuePlaylist?: boolean;
  /**
   * Target playlist; read only when `useQueuePlaylist` is `false`. An index past the last playlist
   * fails with `INVALID_INDEX`; with this and `playlistGuid` omitted and no active playlist the
   * call fails with `NO_ACTIVE_ITEM`.
   * @zh 目标播放列表；只在 `useQueuePlaylist` 为 `false` 时读取。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；它与 `playlistGuid` 都省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。
   * @minimum 0
   */
  playlist?: Int;
  /**
   * The target playlist's `guid`, instead of `playlist`; like `playlist` it is read only when
   * `useQueuePlaylist` is `false`, and not checked at all otherwise.
   * @zh 目标播放列表的 `guid`，用来代替 `playlist`；和 `playlist` 一样只在 `useQueuePlaylist` 为 `false` 时读取，否则完全不检查。
   */
  playlistGuid?: PlaylistGuid;
}

interface AddPathsResult {
  /**
   * How many tracks were appended and queued.
   * @zh 追加并入队的曲目数。
   */
  addedCount: Int;
  /**
   * Entries of `paths` dropped: empty ones, ones longer than 2048 characters, a `|subsong:N` no
   * track could be made for, and the plain paths when none of them resolved. A plain path that
   * resolves to nothing while another plain path of the call resolves is not counted, so a call
   * that succeeds can have dropped more than this.
   * @zh 被丢弃的 `paths` 条数：空的、超过 2048 字符的、建不出曲目的 `|subsong:N`，以及一条都没解析出来时的全部普通路径。同一次调用里别的普通路径解析出了曲目时，解析不出的普通路径不计入，所以成功的调用实际丢弃的可能比这个数多。
   */
  invalidCount: Int;
  /**
   * The playlist the tracks were appended to.
   * @zh 曲目被追加到的播放列表。
   */
  playlist: Int;
  playlistGuid: ReportedPlaylistGuid;
  /**
   * Queue length afterwards.
   * @zh 之后的队列长度。
   */
  queueCount: Int;
}

interface RemoveParams {
  /**
   * One queue position to remove. Takes precedence over `indices`.
   * @zh 要移除的一个队列位置。优先于 `indices`。
   * @minimum 0
   */
  index?: Int;
  /**
   * Queue positions to remove; read only when `index` is absent.
   * @zh 要移除的队列位置；只在没传 `index` 时读取。
   * @minimum 0
   */
  indices?: Int[];
}

interface RemoveResult {
  /**
   * The position removed; only when `index` was given.
   * @zh 被移除的位置；只在传了 `index` 时返回。
   */
  removedIndex?: Int;
  /**
   * How many entries were removed; only when `indices` was given.
   * @zh 移除的条数；只在传了 `indices` 时返回。
   */
  removedCount?: Int;
  /**
   * Queue length afterwards.
   * @zh 之后的队列长度。
   */
  queueCount: Int;
}

interface ClearResult {
  /**
   * How many entries the queue held.
   * @zh 队列原先的条目数。
   */
  clearedCount: Int;
}

interface FlushResult {
  /**
   * How many entries the queue held.
   * @zh 队列原先的条目数。
   */
  clearedCount: Int;
}

interface MoveToTopParams {
  /**
   * Position of the entry to promote; `0` is already the front and is refused.
   * @zh 要提到队首的条目位置；`0` 已经在队首，会被拒绝。
   * @minimum 0
   */
  index: Int;
}

interface MoveToTopResult {
  /**
   * The position the entry came from.
   * @zh 条目原来的位置。
   */
  movedIndex: Int;
  /**
   * Queue length afterwards.
   * @zh 之后的队列长度。
   */
  queueCount: Int;
}

interface SetContentsParams {
  /**
   * The new queue, in order. At most `max(256, current queue length)` entries; more fail with `INVALID_PARAMS`.
   * @zh 新的队列内容，按顺序。最多 `max(256, 当前队列长度)` 条，多了以 `INVALID_PARAMS` 失败。
   */
  items: QueueContentRef[];
}

interface SetContentsResult {
  /**
   * Queue length afterwards.
   * @zh 之后的队列长度。
   */
  queueCount: Int;
}

interface InsertNextParams {
  /**
   * File paths or URLs, each optionally with a `|subsong:N` suffix. The entries they produce carry
   * no playlist position.
   * @zh 文件路径或 URL，每条可带 `|subsong:N` 后缀。由此入队的条目不带播放列表位置。
   * @security MediaRead
   */
  paths?: string[];
  /**
   * Playlist rows; the entries they produce carry that position, so the playback cursor follows
   * them. At most `max(256, current queue length)` entries; more fail with `INVALID_PARAMS`.
   * @zh 播放列表行；由此入队的条目带该位置，播放游标会跟随。最多 `max(256, 当前队列长度)` 条，多了以 `INVALID_PARAMS` 失败。
   */
  items?: QueueListRef[];
  /**
   * Where to insert, counted after any moved entries are taken out; `0` is the front.
   * @zh 插入位置，按移出被移动条目之后的队列计算；`0` 是队首。
   * @minimum 0
   * @default 0
   */
  position?: Int;
}

interface InsertNextResult {
  /**
   * Tracks newly queued.
   * @zh 新入队的曲目数。
   */
  insertedCount: Int;
  /**
   * Entries that were already queued and moved.
   * @zh 已在队列中、被移动的条目数。
   */
  movedCount: Int;
  /**
   * Queue length afterwards.
   * @zh 之后的队列长度。
   */
  queueCount: Int;
  /**
   * Input path count minus the tracks resolved from paths, floored at zero. A folder or playlist
   * file resolves to several tracks, so this is not a count of the paths that failed.
   * @zh 输入路径数减去由路径解析出的曲目数，最低为零。文件夹或播放列表文件会解析出多首曲目，所以它不是失败路径的条数。
   */
  invalidCount: Int;
}

interface PlayNowParams {
  /**
   * Queue position to play.
   * @zh 要播放的队列位置。
   * @minimum 0
   * @default 0
   */
  index?: Int;
}

interface PlayNowResult {
  /**
   * The position that was played.
   * @zh 被播放的位置。
   */
  playedIndex: Int;
  /**
   * Queue length read right after playback started; the host may consume the played entry before
   * or after that read.
   * @zh 播放启动后立即读到的队列长度；宿主消费被播放条目可能在这次读取之前或之后。
   */
  queueCount: Int;
}
