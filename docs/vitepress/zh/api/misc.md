# 其他 API

v1.2.0 新增。提供系统路径查询和常用 UI 命令。

## Misc API - 系统路径与命令

### misc.getFoobarPath

获取 foobar2000 安装目录路径。

**返回值**: `{ "path": "C:\\...\\foobar2000", "value": "C:\\...\\foobar2000" }`

::: tip TIP
`value` 是 `path` 的别名，两者值相同。
:::

```javascript
const result = await fb2k.invoke('misc.getFoobarPath');
console.log('安装目录:', result.path);
```

### misc.getProfilePath

获取用户配置文件目录路径。

**返回值**: `{ "path": "C:\\...\\foobar2000\\profile", "value": "C:\\...\\foobar2000\\profile" }`

```javascript
const result = await fb2k.invoke('misc.getProfilePath');
console.log('配置目录:', result.path);
```

### misc.getComponentPath

获取插件组件目录路径。

**返回值**: `{ "path": "C:\\...\\foobar2000\\user-components\\foo_ui_webview2", "value": "..." }`

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

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `query` | `string` | 否 | 预填入搜索框的关键词；省略则打开空白搜索。 |

**返回值**: `{ "success": true, "query": "..." }`

### misc.showPopupMessage

显示弹出消息对话框。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `message` | `string` | 否 | 对话框正文；省略则弹出空内容对话框。 |
| `msg` | `string` | 否 | 早期别名，仅在未传 `message` 时生效；新代码请用 `message`。 |
| `title` | `string` | 否 | 标题栏文本；默认 `Message`。 |

::: tip 提示
早期版本的 `msg` 仍被接受，但已弃用。同时传入时以 `message` 为准，新代码请只写 `message`。
:::

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

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `command` | `string` | 是 | GUID、叶子命令名或斜杠分隔路径。空值直接以 `command is required` 失败。 |
| `subGuid` | `string` | 否 | 动态子命令的 `subGuid`；此时 `command` 传其所属命令的 GUID。 |


**返回值**: `{"guid":"...","success":true}`

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

```javascript
// 推荐：按 GUID 寻址
await fb2k.invoke('menu.runMainMenuCommand', {
    command: '{11213A01-9F36-4E69-A1BB-7A72F418DE3A}',
});

// 路径形式（仅在标签语言与宿主一致时可用）
await fb2k.invoke('menu.runMainMenuCommand', { command: '文件/首选项' });

// 动态子命令需要「所属命令 GUID + subGuid」
await fb2k.invoke('menu.runMainMenuCommand', {
    command: '{41D98AF1-8C4F-4F0E-8B7A-1A4B0F7B1234}',
    subGuid: '{A222D5A9-2903-AA8C-EEAE-4B9230558B55}',
});
```

### menu.runContextCommand

执行上下文菜单命令（作用于当前选择/播放的曲目）。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `command` | `string` | 是 | GUID 或斜杠分隔的命令路径。空值直接以 `command is required` 失败。 |
| `subGuid` | `string` | 否 | 动态生成子项的节点 GUID。不传则命中其父容器，等于什么都不执行。 |


**返回值**: `{"guid":"...","itemCount":"...","executionConfirmed":true,"success":true}`

`executionConfirmed: false` 表示命令是通过一个不返回结果的入口交给宿主的，因此无法观测是否真的执行。
只有在某个注册项没有稳定 GUID 时才会出现。

```javascript
await fb2k.invoke('menu.runContextCommand', { command: 'Playback Statistics/Rating/5' });

// 动态子项需要「所属命令 GUID + subGuid」
await fb2k.invoke('menu.runContextCommand', {
    command: '{5B69B9E3-1C7C-4C63-A9B0-1D0C0D0F0E0D}',
    subGuid: '{A222D5A9-2903-AA8C-EEAE-4B9230558B55}',
});
```

### menu.getMainMenu

获取主菜单树结构。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `i18n` | `boolean` | 否 | 是否本地化菜单标签；默认 `true`。 |
| `locale` | `string` | 否 | 本地化使用的区域标识；默认 `auto`（跟随宿主）。 |
| `root` | `string` | 否 | 只返回该顶级菜单名下的子树；省略则返回整棵主菜单。 |
| `withAvailability` | `boolean` | 否 | 是否附带启用/勾选等可用性字段；默认 `true`。 |

**返回值**: `{"error":"...","fallback":"...","items":[],"success":true}`

每个 item 包含 `type`（"command"/"submenu"/"separator"）、`label`、`flags`、`guid`、`path`、`children` 等字段。

### menu.getContextMenu

获取上下文菜单树结构。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `handles` | `array` | 否 | 目标曲目列表，元素可为路径字符串（可带 `\|subsong:N`）或 `{ path, subsong }` 对象。`mode: 'handles'` 时必须提供。 |
| `i18n` | `boolean` | 否 | 是否本地化菜单标签；默认 `true`。 |
| `locale` | `string` | 否 | 本地化使用的区域标识；默认 `auto`（跟随宿主）。 |
| `mode` | `string` | 否 | 取值为 `auto`、`selection`、`playlist`、`nowPlaying`、`handles` 之一；默认 `auto`，其他任何值同样按 `auto` 处理。 |
| `withAvailability` | `boolean` | 否 | 是否附带启用/勾选等可用性字段；默认 `true`。 |

**返回值**: `{ "success": true, "mode": "nowPlaying", "items": [...] }`

`auto` 按「`handles` → 当前播放 → 播放列表选中项 → 播放列表本身」顺序取目标，实际采用的模式由返回的 `mode` 给出。

### menu.runContextCommandById

通过 commandId 执行上下文菜单命令（配合 `menu.getContextMenu` 使用）。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `id` | `integer` | 是 | 来自 `menu.getContextMenu` 的 `commandId`。缺失、非整数或负数一律以 `id is required` 失败。 |
| `mode` | `string` | 否 | 取值为 `auto`、`selection`、`playlist`、`nowPlaying`、`handles` 之一；默认 `auto`，其他任何值同样按 `auto` 处理。 |
| `handles` | `array` | 否 | 目标曲目列表；`mode: 'handles'` 时必须提供。 |

::: warning 注意
`commandId` 是某次 `menu.getContextMenu` 结果内的序号，只在相同 `mode` 与相同目标曲目下有效。执行时请沿用取菜单时的 `mode`。
:::

```javascript
// 获取菜单树并执行
const menu = await fb2k.invoke('menu.getContextMenu', { mode: 'nowPlaying' });
// 找到目标 item 后执行
await fb2k.invoke('menu.runContextCommandById', { id: item.commandId, mode: 'nowPlaying' });
```

### menu.showNativePopup

在光标位置弹出 foobar2000 原生上下文菜单（Win32 TrackPopupMenu）。支持活动播放列表选中项、播放列表级上下文、当前播放曲目或指定曲目列表。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `handles` | `array` | 否 | 目标曲目列表，元素可为路径字符串（可带 `\|subsong:N`）或 `{ path, subsong }` 对象。`mode: 'handles'` 时必须提供。 |
| `mode` | `string` | 否 | 取值为 `auto`、`selection`、`playlist`、`nowPlaying`、`handles` 之一；默认 `auto`，其他任何值同样按 `auto` 处理。 |

**返回值**: `{ "success": true }`

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

## System API

API 发现机制和外部插件注册管理。

### system.listAvailableApis

列出所有可用 API，包括内置和外部插件注册的 API。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `includeExternal` | `boolean` | 否 | 是否包含外部插件注册的 API；默认 `true`。 |
| `includeInternal` | `boolean` | 否 | 是否包含内置 API；默认 `true`。 |

### system.getApisByNamespace

获取指定命名空间下的所有 API。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `namespace` | `string` | 是 | 命名空间名，如 `playback`。空值以 `namespace is required` 失败。 |

### system.searchApis

搜索 API（支持方法名和描述的模糊匹配）。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `query` | `string` | 是 | 搜索关键词。空值以 `query is required` 失败。 |

### system.getApiStats


**返回值**: `{"registered":"...","success":true}`

获取 API 统计信息。返回内置/外部 API 数量及命名空间列表。

### system.getRegisteredPlugins


**返回值**: `{"registered":"...","success":true}`

获取所有已注册的外部插件列表。

### system.isPluginRegistered

检查指定插件是否已注册。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `namespace` | `string` | 是 | 待检查的插件命名空间。空值以 `namespace is required` 失败。 |


**返回值**: `{"registered":"...","success":true}`

```javascript
const result = await fb2k.invoke('system.isPluginRegistered', { namespace: 'my_plugin' });
if (result.registered) {
    await fb2k.invoke('my_plugin.doSomething');
}
```

### system.getTheme

获取系统主题信息。

**返回值**: `{"accentColor":"...","darkMode":"...","isDark":true,"transparency":"..."}`

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
| `dpi` | number | dpi |
| `scale` | number | 缩放比例（1.0 = 100%） |

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

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `message` | `json` | 否 | 任意 JSON 值，原样回显到 `echo`；省略时 `echo` 为整个参数对象。 |

**返回值**: `{"echo":"...","input":"...","success":true}`

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

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `enableDragDrop` | `boolean` | 否 | 是否允许拖放到面板；省略则保持当前值。 |
| `grabFocus` | `boolean` | 否 | 是否允许面板抢占焦点；省略则保持当前值。 |
| `panelName` | `string` | 否 | 面板显示名；省略则保持当前值。 |
| `transparentBackground` | `boolean` | 否 | 是否使用透明背景；省略则保持当前值。 |

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
    "files": ["C:\\Music\\song.flac"]
}
```

```javascript
const clip = await fb2k.invoke('clipboard.read');
if (clip.hasText) console.log('文本:', clip.text);
if (clip.hasFiles) console.log('文件:', clip.files);
```

### clipboard.write

写入文本到剪贴板。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `text` | `string` | 是 | 待写入的文本。空值以 `text is required` 失败。 |

**返回值**: `{ "success": true }`

### clipboard.writeHTML

写入 HTML 到剪贴板。同时设置 CF_HTML 和 CF_UNICODETEXT 格式。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `html` | `string` | 是 | HTML 片段，写入 CF_HTML。空值以 `html is required` 失败。 |
| `plainText` | `string` | 否 | 同时写入 CF_UNICODETEXT 的纯文本回退内容。 |

**返回值**: `{"htmlWritten":"...","success":true,"textWritten":"..."}`

```javascript
await fb2k.invoke('clipboard.writeHTML', {
    html: '<b>艺术家</b> - <i>专辑</i>',
    plainText: '艺术家 - 专辑'
});
```

### clipboard.writeFiles

写入文件列表到剪贴板（CF_HDROP 格式，可粘贴到资源管理器）。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `array` | 是 | 必填。 |

**返回值**: `{ "success": true, "fileCount": 3 }`

```javascript
await fb2k.invoke('clipboard.writeFiles', {
    paths: ['C:\\Music\\song1.flac', 'C:\\Music\\song2.flac']
});
```

## Console & Log API - 日志 (6 个 API)

### console.log

输出普通日志到 foobar2000 控制台。前缀 `[WebView]`。

需提供 `message` 或 `args` 之一；空载荷返回 `message is required`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `message` | `string` | 否 | 可选日志文本；非字符串会序列化。 |
| `args` | `array` | 否 | 当省略 `message` 时，用空格拼接的参数列表。 |

**返回值**: `{ "success": true }`

```javascript
await fb2k.invoke('console.log', { message: '普通日志' });
await fb2k.invoke('console.log', { args: ['用户:', userName, '播放次数:', 42] });
```

### console.warn

输出警告日志到 foobar2000 控制台。前缀为 `[WebView][WARN]`。

需提供 `message` 或 `args` 之一；空载荷返回 `message is required`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `message` | `string` | 否 | 可选日志文本；非字符串会序列化。 |
| `args` | `array` | 否 | 当省略 `message` 时，用空格拼接的参数列表。 |

**返回值**: `{ "success": true }`

### console.error

输出错误日志到 foobar2000 控制台。前缀为 `[WebView][ERROR]`。

需提供 `message` 或 `args` 之一；空载荷返回 `message is required`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `message` | `string` | 否 | 可选日志文本；非字符串会序列化。 |
| `args` | `array` | 否 | 当省略 `message` 时，用空格拼接的参数列表。 |

**返回值**: `{ "success": true }`

### log.write

写入日志文件。默认写入 `%profile%\\webview_ui.log`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `message` | `string` | 否 | 日志文本；非字符串会序列化。与 `args` 至少提供其一，否则返回 `message is required`。 |
| `args` | `array` | 否 | 当省略 `message` 时，用空格拼接的参数列表。 |
| `file` | `string` | 否 | 配置目录下的目标文件名；默认 `webview_ui.log`。 |
| `level` | `string` | 否 | 级别文本，转为大写后写入行前缀；默认 `info`。 |
| `append` | `boolean` | 否 | `true` 追加写入，`false` 覆盖重写；默认 `true`。 |
| `timestamp` | `boolean` | 否 | 是否在行首写入时间戳；默认 `true`。 |

::: tip 提示
`file` 只接受不含路径分隔符的裸文件名，扩展名限 `.log` / `.txt`，且不能是 Windows 保留设备名；不满足时回落到默认日志文件。
:::

**返回值**: `{ "success": true, "path": "C:\\...\\webview_ui.log" }`

```javascript
await fb2k.invoke('log.write', {
    message: '播放开始',
    level: 'info',
    file: 'playback.log'
});
```

### log.read

读取日志文件内容。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `lines` | `integer` | 否 | 读取末尾多少行；默认 `100`。负数以 `lines must be non-negative` 失败。 |

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

## 合同补充

以下章节补齐严格参数审计发现的公开 contract；不会改变前文的已有说明。

### menu.close

关闭当前打开的自绘菜单浮层（若有）。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `reason` | `string` | 否 | `api` | 可选关闭原因，传给菜单宿主；默认 `api`。 |

**返回字段**: `{ "success": true }`

```js
const result = await fb2k.invoke('menu.close', { reason: 'api' });
```

### menu.show

打开自绘菜单浮层并返回其 id；用户的选择稍后通过 `menu:select` 与 `menu:dismiss` 事件送达。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `items` | `array` | 否 | 可省略 | 菜单项数组；省略或非数组时按空菜单处理。 |
| `x` | `integer` | 否 | `-1` | 屏幕横坐标；负数或省略时取当前光标位置。 |
| `y` | `integer` | 否 | `-1` | 屏幕纵坐标；负数或省略时取当前光标位置。 |

**返回字段**: `{ "success": true, "menuId": "..." }`

```js
const { menuId } = await fb2k.invoke('menu.show', {
    items: [
        { id: 'play', label: '播放' },
        { id: 'enqueue', label: '加入播放列表' },
    ],
});
```

<!-- phase3-supplement:log.write -->
### Contract 补充：`log.write`

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `message` | `string` | 否 | 可省略 | 日志文本；非字符串会序列化。与 `args` 至少提供其一，否则返回 `message is required`。 |
| `args` | `array` | 否 | `[]` | 当省略 `message` 时，用空格拼接的参数列表。 |
| `file` | `string` | 否 | 可省略 | 配置目录下的目标文件名；默认 `webview_ui.log`。 |
| `level` | `string` | 否 | `info` | 级别文本，转为大写后写入行前缀。 |
| `append` | `boolean` | 否 | `true` | `true` 追加写入，`false` 覆盖重写。 |
| `timestamp` | `boolean` | 否 | `true` | 是否在行首写入时间戳。 |

#### 返回字段

| 字段 | 类型 | 可选 |
| --- | --- | --- |
| `error` | `string` | 是 |
| `success` | `boolean` | 否 |
| `path` | `json` | 否 |

语义：省略可选参数时使用 handler 默认值；失败分支及错误字段以该源文件为准。

```js
await fb2k.invoke('log.write', { message: '播放开始' });
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

语义：省略可选参数时使用 handler 默认值；失败分支及错误字段以该源文件为准。

```js
const { items, mode } = await fb2k.invoke('menu.getContextMenu');
```
<!-- phase3-supplement:menu.runContextCommandById -->
### Contract 补充：`menu.runContextCommandById`

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `id` | `integer` | 是 | 无 | 来自 `menu.getContextMenu` 的 `commandId`。缺失、非整数或负数一律以 `id is required` 失败。 |
| `mode` | `string` | 否 | `auto` | 取值为 `auto`、`selection`、`playlist`、`nowPlaying`、`handles` 之一；其他任何值同样按 `auto` 处理。请沿用取菜单时的 `mode`。 |
| `handles` | `array` | 否 | `[]` | 目标曲目列表；`mode: 'handles'` 时必须提供。 |

#### 返回字段

| 字段 | 类型 | 可选 |
| --- | --- | --- |
| `error` | `string` | 是 |
| `success` | `boolean` | 否 |

语义：省略可选参数时使用 handler 默认值；失败分支及错误字段以该源文件为准。

```js
const { items } = await fb2k.invoke('menu.getContextMenu');
await fb2k.invoke('menu.runContextCommandById', { id: items[0].commandId });
```
<!-- phase3-supplement:misc.showPopupMessage -->
### Contract 补充：`misc.showPopupMessage`

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `message` | `string` | 否 | `` | 对话框正文。 |
| `msg` | `string` | 否 | `` | 早期别名，仅在未传 `message` 时生效。 |
| `title` | `string` | 否 | `Message` | 标题栏文本。 |

#### 返回字段

| 字段 | 类型 | 可选 |
| --- | --- | --- |
| `success` | `boolean` | 否 |

语义：省略可选参数时使用 handler 默认值；失败分支及错误字段以该源文件为准。

```js
await fb2k.invoke('misc.showPopupMessage', { message: '导出完成' });
```
