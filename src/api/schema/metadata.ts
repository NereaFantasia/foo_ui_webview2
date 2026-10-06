import type { Int, Json } from './common.js';

export interface Api {
  /**
   * Read the tags and technical info of one track. The host reads its cached info and falls back
   * to reading the file when that info cannot be had or has no title tag; a track that cannot be
   * opened or read fails with `OPERATION_FAILED`.
   * @zh 读取单个曲目的标签与技术信息。先读宿主缓存的信息，拿不到或其中没有标题标签时改为直接读文件；打不开或读不出的曲目以 `OPERATION_FAILED` 失败。
   * @effect read
   */
  read(params: ReadParams): ReadResult;

  /**
   * Read one track as a flat object: every tag and technical-info field becomes a top-level key
   * under its upper-case name, beside `path`. The track is read the same way as by
   * `metadata.read`, with the same failures.
   * @zh 以扁平对象读取单个曲目：每个标签与技术信息字段都以大写名成为顶层键，与 `path` 并列。读取方式与失败情形同 `metadata.read`。
   */
  readByPath(params: ReadByPathParams): ReadByPathResult;

  /**
   * Read the tags and technical info of one track straight from the file, bypassing the host's
   * cache; the result is that of `metadata.read` plus `source`. A file that cannot be read fails
   * with `OPERATION_FAILED`.
   * @zh 绕过宿主缓存，直接从文件读取单个曲目的标签与技术信息；结果同 `metadata.read`，另加 `source`。读不出的文件以 `OPERATION_FAILED` 失败。
   * @effect read
   */
  readRaw(params: ReadRawParams): ReadRawResult;

  /**
   * Read the flat fields of several tracks, one row per path. A path that cannot be read gets a
   * row with its own error and does not fail the call. The reads run on the main thread; prefer
   * `metadata.probeBatchAsync` for many files.
   * @zh 读取多个曲目的扁平字段，每个路径一行。读不出的路径那一行带自己的错误，不会让整次调用失败。读取在主线程上进行；文件多时改用 `metadata.probeBatchAsync`。
   * @effect read
   */
  readBatch(params: ReadBatchParams): ReadBatchResult;

  /**
   * Start a cancellable batch probe on a worker thread and return a receipt at once. The results
   * arrive in batches on `metadata:probeProgress`, every 64 results or 100 ms, followed by one
   * `metadata:probeComplete`. Each result says whether its info came from the cache or the disk,
   * and a failure is classified as `not-found`, `unsupported-format` or `read-error`.
   * @zh 在 worker 线程上启动可取消的批量探测，立即返回回执。结果经 `metadata:probeProgress` 分批送达（每 64 条或每 100 ms 一批），最后是一条 `metadata:probeComplete`。每条结果说明信息来自缓存还是磁盘，失败分类为 `not-found`、`unsupported-format` 或 `read-error`。
   */
  probeBatchAsync(params: ProbeBatchAsyncParams): ProbeBatchAsyncResult;

  /**
   * Stop a probe started by `metadata.probeBatchAsync`. The read in progress is interrupted, the
   * paths not yet reached are never reported, and the run still ends with
   * `metadata:probeComplete` carrying `cancelled: true`.
   * @zh 停止由 `metadata.probeBatchAsync` 启动的探测。正在进行的读取被打断，尚未轮到的路径不会上报，收尾仍会发出带 `cancelled: true` 的 `metadata:probeComplete`。
   */
  cancelProbe(params: CancelProbeParams): CancelProbeResult;

  /**
   * Queue a tag write to one track and return at once. foobar2000 writes the file in the
   * background and reports the outcome as `metadata:writeComplete`, broadcast to every window. A
   * request that leaves nothing to write succeeds without dispatching.
   * @zh 把对单个曲目的标签写入排入队列并立即返回。foobar2000 在后台写文件，结果以广播给所有窗口的 `metadata:writeComplete` 事件报告。请求里没有可写的内容时直接成功，不派发。
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
   * @zh 每条排入一次 `metadata.write`，报告成功排入的条数。已派发的写入稍后以 `metadata:writeComplete` 报告各自的结果，所以不存在的文件在这里也计为成功。有任何条目失败时调用以 `OPERATION_FAILED` 失败，失败信封同样带 `successCount`、`failCount` 与 `errors`；其他条目照常排入。
   * @effect destructive
   * @idempotent
   */
  writeBatch(params: WriteBatchParams): WriteBatchResult;

  /**
   * Write a picture into a track's tags, into an image file next to it, or both. With one
   * target the result describes that write; with both, each target's outcome is reported under
   * `results` and the call fails with `OPERATION_FAILED` only when both failed.
   * @zh 把图片写进曲目的标签、写成音频旁的图片文件，或两者都写。单个目标时结果描述这次写入；两个目标时各目标的结果在 `results` 下，只有两者都失败才以 `OPERATION_FAILED` 失败。
   * @effect destructive
   * @idempotent
   */
  embedArtwork(params: EmbedArtworkParams): EmbedArtworkResult;

  /**
   * Remove embedded pictures from a file: one type, or every picture when `type` is omitted or
   * `removeAll` is set. A format the album art editor does not support fails with
   * `NOT_SUPPORTED`.
   * @zh 删除文件里嵌入的图片：删一种类型，或在省略 `type`、设置 `removeAll` 时删除全部。专辑封面编辑器不支持的格式以 `NOT_SUPPORTED` 失败。
   * @effect destructive
   * @idempotent
   */
  removeEmbeddedArt(params: RemoveEmbeddedArtParams): RemoveEmbeddedArtResult;

  /**
   * Queue the removal of tags from one track and return at once. The outcome arrives as
   * `metadata:writeComplete` with `operation: "removeTag"`, broadcast to every window.
   * @zh 把对单个曲目的标签删除排入队列并立即返回。结果以广播给所有窗口、`operation` 为 `"removeTag"` 的 `metadata:writeComplete` 事件报告。
   */
  removeTag(params: RemoveTagParams): RemoveTagResult;

  /**
   * Same as `metadata.removeTag`, under the name older callers use; its completion event says
   * `removeTag` as well.
   * @zh 与 `metadata.removeTag` 相同，保留给沿用旧名的调用方；完成事件同样报 `removeTag`。
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
   * @zh `metadata.probeBatchAsync` 的结果，分批送达：积满 64 条或距上一批过了 100 ms 就发一批，最后不满一批的那些在 `metadata:probeComplete` 之前送达。取消时正在读和尚未轮到的路径不上报。发给发起探测的页面；那时该窗口已不在的，发给主窗口的页面。
   * @delivery caller
   */
  probeProgress: ProbeProgressPayload;

  /**
   * The last event of a probe, sent after a cancel and after a failed run too; only an exception
   * while sending it, in practice running out of memory, skips it. Afterwards the `operationId`
   * is gone and `metadata.cancelProbe` reports `cancelled: false` for it. Delivered like
   * `metadata:probeProgress`.
   * @zh 一次探测的最后一个事件，取消之后、探测失败时也会发；只有发送它时抛出异常（实际上只有内存耗尽）才会缺失。之后 `operationId` 失效，对它调用 `metadata.cancelProbe` 报 `cancelled: false`。投递范围同 `metadata:probeProgress`。
   * @delivery caller
   */
  probeComplete: ProbeCompletePayload;

  /**
   * A tag write queued by `metadata.write`, `metadata.writeBatch`, `metadata.removeTag` or
   * `metadata.removeField` finished. Every window receives it.
   * @zh 由 `metadata.write`、`metadata.writeBatch`、`metadata.removeTag` 或 `metadata.removeField` 排入的标签写入结束了。所有窗口都会收到。
   * @delivery broadcast
   */
  writeComplete: WriteCompletePayload;
}

interface ProbeProgressPayload {
  /**
   * Id from the `metadata.probeBatchAsync` receipt.
   * @zh `metadata.probeBatchAsync` 回执里的 id。
   */
  operationId: string;
  /**
   * Paths reported so far across all batches, failures included.
   * @zh 到目前为止各批累计上报的路径数，含失败的。
   */
  done: Int;
  /**
   * Paths the call accepted, its `totalCount`.
   * @zh 调用受理的路径数，即它的 `totalCount`。
   */
  total: Int;
  /**
   * The results of this batch only, in request order.
   * @zh 仅本批的结果，顺序与请求一致。
   */
  results: MetadataProbeResultItem[];
}

/**
 * The result of probing one path.
 * @zh 一条路径的探测结果。
 */
interface MetadataProbeResultItem {
  /**
   * The path as requested, `|subsong:N` included.
   * @zh 请求里的路径，原样，含 `|subsong:N`。
   */
  path: string;
  /**
   * Whether the track was read.
   * @zh 曲目是否读取成功。
   */
  success: boolean;
  /**
   * `cached` when the host's complete cached info was used, `direct` when the file was read, `none`
   * with a failure.
   * @zh 用了宿主缓存里完整的信息为 `cached`，读了文件为 `direct`，失败时为 `none`。
   */
  infoSource: 'cached' | 'direct' | 'none';
  /**
   * Why the path was not read; present when `success` is `false`.
   * @zh 路径读取失败的原因；`success` 为 `false` 时出现。
   */
  failure?: 'not-found' | 'unsupported-format' | 'read-error';
  /**
   * Technical info of the track, as in `metadata.read`; present when `success` is `true`.
   * @zh 曲目的技术信息，同 `metadata.read`；`success` 为 `true` 时出现。
   */
  info?: TrackTechnicalInfo;
  /**
   * The flat fields of the track, as `includeTags` describes; present when `success` is `true`
   * and the call did not pass `includeTags: false`.
   * @zh 曲目的扁平字段，见 `includeTags` 的说明；`success` 为 `true` 且调用没传 `includeTags: false` 时出现。
   */
  tags?: Record<string, Json>;
}

interface ProbeCompletePayload {
  /**
   * Id from the `metadata.probeBatchAsync` receipt.
   * @zh `metadata.probeBatchAsync` 回执里的 id。
   */
  operationId: string;
  /**
   * Paths the call accepted.
   * @zh 调用受理的路径数。
   */
  total: Int;
  /**
   * Paths reported with `success: true`.
   * @zh 以 `success: true` 上报的路径数。
   */
  successCount: Int;
  /**
   * Paths reported with `success: false`.
   * @zh 以 `success: false` 上报的路径数。
   */
  failureCount: Int;
  /**
   * `true` when the probe was stopped early, by `metadata.cancelProbe` or because the popup that
   * started it closed; the two counts then fall short of `total`.
   * @zh 探测被提前停止时为 `true`：`metadata.cancelProbe`，或发起它的 popup 关闭了；此时两个计数之和小于 `total`。
   */
  cancelled: boolean;
}

interface WriteCompletePayload {
  /**
   * `write` for `metadata.write` and `metadata.writeBatch`, `removeTag` for `metadata.removeTag`
   * and `metadata.removeField`.
   * @zh `metadata.write` 与 `metadata.writeBatch` 为 `write`，`metadata.removeTag` 与 `metadata.removeField` 为 `removeTag`。
   */
  operation: 'write' | 'removeTag';
  /**
   * The path as the call gave it, suffix included.
   * @zh 调用给出的路径，原样，含后缀。
   */
  path: string;
  /**
   * The subsong written: `cueIndex` when it is 0 or more, otherwise the one the path selects,
   * otherwise `0`.
   * @zh 写入的子曲目：`cueIndex` 不小于 0 时就是它，否则取路径选中的，都没有时为 `0`。
   */
  subsong: Int;
  /**
   * foobar2000's completion code: `0` success, `1` aborted, `2` errors.
   * @zh foobar2000 的完成代码：`0` 成功，`1` 中止，`2` 出错。
   */
  code: Int;
  /**
   * `true` when `code` is `0`.
   * @zh `code` 为 `0` 时为 `true`。
   */
  success: boolean;
  /**
   * `success`, `aborted` or `error`, after `code`.
   * @zh 按 `code` 为 `success`、`aborted` 或 `error`。
   */
  status: 'success' | 'aborted' | 'error';
}

/**
 * A picture type; `cover_front` and `cover_back` are the foobar2000 spellings of `front` and
 * `back`.
 * @zh 图片类型；`cover_front` 与 `cover_back` 是 foobar2000 对 `front` 与 `back` 的写法。
 */
type ArtworkType = 'front' | 'cover_front' | 'back' | 'cover_back' | 'disc' | 'icon' | 'artist';

/**
 * Technical info of a track, as `metadata.read`, `metadata.readRaw` and the
 * `metadata:probeProgress` results report it.
 * @zh 曲目的技术信息，`metadata.read`、`metadata.readRaw` 与 `metadata:probeProgress` 的结果按此形状报告。
 */
interface TrackTechnicalInfo {
  /**
   * Length in seconds.
   * @zh 时长，单位秒。
   */
  duration: number;
  /**
   * Bitrate in kbit/s as the decoder reports it; `0` when unknown.
   * @zh 解码器报告的码率，单位 kbit/s；未知时为 `0`。
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
}

// ---- read ----

interface ReadParams {
  /**
   * Track path; a `|subsong:N` suffix or a trailing `#N` selects a subsong.
   * @zh 曲目路径；`|subsong:N` 后缀或结尾的 `#N` 选子曲目。
   * @minLength 1
   * @security MediaRead
   */
  path: string;
  /**
   * Subsong index that wins over a suffix in `path`; negative values are ignored.
   * @zh 子曲目序号，优先于 `path` 里的后缀；负数忽略。
   * @default -1
   */
  cueIndex?: Int;
}

interface ReadResult {
  /**
   * The path as given, suffix included.
   * @zh 原样的路径，含后缀。
   */
  path: string;
  /**
   * Every tag of the track, keyed by its name as the file spells it (case kept): a tag with one
   * value is a string, one with several values a string array.
   * @zh 曲目的全部标签，键为文件里原样的字段名（保留大小写）：单个值的标签是字符串，多个值的是字符串数组。
   */
  tags: Record<string, Json>;
  /**
   * Technical info of the track.
   * @zh 曲目的技术信息。
   */
  info: TrackTechnicalInfo;
}

// ---- readByPath ----

interface ReadByPathParams {
  /**
   * Track path; a `|subsong:N` suffix or a trailing `#N` selects a subsong.
   * @zh 曲目路径；`|subsong:N` 后缀或结尾的 `#N` 选子曲目。
   * @minLength 1
   * @security MediaRead
   */
  path: string;
  /**
   * Subsong index that wins over a suffix in `path`; negative values are ignored.
   * @zh 子曲目序号，优先于 `path` 里的后缀；负数忽略。
   * @default -1
   */
  cueIndex?: Int;
}

type ReadByPathResult = {
  /**
   * The path as given, suffix included.
   * @zh 原样的路径，含后缀。
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
 * @zh 曲目的一个标签或技术信息字段，键为大写名。单个值的标签是字符串，多个值的是字符串数组；技术信息字段是字符串。`DURATION` 总会出现：以秒计、保留三位小数的时长，如 `215.400`。`FILESIZE` 是字节数，只在宿主知道时出现，所以媒体库还没索引的文件没有它。文件没有 `TRACKNUMBER` 标签时，从文件名开头不超过三位的数字取得（`07 - Song.flac`、`(07) Song.flac`、`[07] Song.flac`），取不到时不出现。
 */
type FlatField = Json;

// ---- readRaw ----

interface ReadRawParams {
  /**
   * Track path; a `|subsong:N` suffix or a trailing `#N` selects a subsong.
   * @zh 曲目路径；`|subsong:N` 后缀或结尾的 `#N` 选子曲目。
   * @minLength 1
   * @security MediaRead
   */
  path: string;
  /**
   * Subsong index that wins over a suffix in `path`; negative values are ignored.
   * @zh 子曲目序号，优先于 `path` 里的后缀；负数忽略。
   * @default -1
   */
  cueIndex?: Int;
}

interface ReadRawResult {
  /**
   * The path as given, suffix included.
   * @zh 原样的路径，含后缀。
   */
  path: string;
  /**
   * Every tag of the track, as in `metadata.read`.
   * @zh 曲目的全部标签，同 `metadata.read`。
   */
  tags: Record<string, Json>;
  /**
   * Technical info of the track.
   * @zh 曲目的技术信息。
   */
  info: TrackTechnicalInfo;
  /**
   * Always `file`: the tags were read from the file itself, not from the host's cache.
   * @zh 恒为 `file`：标签直接读自文件，而不是宿主的缓存。
   */
  source: string;
}

// ---- readBatch ----

interface ReadBatchParams {
  /**
   * Track paths, each resolved on its own; a `|subsong:N` suffix or a trailing `#N` selects a
   * subsong, and there is no batch-wide `cueIndex`. An empty list is an empty success.
   * @zh 曲目路径，逐条独立解析；`|subsong:N` 后缀或结尾的 `#N` 选子曲目，没有整批的 `cueIndex`。空列表返回空的成功。
   * @security MediaRead
   */
  paths: string[];
}

interface ReadBatchResult {
  /**
   * Number of paths received.
   * @zh 收到的路径数。
   */
  total: Int;
  /**
   * Rows that were read.
   * @zh 读取成功的行数。
   */
  successCount: Int;
  /**
   * Rows that failed.
   * @zh 失败的行数。
   */
  errorCount: Int;
  /**
   * One row per path, in request order.
   * @zh 每个路径一行，顺序与请求一致。
   */
  results: MetadataReadBatchItem[];
}

/**
 * One row of `metadata.readBatch`.
 * @zh `metadata.readBatch` 的一行。
 */
interface MetadataReadBatchItem {
  /**
   * The path of this row, as given.
   * @zh 该行的路径，原样。
   */
  path: string;
  /**
   * Whether this row was read.
   * @zh 该行是否读取成功。
   */
  success: boolean;
  /**
   * The flat fields of the track, as in `metadata.readByPath` but without `path` and without a
   * `TRACKNUMBER` taken from the file name; present when `success` is `true`.
   * @zh 曲目的扁平字段，同 `metadata.readByPath`，但不带 `path`，也不从文件名取 `TRACKNUMBER`；`success` 为 `true` 时出现。
   */
  tags?: Record<string, Json>;
  /**
   * Why this row failed, such as `Failed to get track info`; present when `success` is `false`.
   * @zh 该行失败的原因，如 `Failed to get track info`；`success` 为 `false` 时出现。
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
   * @zh 待探测的路径，按此顺序每条上报一个结果。`|subsong:N` 后缀选子曲目；不认 `#N` 写法，因为它会把以 `#<数字>` 结尾的文件名切开。开始之前逐条检查全部路径；任何一条被拒，整次调用以 `PERMISSION_DENIED` 失败，不产生 `operationId`。
   * @minItems 1
   * @security MediaRead
   */
  paths: string[];
  /**
   * Attach the flat fields, as in `metadata.readByPath` without `path` and without the
   * `TRACKNUMBER` taken from the file name, to each successful result; `false` reports technical
   * info only.
   * @zh 为每条成功结果附上扁平字段（同 `metadata.readByPath`，不带 `path`，也不从文件名取 `TRACKNUMBER`）；为 `false` 时只报技术信息。
   * @default true
   */
  includeTags?: boolean;
}

interface ProbeBatchAsyncResult {
  /**
   * Id of the operation, starting with `probe_`. The events of the operation carry it, and
   * `metadata.cancelProbe` takes it.
   * @zh 操作的 id，以 `probe_` 开头。该操作的事件带有它，`metadata.cancelProbe` 也用它。
   */
  operationId: string;
  /**
   * Number of paths accepted; the events report it as `total`.
   * @zh 受理的路径数；事件里以 `total` 报告。
   */
  totalCount: Int;
}

// ---- cancelProbe ----

interface CancelProbeParams {
  /**
   * Id from the `metadata.probeBatchAsync` receipt.
   * @zh `metadata.probeBatchAsync` 回执里的 id。
   * @minLength 1
   */
  operationId: string;
}

interface CancelProbeResult {
  /**
   * `true` when the probe was still running and has been told to stop; `false` when it had
   * already finished or never existed, which are deliberately indistinguishable.
   * @zh 探测仍在进行并已被通知停止时为 `true`；已结束或从未存在时为 `false`，两者故意不可区分。
   */
  cancelled: boolean;
}

// ---- write ----

interface WriteParams {
  /**
   * Track path; a `|subsong:N` suffix or a trailing `#N` selects a subsong.
   * @zh 曲目路径；`|subsong:N` 后缀或结尾的 `#N` 选子曲目。
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
   * @zh 要改的标签，以名字为键。名字会转成大写，所以只有大小写不同的键指同一个标签。`null`、空字符串或空数组删除该标签。非空字符串数组替换该标签的全部值，保留顺序、重复值和空白，不按逗号或分号拆分。数组元素必须是非空且不含 NUL 的字符串；非法元素使该曲目以 `INVALID_PARAMS` 失败，不派发它的任何字段，错误中包含标签名和从 0 起的元素下标。单个字符串原样写入；整数或小数写成十进制文本（`2.5` 写成 `2.500000`）；布尔或对象被忽略。文件格式或标签写入器可能限制多值支持，最终写入结果以完成事件为准。
   */
  tags: Record<string, Json>;
  /**
   * Subsong index that wins over a suffix in `path`; negative values are ignored.
   * @zh 子曲目序号，优先于 `path` 里的后缀；负数忽略。
   * @default -1
   */
  cueIndex?: Int;
}

interface WriteResult {
  /**
   * The path as given.
   * @zh 原样的路径。
   */
  path: string;
  /**
   * `No tags to update` when nothing was left to write; otherwise a reminder that the outcome
   * arrives as `metadata:writeComplete`.
   * @zh 没有可写的内容时为 `No tags to update`；否则提醒最终结果由 `metadata:writeComplete` 事件报告。
   */
  note: string;
  /**
   * `true` when the write was dispatched; absent when there was nothing to write.
   * @zh 写入已派发时为 `true`；没有可写的内容时不出现。
   */
  dispatched?: boolean;
  /**
   * Path of the handle the write targets, as foobar2000 stores it; present when dispatched.
   * @zh 写入所针对句柄的路径（foobar2000 存储的形式）；派发时出现。
   */
  handlePath?: string;
  /**
   * Subsong the write targets; present when dispatched.
   * @zh 写入所针对的子曲目；派发时出现。
   */
  subsong?: Int;
  /**
   * Every tag queued, keyed by its upper-case name: the string, number or non-empty string array
   * given for a write, `null` for a removal (including an empty array). Present when dispatched.
   * @zh 排入的全部标签，键是大写标签名：写入时为给出的字符串、数字或非空字符串数组，删除时（含空数组）为 `null`。派发时出现。
   */
  tagsApplied?: Record<string, Json>;
  /**
   * Number of tag fields set, not the number of values; present when dispatched.
   * @zh 设置的标签字段数，不是值的数量；派发时出现。
   */
  tagsSet?: Int;
  /**
   * Number of tag fields removed, including those given as empty arrays; present when dispatched.
   * @zh 删除的标签字段数，包含以空数组指定的字段；派发时出现。
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
   * @zh 要排入的写入，每条一个，按此顺序。条目不是对象、带有 `path`、`tags`、`cueIndex` 以外的成员、或没有字符串 `path` 时，调用以 `INVALID_PARAMS` 失败。开始之前逐条检查全部路径；任何一条被拒（空串也算），整次调用以 `PERMISSION_DENIED` 失败。空列表返回空的成功。
   * @security MediaWrite
   * @pathKey path
   */
  items: MetadataWriteBatchItem[];
}

/**
 * One entry of `metadata.writeBatch`.
 * @zh `metadata.writeBatch` 的一条。
 */
interface MetadataWriteBatchItem {
  /**
   * Track path, as in `metadata.write`.
   * @zh 曲目路径，同 `metadata.write`。
   * @minLength 1
   */
  path: string;
  /**
   * Tags to change, as in `metadata.write`. An entry whose `tags` is missing or not an object is
   * reported in `errors` as `Missing tags` instead of refusing the call. An invalid array value
   * also fails only this entry before any of its tags are queued; other entries still run.
   * @zh 要改的标签，同 `metadata.write`。`tags` 缺失或不是对象的条目以 `Missing tags` 记进 `errors`，不会整批拒绝。非法数组也只使本条目失败，不派发它的任何字段；其他条目照常处理。
   */
  tags?: Json;
  /**
   * Subsong index for this entry, as in `metadata.write`.
   * @zh 这一条的子曲目序号，同 `metadata.write`。
   * @default -1
   */
  cueIndex?: Int;
}

interface WriteBatchResult {
  /**
   * Entries whose write was dispatched or had nothing to write; each dispatched write reports its
   * outcome as `metadata:writeComplete`.
   * @zh 已派发写入或没有可写内容的条目数；每个已派发的写入以 `metadata:writeComplete` 报告结果。
   */
  successCount: Int;
  /**
   * Entries that failed; always `0` on success, since a failed entry fails the call.
   * @zh 失败的条目数；成功时恒为 `0`，因为有条目失败时整次调用失败。
   */
  failCount: Int;
  /**
   * The failed entries; always empty on success, since a failed entry fails the call.
   * @zh 失败的条目；成功时恒为空，因为有条目失败时整次调用失败。
   */
  errors: MetadataWriteBatchError[];
}

/**
 * One failed entry of `metadata.writeBatch`.
 * @zh `metadata.writeBatch` 的一条失败条目。
 */
interface MetadataWriteBatchError {
  /**
   * The entry's path.
   * @zh 该条目的路径。
   */
  path: string;
  /**
   * Why it failed: `Missing tags`, or the error `metadata.write` gives for that path.
   * @zh 失败原因：`Missing tags`，或 `metadata.write` 对该路径给出的错误。
   */
  error: string;
}

// ---- embedArtwork ----

interface EmbedArtworkParams {
  /**
   * Track path. The `embedded` target writes into this file, which needs a format the album art
   * editor supports; the `file` target writes into its directory with a `|subsong:N` suffix
   * dropped, so every track of a CUE sheet shares one image.
   * @zh 曲目路径。`embedded` 目标写入这个文件，文件格式须受专辑封面编辑器支持；`file` 目标写入它所在的目录（去掉 `|subsong:N` 后缀），所以同一个 CUE 的全部曲目共用一张图。
   * @minLength 1
   * @security MediaWrite
   */
  path: string;
  /**
   * The image as plain Base64, without a Data URL header or a `base64:` marker. Characters
   * outside the Base64 alphabet are skipped and decoding stops at the first `=`; input that
   * decodes to no bytes fails with `INVALID_PARAMS`.
   * @zh 图片的裸 Base64，不带 Data URL 头或 `base64:` 标记。Base64 字母表以外的字符被跳过，遇到第一个 `=` 即停止解码；解不出任何字节时以 `INVALID_PARAMS` 失败。
   * @minLength 1
   */
  imageData: string;
  /**
   * Picture type.
   * @zh 图片类型。
   * @default "front"
   */
  type?: ArtworkType;
  /**
   * Where to write: `embedded` (into the file's tags), `file` (an image next to the audio file,
   * named after `type`, `cover` for the front picture, with the extension read from the image
   * bytes), or `all` for both; several entries write to each. Omitted, it writes `embedded`.
   * Any other value fails with `INVALID_PARAMS`.
   * @zh 写到哪里：`embedded`（写进文件的标签）、`file`（音频旁的图片，按 `type` 命名，正面图为 `cover`，扩展名由图片字节判断），或 `all` 表示两处；给多项就各写一处。省略时写 `embedded`。其他值以 `INVALID_PARAMS` 失败。
   * @minItems 1
   */
  target?: string[];
  /**
   * File name for the `file` target, used as it is instead of the derived one (no extension is
   * added); must be a plain name without separators or traversal.
   * @zh `file` 目标用的文件名，原样代替推导出来的名字（不补扩展名）；必须是没有分隔符与穿越序列的纯文件名。
   */
  filename?: string;
}

interface EmbedArtworkResult {
  /**
   * The path as given.
   * @zh 原样的路径。
   */
  path: string;
  /**
   * The requested type, as given.
   * @zh 请求的类型，原样。
   */
  type: string;
  /**
   * Picture size in bytes after decoding; present for a single target.
   * @zh 解码后的图片字节数；单个目标时出现。
   */
  size?: Int;
  /**
   * Full path of the image file written; present for a single `file` target.
   * @zh 写出的图片文件的完整路径；单个 `file` 目标时出现。
   */
  savedTo?: string;
  /**
   * With both targets: each target's own envelope (`{ success, path, type, size }`, plus
   * `savedTo` for `file`, or `{ success: false, error, code }`) under `embedded` and `file`.
   * @zh 两个目标都写时：每个目标自己的信封（`{ success, path, type, size }`，`file` 另带 `savedTo`；或 `{ success: false, error, code }`），键为 `embedded` 与 `file`。
   */
  results?: Record<string, Json>;
}

// ---- removeEmbeddedArt ----

interface RemoveEmbeddedArtParams {
  /**
   * Track path; the file needs a format the album art editor supports.
   * @zh 曲目路径；文件格式须受专辑封面编辑器支持。
   * @minLength 1
   * @security MediaWrite
   */
  path: string;
  /**
   * Picture type to remove; omitted, every picture is removed.
   * @zh 要删除的图片类型；省略时删除全部图片。
   */
  type?: ArtworkType;
  /**
   * Remove every picture whatever `type` says.
   * @zh 不论 `type` 为何，删除全部图片。
   * @default false
   */
  removeAll?: boolean;
}

interface RemoveEmbeddedArtResult {
  /**
   * The path as given.
   * @zh 原样的路径。
   */
  path: string;
  /**
   * What was removed: `["all"]` when every picture went in one step, otherwise the types removed
   * one by one (`front`, `back`, `disc`, `icon`, `artist`), or the single requested `type` as
   * given.
   * @zh 删除了什么：一次删掉全部图片时为 `["all"]`，否则是逐个删除的类型（`front`、`back`、`disc`、`icon`、`artist`），或原样的单个 `type`。
   */
  removedTypes: string[];
}

// ---- removeTag / removeField ----

interface RemoveTagParams {
  /**
   * Track path; a `|subsong:N` suffix or a trailing `#N` selects a subsong.
   * @zh 曲目路径；`|subsong:N` 后缀或结尾的 `#N` 选子曲目。
   * @minLength 1
   * @security MediaWrite
   */
  path: string;
  /**
   * Tag names to remove; they are upper-cased. An empty list removes nothing and succeeds
   * without dispatching.
   * @zh 要删除的标签名，会转成大写。空列表不删除任何标签，直接成功，不派发。
   */
  tags: string[];
  /**
   * Subsong index that wins over a suffix in `path`; negative values are ignored.
   * @zh 子曲目序号，优先于 `path` 里的后缀；负数忽略。
   * @default -1
   */
  cueIndex?: Int;
}

interface RemoveTagResult {
  /**
   * The path as given.
   * @zh 原样的路径。
   */
  path: string;
  /**
   * The tag names queued for removal, upper-cased, in the order given.
   * @zh 排入删除的标签名，已转大写，顺序与给出的一致。
   */
  removedTags: string[];
  /**
   * Number of names in `removedTags`.
   * @zh `removedTags` 里的名字数。
   */
  removedCount: Int;
  /**
   * `true` when the removal was dispatched; absent for an empty `tags`.
   * @zh 删除已派发时为 `true`；`tags` 为空时不出现。
   */
  dispatched?: boolean;
  /**
   * Subsong the removal targets; present when dispatched.
   * @zh 删除所针对的子曲目；派发时出现。
   */
  subsong?: Int;
  /**
   * A reminder that the outcome arrives as `metadata:writeComplete`; present when dispatched.
   * @zh 提醒最终结果由 `metadata:writeComplete` 事件报告；派发时出现。
   */
  note?: string;
}
