import type { Int, PlaylistGuid, Track } from './common.js';

export interface Api {
  /**
   * Whether the foobar2000 media library is enabled, that is, whether any library folder is
   * configured.
   * @zh foobar2000 媒体库是否启用，即是否配置了媒体库文件夹。
   */
  isEnabled(): IsEnabledResult;

  /**
   * Aggregate counts over the whole library. With the library disabled every count is `0`. The
   * library is walked on every call.
   * @zh 整个媒体库的汇总计数。媒体库未启用时各计数都是 `0`。每次调用都会遍历整个媒体库。
   * @effect read
   */
  getStats(): GetStatsResult;

  /**
   * Drop the host's cached library results and the directory tree index; the next call to an
   * endpoint that uses them rebuilds them. They are also dropped whenever the library changes.
   * @zh 丢弃宿主缓存的媒体库结果与目录树索引，下次调用用到它们的端点时重建。媒体库发生变化时也会自动丢弃。
   */
  invalidateCache(): InvalidateCacheResult;

  /**
   * Every genre in the library with its track count, ordered by name. Every value of a
   * multi-value `genre` gets its own entry, and a track tagged with several genres is counted
   * under each. Fails with `LIBRARY_DISABLED` when the library is disabled.
   * @zh 媒体库里的全部流派及各自的曲目数，按名称排序。多值 `genre` 的每个值各成一个条目，标了多个流派的曲目会计进其中每一个。媒体库未启用时以 `LIBRARY_DISABLED` 失败。
   */
  getGenres(): GetGenresResult;

  /**
   * Ask foobar2000 to rescan the library folders by calling `library_manager::rescan()`, which
   * the foobar2000 SDK marks as obsolete and not to be called. The library follows file changes
   * on its own.
   * @zh 调用 `library_manager::rescan()` 请求 foobar2000 重新扫描媒体库文件夹；foobar2000 SDK 已把该方法标为过时、不应调用。媒体库本身会跟踪文件变化。
   */
  rescan(): void;

  /**
   * Same as `library.rescan`.
   * @zh 同 `library.rescan`。
   */
  refresh(): void;

  /**
   * Append tracks to a playlist by path. A path does not have to be in the library, and a file
   * that does not exist is added all the same. A locked playlist fails with `LOCKED` and nothing
   * is added.
   * @zh 按路径把曲目追加到播放列表。路径不必在媒体库里，不存在的文件也照样添加。已上锁的列表以 `LOCKED` 失败，不添加任何曲目。
   */
  addToPlaylist(params: AddToPlaylistParams): AddToPlaylistResult;

  /**
   * The albums an artist appears on. `trackCount`, `duration` and `discCount` count only the
   * tracks of this artist, so an album the artist appears on only partly reports smaller figures
   * than in `library.getAlbums`. Rows are grouped by album name alone; tracks without an `album`
   * tag fall under `(Unknown Album)`. Fails with `LIBRARY_DISABLED` when the library is disabled.
   * @zh 某位艺术家参与的专辑。`trackCount`、`duration` 与 `discCount` 只统计该艺术家的曲目，所以该艺术家只参与了一部分的专辑，数字会比 `library.getAlbums` 给的小。行只按专辑名分组；没有 `album` 标签的曲目归入 `(Unknown Album)`。媒体库未启用时以 `LIBRARY_DISABLED` 失败。
   */
  getArtistAlbums(params: GetArtistAlbumsParams): GetArtistAlbumsResult;

  /**
   * Every distinct value of one tag across the library with its track count, most used first.
   * Fails with `LIBRARY_DISABLED` when the library is disabled.
   * @zh 某个标签在整个媒体库里的全部不同取值及各自的曲目数，用得最多的在前。媒体库未启用时以 `LIBRARY_DISABLED` 失败。
   */
  getFieldValues(params: GetFieldValuesParams): GetFieldValuesResult;

  /**
   * The library's root folders. Only tracks that resolve to a stable local path count toward
   * the roots; `http://`, `file-relative://`, `unpack://`, `archive://` and similar ones are
   * counted in `skippedTracks`. The first call builds the index synchronously and later calls
   * reuse it until the library changes or `library.invalidateCache` is called. With the library
   * disabled it succeeds with `enabled: false` and no roots; a failed build fails with
   * `OPERATION_FAILED`.
   * @zh 媒体库的根文件夹。只有能解析为稳定本地路径的曲目才计入根目录；`http://`、`file-relative://`、`unpack://`、`archive://` 等计入 `skippedTracks`。首次调用同步构建索引，之后复用，直到媒体库变化或调用 `library.invalidateCache`。媒体库未启用时成功返回 `enabled: false` 与空根目录；构建失败以 `OPERATION_FAILED` 失败。
   */
  getRoots(): GetRootsResult;

  /**
   * Number of tracks in the library.
   * @zh 媒体库里的曲目数。
   */
  getCount(): GetCountResult;

  /**
   * Look up one file in the library and return its main fields as a flat object. A file that is
   * not in the library succeeds with `found: false`.
   * @zh 在媒体库中查找一个文件，以扁平对象返回它的主要字段。不在媒体库里的文件成功返回 `found: false`。
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
   * @zh 整个媒体库的专辑，按专辑名加专辑艺术家分组；没有 `album` 标签的曲目跳过。完整的列表（`offset` 为 0、不带 `includeTracks`、全部专辑都在 `limit` 之内）会保留到媒体库变化为止，之后 `query`、`sort`、`includeCover` 相同、从 `offset` 0 开始且不带 `includeTracks` 的调用直接用它回答。列表背后的归组结果同样保留，并与 `library.getAlbumTracks` 共用，所以媒体库变化之前，其他调用也不会再读一遍媒体库。媒体库未启用时成功返回空列表。
   * @effect read
   */
  getAlbums(params: GetAlbumsParams): GetAlbumsResult;

  /**
   * Every credited artist with participation counts: each value of a multi-value `artist` gets
   * its own row. The scan is kept until the library changes. Fails with `LIBRARY_DISABLED` when
   * the library is disabled.
   * @zh 每位署名艺术家及其参与计数：多值 `artist` 的每个值各成一行。扫描结果保留到媒体库变化为止。媒体库未启用时以 `LIBRARY_DISABLED` 失败。
   * @effect read
   */
  getArtists(params: GetArtistsParams): GetArtistsResult;

  /**
   * Whether the library is enabled and how many tracks it holds. The answer is kept until the
   * library changes; with the library disabled nothing is kept.
   * @zh 媒体库是否启用及曲目数。结果保留到媒体库变化为止；媒体库未启用时不保留。
   */
  getStatus(): GetStatusResult;

  /**
   * Counters of the host's library caches and of the directory tree index, for diagnostics.
   * @zh 宿主媒体库缓存与目录树索引的计数，供诊断用。
   */
  getCacheStats(): GetCacheStatsResult;

  /**
   * Tracks of the whole library in library order, one page at a time. A request from `offset` 0
   * that covers every track is kept until the library changes, and a later call from `offset` 0
   * with `useCache` is answered from it. With `asyncResult`, such a request, when it is not
   * answered from the kept list, is built off the main thread: the call answers
   * `{ pending: true, requestId }` at once and the page arrives as the `library:getAllResult`
   * event on the calling window.
   * @zh 整个媒体库的曲目，按媒体库顺序分页返回。从 `offset` 0 开始且覆盖全部曲目的请求会保留到媒体库变化为止，之后带 `useCache` 从 `offset` 0 开始的调用直接用它回答。带 `asyncResult` 时，这样的请求若没有用保留的列表回答，就在主线程之外构建：调用立即返回 `{ pending: true, requestId }`，这一页随后以 `library:getAllResult` 事件发到发起调用的窗口。
   */
  getAll(params: GetAllParams): GetAllResult;

  /**
   * The tracks of one `library.getAlbums` row, named by the row's `name` and `albumArtist`. Tracks
   * are grouped exactly as `library.getAlbums` groups them, so `total` equals the row's
   * `trackCount`. They are sorted by disc number, then track number, then library order. The
   * grouping is kept until the library changes and `library.getAlbums` builds and reads the same
   * one, so a call after it, or after an earlier call, does not read the library again. A name
   * and album artist that no row has, and a disabled library, succeed with no tracks and no
   * `row`.
   * @zh 一行 `library.getAlbums` 专辑的曲目，用该行的 `name` 与 `albumArtist` 指定。曲目的归组与 `library.getAlbums` 完全相同，所以 `total` 等于该行的 `trackCount`。排序先按碟号，再按曲号，再按媒体库顺序。归组结果保留到媒体库变化为止，`library.getAlbums` 建立并读取的也是这一份，所以在它之后或前一次调用之后，不必再读一遍媒体库。没有哪一行是这个名称与专辑艺术家时，以及媒体库未启用时，都成功返回空列表，且没有 `row`。
   */
  getAlbumTracks(params: GetAlbumTracksParams): GetAlbumTracksResult;

  /**
   * Tracks an artist is credited on, in library order. `artist` is matched as
   * `library.getArtistAlbums` matches it under `match: 'exact'`. An empty `artist` and a disabled
   * library both succeed with no tracks.
   * @zh 某位艺术家署名的曲目，按媒体库顺序。`artist` 的匹配方式与 `library.getArtistAlbums` 在 `match: 'exact'` 下相同。`artist` 为空与媒体库未启用时都成功返回空列表。
   */
  getArtistTracks(params: GetArtistTracksParams): GetArtistTracksResult;

  /**
   * Tracks drawn at random from the whole library without repeats, a new draw on every call. A
   * disabled or empty library succeeds with no tracks.
   * @zh 从整个媒体库随机抽取、互不重复的曲目，每次调用重新抽取。媒体库未启用或为空时成功返回空列表。
   */
  getRandomTracks(params: GetRandomTracksParams): GetRandomTracksResult;

  /**
   * The newest tracks of the library. `added` orders by the `%added%` field of foo_playcount; when
   * no track has it, the call orders by file modification time instead and says so in `fallback`.
   * The library is walked on every call.
   * @zh 媒体库里最新的曲目。`added` 按 foo_playcount 的 `%added%` 字段排序；没有任何曲目带这个字段时改按文件修改时间排序，并以 `fallback` 说明。每次调用都会遍历整个媒体库。
   */
  getRecentlyAdded(params: GetRecentlyAddedParams): GetRecentlyAddedResult;

  /**
   * One folder under a library root: its subfolders and, with `includeFiles`, its tracks. Reads
   * the directory tree index `library.getRoots` uses, building it first when needed. An unknown
   * `rootId` or `pathId` fails with `NOT_FOUND`; a failed index build fails with
   * `OPERATION_FAILED`.
   * @zh 媒体库根目录下的一个文件夹：它的子文件夹，带 `includeFiles` 时还有它的曲目。读的是 `library.getRoots` 所用的目录树索引，需要时先构建。未知的 `rootId` 或 `pathId` 以 `NOT_FOUND` 失败；索引构建失败以 `OPERATION_FAILED` 失败。
   */
  browseTree(params: BrowseTreeParams): BrowseTreeResult;

  /**
   * List the library by path prefix: the folders one level below `path` and, with `includeFiles`,
   * every track under it at any depth. The prefix is matched against the paths as foobar2000
   * stores them, so a local folder has to be written as `file://` followed by its path; a plain
   * absolute path matches nothing and succeeds with empty lists. `library.browseTree` walks the
   * real library roots instead. Fails with `LIBRARY_DISABLED` when the library is disabled.
   * @zh 按路径前缀列出媒体库：`path` 下一层的文件夹，带 `includeFiles` 时还有它下面任意深度的全部曲目。前缀与 foobar2000 存储的路径比较，所以本地文件夹要写成 `file://` 加路径；普通绝对路径什么也匹配不到，成功返回空列表。要按真实的媒体库根目录浏览，用 `library.browseTree`。媒体库未启用时以 `LIBRARY_DISABLED` 失败。
   */
  browseDirectory(params: BrowseDirectoryParams): BrowseDirectoryResult;

  /**
   * Tracks matching a foobar2000 query, optionally sorted by `sort`, up to `limit`. A query the
   * parser rejects fails with `INVALID_PARAMS` and `details.param` `query`, and so does one
   * carrying `SORT BY`; many malformed queries are not rejected but simply match nothing. The rows
   * are written off the main thread. Fails with `LIBRARY_DISABLED` when the library is disabled.
   * @zh 符合 foobar2000 查询表达式的曲目，可按 `sort` 排序，最多 `limit` 行。解析器拒绝的表达式以 `INVALID_PARAMS` 失败并带 `details.param` 为 `query`，带 `SORT BY` 的同样如此；很多畸形表达式并不会被拒绝，只是什么也匹配不到。各行在主线程之外写出。媒体库未启用时以 `LIBRARY_DISABLED` 失败。
   */
  query(params: QueryParams): QueryResult;

  /**
   * One page of the tracks matching a foobar2000 query, in library order: a `SORT BY` clause is
   * accepted but not applied. A query the parser rejects fails with `INVALID_PARAMS` and
   * `details.param` `query`. An empty `query` and a disabled library both succeed with no tracks.
   * The rows are written off the main thread.
   * @zh 符合 foobar2000 查询表达式的曲目中的一页，按媒体库顺序：`SORT BY` 子句会被接受但不生效。解析器拒绝的表达式以 `INVALID_PARAMS` 失败并带 `details.param` 为 `query`。`query` 为空与媒体库未启用时都成功返回空列表。各行在主线程之外写出。
   * @effect read
   */
  search(params: SearchParams): SearchResult;
}

export interface Events {
  /**
   * foobar2000 reported that the Media Library has finished initializing. The host drops its
   * library caches before sending it. Every window receives it; a page that loads afterwards
   * does not.
   * @zh foobar2000 报告媒体库已初始化完成。宿主先清掉自己的媒体库缓存再发。所有窗口都会收到；之后才加载的页面收不到。
   * @delivery broadcast
   */
  initialized: InitializedPayload;

  /**
   * Tracks were added to the Media Library. The host drops its library caches before sending it.
   * @zh 媒体库里加入了曲目。宿主先清掉自己的媒体库缓存再发。
   * @delivery broadcast
   */
  itemsAdded: ItemsChangedPayload;

  /**
   * Tracks were removed from the Media Library. The host drops its library caches before sending
   * it.
   * @zh 媒体库里移除了曲目。宿主先清掉自己的媒体库缓存再发。
   * @delivery broadcast
   */
  itemsRemoved: ItemsChangedPayload;

  /**
   * Tracks in the Media Library were modified, for example by a tag edit. The host drops its
   * library caches before sending it.
   * @zh 媒体库里的曲目被修改了，例如改了标签。宿主先清掉自己的媒体库缓存再发。
   * @delivery broadcast
   */
  itemsModified: ItemsChangedPayload;

  /**
   * The page of a `library.getAll` that answered `{ pending: true, requestId }`: the whole library
   * in library order, built off the main thread. The list is kept for later calls only when the
   * library did not change meanwhile. Sent to the page that made the call, or to a page under the
   * same top-level window when that one cannot be found, or else to the main window's page.
   * @zh 回答了 `{ pending: true, requestId }` 的 `library.getAll` 的这一页：在主线程之外构建的整个媒体库，按媒体库顺序。只有构建期间媒体库没有变化，这份列表才保留给之后的调用。发给发起调用的页面；找不到它时发给同一顶层窗口下的页面，再没有就发给主窗口的页面。
   * @delivery caller
   */
  getAllResult: GetAllResultPayload;
}

interface InitializedPayload {
  /**
   * When the host sent it, in milliseconds since the Unix epoch.
   * @zh 宿主发出它的时间，自 Unix 纪元起的毫秒数。
   */
  timestamp: Int;
}

interface GetAllResultPayload {
  /**
   * Id from the `{ pending: true, requestId }` answer.
   * @zh `{ pending: true, requestId }` 回答里的 id。
   */
  requestId: string;
  /**
   * The whole library in library order; `index` is the position in the library. Empty when
   * `error` is present.
   * @zh 整个媒体库，按媒体库顺序；`index` 是在媒体库中的位置。带 `error` 时为空。
   */
  tracks: LibraryTrack[];
  /**
   * The same list as `tracks`.
   * @zh 与 `tracks` 相同的列表。
   */
  items: LibraryTrack[];
  /**
   * Tracks in the library when the call was made; `0` when `error` is present.
   * @zh 调用时媒体库的曲目数；带 `error` 时为 `0`。
   */
  total: Int;
  /**
   * The `offset` of the call, always `0`.
   * @zh 调用的 `offset`，恒为 `0`。
   */
  offset: Int;
  /**
   * The `limit` of the call.
   * @zh 调用的 `limit`。
   */
  limit: Int;
  /**
   * Always `false`.
   * @zh 恒为 `false`。
   */
  fromCache: boolean;
  /**
   * Why the list could not be built; present only then.
   * @zh 列表构建失败的原因；只在失败时出现。
   */
  error?: string;
}

interface ItemsChangedPayload {
  /**
   * How many tracks.
   * @zh 曲目数。
   */
  count: Int;
  /**
   * When the host sent it, in milliseconds since the Unix epoch.
   * @zh 宿主发出它的时间，自 Unix 纪元起的毫秒数。
   */
  timestamp: Int;
}

/**
 * One value of a tag and the number of tracks carrying it.
 * @zh 标签的一个取值及带有它的曲目数。
 */
interface LibraryValueCount {
  /**
   * The value.
   * @zh 取值。
   */
  name: string;
  /**
   * Tracks carrying the value.
   * @zh 带有该取值的曲目数。
   */
  trackCount: Int;
}

/**
 * One album row, as `library.getAlbums` and `library.getArtistAlbums` return it; the `row` of
 * `library.getAlbumTracks` has the same shape.
 * @zh 一行专辑，`library.getAlbums` 与 `library.getArtistAlbums` 按此形状返回；`library.getAlbumTracks` 的 `row` 也是这个形状。
 */
interface AlbumInfo {
  /**
   * Album name.
   * @zh 专辑名。
   */
  name: string;
  /**
   * `albumArtist` when it is set, otherwise the first `artist` value found among the tracks.
   * @zh `albumArtist` 有值时为它，否则为这些曲目里找到的第一个 `artist` 值。
   */
  artist: string;
  /**
   * The first `album artist` value found among the tracks, falling back to the track's `artist`;
   * empty when neither exists.
   * @zh 这些曲目里找到的第一个 `album artist` 值，没有时回退到曲目的 `artist`；两者都没有时为空。
   */
  albumArtist: string;
  /**
   * Tracks counted into the row.
   * @zh 计入该行的曲目数。
   */
  trackCount: Int;
  /**
   * Distinct disc numbers among those tracks; `1` when none carries one.
   * @zh 这些曲目里不同碟号的个数；都没有碟号时为 `1`。
   */
  discCount: Int;
  /**
   * Summed length of those tracks, in seconds.
   * @zh 这些曲目的总时长，单位秒。
   */
  duration: number;
  /**
   * The first `date` value found among the tracks; empty when none has one.
   * @zh 这些曲目里找到的第一个 `date` 值；都没有时为空。
   */
  year: string;
  /**
   * The first `genre` value found among the tracks; empty when none has one.
   * @zh 这些曲目里找到的第一个 `genre` 值；都没有时为空。
   */
  genre: string;
  /**
   * The first `publisher` value found among the tracks, or else the first `label` value; empty
   * when none has either.
   * @zh 这些曲目里找到的第一个 `publisher` 值，没有时取第一个 `label` 值；都没有时为空。
   */
  label: string;
  /**
   * Path of the first track counted, as foobar2000 stores it; it can be a `file-relative://`
   * URI.
   * @zh 计入的第一首曲目的路径（foobar2000 存储的形式），可能是 `file-relative://` URI。
   */
  firstTrackPath: string;
  /**
   * `firstTrackPath` as a native file path, the form to pass to `artwork.getForTrack`; absent
   * when there is no first track path.
   * @zh `firstTrackPath` 的本地文件路径形式，交给 `artwork.getForTrack` 的应是它；没有首曲路径时不出现。
   */
  firstTrackAbsolutePath?: string;
  /**
   * Front cover as a `data:image/...` URL; only `library.getAlbums` with `includeCover` fills it,
   * and only when a cover exists.
   * @zh 正面封面的 `data:image/...` URL；只有带 `includeCover` 的 `library.getAlbums` 会填，且仅在有封面时。
   */
  coverDataUrl?: string;
  /**
   * The album's tracks in track-number order; only `library.getAlbums` with `includeTracks`
   * fills it.
   * @zh 专辑的曲目，按曲号排序；只有带 `includeTracks` 的 `library.getAlbums` 会填。
   */
  tracks?: AlbumTrackRef[];
}

/**
 * One track of an album row.
 * @zh 专辑行里的一首曲目。
 */
interface AlbumTrackRef {
  /**
   * Track number; `0` when the track has none.
   * @zh 曲号；没有时为 `0`。
   */
  trackNumber: Int;
  /**
   * Track path, as foobar2000 stores it.
   * @zh 曲目路径（foobar2000 存储的形式）。
   */
  path: string;
  /**
   * `path` as a native file path.
   * @zh `path` 的本地文件路径形式。
   */
  absolutePath: string;
}

/**
 * One library root folder.
 * @zh 一个媒体库根文件夹。
 */
interface LibraryRootInfo {
  /**
   * Stable identifier of the root, currently its `absolutePath`; `library.browseTree` takes it.
   * @zh 根目录的稳定标识，当前就是 `absolutePath`；`library.browseTree` 用它。
   */
  id: string;
  /**
   * The folder name, or the full path when two roots share a name.
   * @zh 文件夹名；两个根目录同名时为完整路径。
   */
  displayName: string;
  /**
   * Currently the same as `absolutePath`.
   * @zh 当前与 `absolutePath` 相同。
   */
  rawPath: string;
  /**
   * Canonical local path of the folder.
   * @zh 文件夹的规范化本地路径。
   */
  absolutePath: string;
  /**
   * Library tracks under the folder.
   * @zh 该文件夹下的媒体库曲目数。
   */
  trackCount: Int;
}

/**
 * One track row of a library list: the shared track fields plus `index`.
 * @zh 媒体库列表里的一行曲目：共用的曲目字段加 `index`。
 */
interface LibraryTrack extends Track {
  /**
   * Row number; the list the row is in says what it counts.
   * @zh 行号；数的是什么由所在的列表说明。
   */
  index: Int;
}

/**
 * One row of `library.getRecentlyAdded`.
 * @zh `library.getRecentlyAdded` 的一行。
 */
interface RecentLibraryTrack extends LibraryTrack {
  /**
   * The `%added%` value as foo_playcount formats it, such as `2024-05-01 12:34:56`; present only
   * when the list is ordered by it and the track has one.
   * @zh foo_playcount 格式化的 `%added%` 值，如 `2024-05-01 12:34:56`；只在列表按它排序且该曲目有值时出现。
   */
  added?: string;
  /**
   * File modification time, in seconds since the Unix epoch; present only when the list is
   * ordered by it and the time is known.
   * @zh 文件修改时间，自 Unix 纪元起的秒数；只在列表按它排序且时间已知时出现。
   */
  modified?: Int;
}

/**
 * One folder of the directory tree index.
 * @zh 目录树索引里的一个文件夹。
 */
interface LibraryDirectoryNodeInfo {
  /**
   * `rootId`, then `::`, then `pathId`.
   * @zh `rootId` 加 `::` 再加 `pathId`。
   */
  id: string;
  /**
   * The root the folder is under.
   * @zh 文件夹所在的根目录。
   */
  rootId: string;
  /**
   * Path below the root with `/` between folder names; pass it to `library.browseTree` to open
   * the folder.
   * @zh 相对根目录的路径，文件夹名之间用 `/` 分隔；传给 `library.browseTree` 即可打开这个文件夹。
   */
  pathId: string;
  /**
   * `pathId` of the parent folder; `""` directly under the root.
   * @zh 父文件夹的 `pathId`；直接位于根目录下时为 `""`。
   */
  parentPathId: string;
  /**
   * Folder name.
   * @zh 文件夹名。
   */
  name: string;
  /**
   * Currently the same as `name`.
   * @zh 当前与 `name` 相同。
   */
  displayName: string;
  /**
   * Currently the same as `absolutePath`.
   * @zh 当前与 `absolutePath` 相同。
   */
  rawPath: string;
  /**
   * Local path of the folder.
   * @zh 文件夹的本地路径。
   */
  absolutePath: string;
  /**
   * Currently the same as `pathId`.
   * @zh 当前与 `pathId` 相同。
   */
  relativePath: string;
  /**
   * Folder names in `pathId`: `1` directly under the root.
   * @zh `pathId` 里的文件夹层数：直接位于根目录下时为 `1`。
   */
  depth: Int;
  /**
   * Library tracks in the folder and every folder below it.
   * @zh 该文件夹及其下所有文件夹里的媒体库曲目数。
   */
  trackCount: Int;
  /**
   * Subfolders directly inside.
   * @zh 直接包含的子文件夹数。
   */
  childDirectoryCount: Int;
  /**
   * Whether `childDirectoryCount` is above `0`.
   * @zh `childDirectoryCount` 是否大于 `0`。
   */
  hasChildren: boolean;
}

// ---- isEnabled ----

interface IsEnabledResult {
  /**
   * Whether the media library is enabled.
   * @zh 媒体库是否启用。
   */
  enabled: boolean;
}

// ---- getStats ----

interface GetStatsResult {
  /**
   * Tracks in the library.
   * @zh 媒体库曲目数。
   */
  totalTracks: Int;
  /**
   * Albums, counted by album name plus album artist, as `library.getAlbums` groups them.
   * @zh 专辑数，按专辑名加专辑艺术家计，与 `library.getAlbums` 的分组一致。
   */
  totalAlbums: Int;
  /**
   * Credited artists: every value of a multi-value `artist` counts, matching the entry count of
   * `library.getArtists`.
   * @zh 参与艺术家数：多值 `artist` 的每个值都计入，与 `library.getArtists` 的条目数一致。
   */
  totalArtists: Int;
  /**
   * Summed length of all tracks, in seconds.
   * @zh 全部曲目的总时长，单位秒。
   */
  totalDuration: number;
  /**
   * Summed file size, in bytes.
   * @zh 文件总大小，单位字节。
   */
  totalSize: Int;
  /**
   * Whether the host holds cached library results written since the cache was last dropped.
   * @zh 宿主是否持有自上次丢弃缓存以来写入的媒体库缓存结果。
   */
  cacheValid: boolean;
  /**
   * When the cache was last dropped, in milliseconds since the Unix epoch.
   * @zh 缓存上次被丢弃的时间，自 Unix 纪元起的毫秒数。
   */
  lastModified: Int;
}

// ---- invalidateCache ----

interface InvalidateCacheResult {
  /**
   * When the cache was dropped, in milliseconds since the Unix epoch.
   * @zh 缓存被丢弃的时间，自 Unix 纪元起的毫秒数。
   */
  timestamp: Int;
}

// ---- getGenres ----

interface GetGenresResult {
  /**
   * Every genre with its track count.
   * @zh 全部流派及各自的曲目数。
   */
  genres: LibraryValueCount[];
}

// ---- addToPlaylist ----

interface AddToPlaylistParams {
  /**
   * Paths to add, in this order; a `|subsong:N` suffix selects a subsong.
   * @zh 要添加的路径，按此顺序；`|subsong:N` 后缀选子曲目。
   * @minItems 1
   * @security MediaRead
   */
  paths: string[];
  /**
   * Index of the target playlist; omitted, the active playlist. An index past the last playlist
   * fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with
   * `NO_ACTIVE_ITEM`.
   * @zh 目标播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。
   * @minimum 0
   */
  playlist?: Int;
  playlistGuid?: PlaylistGuid;
}

interface AddToPlaylistResult {
  /**
   * Tracks added.
   * @zh 添加的曲目数。
   */
  added: Int;
}

// ---- getArtistAlbums ----

interface GetArtistAlbumsParams {
  /**
   * Artist name, compared byte for byte against each value of the `artist` tag, so a name from
   * `library.getArtists` matches even when it is not the first of several credited artists.
   * @zh 艺术家名，与 `artist` 标签的每个值逐字节比较，所以 `library.getArtists` 给出的名字即使不是多位艺术家中的第一位也能命中。
   * @minLength 1
   */
  artist: string;
  /**
   * Most albums to return, applied after grouping; there is no offset.
   * @zh 最多返回的专辑数，在分组之后截断；没有 offset。
   * @default 100
   * @minimum 0
   */
  limit?: Int;
  /**
   * `name`, `artist`, `year` or `trackCount`, as in `library.getAlbums`; any other value sorts by
   * name.
   * @zh `name`、`artist`、`year` 或 `trackCount`，与 `library.getAlbums` 相同；其他值按名称排序。
   * @default "name"
   */
  sort?: string;
  /**
   * `exact` needs a whole tag value to equal `artist`; `substring` matches a tag value that
   * contains it, and then a lowercase letter in `artist` matches either case while an uppercase
   * one matches only uppercase.
   * @zh `exact` 要求某个标签值整个等于 `artist`；`substring` 匹配包含它的标签值，此时 `artist` 里的小写字母匹配任意大小写，大写字母只匹配大写。
   * @default "exact"
   */
  match?: 'exact' | 'substring';
}

interface GetArtistAlbumsResult {
  /**
   * The artist, as given.
   * @zh 原样的艺术家名。
   */
  artist: string;
  /**
   * The albums, sorted by `sort`. Rows never carry `coverDataUrl` or `tracks`.
   * @zh 专辑，按 `sort` 排序。行里不会有 `coverDataUrl` 与 `tracks`。
   */
  albums: AlbumInfo[];
  /**
   * Albums found, before `limit`.
   * @zh 找到的专辑数（`limit` 截断之前）。
   */
  total: Int;
  /**
   * Whether `limit` cut the list short.
   * @zh `limit` 是否截掉了一部分。
   */
  hasMore: boolean;
}

// ---- getFieldValues ----

interface GetFieldValuesParams {
  /**
   * Tag name, such as `genre`.
   * @zh 标签名，如 `genre`。
   * @minLength 1
   */
  field: string;
  /**
   * Splits each tag value into several values at this string, trimming spaces and tabs around
   * each; empty, values are taken whole.
   * @zh 按此字符串把每个标签值再拆成多个值，并去掉各段前后的空格与制表符；为空时整值计入。
   * @default ""
   */
  separator?: string;
  /**
   * Most values to return.
   * @zh 最多返回的取值数。
   * @default 5000
   * @minimum 0
   */
  limit?: Int;
}

interface GetFieldValuesResult {
  /**
   * The values, most tracks first.
   * @zh 各取值，曲目数多的在前。
   */
  values: LibraryValueCount[];
  /**
   * Distinct values found, before `limit`.
   * @zh 找到的不同取值数（`limit` 截断之前）。
   */
  total: Int;
  /**
   * The tag name, as given.
   * @zh 原样的标签名。
   */
  field: string;
}

// ---- getRoots ----

interface GetRootsResult {
  /**
   * Whether the media library is enabled.
   * @zh 媒体库是否启用。
   */
  enabled: boolean;
  /**
   * The root folders.
   * @zh 根文件夹。
   */
  roots: LibraryRootInfo[];
  /**
   * Number of roots.
   * @zh 根目录数。
   */
  total: Int;
  /**
   * Tracks placed under a root.
   * @zh 归入某个根目录的曲目数。
   */
  indexedTracks: Int;
  /**
   * Tracks left out because they have no stable local path.
   * @zh 因没有稳定本地路径而跳过的曲目数。
   */
  skippedTracks: Int;
  /**
   * `true` when the index already existed before this call.
   * @zh 本次调用之前索引已存在时为 `true`。
   */
  fromCache: boolean;
}

// ---- getCount ----

interface GetCountResult {
  /**
   * Tracks in the library.
   * @zh 媒体库曲目数。
   */
  count: Int;
}

// ---- getByPath ----

interface GetByPathParams {
  /**
   * File path. It is looked up as the file's first subsong; a `|subsong:N` suffix is not
   * recognized.
   * @zh 文件路径。按文件的第一个子曲目查找，不认 `|subsong:N` 后缀。
   * @minLength 1
   * @security MediaRead
   */
  path: string;
}

interface GetByPathResult {
  /**
   * Whether the file is in the library; the other fields besides `path` are present only when
   * it is.
   * @zh 文件是否在媒体库里；除 `path` 外的其他字段只在找到时出现。
   */
  found: boolean;
  /**
   * The track path as foobar2000 stores it when found; otherwise the path as given.
   * @zh 找到时为 foobar2000 存储的曲目路径；否则为原样的路径。
   */
  path: string;
  /**
   * The track path as a native file path.
   * @zh 曲目路径的本地文件路径形式。
   */
  absolutePath?: string;
  /**
   * The first `title` value; empty when absent.
   * @zh 第一个 `title` 值；没有时为空。
   */
  title?: string;
  /**
   * Every `artist` value joined with `, ` in their original order.
   * @zh 全部 `artist` 值按原顺序以 `, ` 连接。
   */
  artist?: string;
  /**
   * Every `artist` value; `artists.join(', ')` equals `artist`.
   * @zh 全部 `artist` 值；`artists.join(', ')` 等于 `artist`。
   */
  artists?: string[];
  /**
   * The first `album` value; empty when absent.
   * @zh 第一个 `album` 值；没有时为空。
   */
  album?: string;
  /**
   * Length in seconds.
   * @zh 时长，单位秒。
   */
  duration?: number;
  /**
   * The first `tracknumber` value as written in the tag, such as `2` or `02/12`; empty when
   * absent.
   * @zh 第一个 `tracknumber` 值，保持标签里的写法（如 `2` 或 `02/12`）；没有时为空。
   */
  trackNumber?: string;
  /**
   * Every `genre` value joined with `, `.
   * @zh 全部 `genre` 值以 `, ` 连接。
   */
  genre?: string;
  /**
   * The first `date` value; empty when absent.
   * @zh 第一个 `date` 值；没有时为空。
   */
  date?: string;
}

// ---- getAlbums ----

interface GetAlbumsParams {
  /**
   * `name`, `artist` (the album artist), `year` or `trackCount`; any other value sorts by name.
   * @zh `name`、`artist`（专辑艺术家）、`year` 或 `trackCount`；其他值按名称排序。
   * @default "name"
   */
  sort?: string;
  /**
   * Keep only albums whose name or artist contains this text, compared case-insensitively for
   * ASCII letters; empty keeps every album.
   * @zh 只保留名称或艺术家包含这段文本的专辑，ASCII 字母不区分大小写；为空时保留全部。
   * @default ""
   */
  query?: string;
  /**
   * Albums to skip.
   * @zh 跳过的专辑数。
   * @default 0
   * @minimum 0
   */
  offset?: Int;
  /**
   * Most albums to return.
   * @zh 最多返回的专辑数。
   * @default 100
   * @minimum 0
   */
  limit?: Int;
  /**
   * Add each album's tracks as `tracks`.
   * @zh 为每张专辑附上 `tracks`。
   * @default false
   */
  includeTracks?: boolean;
  /**
   * Add each album's front cover as `coverDataUrl`, read from its first track.
   * @zh 为每张专辑附上 `coverDataUrl`（取自首曲的正面封面）。
   * @default false
   */
  includeCover?: boolean;
  /**
   * Largest cover to inline, in KiB; a larger cover is left out. `0` or less inlines any size.
   * @zh 内联封面的上限，单位 KiB；更大的封面不附带。`0` 或负数不限大小。
   * @default 500
   */
  coverMaxSize?: Int;
  /**
   * Answer from the kept list when there is one; `false` always scans the library, and the
   * result is still kept.
   * @zh 有保留的列表时直接用它回答；为 `false` 时总是扫描媒体库，结果照样保留。
   * @default true
   */
  useCache?: boolean;
}

interface GetAlbumsResult {
  /**
   * The albums of this page, sorted by `sort`.
   * @zh 这一页的专辑，按 `sort` 排序。
   */
  albums: AlbumInfo[];
  /**
   * Albums matching `query`, before `offset` and `limit`.
   * @zh 符合 `query` 的专辑数（`offset` 与 `limit` 之前）。
   */
  total: Int;
  /**
   * The `offset` applied.
   * @zh 生效的 `offset`。
   */
  offset: Int;
  /**
   * The `limit` applied.
   * @zh 生效的 `limit`。
   */
  limit: Int;
  /**
   * Whether albums remain after this page.
   * @zh 这一页之后是否还有专辑。
   */
  hasMore: boolean;
  /**
   * The `includeCover` applied.
   * @zh 生效的 `includeCover`。
   */
  includeCover: boolean;
  /**
   * `true` when the answer came from the kept list.
   * @zh 结果来自保留的列表时为 `true`。
   */
  fromCache: boolean;
}

// ---- getArtists ----

/**
 * One artist row of `library.getArtists`. `albumCount` counts album names, while `albums` keeps
 * albums of the same name by different album artists apart, so the two can differ.
 * @zh `library.getArtists` 的一行。`albumCount` 按专辑名计数，而 `albums` 会把同名但专辑艺术家不同的专辑分开，所以两者可能不一致。
 */
interface ArtistInfo {
  /**
   * The artist.
   * @zh 艺术家名。
   */
  name: string;
  /**
   * Distinct album names among the artist's tracks.
   * @zh 该艺术家曲目中不同专辑名的个数。
   */
  albumCount: Int;
  /**
   * Tracks the artist is credited on.
   * @zh 该艺术家署名的曲目数。
   */
  trackCount: Int;
  /**
   * Summed length of those tracks, in seconds.
   * @zh 这些曲目的总时长，单位秒。
   */
  totalDuration: number;
  /**
   * The albums the artist is credited on, sorted by name and then artist; present only with
   * `includeAlbums`, and never cut by `limit`.
   * @zh 该艺术家署名的专辑，按名称再按艺术家排序；只在带 `includeAlbums` 时出现，且不受 `limit` 截断。
   */
  albums?: ArtistAlbumRef[];
}

/**
 * One album an artist is credited on. `(name, artist)` is the grouping `library.getAlbums` uses,
 * so the pair matches exactly one of its rows.
 * @zh 艺术家署名的一张专辑。`(name, artist)` 与 `library.getAlbums` 的分组一致，所以恰好对应其中一行。
 */
interface ArtistAlbumRef {
  /**
   * Album name.
   * @zh 专辑名。
   */
  name: string;
  /**
   * The first `album artist` value, or the first `artist` value when there is none.
   * @zh 第一个 `album artist` 值；没有时为第一个 `artist` 值。
   */
  artist: string;
}

interface GetArtistsParams {
  /**
   * `name`, `trackCount` or `albumCount` (both largest first); any other value keeps name order.
   * @zh `name`、`trackCount` 或 `albumCount`（后两者从大到小）；其他值保持名称顺序。
   * @default "name"
   */
  sort?: string;
  /**
   * Most artists to return.
   * @zh 最多返回的艺术家数。
   * @default 1000
   * @minimum 0
   */
  limit?: Int;
  /**
   * Add each artist's albums as `albums`.
   * @zh 为每位艺术家附上 `albums`。
   * @default false
   */
  includeAlbums?: boolean;
}

interface GetArtistsResult {
  /**
   * The artists, sorted by `sort`.
   * @zh 艺术家，按 `sort` 排序。
   */
  items: ArtistInfo[];
  /**
   * Rows returned, after `limit`; there is no total, so a full page means there may be more.
   * @zh 返回的行数（`limit` 截断之后）；没有总数，满页即表示可能还有更多。
   */
  count: Int;
}

// ---- getStatus ----

interface GetStatusResult {
  /**
   * Whether the media library is enabled.
   * @zh 媒体库是否启用。
   */
  enabled: boolean;
  /**
   * Always the same as `enabled`.
   * @zh 恒与 `enabled` 相同。
   */
  initialized: boolean;
  /**
   * Always `false`; the host does not report scanning.
   * @zh 恒为 `false`；宿主不报告扫描状态。
   */
  scanning: boolean;
  /**
   * Tracks in the library; `0` when it is disabled.
   * @zh 媒体库曲目数；未启用时为 `0`。
   */
  itemCount: Int;
  /**
   * The same as `itemCount`.
   * @zh 与 `itemCount` 相同。
   */
  count: Int;
}

// ---- getCacheStats ----

interface GetCacheStatsResult {
  /**
   * Whether any library result was kept since the cache was last dropped.
   * @zh 自上次丢弃缓存以来是否保留过任何媒体库结果。
   */
  valid: boolean;
  /**
   * When the cache was last dropped, in milliseconds since the Unix epoch.
   * @zh 缓存上次被丢弃的时间，自 Unix 纪元起的毫秒数。
   */
  lastModified: Int;
  /**
   * Kept `library.getAlbums` lists, one per `query`, `sort` and `includeCover`.
   * @zh 保留的 `library.getAlbums` 列表数，每种 `query`、`sort`、`includeCover` 组合一份。
   */
  albumsCacheEntries: Int;
  /**
   * Whether a full `library.getAll` result is kept.
   * @zh 是否保留了完整的 `library.getAll` 结果。
   */
  tracksCached: boolean;
  /**
   * Whether the `library.getArtists` scan is kept.
   * @zh 是否保留了 `library.getArtists` 的扫描结果。
   */
  artistsCached: boolean;
  /**
   * Always `false`; genres are not kept.
   * @zh 恒为 `false`；流派结果不保留。
   */
  genresCached: boolean;
  /**
   * Whether the `library.getStatus` answer is kept.
   * @zh 是否保留了 `library.getStatus` 的结果。
   */
  statsCached: boolean;
  /**
   * Always `0`; covers are not kept.
   * @zh 恒为 `0`；封面不保留。
   */
  coversCached: Int;
  /**
   * Always `0`; covers are not kept.
   * @zh 恒为 `0`；封面不保留。
   */
  coverCacheBytes: Int;
  /**
   * Always `0`; covers are not kept.
   * @zh 恒为 `0`；封面不保留。
   */
  coverCacheMB: number;
  /**
   * Lookups answered from a kept result since the host started.
   * @zh 自宿主启动以来由保留结果回答的查找次数。
   */
  cacheHits: Int;
  /**
   * Lookups that found nothing kept since the host started.
   * @zh 自宿主启动以来没有找到保留结果的查找次数。
   */
  cacheMisses: Int;
  /**
   * Whether the directory tree index is built.
   * @zh 目录树索引是否已构建。
   */
  treeIndexValid: boolean;
  /**
   * Roots in the tree index; `0` when it is not built.
   * @zh 目录树索引里的根目录数；未构建时为 `0`。
   */
  rootsCached: Int;
  /**
   * Tracks placed in the tree index at its last build.
   * @zh 目录树索引上次构建时归入的曲目数。
   */
  treeIndexedTracks: Int;
  /**
   * Tracks left out of the tree index at its last build.
   * @zh 目录树索引上次构建时跳过的曲目数。
   */
  treeSkippedTracks: Int;
  /**
   * When the tree index was last built, in milliseconds since the Unix epoch; `0` before the first
   * build.
   * @zh 目录树索引上次构建的时间，自 Unix 纪元起的毫秒数；首次构建之前为 `0`。
   */
  treeLastBuilt: Int;
}

// ---- getAll ----

interface GetAllParams {
  /**
   * Tracks to skip.
   * @zh 跳过的曲目数。
   * @default 0
   * @minimum 0
   */
  offset?: Int;
  /**
   * Most tracks to return.
   * @zh 最多返回的曲目数。
   * @default 100
   * @minimum 0
   */
  limit?: Int;
  /**
   * From `offset` 0, answer from the kept list when there is one. A request from `offset` 0 that
   * covers every track is kept either way.
   * @zh 从 `offset` 0 开始时，有保留的列表就直接用它回答。无论取值如何，从 `offset` 0 开始且覆盖全部曲目的请求都会被保留。
   * @default true
   */
  useCache?: boolean;
  /**
   * Build a request from `offset` 0 that covers every track off the main thread and deliver it as
   * the `library:getAllResult` event; takes effect only with `useCache`, and not when the kept
   * list answers.
   * @zh 从 `offset` 0 开始且覆盖全部曲目的请求在主线程之外构建，以 `library:getAllResult` 事件送达；只在带 `useCache` 时生效，由保留的列表回答时也不生效。
   * @default false
   */
  asyncResult?: boolean;
}

interface GetAllResult {
  /**
   * `true` when the page will arrive as the `library:getAllResult` event; the page fields are then
   * absent.
   * @zh 这一页将以 `library:getAllResult` 事件送达时为 `true`，此时没有这一页的各字段。
   */
  pending?: boolean;
  /**
   * Id the `library:getAllResult` event carries; present with `pending`.
   * @zh `library:getAllResult` 事件带的标识；随 `pending` 出现。
   */
  requestId?: string;
  /**
   * The page in library order; `index` is the position in the library.
   * @zh 这一页，按媒体库顺序；`index` 是在媒体库中的位置。
   */
  tracks?: LibraryTrack[];
  /**
   * The same list as `tracks`.
   * @zh 与 `tracks` 相同的列表。
   */
  items?: LibraryTrack[];
  /**
   * Tracks in the library.
   * @zh 媒体库曲目数。
   */
  total?: Int;
  /**
   * The `offset` applied.
   * @zh 生效的 `offset`。
   */
  offset?: Int;
  /**
   * The `limit` applied.
   * @zh 生效的 `limit`。
   */
  limit?: Int;
  /**
   * `true` when the page came from the kept list.
   * @zh 这一页来自保留的列表时为 `true`。
   */
  fromCache?: boolean;
}

// ---- getAlbumTracks ----

interface GetAlbumTracksParams {
  /**
   * The row's `name`: the first `album` value of its tracks, compared byte for byte.
   * @zh 该行的 `name`：其曲目的第一个 `album` 值，逐字节比较。
   */
  album: string;
  /**
   * The row's `albumArtist`: the first `album artist` value of its tracks, or their first `artist`
   * value when they have no `album artist`, or `""` when they have neither. Compared byte for
   * byte, so the row's `artist` is not a substitute.
   * @zh 该行的 `albumArtist`：其曲目的第一个 `album artist` 值；没有 `album artist` 时为第一个 `artist` 值；两者都没有时为 `""`。逐字节比较，所以不能用该行的 `artist` 代替。
   */
  albumArtist: string;
}

interface GetAlbumTracksResult {
  /**
   * The album name, as given.
   * @zh 原样的专辑名。
   */
  album: string;
  /**
   * The album artist, as given.
   * @zh 原样的专辑艺术家。
   */
  albumArtist: string;
  /**
   * The album's row as `library.getAlbums` returns it, without `coverDataUrl` and `tracks`; absent
   * when no row has this name and album artist.
   * @zh 该专辑在 `library.getAlbums` 里的那一行，不带 `coverDataUrl` 与 `tracks`；没有哪一行是这个名称与专辑艺术家时没有这个键。
   */
  row?: AlbumInfo;
  /**
   * The tracks, sorted by disc number, then track number, then library order; `index` is the
   * position in this list.
   * @zh 曲目，先按碟号、再按曲号、再按媒体库顺序排序；`index` 是在这个列表中的位置。
   */
  tracks: LibraryTrack[];
  /**
   * The same list as `tracks`.
   * @zh 与 `tracks` 相同的列表。
   */
  items: LibraryTrack[];
  /**
   * Number of tracks.
   * @zh 曲目数。
   */
  total: Int;
}

// ---- getArtistTracks ----

interface GetArtistTracksParams {
  /**
   * Artist name, compared byte for byte against each value of the `artist` tag; empty, the call
   * succeeds with no tracks.
   * @zh 艺术家名，与 `artist` 标签的每个值逐字节比较；为空时成功返回空列表。
   * @default ""
   */
  artist?: string;
  /**
   * Most tracks to return; the first ones in library order are kept.
   * @zh 最多返回的曲目数；保留按媒体库顺序的前若干首。
   * @default 500
   * @minimum 0
   */
  limit?: Int;
}

interface GetArtistTracksResult {
  /**
   * The artist, as given.
   * @zh 原样的艺术家名。
   */
  artist: string;
  /**
   * The tracks in library order; `index` is the position in this list.
   * @zh 曲目，按媒体库顺序；`index` 是在这个列表中的位置。
   */
  tracks: LibraryTrack[];
  /**
   * The same list as `tracks`.
   * @zh 与 `tracks` 相同的列表。
   */
  items: LibraryTrack[];
  /**
   * The same as `count`; there is no count before `limit`, so a full page means there may be
   * more.
   * @zh 与 `count` 相同；没有 `limit` 之前的总数，满页即表示可能还有更多。
   */
  total: Int;
  /**
   * Tracks returned.
   * @zh 返回的曲目数。
   */
  count: Int;
}

// ---- getRandomTracks ----

interface GetRandomTracksParams {
  /**
   * Tracks to draw; at most the library size is returned.
   * @zh 抽取的曲目数；最多返回媒体库的曲目总数。
   * @default 10
   * @minimum 0
   */
  count?: Int;
}

interface GetRandomTracksResult {
  /**
   * The drawn tracks; `index` is the position in this list.
   * @zh 抽到的曲目；`index` 是在这个列表中的位置。
   */
  tracks: LibraryTrack[];
  /**
   * Tracks returned.
   * @zh 返回的曲目数。
   */
  count: Int;
}

// ---- getRecentlyAdded ----

interface GetRecentlyAddedParams {
  /**
   * Most tracks to return.
   * @zh 最多返回的曲目数。
   * @default 50
   * @minimum 0
   */
  limit?: Int;
  /**
   * `added` orders by `%added%`, newest first, with tracks that lack it last; `modified` orders
   * by file modification time, newest first.
   * @zh `added` 按 `%added%` 从新到旧排序，没有该值的曲目排在最后；`modified` 按文件修改时间从新到旧排序。
   * @default "added"
   */
  sortBy?: 'added' | 'modified';
}

interface GetRecentlyAddedResult {
  /**
   * The tracks, newest first; `index` is the position in the library.
   * @zh 曲目，从新到旧；`index` 是在媒体库中的位置。
   */
  tracks: RecentLibraryTrack[];
  /**
   * Tracks in the library, not the number returned.
   * @zh 媒体库曲目数，不是返回的曲目数。
   */
  total: Int;
  /**
   * The `limit` applied.
   * @zh 生效的 `limit`。
   */
  limit: Int;
  /**
   * The order used: `modified` when `added` was asked for and no track has `%added%`.
   * @zh 实际采用的排序：请求 `added` 而没有任何曲目带 `%added%` 时为 `modified`。
   */
  sortBy: 'added' | 'modified';
  /**
   * Whether the call fell back from `added` to `modified`.
   * @zh 是否从 `added` 回退到了 `modified`。
   */
  fallback: boolean;
}

// ---- browseTree ----

interface BrowseTreeParams {
  /**
   * `id` of a root from `library.getRoots`, compared case-insensitively.
   * @zh `library.getRoots` 给出的根目录 `id`，不区分大小写比较。
   * @minLength 1
   */
  rootId: string;
  /**
   * `pathId` of the folder to open, as a `directories` entry reports it; empty opens the root.
   * Folder names are separated by `/`, so a path written with `\` is not found.
   * @zh 要打开的文件夹的 `pathId`，取 `directories` 条目给出的值；为空时打开根目录。文件夹名之间用 `/` 分隔，所以用 `\` 写的路径找不到。
   * @default ""
   */
  pathId?: string;
  /**
   * Add the folder's tracks as `files`.
   * @zh 附上文件夹的曲目 `files`。
   * @default false
   */
  includeFiles?: boolean;
  /**
   * With `includeFiles`, add the tracks of every folder below as well.
   * @zh 带 `includeFiles` 时，连同其下所有文件夹的曲目一并附上。
   * @default false
   */
  recursiveFiles?: boolean;
}

interface BrowseTreeResult {
  /**
   * The root.
   * @zh 根目录。
   */
  root: LibraryRootInfo;
  /**
   * The `pathId` opened.
   * @zh 打开的 `pathId`。
   */
  pathId: string;
  /**
   * Local path of the folder.
   * @zh 文件夹的本地路径。
   */
  absolutePath: string;
  /**
   * Subfolders directly inside, ordered by `displayName` and then `absolutePath`, ignoring case.
   * @zh 直接包含的子文件夹，按 `displayName` 再按 `absolutePath` 排序，不区分大小写。
   */
  directories: LibraryDirectoryNodeInfo[];
  /**
   * The folder's tracks in library order, followed with `recursiveFiles` by those of the folders
   * below in no fixed order; empty without `includeFiles`. `index` is the position in the library
   * when the tree index was built.
   * @zh 文件夹的曲目，按媒体库顺序；带 `recursiveFiles` 时其后接下层文件夹的曲目，顺序不固定；不带 `includeFiles` 时为空。`index` 是构建目录树索引时在媒体库中的位置。
   */
  files: LibraryTrack[];
  /**
   * `true` when the tree index already existed before this call.
   * @zh 本次调用之前目录树索引已存在时为 `true`。
   */
  fromCache: boolean;
}

// ---- browseDirectory ----

interface BrowseDirectoryParams {
  /**
   * Path prefix, compared with ASCII letters case-insensitive; empty matches every track.
   * @zh 路径前缀，ASCII 字母不区分大小写；为空时匹配全部曲目。
   * @default ""
   */
  path?: string;
  /**
   * Add every track under `path` as `files`. On by default, so an empty `path` returns the whole
   * library.
   * @zh 附上 `path` 下的全部曲目 `files`。默认开启，所以 `path` 为空时返回整个媒体库。
   * @default true
   */
  includeFiles?: boolean;
}

interface BrowseDirectoryResult {
  /**
   * Folders one level below `path`, as stored paths, in byte order.
   * @zh `path` 下一层的文件夹，是存储形式的路径，按字节序排列。
   */
  directories: string[];
  /**
   * The same list as `directories`.
   * @zh 与 `directories` 相同的列表。
   */
  items: string[];
  /**
   * Tracks under `path` in library order; `index` is the position in the library. Empty without
   * `includeFiles`.
   * @zh `path` 下的曲目，按媒体库顺序；`index` 是在媒体库中的位置。不带 `includeFiles` 时为空。
   */
  files: LibraryTrack[];
}

// ---- query ----

interface QueryParams {
  /**
   * foobar2000 query expression.
   * @zh foobar2000 查询表达式。
   * @minLength 1
   */
  query: string;
  /**
   * Title Formatting expression to sort the matches by before `limit` applies; empty keeps library
   * order, and an expression that fails to compile is ignored.
   * @zh 在截断到 `limit` 之前用来排序的 Title Formatting 表达式；为空时保持媒体库顺序，编译失败的表达式被忽略。
   * @default ""
   */
  sort?: string;
  /**
   * Most rows to return; `total` still counts every match.
   * @zh 最多返回的行数；`total` 仍计入全部匹配。
   * @default 100
   * @minimum 0
   */
  limit?: Int;
  /**
   * Keys each row carries; omitted, every key of a library track row. Names are matched
   * case-sensitively against those keys: `index`, `handle`, `title`, `artist`, `artists`, `album`,
   * `albumArtist`, `albumArtists`, `genre`, `date`, `trackNumber`, `discNumber`, `duration`,
   * `path`, `absolutePath`, `fileSize`, `bitrate`, `sampleRate`, `channels`, `codec`, `subsong`
   * and `rating`. An unknown name fails with `INVALID_PARAMS` and is listed in
   * `details.unknownFields`.
   * @zh 每行带的键；省略时为媒体库曲目行的全部键。名字与这些键按大小写精确比较：`index`、`handle`、`title`、`artist`、`artists`、`album`、`albumArtist`、`albumArtists`、`genre`、`date`、`trackNumber`、`discNumber`、`duration`、`path`、`absolutePath`、`fileSize`、`bitrate`、`sampleRate`、`channels`、`codec`、`subsong`、`rating`。未知的名字以 `INVALID_PARAMS` 失败，并列在 `details.unknownFields` 里。
   * @minItems 1
   */
  fields?: string[];
}

interface QueryResult {
  /**
   * The matches, sorted by `sort` or in library order; `index` is the position in this list. Each
   * row carries exactly the `fields` asked for.
   * @zh 匹配的曲目，按 `sort` 排序或按媒体库顺序；`index` 是在这个列表中的位置。每行恰好带 `fields` 要求的键。
   */
  tracks: Partial<LibraryTrack>[];
  /**
   * Matches, before `limit`.
   * @zh 匹配数（`limit` 截断之前）。
   */
  total: Int;
}

// ---- search ----

interface SearchParams {
  /**
   * foobar2000 query expression; empty, the call succeeds with no tracks.
   * @zh foobar2000 查询表达式；为空时成功返回空列表。
   * @default ""
   */
  query?: string;
  /**
   * Matches to skip.
   * @zh 跳过的匹配数。
   * @default 0
   * @minimum 0
   */
  offset?: Int;
  /**
   * Most rows to return.
   * @zh 最多返回的行数。
   * @default 100
   * @minimum 0
   */
  limit?: Int;
  /**
   * Keys each row carries, as for `library.query`.
   * @zh 每行带的键，与 `library.query` 相同。
   * @minItems 1
   */
  fields?: string[];
}

interface SearchResult {
  /**
   * This page of the matches in library order; `index` is the position among all matches. Each
   * row carries exactly the `fields` asked for.
   * @zh 这一页匹配的曲目，按媒体库顺序；`index` 是在全部匹配中的位置。每行恰好带 `fields` 要求的键。
   */
  tracks: Partial<LibraryTrack>[];
  /**
   * Matches, before `offset` and `limit`.
   * @zh 匹配数（`offset` 与 `limit` 之前）。
   */
  total: Int;
  /**
   * The `offset` applied.
   * @zh 生效的 `offset`。
   */
  offset: Int;
  /**
   * The `limit` applied.
   * @zh 生效的 `limit`。
   */
  limit: Int;
  /**
   * Whether matches remain after this page.
   * @zh 这一页之后是否还有匹配。
   */
  hasMore: boolean;
}
