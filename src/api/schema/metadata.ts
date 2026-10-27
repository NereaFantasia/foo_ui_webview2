import type { Int, Json } from './common.js';

export interface Api {
  /**
   * Read the tags and technical info of one track. The host reads its cached info and falls back
   * to reading the file when that info cannot be had or has no title tag; a track that cannot be
   * opened or read fails with `OPERATION_FAILED`. Invalid UTF-8 in values is replaced with
   * U+FFFD; fields whose names are invalid UTF-8 are omitted, without changing the file.
   * @zh 读取单个曲目的元数据标签与音频技术信息；优先读取宿主内存缓存，若缓存缺失或未收录标题标签则回退至直接读取音频文件；文件无法打开或读取失败时以 `OPERATION_FAILED` 失败；字段值中的非法 UTF-8 字符替换为 U+FFFD，字段名包含非法 UTF-8 的项自动省略且不修改原文件
   * @effect read
   */
  read(params: ReadParams): ReadResult;

  /**
   * Read one track as a flat object: every tag and technical-info field becomes a top-level key
   * under its upper-case name, beside `path`. The track is read the same way as by
   * `metadata.read`, with the same failures and UTF-8 handling. Only ASCII letters in names
   * are upper-cased.
   * @zh 以扁平键值对象格式读取单个曲目元数据；每个标签与技术信息字段均转为大写键名（仅转换 ASCII 字母）与 `path` 并列输出为顶层属性；读取机制、失败规则与 UTF-8 处理逻辑与 `metadata.read` 完全一致
   */
  readByPath(params: ReadByPathParams): ReadByPathResult;

  /**
   * Read the tags and technical info of one track straight from the file, bypassing the host's
   * cache; the result is that of `metadata.read` plus `source`. A file that cannot be read fails
   * with `OPERATION_FAILED`.
   * @zh 绕过宿主内存缓存，直接从物理文件读取单个曲目的元数据标签与技术信息；返回结构同 `metadata.read` 且额外附带 `source: "file"` 字段；文件读取失败以 `OPERATION_FAILED` 失败
   * @effect read
   */
  readRaw(params: ReadRawParams): ReadRawResult;

  /**
   * Read the flat fields of several tracks, one row per path. A path that cannot be read gets a
   * row with its own error and does not fail the call. The reads run on the main thread; prefer
   * `metadata.probeBatchAsync` for many files. UTF-8 handling is the same as `metadata.read`.
   * @zh 批量读取多个曲目的扁平元数据字段，按输入路径顺序逐行返回；单项路径读取失败在对应行报告自身错误而不导致整次调用失败；在宿主主线程同步执行（大量文件推荐改用 `metadata.probeBatchAsync`）；UTF-8 字符处理规则同 `metadata.read`
   * @effect read
   */
  readBatch(params: ReadBatchParams): ReadBatchResult;

  /**
   * Start a cancellable batch probe on a worker thread and return a receipt at once. The results
   * arrive in batches on `metadata:probeProgress`, every 64 results or 100 ms, followed by one
   * `metadata:probeComplete`. Each result says whether its info came from the cache or the disk,
   * and a failure is classified as `not-found`, `unsupported-format` or `read-error`. Values
   * and field names use the same UTF-8 handling as `metadata.read`.
   * @zh 在后台工作线程启动可取消的异步批量元数据探测任务并立即返回任务回执；探测结果分批通过 `metadata:probeProgress` 事件推送（每累积 64 条或每隔 100 毫秒推送一批），并在任务结束时投递 `metadata:probeComplete` 事件；单项结果标明数据源来自缓存还是磁盘，失败项分类标记为 `not-found`、`unsupported-format` 或 `read-error`；UTF-8 处理规则同 `metadata.read`
   */
  probeBatchAsync(params: ProbeBatchAsyncParams): ProbeBatchAsyncResult;

  /**
   * Stop a probe started by `metadata.probeBatchAsync`. The read in progress is interrupted, the
   * paths not yet reached are never reported, and the run still ends with
   * `metadata:probeComplete` carrying `cancelled: true`.
   * @zh 停止由 `metadata.probeBatchAsync` 启动的正在运行的异步探测任务；立即中断当前正在执行的文件读取，未到达的后续路径不再上报，收尾依然触发标记为 `cancelled: true` 的 `metadata:probeComplete` 事件
   */
  cancelProbe(params: CancelProbeParams): CancelProbeResult;

  /**
   * Queue a tag write to one track and return at once. foobar2000 writes the file in the
   * background and reports the outcome as `metadata:writeComplete`, broadcast to every window. A
   * request that leaves nothing to write succeeds without dispatching.
   * @zh 将针对单个曲目的标签写入请求加入后台队列并立即返回；foobar2000 宿主在后台线程执行物理文件写入，并在操作完成后向所有窗口广播 `metadata:writeComplete` 事件；请求中未包含有效待写标签时直接返回成功且不派发后台写入
   * @effect destructive
   * @idempotent
   */
  write(params: WriteParams): WriteResult;

  /**
   * Queue one `metadata.write` per entry and report how many went through. A dispatched write
   * reports its own outcome later as `metadata:writeComplete`, so a file that does not exist
   * still counts here. When any entry fails, the call fails with `OPERATION_FAILED` and the
   * failure carries the same `successCount`, `failCount` and `errors`; the other entries were
   * queued all the same.
   * @zh 批量排入多个曲目的标签写入请求，并报告成功入队的数量；已派发的单项写入在完成后独立广播 `metadata:writeComplete` 事件（不存在的文件在此阶段亦计入入队成功）；若有条目参数非法则整次调用以 `OPERATION_FAILED` 失败（错误详情附带 `successCount`、`failCount` 与 `errors` 列表，其他合法项照常入队）
   * @effect destructive
   * @idempotent
   */
  writeBatch(params: WriteBatchParams): WriteBatchResult;

  /**
   * Write a picture into a track's tags, into an image file next to it, or both. With one
   * target the result describes that write; with both, each target's outcome is reported under
   * `results` and the call fails with `OPERATION_FAILED` only when both failed.
   * @zh 将封面图片写入音频文件内嵌标签、写为同目录下的独立图片文件或两处同时写入；指定单个目标时返回该目标的写入结果；同时指定两处目标时在 `results` 下分别列出各自状态，仅当两处写入均失败时整次调用以 `OPERATION_FAILED` 失败
   * @effect destructive
   * @idempotent
   */
  embedArtwork(params: EmbedArtworkParams): EmbedArtworkResult;

  /**
   * Remove embedded pictures from a file: one type, or every picture when `type` is omitted or
   * `removeAll` is set. A format the album art editor does not support fails with
   * `NOT_SUPPORTED`.
   * @zh 从音频文件中移除内嵌的封面图片；支持按 `type` 删除指定类型，或在省略 `type` 及设置 `removeAll: true` 时全量清空；音频格式不受专辑封面编辑器支持时以 `NOT_SUPPORTED` 失败
   * @effect destructive
   * @idempotent
   */
  removeEmbeddedArt(params: RemoveEmbeddedArtParams): RemoveEmbeddedArtResult;

  /**
   * Queue the removal of tags from one track and return at once. The outcome arrives as
   * `metadata:writeComplete` with `operation: "removeTag"`, broadcast to every window.
   * @zh 将针对单个曲目的标签删除请求加入后台队列并立即返回；操作完成后向所有窗口广播 `operation: "removeTag"` 的 `metadata:writeComplete` 事件
   */
  removeTag(params: RemoveTagParams): RemoveTagResult;

  /**
   * Same as `metadata.removeTag`, under the name older callers use; its completion event says
   * `removeTag` as well.
   * @zh 从单个曲目中删除指定标签（旧名兼容接口，功能同 `metadata.removeTag`，完成事件同样标识为 `removeTag`）
   */
  removeField(params: RemoveTagParams): RemoveTagResult;
}

export interface Events {
  /**
   * Results of a `metadata.probeBatchAsync` run, in batches: a batch goes out once 64 results are
   * pending or 100 ms have passed since the previous one, and the last partial batch arrives
   * before `metadata:probeComplete`. A path the run was cancelled on or never reached is not
   * reported. Sent to the page that started the probe; when that window is gone by then, to the
   * main window's page.
   * @zh `metadata.probeBatchAsync` 异步探测任务的进度批次事件；每累积 64 条结果或距上一批超过 100 毫秒即分批推送，最终未满批次在 `metadata:probeComplete` 前推送完毕；被取消或未处理的路径不予上报；定向投递给发起探测的页面窗口（窗口已销毁时退至主窗口页面）
   * @delivery caller
   */
  probeProgress: ProbeProgressPayload;

  /**
   * The last event of a probe, sent after a cancel and after a failed run too; only an exception
   * while sending it, in practice running out of memory, skips it. Afterwards the `operationId`
   * is gone and `metadata.cancelProbe` reports `cancelled: false` for it. Delivered like
   * `metadata:probeProgress`.
   * @zh 异步探测任务结束事件；任务正常完成、被主动取消或因故失败后均会触发（仅在分发异常如内存耗尽时可能缺省）；触发后原 `operationId` 立即失效（对其调用 `metadata.cancelProbe` 将返回 `cancelled: false`）；投递目标与 `metadata:probeProgress` 相同
   * @delivery caller
   */
  probeComplete: ProbeCompletePayload;

  /**
   * A tag write queued by `metadata.write`, `metadata.writeBatch`, `metadata.removeTag` or
   * `metadata.removeField` finished. Every window receives it.
   * @zh 由 `metadata.write`、`metadata.writeBatch`、`metadata.removeTag` 或 `metadata.removeField` 排入的后台标签写入完成事件；广播通告所有打开的窗口
   * @delivery broadcast
   */
  writeComplete: WriteCompletePayload;
}

interface ProbeProgressPayload {
  /**
   * Id from the `metadata.probeBatchAsync` receipt.
   * @zh 对应 `metadata.probeBatchAsync` 返回的操作标识符 `operationId`
   */
  operationId: string;
  /**
   * Paths reported so far across all batches, failures included.
   * @zh 截至当前批次各批累计上报的路径总数（包含失败项）
   */
  done: Int;
  /**
   * Paths the call accepted, its `totalCount`.
   * @zh 本次探测任务受理的路径总数（对应受理时的 `totalCount`）
   */
  total: Int;
  /**
   * The results of this batch only, in request order.
   * @zh 仅包含本进度批次的探测结果列表，顺序与请求一致
   */
  results: MetadataProbeResultItem[];
}

/**
 * The result of probing one path.
 * @zh 单条路径的元数据探测结果信息
 */
interface MetadataProbeResultItem {
  /**
   * The path as requested, `|subsong:N` included.
   * @zh 请求传入的原样路径（包含 `|subsong:N` 后缀）
   */
  path: string;
  /**
   * Whether the track was read.
   * @zh 该曲目是否成功读取并解析
   */
  success: boolean;
  /**
   * `cached` when the host's complete cached info was used, `direct` when the file was read, `none`
   * with a failure.
   * @zh 元数据来源：`cached` 表示取自宿主完整内存缓存，`direct` 表示直接读取物理文件，`none` 表示读取失败
   */
  infoSource: 'cached' | 'direct' | 'none';
  /**
   * Why the path was not read; present when `success` is `false`.
   * @zh 路径读取失败的原因分类：`not-found`、`unsupported-format` 或 `read-error`；仅在 `success: false` 时出现
   */
  failure?: 'not-found' | 'unsupported-format' | 'read-error';
  /**
   * Technical info of the track, as in `metadata.read`; present when `success` is `true`.
   * @zh 曲目的音频技术参数；仅在 `success: true` 时返回
   */
  info?: TrackTechnicalInfo;
  /**
   * The flat fields of the track, as `includeTags` describes; present when `success` is `true`
   * and the call did not pass `includeTags: false`.
   * @zh 曲目的扁平元数据标签字典；仅在 `success: true` 且未指定 `includeTags: false` 时返回
   */
  tags?: Record<string, Json>;
}

interface ProbeCompletePayload {
  /**
   * Id from the `metadata.probeBatchAsync` receipt.
   * @zh 对应 `metadata.probeBatchAsync` 返回的操作标识符 `operationId`
   */
  operationId: string;
  /**
   * Paths the call accepted.
   * @zh 本次探测任务受理的路径总数
   */
  total: Int;
  /**
   * Paths reported with `success: true`.
   * @zh 探测成功并上报的路径总数
   */
  successCount: Int;
  /**
   * Paths reported with `success: false`.
   * @zh 探测失败并上报的路径总数
   */
  failureCount: Int;
  /**
   * `true` when the probe was stopped early, by `metadata.cancelProbe` or because the popup that
   * started it closed; the two counts then fall short of `total`.
   * @zh 探测任务是否被提前中止（通过 `metadata.cancelProbe` 或发起窗口关闭触发）；为 `true` 时成功与失败计数之和小于 `total`
   */
  cancelled: boolean;
}

interface WriteCompletePayload {
  /**
   * `write` for `metadata.write` and `metadata.writeBatch`, `removeTag` for `metadata.removeTag`
   * and `metadata.removeField`.
   * @zh 完成的写入操作类型：`write` 或 `removeTag`
   */
  operation: 'write' | 'removeTag';
  /**
   * The path as the call gave it, suffix included.
   * @zh 请求传入的原样文件路径（包含后缀）
   */
  path: string;
  /**
   * The subsong written: `cueIndex` when it is 0 or more, otherwise the one the path selects,
   * otherwise `0`.
   * @zh 实际写入的目标子曲目序号（`cueIndex >= 0` 时取该值，否则取路径解析值，均无时为 `0`）
   */
  subsong: Int;
  /**
   * foobar2000's completion code: `0` success, `1` aborted, `2` errors.
   * @zh foobar2000 宿主完成状态码：`0` 成功，`1` 中止，`2` 出错
   */
  code: Int;
  /**
   * `true` when `code` is `0`.
   * @zh 写入是否完全成功（当且仅当 `code === 0` 时为 `true`）
   */
  success: boolean;
  /**
   * `success`, `aborted` or `error`, after `code`.
   * @zh 依据状态码转换的语义化状态：`success`、`aborted` 或 `error`
   */
  status: 'success' | 'aborted' | 'error';
}

/**
 * A picture type; `cover_front` and `cover_back` are the foobar2000 spellings of `front` and
 * `back`.
 * @zh 封面图片类型（`cover_front` 与 `cover_back` 分别为 foobar2000 中对应 `front` 与 `back` 的等价写法）
 */
type ArtworkType = 'front' | 'cover_front' | 'back' | 'cover_back' | 'disc' | 'icon' | 'artist';

/**
 * Technical info of a track, as `metadata.read`, `metadata.readRaw` and the
 * `metadata:probeProgress` results report it.
 * @zh 音频曲目的技术信息参数（包含时长、码率、采样率、声道数与编码格式）
 */
interface TrackTechnicalInfo {
  /**
   * Length in seconds.
   * @zh 曲目时长（秒）
   */
  duration: number;
  /**
   * Bitrate in kbit/s as the decoder reports it; `0` when unknown.
   * @zh 解码器报告的音频平均码率（kbit/s）；未知时为 `0`
   */
  bitrate: Int;
  /**
   * Sample rate in Hz; `0` when unknown.
   * @zh 音频采样率（Hz）；未知时为 `0`
   */
  sampleRate: Int;
  /**
   * Channel count; `0` when unknown.
   * @zh 声道数量；未知时为 `0`
   */
  channels: Int;
  /**
   * Codec name as the decoder reports it, such as `FLAC` or `MP3`; empty when unknown.
   * @zh 解码器报告的音频编码名称（如 `FLAC`、`MP3`）；未知时为空字符串
   */
  codec: string;
}

// ---- read ----

interface ReadParams {
  /**
   * Track path; a `|subsong:N` suffix or a trailing `#N` selects a subsong.
   * @zh 目标曲目路径；支持 `|subsong:N` 后缀或尾部 `#N` 指定子曲目
   * @minLength 1
   * @security MediaRead
   */
  path: string;
  /**
   * Subsong index that wins over a suffix in `path`; negative values are ignored.
   * @zh 显式指定的子曲目序号（优先级高于 `path` 中的后缀）；传入负数时忽略
   * @default -1
   */
  cueIndex?: Int;
}

interface ReadResult {
  /**
   * The path as given, suffix included.
   * @zh 请求传入的原样路径（包含后缀）
   */
  path: string;
  /**
   * Every tag of the track, keyed by its name as the file spells it (case kept): a tag with one
   * value is a string, one with several values a string array.
   * @zh 曲目包含的全部标签字段（以文件内原始大小写为键名）：单值标签解析为字符串，多值标签解析为字符串数组
   */
  tags: Record<string, Json>;
  /**
   * Technical info of the track.
   * @zh 曲目的音频技术参数
   */
  info: TrackTechnicalInfo;
}

// ---- readByPath ----

interface ReadByPathParams {
  /**
   * Track path; a `|subsong:N` suffix or a trailing `#N` selects a subsong.
   * @zh 目标曲目路径；支持 `|subsong:N` 后缀或尾部 `#N` 指定子曲目
   * @minLength 1
   * @security MediaRead
   */
  path: string;
  /**
   * Subsong index that wins over a suffix in `path`; negative values are ignored.
   * @zh 显式指定的子曲目序号（优先级高于 `path` 中的后缀）；传入负数时忽略
   * @default -1
   */
  cueIndex?: Int;
}

type ReadByPathResult = {
  /**
   * The path as given, suffix included.
   * @zh 请求传入的原样路径（包含后缀）
   */
  path: string;
} & Record<string, FlatField>;

/**
 * One tag or technical-info field of the track under its upper-case name. A tag with one value
 * is a string and one with several values a string array; technical-info fields are strings.
 * `DURATION` is always present: the length in seconds with three decimals, such as `215.400`.
 * `FILESIZE` is the size in bytes and appears only when the host knows it, so a file the
 * library has not indexed yet has none. `TRACKNUMBER`, when the file has no such tag, is taken
 * from a leading number of up to three digits in the file name (`07 - Song.flac`,
 * `(07) Song.flac`, `[07] Song.flac`) and is absent when there is none.
 * @zh 曲目的单个元数据标签或音频技术信息字段（键名均为大写）；单值标签为字符串，多值标签表现为字符串数组，技术信息字段为字符串；`DURATION` 始终存在（以秒为单位保留 3 位小数的持续时长，如 `215.400`）；`FILESIZE` 表示文件字节数（仅在宿主获知时存在，媒体库尚未索引的文件不含该字段）；若文件缺失 `TRACKNUMBER` 标签，则自动尝试从文件名起始处不超过 3 位的数字推断（支持 `07 - Song.flac`、`(07) Song.flac`、`[07] Song.flac` 等格式），无法提取时不输出
 */
type FlatField = Json;

// ---- readRaw ----

interface ReadRawParams {
  /**
   * Track path; a `|subsong:N` suffix or a trailing `#N` selects a subsong.
   * @zh 目标曲目路径；支持 `|subsong:N` 后缀或尾部 `#N` 指定子曲目
   * @minLength 1
   * @security MediaRead
   */
  path: string;
  /**
   * Subsong index that wins over a suffix in `path`; negative values are ignored.
   * @zh 显式指定的子曲目序号（优先级高于 `path` 中的后缀）；传入负数时忽略
   * @default -1
   */
  cueIndex?: Int;
}

interface ReadRawResult {
  /**
   * The path as given, suffix included.
   * @zh 请求传入的原样路径（包含后缀）
   */
  path: string;
  /**
   * Every tag of the track, as in `metadata.read`.
   * @zh 曲目包含的全部标签字段（结构同 `metadata.read`）
   */
  tags: Record<string, Json>;
  /**
   * Technical info of the track.
   * @zh 曲目的音频技术参数
   */
  info: TrackTechnicalInfo;
  /**
   * Always `file`: the tags were read from the file itself, not from the host's cache.
   * @zh 元数据来源标识，固定为 `file`（直接读取物理文件而非内存缓存）
   */
  source: string;
}

// ---- readBatch ----

interface ReadBatchParams {
  /**
   * Track paths, each resolved on its own; a `|subsong:N` suffix or a trailing `#N` selects a
   * subsong, and there is no batch-wide `cueIndex`. An empty list is an empty success.
   * @zh 待逐条独立解析读取的曲目路径列表；支持 `|subsong:N` 后缀或 `#N` 语法；传入空列表直接返回空成功
   * @security MediaRead
   */
  paths: string[];
}

interface ReadBatchResult {
  /**
   * Number of paths received.
   * @zh 本次批量请求受理的路径总数
   */
  total: Int;
  /**
   * Rows that were read.
   * @zh 成功读取并解析元数据的路径总数
   */
  successCount: Int;
  /**
   * Rows that failed.
   * @zh 读取或解析失败的路径总数
   */
  errorCount: Int;
  /**
   * One row per path, in request order.
   * @zh 批量读取结果列表，严格保持与请求一致的顺序
   */
  results: MetadataReadBatchItem[];
}

/**
 * One row of `metadata.readBatch`.
 * @zh 批量同步读取中的单项曲目元数据结果
 */
interface MetadataReadBatchItem {
  /**
   * The path of this row, as given.
   * @zh 请求传入的原样路径
   */
  path: string;
  /**
   * Whether this row was read.
   * @zh 该曲目是否成功读取
   */
  success: boolean;
  /**
   * The flat fields of the track, as in `metadata.readByPath` but without `path` and without a
   * `TRACKNUMBER` taken from the file name; present when `success` is `true`.
   * @zh 曲目的扁平元数据字段字典（结构同 `metadata.readByPath`，但不含 `path` 且不从文件名猜测 `TRACKNUMBER`）；仅在 `success: true` 时返回
   */
  tags?: Record<string, Json>;
  /**
   * Why this row failed, such as `Failed to get track info`; present when `success` is `false`.
   * @zh 该曲目读取失败的原因说明；仅在 `success: false` 时返回
   */
  error?: string;
}

// ---- probeBatchAsync ----

interface ProbeBatchAsyncParams {
  /**
   * Paths to probe, reported one result each, in this order. A `|subsong:N` suffix selects a
   * subsong; the `#N` spelling is not recognized, since it would split a file name that ends in
   * `#<digits>`. Every path is checked before anything starts; one refused path fails the whole
   * call with `PERMISSION_DENIED` and no `operationId`.
   * @zh 待异步探测的路径列表，按传入顺序逐条上报；支持 `|subsong:N` 后缀（不支持 `#N` 语法以避免误切文件名）；开始前预检全部路径，任一受拒整次调用以 `PERMISSION_DENIED` 失败
   * @minItems 1
   * @security MediaRead
   */
  paths: string[];
  /**
   * Attach the flat fields, as in `metadata.readByPath` without `path` and without the
   * `TRACKNUMBER` taken from the file name, to each successful result; `false` reports technical
   * info only.
   * @zh 是否为每条成功结果附带扁平元数据字段；设为 `false` 时仅返回音频技术参数
   * @default true
   */
  includeTags?: boolean;
}

interface ProbeBatchAsyncResult {
  /**
   * Id of the operation, starting with `probe_`. The events of the operation carry it, and
   * `metadata.cancelProbe` takes it.
   * @zh 异步探测任务唯一标识符（以 `probe_` 开头），后续事件携带此 ID，取消操作亦须传入
   */
  operationId: string;
  /**
   * Number of paths accepted; the events report it as `total`.
   * @zh 本次任务受理的路径总数（后续事件中以 `total` 报告）
   */
  totalCount: Int;
}

// ---- cancelProbe ----

interface CancelProbeParams {
  /**
   * Id from the `metadata.probeBatchAsync` receipt.
   * @zh 对应 `metadata.probeBatchAsync` 返回的操作标识符 `operationId`
   * @minLength 1
   */
  operationId: string;
}

interface CancelProbeResult {
  /**
   * `true` when the probe was still running and has been told to stop; `false` when it had
   * already finished or never existed, which are deliberately indistinguishable.
   * @zh 探测任务是否成功发送停止信号；若任务已结束或不存在则返回 `false`（两者故意不可区分）
   */
  cancelled: boolean;
}

// ---- write ----

interface WriteParams {
  /**
   * Track path; a `|subsong:N` suffix or a trailing `#N` selects a subsong.
   * @zh 目标曲目路径；支持 `|subsong:N` 后缀或尾部 `#N` 指定子曲目
   * @minLength 1
   * @security MediaWrite
   */
  path: string;
  /**
   * Tags to change, keyed by name. Names are upper-cased, so keys that differ only in case name
   * the same tag. `null`, an empty string or an empty array removes the tag. A non-empty string
   * array replaces all values of the tag, preserving order, duplicates and whitespace without
   * splitting commas or semicolons. Every array element must be a non-empty string without NUL;
   * an invalid element fails this track with `INVALID_PARAMS` before any of its tags are queued,
   * with the tag name and zero-based element index in the error. A scalar string is written as
   * it is; an integer or a fraction is written as its decimal text (`2.5` becomes `2.500000`).
   * Boolean and object values are ignored. File formats and tag writers may limit multivalue
   * support; the completion event reports the final write result.
   * @zh 待写入或修改的标签字典（键名不区分大小写自动转为大写）；传入 `null`、空字符串或空数组表示删除该标签；非空字符串数组全量替换该标签的所有取值并保留原序与空白（不按逗号或分号拆分）；数组元素必须为非空且不含 NUL 字符的字符串；标量数字转为十进制文本；布尔值与对象自动忽略；最终写入效果以完成事件为准
   */
  tags: Record<string, Json>;
  /**
   * Subsong index that wins over a suffix in `path`; negative values are ignored.
   * @zh 显式指定的子曲目序号（优先级高于 `path` 中的后缀）；传入负数时忽略
   * @default -1
   */
  cueIndex?: Int;
}

interface WriteResult {
  /**
   * The path as given.
   * @zh 请求传入的原样路径
   */
  path: string;
  /**
   * `No tags to update` when nothing was left to write; otherwise a reminder that the outcome
   * arrives as `metadata:writeComplete`.
   * @zh 状态提示说明（无待写内容时返回 `No tags to update`，否则提示通过 `metadata:writeComplete` 广播最终结果）
   */
  note: string;
  /**
   * `true` when the write was dispatched; absent when there was nothing to write.
   * @zh 写入请求是否已成功派发至后台线程；无待写内容时缺省
   */
  dispatched?: boolean;
  /**
   * Path of the handle the write targets, as foobar2000 stores it; present when dispatched.
   * @zh 写入操作所针对的曲目句柄存储路径；派发时返回
   */
  handlePath?: string;
  /**
   * Subsong the write targets; present when dispatched.
   * @zh 写入操作所针对的目标子曲目序号；派发时返回
   */
  subsong?: Int;
  /**
   * Every tag queued, keyed by its upper-case name: the string, number or non-empty string array
   * given for a write, `null` for a removal (including an empty array). Present when dispatched.
   * @zh 排入写入队列的全部标签字典（键名为大写标签名，值为字符串、数字或字符串数组，删除项记为 `null`）；派发时返回
   */
  tagsApplied?: Record<string, Json>;
  /**
   * Number of tag fields set, not the number of values; present when dispatched.
   * @zh 本次成功设置的标签字段数量（按字段数而非值数统计）；派发时返回
   */
  tagsSet?: Int;
  /**
   * Number of tag fields removed, including those given as empty arrays; present when dispatched.
   * @zh 本次成功删除的标签字段数量（包含传入空数组的项）；派发时返回
   */
  tagsRemoved?: Int;
}

// ---- writeBatch ----

interface WriteBatchParams {
  /**
   * Writes to queue, one per entry, in this order. An entry that is not an object, holds a member
   * other than `path`, `tags` and `cueIndex`, or has no string `path` fails the call with
   * `INVALID_PARAMS`. Every path is checked before anything starts; one refused path, an empty
   * string included, fails the whole call with `PERMISSION_DENIED`. An empty list is an empty
   * success.
   * @zh 待批量排入的写入条目列表；条目结构非法以 `INVALID_PARAMS` 失败；开始前预检全部路径，任一路径受拒整次调用以 `PERMISSION_DENIED` 失败；空列表返回空成功
   * @security MediaWrite
   * @pathKey path
   */
  items: MetadataWriteBatchItem[];
}

/**
 * One entry of `metadata.writeBatch`.
 * @zh 批量标签写入中的单项配置
 */
interface MetadataWriteBatchItem {
  /**
   * Track path, as in `metadata.write`.
   * @zh 目标曲目路径
   * @minLength 1
   */
  path: string;
  /**
   * Tags to change, as in `metadata.write`. An entry whose `tags` is missing or not an object is
   * reported in `errors` as `Missing tags` instead of refusing the call. An invalid array value
   * also fails only this entry before any of its tags are queued; other entries still run.
   * @zh 待写入或修改的标签字典；缺失或非对象时在 `errors` 中报告 `Missing tags` 而不拒绝整批任务
   */
  tags?: Json;
  /**
   * Subsong index for this entry, as in `metadata.write`.
   * @zh 显式指定的子曲目序号；负数忽略
   * @default -1
   */
  cueIndex?: Int;
}

interface WriteBatchResult {
  /**
   * Entries whose write was dispatched or had nothing to write; each dispatched write reports its
   * outcome as `metadata:writeComplete`.
   * @zh 成功派发写入或无须写入的条目总数
   */
  successCount: Int;
  /**
   * Entries that failed; always `0` on success, since a failed entry fails the call.
   * @zh 派发失败的条目总数（整批成功时固定为 `0`）
   */
  failCount: Int;
  /**
   * The failed entries; always empty on success, since a failed entry fails the call.
   * @zh 派发失败的条目详情列表（整批成功时固定为空数组）
   */
  errors: MetadataWriteBatchError[];
}

/**
 * One failed entry of `metadata.writeBatch`.
 * @zh 批量标签写入失败条目详情
 */
interface MetadataWriteBatchError {
  /**
   * The entry's path.
   * @zh 失败条目的曲目路径
   */
  path: string;
  /**
   * Why it failed: `Missing tags`, or the error `metadata.write` gives for that path.
   * @zh 失败原因说明
   */
  error: string;
}

// ---- embedArtwork ----

interface EmbedArtworkParams {
  /**
   * Track path. The `embedded` target writes into this file, which needs a format the album art
   * editor supports; the `file` target writes into its directory with a `|subsong:N` suffix
   * dropped, so every track of a CUE sheet shares one image.
   * @zh 目标曲目路径；`embedded` 目标写入该文件（须格式支持），`file` 目标写入所在目录并剥离 `|subsong:N` 后缀（同 CUE 曲目共享单张图）
   * @minLength 1
   * @security MediaWrite
   */
  path: string;
  /**
   * The image as plain Base64, without a Data URL header or a `base64:` marker. Characters
   * outside the Base64 alphabet are skipped and decoding stops at the first `=`; input that
   * decodes to no bytes fails with `INVALID_PARAMS`.
   * @zh 待内嵌图片的裸 Base64 字符串（不带 Data URL 前缀与 `base64:` 标记）；非字母表字符自动忽略并在首个 `=` 处终止解码；无有效字节时以 `INVALID_PARAMS` 失败
   * @minLength 1
   */
  imageData: string;
  /**
   * Picture type.
   * @zh 待内嵌的图片类型
   * @default "front"
   */
  type?: ArtworkType;
  /**
   * Where to write: `embedded` (into the file's tags), `file` (an image next to the audio file,
   * named after `type`, `cover` for the front picture, with the extension read from the image
   * bytes), or `all` for both; several entries write to each. Omitted, it writes `embedded`.
   * Any other value fails with `INVALID_PARAMS`.
   * @zh 写入目标位置数组：`embedded`（写入音频内嵌标签）、`file`（音频同目录下外置图片，正面图命名为 `cover`，扩展名由字节解析）或 `all`（两处均写）；省略时默认 `["embedded"]`，非法值以 `INVALID_PARAMS` 失败
   * @minItems 1
   */
  target?: string[];
  /**
   * File name for the `file` target, used as it is instead of the derived one (no extension is
   * added); must be a plain name without separators or traversal.
   * @zh `file` 目标指定的自定义文件名（原样使用不补扩展名）；必须为不含路径分隔符或穿越序列的纯文件名
   */
  filename?: string;
}

interface EmbedArtworkResult {
  /**
   * The path as given.
   * @zh 请求传入的原样路径
   */
  path: string;
  /**
   * The requested type, as given.
   * @zh 请求传入的原样图片类型
   */
  type: string;
  /**
   * Picture size in bytes after decoding; present for a single target.
   * @zh 解码后的图片二进制字节数；仅在指定单个目标时返回
   */
  size?: Int;
  /**
   * Full path of the image file written; present for a single `file` target.
   * @zh 写入的独立图片文件的完整物理路径；仅在指定单个 `file` 目标时返回
   */
  savedTo?: string;
  /**
   * With both targets: each target's own envelope (`{ success, path, type, size }`, plus
   * `savedTo` for `file`, or `{ success: false, error, code }`) under `embedded` and `file`.
   * @zh 同时写入两处目标时各自的写入信封详情字典（键名为 `embedded` 与 `file`）
   */
  results?: Record<string, Json>;
}

// ---- removeEmbeddedArt ----

interface RemoveEmbeddedArtParams {
  /**
   * Track path; the file needs a format the album art editor supports.
   * @zh 目标曲目路径（音频格式须受专辑封面编辑器支持）
   * @minLength 1
   * @security MediaWrite
   */
  path: string;
  /**
   * Picture type to remove; omitted, every picture is removed.
   * @zh 待删除的图片类型；省略时默认删除所有内嵌图片
   */
  type?: ArtworkType;
  /**
   * Remove every picture whatever `type` says.
   * @zh 是否全量删除所有内嵌图片（优先级高于 `type`）
   * @default false
   */
  removeAll?: boolean;
}

interface RemoveEmbeddedArtResult {
  /**
   * The path as given.
   * @zh 请求传入的原样路径
   */
  path: string;
  /**
   * What was removed: `["all"]` when every picture went in one step, otherwise the types removed
   * one by one (`front`, `back`, `disc`, `icon`, `artist`), or the single requested `type` as
   * given.
   * @zh 实际执行删除的图片类型列表：全量删除时为 `["all"]`，否则为逐个删除的类型名称列表
   */
  removedTypes: string[];
}

// ---- removeTag / removeField ----

interface RemoveTagParams {
  /**
   * Track path; a `|subsong:N` suffix or a trailing `#N` selects a subsong.
   * @zh 目标曲目路径；支持 `|subsong:N` 后缀或尾部 `#N` 指定子曲目
   * @minLength 1
   * @security MediaWrite
   */
  path: string;
  /**
   * Tag names to remove; they are upper-cased. An empty list removes nothing and succeeds
   * without dispatching.
   * @zh 待删除的标签名称数组（自动转为大写比对）；传入空数组不执行删除且直接返回成功
   */
  tags: string[];
  /**
   * Subsong index that wins over a suffix in `path`; negative values are ignored.
   * @zh 显式指定的子曲目序号（优先级高于 `path` 中的后缀）；传入负数时忽略
   * @default -1
   */
  cueIndex?: Int;
}

interface RemoveTagResult {
  /**
   * The path as given.
   * @zh 请求传入的原样路径
   */
  path: string;
  /**
   * The tag names queued for removal, upper-cased, in the order given.
   * @zh 成功排入删除队列的标签名称列表（已转为大写并保持输入顺序）
   */
  removedTags: string[];
  /**
   * Number of names in `removedTags`.
   * @zh 成功排入删除队列的标签字段数量
   */
  removedCount: Int;
  /**
   * `true` when the removal was dispatched; absent for an empty `tags`.
   * @zh 删除请求是否已成功派发至后台线程；`tags` 为空时缺省
   */
  dispatched?: boolean;
  /**
   * Subsong the removal targets; present when dispatched.
   * @zh 删除操作所针对的目标子曲目序号；派发时返回
   */
  subsong?: Int;
  /**
   * A reminder that the outcome arrives as `metadata:writeComplete`; present when dispatched.
   * @zh 状态提示说明（提示最终删除结果由 `metadata:writeComplete` 广播）
   */
  note?: string;
}
