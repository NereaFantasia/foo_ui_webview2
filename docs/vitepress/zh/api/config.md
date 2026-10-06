# Config API

配置存储、系统信息、输出设备、高级配置、DSP 预设、播放行为配置。

## 配置存储

### config.get

<!-- api-schema:begin config.get -->
读取某个键下存的值。键不存在不算错误：`found` 为 `false`，`value` 为 `default`，没给时为 `null`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `key` | `string` | 是 | 要读取的键。不能为空。 |
| `default` | `any` | 否 | 键不存在时用来作答的值。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `key` | `string` | 请求的键。 |
| `value` | `any` | 存储的值；键不存在时为 `default`（给了的话），否则为 `null`。 |
| `found` | `boolean` | 该键是否在存储中。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

> 键不存在时 `found` 为 `false`，若提供了 `default` 参数则返回默认值。

```javascript
const theme = await fb2k.invoke('config.get', { key: 'theme', default: 'light' });
```

### config.set

<!-- api-schema:begin config.set -->
把一个值存到本组件自己的键值存储里的某个键下，覆盖该键原有的值。存储放在 foobar2000 的配置存储里，重启后仍在。存储读不出或值保存不了时以 `OPERATION_FAILED` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `key` | `string` | 是 | 存储用的键。本组件的所有页面共用一个存储，请给键加上自己的前缀。不能为空。 |
| `value` | `any` | 是 | 要存的值，任意 JSON 值。顶层 `null` 按缺失处理并被拒绝，清除键请用 `config.remove`；对象或数组里的 `null` 照常保存。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `key` | `string` | 写入的键。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
await fb2k.invoke('config.set', { key: 'volume', value: 0.8 });
```

### config.remove

<!-- api-schema:begin config.remove -->
从存储中删除一个键。删除不存在的键也成功，`existed` 为 `false`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `key` | `string` | 是 | 要删除的键。不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `key` | `string` | 请求的键。 |
| `existed` | `boolean` | 调用之前该键是否在存储中。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### config.getAll

<!-- api-schema:begin config.getAll -->
读取整个存储：每个键及其值。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `items` | `Record<string, any>` | 存储里的每个键及其值。 |
| `configs` | `Record<string, any>` | 与 `items` 相同的映射，这是它的旧名字。 |
| `count` | `integer` | 存储里的键数。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

> `items` 和 `configs` 内容相同（后者为兼容别名）。

```javascript
const cfg = await fb2k.invoke('config.getAll');
if (cfg.success === false) throw new Error(cfg.error);
console.log(`配置项数: ${cfg.count}`, cfg.items);
```

### config.export

<!-- api-schema:begin config.export -->
读取整个存储，同时给出映射与一段 JSON 文本两种形式。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `data` | `Record<string, any>` | 存储里的每个键及其值；即 `config.getAll` 报告的映射。 |
| `json` | `string` | `data` 序列化成的一段紧凑 JSON 文本。 |
| `count` | `integer` | 存储里的键数。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

> `data` 为 JSON 对象，`json` 为序列化字符串。

```javascript
const exported = await fb2k.invoke('config.export');
if (exported.success === false) throw new Error(exported.error);
localStorage.setItem('fb2k_backup', exported.json);
```

## 系统信息

### config.getVersionInfo

<!-- api-schema:begin config.getVersionInfo -->
报告 foobar2000 的版本、本组件的版本与配置目录。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `version` | `string` | 核心报告的 foobar2000 版本字符串，其中含产品名。 |
| `foobar2000` | `string` | 与 `version` 相同。 |
| `versionFull` | `string` | `core_version_info_v2::get_name()` 的返回值，foobar2000 SDK 注明它是产品名 `foobar2000`；虽然键名如此，它并不是 `version` 的完整形式。 |
| `is64bit` | `boolean` | 是否为 64 位构建。 |
| `isPortable` | `boolean` | foobar2000 是否以便携模式运行。 |
| `plugin` | `object` | 本组件。 |
| `plugin.name` | `string` | 恒为 `foo_ui_webview2`。 |
| `plugin.version` | `string` | 本组件的版本，`主.次.修订`。 |
| `profilePath` | `string` | 配置目录，显示路径形式。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const info = await fb2k.invoke('config.getVersionInfo');
if (info.success === false) throw new Error(info.error);
console.log(`${info.versionFull} | ${info.is64bit ? '64-bit' : '32-bit'}`);
```

### config.getComponents

<!-- api-schema:begin config.getComponents -->
列出已安装的组件及其版本。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `components` | `ConfigComponentInfo[]` | 所有已安装的组件，按枚举顺序。 |
| `components[].name` | `string` | 组件报告的名称。 |
| `components[].version` | `string` | 组件报告的版本字符串。 |
| `components[].filename` | `string` | 组件报告的模块文件名；不提供时不出现。 |
| `components[].fileName` | `string` | 与 `filename` 相同。 |
| `count` | `integer` | `components` 的条目数。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

> `fileName` 和 `filename` 相同（后者为兼容别名）。

### config.getOutputDevices

<!-- api-schema:begin config.getOutputDevices -->
列出所有输出模块的设备，并标出当前生效的那个。与 `output.getDevices` 列出的是同一批设备，这里是扁平形状，每项都带所属模块的 GUID。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `devices` | `ConfigOutputDevice[]` | 所有输出模块的设备。 |
| `devices[].name` | `string` | 显示名：输出管理器报告的完整名称；设备列表取自各输出模块本身时为 `<模块名>: <设备名>`。 |
| `devices[].id` | `string` | 与 `deviceId` 相同。 |
| `devices[].outputId` | `string` | 输出模块的 GUID，形如 `{...}`。 |
| `devices[].deviceId` | `string` | 设备 GUID，形如 `{...}`。单独不唯一：模块用全零 GUID 表示其默认设备，要用 `(outputId, deviceId)` 作键。 |
| `devices[].isCurrent` | `boolean` | 该模块与设备是否就是当前生效的那一对，即 `config.getOutputConfig` 报告的那一对。 |
| `count` | `integer` | `devices` 的条目数。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const res = await fb2k.invoke('config.getOutputDevices');
if (res.success === false) throw new Error(res.error);
const { devices } = res;
const current = devices.find(d => d.isCurrent);
console.log(`当前输出: ${current.name}`);
```

### config.setOutputDevice

<!-- api-schema:begin config.setOutputDevice -->
把输出切换到某个输出模块的某个设备；两个 GUID 都取自 `config.getOutputDevices`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `outputId` | `string` | 是 | 输出模块的 GUID，取自 `config.getOutputDevices` 的 `outputId`。不能为空。 |
| `deviceId` | `string` | 是 | 设备的 GUID，取自 `config.getOutputDevices` 的 `deviceId`。不能为空。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
await fb2k.invoke('config.setOutputDevice', { outputId: '{...}', deviceId: '{...}' });
```

### config.getOutputConfig

<!-- api-schema:begin config.getOutputConfig -->
报告当前生效的输出设置：模块、设备、缓冲长度、位深、抖动与淡入淡出。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `outputId` | `string` | 当前生效的输出模块的 GUID，形如 `{...}`。 |
| `deviceId` | `string` | 当前生效的设备的 GUID，形如 `{...}`。 |
| `bufferLength` | `number` | 输出缓冲长度，单位秒。 |
| `bitDepth` | `integer` | 输出位深设置，单位位。 |
| `useDither` | `boolean` | 是否开启抖动。 |
| `useFades` | `boolean` | 是否开启淡入淡出。 |
| `outputName` | `string` | 输出模块的显示名；没有已安装的模块对应该 GUID 时不出现。 |
| `deviceName` | `string` | 设备的显示名；模块不提供名称时不出现。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### config.setOutputBuffer

<!-- api-schema:begin config.setOutputBuffer -->
设置输出缓冲长度，单位秒（`bufferLength`）或毫秒（`milliseconds`）。两者至少给一个；都给时以 `milliseconds` 为准，此时 `bufferLength` 仍会做范围检查。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `bufferLength` | `number` | 否 | 缓冲长度，单位秒。给了 `milliseconds` 时不采用，但仍做范围检查。取值 `0.05` 到 `2`（含端点）。 |
| `milliseconds` | `number` | 否 | 缓冲长度，单位毫秒；优先于 `bufferLength`。取值 `50` 到 `2000`（含端点）。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

## 高级配置

### config.getAdvancedConfig

<!-- api-schema:begin config.getAdvancedConfig -->
以树的形式列出 foobar2000 的高级首选项，从根开始，或从 `parentGuid` 之下开始。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `parentGuid` | `string` | 否 | 要列出的分支的 GUID，形如 `{...}`；省略或为空串时从树根开始。格式正确但不是分支的 GUID 列出空结果。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `entries` | `AdvancedConfigItem[]` | 请求的分支下一层的条目；每个分支在 `children` 里带着自己的条目。 |
| `entries[].name` | `string` | 显示名。 |
| `entries[].guid` | `string` | 条目 GUID，形如 `{...}`，`config.getAdvancedConfigValue`、`config.setAdvancedConfigValue` 与 `config.resetAdvancedConfig` 用它。 |
| `entries[].sortPriority` | `number` | 条目报告的排序优先级。 |
| `entries[].type` | `"branch" \| "checkbox" \| "radio" \| "integer" \| "string" \| "unknown"` | 高级首选项条目的类别：`branch` 容纳其他条目，`checkbox` 与 `radio` 存布尔值，`integer` 与 `string` 存文本，`unknown` 是其他类别。 |
| `entries[].value` | `any` | 当前值：`checkbox` 与 `radio` 上是布尔值；`integer` 与 `string` 上是存储的文本，所以整数条目在这里是十进制文本，而 `config.getAdvancedConfigValue` 报告的是数字；`branch` 与 `unknown` 上没有这个键。 |
| `entries[].defaultValue` | `any` | 默认值，形式与 `value` 相同；只在条目提供默认值时出现。 |
| `entries[].isSigned` | `boolean` | 条目是否标记为有符号整数；只在 `integer` 与 `string` 上出现。 |
| `entries[].isFilePath` | `boolean` | 条目是否标记为存放文件路径；只在 `integer` 与 `string` 上出现。 |
| `entries[].isFolderPath` | `boolean` | 条目是否标记为存放文件夹路径；只在 `integer` 与 `string` 上出现。 |
| `entries[].children` | `AdvancedConfigItem[]` | 分支里的条目；只在 `branch` 上出现。树在请求的父节点之下第十一层截止：这一层的分支以空的 `children` 列出。 |
| `count` | `integer` | `entries` 的条目数，不计嵌套的条目。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### config.getAdvancedConfigValue

<!-- api-schema:begin config.getAdvancedConfigValue -->
按 GUID 读取一个高级首选项条目。整数条目的值在这里是数字，而 `config.getAdvancedConfig` 列出的是它的十进制文本。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `guid` | `string` | 是 | 条目 GUID，形如 `{...}`。不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `name` | `string` | 条目的显示名。 |
| `guid` | `string` | 原样回显请求中的 GUID，不规范化大小写。 |
| `type` | `"branch" \| "checkbox" \| "radio" \| "integer" \| "string" \| "unknown"` | 高级首选项条目的类别：`branch` 容纳其他条目，`checkbox` 与 `radio` 存布尔值，`integer` 与 `string` 存文本，`unknown` 是其他类别。 |
| `value` | `any` | 当前值：`checkbox` 与 `radio` 上是布尔值，`integer` 上是数字（存储的文本不以数字开头时为 `0`），`string` 上是字符串，`branch` 与 `unknown` 上是 `null`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### config.setAdvancedConfigValue

<!-- api-schema:begin config.setAdvancedConfigValue -->
写入一个高级首选项条目。值要与条目相符：`checkbox` 或 `radio` 条目要布尔值；`integer` 或 `string` 条目要字符串或数字，数字按十进制文本存储（`integer` 条目舍去小数部分，`string` 条目保留六位小数）。`branch` 与其他类别的条目不能写。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `guid` | `string` | 是 | 条目 GUID，形如 `{...}`。不能为空。 |
| `value` | `any` | 是 | 要写入的值；各类条目接受什么见方法说明。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### config.resetAdvancedConfig

<!-- api-schema:begin config.resetAdvancedConfig -->
把一个高级首选项条目恢复为默认值。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `guid` | `string` | 是 | 条目 GUID，形如 `{...}`。不能为空。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
// 获取、修改、重置高级配置项
const val = await fb2k.invoke('config.getAdvancedConfigValue', { guid: '{...}' });
await fb2k.invoke('config.setAdvancedConfigValue', { guid: '{...}', value: true });
await fb2k.invoke('config.resetAdvancedConfig', { guid: '{...}' });
```

## 偏好设置

### config.getPreferencesPages

<!-- api-schema:begin config.getPreferencesPages -->
列出所有已注册的首选项页面与首选项分支：先列页面，再列分支。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `pages` | `ConfigPreferencesPage[]` | 先是所有页面，然后是所有分支。 |
| `pages[].name` | `string` | 显示名。 |
| `pages[].guid` | `string` | 页面或分支的 GUID，形如 `{...}`。 |
| `pages[].parentGuid` | `string` | 它所在的上级页面或分支的 GUID，形如 `{...}`；顶层页面是 `config.getPreferencesStandardGuids` 里的某个标准父节点。 |
| `pages[].sortPriority` | `number` | 它报告的排序优先级：越小越靠前，`0` 按名称排序；不提供时也是 `0`。 |
| `pages[].isBranch` | `boolean` | 分支上为 `true`；页面上没有这个键。 |
| `count` | `integer` | `pages` 的条目数。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### config.getPreferencesStandardGuids

<!-- api-schema:begin config.getPreferencesStandardGuids -->
报告 foobar2000 标准首选项父节点的 GUID；已注册的页面与分支以它们作为父节点。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `root` | `string` | `preferences_page::guid_root`，形如 `{...}`：首选项树的根。 |
| `hidden` | `string` | `preferences_page::guid_hidden`，形如 `{...}`。 |
| `tools` | `string` | `preferences_page::guid_tools`，形如 `{...}`。 |
| `core` | `string` | `preferences_page::guid_core`，形如 `{...}`。 |
| `display` | `string` | `preferences_page::guid_display`，形如 `{...}`。 |
| `playback` | `string` | `preferences_page::guid_playback`，形如 `{...}`。 |
| `visualisations` | `string` | `preferences_page::guid_visualisations`，形如 `{...}`。 |
| `input` | `string` | `preferences_page::guid_input`，形如 `{...}`。 |
| `tagWriting` | `string` | `preferences_page::guid_tag_writing`，形如 `{...}`。 |
| `mediaLibrary` | `string` | `preferences_page::guid_media_library`，形如 `{...}`。 |
| `tagging` | `string` | `preferences_page::guid_tagging`，形如 `{...}`。 |
| `output` | `string` | `preferences_page::guid_output`，形如 `{...}`。 |
| `advanced` | `string` | `preferences_page::guid_advanced`，形如 `{...}`。 |
| `components` | `string` | `preferences_page::guid_components`，形如 `{...}`。 |
| `dsp` | `string` | `preferences_page::guid_dsp`，形如 `{...}`。 |
| `shell` | `string` | `preferences_page::guid_shell`，形如 `{...}`。 |
| `keyboardShortcuts` | `string` | `preferences_page::guid_keyboard_shortcuts`，形如 `{...}`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

## 媒体库配置

### config.getLibraryStatus

<!-- api-schema:begin config.getLibraryStatus -->
报告媒体库是否启用、是否载入完成，以及其中有多少条目。计数在每次调用时遍历整个媒体库。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `enabled` | `boolean` | 媒体库是否启用；foobar2000 以「至少配置了一个媒体库文件夹」为准。 |
| `itemCount` | `integer` | 媒体库中的条目数，每次调用时重新计数。 |
| `initialized` | `boolean` | 媒体库是否已载入完成；宿主无法判断时为 `true`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### config.getLibraryFilePatterns

<!-- api-schema:begin config.getLibraryFilePatterns -->
报告 foobar2000 把新编码、复制或移动的曲目与专辑图片放到哪里。没有配置的模式不出现，两者都没配置时响应只有 `{ success: true }`。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `tracks` | `ConfigLibraryFilePattern` | 新编码、复制或移动的曲目所用的模式；没有配置时不出现。 |
| `tracks.directory` | `string` | 配置的目标文件夹。 |
| `tracks.format` | `string` | 该文件夹之下的子文件夹与文件名所用的标题格式化模式。 |
| `images` | `ConfigLibraryFilePattern` | 新编码、复制或移动的专辑图片所用的模式；没有配置时不出现。 |
| `images.directory` | `string` | 配置的目标文件夹。 |
| `images.format` | `string` | 该文件夹之下的子文件夹与文件名所用的标题格式化模式。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### config.showLibraryPreferences

<!-- api-schema:begin config.showLibraryPreferences -->
打开 foobar2000 的媒体库首选项页面。

无参数。

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

- **返回值**: `{ "success": true }`

## DSP 预设

### config.getDspPresets

<!-- api-schema:begin config.getDspPresets -->
列出配置目录里保存的 DSP 预设。宿主不支持 DSP 预设时报告空列表。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `presets` | `ConfigDspPreset[]` | 所有已保存的预设，按列表顺序。 |
| `presets[].index` | `integer` | 在预设列表中的位置，`config.setActiveDspPreset` 用它。 |
| `presets[].name` | `string` | 预设名。 |
| `count` | `integer` | `presets` 的条目数。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const res = await fb2k.invoke('config.getDspPresets');
if (res.success === false) throw new Error(res.error);
const { presets } = res;
presets.forEach(p => console.log(`${p.index}: ${p.name}`));
```

### config.getActiveDspPreset

<!-- api-schema:begin config.getActiveDspPreset -->
报告当前选中的 DSP 预设。没有选中或预设不可用时 `index` 与 `name` 为 `null`，`isActive` 为 `false`。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `index` | `integer \| null` | 选中预设的位置；没有选中时为 `null`。 |
| `name` | `string \| null` | 选中预设的名称；没有选中时为 `null`。 |
| `isActive` | `boolean` | 是否有预设被选中。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

> 无激活预设时 `index` 和 `name` 为 `null`，`isActive` 为 `false`。

### config.setActiveDspPreset

<!-- api-schema:begin config.setActiveDspPreset -->
按索引选中一个 DSP 预设，用它整条替换当前 DSP 链；与按索引调用 `dsp.applyPreset` 是同一个操作。选中之后无法再回到「未选中」。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `index` | `integer` | 是 | 预设在 `config.getDspPresets` 中的位置。不小于 `0`。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

## 播放行为配置

### config.getCursorFollowPlayback

<!-- api-schema:begin config.getCursorFollowPlayback -->
报告 foobar2000 的「光标跟随播放」设置。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `enabled` | `boolean` | 该设置是否打开。 |
| `value` | `boolean` | 与 `enabled` 相同。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### config.setCursorFollowPlayback

<!-- api-schema:begin config.setCursorFollowPlayback -->
打开或关闭 foobar2000 的「光标跟随播放」设置。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `enabled` | `boolean` | 是 | `true` 打开该设置，`false` 关闭。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `enabled` | `boolean` | 写入的值。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### config.getPlaybackFollowCursor

<!-- api-schema:begin config.getPlaybackFollowCursor -->
报告 foobar2000 的「播放跟随光标」设置。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `enabled` | `boolean` | 该设置是否打开。 |
| `value` | `boolean` | 与 `enabled` 相同。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### config.setPlaybackFollowCursor

<!-- api-schema:begin config.setPlaybackFollowCursor -->
打开或关闭 foobar2000 的「播放跟随光标」设置。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `enabled` | `boolean` | 是 | `true` 打开该设置，`false` 关闭。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `enabled` | `boolean` | 写入的值。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### config.getReplaygainMode

<!-- api-schema:begin config.getReplaygainMode -->
以数字报告 ReplayGain 音源模式。`replaygain.getMode` 以名称报告同一项设置。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `mode` | `integer` | 音源模式：`0` 无，`1` 音轨，`2` 专辑，`3` 按播放顺序。 |
| `value` | `integer` | 与 `mode` 相同。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

| mode 值 | 含义 |
| 0 | none |
| 1 | track |
| 2 | album |
| 3 | byPlaybackOrder (auto) |

### config.setReplaygainMode

<!-- api-schema:begin config.setReplaygainMode -->
按数字（`mode`）或名称（`sourceMode`）设置 ReplayGain 音源模式。两者至少给一个；都给时以 `mode` 为准。`replaygain.setMode` 按名称修改同一项设置。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `mode` | `integer` | 否 | 按数字给出的音源模式，与 `config.getReplaygainMode` 报告的一致；优先于 `sourceMode`。取值 `0` 到 `3`（含端点）。 |
| `sourceMode` | `"none" \| "track" \| "album" \| "byPlaybackOrder" \| "auto"` | 否 | 按名称给出的音源模式：`none` 为 `0`，`track` 为 `1`，`album` 为 `2`，`byPlaybackOrder` 或其另一写法 `auto` 为 `3`。只在没给 `mode` 时读取。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `mode` | `integer` | 现在生效的音源模式，以数字表示。 |
| `value` | `integer` | 与 `mode` 相同。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

`sourceMode` 传入无法识别的字符串时返回 `{ "error": "...", "code": "INVALID_PARAMS" }`——该分支**完全不含** `success` 键，因此不要用 `result.success === false` 判断失败。负数 `mode` 会静默按 `none`（0）处理并报告成功；大于 3 的数值不会被校验，原样写入并回显。

```javascript
await fb2k.invoke('config.setReplaygainMode', { mode: 1 });
// 或
await fb2k.invoke('config.setReplaygainMode', { sourceMode: 'track' });
```

## 存储与偏好设置语义

`config.set`、`config.get`、`config.remove`、`config.getAll` 与 `config.export`
操作组件的持久配置对象。`config.get` 要求 `key`；键不存在时返回 `found: false`，且在
提供可选 `default` 时使用该值。`config.set` 同时要求 `key` 和 `value`，值可以是任意
JSON 值。

输出与高级偏好设置方法使用 foobar2000 service。具体而言，`config.setOutputDevice`
要求有效的 `outputId` 与 `deviceId` GUID；`config.setOutputBuffer` 接受以秒为单位的
`bufferLength` 或以毫秒为单位的 `milliseconds`。高级项要求有效 `guid`，其可接受的
`value` 类型取决于条目类型，并非统一 schema。

光标跟随和 ReplayGain setter 接受文档中的兼容形式。ReplayGain 的 `mode` 与 `value`
为数字形式，`sourceMode` 接受 `track`、`album`、`auto`、`byPlaybackOrder` 或 `none`。
`sourceMode` 传入无法识别的字符串时，handler 返回 `INVALID_PARAMS`。
