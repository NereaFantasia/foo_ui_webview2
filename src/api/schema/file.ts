import type { Int } from './common.js';

export interface Api {
  /**
   * Read a file as text, or as Base64 with `encoding: "binary"`. A path that does not exist
   * fails with `NOT_FOUND`, and one that is not a regular file with `INVALID_PATH`.
   * @zh 以文本读取文件，`encoding: "binary"` 时以 Base64 读取。路径不存在时以 `NOT_FOUND` 失败，不是普通文件时以 `INVALID_PATH` 失败。
   */
  read(params: ReadParams): ReadResult;

  /**
   * Write text, or bytes decoded from Base64, to a file. Missing parent directories are created
   * first.
   * @zh 把文本或由 Base64 解码出的字节写入文件。父目录不存在时先创建。
   */
  write(params: WriteParams): WriteResult;

  /**
   * Check whether a path exists and whether it is a file or a directory.
   * @zh 检查路径是否存在，以及它是文件还是目录。
   */
  exists(params: ExistsParams): ExistsResult;

  /**
   * List the files and subdirectories of a directory. A path that does not exist fails with
   * `NOT_FOUND`, and one that is not a directory with `INVALID_PATH`.
   * @zh 列出目录下的文件与子目录。路径不存在时以 `NOT_FOUND` 失败，不是目录时以 `INVALID_PATH` 失败。
   */
  list(params: ListParams): ListResult;

  /**
   * Delete a file or directory, to the Recycle Bin unless `moveToTrash` is `false`. A path that
   * does not exist fails with `NOT_FOUND`.
   * @zh 删除文件或目录；除非 `moveToTrash` 为 `false`，否则移入回收站。路径不存在时以 `NOT_FOUND` 失败。
   */
  delete(params: DeleteParams): void;

  /**
   * Create a directory together with any missing parents. A directory that already exists is a
   * success with `created: false`; a file at the path fails with `INVALID_PATH`.
   * @zh 创建目录，缺失的上级目录一并创建。目录已存在时成功并返回 `created: false`；该路径是文件时以 `INVALID_PATH` 失败。
   */
  mkdir(params: MkdirParams): MkdirResult;

  /**
   * Copy a file or a whole directory tree, blocking until the copy is done. A source that does
   * not exist fails with `NOT_FOUND`. Prefer `file.copyAsync` for anything large.
   * @zh 复制文件或整棵目录树，复制完成前一直阻塞。源不存在时以 `NOT_FOUND` 失败。体量大时改用 `file.copyAsync`。
   */
  copy(params: CopyParams): CopyResult;

  /**
   * Move a file or directory, blocking until the move is done. A file can move to another
   * volume; a directory cannot, and that fails with `NOT_SUPPORTED` and
   * `details.reason: "cross-volume"`. A source that does not exist fails with `NOT_FOUND`.
   * @zh 移动文件或目录，移动完成前一直阻塞。文件可以跨卷移动；目录不能，跨卷时以 `NOT_SUPPORTED` 失败并带 `details.reason: "cross-volume"`。源不存在时以 `NOT_FOUND` 失败。
   */
  move(params: MoveParams): MoveResult;

  /**
   * Rename a file or directory within its parent directory. A path that does not exist fails
   * with `NOT_FOUND`, and a name that is already taken with `OPERATION_FAILED`.
   * @zh 在所在目录内重命名文件或目录。路径不存在时以 `NOT_FOUND` 失败，新名字已被占用时以 `OPERATION_FAILED` 失败。
   */
  rename(params: RenameParams): RenameResult;

  /**
   * Describe a file or directory. A path that does not exist is a success with `exists: false`
   * and no other field.
   * @zh 描述文件或目录。路径不存在时成功返回 `exists: false`，不带其他字段。
   */
  getInfo(params: GetInfoParams): GetInfoResult;

  /**
   * Start a cancellable batch copy on a worker thread and return a receipt at once. The results
   * arrive in batches on `file:opProgress`, followed by one `file:opComplete`. At most 8 batch
   * operations run at a time across the process; beyond that the call fails with
   * `OPERATION_FAILED`.
   * @zh 在 worker 线程上启动可取消的批量复制，立即返回回执。结果经 `file:opProgress` 分批送达，最后是一条 `file:opComplete`。全进程同时最多进行 8 个批量操作，超出时以 `OPERATION_FAILED` 失败。
   */
  copyAsync(params: CopyAsyncParams): CopyAsyncResult;

  /**
   * Start a cancellable batch move on a worker thread, with the same receipt and events as
   * `file.copyAsync`. Within one volume a move is a rename; across volumes an entry is copied
   * and its source then deleted.
   * @zh 在 worker 线程上启动可取消的批量移动，回执与事件同 `file.copyAsync`。同卷移动是一次改名；跨卷时先复制该条目再删除源。
   */
  moveAsync(params: MoveAsyncParams): MoveAsyncResult;

  /**
   * Start a cancellable batch delete, with the same receipt and events as `file.copyAsync`; its
   * results carry no `destination`. Recycle Bin deletes run on the main thread in batches of
   * 16, permanent deletes on a worker thread.
   * @zh 启动可取消的批量删除，回执与事件同 `file.copyAsync`，结果不带 `destination`。回收站删除在主线程上每 16 条一批执行，永久删除走 worker 线程。
   */
  deleteAsync(params: DeleteAsyncParams): DeleteAsyncResult;

  /**
   * Stop an operation started by `file.copyAsync`, `file.moveAsync` or `file.deleteAsync`. The
   * entries it has not reached are reported as `skipped` / `cancelled`, and the run still ends
   * with `file:opComplete`.
   * @zh 停止由 `file.copyAsync`、`file.moveAsync` 或 `file.deleteAsync` 启动的操作。尚未处理的条目记为 `skipped` / `cancelled`，收尾仍会发出 `file:opComplete`。
   */
  cancelOp(params: CancelOpParams): CancelOpResult;
}

export interface Events {
  /**
   * Results of a `file.copyAsync`, `file.moveAsync` or `file.deleteAsync` run, in batches: a batch
   * goes out once 64 results are pending or 100 ms have passed since the previous one, and the
   * last partial batch arrives before `file:opComplete`. Sent to the page that started the run;
   * when that window is gone by then, to the main window's page.
   * @zh `file.copyAsync`、`file.moveAsync` 或 `file.deleteAsync` 的结果，分批送达：积满 64 条或距上一批过了 100 ms 就发一批，最后不满一批的那些在 `file:opComplete` 之前送达。发给发起操作的页面；那时该窗口已不在的，发给主窗口的页面。
   * @delivery caller
   */
  opProgress: OpProgressPayload;

  /**
   * The last event of a run, sent after a cancel and after failed entries too. It is skipped
   * only when foobar2000 is shutting down or the worker fails unexpectedly, so a listener that
   * cleans up on it should keep its own timeout. Afterwards the `operationId` is gone and
   * `file.cancelOp` reports `cancelled: false` for it. Delivered like `file:opProgress`.
   * @zh 一次操作的最后一个事件，取消之后、有条目失败时也会发。只有 foobar2000 正在退出或 worker 意外失败时不发，所以靠它收尾的监听方应自带超时。之后 `operationId` 失效，对它调用 `file.cancelOp` 报 `cancelled: false`。投递范围同 `file:opProgress`。
   * @delivery caller
   */
  opComplete: OpCompletePayload;
}

interface OpProgressPayload {
  /**
   * Id from the receipt of the call that started the run.
   * @zh 发起操作的调用回执里的 id。
   */
  operationId: string;
  /**
   * Which operation: `copy`, `move` or `delete`.
   * @zh 哪种操作：`copy`、`move` 或 `delete`。
   */
  op: 'copy' | 'move' | 'delete';
  /**
   * Entries reported so far across all batches, failures included.
   * @zh 到目前为止各批累计上报的条目数，含失败的。
   */
  done: Int;
  /**
   * Entries the call accepted.
   * @zh 调用受理的条目数。
   */
  total: Int;
  /**
   * The results of this batch only, in request order.
   * @zh 仅本批的结果，顺序与请求一致。
   */
  results: FileOpResultItem[];
}

/**
 * The result of one requested entry. A directory is one entry, reported once its whole tree has
 * been handled.
 * @zh 请求中一个条目的结果。一个目录算一个条目，整棵树处理完后上报一次。
 */
interface FileOpResultItem {
  /**
   * The source as requested, path variables unexpanded.
   * @zh 请求里的源路径，原样，不展开路径变量。
   */
  source: string;
  /**
   * The destination as requested; absent for `file.deleteAsync`.
   * @zh 请求里的目标路径，原样；`file.deleteAsync` 没有。
   */
  destination?: string;
  /**
   * `ok` when carried out; `skipped` when deliberately not carried out, because the destination
   * existed or the run was cancelled first; `failed` otherwise.
   * @zh 完成为 `ok`；因目标已存在或操作先被取消而有意没做为 `skipped`；其余为 `failed`。
   */
  status: 'ok' | 'skipped' | 'failed';
  /**
   * Why: `already-exists` or `cancelled` with `skipped`; `not-found`, `permission`,
   * `path-too-long` or `io-error` with `failed`; `cross-volume` with `ok`, for a move across
   * volumes done as a copy and a delete. `path-too-long` means a path the entry needed, inside a
   * copied folder included, is longer than 259 characters, or a folder it had to create is longer
   * than 247. Absent when the entry was carried out plainly.
   * @zh 原因：`skipped` 配 `already-exists` 或 `cancelled`；`failed` 配 `not-found`、`permission`、`path-too-long` 或 `io-error`；`ok` 配 `cross-volume`，表示跨卷移动以复制加删除完成。`path-too-long` 表示这一条用到的某个路径（包括复制的文件夹里面的）超过 259 个字符，或要新建的文件夹超过 247 个字符。条目直接完成时不出现。
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
   * @zh 发起操作的调用回执里的 id。
   */
  operationId: string;
  /**
   * Which operation: `copy`, `move` or `delete`.
   * @zh 哪种操作：`copy`、`move` 或 `delete`。
   */
  op: 'copy' | 'move' | 'delete';
  /**
   * Entries the call accepted.
   * @zh 调用受理的条目数。
   */
  total: Int;
  /**
   * Entries reported `ok`.
   * @zh 上报为 `ok` 的条目数。
   */
  successCount: Int;
  /**
   * Entries reported `skipped`.
   * @zh 上报为 `skipped` 的条目数。
   */
  skippedCount: Int;
  /**
   * Entries reported `failed`.
   * @zh 上报为 `failed` 的条目数。
   */
  failureCount: Int;
  /**
   * `true` when at least one entry was reported with reason `cancelled`.
   * @zh 至少有一个条目以 `cancelled` 为原因上报时为 `true`。
   */
  cancelled: boolean;
}

interface ReadParams {
  /**
   * File to read.
   * @zh 要读取的文件。
   * @minLength 1
   * @security Read
   */
  path: string;
  /**
   * Only exactly `binary` reads the raw bytes and returns them as Base64; any other value reads
   * the file as text.
   * @zh 只有恰为 `binary` 才按字节读取并以 Base64 返回；其他任何值都按文本读取。
   * @default "utf-8"
   */
  encoding?: string;
}

interface ReadResult {
  /**
   * The file's text; for a binary read, its bytes as plain Base64 without a `base64:` prefix.
   * @zh 文件的文本；二进制读取时为其字节的裸 Base64，不带 `base64:` 前缀。
   */
  content: string;
  /**
   * Size of the file on disk in bytes; for a text read it can differ from the length of
   * `content`.
   * @zh 文件在磁盘上的字节数；文本读取时可能与 `content` 的长度不同。
   */
  size: Int;
  /**
   * Present only for a binary read, and then always `base64`.
   * @zh 只在二进制读取时出现，恒为 `base64`。
   */
  encoding?: string;
}

interface WriteParams {
  /**
   * File to write; missing parent directories are created.
   * @zh 要写入的文件；缺失的父目录会被创建。
   * @minLength 1
   * @security FileWrite
   */
  path: string;
  /**
   * What to write. With `encoding: "binary"` a value starting with `base64:` is decoded and the
   * bytes are written: characters outside the Base64 alphabet are skipped and decoding stops at
   * the first `=`. Any other value, plain Base64 or a Data URL included, is written as it is.
   * An empty string empties the file unless `append` is set.
   * @zh 要写入的内容。`encoding: "binary"` 时，以 `base64:` 开头的值被解码后按字节写入：Base64 字母表以外的字符被跳过，遇到第一个 `=` 即停止解码。其他任何值（包括裸 Base64 与 Data URL）都按原样写入。空串会清空文件，除非设置了 `append`。
   * @default ""
   */
  content?: string;
  /**
   * Only exactly `binary` changes anything: the file is written in binary mode and a `content`
   * starting with `base64:` is decoded. Any other value writes text.
   * @zh 只有恰为 `binary` 才有区别：以二进制模式写入，并解码以 `base64:` 开头的 `content`。其他任何值都按文本写入。
   * @default "utf-8"
   */
  encoding?: string;
  /**
   * Add to the end of the file instead of replacing its content.
   * @zh 追加到文件末尾，而不是替换原有内容。
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
   * @zh 先在目标旁边写一个临时文件并落盘，再用一次改名替换目标：读者只会看到旧内容或新内容，不会看到写了一半的文件。替换失败时（例如目标被其他程序打开着）调用失败，目标保留原内容。临时文件名为 `.~<进程号>-<序号>.tmp`，它的路径同样不能超过 259 个字符。不能与 `append` 同用，两者同时传时以 `INVALID_PARAMS` 失败。
   * @default false
   */
  atomic?: boolean;
}

interface WriteResult {
  /**
   * Size of the file after the write, in bytes; with `append` it is the whole file, not just the
   * part added.
   * @zh 写入后文件的字节数；`append` 时是整个文件的大小，而不只是新增的部分。
   */
  bytesWritten: Int;
}

interface ExistsParams {
  /**
   * Path to check.
   * @zh 要检查的路径。
   * @minLength 1
   * @security Read
   */
  path: string;
}

interface ExistsResult {
  /**
   * Whether the path exists.
   * @zh 路径是否存在。
   */
  exists: boolean;
  /**
   * Whether the path is a regular file.
   * @zh 路径是否是普通文件。
   */
  isFile: boolean;
  /**
   * Whether the path is a directory.
   * @zh 路径是否是目录。
   */
  isDirectory: boolean;
}

interface ListParams {
  /**
   * Directory to list.
   * @zh 要列出的目录。
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
   * @zh 只过滤文件，目录总会列出。只认 `*`、`*.*` 和单个扩展名（如 `*.flac`），任何写法都不会被拒绝：不以 `*.` 开头的写法（`song*`、`?.txt`）匹配全部文件；`*.ext` 写法按不区分大小写的方式与文件名最后一个点起的文本比较，所以 `*.{flac,mp3}` 一个都匹配不上，没有扩展名的文件也匹配不上任何 `*.ext`。
   * @default "*"
   */
  pattern?: string;
  /**
   * Walk the subdirectories as well; the entries are then full paths instead of names. A
   * subdirectory that cannot be read makes the call fail with `OPERATION_FAILED`.
   * @zh 同时遍历子目录；此时各项是完整路径而不是名称。遇到读不了的子目录时调用以 `OPERATION_FAILED` 失败。
   * @default false
   */
  recursive?: boolean;
}

interface ListResult {
  /**
   * Files that match `pattern`: names, or full paths when `recursive` is set.
   * @zh 匹配 `pattern` 的文件：名称；设置 `recursive` 时为完整路径。
   */
  files: string[];
  /**
   * Subdirectories, whatever `pattern` says: names, or full paths when `recursive` is set.
   * @zh 子目录，不受 `pattern` 影响：名称；设置 `recursive` 时为完整路径。
   */
  directories: string[];
  /**
   * The same list as `files`; kept for callers that read this name.
   * @zh 与 `files` 相同的列表；为读取这个名字的调用方保留。
   */
  items: string[];
}

interface DeleteParams {
  /**
   * File or directory to delete.
   * @zh 要删除的文件或目录。
   * @minLength 1
   * @security FileWrite
   */
  path: string;
  /**
   * Send it to the Recycle Bin. `false` deletes it permanently, which fails on a directory that
   * is not empty.
   * @zh 移入回收站。为 `false` 时永久删除，此时非空目录会删除失败。
   * @default true
   */
  moveToTrash?: boolean;
}

interface MkdirParams {
  /**
   * Directory to create.
   * @zh 要创建的目录。
   * @minLength 1
   * @security FileWrite
   */
  path: string;
}

interface MkdirResult {
  /**
   * Whether this call created the directory; `false` when it already existed.
   * @zh 本次调用是否创建了目录；目录已存在时为 `false`。
   */
  created: boolean;
  /**
   * `Directory already exists` when the directory was already there; absent otherwise.
   * @zh 目录已存在时为 `Directory already exists`；否则不出现。
   */
  message?: string;
}

interface CopyParams {
  /**
   * File or directory to copy; a directory is copied with everything below it.
   * @zh 要复制的文件或目录；目录连同其下全部内容一起复制。
   * @minLength 1
   * @security Read
   */
  source: string;
  /**
   * Target path. Its parent directory has to exist already.
   * @zh 目标路径。其父目录必须已经存在。
   * @minLength 1
   * @security FileWrite
   */
  destination: string;
  /**
   * Replace files that already exist at the target. Otherwise they are left as they are and the
   * call still succeeds.
   * @zh 替换目标处已存在的文件。否则保留它们不动，调用仍然成功。
   * @default false
   */
  overwrite?: boolean;
}

interface CopyResult {
  /**
   * The source as given.
   * @zh 原样的源路径。
   */
  source: string;
  /**
   * The destination as given.
   * @zh 原样的目标路径。
   */
  destination: string;
}

interface MoveParams {
  /**
   * File or directory to move.
   * @zh 要移动的文件或目录。
   * @minLength 1
   * @security FileWrite
   */
  source: string;
  /**
   * Target path. An existing file there is replaced; moving a file onto an existing directory,
   * or under a parent directory that does not exist, fails.
   * @zh 目标路径。该处已有的文件会被替换；把文件移到已存在的目录上、或移到不存在的父目录下，都会失败。
   * @minLength 1
   * @security FileWrite
   */
  destination: string;
}

interface MoveResult {
  /**
   * The source as given.
   * @zh 原样的源路径。
   */
  source: string;
  /**
   * The destination as given.
   * @zh 原样的目标路径。
   */
  destination: string;
}

interface RenameParams {
  /**
   * File or directory to rename.
   * @zh 要重命名的文件或目录。
   * @minLength 1
   * @security FileWrite
   */
  path: string;
  /**
   * New name, kept in the same directory. A name containing `/` or `\` fails with
   * `INVALID_PARAMS`.
   * @zh 新名字，仍位于同一目录。含 `/` 或 `\` 的名字以 `INVALID_PARAMS` 失败。
   * @minLength 1
   */
  newName: string;
}

interface RenameResult {
  /**
   * The path as given.
   * @zh 原样的路径。
   */
  oldPath: string;
  /**
   * The parent directory of the expanded `path`, joined with `newName`.
   * @zh 展开后的 `path` 的父目录拼上 `newName`。
   */
  newPath: string;
}

interface GetInfoParams {
  /**
   * File or directory to describe.
   * @zh 要描述的文件或目录。
   * @minLength 1
   * @security Read
   */
  path: string;
}

interface GetInfoResult {
  /**
   * Whether the path exists; when `false`, no other field is present.
   * @zh 路径是否存在；为 `false` 时不带其他字段。
   */
  exists: boolean;
  /**
   * Whether the path is a directory.
   * @zh 路径是否是目录。
   */
  isDirectory?: boolean;
  /**
   * Whether the path is a regular file.
   * @zh 路径是否是普通文件。
   */
  isFile?: boolean;
  /**
   * Size in bytes; `0` for a directory.
   * @zh 字节数；目录为 `0`。
   */
  size?: Int;
  /**
   * Last write time as a JavaScript timestamp in milliseconds, truncated to whole seconds.
   * @zh 最后写入时间，JavaScript 毫秒时间戳，截到整秒。
   */
  modified?: Int;
  /**
   * File name with its extension.
   * @zh 带扩展名的文件名。
   */
  name?: string;
  /**
   * Extension with its leading dot, such as `.flac`; empty when there is none.
   * @zh 带前导点的扩展名，如 `.flac`；没有扩展名时为空串。
   */
  extension?: string;
  /**
   * Parent directory of the path after `%variable%` expansion. The path is not normalized, so a
   * doubled separator left by the expansion stays.
   * @zh 展开 `%变量%` 之后的路径的父目录。路径不做归一化，展开留下的双分隔符会原样保留。
   */
  parent?: string;
}

/**
 * One entry of a `file.copyAsync` or `file.moveAsync` batch.
 * @zh `file.copyAsync` 或 `file.moveAsync` 批次中的一条。
 */
interface FileOpEntry {
  /**
   * File or directory to copy or move. Echoed back verbatim, `%variable%` placeholders
   * unexpanded, in the `file:opProgress` results.
   * @zh 要复制或移动的文件或目录。在 `file:opProgress` 的结果里原样回显，`%变量%` 不展开。
   * @minLength 1
   */
  source: string;
  /**
   * Target path; a missing parent directory is created. When it names an existing directory and
   * `source` is a file, the file is placed inside it under its own name. Echoed back verbatim
   * like `source`.
   * @zh 目标路径；缺失的父目录会被创建。它指向已存在的目录且 `source` 是文件时，文件以原名放进该目录。与 `source` 一样原样回显。
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
   * @zh 要复制的条目，按此顺序每条上报一个结果。条目不是恰好含字符串成员 `source` 与 `destination` 的对象时，调用以 `INVALID_PARAMS` 失败。开始之前逐条检查全部路径，`source` 按读取、`destination` 按写入检查；任何一条被拒（空串也算），整次调用以 `PERMISSION_DENIED` 失败，不产生 `operationId`。
   * @minItems 1
   * @pathKey source=Read destination=FileWrite
   */
  items: FileOpEntry[];
  /**
   * Replace an existing file destination instead of reporting the entry as `skipped` /
   * `already-exists`. A directory copied onto an existing directory is merged into it either
   * way; the files already inside are skipped, without being reported, unless this is set.
   * @zh 替换已存在的文件目标，而不是把该条记为 `skipped` / `already-exists`。目录复制到已存在的目录时总会并入其中；除非设置本项，目录内已存在的文件会被跳过且不单独上报。
   * @default false
   */
  overwrite?: boolean;
}

interface CopyAsyncResult {
  /**
   * Id of the operation, starting with `fileop_`. The events of the operation carry it, and
   * `file.cancelOp` takes it.
   * @zh 操作的 id，以 `fileop_` 开头。该操作的事件带有它，`file.cancelOp` 也用它。
   */
  operationId: string;
  /**
   * Number of entries accepted; the events report it as `total`.
   * @zh 受理的条目数；事件里以 `total` 报告。
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
   * @zh 要移动的条目，按此顺序每条上报一个结果。条目不是恰好含字符串成员 `source` 与 `destination` 的对象时，调用以 `INVALID_PARAMS` 失败。开始之前逐条按写入检查全部路径，`source` 也不例外，因为移动会删掉它；任何一条被拒（空串也算），整次调用以 `PERMISSION_DENIED` 失败，不产生 `operationId`。
   * @minItems 1
   * @pathKey source=FileWrite destination=FileWrite
   */
  items: FileOpEntry[];
  /**
   * Replace an existing file destination instead of reporting the entry as `skipped` /
   * `already-exists`; the synchronous `file.move` always replaces one. An existing directory
   * destination is never replaced: Windows cannot swap a directory in place, so on the same
   * volume such an entry ends as `skipped` or `failed`.
   * @zh 替换已存在的文件目标，而不是把该条记为 `skipped` / `already-exists`；同步的 `file.move` 总是替换。已存在的目录目标永远不会被替换：Windows 不能原地换掉目录，同卷下这样的条目以 `skipped` 或 `failed` 结束。
   * @default false
   */
  overwrite?: boolean;
}

interface MoveAsyncResult {
  /**
   * Id of the operation, starting with `fileop_`. The events of the operation carry it, and
   * `file.cancelOp` takes it.
   * @zh 操作的 id，以 `fileop_` 开头。该操作的事件带有它，`file.cancelOp` 也用它。
   */
  operationId: string;
  /**
   * Number of entries accepted; the events report it as `total`.
   * @zh 受理的条目数；事件里以 `total` 报告。
   */
  totalCount: Int;
}

interface DeleteAsyncParams {
  /**
   * Paths to delete, reported one result each, in this order. An entry that is not a string
   * fails the call with `INVALID_PARAMS`. Every path is checked for writing before anything
   * starts; one refused path, an empty string included, fails the whole call with
   * `PERMISSION_DENIED` and no `operationId`.
   * @zh 要删除的路径，按此顺序每条上报一个结果。有非字符串元素时调用以 `INVALID_PARAMS` 失败。开始之前逐条按写入检查全部路径；任何一条被拒（空串也算），整次调用以 `PERMISSION_DENIED` 失败，不产生 `operationId`。
   * @minItems 1
   * @security FileWrite
   */
  paths: string[];
  /**
   * Send each entry to the Recycle Bin. `false` deletes permanently and removes directories
   * that are not empty, which the synchronous `file.delete` refuses to do.
   * @zh 把每一条移入回收站。为 `false` 时永久删除，并且能删除非空目录，这是同步的 `file.delete` 做不到的。
   * @default true
   */
  moveToTrash?: boolean;
}

interface DeleteAsyncResult {
  /**
   * Id of the operation, starting with `fileop_`. The events of the operation carry it, and
   * `file.cancelOp` takes it.
   * @zh 操作的 id，以 `fileop_` 开头。该操作的事件带有它，`file.cancelOp` 也用它。
   */
  operationId: string;
  /**
   * Number of entries accepted; the events report it as `total`.
   * @zh 受理的条目数；事件里以 `total` 报告。
   */
  totalCount: Int;
}

interface CancelOpParams {
  /**
   * Id from the receipt of `file.copyAsync`, `file.moveAsync` or `file.deleteAsync`.
   * @zh `file.copyAsync`、`file.moveAsync` 或 `file.deleteAsync` 回执里的 id。
   * @minLength 1
   */
  operationId: string;
}

interface CancelOpResult {
  /**
   * `true` when the operation was still running and has been told to stop; `false` when it had
   * already finished or never existed, which are deliberately indistinguishable.
   * @zh 操作仍在进行并已被通知停止时为 `true`；已结束或从未存在时为 `false`，两者故意不可区分。
   */
  cancelled: boolean;
}
