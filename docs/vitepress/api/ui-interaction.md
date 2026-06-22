# UI & Keyboard & DnD API 

界面交互、快捷键注册、拖放操作。共13 个 API。

## UI API - 界面交互 (5 个 API) 

### ui.showCustomMenu 

显示自定义右键菜单。支持子菜单、分隔线、快捷键提示、勾选状态。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| items | array | ✓ | 菜单项数组 |
| x | number | ✗ | X 坐标（默认 0） |
| y | number | ✗ | Y 坐标（默认 0） |
| w | number | ✗ | 排除区域宽度（菜单不会覆盖此区域） |
| h | number | ✗ | 排除区域高度 |
| suppressDefault | boolean | ✗ | 强制禁用 WebView 默认右键菜单 |

**菜单项结构**:

| 字段 | 类型 | 描述 |
| --- | --- | --- |
| id | string | 菜单项标识 |
| label | string | 显示文本 |
| type | string | "separator" 表示分隔线 |
| enabled | boolean | 是否可用（默认 true） |
| checked | boolean | 勾选状态（默认 false） |
| shortcut | string | 快捷键提示文本 |
| submenu | array | 子菜单项数组 |

**返回值**: `{ "success": true, "selectedId": "play" }`

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

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| message | string | ✓ | 提示消息 |
| duration | number | ✗ | 显示时长 ms（默认 3000） |
| type | string | ✗ | 类型: info, success, warning, error（默认 info） |
| position | string | ✗ | 位置（默认 bottom-right） |

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

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| title | string | ✗ | 通知标题 |
| body | string | ✗ | 通知内容（title 和 body 至少需要一个） |
| silent | boolean | ✗ | 是否静音（默认 false） |
| timeout | number | ✗ | 超时时间 ms（默认 5000） |

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

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| x | number | ✗ | 屏幕 X 坐标（默认使用当前鼠标位置） |
| y | number | ✗ | 屏幕 Y 坐标（默认使用当前鼠标位置） |

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

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| key | string | ✓ | 组合键字符串，如 "Ctrl+Alt+Space" |
| action | string | ✓ | 热键触发时的动作标识 |
| global | boolean | ✗ | 是否为全局热键（默认 true） |

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

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| key | string | ✓ | 组合键字符串 |
| action | string | ✓ | 动作标识 |

**返回值**: `{ "success": true }`

### keyboard.unregisterHotkey 

注销热键。支持按 ID 或按 key 字符串注销。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| id | number | ✗ | 注册时返回的热键 ID |
| key | string | ✗ | 组合键字符串（id 和 key 二选一） |

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

## DnD API - 拖放 (4 个 API) 

### dnd.registerDropZone 

注册拖放区域。返回用于设置拖放区域的 JavaScript 代码。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| selector | string | ✓ | CSS 选择器 |
| accept | string[] | ✗ | 接受的拖放类型: "files", "text", "tracks"（默认 ["files"]） |
| event | string | ✗ | 回调事件名（默认 "dnd:drop"） |

**返回值**:

```json
{
    "success": true,
    "zoneId": "dropzone_1",
    "selector": "#playlist-area",
    "accept": ["files", "tracks"],
    "event": "dnd:drop",
    "script": "..."
}
```

> `script` 字段包含自动注入的 JavaScript，用于设置 DOM 元素的 dragover/dragleave/drop 事件。

```javascript
await fb2k.invoke('dnd.registerDropZone', {
    selector: '#playlist-area',
    accept: ['files', 'tracks'],
    event: 'dnd:drop'
});

fb2k.on('dnd:drop', (data) => {
    console.log('Zone:', data.zoneId);
    console.log('Files:', data.files);
    console.log('Text:', data.text);
});
```

> 事件总线统一使用 colon 命名约定，与全局 `fb2k.on()` 监听一致。

### dnd.unregisterDropZone 

注销拖放区域。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| zoneId | string | ✓ | 注册时返回的 zoneId |

**返回值**: `{ "success": true, "zoneId": "dropzone_1", "script": "..." }`

### dnd.startDrag 

启动拖拽操作（用于从 WebView 拖出内容）。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| type | string | ✓ | 数据类型: "text", "tracks", "files" |
| data | string | ✗ | 文本数据（type=text 时） |
| paths | string[] | ✗ | 路径数组（type=tracks/files 时） |

**返回值**: `{ "success": true, "type": "text" }`

::: tip注意 原生拖拽操作需要 OLE drag-drop 实现。对于 tracks/files 类型，建议使用 `playlist.add` 或文件选择对话框代替。 :::

### dnd.getDropZones 

获取所有已注册的拖放区域。

- **参数**: 无

**返回值**:

```json
{
    "success": true,
    "zones": [
        { "id": "dropzone_1", "selector": "#playlist-area", "accept": ["files"], "event": "dnd:drop" }
    ],
    "count": 1
}
```
