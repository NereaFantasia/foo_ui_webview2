import type { Int } from './common.js';

export interface Api {
  /**
   * Report where the host loaded the calling WebView's page from: the development server, a
   * URL, a template folder, or the built-in "Frontend not found" page. The host records this
   * when it submits the navigation; a page that later navigates elsewhere by itself is still
   * reported by that record. Fails with `NOT_FOUND` when the caller is not a WebView of this
   * plugin or the host has not loaded a page into it.
   * @zh 获取调用方 WebView 页面的加载来源。来源包括开发服务器、自定义 URL、模板文件夹或内置「Frontend not found」缺省页。宿主在发起导航时记录来源；页面后续自行重定向仍返回该记录。调用方非本插件 WebView 或尚未加载页面时返回 `NOT_FOUND`
   */
  getSource(): GetSourceResult;
}

interface GetSourceResult {
  /**
   * Where the page came from. `devServer`: the development server. `url`: a URL from the
   * panel's configuration, or an absolute URL passed to `window.createPopup`. `panelTemplate`:
   * the panel's own template. `activeTemplate`: the global active template. `componentDirectory`:
   * `foo_ui_webview2_resources\dist` in the component's folder. `defaultTemplate`:
   * `webview-ui\default`, reached because the folders before it had no `index.html`.
   * `builtInPage`: the page shown when no folder had an `index.html`.
   * @zh 页面加载来源。`devServer`：开发服务器；`url`：面板配置 URL 或传给 `window.createPopup` 的绝对 URL；`panelTemplate`：面板独立模板；`activeTemplate`：全局活动模板；`componentDirectory`：组件内置资源目录（`foo_ui_webview2_resources\dist`）；`defaultTemplate`：回退默认模板（`webview-ui\default`，各前序目录均缺失 `index.html` 时使用）；`builtInPage`：各模板均未找到 `index.html` 时展示的内置缺省页
   */
  source:
    | 'devServer'
    | 'url'
    | 'panelTemplate'
    | 'activeTemplate'
    | 'componentDirectory'
    | 'defaultTemplate'
    | 'builtInPage';
  /**
   * Folder mapped to `https://foo-ui-webview2.local/`; present for the four folder sources.
   * @zh 映射到虚拟域 `https://foo-ui-webview2.local/` 的本地目录绝对路径（仅在四种目录类来源下存在）
   */
  directory?: string;
  /**
   * Name of the template folder that was loaded; present for `panelTemplate`, `activeTemplate`
   * and `defaultTemplate`.
   * @zh 已加载的模板文件夹名称（仅在 `panelTemplate`、`activeTemplate` 与 `defaultTemplate` 来源下存在）
   */
  templateName?: string;
  /**
   * Address the host navigated to; present for `devServer` and `url`.
   * @zh 宿主导航的目标地址（仅在 `devServer` 与 `url` 来源下存在）
   */
  url?: string;
  /**
   * The global active template as configured now, which can differ from the one loaded.
   * @zh 当前配置的全局活动模板名称（可能与实际加载的模板不一致）
   */
  activeTemplateName: string;
  /**
   * Folder that holds the templates, `webview-ui` under the foobar2000 profile.
   * @zh 存放模板的根目录绝对路径（对应配置目录下的 `webview-ui`）
   */
  templatesDirectory: string;
}

export interface Events {
  /**
   * A WebView2 process of this plugin failed. It is sent to every window; when the failure
   * could not be recovered (`recovered` is `false`), the window whose WebView failed is left
   * out.
   * @zh 广播本插件关联的 WebView2 核心或辅助进程故障事件。向所有窗口广播；若故障不可恢复（`recovered` 为 `false`），崩溃窗口本身不接收此事件
   * @delivery broadcast
   */
  processFailed: ProcessFailedPayload;
}

interface ProcessFailedPayload {
  /**
   * Which process failed; a kind this plugin does not know is reported as
   * `unknownProcessExited`.
   * @zh 发生故障的进程类型（未知或未识别的种类返回 `unknownProcessExited`）
   */
  kind:
    | 'browserProcessExited'
    | 'renderProcessExited'
    | 'renderProcessUnresponsive'
    | 'frameRenderProcessExited'
    | 'utilityProcessExited'
    | 'sandboxHelperProcessExited'
    | 'gpuProcessExited'
    | 'ppapiPluginProcessExited'
    | 'ppapiBrokerProcessExited'
    | 'unknownProcessExited';
  /**
   * The `COREWEBVIEW2_PROCESS_FAILED_KIND` value WebView2 reported.
   * @zh WebView2 原始报告的 `COREWEBVIEW2_PROCESS_FAILED_KIND` 枚举数值
   */
  kindRaw: Int;
  /**
   * Whether the WebView is usable again: `true` after a render process was reloaded and for
   * processes the runtime restarts by itself, `false` when the reload failed or the browser
   * process exited.
   * @zh WebView 实例是否恢复可用。渲染进程重载成功或运行时自行重启时为 `true`；重载失败或浏览器主进程退出时为 `false`
   */
  recovered: boolean;
  /**
   * What the plugin did: `reload` for a render process (see `recovered` for the outcome),
   * `needRebuild` when the browser process exited and the WebView has to be created anew,
   * `none` for processes the runtime restarts by itself.
   * @zh 插件采取的故障恢复动作。渲染进程故障时为 `reload`（恢复结果见 `recovered`）；浏览器主进程退出且需重建实例时为 `needRebuild`；运行时自行重启时为 `none`
   */
  recoveryAction: 'reload' | 'needRebuild' | 'none';
}
