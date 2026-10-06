import type { Int } from './common.js';

export interface Api {
  /**
   * Read the foo_playcount statistics of tracks. Without foo_playcount installed the counts read
   * as zero and the dates are absent.
   * @zh 读取曲目的 foo_playcount 播放统计。没装 foo_playcount 时次数读作 0，日期不出现。
   */
  get(params: GetParams): GetResult;

  /**
   * Same as `playcount.get`, under the name batch callers expect.
   * @zh 与 `playcount.get` 相同，保留给按批调用的写法。
   */
  getBatch(params: GetParams): GetResult;

  /**
   * Placeholder: foo_playcount offers no way to change statistics, so this always fails with
   * `NOT_SUPPORTED`. Change ratings with `rating.set`.
   * @zh 占位方法：foo_playcount 不提供修改统计的途径，所以总是以 `NOT_SUPPORTED` 失败。改评分请用 `rating.set`。
   */
  set(params: SetParams): void;

  /**
   * Summarise play counts and ratings over the whole media library.
   * @zh 汇总整个媒体库的播放次数与评分。
   */
  getStats(): GetStatsResult;
}

interface GetParams {
  /**
   * Track paths; a `|subsong:N` suffix selects a CUE subsong. A suffix whose index cannot be read,
   * such as `|subsong:abc`, is dropped and the path means the first track.
   * @zh 曲目路径；带 `|subsong:N` 后缀时选 CUE 子曲目。读不出序号的后缀（如 `|subsong:abc`）被去掉，路径指第一首。
   * @security MediaRead
   */
  paths: string[];
}

/** Statistics of one requested track. */
interface PlaycountRow {
  /**
   * The path as requested, including any `|subsong:N` suffix, on successful and failed rows
   * alike.
   * @zh 请求时给的路径，后缀 `|subsong:N` 原样保留；成功行与失败行都一样。
   */
  path: string;
  /**
   * Whether the track could be resolved.
   * @zh 是否解析到了这首曲目。
   */
  success: boolean;
  /**
   * Why the track could not be resolved; present when `success` is `false`.
   * @zh 解析不到这首曲目的原因；`success` 为 `false` 时出现。
   */
  error?: string;
  /**
   * Number of plays; `0` when never played. Present when `success` is `true`.
   * @zh 播放次数，从未播放时为 `0`。`success` 为 `true` 时出现。
   */
  playCount?: Int;
  /**
   * Time of the first play, as foo_playcount formats it. Absent when unknown.
   * @zh 首次播放时间，格式由 foo_playcount 决定。未知时不出现。
   */
  firstPlayed?: string;
  /**
   * Time of the most recent play. Absent when unknown.
   * @zh 最近一次播放时间。未知时不出现。
   */
  lastPlayed?: string;
  /**
   * Time the track was added to the media library. Absent when unknown.
   * @zh 曲目加入媒体库的时间。未知时不出现。
   */
  added?: string;
  /**
   * Rating from 1 to 5. Absent when the track is unrated, never `0`.
   * @zh 评分，1 到 5。未评分时不出现，不会是 `0`。
   */
  rating?: Int;
  /**
   * Whether the track is in the media library. Present when `success` is `true`.
   * @zh 曲目是否在媒体库中。`success` 为 `true` 时出现。
   */
  inLibrary?: boolean;
}

interface GetResult {
  /**
   * Number of rows in `results`.
   * @zh `results` 的行数。
   */
  count: Int;
  /**
   * One row per requested path, in request order.
   * @zh 每个请求路径一行，按请求顺序排列。
   */
  results: PlaycountRow[];
}

interface SetParams {
  /**
   * Path of the track.
   * @zh 曲目路径。
   * @security MediaWrite
   */
  path: string;
}

interface GetStatsResult {
  /**
   * Tracks in the media library.
   * @zh 媒体库里的曲目数。
   */
  totalTracks: Int;
  /**
   * Tracks played at least once.
   * @zh 至少播放过一次的曲目数。
   */
  playedTracks: Int;
  /**
   * Tracks never played.
   * @zh 从未播放过的曲目数。
   */
  unplayedTracks: Int;
  /**
   * Tracks with a rating.
   * @zh 有评分的曲目数。
   */
  ratedTracks: Int;
  /**
   * Sum of all play counts.
   * @zh 所有播放次数之和。
   */
  totalPlayCount: Int;
  /**
   * Highest play count of any track.
   * @zh 单首曲目的最高播放次数。
   */
  maxPlayCount: Int;
  /**
   * Mean play count of the played tracks; `0` when none was played.
   * @zh 已播放曲目的平均播放次数；一首都没播放过时为 `0`。
   */
  averagePlayCount: number;
  /**
   * Mean rating of the rated tracks; `0` when none is rated.
   * @zh 有评分曲目的平均评分；一首都没有评分时为 `0`。
   */
  averageRating: number;
}
