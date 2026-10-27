export interface Api {
  /**
   * Report the foobar2000 installation directory.
   * @zh 获取 foobar2000 主程序所在的安装目录路径
   */
  getFoobarPath(): PathResult;

  /**
   * Report the foobar2000 profile directory, where configuration and components live.
   * @zh 获取 foobar2000 用户配置目录路径（包含配置文件与已安装组件）
   */
  getProfilePath(): PathResult;

  /**
   * Report the directory the component DLL was loaded from.
   * @zh 获取当前组件 DLL 文件所在的安装目录路径
   */
  getComponentPath(): PathResult;

  /**
   * Open the foobar2000 console window.
   * @zh 弹出并激活 foobar2000 宿主控制台日志窗口
   */
  showConsole(): void;

  /**
   * Open the foobar2000 Preferences dialog.
   * @zh 弹出并激活 foobar2000 宿主首选项设置对话框
   */
  showPreferences(): void;

  /**
   * Open the media library search window, optionally with a query filled in.
   * @zh 弹出并激活媒体库搜索窗口（支持预填搜索查询表达式）
   */
  showLibrarySearch(params: ShowLibrarySearchParams): ShowLibrarySearchResult;

  /**
   * Show a foobar2000 popup message dialog. The call returns when the dialog is shown, not when
   * it is closed.
   * @zh 弹出原生提示消息对话框（对话框弹出后立即返回，不阻塞等待用户确认关闭）
   */
  showPopupMessage(params: ShowPopupMessageParams): void;

  /**
   * Ask foobar2000 to restart.
   * @zh 请求重启 foobar2000 应用程序
   */
  restart(): void;

  /**
   * Ask foobar2000 to exit.
   * @zh 请求正常退出 foobar2000 应用程序
   */
  exit(): void;
}

interface PathResult {
  /**
   * The directory, as a native path.
   * @zh 原生目录完整文件路径
   */
  path: string;
  /**
   * The same directory; kept for themes that read `value`.
   * @zh 原生目录完整文件路径（保留供兼容读取 value 的主题）
   */
  value: string;
}

interface ShowLibrarySearchParams {
  /**
   * Query filled into the search box; empty opens a blank search.
   * @zh 预填入搜索框的查询表达式（传入空字符串或省略时展示空白搜索框）
   * @default ""
   */
  query?: string;
}

interface ShowLibrarySearchResult {
  /**
   * The query the window opened with.
   * @zh 搜索窗口打开时实际采用的查询表达式
   */
  query: string;
}

interface ShowPopupMessageParams {
  /**
   * Body text of the dialog.
   * @zh 提示对话框的正文内容
   * @minLength 1
   */
  message: string;
  /**
   * Title bar text.
   * @zh 提示对话框的标题栏文本
   * @default "Message"
   */
  title?: string;
}
