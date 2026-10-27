import type { Int } from './common.js';

export interface Api {
  /**
   * Read the foo_playcount statistics of tracks. Without foo_playcount installed the counts read
   * as zero and the dates are absent.
   * @zh 批量读取曲目的 foo_playcount 播放统计指标（未安装 foo_playcount 时播放次数统一返回 0 且日期字段缺省）
   */
  get(params: GetParams): GetResult;

  /**
   * Same as `playcount.get`, under the name batch callers expect.
   * @zh 批量读取曲目播放统计（功能与参数完全等同于 playcount.get，保留供兼容批量调用命名习惯）
   */
  getBatch(params: GetParams): GetResult;

  /**
   * Placeholder: foo_playcount offers no way to change statistics, so this always fails with
   * `NOT_SUPPORTED`. Change ratings with `rating.set`.
   * @zh 尝试写入播放统计数据（占位接口：因 foo_playcount 未提供外部修改统计机制，调用恒以 NOT_SUPPORTED 失败；如需修改评分请使用 rating.set）
   */
  set(params: SetParams): void;

  /**
   * Summarise play counts and ratings over the whole media library.
   * @zh 汇总全媒体库曲目的播放次数与星级评分综合统计指标
   */
  getStats(): GetStatsResult;
}

interface GetParams {
  /**
   * Track paths; a `|subsong:N` suffix selects a CUE subsong. A suffix whose index cannot be read,
   * such as `|subsong:abc`, is dropped and the path means the first track.
   * @zh 待查询的曲目路径列表（支持 |subsong:N 语法指定 CUE 子曲目；若后缀索引无法解析为有效数字则自动剥除并回退指向首轨）
   * @security MediaRead
   */
  paths: string[];
}

/** Statistics of one requested track. */
interface PlaycountRow {
  /**
   * The path as requested, including any `|subsong:N` suffix, on successful and failed rows
   * alike.
   * @zh 请求时传入的原始曲目路径（在成功与失败行中均原样保留 |subsong:N 等后缀）
   */
  path: string;
  /**
   * Whether the track could be resolved.
   * @zh 宿主是否成功解析到该曲目句柄
   */
  success: boolean;
  /**
   * Why the track could not be resolved; present when `success` is `false`.
   * @zh 曲目解析失败的具体原因（仅在 success 为 false 时提供）
   */
  error?: string;
  /**
   * Number of plays; `0` when never played. Present when `success` is `true`.
   * @zh 累计播放总次数（未曾播放时为 0；仅在 success 为 true 时提供）
   */
  playCount?: Int;
  /**
   * Time of the first play, as foo_playcount formats it. Absent when unknown.
   * @zh 首次播放时间戳文本（格式由 foo_playcount 决定；时间未知时缺省）
   */
  firstPlayed?: string;
  /**
   * Time of the most recent play. Absent when unknown.
   * @zh 最近一次播放时间戳文本（时间未知时缺省）
   */
  lastPlayed?: string;
  /**
   * Time the track was added to the media library. Absent when unknown.
   * @zh 曲目被添加至媒体库的时间戳文本（时间未知时缺省）
   */
  added?: string;
  /**
   * Rating from 1 to 5. Absent when the track is unrated, never `0`.
   * @zh 星级评分数值（取值范围 1 至 5；未评分曲目缺省，不会返回 0）
   */
  rating?: Int;
  /**
   * Whether the track is in the media library. Present when `success` is `true`.
   * @zh 当前曲目是否已纳入媒体库（仅在 success 为 true 时提供）
   */
  inLibrary?: boolean;
}

interface GetResult {
  /**
   * Number of rows in `results`.
   * @zh results 数组包含的统计数据行总数
   */
  count: Int;
  /**
   * One row per requested path, in request order.
   * @zh 逐路径统计结果列表（保持与请求路径列表相同的顺序）
   */
  results: PlaycountRow[];
}

interface SetParams {
  /**
   * Path of the track.
   * @zh 目标曲目文件路径
   * @security MediaWrite
   */
  path: string;
}

interface GetStatsResult {
  /**
   * Tracks in the media library.
   * @zh 媒体库中索引的曲目总数
   */
  totalTracks: Int;
  /**
   * Tracks played at least once.
   * @zh 至少播放过一次的曲目总数
   */
  playedTracks: Int;
  /**
   * Tracks never played.
   * @zh 从未播放过的曲目总数
   */
  unplayedTracks: Int;
  /**
   * Tracks with a rating.
   * @zh 已设定星级评分（1 至 5 星）的曲目总数
   */
  ratedTracks: Int;
  /**
   * Sum of all play counts.
   * @zh 媒体库中所有曲目的累计播放总次数之和
   */
  totalPlayCount: Int;
  /**
   * Highest play count of any track.
   * @zh 单首曲目的最高累计播放次数
   */
  maxPlayCount: Int;
  /**
   * Mean play count of the played tracks; `0` when none was played.
   * @zh 已播放曲目的平均播放次数（若无任何播放记录则返回 0）
   */
  averagePlayCount: number;
  /**
   * Mean rating of the rated tracks; `0` when none is rated.
   * @zh 已评分曲目的平均星级评分（若无任何评分曲目则返回 0）
   */
  averageRating: number;
}
