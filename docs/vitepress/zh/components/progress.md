# B. 进度与音量

## `<fb-seek-bar>` {#fb-seek-bar}

播放进度条，支持鼠标拖拽、滚轮与键盘步进定位。

```html
<fb-seek-bar></fb-seek-bar>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `position` | string | — | 只读反映当前播放位置（整秒数值字符串） |
| `duration` | string | — | 只读反映当前曲目总时长（整秒数值字符串） |
| `progress` | string | — | 只读反映当前播放进度比例（0 到 1 数值字符串） |

**CSS Parts:** `track`, `fill`, `thumb`  
**CSS 自定义属性:** `--fb-seek-progress`（0–1）、`--fb-seek-hover-progress`（悬停时 0–1）  
**键盘控制:** 方向键以 ±5 秒步进；Home 键跳转曲首，End 键跳转曲末

**事件：**

```js
el.addEventListener('fb-seek', e => {
  console.log(e.detail.position); // 最终提交的 seek 位置（秒）
});
el.addEventListener('fb-seeking', e => {
  console.log(e.detail.position); // 拖拽过程中的即时位置（秒）
});
```

**样式示例：**

```css
fb-seek-bar::part(track) { background: #333; height: 4px; }
fb-seek-bar::part(fill) { background: #1db954; }
fb-seek-bar::part(thumb) { width: 12px; height: 12px; background: white; border-radius: 50%; }
```

## `<fb-volume-control>` {#fb-volume-control}

线性音量调节滑块，集成静音切换按钮。

```html
<fb-volume-control></fb-volume-control>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `vertical` | boolean | false | 启用自底向上的垂直滑块布局（observed） |
| `no-icon` | boolean | false | 隐藏静音按钮（observed） |
| `volume` | string | — | 只读反映当前音量数值（0–100 线性范围数值字符串） |
| `muted` | boolean | — | 只读反映当前是否处于静音状态 |

**CSS Parts:** `mute-button`, `track`, `fill`, `thumb`  
**Slots:** `icon`（默认 🔊）  
**CSS 自定义属性:** `--fb-volume`（0–1）  
**键盘控制:** 方向键以 ±5 步进；Home 键置为 0，End 键置为 100  
**鼠标滚轮:** 以 ±5 步进调节

**事件：**

```js
el.addEventListener('fb-volume-change', e => {
  console.log(e.detail.volume); // 0–100
});
el.addEventListener('fb-mute-toggle', e => { /* 静音切换 */ });
```

## `<fb-playback-order>` {#fb-playback-order}

播放顺序选择器，支持原生下拉菜单（select）与循环按钮（button）两种呈现模式。

```html
<fb-playback-order mode="button"></fb-playback-order>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `mode` | `'select' \| 'button'` | `'button'` | 呈现模式（observed，仅取值 `select` 时渲染为下拉框，其余情况为按钮） |
| `order` | string | — | 只读反映当前播放顺序名称 |

**CSS Parts:** `button`（button 模式）或 `select`（select 模式）  
**Slots:** 默认 slot（button 模式下的文本标签）  
**顺序枚举:** `default`, `repeat-playlist`, `repeat-track`, `random`, `shuffle-tracks`, `shuffle-albums`, `shuffle-folders`

**事件：**

```js
el.addEventListener('fb-order-change', e => {
  console.log(e.detail.order, e.detail.name);
});
```
