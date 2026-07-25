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
        "mainMenuGroups": 40,
        "inputFormats": 30,
        "uiElements": 25,
        "dspEntries": 18,
        "outputDevices": 3,
        "preferencePages": 20,
        "components": 32
    },
    "totalServices": 324
}
```

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

**返回值**: `{ "success": true, "commands": [{ "name": "...", "description": "...", "guid": "{...}", "parentGuid": "{...}", "index": 0, "path": "...", "isDynamic": false }], "count": 156, "dynamicCount": 12, "expandDynamic": true }`

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

按名称/描述/菜单路径搜索主菜单命令（不区分大小写）。默认展开动态子菜单，因此能搜到 ESLyric 之类组件的运行时子命令。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `query` | `string` | 是 | 搜索关键词。 |
| `expandDynamic` | `boolean` | 否 | 默认 `true`。设为 `false` 仅搜索静态注册表。 |

**返回值**: `{ "success": true, "query": "lyric", "results": [{ "name": "...", "description": "...", "guid": "{...}", "subGuid": "{...}", "path": "...", "isDynamic": true, "type": "mainmenu" }], "count": 3 }`

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
            "index": 0
        }
    ],
    "count": 200
}
```

### discovery.executeContextMenuCommand

通过 GUID 执行右键菜单命令。作用于当前播放曲目或活动播放列表选中项。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `guid` | `string` | 否 | 可选；默认 。 |

**返回值**: `{ "success": true, "guid": "{...}", "itemCount": 1 }`

### discovery.executeContextMenuByPath

通过菜单路径名称执行右键菜单命令。支持动态子菜单遍历。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 否 | 可选；默认 。 |
| `trackPath` | `string` | 否 | 可选；默认 。 |

**返回值**: `{"error":"...","foundName":"...","itemCount":"...","path":"...","success":true}`

### discovery.getContextMenuTree

获取当前曲目的完整右键菜单树结构（调试用）。作用于当前播放曲目或选中项。

- **参数**: 无

**返回值**: `{ "success": true, "tree": { ... }, "itemCount": 1 }`

树节点结构：每个节点包含 `name`、`type` (`"command"` / `"popup"` / `"separator"`)、`children`（popup 类型）、`fullName`（command 类型）。最多递归 10 层，每层最多 50 个子节点。

## 发现范围与执行规则

- 返回结果枚举当前 foobar2000 进程中已注册的服务；计数与名称会随已安装组件和 host 配置而变化。
- `discovery.executeMainMenuCommand` 与 `discovery.executeContextMenuCommand` 要求有效 GUID。右键菜单命令优先作用于正在播放曲目，否则作用于活动播放列表选中项。
- `discovery.executeContextMenuByPath` 要求 `path`；可选 `trackPath` 受媒体读取安全策略保护。省略时，runtime 使用相同的正在播放/选中项回退逻辑。
- `discovery.getContextMenuTree` 是诊断输出，需要活动目标曲目；递归最多 10 层，每个 popup 最多 50 个子项。
- `discovery.searchCommands` 要求非空 `query`，并在主菜单命令名称与描述中进行不区分大小写的匹配。
