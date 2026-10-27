import type { Int, ReportedPlaylistGuid, Track } from './common.js';

export interface Api {
  /**
   * Report which source the user's Selection Viewer preference favours, derived from the live
   * selection type rather than read from a stored setting.
   * @zh 获取当前 Selection Viewer 优先展示的来源模式（根据当前选区类型实时推导，而非读取持久化配置）
   */
  getViewerMode(): GetViewerModeResult;

  /**
   * Resolve the track a viewer should show: the preferred source first, the other one when it has
   * no track. Always succeeds; `found` says whether there was anything to show.
   * @zh 解析查看器当前应当展示的目标曲目（优先读取偏好来源，无曲目时自动回退至备用来源；调用恒返回成功，通过 found 标识是否存在可展示曲目）
   */
  getViewingTrack(params: GetViewingTrackParams): GetViewingTrackResult;

  /**
   * Read the current global selection as a page of handles. Observes state only; nothing is
   * modified.
   * @zh 分页获取当前全局选中的曲目句柄列表（纯只读查询，不修改选区状态）
   */
  get(params: GetParams): GetResult;

  /**
   * Report the type of the current global selection, as a numeric index and as a name.
   * @zh 获取当前全局选区的来源类型（同时返回数字索引与类型名称）
   */
  getType(): GetTypeResult;

  /**
   * Replace the global selection with the given tracks. Handles are minted from the strings, not
   * looked up, so a path that does not exist is selected just the same. The host holds its
   * selection holder only for the duration of the call; foobar2000 clears the selection once no
   * holder is left, so it outlives the call only while another one is held, such as the calling
   * panel's own while that panel has focus with `grabFocus` on.
   * @zh 将全局选区替换为指定曲目列表（曲目句柄直接依据路径字符串构造而不执行存在性检查，未存在的路径也会被加入选区；宿主仅在调用期间持有选区持有者，foobar2000 在所有持有者释放后会自动清空全局选区，若需在调用结束后维持选区，须由其他持有者继续保持，例如处于聚焦且开启 grabFocus 的调用方面板所持有的实例）
   */
  set(params: SetParams): SetResult;

  /**
   * Set the selection to the whole active playlist, or to that playlist's own selected rows.
   * foobar2000 ends tracking when the holder that started it is released, and the host releases
   * it when the call returns, so later playlist changes are not followed; how long the selection
   * itself lasts is as for `set`.
   * @zh 将全局选区同步为当前活动播放列表的完整曲目或其选中项（foobar2000 在初始持有者释放时自动终止跟踪，宿主在调用返回时即释放该持有者，后续播放列表状态变更将不再跟随同步；选区有效生命周期机制与 set 一致）
   */
  setPlaylistTracking(params: SetPlaylistTrackingParams): SetPlaylistTrackingResult;
}

export interface Events {
  /**
   * foobar2000's global selection changed. A change less than 50 ms after the previous event is
   * dropped, not delayed, so the last of several quick changes may go unannounced; read
   * `selection.get` when the current value matters.
   * @zh foobar2000 全局选区状态变更时广播（两次变更间隔低于 50 ms 时直接丢弃而非延迟派发，连续快速变更中最后一次可能不会触发事件；若需获取最新确切状态请主动调用 selection.get 查询）
   * @delivery broadcast
   */
  changed: ChangedPayload;
}

/**
 * Which source the viewer prefers.
 * @zh 查看器首选的数据来源模式
 */
type ViewerMode = 'prefer_playing' | 'prefer_selection';

/**
 * Where the current selection comes from, named after the SDK's selection callers.
 * @zh 当前全局选区的来源上下文（与 foobar2000 SDK 中的 selection caller 命名对应）
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
   * @zh 选区来源为正在播放的曲目时返回 prefer_playing，其余情况返回 prefer_selection
   */
  mode: ViewerMode;
}

interface GetViewingTrackParams {
  /**
   * Also return the full track row as `track`.
   * @zh 是否在 track 字段中一并返回完整的曲目元数据行
   * @default false
   */
  includeTrackInfo?: boolean;
}

interface GetViewingTrackResult {
  /**
   * Whether either source had a track.
   * @zh 任一候选来源中是否成功解析到可用曲目
   */
  found: boolean;
  /**
   * The preference the resolution used.
   * @zh 本次解析所采用的来源偏好模式
   */
  mode: ViewerMode;
  /**
   * Which source supplied the track; absent when `found` is false.
   * @zh 实际提供该曲目的数据来源（found 为 false 时缺省）
   */
  source?: 'now_playing' | 'selection';
  /**
   * The track's handle (native path, `|subsong:N` appended when the subsong is not `0`); absent
   * when `found` is false.
   * @zh 曲目句柄（原生文件路径，子曲目序号大于 0 时附加 |subsong:N；found 为 false 时缺省）
   */
  handle?: string;
  /**
   * Playlist holding the track; absent when no playlist row is known for it. For the playing
   * track this is where playback took it from; for the selection only the active playlist is
   * looked through, so a selection made elsewhere (the Media Library, another playlist) has no
   * row. Present exactly when `itemIndex` is.
   * @zh 曲目所在的播放列表从零开始的索引（若无法定位行号则缺省；正在播放的曲目取自启动播放时的来源位置，选区曲目仅检索当前活动播放列表，媒体库或非活动列表产生的选区无行号映射；与 itemIndex 同步存在）
   */
  playlistIndex?: Int;
  /**
   * GUID of that playlist, as `playlistGuid` takes it; present exactly when `playlistIndex` is.
   * @zh 目标播放列表的 GUID（格式与 playlistGuid 接收格式一致；与 playlistIndex 同步存在）
   */
  playlistGuid?: ReportedPlaylistGuid;
  /**
   * Row of the track in that playlist, the first one holding it; absent when `playlistIndex` is.
   * @zh 曲目在目标播放列表中首次出现的行号索引（playlistIndex 缺省时此字段亦缺省）
   */
  itemIndex?: Int;
  /**
   * The full track row; only with `includeTrackInfo`.
   * @zh 完整的曲目元数据行（仅在 includeTrackInfo 为 true 时返回）
   */
  track?: Track;
}

interface GetParams {
  /**
   * Index of the first handle to return.
   * @zh 首个返回句柄在选区中的起始偏移索引（从 0 开始）
   * @minimum 0
   * @default 0
   */
  offset?: Int;
  /**
   * Maximum number of handles to return; `0` means every handle from `offset` on. When omitted
   * the page is capped at 100 and `truncated` reports whether the cap applied.
   * @zh 最大返回句柄数量（传入 0 表示返回从 offset 开始的全部句柄；缺省时默认限制返回前 100 条并通过 truncated 指示是否触发自动截断）
   * @minimum 0
   */
  limit?: Int;
}

interface GetResult {
  /**
   * Size of the whole selection, not of this page.
   * @zh 当前全局选区的曲目总数（非单页返回数量）
   */
  count: Int;
  /**
   * Where the selection comes from.
   * @zh 全局选区的数据来源类型
   */
  type: SelectionType;
  /**
   * Handles of this page: native paths with `|subsong:N` appended when the subsong is not `0`.
   * @zh 当前分页包含的曲目句柄列表（原生文件路径，子曲目序号大于 0 时附加 |subsong:N）
   */
  handles: string[];
  /**
   * The `offset` that applied.
   * @zh 实际生效的起始偏移量
   */
  offset: Int;
  /**
   * Whether entries remain after this page.
   * @zh 当前页之后是否仍有后续选区条目
   */
  hasMore: boolean;
  /**
   * Present, and true, only when `limit` was omitted and the selection exceeds 100.
   * @zh 是否触发了未指定 limit 时的 100 条自动截断限制（仅在省略 limit 且总选区超过 100 条时存在且固定为 true）
   */
  truncated?: boolean;
}

interface GetTypeResult {
  /**
   * Numeric index of the type: `0` now_playing, `1` active_playlist_selection, `2`
   * active_playlist, `3` playlist_manager, `5` media_library_viewer, `0` when unknown.
   * @zh 选区来源类型的数值索引（0 now_playing、1 active_playlist_selection、2 active_playlist、3 playlist_manager、5 media_library_viewer；未知类型统一回退为 0）
   */
  type: Int;
  /**
   * Name of the type.
   * @zh 选区来源类型字符串标识
   */
  typeName: SelectionType;
}

interface SetParams {
  /**
   * Tracks to select, as native paths with an optional `|subsong:N` suffix. A suffix that is not a
   * number is read as subsong `0`.
   * @zh 待设置到选区的曲目路径列表（原生文件路径，可带 |subsong:N 后缀；若后缀非有效数字则回退为子曲目 0）
   * @minItems 1
   */
  handles: string[];
}

interface SetResult {
  /**
   * How many tracks were handed to the selection holder, i.e. the handles that resolved. Not a
   * read-back: see `set` for how long the selection lasts.
   * @zh 成功提交至选区持有者的有效曲目句柄总数（该数值为实际解析提交的条数而非重读校验结果；选区维持生命周期见 set 方法说明）
   */
  count: Int;
}

interface SetPlaylistTrackingParams {
  /**
   * `playlist` takes the whole active playlist; `selection` takes its selected rows.
   * @zh 播放列表跟踪模式（playlist 跟踪整个当前活动列表；selection 跟踪列表中当前选中的行项）
   * @default "selection"
   */
  mode?: 'selection' | 'playlist';
}

interface SetPlaylistTrackingResult {
  /**
   * The mode that applied.
   * @zh 实际生效的播放列表跟踪模式
   */
  mode: 'selection' | 'playlist';
}

interface ChangedPayload {
  /**
   * Number of selected tracks.
   * @zh 当前选中的曲目总数
   */
  count: Int;
  /**
   * Where the selection comes from, as `typeName` in the `selection.getType` answer.
   * @zh 选区来源类型（取值与 selection.getType 返回的 typeName 一致）
   */
  type: SelectionType;
  /**
   * Handles of the first 100 selected tracks, in selection order: native paths with
   * `|subsong:N` appended when the subsong is not `0`.
   * @zh 前 100 首选中曲目的句柄列表（保持选区原始顺序；原生文件路径，子曲目序号大于 0 时附加 |subsong:N）
   */
  handles: string[];
  /**
   * Whether more than 100 tracks are selected, so that `handles` lists only the first 100.
   * @zh 选中曲目总数是否超过 100 条（为 true 时 handles 仅截断保留前 100 首）
   */
  truncated: boolean;
  /**
   * The selected track; present only when exactly one track is selected.
   * @zh 当前选中的单首曲目元数据（仅在全局选区恰好包含唯一一首曲目时提供）
   */
  track?: Track;
  /**
   * The track loaded for playback, playing or paused; absent when none is.
   * @zh 当前已载入播放核心的曲目元数据（包含正在播放或处于暂停状态；未处于播放相关状态时缺省）
   */
  nowPlaying?: Track;
}
