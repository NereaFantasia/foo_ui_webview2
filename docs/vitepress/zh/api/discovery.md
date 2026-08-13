# Discovery 服务发现

主动发现 foobar2000 中其他组件注册的服务。v1.1.3+。共 15 个 API。

> 与 PluginRegistry 的被动注册模式不同，Discovery API 主动枚举系统中所有已注册的服务。

## 服务发现

### discovery.getAllServices

获取所有可发现服务的统计摘要。

- **参数**: 无

**返回值**:

```json
{
    "success": true,
    "services": {
        "mainMenuCommands": 156,
        "mainMenuDynamicCommands": 22,
        "mainMenuGroups": 40,
        "contextMenuCommands": 64,
        "inputFormats": 30,
        "uiElements": 25,
        "dspEntries": 18,
        "outputDevices": 3,
        "preferencePages": 20,
        "components": 32
    },
    "contextMenuHiddenFiltered": 5,
    "stateKnown": true,
    "totalServices": 388
}
```

`services.contextMenuCommands` 与 `discovery.getContextMenuCommands` 走同一次枚举，并计入 `totalServices`。两个菜单族都已过滤为宿主实际会显示的条目，因此计数与列举端点可直接对照；被过滤掉的数量由 `contextMenuHiddenFiltered` 给出。无选中且无播放曲目时 `stateKnown` 为 `false`。

```javascript
const summary = await fb2k.invoke('discovery.getAllServices');
console.log(`共 ${summary.totalServices} 个服务`);
```

### discovery.getComponents

获取所有已安装组件的信息。

**返回值**: `{"components":"...","count":0,"success":true}`


### discovery.getInputFormats

获取支持的音频输入格式。

**返回值**: `{ "success": true, "fileTypes": [{ "name": "FLAC", "mask": "*.FLAC", "index": 0 }], "count": 30 }`

### discovery.getUIElements

获取所有已注册的 UI 元素。

**返回值**:

```json
{
    "success": true,
    "elements": [
        {
            "guid": "{...}",
            "subclassGuid": "{...}",
            "name": "Spectrum Analyzer",
            "description": "...",
            "isUserAddable": true
        }
    ],
    "count": 25
}
```

### discovery.getDspEntries

获取所有可用的 DSP 处理器条目。

**返回值**: `{ "success": true, "entries": [{ "guid": "{...}", "name": "Equalizer" }], "count": 18 }`

### discovery.getOutputDevices

获取音频输出设备列表。

**返回值**: `{ "success": true, "devices": [{ "guid": "{...}" }], "count": 3 }`

### discovery.getPreferencePages

获取所有偏好设置页面。

**返回值**: `{ "success": true, "pages": [{ "guid": "{...}", "parentGuid": "{...}", "name": "Display" }], "count": 20 }`

## 主菜单

### discovery.getMainMenuCommands

获取所有主菜单命令。

默认会展开 `mainmenu_commands_v2` 的动态子菜单（ESLyric 等 SMP 老组件常用），因此结果中除父命令槽位外还包含其运行时子命令。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `expandDynamic` | `boolean` | 否 | 默认 `true`。设为 `false` 仅返回静态注册表条目。 |
| `includeHidden` | `boolean` | 否 | 默认 `false`。设为 `true` 时把宿主不会显示的隐藏条目也列出。 |

**返回值**: `{ "success": true, "commands": [{ "name": "...", "description": "...", "guid": "{...}", "parentGuid": "{...}", "index": 0, "path": "...", "isDynamic": false }], "count": 156, "dynamicCount": 12, "expandDynamic": true, "includeHidden": false }`

动态展开出来的条目额外带 `subGuid`、`isDynamic: true`、`path`（如 `ESLyric/Search lyric`）以及 `flags`（`mainmenu_commands` 原始显示位掩码：`1` 禁用、`2` 勾选、`4` 单选勾选、`8` 默认隐藏）。父命令槽位为 `isDynamicParent: true`，仅作容器、本身不可执行。执行动态子命令必须把 `subGuid` 一起传给 `discovery.executeMainMenuCommand`。

### discovery.getMainMenuGroups

获取主菜单组结构。

- **参数**: 无

**返回值**: `{ "success": true, "groups": [{ "guid": "{...}", "parentGuid": "{...}", "name": "File", "sortPriority": 0 }], "count": 40 }`

### discovery.executeMainMenuCommand

通过 GUID 执行主菜单命令。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `guid` | `string` | 是 | 命令 GUID；动态子命令传其父命令 GUID。 |
| `subGuid` | `string` | 否 | 动态子命令的 `subGuid`。传入时走 `mainmenu_commands::g_execute_dynamic`。 |

**返回值**: `{ "success": true, "guid": "{...}", "subGuid": "{...}", "dynamic": true }`

### discovery.searchCommands

按名称/描述/菜单路径搜索菜单命令（不区分大小写），默认同时覆盖主菜单与右键菜单。默认展开动态子菜单，因此能搜到 ESLyric 之类组件的运行时子命令。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `query` | `string` | 是 | 搜索关键词。 |
| `expandDynamic` | `boolean` | 否 | 默认 `true`。设为 `false` 仅搜索静态注册表。 |
| `scope` | `string` | 否 | 默认 `all`。设为 `mainmenu` 或 `contextmenu` 仅搜索单一菜单族；无法识别的取值放宽为 `all`，而不是丢结果。 |
| `includeHidden` | `boolean` | 否 | 默认 `false`。设为 `true` 时连宿主不会显示的条目一并搜索。 |

**返回值**: `{ "success": true, "query": "lyric", "results": [{ "name": "...", "description": "...", "guid": "{...}", "subGuid": "{...}", "path": "...", "isDynamic": true, "type": "mainmenu", "enabled": true, "checked": false, "stateKnown": true, "executable": true }], "count": 3, "scope": "all", "includeHidden": false, "mainMenuHits": 3, "contextMenuHits": 0, "stateKnown": true }`

每条结果的 `type` 为 `"mainmenu"` 或 `"contextmenu"`，并携带与列举端点一致的状态字段（`enabled`、`checked`、`radioChecked`、`hidden`、`stateKnown`、`flags`、`source`、`executable`、`unaddressableReason`），调用方无需再发一次请求即可判断能否执行。动态父槽位会被跳过。右键菜单条目是扁平注册、由宿主决定位置，因此其 `path` 即标签本身。

搜索包含右键菜单但无选中且无播放曲目时，响应的 `stateKnown` 为 `false`：此时结果里的 `enabled` / `checked` 不构成观测，不得用于过滤。

```javascript
// 搜索并执行命令（动态子命令需要带上 subGuid）
const result = await fb2k.invoke('discovery.searchCommands', { query: 'lyric' });
const hit = result.results[0];
if (hit) {
    await fb2k.invoke('discovery.executeMainMenuCommand',
        hit.subGuid ? { guid: hit.guid, subGuid: hit.subGuid } : { guid: hit.guid });
}
```

## 右键菜单

### discovery.getContextMenuCommands

获取所有已注册的右键菜单命令。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `includeHidden` | `boolean` | 否 | 默认 `false`。设为 `true` 时连宿主不会显示的条目一并列出。 |

**返回值**:

```json
{
    "success": true,
    "commands": [
        {
            "name": "Properties",
            "description": "Shows track properties",
            "guid": "{...}",
            "parentGuid": "{...}",
            "index": 0,
            "enabled": true,
            "checked": false,
            "radioChecked": false,
            "hidden": false,
            "stateKnown": true,
            "flags": 0,
            "source": "contextmenu_static",
            "executable": true,
            "unaddressableReason": ""
        }
    ],
    "count": 64,
    "includeHidden": false,
    "hiddenFiltered": 5,
    "stateKnown": true,
    "selectionCount": 1
}
```

`enabled` / `checked` 只有在有选中曲目或正在播放时才可观测——SDK 的 `item_get_display_data_root()` 需要一个 `metadb_handle_list`。因此请先看响应的 `stateKnown`：为 `false` 时这两个字段不构成观测，只有 `hidden` 仍然有意义（`FORCE_OFF` 是条目的固有属性，与选中无关）。

### discovery.executeContextMenuCommand

通过 GUID 执行右键菜单命令。作用于当前播放曲目或活动播放列表选中项。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `guid` | `string` | 是 | 带花括号的命令 GUID，取自 `discovery.getContextMenuCommands`；缺失或为空时被拒绝。 |
| `force` | `boolean` | 否 | 默认 `false`。设为 `true` 时即使宿主永不绘制该命令也照样派发。 |

**返回值**: `{ "success": true, "guid": "{...}", "name": "...", "hidden": false, "resolved": true, "force": false, "itemCount": 1 }`

`FORCE_OFF` 的命令会被拒绝而非派发：SDK 将该状态定义为「仅出现在快捷键列表」，宿主从不绘制它，执行等于做了一件用户根本点不到的事。拒绝时返回 `success: false` 且 `hidden: true`。`DEFAULT_OFF`（按 Shift 才显示）仍可到达，永不拒绝。

`hidden` 与 `resolved` 在拒绝与成功两条路径上都会返回，因此「未被拒绝」与「本版本不返回该字段」可以区分。无注册项拥有该 GUID 时 `resolved` 为 `false`，此时没有可评估的状态。

### discovery.executeContextMenuByPath

通过菜单路径名称执行右键菜单命令。支持动态子菜单遍历。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 以斜杠分隔的命令路径，例如 `Playback Statistics/Rating/5`；为空时被拒绝。 |
| `trackPath` | `string` | 否 | 指定其他文件而非当前曲目，受媒体读取安全约束。省略时作用于正在播放的曲目，无播放时作用于活动播放列表的选中项。 |

**返回值**: `{"error":"...","foundName":"...","itemCount":"...","path":"...","success":true}`

### discovery.getContextMenuTree

获取当前曲目的完整右键菜单树结构（调试用）。作用于当前播放曲目或选中项。

- **参数**: 无

**返回值**: `{ "success": true, "tree": { ... }, "itemCount": 1, "truncated": false, "depthExceeded": false, "childrenExceeded": false, "maxDepth": 16, "maxChildrenPerNode": 512 }`

树节点结构：每个节点包含 `name`、`type` (`"command"` / `"popup"` / `"separator"`) 与 `depth`。非分隔符节点还带有与列举端点一致的状态字段：`enabled`、`checked`、`radioChecked`、`hidden`、`stateKnown` 以及原始 `flags`；`command` 节点另有 `fullName`。

截断不再静默：`popup` 节点同时给出 `childCount`（宿主的真实子项数）与 `childrenReturned`（本次响应实际包含的数量），无需自行数数组即可对账。子树被裁剪的节点带 `truncated`，并由 `depthExceeded` / `childrenExceeded` 区分原因；标记会向上传播，因此顶层 `truncated` 覆盖整棵树。`maxDepth` 与 `maxChildrenPerNode` 回显本次生效的上限。

## 发现范围与执行规则

- 返回结果枚举当前 foobar2000 进程中已注册的服务；计数与名称会随已安装组件和 host 配置而变化。
- `discovery.executeMainMenuCommand` 与 `discovery.executeContextMenuCommand` 要求有效 GUID。右键菜单命令优先作用于正在播放曲目，否则作用于活动播放列表选中项。
- `discovery.executeContextMenuByPath` 要求 `path`；可选 `trackPath` 受媒体读取安全策略保护。省略时，runtime 使用相同的正在播放/选中项回退逻辑。
- `discovery.getContextMenuTree` 是诊断输出，需要活动目标曲目；递归深度与每节点子项数均有上限，任何裁剪都通过 `truncated` / `depthExceeded` / `childrenExceeded` 显式上报，生效上限由 `maxDepth` 与 `maxChildrenPerNode` 回显。
- `discovery.searchCommands` 要求非空 `query`，并在主菜单与右键菜单两侧的命令名称、描述、菜单路径中进行不区分大小写的匹配。大小写折叠仅作用于 ASCII，UTF-8 多字节序列（如无大小写可折叠的中文标签）原样通过。
