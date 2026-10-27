# System 系统 API

系统信息、运行时环境及 API 发现机制。

## System API - 系统与运行环境 {#system-api}

提供 API 发现机制、外部插件注册管理以及系统主题、DPI 与区域设置查询。

### system.listAvailableApis

<!-- api-schema:begin system.listAvailableApis -->
列出当前 Bridge 支持调用的全部 API 方法，包含内置方法与外部插件注册的方法

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `includeInternal` | `boolean` | 否 | 是否包含内置 API 方法默认 `true`。 |
| `includeExternal` | `boolean` | 否 | 是否包含外部插件注册的 API 方法默认 `true`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `apis` | [SystemApiInfo[]](../reference/types.md#systemapiinfo) | 可用 API 方法列表，按 `fullName` 逐字节升序排列 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### system.getApisByNamespace

<!-- api-schema:begin system.getApisByNamespace -->
列出指定命名空间下已注册的 API 方法；若命名空间不存在则返回空列表

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `namespace` | `string` | 是 | 待查询的命名空间名称，例如 `"playback"`不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `apis` | [SystemApiInfo[]](../reference/types.md#systemapiinfo) | 该命名空间下已注册的方法列表，排序与 `system.listAvailableApis` 一致；未知命名空间返回空列表 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### system.searchApis

<!-- api-schema:begin system.searchApis -->
检索方法名全称或描述中包含指定关键字的方法，匹配时不区分大小写

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `query` | `string` | 是 | 搜索关键字，用于匹配方法全称与描述文本不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `apis` | [SystemApiInfo[]](../reference/types.md#systemapiinfo) | 匹配成功的 API 方法列表，排序与 `system.listAvailableApis` 一致 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### system.getApiStats

<!-- api-schema:begin system.getApiStats -->
获取 API 统计数据，包括已注册方法总数、各命名空间分布以及外部插件数量

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `totalApis` | `integer` | 已注册的 API 方法总数 |
| `internalApis` | `integer` | 内置 API 方法总数 |
| `externalApis` | `integer` | 外部插件注册的 API 方法总数 |
| `pluginCount` | `integer` | 已注册的外部插件总数 |
| `byNamespace` | `Record<string, integer>` | 各命名空间与其已注册方法数量的映射字典 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### system.getRegisteredPlugins

<!-- api-schema:begin system.getRegisteredPlugins -->
列出所有向 Bridge 注册过方法的外部插件列表

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `plugins` | [SystemPluginInfo[]](../reference/types.md#systemplugininfo) | 已向 Bridge 注册方法的外部插件信息列表 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### system.isPluginRegistered

<!-- api-schema:begin system.isPluginRegistered -->
查询指定命名空间是否已由外部插件注册

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `namespace` | `string` | 是 | 待查询的命名空间名称不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `registered` | `boolean` | 目标命名空间是否已由外部插件成功注册 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const result = await fb2k.invoke('system.isPluginRegistered', { namespace: 'my_plugin' });
if (result.success === false) throw new Error(result.error);
if (result.registered) {
    await fb2k.invoke('my_plugin.doSomething');
}
```

### system.getTheme

<!-- api-schema:begin system.getTheme -->
获取 Windows 系统的个性化外观设置，包括应用深浅色主题、系统强调色与窗口透明效果

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `darkMode` | `boolean` | Windows 应用主题是否为深色模式；深色时为 `true` |
| `isDark` | `boolean` | 与 `darkMode` 含义相同的兼容别名 |
| `accentColor` | `string` | 系统强调色，格式为 `#RRGGBB`；若 DWM 无法提供则返回默认值 `#0078D4` |
| `transparency` | `boolean` | 系统是否启用了窗口透明与亚克力/云母效果 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const theme = await fb2k.invoke('system.getTheme');
if (theme.success === false) throw new Error(theme.error);
console.log(theme.darkMode ? '深色模式' : '浅色模式');
```

### system.getDPI

<!-- api-schema:begin system.getDPI -->
获取当前调用窗口所在显示器的 DPI 与缩放比；若无法解析调用方窗口则取主窗口所在显示器参数

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `dpi` | `integer` | 显示器屏幕 DPI 值；若无法关联到窗口则返回标准基准值 `96` |
| `scale` | `number` | 显示器缩放比例，计算公式为 `dpi / 96`（100% 缩放时为 1.0） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### system.getLocale

<!-- api-schema:begin system.getLocale -->
获取 Windows 用户区域设置以及本地化的语言与国家/地区名称

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `locale` | `string` | BCP 47 区域语言标签（如 `"en-US"`、`"zh-CN"`）；若系统未提供则缺省为 `"en-US"` |
| `language` | `string` | 当前区域设置下的本地化语言显示名称；不可用时为空字符串 |
| `country` | `string` | 当前区域设置下的本地化国家或地区显示名称；不可用时为空字符串 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const locale = await fb2k.invoke('system.getLocale');
if (locale.success === false) throw new Error(locale.error);
console.log(`区域: ${locale.locale}, 语言: ${locale.language}`);
```

## 行为与限制

- `system.*` 仅用于查询已注册的运行时信息、外部插件状态与 Windows 系统环境。
