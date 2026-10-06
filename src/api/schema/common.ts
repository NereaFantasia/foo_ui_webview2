// Types shared by the namespace files in this directory. See README.md.

/**
 * An integer; the C++ side reads it as std::int64_t. JSON numbers with a fraction are rejected.
 * JavaScript numbers hold integers exactly only up to 2^53, so a value that can exceed that
 * (a hash, a 64-bit id) belongs in a string instead.
 */
export type Int = number;

/**
 * Any JSON value, passed through unchanged; the C++ side holds it as nlohmann::json. Only for
 * data the method really does not interpret, such as a value to log or to store as is.
 */
export type Json = unknown;

/**
 * A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every
 * result or event that names a playlist report it; either hex case is accepted. It keeps naming
 * the same playlist while other playlists are added, removed or reordered, which an index does
 * not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A
 * playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another
 * playlist. A malformed GUID and a missing playlist carry the GUID given as
 * `details.playlistGuid`.
 * @zh 播放列表的 GUID，用来代替序号；写法带花括号，与 `playlist.getAll` 以及所有指明播放列表的结果、事件报出的一致；十六进制大小写均可。别的列表增删或重排时它仍指向同一个列表，序号做不到。同时给了它和序号，或 GUID 格式不对，以 `INVALID_PARAMS` 失败。列表已不存在时以 `NOT_FOUND` 失败，不会改用别的列表。GUID 格式不对与列表不存在这两种失败都以 `details.playlistGuid` 带回传入的 GUID。
 */
export type PlaylistGuid = string;

/**
 * The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as
 * `playlistGuid` to address this playlist even after the playlist list has changed.
 * @zh 播放列表的 GUID，写成 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。作为 `playlistGuid` 传入，播放列表清单变了之后仍能指定这个列表。
 */
export type ReportedPlaylistGuid = string;

/**
 * One track, as every method whose declaration returns whole rows reports it: the playing
 * track, a media library row, a playlist row, a queue entry. Containers add their own fields
 * (a row number, a queue position) on top of these.
 * @zh 一首曲目；声明中返回整行曲目的方法（正在播放、媒体库行、播放列表行、队列项）都用这一形状，容器再在它之上加自己的字段（行号、队列位置等）。
 */
export interface Track {
  /**
   * The key that identifies the track across endpoints: `absolutePath`, with a `|subsong:N`
   * suffix when `subsong` is not `0`. The same track yields the same handle from every such
   * method; because
   * of the suffix it is not a plain file path, and a method that takes a path documents whether
   * it accepts one.
   * @zh 跨端点识别同一首曲目的键：`absolutePath`，`subsong` 不为 `0` 时带 `|subsong:N` 后缀。同一首曲目在这些方法里得到同一个 handle；因为带后缀，它不是普通文件路径，接收路径的方法会各自说明是否接受它。
   */
  handle: string;
  /**
   * Path as foobar2000 stores it: `file://` for a local file, `file-relative://` for a path
   * stored relative to the foobar2000 folder (a portable install), or a remote URL. No subsong
   * suffix.
   * @zh foobar2000 存储的路径：本地文件是 `file://`，相对 foobar2000 目录存储的路径（便携安装）是 `file-relative://`，网络流是 URL。不带子曲目后缀。
   */
  path: string;
  /**
   * Native filesystem path without the subsong suffix; the same as `path` for a remote URL.
   * @zh 不带子曲目后缀的原生文件系统路径；网络流与 `path` 相同。
   */
  absolutePath: string;
  /**
   * Subsong identifier the decoder assigns inside the file, not necessarily a sequence number;
   * `0` for a whole file and for a remote stream.
   * @zh 解码器在文件内分配的子曲目标识，不一定是连续序号；整个文件和网络流都是 `0`。
   */
  subsong: Int;
  /**
   * First TITLE value; empty when untagged.
   * @zh 第一个 TITLE 值；没有标签时为空。
   */
  title: string;
  /**
   * Every ARTIST value joined with `", "`; empty when untagged.
   * @zh 全部 ARTIST 值用 `", "` 连接；没有标签时为空。
   */
  artist: string;
  /**
   * Every ARTIST value in tag order; empty when untagged.
   * @zh 全部 ARTIST 值，按标签顺序；没有标签时为空数组。
   */
  artists: string[];
  /**
   * First ALBUM value; empty when untagged.
   * @zh 第一个 ALBUM 值；没有标签时为空。
   */
  album: string;
  /**
   * Every ALBUM ARTIST value joined with `", "`; empty when untagged.
   * @zh 全部 ALBUM ARTIST 值用 `", "` 连接；没有标签时为空。
   */
  albumArtist: string;
  /**
   * Every ALBUM ARTIST value in tag order, so `albumArtists.join(", ")` equals `albumArtist`;
   * empty when untagged. `library.getAlbums` files a track that has an `album` under the name
   * `album` and the album artist `albumArtists[0]`, or `artists[0]` when this array is empty
   * (`""` when both are); those are the `name` and `albumArtist` of that album's row.
   * @zh 全部 ALBUM ARTIST 值，按标签顺序，`albumArtists.join(", ")` 等于 `albumArtist`；没有标签时为空数组。`library.getAlbums` 把有 `album` 的曲目归入名为 `album`、专辑艺术家为 `albumArtists[0]` 的专辑，本数组为空时取 `artists[0]`（两者都空时为 `""`）；这两个值就是该专辑行的 `name` 与 `albumArtist`。
   */
  albumArtists: string[];
  /**
   * Every GENRE value joined with `", "`; empty when untagged.
   * @zh 全部 GENRE 值用 `", "` 连接；没有标签时为空。
   */
  genre: string;
  /**
   * First DATE value as tagged, such as `2019` or `2019-05-01`; empty when untagged.
   * @zh 第一个 DATE 值，按标签原样，如 `2019` 或 `2019-05-01`；没有标签时为空。
   */
  date: string;
  /**
   * TRACKNUMBER read as an integer; `0` when absent or not a number.
   * @zh TRACKNUMBER 按整数读取；缺失或不是数字时为 `0`。
   */
  trackNumber: Int;
  /**
   * DISCNUMBER read as an integer; `0` when absent or not a number.
   * @zh DISCNUMBER 按整数读取；缺失或不是数字时为 `0`。
   */
  discNumber: Int;
  /**
   * Length in seconds; `0` when unknown.
   * @zh 时长，单位秒；未知时为 `0`。
   */
  duration: number;
  /**
   * File size in bytes; `-1` when unknown, as for a remote stream.
   * @zh 文件大小，单位字节；未知（如网络流）时为 `-1`。
   */
  fileSize: Int;
  /**
   * Average bitrate in kbit/s; `0` when unknown.
   * @zh 平均码率，单位 kbit/s；未知时为 `0`。
   */
  bitrate: Int;
  /**
   * Sample rate in Hz; `0` when unknown.
   * @zh 采样率，单位 Hz；未知时为 `0`。
   */
  sampleRate: Int;
  /**
   * Channel count; `0` when unknown.
   * @zh 声道数；未知时为 `0`。
   */
  channels: Int;
  /**
   * Codec name as the decoder reports it, such as `FLAC` or `MP3`; empty when unknown.
   * @zh 解码器报告的编码名，如 `FLAC`、`MP3`；未知时为空。
   */
  codec: string;
  /**
   * Rating from 0 to 5: the `%rating%` statistic (foo_playcount) when it is 1 to 5, otherwise
   * the RATING tag clamped to that range; `0` when neither rates the track.
   * @zh 评分 0 到 5：`%rating%` 统计值（foo_playcount）在 1 到 5 之间时用它，否则用 RATING 标签并夹到该范围；两处都没有评分时为 `0`。
   */
  rating: Int;
}

/**
 * A track projected down to the fields a caller asked for.
 * @zh 按调用方要求的字段投影后的曲目。
 */
export type TrackPartial = Partial<Track>;

/**
 * One registered method.
 * @zh 一个已注册的方法。
 */
export interface SystemApiInfo {
  /**
   * Full method name, `namespace.method`.
   * @zh 完整方法名，形如 `namespace.method`。
   */
  fullName: string;
  /**
   * Display name of the owning plugin; `foo_ui_webview2` for built-in methods.
   * @zh 所属插件的显示名；内置方法为 `foo_ui_webview2`。
   */
  plugin: string;
  /**
   * Namespace part of the name.
   * @zh 名字里的命名空间部分。
   */
  namespace: string;
  /**
   * Method part of the name.
   * @zh 名字里的方法部分。
   */
  method: string;
  /**
   * Description given at registration; empty when none.
   * @zh 注册时给的描述；没有时为空。
   */
  description: string;
  /**
   * Version given at registration.
   * @zh 注册时给的版本。
   */
  version: string;
  /**
   * `true` when an external plugin registered it.
   * @zh 由外部插件注册时为 `true`。
   */
  isExternal: boolean;
}

/**
 * One registered external plugin.
 * @zh 一个已注册的外部插件。
 */
export interface SystemPluginInfo {
  /**
   * Display name.
   * @zh 显示名。
   */
  name: string;
  /**
   * Namespace the plugin owns; unique among plugins.
   * @zh 插件拥有的命名空间；插件之间唯一。
   */
  namespace: string;
  /**
   * Plugin version.
   * @zh 插件版本。
   */
  version: string;
  /**
   * Author.
   * @zh 作者。
   */
  author: string;
  /**
   * Description.
   * @zh 描述。
   */
  description: string;
  /**
   * Number of entries in `apis`.
   * @zh `apis` 的条数。
   */
  apiCount: Int;
  /**
   * Full names of the methods the plugin registered.
   * @zh 插件注册的方法全名。
   */
  apis: string[];
}
