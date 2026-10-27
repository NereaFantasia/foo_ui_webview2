import type { Int } from './common.js';

export interface Api {
  /**
   * Report the configuration of the calling panel. A panel configuration exists only for a
   * DUI element or a CUI panel; on a standalone window the call fails.
   * @zh 获取当前调用方面板的配置信息（仅 DUI 元素与 CUI 面板拥有面板配置，在独立窗口模式下调用将失败）
   */
  getConfig(): GetConfigResult;

  /**
   * Change the calling panel's configuration. Only the keys below can be set from a page;
   * omitted keys keep their value. The other fields change only through the panel's dialog.
   * @zh 更新当前调用方面板的配置项（仅允许修改声明的特定配置键，省略的键保持原值不变；模板或调试等其余字段仅支持通过面板配置对话框调整）
   */
  setConfig(params: SetConfigParams): SetConfigResult;
}

export interface Events {
  /**
   * The panel got keyboard focus. Sent only when the panel's `grabFocus` option is on.
   * @zh 面板获得键盘焦点时派发（仅在面板启用 grabFocus 选项时触发）
   * @delivery window
   */
  focus: void;

  /**
   * The panel lost keyboard focus. Sent only when the panel's `grabFocus` option is on.
   * @zh 面板失去键盘焦点时派发（仅在面板启用 grabFocus 选项时触发）
   * @delivery window
   */
  blur: void;

  /**
   * A DUI element or CUI panel finished creating its WebView. It is sent once, as soon as the
   * WebView is ready, which can be before the page has subscribed; a page that needs the mode
   * reads `window.getMode` on startup instead.
   * @zh DUI 元素或 CUI 面板完成 WebView 实例初始化时派发（在 WebView 就绪时单次触发，此时页面可能尚未完成事件订阅；如需检测运行模式建议在页面启动时主动调用 window.getMode）
   * @delivery window
   */
  initialized: InitializedPayload;

  /**
   * A DUI element was shown or hidden, for instance by switching tabs in a tab container.
   * Columns UI panels do not send it.
   * @zh DUI 元素可见性发生切换时派发（例如选项卡容器中切换标签页；Columns UI 面板不派发此事件）
   * @delivery window
   */
  visibilityChanged: VisibilityChangedPayload;

  /**
   * The panel's configuration changed, through `panel.setConfig`, the panel's settings dialog, or
   * a configuration foobar2000 handed to the element. The payload has the same fields as
   * `config` in `panel.getConfig`.
   * @zh 面板配置发生变更时派发（通过 panel.setConfig、面板设置对话框或宿主注入更新触发；数据结构与 panel.getConfig 返回的 config 字段一致）
   * @delivery window
   */
  configChanged: PanelConfig;
}

interface InitializedPayload {
  /**
   * `dui` for a Default UI element, `cui` for a Columns UI panel.
   * @zh 面板类型（dui 表示 Default UI 元素，cui 表示 Columns UI 面板）
   */
  mode: 'dui' | 'cui';
  /**
   * Always `true`.
   * @zh 是否处于面板模式（固定为 true）
   */
  panelMode: boolean;
  /**
   * Id of the panel, as `window.getMode` reports it.
   * @zh 面板所属窗口唯一标识符 windowId（与 window.getMode 返回值一致）
   */
  windowId: string;
}

interface VisibilityChangedPayload {
  /**
   * Whether the element is now visible.
   * @zh 面板当前是否处于可见状态
   */
  visible: boolean;
}

/**
 * A panel's configuration.
 * @zh 面板实例配置信息
 */
interface PanelConfig {
  /**
   * Display name of the panel.
   * @zh 面板自定义显示名称
   */
  panelName: string;
  /**
   * Name of the page template the panel loads.
   * @zh 面板加载的页面模板名称
   */
  templateName: string;
  /**
   * Edge style of the panel frame: `0` none, `1` sunken, `2` grey.
   * @zh 面板边框样式（0 无边框、1 凹陷立体效果、2 纯灰边框）
   */
  edgeStyle: Int;
  /**
   * URL loaded instead of the template; empty when none.
   * @zh 覆盖模板的自定义加载 URL（未设置时为空字符串）
   */
  urlOverride: string;
  /**
   * Whether the panel renders with a transparent background.
   * @zh 面板是否启用透明背景渲染
   */
  transparentBackground: boolean;
  /**
   * Whether a click gives the panel keyboard focus.
   * @zh 点击面板时是否主动获取键盘焦点
   */
  grabFocus: boolean;
  /**
   * Whether files can be dropped onto the panel.
   * @zh 是否允许向面板拖放文件
   */
  enableDragDrop: boolean;
  /**
   * Whether the developer tools are enabled.
   * @zh 是否启用 WebView2 开发者工具 F12/右键审查
   */
  enableDevTools: boolean;
}

interface GetConfigResult {
  /**
   * The configuration.
   * @zh 面板当前的完整配置对象
   */
  config: PanelConfig;
}

interface SetConfigParams {
  /**
   * Display name of the panel.
   * @zh 面板自定义显示名称
   */
  panelName?: string;
  /**
   * Whether the panel renders with a transparent background.
   * @zh 面板是否启用透明背景渲染
   */
  transparentBackground?: boolean;
  /**
   * Whether a click gives the panel keyboard focus.
   * @zh 点击面板时是否主动获取键盘焦点
   */
  grabFocus?: boolean;
  /**
   * Whether files can be dropped onto the panel.
   * @zh 是否允许向面板拖放文件
   */
  enableDragDrop?: boolean;
}

interface SetConfigResult {
  /**
   * Whether anything changed.
   * @zh 传入配置是否导致面板实际状态发生改变
   */
  changed: boolean;
}
