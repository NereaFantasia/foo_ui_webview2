import type { Int } from './common.js';

export interface Api {
  /**
   * Report where the host loaded the calling WebView's page from: the development server, a
   * URL, a template folder, or the built-in "Frontend not found" page. The host records this
   * when it submits the navigation; a page that later navigates elsewhere by itself is still
   * reported by that record. Fails with `NOT_FOUND` when the caller is not a WebView of this
   * plugin or the host has not loaded a page into it.
   * @zh 报告宿主把调用方 WebView 的页面从哪里加载：开发服务器、某个 URL、某个模板文件夹，或内置的「Frontend not found」页面。宿主在提交导航时记下来源；页面之后自己导航到别处，报告的仍是这份记录。调用方不是本插件的 WebView，或宿主还没给它加载页面时，以 `NOT_FOUND` 失败。
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
   * @zh 页面的来源。`devServer`：开发服务器。`url`：面板配置里的 URL，或传给 `window.createPopup` 的绝对 URL。`panelTemplate`：面板自己的模板。`activeTemplate`：全局活动模板。`componentDirectory`：组件文件夹里的 `foo_ui_webview2_resources\dist`。`defaultTemplate`：`webview-ui\default`，因为它前面的文件夹都没有 `index.html` 才落到这里。`builtInPage`：所有文件夹都没有 `index.html` 时显示的页面。
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
   * @zh 映射到 `https://foo-ui-webview2.local/` 的文件夹；只在四种文件夹来源下出现。
   */
  directory?: string;
  /**
   * Name of the template folder that was loaded; present for `panelTemplate`, `activeTemplate`
   * and `defaultTemplate`.
   * @zh 加载的模板文件夹名；只在 `panelTemplate`、`activeTemplate`、`defaultTemplate` 下出现。
   */
  templateName?: string;
  /**
   * Address the host navigated to; present for `devServer` and `url`.
   * @zh 宿主导航到的地址；只在 `devServer` 与 `url` 下出现。
   */
  url?: string;
  /**
   * The global active template as configured now, which can differ from the one loaded.
   * @zh 现在配置的全局活动模板，可能与已加载的那个不同。
   */
  activeTemplateName: string;
  /**
   * Folder that holds the templates, `webview-ui` under the foobar2000 profile.
   * @zh 存放模板的文件夹，即 foobar2000 profile 下的 `webview-ui`。
   */
  templatesDirectory: string;
}

export interface Events {
  /**
   * A WebView2 process of this plugin failed. It is sent to every window; when the failure
   * could not be recovered (`recovered` is `false`), the window whose WebView failed is left
   * out.
   * @zh 本插件的某个 WebView2 进程出了故障。此事件发给所有窗口；无法恢复（`recovered` 为 `false`）时，出故障的那个窗口不在其中。
   * @delivery broadcast
   */
  processFailed: ProcessFailedPayload;
}

interface ProcessFailedPayload {
  /**
   * Which process failed; a kind this plugin does not know is reported as
   * `unknownProcessExited`.
   * @zh 出故障的进程；本插件不认识的种类报为 `unknownProcessExited`。
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
   * @zh WebView2 报告的 `COREWEBVIEW2_PROCESS_FAILED_KIND` 值。
   */
  kindRaw: Int;
  /**
   * Whether the WebView is usable again: `true` after a render process was reloaded and for
   * processes the runtime restarts by itself, `false` when the reload failed or the browser
   * process exited.
   * @zh WebView 是否已恢复可用：渲染进程重新加载成功、以及运行时自行重启的进程为 `true`；重新加载失败或浏览器进程退出为 `false`。
   */
  recovered: boolean;
  /**
   * What the plugin did: `reload` for a render process (see `recovered` for the outcome),
   * `needRebuild` when the browser process exited and the WebView has to be created anew,
   * `none` for processes the runtime restarts by itself.
   * @zh 插件采取的处理：渲染进程为 `reload`（结果见 `recovered`）；浏览器进程退出、WebView 需要重新创建时为 `needRebuild`；运行时自行重启的进程为 `none`。
   */
  recoveryAction: 'reload' | 'needRebuild' | 'none';
}
