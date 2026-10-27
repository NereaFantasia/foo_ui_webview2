# Config API

管理持久化配置存储、音频输出设备、DSP 预设、高级偏好设置及播放行为选项。

## 配置存储

### config.get

<!-- api-schema:begin config.get -->
读取指定键下存储的值；键不存在时不报错，返回 `found: false`，且 `value` 取 `default` 传入值（未提供时为 `null`）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `key` | `string` | 是 | 待读取的存储键名不能为空。 |
| `default` | `any` | 否 | 目标键不存在时返回的默认兜底值 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `key` | `string` | 请求读取的存储键名 |
| `value` | `any` | 存储的值；键不存在时返回 `default` 传入值（未提供时返回 `null`） |
| `found` | `boolean` | 目标键是否存在于存储中 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

> 目标键不存在时 `found` 为 `false`；若传入了 `default` 则返回该默认兜底值。

```javascript
const theme = await fb2k.invoke('config.get', { key: 'theme', default: 'light' });
```

### config.set

<!-- api-schema:begin config.set -->
将指定值存储至本组件专属的持久化键值存储中，覆盖该键原有数据；配置保存在 foobar2000 宿主配置区中，软件重启后依然保留；若存储读取失败或值无法保存则以 `OPERATION_FAILED` 失败

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `key` | `string` | 是 | 存储键名；本组件所有页面共享同一存储空间，建议添加主题特有前缀不能为空。 |
| `value` | `any` | 是 | 待存储的值，支持任意可序列化的 JSON 数据；顶层 `null` 会被视作无效键值拒绝，清除键请调用 `config.remove`，嵌套对象或数组内的 `null` 照常保留 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `key` | `string` | 成功写入的存储键名 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
await fb2k.invoke('config.set', { key: 'volume', value: 0.8 });
```

### config.remove

<!-- api-schema:begin config.remove -->
从键值存储中删除指定的键；删除不存在的键仍视为成功，返回 `existed: false`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `key` | `string` | 是 | 待删除的存储键名不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `key` | `string` | 请求删除的存储键名 |
| `existed` | `boolean` | 本次删除操作前该键是否存在于存储中 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### config.getAll

<!-- api-schema:begin config.getAll -->
获取持久化存储中的全部键值对数据

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `items` | `Record<string, any>` | 包含所有存储键值对的字典对象 |
| `configs` | `Record<string, any>` | 与 `items` 完全一致的旧名兼容字典 |
| `count` | `integer` | 存储中包含的键值对总数 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

> `items` 与 `configs` 返回相同字典（后者为旧版兼容别名）。

```javascript
const cfg = await fb2k.invoke('config.getAll');
if (cfg.success === false) throw new Error(cfg.error);
console.log(`配置项数: ${cfg.count}`, cfg.items);
```

### config.export

<!-- api-schema:begin config.export -->
导出持久化存储中的全部配置数据，同时提供对象映射与序列化 JSON 文本两种形式

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `data` | `Record<string, any>` | 包含所有存储键值对的字典对象，同 `config.getAll` 的 `items` |
| `json` | `string` | 将 `data` 序列化后的紧凑 JSON 字符串 |
| `count` | `integer` | 存储中包含的键值对总数 |

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
获取宿主运行环境版本信息，包括 foobar2000 核心版本、本组件版本、系统架构及配置目录绝对路径

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `version` | `string` | 核心报告的 foobar2000 版本字符串，包含产品名称 |
| `foobar2000` | `string` | 与 `version` 完全一致的兼容别名字段 |
| `versionFull` | `string` | SDK `core_version_info_v2::get_name()` 报告的产品名称（固定为 `"foobar2000"`，并非完整版本字符串） |
| `is64bit` | `boolean` | 是否为 64 位（x64）编译版本 |
| `isPortable` | `boolean` | foobar2000 当前是否以便携（Portable）模式运行 |
| `plugin` | `object` | 本 WebView2 UI 组件信息 |
| `plugin.name` | `string` | 固定为 `"foo_ui_webview2"` |
| `plugin.version` | `string` | 本组件版本号，格式为 `主.次.修订` |
| `profilePath` | `string` | 用户配置（Profile）目录的显示路径 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const info = await fb2k.invoke('config.getVersionInfo');
if (info.success === false) throw new Error(info.error);
console.log(`${info.versionFull} | ${info.is64bit ? '64-bit' : '32-bit'}`);
```

### config.getComponents

<!-- api-schema:begin config.getComponents -->
获取当前已安装的 foobar2000 原生组件列表及其版本与模块文件名

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `components` | `ConfigComponentInfo[]` | 按枚举顺序排列的所有已安装组件信息列表 |
| `components[].name` | `string` | 组件自身报告的显示名称 |
| `components[].version` | `string` | 组件自身报告的版本号字符串 |
| `components[].filename` | `string` | 组件对应的 DLL 模块文件名；组件未声明时不返回 |
| `components[].fileName` | `string` | 与 `filename` 完全一致的兼容别名字段 |
| `count` | `integer` | `components` 列表包含的组件总数 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

> `fileName` 和 `filename` 相同（后者为兼容别名）。

### config.getOutputDevices

<!-- api-schema:begin config.getOutputDevices -->
列出所有输出模块下的音频设备并标记当前生效设备；数据形态为扁平列表，每个设备项均关联其所属模块的 GUID

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `devices` | `ConfigOutputDevice[]` | 所有已安装输出模块下的设备列表 |
| `devices[].name` | `string` | 设备显示名称；输出管理器提供时为完整名称，若直接源自输出模块则为 `<模块名>: <设备名>` |
| `devices[].id` | `string` | 设备标识符，与 `deviceId` 相同 |
| `devices[].outputId` | `string` | 输出模块 GUID，格式形如 `{...}` |
| `devices[].deviceId` | `string` | 设备 GUID，格式形如 `{...}`；单独并不保证全局唯一（模块默认设备全为零 GUID），需以 `(outputId, deviceId)` 二元组唯一标识 |
| `devices[].isCurrent` | `boolean` | 该设备是否为当前系统生效的活动输出设备 |
| `count` | `integer` | `devices` 列表包含的设备总数 |

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
将音频输出切换至指定输出模块下的目标设备；模块与设备 GUID 均通过 `config.getOutputDevices` 获取

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `outputId` | `string` | 是 | 目标输出模块 GUID，取自 `config.getOutputDevices` 返回的 `outputId`不能为空。 |
| `deviceId` | `string` | 是 | 目标设备 GUID，取自 `config.getOutputDevices` 返回的 `deviceId`不能为空。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
await fb2k.invoke('config.setOutputDevice', { outputId: '{...}', deviceId: '{...}' });
```

### config.getOutputConfig

<!-- api-schema:begin config.getOutputConfig -->
获取当前生效的音频输出配置，包括所属模块、设备 GUID、缓冲长度、输出位深、抖动与淡入淡出设置

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `outputId` | `string` | 当前生效的输出模块 GUID，格式形如 `{...}` |
| `deviceId` | `string` | 当前生效的设备 GUID，格式形如 `{...}` |
| `bufferLength` | `number` | 当前配置的输出缓冲长度，单位为秒 |
| `bitDepth` | `integer` | 当前配置的输出位深，单位为比特（bit） |
| `useDither` | `boolean` | 是否启用了音频抖动处理 |
| `useFades` | `boolean` | 是否启用了淡入淡出效果 |
| `outputName` | `string` | 当前生效输出模块的显示名称；未匹配到对应模块时不返回 |
| `deviceName` | `string` | 当前生效输出设备的显示名称；模块未提供时不返回 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### config.setOutputBuffer

<!-- api-schema:begin config.setOutputBuffer -->
设置音频输出缓冲长度；支持传入以秒为单位的 `bufferLength` 或以毫秒为单位的 `milliseconds`（两者至少传其一，均提供时以 `milliseconds` 为准且 `bufferLength` 仍受范围检查）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `bufferLength` | `number` | 否 | 缓冲长度，单位为秒；若同时传入 `milliseconds` 则以此为辅助校验范围，以毫秒值为准取值 `0.05` 到 `2`（含端点）。 |
| `milliseconds` | `number` | 否 | 缓冲长度，单位为毫秒；优先级高于 `bufferLength`取值 `50` 到 `2000`（含端点）。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

## 高级配置

### config.getAdvancedConfig

<!-- api-schema:begin config.getAdvancedConfig -->
获取 foobar2000 高级首选项树；可从根节点遍历或从指定的 `parentGuid` 分支节点展开

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `parentGuid` | `string` | 否 | 待遍历展开的分支节点 GUID，格式形如 `{...}`；缺省或为空串时从根节点开始展开，格式合法但非分支的 GUID 返回空列表 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `entries` | `AdvancedConfigItem[]` | 目标分支下一层直接包含的条目列表；子分支项在各自的 `children` 中嵌套包含 |
| `entries[].name` | `string` | 节点条目的显示名称 |
| `entries[].guid` | `string` | 节点 GUID，格式形如 `{...}`，供读取、修改与重置接口传参使用 |
| `entries[].sortPriority` | `number` | 条目声明的排序优先级 |
| `entries[].type` | `"branch" \| "checkbox" \| "radio" \| "integer" \| "string" \| "unknown"` | 高级首选项条目类别：`branch` 容纳子条目，`checkbox` 与 `radio` 存储布尔值，`integer` 与 `string` 存储文本，`unknown` 为其他类型 |
| `entries[].value` | `any` | 条目当前值；`checkbox`/`radio` 为布尔值，`integer`/`string` 为存储的原始文本（整数条目在此为十进制文本格式，而 `config.getAdvancedConfigValue` 返回数字），`branch` 与 `unknown` 省略此字段 |
| `entries[].defaultValue` | `any` | 条目的默认值，数据类型与 `value` 相同；仅在条目提供默认值时出现 |
| `entries[].isSigned` | `boolean` | 是否标记为有符号整数；仅在 `integer` 与 `string` 类型条目上提供 |
| `entries[].isFilePath` | `boolean` | 是否标记为文件路径属性；仅在 `integer` 与 `string` 类型条目上提供 |
| `entries[].isFolderPath` | `boolean` | 是否标记为文件夹路径属性；仅在 `integer` 与 `string` 类型条目上提供 |
| `entries[].children` | `AdvancedConfigItem[]` | 分支节点下包含的子条目列表；仅在 `branch` 类型出现，超过 11 层的深层分支子列表置空截断 |
| `count` | `integer` | `entries` 顶层包含的条目总数（不计深层嵌套项） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### config.getAdvancedConfigValue

<!-- api-schema:begin config.getAdvancedConfigValue -->
根据 GUID 读取单个高级首选项条目；整数类型条目在此方法中直接以数字格式返回，而非十进制文本

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `guid` | `string` | 是 | 待查询条目的 GUID，格式形如 `{...}`不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `name` | `string` | 目标条目的显示名称 |
| `guid` | `string` | 请求中传入的 GUID 原样回显，未做大小写归一化 |
| `type` | `"branch" \| "checkbox" \| "radio" \| "integer" \| "string" \| "unknown"` | 高级首选项条目类别：`branch` 容纳子条目，`checkbox` 与 `radio` 存储布尔值，`integer` 与 `string` 存储文本，`unknown` 为其他类型 |
| `value` | `any` | 目标条目当前值；`checkbox`/`radio` 为布尔值，`integer` 为数字（文本不以数字开头时为 `0`），`string` 为字符串，`branch`/`unknown` 为 `null` |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### config.setAdvancedConfigValue

<!-- api-schema:begin config.setAdvancedConfigValue -->
写入单个高级首选项条目的值；传入的值须与条目类型严格匹配：`checkbox` 与 `radio` 需传入布尔值，`integer` 与 `string` 需传入字符串或数字（数值会以十进制文本保存，`integer` 舍去小数部分，`string` 保留六位小数）；分支节点（`branch`）及其他未知类别条目不可写入

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `guid` | `string` | 是 | 待写入条目的 GUID，格式形如 `{...}`不能为空。 |
| `value` | `any` | 是 | 待写入的新值；各条目类型可接受的数据格式详见方法说明 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### config.resetAdvancedConfig

<!-- api-schema:begin config.resetAdvancedConfig -->
将指定 GUID 的高级首选项条目重置恢复为其默认值

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `guid` | `string` | 是 | 待重置条目的 GUID，格式形如 `{...}`不能为空。 |

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
列出所有已注册的首选项页面与分支节点，先按顺序排列常规页面，随后排列分支节点

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `pages` | `ConfigPreferencesPage[]` | 所有首选项页面与分支列表，页面在前，分支在后 |
| `pages[].name` | `string` | 首选项页面或分支的显示名称 |
| `pages[].guid` | `string` | 页面或分支自身的 GUID，格式形如 `{...}` |
| `pages[].parentGuid` | `string` | 其上级页面或分支的 GUID，格式形如 `{...}`；顶层页面指向标准预设父节点之一 |
| `pages[].sortPriority` | `number` | 页面报告的排序优先级，数值越小越靠前，`0` 按名称字母排序；未声明时默认为 `0` |
| `pages[].isBranch` | `boolean` | 是否为分支节点；为 `true` 时为分支，页面项省略此字段 |
| `count` | `integer` | `pages` 包含的条目总数 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### config.getPreferencesStandardGuids

<!-- api-schema:begin config.getPreferencesStandardGuids -->
获取 foobar2000 内置标准首选项父节点的 GUID 集合；扩展页面与分支以此作为挂载父级

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `root` | `string` | `preferences_page::guid_root` GUID，代表首选项根节点 |
| `hidden` | `string` | `preferences_page::guid_hidden` GUID，代表隐藏首选项节点 |
| `tools` | `string` | `preferences_page::guid_tools` GUID，代表工具类首选项父节点 |
| `core` | `string` | `preferences_page::guid_core` GUID，代表核心类首选项父节点 |
| `display` | `string` | `preferences_page::guid_display` GUID，代表界面与显示类首选项父节点 |
| `playback` | `string` | `preferences_page::guid_playback` GUID，代表播放类首选项父节点 |
| `visualisations` | `string` | `preferences_page::guid_visualisations` GUID，代表可视化类首选项父节点 |
| `input` | `string` | `preferences_page::guid_input` GUID，代表解码与输入类首选项父节点 |
| `tagWriting` | `string` | `preferences_page::guid_tag_writing` GUID，代表标签写入类首选项父节点 |
| `mediaLibrary` | `string` | `preferences_page::guid_media_library` GUID，代表媒体库类首选项父节点 |
| `tagging` | `string` | `preferences_page::guid_tagging` GUID，代表标签处理类首选项父节点 |
| `output` | `string` | `preferences_page::guid_output` GUID，代表音频输出类首选项父节点 |
| `advanced` | `string` | `preferences_page::guid_advanced` GUID，代表高级首选项父节点 |
| `components` | `string` | `preferences_page::guid_components` GUID，代表组件管理类首选项父节点 |
| `dsp` | `string` | `preferences_page::guid_dsp` GUID，代表 DSP 管理类首选项父节点 |
| `shell` | `string` | `preferences_page::guid_shell` GUID，代表系统外壳类首选项父节点 |
| `keyboardShortcuts` | `string` | `preferences_page::guid_keyboard_shortcuts` GUID，代表快捷键设置类首选项父节点 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

## 媒体库配置

### config.getLibraryStatus

<!-- api-schema:begin config.getLibraryStatus -->
获取媒体库状态，包括是否启用、初始化是否就绪以及曲目总数；条目总数每次调用时重新遍历统计

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `enabled` | `boolean` | 媒体库是否已启用（foobar2000 以至少配置了一个媒体库文件夹为准） |
| `itemCount` | `integer` | 媒体库当前包含的音频条目总数，每次调用时重新遍历统计 |
| `initialized` | `boolean` | 媒体库是否已载入初始化完成；宿主无法获知时默认为 `true` |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### config.getLibraryFilePatterns

<!-- api-schema:begin config.getLibraryFilePatterns -->
获取媒体库在新编码、复制或移动操作时归档曲目与专辑封面的目标路径规则；未配置的规则不会出现在结果中，两者均未配置时仅返回成功信封

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `tracks` | `ConfigLibraryFilePattern` | 针对新编码、复制或移动的曲目归档命名规则；未配置时不返回 |
| `tracks.directory` | `string` | 配置的目标存储文件夹路径 |
| `tracks.format` | `string` | 用于生成子文件夹与文件名的标题格式化（Titleformat）模式串 |
| `images` | `ConfigLibraryFilePattern` | 针对新编码、复制或移动的专辑封面归档命名规则；未配置时不返回 |
| `images.directory` | `string` | 配置的目标存储文件夹路径 |
| `images.format` | `string` | 用于生成子文件夹与文件名的标题格式化（Titleformat）模式串 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### config.showLibraryPreferences

<!-- api-schema:begin config.showLibraryPreferences -->
弹出并定位到 foobar2000 的媒体库（Media Library）首选项设置窗口

无参数。

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

- **返回值**: `{ "success": true }`

## DSP 预设

### config.getDspPresets

<!-- api-schema:begin config.getDspPresets -->
获取用户配置目录中已保存的 DSP 预设列表；宿主不支持 DSP 预设时返回空列表

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `presets` | `ConfigDspPreset[]` | 所有已保存的 DSP 预设条目列表，保持列表排序 |
| `presets[].index` | `integer` | 预设在列表中的位置索引，供 `config.setActiveDspPreset` 传参调用 |
| `presets[].name` | `string` | 预设配置名称 |
| `count` | `integer` | `presets` 列表包含的预设总数 |

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
获取当前生效选中的 DSP 预设；未选中预设或预设功能不可用时，`index` 与 `name` 返回 `null` 且 `isActive` 为 `false`

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `index` | `integer \| null` | 当前激活选中的 DSP 预设索引；未选中预设时为 `null` |
| `name` | `string \| null` | 当前激活选中的 DSP 预设名称；未选中预设时为 `null` |
| `isActive` | `boolean` | 是否有 DSP 预设处于激活生效状态 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

> 无激活预设时 `index` 和 `name` 为 `null`，`isActive` 为 `false`。

### config.setActiveDspPreset

<!-- api-schema:begin config.setActiveDspPreset -->
根据索引激活指定的 DSP 预设并整体替换当前活动 DSP 链（效果等同于调用 `dsp.applyPreset` 传入索引）；激活后无法通过此接口恢复为未选中状态

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `index` | `integer` | 是 | 待激活的 DSP 预设在 `config.getDspPresets` 中的位置索引不小于 `0`。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

## 播放行为配置

### config.getCursorFollowPlayback

<!-- api-schema:begin config.getCursorFollowPlayback -->
查询 foobar2000 的「光标跟随播放（Cursor Follows Playback）」开关状态

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `enabled` | `boolean` | 「光标跟随播放」是否已开启 |
| `value` | `boolean` | 与 `enabled` 完全一致的兼容别名字段 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### config.setCursorFollowPlayback

<!-- api-schema:begin config.setCursorFollowPlayback -->
设置 foobar2000 的「光标跟随播放（Cursor Follows Playback）」启用状态

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `enabled` | `boolean` | 是 | 是否开启「光标跟随播放」；`true` 为开启，`false` 为关闭 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `enabled` | `boolean` | 成功设置后的「光标跟随播放」状态 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### config.getPlaybackFollowCursor

<!-- api-schema:begin config.getPlaybackFollowCursor -->
查询 foobar2000 的「播放跟随光标（Playback Follows Cursor）」开关状态

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `enabled` | `boolean` | 「播放跟随光标」是否已开启 |
| `value` | `boolean` | 与 `enabled` 完全一致的兼容别名字段 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### config.setPlaybackFollowCursor

<!-- api-schema:begin config.setPlaybackFollowCursor -->
设置 foobar2000 的「播放跟随光标（Playback Follows Cursor）」启用状态

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `enabled` | `boolean` | 是 | 是否开启「播放跟随光标」；`true` 为开启，`false` 为关闭 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `enabled` | `boolean` | 成功设置后的「播放跟随光标」状态 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### config.getReplaygainMode

<!-- api-schema:begin config.getReplaygainMode -->
以数值编号形式获取当前 ReplayGain 音源模式（`replaygain.getMode` 提供同项设置的字符串名称）

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `mode` | `integer` | 音源模式数值编号：`0` 为无，`1` 为音轨，`2` 为专辑，`3` 为按播放顺序 |
| `value` | `integer` | 与 `mode` 完全一致的兼容别名字段 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

| mode 值 | 含义 |
| 0 | none |
| 1 | track |
| 2 | album |
| 3 | byPlaybackOrder (auto) |

### config.setReplaygainMode

<!-- api-schema:begin config.setReplaygainMode -->
设置 ReplayGain 音源模式；支持传入数值编号 `mode` 或字符串名称 `sourceMode`（两者至少传其一，均提供时以 `mode` 优先，亦可通过 `replaygain.setMode` 修改）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `mode` | `integer` | 否 | 以数值编号指定的音源模式（取值同 `config.getReplaygainMode`），优先级高于 `sourceMode`取值 `0` 到 `3`（含端点）。 |
| `sourceMode` | `"none" \| "track" \| "album" \| "byPlaybackOrder" \| "auto"` | 否 | 以名称字符串指定的音源模式：`"none"` (0)、`"track"` (1)、`"album"` (2)、`"byPlaybackOrder"` / `"auto"` (3)；仅在未提供 `mode` 时生效 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `mode` | `integer` | 成功应用后的生效音源模式数值编号 |
| `value` | `integer` | 与 `mode` 完全一致的兼容别名字段 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

传入无法识别的 `sourceMode` 字符串时返回 `{ "error": "...", "code": "INVALID_PARAMS" }`。注意该失败响应不含 `success` 字段，请勿使用 `result.success === false` 做判定。传入负数 `mode` 时静默按 `none` (0) 处理并返回成功；传入大于 3 的数值不会被校验，直接原样写入并回显。

```javascript
await fb2k.invoke('config.setReplaygainMode', { mode: 1 });
// 或
await fb2k.invoke('config.setReplaygainMode', { sourceMode: 'track' });
```

## 存储与偏好设置语义

`config.set`、`config.get`、`config.remove`、`config.getAll` 与 `config.export`
操作组件专属的持久化配置对象。`config.get` 必填 `key`；键不存在时返回 `found: false`，若
提供了可选的 `default` 则返回该值。`config.set` 必须同时提供 `key` 与 `value`，值支持任意可序列化
JSON 数据。

音频输出与高级偏好设置方法经由 foobar2000 服务执行。其中 `config.setOutputDevice`
要求有效的 `outputId` 与 `deviceId` GUID；`config.setOutputBuffer` 支持以秒为单位的
`bufferLength` 或以毫秒为单位的 `milliseconds`。高级设置项要求传入有效 `guid`，其可接受的
`value` 类型取决于条目类型，不存在统一 schema。

光标跟随与 ReplayGain 的修改方法支持文档记载的兼容传参形式。ReplayGain 的 `mode` 与 `value`
为数值格式，`sourceMode` 支持 `track`、`album`、`auto`、`byPlaybackOrder` 或 `none`。
传入无法识别的 `sourceMode` 字符串时，handler 返回 `INVALID_PARAMS` 错误。
