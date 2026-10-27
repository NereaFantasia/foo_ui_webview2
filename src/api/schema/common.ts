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
 * Persistent unique identifier of a playlist (bracketed GUID, case-insensitive), used in place
 * of a mutable index. Mutually exclusive with `playlist`; passing both or an invalid format fails
 * with `INVALID_PARAMS`. Fails with `NOT_FOUND` if the playlist does not exist, never falling
 * back. Invalid-format and not-found failures return the given GUID in `details.playlistGuid`.
 * @zh 播放列表的持久唯一标识（带花括号 GUID，大小写均可）。用于替代易变序号。与序号互斥，同时传入或格式非法以 `INVALID_PARAMS` 失败。列表不存在时以 `NOT_FOUND` 失败，不回退至其他列表。格式非法或列表不存在时，错误详情均带回 `details.playlistGuid`。
 */
export type PlaylistGuid = string;

/**
 * Unique identifier of the playlist, formatted as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass
 * as `playlistGuid` to reliably target this playlist.
 * @zh 播放列表的唯一标识，格式为 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。后续可作为 `playlistGuid` 稳定引用该列表。
 */
export type ReportedPlaylistGuid = string;

/**
 * Opaque version token for playlist tracks or queue items, comparable only for equality. Changes
 * whenever items are added, removed, reordered, or replaced, and for the queue also when
 * playback consumes its next entry; selection, focus, renames, and tag edits do not affect it.
 * Invalid across process restarts.
 * @zh 曲目行集合或队列的不透明版本号，仅支持相等性比较。条目增删、重排或替换时均会变化，队列在播放取走下一项时也会变化；选择、焦点、重命名与标签编辑不影响此值。进程重启后旧版本永久失效。
 */
export type ContentVersion = string;

/**
 * Expected content version for optimistic concurrency control. When mismatched, mutations are
 * aborted with `STALE_TARGET` (`details.current` carries current version). Omitted, no check is made.
 * @zh 预期内容版本，用于乐观并发控制。版本不匹配时不执行变更并以 `STALE_TARGET` 失败（`details.current` 返回当前最新版本）。省略时不校验。
 */
export type ExpectedContentVersion = string;

/**
 * How an operation ended: `completed` when everything it was asked to do was done; `partial` when
 * some of it was done and the rest was not; `cancelled` when the user or the caller stopped it;
 * `failed` when none of it was done; `unknown` when the host cannot tell, as with commands that
 * report nothing back.
 * @zh 操作如何结束：`completed` 要求的都做完了；`partial` 做了一部分，其余没做成；`cancelled` 被用户或调用方停下；`failed` 什么也没做成；`unknown` 宿主无从得知，例如不回报结果的命令。
 */
export type OperationStatus = 'completed' | 'partial' | 'cancelled' | 'failed' | 'unknown';

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
   * Opaque album identity when its original name or album artist contains invalid UTF-8.
   * Use this instead of the display names to match an album row; absent for ordinary names.
   * @zh 原始专辑名或专辑艺术家含非法 UTF-8 时提供的不透明身份标识。此时用它匹配专辑行，不用显示名称；普通名称不带此字段。
   */
  albumId?: string;
  /**
   * Every ALBUM ARTIST value joined with `", "`; empty when untagged.
   * @zh 全部 ALBUM ARTIST 值用 `", "` 连接；没有标签时为空。
   */
  albumArtist: string;
  /**
   * Every ALBUM ARTIST value in tag order, so `albumArtists.join(", ")` equals `albumArtist`;
   * empty when untagged. `library.getAlbums` files a track that has an `album` under the name
   * `album` and the album artist `albumArtists[0]`, or `artists[0]` when this array is empty
   * (`""` when both are); those are the `name` and `albumArtist` of that album's row. When
   * `albumId` is present, use it instead: replacement characters can make display names equal.
   * @zh 全部 ALBUM ARTIST 值，按标签顺序，`albumArtists.join(", ")` 等于 `albumArtist`；没有标签时为空数组。`library.getAlbums` 把有 `album` 的曲目归入名为 `album`、专辑艺术家为 `albumArtists[0]` 的专辑，本数组为空时取 `artists[0]`（两者都空时为 `""`）；这两个值就是该专辑行的 `name` 与 `albumArtist`。存在 `albumId` 时应改用它匹配，因为替换字符可能让不同原始名称显示相同。
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
