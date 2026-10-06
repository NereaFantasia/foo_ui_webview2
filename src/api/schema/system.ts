import type { Int, SystemApiInfo, SystemPluginInfo } from './common.js';

export interface Api {
  /**
   * List the methods the bridge currently accepts, built-in ones and those registered by
   * external plugins.
   * @zh 列出桥当前接受的方法，包括内置的和外部插件注册的。
   */
  listAvailableApis(params: ListAvailableApisParams): ListAvailableApisResult;

  /**
   * List the registered methods of one namespace; an unknown namespace lists nothing.
   * @zh 列出某个命名空间下已注册的方法；未知命名空间列出空表。
   */
  getApisByNamespace(params: GetApisByNamespaceParams): GetApisByNamespaceResult;

  /**
   * Find methods whose full name or description contains `query`, ignoring case.
   * @zh 查找全名或描述里包含 `query` 的方法，不区分大小写。
   */
  searchApis(params: SearchApisParams): SearchApisResult;

  /**
   * Count the registered methods, overall and per namespace, and the registered plugins.
   * @zh 统计已注册方法的总数、按命名空间的分布，以及已注册插件数。
   */
  getApiStats(): GetApiStatsResult;

  /**
   * List the external plugins that registered methods with the bridge.
   * @zh 列出向桥注册过方法的外部插件。
   */
  getRegisteredPlugins(): GetRegisteredPluginsResult;

  /**
   * Report whether an external plugin owns the namespace.
   * @zh 报告某个命名空间是否已由外部插件注册。
   */
  isPluginRegistered(params: IsPluginRegisteredParams): IsPluginRegisteredResult;

  /**
   * Report the Windows personalization settings: app theme, accent color and transparency.
   * @zh 报告 Windows 个性化设置：应用主题、强调色与透明效果。
   */
  getTheme(): GetThemeResult;

  /**
   * Report the DPI of the display showing the calling window; the main window's when the caller
   * cannot be resolved.
   * @zh 报告调用窗口所在显示器的 DPI；调用方无法确定时取主窗口的。
   */
  getDPI(): GetDPIResult;

  /**
   * Report the Windows user locale with its localized language and country names.
   * @zh 报告 Windows 用户区域设置及其本地化的语言名与国家名。
   */
  getLocale(): GetLocaleResult;
}

export interface Events {
  /**
   * The colours of foobar2000's Default UI changed, dark mode included. Only pages in this
   * plugin's Default UI panels receive it, together with `ui:coloursChanged`; Columns UI panels
   * and the plugin's own windows do not.
   * @zh foobar2000 默认界面（Default UI）的配色变了，包括深浅色切换。只有本插件 Default UI 面板里的页面会收到，与 `ui:coloursChanged` 一起发；Columns UI 面板与插件自己的窗口收不到。
   * @delivery window
   */
  themeChanged: ThemeChangedPayload;
}

interface ThemeChangedPayload {
  /**
   * Whether Default UI now uses its dark mode. It follows foobar2000's own setting, which can
   * differ from the Windows app theme that `system.getTheme` reports.
   * @zh Default UI 现在是否为深色模式。它跟随 foobar2000 自己的设置，可能与 `system.getTheme` 报告的 Windows 应用主题不同。
   */
  darkMode: boolean;
}

interface ListAvailableApisParams {
  /**
   * Include built-in methods.
   * @zh 包含内置方法。
   * @default true
   */
  includeInternal?: boolean;
  /**
   * Include methods registered by external plugins.
   * @zh 包含外部插件注册的方法。
   * @default true
   */
  includeExternal?: boolean;
}

interface ListAvailableApisResult {
  /**
   * The methods, sorted by `fullName` in ascending byte order.
   * @zh 方法列表，按 `fullName` 逐字节升序排列。
   */
  apis: SystemApiInfo[];
}

interface GetApisByNamespaceParams {
  /**
   * Namespace to list, such as `playback`.
   * @zh 要列出的命名空间，如 `playback`。
   * @minLength 1
   */
  namespace: string;
}

interface GetApisByNamespaceResult {
  /**
   * The namespace's methods, in the order of `system.listAvailableApis`; empty for an unknown
   * namespace.
   * @zh 该命名空间的方法，顺序与 `system.listAvailableApis` 相同；未知命名空间为空。
   */
  apis: SystemApiInfo[];
}

interface SearchApisParams {
  /**
   * Text to look for in full names and descriptions.
   * @zh 在全名与描述里查找的文本。
   * @minLength 1
   */
  query: string;
}

interface SearchApisResult {
  /**
   * The matching methods, in the order of `system.listAvailableApis`.
   * @zh 匹配的方法，顺序与 `system.listAvailableApis` 相同。
   */
  apis: SystemApiInfo[];
}

interface GetApiStatsResult {
  /**
   * Number of registered methods.
   * @zh 已注册方法总数。
   */
  totalApis: Int;
  /**
   * Number of built-in methods.
   * @zh 内置方法数。
   */
  internalApis: Int;
  /**
   * Number of methods registered by external plugins.
   * @zh 外部插件注册的方法数。
   */
  externalApis: Int;
  /**
   * Number of registered external plugins.
   * @zh 已注册的外部插件数。
   */
  pluginCount: Int;
  /**
   * Method count per namespace.
   * @zh 每个命名空间的方法数。
   */
  byNamespace: Record<string, Int>;
}

interface GetRegisteredPluginsResult {
  /**
   * The plugins.
   * @zh 插件列表。
   */
  plugins: SystemPluginInfo[];
}

interface IsPluginRegisteredParams {
  /**
   * Namespace to look up.
   * @zh 要查的命名空间。
   * @minLength 1
   */
  namespace: string;
}

interface IsPluginRegisteredResult {
  /**
   * Whether an external plugin owns the namespace.
   * @zh 该命名空间是否已由外部插件注册。
   */
  registered: boolean;
}

interface GetThemeResult {
  /**
   * `true` when Windows uses the dark app theme.
   * @zh Windows 应用主题为深色时为 `true`。
   */
  darkMode: boolean;
  /**
   * Same as `darkMode`; kept for callers that read this name.
   * @zh 与 `darkMode` 相同，为读这个名字的调用方保留。
   */
  isDark: boolean;
  /**
   * Accent color as `#RRGGBB`; `#0078D4` when DWM cannot report one.
   * @zh 强调色，形如 `#RRGGBB`；DWM 报不出时为 `#0078D4`。
   */
  accentColor: string;
  /**
   * Whether transparency effects are enabled.
   * @zh 是否启用透明效果。
   */
  transparency: boolean;
}

interface GetDPIResult {
  /**
   * DPI of the display; `96` when no window can be resolved.
   * @zh 显示器 DPI；找不到窗口时为 `96`。
   */
  dpi: Int;
  /**
   * `dpi / 96`.
   * @zh 等于 `dpi / 96`。
   */
  scale: number;
}

interface GetLocaleResult {
  /**
   * Locale tag such as `en-US`, the default when Windows reports none.
   * @zh 区域标签，如 `en-US`；Windows 报不出时用这个默认值。
   */
  locale: string;
  /**
   * Localized language name; empty when unavailable.
   * @zh 本地化的语言名；取不到时为空。
   */
  language: string;
  /**
   * Localized country or region name; empty when unavailable.
   * @zh 本地化的国家或地区名；取不到时为空。
   */
  country: string;
}
