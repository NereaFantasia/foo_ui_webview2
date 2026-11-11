# Keyboard 键盘 API

提供 Windows 系统级全局热键与 WebView 应用内快捷键的注册、管理与事件监听能力。

## Keyboard API - 快捷键 (5 个 API) {#keyboard-api-快捷键-4-个-api}

### keyboard.registerHotkey

<!-- api-schema:begin keyboard.registerHotkey -->
注册系统级全局热键：无论焦点在哪个程序，按下时都向发起注册的窗口派发 `keyboard:hotkey`（载荷为 `{ id, key, action }`）。组合须含 Ctrl、Alt 或 Win，单按媒体键除外；Ctrl+A、C、V、X、Y、Z，Ctrl+Shift+Z，Alt+F4、Alt+Tab、Alt+Space 与 Ctrl+Esc 也会被拒绝，均返回 `INVALID_PARAMS`。本组件的某个窗口已持有该组合时以 `OPERATION_FAILED` 失败，错误信息写明持有的窗口；被其他程序或 foobar2000 的全局快捷键占用、Windows 拒绝注册时同样返回 `OPERATION_FAILED`，`details.win32Error` 为 Windows 错误码。注册页面导航或重载、所在窗口关闭时，热键随之注销

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `key` | `string` | 是 | 待注册的按键组合字符串不能为空。 |
| `action` | `string` | 是 | 触发热键时随 `keyboard:hotkey` 事件载荷回传的动作名称不能为空。 |
| `global` | `boolean` | 否 | 是否标记为全局热键（记录于条目属性中供 `getRegisteredHotkeys` 查询，实际均注册为系统级热键）默认 `true`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `id` | `integer` | 分配的热键唯一标识符（自 `1` 开始递增，注销时可传给 `unregisterHotkey`） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

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

<!-- api-schema:begin keyboard.registerShortcut -->
已弃用：没有任何效果，3.0 移除；请改为在页面里监听 `keydown`。

以指定动作名登记一个按键组合，但不会有任何代码按它匹配按键。通过 `getRegisteredHotkeys` 列出时 `id` 为 `0`，用 `unregisterHotkey` 传入 key 字符串移除

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `key` | `string` | 是 | 待登记的按键组合字符串不能为空。 |
| `action` | `string` | 是 | 与快捷键关联绑定的动作名称不能为空。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### keyboard.unregisterHotkey

<!-- api-schema:begin keyboard.unregisterHotkey -->
注销已注册的热键或应用内快捷键（支持通过数字 `id` 注销热键，或通过按键组合 `key` 字符串注销热键或快捷键；两者必须恰好提供一项）。注销快捷键的用法随 `registerShortcut` 在 3.0 移除

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `id` | `integer` | 否 | 待注销热键的数值标识符 id（取自 `registerHotkey` 的返回值）不小于 `1`。 |
| `key` | `string` | 否 | 待注销热键或快捷键的原始按键组合字符串 key |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
await fb2k.invoke('keyboard.unregisterHotkey', { id: result.id });
// 或
await fb2k.invoke('keyboard.unregisterHotkey', { key: 'Ctrl+Alt+Space' });
```

### keyboard.getRegisteredHotkeys

<!-- api-schema:begin keyboard.getRegisteredHotkeys -->
获取已注册的全部全局热键及应用内快捷键列表（先列出系统热键，后列出应用内快捷键）。应用内快捷键条目随 `registerShortcut` 在 3.0 移除

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `hotkeys` | `KeyboardHotkey[]` | 已注册条目列表（系统级全局热键在前，应用内快捷键在后） |
| `hotkeys[].id` | `integer` | 热键标识符 id（系统级全局热键大于等于 1，应用内快捷键固定为 `0`） |
| `hotkeys[].key` | `string` | 注册时配置的按键组合字符串 |
| `hotkeys[].action` | `string` | 关联绑定的动作名称 |
| `hotkeys[].global` | `boolean` | 是否为全局热键（全局热键为 true，应用内快捷键为 false） |
| `hotkeys[].windowId` | `string` | 注册该热键的窗口 id，即按下时事件投递到的窗口。应用内快捷键的条目，以及调用方匹配不到窗口的热键，应答里没有此字段 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```json
{
    "success": true,
    "hotkeys": [
        { "id": 1, "key": "Ctrl+Alt+Space", "action": "play_pause", "global": true }
    ]
}
```

### keyboard.setReservedKeys

<!-- api-schema:begin keyboard.setReservedKeys -->
全量替换当前页面的保留按键列表。页面聚焦时，组件首选项选定的按键执行 foobar2000「首选项 > 键盘快捷键」中为其绑定的快捷键；列表中指定的保留键始终优先由页面自身接收处理。保留列表在页面导航、重新加载或所在窗口关闭时清空。若存在无法解析的按键字符串，调用以 `INVALID_PARAMS` 失败且现有列表保持不变；传入空数组可释放全部保留键。设置 `all: true` 时所有按键均由页面接收（如快捷键录制场景）；系统级热键与开发服务器模式下的 F12 快捷键除外。页面的 `keydown` 监听没有调用 `preventDefault()` 时，F5 重载页面。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `keys` | `string[]` | 是 | 留给页面的按键组合，写法与 `registerHotkey` 的 key 相同；重复项无妨 |
| `all` | `boolean` | 否 | 是否把全部按键都留给页面，而不只是 `keys`。之后再调用一次不带它的，就回到只保留 `keys`默认 `false`。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
// 页面自己处理 Ctrl+F 与 Ctrl+S，即使 foobar2000 给它们绑了快捷键
await fb2k.invoke('keyboard.setReservedKeys', { keys: ['Ctrl+F', 'Ctrl+S'] });
// 全部释放
await fb2k.invoke('keyboard.setReservedKeys', { keys: [] });
// 录制快捷键期间把全部按键留给页面，录完再调一次不带 all 的
await fb2k.invoke('keyboard.setReservedKeys', { keys: ['Ctrl+F', 'Ctrl+S'], all: true });
```

## WebView 内的快捷键

页面获得焦点时，按键会交给 foobar2000「首选项 > 键盘快捷键」里设置的快捷键：用户在那里绑定的主菜单与右键菜单命令照常生效，没有设置快捷键的按键仍交给页面。哪些按键参与由用户在组件首选项「WebView2 UI > 窗口 > 快捷键」里决定，默认是含 Ctrl、Alt、Win 的组合与 F1 到 F24。主题读不到也改不了这项设置。

焦点在输入框里（input、textarea、select 或可编辑内容）时，打字与移动光标的按键留给页面：字母、数字、空格、标点，不带 Alt 或 Win 的方向键、Home、End，以及 PageUp、PageDown。焦点在别处时，这些键同样交给快捷键：不带修饰键或只带 Shift 的归首选项里的「其他按键」，默认关；Ctrl+方向键、Ctrl+Home、Ctrl+End 归「Ctrl 组合」。快捷键执行了，这次按键的默认动作就取消，例如滚动页面、激活获得焦点的按钮。

以下按键始终留给页面：Ctrl+A、C、V、X、Y、Z，Ctrl+Shift+Z，Ctrl+Insert，退格、Delete、Esc、Tab，不带 Alt 或 Win 的回车，以及开发服务器模式下的 F12。F5 不交给 foobar2000 的快捷键：页面的 `keydown` 监听没有调用 `preventDefault()` 时，组件重载页面。页面正在加载、加载失败或来源不可信时，F5 直接重载页面，页面收不到这次按键。右键菜单类命令作用于主题经 `selection` 交给宿主的选区。

主题自己的快捷键与 foobar2000 的绑定撞车时，用 [`keyboard.setReservedKeys`](#keyboard-setreservedkeys) 把这些组合留给页面。焦点不在输入框时，打字与移动光标的按键要等页面的全部 `keydown` 监听执行完（包括 `window` 上的）才交给快捷键，页面调用 `preventDefault()` 即可留下这类按键。其余组合在页面收到之前就已交给快捷键，只能用保留键留给页面。

## 交互投递与限制

`keyboard.registerHotkey` 向 Windows 注册系统级全局热键，触发时将 `keyboard:hotkey` 事件定向派发给发起注册的窗口；注册页面导航、重载或所在窗口关闭时，热键随之注销。`keyboard.registerShortcut` 已废弃：只登记，不匹配按键，3.0 移除。两类注册方法均要求传入非空的 `key` 与 `action`；`keyboard.unregisterHotkey` 必须在数字 `id` 与原始 `key` 按键组合字符串中恰好指定一项。
