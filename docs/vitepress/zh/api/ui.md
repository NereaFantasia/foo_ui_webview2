# UI 界面 API

原生菜单、Toast 与气泡通知等界面交互。

## UI API - 界面交互 (5 个 API)

### ui.showCustomMenu

<!-- api-schema:begin ui.showCustomMenu -->
在系统光标处弹出原生菜单并等待选择。选中的行以 `selectedId` 报回，并向调用窗口发 `ui:menuItemClicked`（`{ id, label }`）；菜单被取消时 `selectedId` 为 `null`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `items` | `UiMenuItem[]` | 是 | 各行。 |
| `items[].id` | `string` | 否 | 以 `selectedId` 与 `ui:menuItemClicked` 报回的标识。 |
| `items[].label` | `string` | 否 | 显示文本。 |
| `items[].type` | `string` | 否 | `separator` 画分隔线；其他值都是普通行。默认 `"item"`。 |
| `items[].enabled` | `boolean` | 否 | 是否可选。默认 `true`。 |
| `items[].checked` | `boolean` | 否 | 是否显示勾选标记。默认 `false`。 |
| `items[].shortcut` | `string` | 否 | 画在文字右侧的快捷键提示。 |
| `items[].submenu` | `UiMenuItem[]` | 否 | 子行；带这个键的行展开子菜单而不能被选中。 |
| `x` | `integer` | 否 | 兼容保留；菜单固定在系统光标处弹出。默认 `0`。 |
| `y` | `integer` | 否 | 兼容保留；菜单固定在系统光标处弹出。默认 `0`。 |
| `w` | `integer` | 否 | 与 `h` 一起给出光标下方一块菜单不得遮盖的矩形；菜单在其下方弹出。默认 `0`。 |
| `h` | `integer` | 否 | 见 `w`。默认 `0`。 |
| `suppressDefault` | `boolean` | 否 | 兼容保留；没有作用。默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `selectedId` | `string \| null` | 选中行的 id；菜单被取消时为 `null`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

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
if (result.success === false) throw new Error(result.error);
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

<!-- api-schema:begin ui.showToast -->
让调用窗口的页面显示一条 toast：宿主不绘制，只把载荷以 `ui:toast`（`{ message, duration, type, position }`）发给该窗口。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `message` | `string` | 是 | toast 文本。不能为空。 |
| `duration` | `integer` | 否 | 显示时长，单位毫秒。默认 `3000`。 |
| `type` | `"info" \| "success" \| "warning" \| "error"` | 否 | toast 种类，原样传给页面。默认 `"info"`。 |
| `position` | `string` | 否 | 屏幕角落，原样传给页面。默认 `"bottom-right"`。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

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

<!-- api-schema:begin ui.showNotification -->
从插件的通知图标显示一条 Windows 气泡通知，首次使用时创建图标。`title` 与 `body` 至少给一个。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `title` | `string` | 否 | 通知标题。默认 `""`。 |
| `body` | `string` | 否 | 通知正文。默认 `""`。 |
| `silent` | `boolean` | 否 | `true` 不播放通知提示音。默认 `false`。 |
| `timeout` | `integer` | 否 | 显示时长，单位毫秒，供 shell 参考。默认 `5000`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `id` | `integer` | 通知的序号，从 `1` 起。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const res = await fb2k.invoke('ui.showNotification', {
    title: '正在播放',
    body: 'Artist - Song Title',
    timeout: 5000
});
if (res.success === false) throw new Error(res.error);
const { id } = res;
```

### ui.hideNotification

<!-- api-schema:begin ui.hideNotification -->
隐藏 `showNotification` 显示的气泡；没显示过时什么也不做。

无参数。

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### ui.showContextMenu

<!-- api-schema:begin ui.showContextMenu -->
打开主窗口自己的上下文菜单。省略、非正数或与真实光标相差超过 50 px 的坐标改用光标位置。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `x` | `integer` | 否 | 屏幕 x，单位像素。默认 `-1`。 |
| `y` | `integer` | 否 | 屏幕 y，单位像素。默认 `-1`。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

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

## 交互投递与限制

`ui.showCustomMenu` 使用当前光标位置放置 native 菜单，并只向调用者路由
`ui:menuItemClicked`。取消菜单会成功返回 `selectedId: null`。`ui.showToast` 不在
native 代码中绘制 UI，而是向调用者发射 `ui:toast`，因此主题负责渲染。
