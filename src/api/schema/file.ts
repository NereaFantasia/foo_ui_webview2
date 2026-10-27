import type { Int } from './common.js';

export interface Api {
  /**
   * Read a file as text, or as Base64 with `encoding: "binary"`. A path that does not exist
   * fails with `NOT_FOUND`, and one that is not a regular file with `INVALID_PATH`.
   * @zh 读取文件为文本；`encoding: "binary"` 时读取原始字节并编码为 Base64；路径不存在时以 `NOT_FOUND` 失败，不是普通文件时以 `INVALID_PATH` 失败
   */
  read(params: ReadParams): ReadResult;

  /**
   * Write text, or bytes decoded from Base64, to a file. Missing parent directories are created
   * first.
   * @zh 将文本或 Base64 解码后的字节写入文件；父目录不存在时会自动创建
   */
  write(params: WriteParams): WriteResult;

  /**
   * Check whether a path exists and whether it is a file or a directory.
   * @zh 检查路径是否存在，并区分它是文件还是目录
   */
  exists(params: ExistsParams): ExistsResult;

  /**
   * List the files and subdirectories of a directory. A path that does not exist fails with
   * `NOT_FOUND`, and one that is not a directory with `INVALID_PATH`.
   * @zh 列出目录下的文件与子目录；路径不存在时以 `NOT_FOUND` 失败，不是目录时以 `INVALID_PATH` 失败
   */
  list(params: ListParams): ListResult;

  /**
   * Delete a file or directory, to the Recycle Bin unless `moveToTrash` is `false`. A path that
   * does not exist fails with `NOT_FOUND`.
   * @zh 删除文件或目录，默认移入回收站，`moveToTrash` 为 `false` 时永久删除；路径不存在时以 `NOT_FOUND` 失败
   */
  delete(params: DeleteParams): void;

  /**
   * Create a directory together with any missing parents. A directory that already exists is a
   * success with `created: false`; a file at the path fails with `INVALID_PATH`.
   * @zh 创建目录并自动补齐缺失的父目录；目录已存在时成功并返回 `created: false`，目标路径为已有文件时以 `INVALID_PATH` 失败
   */
  mkdir(params: MkdirParams): MkdirResult;

  /**
   * Copy a file or a whole directory tree, blocking until the copy is done. A source that does
   * not exist fails with `NOT_FOUND`. Prefer `file.copyAsync` for anything large.
   * @zh 复制文件或整棵目录树，复制完成前一直阻塞；源路径不存在时以 `NOT_FOUND` 失败；体量较大时建议改用 `file.copyAsync`
   */
  copy(params: CopyParams): CopyResult;

  /**
   * Move a file or directory, blocking until the move is done. A file can move to another
   * volume; a directory cannot, and that fails with `NOT_SUPPORTED` and
   * `details.reason: "cross-volume"`. A source that does not exist fails with `NOT_FOUND`.
   * @zh 移动文件或目录，移动完成前一直阻塞；文件支持跨卷移动，目录跨卷移动则会以 `NOT_SUPPORTED` 失败且附带 `details.reason: "cross-volume"`；源路径不存在时以 `NOT_FOUND` 失败
   */
  move(params: MoveParams): MoveResult;

  /**
   * Rename a file or directory within its parent directory. A path that does not exist fails
   * with `NOT_FOUND`, and a name that is already taken with `OPERATION_FAILED`.
   * @zh 在所在目录内重命名文件或目录；原路径不存在时以 `NOT_FOUND` 失败，新名称已被占用时以 `OPERATION_FAILED` 失败
   */
  rename(params: RenameParams): RenameResult;

  /**
   * Describe a file or directory. A path that does not exist is a success with `exists: false`
   * and no other field.
   * @zh 获取文件或目录的元信息；路径不存在时仍然成功返回，结果中 `exists: false` 且不包含其他字段
   */
  getInfo(params: GetInfoParams): GetInfoResult;

  /**
   * Start a cancellable batch copy on a worker thread and return a receipt at once. The results
   * arrive in batches on `file:opProgress`, followed by one `file:opComplete`. At most 8 batch
   * operations run at a time across the process; beyond that the call fails with
   * `OPERATION_FAILED`.
   * @zh 在 worker 线程启动可取消的批量复制并立即返回任务回执；进度结果由 `file:opProgress` 分批推送，最终发送一条 `file:opComplete`；全进程最多同时进行 8 个批量操作，超出时以 `OPERATION_FAILED` 失败
   */
  copyAsync(params: CopyAsyncParams): CopyAsyncResult;

  /**
   * Start a cancellable batch move on a worker thread, with the same receipt and events as
   * `file.copyAsync`. Within one volume a move is a rename; across volumes an entry is copied
   * and its source then deleted.
   * @zh 在 worker 线程启动可取消的批量移动，回执与事件同 `file.copyAsync`；同卷移动直接重命名，跨卷移动则先复制该条目再删除源
   */
  moveAsync(params: MoveAsyncParams): MoveAsyncResult;

  /**
   * Start a cancellable batch delete, with the same receipt and events as `file.copyAsync`; its
   * results carry no `destination`. Recycle Bin deletes run on the main thread in batches of
   * 16, permanent deletes on a worker thread.
   * @zh 启动可取消的批量删除，回执与事件同 `file.copyAsync`（结果中不带 `destination`）；回收站删除在主线程上每 16 条一批执行，永久删除走 worker 线程
   */
  deleteAsync(params: DeleteAsyncParams): DeleteAsyncResult;

  /**
   * Stop an operation started by `file.copyAsync`, `file.moveAsync` or `file.deleteAsync`. The
   * entries it has not reached are reported as `skipped` / `cancelled`, and the run still ends
   * with `file:opComplete`.
   * @zh 停止由 `file.copyAsync`、`file.moveAsync` 或 `file.deleteAsync` 启动的操作；尚未处理的条目记为 `skipped` 且原因为 `cancelled`，收尾仍会发出 `file:opComplete`
   */
  cancelOp(params: CancelOpParams): CancelOpResult;
}

export interface Events {
  /**
   * Results of a `file.copyAsync`, `file.moveAsync` or `file.deleteAsync` run, in batches: a batch
   * goes out once 64 results are pending or 100 ms have passed since the previous one, and the
   * last partial batch arrives before `file:opComplete`. Sent to the page that started the run;
   * when that window is gone by then, to the main window's page.
   * @zh 分批推送 `file.copyAsync`、`file.moveAsync` 或 `file.deleteAsync` 的批量操作结果；待推送结果达到 64 条或距上次推送已满 100 ms 时发送一批，最后一批残余结果在 `file:opComplete` 之前送达；事件发送给发起操作的页面，若该窗口已销毁则转投主窗口页面
   * @delivery caller
   */
  opProgress: OpProgressPayload;

  /**
   * The last event of a run, sent after a cancel and after failed entries too. It is skipped
   * only when foobar2000 is shutting down or the worker fails unexpectedly, so a listener that
   * cleans up on it should keep its own timeout. Afterwards the `operationId` is gone and
   * `file.cancelOp` reports `cancelled: false` for it. Delivered like `file:opProgress`.
   * @zh 批量操作的终末事件；操作取消后或存在失败条目时均会发出，仅在 foobar2000 正在退出或 worker 意外失败时不发，依赖此事件清理状态的监听方应自带超时；事件发出后 `operationId` 即失效，对其调用 `file.cancelOp` 将返回 `cancelled: false`；投递规则同 `file:opProgress`
   * @delivery caller
   */
  opComplete: OpCompletePayload;
}

interface OpProgressPayload {
  /**
   * Id from the receipt of the call that started the run.
   * @zh 发起操作时返回的任务回执 ID
   */
  operationId: string;
  /**
   * Which operation: `copy`, `move` or `delete`.
   * @zh 操作类型：`copy`、`move` 或 `delete`
   */
  op: 'copy' | 'move' | 'delete';
  /**
   * Entries reported so far across all batches, failures included.
   * @zh 截至当前批次已处理完成的累计条目数，含失败条目
   */
  done: Int;
  /**
   * Entries the call accepted.
   * @zh 本次批量调用受理的条目总数
   */
  total: Int;
  /**
   * The results of this batch only, in request order.
   * @zh 仅当前批次的条目结果，保持请求时的先后顺序
   */
  results: FileOpResultItem[];
}

/**
 * The result of one requested entry. A directory is one entry, reported once its whole tree has
 * been handled.
 * @zh 请求中单个条目的处理结果；一个目录计为一个条目，整棵树处理完毕后统一上报一次
 */
interface FileOpResultItem {
  /**
   * The source as requested, path variables unexpanded.
   * @zh 请求中传入的源路径，原样保留且不展开路径变量
   */
  source: string;
  /**
   * The destination as requested; absent for `file.deleteAsync`.
   * @zh 请求中传入的目标路径，原样保留；`file.deleteAsync` 时无此字段
   */
  destination?: string;
  /**
   * `ok` when carried out; `skipped` when deliberately not carried out, because the destination
   * existed or the run was cancelled first; `failed` otherwise.
   * @zh 条目处理状态：成功执行为 `ok`；因目标已存在或操作提前取消而跳过为 `skipped`；其余为 `failed`
   */
  status: 'ok' | 'skipped' | 'failed';
  /**
   * Why: `already-exists` or `cancelled` with `skipped`; `not-found`, `permission`,
   * `path-too-long` or `io-error` with `failed`; `cross-volume` with `ok`, for a move across
   * volumes done as a copy and a delete. `path-too-long` means a path the entry needed, inside a
   * copied folder included, is longer than 259 characters, or a folder it had to create is longer
   * than 247. Absent when the entry was carried out plainly.
   * @zh 状态原因说明；条目直接成功时无此字段；`skipped` 对应 `already-exists` 或 `cancelled`；`failed` 对应 `not-found`、`permission`、`path-too-long` 或 `io-error`；`ok` 对应 `cross-volume`（表示跨卷移动以复制加删除完成）；其中 `path-too-long` 表示涉及路径超过 259 个字符（含所复制文件夹内部路径）或新建文件夹超过 247 个字符
   */
  reason?:
    | 'already-exists'
    | 'not-found'
    | 'permission'
    | 'cross-volume'
    | 'path-too-long'
    | 'io-error'
    | 'cancelled';
}

interface OpCompletePayload {
  /**
   * Id from the receipt of the call that started the run.
   * @zh 发起操作时返回的任务回执 ID
   */
  operationId: string;
  /**
   * Which operation: `copy`, `move` or `delete`.
   * @zh 操作类型：`copy`、`move` 或 `delete`
   */
  op: 'copy' | 'move' | 'delete';
  /**
   * Entries the call accepted.
   * @zh 本次批量调用受理的条目总数
   */
  total: Int;
  /**
   * Entries reported `ok`.
   * @zh 状态为 `ok` 的成功条目数
   */
  successCount: Int;
  /**
   * Entries reported `skipped`.
   * @zh 状态为 `skipped` 的跳过条目数
   */
  skippedCount: Int;
  /**
   * Entries reported `failed`.
   * @zh 状态为 `failed` 的失败条目数
   */
  failureCount: Int;
  /**
   * `true` when at least one entry was reported with reason `cancelled`.
   * @zh 至少有一个条目的原因上报为 `cancelled` 时为 `true`
   */
  cancelled: boolean;
}

interface ReadParams {
  /**
   * File to read.
   * @zh 要读取的文件路径
   * @minLength 1
   * @security Read
   */
  path: string;
  /**
   * Only exactly `binary` reads the raw bytes and returns them as Base64; any other value reads
   * the file as text.
   * @zh 字符编码；仅当取值为 `"binary"` 时按原始字节读取并编码为 Base64；其他任何取值均按文本读取
   * @default "utf-8"
   */
  encoding?: string;
}

interface ReadResult {
  /**
   * The file's text; for a binary read, its bytes as plain Base64 without a `base64:` prefix.
   * @zh 文件内容；文本读取时为文本字符串，二进制读取时为不带 `base64:` 前缀的裸 Base64 字符串
   */
  content: string;
  /**
   * Size of the file on disk in bytes; for a text read it can differ from the length of
   * `content`.
   * @zh 文件在磁盘上的字节大小；文本读取时可能与 `content` 字符串长度不同
   */
  size: Int;
  /**
   * Present only for a binary read, and then always `base64`.
   * @zh 内容编码标识；仅在二进制读取时出现，固定为 `"base64"`
   */
  encoding?: string;
}

interface WriteParams {
  /**
   * File to write; missing parent directories are created.
   * @zh 要写入的文件路径；父目录不存在时会自动创建
   * @minLength 1
   * @security FileWrite
   */
  path: string;
  /**
   * What to write. With `encoding: "binary"` a value starting with `base64:` is decoded and the
   * bytes are written: characters outside the Base64 alphabet are skipped and decoding stops at
   * the first `=`. Any other value, plain Base64 or a Data URL included, is written as it is.
   * An empty string empties the file unless `append` is set.
   * @zh 待写入的内容；未设置 `append` 时空字符串会清空文件；当 `encoding: "binary"` 且内容以 `base64:` 开头时，宿主将解码为字节后写入（跳过 Base64 字母表以外字符，遇首个 `=` 停止解码）；其他任何取值（含裸 Base64 与 Data URL）均按原样文本写入
   * @default ""
   */
  content?: string;
  /**
   * Only exactly `binary` changes anything: the file is written in binary mode and a `content`
   * starting with `base64:` is decoded. Any other value writes text.
   * @zh 字符编码；仅当取值为 `"binary"` 时进入二进制写入模式并解码以 `base64:` 开头的 `content`；其他任何取值均按文本写入
   * @default "utf-8"
   */
  encoding?: string;
  /**
   * Add to the end of the file instead of replacing its content.
   * @zh 是否追加到文件末尾，为 `false` 时替换原有内容
   * @default false
   */
  append?: boolean;
  /**
   * Write a temporary file next to the target, flush it to disk, then replace the target with
   * one rename: readers see the old content or the new content, never a partly written file.
   * If the replacement fails, for example because another program has the target open, the call
   * fails and the target keeps its old content. The temporary file is named
   * `.~<process id>-<sequence>.tmp`, and its path has to fit in 259 characters as well. Cannot be
   * combined with `append`; passing both fails with `INVALID_PARAMS`.
   * @zh 是否启用原子写入；开启时先在目标同目录下写入临时文件并落盘，再通过原子重命名替换目标，确保读取方只能读到完整的新内容或旧内容；若替换失败（如目标文件被其他程序占用），调用将失败且目标保留原内容；临时文件命名为 `.~<进程号>-<序号>.tmp`，路径长度同样受 259 字符限制；不可与 `append` 同时使用，两者同时传入时以 `INVALID_PARAMS` 失败
   * @default false
   */
  atomic?: boolean;
}

interface WriteResult {
  /**
   * Size of the file after the write, in bytes; with `append` it is the whole file, not just the
   * part added.
   * @zh 写入后文件的字节总大小；`append` 为 `true` 时是追加后的完整文件大小，而非仅新增字节数
   */
  bytesWritten: Int;
}

interface ExistsParams {
  /**
   * Path to check.
   * @zh 要检查的路径
   * @minLength 1
   * @security Read
   */
  path: string;
}

interface ExistsResult {
  /**
   * Whether the path exists.
   * @zh 路径是否存在
   */
  exists: boolean;
  /**
   * Whether the path is a regular file.
   * @zh 路径是否是普通文件
   */
  isFile: boolean;
  /**
   * Whether the path is a directory.
   * @zh 路径是否是目录
   */
  isDirectory: boolean;
}

interface ListParams {
  /**
   * Directory to list.
   * @zh 要列出的目录路径
   * @minLength 1
   * @security Read
   */
  path: string;
  /**
   * Filter for files; directories are always listed. Only `*`, `*.*` and a single extension such
   * as `*.flac` are understood, and nothing is refused: a pattern that does not start with `*.`
   * (`song*`, `?.txt`) matches every file, while a `*.ext` pattern is compared, ignoring case,
   * with the text from the file name's last dot on, so `*.{flac,mp3}` matches nothing and no
   * `*.ext` matches a file without an extension.
   * @zh 文件匹配规则，仅过滤文件，目录始终列出；仅支持 `*`、`*.*` 和单个扩展名（如 `*.flac`）；不以 `*.` 开头的模式（如 `song*`、`?.txt`）匹配全部文件；`*.ext` 模式按不区分大小写比对文件名末尾点号起的文本，因此 `*.{flac,mp3}` 无法匹配任何文件，无扩展名文件也无法匹配任何 `*.ext`
   * @default "*"
   */
  pattern?: string;
  /**
   * Walk the subdirectories as well; the entries are then full paths instead of names. A
   * subdirectory that cannot be read makes the call fail with `OPERATION_FAILED`.
   * @zh 是否遍历子目录；开启时条目为完整路径，关闭时仅为名称；遇到无法读取的子目录时调用以 `OPERATION_FAILED` 失败
   * @default false
   */
  recursive?: boolean;
}

interface ListResult {
  /**
   * Files that match `pattern`: names, or full paths when `recursive` is set.
   * @zh 匹配 `pattern` 的文件列表：未开启 `recursive` 时为文件名，开启时为完整路径
   */
  files: string[];
  /**
   * Subdirectories, whatever `pattern` says: names, or full paths when `recursive` is set.
   * @zh 子目录列表，不受 `pattern` 影响：未开启 `recursive` 时为子目录名，开启时为完整路径
   */
  directories: string[];
  /**
   * The same list as `files`; kept for callers that read this name.
   * @zh 与 `files` 相同的列表，为读取此字段的旧调用方保留
   */
  items: string[];
}

interface DeleteParams {
  /**
   * File or directory to delete.
   * @zh 要删除的文件或目录路径
   * @minLength 1
   * @security FileWrite
   */
  path: string;
  /**
   * Send it to the Recycle Bin. `false` deletes it permanently, which fails on a directory that
   * is not empty.
   * @zh 是否移入回收站；为 `false` 时永久删除，且无法删除非空目录
   * @default true
   */
  moveToTrash?: boolean;
}

interface MkdirParams {
  /**
   * Directory to create.
   * @zh 要创建的目录路径
   * @minLength 1
   * @security FileWrite
   */
  path: string;
}

interface MkdirResult {
  /**
   * Whether this call created the directory; `false` when it already existed.
   * @zh 本次调用是否创建了新目录；目录已存在时为 `false`
   */
  created: boolean;
  /**
   * `Directory already exists` when the directory was already there; absent otherwise.
   * @zh 提示信息；目录已存在时为 `"Directory already exists"`，否则不出现
   */
  message?: string;
}

interface CopyParams {
  /**
   * File or directory to copy; a directory is copied with everything below it.
   * @zh 要复制的源文件或目录；若为目录则连同其下全部内容递归复制
   * @minLength 1
   * @security Read
   */
  source: string;
  /**
   * Target path. Its parent directory has to exist already.
   * @zh 目标路径；其父目录必须已经存在
   * @minLength 1
   * @security FileWrite
   */
  destination: string;
  /**
   * Replace files that already exist at the target. Otherwise they are left as they are and the
   * call still succeeds.
   * @zh 是否替换目标处已存在的文件；为 `false` 时保留已有文件不动且调用仍然成功
   * @default false
   */
  overwrite?: boolean;
}

interface CopyResult {
  /**
   * The source as given.
   * @zh 请求中传入的源路径
   */
  source: string;
  /**
   * The destination as given.
   * @zh 请求中传入的目标路径
   */
  destination: string;
}

interface MoveParams {
  /**
   * File or directory to move.
   * @zh 要移动的文件或目录路径
   * @minLength 1
   * @security FileWrite
   */
  source: string;
  /**
   * Target path. An existing file there is replaced; moving a file onto an existing directory,
   * or under a parent directory that does not exist, fails.
   * @zh 目标路径；该处已有的同名文件会被替换；若将文件移到已存在的目录上，或父目录不存在，调用均会失败
   * @minLength 1
   * @security FileWrite
   */
  destination: string;
}

interface MoveResult {
  /**
   * The source as given.
   * @zh 请求中传入的源路径
   */
  source: string;
  /**
   * The destination as given.
   * @zh 请求中传入的目标路径
   */
  destination: string;
}

interface RenameParams {
  /**
   * File or directory to rename.
   * @zh 要重命名的文件或目录路径
   * @minLength 1
   * @security FileWrite
   */
  path: string;
  /**
   * New name, kept in the same directory. A name containing `/` or `\` fails with
   * `INVALID_PARAMS`.
   * @zh 新名称，仍位于同一目录；包含 `/` 或 `\` 时以 `INVALID_PARAMS` 失败
   * @minLength 1
   */
  newName: string;
}

interface RenameResult {
  /**
   * The path as given.
   * @zh 请求中传入的原路径
   */
  oldPath: string;
  /**
   * The parent directory of the expanded `path`, joined with `newName`.
   * @zh 展开路径变量后的父目录与 `newName` 拼接所得的新路径
   */
  newPath: string;
}

interface GetInfoParams {
  /**
   * File or directory to describe.
   * @zh 要查询信息的文件或目录路径
   * @minLength 1
   * @security Read
   */
  path: string;
}

interface GetInfoResult {
  /**
   * Whether the path exists; when `false`, no other field is present.
   * @zh 路径是否存在；为 `false` 时不包含其他字段
   */
  exists: boolean;
  /**
   * Whether the path is a directory.
   * @zh 路径是否是目录
   */
  isDirectory?: boolean;
  /**
   * Whether the path is a regular file.
   * @zh 路径是否是普通文件
   */
  isFile?: boolean;
  /**
   * Size in bytes; `0` for a directory.
   * @zh 文件字节大小；目录为 `0`
   */
  size?: Int;
  /**
   * Last write time as a JavaScript timestamp in milliseconds, truncated to whole seconds.
   * @zh 最后修改时间，截断到整秒的 JavaScript 毫秒时间戳
   */
  modified?: Int;
  /**
   * File name with its extension.
   * @zh 文件或目录名称，包含扩展名
   */
  name?: string;
  /**
   * Extension with its leading dot, such as `.flac`; empty when there is none.
   * @zh 带前导点的扩展名（如 `".flac"`）；无扩展名时为空字符串
   */
  extension?: string;
  /**
   * Parent directory of the path after `%variable%` expansion. The path is not normalized, so a
   * doubled separator left by the expansion stays.
   * @zh 展开 `%变量%` 后的父目录路径；由于不做路径归一化，展开留下的多余斜杠会原样保留
   */
  parent?: string;
}

/**
 * One entry of a `file.copyAsync` or `file.moveAsync` batch.
 * @zh `file.copyAsync` 或 `file.moveAsync` 批次中的单个条目
 */
interface FileOpEntry {
  /**
   * File or directory to copy or move. Echoed back verbatim, `%variable%` placeholders
   * unexpanded, in the `file:opProgress` results.
   * @zh 要复制或移动的文件或目录路径；在 `file:opProgress` 结果中原样回显且不展开 `%变量%`
   * @minLength 1
   */
  source: string;
  /**
   * Target path; a missing parent directory is created. When it names an existing directory and
   * `source` is a file, the file is placed inside it under its own name. Echoed back verbatim
   * like `source`.
   * @zh 目标路径；缺失的父目录会被自动创建；若目标指向已存在的目录且 `source` 为文件，则以原文件名存入该目录；结果中同样原样回显
   * @minLength 1
   */
  destination: string;
}

// `source` is written first in @pathKey: the bridge checks the members in that order, and a
// refusal names the first member that fails.
interface CopyAsyncParams {
  /**
   * Entries to copy, reported one result each, in this order. An entry that is not an object
   * holding exactly the string members `source` and `destination` fails the call with
   * `INVALID_PARAMS`. Every path is checked before anything starts, `source` for reading and
   * `destination` for writing; one refused path, an empty string included, fails the whole call
   * with `PERMISSION_DENIED` and no `operationId`.
   * @zh 要复制的条目列表，按此顺序每条上报一个结果；若条目不是仅包含字符串成员 `source` 与 `destination` 的对象，调用以 `INVALID_PARAMS` 失败；开始前逐条校验路径权限（`source` 按读取、`destination` 按写入），任一路径被拒绝（含空字符串）都会导致整次调用以 `PERMISSION_DENIED` 失败且不产生 `operationId`
   * @minItems 1
   * @pathKey source=Read destination=FileWrite
   */
  items: FileOpEntry[];
  /**
   * Replace an existing file destination instead of reporting the entry as `skipped` /
   * `already-exists`. A directory copied onto an existing directory is merged into it either
   * way; the files already inside are skipped, without being reported, unless this is set.
   * @zh 是否替换已存在的目标文件；为 `false` 时将该条记录为 `skipped` 且原因为 `already-exists`；目录复制到已存在的目录时总会并入其中，目录内已存在的同名文件仅在此项开启时才被替换，否则静默跳过且不单独上报
   * @default false
   */
  overwrite?: boolean;
}

interface CopyAsyncResult {
  /**
   * Id of the operation, starting with `fileop_`. The events of the operation carry it, and
   * `file.cancelOp` takes it.
   * @zh 批量操作任务 ID，以 `fileop_` 开头；该操作的事件带有此 ID，`file.cancelOp` 也用它
   */
  operationId: string;
  /**
   * Number of entries accepted; the events report it as `total`.
   * @zh 本次操作受理的条目总数，对应事件里的 `total`
   */
  totalCount: Int;
}

// Both members are checked for writing: a move deletes its source.
interface MoveAsyncParams {
  /**
   * Entries to move, reported one result each, in this order. An entry that is not an object
   * holding exactly the string members `source` and `destination` fails the call with
   * `INVALID_PARAMS`. Every path is checked for writing before anything starts, `source`
   * included, since a move deletes it; one refused path, an empty string included, fails the
   * whole call with `PERMISSION_DENIED` and no `operationId`.
   * @zh 要移动的条目列表，按此顺序每条上报一个结果；若条目不是仅包含字符串成员 `source` 与 `destination` 的对象，调用以 `INVALID_PARAMS` 失败；开始前逐条按写入权限校验全部路径（包括 `source`，因为移动会删除源文件），任一路径被拒绝（含空字符串）都会导致整次调用以 `PERMISSION_DENIED` 失败且不产生 `operationId`
   * @minItems 1
   * @pathKey source=FileWrite destination=FileWrite
   */
  items: FileOpEntry[];
  /**
   * Replace an existing file destination instead of reporting the entry as `skipped` /
   * `already-exists`; the synchronous `file.move` always replaces one. An existing directory
   * destination is never replaced: Windows cannot swap a directory in place, so on the same
   * volume such an entry ends as `skipped` or `failed`.
   * @zh 是否替换已存在的目标文件；为 `false` 时将该条记录为 `skipped` 且原因为 `already-exists`（同步 `file.move` 总是替换）；已存在的目标目录永远不会被替换（Windows 不支持原地替换目录），同卷移动遇到已有同名目录时条目将以 `skipped` 或 `failed` 结束
   * @default false
   */
  overwrite?: boolean;
}

interface MoveAsyncResult {
  /**
   * Id of the operation, starting with `fileop_`. The events of the operation carry it, and
   * `file.cancelOp` takes it.
   * @zh 批量操作任务 ID，以 `fileop_` 开头；该操作的事件带有此 ID，`file.cancelOp` 也用它
   */
  operationId: string;
  /**
   * Number of entries accepted; the events report it as `total`.
   * @zh 本次操作受理的条目总数，对应事件里的 `total`
   */
  totalCount: Int;
}

interface DeleteAsyncParams {
  /**
   * Paths to delete, reported one result each, in this order. An entry that is not a string
   * fails the call with `INVALID_PARAMS`. Every path is checked for writing before anything
   * starts; one refused path, an empty string included, fails the whole call with
   * `PERMISSION_DENIED` and no `operationId`.
   * @zh 要删除的路径列表，按此顺序每条上报一个结果；数组中存在非字符串元素时调用以 `INVALID_PARAMS` 失败；开始前逐条按写入权限校验全部路径，任一路径被拒绝（含空字符串）都会导致整次调用以 `PERMISSION_DENIED` 失败且不产生 `operationId`
   * @minItems 1
   * @security FileWrite
   */
  paths: string[];
  /**
   * Send each entry to the Recycle Bin. `false` deletes permanently and removes directories
   * that are not empty, which the synchronous `file.delete` refuses to do.
   * @zh 是否移入回收站；为 `false` 时执行永久删除，且支持递归删除非空目录（同步 `file.delete` 拒绝删除非空目录）
   * @default true
   */
  moveToTrash?: boolean;
}

interface DeleteAsyncResult {
  /**
   * Id of the operation, starting with `fileop_`. The events of the operation carry it, and
   * `file.cancelOp` takes it.
   * @zh 批量操作任务 ID，以 `fileop_` 开头；该操作的事件带有此 ID，`file.cancelOp` 也用它
   */
  operationId: string;
  /**
   * Number of entries accepted; the events report it as `total`.
   * @zh 本次操作受理的条目总数，对应事件里的 `total`
   */
  totalCount: Int;
}

interface CancelOpParams {
  /**
   * Id from the receipt of `file.copyAsync`, `file.moveAsync` or `file.deleteAsync`.
   * @zh `file.copyAsync`、`file.moveAsync` 或 `file.deleteAsync` 回执里的任务 ID
   * @minLength 1
   */
  operationId: string;
}

interface CancelOpResult {
  /**
   * `true` when the operation was still running and has been told to stop; `false` when it had
   * already finished or never existed, which are deliberately indistinguishable.
   * @zh 操作是否成功收到停止通知；若操作仍在进行并已通知停止则为 `true`；若操作已结束或从未存在则为 `false`（两者故意不可区分）
   */
  cancelled: boolean;
}
