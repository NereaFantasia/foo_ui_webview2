# Discovery 服务发现

`discovery` 命名空间提供主动枚举 foobar2000 系统中已注册服务的能力（v1.1.3+），涵盖菜单命令、格式、UI 元素、DSP、输出设备与首选项页面。

> 与 PluginRegistry 的被动注册模式不同，Discovery API 主动枚举当前宿主进程内注册的所有服务条目。

## 服务发现

### discovery.getAllServices

<!-- api-schema:begin discovery.getAllServices -->
统计当前进程中可发现的各类服务数量（涵盖主菜单命令、右键菜单命令、菜单组、输入格式、UI 元素、DSP、输出后端、首选项页与组件；菜单统计与列举端点复用同一枚举遍历，仅计入宿主界面实际显示的条目）

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `services` | `DiscoveryServiceCounts` | 各类服务的统计数量 |
| `services.mainMenuCommands` | `integer` | 宿主界面会显示的主菜单命令数（含动态子项） |
| `services.mainMenuDynamicCommands` | `integer` | 主菜单命令中自动态子菜单展开的子项数量 |
| `services.mainMenuGroups` | `integer` | 主菜单分组总数 |
| `services.contextMenuCommands` | `integer` | 宿主界面会显示的右键菜单命令总数 |
| `services.inputFormats` | `integer` | 可播放音频输入格式总数 |
| `services.uiElements` | `integer` | 已注册的 UI 元素总数 |
| `services.dspEntries` | `integer` | 已注册的 DSP 条目总数 |
| `services.outputDevices` | `integer` | 已注册的音频输出后端总数 |
| `services.preferencePages` | `integer` | 首选项设置页面总数 |
| `services.components` | `integer` | 已安装组件总数 |
| `contextMenuHiddenFiltered` | `integer` | 因宿主不显示而被过滤排除的右键菜单项总数 |
| `stateKnown` | `boolean` | 右键菜单状态是否可观测（无选中或未播放曲目时为 false） |
| `totalServices` | `integer` | 各类服务总量（不重复计算已计入 mainMenuCommands 的动态子项） |

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
列出当前安装的所有组件信息

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `components` | `DiscoveryComponentInfo[]` | 已安装组件列表 |
| `components[].filename` | `string` | 组件 DLL 文件名 |
| `components[].name` | `string` | 组件名称 |
| `components[].version` | `string` | 组件报告的版本字符串 |
| `components[].about` | `string` | 组件关于说明文本 |
| `count` | `integer` | components 列表条目总数 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### discovery.getInputFormats

<!-- api-schema:begin discovery.getInputFormats -->
列出可播放的输入文件格式列表（包含显示名称与文件名掩码）

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `fileTypes` | `DiscoveryInputFormatType[]` | 可播放音频输入格式列表 |
| `fileTypes[].name` | `string` | 格式显示名称（例如 FLAC） |
| `fileTypes[].mask` | `string` | 文件名匹配掩码（例如 *.FLAC） |
| `fileTypes[].index` | `integer` | 在注册服务内部的格式序号 |
| `count` | `integer` | fileTypes 列表条目总数 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### discovery.getUIElements

<!-- api-schema:begin discovery.getUIElements -->
列出已注册的 UI 元素列表

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `elements` | `DiscoveryUIElementInfo[]` | 已注册 UI 元素列表 |
| `elements[].guid` | `string` | 元素 GUID |
| `elements[].subclassGuid` | `string` | 元素子类 GUID |
| `elements[].name` | `string` | 元素显示名称 |
| `elements[].description` | `string` | 元素功能描述 |
| `elements[].isUserAddable` | `boolean` | 用户是否可将该元素添加至界面布局 |
| `count` | `integer` | elements 列表条目总数 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### discovery.getDspEntries

<!-- api-schema:begin discovery.getDspEntries -->
列出已注册的 DSP 条目列表

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `entries` | `DiscoveryDspEntryInfo[]` | 已注册 DSP 条目列表 |
| `entries[].guid` | `string` | DSP 唯一标识 GUID |
| `entries[].name` | `string` | DSP 显示名称 |
| `count` | `integer` | entries 列表条目总数 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### discovery.getOutputDevices

<!-- api-schema:begin discovery.getOutputDevices -->
列出已注册的音频输出后端设备列表

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `devices` | `DiscoveryOutputDeviceEntry[]` | 已注册音频输出后端设备列表 |
| `devices[].guid` | `string` | 输出后端 GUID |
| `count` | `integer` | devices 列表条目总数 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### discovery.getPreferencePages

<!-- api-schema:begin discovery.getPreferencePages -->
列出首选项设置对话框的页面层级列表

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `pages` | `DiscoveryPreferencePageInfo[]` | 首选项设置页面层级列表 |
| `pages[].guid` | `string` | 页面唯一标识 GUID |
| `pages[].parentGuid` | `string` | 上级父页面 GUID |
| `pages[].name` | `string` | 页面显示名称 |
| `count` | `integer` | pages 列表条目总数 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

## 主菜单

### discovery.getMainMenuCommands

<!-- api-schema:begin discovery.getMainMenuCommands -->
扁平列出已注册的主菜单命令（附带标签路径、GUID 与界面显示状态；默认展开组件在运行时动态构建的子菜单 mainmenu_commands_v2，子命令与父级槽位并列呈现）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `expandDynamic` | `boolean` | 否 | 是否展开组件在运行时动态构建的子菜单（传入 false 仅列出静态注册表）默认 `true`。 |
| `includeHidden` | `boolean` | 否 | 是否包含宿主界面通常不显示的隐藏菜单项默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `commands` | `DiscoveryMainMenuCommand[]` | 主菜单命令列表（每个服务内优先排列静态槽位，后随动态展开子项） |
| `commands[].enabled` | `boolean` | 菜单项是否可点击（stateKnown 为 false 时无参考意义） |
| `commands[].checked` | `boolean` | 是否显示勾选标记（含单选勾选标记） |
| `commands[].radioChecked` | `boolean` | 勾选标记是否为单选圆点样式 |
| `commands[].hidden` | `boolean` | 宿主界面是否不绘制该菜单项 |
| `commands[].stateKnown` | `boolean` | enabled 与 checked 状态是否已成功观测（无选中曲目或未播放时评估失败返回 false） |
| `commands[].flags` | `integer` | 组件报告的原始显示标志位（主菜单：1 禁用、2 勾选、4 单选、8 默认隐藏；右键菜单：1 勾选、2 禁用、4 灰显、8 单选） |
| `commands[].name` | `string` | 宿主报告的命令标签（在汉化/多语言版本中为本地化文本） |
| `commands[].description` | `string` | 组件提供的命令描述（无描述时为空字符串） |
| `commands[].guid` | `string` | 命令 GUID（对于动态子菜单展开项，此项为所属父命令 GUID） |
| `commands[].parentGuid` | `string` | 命令所属分组 GUID |
| `commands[].index` | `integer` | 命令在其所属服务中的索引序号 |
| `commands[].path` | `string` | 斜杠分隔的标签路径（顶级静态槽位直接为命令标签） |
| `commands[].isDynamic` | `boolean` | 是否属于动态子菜单（包含父级槽位与展开子项） |
| `commands[].isDynamicParent` | `boolean` | 是否为动态子菜单的父级槽位（仅作为菜单容器，本身不可执行） |
| `commands[].subGuid` | `string` | 动态子菜单项的专属 GUID（执行动态子命令时与 guid 一同传入；静态条目缺省） |
| `commands[].source` | `"mainmenu_static" \| "mainmenu_dynamic" \| "contextmenu_static" \| "contextmenu_dynamic" \| "hmenu_fallback"` | 菜单项的来源分类 |
| `commands[].executable` | `boolean` | 菜单项是否可被调用执行 |
| `commands[].unaddressableReason` | `"" \| "separator" \| "dynamicParent" \| "noStableIdentifier" \| "emptyNode"` | 菜单项不可执行的具体原因（可执行时为空字符串） |
| `count` | `integer` | commands 列表条目总数 |
| `expandDynamic` | `boolean` | 实际生效的 expandDynamic 参数值 |
| `includeHidden` | `boolean` | 实际生效的 includeHidden 参数值 |
| `dynamicCount` | `integer` | 从动态子菜单中展开出的条目数量 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

默认会展开 `mainmenu_commands_v2` 的动态子菜单（ESLyric 等 SMP 老组件常用）。父命令槽位为 `isDynamicParent: true`，仅作容器、本身不可执行；展开出来的条目带 `isDynamic: true` 与 `subGuid`，执行时必须把 `subGuid` 一起传给 `discovery.executeMainMenuCommand`。

### discovery.getMainMenuGroups

<!-- api-schema:begin discovery.getMainMenuGroups -->
列出主菜单命令所属的分组列表

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `groups` | `DiscoveryMainMenuGroup[]` | 主菜单分组列表 |
| `groups[].guid` | `string` | 分组 GUID |
| `groups[].parentGuid` | `string` | 上级分组 GUID |
| `groups[].name` | `string` | 分组显示名称（非弹出菜单分组为空字符串） |
| `groups[].sortPriority` | `integer` | 在上级分组内的排序优先级权重 |
| `count` | `integer` | groups 列表分组总数 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### discovery.executeMainMenuCommand

<!-- api-schema:begin discovery.executeMainMenuCommand -->
按 GUID 执行主菜单命令（动态子项须同时提供父命令 guid 与自身 subGuid，且必须唯一定位至具体叶子项；动态容器不可执行；命令不存在返回 NOT_FOUND，定位产生歧义返回 MENU_MATCH_AMBIGUOUS，菜单项禁用返回 MENU_ITEM_DISABLED，状态不可读返回 OPERATION_FAILED）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `guid` | `string` | 是 | {...} 格式的命令 GUID（动态子项传入其所属父命令 GUID）不能为空。 |
| `subGuid` | `string` | 否 | 动态子项专属 subGuid（取自 getMainMenuCommands；空字符串视为未指定） |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `guid` | `string` | 执行的目标命令 GUID |
| `subGuid` | `string` | 执行的动态子项 subGuid（仅在动态执行路径返回） |
| `dynamic` | `boolean` | 是否通过动态子菜单路径执行 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

`NOT_FOUND` 失败时回带 `guid`、`dynamic`，走动态路径时还带 `subGuid`。

### discovery.searchCommands

<!-- api-schema:begin discovery.searchCommands -->
跨主菜单与右键菜单搜索命令名称、描述与路径（不区分 ASCII 大小写；搜索命中直接携带完整菜单状态字段，无需二次调用即可判断可执行性）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `query` | `string` | 是 | 搜索关键词文本（在名称、描述与路径中匹配，忽略 ASCII 大小写）不能为空。 |
| `expandDynamic` | `boolean` | 否 | 是否同时搜索从动态子菜单中展开的子命令默认 `true`。 |
| `scope` | `"all" \| "mainmenu" \| "contextmenu"` | 否 | 搜索的目标菜单族范围默认 `"all"`。 |
| `includeHidden` | `boolean` | 否 | 是否同时搜索宿主界面通常不绘制的隐藏菜单项默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `query` | `string` | 搜索查询关键词 |
| `results` | `DiscoverySearchResult[]` | 命令搜索命中列表（主菜单命令优先排列） |
| `results[].enabled` | `boolean` | 菜单项是否可点击（stateKnown 为 false 时无参考意义） |
| `results[].checked` | `boolean` | 是否显示勾选标记（含单选勾选标记） |
| `results[].radioChecked` | `boolean` | 勾选标记是否为单选圆点样式 |
| `results[].hidden` | `boolean` | 宿主界面是否不绘制该菜单项 |
| `results[].stateKnown` | `boolean` | enabled 与 checked 状态是否已成功观测（无选中曲目或未播放时评估失败返回 false） |
| `results[].flags` | `integer` | 组件报告的原始显示标志位（主菜单：1 禁用、2 勾选、4 单选、8 默认隐藏；右键菜单：1 勾选、2 禁用、4 灰显、8 单选） |
| `results[].name` | `string` | 宿主报告的命令标签 |
| `results[].description` | `string` | 命令功能描述（无描述时为空字符串） |
| `results[].guid` | `string` | 命令 GUID（动态子项为其所属父命令 GUID） |
| `results[].path` | `string` | 斜杠分隔的标签路径（右键菜单项为扁平注册，路径即为自身标签） |
| `results[].isDynamic` | `boolean` | 搜索命中项是否自动态子菜单展开 |
| `results[].type` | `"mainmenu" \| "contextmenu"` | 命令所属菜单族（mainmenu 或 contextmenu） |
| `results[].subGuid` | `string` | 动态子节点 GUID（执行动态主菜单命令时与 guid 一同传入） |
| `results[].source` | `"mainmenu_static" \| "mainmenu_dynamic" \| "contextmenu_static" \| "contextmenu_dynamic" \| "hmenu_fallback"` | 菜单项的来源分类 |
| `results[].executable` | `boolean` | 菜单项是否可被调用执行 |
| `results[].unaddressableReason` | `"" \| "separator" \| "dynamicParent" \| "noStableIdentifier" \| "emptyNode"` | 菜单项不可执行的具体原因（可执行时为空字符串） |
| `count` | `integer` | results 列表命中总数 |
| `expandDynamic` | `boolean` | 实际生效的 expandDynamic 参数值 |
| `scope` | `"all" \| "mainmenu" \| "contextmenu"` | 实际生效的 scope 搜索范围 |
| `includeHidden` | `boolean` | 实际生效的 includeHidden 参数值 |
| `mainMenuHits` | `integer` | 主菜单命令命中数量 |
| `contextMenuHits` | `integer` | 右键菜单命令命中数量 |
| `stateKnown` | `boolean` | 右键菜单状态是否可观测（无选中或未播放曲目时为 false；此时命中项的 enabled 与 checked 不可用于条件过滤） |

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
扁平列出已注册的右键菜单命令及针对当前目标的状态（enabled 与 checked 仅在存在选中或正在播放的曲目时可观测，使用前须核对 stateKnown）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `includeHidden` | `boolean` | 否 | 是否包含宿主界面通常不绘制的隐藏菜单项（FORCE_OFF，仅出现在快捷键配置列表）默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `commands` | `DiscoveryContextMenuCommand[]` | 右键菜单命令列表 |
| `commands[].enabled` | `boolean` | 菜单项是否可点击（stateKnown 为 false 时无参考意义） |
| `commands[].checked` | `boolean` | 是否显示勾选标记（含单选勾选标记） |
| `commands[].radioChecked` | `boolean` | 勾选标记是否为单选圆点样式 |
| `commands[].hidden` | `boolean` | 宿主界面是否不绘制该菜单项 |
| `commands[].stateKnown` | `boolean` | enabled 与 checked 状态是否已成功观测（无选中曲目或未播放时评估失败返回 false） |
| `commands[].flags` | `integer` | 组件报告的原始显示标志位（主菜单：1 禁用、2 勾选、4 单选、8 默认隐藏；右键菜单：1 勾选、2 禁用、4 灰显、8 单选） |
| `commands[].name` | `string` | 宿主报告的命令标签 |
| `commands[].description` | `string` | 组件提供的命令描述（无描述时为空字符串） |
| `commands[].guid` | `string` | 命令 GUID |
| `commands[].parentGuid` | `string` | 上级分组 GUID（无所属分组时为空 GUID） |
| `commands[].index` | `integer` | 命令在其所属服务中的索引序号 |
| `commands[].source` | `"mainmenu_static" \| "mainmenu_dynamic" \| "contextmenu_static" \| "contextmenu_dynamic" \| "hmenu_fallback"` | 菜单项的来源分类 |
| `commands[].executable` | `boolean` | 菜单项是否可被调用执行 |
| `commands[].unaddressableReason` | `"" \| "separator" \| "dynamicParent" \| "noStableIdentifier" \| "emptyNode"` | 菜单项不可执行的具体原因（可执行时为空字符串） |
| `count` | `integer` | commands 列表命令总数 |
| `includeHidden` | `boolean` | 实际生效的 includeHidden 参数值 |
| `hiddenFiltered` | `integer` | 因宿主不显示而被过滤排除的条目数（includeHidden 为 true 时为 0） |
| `stateKnown` | `boolean` | enabled 与 checked 状态是否已成功观测（仅在有选中或正在播放曲目时为 true） |
| `selectionCount` | `integer` | 评估菜单状态所针对的目标曲目数量 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

`enabled` / `checked` 只有在有选中曲目或正在播放时才可观测——SDK 的 `item_get_display_data_root()` 需要一个 `metadb_handle_list`。因此请先看响应的 `stateKnown`：为 `false` 时这两个字段不构成观测，只有 `hidden` 仍然有意义（`FORCE_OFF` 是条目的固有属性，与选中无关）。

### discovery.executeContextMenuCommand

<!-- api-schema:begin discovery.executeContextMenuCommand -->
按 GUID 对当前目标曲目执行右键菜单命令（优先作用于正在播放的曲目，未播放时作用于活动播放列表的选中项；宿主永不绘制的命令 FORCE_OFF 默认以 NOT_SUPPORTED 拒绝，除非显式指定 force 为 true；无可用目标曲目时返回 NO_ACTIVE_ITEM）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `guid` | `string` | 是 | {...} 格式的命令 GUID（取自 discovery.getContextMenuCommands）不能为空。 |
| `force` | `boolean` | 否 | 是否强制派发宿主永不绘制的隐藏命令（FORCE_OFF）默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `guid` | `string` | 执行的目标命令 GUID |
| `name` | `string` | 执行的命令标签 |
| `hidden` | `boolean` | 宿主界面是否永不绘制该命令（FORCE_OFF） |
| `resolved` | `boolean` | 是否成功匹配到拥有该 GUID 的注册命令 |
| `force` | `boolean` | 实际生效的 force 参数值 |
| `itemCount` | `integer` | 该命令实际作用的目标曲目数量 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

`FORCE_OFF` 的命令会被拒绝而非派发：SDK 将该状态定义为「仅出现在快捷键列表」，宿主从不绘制它，执行等于做了一件用户根本点不到的事。拒绝以 `NOT_SUPPORTED` 失败，附带 `hidden: true`、`name`、`resolved` 与 `force`。`DEFAULT_OFF`（按 Shift 才显示）仍可到达，永不拒绝。没有注册项拥有该 GUID 时以 `NOT_FOUND` 失败并附带 `resolved: false`。

`hidden` 与 `resolved` 在拒绝与成功两条路径上都会返回，因此「未被拒绝」与「本版本不返回该字段」可以区分。

### discovery.executeContextMenuByPath

<!-- api-schema:begin discovery.executeContextMenuByPath -->
按斜杠分隔的标签路径执行右键菜单命令（如 Playback Statistics/Rating/5；各级路径在规范化后须唯一匹配一个菜单标签，忽略助记符 &、末尾省略号、快捷键文本与 ASCII 大小写；命中多个候选时返回 INVALID_PARAMS 并列出歧义候选，未命中任何命令返回 NOT_FOUND）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 斜杠分隔的命令标签路径（例如 Playback Statistics/Rating/5）不能为空。 |
| `trackPath` | `string` | 否 | 显式指定执行的目标曲目路径（省略时取当前播放或选中的曲目；支持 \|subsong:N 子曲目后缀，缺省取首个子曲目） |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `path` | `string` | 调用的输入命令路径 |
| `foundName` | `string` | 实际匹配并执行的完整命令路径名称 |
| `match` | `"unique"` | 路径匹配模式（成功执行时恒为 unique，表示唯一精确匹配） |
| `candidateCount` | `integer` | 路径匹配命中的候选命令数量（成功时恒为 1） |
| `itemCount` | `integer` | 该命令实际作用的目标曲目数量 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

匹配不是子串匹配，`Rating/1` 不会落到 `Rating/10`。路径匹配到多个命令时拒绝执行而不是猜一个（真实宿主里重名标签很常见）：失败附带 `match: "ambiguous"`、`candidateCount` 与完整名列表 `candidates`，据此细化路径，或改用 `discovery.executeContextMenuCommand` 按 GUID 执行——GUID 才是稳定标识。一个都匹配不到以 `NOT_FOUND` 失败并附带 `match: "notFound"`。

### discovery.getContextMenuTree

<!-- api-schema:begin discovery.getContextMenuTree -->
导出当前目标曲目的完整右键菜单嵌套树（保留各级子菜单与分隔符；树深度与单节点子项数设有限额，任何裁剪均通过 truncated 状态显式标注）

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `tree` | `DiscoveryContextMenuTreeNode` | 右键菜单树的根节点 |
| `tree.name` | `string` | 宿主报告的节点标签 |
| `tree.type` | `"command" \| "popup" \| "separator" \| "unknown"` | 菜单节点类型 |
| `tree.depth` | `integer` | 相对根节点的嵌套深度（根节点为 0） |
| `tree.enabled` | `boolean` | 菜单项是否可点击（分隔符节点缺省） |
| `tree.checked` | `boolean` | 是否显示勾选标记（分隔符节点缺省） |
| `tree.radioChecked` | `boolean` | 勾选标记是否为单选样式（分隔符节点缺省） |
| `tree.hidden` | `boolean` | 宿主界面是否不绘制该项（从宿主实际显示构建时恒为 false；分隔符节点缺省） |
| `tree.stateKnown` | `boolean` | 状态是否已成功观测（树节点恒为 true；分隔符节点缺省） |
| `tree.flags` | `integer` | 组件原始显示标志位（分隔符节点缺省） |
| `tree.fullName` | `string` | 斜杠分隔的完整命令路径（仅 command 节点提供） |
| `tree.childCount` | `integer` | 宿主中的实际子菜单项总数（仅 popup 节点提供） |
| `tree.childrenReturned` | `integer` | 当前响应实际返回的子项数量（仅 popup 节点提供） |
| `tree.children` | `DiscoveryContextMenuTreeNode[]` | 按菜单顺序排列的子项列表（仅 popup 节点提供） |
| `tree.truncated` | `boolean` | 该节点下属内容是否发生截断裁剪 |
| `tree.depthExceeded` | `boolean` | 该节点下属内容是否因超出深度限额而被截断 |
| `tree.childrenExceeded` | `boolean` | 该节点下属内容是否因超出单节点子项限额而被截断 |
| `truncated` | `boolean` | 菜单树是否存在截断裁剪 |
| `depthExceeded` | `boolean` | 是否因超出最大树深度限额而被截断 |
| `childrenExceeded` | `boolean` | 是否因超出单节点子项限额而被截断 |
| `maxDepth` | `integer` | 实际生效的最大树深度限额 |
| `maxChildrenPerNode` | `integer` | 实际生效的单节点子项数量限额 |
| `itemCount` | `integer` | 构建菜单树所针对的目标曲目数量 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

截断不会静默：`popup` 节点同时给出 `childCount`（宿主的真实子项数）与 `childrenReturned`（本次响应实际包含的数量），无需自行数数组即可对账。子树被裁剪的节点带 `truncated`，并由 `depthExceeded` / `childrenExceeded` 区分原因；标记会向上传播，因此顶层 `truncated` 覆盖整棵树。`maxDepth` 与 `maxChildrenPerNode` 回显本次生效的上限。

## 发现范围与执行规则

- 返回结果枚举当前 foobar2000 进程中已注册的服务；计数与名称会随已安装组件和 host 配置而变化。
- `discovery.executeMainMenuCommand` 与 `discovery.executeContextMenuCommand` 要求格式合法的 GUID：格式非法以 `INVALID_PARAMS` 失败，没有命令拥有它以 `NOT_FOUND` 失败。右键菜单命令优先作用于正在播放曲目，否则作用于活动播放列表选中项；两者都没有时以 `NO_ACTIVE_ITEM` 失败。
- `discovery.executeContextMenuByPath` 要求 `path`；可选 `trackPath` 受媒体读取安全策略保护。省略时，runtime 使用相同的正在播放/选中项回退逻辑。
- `discovery.getContextMenuTree` 是诊断输出，需要活动目标曲目；递归深度与每节点子项数均有上限，任何裁剪都通过 `truncated` / `depthExceeded` / `childrenExceeded` 显式上报，生效上限由 `maxDepth` 与 `maxChildrenPerNode` 回显。
- `discovery.searchCommands` 要求非空 `query`，按不区分大小写匹配主菜单与右键菜单两侧的命令名称、描述与菜单路径。大小写折叠仅作用于 ASCII，UTF-8 多字节序列（如无大小写可折叠的中文标签）原样通过。`scope` 只接受 `all`、`mainmenu`、`contextmenu`，其他取值以 `INVALID_PARAMS` 失败。
