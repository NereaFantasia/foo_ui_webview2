# Cursor API

显式控制客户区光标的可见性。

> 此 API 命名空间为 `cursor.*`，不支持别名。

## 背景：为什么需要 Cursor API

`foo_ui_webview2` 使用 Visual Hosting (`CompositionController`) 模式承载 WebView2，宿主窗口不会把 HWND 转交给 Chromium，所有 `WM_SETCURSOR` 消息都由 foobar2000 父窗口处理。

实测下来，仅靠 CSS `cursor: none` 隐藏光标在该模式下并不可靠：Chromium 的 `CursorChanged` 回调只在 hover 元素的 cursor 类型 **切换** 时触发，纯粹通过样式规则隐藏鼠标无法稳定地通知到 C++ 层，光标会出现"隐藏后无法恢复"或"空闲状态忽然显现"等问题。

`cursor.*` 提供了一条显式的 JS → C++ 通道，全屏播放器、桌面歌词等需要"空闲隐藏"行为的主题应通过它驱动隐藏。

::: tip 与 CSS 的关系
推荐"CSS + Cursor API"双管齐下：

- CSS `cursor: none` 仍是首选的视觉表达方式，对于鼠标在 WebView 内 hover 的场景能直接生效。
- 当主题需要在 **没有 hover 切换** 时隐藏（如全屏播放器空闲 3 秒后），调用 `cursor.setHidden(true)` 让 C++ 层在父窗口的 `WM_SETCURSOR` 中拦截 `SetCursor(nullptr)`。
:::

## API 列表

### cursor.setHidden

<!-- api-schema:begin cursor.setHidden -->
隐藏或恢复调用窗口客户区的光标。只有真正改变了状态的调用才向该窗口发 `cursor:hiddenChanged`；重复同一个值报 `changed: false`。每个窗口各自维护状态。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `hidden` | `boolean` | 是 | `true` 隐藏光标，`false` 恢复。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `changed` | `boolean` | 状态是否改变；已经是请求的状态时为 `false`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

::: tip 幂等行为
连续多次以同一 `hidden` 值调用，host 只会在第一次实际翻转标志位的调用上触发 `cursor:hiddenChanged` 事件，后续调用返回 `success: true, changed: false`。主题层因此可以用引用计数或 `Set` 协调多个独立的隐藏请求。
:::

```javascript
// 全屏播放器空闲隐藏
let idleTimer;
function scheduleHide() {
    clearTimeout(idleTimer);
    idleTimer = setTimeout(() => fb2k.invoke('cursor.setHidden', { hidden: true }), 3000);
}

window.addEventListener('mousemove', () => {
    fb2k.invoke('cursor.setHidden', { hidden: false });
    scheduleHide();
});

scheduleHide();
```

### cursor.isHidden

<!-- api-schema:begin cursor.isHidden -->
报告调用窗口的光标是否隐藏；解析不到调用窗口时为 `false`。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `hidden` | `boolean` | 光标是否隐藏。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const res = await fb2k.invoke('cursor.isHidden');
if (res.success === false) throw new Error(res.error);
const { hidden } = res;
if (hidden) restoreCursor();
```

## 事件

### cursor:hiddenChanged

仅在 `cursor.setHidden` 真实翻转标志位时由调用窗口派发。**不会**跨窗口广播：每个 popup 维护各自的隐藏状态。

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| hidden | boolean | 翻转后的新状态 |

```javascript
fb2k.on('cursor:hiddenChanged', ({ hidden }) => {
    document.documentElement.classList.toggle('cursor-hidden', hidden);
});
```

## 使用 SDK

```javascript
import { fb } from 'foo-webview-sdk/bridge';

// 隐藏 / 显示
await fb.cursor.setHidden(true);
await fb.cursor.setHidden(false);

// 查询
const res = await fb.cursor.isHidden();
if (res.success === false) throw new Error(res.error);
const { hidden } = res;

// 监听变化
const off = fb.on('cursor:hiddenChanged', ({ hidden }) => {
    console.log('cursor hidden =>', hidden);
});
off();
```

## 限制与注意事项

- **作用域为客户区**：`cursor.setHidden(true)` 只影响 WebView 客户区，无法控制窗口边框、标题栏或系统范围内的鼠标。
- **每窗口独立**：popup 与主窗口、popup 与 popup 之间状态互相隔离。
- **不替代 CSS**：当 hover 路径上某个元素仍声明了 `cursor: pointer` 等显式样式，foreground 鼠标处理依然会按 hover 路径优先生效；推荐主题样式与 API 协同，不要相互冲突。
- **关闭窗口自动恢复**：popup 关闭时其内部 hidden 标志自然失效，无需主题主动 `setHidden(false)`。

## Cursor 可见性生命周期

`cursor.setHidden` 只改变调用 WebView host 的光标状态，不影响桌面或其他 popup。
它要求 boolean 类型的 `hidden`；若请求状态已经生效，返回 `changed: false`。
状态实际变化时，调用者收到 `cursor:hiddenChanged` 及 payload `{ hidden }`；该事件不会
广播到其他窗口。

`foo_ui_webview2` 使用 Visual Hosting，因此单独使用 CSS `cursor: none` 并不能可靠
替代此 API。主题可结合二者：CSS 处理元素级视觉规则，API 处理空闲时间等宿主级行为。

```js
import { fb } from 'foo-webview-sdk/bridge';

await fb.cursor.setHidden(true);
const res = await fb.cursor.isHidden();
if (res.success === false) throw new Error(res.error);
const { hidden } = res;
fb.on('cursor:hiddenChanged', ({ hidden: next }) => {
    document.documentElement.classList.toggle('cursor-hidden', next);
});
```
