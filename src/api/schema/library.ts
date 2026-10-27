import type { Int, PlaylistGuid, Track } from './common.js';

export interface Api {
  /**
   * Whether the foobar2000 media library is enabled, that is, whether any library folder is
   * configured.
   * @zh 查询 foobar2000 媒体库是否已启用（即是否配置了媒体库文件夹）
   */
  isEnabled(): IsEnabledResult;

  /**
   * Aggregate counts over the whole library. With the library disabled every count is `0`. The
   * library is walked on every call.
   * @zh 获取全媒体库汇总统计计数；每次调用均实时遍历全库，媒体库未启用时各项计数均为 `0`
   * @effect read
   */
  getStats(): GetStatsResult;

  /**
   * Drop the host's cached library results and the directory tree index; the next call to an
   * endpoint that uses them rebuilds them. They are also dropped whenever the library changes.
   * @zh 丢弃宿主缓存的媒体库查询结果与目录树索引；后续调用对应端点时按需重建，媒体库内容变更时亦会自动清空
   */
  invalidateCache(): InvalidateCacheResult;

  /**
   * Every genre in the library with its track count, ordered by name. Every value of a
   * multi-value `genre` gets its own entry, and a track tagged with several genres is counted
   * under each. Fails with `LIBRARY_DISABLED` when the library is disabled.
   * @zh 按名称升序列出全媒体库所有流派及其曲目数；多值 `genre` 的每个取值单独成项，包含多个流派的曲目重复计入各所属流派；媒体库未启用时以 `LIBRARY_DISABLED` 失败
   */
  getGenres(): GetGenresResult;

  /**
   * Ask foobar2000 to rescan the library folders by calling `library_manager::rescan()`, which
   * the foobar2000 SDK marks as obsolete and not to be called. The library follows file changes
   * on its own.
   * @zh 调用 `library_manager::rescan()` 请求 foobar2000 重新扫描媒体库文件夹（foobar2000 SDK 已将该接口标为废弃且不建议调用，媒体库本身会自动监视跟踪文件变更）
   */
  rescan(): void;

  /**
   * Same as `library.rescan`.
   * @zh 同 `library.rescan`
   */
  refresh(): void;

  /**
   * Append tracks to a playlist by path. A path does not have to be in the library, and a file
   * that does not exist is added all the same. A locked playlist fails with `LOCKED` and nothing
   * is added.
   * @zh 按文件路径将曲目追加至指定播放列表；路径无需存在于媒体库中，不存在的文件亦照常添加；目标播放列表已锁定时以 `LOCKED` 失败且不添加任何曲目
   */
  addToPlaylist(params: AddToPlaylistParams): AddToPlaylistResult;

  /**
   * The albums an artist appears on. `trackCount`, `duration` and `discCount` count only the
   * tracks of this artist, so an album the artist appears on only partly reports smaller figures
   * than in `library.getAlbums`. Rows are grouped by album name alone; tracks without an `album`
   * tag fall under `(Unknown Album)`. Fails with `LIBRARY_DISABLED` when the library is disabled.
   * @zh 获取指定艺术家参与的所有专辑；`trackCount`、`duration` 与 `discCount` 仅统计该艺术家所属曲目（部分参与的专辑统计值会小于 `library.getAlbums`）；返回行仅按专辑名归组，缺失 `album` 标签的曲目归入 `(Unknown Album)`；媒体库未启用时以 `LIBRARY_DISABLED` 失败
   */
  getArtistAlbums(params: GetArtistAlbumsParams): GetArtistAlbumsResult;

  /**
   * Every distinct value of one tag across the library with its track count, most used first.
   * Fails with `LIBRARY_DISABLED` when the library is disabled.
   * @zh 获取指定标签在全媒体库中的所有不同取值及其曲目数，按出现频次降序排列；媒体库未启用时以 `LIBRARY_DISABLED` 失败
   */
  getFieldValues(params: GetFieldValuesParams): GetFieldValuesResult;

  /**
   * The library's root folders. Only tracks that resolve to a stable local path count toward
   * the roots; `http://`, `file-relative://`, `unpack://`, `archive://` and similar ones are
   * counted in `skippedTracks`. The first call builds the index synchronously and later calls
   * reuse it until the library changes or `library.invalidateCache` is called. With the library
   * disabled it succeeds with `enabled: false` and no roots; a failed build fails with
   * `OPERATION_FAILED`.
   * @zh 获取媒体库已配置的根文件夹列表；仅解析为稳定本地路径的曲目计入根目录，`http://`、`file-relative://`、`unpack://`、`archive://` 等协议路径计入 `skippedTracks`；首次调用同步构建索引后复用，直至媒体库变更或调用 `library.invalidateCache` 为止；媒体库未启用时返回 `enabled: false` 且 roots 为空，构建失败时以 `OPERATION_FAILED` 失败
   */
  getRoots(): GetRootsResult;

  /**
   * Number of tracks in the library.
   * @zh 获取媒体库中的曲目总数
   */
  getCount(): GetCountResult;

  /**
   * Look up one file in the library and return its main fields as a flat object. A file that is
   * not in the library succeeds with `found: false`.
   * @zh 在媒体库中按路径检索单个文件并以扁平对象返回主要字段；文件未收录于媒体库时返回 `found: false`
   */
  getByPath(params: GetByPathParams): GetByPathResult;

  /**
   * Albums of the whole library, grouped by album name plus album artist; tracks without an
   * `album` tag are skipped. A complete list (`offset` 0, no `includeTracks`, every album within
   * `limit`) is kept until the library changes, and a later call with the same `query`, `sort`
   * and `includeCover` from `offset` 0 without `includeTracks` is answered from it. The grouping
   * behind the list is kept the same way and shared with `library.getAlbumTracks`, so other calls
   * do not read the library again either until it changes. With the library disabled it succeeds
   * with no albums.
   * @zh 获取全媒体库专辑列表，按专辑名加专辑艺术家归组并跳过缺失 `album` 标签的曲目；完整查询结果（`offset: 0`、未传 `includeTracks` 且全部专辑均在 `limit` 内）将缓存至媒体库变更，后续参数一致的调用直接复用缓存；归组缓存同时共享给 `library.getAlbumTracks`，避免重复全库遍历；媒体库未启用时返回空列表
   * @effect read
   */
  getAlbums(params: GetAlbumsParams): GetAlbumsResult;

  /**
   * Every credited artist with participation counts: each value of a multi-value `artist` gets
   * its own row. The scan is kept until the library changes. Fails with `LIBRARY_DISABLED` when
   * the library is disabled.
   * @zh 获取全媒体库所有署名艺术家及其参与统计；多值 `artist` 的每个取值单独成行；全库扫描结果将缓存至媒体库变更；媒体库未启用时以 `LIBRARY_DISABLED` 失败
   * @effect read
   */
  getArtists(params: GetArtistsParams): GetArtistsResult;

  /**
   * Whether the library is enabled and how many tracks it holds. The answer is kept until the
   * library changes; with the library disabled nothing is kept.
   * @zh 查询媒体库启用状态及当前曲目数；查询结果缓存至媒体库变更，媒体库未启用时不缓存
   */
  getStatus(): GetStatusResult;

  /**
   * Counters of the host's library caches and of the directory tree index, for diagnostics.
   * @zh 获取宿主媒体库缓存与目录树索引的运行计数，供状态诊断使用
   */
  getCacheStats(): GetCacheStatsResult;

  /**
   * Tracks of the whole library in library order, one page at a time. A request from `offset` 0
   * that covers every track is kept until the library changes, and a later call from `offset` 0
   * with `useCache` is answered from it. With `asyncResult`, such a request, when it is not
   * answered from the kept list, is built off the main thread: the call answers
   * `{ pending: true, requestId }` at once and the page arrives as the `library:getAllResult`
   * event on the calling window.
   * @zh 按媒体库原生顺序分页获取全库曲目；覆盖全库的起始分页请求（`offset: 0` 且容纳全部曲目）将缓存至媒体库变更，后续指定 `useCache: true` 的调用直接复用缓存；传入 `asyncResult: true` 且未命中缓存时在后台线程异步构建，立即返回 `{ pending: true, requestId }` 并通过 `library:getAllResult` 事件将该页数据推送至调用窗口
   */
  getAll(params: GetAllParams): GetAllResult;

  /**
   * The tracks of one `library.getAlbums` row, named by the row's `name` and `albumArtist`. Tracks
   * are grouped exactly as `library.getAlbums` groups them, so `total` equals the row's
   * `trackCount`. They are sorted by disc number, then track number, then library order. The
   * grouping is kept until the library changes and `library.getAlbums` builds and reads the same
   * one, so a call after it, or after an earlier call, does not read the library again. A name
   * and album artist that no row has, and a disabled library, succeed with no tracks and no
   * `row`. Pass the row's `albumId` as well when present; it takes precedence over the names.
   * A malformed ID fails with `INVALID_PARAMS` and `details.param` `albumId`.
   * @zh 获取指定专辑所包含的全部曲目，通过 `library.getAlbums` 返回的 `name` 与 `albumArtist` 指定（若存在 `albumId` 建议一并传入且优先级高于名称）；曲目归组逻辑与 `library.getAlbums` 完全一致，`total` 等于专辑行的 `trackCount`；曲目依次按碟号、曲号及媒体库顺序排序并共享归组缓存；未找到对应专辑或媒体库未启用时返回空曲目列表且不含 `row`，传入畸形 `albumId` 时以 `INVALID_PARAMS` 失败（`details.param` 为 `albumId`）
   */
  getAlbumTracks(params: GetAlbumTracksParams): GetAlbumTracksResult;

  /**
   * Tracks an artist is credited on, in library order. `artist` is matched as
   * `library.getArtistAlbums` matches it under `match: 'exact'`. An empty `artist` and a disabled
   * library both succeed with no tracks.
   * @zh 按媒体库顺序获取指定艺术家参与的所有曲目；艺术家匹配规则与 `library.getArtistAlbums` 的 `match: 'exact'` 一致；`artist` 为空或媒体库未启用时均返回空列表
   */
  getArtistTracks(params: GetArtistTracksParams): GetArtistTracksResult;

  /**
   * Tracks drawn at random from the whole library without repeats, a new draw on every call. A
   * disabled or empty library succeeds with no tracks.
   * @zh 从全媒体库中随机抽取互不重复的曲目列表；每次调用均重新独立抽取；媒体库未启用或为空库时返回空列表
   */
  getRandomTracks(params: GetRandomTracksParams): GetRandomTracksResult;

  /**
   * The newest tracks of the library. `added` orders by the `%added%` field of foo_playcount; when
   * no track has it, the call orders by file modification time instead and says so in `fallback`.
   * The library is walked on every call.
   * @zh 获取媒体库中最新添加的曲目；默认按 foo_playcount 的 `%added%` 字段降序排序，全库曲目均缺失该字段时降级按文件修改时间排序并在 `fallback` 中注明；每次调用均实时遍历全库
   */
  getRecentlyAdded(params: GetRecentlyAddedParams): GetRecentlyAddedResult;

  /**
   * One folder under a library root: its subfolders and, with `includeFiles`, its tracks. Reads
   * the directory tree index `library.getRoots` uses, building it first when needed. An unknown
   * `rootId` or `pathId` fails with `NOT_FOUND`; a failed index build fails with
   * `OPERATION_FAILED`.
   * @zh 浏览媒体库根目录下的指定文件夹及其子目录；指定 `includeFiles: true` 时一并返回包含的曲目；读取与 `library.getRoots` 共用的目录树索引并在必要时同步构建；未知的 `rootId` 或 `pathId` 以 `NOT_FOUND` 失败，索引构建失败以 `OPERATION_FAILED` 失败
   */
  browseTree(params: BrowseTreeParams): BrowseTreeResult;

  /**
   * List the library by path prefix: the folders one level below `path` and, with `includeFiles`,
   * every track under it at any depth. The prefix is matched against the paths as foobar2000
   * stores them, so a local folder has to be written as `file://` followed by its path; a plain
   * absolute path matches nothing and succeeds with empty lists. `library.browseTree` walks the
   * real library roots instead. Fails with `LIBRARY_DISABLED` when the library is disabled.
   * @zh 按路径前缀列出媒体库下一层目录；指定 `includeFiles: true` 时递归返回该路径下任意深度的所有曲目；前缀直接匹配 foobar2000 内部存储路径，本地目录须使用 `file://` 协议前缀（传入纯绝对路径无法匹配任何结果）；浏览物理根目录推荐使用 `library.browseTree`；媒体库未启用时以 `LIBRARY_DISABLED` 失败
   */
  browseDirectory(params: BrowseDirectoryParams): BrowseDirectoryResult;

  /**
   * Tracks matching a foobar2000 query, optionally sorted by `sort`, up to `limit`. A query the
   * parser rejects fails with `INVALID_PARAMS` and `details.param` `query`, and so does one
   * carrying `SORT BY`; many malformed queries are not rejected but simply match nothing. The rows
   * are written off the main thread. Fails with `LIBRARY_DISABLED` when the library is disabled.
   * @zh 检索符合 foobar2000 查询表达式的曲目列表，支持通过 `sort` 排序并限制返回最多 `limit` 行；查询解析失败或包含 `SORT BY` 子句时以 `INVALID_PARAMS` 失败（`details.param` 为 `query`）；部分畸形语法不会报错但将匹配不到任何结果；数据在后台线程序列化写出；媒体库未启用时以 `LIBRARY_DISABLED` 失败
   */
  query(params: QueryParams): QueryResult;

  /**
   * One page of the tracks matching a foobar2000 query, in library order: a `SORT BY` clause is
   * accepted but not applied. A query the parser rejects fails with `INVALID_PARAMS` and
   * `details.param` `query`. An empty `query` and a disabled library both succeed with no tracks.
   * The rows are written off the main thread.
   * @zh 分页检索符合 foobar2000 查询表达式的曲目，固定按媒体库原生顺序排列（支持传入 `SORT BY` 子句但不生效）；查询解析失败时以 `INVALID_PARAMS` 失败（`details.param` 为 `query`）；`query` 为空或媒体库未启用时均返回空列表；数据在后台线程序列化写出
   * @effect read
   */
  search(params: SearchParams): SearchResult;

  /**
   * The library revision and tracks changed after revision `since`, for local incremental sync.
   * The revision persists across restarts. When `epoch` matches and `since` is within the recorded
   * range, returns `changed` and `removed`. Otherwise sets `resync: true` with full `handles` for
   * the page to rebuild its copy. Changes reported after startup loading advance the revision,
   * including play statistics from components like foo_playcount. Fails with `OPERATION_PENDING`
   * (`details.reason: 'libraryLoading'`) while the library is still loading. When disabled, returns
   * `resync: true` with empty `handles`.
   * @zh 获取当前媒体库修订号及指定修订号 `since` 之后的变化曲目，用于前端增量同步；修订版本号跨进程重启持久化保存；`epoch` 匹配且 `since` 落在有效历史范围内时返回 `changed` 与 `removed` 列表；超出范围时返回 `resync: true` 及全量 `handles` 由前端全量重建；媒体库初始载入完成后的所有变更（含 foo_playcount 等组件写入的统计数据）均会递增修订号；启动载入期间调用以 `OPERATION_PENDING` 失败（`details.reason: "libraryLoading"`）；媒体库未启用时返回 `resync: true` 且 `handles` 为空
   * @effect read
   */
  getChanges(params: GetChangesParams): GetChangesResult;

  /**
   * Fetch library rows in the given handle order without reading media files. Missing handles (or
   * all handles when disabled) are reported in `missing`. More than 5000 handles fail with
   * `INVALID_PARAMS`. Rows are serialized off the main thread.
   * @zh 按传入的 handle 顺序批量获取媒体库曲目行；数据直接从内存读取，不访问媒体文件；未收录或媒体库禁用时的 handle 归入 `missing` 列表；请求超过 5000 个 handle 时以 `INVALID_PARAMS` 失败；数据在后台线程序列化写出
   * @effect read
   */
  getTracks(params: GetTracksParams): GetTracksResult;
}

export interface Events {
  /**
   * Sent after foobar2000 finishes loading the library at startup and the host purges its library
   * caches. Broadcast to all open windows; pages loaded afterwards will not receive it. Use
   * `library.getStatus` to query current status.
   * @zh foobar2000 启动阶段载入媒体库完毕且宿主完成缓存清理后触发；广播通知所有当前已打开的窗口，后续加载的页面不会补发；可通过 `library.getStatus` 主动查询当前就绪状态
   * @delivery broadcast
   */
  initialized: InitializedPayload;

  /**
   * Tracks were added to the Media Library. The host drops its library caches before sending it.
   * @zh 媒体库新增曲目时触发；宿主在广播前先行清空自身媒体库缓存
   * @delivery broadcast
   */
  itemsAdded: ItemsChangedPayload;

  /**
   * Tracks were removed from the Media Library. The host drops its library caches before sending
   * it.
   * @zh 媒体库移除曲目时触发；宿主在广播前先行清空自身媒体库缓存
   * @delivery broadcast
   */
  itemsRemoved: ItemsChangedPayload;

  /**
   * Tracks in the Media Library were modified: their tags changed, or a component such as
   * `foo_playcount` asked foobar2000 to show its fields anew (`fromHook`). Paths stay as they
   * were, so the host keeps the directory index behind `library.getRoots` and
   * `library.browseTree`. It drops its other library caches before sending it; for a
   * `fromHook` change only the kept `library.getAll` list, because `rating` is the one field of
   * the kept results that such a component can supply.
   * @zh 媒体库曲目发生变更时触发（如标签更新或 `foo_playcount` 等第三方组件请求刷新播放统计字段 `fromHook`）；由于文件路径未变，宿主保留 `library.getRoots` 与 `library.browseTree` 所需的目录树索引，清空其余媒体库缓存后对外广播；`fromHook` 变更时仅清除缓存的 `library.getAll` 列表（因缓存字段中仅 `rating` 可能受组件影响）
   * @delivery broadcast
   */
  itemsModified: ItemsModifiedPayload;

  /**
   * The page of a `library.getAll` that answered `{ pending: true, requestId }`: the whole library
   * in library order, built off the main thread. The list is kept for later calls only when the
   * library did not change meanwhile. Sent to the page that made the call, or to a page under the
   * same top-level window when that one cannot be found, or else to the main window's page.
   * @zh `library.getAll` 返回 `{ pending: true, requestId }` 后的异步数据就绪事件；包含在后台线程按媒体库顺序构建的全库曲目列表；仅在构建期间媒体库未发生变更时缓存供后续调用；优先投递至发起调用的页面，若该页面已销毁则退至同一顶层窗口内的页面或主窗口页面
   * @delivery caller
   */
  getAllResult: GetAllResultPayload;
}

interface InitializedPayload {
  /**
   * When the host sent it, in milliseconds since the Unix epoch.
   * @zh 宿主发出事件的时间戳（自 Unix 纪元起的毫秒数）
   */
  timestamp: Int;
}

interface GetAllResultPayload {
  /**
   * Id from the `{ pending: true, requestId }` answer.
   * @zh 对应异步请求调用返回的标识符 `requestId`
   */
  requestId: string;
  /**
   * The whole library in library order; `index` is the position in the library. Empty when
   * `error` is present.
   * @zh 整个媒体库曲目列表，按媒体库原生顺序排列（发生错误时为空列表）
   */
  tracks: LibraryTrack[];
  /**
   * The same list as `tracks`.
   * @zh 与 `tracks` 完全一致的兼容曲目列表
   */
  items: LibraryTrack[];
  /**
   * Tracks in the library when the call was made; `0` when `error` is present.
   * @zh 调用时媒体库曲目总数；发生错误时为 `0`
   */
  total: Int;
  /**
   * The `offset` of the call, always `0`.
   * @zh 请求的起始偏移量，固定为 `0`
   */
  offset: Int;
  /**
   * The `limit` of the call.
   * @zh 请求的分页大小上限
   */
  limit: Int;
  /**
   * Always `false`.
   * @zh 固定为 `false`
   */
  fromCache: boolean;
  /**
   * Why the list could not be built; present only then.
   * @zh 列表异步构建失败的原因说明；仅在失败时出现
   */
  error?: string;
}

interface ItemsChangedPayload {
  /**
   * How many tracks.
   * @zh 发生变动的曲目数量
   */
  count: Int;
  /**
   * The `handle` of every track, the key track rows carry, in no particular order. Empty when
   * `truncated`.
   * @zh 发生变动的每首曲目 handle 列表，顺序不固定；`truncated: true` 时为空列表
   */
  handles: string[];
  /**
   * `true` when more than 5000 tracks changed at once. `handles` is then empty, and a page
   * should read again everything it shows.
   * @zh 单次变动是否超过 5000 首；为 `true` 时 `handles` 为空，前端应全量重新拉取数据
   */
  truncated: boolean;
  /**
   * When the host sent it, in milliseconds since the Unix epoch.
   * @zh 宿主发出事件的时间戳（自 Unix 纪元起的毫秒数）
   */
  timestamp: Int;
}

interface ItemsModifiedPayload {
  /**
   * How many tracks.
   * @zh 发生变动的曲目数量
   */
  count: Int;
  /**
   * The `handle` of every track, the key track rows carry, in no particular order. Empty when
   * `truncated`.
   * @zh 发生变动的每首曲目 handle 列表，顺序不固定；`truncated: true` 时为空列表
   */
  handles: string[];
  /**
   * `true` when more than 5000 tracks changed at once. `handles` is then empty, and a page
   * should read again everything it shows.
   * @zh 单次变动是否超过 5000 首；为 `true` 时 `handles` 为空，前端应全量重新拉取数据
   */
  truncated: boolean;
  /**
   * `true` when the files themselves did not change and a component such as `foo_playcount`
   * asked for its fields to be shown anew. Paths, tags and album grouping are then as before.
   * @zh 文件物理内容未变，仅由 `foo_playcount` 等第三方组件请求刷新统计字段时为 `true`；此时路径、标签与专辑归组均保持不变
   */
  fromHook: boolean;
  /**
   * When the host sent it, in milliseconds since the Unix epoch.
   * @zh 宿主发出事件的时间戳（自 Unix 纪元起的毫秒数）
   */
  timestamp: Int;
}

/**
 * One value of a tag and the number of tracks carrying it.
 * @zh 标签取值及其在媒体库中的曲目计数
 */
interface LibraryValueCount {
  /**
   * The value.
   * @zh 标签取值内容
   */
  name: string;
  /**
   * Tracks carrying the value.
   * @zh 带有该取值的曲目总数
   */
  trackCount: Int;
}

/**
 * One album row, as `library.getAlbums` and `library.getArtistAlbums` return it; the `row` of
 * `library.getAlbumTracks` has the same shape.
 * @zh 专辑信息行（`library.getAlbums`、`library.getArtistAlbums` 及 `library.getAlbumTracks` 的 `row` 均为此结构）
 */
interface AlbumInfo {
  /**
   * Opaque identity for an album whose original name or album artist is not valid UTF-8.
   * Pass it to `library.getAlbumTracks`; display text alone cannot identify such an album.
   * Absent for ordinary names and for the differently grouped `library.getArtistAlbums` rows.
   * @zh 原始专辑名或专辑艺术家包含非法 UTF-8 字符时的不透明身份标识；传给 `library.getAlbumTracks` 精确匹配（普通专辑或 `library.getArtistAlbums` 分组行不包含此字段）
   */
  albumId?: string;
  /**
   * Album name.
   * @zh 专辑名称
   */
  name: string;
  /**
   * `albumArtist` when it is set, otherwise the first `artist` value found among the tracks.
   * @zh 专辑艺术家；未设置时回退至专辑内第一首曲目的艺术家
   */
  artist: string;
  /**
   * The first `album artist` value found among the tracks, falling back to the track's `artist`;
   * empty when neither exists.
   * @zh 专辑内第一首曲目的 `album artist` 标签值，缺失时回退至首曲 `artist` 标签，两者均无时为空字符串
   */
  albumArtist: string;
  /**
   * Tracks counted into the row.
   * @zh 归入该专辑行的曲目数量
   */
  trackCount: Int;
  /**
   * Distinct disc numbers among those tracks; `1` when none carries one.
   * @zh 该专辑内包含的不同碟号总数；曲目均无碟号时为 `1`
   */
  discCount: Int;
  /**
   * Summed length of those tracks, in seconds.
   * @zh 该专辑所有曲目的总时长（秒）
   */
  duration: number;
  /**
   * The first `date` value found among the tracks; empty when none has one.
   * @zh 专辑内曲目中首个读取到的 `date` 标签值；均无时为空字符串
   */
  year: string;
  /**
   * The first `genre` value found among the tracks; empty when none has one.
   * @zh 专辑内曲目中首个读取到的 `genre` 标签值；均无时为空字符串
   */
  genre: string;
  /**
   * The first `publisher` value found among the tracks, or else the first `label` value; empty
   * when none has either.
   * @zh 专辑内曲目中首个读取到的 `publisher` 标签值，缺失时读取首个 `label` 标签值，均无时为空字符串
   */
  label: string;
  /**
   * Path of the first track counted, as foobar2000 stores it; it can be a `file-relative://`
   * URI.
   * @zh 归入该专辑的首首曲目路径（foobar2000 内部存储格式，可能包含 `file-relative://` 等协议）
   */
  firstTrackPath: string;
  /**
   * `firstTrackPath` as a native file path, the form to pass to `artwork.getForTrack`; absent
   * when there is no first track path.
   * @zh `firstTrackPath` 对应的规范化本地物理文件路径，可直接传给 `artwork.getForTrack`；首曲无物理路径时缺省
   */
  firstTrackAbsolutePath?: string;
  /**
   * Front cover as a `data:image/...` URL; only `library.getAlbums` with `includeCover` fills it,
   * and only when a cover exists.
   * @zh 专辑正面封面 Data URL（`data:image/...`）；仅在 `library.getAlbums` 传入 `includeCover: true` 且存在有效封面时返回
   */
  coverDataUrl?: string;
  /**
   * Tracks in track-number order. `library.getAlbums` fills it with `includeTracks`;
   * `library.getArtistAlbums` fills it with the tracks credited to the requested artist in
   * this group. Use these references to keep groups with equal display names apart.
   * @zh 按曲号排序的曲目列表；`library.getAlbums` 传入 `includeTracks: true` 时填充，`library.getArtistAlbums` 填充该艺术家参与的曲目
   */
  tracks?: AlbumTrackRef[];
}

/**
 * One track of an album row.
 * @zh 专辑行引用的曲目简明信息
 */
interface AlbumTrackRef {
  /**
   * Subsong index; combine with `path` when addressing a track in a multi-track file.
   * @zh 子曲目索引；与 `path` 配合唯一定位多子曲目文件中的单曲
   */
  subsong?: Int;
  /**
   * Track number; `0` when the track has none.
   * @zh 曲号序号；未标记时为 `0`
   */
  trackNumber: Int;
  /**
   * Track path, as foobar2000 stores it.
   * @zh 曲目路径（foobar2000 内部存储格式）
   */
  path: string;
  /**
   * `path` as a native file path.
   * @zh 曲目的规范化本地物理文件路径
   */
  absolutePath: string;
}

/**
 * One library root folder.
 * @zh 媒体库配置的物理根文件夹信息
 */
interface LibraryRootInfo {
  /**
   * Stable identifier of the root, currently its `absolutePath`; `library.browseTree` takes it.
   * @zh 根目录的稳定唯一标识（当前实现为其规范化物理路径），可传给 `library.browseTree`
   */
  id: string;
  /**
   * The folder name, or the full path when two roots share a name.
   * @zh 目录显示名称；存在重名根目录时显示完整物理路径
   */
  displayName: string;
  /**
   * Currently the same as `absolutePath`.
   * @zh 根目录原始物理路径（当前与 `absolutePath` 相同）
   */
  rawPath: string;
  /**
   * Canonical local path of the folder.
   * @zh 根目录的规范化本地物理路径
   */
  absolutePath: string;
  /**
   * Library tracks under the folder.
   * @zh 归入该根目录下的媒体库曲目总数
   */
  trackCount: Int;
}

/**
 * One track row of a library list: the shared track fields plus `index`.
 * @zh 媒体库曲目行数据（包含通用曲目字段及当前列表上下文下的行号 `index`）
 */
interface LibraryTrack extends Track {
  /**
   * Row number; the list the row is in says what it counts.
   * @zh 当前上下文列表中的行索引序号
   */
  index: Int;
}

/**
 * One row of `library.getRecentlyAdded`.
 * @zh `library.getRecentlyAdded` 返回的近期添加曲目信息
 */
interface RecentLibraryTrack extends LibraryTrack {
  /**
   * The `%added%` value as foo_playcount formats it, such as `2024-05-01 12:34:56`; present only
   * when the list is ordered by it and the track has one.
   * @zh foo_playcount 格式化输出的 `%added%` 时间字符串（如 `2024-05-01 12:34:56`）；仅在按此字段排序且曲目存在记录时返回
   */
  added?: string;
  /**
   * File modification time, in seconds since the Unix epoch; present only when the list is
   * ordered by it and the time is known.
   * @zh 文件物理修改时间戳（自 Unix 纪元起的秒数）；仅在按修改时间排序且时间有效时返回
   */
  modified?: Int;
}

/**
 * One folder of the directory tree index.
 * @zh 目录树索引节点文件夹信息
 */
interface LibraryDirectoryNodeInfo {
  /**
   * `rootId`, then `::`, then `pathId`.
   * @zh 文件夹节点复合标识符（格式为 `${rootId}::${pathId}`）
   */
  id: string;
  /**
   * The root the folder is under.
   * @zh 文件夹所属的根目录 `id`
   */
  rootId: string;
  /**
   * Path below the root with `/` between folder names; pass it to `library.browseTree` to open
   * the folder.
   * @zh 相对于根目录的路径（以 `/` 分隔各级目录名），可直接传给 `library.browseTree` 打开该目录
   */
  pathId: string;
  /**
   * `pathId` of the parent folder; `""` directly under the root.
   * @zh 父级目录的 `pathId`；直接位于根目录下时为空字符串 `""`
   */
  parentPathId: string;
  /**
   * Folder name.
   * @zh 文件夹名称
   */
  name: string;
  /**
   * Currently the same as `name`.
   * @zh 文件夹显示名称（当前与 `name` 相同）
   */
  displayName: string;
  /**
   * Currently the same as `absolutePath`.
   * @zh 根目录原始物理路径（当前与 `absolutePath` 相同）
   */
  rawPath: string;
  /**
   * Local path of the folder.
   * @zh 文件夹的规范化本地物理路径
   */
  absolutePath: string;
  /**
   * Currently the same as `pathId`.
   * @zh 相对路径（当前与 `pathId` 相同）
   */
  relativePath: string;
  /**
   * Folder names in `pathId`: `1` directly under the root.
   * @zh 目录在 `pathId` 中的层级深度；直接位于根目录下时为 `1`
   */
  depth: Int;
  /**
   * Library tracks in the folder and every folder below it.
   * @zh 该文件夹及其所有子目录中包含的媒体库曲目总数
   */
  trackCount: Int;
  /**
   * Subfolders directly inside.
   * @zh 直接下属的子文件夹数量
   */
  childDirectoryCount: Int;
  /**
   * Whether `childDirectoryCount` is above `0`.
   * @zh 是否包含直接子文件夹（即 `childDirectoryCount > 0`）
   */
  hasChildren: boolean;
}

// ---- isEnabled ----

interface IsEnabledResult {
  /**
   * Whether the media library is enabled.
   * @zh 媒体库是否已配置文件夹并启用
   */
  enabled: boolean;
}

// ---- getStats ----

interface GetStatsResult {
  /**
   * Tracks in the library.
   * @zh 媒体库曲目总数
   */
  totalTracks: Int;
  /**
   * Albums, counted by album name plus album artist, as `library.getAlbums` groups them.
   * @zh 专辑总数（按专辑名加专辑艺术家归组，口径与 `library.getAlbums` 一致）
   */
  totalAlbums: Int;
  /**
   * Credited artists: every value of a multi-value `artist` counts, matching the entry count of
   * `library.getArtists`.
   * @zh 参与艺术家总数（多值 `artist` 逐项计数，与 `library.getArtists` 条目数一致）
   */
  totalArtists: Int;
  /**
   * Summed length of all tracks, in seconds.
   * @zh 全库曲目总时长（秒）
   */
  totalDuration: number;
  /**
   * Summed file size, in bytes.
   * @zh 全库媒体文件总大小（字节）
   */
  totalSize: Int;
  /**
   * Whether the host holds cached library results written since the cache was last dropped.
   * @zh 宿主是否持有有效的媒体库内存缓存
   */
  cacheValid: boolean;
  /**
   * When the cache was last dropped, in milliseconds since the Unix epoch.
   * @zh 上次清理或丢弃缓存的时间戳（自 Unix 纪元起的毫秒数）
   */
  lastModified: Int;
}

// ---- invalidateCache ----

interface InvalidateCacheResult {
  /**
   * When the cache was dropped, in milliseconds since the Unix epoch.
   * @zh 缓存被清空丢弃的时间戳（自 Unix 纪元起的毫秒数）
   */
  timestamp: Int;
}

// ---- getGenres ----

interface GetGenresResult {
  /**
   * Every genre with its track count.
   * @zh 全媒体库所有流派及其曲目数列表
   */
  genres: LibraryValueCount[];
}

// ---- addToPlaylist ----

interface AddToPlaylistParams {
  /**
   * Paths to add, in this order; a `|subsong:N` suffix selects a subsong.
   * @zh 待追加的文件路径列表，按传入顺序依次添加；可附加 `|subsong:N` 后缀指定子曲目
   * @minItems 1
   * @security MediaRead
   */
  paths: string[];
  /**
   * Index of the target playlist; omitted, the active playlist. An index past the last playlist
   * fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with
   * `NO_ACTIVE_ITEM`.
   * @zh 目标播放列表的索引序号；省略时默认操作活动播放列表；越界索引以 `INVALID_INDEX` 失败，缺省且无活动列表时以 `NO_ACTIVE_ITEM` 失败
   * @minimum 0
   */
  playlist?: Int;
  playlistGuid?: PlaylistGuid;
}

interface AddToPlaylistResult {
  /**
   * Tracks added.
   * @zh 成功追加到播放列表的曲目总数
   */
  added: Int;
}

// ---- getArtistAlbums ----

interface GetArtistAlbumsParams {
  /**
   * Artist name, compared byte for byte against each value of the `artist` tag, so a name from
   * `library.getArtists` matches even when it is not the first of several credited artists.
   * @zh 目标艺术家名称；与曲目中的每个 `artist` 标签值进行逐字节匹配（多位艺术家的非首位名字亦可命中）
   * @minLength 1
   */
  artist: string;
  /**
   * Most albums to return, applied after grouping; there is no offset.
   * @zh 返回的专辑数量上限（在归组完成后截断，无 offset 偏移）
   * @default 100
   * @minimum 0
   */
  limit?: Int;
  /**
   * `name`, `artist`, `year` or `trackCount`, as in `library.getAlbums`; any other value sorts by
   * name.
   * @zh 排序字段，支持 `name`、`artist`、`year` 或 `trackCount`（规则与 `library.getAlbums` 一致）；其他值默认按名称升序排列
   * @default "name"
   */
  sort?: string;
  /**
   * `exact` needs a whole tag value to equal `artist`; `substring` matches a tag value that
   * contains it, and then a lowercase letter in `artist` matches either case while an uppercase
   * one matches only uppercase.
   * @zh 匹配规则：`exact` 精确匹配整个标签值，`substring` 包含子串匹配（小写字母忽略大小写，大写字母严格区分）
   * @default "exact"
   */
  match?: 'exact' | 'substring';
}

interface GetArtistAlbumsResult {
  /**
   * The artist, as given.
   * @zh 请求传入的原样艺术家名称
   */
  artist: string;
  /**
   * The albums, sorted by `sort`, with `tracks` referencing this artist's members of each
   * group. Rows do not carry `coverDataUrl` or a canonical `albumId`.
   * @zh 排序后的专辑列表，各行的 `tracks` 仅引用该艺术家参与的曲目；不包含 `coverDataUrl` 与 `albumId`
   */
  albums: AlbumInfo[];
  /**
   * Albums found, before `limit`.
   * @zh 截断前匹配到的专辑总数
   */
  total: Int;
  /**
   * Whether `limit` cut the list short.
   * @zh 是否因达到 `limit` 上限而截断了后续专辑
   */
  hasMore: boolean;
}

// ---- getFieldValues ----

interface GetFieldValuesParams {
  /**
   * Tag name, such as `genre`.
   * @zh 目标元数据标签名称（如 `genre`）
   * @minLength 1
   */
  field: string;
  /**
   * Splits each tag value into several values at this string, trimming spaces and tabs around
   * each; empty, values are taken whole.
   * @zh 拆分多值标签的分隔符；依此字符串拆分为多个独立取值并去除各段前后空白，为空字符串时不拆分按整值统计
   * @default ""
   */
  separator?: string;
  /**
   * Most values to return.
   * @zh 最多返回的取值数量上限
   * @default 5000
   * @minimum 0
   */
  limit?: Int;
}

interface GetFieldValuesResult {
  /**
   * The values, most tracks first.
   * @zh 标签取值列表，按包含曲目数降序排列
   */
  values: LibraryValueCount[];
  /**
   * Distinct values found, before `limit`.
   * @zh 截断前匹配到的不同取值总数
   */
  total: Int;
  /**
   * The tag name, as given.
   * @zh 请求传入的原样标签名称
   */
  field: string;
}

// ---- getRoots ----

interface GetRootsResult {
  /**
   * Whether the media library is enabled.
   * @zh 媒体库是否已配置文件夹并启用
   */
  enabled: boolean;
  /**
   * The root folders.
   * @zh 媒体库根文件夹列表
   */
  roots: LibraryRootInfo[];
  /**
   * Number of roots.
   * @zh 根文件夹总数
   */
  total: Int;
  /**
   * Tracks placed under a root.
   * @zh 归入物理根目录下的有效曲目数
   */
  indexedTracks: Int;
  /**
   * Tracks left out because they have no stable local path.
   * @zh 因缺少稳定本地物理路径而跳过的曲目数
   */
  skippedTracks: Int;
  /**
   * `true` when the index already existed before this call.
   * @zh 是否直接命中已构建的目录树索引缓存
   */
  fromCache: boolean;
}

// ---- getCount ----

interface GetCountResult {
  /**
   * Tracks in the library.
   * @zh 媒体库曲目总数
   */
  count: Int;
}

// ---- getByPath ----

interface GetByPathParams {
  /**
   * File path. It is looked up as the file's first subsong; a `|subsong:N` suffix is not
   * recognized.
   * @zh 目标媒体文件路径；按首个子曲目检索，不支持 `|subsong:N` 后缀
   * @minLength 1
   * @security MediaRead
   */
  path: string;
}

interface GetByPathResult {
  /**
   * Whether the file is in the library; the other fields besides `path` are present only when
   * it is.
   * @zh 媒体库中是否收录该文件；为 `false` 时仅返回 `path`
   */
  found: boolean;
  /**
   * The track path as foobar2000 stores it when found; otherwise the path as given.
   * @zh 收录时返回 foobar2000 存储格式的路径，未收录时返回传入的原样路径
   */
  path: string;
  /**
   * The track path as a native file path.
   * @zh 曲目的规范化本地物理文件路径
   */
  absolutePath?: string;
  /**
   * The first `title` value; empty when absent.
   * @zh 首个 `title` 标签值；缺失时为空字符串
   */
  title?: string;
  /**
   * Every `artist` value joined with `, ` in their original order.
   * @zh 所有 `artist` 标签值按原顺序以 `, ` 连接
   */
  artist?: string;
  /**
   * Every `artist` value; `artists.join(', ')` equals `artist`.
   * @zh 所有 `artist` 标签值的数组列表
   */
  artists?: string[];
  /**
   * The first `album` value; empty when absent.
   * @zh 首个 `album` 标签值；缺失时为空字符串
   */
  album?: string;
  /**
   * Length in seconds.
   * @zh 曲目时长（秒）
   */
  duration?: number;
  /**
   * The first `tracknumber` value as written in the tag, such as `2` or `02/12`; empty when
   * absent.
   * @zh 首个 `tracknumber` 标签值（保留标签原始书写格式，如 `2` 或 `02/12`）；缺失时为空字符串
   */
  trackNumber?: string;
  /**
   * Every `genre` value joined with `, `.
   * @zh 所有 `genre` 标签值以 `, ` 连接
   */
  genre?: string;
  /**
   * The first `date` value; empty when absent.
   * @zh 首个 `date` 标签值；缺失时为空字符串
   */
  date?: string;
}

// ---- getAlbums ----

interface GetAlbumsParams {
  /**
   * `name`, `artist` (the album artist), `year` or `trackCount`; any other value sorts by name.
   * @zh 排序字段，支持 `name`、`artist`（专辑艺术家）、`year` 或 `trackCount`；传入其他值时默认按名称排序
   * @default "name"
   */
  sort?: string;
  /**
   * Keep only albums whose name or artist contains this text, compared case-insensitively for
   * ASCII letters; empty keeps every album.
   * @zh 过滤关键词；仅保留名称或艺术家包含该文本的专辑（ASCII 字母不区分大小写），为空字符串时不过滤
   * @default ""
   */
  query?: string;
  /**
   * Albums to skip.
   * @zh 跳过的专辑偏移量
   * @default 0
   * @minimum 0
   */
  offset?: Int;
  /**
   * Most albums to return.
   * @zh 本页返回的专辑数量上限
   * @default 100
   * @minimum 0
   */
  limit?: Int;
  /**
   * Add each album's tracks as `tracks`.
   * @zh 是否在每张专辑行中附带曲目列表 `tracks`
   * @default false
   */
  includeTracks?: boolean;
  /**
   * Add each album's front cover as `coverDataUrl`, read from its first track.
   * @zh 是否在每张专辑行中附带首曲正面封面 `coverDataUrl`
   * @default false
   */
  includeCover?: boolean;
  /**
   * Largest cover to inline, in KiB; a larger cover is left out. `0` or less inlines any size.
   * @zh 内联封面的最大尺寸上限（KiB）；超出上限的封面不附带；`0` 或负数表示不限尺寸
   * @default 500
   */
  coverMaxSize?: Int;
  /**
   * Answer from the kept list when there is one; `false` always scans the library, and the
   * result is still kept.
   * @zh 是否在存在有效缓存时直接复用；为 `false` 时强制重新扫描媒体库，扫描后依然写入缓存
   * @default true
   */
  useCache?: boolean;
}

interface GetAlbumsResult {
  /**
   * The albums of this page, sorted by `sort`.
   * @zh 当前分页的专辑列表，按 `sort` 规则排序
   */
  albums: AlbumInfo[];
  /**
   * Albums matching `query`, before `offset` and `limit`.
   * @zh 满足 `query` 过滤条件的专辑总数（分页前）
   */
  total: Int;
  /**
   * The `offset` applied.
   * @zh 实际生效的分页起始偏移量
   */
  offset: Int;
  /**
   * The `limit` applied.
   * @zh 实际生效的分页大小上限
   */
  limit: Int;
  /**
   * Whether albums remain after this page.
   * @zh 当前页之后是否仍有未返回的专辑
   */
  hasMore: boolean;
  /**
   * The `includeCover` applied.
   * @zh 实际生效的封面内联配置
   */
  includeCover: boolean;
  /**
   * `true` when the answer came from the kept list.
   * @zh 当前返回结果是否直接命中缓存
   */
  fromCache: boolean;
}

// ---- getArtists ----

/**
 * One artist row of `library.getArtists`. `albumCount` counts album names, while `albums` keeps
 * albums of the same name by different album artists apart, so the two can differ.
 * @zh 艺术家信息行（`albumCount` 按专辑名计数，`albums` 按专辑名与专辑艺术家联合区分，两者可能不一致）
 */
interface ArtistInfo {
  /**
   * The artist.
   * @zh 艺术家名称
   */
  name: string;
  /**
   * Distinct album names among the artist's tracks.
   * @zh 该艺术家曲目中包含的不同专辑名称总数
   */
  albumCount: Int;
  /**
   * Tracks the artist is credited on.
   * @zh 该艺术家署名的曲目总数
   */
  trackCount: Int;
  /**
   * Summed length of those tracks, in seconds.
   * @zh 该艺术家曲目的总时长（秒）
   */
  totalDuration: number;
  /**
   * The albums the artist is credited on, sorted by name and then artist; present only with
   * `includeAlbums`, and never cut by `limit`.
   * @zh 该艺术家署名的专辑列表，按专辑名升序再按艺术家升序排列；仅在传入 `includeAlbums: true` 时返回且不受 `limit` 截断
   */
  albums?: ArtistAlbumRef[];
}

/**
 * One album an artist is credited on. `(name, artist)` is the grouping `library.getAlbums` uses,
 * so the pair matches exactly one of its rows when no `albumId` is present. With `albumId`,
 * match by that opaque value instead of the display names.
 * @zh 艺术家署名的单张专辑引用（无 `albumId` 时与 `library.getAlbums` 归组行完全对应，存在 `albumId` 时用于精确匹配）
 */
interface ArtistAlbumRef {
  /**
   * The album row's opaque identity when its original grouping fields contain invalid UTF-8.
   * @zh 原始分组字段包含非法 UTF-8 时对应专辑行的不透明身份标识
   */
  albumId?: string;
  /**
   * Album name.
   * @zh 专辑名称
   */
  name: string;
  /**
   * The first `album artist` value, or the first `artist` value when there is none.
   * @zh 首个 `album artist` 标签值，缺失时为首个 `artist` 标签值
   */
  artist: string;
}

interface GetArtistsParams {
  /**
   * `name`, `trackCount` or `albumCount` (both largest first); any other value keeps name order.
   * @zh 排序规则：`name` 按名称升序，`trackCount` 或 `albumCount` 按曲目数或专辑数降序；传入其他值时保持名称排序
   * @default "name"
   */
  sort?: string;
  /**
   * Most artists to return.
   * @zh 返回的艺术家数量上限
   * @default 1000
   * @minimum 0
   */
  limit?: Int;
  /**
   * Add each artist's albums as `albums`.
   * @zh 是否为每位艺术家附带完整的专辑列表 `albums`
   * @default false
   */
  includeAlbums?: boolean;
}

interface GetArtistsResult {
  /**
   * The artists, sorted by `sort`.
   * @zh 排序后的艺术家列表
   */
  items: ArtistInfo[];
  /**
   * Rows returned, after `limit`; there is no total, so a full page means there may be more.
   * @zh 实际返回的艺术家数量（受 `limit` 截断影响，满页表示可能仍有更多）
   */
  count: Int;
}

// ---- getStatus ----

interface GetStatusResult {
  /**
   * Whether the media library is enabled.
   * @zh 媒体库是否已配置文件夹并启用
   */
  enabled: boolean;
  /**
   * Whether foobar2000 has finished loading the library at startup. Prior to completion,
   * `library:itemsAdded` announces loaded tracks rather than newly added ones.
   * @zh 启动初始化是否已完成媒体库载入；完成前收到的 `library:itemsAdded` 表示存量载入曲目而非新增
   */
  initialized: boolean;
  /**
   * Always `false`; the host does not report scanning.
   * @zh 是否正在扫描媒体库；当前实现固定为 `false`（宿主不报告扫描状态）
   */
  scanning: boolean;
  /**
   * Tracks in the library; `0` when it is disabled.
   * @zh 媒体库曲目总数；未启用时为 `0`
   */
  itemCount: Int;
  /**
   * The same as `itemCount`.
   * @zh 与 `itemCount` 完全一致的兼容别名
   */
  count: Int;
}

// ---- getCacheStats ----

interface GetCacheStatsResult {
  /**
   * Whether any library result was kept since the cache was last dropped.
   * @zh 自上次清空缓存以来是否存在有效的媒体库缓存结果
   */
  valid: boolean;
  /**
   * When the cache was last dropped, in milliseconds since the Unix epoch.
   * @zh 上次清理或丢弃缓存的时间戳（自 Unix 纪元起的毫秒数）
   */
  lastModified: Int;
  /**
   * Kept `library.getAlbums` lists, one per `query`, `sort` and `includeCover`.
   * @zh 缓存的 `library.getAlbums` 列表数量（按不同 `query`、`sort`、`includeCover` 组合分别缓存）
   */
  albumsCacheEntries: Int;
  /**
   * Whether a full `library.getAll` result is kept.
   * @zh 是否已缓存全量 `library.getAll` 结果
   */
  tracksCached: boolean;
  /**
   * Whether the `library.getArtists` scan is kept.
   * @zh 是否已缓存 `library.getArtists` 全库扫描结果
   */
  artistsCached: boolean;
  /**
   * Always `false`; genres are not kept.
   * @zh 是否缓存流派列表；固定为 `false`（流派不缓存）
   */
  genresCached: boolean;
  /**
   * Whether the `library.getStatus` answer is kept.
   * @zh 是否已缓存 `library.getStatus` 查询结果
   */
  statsCached: boolean;
  /**
   * Always `0`; covers are not kept.
   * @zh 缓存的封面数量；固定为 `0`（封面不缓存）
   */
  coversCached: Int;
  /**
   * Always `0`; covers are not kept.
   * @zh 缓存的封面数量；固定为 `0`（封面不缓存）
   */
  coverCacheBytes: Int;
  /**
   * Always `0`; covers are not kept.
   * @zh 缓存的封面数量；固定为 `0`（封面不缓存）
   */
  coverCacheMB: number;
  /**
   * Lookups answered from a kept result since the host started.
   * @zh 宿主启动以来命中内存缓存的查询总次数
   */
  cacheHits: Int;
  /**
   * Lookups that found nothing kept since the host started.
   * @zh 宿主启动以来未命中内存缓存的查询总次数
   */
  cacheMisses: Int;
  /**
   * Whether the directory tree index is built.
   * @zh 目录树索引是否已构建有效
   */
  treeIndexValid: boolean;
  /**
   * Roots in the tree index; `0` when it is not built.
   * @zh 目录树索引中的根目录数量；未构建时为 `0`
   */
  rootsCached: Int;
  /**
   * Tracks placed in the tree index at its last build.
   * @zh 目录树索引上次构建时归入的曲目总数
   */
  treeIndexedTracks: Int;
  /**
   * Tracks left out of the tree index at its last build.
   * @zh 目录树索引上次构建时跳过的曲目总数
   */
  treeSkippedTracks: Int;
  /**
   * When the tree index was last built, in milliseconds since the Unix epoch; `0` before the first
   * build.
   * @zh 目录树索引上次构建的时间戳（自 Unix 纪元起的毫秒数）；未构建时为 `0`
   */
  treeLastBuilt: Int;
}

// ---- getAll ----

interface GetAllParams {
  /**
   * Tracks to skip.
   * @zh 跳过的曲目偏移量
   * @default 0
   * @minimum 0
   */
  offset?: Int;
  /**
   * Most tracks to return.
   * @zh 最多返回的曲目数量上限
   * @default 100
   * @minimum 0
   */
  limit?: Int;
  /**
   * From `offset` 0, answer from the kept list when there is one. A request from `offset` 0 that
   * covers every track is kept either way.
   * @zh 从 `offset: 0` 开始时是否复用全库缓存；无论此项为何值，覆盖全库的起始请求均会被宿主缓存
   * @default true
   */
  useCache?: boolean;
  /**
   * Build a request from `offset` 0 that covers every track off the main thread and deliver it as
   * the `library:getAllResult` event; takes effect only with `useCache`, and not when the kept
   * list answers.
   * @zh 未命中缓存时是否在后台线程异步构建并通过 `library:getAllResult` 事件投递结果；仅在 `useCache: true` 时生效
   * @default false
   */
  asyncResult?: boolean;
}

interface GetAllResult {
  /**
   * `true` when the page will arrive as the `library:getAllResult` event; the page fields are then
   * absent.
   * @zh 结果是否将在后台线程异步构建并通过 `library:getAllResult` 事件投递；为 `true` 时不返回本页曲目字段
   */
  pending?: boolean;
  /**
   * Id the `library:getAllResult` event carries; present with `pending`.
   * @zh 异步结果事件所携带的匹配请求标识符；与 `pending: true` 配套返回
   */
  requestId?: string;
  /**
   * The page in library order; `index` is the position in the library.
   * @zh 当前分页曲目列表，按媒体库原生顺序排列；`index` 为在媒体库中的绝对位置
   */
  tracks?: LibraryTrack[];
  /**
   * The same list as `tracks`.
   * @zh 与 `tracks` 完全一致的兼容曲目列表
   */
  items?: LibraryTrack[];
  /**
   * Tracks in the library.
   * @zh 媒体库曲目总数
   */
  total?: Int;
  /**
   * The `offset` applied.
   * @zh 实际生效的分页起始偏移量
   */
  offset?: Int;
  /**
   * The `limit` applied.
   * @zh 实际生效的分页大小上限
   */
  limit?: Int;
  /**
   * `true` when the page came from the kept list.
   * @zh 当前返回结果是否直接命中全库缓存
   */
  fromCache?: boolean;
}

// ---- getAlbumTracks ----

interface GetAlbumTracksParams {
  /**
   * The row's opaque album identity. When provided, it takes precedence over both names;
   * empty or malformed strings fail rather than falling back to a name lookup. As for other
   * optional parameters, JSON null is treated as omitted.
   * @zh 专辑行的不透明唯一标识；提供时优先匹配且空串/畸形格式直接报错，不回退至名称查询；JSON null 视作未提供
   */
  albumId?: string;
  /**
   * The row's `name`: the first `album` value of its tracks, compared byte for byte.
   * @zh 专辑名称（对应曲目的首个 `album` 标签值，逐字节严格比对）
   */
  album: string;
  /**
   * The row's `albumArtist`: the first `album artist` value of its tracks, or their first `artist`
   * value when they have no `album artist`, or `""` when they have neither. Compared byte for
   * byte, so the row's `artist` is not a substitute.
   * @zh 专辑艺术家（对应曲目的首个 `album artist` 标签值，缺失时比对首个 `artist`，均无时为 `""`，逐字节严格比对）
   */
  albumArtist: string;
}

interface GetAlbumTracksResult {
  /**
   * The album name, as given.
   * @zh 请求传入的原样专辑名称
   */
  album: string;
  /**
   * The album artist, as given.
   * @zh 请求传入的原样专辑艺术家
   */
  albumArtist: string;
  /**
   * The album's row as `library.getAlbums` returns it, without `coverDataUrl` and `tracks`; absent
   * when no row has this name and album artist.
   * @zh 该专辑在 `library.getAlbums` 中的完整归组行（不含 `coverDataUrl` 与 `tracks`）；未匹配到对应专辑时不出现
   */
  row?: AlbumInfo;
  /**
   * The tracks, sorted by disc number, then track number, then library order; `index` is the
   * position in this list.
   * @zh 专辑曲目列表，依次按碟号、曲号及媒体库顺序排列；`index` 为在当前列表中的索引
   */
  tracks: LibraryTrack[];
  /**
   * The same list as `tracks`.
   * @zh 与 `tracks` 完全一致的兼容曲目列表
   */
  items: LibraryTrack[];
  /**
   * Number of tracks.
   * @zh 专辑包含的曲目总数
   */
  total: Int;
}

// ---- getArtistTracks ----

interface GetArtistTracksParams {
  /**
   * Artist name, compared byte for byte against each value of the `artist` tag; empty, the call
   * succeeds with no tracks.
   * @zh 目标艺术家名称，与曲目中的每个 `artist` 标签值逐字节精确比对；为空字符串时返回空列表
   * @default ""
   */
  artist?: string;
  /**
   * Most tracks to return; the first ones in library order are kept.
   * @zh 最多返回的曲目数量上限；按媒体库顺序取前若干首
   * @default 500
   * @minimum 0
   */
  limit?: Int;
}

interface GetArtistTracksResult {
  /**
   * The artist, as given.
   * @zh 请求传入的原样艺术家名称
   */
  artist: string;
  /**
   * The tracks in library order; `index` is the position in this list.
   * @zh 匹配的曲目列表，按媒体库顺序排列；`index` 为在当前列表中的索引
   */
  tracks: LibraryTrack[];
  /**
   * The same list as `tracks`.
   * @zh 与 `tracks` 完全一致的兼容曲目列表
   */
  items: LibraryTrack[];
  /**
   * The same as `count`; there is no count before `limit`, so a full page means there may be
   * more.
   * @zh 实际返回的曲目总数（与 `count` 相同，不提供 `limit` 截断前的总数）
   */
  total: Int;
  /**
   * Tracks returned.
   * @zh 实际返回的曲目数量
   */
  count: Int;
}

// ---- getRandomTracks ----

interface GetRandomTracksParams {
  /**
   * Tracks to draw; at most the library size is returned.
   * @zh 随机抽取的曲目数量上限；最多返回媒体库曲目总数
   * @default 10
   * @minimum 0
   */
  count?: Int;
}

interface GetRandomTracksResult {
  /**
   * The drawn tracks; `index` is the position in this list.
   * @zh 随机抽取的曲目列表；`index` 为在当前列表中的索引
   */
  tracks: LibraryTrack[];
  /**
   * Tracks returned.
   * @zh 实际返回的曲目数量
   */
  count: Int;
}

// ---- getRecentlyAdded ----

interface GetRecentlyAddedParams {
  /**
   * Most tracks to return.
   * @zh 最多返回的曲目数量上限
   * @default 50
   * @minimum 0
   */
  limit?: Int;
  /**
   * `added` orders by `%added%`, newest first, with tracks that lack it last; `modified` orders
   * by file modification time, newest first.
   * @zh 排序依据：`added` 按 `%added%` 时间降序（缺失曲目排在最后），`modified` 按文件物理修改时间降序
   * @default "added"
   */
  sortBy?: 'added' | 'modified';
}

interface GetRecentlyAddedResult {
  /**
   * The tracks, newest first; `index` is the position in the library.
   * @zh 最近添加或修改的曲目列表（新曲在前）；`index` 为在媒体库中的绝对位置
   */
  tracks: RecentLibraryTrack[];
  /**
   * Tracks in the library, not the number returned.
   * @zh 媒体库曲目总数（非本次返回数量）
   */
  total: Int;
  /**
   * The `limit` applied.
   * @zh 实际生效的分页大小上限
   */
  limit: Int;
  /**
   * The order used: `modified` when `added` was asked for and no track has `%added%`.
   * @zh 最终实际采用的排序依据；若请求 `added` 但全库曲目均无 `%added%` 记录则回退为 `modified`
   */
  sortBy: 'added' | 'modified';
  /**
   * Whether the call fell back from `added` to `modified`.
   * @zh 是否因全库缺失 `%added%` 字段而自动降级为 `modified` 排序
   */
  fallback: boolean;
}

// ---- browseTree ----

interface BrowseTreeParams {
  /**
   * `id` of a root from `library.getRoots`, compared case-insensitively.
   * @zh 根目录唯一标识（取自 `library.getRoots` 返回的 `id`，不区分大小写比对）
   * @minLength 1
   */
  rootId: string;
  /**
   * `pathId` of the folder to open, as a `directories` entry reports it; empty opens the root.
   * Folder names are separated by `/`, so a path written with `\` is not found.
   * @zh 待浏览的目标文件夹 `pathId`（以 `/` 分隔各级子目录，不支持 `\` 分隔符）；为空字符串时打开根目录
   * @default ""
   */
  pathId?: string;
  /**
   * Add the folder's tracks as `files`.
   * @zh 是否在返回数据中附带当前目录下的曲目列表 `files`
   * @default false
   */
  includeFiles?: boolean;
  /**
   * With `includeFiles`, add the tracks of every folder below as well.
   * @zh 配合 `includeFiles` 使用，是否递归包含所有层级子目录中的全部曲目
   * @default false
   */
  recursiveFiles?: boolean;
}

interface BrowseTreeResult {
  /**
   * The root.
   * @zh 所在根目录的配置信息
   */
  root: LibraryRootInfo;
  /**
   * The `pathId` opened.
   * @zh 当前浏览的目标文件夹 `pathId`
   */
  pathId: string;
  /**
   * Local path of the folder.
   * @zh 文件夹的规范化本地物理路径
   */
  absolutePath: string;
  /**
   * Subfolders directly inside, ordered by `displayName` and then `absolutePath`, ignoring case.
   * @zh 直接下属的子文件夹列表，依次按 `displayName` 与 `absolutePath` 忽略大小写排序
   */
  directories: LibraryDirectoryNodeInfo[];
  /**
   * The folder's tracks in library order, followed with `recursiveFiles` by those of the folders
   * below in no fixed order; empty without `includeFiles`. `index` is the position in the library
   * when the tree index was built.
   * @zh 包含的曲目列表，按媒体库顺序排列（启用 `recursiveFiles` 时后接子目录曲目，顺序不固定）；未指定 `includeFiles` 时为空列表；`index` 为构建索引时的媒体库位置
   */
  files: LibraryTrack[];
  /**
   * `true` when the tree index already existed before this call.
   * @zh 是否直接命中构建完毕的目录树索引缓存
   */
  fromCache: boolean;
}

// ---- browseDirectory ----

interface BrowseDirectoryParams {
  /**
   * Path prefix, compared with ASCII letters case-insensitive; empty matches every track.
   * @zh 路径前缀（ASCII 字母不区分大小写）；为空字符串时匹配全库曲目
   * @default ""
   */
  path?: string;
  /**
   * Add every track under `path` as `files`. On by default, so an empty `path` returns the whole
   * library.
   * @zh 是否在返回数据中附带匹配路径下的曲目列表 `files`；默认开启，`path` 为空时将返回全库曲目
   * @default true
   */
  includeFiles?: boolean;
}

interface BrowseDirectoryResult {
  /**
   * Folders one level below `path`, as stored paths, in byte order.
   * @zh `path` 下一层的子文件夹路径列表（foobar2000 存储格式，按字节序排列）
   */
  directories: string[];
  /**
   * The same list as `directories`.
   * @zh 与 `directories` 完全一致的兼容目录列表
   */
  items: string[];
  /**
   * Tracks under `path` in library order; `index` is the position in the library. Empty without
   * `includeFiles`.
   * @zh 匹配路径下的曲目列表，按媒体库顺序排列；未指定 `includeFiles` 时为空列表；`index` 为在媒体库中的绝对位置
   */
  files: LibraryTrack[];
}

// ---- query ----

interface QueryParams {
  /**
   * foobar2000 query expression.
   * @zh foobar2000 检索查询表达式（不支持附带 `SORT BY` 子句）
   * @minLength 1
   */
  query: string;
  /**
   * Title Formatting expression to sort the matches by before `limit` applies; empty keeps library
   * order, and an expression that fails to compile is ignored.
   * @zh 用于在应用 `limit` 截断前进行排序的 Title Formatting 表达式；为空字符串时保持媒体库顺序，语法编译失败时忽略该项
   * @default ""
   */
  sort?: string;
  /**
   * Most rows to return; `total` still counts every match.
   * @zh 最多返回的曲目行数上限；`total` 依然统计全部匹配数
   * @default 100
   * @minimum 0
   */
  limit?: Int;
  /**
   * Keys each row carries; omitted, every key of a library track row. Names are matched
   * case-sensitively against those keys: `index`, `handle`, `title`, `artist`, `artists`, `album`,
   * `albumId`, `albumArtist`, `albumArtists`, `genre`, `date`, `trackNumber`, `discNumber`, `duration`,
   * `path`, `absolutePath`, `fileSize`, `bitrate`, `sampleRate`, `channels`, `codec`, `subsong`
   * and `rating`. An unknown name fails with `INVALID_PARAMS` and is listed in
   * `details.unknownFields`. The optional `albumId` is omitted when the track has none, even
   * when requested.
   * @zh 每行曲目返回的字段白名单（严格区分大小写）；省略时返回曲目所有字段；传入未知字段名以 `INVALID_PARAMS` 失败（列入 `details.unknownFields`）；无 `albumId` 的曲目即使请求了该键也会自动省略
   * @minItems 1
   */
  fields?: string[];
}

interface QueryResult {
  /**
   * The matches, sorted by `sort` or in library order; `index` is the position in this list. Each
   * row carries the `fields` asked for; `albumId` is omitted when unavailable.
   * @zh 匹配的曲目列表，按 `sort` 或媒体库顺序排列；`index` 为在当前结果列表中的位置；各行仅包含 `fields` 要求的字段
   */
  tracks: Partial<LibraryTrack>[];
  /**
   * Matches, before `limit`.
   * @zh 满足查询条件的曲目总数（`limit` 截断前）
   */
  total: Int;
}

// ---- search ----

interface SearchParams {
  /**
   * foobar2000 query expression; empty, the call succeeds with no tracks.
   * @zh foobar2000 检索查询表达式；为空字符串时直接返回空列表
   * @default ""
   */
  query?: string;
  /**
   * Matches to skip.
   * @zh 跳过的匹配曲目偏移量
   * @default 0
   * @minimum 0
   */
  offset?: Int;
  /**
   * Most rows to return.
   * @zh 本页返回的曲目行数上限
   * @default 100
   * @minimum 0
   */
  limit?: Int;
  /**
   * Keys each row carries, as for `library.query`.
   * @zh 每行曲目返回的字段白名单（用法同 `library.query`）
   * @minItems 1
   */
  fields?: string[];
}

interface SearchResult {
  /**
   * This page of the matches in library order; `index` is the position among all matches. Each
   * row carries the `fields` asked for; `albumId` is omitted when unavailable.
   * @zh 当前分页匹配的曲目列表，按媒体库原生顺序排列；`index` 为在全部匹配曲目中的位置；各行仅包含 `fields` 要求的字段
   */
  tracks: Partial<LibraryTrack>[];
  /**
   * Matches, before `offset` and `limit`.
   * @zh 满足查询条件的匹配曲目总数（分页前）
   */
  total: Int;
  /**
   * The `offset` applied.
   * @zh 实际生效的分页起始偏移量
   */
  offset: Int;
  /**
   * The `limit` applied.
   * @zh 实际生效的分页大小上限
   */
  limit: Int;
  /**
   * Whether matches remain after this page.
   * @zh 当前页之后是否仍有未返回的匹配曲目
   */
  hasMore: boolean;
}

// ---- getChanges ----

interface GetChangesParams {
  /**
   * The `epoch` the page's copy was built under; omitted, the answer is a `resync`.
   * @zh 前端副本建立时的服务周期标识符 `epoch`；省略时直接触发全量同步（`resync: true`）
   */
  epoch?: string;
  /**
   * The `revision` the page's copy is at; omitted, the answer is a `resync`.
   * @zh 前端副本当前所处的修订版本号 `revision`；省略时直接触发全量同步（`resync: true`）
   * @minimum 0
   */
  since?: Int;
}

interface GetChangesResult {
  /**
   * Changes when the host starts its record over, for example after an abnormal shutdown; copies
   * from another epoch must be rebuilt.
   * @zh 宿主当前的记录周期标识；宿主异常退出或重置记录时会变更，前端持有旧 epoch 必须全量重建副本
   */
  epoch: string;
  /**
   * The revision this response brings the copy to; pass as `since` next time.
   * @zh 本次同步后最新达到的修订版本号；下次增量查询时作为 `since` 传入
   */
  revision: Int;
  /**
   * Whether the page must rebuild its copy from `handles`.
   * @zh 是否需要根据 `handles` 全量重建前端数据副本
   */
  resync: boolean;
  /**
   * Tracks added or modified after `since` that are still in the library; fetch via
   * `library.getTracks`. Absent when `resync` is true.
   * @zh 自 `since` 版本以来新增或修改且当前仍在库中的曲目 handle 列表（可调用 `library.getTracks` 获取详情）；全量同步（`resync: true`）时缺省
   */
  changed?: string[];
  /**
   * Tracks removed from the library after `since`. Absent when `resync` is true.
   * @zh 自 `since` 版本以来从媒体库中移除的曲目 handle 列表；全量同步（`resync: true`）时缺省
   */
  removed?: string[];
  /**
   * All tracks currently in the library. Absent when `resync` is false.
   * @zh 当前媒体库的全量曲目 handle 列表；增量同步（`resync: false`）时缺省
   */
  handles?: string[];
}

// ---- getTracks ----

interface GetTracksParams {
  /**
   * Track handles to look up, as given by a track row's `handle`; at most 5000.
   * @zh 待批量查询的曲目 handle 列表（最多 5000 项）
   */
  handles: string[];
  /**
   * Keys to include in each row, as in `library.query`.
   * @zh 每行曲目返回的字段白名单（用法同 `library.query`）
   * @minItems 1
   */
  fields?: string[];
}

interface GetTracksResult {
  /**
   * Found rows in the order of `handles`; `index` is the position in this list. Contains
   * requested `fields`; `albumId` omitted when unavailable.
   * @zh 查找到的曲目列表，保持与输入 `handles` 严格一致的顺序；`index` 为在当前返回列表中的位置；各行仅包含 `fields` 要求的字段
   */
  tracks: Partial<LibraryTrack>[];
  /**
   * Handles not found in the library, in the order given.
   * @zh 未在媒体库中找到或媒体库禁用时的 handle 列表，保持输入顺序
   */
  missing: string[];
}
