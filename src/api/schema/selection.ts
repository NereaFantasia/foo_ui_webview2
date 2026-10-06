import type { Int, ReportedPlaylistGuid, Track } from './common.js';

export interface Api {
  /**
   * Report which source the user's Selection Viewer preference favours, derived from the live
   * selection type rather than read from a stored setting.
   * @zh 报告用户的 Selection Viewer 偏好倾向哪个来源；由当前的选择类型推导，不是读取某项设置。
   */
  getViewerMode(): GetViewerModeResult;

  /**
   * Resolve the track a viewer should show: the preferred source first, the other one when it has
   * no track. Always succeeds; `found` says whether there was anything to show.
   * @zh 解析查看器应当显示的曲目：先按偏好来源，没有曲目时回退到另一来源。恒成功，有没有东西可显示看 `found`。
   */
  getViewingTrack(params: GetViewingTrackParams): GetViewingTrackResult;

  /**
   * Read the current global selection as a page of handles. Observes state only; nothing is
   * modified.
   * @zh 按页读取当前的全局选择（handle 列表）。只读状态，不做修改。
   */
  get(params: GetParams): GetResult;

  /**
   * Report the type of the current global selection, as a numeric index and as a name.
   * @zh 报告当前全局选择的类型，同时给出数字序号与名称。
   */
  getType(): GetTypeResult;

  /**
   * Replace the global selection with the given tracks. Handles are minted from the strings, not
   * looked up, so a path that does not exist is selected just the same. The host holds its
   * selection holder only for the duration of the call; foobar2000 clears the selection once no
   * holder is left, so it outlives the call only while another one is held, such as the calling
   * panel's own while that panel has focus with `grabFocus` on.
   * @zh 用给定曲目替换全局选择。handle 按字符串铸出、不做查找，不存在的路径同样会被选中。宿主只在调用期间持有选择持有者；foobar2000 在没有任何持有者时清空选择，所以选择要在调用结束后留下，得有别的持有者在，比如开着 `grabFocus` 且有焦点的调用方面板自己持有的那个。
   */
  set(params: SetParams): SetResult;

  /**
   * Set the selection to the whole active playlist, or to that playlist's own selected rows.
   * foobar2000 ends tracking when the holder that started it is released, and the host releases
   * it when the call returns, so later playlist changes are not followed; how long the selection
   * itself lasts is as for `set`.
   * @zh 把选择设为整个活动播放列表，或该列表自己的选中行。foobar2000 在发起跟踪的持有者被释放时结束跟踪，而宿主在调用返回时就释放它，所以之后播放列表的变化不会跟进；选择本身能留多久同 `set`。
   */
  setPlaylistTracking(params: SetPlaylistTrackingParams): SetPlaylistTrackingResult;
}

export interface Events {
  /**
   * foobar2000's global selection changed. A change less than 50 ms after the previous event is
   * dropped, not delayed, so the last of several quick changes may go unannounced; read
   * `selection.get` when the current value matters.
   * @zh foobar2000 的全局选择变了。距上一个事件不到 50 ms 的变化直接丢弃而不是推迟，所以连续快速变化中的最后一次可能不发；需要当前值时读 `selection.get`。
   * @delivery broadcast
   */
  changed: ChangedPayload;
}

/**
 * Which source the viewer prefers.
 * @zh 查看器偏好的来源。
 */
type ViewerMode = 'prefer_playing' | 'prefer_selection';

/**
 * Where the current selection comes from, named after the SDK's selection callers.
 * @zh 当前选择的来源，按 SDK 的 selection caller 命名。
 */
type SelectionType =
  | 'now_playing'
  | 'active_playlist_selection'
  | 'active_playlist'
  | 'playlist_manager'
  | 'media_library_viewer'
  | 'unknown';

interface GetViewerModeResult {
  /**
   * `prefer_playing` when the selection type is the playing track, else `prefer_selection`.
   * @zh 选择类型是正在播放的曲目时为 `prefer_playing`，否则为 `prefer_selection`。
   */
  mode: ViewerMode;
}

interface GetViewingTrackParams {
  /**
   * Also return the full track row as `track`.
   * @zh 是否把完整的曲目行作为 `track` 一并返回。
   * @default false
   */
  includeTrackInfo?: boolean;
}

interface GetViewingTrackResult {
  /**
   * Whether either source had a track.
   * @zh 两个来源里是否有曲目。
   */
  found: boolean;
  /**
   * The preference the resolution used.
   * @zh 本次解析依据的偏好。
   */
  mode: ViewerMode;
  /**
   * Which source supplied the track; absent when `found` is false.
   * @zh 曲目来自哪个来源；`found` 为 false 时缺省。
   */
  source?: 'now_playing' | 'selection';
  /**
   * The track's handle (native path, `|subsong:N` appended when the subsong is not `0`); absent
   * when `found` is false.
   * @zh 曲目的 handle（原生路径，子曲目不为 `0` 时附加 `|subsong:N`）；`found` 为 false 时缺省。
   */
  handle?: string;
  /**
   * Playlist holding the track; absent when no playlist row is known for it. For the playing
   * track this is where playback took it from; for the selection only the active playlist is
   * looked through, so a selection made elsewhere (the Media Library, another playlist) has no
   * row. Present exactly when `itemIndex` is.
   * @zh 曲目所在的播放列表；不知道它在哪一行时缺省。正在播放的曲目取播放时的来源位置；选中的曲目只在活动播放列表里找，所以在别处（媒体库、另一张列表）做的选择没有行。恰在 `itemIndex` 出现时出现。
   */
  playlistIndex?: Int;
  /**
   * GUID of that playlist, as `playlistGuid` takes it; present exactly when `playlistIndex` is.
   * @zh 该播放列表的 GUID，写法与 `playlistGuid` 接受的一致；恰在 `playlistIndex` 出现时出现。
   */
  playlistGuid?: ReportedPlaylistGuid;
  /**
   * Row of the track in that playlist, the first one holding it; absent when `playlistIndex` is.
   * @zh 曲目在该播放列表中的行号，取第一处；`playlistIndex` 缺省时缺省。
   */
  itemIndex?: Int;
  /**
   * The full track row; only with `includeTrackInfo`.
   * @zh 完整的曲目行；只在 `includeTrackInfo` 时返回。
   */
  track?: Track;
}

interface GetParams {
  /**
   * Index of the first handle to return.
   * @zh 返回的第一个 handle 的下标。
   * @minimum 0
   * @default 0
   */
  offset?: Int;
  /**
   * Maximum number of handles to return; `0` means every handle from `offset` on. When omitted
   * the page is capped at 100 and `truncated` reports whether the cap applied.
   * @zh 最多返回的 handle 数；`0` 表示从 `offset` 起的全部。省略时按 100 封顶，`truncated` 报告上限是否生效。
   * @minimum 0
   */
  limit?: Int;
}

interface GetResult {
  /**
   * Size of the whole selection, not of this page.
   * @zh 整个选择的条数，不是本页的条数。
   */
  count: Int;
  /**
   * Where the selection comes from.
   * @zh 选择的来源。
   */
  type: SelectionType;
  /**
   * Handles of this page: native paths with `|subsong:N` appended when the subsong is not `0`.
   * @zh 本页的 handle：原生路径，子曲目不为 `0` 时附加 `|subsong:N`。
   */
  handles: string[];
  /**
   * The `offset` that applied.
   * @zh 实际生效的 `offset`。
   */
  offset: Int;
  /**
   * Whether entries remain after this page.
   * @zh 本页之后是否还有条目。
   */
  hasMore: boolean;
  /**
   * Present, and true, only when `limit` was omitted and the selection exceeds 100.
   * @zh 只在省略 `limit` 且选择超过 100 条时出现，且为 true。
   */
  truncated?: boolean;
}

interface GetTypeResult {
  /**
   * Numeric index of the type: `0` now_playing, `1` active_playlist_selection, `2`
   * active_playlist, `3` playlist_manager, `5` media_library_viewer, `0` when unknown.
   * @zh 类型的数字序号：`0` now_playing、`1` active_playlist_selection、`2` active_playlist、`3` playlist_manager、`5` media_library_viewer，未知时为 `0`。
   */
  type: Int;
  /**
   * Name of the type.
   * @zh 类型名。
   */
  typeName: SelectionType;
}

interface SetParams {
  /**
   * Tracks to select, as native paths with an optional `|subsong:N` suffix. A suffix that is not a
   * number is read as subsong `0`.
   * @zh 要选中的曲目，原生路径加可选的 `|subsong:N` 后缀。后缀不是数字时按子曲目 `0` 处理。
   * @minItems 1
   */
  handles: string[];
}

interface SetResult {
  /**
   * How many tracks were handed to the selection holder, i.e. the handles that resolved. Not a
   * read-back: see `set` for how long the selection lasts.
   * @zh 交给选择持有者的曲目数，即能铸出 handle 的条数。不是回读的结果，选择能留多久见 `set`。
   */
  count: Int;
}

interface SetPlaylistTrackingParams {
  /**
   * `playlist` takes the whole active playlist; `selection` takes its selected rows.
   * @zh `playlist` 取整个活动播放列表；`selection` 取其中的选中行。
   * @default "selection"
   */
  mode?: 'selection' | 'playlist';
}

interface SetPlaylistTrackingResult {
  /**
   * The mode that applied.
   * @zh 实际生效的模式。
   */
  mode: 'selection' | 'playlist';
}

interface ChangedPayload {
  /**
   * Number of selected tracks.
   * @zh 选中的曲目数。
   */
  count: Int;
  /**
   * Where the selection comes from, as `typeName` in the `selection.getType` answer.
   * @zh 选择的来源，同 `selection.getType` 应答里的 `typeName`。
   */
  type: SelectionType;
  /**
   * Handles of the first 100 selected tracks, in selection order: native paths with
   * `|subsong:N` appended when the subsong is not `0`.
   * @zh 前 100 首选中曲目的 handle，按选择顺序：原生路径，子曲目不为 `0` 时附加 `|subsong:N`。
   */
  handles: string[];
  /**
   * Whether more than 100 tracks are selected, so that `handles` lists only the first 100.
   * @zh 是否选中了超过 100 首，因而 `handles` 只列出前 100 首。
   */
  truncated: boolean;
  /**
   * The selected track; present only when exactly one track is selected.
   * @zh 选中的那首曲目；只在恰好选中一首时出现。
   */
  track?: Track;
  /**
   * The track loaded for playback, playing or paused; absent when none is.
   * @zh 已载入播放的曲目（正在播放或暂停）；没有时缺省。
   */
  nowPlaying?: Track;
}
