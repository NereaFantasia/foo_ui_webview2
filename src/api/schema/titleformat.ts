import type { Int } from './common.js';

export interface Api {
  /**
   * Evaluate one title formatting pattern against one track, or against the playing track when
   * no path is given.
   * @zh 对单个文件求值单个 titleformat 表达式；不给路径时对正在播放的曲目求值。
   */
  eval(params: EvalParams): EvalResult;

  /**
   * Evaluate one pattern against many tracks. The pattern is compiled once.
   * @zh 对多个文件求值同一个表达式，表达式只编译一次。
   */
  evalBatch(params: EvalBatchParams): EvalBatchResult;

  /**
   * Evaluate several named patterns against one track in a single pass. The patterns are
   * compiled as one script, so a pattern that fails to compile does not fail the call: the
   * result then has no field keys and no `infoAvailable`, unlike `titleformat.evalFieldsBatch`,
   * which fails with `INVALID_PARAMS`.
   * @zh 对单个文件一次求值多个具名表达式。各表达式合成一个脚本编译，所以编译失败不会让调用失败：此时结果里没有任何字段键，也没有 `infoAvailable`；`titleformat.evalFieldsBatch` 则以 `INVALID_PARAMS` 失败。
   */
  evalFields(params: EvalFieldsParams): EvalFieldsResult;

  /**
   * Evaluate several named patterns against many tracks. The merged pattern is compiled once.
   * @zh 对多个文件求值多个具名表达式，合并后的表达式只编译一次。
   */
  evalFieldsBatch(params: EvalFieldsBatchParams): EvalFieldsBatchResult;

  /**
   * List commonly used title formatting fields, keyed by a readable name.
   * @zh 列出常用的 titleformat 字段，以易读的名称为键。
   */
  getBuiltinFields(): GetBuiltinFieldsResult;
}

// ---- eval ----

interface EvalParams {
  /**
   * Path of the track to evaluate against; a `|subsong:N` suffix selects a subsong, such as one
   * track of a CUE sheet. Omit it to evaluate against the playing track, which also fills dynamic
   * fields such as `%playback_time%` and stream titles; the call then fails with `NO_ACTIVE_ITEM`
   * when nothing is playing.
   * @zh 要求值的曲目路径；带 `|subsong:N` 后缀时选子曲目，如 CUE 里的一首。省略时对正在播放的曲目求值，`%playback_time%`、网络流标题等动态字段也会填上；此时没有曲目在播放则以 `NO_ACTIVE_ITEM` 失败。
   * @minLength 1
   * @security MediaRead
   */
  path?: string;
  /**
   * Title formatting pattern, such as `%artist% - %title%`.
   * @zh titleformat 表达式，如 `%artist% - %title%`。
   * @minLength 1
   */
  pattern: string;
}

interface EvalResult {
  /**
   * The path that was evaluated; the playing track's path when the request gave none.
   * @zh 被求值的路径；请求没给路径时为正在播放曲目的路径。
   */
  path: string;
  /**
   * The pattern that was evaluated.
   * @zh 被求值的表达式。
   */
  pattern: string;
  /**
   * Formatted text.
   * @zh 格式化后的文本。
   */
  result: string;
  /**
   * `false` when the host could not get the track's info (a remote path with nothing cached, or
   * a file that cannot be read), so tag-derived output is untrustworthy. A local file that
   * foobar2000 has not loaded is read from disk and reports `true`. Does not cover foo_playcount
   * fields such as `%rating%`.
   * @zh 为 `false` 表示宿主拿不到该曲目的信息（远程路径且没有缓存，或文件读不了），基于标签的输出不可信。foobar2000 没载入过的本地文件会从磁盘读取，报 `true`。不涵盖 `%rating%` 等 foo_playcount 字段。
   */
  infoAvailable: boolean;
}

// ---- evalBatch ----

interface EvalBatchParams {
  /**
   * Paths of the tracks to evaluate against; a `|subsong:N` suffix selects a subsong.
   * @zh 要求值的曲目路径列表；带 `|subsong:N` 后缀时选子曲目。
   * @security MediaRead
   */
  paths: string[];
  /**
   * Title formatting pattern applied to every path.
   * @zh 对每个路径应用的 titleformat 表达式。
   * @minLength 1
   */
  pattern: string;
}

interface EvalBatchResult {
  /**
   * The pattern that was evaluated.
   * @zh 被求值的表达式。
   */
  pattern: string;
  /**
   * Number of paths received.
   * @zh 收到的路径数。
   */
  total: Int;
  /**
   * Rows that evaluated.
   * @zh 求值成功的行数。
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
  results: EvalBatchRow[];
}

interface EvalBatchRow {
  /**
   * The path of this row.
   * @zh 该行的路径。
   */
  path: string;
  /**
   * Whether this row evaluated.
   * @zh 该行是否求值成功。
   */
  success: boolean;
  /**
   * Formatted text; present when `success` is `true`.
   * @zh 格式化后的文本；`success` 为 `true` 时出现。
   */
  result?: string;
  /**
   * Same meaning as in `titleformat.eval`; present when `success` is `true`.
   * @zh 含义同 `titleformat.eval`；`success` 为 `true` 时出现。
   */
  infoAvailable?: boolean;
  /**
   * Why this row failed; present when `success` is `false`.
   * @zh 该行失败的原因；`success` 为 `false` 时出现。
   */
  error?: string;
}

// ---- evalFields ----

interface EvalFieldsParams {
  /**
   * Path of the track to evaluate against; a `|subsong:N` suffix selects a subsong.
   * @zh 要求值的曲目路径；带 `|subsong:N` 后缀时选子曲目。
   * @minLength 1
   * @security MediaRead
   */
  path: string;
  /**
   * Map from output key to title formatting pattern, such as `{ "year": "$year(%date%)" }`.
   * @zh 输出键到 titleformat 表达式的映射，如 `{ "year": "$year(%date%)" }`。
   */
  fields: Record<string, string>;
}

type EvalFieldsResult = EvalFieldsNamed & Record<string, FieldText>;

interface EvalFieldsNamed {
  /**
   * The path that was evaluated.
   * @zh 被求值的路径。
   */
  path: string;
  /**
   * `false` under the same conditions as in `titleformat.eval`, so tag-derived values are
   * untrustworthy. One flag covers the whole request. Absent when `fields` is empty.
   * @zh 与 `titleformat.eval` 同样的条件下为 `false`，基于标签的值不可信；整个请求共用一个标志。`fields` 为空时不出现。
   */
  infoAvailable?: boolean;
}

/**
 * One entry per key of `fields`, holding the formatted text. Keys named `success`, `path` or
 * `infoAvailable` are dropped because the response fields of those names take precedence. Keys
 * named `error` or `code` are kept, so check `success` to tell a failure.
 * @zh `fields` 的每个键对应一项，值为格式化后的文本。键名为 `success`、`path` 或 `infoAvailable` 的项会被丢弃，
 * 响应里的同名字段优先；键名为 `error` 或 `code` 的项会保留，判断是否失败要看 `success`。
 */
type FieldText = string;

// ---- evalFieldsBatch ----

interface EvalFieldsBatchParams {
  /**
   * Paths of the tracks to evaluate against; a `|subsong:N` suffix selects a subsong.
   * @zh 要求值的曲目路径列表；带 `|subsong:N` 后缀时选子曲目。
   * @security MediaRead
   */
  paths: string[];
  /**
   * Map from output key to title formatting pattern.
   * @zh 输出键到 titleformat 表达式的映射。
   */
  fields: Record<string, string>;
}

interface EvalFieldsBatchResult {
  /**
   * Number of paths received, or 0 when `fields` is empty.
   * @zh 收到的路径数；`fields` 为空时为 0。
   */
  total: Int;
  /**
   * Rows that evaluated.
   * @zh 求值成功的行数。
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
  results: (EvalFieldsBatchRow & Record<string, RowFieldText>)[];
}

interface EvalFieldsBatchRow {
  /**
   * The path of this row.
   * @zh 该行的路径。
   */
  path: string;
  /**
   * Whether this row evaluated.
   * @zh 该行是否求值成功。
   */
  success: boolean;
  /**
   * Same meaning as in `titleformat.evalFields`; present when `success` is `true`.
   * @zh 含义同 `titleformat.evalFields`；`success` 为 `true` 时出现。
   */
  infoAvailable?: boolean;
  /**
   * Why this row failed; present when `success` is `false`.
   * @zh 该行失败的原因；`success` 为 `false` 时出现。
   */
  error?: string;
}

/**
 * One entry per key of `fields`, holding the formatted text. Keys named `path`, `success` or
 * `infoAvailable` are dropped because the row's own fields of those names take precedence. A key
 * named `error` is kept, so check `success` to tell a failed row.
 * @zh `fields` 的每个键对应一项，值为格式化后的文本。键名为 `path`、`success` 或 `infoAvailable` 的项会被丢弃，
 * 该行的同名字段优先；键名为 `error` 的项会保留，判断该行是否失败要看 `success`。
 */
type RowFieldText = string;

// ---- getBuiltinFields ----

interface GetBuiltinFieldsResult {
  /**
   * Map from readable name to pattern, such as `"year": "$year(%date%)"`. Entries under
   * playcount need the foo_playcount component.
   * @zh 易读名称到表达式的映射，如 `"year": "$year(%date%)"`。播放统计类条目需要 foo_playcount 组件。
   */
  fields: Record<string, string>;
}
