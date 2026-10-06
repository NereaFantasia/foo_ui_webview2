# System 系统 API

`system` 命名空间的方法。

## System API

API 发现机制和外部插件注册管理。

### system.listAvailableApis

<!-- api-schema:begin system.listAvailableApis -->
列出桥当前接受的方法，包括内置的和外部插件注册的。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `includeInternal` | `boolean` | 否 | 包含内置方法。默认 `true`。 |
| `includeExternal` | `boolean` | 否 | 包含外部插件注册的方法。默认 `true`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `apis` | [SystemApiInfo[]](../reference/types.md#systemapiinfo) | 方法列表，按 `fullName` 逐字节升序排列。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### system.getApisByNamespace

<!-- api-schema:begin system.getApisByNamespace -->
列出某个命名空间下已注册的方法；未知命名空间列出空表。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `namespace` | `string` | 是 | 要列出的命名空间，如 `playback`。不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `apis` | [SystemApiInfo[]](../reference/types.md#systemapiinfo) | 该命名空间的方法，顺序与 `system.listAvailableApis` 相同；未知命名空间为空。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### system.searchApis

<!-- api-schema:begin system.searchApis -->
查找全名或描述里包含 `query` 的方法，不区分大小写。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `query` | `string` | 是 | 在全名与描述里查找的文本。不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `apis` | [SystemApiInfo[]](../reference/types.md#systemapiinfo) | 匹配的方法，顺序与 `system.listAvailableApis` 相同。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### system.getApiStats

<!-- api-schema:begin system.getApiStats -->
统计已注册方法的总数、按命名空间的分布，以及已注册插件数。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `totalApis` | `integer` | 已注册方法总数。 |
| `internalApis` | `integer` | 内置方法数。 |
| `externalApis` | `integer` | 外部插件注册的方法数。 |
| `pluginCount` | `integer` | 已注册的外部插件数。 |
| `byNamespace` | `Record<string, integer>` | 每个命名空间的方法数。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### system.getRegisteredPlugins

<!-- api-schema:begin system.getRegisteredPlugins -->
列出向桥注册过方法的外部插件。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `plugins` | [SystemPluginInfo[]](../reference/types.md#systemplugininfo) | 插件列表。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### system.isPluginRegistered

<!-- api-schema:begin system.isPluginRegistered -->
报告某个命名空间是否已由外部插件注册。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `namespace` | `string` | 是 | 要查的命名空间。不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `registered` | `boolean` | 该命名空间是否已由外部插件注册。 |

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
报告 Windows 个性化设置：应用主题、强调色与透明效果。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `darkMode` | `boolean` | Windows 应用主题为深色时为 `true`。 |
| `isDark` | `boolean` | 与 `darkMode` 相同，为读这个名字的调用方保留。 |
| `accentColor` | `string` | 强调色，形如 `#RRGGBB`；DWM 报不出时为 `#0078D4`。 |
| `transparency` | `boolean` | 是否启用透明效果。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const theme = await fb2k.invoke('system.getTheme');
if (theme.success === false) throw new Error(theme.error);
console.log(theme.darkMode ? '深色模式' : '浅色模式');
```

### system.getDPI

<!-- api-schema:begin system.getDPI -->
报告调用窗口所在显示器的 DPI；调用方无法确定时取主窗口的。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `dpi` | `integer` | 显示器 DPI；找不到窗口时为 `96`。 |
| `scale` | `number` | 等于 `dpi / 96`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### system.getLocale

<!-- api-schema:begin system.getLocale -->
报告 Windows 用户区域设置及其本地化的语言名与国家名。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `locale` | `string` | 区域标签，如 `en-US`；Windows 报不出时用这个默认值。 |
| `language` | `string` | 本地化的语言名；取不到时为空。 |
| `country` | `string` | 本地化的国家或地区名；取不到时为空。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const locale = await fb2k.invoke('system.getLocale');
if (locale.success === false) throw new Error(locale.error);
console.log(`区域: ${locale.locale}, 语言: ${locale.language}`);
```
