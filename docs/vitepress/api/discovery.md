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

**返回值**:

```json
{
    "count": 32,
    "components": [
        {
            "filename": "foo_ui_webview2.dll",
            "name": "WebView UI",
            "version": "1.1.3",
            "about": "..."
        }
    ]
}
```

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

- **参数**: 无

**返回值**: `{ "success": true, "commands": [{ "name": "...", "description": "...", "guid": "{...}", "parentGuid": "{...}", "index": 0 }], "count": 156 }`

### discovery.getMainMenuGroups 

获取主菜单组结构。

- **参数**: 无

**返回值**: `{ "success": true, "groups": [{ "guid": "{...}", "parentGuid": "{...}", "name": "File", "sortPriority": 0 }], "count": 40 }`

### discovery.executeMainMenuCommand 

通过 GUID 执行主菜单命令。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| guid | string | ✓ | 命令 GUID |

**返回值**: `{ "success": true, "guid": "{...}" }`

### discovery.searchCommands 

按名称/描述搜索主菜单命令（不区分大小写）。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| query | string | ✓ | 搜索关键词 |

**返回值**: `{ "success": true, "query": "lyric", "results": [{ "name": "...", "description": "...", "guid": "{...}", "type": "mainmenu" }], "count": 3 }`

```javascript
// 搜索并执行命令
const result = await fb2k.invoke('discovery.searchCommands', { query: 'lyric' });
if (result.results[0]) {
    await fb2k.invoke('discovery.executeMainMenuCommand', { guid: result.results[0].guid });
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

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| guid | string | ✓ | 命令 GUID |

**返回值**: `{ "success": true, "guid": "{...}", "itemCount": 1 }`

### discovery.executeContextMenuByPath 

通过菜单路径名称执行右键菜单命令。支持动态子菜单遍历。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| path | string | ✓ | 菜单路径，用 / 分隔，如 "Playback Statistics/Rating/5" |
| trackPath | string | ✗ | 目标曲目路径（省略则用当前播放/选中项） |

**返回值**: `{ "success": true, "path": "Playback Statistics/Rating/5", "foundName": "...", "itemCount": 1 }`

### discovery.getContextMenuTree 

获取当前曲目的完整右键菜单树结构（调试用）。作用于当前播放曲目或选中项。

- **参数**: 无

**返回值**: `{ "success": true, "tree": { ... }, "itemCount": 1 }`

树节点结构：每个节点包含 `name`、`type` (`"command"` / `"popup"` / `"separator"`)、`children`（popup 类型）、`fullName`（command 类型）。最多递归 10 层，每层最多 50 个子节点。
