import type { Int } from './common.js';

export interface Events {
  /**
   * The metadata of tracks changed: tags were written or read again, or a component such as
   * `foo_playcount` asked foobar2000 to show its fields anew (`fromHook`). Each entry names its
   * track by `handle`, the key the track rows of other methods carry, so a single track of a cue
   * sheet or a multi-track file can be told from the rest of the file.
   * @zh 曲目的元数据变了：标签被写入或重新读取，或 `foo_playcount` 之类的组件请 foobar2000 刷新它提供的字段（`fromHook`）。每一条用 `handle` 指明曲目，与其他方法返回的曲目行里的 `handle` 是同一个键，所以能从 cue 或多曲目文件里分辨出具体是哪一首。
   * @delivery broadcast
   */
  changed: ChangedPayload;
}

interface ChangedPayload {
  /**
   * Up to 50 of the changed tracks, in no particular order. An invalid entry is skipped, so the
   * list can hold fewer than the smaller of `count` and 50.
   * @zh 最多 50 首变化的曲目，顺序不固定。无效条目会被跳过，所以条数可能少于 `count` 与 50 中的较小者。
   */
  tracks: MetadbChangedTrackItem[];
  /**
   * How many tracks changed, at least `1`; can exceed the length of `tracks`.
   * @zh 变化的曲目数，至少为 `1`；可能超过 `tracks` 的长度。
   */
  count: Int;
  /**
   * `true` when the files themselves did not change and a component such as `foo_playcount`
   * asked for its fields to be shown anew.
   * @zh 文件本身没变、而是 `foo_playcount` 之类的组件请求刷新它提供的字段时为 `true`。
   */
  fromHook: boolean;
  /**
   * When the event was sent, in milliseconds since the Unix epoch.
   * @zh 事件发出的时间，单位为自 Unix 纪元起的毫秒数。
   */
  timestamp: Int;
}

/**
 * One changed track in `metadb:changed`.
 * @zh `metadb:changed` 里的一首变化的曲目。
 */
interface MetadbChangedTrackItem {
  /**
   * The track's key, built the way a track row's `handle` is: the native path (foobar2000's own
   * path for a location that has none), with `|subsong:N` when `subsong` is not `0`. It equals
   * the `handle` of the same track in rows from `library.query`, `playlist.getTracks` and the
   * like.
   * @zh 曲目的键，构成方式与曲目行的 `handle` 相同：原生路径（没有原生形式的位置用 foobar2000 自己的路径），`subsong` 不为 `0` 时带 `|subsong:N`。与同一首曲目在 `library.query`、`playlist.getTracks` 等方法返回的行里的 `handle` 相等。
   */
  handle: string;
  /**
   * The file path in native form, or foobar2000's own path for a location that has none, such
   * as a stream. It carries no subsong, so every track of a cue sheet or a multi-track file has
   * the same `path`; `handle` tells them apart.
   * @zh 原生形式的文件路径；没有原生形式的位置（如网络流）给 foobar2000 自己的路径。不带 subsong，所以 cue 或多曲目文件里的各首 `path` 相同，要靠 `handle` 区分。
   */
  path: string;
  /**
   * Subsong identifier inside the file, as a track row's `subsong`; `0` for a whole file.
   * @zh 文件内的子曲目标识，与曲目行的 `subsong` 相同；整个文件为 `0`。
   */
  subsong: Int;
  /**
   * Rating `0` to `5`, `0` for unrated, read the same way as `rating.get`. When foobar2000
   * holds no information for the track, only the playback statistics are read, and the key is
   * absent unless they carry a rating.
   * @zh 评分 `0` 到 `5`，`0` 为未评分，读取规则与 `rating.get` 相同。foobar2000 没有该曲目的信息时只读播放统计，统计里没有评分就不带。
   */
  rating?: Int;
  /**
   * The file's `PLAY_COUNT` tag, when it has one; not the playback statistics.
   * @zh 文件的 `PLAY_COUNT` 标签，有这个标签时才带；不是播放统计。
   */
  playCount?: Int;
  /**
   * The first `TITLE` value, when the file has one.
   * @zh 第一个 `TITLE` 值，有这个标签时才带。
   */
  title?: string;
  /**
   * All `ARTIST` values joined with `, `, when the file has any.
   * @zh 全部 `ARTIST` 值以 `, ` 连接，有这个标签时才带。
   */
  artist?: string;
}
