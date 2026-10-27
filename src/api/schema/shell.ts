import type { Int } from './common.js';

export interface Api {
  /**
   * Reveal a file or folder in Windows Explorer, selecting it.
   * @zh 在 Windows 资源管理器中打开指定路径并自动高亮选中目标文件或文件夹
   */
  showInExplorer(params: ShowInExplorerParams): void;

  /**
   * Open a file with the application Windows associates with its type. Executables, scripts,
   * installers, shortcuts and libraries are refused with `PERMISSION_DENIED`.
   * @zh 使用 Windows 文件类型关联的默认程序打开文件；可执行文件、脚本、安装包、快捷方式与动态库等危险类型一律拒绝并返回 `PERMISSION_DENIED`
   */
  openWith(params: OpenWithParams): void;

  /**
   * Open an `http://`, `https://` or `mailto:` URL with the default handler.
   * @zh 调用系统默认关联程序打开外部链接，仅支持 `http://`、`https://` 与 `mailto:` 协议
   */
  openExternal(params: OpenExternalParams): void;

  /**
   * Start a process from a command line and return at once; nothing is waited for. Commands are
   * not allow-listed: a theme is trusted like an installed component. `cwd` goes through path
   * security.
   * @zh 启动命令行进程并立即返回，不等待执行结束；不设命令白名单（主题信任边界等同于已安装组件）；若指定 `cwd` 则会进行路径安全校验
   */
  exec(params: ExecParams): ExecResult;

  /**
   * Start a process from an executable and an argument list. With `waitForExitMs` the call waits
   * that long for an early exit; a non-zero exit within the wait fails with `OPERATION_FAILED`
   * and the failure carries `processId`, `exited` and `exitCode`. An absolute executable path and
   * `cwd` go through path security.
   * @zh 根据可执行程序路径与参数列表创建子进程；若指定 `waitForExitMs` 则等待对应毫秒以检测提前退出，等待期内以非零退出码退出时以 `OPERATION_FAILED` 失败且错误信封附带 `processId`、`exited` 与 `exitCode`；绝对路径可执行文件与 `cwd` 均受路径安全校验保护
   */
  spawn(params: SpawnParams): SpawnResult;
}

interface ShowInExplorerParams {
  /**
   * File or folder to reveal.
   * @zh 待在资源管理器中定位的文件或文件夹路径
   * @minLength 1
   * @security MediaRead
   */
  path: string;
}

interface OpenWithParams {
  /**
   * File to hand to its associated application.
   * @zh 待通过系统关联程序打开的文件路径
   * @minLength 1
   * @security MediaRead
   */
  path: string;
}

interface OpenExternalParams {
  /**
   * URL to open; any other scheme fails with `INVALID_PARAMS`.
   * @zh 待打开的外部链接，仅允许 `http://`、`https://` 或 `mailto:` 协议，其他协议以 `INVALID_PARAMS` 失败
   * @minLength 1
   */
  url: string;
}

interface ExecParams {
  /**
   * Command line, run as given.
   * @zh 待执行的命令行字符串，按原样传入执行
   * @minLength 1
   */
  command: string;
  /**
   * Arguments appended to the command line, each quoted.
   * @zh 追加至命令行末尾的参数列表，各参数会自动添加双引号转义
   */
  args?: string[];
  /**
   * Working directory; the process inherits foobar2000's when omitted or empty.
   * @zh 子进程工作目录；缺省或为空时继承 foobar2000 主进程工作目录
   */
  cwd?: string;
  /**
   * Start the process without a visible window.
   * @zh 是否以隐藏窗口模式启动子进程
   * @default true
   */
  hidden?: boolean;
}

interface ExecResult {
  /**
   * Windows process id of the started process.
   * @zh 已启动子进程的 Windows 进程 ID
   */
  processId: Int;
}

interface SpawnParams {
  /**
   * Executable name (resolved through PATH) or path. Surrounding whitespace and quotes are removed.
   * @zh 待启动的可执行文件名（从系统 PATH 环境变量解析）或绝对路径，首尾空白符与引号会自动剔除
   * @minLength 1
   */
  executable: string;
  /**
   * Arguments passed to the process, each quoted.
   * @zh 传递给子进程的参数数组，各参数会自动添加双引号转义
   */
  args?: string[];
  /**
   * Working directory, which has to exist; the process inherits foobar2000's when omitted or empty.
   * @zh 子进程工作目录，目录必须已存在；缺省或为空时继承 foobar2000 主进程工作目录
   */
  cwd?: string;
  /**
   * Start the process without a visible window.
   * @zh 是否以隐藏窗口模式启动子进程
   * @default true
   */
  hidden?: boolean;
  /**
   * Milliseconds to wait for an early exit; `0` returns as soon as the process starts.
   * @zh 等待子进程提前退出的超时毫秒数；设为 `0` 时进程启动后立即返回
   * @minimum 0
   * @default 0
   */
  waitForExitMs?: Int;
}

interface SpawnResult {
  /**
   * Windows process id of the started process.
   * @zh 已启动子进程的 Windows 进程 ID
   */
  processId: Int;
  /**
   * Whether the process exited within `waitForExitMs`; absent when no wait was requested.
   * @zh 子进程是否在 `waitForExitMs` 等待时间内已退出；未请求等待时不返回此字段
   */
  exited?: boolean;
  /**
   * The exit code, present when `exited` is `true`.
   * @zh 子进程退出码；仅当 `exited` 为 `true` 时返回
   */
  exitCode?: Int;
}
