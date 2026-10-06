# Discovery 服务发现

（v1.1.3+）主动发现 foobar2000 中其他组件注册的服务。

> 与 PluginRegistry 的被动注册模式不同，Discovery API 主动枚举系统中所有已注册的服务。

## 服务发现

### discovery.getAllServices

<!-- api-schema:begin discovery.getAllServices -->
统计当前进程里可发现的各类服务数量：主菜单命令、右键菜单命令、菜单组、输入格式、UI 元素、DSP、输出后端、首选项页与组件。两个菜单族与列举端点走同一次枚举，只计宿主会显示的条目。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `services` | `DiscoveryServiceCounts` | 各类服务的数量。 |
| `services.mainMenuCommands` | `integer` | 宿主会显示的主菜单命令数，含动态子项。 |
| `services.mainMenuDynamicCommands` | `integer` | `mainMenuCommands` 里有多少是从动态子菜单展开的。 |
| `services.mainMenuGroups` | `integer` | 主菜单分组数。 |
| `services.contextMenuCommands` | `integer` | 宿主会显示的右键菜单命令数。 |
| `services.inputFormats` | `integer` | 可播放文件类型数。 |
| `services.uiElements` | `integer` | 已注册的 UI 元素数。 |
| `services.dspEntries` | `integer` | 已注册的 DSP 数。 |
| `services.outputDevices` | `integer` | 已注册的输出后端数。 |
| `services.preferencePages` | `integer` | 首选项页数。 |
| `services.components` | `integer` | 已安装组件数。 |
| `contextMenuHiddenFiltered` | `integer` | 「宿主是否会显示」这道过滤去掉了多少右键菜单条目。 |
| `stateKnown` | `boolean` | 右键菜单状态是否可观测；没有选中或播放曲目时为 false。 |
| `totalServices` | `integer` | 各类之和，不含已计入 `mainMenuCommands` 的 `mainMenuDynamicCommands`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

`services.contextMenuCommands` 与 `discovery.getContextMenuCommands` 走同一次枚举，并计入 `totalServices`。两个菜单族都已过滤为宿主实际会显示的条目，因此计数与列举端点可直接对照。

```javascript
const summary = await fb2k.invoke('discovery.getAllServices');
if (summary.success === false) throw new Error(summary.error);
console.log(`共 ${summary.totalServices} 个服务`);
```

### discovery.getComponents

<!-- api-schema:begin discovery.getComponents -->
列出已安装的组件。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `components` | `DiscoveryComponentInfo[]` | 组件列表。 |
| `components[].filename` | `string` | DLL 文件名。 |
| `components[].name` | `string` | 组件名。 |
| `components[].version` | `string` | 组件报告的版本串。 |
| `components[].about` | `string` | 关于文本。 |
| `count` | `integer` | `components` 的条数。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### discovery.getInputFormats

<!-- api-schema:begin discovery.getInputFormats -->
列出可播放的输入文件类型，每项带显示名与文件名掩码。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `fileTypes` | `DiscoveryInputFormatType[]` | 文件类型列表。 |
| `fileTypes[].name` | `string` | 显示名，如 `FLAC`。 |
| `fileTypes[].mask` | `string` | 文件名掩码，如 `*.FLAC`。 |
| `fileTypes[].index` | `integer` | 在注册它的服务中的序号。 |
| `count` | `integer` | `fileTypes` 的条数。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### discovery.getUIElements

<!-- api-schema:begin discovery.getUIElements -->
列出已注册的 UI 元素。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `elements` | `DiscoveryUIElementInfo[]` | 元素列表。 |
| `elements[].guid` | `string` | 元素 GUID。 |
| `elements[].subclassGuid` | `string` | 子类 GUID。 |
| `elements[].name` | `string` | 显示名。 |
| `elements[].description` | `string` | 描述。 |
| `elements[].isUserAddable` | `boolean` | 用户能否把它加进布局。 |
| `count` | `integer` | `elements` 的条数。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### discovery.getDspEntries

<!-- api-schema:begin discovery.getDspEntries -->
列出已注册的 DSP 条目。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `entries` | `DiscoveryDspEntryInfo[]` | DSP 列表。 |
| `entries[].guid` | `string` | DSP 的 GUID。 |
| `entries[].name` | `string` | 显示名。 |
| `count` | `integer` | `entries` 的条数。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### discovery.getOutputDevices

<!-- api-schema:begin discovery.getOutputDevices -->
按 GUID 列出已注册的输出后端。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `devices` | `DiscoveryOutputDeviceEntry[]` | 后端列表。 |
| `devices[].guid` | `string` | 后端 GUID。 |
| `count` | `integer` | `devices` 的条数。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### discovery.getPreferencePages

<!-- api-schema:begin discovery.getPreferencePages -->
列出首选项对话框的页面。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `pages` | `DiscoveryPreferencePageInfo[]` | 页面列表。 |
| `pages[].guid` | `string` | 页面 GUID。 |
| `pages[].parentGuid` | `string` | 上级页面的 GUID。 |
| `pages[].name` | `string` | 显示名。 |
| `count` | `integer` | `pages` 的条数。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

## 主菜单

### discovery.getMainMenuCommands

<!-- api-schema:begin discovery.getMainMenuCommands -->
列出主菜单命令（扁平），带标签路径、GUID 与显示状态。组件在运行时构建的子菜单（`mainmenu_commands_v2`）默认展开，子命令与父槽位并列。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `expandDynamic` | `boolean` | 否 | 是否展开组件在运行时构建的子菜单。`false` 只列静态注册表。默认 `true`。 |
| `includeHidden` | `boolean` | 否 | 是否把宿主不会显示的条目也列出。默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `commands` | `DiscoveryMainMenuCommand[]` | 命令列表，每个服务内先是静态槽位，随后是展开出的子项。 |
| `commands[].enabled` | `boolean` | 是否可点击。`stateKnown` 为 false 时无意义。 |
| `commands[].checked` | `boolean` | 是否带勾选标记（单选标记也算）。 |
| `commands[].radioChecked` | `boolean` | 勾选标记是否为单选样式。 |
| `commands[].hidden` | `boolean` | 宿主是否不会绘制该条目。 |
| `commands[].stateKnown` | `boolean` | `enabled` 与 `checked` 是否真正观测到。无法评估时为 false，例如没有选中或播放曲目时的右键菜单项。 |
| `commands[].flags` | `integer` | 组件报告的原始显示位。主菜单：`1` 禁用、`2` 勾选、`4` 单选、`8` 默认隐藏。右键菜单：`1` 勾选、`2` 禁用、`4` 灰显、`8` 单选。 |
| `commands[].name` | `string` | 宿主报告的标签（翻译版本上是本地化文本）。 |
| `commands[].description` | `string` | 组件提供的描述；没有时为空。 |
| `commands[].guid` | `string` | 命令 GUID。从动态子菜单展开的条目，这是其所属命令的 GUID。 |
| `commands[].parentGuid` | `string` | 命令所属分组的 GUID。 |
| `commands[].index` | `integer` | 命令在其服务中的序号。 |
| `commands[].path` | `string` | 斜杠分隔的标签路径；静态槽位就是它的标签。 |
| `commands[].isDynamic` | `boolean` | 是否属于动态子菜单，无论是父槽位还是展开出的子项。 |
| `commands[].isDynamicParent` | `boolean` | 是否为动态子菜单的父槽位：只是容器，本身不能执行。 |
| `commands[].subGuid` | `string` | 动态子菜单里子节点的 GUID；执行时与 `guid` 一起传给 `discovery.executeMainMenuCommand`。静态条目没有。 |
| `commands[].source` | `"mainmenu_static" \| "mainmenu_dynamic" \| "contextmenu_static" \| "contextmenu_dynamic" \| "hmenu_fallback"` | 条目的来源。 |
| `commands[].executable` | `boolean` | 能否执行。 |
| `commands[].unaddressableReason` | `"" \| "separator" \| "dynamicParent" \| "noStableIdentifier" \| "emptyNode"` | 不能执行的原因；能执行时为空串。 |
| `count` | `integer` | `commands` 的条数。 |
| `expandDynamic` | `boolean` | 实际生效的 `expandDynamic`。 |
| `includeHidden` | `boolean` | 实际生效的 `includeHidden`。 |
| `dynamicCount` | `integer` | 从动态子菜单展开出的条数。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

默认会展开 `mainmenu_commands_v2` 的动态子菜单（ESLyric 等 SMP 老组件常用）。父命令槽位为 `isDynamicParent: true`，仅作容器、本身不可执行；展开出来的条目带 `isDynamic: true` 与 `subGuid`，执行时必须把 `subGuid` 一起传给 `discovery.executeMainMenuCommand`。

### discovery.getMainMenuGroups

<!-- api-schema:begin discovery.getMainMenuGroups -->
列出主菜单命令所属的分组。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `groups` | `DiscoveryMainMenuGroup[]` | 分组列表。 |
| `groups[].guid` | `string` | 分组 GUID。 |
| `groups[].parentGuid` | `string` | 上级分组的 GUID。 |
| `groups[].name` | `string` | 显示名；不是弹出菜单的分组为空。 |
| `groups[].sortPriority` | `integer` | 在上级里的排序优先级。 |
| `count` | `integer` | `groups` 的条数。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### discovery.executeMainMenuCommand

<!-- api-schema:begin discovery.executeMainMenuCommand -->
按 GUID 执行主菜单命令。动态子菜单里的子命令用父命令的 `guid` 加自己的 `subGuid` 寻址。没有命令拥有该 GUID 时以 `NOT_FOUND` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `guid` | `string` | 是 | `{...}` 形式的命令 GUID。动态子项传其父命令的 GUID。不能为空。 |
| `subGuid` | `string` | 否 | 动态子项的 `subGuid`，取自 `discovery.getMainMenuCommands`。空串视为没传。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `guid` | `string` | 执行的 GUID。 |
| `subGuid` | `string` | 执行的 `subGuid`；只在动态路径上有。 |
| `dynamic` | `boolean` | 是否走了动态执行路径。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

`NOT_FOUND` 失败时回带 `guid`、`dynamic`，走动态路径时还带 `subGuid`。

### discovery.searchCommands

<!-- api-schema:begin discovery.searchCommands -->
在两个菜单族里按命令名、描述与菜单路径搜索，不区分大小写（只折叠 ASCII）。每条命中都带列举端点那套状态字段，不用再调一次就能判断可否执行。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `query` | `string` | 是 | 在名称、描述与路径里查找的文本；忽略 ASCII 大小写。不能为空。 |
| `expandDynamic` | `boolean` | 否 | 是否连从动态子菜单展开的命令一起搜索。默认 `true`。 |
| `scope` | `"all" \| "mainmenu" \| "contextmenu"` | 否 | 搜索哪个菜单族。默认 `"all"`。 |
| `includeHidden` | `boolean` | 否 | 是否连宿主不会显示的条目一起搜索。默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `query` | `string` | 传入的查询。 |
| `results` | `DiscoverySearchResult[]` | 命中列表，主菜单在前。 |
| `results[].enabled` | `boolean` | 是否可点击。`stateKnown` 为 false 时无意义。 |
| `results[].checked` | `boolean` | 是否带勾选标记（单选标记也算）。 |
| `results[].radioChecked` | `boolean` | 勾选标记是否为单选样式。 |
| `results[].hidden` | `boolean` | 宿主是否不会绘制该条目。 |
| `results[].stateKnown` | `boolean` | `enabled` 与 `checked` 是否真正观测到。无法评估时为 false，例如没有选中或播放曲目时的右键菜单项。 |
| `results[].flags` | `integer` | 组件报告的原始显示位。主菜单：`1` 禁用、`2` 勾选、`4` 单选、`8` 默认隐藏。右键菜单：`1` 勾选、`2` 禁用、`4` 灰显、`8` 单选。 |
| `results[].name` | `string` | 宿主报告的标签。 |
| `results[].description` | `string` | 描述；组件没有时为空。 |
| `results[].guid` | `string` | 命令 GUID；动态子项是其所属命令的 GUID。 |
| `results[].path` | `string` | 斜杠分隔的标签路径。右键菜单项是扁平注册的，路径就是标签。 |
| `results[].isDynamic` | `boolean` | 命中是否从动态子菜单展开而来。 |
| `results[].type` | `"mainmenu" \| "contextmenu"` | 命中属于哪个菜单族。 |
| `results[].subGuid` | `string` | 动态子节点的 GUID；执行时与 `guid` 一起传给 `discovery.executeMainMenuCommand`。 |
| `results[].source` | `"mainmenu_static" \| "mainmenu_dynamic" \| "contextmenu_static" \| "contextmenu_dynamic" \| "hmenu_fallback"` | 条目的来源。 |
| `results[].executable` | `boolean` | 能否执行。 |
| `results[].unaddressableReason` | `"" \| "separator" \| "dynamicParent" \| "noStableIdentifier" \| "emptyNode"` | 不能执行的原因；能执行时为空串。 |
| `count` | `integer` | `results` 的条数。 |
| `expandDynamic` | `boolean` | 实际生效的 `expandDynamic`。 |
| `scope` | `"all" \| "mainmenu" \| "contextmenu"` | 实际生效的 `scope`。 |
| `includeHidden` | `boolean` | 实际生效的 `includeHidden`。 |
| `mainMenuHits` | `integer` | 来自主菜单的命中数。 |
| `contextMenuHits` | `integer` | 来自右键菜单的命中数。 |
| `stateKnown` | `boolean` | 右键菜单状态是否真正观测到。没有选中或播放曲目时搜右键菜单为 false，此时命中的 `enabled` 与 `checked` 不能用来过滤。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

动态父槽位会被跳过，可执行的身份在展开出的子项上；来自动态子菜单的命中带 `subGuid`，执行时与 `guid` 一起传给 `discovery.executeMainMenuCommand`。右键菜单条目是扁平注册、由宿主决定位置，因此其 `path` 即标签本身。

```javascript
// 搜索并执行命令（动态子命令需要带上 subGuid）
const result = await fb2k.invoke('discovery.searchCommands', { query: 'lyric' });
if (result.success === false) throw new Error(result.error);
const hit = result.results[0];
if (hit) {
    await fb2k.invoke('discovery.executeMainMenuCommand',
        hit.subGuid ? { guid: hit.guid, subGuid: hit.subGuid } : { guid: hit.guid });
}
```

## 右键菜单

### discovery.getContextMenuCommands

<!-- api-schema:begin discovery.getContextMenuCommands -->
列出已注册的右键菜单命令（扁平），带针对当前目标的状态。`enabled` 与 `checked` 只有在有选中或正在播放的曲目时才可观测，先看 `stateKnown`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `includeHidden` | `boolean` | 否 | 是否把宿主不会显示的条目（`FORCE_OFF`，只出现在快捷键列表）也列出。默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `commands` | `DiscoveryContextMenuCommand[]` | 命令列表。 |
| `commands[].enabled` | `boolean` | 是否可点击。`stateKnown` 为 false 时无意义。 |
| `commands[].checked` | `boolean` | 是否带勾选标记（单选标记也算）。 |
| `commands[].radioChecked` | `boolean` | 勾选标记是否为单选样式。 |
| `commands[].hidden` | `boolean` | 宿主是否不会绘制该条目。 |
| `commands[].stateKnown` | `boolean` | `enabled` 与 `checked` 是否真正观测到。无法评估时为 false，例如没有选中或播放曲目时的右键菜单项。 |
| `commands[].flags` | `integer` | 组件报告的原始显示位。主菜单：`1` 禁用、`2` 勾选、`4` 单选、`8` 默认隐藏。右键菜单：`1` 勾选、`2` 禁用、`4` 灰显、`8` 单选。 |
| `commands[].name` | `string` | 宿主报告的标签。 |
| `commands[].description` | `string` | 组件提供的描述；没有时为空。 |
| `commands[].guid` | `string` | 命令 GUID。 |
| `commands[].parentGuid` | `string` | 上级分组的 GUID；没有分组的服务为空 GUID。 |
| `commands[].index` | `integer` | 命令在其服务中的序号。 |
| `commands[].source` | `"mainmenu_static" \| "mainmenu_dynamic" \| "contextmenu_static" \| "contextmenu_dynamic" \| "hmenu_fallback"` | 条目的来源。 |
| `commands[].executable` | `boolean` | 能否执行。 |
| `commands[].unaddressableReason` | `"" \| "separator" \| "dynamicParent" \| "noStableIdentifier" \| "emptyNode"` | 不能执行的原因；能执行时为空串。 |
| `count` | `integer` | `commands` 的条数。 |
| `includeHidden` | `boolean` | 实际生效的 `includeHidden`。 |
| `hiddenFiltered` | `integer` | 过滤去掉的条数；`includeHidden` 时为 `0`。 |
| `stateKnown` | `boolean` | `enabled` 与 `checked` 是否真正观测到：只有有选中或播放曲目时为 true。 |
| `selectionCount` | `integer` | 状态是针对多少首曲目评估的。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

`enabled` / `checked` 只有在有选中曲目或正在播放时才可观测——SDK 的 `item_get_display_data_root()` 需要一个 `metadb_handle_list`。因此请先看响应的 `stateKnown`：为 `false` 时这两个字段不构成观测，只有 `hidden` 仍然有意义（`FORCE_OFF` 是条目的固有属性，与选中无关）。

### discovery.executeContextMenuCommand

<!-- api-schema:begin discovery.executeContextMenuCommand -->
按 GUID 对正在播放的曲目执行右键菜单命令，没有播放时对活动播放列表的选中项执行。宿主永不绘制的命令（`FORCE_OFF`）以 `NOT_SUPPORTED` 拒绝，除非传 `force`；没有目标时是 `NO_ACTIVE_ITEM`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `guid` | `string` | 是 | `{...}` 形式的命令 GUID，取自 `discovery.getContextMenuCommands`。不能为空。 |
| `force` | `boolean` | 否 | 即使宿主永不绘制该命令也照样派发。默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `guid` | `string` | 执行的 GUID。 |
| `name` | `string` | 命令的标签。 |
| `hidden` | `boolean` | 宿主是否永不绘制该命令（`FORCE_OFF`）。 |
| `resolved` | `boolean` | 是否有注册项拥有该 GUID。 |
| `force` | `boolean` | 实际生效的 `force`。 |
| `itemCount` | `integer` | 命令作用于多少首曲目。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

`FORCE_OFF` 的命令会被拒绝而非派发：SDK 将该状态定义为「仅出现在快捷键列表」，宿主从不绘制它，执行等于做了一件用户根本点不到的事。拒绝以 `NOT_SUPPORTED` 失败，附带 `hidden: true`、`name`、`resolved` 与 `force`。`DEFAULT_OFF`（按 Shift 才显示）仍可到达，永不拒绝。没有注册项拥有该 GUID 时以 `NOT_FOUND` 失败并附带 `resolved: false`。

`hidden` 与 `resolved` 在拒绝与成功两条路径上都会返回，因此「未被拒绝」与「本版本不返回该字段」可以区分。

### discovery.executeContextMenuByPath

<!-- api-schema:begin discovery.executeContextMenuByPath -->
按斜杠分隔的标签路径执行右键菜单命令，如 `Playback Statistics/Rating/5`。每一段都要在规范化后（忽略助记符 `&`、末尾省略号、快捷键文本与 ASCII 大小写）恰好匹配一个标签。匹配到多个命令以 `INVALID_PARAMS` 拒绝并列出候选；一个都没有是 `NOT_FOUND`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 斜杠分隔的命令标签路径，如 `Playback Statistics/Rating/5`。不能为空。 |
| `trackPath` | `string` | 否 | 对这首曲目执行，而不是当前目标。带 `\|subsong:N` 后缀时选子曲目，不带时指文件的第一首。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `path` | `string` | 传入的路径。 |
| `foundName` | `string` | 实际执行的命令的完整名。 |
| `match` | `"unique"` | 成功时恒为 `unique`：恰好匹配到一个命令。 |
| `candidateCount` | `integer` | 路径匹配到的命令数；成功时为 `1`。 |
| `itemCount` | `integer` | 命令作用于多少首曲目。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

匹配不是子串匹配，`Rating/1` 不会落到 `Rating/10`。路径匹配到多个命令时拒绝执行而不是猜一个（真实宿主里重名标签很常见）：失败附带 `match: "ambiguous"`、`candidateCount` 与完整名列表 `candidates`，据此细化路径，或改用 `discovery.executeContextMenuCommand` 按 GUID 执行——GUID 才是稳定标识。一个都匹配不到以 `NOT_FOUND` 失败并附带 `match: "notFound"`。

### discovery.getContextMenuTree

<!-- api-schema:begin discovery.getContextMenuTree -->
把当前目标的右键菜单整棵导出为嵌套树，保留子菜单与分隔符。深度与每个节点的子项数有上限，任何裁剪都通过 `truncated` 系列标记报告，不会静默。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `tree` | `DiscoveryContextMenuTreeNode` | 根节点。 |
| `tree.name` | `string` | 宿主报告的标签。 |
| `tree.type` | `"command" \| "popup" \| "separator" \| "unknown"` | 节点类型。 |
| `tree.depth` | `integer` | 相对根节点的深度，根为 `0`。 |
| `tree.enabled` | `boolean` | 是否可点击。分隔符没有。 |
| `tree.checked` | `boolean` | 是否带勾选标记。分隔符没有。 |
| `tree.radioChecked` | `boolean` | 勾选标记是否为单选样式。分隔符没有。 |
| `tree.hidden` | `boolean` | 宿主是否不会绘制该条目；这里恒为 false，因为树本来就按宿主显示的内容构建。分隔符没有。 |
| `tree.stateKnown` | `boolean` | 状态是否真正观测到；这里恒为 true。分隔符没有。 |
| `tree.flags` | `integer` | 原始显示位。分隔符没有。 |
| `tree.fullName` | `string` | 完整的斜杠分隔名；只有 `command` 节点有。 |
| `tree.childCount` | `integer` | 宿主的真实子项数；只有 `popup` 节点有。 |
| `tree.childrenReturned` | `integer` | 本次响应里包含的子项数；只有 `popup` 节点有。 |
| `tree.children` | `DiscoveryContextMenuTreeNode[]` | 子项，按菜单顺序；只有 `popup` 节点有。 |
| `tree.truncated` | `boolean` | 该节点之下是否有内容被裁剪。 |
| `tree.depthExceeded` | `boolean` | 该节点之下是否因深度上限被裁剪。 |
| `tree.childrenExceeded` | `boolean` | 该节点之下是否因每节点子项上限被裁剪。 |
| `truncated` | `boolean` | 树里是否有内容被裁剪。 |
| `depthExceeded` | `boolean` | 是否因深度上限裁剪。 |
| `childrenExceeded` | `boolean` | 是否因每节点子项上限裁剪。 |
| `maxDepth` | `integer` | 生效的深度上限。 |
| `maxChildrenPerNode` | `integer` | 生效的每节点子项上限。 |
| `itemCount` | `integer` | 菜单是为多少首曲目构建的。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

截断不会静默：`popup` 节点同时给出 `childCount`（宿主的真实子项数）与 `childrenReturned`（本次响应实际包含的数量），无需自行数数组即可对账。子树被裁剪的节点带 `truncated`，并由 `depthExceeded` / `childrenExceeded` 区分原因；标记会向上传播，因此顶层 `truncated` 覆盖整棵树。`maxDepth` 与 `maxChildrenPerNode` 回显本次生效的上限。

## 发现范围与执行规则

- 返回结果枚举当前 foobar2000 进程中已注册的服务；计数与名称会随已安装组件和 host 配置而变化。
- `discovery.executeMainMenuCommand` 与 `discovery.executeContextMenuCommand` 要求格式合法的 GUID：格式非法以 `INVALID_PARAMS` 失败，没有命令拥有它以 `NOT_FOUND` 失败。右键菜单命令优先作用于正在播放曲目，否则作用于活动播放列表选中项；两者都没有时以 `NO_ACTIVE_ITEM` 失败。
- `discovery.executeContextMenuByPath` 要求 `path`；可选 `trackPath` 受媒体读取安全策略保护。省略时，runtime 使用相同的正在播放/选中项回退逻辑。
- `discovery.getContextMenuTree` 是诊断输出，需要活动目标曲目；递归深度与每节点子项数均有上限，任何裁剪都通过 `truncated` / `depthExceeded` / `childrenExceeded` 显式上报，生效上限由 `maxDepth` 与 `maxChildrenPerNode` 回显。
- `discovery.searchCommands` 要求非空 `query`，按不区分大小写匹配主菜单与右键菜单两侧的命令名称、描述与菜单路径。大小写折叠仅作用于 ASCII，UTF-8 多字节序列（如无大小写可折叠的中文标签）原样通过。`scope` 只接受 `all`、`mainmenu`、`contextmenu`，其他取值以 `INVALID_PARAMS` 失败。
