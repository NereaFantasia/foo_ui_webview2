import type { Int, PlaylistGuid, ReportedPlaylistGuid } from './common.js';

export interface Api {
  /**
   * Read the picture of the playing track. Three lookups are tried in order and `source`
   * names the one that answered; `available: false` with `reason: "no_track"` when nothing
   * is playing and `reason: "not_found"` when none of them had a picture.
   * @zh 读正在播放曲目的图片。按顺序尝试三种查法，`source` 报是哪一种答的；没在播放时 `available: false` 且 `reason: "no_track"`，三种都没有图片时 `reason: "not_found"`。
   * @effect read
   */
  getCurrent(params: GetCurrentParams): GetCurrentResult;

  /**
   * Read a picture straight from the file at `path` with the album art extractor. The
   * `|subsong:N` suffix is dropped, since pictures belong to the file; a `file-relative://`
   * path is refused with `INVALID_PATH`, because only a playlist row can resolve it.
   * @zh 用专辑封面提取器直接从 `path` 的文件里读图片。`|subsong:N` 后缀被去掉，因为图片属于文件；`file-relative://` 路径以 `INVALID_PATH` 拒绝，只有播放列表行才能解析它。
   */
  getByPath(params: GetByPathParams): GetByPathResult;

  /**
   * Read a picture through the album art manager, which also finds folder covers, and measure
   * it. `width` and `height` are read from the PNG header and are `0` for other formats. A
   * `file-relative://` path is refused with `INVALID_PATH`.
   * @zh 经专辑封面管理器读图片（也会找到文件夹封面）并量尺寸。`width` 与 `height` 从 PNG 头读出，其他格式为 `0`。`file-relative://` 路径以 `INVALID_PATH` 拒绝。
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
   * @zh 经专辑封面管理器读播放列表某一行的图片。`playlist` 为负表示活动播放列表，`index` 为负表示第一行。越界的行（含空列表）以 `NOT_FOUND` 失败。超出最后一个播放列表的序号以 `INVALID_INDEX` 失败，没有列表持有的 `playlistGuid` 以 `NOT_FOUND` 失败，要用活动播放列表而没有时以 `NO_ACTIVE_ITEM` 失败。那一行没有这张图片时成功，`available` 为 `false`。
   */
  getByPlaylistItem(params: GetByPlaylistItemParams): GetByPlaylistItemResult;

  /**
   * List the picture types embedded in a file, in the fixed probe order front, back, disc,
   * icon, artist. Without `path` the playing track is used; with nothing playing the call
   * fails with `NO_ACTIVE_ITEM`. A file that is missing, cannot be read or has a format without
   * embedded pictures is not an error: it lists no types.
   * @zh 列出文件里嵌入的图片类型，按固定探测顺序 front、back、disc、icon、artist。不给 `path` 时用正在播放的曲目；没在播放时以 `NO_ACTIVE_ITEM` 失败。文件不存在、读不了或格式不带嵌入图片都不算错误，只是列不出类型。
   */
  getAvailableTypes(params: GetAvailableTypesParams): GetAvailableTypesResult;

  /**
   * Build the `fb2k://artwork/` URL of the playing track's picture; the resource handler reads
   * and scales the picture when the page loads the URL. `available: false` with
   * `reason: "no_track"` when nothing is playing.
   * @zh 生成正在播放曲目图片的 `fb2k://artwork/` URL；页面加载它时由资源处理器读图并缩放。没在播放时 `available: false` 且 `reason: "no_track"`。
   */
  getFb2kUrl(params: GetFb2kUrlParams): GetFb2kUrlResult;

  /**
   * Build the `fb2k://artwork/` URL for a path. Only a string is built: the file is not opened,
   * so a path with no file behind it is still reported available.
   * @zh 为某个路径生成 `fb2k://artwork/` URL。只拼字符串，不打开文件，所以没有文件的路径同样报可用。
   */
  getFb2kUrlByPath(params: GetFb2kUrlByPathParams): GetFb2kUrlByPathResult;

  /**
   * Build `fb2k://artwork/` URLs for up to 100 entries given as exactly one of `paths` or
   * `items`; giving both or neither fails with `INVALID_PARAMS`. Each row reports its own
   * outcome, so a bad entry does not fail the call.
   * @zh 为最多 100 个条目生成 `fb2k://artwork/` URL，条目由 `paths` 与 `items` 之一给出；两个都给或都不给以 `INVALID_PARAMS` 失败。每行报各自的结果，坏条目不会让整次调用失败。
   */
  getFb2kUrlByPathBatch(params: GetFb2kUrlByPathBatchParams): GetFb2kUrlByPathBatchResult;

  /**
   * Read the lyrics tag of a track. The host's cached track info is used when complete; a local
   * file it has not read is read from disk, and a remote or unrecognised path keeps whatever the
   * cache has. The tags `LYRICS`, `UNSYNCED LYRICS`, `UNSYNCEDLYRICS`, `SYNCEDLYRICS` and
   * `SYNCED LYRICS` are probed in that order and the first non-empty one wins. Without `path`
   * the playing track is used; with nothing playing the call fails with `NO_ACTIVE_ITEM`. A path
   * no track can be made for fails with `NOT_FOUND`, a local file that cannot be read with
   * `OPERATION_FAILED`.
   * @zh 读曲目的歌词标签。宿主缓存的曲目信息齐全时用缓存；没读过的本地文件从磁盘读，远程或未识别的路径只用缓存里有的。按 `LYRICS`、`UNSYNCED LYRICS`、`UNSYNCEDLYRICS`、`SYNCEDLYRICS`、`SYNCED LYRICS` 的顺序探测，第一个非空的胜出。不给 `path` 时用正在播放的曲目；没在播放时以 `NO_ACTIVE_ITEM` 失败。建不出曲目的路径以 `NOT_FOUND` 失败，读不了的本地文件以 `OPERATION_FAILED` 失败。
   */
  getLyrics(params: GetLyricsParams): GetLyricsResult;

  /**
   * Read the album-level tags of a track, plus whether the file embeds any picture and whether
   * it carries a lyrics tag. The tags come from the host's cached track info when complete; a
   * local file it has not read is read from disk, and a remote or unrecognised path keeps
   * whatever the cache has, which can be blank. Without `path` the playing track is used; with
   * nothing playing the call fails with `NO_ACTIVE_ITEM`. A path no track can be made for fails
   * with `NOT_FOUND`, a local file that cannot be read with `OPERATION_FAILED`.
   * @zh 读曲目的专辑级标签，并报文件是否嵌有图片、是否带歌词标签。标签在宿主缓存的曲目信息齐全时取自缓存；没读过的本地文件从磁盘读，远程或未识别的路径只用缓存里有的，可能是空的。不给 `path` 时用正在播放的曲目；没在播放时以 `NO_ACTIVE_ITEM` 失败。建不出曲目的路径以 `NOT_FOUND` 失败，读不了的本地文件以 `OPERATION_FAILED` 失败。
   */
  getMetadata(params: GetMetadataParams): GetMetadataResult;

  /**
   * Read one picture type from several files with the album art extractor, one row per path in
   * the given order; a file without the picture gets a row with `available: false`.
   * @zh 用专辑封面提取器从多个文件读同一种图片，按给定顺序每个路径一行；没有该图片的文件那一行 `available: false`。
   */
  getBatch(params: GetBatchParams): GetBatchResult;

  /**
   * Report which picture types a file embeds and which cover files sit next to it.
   * @zh 报告文件嵌入了哪些图片类型，以及旁边有哪些封面文件。
   */
  getAvailableArtwork(params: GetAvailableArtworkParams): GetAvailableArtworkResult;

  /**
   * List the image files (`.jpg`, `.jpeg`, `.png`, `.gif`, `.bmp`, `.webp`) directly inside a
   * directory. A directory that is missing, is a file or cannot be listed is not an error: it
   * lists no images.
   * @zh 列出目录下直接包含的图片文件（`.jpg`、`.jpeg`、`.png`、`.gif`、`.bmp`、`.webp`）。目录不存在、是文件或列不出内容都不算错误，只是列不出图片。
   */
  getFolderImages(params: GetFolderImagesParams): GetFolderImagesResult;
}

/**
 * A picture type; `cover_front` and `cover_back` are the foobar2000 spellings of `front` and
 * `back`.
 * @zh 图片类型；`cover_front` 与 `cover_back` 是 foobar2000 对 `front` 与 `back` 的写法。
 */
type ArtworkType = 'front' | 'cover_front' | 'back' | 'cover_back' | 'disc' | 'icon' | 'artist';

interface GetCurrentParams {
  /**
   * Picture type.
   * @zh 图片类型。
   * @default "front"
   */
  type?: ArtworkType;
}

interface GetCurrentResult {
  /**
   * Whether a picture was found.
   * @zh 是否找到图片。
   */
  available: boolean;
  /**
   * The requested type.
   * @zh 请求的类型。
   */
  type: string;
  /**
   * Lookup that answered: the now-playing cache, the album art manager, or the extractor.
   * @zh 答复的查法：正在播放缓存、专辑封面管理器或提取器。
   */
  source?: 'now_playing_manager' | 'album_art_manager_v2' | 'extractor';
  /**
   * MIME type detected from the bytes.
   * @zh 从字节判断出的 MIME 类型。
   */
  mimeType?: string;
  /**
   * Picture size in bytes.
   * @zh 图片字节数。
   */
  size?: Int;
  /**
   * `data:<mime>;base64,...` URL of the picture.
   * @zh 图片的 `data:<mime>;base64,...` URL。
   */
  dataUrl?: string;
  /**
   * Why no picture was found.
   * @zh 没找到图片的原因。
   */
  reason?: 'no_track' | 'not_found';
  /**
   * Path of the playing track; reported when no picture was found for it.
   * @zh 正在播放曲目的路径；没为它找到图片时报出。
   */
  path?: string;
}

interface GetByPathParams {
  /**
   * Track path: native, `file://`, or with a `|subsong:N` suffix.
   * @zh 曲目路径：原生路径、`file://`，或带 `|subsong:N` 后缀。
   * @minLength 1
   * @security MediaRead
   */
  path: string;
  /**
   * Picture type.
   * @zh 图片类型。
   * @default "front"
   */
  type?: ArtworkType;
}

interface GetByPathResult {
  /**
   * Whether the file has the picture.
   * @zh 文件是否有这张图片。
   */
  available: boolean;
  /**
   * The requested type.
   * @zh 请求的类型。
   */
  type: string;
  /**
   * The path as given.
   * @zh 原样的路径。
   */
  path: string;
  /**
   * MIME type detected from the bytes.
   * @zh 从字节判断出的 MIME 类型。
   */
  mimeType?: string;
  /**
   * Picture size in bytes.
   * @zh 图片字节数。
   */
  size?: Int;
  /**
   * `data:<mime>;base64,...` URL of the picture.
   * @zh 图片的 `data:<mime>;base64,...` URL。
   */
  dataUrl?: string;
}

interface GetForTrackParams {
  /**
   * Track path: native, `file://`, or with a `|subsong:N` suffix.
   * @zh 曲目路径：原生路径、`file://`，或带 `|subsong:N` 后缀。
   * @minLength 1
   * @security MediaRead
   */
  path: string;
  /**
   * Picture type.
   * @zh 图片类型。
   * @default "front"
   */
  type?: ArtworkType;
}

interface GetForTrackResult {
  /**
   * Whether a picture was found.
   * @zh 是否找到图片。
   */
  available: boolean;
  /**
   * The requested type.
   * @zh 请求的类型。
   */
  type: string;
  /**
   * The path as given.
   * @zh 原样的路径。
   */
  path: string;
  /**
   * MIME type detected from the bytes.
   * @zh 从字节判断出的 MIME 类型。
   */
  mimeType?: string;
  /**
   * Width in pixels from the PNG header; `0` for other formats.
   * @zh 从 PNG 头读出的宽度（像素）；其他格式为 `0`。
   */
  width?: Int;
  /**
   * Height in pixels from the PNG header; `0` for other formats.
   * @zh 从 PNG 头读出的高度（像素）；其他格式为 `0`。
   */
  height?: Int;
  /**
   * Picture size in bytes.
   * @zh 图片字节数。
   */
  size?: Int;
  /**
   * `data:<mime>;base64,...` URL of the picture.
   * @zh 图片的 `data:<mime>;base64,...` URL。
   */
  dataUrl?: string;
}

interface GetByPlaylistItemParams {
  /**
   * Playlist index; negative for the active playlist. An index past the last playlist fails with
   * `INVALID_INDEX`.
   * @zh 播放列表序号；负数表示活动播放列表。超出最后一个播放列表的序号以 `INVALID_INDEX` 失败。
   * @default -1
   */
  playlist?: Int;
  playlistGuid?: PlaylistGuid;
  /**
   * Row index; negative for the first row.
   * @zh 行号；负数表示第一行。
   * @default -1
   */
  index?: Int;
  /**
   * Picture type.
   * @zh 图片类型。
   * @default "front"
   */
  type?: ArtworkType;
}

interface GetByPlaylistItemResult {
  /**
   * Whether a picture was found.
   * @zh 是否找到图片。
   */
  available: boolean;
  /**
   * The requested type.
   * @zh 请求的类型。
   */
  type: string;
  /**
   * The playlist actually read.
   * @zh 实际读的播放列表。
   */
  playlist: Int;
  playlistGuid: ReportedPlaylistGuid;
  /**
   * The row actually read.
   * @zh 实际读的行。
   */
  index: Int;
  /**
   * MIME type detected from the bytes.
   * @zh 从字节判断出的 MIME 类型。
   */
  mimeType?: string;
  /**
   * Picture size in bytes.
   * @zh 图片字节数。
   */
  size?: Int;
  /**
   * `data:<mime>;base64,...` URL of the picture.
   * @zh 图片的 `data:<mime>;base64,...` URL。
   */
  dataUrl?: string;
}

interface GetAvailableTypesParams {
  /**
   * Track path; omitted for the playing track.
   * @zh 曲目路径；省略则取正在播放的曲目。
   * @security MediaRead
   */
  path?: string;
}

interface GetAvailableTypesResult {
  /**
   * The embedded picture types, in probe order.
   * @zh 嵌入的图片类型，按探测顺序。
   */
  types: string[];
}

interface GetFb2kUrlParams {
  /**
   * Picture type.
   * @zh 图片类型。
   * @default "front"
   */
  type?: ArtworkType;
  /**
   * Longest side in pixels the handler scales the picture down to; omitted or `0` keeps the
   * original size.
   * @zh 处理器把图片缩到的最长边（像素）；省略或 `0` 保持原尺寸。
   */
  maxSize?: Int;
}

interface GetFb2kUrlResult {
  /**
   * Whether a track is playing.
   * @zh 是否有正在播放的曲目。
   */
  available: boolean;
  /**
   * The type the URL carries: `front` or `back` for their `cover_` spellings.
   * @zh URL 里带的类型：`cover_` 写法归一为 `front` 或 `back`。
   */
  type: string;
  /**
   * The `fb2k://artwork/?path=...` URL; not a data URL, only the component's WebView2
   * resource handler resolves it.
   * @zh `fb2k://artwork/?path=...` URL；不是 data URL，只有本组件的 WebView2 资源处理器认得它。
   */
  dataUrl?: string;
  /**
   * Why there is no URL; only `no_track` occurs here.
   * @zh 没有 URL 的原因；这里只会出现 `no_track`。
   */
  reason?: 'no_track' | 'not_found';
}

interface GetFb2kUrlByPathParams {
  /**
   * Track path.
   * @zh 曲目路径。
   * @minLength 1
   * @security MediaRead
   */
  path: string;
  /**
   * Picture type.
   * @zh 图片类型。
   * @default "front"
   */
  type?: ArtworkType;
  /**
   * Longest side in pixels the handler scales the picture down to; omitted or `0` keeps the
   * original size.
   * @zh 处理器把图片缩到的最长边（像素）；省略或 `0` 保持原尺寸。
   */
  maxSize?: Int;
}

interface GetFb2kUrlByPathResult {
  /**
   * Always `true`: the URL is built without opening the file.
   * @zh 恒为 `true`：URL 不打开文件就能生成。
   */
  available: boolean;
  /**
   * The type the URL carries: `front` or `back` for their `cover_` spellings.
   * @zh URL 里带的类型：`cover_` 写法归一为 `front` 或 `back`。
   */
  type: string;
  /**
   * The path as given.
   * @zh 原样的路径。
   */
  path: string;
  /**
   * The `fb2k://artwork/?path=...` URL; not a data URL, only the component's WebView2
   * resource handler resolves it.
   * @zh `fb2k://artwork/?path=...` URL；不是 data URL，只有本组件的 WebView2 资源处理器认得它。
   */
  dataUrl: string;
}

/**
 * One entry of `getFb2kUrlByPathBatch`'s `items`.
 * @zh `getFb2kUrlByPathBatch` 的 `items` 里的一条。
 */
interface ArtworkBatchItem {
  /**
   * Track path.
   * @zh 曲目路径。
   */
  path: string;
  /**
   * Picture type for this entry; the batch-wide `type` when omitted.
   * @zh 这一条的图片类型；省略则用整批的 `type`。
   */
  type?: ArtworkType;
  /**
   * Scale limit for this entry; the batch-wide `maxSize` when omitted.
   * @zh 这一条的缩放上限；省略则用整批的 `maxSize`。
   */
  maxSize?: Int;
}

interface GetFb2kUrlByPathBatchParams {
  /**
   * Track paths; exactly one of `paths` and `items`.
   * @zh 曲目路径；`paths` 与 `items` 恰好给一个。
   * @security MediaRead
   */
  paths?: string[];
  /**
   * Entries with their own type or scale limit; exactly one of `paths` and `items`.
   * @zh 带各自类型或缩放上限的条目；`paths` 与 `items` 恰好给一个。
   * @security MediaRead
   * @pathKey path
   */
  items?: ArtworkBatchItem[];
  /**
   * Picture type for entries that do not name their own.
   * @zh 没写自己类型的条目用的图片类型。
   * @default "front"
   */
  type?: ArtworkType;
  /**
   * Longest side in pixels for entries that do not name their own; omitted or `0` keeps the
   * original size.
   * @zh 没写自己上限的条目用的最长边（像素）；省略或 `0` 保持原尺寸。
   */
  maxSize?: Int;
}

/**
 * One row of `getFb2kUrlByPathBatch`.
 * @zh `getFb2kUrlByPathBatch` 的一行。
 */
interface ArtworkUrlRow {
  /**
   * The entry's path.
   * @zh 该条目的路径。
   */
  path: string;
  /**
   * Whether a URL was built.
   * @zh 是否生成了 URL。
   */
  available: boolean;
  /**
   * The type the URL carries.
   * @zh URL 里带的类型。
   */
  type?: string;
  /**
   * The `fb2k://artwork/?path=...` URL.
   * @zh `fb2k://artwork/?path=...` URL。
   */
  dataUrl?: string;
  /**
   * Why no URL was built, as the URL builder's error name such as `invalid_type`.
   * @zh 没生成 URL 的原因，是 URL 生成器的错误名，如 `invalid_type`。
   */
  error?: string;
}

interface GetFb2kUrlByPathBatchResult {
  /**
   * One row per entry, in the given order.
   * @zh 每个条目一行，按给定顺序。
   */
  artworks: ArtworkUrlRow[];
}

interface GetLyricsParams {
  /**
   * Track path; omitted for the playing track.
   * @zh 曲目路径；省略则取正在播放的曲目。
   * @security MediaRead
   */
  path?: string;
}

interface GetLyricsResult {
  /**
   * Whether a non-empty lyrics tag was found.
   * @zh 是否找到非空的歌词标签。
   */
  available: boolean;
  /**
   * The tag that supplied the lyrics.
   * @zh 提供歌词的标签。
   */
  tag?: string;
  /**
   * The lyrics text.
   * @zh 歌词文本。
   */
  lyrics?: string;
  /**
   * `true` when the tag name marks the lyrics as synced.
   * @zh 标签名表明是同步歌词时为 `true`。
   */
  synced?: boolean;
}

interface GetMetadataParams {
  /**
   * Track path; omitted for the playing track.
   * @zh 曲目路径；省略则取正在播放的曲目。
   * @security MediaRead
   */
  path?: string;
}

interface GetMetadataResult {
  /**
   * Always `true`: the track was resolved, whether or not any of its tags could be read.
   * @zh 恒为 `true`：曲目已解析，不管读没读到它的标签。
   */
  available: boolean;
  /**
   * `ALBUM` tag; empty when absent.
   * @zh `ALBUM` 标签；没有时为空。
   */
  album: string;
  /**
   * `ARTIST` values joined with `, ` in tag order.
   * @zh `ARTIST` 各值按标签顺序以 `, ` 连接。
   */
  artist: string;
  /**
   * `ALBUM ARTIST` values joined with `, ` in tag order.
   * @zh `ALBUM ARTIST` 各值按标签顺序以 `, ` 连接。
   */
  albumArtist: string;
  /**
   * `TITLE` tag; empty when absent.
   * @zh `TITLE` 标签；没有时为空。
   */
  title: string;
  /**
   * `DATE` tag as written; empty when absent.
   * @zh 原样的 `DATE` 标签；没有时为空。
   */
  year: string;
  /**
   * `GENRE` values joined with `, ` in tag order.
   * @zh `GENRE` 各值按标签顺序以 `, ` 连接。
   */
  genre: string;
  /**
   * `TRACKNUMBER` tag as written, such as `3` or `3/12`; empty when absent.
   * @zh 原样的 `TRACKNUMBER` 标签，如 `3` 或 `3/12`；没有时为空。
   */
  trackNumber: string;
  /**
   * `DISCNUMBER` tag as written; empty when absent.
   * @zh 原样的 `DISCNUMBER` 标签；没有时为空。
   */
  discNumber: string;
  /**
   * Whether the file embeds any of the five picture types.
   * @zh 文件是否嵌有五种图片类型中的任何一种。
   */
  hasEmbedded: boolean;
  /**
   * Whether any of the five tags recognized by `artwork.getLyrics` exists, even an empty one.
   * @zh 是否存在 `artwork.getLyrics` 识别的五种标签中的任意一种，空的也算。
   */
  hasLyrics: boolean;
}

interface GetBatchParams {
  /**
   * Track paths; a `|subsong:N` suffix is dropped.
   * @zh 曲目路径；`|subsong:N` 后缀被去掉。
   * @security MediaRead
   */
  paths: string[];
  /**
   * Picture type for every path.
   * @zh 每个路径用的图片类型。
   * @default "front"
   */
  type?: ArtworkType;
}

/**
 * One row of `getBatch`.
 * @zh `getBatch` 的一行。
 */
interface ArtworkBatchRow {
  /**
   * The path as given.
   * @zh 原样的路径。
   */
  path: string;
  /**
   * Whether the file has the picture.
   * @zh 文件是否有这张图片。
   */
  available: boolean;
  /**
   * MIME type detected from the bytes.
   * @zh 从字节判断出的 MIME 类型。
   */
  mimeType?: string;
  /**
   * Picture size in bytes.
   * @zh 图片字节数。
   */
  size?: Int;
  /**
   * `data:<mime>;base64,...` URL of the picture.
   * @zh 图片的 `data:<mime>;base64,...` URL。
   */
  dataUrl?: string;
}

interface GetBatchResult {
  /**
   * One row per path, in the given order.
   * @zh 每个路径一行，按给定顺序。
   */
  artworks: ArtworkBatchRow[];
}

interface GetAvailableArtworkParams {
  /**
   * Track path; a `|subsong:N` suffix is dropped.
   * @zh 曲目路径；`|subsong:N` 后缀被去掉。
   * @minLength 1
   * @security MediaRead
   */
  path: string;
}

/**
 * One embedded picture of `getAvailableArtwork`.
 * @zh `getAvailableArtwork` 报的一张嵌入图片。
 */
interface ArtworkAvailableEntry {
  /**
   * Picture type.
   * @zh 图片类型。
   */
  type: string;
  /**
   * Where it comes from; always `embedded`, since only embedded pictures are listed.
   * @zh 来源；恒为 `embedded`，因为只列嵌入的图片。
   */
  source: string;
}

interface GetAvailableArtworkResult {
  /**
   * Whether the file embeds at least one picture.
   * @zh 文件是否至少嵌有一张图片。
   */
  available: boolean;
  /**
   * The embedded pictures, in probe order.
   * @zh 嵌入的图片，按探测顺序。
   */
  artworks: ArtworkAvailableEntry[];
  /**
   * `embedded` once when any picture is embedded, then `folder:<name>` for each cover file
   * (`cover`, `folder`, `front`, `album` as `.jpg` or `.png`) found next to the file.
   * @zh 嵌有图片时先有一个 `embedded`，然后是文件旁每个封面文件（`cover`、`folder`、`front`、`album` 的 `.jpg` 或 `.png`）对应的 `folder:<名字>`。
   */
  sources: string[];
}

interface GetFolderImagesParams {
  /**
   * Directory to list.
   * @zh 要列的目录。
   * @minLength 1
   * @security Read
   */
  directory: string;
}

/**
 * One image file of `getFolderImages`.
 * @zh `getFolderImages` 的一个图片文件。
 */
interface ArtworkFolderImage {
  /**
   * File name.
   * @zh 文件名。
   */
  name: string;
  /**
   * Full path.
   * @zh 完整路径。
   */
  path: string;
  /**
   * File size in bytes.
   * @zh 文件字节数。
   */
  size: Int;
}

interface GetFolderImagesResult {
  /**
   * The image files, in directory order.
   * @zh 图片文件，按目录顺序。
   */
  images: ArtworkFolderImage[];
}
