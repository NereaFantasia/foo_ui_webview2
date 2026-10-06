import type { Int } from './common.js';

export interface Api {
  /**
   * Reveal a file or folder in Windows Explorer, selecting it.
   * @zh 在资源管理器中显示并选中文件或文件夹。
   */
  showInExplorer(params: ShowInExplorerParams): void;

  /**
   * Open a file with the application Windows associates with its type. Executables, scripts,
   * installers, shortcuts and libraries are refused with `PERMISSION_DENIED`.
   * @zh 用 Windows 关联的程序打开文件。可执行文件、脚本、安装包、快捷方式与库一律以 `PERMISSION_DENIED` 拒绝。
   */
  openWith(params: OpenWithParams): void;

  /**
   * Open an `http://`, `https://` or `mailto:` URL with the default handler.
   * @zh 用默认程序打开 `http://`、`https://` 或 `mailto:` URL。
   */
  openExternal(params: OpenExternalParams): void;

  /**
   * Start a process from a command line and return at once; nothing is waited for. Commands are
   * not allow-listed: a theme is trusted like an installed component. `cwd` goes through path
   * security.
   * @zh 按命令行启动进程并立即返回，不等待。不设命令白名单：主题的信任边界等同于已安装的组件。`cwd` 经路径安全校验。
   */
  exec(params: ExecParams): ExecResult;

  /**
   * Start a process from an executable and an argument list. With `waitForExitMs` the call waits
   * that long for an early exit; a non-zero exit within the wait fails with `OPERATION_FAILED`
   * and the failure carries `processId`, `exited` and `exitCode`. An absolute executable path and
   * `cwd` go through path security.
   * @zh 按可执行文件与参数列表启动进程。给了 `waitForExitMs` 时等待这么久看它是否提前退出；等待内以非零码退出则以 `OPERATION_FAILED` 失败，失败信封带 `processId`、`exited` 与 `exitCode`。绝对路径的可执行文件与 `cwd` 经路径安全校验。
   */
  spawn(params: SpawnParams): SpawnResult;
}

interface ShowInExplorerParams {
  /**
   * File or folder to reveal.
   * @zh 要显示的文件或文件夹。
   * @minLength 1
   * @security Read
   */
  path: string;
}

interface OpenWithParams {
  /**
   * File to hand to its associated application.
   * @zh 交给关联程序打开的文件。
   * @minLength 1
   * @security Read
   */
  path: string;
}

interface OpenExternalParams {
  /**
   * URL to open; any other scheme fails with `INVALID_PARAMS`.
   * @zh 要打开的 URL；其他协议以 `INVALID_PARAMS` 失败。
   * @minLength 1
   */
  url: string;
}

interface ExecParams {
  /**
   * Command line, run as given.
   * @zh 命令行，按原样执行。
   * @minLength 1
   */
  command: string;
  /**
   * Arguments appended to the command line, each quoted.
   * @zh 追加到命令行末尾的参数，逐个加引号。
   */
  args?: string[];
  /**
   * Working directory; the process inherits foobar2000's when omitted or empty.
   * @zh 工作目录；缺省或为空时继承 foobar2000 的。
   */
  cwd?: string;
  /**
   * Start the process without a visible window.
   * @zh 不显示进程窗口。
   * @default true
   */
  hidden?: boolean;
}

interface ExecResult {
  /**
   * Windows process id of the started process.
   * @zh 已启动进程的 Windows 进程 id。
   */
  processId: Int;
}

interface SpawnParams {
  /**
   * Executable name (resolved through PATH) or path. Surrounding whitespace and quotes are removed.
   * @zh 可执行文件名（经 PATH 解析）或路径。首尾空白与引号会被去掉。
   * @minLength 1
   */
  executable: string;
  /**
   * Arguments passed to the process, each quoted.
   * @zh 传给进程的参数，逐个加引号。
   */
  args?: string[];
  /**
   * Working directory, which has to exist; the process inherits foobar2000's when omitted or empty.
   * @zh 工作目录，必须存在；缺省或为空时继承 foobar2000 的。
   */
  cwd?: string;
  /**
   * Start the process without a visible window.
   * @zh 不显示进程窗口。
   * @default true
   */
  hidden?: boolean;
  /**
   * Milliseconds to wait for an early exit; `0` returns as soon as the process starts.
   * @zh 等待进程提前退出的毫秒数；`0` 表示进程一启动就返回。
   * @minimum 0
   * @default 0
   */
  waitForExitMs?: Int;
}

interface SpawnResult {
  /**
   * Windows process id of the started process.
   * @zh 已启动进程的 Windows 进程 id。
   */
  processId: Int;
  /**
   * Whether the process exited within `waitForExitMs`; absent when no wait was requested.
   * @zh 进程是否在 `waitForExitMs` 内退出；没有要求等待时不出现。
   */
  exited?: boolean;
  /**
   * The exit code, present when `exited` is `true`.
   * @zh 退出码，`exited` 为 `true` 时出现。
   */
  exitCode?: Int;
}
