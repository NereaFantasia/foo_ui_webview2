import type { Int, Json } from './common.js';

export interface Api {
  /**
   * Read non-empty lyrics from the five known lyrics tags, then files next to the audio.
   * Automatic lookup tries the audio file's stem only for a verified single-track file,
   * followed by `<first artist> - <title>`; subsongs use only the latter. Unknown input
   * layouts skip the audio-stem candidate. No numbered or shared album fallback is used.
   * Results are cached for 1200 ms per track, filename and filters. Successful file saves
   * and metadata changes invalidate the cache.
   * @zh 先从五种已知歌词标签读取非空歌词，再查音频旁的文件。自动查找仅在确认整文件单曲时尝试音频同名文件，然后尝试 `<首个艺人值> - <曲名>`；子曲目只尝试后者。无法确认输入结构时跳过音频同名候选，不查编号文件或整张镜像共享文件。按曲目、文件名与过滤条件缓存 1200 ms，文件保存成功和元数据变更会使缓存失效。
   */
  get(params: GetParams): GetResult;

  /**
   * List the known lyrics tags and sidecar files that exist for a track. File candidates
   * and access checks match `lyrics.get`, but contents are not read: empty tags and files
   * still count as existing.
   * @zh 列出曲目存在的已知歌词标签与外挂文件。文件候选和访问检查与 `lyrics.get` 一致，但不读取歌词内容：空标签与空文件仍算存在。
   */
  exists(params: ExistsParams): ExistsResult;

  /**
   * Save lyrics to a sidecar file, an embedded tag, or both. A file save invalidates the read
   * cache; an embedded save dispatches an asynchronous write and metadata changes invalidate
   * the cache. Without `filename`, verified single-track files use the audio stem; other
   * tracks use `<first artist> - <title>`. Missing tags in the latter case fail that target
   * with `INVALID_PARAMS`. Existing files are overwritten, so use distinct filenames when
   * tracks have the same artist and title. With one target the result describes that write;
   * with several, each outcome is under `results` and `OPERATION_FAILED` means all failed.
   * @zh 把歌词保存到音频旁的外挂文件、内嵌标签，或两处。文件保存使读取缓存失效；内嵌保存异步派发写入，元数据变更后使缓存失效。不指定 `filename` 时，确认整文件单曲后使用音频同名文件，其余使用 `<首个艺人值> - <曲名>`；后者缺少标签时，该目标以 `INVALID_PARAMS` 失败。已有文件会被覆盖，艺人与曲名相同的不同曲目应指定不同文件名。单个目标时结果描述这次写入；多个目标时各结果在 `results` 下，全部失败才以 `OPERATION_FAILED` 失败。
   */
  save(params: SaveParams): SaveResult;
}

interface GetParams {
  /**
   * Track path; a `|subsong:N` suffix selects a subsong. Omit it for the playing track; with
   * nothing playing the call fails with `NO_ACTIVE_ITEM`. The playing track keeps its subsong.
   * @zh 曲目路径，带 `|subsong:N` 时选子曲目。省略则取正在播放的曲目并保留其子曲目标识；没在播放时以 `NO_ACTIVE_ITEM` 失败。
   * @minLength 1
   * @security MediaRead
   */
  path?: string;
  /**
   * Exact file name, including any extension, next to the audio. A non-empty value replaces
   * all automatic file candidates and ignores `format`; `source` and `type` still apply.
   * Empty means automatic lookup. Must be a valid single Windows filename, without path
   * syntax, device names or trailing dots/spaces; invalid names fail with `INVALID_PARAMS`.
   * @zh 音频旁文件的精确名称，含扩展名。非空时替代全部自动文件候选，并忽略 `format`；`source` 与 `type` 仍生效。空字符串表示自动查找。必须是合法的单个 Windows 文件名，不含路径语法、设备保留名或尾随句点与空格；非法名称以 `INVALID_PARAMS` 失败。
   */
  filename?: string;
  /**
   * Where to look: the embedded tags, the sidecar files, or both in that order.
   * @zh 查找范围：内嵌标签、外挂文件，或按此顺序两者都查。
   * @default "any"
   */
  source?: 'embedded' | 'file' | 'any';
  /**
   * Keep only synced lyrics (with LRC timestamps) or only unsynced ones.
   * @zh 只要同步歌词（带 LRC 时间戳）或只要非同步歌词。
   * @default "any"
   */
  type?: 'synced' | 'unsynced' | 'any';
  /**
   * Sidecar extension to try; `.lrc` candidates are tried before `.txt` with `any`. Applies to
   * automatic sidecar lookup only; a non-empty `filename` takes precedence.
   * @zh 自动查找的外挂文件扩展名；`any` 时先试遍 `.lrc` 再试 `.txt`。只影响自动查找，非空 `filename` 优先。
   * @default "any"
   */
  format?: 'lrc' | 'txt' | 'any';
}

interface GetResult {
  /**
   * Whether lyrics were found.
   * @zh 是否找到歌词。
   */
  available: boolean;
  /**
   * The path looked up: as given, or the playing track's path including its subsong suffix.
   * @zh 查的路径：原样给出的，或正在播放曲目的路径（保留子曲目后缀）。
   */
  path: string;
  /**
   * Where the lyrics came from; present when `available` is `true`.
   * @zh 歌词的来源；`available` 为 `true` 时出现。
   */
  source?: 'embedded' | 'file';
  /**
   * Full path of the sidecar file; present when `source` is `file`.
   * @zh 外挂文件的完整路径；`source` 为 `file` 时出现。
   */
  sourcePath?: string;
  /**
   * The tag that matched a `type` filter, one of `LYRICS`, `UNSYNCED LYRICS`, `UNSYNCEDLYRICS`,
   * `SYNCEDLYRICS`, `SYNCED LYRICS`; present when `source` is `embedded`, `type` was not `any`
   * and a known tag matched.
   * @zh 命中 `type` 过滤的标签，`LYRICS`、`UNSYNCED LYRICS`、`UNSYNCEDLYRICS`、`SYNCEDLYRICS`、`SYNCED LYRICS` 之一；`source` 为 `embedded`、`type` 不是 `any` 且命中已知标签时出现。
   */
  tagName?: string;
  /**
   * The lyrics text; present when `available` is `true`.
   * @zh 歌词文本；`available` 为 `true` 时出现。
   */
  lyrics?: string;
  /**
   * Whether the text carries LRC timestamps; present when `available` is `true`.
   * @zh 文本是否带 LRC 时间戳；`available` 为 `true` 时出现。
   */
  synced?: boolean;
}

interface ExistsParams {
  /**
   * Track path; a `|subsong:N` suffix selects a subsong.
   * @zh 曲目路径，带 `|subsong:N` 时选子曲目。
   * @minLength 1
   * @security MediaRead
   */
  path: string;
  /**
   * Exact file name, including any extension, next to the audio. Replaces the automatic
   * file candidates; embedded tags are still checked. Empty means automatic lookup.
   * The same Windows filename rules and `INVALID_PARAMS` failure as `lyrics.get` apply.
   * @zh 音频旁文件的精确名称，含扩展名，替代自动文件候选；仍检查内嵌标签。空字符串表示自动查找。文件名校验与 `lyrics.get` 相同，非法名称以 `INVALID_PARAMS` 失败。
   */
  filename?: string;
}

interface ExistsResult {
  /**
   * Whether any source has lyrics.
   * @zh 是否有任何来源带歌词。
   */
  exists: boolean;
  /**
   * Every source found: `embedded` for a lyrics tag, then `file:<name>` for each sidecar file,
   * in the same candidate order as `lyrics.get` with `format=any`, or just the explicit file.
   * @zh 找到的全部来源：有已知歌词标签为 `embedded`，每个外挂文件为 `file:<文件名>`；候选顺序与 `lyrics.get` 的 `format=any` 一致，指定文件名时只检查该文件。
   */
  sources: string[];
}

interface SaveParams {
  /**
   * Track path the lyrics belong to; a `|subsong:N` suffix selects a subsong.
   * @zh 歌词所属的曲目路径；带 `|subsong:N` 时选子曲目。
   * @minLength 1
   * @security MediaWrite
   */
  path: string;
  /**
   * Lyrics text to save.
   * @zh 要保存的歌词文本。
   * @minLength 1
   */
  lyrics: string;
  /**
   * Where to write: `file` (a sidecar next to the audio file), `embedded` (a tag in the file),
   * or `all` for both; several entries write to each. Any other value fails with
   * `INVALID_PARAMS`. Omitted means `file`.
   * @zh 写到哪里：`file`（音频旁的外挂文件）、`embedded`（文件里的标签），或 `all` 表示两处；给多项就各写一处。其他值以 `INVALID_PARAMS` 失败，省略表示 `file`。
   * @minItems 1
   */
  target?: string[];
  /**
   * Exact file name, including any extension, for the `file` target. A non-empty value
   * replaces the derived name and ignores `format`; empty means the default name.
   * The same Windows filename rules and `INVALID_PARAMS` failure as `lyrics.get` apply.
   * @zh `file` 目标的精确文件名，含扩展名。非空时替代推导名称并忽略 `format`，空字符串表示默认名称。文件名校验与 `lyrics.get` 相同，非法名称以 `INVALID_PARAMS` 失败。
   */
  filename?: string;
  /**
   * Tag the `embedded` target writes.
   * @zh `embedded` 目标写的标签。
   * @default "LYRICS"
   */
  tagName?: string;
  /**
   * Extension for the default sidecar name; ignored when `filename` is non-empty.
   * @zh 默认外挂文件名的扩展名；非空 `filename` 时忽略。
   * @default "lrc"
   */
  format?: 'lrc' | 'txt';
}

interface SaveResult {
  /**
   * Full path of the file written; present for a single `file` target.
   * @zh 写出的文件的完整路径；单个 `file` 目标时出现。
   */
  savedTo?: string;
  /**
   * `true` when a single `embedded` target dispatched the tag write; the final outcome arrives as
   * `metadata:writeComplete`.
   * @zh 单个 `embedded` 目标已派发标签写入时为 `true`；最终结果由 `metadata:writeComplete` 事件报告。
   */
  dispatched?: boolean;
  /**
   * The path as given; present for a single `embedded` target.
   * @zh 原样的路径；单个 `embedded` 目标时出现。
   */
  path?: string;
  /**
   * Path of the handle the tag write targets; present for a single `embedded` target.
   * @zh 标签写入所针对句柄的路径；单个 `embedded` 目标时出现。
   */
  handlePath?: string;
  /**
   * Subsong the tag write targets; present for a single `embedded` target.
   * @zh 标签写入所针对的子曲目；单个 `embedded` 目标时出现。
   */
  subsong?: Int;
  /**
   * The tag written, keyed by its upper-case name; present for a single `embedded` target.
   * @zh 写入的标签，键是大写标签名；单个 `embedded` 目标时出现。
   */
  tagsApplied?: Record<string, string>;
  /**
   * Number of tags set; present for a single `embedded` target.
   * @zh 设置的标签数；单个 `embedded` 目标时出现。
   */
  tagsSet?: Int;
  /**
   * Number of tags removed; present for a single `embedded` target.
   * @zh 删除的标签数；单个 `embedded` 目标时出现。
   */
  tagsRemoved?: Int;
  /**
   * A note about the dispatch; present for a single `embedded` target.
   * @zh 关于派发的说明；单个 `embedded` 目标时出现。
   */
  note?: string;
  /**
   * With several targets: each target's own envelope (`{ success, savedTo }`, the tag write
   * receipt, or `{ success: false, error, code }`) under `file` or `embedded`.
   * @zh 多个目标时：每个目标自己的信封（`{ success, savedTo }`、标签写入回执，或 `{ success: false, error, code }`），键为 `file` 或 `embedded`。
   */
  results?: Record<string, Json>;
}
