# UI 界面 API

提供原生弹出上下文菜单、Toast 消息通知与 Windows 系统托盘气泡通知等界面交互能力。

## UI API - 界面交互 (5 个 API)

### ui.showCustomMenu

<!-- api-schema:begin ui.showCustomMenu -->
在系统光标位置弹出原生上下文菜单并阻塞等待用户选择；选中菜单项时返回对应 `selectedId` 并向发起窗口定向投递 `ui:menuItemClicked` 事件（载荷为 `{ id, label }`）；用户取消或点击外部关闭菜单时返回 `selectedId: null`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `items` | `UiMenuItem[]` | 是 | 待弹出的原生菜单条目树结构数组 |
| `items[].id` | `string` | 否 | 菜单项唯一标识符（选中时通过 selectedId 与 ui:menuItemClicked 上报） |
| `items[].label` | `string` | 否 | 菜单项显示文本内容 |
| `items[].type` | `string` | 否 | 条目类型：设置为 "separator" 渲染为原生菜单分隔线，其他值均为普通可点击条目默认 `"item"`。 |
| `items[].enabled` | `boolean` | 否 | 菜单项是否处于可用状态（为 false 时呈现为置灰禁用）默认 `true`。 |
| `items[].checked` | `boolean` | 否 | 菜单项左侧是否显示勾选标记默认 `false`。 |
| `items[].shortcut` | `string` | 否 | 渲染在菜单项右侧的快捷键提示文本（如 "Ctrl+P"） |
| `items[].submenu` | `UiMenuItem[]` | 否 | 级联子菜单条目列表（包含子条目时该项表现为级联展开菜单，不可直接点击选中） |
| `x` | `integer` | 否 | 兼容保留参数（原生菜单固定在当前系统光标位置弹出）默认 `0`。 |
| `y` | `integer` | 否 | 兼容保留参数（原生菜单固定在当前系统光标位置弹出）默认 `0`。 |
| `w` | `integer` | 否 | 与 h 搭配指定光标下方菜单避让矩形区域的宽度（菜单将在避让区域下方或上方弹出）默认 `0`。 |
| `h` | `integer` | 否 | 与 w 搭配指定光标下方菜单避让矩形区域的高度（菜单将在避让区域下方或上方弹出）默认 `0`。 |
| `suppressDefault` | `boolean` | 否 | 兼容保留参数（已废弃无实际效果）默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `selectedId` | `string \| null` | 用户选中的菜单项标识符 id；用户取消菜单或点击外部区域关闭时为 null |

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
向发起调用窗口投递 `ui:toast` 事件以触发前端 Toast 提示（宿主不负责原生绘制，由前端页面接收 `{ message, duration, type, position }` 自行渲染）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `message` | `string` | 是 | Toast 提示正文文本内容不能为空。 |
| `duration` | `integer` | 否 | 提示显示时长（毫秒）默认 `3000`。 |
| `type` | `"info" \| "success" \| "warning" \| "error"` | 否 | Toast 提示类型：info、success、warning 或 error默认 `"info"`。 |
| `position` | `string` | 否 | 提示在屏幕中的对齐角落位置（默认 "bottom-right"）默认 `"bottom-right"`。 |

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

::: tip 前端渲染机制
`ui.showToast` 并不在原生 C++ 层面绘制 UI，而是向发起调用的页面定向投递 `ui:toast` 事件；前端需监听该事件并结合主题风格的 Toast 组件执行渲染，以确保视觉表现的一致性。
:::

### ui.showNotification

<!-- api-schema:begin ui.showNotification -->
通过插件托盘通知图标弹出 Windows 系统气泡通知（首次调用自动创建托盘图标；`title` 与 `body` 必须至少提供一项）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `title` | `string` | 否 | 系统气泡通知标题文本默认 `""`。 |
| `body` | `string` | 否 | 系统气泡通知正文文本默认 `""`。 |
| `silent` | `boolean` | 否 | 是否静音弹出通知（为 true 时不播放系统提示音）默认 `false`。 |
| `timeout` | `integer` | 否 | 通知建议展示时长（毫秒，作为提示提供给系统 Shell）默认 `5000`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `id` | `integer` | 系统通知自增序号标识符（从 1 开始递增） |

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
隐藏当前由 `ui.showNotification` 弹出的系统气泡通知（当前无活动通知时不执行任何操作）

无参数。

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### ui.showContextMenu

<!-- api-schema:begin ui.showContextMenu -->
弹出 foobar2000 主窗口上下文菜单（省略坐标、传入非正数或传入坐标与实际光标距离超过 50 像素时，自动纠正为当前系统光标位置以适配高 DPI 场景）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `x` | `integer` | 否 | 目标屏幕横坐标 X（像素；缺省或传入 -1 时自动采用当前鼠标光标横坐标）默认 `-1`。 |
| `y` | `integer` | 否 | 目标屏幕纵坐标 Y（像素；缺省或传入 -1 时自动采用当前鼠标光标纵坐标）默认 `-1`。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

::: tip 坐标自动纠正
当传入坐标与实际鼠标光标位置偏差超过 50 像素时，宿主会自动改用当前系统光标位置，以确保在高 DPI 缩放与多显示器环境下的弹窗定位准确性。
:::

```javascript
// 右键事件中弹出原生菜单
document.addEventListener('contextmenu', (e) => {
    e.preventDefault();
    fb2k.invoke('ui.showContextMenu', { x: e.screenX, y: e.screenY });
});
```

## 交互投递与限制

`ui.showCustomMenu` 固定在当前系统光标位置弹出原生菜单，并仅向发起调用的页面定向派发 `ui:menuItemClicked` 事件；用户取消菜单或点击外部区域时返回 `selectedId: null`。`ui.showToast` 不执行原生控件绘制，而是向调用窗口派发 `ui:toast` 事件，交由前端主题自行实现渲染呈现。
