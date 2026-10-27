import type { Int } from './common.js';

export interface Api {
  /**
   * Read a track's rating from 0 to 5: a foo_playcount `%rating%` between 1 and 5 wins,
   * otherwise the file's `RATING` tag clamped to 0..5. `0` means unrated. A file whose tags
   * cannot be read, a missing file included, is not an error: it reads as having no `RATING`
   * tag.
   * @zh 获取指定曲目的星级评分（范围 0 至 5；优先读取 foo_playcount 维护的 1 至 5 星 %rating% 统计数据，未找到时回退读取音频文件的 RATING 标签并夹取至 0 至 5 区间；0 表示未评分；无法读取标签或文件不存在时不返回错误，统一视作无 RATING 标签处理）
   * @effect read
   */
  get(params: GetParams): GetResult;

  /**
   * Set a track's rating through foo_playcount's context menu, or write the file's `RATING`
   * tag when that menu is not available. Without `path` the playing track is rated, or else the
   * active playlist's selection; with neither the call fails with `NO_ACTIVE_ITEM`.
   * @zh 设置指定曲目的星级评分（优先通过 foo_playcount 上下文菜单写入统计，菜单不可用时回退写入音频文件的 RATING 标签；省略 path 时优先为正在播放的曲目评分，无播放曲目时回退为当前活动播放列表的选中项，两者均无时返回 NO_ACTIVE_ITEM 失败）
   * @effect destructive
   * @idempotent
   */
  set(params: SetParams): SetResult;
}

interface GetParams {
  /**
   * Track path; a `|subsong:N` suffix or a trailing `#N` selects a subsong.
   * @zh 待查询曲目的文件路径（支持 |subsong:N 或末尾 #N 语法指定子曲目）
   * @minLength 1
   * @security MediaRead
   */
  path: string;
  /**
   * Subsong index that wins over a suffix in `path`; negative values are ignored.
   * @zh 子曲目索引（优先级高于 path 中的后缀；传入负数时忽略该参数）
   * @default -1
   */
  cueIndex?: Int;
}

interface GetResult {
  /**
   * The path as given, suffix included.
   * @zh 查询时传入的原样曲目路径（包含子曲目后缀）
   */
  path: string;
  /**
   * Rating from 0 to 5; `0` means unrated.
   * @zh 曲目星级评分（取值范围 0 至 5，0 表示未评分）
   */
  rating: Int;
  /**
   * `stats` for a foo_playcount rating; `file` for the `RATING` tag, and also when neither
   * source has a rating.
   * @zh 评分数据存储来源（stats 表示来自 foo_playcount 统计数据库；file 表示来自音频文件 RATING 标签，两处均未评分时亦返回 file）
   */
  storage: 'stats' | 'file';
}

interface SetParams {
  /**
   * Track path; a `|subsong:N` suffix or a trailing `#N` selects a subsong. Omit it for the
   * playing track, or else the active playlist's selection.
   * @zh 待评分曲目的文件路径（支持 |subsong:N 或末尾 #N 语法指定子曲目；省略时优先对正在播放曲目评分，无播放曲目时对活动播放列表选中项评分）
   * @minLength 1
   * @security MediaWrite
   */
  path?: string;
  /**
   * Rating from 0 to 5; `0` clears it.
   * @zh 待设置的星级评分（取值范围 0 至 5，传入 0 表示清除评分）
   * @minimum 0
   * @maximum 5
   */
  rating: Int;
  /**
   * Subsong index that wins over a suffix in `path`; negative values are ignored.
   * @zh 子曲目索引（优先级高于 path 中的后缀；传入负数时忽略该参数）
   * @default -1
   */
  cueIndex?: Int;
}

interface SetResult {
  /**
   * The path as given; `(current)` when `path` was omitted and foo_playcount took the rating.
   * @zh 实际评分的曲目路径（省略 path 且由 foo_playcount 处理时固定为 (current)）
   */
  path: string;
  /**
   * The rating that was set.
   * @zh 实际成功设置的星级评分数值
   */
  rating: Int;
  /**
   * `stats` when foo_playcount's menu ran; `file` when the `RATING` tag write was dispatched.
   * @zh 实际生效的评分写入目标（stats 表示通过 foo_playcount 上下文菜单写入；file 表示派发写入音频文件 RATING 标签）
   */
  storage: 'stats' | 'file';
  /**
   * The context-menu path that ran; present when `storage` is `stats`.
   * @zh 实际执行的上下文菜单命令路径（仅在 storage 为 stats 时提供）
   */
  menuPath?: string;
  /**
   * Why the tag was written instead; present when `storage` is `file`.
   * @zh 回退写入文件标签的具体原因说明（仅在 storage 为 file 时提供）
   */
  note?: string;
}
