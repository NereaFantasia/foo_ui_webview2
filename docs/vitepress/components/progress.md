# B. 进度与音量 

## `<fb-seek-bar>` {#fb-seek-bar}

播放进度条，支持拖拽和键盘控制。

```html
<fb-seek-bar></fb-seek-bar>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| position | string | — | 只读反映：当前秒数 |
| duration | string | — | 只读反映：总时长秒数 |
| progress | string | — | 只读反映：进度 0-1 |

**CSS Parts:** `track`, `fill`, `thumb`
**CSS 自定义属性:** `--fb-seek-progress`（0-1）、`--fb-seek-hover-progress`（0-1，hover 时）
**键盘:** Arrow ±5s，Home/End 跳转首尾

**事件：**

```js
el.addEventListener('fb-seek', e => {
  console.log(e.detail.position); // 最终 seek 位置（秒）
});
el.addEventListener('fb-seeking', e => {
  console.log(e.detail.position); // 拖拽过程中的位置
});
```

**样式示例：**

```css
fb-seek-bar::part(track) { background: #333; height: 4px; }
fb-seek-bar::part(fill) { background: #1db954; }
fb-seek-bar::part(thumb) { width: 12px; height: 12px; background: white; border-radius: 50%; }
```

## `<fb-volume-control>` {#fb-volume-control}

音量控制，含静音按钮和滑块。

```html
<fb-volume-control></fb-volume-control>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| vertical | boolean | false | 垂直布局（observed） |
| no-icon | boolean | false | 隐藏静音按钮（observed） |
| volume | string | — | 只读反映：当前音量 0-100 |
| muted | boolean | — | 只读反映：是否静音 |

**CSS Parts:** `mute-button`, `track`, `fill`, `thumb`
**Slots:** `icon`（默认 🔊）
**CSS 自定义属性:** `--fb-volume`（0-1）
**键盘:** Arrow ±5，Home=0，End=100
**滚轮:** ±5

**事件：**

```js
el.addEventListener('fb-volume-change', e => {
  console.log(e.detail.volume); // 0-100
});
el.addEventListener('fb-mute-toggle', e => { /* 静音切换 */ });
```

## `<fb-playback-order>` {#fb-playback-order}

播放顺序选择器，支持下拉菜单和按钮循环两种模式。

```html
<fb-playback-order mode="button"></fb-playback-order>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| mode | `'select' \| 'button'` | 'button' | 显示模式（observed） |
| order | string | — | 只读反映：当前顺序名 |

**CSS Parts:** `button`（button 模式）或 `select`（select 模式） **Slots:** 默认 slot（button 模式文本） **7 种顺序:** default / repeat-playlist / repeat-track / random / shuffle-tracks / shuffle-albums / shuffle-folders

**事件：**

```js
el.addEventListener('fb-order-change', e => {
  console.log(e.detail.order, e.detail.name);
});
```
