import type { Int } from './common.js';

export interface Api {
  /**
   * Read a track's rating from 0 to 5: a foo_playcount `%rating%` between 1 and 5 wins,
   * otherwise the file's `RATING` tag clamped to 0..5. `0` means unrated. A file whose tags
   * cannot be read, a missing file included, is not an error: it reads as having no `RATING`
   * tag.
   * @zh 读取曲目 0 到 5 的评分：foo_playcount 的 `%rating%` 在 1 到 5 之间时优先，否则取文件的 `RATING` 标签并夹到 0..5。`0` 表示未评分。读不出标签的文件（含不存在的文件）不算错误，按没有 `RATING` 标签处理。
   * @effect read
   */
  get(params: GetParams): GetResult;

  /**
   * Set a track's rating through foo_playcount's context menu, or write the file's `RATING`
   * tag when that menu is not available. Without `path` the playing track is rated, or else the
   * active playlist's selection; with neither the call fails with `NO_ACTIVE_ITEM`.
   * @zh 通过 foo_playcount 的右键菜单设置评分，菜单不可用时改写文件的 `RATING` 标签。省略 `path` 时给正在播放的曲目评分，否则给活动播放列表的选中项；两者都没有时以 `NO_ACTIVE_ITEM` 失败。
   * @effect destructive
   * @idempotent
   */
  set(params: SetParams): SetResult;
}

interface GetParams {
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

interface GetResult {
  /**
   * The path as given, suffix included.
   * @zh 原样的路径，含后缀。
   */
  path: string;
  /**
   * Rating from 0 to 5; `0` means unrated.
   * @zh 0 到 5 的评分；`0` 表示未评分。
   */
  rating: Int;
  /**
   * `stats` for a foo_playcount rating; `file` for the `RATING` tag, and also when neither
   * source has a rating.
   * @zh foo_playcount 的评分为 `stats`；`RATING` 标签为 `file`，两处都没有评分时也是 `file`。
   */
  storage: 'stats' | 'file';
}

interface SetParams {
  /**
   * Track path; a `|subsong:N` suffix or a trailing `#N` selects a subsong. Omit it for the
   * playing track, or else the active playlist's selection.
   * @zh 曲目路径；`|subsong:N` 后缀或结尾的 `#N` 选子曲目。省略则取正在播放的曲目，否则取活动播放列表的选中项。
   * @minLength 1
   * @security MediaWrite
   */
  path?: string;
  /**
   * Rating from 0 to 5; `0` clears it.
   * @zh 0 到 5 的评分；`0` 清除评分。
   * @minimum 0
   * @maximum 5
   */
  rating: Int;
  /**
   * Subsong index that wins over a suffix in `path`; negative values are ignored.
   * @zh 子曲目序号，优先于 `path` 里的后缀；负数忽略。
   * @default -1
   */
  cueIndex?: Int;
}

interface SetResult {
  /**
   * The path as given; `(current)` when `path` was omitted and foo_playcount took the rating.
   * @zh 原样的路径；省略 `path` 且由 foo_playcount 记录时为 `(current)`。
   */
  path: string;
  /**
   * The rating that was set.
   * @zh 设置的评分。
   */
  rating: Int;
  /**
   * `stats` when foo_playcount's menu ran; `file` when the `RATING` tag write was dispatched.
   * @zh foo_playcount 的菜单执行了为 `stats`；派发了 `RATING` 标签写入为 `file`。
   */
  storage: 'stats' | 'file';
  /**
   * The context-menu path that ran; present when `storage` is `stats`.
   * @zh 执行的右键菜单路径；`storage` 为 `stats` 时出现。
   */
  menuPath?: string;
  /**
   * Why the tag was written instead; present when `storage` is `file`.
   * @zh 改写标签的原因；`storage` 为 `file` 时出现。
   */
  note?: string;
}
