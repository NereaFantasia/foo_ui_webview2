import type { Int, Json } from './common.js';

export interface Api {
  /**
   * Read non-empty lyrics from the five known lyrics tags, then files next to the audio.
   * Automatic lookup tries the audio file's stem only for a verified single-track file,
   * followed by `<first artist> - <title>`; subsongs use only the latter. Unknown input
   * layouts skip the audio-stem candidate. No numbered or shared album fallback is used.
   * Results are cached for 1200 ms per track, filename and filters. Successful file saves
   * and metadata changes invalidate the cache. Invalid UTF-8 in the returned lyrics is
   * replaced with U+FFFD; no encoding is guessed and the source is not rewritten.
   * @zh 读取曲目的非空歌词文本（依次检索五种已知内嵌标签及音频同目录下的外挂歌词文件）；自动查找时，整文件单曲优先匹配音频同名文件并回退至 `<首个艺人值> - <曲名>`（子曲目仅匹配后者，未知容器结构自动跳过同名候选且不匹配序号分轨或镜像共享文件）；基于曲目路径、文件名与过滤参数缓存 1200 毫秒（文件写入成功或元数据变更自动失效缓存）；返回歌词中的非法 UTF-8 替换为 U+FFFD（不猜测字符编码且不改写源文件）
   */
  get(params: GetParams): GetResult;

  /**
   * List the known lyrics tags and sidecar files that exist for a track. File candidates
   * and access checks match `lyrics.get`, but contents are not read: empty tags and files
   * still count as existing.
   * @zh 检测曲目关联的已知内嵌歌词标签与同目录下外挂歌词文件是否存在；文件探测规则与路径安全检查同 `lyrics.get`（不读取歌词实际文本内容，空标签或空文件仍计入存在）
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
   * @zh 将歌词保存至音频同目录下的外挂文件、音频内嵌元数据标签或同时写入两处；文件写入成功使读取缓存失效，内嵌标签写入采用异步派发并在元数据落盘后使缓存失效；省略 `filename` 时，整文件单曲使用音频同名文件，其余曲目使用 `<首个艺人值> - <曲名>`（缺少对应标签时该目标以 `INVALID_PARAMS` 失败）；目标文件存在时直接覆盖（同艺人同曲名的不同曲目应显式指定文件名以防覆盖冲突）；指定单个目标时直接返回该目标的写入状态，指定多个目标时在 `results` 下分别记录各自结果（仅当所有目标均失败时整次调用以 `OPERATION_FAILED` 失败）
   */
  save(params: SaveParams): SaveResult;
}

interface GetParams {
  /**
   * Track path; a `|subsong:N` suffix selects a subsong. Omit it for the playing track; with
   * nothing playing the call fails with `NO_ACTIVE_ITEM`. The playing track keeps its subsong.
   * @zh 目标曲目路径（支持使用 `|subsong:N` 后缀指定子曲目）；省略时默认查询当前正在播放的曲目（保留其子曲目标识），未处于播放状态时以 `NO_ACTIVE_ITEM` 失败
   * @minLength 1
   * @security MediaRead
   */
  path?: string;
  /**
   * Exact file name, including any extension, next to the audio. A non-empty value replaces
   * all automatic file candidates and ignores `format`; `source` and `type` still apply.
   * Empty means automatic lookup. Must be a valid single Windows filename, without path
   * syntax, device names or trailing dots/spaces; invalid names fail with `INVALID_PARAMS`.
   * @zh 音频同目录下的精确外挂文件名（包含扩展名）；非空时覆盖全部自动候选并忽略 `format`（`source` 与 `type` 过滤规则仍生效），空字符串代表启用自动查找；必须为合法的单个 Windows 文件名（禁止包含路径分隔符、设备保留名或尾随句点空格），非法格式以 `INVALID_PARAMS` 失败
   */
  filename?: string;
  /**
   * Where to look: the embedded tags, the sidecar files, or both in that order.
   * @zh 歌词检索来源范围：`embedded`（仅内嵌标签）、`file`（仅外挂文件）或 `any`（按先内嵌后外挂顺序均检索）
   * @default "any"
   */
  source?: 'embedded' | 'file' | 'any';
  /**
   * Keep only synced lyrics (with LRC timestamps) or only unsynced ones.
   * @zh 歌词时间戳类型过滤：`synced`（仅带 LRC 时间戳的同步歌词）、`unsynced`（仅无时间戳歌词）或 `any`（不作过滤）
   * @default "any"
   */
  type?: 'synced' | 'unsynced' | 'any';
  /**
   * Sidecar extension to try; `.lrc` candidates are tried before `.txt` with `any`. Applies to
   * automatic sidecar lookup only; a non-empty `filename` takes precedence.
   * @zh 自动查找外挂文件时匹配的扩展名（`any` 时优先遍历 `.lrc` 候选再匹配 `.txt` 候选；仅在自动查找生效，显式指定非空 `filename` 时忽略本参数）
   * @default "any"
   */
  format?: 'lrc' | 'txt' | 'any';
}

interface GetResult {
  /**
   * Whether lyrics were found.
   * @zh 是否成功查得有效歌词
   */
  available: boolean;
  /**
   * The path looked up: as given, or the playing track's path including its subsong suffix.
   * @zh 实际查询的曲目路径（传入的原样路径或当前播放曲目的路径，包含子曲目后缀）
   */
  path: string;
  /**
   * Where the lyrics came from; present when `available` is `true`.
   * @zh 胜出的歌词来源类型（`embedded` 或 `file`；仅在 `available` 为 `true` 时返回）
   */
  source?: 'embedded' | 'file';
  /**
   * Full path of the sidecar file; present when `source` is `file`.
   * @zh 匹配的外挂歌词文件绝对路径（仅在 `source` 为 `file` 时返回）
   */
  sourcePath?: string;
  /**
   * The tag that matched a `type` filter, one of `LYRICS`, `UNSYNCED LYRICS`, `UNSYNCEDLYRICS`,
   * `SYNCEDLYRICS`, `SYNCED LYRICS`; present when `source` is `embedded`, `type` was not `any`
   * and a known tag matched.
   * @zh 命中过滤条件的具体内嵌标签名称（`LYRICS`、`UNSYNCED LYRICS`、`UNSYNCEDLYRICS`、`SYNCEDLYRICS` 或 `SYNCED LYRICS`；仅在 `source` 为 `embedded`、指定了 `type` 过滤且命中已知标签时返回）
   */
  tagName?: string;
  /**
   * The lyrics text; present when `available` is `true`.
   * @zh 读取到的完整歌词文本（非法 UTF-8 字符已替换为 U+FFFD；仅在 `available` 为 `true` 时返回）
   */
  lyrics?: string;
  /**
   * Whether the text carries LRC timestamps; present when `available` is `true`.
   * @zh 歌词文本是否包含标准 LRC 时间戳（仅在 `available` 为 `true` 时返回）
   */
  synced?: boolean;
}

interface ExistsParams {
  /**
   * Track path; a `|subsong:N` suffix selects a subsong.
   * @zh 目标曲目路径（支持使用 `|subsong:N` 后缀指定子曲目）
   * @minLength 1
   * @security MediaRead
   */
  path: string;
  /**
   * Exact file name, including any extension, next to the audio. Replaces the automatic
   * file candidates; embedded tags are still checked. Empty means automatic lookup.
   * The same Windows filename rules and `INVALID_PARAMS` failure as `lyrics.get` apply.
   * @zh 音频同目录下的精确外挂文件名（包含扩展名，非空时覆盖自动文件候选，仍检查内嵌标签，空字符串代表自动查找；文件名校验规则同 `lyrics.get`，非法格式以 `INVALID_PARAMS` 失败）
   */
  filename?: string;
}

interface ExistsResult {
  /**
   * Whether any source has lyrics.
   * @zh 是否在任一检查来源中检测到歌词存在（含空标签与空文件）
   */
  exists: boolean;
  /**
   * Every source found: `embedded` for a lyrics tag, then `file:<name>` for each sidecar file,
   * in the same candidate order as `lyrics.get` with `format=any`, or just the explicit file.
   * @zh 检测到存在歌词的所有来源标识列表（内嵌标签标记为 `embedded`，各外挂文件标记为 `file:<文件名>`；候选遍历顺序与 `lyrics.get` 的 `format="any"` 一致，显式指定文件名时仅检测该文件）
   */
  sources: string[];
}

interface SaveParams {
  /**
   * Track path the lyrics belong to; a `|subsong:N` suffix selects a subsong.
   * @zh 歌词所归属的目标曲目路径（支持使用 `|subsong:N` 后缀指定子曲目）
   * @minLength 1
   * @security MediaWrite
   */
  path: string;
  /**
   * Lyrics text to save.
   * @zh 待保存的完整歌词文本内容
   * @minLength 1
   */
  lyrics: string;
  /**
   * Where to write: `file` (a sidecar next to the audio file), `embedded` (a tag in the file),
   * or `all` for both; several entries write to each. Any other value fails with
   * `INVALID_PARAMS`. Omitted means `file`.
   * @zh 保存目标位置数组：`file`（音频同目录下的外挂文件）、`embedded`（音频文件内嵌标签）或 `all`（两处均写）；指定多项时分别写入各处（省略时默认 `["file"]`，传入非法值以 `INVALID_PARAMS` 失败）
   * @minItems 1
   */
  target?: string[];
  /**
   * Exact file name, including any extension, for the `file` target. A non-empty value
   * replaces the derived name and ignores `format`; empty means the default name.
   * The same Windows filename rules and `INVALID_PARAMS` failure as `lyrics.get` apply.
   * @zh `file` 目标写入的精确文件名（包含扩展名；非空时覆盖自动推导名称并忽略 `format`，空字符串代表采用默认命名；文件名校验规则同 `lyrics.get`，非法格式以 `INVALID_PARAMS` 失败）
   */
  filename?: string;
  /**
   * Tag the `embedded` target writes.
   * @zh `embedded` 目标写入的元数据标签名称（写入时自动转为大写键名）
   * @default "LYRICS"
   */
  tagName?: string;
  /**
   * Extension for the default sidecar name; ignored when `filename` is non-empty.
   * @zh 自动推导外挂文件名时使用的文件扩展名（显式指定非空 `filename` 时忽略本参数）
   * @default "lrc"
   */
  format?: 'lrc' | 'txt';
}

interface SaveResult {
  /**
   * Full path of the file written; present for a single `file` target.
   * @zh 写入的外挂歌词文件绝对路径（仅在单个 `file` 目标成功写入时返回）
   */
  savedTo?: string;
  /**
   * `true` when a single `embedded` target dispatched the tag write; the final outcome arrives as
   * `metadata:writeComplete`.
   * @zh 单个 `embedded` 目标是否成功派发后台标签写入（为 `true` 时最终写入结果通过 `metadata:writeComplete` 事件广播）
   */
  dispatched?: boolean;
  /**
   * The path as given; present for a single `embedded` target.
   * @zh 请求传入的原样曲目路径（仅在单个 `embedded` 目标时返回）
   */
  path?: string;
  /**
   * Path of the handle the tag write targets; present for a single `embedded` target.
   * @zh 标签写入目标曲目句柄规范化路径（仅在单个 `embedded` 目标时返回）
   */
  handlePath?: string;
  /**
   * Subsong the tag write targets; present for a single `embedded` target.
   * @zh 标签写入的目标子曲目序号（仅在单个 `embedded` 目标时返回）
   */
  subsong?: Int;
  /**
   * The tag written, keyed by its upper-case name; present for a single `embedded` target.
   * @zh 实际写入的元数据标签键值映射（键名为大写标签名；仅在单个 `embedded` 目标时返回）
   */
  tagsApplied?: Record<string, string>;
  /**
   * Number of tags set; present for a single `embedded` target.
   * @zh 本次操作设置的标签总数（仅在单个 `embedded` 目标时返回）
   */
  tagsSet?: Int;
  /**
   * Number of tags removed; present for a single `embedded` target.
   * @zh 本次操作清除的标签总数（仅在单个 `embedded` 目标时返回）
   */
  tagsRemoved?: Int;
  /**
   * A note about the dispatch; present for a single `embedded` target.
   * @zh 异步派发状态提示说明（仅在单个 `embedded` 目标时返回）
   */
  note?: string;
  /**
   * With several targets: each target's own envelope (`{ success, savedTo }`, the tag write
   * receipt, or `{ success: false, error, code }`) under `file` or `embedded`.
   * @zh 多目标写入时各目标的独立响应结果映射（以 `file` 或 `embedded` 为键名，对应包含各自的成功回执或错误信封）
   */
  results?: Record<string, Json>;
}
