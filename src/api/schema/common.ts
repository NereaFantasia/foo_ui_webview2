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
 * Operation completion status: `completed` when all requested operations finished successfully;
 * `partial` when only some operations succeeded; `cancelled` when cancelled by user or caller;
 * `failed` when no operation succeeded; `unknown` when host cannot determine status (such as menu commands without completion feedback).
 * @zh 操作完成状态：`completed` 表示全部请求项已成功完成；`partial` 表示部分项完成、其余未完成；`cancelled` 表示操作被用户或调用方取消；`failed` 表示全部项均未成功；`unknown` 表示宿主无法获取执行结果（如未提供状态回报的菜单命令）。
 */
export type OperationStatus = 'completed' | 'partial' | 'cancelled' | 'failed' | 'unknown';

/**
 * Metadata and playback properties of a single track, shared by playback, library,
 * playlist, and queue endpoints. Containers extend this structure with domain-specific fields
 * such as playlist row indices or queue positions.
 * @zh 曲目基础元数据与回放属性，由播放状态、媒体库、播放列表与播放队列等端点通用返回。具体容器可在本结构基础上扩展行索引或队列位次等专用字段
 */
export interface Track {
  /**
   * Cross-endpoint unique track identifier: formatted as `absolutePath`, appended with `|subsong:N`
   * when `subsong` is non-zero. The same track yields the same handle from every method. Differs from standard filesystem paths; endpoints document
   * whether handle identifiers are accepted in place of paths.
   * @zh 跨端点曲目唯一标识：格式为 `absolutePath`，`subsong` 非 0 时追加 `|subsong:N` 后缀。同一首曲目在所有方法中的 handle 相同。因包含后缀故不同于普通文件路径，各路径参数接口均单独说明是否接受此标识
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
   * @zh 原始专辑名或专辑艺术家含非法 UTF-8 时提供的不透明身份标识。出现此标识时优先据此匹配专辑行，避免依赖显示名称；标准名称不包含此字段
   */
  albumId?: string;
  /**
   * Every ALBUM ARTIST value joined with `", "`; empty when untagged.
   * @zh 全部 ALBUM ARTIST 值用 `", "` 连接；没有标签时为空。
   */
  albumArtist: string;
  /**
   * All ALBUM ARTIST values in tag order (`albumArtists.join(", ")` equals `albumArtist`);
   * empty array when untagged. In `library.getAlbums`, a track that has an `album` is filed under
   * the name `album` and the album artist `albumArtists[0]` (falling back to `artists[0]`, or `""`
   * when both are empty); those are the `name` and `albumArtist` of that album's row. When `albumId`
   * is present, match by `albumId` instead of display names to avoid collision from character replacement.
   * @zh 按标签顺序排列的全部 ALBUM ARTIST 值，`albumArtists.join(", ")` 等于 `albumArtist`；无标签时为空数组。`library.getAlbums` 把有 `album` 的曲目归入名为 `album`、专辑艺术家为 `albumArtists[0]` 的专辑（本数组为空时回退至 `artists[0]`，两者都空时为 `""`）；这两个值即该专辑行的 `name` 与 `albumArtist`。存在 `albumId` 时优先按其匹配，避免替换字符导致显示名冲突
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
   * Rating value from 0 to 5. Prefers the `%rating%` statistic (foo_playcount) if within 1..5;
   * otherwise falls back to the RATING tag clamped to 0..5; `0` if neither is present.
   * @zh 评分（0 到 5）。优先读取 1..5 范围内的 `%rating%` 统计字段（foo_playcount）；否则回退至 RATING 标签并钳位到 0..5；两处均未提供时为 0
   */
  rating: Int;
}

/**
 * Track representation containing a subset of fields requested by the caller.
 * @zh 按请求字段投影的曲目子集对象
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
   * Namespace component of the method name.
   * @zh 方法所属命名空间
   */
  namespace: string;
  /**
   * Method name component without namespace.
   * @zh 不含命名空间的方法名
   */
  method: string;
  /**
   * Method description provided during registration; empty string if omitted.
   * @zh 注册时声明的方法描述；未提供时为空字符串
   */
  description: string;
  /**
   * Method API version declared during registration.
   * @zh 注册时声明的 API 版本
   */
  version: string;
  /**
   * Whether the method was registered by an external plugin.
   * @zh 是否由外部插件注册
   */
  isExternal: boolean;
}

/**
 * One registered external plugin.
 * @zh 一个已注册的外部插件。
 */
export interface SystemPluginInfo {
  /**
   * Plugin display name.
   * @zh 插件显示名称
   */
  name: string;
  /**
   * Namespace unique to this plugin.
   * @zh 插件独占的命名空间标识
   */
  namespace: string;
  /**
   * Plugin version string.
   * @zh 插件版本字符串
   */
  version: string;
  /**
   * Plugin author.
   * @zh 插件作者
   */
  author: string;
  /**
   * Plugin description.
   * @zh 插件功能描述
   */
  description: string;
  /**
   * Number of methods in `apis`.
   * @zh 插件注册的方法总数
   */
  apiCount: Int;
  /**
   * Full method names registered by the plugin.
   * @zh 插件注册的方法完整全名列表
   */
  apis: string[];
}
