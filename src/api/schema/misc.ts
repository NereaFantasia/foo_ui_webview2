export interface Api {
  /**
   * Report the foobar2000 installation directory.
   * @zh 报告 foobar2000 的安装目录。
   */
  getFoobarPath(): PathResult;

  /**
   * Report the foobar2000 profile directory, where configuration and components live.
   * @zh 报告 foobar2000 的配置目录，配置与组件都在这里。
   */
  getProfilePath(): PathResult;

  /**
   * Report the directory the component DLL was loaded from.
   * @zh 报告组件 DLL 所在的目录。
   */
  getComponentPath(): PathResult;

  /**
   * Open the foobar2000 console window.
   * @zh 打开 foobar2000 控制台窗口。
   */
  showConsole(): void;

  /**
   * Open the foobar2000 Preferences dialog.
   * @zh 打开 foobar2000 首选项对话框。
   */
  showPreferences(): void;

  /**
   * Open the media library search window, optionally with a query filled in.
   * @zh 打开媒体库搜索窗口，可预填一个查询。
   */
  showLibrarySearch(params: ShowLibrarySearchParams): ShowLibrarySearchResult;

  /**
   * Show a foobar2000 popup message dialog. The call returns when the dialog is shown, not when
   * it is closed.
   * @zh 显示 foobar2000 的弹出消息对话框。对话框显示出来就返回，不等它关闭。
   */
  showPopupMessage(params: ShowPopupMessageParams): void;

  /**
   * Ask foobar2000 to restart.
   * @zh 让 foobar2000 重启。
   */
  restart(): void;

  /**
   * Ask foobar2000 to exit.
   * @zh 让 foobar2000 退出。
   */
  exit(): void;
}

interface PathResult {
  /**
   * The directory, as a native path.
   * @zh 该目录的原生路径。
   */
  path: string;
  /**
   * The same directory; kept for themes that read `value`.
   * @zh 同一个目录；保留给读 `value` 的主题。
   */
  value: string;
}

interface ShowLibrarySearchParams {
  /**
   * Query filled into the search box; empty opens a blank search.
   * @zh 预填进搜索框的查询；为空则打开空白搜索。
   * @default ""
   */
  query?: string;
}

interface ShowLibrarySearchResult {
  /**
   * The query the window opened with.
   * @zh 窗口打开时使用的查询。
   */
  query: string;
}

interface ShowPopupMessageParams {
  /**
   * Body text of the dialog.
   * @zh 对话框正文。
   * @minLength 1
   */
  message: string;
  /**
   * Title bar text.
   * @zh 标题栏文本。
   * @default "Message"
   */
  title?: string;
}
