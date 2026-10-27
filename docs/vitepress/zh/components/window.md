# E. 窗口管理

## `<fb-titlebar>` {#fb-titlebar}

自定义窗口标题栏拖拽区域组件。双击切换最大化与还原状态，右键呼出系统窗口菜单。

```html
<fb-titlebar>
  <span slot="left">foobar2000</span>
  <span slot="right"><fb-window-controls></fb-window-controls></span>
</fb-titlebar>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `maximized` | boolean | — | 只读反映当前窗口是否处于最大化状态 |

**CSS Parts:** `drag-region`  
**Slots:** `left`, `right`（插槽内容从拖拽区域中排除）

**事件：**

```js
el.addEventListener('fb-titlebar-dblclick', e => {
  console.log('双击标题栏');
});
```

## `<fb-window-controls>` {#fb-window-controls}

无边框窗口控制按钮组（最小化、最大化/还原、关闭）。根据 `maximized` 状态互斥显示 `maximize-button` 与 `restore-icon`。

```html
<fb-window-controls>
  <span slot="minimize-icon">—</span>
  <span slot="maximize-icon">□</span>
  <span slot="restore-icon">❐</span>
  <span slot="close-icon">✕</span>
</fb-window-controls>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `maximized` | boolean | — | 只读反映当前窗口是否处于最大化状态 |

**CSS Parts:** `controls`, `minimize-button`, `maximize-button`, `restore-icon`, `close-button`  
**Slots:** `minimize-icon`（默认 —）, `maximize-icon`（默认 □）, `restore-icon`（默认 ❐）, `close-icon`（默认 ✕）

**事件：**

```js
el.addEventListener('fb-window-minimize', e => { /* 最小化 */ });
el.addEventListener('fb-window-maximize', e => { /* 最大化/还原 */ });
el.addEventListener('fb-window-close', e => { /* 关闭 */ });
```

## `<fb-popup-panel>` {#fb-popup-panel}

声明式弹出窗口触发器与容器组件。从 DOM 移除元素时自动关闭对应弹窗。

```html
<fb-popup-panel url="settings.html" width="600" height="400" popup-title="设置">
  ⚙️ 打开设置
</fb-popup-panel>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `url` | string | `''` | 弹窗页面 URL |
| `width` | string | `'400'` | 弹窗初始宽度（像素） |
| `height` | string | `'300'` | 弹窗初始高度（像素） |
| `popup-title` | string | `''` | 弹窗窗口标题 |
| `resizable` | string | enabled | 允许调整大小；设置为 exact 字符串 `false` 时禁用调整 |
| `always-on-top` | boolean | false | 启用窗口置顶（observed） |
| `show-in-taskbar` | boolean | false | 在系统任务栏显示图标（observed） |
| `frame` | string | enabled | 启用窗口边框；设置为 exact 字符串 `false` 时为无边框窗口 |
| `transparent` | boolean | false | 启用窗口背景透明（observed） |
| `before-close` | boolean | false | 启用窗口关闭前确认拦截（observed） |
| `open` | boolean | — | 只读反映弹窗当前是否处于打开状态 |
| `window-id` | string | — | 只读反映当前弹窗的窗口 ID |

**Slots:** 默认 slot（触发弹窗的按钮内容）

**JS API：**

```js
const windowId = await el.open();  // 打开弹窗，返回 windowId
el.close();                         // 关闭弹窗
console.log(el.windowId);           // 当前窗口 ID
```

**事件：**

```js
el.addEventListener('fb-popup-open', e => {
  console.log('弹窗打开:', e.detail.windowId);
});
el.addEventListener('fb-popup-close', e => {
  console.log('弹窗关闭:', e.detail.windowId);
});
el.addEventListener('fb-popup-message', e => {
  console.log('弹窗消息:', e.detail.sourceWindowId, e.detail.message);
});
```
