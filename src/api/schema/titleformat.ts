import type { Int } from './common.js';

export interface Api {
  /**
   * Evaluate one title formatting pattern against one track, or against the playing track when
   * no path is given.
   * @zh 对单首曲目求值指定 Titleformat 表达式（省略路径时默认针对正在播放的曲目求值）
   */
  eval(params: EvalParams): EvalResult;

  /**
   * Evaluate one pattern against many tracks. The pattern is compiled once.
   * @zh 批量对多首曲目求值同一个 Titleformat 表达式（表达式在底层仅编译一次）
   */
  evalBatch(params: EvalBatchParams): EvalBatchResult;

  /**
   * Evaluate several named patterns against one track in a single pass. The patterns are
   * compiled as one script, so a pattern that fails to compile does not fail the call: the
   * result then has no field keys and no `infoAvailable`, unlike `titleformat.evalFieldsBatch`,
   * which fails with `INVALID_PARAMS`.
   * @zh 对单首曲目单次求值多个具名 Titleformat 表达式（各表达式合并为单一脚本编译；若编译失败调用仍返回成功，但结果中不包含字段键与 infoAvailable，行为与 titleformat.evalFieldsBatch 报错 INVALID_PARAMS 不同）
   */
  evalFields(params: EvalFieldsParams): EvalFieldsResult;

  /**
   * Evaluate several named patterns against many tracks. The merged pattern is compiled once.
   * @zh 批量对多首曲目求值多个具名 Titleformat 表达式（合并后的表达式仅编译一次）
   */
  evalFieldsBatch(params: EvalFieldsBatchParams): EvalFieldsBatchResult;

  /**
   * List commonly used title formatting fields, keyed by a readable name.
   * @zh 获取常用 Titleformat 字段表达式字典（以易读字段名为键）
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
   * @zh 待求值的曲目路径（支持 |subsong:N 后缀指定子曲目；省略时针对正在播放的曲目求值并填充 %playback_time% 与流媒体标题等动态字段，若当前未处于播放状态则以 NO_ACTIVE_ITEM 失败）
   * @minLength 1
   * @security MediaRead
   */
  path?: string;
  /**
   * Title formatting pattern, such as `%artist% - %title%`.
   * @zh Titleformat 格式化表达式（例如 %artist% - %title%）
   * @minLength 1
   */
  pattern: string;
}

interface EvalResult {
  /**
   * The path that was evaluated; the playing track's path when the request gave none.
   * @zh 实际参与求值的曲目路径（请求未传入路径时为当前播放曲目的路径）
   */
  path: string;
  /**
   * The pattern that was evaluated.
   * @zh 实际求值的 Titleformat 表达式
   */
  pattern: string;
  /**
   * Formatted text.
   * @zh 表达式格式化后的输出文本
   */
  result: string;
  /**
   * `false` when the host could not get the track's info (a remote path with nothing cached, or
   * a file that cannot be read), so tag-derived output is untrustworthy. A local file that
   * foobar2000 has not loaded is read from disk and reports `true`. Does not cover foo_playcount
   * fields such as `%rating%`.
   * @zh 曲目元数据是否可用（为 false 表示宿主无法读取曲目信息，如无缓存的网络流或不可读文件，此时基于标签的输出不可信；未载入过 foobar2000 的本地文件会从磁盘直接读取并返回 true；该标志不涵盖 foo_playcount 维护的 %rating% 等统计字段）
   */
  infoAvailable: boolean;
}

// ---- evalBatch ----

interface EvalBatchParams {
  /**
   * Paths of the tracks to evaluate against; a `|subsong:N` suffix selects a subsong.
   * @zh 待求值的曲目路径列表（支持 |subsong:N 后缀指定子曲目）
   * @security MediaRead
   */
  paths: string[];
  /**
   * Title formatting pattern applied to every path.
   * @zh 应用于列表中每个路径的 Titleformat 表达式
   * @minLength 1
   */
  pattern: string;
}

interface EvalBatchResult {
  /**
   * The pattern that was evaluated.
   * @zh 实际求值的 Titleformat 表达式
   */
  pattern: string;
  /**
   * Number of paths received.
   * @zh 传入待求值的曲目路径总数
   */
  total: Int;
  /**
   * Rows that evaluated.
   * @zh 成功完成格式化求值的条目数
   */
  successCount: Int;
  /**
   * Rows that failed.
   * @zh 格式化求值失败的条目数
   */
  errorCount: Int;
  /**
   * One row per path, in request order.
   * @zh 逐路径求值结果列表（保持与请求路径列表相同的顺序）
   */
  results: EvalBatchRow[];
}

interface EvalBatchRow {
  /**
   * The path of this row.
   * @zh 当前行对应的曲目路径
   */
  path: string;
  /**
   * Whether this row evaluated.
   * @zh 当前行是否成功完成求值
   */
  success: boolean;
  /**
   * Formatted text; present when `success` is `true`.
   * @zh 格式化后的输出文本（仅在 success 为 true 时提供）
   */
  result?: string;
  /**
   * Same meaning as in `titleformat.eval`; present when `success` is `true`.
   * @zh 曲目元数据是否可用（语义同 titleformat.eval；仅在 success 为 true 时提供）
   */
  infoAvailable?: boolean;
  /**
   * Why this row failed; present when `success` is `false`.
   * @zh 当前行求值失败的具体错误信息（仅在 success 为 false 时提供）
   */
  error?: string;
}

// ---- evalFields ----

interface EvalFieldsParams {
  /**
   * Path of the track to evaluate against; a `|subsong:N` suffix selects a subsong.
   * @zh 待求值的曲目路径（支持 |subsong:N 后缀指定子曲目）
   * @minLength 1
   * @security MediaRead
   */
  path: string;
  /**
   * Map from output key to title formatting pattern, such as `{ "year": "$year(%date%)" }`.
   * @zh 输出字段名到 Titleformat 表达式的键值映射字典（例如 { "year": "$year(%date%)" }）
   */
  fields: Record<string, string>;
}

type EvalFieldsResult = EvalFieldsNamed & Record<string, FieldText>;

interface EvalFieldsNamed {
  /**
   * The path that was evaluated.
   * @zh 实际参与求值的曲目路径
   */
  path: string;
  /**
   * `false` under the same conditions as in `titleformat.eval`, so tag-derived values are
   * untrustworthy. One flag covers the whole request. Absent when `fields` is empty.
   * @zh 曲目元数据是否可用（取值条件同 titleformat.eval，为 false 表示基于标签的输出不可信；单次请求内所有字段共享该标志；fields 为空时不提供）
   */
  infoAvailable?: boolean;
}

/**
 * One entry per key of `fields`, holding the formatted text. Keys named `success`, `path` or
 * `infoAvailable` are dropped because the response fields of those names take precedence. Keys
 * named `error` or `code` are kept, so check `success` to tell a failure.
 * @zh fields 映射中各字段格式化求值后的文本（键名为 success、path 或 infoAvailable 时将被丢弃以避免覆盖顶层同名结果字段；error 或 code 键名将被保留，调用成功与否以顶层 success 为准）
 */
type FieldText = string;

// ---- evalFieldsBatch ----

interface EvalFieldsBatchParams {
  /**
   * Paths of the tracks to evaluate against; a `|subsong:N` suffix selects a subsong.
   * @zh 待求值的曲目路径列表（支持 |subsong:N 后缀指定子曲目）
   * @security MediaRead
   */
  paths: string[];
  /**
   * Map from output key to title formatting pattern.
   * @zh 输出字段名到 Titleformat 表达式的键值映射字典
   */
  fields: Record<string, string>;
}

interface EvalFieldsBatchResult {
  /**
   * Number of paths received, or 0 when `fields` is empty.
   * @zh 传入待求值的曲目路径总数（fields 为空时固定为 0）
   */
  total: Int;
  /**
   * Rows that evaluated.
   * @zh 成功完成格式化求值的条目数
   */
  successCount: Int;
  /**
   * Rows that failed.
   * @zh 格式化求值失败的条目数
   */
  errorCount: Int;
  /**
   * One row per path, in request order.
   * @zh 逐路径批量求值结果列表（保持与请求路径列表相同的顺序）
   */
  results: (EvalFieldsBatchRow & Record<string, RowFieldText>)[];
}

interface EvalFieldsBatchRow {
  /**
   * The path of this row.
   * @zh 当前行对应的曲目路径
   */
  path: string;
  /**
   * Whether this row evaluated.
   * @zh 当前行是否成功完成求值
   */
  success: boolean;
  /**
   * Same meaning as in `titleformat.evalFields`; present when `success` is `true`.
   * @zh 曲目元数据是否可用（语义同 titleformat.evalFields；仅在 success 为 true 时提供）
   */
  infoAvailable?: boolean;
  /**
   * Why this row failed; present when `success` is `false`.
   * @zh 当前行求值失败的具体错误信息（仅在 success 为 false 时提供）
   */
  error?: string;
}

/**
 * One entry per key of `fields`, holding the formatted text. Keys named `path`, `success` or
 * `infoAvailable` are dropped because the row's own fields of those names take precedence. A key
 * named `error` is kept, so check `success` to tell a failed row.
 * @zh fields 映射中各字段格式化求值后的文本（键名为 path、success 或 infoAvailable 时将被丢弃以避免覆盖当前行同名属性；error 键名将被保留，当前行成功与否以行内 success 为准）
 */
type RowFieldText = string;

// ---- getBuiltinFields ----

interface GetBuiltinFieldsResult {
  /**
   * Map from readable name to pattern, such as `"year": "$year(%date%)"`. Entries under
  * playcount need the foo_playcount component.
   * @zh 常用字段名到对应 Titleformat 表达式的映射字典（例如 "year": "$year(%date%)"；播放统计相关字段依赖 foo_playcount 组件）
   */
  fields: Record<string, string>;
}
