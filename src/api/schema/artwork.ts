import type { Int, PlaylistGuid, ReportedPlaylistGuid } from './common.js';

export interface Api {
  /**
   * Read the picture of the playing track. Three lookups are tried in order and `source`
   * names the one that answered; `available: false` with `reason: "no_track"` when nothing
   * is playing and `reason: "not_found"` when none of them had a picture.
   * @zh 读取当前正在播放曲目的封面图片；按顺序尝试三种检索策略并在 `source` 中指明胜出源；未在播放时返回 `available: false`（`reason: "no_track"`），均未查得图片时返回 `reason: "not_found"`
   * @effect read
   */
  getCurrent(params: GetCurrentParams): GetCurrentResult;

  /**
   * Read a picture straight from the file at `path` with the album art extractor. The
   * `|subsong:N` suffix is dropped, since pictures belong to the file; a `file-relative://`
   * path is refused with `INVALID_PATH`, because only a playlist row can resolve it.
   * @zh 使用专辑封面提取器直接从指定文件路径中读取内嵌图片；自动剥离 `|subsong:N` 子曲目后缀（图片归属于物理文件本身）；`file-relative://` 相对路径以 `INVALID_PATH` 拒绝（仅播放列表行可解析该协议）
   */
  getByPath(params: GetByPathParams): GetByPathResult;

  /**
   * Read a picture through the album art manager, which also finds folder covers, and measure
   * it. `width` and `height` are read from the PNG header and are `0` for other formats. A
   * `file-relative://` path is refused with `INVALID_PATH`.
   * @zh 通过专辑封面管理器读取曲目封面（包含内嵌封面及所在目录的外置封面文件）并获取尺寸；从 PNG 头信息解析 `width` 与 `height`（其他格式返回 `0`）；`file-relative://` 路径以 `INVALID_PATH` 拒绝
   * @effect read
   */
  getForTrack(params: GetForTrackParams): GetForTrackResult;

  /**
   * Read the picture of a playlist row through the album art manager. A negative `playlist`
   * means the active playlist and a negative `index` means the first row. A row past the end,
   * an empty playlist included, fails with `NOT_FOUND`. A playlist index past the last playlist
   * fails with `INVALID_INDEX`, a `playlistGuid` no playlist has with `NOT_FOUND`, and no active
   * playlist when the active one is meant with `NO_ACTIVE_ITEM`. A row without the picture is a
   * success with `available` `false`.
   * @zh 通过专辑封面管理器读取指定播放列表行的封面图片；传入负数 `playlist` 表示活动播放列表，传入负数 `index` 表示首行；行索引越界或空列表以 `NOT_FOUND` 失败，播放列表索引越界以 `INVALID_INDEX` 失败，`playlistGuid` 不存在以 `NOT_FOUND` 失败，未指定且无活动播放列表时以 `NO_ACTIVE_ITEM` 失败；该行无图片时成功返回 `available: false`
   */
  getByPlaylistItem(params: GetByPlaylistItemParams): GetByPlaylistItemResult;

  /**
   * List the picture types embedded in a file, in the fixed probe order front, back, disc,
   * icon, artist. Without `path` the playing track is used; with nothing playing the call
   * fails with `NO_ACTIVE_ITEM`. A file that is missing, cannot be read or has a format without
   * embedded pictures is not an error: it lists no types.
   * @zh 列出指定音频文件内嵌的图片类型列表，按固定探测顺序（front、back、disc、icon、artist）排列；省略 `path` 时检测当前播放曲目（未播放时以 `NO_ACTIVE_ITEM` 失败）；文件缺失、不可读或格式不支持内嵌图片时不报错，返回空类型列表
   */
  getAvailableTypes(params: GetAvailableTypesParams): GetAvailableTypesResult;

  /**
   * Build the `fb2k://artwork/` URL of the playing track's picture; the resource handler reads
   * and scales the picture when the page loads the URL. `available: false` with
   * `reason: "no_track"` when nothing is playing.
   * @zh 生成当前播放曲目封面的 `fb2k://artwork/` 专用协议 URL；前端页面加载该 URL 时由宿主资源处理器动态读取并缩放；未在播放时返回 `available: false`（`reason: "no_track"`）
   */
  getFb2kUrl(params: GetFb2kUrlParams): GetFb2kUrlResult;

  /**
   * Build the `fb2k://artwork/` URL for a path. Only a string is built: the file is not opened,
   * so a path with no file behind it is still reported available.
   * @zh 为指定曲目路径生成 `fb2k://artwork/` 协议 URL；仅执行字符串拼接而不实际打开文件（目标文件不存在时亦正常返回可用 URL）
   */
  getFb2kUrlByPath(params: GetFb2kUrlByPathParams): GetFb2kUrlByPathResult;

  /**
   * Build `fb2k://artwork/` URLs for up to 100 entries given as exactly one of `paths` or
   * `items`; giving both or neither fails with `INVALID_PARAMS`. Each row reports its own
   * outcome, so a bad entry does not fail the call.
   * @zh 批量生成最多 100 个条目的 `fb2k://artwork/` 协议 URL；`paths` 与 `items` 必须二选一传入，同时传入或均未传入以 `INVALID_PARAMS` 失败；逐项返回生成结果，单项错误不影响整批调用
   */
  getFb2kUrlByPathBatch(params: GetFb2kUrlByPathBatchParams): GetFb2kUrlByPathBatchResult;

  /**
   * Read the lyrics tag of a track. The host's cached track info is used when complete; a local
   * file it has not read is read from disk, and a remote or unrecognised path keeps whatever the
   * cache has. The tags `LYRICS`, `UNSYNCED LYRICS`, `UNSYNCEDLYRICS`, `SYNCEDLYRICS` and
   * `SYNCED LYRICS` are probed in that order and the first non-empty one wins. Without `path`
   * the playing track is used; with nothing playing the call fails with `NO_ACTIVE_ITEM`. A path
   * no track can be made for fails with `NOT_FOUND`, a local file that cannot be read with
   * `OPERATION_FAILED`. Invalid UTF-8 in lyrics is replaced with U+FFFD without changing tags.
   * @zh 读取曲目内嵌的歌词标签；宿主缓存完备时优先从缓存读取，未缓存的本地文件从磁盘读取，远程网络流或无法识别的路径仅使用现有缓存；依次按 `LYRICS`、`UNSYNCED LYRICS`、`UNSYNCEDLYRICS`、`SYNCEDLYRICS`、`SYNCED LYRICS` 顺序探测并取首个非空标签；省略 `path` 时读取当前播放曲目（未播放时以 `NO_ACTIVE_ITEM` 失败）；无法解析曲目以 `NOT_FOUND` 失败，文件读取失败以 `OPERATION_FAILED` 失败；歌词中的非法 UTF-8 字符自动替换为 U+FFFD 且不修改原文件
   */
  getLyrics(params: GetLyricsParams): GetLyricsResult;

  /**
   * Read the album-level tags of a track, plus whether the file embeds any picture and whether
   * it carries a lyrics tag. The tags come from the host's cached track info when complete; a
   * local file it has not read is read from disk, and a remote or unrecognised path keeps
   * whatever the cache has, which can be blank. Without `path` the playing track is used; with
   * nothing playing the call fails with `NO_ACTIVE_ITEM`. A path no track can be made for fails
   * with `NOT_FOUND`, a local file that cannot be read with `OPERATION_FAILED`. Invalid UTF-8
   * in returned tag values is replaced with U+FFFD without changing the file.
   * @zh 读取曲目的专辑级基本标签，并检测文件是否内嵌图片或包含歌词标签；缓存完备时优先取自缓存，未缓存的本地文件从磁盘读取；省略 `path` 时检测当前播放曲目（未播放时以 `NO_ACTIVE_ITEM` 失败）；无法解析曲目以 `NOT_FOUND` 失败，文件不可读以 `OPERATION_FAILED` 失败；返回标签中的非法 UTF-8 替换为 U+FFFD 且不修改原文件
   */
  getMetadata(params: GetMetadataParams): GetMetadataResult;

  /**
   * Read one picture type from several files with the album art extractor, one row per path in
   * the given order; a file without the picture gets a row with `available: false`.
   * @zh 使用专辑封面提取器批量读取多个音频文件的同一种封面图片；按输入路径顺序逐行返回，目标文件未包含该图片类型时对应行返回 `available: false`
   */
  getBatch(params: GetBatchParams): GetBatchResult;

  /**
   * Report which picture types a file embeds and which cover files sit next to it.
   * @zh 探测指定文件内嵌的图片类型以及同目录下存在的外置封面文件
   */
  getAvailableArtwork(params: GetAvailableArtworkParams): GetAvailableArtworkResult;

  /**
   * List the image files (`.jpg`, `.jpeg`, `.png`, `.gif`, `.bmp`, `.webp`) directly inside a
   * directory. A directory that is missing, is a file or cannot be listed is not an error: it
   * lists no images.
   * @zh 列出指定目录下一级直接包含的常用图片文件（支持 `.jpg`、`.jpeg`、`.png`、`.gif`、`.bmp`、`.webp`）；目录不存在、路径为文件或无读取权限时不报错，返回空图片列表
   */
  getFolderImages(params: GetFolderImagesParams): GetFolderImagesResult;
}

/**
 * A picture type; `cover_front` and `cover_back` are the foobar2000 spellings of `front` and
 * `back`.
 * @zh 封面图片类型；`cover_front` 与 `cover_back` 为 foobar2000 原生兼容写法
 */
type ArtworkType = 'front' | 'cover_front' | 'back' | 'cover_back' | 'disc' | 'icon' | 'artist';

interface GetCurrentParams {
  /**
   * Picture type.
   * @zh 待读取的图片类型
   * @default "front"
   */
  type?: ArtworkType;
}

interface GetCurrentResult {
  /**
   * Whether a picture was found.
   * @zh 是否成功获取到图片
   */
  available: boolean;
  /**
   * The requested type.
   * @zh 请求的图片类型
   */
  type: string;
  /**
   * Lookup that answered: the now-playing cache, the album art manager, or the extractor.
   * @zh 实际响应的检索来源：正在播放缓存（`now_playing_manager`）、专辑封面管理器（`album_art_manager_v2`）或提取器（`extractor`）
   */
  source?: 'now_playing_manager' | 'album_art_manager_v2' | 'extractor';
  /**
   * MIME type detected from the bytes.
   * @zh 根据二进制数据特征检测出的 MIME 类型
   */
  mimeType?: string;
  /**
   * Picture size in bytes.
   * @zh 图片二进制数据大小（字节）
   */
  size?: Int;
  /**
   * `data:<mime>;base64,...` URL of the picture.
   * @zh 图片的 Base64 Data URL（`data:<mime>;base64,...`）
   */
  dataUrl?: string;
  /**
   * Why no picture was found.
   * @zh 未找到图片的原因代码
   */
  reason?: 'no_track' | 'not_found';
  /**
   * Path of the playing track; reported when no picture was found for it.
   * @zh 当前播放曲目的文件路径（未找到图片时返回）
   */
  path?: string;
}

interface GetByPathParams {
  /**
   * Track path: native, `file://`, or with a `|subsong:N` suffix.
   * @zh 目标曲目路径（支持本地路径、`file://` 或附带 `|subsong:N` 后缀）
   * @minLength 1
   * @security MediaRead
   */
  path: string;
  /**
   * Picture type.
   * @zh 待读取的图片类型
   * @default "front"
   */
  type?: ArtworkType;
}

interface GetByPathResult {
  /**
   * Whether the file has the picture.
   * @zh 目标文件是否包含该图片
   */
  available: boolean;
  /**
   * The requested type.
   * @zh 请求的图片类型
   */
  type: string;
  /**
   * The path as given.
   * @zh 请求传入的原样路径
   */
  path: string;
  /**
   * MIME type detected from the bytes.
   * @zh 根据二进制数据特征检测出的 MIME 类型
   */
  mimeType?: string;
  /**
   * Picture size in bytes.
   * @zh 图片二进制数据大小（字节）
   */
  size?: Int;
  /**
   * `data:<mime>;base64,...` URL of the picture.
   * @zh 图片的 Base64 Data URL（`data:<mime>;base64,...`）
   */
  dataUrl?: string;
}

interface GetForTrackParams {
  /**
   * Track path: native, `file://`, or with a `|subsong:N` suffix.
   * @zh 目标曲目路径（支持本地路径、`file://` 或附带 `|subsong:N` 后缀）
   * @minLength 1
   * @security MediaRead
   */
  path: string;
  /**
   * Picture type.
   * @zh 待读取的图片类型
   * @default "front"
   */
  type?: ArtworkType;
}

interface GetForTrackResult {
  /**
   * Whether a picture was found.
   * @zh 是否成功获取到图片
   */
  available: boolean;
  /**
   * The requested type.
   * @zh 请求的图片类型
   */
  type: string;
  /**
   * The path as given.
   * @zh 请求传入的原样路径
   */
  path: string;
  /**
   * MIME type detected from the bytes.
   * @zh 根据二进制数据特征检测出的 MIME 类型
   */
  mimeType?: string;
  /**
   * Width in pixels from the PNG header; `0` for other formats.
   * @zh 图片像素宽度（从 PNG 头解析得出，其他格式为 `0`）
   */
  width?: Int;
  /**
   * Height in pixels from the PNG header; `0` for other formats.
   * @zh 图片像素高度（从 PNG 头解析得出，其他格式为 `0`）
   */
  height?: Int;
  /**
   * Picture size in bytes.
   * @zh 图片二进制数据大小（字节）
   */
  size?: Int;
  /**
   * `data:<mime>;base64,...` URL of the picture.
   * @zh 图片的 Base64 Data URL（`data:<mime>;base64,...`）
   */
  dataUrl?: string;
}

interface GetByPlaylistItemParams {
  /**
   * Playlist index; negative for the active playlist. An index past the last playlist fails with
   * `INVALID_INDEX`.
   * @zh 目标播放列表序号；传入负数表示活动播放列表；超出范围以 `INVALID_INDEX` 失败
   * @default -1
   */
  playlist?: Int;
  playlistGuid?: PlaylistGuid;
  /**
   * Row index; negative for the first row.
   * @zh 目标行号索引；传入负数表示首行
   * @default -1
   */
  index?: Int;
  /**
   * Picture type.
   * @zh 待读取的图片类型
   * @default "front"
   */
  type?: ArtworkType;
}

interface GetByPlaylistItemResult {
  /**
   * Whether a picture was found.
   * @zh 是否成功获取到图片
   */
  available: boolean;
  /**
   * The requested type.
   * @zh 请求的图片类型
   */
  type: string;
  /**
   * The playlist actually read.
   * @zh 实际读取的播放列表索引
   */
  playlist: Int;
  playlistGuid: ReportedPlaylistGuid;
  /**
   * The row actually read.
   * @zh 实际读取的播放列表行索引
   */
  index: Int;
  /**
   * MIME type detected from the bytes.
   * @zh 根据二进制数据特征检测出的 MIME 类型
   */
  mimeType?: string;
  /**
   * Picture size in bytes.
   * @zh 图片二进制数据大小（字节）
   */
  size?: Int;
  /**
   * `data:<mime>;base64,...` URL of the picture.
   * @zh 图片的 Base64 Data URL（`data:<mime>;base64,...`）
   */
  dataUrl?: string;
}

interface GetAvailableTypesParams {
  /**
   * Track path; omitted for the playing track.
   * @zh 目标曲目路径；省略时默认检测当前播放曲目
   * @security MediaRead
   */
  path?: string;
}

interface GetAvailableTypesResult {
  /**
   * The embedded picture types, in probe order.
   * @zh 文件中内嵌的图片类型列表，按探测先后顺序排列
   */
  types: string[];
}

interface GetFb2kUrlParams {
  /**
   * Picture type.
   * @zh 待读取的图片类型
   * @default "front"
   */
  type?: ArtworkType;
  /**
   * Longest side in pixels the handler scales the picture down to; omitted or `0` keeps the
   * original size.
   * @zh 图片缩放的最长边像素上限；省略或传 `0` 时保持原始尺寸
   */
  maxSize?: Int;
}

interface GetFb2kUrlResult {
  /**
   * Whether a track is playing.
   * @zh 当前是否有正在播放的有效曲目
   */
  available: boolean;
  /**
   * The type the URL carries: `front` or `back` for their `cover_` spellings.
   * @zh 最终写入 URL 的类型标识（`cover_front` / `cover_back` 归一为 `front` / `back`）
   */
  type: string;
  /**
   * The `fb2k://artwork/?path=...` URL; not a data URL, only the component's WebView2
   * resource handler resolves it.
   * @zh 生成的 `fb2k://artwork/?path=...` 专用协议 URL（仅供本组件 WebView2 内部加载解析）
   */
  dataUrl?: string;
  /**
   * Why there is no URL; only `no_track` occurs here.
   * @zh 未生成 URL 的原因说明（此处仅可能为 `no_track`）
   */
  reason?: 'no_track' | 'not_found';
}

interface GetFb2kUrlByPathParams {
  /**
   * Track path.
   * @zh 目标曲目路径
   * @minLength 1
   * @security MediaRead
   */
  path: string;
  /**
   * Picture type.
   * @zh 待读取的图片类型
   * @default "front"
   */
  type?: ArtworkType;
  /**
   * Longest side in pixels the handler scales the picture down to; omitted or `0` keeps the
   * original size.
   * @zh 图片缩放的最长边像素上限；省略或传 `0` 时保持原始尺寸
   */
  maxSize?: Int;
}

interface GetFb2kUrlByPathResult {
  /**
   * Always `true`: the URL is built without opening the file.
   * @zh URL 是否成功构建，固定为 `true`（仅拼接路径不打开文件）
   */
  available: boolean;
  /**
   * The type the URL carries: `front` or `back` for their `cover_` spellings.
   * @zh 最终写入 URL 的类型标识（`cover_front` / `cover_back` 归一为 `front` / `back`）
   */
  type: string;
  /**
   * The path as given.
   * @zh 请求传入的原样路径
   */
  path: string;
  /**
   * The `fb2k://artwork/?path=...` URL; not a data URL, only the component's WebView2
   * resource handler resolves it.
   * @zh 生成的 `fb2k://artwork/?path=...` 专用协议 URL（仅供本组件 WebView2 内部加载解析）
   */
  dataUrl: string;
}

/**
 * One entry of `getFb2kUrlByPathBatch`'s `items`.
 * @zh 批量图片 URL 请求中的单项参数
 */
interface ArtworkBatchItem {
  /**
   * Track path.
   * @zh 目标曲目路径
   */
  path: string;
  /**
   * Picture type for this entry; the batch-wide `type` when omitted.
   * @zh 该项请求的图片类型；省略时沿用整批的 `type` 参数
   */
  type?: ArtworkType;
  /**
   * Scale limit for this entry; the batch-wide `maxSize` when omitted.
   * @zh 该项缩放的最长边像素上限；省略时沿用整批的 `maxSize` 参数
   */
  maxSize?: Int;
}

interface GetFb2kUrlByPathBatchParams {
  /**
   * Track paths; exactly one of `paths` and `items`.
   * @zh 批量曲目路径列表；与 `items` 严格二选一传入
   * @security MediaRead
   */
  paths?: string[];
  /**
   * Entries with their own type or scale limit; exactly one of `paths` and `items`.
   * @zh 附带独立类型或尺寸选项的批量条目列表；与 `paths` 严格二选一传入
   * @security MediaRead
   * @pathKey path
   */
  items?: ArtworkBatchItem[];
  /**
   * Picture type for entries that do not name their own.
   * @zh 未单独指定类型的条目所使用的全局默认图片类型
   * @default "front"
   */
  type?: ArtworkType;
  /**
   * Longest side in pixels for entries that do not name their own; omitted or `0` keeps the
   * original size.
   * @zh 未单独指定尺寸的条目所使用的全局最长边像素上限；省略或传 `0` 时保持原始尺寸
   */
  maxSize?: Int;
}

/**
 * One row of `getFb2kUrlByPathBatch`.
 * @zh 批量图片 URL 结果中的单项信息
 */
interface ArtworkUrlRow {
  /**
   * The entry's path.
   * @zh 该条目对应的曲目路径
   */
  path: string;
  /**
   * Whether a URL was built.
   * @zh 该条目是否成功生成 URL
   */
  available: boolean;
  /**
   * The type the URL carries.
   * @zh URL 中生效的图片类型
   */
  type?: string;
  /**
   * The `fb2k://artwork/?path=...` URL.
   * @zh 生成的 `fb2k://artwork/?path=...` 专用协议 URL
   */
  dataUrl?: string;
  /**
   * Why no URL was built, as the URL builder's error name such as `invalid_type`.
   * @zh URL 生成失败的原因代码（如 `invalid_type`）
   */
  error?: string;
}

interface GetFb2kUrlByPathBatchResult {
  /**
   * One row per entry, in the given order.
   * @zh 批量生成的图片 URL 结果列表，按输入顺序排列
   */
  artworks: ArtworkUrlRow[];
}

interface GetLyricsParams {
  /**
   * Track path; omitted for the playing track.
   * @zh 目标曲目路径；省略时默认检测当前播放曲目
   * @security MediaRead
   */
  path?: string;
}

interface GetLyricsResult {
  /**
   * Whether a non-empty lyrics tag was found.
   * @zh 是否成功读取到非空歌词标签
   */
  available: boolean;
  /**
   * The tag that supplied the lyrics.
   * @zh 最终胜出提供歌词的标签名称
   */
  tag?: string;
  /**
   * The lyrics text.
   * @zh 读取出的完整歌词文本
   */
  lyrics?: string;
  /**
   * `true` when the tag name marks the lyrics as synced.
   * @zh 标签名是否指示该歌词为时间同步歌词
   */
  synced?: boolean;
}

interface GetMetadataParams {
  /**
   * Track path; omitted for the playing track.
   * @zh 目标曲目路径；省略时默认检测当前播放曲目
   * @security MediaRead
   */
  path?: string;
}

interface GetMetadataResult {
  /**
   * Always `true`: the track was resolved, whether or not any of its tags could be read.
   * @zh 曲目是否成功解析，固定为 `true`（即使所有标签均为空）
   */
  available: boolean;
  /**
   * `ALBUM` tag; empty when absent.
   * @zh `ALBUM` 标签值；缺失时为空字符串
   */
  album: string;
  /**
   * `ARTIST` values joined with `, ` in tag order.
   * @zh 所有 `ARTIST` 标签值按原顺序以 `, ` 连接
   */
  artist: string;
  /**
   * `ALBUM ARTIST` values joined with `, ` in tag order.
   * @zh 所有 `ALBUM ARTIST` 标签值按原顺序以 `, ` 连接
   */
  albumArtist: string;
  /**
   * `TITLE` tag; empty when absent.
   * @zh `TITLE` 标签值；缺失时为空字符串
   */
  title: string;
  /**
   * `DATE` tag as written; empty when absent.
   * @zh `DATE` 标签原始字符串；缺失时为空字符串
   */
  year: string;
  /**
   * `GENRE` values joined with `, ` in tag order.
   * @zh 所有 `GENRE` 标签值按原顺序以 `, ` 连接
   */
  genre: string;
  /**
   * `TRACKNUMBER` tag as written, such as `3` or `3/12`; empty when absent.
   * @zh `TRACKNUMBER` 标签原始书写格式（如 `3` 或 `3/12`）；缺失时为空字符串
   */
  trackNumber: string;
  /**
   * `DISCNUMBER` tag as written; empty when absent.
   * @zh `DISCNUMBER` 标签原始书写格式；缺失时为空字符串
   */
  discNumber: string;
  /**
   * Whether the file embeds any of the five picture types.
   * @zh 文件是否内嵌了五种封面图片类型中的任意一种
   */
  hasEmbedded: boolean;
  /**
   * Whether any of the five tags recognized by `artwork.getLyrics` exists, even an empty one.
   * @zh 文件是否包含五种常见歌词标签中的任意一种（空标签亦计入）
   */
  hasLyrics: boolean;
}

interface GetBatchParams {
  /**
   * Track paths; a `|subsong:N` suffix is dropped.
   * @zh 待批量读取的曲目路径列表；自动剥离 `|subsong:N` 后缀
   * @security MediaRead
   */
  paths: string[];
  /**
   * Picture type for every path.
   * @zh 整批读取的统一图片类型
   * @default "front"
   */
  type?: ArtworkType;
}

/**
 * One row of `getBatch`.
 * @zh 批量读取封面图片的单项结果信息
 */
interface ArtworkBatchRow {
  /**
   * The path as given.
   * @zh 请求传入的原样路径
   */
  path: string;
  /**
   * Whether the file has the picture.
   * @zh 目标文件是否包含该图片
   */
  available: boolean;
  /**
   * MIME type detected from the bytes.
   * @zh 根据二进制数据特征检测出的 MIME 类型
   */
  mimeType?: string;
  /**
   * Picture size in bytes.
   * @zh 图片二进制数据大小（字节）
   */
  size?: Int;
  /**
   * `data:<mime>;base64,...` URL of the picture.
   * @zh 图片的 Base64 Data URL（`data:<mime>;base64,...`）
   */
  dataUrl?: string;
}

interface GetBatchResult {
  /**
   * One row per path, in the given order.
   * @zh 批量读取结果列表，按输入路径顺序排列
   */
  artworks: ArtworkBatchRow[];
}

interface GetAvailableArtworkParams {
  /**
   * Track path; a `|subsong:N` suffix is dropped.
   * @zh 待批量读取的曲目路径列表；自动剥离 `|subsong:N` 后缀
   * @minLength 1
   * @security MediaRead
   */
  path: string;
}

/**
 * One embedded picture of `getAvailableArtwork`.
 * @zh 内嵌图片探测条目信息
 */
interface ArtworkAvailableEntry {
  /**
   * Picture type.
   * @zh 待读取的图片类型
   */
  type: string;
  /**
   * Where it comes from; always `embedded`, since only embedded pictures are listed.
   * @zh 封面来源标识；固定为 `embedded`（仅列出内嵌图片）
   */
  source: string;
}

interface GetAvailableArtworkResult {
  /**
   * Whether the file embeds at least one picture.
   * @zh 文件是否至少内嵌有一张图片
   */
  available: boolean;
  /**
   * The embedded pictures, in probe order.
   * @zh 探测到的所有内嵌图片列表，按探测先后顺序排列
   */
  artworks: ArtworkAvailableEntry[];
  /**
   * `embedded` once when any picture is embedded, then `folder:<name>` for each cover file
   * (`cover`, `folder`, `front`, `album` as `.jpg` or `.png`) found next to the file.
   * @zh 封面来源清单：内嵌图片标记为 `embedded`，同目录下外置封面文件按 `folder:<名字>` 格式列出（匹配 `cover`、`folder`、`front`、`album` 的 `.jpg` 或 `.png`）
   */
  sources: string[];
}

interface GetFolderImagesParams {
  /**
   * Directory to list.
   * @zh 待扫描的目标目录路径
   * @minLength 1
   * @security MediaRead
   */
  directory: string;
}

/**
 * One image file of `getFolderImages`.
 * @zh 目录下扫描到的图片文件信息
 */
interface ArtworkFolderImage {
  /**
   * File name.
   * @zh 图片文件名称
   */
  name: string;
  /**
   * Full path.
   * @zh 图片完整物理路径
   */
  path: string;
  /**
   * File size in bytes.
   * @zh 图片文件大小（字节）
   */
  size: Int;
}

interface GetFolderImagesResult {
  /**
   * The image files, in directory order.
   * @zh 目录下扫描到的图片文件列表，按目录遍历顺序排列
   */
  images: ArtworkFolderImage[];
}
