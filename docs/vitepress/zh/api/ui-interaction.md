# UI & Keyboard & DnD API

界面交互、快捷键注册、拖放操作。共12 个 API。

## UI API - 界面交互 (5 个 API)

### ui.showCustomMenu

显示自定义右键菜单。支持子菜单、分隔线、快捷键提示、勾选状态。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `items` | `array` | 是 | — | 菜单项数组，结构见下表。 |
| `x` | `integer` | 否 | `0` | 当前实现忽略此参数，菜单固定在系统光标处弹出。 |
| `y` | `integer` | 否 | `0` | 当前实现忽略此参数。 |
| `w` | `integer` | 否 | `0` |  |
| `h` | `integer` | 否 | `0` |  |
| `suppressDefault` | `boolean` | 否 | `false` |  |

**菜单项结构**:

| 字段 | 类型 | 描述 |
| --- | --- | --- |
| `id` | string | 菜单项标识，选中时经 `selectedId` 返回 |
| `label` | string | 显示文本 |
| `type` | string | `separator` 表示分隔线 |
| `enabled` | boolean | 是否可用（默认 true） |
| `checked` | boolean | 勾选状态 |
| `shortcut` | string | 快捷键提示文本 |
| `submenu` | array | 子菜单项数组 |

**返回值**: `{"selectedId":"...","success":true}`

用户取消菜单时 `selectedId` 为 `null`。

**事件**: 点击菜单项时触发 `ui:menuItemClicked` 事件，携带 `{ id, label }`。

```javascript
const result = await fb2k.invoke('ui.showCustomMenu', {
    items: [
        { id: 'play', label: '播放', shortcut: 'Enter' },
        { type: 'separator' },
        { id: 'edit', label: '编辑', submenu: [
            { id: 'rename', label: '重命名' },
            { id: 'delete', label: '删除', enabled: false }
        ]},
        { id: 'favorite', label: '收藏', checked: true }
    ],
    x: event.clientX,
    y: event.clientY,
    suppressDefault: true
});
if (result.selectedId) {
    console.log('选中:', result.selectedId);
}

// 监听菜单项点击事件
fb2k.on('ui:menuItemClicked', (data) => {
    console.log(`菜单项 ${data.id} (${data.label}) 被点击`);
    // 处理菜单项点击逻辑
    switch (data.id) {
        case 'play':
            fb2k.invoke('playback.play');
            break;
        case 'rename':
            showRenameDialog();
            break;
    }
});
```

### ui.showToast

显示 Toast 提示。通过触发 `ui:toast` 事件由前端渲染。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `message` | `string` | 是 | — | Toast 文本。 |
| `type` | `string` | 否 | `info` | 可取 `info` / `success` / `warning` / `error`。 |
| `duration` | `integer` | 否 | `3000` | 显示时长（毫秒）。 |
| `position` | `string` | 否 | `bottom-right` |  |

**返回值**: `{ "success": true }`

**事件**: 触发 `ui:toast` 事件，携带 `{ message, duration, type, position }`。

```javascript
// 调用 API 显示 Toast
await fb2k.invoke('ui.showToast', {
    message: '已添加到播放列表',
    type: 'success',
    duration: 2000
});

// 监听 Toast 事件（由前端渲染）
fb2k.on('ui:toast', (data) => {
    // 使用自定义 Toast 组件渲染
    showToast({
        message: data.message,
        type: data.type,
        duration: data.duration,
        position: data.position
    });
});
```

::: tip 提示
`ui.showToast` 不直接渲染 Toast，而是触发 `ui:toast` 事件。前端需要监听该事件并使用自己的 Toast 组件渲染。这样可以保持 UI 风格的一致性。
:::

### ui.showNotification

显示系统托盘通知（Windows Balloon Notification）。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `title` | `string` | 否 | — | 通知标题。 |
| `body` | `string` | 否 | — | 通知正文。 |
| `timeout` | `integer` | 否 | `5000` | 显示时长（毫秒）。 |
| `silent` | `boolean` | 否 | `false` |  |

**返回值**: `{ "success": true, "id": 1 }`

```javascript
const { id } = await fb2k.invoke('ui.showNotification', {
    title: '正在播放',
    body: 'Artist - Song Title',
    timeout: 5000
});
```

### ui.hideNotification

隐藏当前显示的系统托盘通知。

- **参数**: 无

**返回值**: `{ "success": true }`

### ui.showContextMenu

显示 foobar2000 原生上下文菜单。通常用于响应右键事件，在指定坐标位置弹出原生菜单。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `x` | `integer` | 否 | `-1` | 省略或 `-1` 时使用当前鼠标位置。 |
| `y` | `integer` | 否 | `-1` | 省略或 `-1` 时使用当前鼠标位置。 |

**返回值**: `{ "success": true }`

::: tip 提示
坐标与实际鼠标位置差距超过 50 像素时，会自动使用当前鼠标位置以确保 DPI 缩放场景下的准确性。
:::

```javascript
// 右键事件中弹出原生菜单
document.addEventListener('contextmenu', (e) => {
    e.preventDefault();
    fb2k.invoke('ui.showContextMenu', { x: e.screenX, y: e.screenY });
});
```

## Keyboard API - 快捷键 (4 个 API)

### keyboard.registerHotkey

注册全局热键。热键触发时通过 `keyboard:hotkey` 事件通知。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `key` | `string` | 是 | — | 组合键，如 `Ctrl+Alt+Space`。 |
| `action` | `string` | 是 | — | 动作名，原样出现在 `keyboard:hotkey` 载荷中。 |
| `global` | `boolean` | 否 | `true` |  |

**返回值**: `{ "success": true, "id": 1 }`

**支持的修饰键**: `Ctrl`/`Control`, `Alt`, `Shift`, `Win`

**支持的按键**: A-Z, 0-9, F1-F12, Space, Enter, Tab, Escape, Backspace, Delete, Insert, Home, End, PageUp, PageDown, 方向键, 媒体键 (PlayPause/MediaStop/NextTrack/PrevTrack/VolumeUp/VolumeDown/VolumeMute), 标点符号

**事件**: `keyboard:hotkey` — 携带 `{ id, key, action }`

```javascript
const result = await fb2k.invoke('keyboard.registerHotkey', {
    key: 'Ctrl+Alt+Space',
    action: 'play_pause',
    global: true
});
// result.id 可用于后续 unregisterHotkey

fb2k.on('keyboard:hotkey', (data) => {
    if (data.action === 'play_pause') fb2k.invoke('playback.playOrPause');
});
```

### keyboard.registerShortcut

注册 WebView 应用内快捷键（非全局）。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `key` | `string` | 是 | 组合键，如 `Ctrl+Shift+L`。 |
| `action` | `string` | 是 | 随快捷键保存的动作名。 |

**返回值**: `{ "success": true }`

### keyboard.unregisterHotkey

注销热键。支持按 ID 或按 key 字符串注销。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `id` | `string` | 否 | 注册时返回的数字 id。 |
| `key` | `string` | 否 | 注册时的原始 key 字符串。 |

**返回值**: `{ "success": true }`

```javascript
await fb2k.invoke('keyboard.unregisterHotkey', { id: result.id });
// 或
await fb2k.invoke('keyboard.unregisterHotkey', { key: 'Ctrl+Alt+Space' });
```

### keyboard.getRegisteredHotkeys

获取所有已注册的热键列表。

- **参数**: 无

**返回值**:

```json
{
    "success": true,
    "hotkeys": [
        { "id": 1, "key": "Ctrl+Alt+Space", "action": "play_pause", "global": true }
    ]
}
```

## DnD API - 外部文件拖入 (3 个 API)

Windows 把拖入的文件列表交给原生窗口而非页面，且 HTML5 `File` 对象隐藏文件系统
路径，因此本命名空间作为**旁路通道**提供宿主视角的真实路径。它不替代 HTML5 拖放：
`dragenter` / `dragover` / `drop` 照原样触发。

事件：`dnd:enter`、`dnd:leave`、`dnd:drop`、`dnd:capabilitiesChanged`。**没有**
`dnd:over` 事件——一次拖放会产生上百次 `DragOver`，逐次发射会淹没 bridge；需要跟踪
光标的页面用 HTML5 `dragover`。

### dnd.getCapabilities

查询本窗口的拖放集成当前能提供什么。

- **参数**: 无

**返回值**:

```json
{
    "success": true,
    "html5": true,
    "paths": true,
    "hosting": "visual",
    "pathsUnavailableReason": "origin-untrusted"
}
```

`html5` 与 `paths` 相互独立：面板模式宿主会失去路径旁路通道，而 HTML5 拖放事件仍然
工作（Chromium 自行处理）。`hosting` 为 `"visual"`（主窗口 / 弹出窗口）或
`"standard"`（DUI / CUI 面板，此时路径不可用）。`pathsUnavailableReason` 仅当
`paths` 为 `false` 时出现，取值为 `origin-untrusted`、`inner-target-not-found`、
`forward-unavailable`、`chain-failed`、`displaced`、`register-failed` 之一。

能力在窗口生命周期内并非恒定：导航到不同 origin 会收回路径访问权，并发射
`dnd:capabilitiesChanged`，载荷携带同样的四个字段。

```javascript
const caps = await fb2k.invoke('dnd.getCapabilities');

fb2k.on('dnd:capabilitiesChanged', (data) => {
    dropZoneEl.hidden = !data.paths;
});
```

### dnd.getPathsAsync

查询某次拖放会话的真实文件系统路径。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `sessionId` | `string` | 否 | 要查询的会话，取自 `dnd:*` 载荷；省略则查询本窗口当前活动或最近结束的会话。 |

**返回值**: `{ "success": true, "sessionId": "dnd-1-12345", "paths": [], "resolvedPaths": [] }`

在 HTML5 `drop` 处理函数中取路径的可靠方式：读宿主会话状态而非推送到页面的快照，
不依赖消息投递顺序。路径顺序与 `DataTransfer.files` 一致，可按下标配对。

`resolvedPaths` 携带快捷方式（`.lnk`）的目标路径，**长度恒等于 `paths`**，因此对一个
数组有效的下标对另一个同样有效；非快捷方式的项为 `null`。它与 `paths` 同时被清空，
不会单独为空。

会话不存在、已过期、未携带文件列表，或 origin 不被信任时，`paths` 为空数组。会话
存储是 per-window 的，仅凭 id 无法读取其他窗口的路径。

```javascript
document.addEventListener('drop', async (event) => {
    event.preventDefault();
    const { paths } = await fb2k.invoke('dnd.getPathsAsync');
    if (paths.length) {
        await fb2k.invoke('playlist.addPaths', { paths });
    }
});
```

### dnd.startDrag

把内容从窗口**拖出**到其他应用。

- **参数**: 无

**返回值**: 总是返回 `NOT_SUPPORTED` 错误信封。

::: tip 注意
拖出需要 `IDropSource` 实现与宿主产出的数据对象，两者都不存在。它显式失败而非伪造
`success: true`，避免调用方基于虚假的成功继续往下做。

该 Promise 会 **resolve** 这个信封而不是 reject——宿主把 handler 返回的错误信封当作
正常结果投递，因此应判断 `success`，不要依赖 `catch`。
:::

## 交互投递与限制

`ui.showCustomMenu` 使用当前光标位置放置 native 菜单，并只向调用者路由
`ui:menuItemClicked`。取消菜单会成功返回 `selectedId: null`。`ui.showToast` 不在
native 代码中绘制 UI，而是向调用者发射 `ui:toast`，因此主题负责渲染。

`keyboard.registerHotkey` 注册 Windows 热键，随后将 `keyboard:hotkey` 路由到
注册它的窗口。`registerShortcut` 只保存应用内快捷键。两个注册方法都要求非空
`key` 和 `action`；`unregisterHotkey` 接受数字 `id` 或原始 key 字符串。

`dnd.getPathsAsync` 与 `dnd.getCapabilities` 都从消息自带的 HWND 解析调用窗口，
因此页面无法读取其他窗口的拖放会话。不受信任的 origin 拿不到路径，而 HTML5 拖放
事件仍然工作，因此应对 `paths` 与 `html5` 分别判断，而不是一并隐藏全部拖放提示。
`dnd.startDrag` 会报告拖出目前的 native 限制，而不是实现 OLE drag source。
