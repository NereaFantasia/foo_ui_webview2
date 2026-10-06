import type { Int } from './common.js';

export interface Api {
  /**
   * Report the configuration of the calling panel. A panel configuration exists only for a
   * DUI element or a CUI panel; on a standalone window the call fails.
   * @zh 报告调用方面板的配置。只有 DUI 元素与 CUI 面板才有面板配置；独立窗口上调用失败。
   */
  getConfig(): GetConfigResult;

  /**
   * Change the calling panel's configuration. Only the keys below can be set from a page;
   * omitted keys keep their value. The other fields change only through the panel's dialog.
   * @zh 修改调用方面板的配置。页面只能改下面这些键，省略的键保持原值；其余字段只能经面板对话框修改。
   */
  setConfig(params: SetConfigParams): SetConfigResult;
}

export interface Events {
  /**
   * The panel got keyboard focus. Sent only when the panel's `grabFocus` option is on.
   * @zh 面板获得了键盘焦点。只在面板的 `grabFocus` 选项打开时发。
   * @delivery window
   */
  focus: void;

  /**
   * The panel lost keyboard focus. Sent only when the panel's `grabFocus` option is on.
   * @zh 面板失去了键盘焦点。只在面板的 `grabFocus` 选项打开时发。
   * @delivery window
   */
  blur: void;

  /**
   * A DUI element or CUI panel finished creating its WebView. It is sent once, as soon as the
   * WebView is ready, which can be before the page has subscribed; a page that needs the mode
   * reads `window.getMode` on startup instead.
   * @zh DUI 元素或 CUI 面板建好了 WebView。只发一次，WebView 一就绪就发，此时页面可能还没订阅；需要运行模式的页面应在启动时读 `window.getMode`。
   * @delivery window
   */
  initialized: InitializedPayload;

  /**
   * A DUI element was shown or hidden, for instance by switching tabs in a tab container.
   * Columns UI panels do not send it.
   * @zh DUI 元素被显示或隐藏，例如在选项卡容器里切换标签页。Columns UI 面板不发。
   * @delivery window
   */
  visibilityChanged: VisibilityChangedPayload;

  /**
   * The panel's configuration changed, through `panel.setConfig`, the panel's settings dialog, or
   * a configuration foobar2000 handed to the element. The payload has the same fields as
   * `config` in `panel.getConfig`.
   * @zh 面板配置变了：经 `panel.setConfig`、面板的设置对话框，或 foobar2000 交给元素的一份配置。载荷与 `panel.getConfig` 的 `config` 字段相同。
   * @delivery window
   */
  configChanged: PanelConfig;
}

interface InitializedPayload {
  /**
   * `dui` for a Default UI element, `cui` for a Columns UI panel.
   * @zh Default UI 元素为 `dui`，Columns UI 面板为 `cui`。
   */
  mode: 'dui' | 'cui';
  /**
   * Always `true`.
   * @zh 恒为 `true`。
   */
  panelMode: boolean;
  /**
   * Id of the panel, as `window.getMode` reports it.
   * @zh 面板的 id，与 `window.getMode` 报告的相同。
   */
  windowId: string;
}

interface VisibilityChangedPayload {
  /**
   * Whether the element is now visible.
   * @zh 元素现在是否可见。
   */
  visible: boolean;
}

/**
 * A panel's configuration.
 * @zh 面板配置。
 */
interface PanelConfig {
  /**
   * Display name of the panel.
   * @zh 面板显示名。
   */
  panelName: string;
  /**
   * Name of the page template the panel loads.
   * @zh 面板加载的页面模板名。
   */
  templateName: string;
  /**
   * Edge style of the panel frame: `0` none, `1` sunken, `2` grey.
   * @zh 面板边框样式：`0` 无边框，`1` 凹陷，`2` 灰边。
   */
  edgeStyle: Int;
  /**
   * URL loaded instead of the template; empty when none.
   * @zh 代替模板加载的 URL；没有时为空。
   */
  urlOverride: string;
  /**
   * Whether the panel renders with a transparent background.
   * @zh 面板是否透明背景。
   */
  transparentBackground: boolean;
  /**
   * Whether a click gives the panel keyboard focus.
   * @zh 点击是否让面板获得键盘焦点。
   */
  grabFocus: boolean;
  /**
   * Whether files can be dropped onto the panel.
   * @zh 是否允许拖放到面板。
   */
  enableDragDrop: boolean;
  /**
   * Whether the developer tools are enabled.
   * @zh 是否启用开发者工具。
   */
  enableDevTools: boolean;
}

interface GetConfigResult {
  /**
   * The configuration.
   * @zh 配置。
   */
  config: PanelConfig;
}

interface SetConfigParams {
  /**
   * Display name of the panel.
   * @zh 面板显示名。
   */
  panelName?: string;
  /**
   * Whether the panel renders with a transparent background.
   * @zh 面板是否透明背景。
   */
  transparentBackground?: boolean;
  /**
   * Whether a click gives the panel keyboard focus.
   * @zh 点击是否让面板获得键盘焦点。
   */
  grabFocus?: boolean;
  /**
   * Whether files can be dropped onto the panel.
   * @zh 是否允许拖放到面板。
   */
  enableDragDrop?: boolean;
}

interface SetConfigResult {
  /**
   * Whether anything changed.
   * @zh 是否有改动。
   */
  changed: boolean;
}
