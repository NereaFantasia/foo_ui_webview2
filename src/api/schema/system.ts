import type { Int, SystemApiInfo, SystemPluginInfo } from './common.js';

export interface Api {
  /**
   * List the methods the bridge currently accepts, built-in ones and those registered by
   * external plugins.
   * @zh 列出当前 Bridge 支持调用的全部 API 方法，包含内置方法与外部插件注册的方法
   */
  listAvailableApis(params: ListAvailableApisParams): ListAvailableApisResult;

  /**
   * List the registered methods of one namespace; an unknown namespace lists nothing.
   * @zh 列出指定命名空间下已注册的 API 方法；若命名空间不存在则返回空列表
   */
  getApisByNamespace(params: GetApisByNamespaceParams): GetApisByNamespaceResult;

  /**
   * Find methods whose full name or description contains `query`, ignoring case.
   * @zh 检索方法名全称或描述中包含指定关键字的方法，匹配时不区分大小写
   */
  searchApis(params: SearchApisParams): SearchApisResult;

  /**
   * Count the registered methods, overall and per namespace, and the registered plugins.
   * @zh 获取 API 统计数据，包括已注册方法总数、各命名空间分布以及外部插件数量
   */
  getApiStats(): GetApiStatsResult;

  /**
   * List the external plugins that registered methods with the bridge.
   * @zh 列出所有向 Bridge 注册过方法的外部插件列表
   */
  getRegisteredPlugins(): GetRegisteredPluginsResult;

  /**
   * Report whether an external plugin owns the namespace.
   * @zh 查询指定命名空间是否已由外部插件注册
   */
  isPluginRegistered(params: IsPluginRegisteredParams): IsPluginRegisteredResult;

  /**
   * Report the Windows personalization settings: app theme, accent color and transparency.
   * @zh 获取 Windows 系统的个性化外观设置，包括应用深浅色主题、系统强调色与窗口透明效果
   */
  getTheme(): GetThemeResult;

  /**
   * Report the DPI of the display showing the calling window; the main window's when the caller
   * cannot be resolved.
   * @zh 获取当前调用窗口所在显示器的 DPI 与缩放比；若无法解析调用方窗口则取主窗口所在显示器参数
   */
  getDPI(): GetDPIResult;

  /**
   * Report the Windows user locale with its localized language and country names.
   * @zh 获取 Windows 用户区域设置以及本地化的语言与国家/地区名称
   */
  getLocale(): GetLocaleResult;
}

export interface Events {
  /**
   * The colours of foobar2000's Default UI changed, dark mode included. Only pages in this
   * plugin's Default UI panels receive it, together with `ui:coloursChanged`; Columns UI panels
   * and the plugin's own windows do not.
   * @zh foobar2000 默认界面（Default UI）配色或深浅色模式发生变更时触发；仅宿主在 Default UI 面板内的页面会收到此事件（与 `ui:coloursChanged` 同步触发），Columns UI 面板及独立弹窗不接收此事件
   * @delivery window
   */
  themeChanged: ThemeChangedPayload;
}

interface ThemeChangedPayload {
  /**
   * Whether Default UI now uses its dark mode. It follows foobar2000's own setting, which can
   * differ from the Windows app theme that `system.getTheme` reports.
   * @zh Default UI 当前是否处于深色模式；该状态由 foobar2000 自身外观设置决定，可能与 `system.getTheme` 返回的 Windows 系统主题不一致
   */
  darkMode: boolean;
}

interface ListAvailableApisParams {
  /**
   * Include built-in methods.
   * @zh 是否包含内置 API 方法
   * @default true
   */
  includeInternal?: boolean;
  /**
   * Include methods registered by external plugins.
   * @zh 是否包含外部插件注册的 API 方法
   * @default true
   */
  includeExternal?: boolean;
}

interface ListAvailableApisResult {
  /**
   * The methods, sorted by `fullName` in ascending byte order.
   * @zh 可用 API 方法列表，按 `fullName` 逐字节升序排列
   */
  apis: SystemApiInfo[];
}

interface GetApisByNamespaceParams {
  /**
   * Namespace to list, such as `playback`.
   * @zh 待查询的命名空间名称，例如 `"playback"`
   * @minLength 1
   */
  namespace: string;
}

interface GetApisByNamespaceResult {
  /**
   * The namespace's methods, in the order of `system.listAvailableApis`; empty for an unknown
   * namespace.
   * @zh 该命名空间下已注册的方法列表，排序与 `system.listAvailableApis` 一致；未知命名空间返回空列表
   */
  apis: SystemApiInfo[];
}

interface SearchApisParams {
  /**
   * Text to look for in full names and descriptions.
   * @zh 搜索关键字，用于匹配方法全称与描述文本
   * @minLength 1
   */
  query: string;
}

interface SearchApisResult {
  /**
   * The matching methods, in the order of `system.listAvailableApis`.
   * @zh 匹配成功的 API 方法列表，排序与 `system.listAvailableApis` 一致
   */
  apis: SystemApiInfo[];
}

interface GetApiStatsResult {
  /**
   * Number of registered methods.
   * @zh 已注册的 API 方法总数
   */
  totalApis: Int;
  /**
   * Number of built-in methods.
   * @zh 内置 API 方法总数
   */
  internalApis: Int;
  /**
   * Number of methods registered by external plugins.
   * @zh 外部插件注册的 API 方法总数
   */
  externalApis: Int;
  /**
   * Number of registered external plugins.
   * @zh 已注册的外部插件总数
   */
  pluginCount: Int;
  /**
   * Method count per namespace.
   * @zh 各命名空间与其已注册方法数量的映射字典
   */
  byNamespace: Record<string, Int>;
}

interface GetRegisteredPluginsResult {
  /**
   * The plugins.
   * @zh 已向 Bridge 注册方法的外部插件信息列表
   */
  plugins: SystemPluginInfo[];
}

interface IsPluginRegisteredParams {
  /**
   * Namespace to look up.
   * @zh 待查询的命名空间名称
   * @minLength 1
   */
  namespace: string;
}

interface IsPluginRegisteredResult {
  /**
   * Whether an external plugin owns the namespace.
   * @zh 目标命名空间是否已由外部插件成功注册
   */
  registered: boolean;
}

interface GetThemeResult {
  /**
   * `true` when Windows uses the dark app theme.
   * @zh Windows 应用主题是否为深色模式；深色时为 `true`
   */
  darkMode: boolean;
  /**
   * Same as `darkMode`; kept for callers that read this name.
   * @zh 与 `darkMode` 含义相同的兼容别名
   */
  isDark: boolean;
  /**
   * Accent color as `#RRGGBB`; `#0078D4` when DWM cannot report one.
   * @zh 系统强调色，格式为 `#RRGGBB`；若 DWM 无法提供则返回默认值 `#0078D4`
   */
  accentColor: string;
  /**
   * Whether transparency effects are enabled.
   * @zh 系统是否启用了窗口透明与亚克力/云母效果
   */
  transparency: boolean;
}

interface GetDPIResult {
  /**
   * DPI of the display; `96` when no window can be resolved.
   * @zh 显示器屏幕 DPI 值；若无法关联到窗口则返回标准基准值 `96`
   */
  dpi: Int;
  /**
   * `dpi / 96`.
   * @zh 显示器缩放比例，计算公式为 `dpi / 96`（100% 缩放时为 1.0）
   */
  scale: number;
}

interface GetLocaleResult {
  /**
   * Locale tag such as `en-US`, the default when Windows reports none.
   * @zh BCP 47 区域语言标签（如 `"en-US"`、`"zh-CN"`）；若系统未提供则缺省为 `"en-US"`
   */
  locale: string;
  /**
   * Localized language name; empty when unavailable.
   * @zh 当前区域设置下的本地化语言显示名称；不可用时为空字符串
   */
  language: string;
  /**
   * Localized country or region name; empty when unavailable.
   * @zh 当前区域设置下的本地化国家或地区显示名称；不可用时为空字符串
   */
  country: string;
}
