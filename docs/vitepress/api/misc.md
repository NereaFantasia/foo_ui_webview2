# 其他 API

v1.2.0 新增。提供系统路径查询和常用 UI 命令。

## Misc API - 系统路径与命令

### misc.getFoobarPath 

获取 foobar2000 安装目录路径。

**返回值**: `{ "path": "C:\\\\...\\\\foobar2000", "value": "C:\\\\...\\\\foobar2000" }`

::: tip TIP
`value` 是 `path` 的别名，两者值相同。
:::

```javascript
const result = await fb2k.invoke('misc.getFoobarPath');
console.log('安装目录:', result.path);
```

### misc.getProfilePath 

获取用户配置文件目录路径。

**返回值**: `{ "path": "C:\\\\...\\\\foobar2000\\\\profile", "value": "C:\\\\...\\\\foobar2000\\\\profile" }`

```javascript
const result = await fb2k.invoke('misc.getProfilePath');
console.log('配置目录:', result.path);
```

### misc.getComponentPath 

获取插件组件目录路径。

**返回值**: `{ "path": "C:\\\\...\\\\foobar2000\\\\user-components\\\\foo_ui_webview2", "value": "..." }`

```javascript
const result = await fb2k.invoke('misc.getComponentPath');
console.log('插件目录:', result.path);
```

### misc.showConsole 

显示 foobar2000 控制台窗口。

**返回值**: `{ "success": true }`

```javascript
await fb2k.invoke('misc.showConsole');
```

### misc.showPreferences 

打开 foobar2000 首选项对话框。

**返回值**: `{ "success": true }`

```javascript
await fb2k.invoke('misc.showPreferences');
```

### misc.showLibrarySearch 

打开媒体库搜索 UI。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| query | string | ✗ | 搜索关键词 |

**返回值**: `{ "success": true, "query": "..." }`

### misc.showPopupMessage 

显示弹出消息对话框。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| message | string | ✓ | 消息内容 |
| title | string | ✗ | 标题（默认 "Message"） |

### misc.restart 

重启 foobar2000。

**返回值**: `{ "success": true }`

### misc.exit 

退出 foobar2000。

**返回值**: `{ "success": true }`

```javascript
// 获取路径
const { path } = await fb2k.invoke('misc.getFoobarPath');
console.log('foobar2000 path:', path);

// 显示控制台
await fb2k.invoke('misc.showConsole');

// 弹出消息
await fb2k.invoke('misc.showPopupMessage', { message: 'Hello!', title: 'Test' });
```

v1.2.0 新增。提供主菜单和上下文菜单的执行和查询。

## Menu API - 菜单

### menu.runMainMenuCommand 

执行主菜单命令。支持路径形式、命令名或 GUID。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| command | string | ✓ | 菜单路径、命令名或 GUID |

```javascript
// 路径形式
await fb2k.invoke('menu.runMainMenuCommand', { command: 'File/Preferences' });
// 命令名
await fb2k.invoke('menu.runMainMenuCommand', { command: 'Preferences' });
```

### menu.runContextCommand 

执行上下文菜单命令（作用于当前选择/播放的曲目）。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| command | string | ✓ | 命令名或 GUID |

```javascript
await fb2k.invoke('menu.runContextCommand', { command: 'Playback Statistics/Rating/5' });
```

### menu.getMainMenu 

获取主菜单树结构。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| root | string | ✗ | 根菜单名（如 "File"、"Playback"），空则返回全部 |
| i18n | boolean | ✗ | 是否返回本地化名称 |
| locale | string | ✗ | 目标区域设置（配合 i18n 使用） |
| withAvailability | boolean | ✗ | 是否返回命令可用性状态 |

**返回值**: `{ "success": true, "root": "File", "items": [...] }`

每个 item 包含 `type`（"command"/"submenu"/"separator"）、`label`、`flags`、`guid`、`path`、`children` 等字段。

### menu.getContextMenu 

获取上下文菜单树结构。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| mode | string | ✗ | "auto"/"handles"/"playlist"/"nowPlaying" |
| handles | string[] | ✗ | 曲目路径数组（mode=handles 时必填） |
| i18n | boolean | ✗ | 是否返回本地化名称 |
| locale | string | ✗ | 目标区域设置（配合 i18n 使用） |
| withAvailability | boolean | ✗ | 是否返回命令可用性状态 |

**返回值**: `{ "success": true, "mode": "nowPlaying", "items": [...] }`

### menu.runContextCommandById 

通过 commandId 执行上下文菜单命令（配合 `menu.getContextMenu` 使用）。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| id | number | ✓ | 菜单项 commandId |
| mode | string | ✗ | 同 getContextMenu |
| handles | string[] | ✗ | 同 getContextMenu |

```javascript
// 获取菜单树并执行
const menu = await fb2k.invoke('menu.getContextMenu', { mode: 'nowPlaying' });
// 找到目标 item 后执行
await fb2k.invoke('menu.runContextCommandById', { id: item.commandId, mode: 'nowPlaying' });
```

### menu.showNativePopup 

在光标位置弹出 foobar2000 原生上下文菜单（Win32 TrackPopupMenu）。支持播放列表选中项、当前播放曲目或指定曲目列表。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| mode | string | ✗ | 菜单模式: "playlist"（默认，当前选中项）、"nowPlaying"（当前播放曲目）、"handles"（指定曲目） |
| handles | string[] | ✗ | 曲目路径数组（仅 mode: "handles" 时必填） |

**返回值**: `{ "success": true }`

::: tip 提示
菜单通过 Win32 `SetTimer` 延迟执行，以避免在 WebView2 桥接回调中阻塞。坐标始终使用系统光标位置（最可靠，不受 DPI/CSS 像素差异影响）。
:::

```javascript
// 播放列表选中项的原生右键菜单
await fb2k.invoke('menu.showNativePopup', { mode: 'playlist' });

// 当前播放曲目的原生右键菜单
await fb2k.invoke('menu.showNativePopup', { mode: 'nowPlaying' });

// 指定曲目的原生右键菜单
await fb2k.invoke('menu.showNativePopup', {
    mode: 'handles',
    handles: ['C:\\\\Music\\\\song.flac']
});
```

## System API 

API 发现机制和外部插件注册管理。

### system.listAvailableApis 

列出所有可用 API，包括内置和外部插件注册的 API。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| includeInternal | boolean | ✗ | 是否包含内部 API（默认 true） |
| includeExternal | boolean | ✗ | 是否包含外部插件 API |

### system.getApisByNamespace 

获取指定命名空间下的所有 API。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| namespace | string | ✓ | 命名空间名称 |

### system.searchApis 

搜索 API（支持方法名和描述的模糊匹配）。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| query | string | ✓ | 搜索关键词 |

### system.getApiStats 

获取 API 统计信息。返回内置/外部 API 数量及命名空间列表。

### system.getRegisteredPlugins 

获取所有已注册的外部插件列表。

### system.isPluginRegistered 

检查指定插件是否已注册。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| namespace | string | ✓ | 插件命名空间 |

```javascript
const result = await fb2k.invoke('system.isPluginRegistered', { namespace: 'my_plugin' });
if (result.registered) {
    await fb2k.invoke('my_plugin.doSomething');
}
```

### system.getTheme 

获取系统主题信息。

**返回值**: `{ "darkMode": true, "accentColor": "#0078D4", "transparency": true }`

```javascript
const theme = await fb2k.invoke('system.getTheme');
console.log(theme.darkMode ? '深色模式' : '浅色模式');
```

### system.getDPI 

获取当前窗口的 DPI 缩放信息。

- **参数**: 无

**返回值**:

```json
{
    "dpi": 144,
    "scale": 1.5
}
```

| 字段 | 类型 | 描述 |
| --- | --- | --- |
| dpi | number | 原始 DPI 值（96 = 100%） |
| scale | number | 缩放比例（1.0 = 100%） |

### system.getLocale 

获取系统区域设置信息。

- **参数**: 无

**返回值**:

```json
{
    "locale": "zh-CN",
    "language": "中文(简体)",
    "country": "中国"
}
```

```javascript
const locale = await fb2k.invoke('system.getLocale');
console.log(`区域: ${locale.locale}, 语言: ${locale.language}`);
```

## Test API 

### test.echo 

回显传入的消息，用于测试 bridge 连接。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| message | any | - | 要回显的内容 |

**返回值**: `{ "echo": "hello", "input": { "message": "hello" } }`

```javascript
const result = await fb2k.invoke('test.echo', { message: 'hello' });
console.log(result.echo); // "hello"
```

### test.ping 

心跳检测，返回当前服务端时间戳。

- **参数**: 无

**返回值**: `{ "pong": true, "timestamp": 1707500000 }`

```javascript
const result = await fb2k.invoke('test.ping');
console.log('pong:', result.timestamp);
```

## Panel API - 面板配置 

### panel.getConfig 

获取当前面板的配置信息。

- **参数**: 无

**返回值**:

```json
{
    "success": true,
    "config": {
        "panelName": "MyPanel",
        "templateName": "default",
        "edgeStyle": "none",
        "urlOverride": "",
        "transparentBackground": false,
        "grabFocus": true,
        "enableDragDrop": false,
        "enableDevTools": false
    }
}
```

### panel.setConfig 

设置面板配置。仅允许修改白名单字段。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| panelName | string | ✗ | 面板名称 |
| transparentBackground | boolean | ✗ | 透明背景 |
| grabFocus | boolean | ✗ | 是否获取焦点 |
| enableDragDrop | boolean | ✗ | 启用拖放 |

::: warning WARNING
`enableDevTools`、`urlOverride`、`templateName` 仅可通过配置对话框修改。
:::

**返回值**: `{ "success": true, "changed": true }`

## Clipboard API - 剪贴板 (4 个 API) 

### clipboard.read 

读取剪贴板内容。支持检测文本、文件列表、图片。

- **参数**: 无

**返回值**:

```json
{
    "success": true,
    "hasText": true,
    "hasImage": false,
    "hasFiles": true,
    "text": "剪贴板文本",
    "files": ["C:\\\\Music\\\\song.flac"]
}
```

```javascript
const clip = await fb2k.invoke('clipboard.read');
if (clip.hasText) console.log('文本:', clip.text);
if (clip.hasFiles) console.log('文件:', clip.files);
```

### clipboard.write 

写入文本到剪贴板。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| text | string | ✓ | 要写入的文本 |

**返回值**: `{ "success": true }`

### clipboard.writeHTML 

写入 HTML 到剪贴板。同时设置 CF_HTML 和 CF_UNICODETEXT 格式。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| html | string | ✓ | HTML 内容 |
| plainText | string | ✗ | 纯文本备用（默认使用 html 原值） |

**返回值**: `{ "success": true }`

```javascript
await fb2k.invoke('clipboard.writeHTML', {
    html: '<b>艺术家</b> - <i>专辑</i>',
    plainText: '艺术家 - 专辑'
});
```

### clipboard.writeFiles 

写入文件列表到剪贴板（CF_HDROP 格式，可粘贴到资源管理器）。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| paths | string[] | ✓ | 文件路径数组 |

**返回值**: `{ "success": true, "fileCount": 3 }`

```javascript
await fb2k.invoke('clipboard.writeFiles', {
    paths: ['C:\\\\Music\\\\song1.flac', 'C:\\\\Music\\\\song2.flac']
});
```

## Console & Log API - 日志 (6 个 API) 

### console.log 

输出普通日志到 foobar2000 控制台。前缀 `[WebView]`。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| message | string/any | ✗ | 日志内容（非字符串自动 JSON 序列化） |
| args | any[] | ✗ | 参数数组（空格拼接，与 message 二选一） |

**返回值**: `{ "success": true }`

```javascript
await fb2k.invoke('console.log', { message: '普通日志' });
await fb2k.invoke('console.log', { args: ['用户:', userName, '播放次数:', 42] });
```

### console.warn 

输出警告日志到 foobar2000 控制台。前缀为 `[WebView][WARN]`。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| message | string/any | ✗ | 警告内容 |
| args | any[] | ✗ | 参数数组 |

**返回值**: `{ "success": true }`

### console.error 

输出错误日志到 foobar2000 控制台。前缀为 `[WebView][ERROR]`。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| message | string/any | ✗ | 错误内容 |
| args | any[] | ✗ | 参数数组 |

**返回值**: `{ "success": true }`

### log.write 

写入日志文件。默认写入 `%profile%\\webview_ui.log`。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| message | string | ✓ | 日志内容 |
| level | string | ✗ | 日志级别（默认 info） |
| append | boolean | ✗ | 是否追加（默认 true） |
| timestamp | boolean | ✗ | 是否添加时间戳（默认 true） |
| file | string | ✗ | 自定义文件名（仅允许文件名，无路径分隔符）。扩展名限 .log / .txt（其他扩展名静默回退到默认日志文件），禁止 Windows 保留设备名和 .. 路径遍历 |

**返回值**: `{ "success": true, "path": "C:\\\\...\\\\webview_ui.log" }`

```javascript
await fb2k.invoke('log.write', {
    message: '播放开始',
    level: 'info',
    file: 'playback.log'
});
```

### log.read 

读取日志文件内容。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| lines | number | ✗ | 返回最后 N 行（默认 100） |

**返回值**:

```json
{
    "success": true,
    "content": "...",
    "lines": ["[2026-02-10 12:00:00.123][INFO] ..."],
    "lineCount": 50,
    "totalLines": 200
}
```

### log.clear 

清空日志文件。

- **参数**: 无

**返回值**: `{ "success": true }`

## Lyrics API - 歌词 (3 个 API) 

### lyrics.get 

获取曲目歌词。支持嵌入式歌词和外部 .lrc/.txt 文件。显式传入路径时会自动规范化（canonical path），支持 `path|subsong:N` 格式。支持按歌词类型和文件格式过滤。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| path | string | ✗ | 音频文件路径（省略则使用当前播放曲目） |
| source | string | ✗ | 来源: "embedded", "file", "any"（默认 "any"） |
| type | string | ✗ | 歌词类型: "synced", "unsynced", "any"（默认 "any"） |
| format | string | ✗ | 文件格式（仅 source=file 时生效）: "lrc", "txt", "any"（默认 "any"） |

**返回值**（找到时）:

```json
{
    "success": true,
    "available": true,
    "path": "C:\\\\Music\\\\song.flac",
    "source": "embedded",
    "lyrics": "[00:12.34]First line...",
    "synced": true
}
```

**返回值**（未找到）: `{ "success": true, "available": false, "path": "..." }`

> 搜索顺序：嵌入式标签（LYRICS / UNSYNCED LYRICS / SYNCEDLYRICS / UNSYNCEDLYRICS）→ 外部 .lrc → 外部 .txt

> 外部文件模式返回额外字段 `sourcePath`。type 过滤下嵌入模式返回额外字段 `tagName`。

```javascript
const result = await fb2k.invoke('lyrics.get', { source: 'any' });
if (result.available) {
    console.log(result.synced ? 'LRC 同步歌词' : '纯文本歌词');
    console.log(result.lyrics);
}

// 仅获取同步歌词
const synced = await fb2k.invoke('lyrics.get', { type: 'synced' });

// 读取 .txt 格式歌词
const txt = await fb2k.invoke('lyrics.get', { format: 'txt' });
```

### lyrics.save 

保存歌词到外部文件（.lrc/.txt）、音频文件内嵌标签和/或配置文件夹。支持一次调用同时写入多个目标。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| path | string | ✓ | 音频文件路径 |
| lyrics | string | ✓ | 歌词内容 |
| target | string \| string[] | ✗ | 保存目标: "file"（默认）/ "embedded" / "config" / "all"，或数组 ["file","config"] |
| filename | string | ✗ | 自定义文件名（默认与音频同名） |
| tagName | string | ✗ | 嵌入标签名（默认 "LYRICS"，仅 target 含 embedded） |
| format | string | ✗ | 文件格式: "lrc"（默认）/ "txt"（仅 target 含 file/config） |

**Target 说明**: `"file"` 保存到音轨同目录，`"embedded"` 写入标签，`"config"` 保存到 `%profile%\lyrics\`，`"all"` 三种全部。

**返回值**（单目标）: `{ "success": true, "savedTo": "C:\\\\Music\\\\song.lrc" }`

**返回值**（多目标）:

```json
{
    "success": true,
    "results": {
        "file": { "success": true, "savedTo": "C:\\\\Music\\\\song.lrc" },
        "embedded": { "success": true, "path": "...", "tagsApplied": { "LYRICS": "..." } },
        "config": { "success": true, "savedTo": "...\\\\lyrics\\\\song.lrc" }
    }
}
```

```javascript
// 保存到 .lrc 文件
await fb2k.invoke('lyrics.save', {
    path: 'C:\\\\Music\\\\song.flac',
    lyrics: '[00:00.00]歌词第一行\\n[00:05.00]歌词第二行'
});

// 三合一：同时写入文件、标签和配置文件夹
const all = await fb2k.invoke('lyrics.save', {
    path: 'C:\\\\Music\\\\song.flac',
    lyrics: '[00:01.00]Hello world',
    target: 'all'
});

// 数组组合
await fb2k.invoke('lyrics.save', {
    path: 'C:\\\\Music\\\\song.flac',
    lyrics: '[00:01.00]Hello world',
    target: ['file', 'config']
});
```

### lyrics.exists 

检查歌词是否存在。同时检查嵌入式标签、.lrc 文件和 .txt 文件。显式传入路径时会自动规范化（canonical path）。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| path | string | ✓ | 音频文件路径 |

**返回值**:

```json
{
    "exists": true,
    "sources": ["embedded", "file:song.lrc", "file:song.txt"]
}
```

```javascript
const { exists, sources } = await fb2k.invoke('lyrics.exists', { path: trackPath });
if (exists) console.log('歌词来源:', sources);
```

::: tip Selection API
选择管理 API（`selection.*`）已移至 [Queue & Selection API](./queue) 页面。
:::
