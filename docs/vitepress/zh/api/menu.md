# Menu 菜单 API

（v1.2.0+）提供主菜单和上下文菜单的执行和查询。

SDK 定义的地址见[常用命令 GUID](../sdk/menu.md#standard-command-guids)，各执行入口的限制见[操作目标与完成结果](../sdk/menu.md#command-targets)。SDK 调用与 `fb2k.invoke` 使用相同的地址。

## Menu API - 菜单

### menu.runMainMenuCommand

<!-- api-schema:begin menu.runMainMenuCommand -->
运行主菜单命令，按 GUID、叶子名或斜杠分隔的路径寻址。命令被禁用时以 `MENU_ITEM_DISABLED` 失败；名称匹配到多条命令时以 `MENU_MATCH_AMBIGUOUS` 失败并在 `candidates` 里列出；名称一条都没匹配到时以 `MENU_COMMAND_NOT_FOUND` 失败，没有命令拥有该 GUID 时以 `NOT_FOUND` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `command` | `string` | 是 | 命令：`{11213A01-...}` 这样的 GUID、叶子名或斜杠分隔的路径。优先用 GUID，只有它不受宿主语言影响。不能为空。 |
| `subGuid` | `string` | 否 | 该命令的某个动态子命令的 GUID；只配合 GUID 形式使用。空串等同没传。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `guid` | `string` | 已运行命令的 GUID；菜单树按名称或路径运行时没有。 |
| `dynamic` | `boolean` | 是否为动态子命令。 |
| `subGuid` | `string` | 已运行的动态子命令的 GUID。 |
| `source` | `"v2-tree" \| "index"` | 名称或路径的解析方式：`v2-tree` 为菜单树，`index` 为命令列表。GUID 形式没有。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

`command` 接受 GUID、叶子命令名或斜杠分隔的路径。**推荐用 GUID**：它是唯一跨宿主稳定的形式。
汉化版 foobar2000 上报的是中文命令名，因此英文名或英文路径在该宿主上解析不到。

命令名与路径按段精确匹配。若某个名字匹配到多条命令，调用会以
`MENU_MATCH_AMBIGUOUS` 失败并列出候选，而不会替你挑一个。

失败一律以 `success: false` 加 `code` 返回：

| `code` | 含义 |
| --- | --- |
| `MENU_ITEM_DISABLED` | 命令存在但当前为禁用（灰显）态。 |
| `MENU_MATCH_AMBIGUOUS` | 名字匹配到多条命令，详见 `candidates`。 |
| `MENU_COMMAND_NOT_FOUND` | 没有匹配到命令。 |
| `NOT_FOUND` | 没有命令拥有该 GUID。 |

```javascript
// 偏好设置：SDK 定义的 GUID 不受宿主语言影响。
await fb2k.invoke('menu.runMainMenuCommand', {
    command: '{11213A01-9F36-4E69-A1BB-7A72F418DE3A}',
});

// 路径形式（仅在标签语言与宿主一致时可用）
await fb2k.invoke('menu.runMainMenuCommand', { command: '文件/首选项' });

// node 是用户从 menu.getMainMenu() 结果中选中的动态叶子。
async function runChosenDynamicMainCommand(node) {
    if (node.type !== 'command' || !node.guid || !node.subGuid) return;
    return fb2k.invoke('menu.runMainMenuCommand', {
        command: node.guid,
        subGuid: node.subGuid,
    });
}
```

### menu.runContextCommand

<!-- api-schema:begin menu.runContextCommand -->
对正在播放的曲目运行右键菜单命令，没有播放时对活动播放列表的选中项运行；两者都没有时以 `NO_ACTIVE_ITEM` 失败。名称没匹配到命令时以 `MENU_COMMAND_NOT_FOUND` 失败，没有命令拥有该 GUID 时以 `NOT_FOUND` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `command` | `string` | 是 | 命令：GUID 或命令名。不能为空。 |
| `subGuid` | `string` | 否 | 动态子项的 GUID，例如某个评分值或转换预设；不传时目标是它所属的容器，什么也不会发生。空串等同没传。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `guid` | `string` | 已运行命令的 GUID；按名称找到且没有 GUID 时没有。 |
| `itemCount` | `integer` | 命令作用的曲目数。 |
| `executionConfirmed` | `boolean` | 命令没有 GUID、只能交给不回报结果的宿主入口时为 `false`，此时不知道它是否真的运行了。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

`executionConfirmed: false` 表示命令是通过一个不返回结果的入口交给宿主的，因此无法观测是否真的执行。
只有在某个注册项没有稳定 GUID 时才会出现。

```javascript
// 曲目属性；操作目标在调用时确定。
await fb2k.invoke('menu.runContextCommand', {
    command: '{6F441057-1D18-4A58-9AC4-8F409CDA7DFD}',
});

// node 是用户从 menu.getContextMenu() 结果中选中的动态叶子。
async function runChosenDynamicContextCommand(node) {
    if (node.type !== 'command' || !node.guid || !node.subGuid) return;
    return fb2k.invoke('menu.runContextCommand', {
        command: node.guid,
        subGuid: node.subGuid,
    });
}
```

### menu.getMainMenu

<!-- api-schema:begin menu.getMainMenu -->
以树的形式获取主菜单。宿主先按菜单树构建，失败时退到 Win32 菜单（`source: "v1-hmenu"`），最后退到扁平命令列表（`fallback: "flat-mainmenu-commands"`）；都建不出时以 `OPERATION_FAILED` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `root` | `string` | 否 | 要返回的子菜单的标签或斜杠分隔路径，不传则返回整个菜单。没有匹配时返回整个菜单，`rootMatched` 为 false。默认 `""`。 |
| `locale` | `string` | 否 | `displayLabel` 用的语言：`auto` 保留宿主的标签；`zh` 或 `en` 开头的标签会翻译常见标签。默认 `"auto"`。 |
| `i18n` | `boolean` | 否 | 为 `false` 时关闭标签翻译。默认 `true`。 |
| `withAvailability` | `boolean` | 否 | 给子菜单加上命令计数。默认 `true`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `root` | `string` | 返回的子菜单的标签；返回整个菜单时为空。 |
| `requestedRoot` | `string` | 请求的 `root`。 |
| `rootMatched` | `boolean` | 是否找到了 `root`；没有请求时为 true，扁平列表除外，它恒为 false。 |
| `locale` | `string` | 使用的 `locale`。 |
| `i18n` | `boolean` | 使用的 `i18n`。 |
| `withAvailability` | `boolean` | 使用的 `withAvailability`。 |
| `items` | `MenuTreeNode[]` | 顶层节点。 |
| `items[].type` | `"separator" \| "submenu" \| "command"` | 节点种类。 |
| `items[].label` | `string` | 宿主报告的标签（汉化版上是译名）。 |
| `items[].displayLabel` | `string` | 按 `locale` 翻译后的 `label`；不翻译或没有对应译名时与 `label` 相同。 |
| `items[].path` | `string` | 从菜单顶层起的斜杠分隔标签。 |
| `items[].displayPath` | `string` | 由显示标签组成的 `path`。 |
| `items[].flags` | `integer` | 原始显示位。位的含义取决于节点来源：菜单树与扁平列表是 SDK 菜单标志，Win32 菜单是 Win32 菜单状态。Win32 菜单的子菜单没有。 |
| `items[].children` | `MenuTreeNode[]` | 子节点，按菜单顺序；只有子菜单有。 |
| `items[].availability` | `MenuAvailability` | 该子菜单之下的命令计数；只在 `withAvailability` 时出现，嵌在 Win32 菜单里的子菜单没有。 |
| `items[].availability.totalCommands` | `integer` | 子菜单之下的全部命令数。 |
| `items[].availability.availableCommands` | `integer` | 可用的命令数。 |
| `items[].availability.disabledCommands` | `integer` | 被禁用的命令数。 |
| `items[].availability.allAvailable` | `boolean` | 是否全部命令都可用；一条命令都没有时也为 true。 |
| `items[].enabled` | `boolean` | 命令能否运行；只有命令有。 |
| `items[].available` | `boolean` | 与 `enabled` 相同；只有命令有。 |
| `items[].checked` | `boolean` | 命令是否带勾选标记（单选标记也算）；只有命令有。 |
| `items[].radioChecked` | `boolean` | 勾选标记是否为单选样式；只有命令有。 |
| `items[].hidden` | `boolean` | 宿主是否不会绘制该命令；只有命令有。 |
| `items[].commandId` | `integer` | 菜单项 id。右键菜单命令可以交给 `menu.runContextCommandById` 运行。id 只对产生它的那份菜单有效，不要保存。扁平列表里没有。 |
| `items[].guid` | `string` | 命令 GUID，是交给 `menu.runMainMenuCommand` 或 `menu.runContextCommand` 的稳定地址；解析不到时没有。 |
| `items[].subGuid` | `string` | 动态子命令的 GUID，与 `guid` 一起传。 |
| `items[].source` | `"mainmenu_static" \| "mainmenu_dynamic" \| "contextmenu_static" \| "contextmenu_dynamic" \| "hmenu_fallback"` | 命令的来源：带 `subGuid` 时为动态值，否则为静态值；Win32 菜单的命令一律为 `hmenu_fallback`。 |
| `items[].executable` | `boolean` | 命令是否有可用来运行它的 `guid`。 |
| `items[].unaddressableReason` | `"noStableIdentifier"` | 命令不能运行的原因；`executable` 为 false 时出现。 |
| `items[].fallback` | `boolean` | 扁平列表里的命令为 `true`。 |
| `source` | `"v1-hmenu"` | 树由 Win32 菜单构建时为 `v1-hmenu`。 |
| `fallback` | `"flat-mainmenu-commands"` | 只能建出扁平命令列表时为 `flat-mainmenu-commands`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

命令叶节点带 `source`：菜单树上带 `subGuid` 的叶节点为 `mainmenu_dynamic`，不带的为 `mainmenu_static`；宿主改走 Win32 菜单时，响应带 `source: "v1-hmenu"`，所有叶节点为 `hmenu_fallback`。扁平回退（`fallback: "flat-mainmenu-commands"`）的每一项带 `fallback: true`，保留 `flags`，没有 `commandId`。

### menu.getContextMenu

<!-- api-schema:begin menu.getContextMenu -->
以树的形式获取 `mode` 所选曲目的右键菜单。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `mode` | `string` | 否 | 为哪些曲目构建菜单：`handles` 用 `handles`，`playlist` 用活动播放列表，`nowPlaying` 用正在播放的曲目，`selection` 用活动播放列表的选中项。其他值（包括 `auto`）按 `handles`、正在播放的曲目、选中项、活动播放列表的顺序取第一个可用的。给了 `handles` 却没有一条可用时，任何模式（包括 `auto`）都以 `INVALID_PARAMS` 失败，不会落到别的曲目上；`nowPlaying` 没有播放、`selection` 没有选中时以 `NO_ACTIVE_ITEM` 失败。默认 `"auto"`。 |
| `locale` | `string` | 否 | `displayLabel` 用的语言：`auto` 保留宿主的标签；`zh` 或 `en` 开头的标签会翻译常见标签。默认 `"auto"`。 |
| `i18n` | `boolean` | 否 | 为 `false` 时关闭标签翻译。默认 `true`。 |
| `withAvailability` | `boolean` | 否 | 给子菜单加上命令计数。默认 `true`。 |
| `handles` | `any[]` | 否 | `mode: "handles"` 与 `auto` 用的曲目：每项是路径（可带 `\|subsong:N` 后缀）或 `{ path, subsong }` 对象。安全策略拒绝的路径与其他类型的条目会被跳过。每个路径都要在磁盘上检查，所以宿主列表里已有的曲目优先用 `selection`、`playlist` 或 `nowPlaying`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `mode` | `"handles" \| "playlist" \| "nowPlaying" \| "selection"` | 实际使用的模式；`auto` 会落到其余某一个。 |
| `locale` | `string` | 使用的 `locale`。 |
| `i18n` | `boolean` | 使用的 `i18n`。 |
| `withAvailability` | `boolean` | 使用的 `withAvailability`。 |
| `items` | `MenuTreeNode[]` | 顶层节点。 |
| `items[].type` | `"separator" \| "submenu" \| "command"` | 节点种类。 |
| `items[].label` | `string` | 宿主报告的标签（汉化版上是译名）。 |
| `items[].displayLabel` | `string` | 按 `locale` 翻译后的 `label`；不翻译或没有对应译名时与 `label` 相同。 |
| `items[].path` | `string` | 从菜单顶层起的斜杠分隔标签。 |
| `items[].displayPath` | `string` | 由显示标签组成的 `path`。 |
| `items[].flags` | `integer` | 原始显示位。位的含义取决于节点来源：菜单树与扁平列表是 SDK 菜单标志，Win32 菜单是 Win32 菜单状态。Win32 菜单的子菜单没有。 |
| `items[].children` | `MenuTreeNode[]` | 子节点，按菜单顺序；只有子菜单有。 |
| `items[].availability` | `MenuAvailability` | 该子菜单之下的命令计数；只在 `withAvailability` 时出现，嵌在 Win32 菜单里的子菜单没有。 |
| `items[].availability.totalCommands` | `integer` | 子菜单之下的全部命令数。 |
| `items[].availability.availableCommands` | `integer` | 可用的命令数。 |
| `items[].availability.disabledCommands` | `integer` | 被禁用的命令数。 |
| `items[].availability.allAvailable` | `boolean` | 是否全部命令都可用；一条命令都没有时也为 true。 |
| `items[].enabled` | `boolean` | 命令能否运行；只有命令有。 |
| `items[].available` | `boolean` | 与 `enabled` 相同；只有命令有。 |
| `items[].checked` | `boolean` | 命令是否带勾选标记（单选标记也算）；只有命令有。 |
| `items[].radioChecked` | `boolean` | 勾选标记是否为单选样式；只有命令有。 |
| `items[].hidden` | `boolean` | 宿主是否不会绘制该命令；只有命令有。 |
| `items[].commandId` | `integer` | 菜单项 id。右键菜单命令可以交给 `menu.runContextCommandById` 运行。id 只对产生它的那份菜单有效，不要保存。扁平列表里没有。 |
| `items[].guid` | `string` | 命令 GUID，是交给 `menu.runMainMenuCommand` 或 `menu.runContextCommand` 的稳定地址；解析不到时没有。 |
| `items[].subGuid` | `string` | 动态子命令的 GUID，与 `guid` 一起传。 |
| `items[].source` | `"mainmenu_static" \| "mainmenu_dynamic" \| "contextmenu_static" \| "contextmenu_dynamic" \| "hmenu_fallback"` | 命令的来源：带 `subGuid` 时为动态值，否则为静态值；Win32 菜单的命令一律为 `hmenu_fallback`。 |
| `items[].executable` | `boolean` | 命令是否有可用来运行它的 `guid`。 |
| `items[].unaddressableReason` | `"noStableIdentifier"` | 命令不能运行的原因；`executable` 为 false 时出现。 |
| `items[].fallback` | `boolean` | 扁平列表里的命令为 `true`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

`auto` 按「`handles` → 当前播放 → 播放列表选中项 → 播放列表本身」顺序取目标，实际采用的模式由返回的 `mode` 给出。

### menu.runContextCommandById

<!-- api-schema:begin menu.runContextCommandById -->
运行 `menu.getContextMenu` 报告的 `commandId` 对应的右键菜单项。菜单会按 `mode` 与 `handles` 重建，所以要传那次调用用的 `handles` 和它报告的 `mode`，不要再传 `auto`。id 是重建后菜单里的位置而不是名称：超出菜单末尾的 id 以 `NOT_FOUND` 失败；但曲目、选择、正在播放的曲目或已装组件在此期间变了时，同一个 id 可能不报错地运行另一项。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `id` | `integer` | 是 | 右键菜单节点的 `commandId`。取值 `0` 到 `4294967295`（含端点）。 |
| `mode` | `string` | 否 | 构建菜单时用的 `mode`；见 `menu.getContextMenu`。默认 `"auto"`。 |
| `handles` | `any[]` | 否 | 构建菜单时用的 `handles`；见 `menu.getContextMenu`。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

::: warning 注意
`commandId` 是菜单位置。执行时沿用取菜单时的 `mode` 与 `handles`，仍不能防止动态项变化后同位置指向另一命令；当前没有原子身份校验。详见[操作目标与完成结果](../sdk/menu.md#command-targets)。
:::

```javascript
// 获取菜单树并执行
const menu = await fb2k.invoke('menu.getContextMenu', { mode: 'nowPlaying' });
// 找到目标 item 后执行
await fb2k.invoke('menu.runContextCommandById', { id: item.commandId, mode: 'nowPlaying' });
```

### menu.showNativePopup

<!-- api-schema:begin menu.showNativePopup -->
在鼠标指针处为 `mode` 所选曲目弹出宿主的原生右键菜单。菜单在调用返回后随即打开。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `mode` | `string` | 否 | 为哪些曲目构建菜单；见 `menu.getContextMenu`。默认 `"auto"`。 |
| `handles` | `any[]` | 否 | `mode: "handles"` 与 `auto` 用的曲目；见 `menu.getContextMenu`。 |
| `x` | `integer` | 否 | 为兼容而接受，没有作用：菜单总在鼠标指针处打开。 |
| `y` | `integer` | 否 | 为兼容而接受，没有作用：菜单总在鼠标指针处打开。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

::: tip 提示
菜单通过 Win32 `SetTimer` 延迟执行，以避免在 WebView2 桥接回调中阻塞。坐标始终使用系统光标位置（最可靠，不受 DPI/CSS 像素差异影响）。
:::

```javascript
// 播放列表选中曲目的原生右键菜单
await fb2k.invoke('menu.showNativePopup', { mode: 'selection' });

// 播放列表级上下文菜单
await fb2k.invoke('menu.showNativePopup', { mode: 'playlist' });

// 当前播放曲目的原生右键菜单
await fb2k.invoke('menu.showNativePopup', { mode: 'nowPlaying' });

// 指定曲目的原生右键菜单
await fb2k.invoke('menu.showNativePopup', {
    mode: 'handles',
    handles: ['C:\\Music\\song.flac']
});
```

## 契约补充

以下补充这些方法的完整参数契约，不改变前文的已有说明。

### menu.close

<!-- api-schema:begin menu.close -->
关闭打开着的自绘菜单；它的 `menu:dismiss` 事件带上 `reason`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `reason` | `string` | 否 | 在 `menu:dismiss` 里报告的原因。默认 `"api"`。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('menu.close', { reason: 'api' });
```

### menu.show

<!-- api-schema:begin menu.show -->
显示自绘菜单。选中结果以 `menu:select` 事件到达，关闭以 `menu:dismiss` 到达；调整评分、滑块或分段行时报告 `menu:valueChanged`，菜单保持打开。行数过多、嵌套过深、分段过多或样式文本过长时以 `INVALID_PARAMS` 失败并带 `details`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `items` | `any[]` | 否 | 菜单行；各字段见 SDK 的 `MenuPopupItem`。超过 32 KiB 的行图标会被丢弃。 |
| `x` | `integer` | 否 | 锚点的屏幕 x，单位为物理像素；不传或为负时用鼠标指针位置。 |
| `y` | `integer` | 否 | 锚点的屏幕 y，单位为物理像素；不传或为负时用鼠标指针位置。 |
| `windowModel` | `string` | 否 | `contentSized` 让每个面板画在按内容定尺寸的窗口里；其他值使用一个全屏覆盖窗口。 |
| `css` | `string` | 否 | 叠加在内置样式之上的菜单样式表，最多 256 KiB。 |
| `cssReplace` | `boolean` | 否 | 用 `css` 取代内置样式，而不是叠加在上面。 |
| `backdrop` | `string` | 否 | 窗口背景材质：`acrylic`（缺省）、`mica`、`mica-alt` 或 `none`。其他值保持缺省。 |
| `backdropDarkMode` | `boolean` | 否 | 背景材质用深色调；缺省为 `true`。 |
| `closeAnimationMs` | `integer` | 否 | 关闭动画的时长，单位毫秒，夹在 0 到 1000 之间；缺省 `0` 表示立即关闭。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `menuId` | `string` | 菜单的 id；`menu:select`、`menu:dismiss` 与 `menu:valueChanged` 都带着它。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('menu.show', {
    items: [
        { id: 'play', label: '播放' },
        { id: 'enqueue', label: '加入播放列表' },
    ],
});
if (res.success === false) throw new Error(res.error);
const { menuId } = res;
```

<!-- phase3-supplement:menu.getContextMenu -->
### Contract 补充：`menu.getContextMenu`

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `handles` | `array` | 否 | `[]` | 目标曲目列表；`mode: 'handles'` 时必须提供。 |
| `i18n` | `boolean` | 否 | `true` | 是否本地化菜单标签。 |
| `locale` | `string` | 否 | `auto` | 本地化使用的区域标识；`auto` 表示跟随宿主。 |
| `mode` | `string` | 否 | `auto` | 取值为 `auto`、`selection`、`playlist`、`nowPlaying`、`handles` 之一；其他任何值同样按 `auto` 处理。 |
| `withAvailability` | `boolean` | 否 | `true` | 是否附带启用/勾选等可用性字段。 |

#### 返回字段

| 字段 | 类型 | 可选 |
| --- | --- | --- |
| `error` | `string` | 是 |
| `success` | `boolean` | 否 |
| `i18n` | `json` | 否 |
| `items` | `json` | 否 |
| `locale` | `json` | 否 |
| `mode` | `json` | 否 |
| `withAvailability` | `json` | 否 |

```js
const res = await fb2k.invoke('menu.getContextMenu');
if (res.success === false) throw new Error(res.error);
const { items, mode } = res;
```

<!-- phase3-supplement:menu.runContextCommandById -->
### Contract 补充：`menu.runContextCommandById`

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `id` | `integer` | 是 | 无 | 来自 `menu.getContextMenu` 的 `commandId`。缺失时报 `id is required`，非整数报 `id must be an integer`，负数报 `id is out of range`，都是 `INVALID_PARAMS`。 |
| `mode` | `string` | 否 | `auto` | 取值为 `auto`、`selection`、`playlist`、`nowPlaying`、`handles` 之一；其他任何值同样按 `auto` 处理。请沿用取菜单时的 `mode`。 |
| `handles` | `array` | 否 | `[]` | 目标曲目列表；`mode: 'handles'` 时必须提供。 |

#### 返回字段

| 字段 | 类型 | 可选 |
| --- | --- | --- |
| `error` | `string` | 是 |
| `success` | `boolean` | 否 |

```js
const res = await fb2k.invoke('menu.getContextMenu');
if (res.success === false) throw new Error(res.error);
const { items } = res;
await fb2k.invoke('menu.runContextCommandById', { id: items[0].commandId });
```
