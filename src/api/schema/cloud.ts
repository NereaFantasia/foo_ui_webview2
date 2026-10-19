import type { Int } from './common.js';

export interface Api {
  /**
   * Report the local/cloud state of media paths from file attributes alone. No file is
   * opened and no content is read, so an online-only OneDrive placeholder is reported with
   * `placeholder: true` instead of being hydrated; this is a metadata read, not a content read.
   * Give exactly one of `paths` or `directory`: `paths` reports one entry per path in the
   * given order, while `directory` reports the immediate children of one directory and does
   * not recurse. A path that does not exist is reported with `exists: false` rather than
   * failing the call; in `directory` mode a missing or non-directory path fails the call.
   * @zh 仅凭文件属性报告媒体路径的本地/云端状态。不打开文件、不读内容，因此仅在线可用的
   * OneDrive 占位符只会被标为 `placeholder: true` 而不会被水合；这是一次元数据读取，不是内容读。
   * `paths` 与 `directory` 必须恰好给一个：`paths` 按给定顺序每个路径一行，`directory` 只报该
   * 目录的直接子项、不递归。不存在的路径记 `exists: false`，不让整次调用失败；`directory` 模式
   * 下目录不存在或不是目录时整次调用失败。
   * @effect read
   */
  getStates(params: GetStatesParams): GetStatesResult;
}

interface GetStatesParams {
  /**
   * Media paths to inspect, at most 256. A path that does not exist is a success with
   * `exists: false`; `file://`, `file-relative://` and a `|subsong:N` suffix are accepted and
   * resolve to the underlying file.
   * @zh 要检查的媒体路径，最多 256 个。路径不存在时成功返回 `exists: false`；接受 `file://`、
   * `file-relative://` 与 `|subsong:N` 后缀，并解析到其底层文件。
   * @minItems 1
   * @security MediaRead
   */
  paths?: string[];

  /**
   * Directory whose immediate children are inspected, in native path form. A directory that
   * does not exist fails with `NOT_FOUND`; a path that is not a directory fails with
   * `INVALID_PATH`.
   * @zh 要检查其直接子项的目录（原生路径形式）。目录不存在时以 `NOT_FOUND` 失败；路径不是
   * 目录时以 `INVALID_PATH` 失败。
   * @minLength 1
   * @security MediaRead
   */
  directory?: string;
}

interface CloudPathState {
  /**
   * The path as given for `paths`, or the native child path for `directory`.
   * @zh `paths` 模式下原样给出的路径；`directory` 模式下子项的原生路径。
   */
  path: string;

  /**
   * Whether the path exists on disk. A cloud placeholder does exist.
   * @zh 路径在盘上是否存在。云占位符算存在。
   */
  exists: boolean;

  /**
   * Whether the path exists and its content is local, i.e. reading it would not need the
   * cloud to supply data.
   * @zh 路径存在且内容在本地，即读取它不需要云端再提供数据。
   */
  local: boolean;

  /**
   * Whether the path exists and is a cloud placeholder that would be hydrated by a read
   * (`FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS` set). Directory entries are the same shape:
   * the directory itself being a placeholder is reported here too.
   * @zh 路径存在且是读取时会水合的云占位符（`FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS` 置位）。
   * 目录项同样按此形状报告：目录本身是占位符也记在这里。
   */
  placeholder: boolean;

  /**
   * Raw Win32 file attribute bits (`GetFileAttributesW`), present when `exists` is `true`.
   * `FILE_ATTRIBUTE_DIRECTORY` (`0x10`) tells a directory entry from a file entry.
   * @zh 原始 Win32 文件属性位（`GetFileAttributesW`），`exists` 为 `true` 时出现。
   * `FILE_ATTRIBUTE_DIRECTORY`（`0x10`）用于区分目录项与文件项。
   */
  attrs?: Int;
}

interface GetStatesResult {
  /**
   * One entry per requested path, or one entry per immediate child of `directory`; directory
   * entries are sorted by `path`.
   * @zh 每个请求路径一行，或 `directory` 的每个直接子项一行；目录项按 `path` 排序。
   */
  states: CloudPathState[];
}
