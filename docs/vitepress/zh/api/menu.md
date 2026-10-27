# Menu 菜单 API

（v1.2.0+）提供 foobar2000 主菜单与上下文右键菜单的树形结构查询、命令定位执行与自绘菜单交互能力。

SDK 定义的地址见[常用命令 GUID](../sdk/menu.md#standard-command-guids)，各执行入口的限制见[操作目标与完成结果](../sdk/menu.md#command-targets)。SDK 调用与 `fb2k.invoke` 使用相同的地址。

## Menu API - 菜单

### menu.runMainMenuCommand

<!-- api-schema:begin menu.runMainMenuCommand -->
执行主菜单命令（支持按 GUID、叶子命令名或斜杠分隔的完整菜单路径寻址）；处于禁用状态的命令以 `MENU_ITEM_DISABLED` 失败；路径匹配必须严格匹配每一级祖先（前缀不匹配时不回退至叶子名）；存在重名歧义时以 `MENU_MATCH_AMBIGUOUS` 失败并在 `details.candidates` 列出候选列表；名称或路径未查得为 `MENU_COMMAND_NOT_FOUND`，指定 GUID 不存在为 `NOT_FOUND`；按 GUID 寻址动态叶子命令时必须同时提供主命令与动态子命令的两个 GUID；菜单状态不可读以 `OPERATION_FAILED` 失败；调用在底层命令执行返回后应答（若命令弹出模态对话框如重命名播放列表，则等待对话框关闭后应答）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `command` | `string` | 是 | 目标命令标识：支持 GUID（如 `{11213A01-...}`）、叶子命令名称或斜杠分隔的完整菜单路径（推荐优先使用 GUID 形式以规避宿主语言差异）不能为空。 |
| `subGuid` | `string` | 否 | 目标动态子命令的 GUID 标识符（仅在 command 传入 GUID 形式时生效，空字符串等同缺省） |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `guid` | `string` | 实际执行命令的 GUID 标识符（由名称或路径成功解析时亦返回，旧版本宿主可能在非 GUID 调用时缺省） |
| `dynamic` | `boolean` | 所执行的命令是否属于动态子命令 |
| `subGuid` | `string` | 实际执行的动态子命令 GUID 标识符 |
| `source` | `"v2-tree" \| "index"` | 寻址名称或路径的底层解析源：`v2-tree` 表示基于菜单树解析，`index` 表示基于命令列表索引（以 GUID 形式调用时不提供） |
| `completion` | `"completed" \| "partial" \| "cancelled" \| "failed" \| "unknown"` | 底层操作完成状态（foobar2000 宿主不反馈命令实际执行结果或模态对话框取消状态，因此固定为 "unknown"） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

`command` 参数支持传入 GUID、叶子命令名或斜杠分隔的完整菜单路径；推荐优先使用 GUID 形式（不受宿主语言和汉化影响，跨版本与环境保持稳定）。在汉化版 foobar2000 中，宿主内部注册为中文菜单名称，传入英文名称或英文路径将无法匹配解析。

命令名与路径匹配要求每一级层级严格精确匹配；当存在同名命令产生歧义时，调用以 `MENU_MATCH_AMBIGUOUS` 失败并在 `details.candidates` 中返回全部候选条目供调用方确认，宿主不执行默认盲选。

失败一律以 `success: false` 加 `code` 返回：

| `code` | 含义 |
| --- | --- |
| `MENU_ITEM_DISABLED` | 命令存在但当前处于禁用（灰显）状态 |
| `MENU_MATCH_AMBIGUOUS` | 命令名称或路径匹配到多条条目（详见 `details.candidates` 列表） |
| `MENU_COMMAND_NOT_FOUND` | 未匹配到任何对应名称或路径的菜单命令 |
| `NOT_FOUND` | 传入的 GUID 未在宿主命令注册表中查得对应条目 |

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
执行上下文右键菜单命令；优先作用于当前正在播放的曲目，未在播放时作用于活动播放列表当前选中的曲目（两者均无目标曲目时以 `NO_ACTIVE_ITEM` 失败）；名称未匹配到有效命令以 `MENU_COMMAND_NOT_FOUND` 失败，GUID 未查得对应命令以 `NOT_FOUND` 失败；调用在底层命令执行返回后应答（若命令弹出模态对话框则等待对话框关闭后应答）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `command` | `string` | 是 | 目标命令标识：支持 GUID 标识符或命令名称不能为空。 |
| `subGuid` | `string` | 否 | 目标动态子项的 GUID 标识符（如指定评分星级或格式转换预设；未提供时默认作用于父容器而不触发动作，空字符串等同缺省） |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `guid` | `string` | 实际执行命令的 GUID 标识符（按名称查找且底层无 GUID 时缺省） |
| `itemCount` | `integer` | 本次命令实际作用的目标曲目句柄总数 |
| `executionConfirmed` | `boolean` | 命令执行是否获得明确调度确认（若命令缺失 GUID 仅能派发至单向宿主入口时为 false，此时无法确认底层是否实际执行） |
| `completion` | `"completed" \| "partial" \| "cancelled" \| "failed" \| "unknown"` | 底层操作完成状态（foobar2000 宿主不反馈命令实际执行结果或模态对话框取消状态，因此固定为 "unknown"） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

`executionConfirmed: false` 表示命令是通过单向宿主入口进行派发的，无法观测到底层是否实际执行（仅在菜单条目缺失稳定 GUID 标识符时才会出现）。

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
获取完整主菜单树形结构；宿主优先通过内部菜单树构建，无法构建时回退至 Win32 菜单（`source: "v1-hmenu"`），最终兜底回退至扁平命令列表（`fallback: "flat-mainmenu-commands"`）；构建过程中状态不可读以 `OPERATION_FAILED` 失败且不猜测可用状态；指定 `root` 过滤时各节点依然保留完整全局路径；命令是否可寻址（`executable`）与当前是否启用（`enabled`）相互独立

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `root` | `string` | 否 | 需定向提取的子菜单标签或斜杠层级路径（缺省时返回整棵主菜单树；若未匹配到目标子菜单则回退返回整棵菜单树且标记 `rootMatched` 为 false）默认 `""`。 |
| `locale` | `string` | 否 | `displayLabel` 显示标签的目标区域代码（`auto` 保留宿主原始标签，以 `zh` 或 `en` 开头时自动翻译常见菜单命令）默认 `"auto"`。 |
| `i18n` | `boolean` | 否 | 是否启用菜单标签本地化翻译（设置为 false 时关闭翻译）默认 `true`。 |
| `withAvailability` | `boolean` | 否 | 是否在各子菜单节点中附加命令数量与可用性统计信息 `availability`默认 `true`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `root` | `string` | 实际返回的根子菜单显示标签（返回整棵主菜单树时为空字符串） |
| `requestedRoot` | `string` | 请求时传入的原始 `root` 参数 |
| `rootMatched` | `boolean` | 请求的 `root` 是否成功匹配（未指定 `root` 时为 true；触发扁平兜底列表时恒为 false） |
| `locale` | `string` | 实际采用的区域语言标识 |
| `i18n` | `boolean` | 实际采用的本地化翻译开关状态 |
| `withAvailability` | `boolean` | 实际采用的命令统计包含开关状态 |
| `items` | `MenuTreeNode[]` | 菜单树顶层节点列表 |
| `items[].type` | `"separator" \| "submenu" \| "command"` | 菜单节点类型：separator（分隔线）、submenu（子菜单）或 command（具体命令） |
| `items[].label` | `string` | 宿主报告的原始菜单标签文本（已汉化宿主为对应中文译名） |
| `items[].displayLabel` | `string` | 依据 `locale` 参数本地化翻译后的显示标签（未开启翻译或无对应译名时与 `label` 保持一致） |
| `items[].path` | `string` | 从顶层菜单起的斜杠分隔层级路径字符串 |
| `items[].displayPath` | `string` | 由本地化显示标签组成的层级路径字符串 |
| `items[].flags` | `integer` | 原始显示标志位（位定义取决于节点数据源：菜单树与扁平列表对应 SDK 菜单标志，Win32 菜单对应 Win32 菜单状态；Win32 子菜单节点缺省本字段） |
| `items[].children` | `MenuTreeNode[]` | 子菜单所包含的下级子节点列表（按菜单展现顺序排列，仅在 submenu 节点提供） |
| `items[].availability` | `MenuAvailability` | 子菜单层级下的命令可用性统计（仅在 `withAvailability` 为 true 时返回，Win32 菜单下级子菜单不提供本字段） |
| `items[].availability.totalCommands` | `integer` | 子菜单层级下包含的命令总数 |
| `items[].availability.availableCommands` | `integer` | 子菜单层级下当前处于可用状态的命令总数 |
| `items[].availability.disabledCommands` | `integer` | 子菜单层级下当前处于禁用状态的命令总数 |
| `items[].availability.allAvailable` | `boolean` | 是否包含的全部命令均处于可用状态（未包含任何命令时亦为 true） |
| `items[].enabled` | `boolean` | 命令当前是否处于可用可执行状态（仅在 command 节点提供） |
| `items[].available` | `boolean` | 命令可用状态别名（数值与 `enabled` 相同，仅在 command 节点提供） |
| `items[].checked` | `boolean` | 命令当前是否处于勾选状态（包含单选标记样式，仅在 command 节点提供） |
| `items[].radioChecked` | `boolean` | 勾选标记是否呈现为圆点单选样式（仅在 command 节点提供） |
| `items[].hidden` | `boolean` | 宿主在渲染时是否隐藏该命令（仅在 command 节点提供） |
| `items[].commandId` | `integer` | 临时菜单项索引标识符（供 `menu.runContextCommandById` 执行使用；仅对当前次菜单树有效且禁止持久化保存，扁平命令列表不提供） |
| `items[].guid` | `string` | 跨语言稳定的命令 GUID 标识符（供 `menu.runMainMenuCommand` 或 `menu.runContextCommand` 寻址使用，未解析时缺省） |
| `items[].subGuid` | `string` | 动态子命令的全局唯一标识符 subGuid（须与主命令 guid 搭配使用） |
| `items[].source` | `"mainmenu_static" \| "mainmenu_dynamic" \| "contextmenu_static" \| "contextmenu_dynamic" \| "hmenu_fallback"` | 菜单节点数据源分类：主菜单按提供者节点定义区分（动态叶子即便缺失 subGuid 仍标记为动态）；上下文菜单按是否存在 subGuid 区分；Win32 菜单命令标记为 `hmenu_fallback` |
| `items[].executable` | `boolean` | 命令是否具备确定性的唯一执行地址（主菜单表示是否存在完整寻址凭据，上下文菜单表示是否存在命令 GUID） |
| `items[].unaddressableReason` | `"noStableIdentifier" \| "ambiguousMatch" \| "missingDynamicIdentifier" \| "dynamicParent" \| "incompleteHierarchy"` | 命令不可寻址执行的具体原因分类（仅在 `executable` 为 false 时提供） |
| `items[].addressCandidates` | `MenuAddressCandidate[]` | 主菜单节点遭遇路径歧义时的所有匹配候选列表（供调用方诊断或展示，禁止前端自动盲选首项） |
| `items[].addressCandidates[].path` | `string` | 观测到的完整菜单标签层级路径 |
| `items[].addressCandidates[].guid` | `string` | 所属命令的全局唯一标识符 GUID |
| `items[].addressCandidates[].subGuid` | `string` | 动态叶子子命令的 GUID 标识符（存在时返回） |
| `items[].addressCandidates[].groupGuid` | `string` | 直接父分组的注册 GUID（无法获取时为空字符串） |
| `items[].fallback` | `boolean` | 是否属于扁平命令兜底列表项（为 true 时表示节点源自兜底扁平列表） |
| `source` | `"v1-hmenu"` | 菜单树数据源标记（当且仅当回退采用 Win32 原生菜单构建时固定为 `"v1-hmenu"`） |
| `fallback` | `"flat-mainmenu-commands"` | 兜底构建模式标记（当且仅当仅能构建出扁平命令列表时固定为 `"flat-mainmenu-commands"`） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

命令叶子节点包含 `source` 数据源标记：标准菜单树中包含 `subGuid` 的节点标记为 `mainmenu_dynamic`，普通静态命令标记为 `mainmenu_static`；当宿主改走 Win32 原生菜单时，根响应附带 `source: "v1-hmenu"`，其下所有叶子节点标记为 `hmenu_fallback`；触发扁平列表兜底（`fallback: "flat-mainmenu-commands"`）时，每个条目附带 `fallback: true`，保留 `flags` 显示位且不包含 `commandId`。

### menu.getContextMenu

<!-- api-schema:begin menu.getContextMenu -->
获取指定 `mode` 寻址的目标曲目上下文右键菜单树形结构

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `mode` | `string` | 否 | 菜单目标曲目解析模式：`handles`（显式传入曲目）、`playlist`（活动播放列表全量曲目）、`nowPlaying`（当前播放曲目）、`selection`（活动播放列表选中项）；传入 `auto` 或其他值时按 handles -> nowPlaying -> selection -> playlist 优先级自动选择首个可用目标；若显式指定 `handles` 但无有效曲目句柄，任何模式（含 auto）均以 `INVALID_PARAMS` 失败且不回退；若指定 `nowPlaying` 但未在播放，或指定 `selection` 但无选中项，均以 `NO_ACTIVE_ITEM` 失败默认 `"auto"`。 |
| `locale` | `string` | 否 | `displayLabel` 显示标签的目标区域代码（`auto` 保留宿主原始标签，以 `zh` 或 `en` 开头时自动翻译常见菜单命令）默认 `"auto"`。 |
| `i18n` | `boolean` | 否 | 是否启用菜单标签本地化翻译（设置为 false 时关闭翻译）默认 `true`。 |
| `withAvailability` | `boolean` | 否 | 是否在各子菜单节点中附加命令数量与可用性统计信息 `availability`默认 `true`。 |
| `handles` | `any[]` | 否 | 供 `mode: "handles"` 及 `auto` 使用的目标曲目列表（数组项支持文件路径字符串如带 `\|subsong:N` 后缀，或 `{ path, subsong }` 对象；受安全策略拦截的路径或非法类型条目自动忽略；因物理路径需执行磁盘检查，宿主已有曲目推荐优先使用 `selection`、`playlist` 或 `nowPlaying`） |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `mode` | `"handles" \| "playlist" \| "nowPlaying" \| "selection"` | 最终实际采用的目标曲目模式（`auto` 自动解析为 `handles`、`playlist`、`nowPlaying` 或 `selection` 之一） |
| `locale` | `string` | 实际采用的区域语言标识 |
| `i18n` | `boolean` | 实际采用的本地化翻译开关状态 |
| `withAvailability` | `boolean` | 实际采用的命令统计包含开关状态 |
| `items` | `MenuTreeNode[]` | 菜单树顶层节点列表 |
| `items[].type` | `"separator" \| "submenu" \| "command"` | 菜单节点类型：separator（分隔线）、submenu（子菜单）或 command（具体命令） |
| `items[].label` | `string` | 宿主报告的原始菜单标签文本（已汉化宿主为对应中文译名） |
| `items[].displayLabel` | `string` | 依据 `locale` 参数本地化翻译后的显示标签（未开启翻译或无对应译名时与 `label` 保持一致） |
| `items[].path` | `string` | 从顶层菜单起的斜杠分隔层级路径字符串 |
| `items[].displayPath` | `string` | 由本地化显示标签组成的层级路径字符串 |
| `items[].flags` | `integer` | 原始显示标志位（位定义取决于节点数据源：菜单树与扁平列表对应 SDK 菜单标志，Win32 菜单对应 Win32 菜单状态；Win32 子菜单节点缺省本字段） |
| `items[].children` | `MenuTreeNode[]` | 子菜单所包含的下级子节点列表（按菜单展现顺序排列，仅在 submenu 节点提供） |
| `items[].availability` | `MenuAvailability` | 子菜单层级下的命令可用性统计（仅在 `withAvailability` 为 true 时返回，Win32 菜单下级子菜单不提供本字段） |
| `items[].availability.totalCommands` | `integer` | 子菜单层级下包含的命令总数 |
| `items[].availability.availableCommands` | `integer` | 子菜单层级下当前处于可用状态的命令总数 |
| `items[].availability.disabledCommands` | `integer` | 子菜单层级下当前处于禁用状态的命令总数 |
| `items[].availability.allAvailable` | `boolean` | 是否包含的全部命令均处于可用状态（未包含任何命令时亦为 true） |
| `items[].enabled` | `boolean` | 命令当前是否处于可用可执行状态（仅在 command 节点提供） |
| `items[].available` | `boolean` | 命令可用状态别名（数值与 `enabled` 相同，仅在 command 节点提供） |
| `items[].checked` | `boolean` | 命令当前是否处于勾选状态（包含单选标记样式，仅在 command 节点提供） |
| `items[].radioChecked` | `boolean` | 勾选标记是否呈现为圆点单选样式（仅在 command 节点提供） |
| `items[].hidden` | `boolean` | 宿主在渲染时是否隐藏该命令（仅在 command 节点提供） |
| `items[].commandId` | `integer` | 临时菜单项索引标识符（供 `menu.runContextCommandById` 执行使用；仅对当前次菜单树有效且禁止持久化保存，扁平命令列表不提供） |
| `items[].guid` | `string` | 跨语言稳定的命令 GUID 标识符（供 `menu.runMainMenuCommand` 或 `menu.runContextCommand` 寻址使用，未解析时缺省） |
| `items[].subGuid` | `string` | 动态子命令的全局唯一标识符 subGuid（须与主命令 guid 搭配使用） |
| `items[].source` | `"mainmenu_static" \| "mainmenu_dynamic" \| "contextmenu_static" \| "contextmenu_dynamic" \| "hmenu_fallback"` | 菜单节点数据源分类：主菜单按提供者节点定义区分（动态叶子即便缺失 subGuid 仍标记为动态）；上下文菜单按是否存在 subGuid 区分；Win32 菜单命令标记为 `hmenu_fallback` |
| `items[].executable` | `boolean` | 命令是否具备确定性的唯一执行地址（主菜单表示是否存在完整寻址凭据，上下文菜单表示是否存在命令 GUID） |
| `items[].unaddressableReason` | `"noStableIdentifier" \| "ambiguousMatch" \| "missingDynamicIdentifier" \| "dynamicParent" \| "incompleteHierarchy"` | 命令不可寻址执行的具体原因分类（仅在 `executable` 为 false 时提供） |
| `items[].addressCandidates` | `MenuAddressCandidate[]` | 主菜单节点遭遇路径歧义时的所有匹配候选列表（供调用方诊断或展示，禁止前端自动盲选首项） |
| `items[].addressCandidates[].path` | `string` | 观测到的完整菜单标签层级路径 |
| `items[].addressCandidates[].guid` | `string` | 所属命令的全局唯一标识符 GUID |
| `items[].addressCandidates[].subGuid` | `string` | 动态叶子子命令的 GUID 标识符（存在时返回） |
| `items[].addressCandidates[].groupGuid` | `string` | 直接父分组的注册 GUID（无法获取时为空字符串） |
| `items[].fallback` | `boolean` | 是否属于扁平命令兜底列表项（为 true 时表示节点源自兜底扁平列表） |
| `targetKey` | `string` | 上下文菜单目标曲目集合的不透明指纹凭据（供 `runContextCommandById` 的 `expectTarget` 校验使用；`mode` 为 `playlist` 时缺省，仅在当前宿主进程生命周期内有效） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

传入 `auto` 时宿主按「`handles` 传入曲目 → 当前播放曲目 → 播放列表选中项 → 播放列表全量曲目」优先级自动选择首个有效目标，实际采用的寻址模式记录于返回结果的 `mode` 字段。

### menu.runContextCommandById

<!-- api-schema:begin menu.runContextCommandById -->
执行由 `menu.getContextMenu` 返回的 `commandId` 所对应的右键菜单项；执行时会依据传入的 `mode` 与 `handles` 重新构建菜单（必须传入此前获取菜单时所用的具体 `handles` 及其实际解析出的 `mode`，不可再次传入 `auto`）；`id` 代表菜单重建后的条目索引而非命令名称（索引超出范围以 `NOT_FOUND` 失败；若在此期间曲目集合、选择状态、播放曲目或已安装组件发生变动，相同索引可能指向其他命令条目）；建议同时传入 `expectPath` 与 `expectTarget` 进行安全校验（校验通过方才执行，若检测到不一致则拒绝执行且返回明确原因，并在 `details.reason` 中标记为 `targetChanged`、`commandChanged`、`ambiguousPath`、`disabled` 或 `unsupportedTargetMode`）；调用在底层命令执行返回后应答（若命令弹出模态对话框则等待对话框关闭后应答）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `id` | `integer` | 是 | 目标右键菜单节点的 `commandId` 索引取值 `0` 到 `4294967295`（含端点）。 |
| `mode` | `string` | 否 | 获取菜单树时所使用的具体 `mode` 参数（禁止传入 auto，须沿用解析后的实际模式）默认 `"auto"`。 |
| `handles` | `any[]` | 否 | 获取菜单树时传入的 `handles` 曲目列表 |
| `expectPath` | `string` | 否 | 预期执行节点的层级路径 `path`（须与 `expectTarget` 搭配使用；指定 `id` 的菜单项必须完全匹配该路径且整棵树中不得存在重名路径）不能为空。 |
| `expectTarget` | `string` | 否 | 预期目标曲目指纹凭据 `targetKey`（须与 `expectPath` 搭配使用；当前重新解析出的曲目指纹必须与之严格相等）不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `completion` | `"completed" \| "partial" \| "cancelled" \| "failed" \| "unknown"` | 底层操作完成状态（foobar2000 宿主不反馈命令实际执行结果或模态对话框取消状态，因此固定为 "unknown"） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

::: warning 菜单项索引位置与校验
`commandId` 代表重建菜单树时的动态条目索引；若两次调用期间曲目或环境发生变动，相同索引可能指向不同命令；建议在调用 `runContextCommandById` 时同时传入 `expectPath` 与 `expectTarget` 启用前置一致性校验，详见[操作目标与完成结果](../sdk/menu.md#command-targets)。
:::

```javascript
// 获取菜单树并执行
const menu = await fb2k.invoke('menu.getContextMenu', { mode: 'nowPlaying' });
// 找到目标 item 后执行
await fb2k.invoke('menu.runContextCommandById', { id: item.commandId, mode: 'nowPlaying' });
```

### menu.showNativePopup

<!-- api-schema:begin menu.showNativePopup -->
在鼠标光标位置为 `mode` 所选曲目弹出宿主的原生右键上下文菜单（菜单在调用返回后异步唤起）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `mode` | `string` | 否 | 弹出原生菜单的目标曲目模式（取值同 `menu.getContextMenu`）默认 `"auto"`。 |
| `handles` | `any[]` | 否 | 供 `mode: "handles"` 及 `auto` 使用的目标曲目列表（定义同 `menu.getContextMenu`） |
| `x` | `integer` | 否 | 兼容保留参数（无实际效果，原生菜单总在当前鼠标光标位置弹出） |
| `y` | `integer` | 否 | 兼容保留参数（无实际效果，原生菜单总在当前鼠标光标位置弹出） |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

::: tip 原生弹出机制
原生菜单通过 Win32 `SetTimer` 机制异步延迟弹出，避免在 WebView2 桥接事件回调中发生线程死锁或卡顿；弹窗坐标始终绑定当前系统光标位置，有效规避高 DPI 与多显示器缩放下的坐标偏移。
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
主动关闭当前处于打开状态的自绘菜单（并在随后派发的 `menu:dismiss` 事件中携带传入的 `reason`）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `reason` | `string` | 否 | 菜单关闭原因文本（将在随后的 `menu:dismiss` 事件中携带）默认 `"api"`。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('menu.close', { reason: 'api' });
```

### menu.show

<!-- api-schema:begin menu.show -->
弹出自定义自绘菜单窗口；用户选中菜单项时触发 `menu:select` 事件，菜单关闭时触发 `menu:dismiss` 事件；交互调整评分、滑块或分段控件时派发 `menu:valueChanged` 事件且菜单保持打开；条目数量超出上限、嵌套层级过深、分段数量超标或自定义 CSS 文本过长时以 `INVALID_PARAMS` 失败（错误详情附带 `details`）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `items` | `any[]` | 否 | 自绘菜单条目定义列表（单项字段结构详见 SDK 的 `MenuPopupItem`；超出 32 KiB 的条目图标将自动忽略） |
| `x` | `integer` | 否 | 弹出锚点屏幕横坐标 X（物理像素；缺省或传入负数时使用当前鼠标光标位置） |
| `y` | `integer` | 否 | 弹出锚点屏幕纵坐标 Y（物理像素；缺省或传入负数时使用当前鼠标光标位置） |
| `windowModel` | `string` | 否 | 弹窗窗口渲染模型：传入 `contentSized` 各级面板在独立自适应窗口中渲染，其他值采用全屏透明遮罩覆盖窗口 |
| `css` | `string` | 否 | 叠加于内置样式之上的自定义 CSS 样式表字符串（最大限制 256 KiB） |
| `cssReplace` | `boolean` | 否 | 是否全量替换内置菜单样式（为 true 时完全使用 `css` 参数而不叠加内置样式） |
| `backdrop` | `string` | 否 | 菜单窗口背景材质类型：`acrylic`（默认）、`mica`、`mica-alt` 或 `none`（其他值回退至默认材质） |
| `backdropDarkMode` | `boolean` | 否 | 背景材质是否采用深色色调（默认 true） |
| `closeAnimationMs` | `integer` | 否 | 菜单关闭淡出动画时长（毫秒，限制在 0 至 1000 毫秒之间，默认 `0` 表示立即关闭） |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `menuId` | `string` | 自绘菜单唯一标识符（随附于 `menu:select`、`menu:dismiss` 与 `menu:valueChanged` 事件中） |

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
| `handles` | `array` | 否 | `[]` | 目标曲目列表（`mode: 'handles'` 时必须提供） |
| `i18n` | `boolean` | 否 | `true` | 是否启用菜单标签本地化翻译 |
| `locale` | `string` | 否 | `auto` | 本地化采用的区域代码（`auto` 表示跟随宿主语言） |
| `mode` | `string` | 否 | `auto` | 目标曲目寻址模式：`auto`、`selection`、`playlist`、`nowPlaying` 或 `handles`（其他值均按 `auto` 处理） |
| `withAvailability` | `boolean` | 否 | `true` | 是否附加命令统计与可用性数据 availability |

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
| `id` | `integer` | 是 | 无 | 来自 `menu.getContextMenu` 的 `commandId` 菜单项索引（缺失时以 `INVALID_PARAMS` 报 `id is required`，非整数报 `id must be an integer`，负数报 `id is out of range`） |
| `mode` | `string` | 否 | `auto` | 目标曲目寻址模式：`auto`、`selection`、`playlist`、`nowPlaying` 或 `handles`（建议沿用获取菜单时的实际模式） |
| `handles` | `array` | 否 | `[]` | 目标曲目列表（`mode: 'handles'` 时必须提供） |

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
