# E. 窗口管理 

## `<fb-titlebar>` {#fb-titlebar}

自定义标题栏拖拽区域。双击切换最大化/还原，右键显示系统菜单。

```html
<fb-titlebar>
  <span slot="left">foobar2000</span>
  <span slot="right"><fb-window-controls></fb-window-controls></span>
</fb-titlebar>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| maximized | boolean | — | 只读反映：窗口是否最大化 |

**CSS Parts:** `drag-region`
**Slots:** `left`, `right`（不参与拖拽）

**事件：**

```js
el.addEventListener('fb-titlebar-dblclick', e => {
  console.log('双击标题栏');
});
```

## `<fb-window-controls>` {#fb-window-controls}

窗口控制按钮（最小化/最大化/关闭）。`maximized` 时 maximize-button 和 restore-icon 互斥显示。

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
| maximized | boolean | — | 只读反映：窗口是否最大化 |

**CSS Parts:** `controls`, `minimize-button`, `maximize-button`, `restore-icon`, `close-button`
**Slots:** `minimize-icon`（默认 —）, `maximize-icon`（默认 □）, `restore-icon`（默认 ❐）, `close-icon`（默认 ✕）

**事件：**

```js
el.addEventListener('fb-window-minimize', e => { /* 最小化 */ });
el.addEventListener('fb-window-maximize', e => { /* 最大化/还原 */ });
el.addEventListener('fb-window-close', e => { /* 关闭 */ });
```

## `<fb-popup-panel>` {#fb-popup-panel}

弹出面板，`disconnectedCallback` 自动关闭弹窗。

```html
<fb-popup-panel url="settings.html" width="600" height="400" popup-title="设置">
  ⚙️ 打开设置
</fb-popup-panel>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| url | string | — | 弹窗页面 URL |
| width | string | '400' | 弹窗宽度 |
| height | string | '300' | 弹窗高度 |
| popup-title | string | — | 弹窗标题 |
| resizable | boolean | true | 允许调整大小 |
| always-on-top | boolean | false | 置顶 |
| show-in-taskbar | boolean | false | 在任务栏显示 |
| frame | boolean | true | 显示窗口边框 |
| transparent | boolean | false | 背景透明 |
| before-close | boolean | — | 设置后关闭前弹出确认 |
| open | boolean | — | 只读反映：弹窗是否打开 |
| window-id | string | — | 只读反映：弹窗窗口 ID |

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
  console.log('弹窗消息:', e.detail);
});
```
