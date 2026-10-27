# A. 播放控制

## `<fb-play-button>` {#fb-play-button}

播放与暂停状态切换按钮，内部自动同步播放器运行状态。

```html
<fb-play-button>
  <span slot="play-icon">▶</span>
  <span slot="pause-icon">⏸</span>
</fb-play-button>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `playing` | boolean | — | 只读反映当前播放状态 |

**CSS Parts:** `button`  
**Slots:** `play-icon`（默认 ▶）、`pause-icon`（默认 ⏸）  
**ARIA:** `role="button"`，`aria-label` 在 `Play` 与 `Pause` 之间自动切换

**事件：**

```js
el.addEventListener('fb-play', e => { /* 播放 */ });
el.addEventListener('fb-pause', e => { /* 暂停 */ });
```

## `<fb-stop-button>` {#fb-stop-button}

停止播放按钮，点击后停止当前曲目播放。

```html
<fb-stop-button>⏹</fb-stop-button>
```

**CSS Parts:** `button`  
**Slots:** 默认 slot（默认 ⏹）

**事件：**

```js
el.addEventListener('fb-stop', e => { /* 停止 */ });
```

## `<fb-prev-button>` {#fb-prev-button}

切换上一首按钮，点击后跳至播放列表中上一首曲目。

```html
<fb-prev-button>⏮</fb-prev-button>
```

**CSS Parts:** `button`  
**Slots:** 默认 slot（默认 ⏮）

**事件：**

```js
el.addEventListener('fb-prev', e => { /* 上一首 */ });
```

## `<fb-next-button>` {#fb-next-button}

切换下一首按钮，点击后跳至播放列表中下一首曲目。

```html
<fb-next-button>⏭</fb-next-button>
```

**CSS Parts:** `button`  
**Slots:** 默认 slot（默认 ⏭）

**事件：**

```js
el.addEventListener('fb-next', e => { /* 下一首 */ });
```

## `<fb-shuffle-button>` {#fb-shuffle-button}

双态随机播放控制按钮。在非随机顺序下激活切换至曲目随机（order 4，`shuffle-tracks`）；在任一随机顺序（3–6）下激活则还原至默认顺序（order 0，`default`）。

```html
<fb-shuffle-button>🔀</fb-shuffle-button>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `active` | boolean | — | 只读反映当前是否处于随机播放模式（适用于播放顺序 3–6） |

**CSS Parts:** `button`  
**Slots:** 默认 slot（默认 🔀）  
**ARIA:** `aria-pressed` 反映激活状态

**事件：**

```js
el.addEventListener('fb-shuffle-toggle', e => {
  console.log(e.detail.active, e.detail.order);
});
```

## `<fb-repeat-button>` {#fb-repeat-button}

循环切换重复模式按钮：按顺序循环 `off` → `repeat-playlist` → `repeat-track` → `off`；当前处于随机播放时首次激活直接切换至 `repeat-playlist`。

```html
<fb-repeat-button>
  <span slot="off">🔁</span>
  <span slot="playlist">🔁</span>
  <span slot="track">🔂</span>
</fb-repeat-button>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `mode` | `'off' \| 'playlist' \| 'track'` | `'off'` | 只读反映当前解析后的重复播放模式 |

**CSS Parts:** `button`  
**Slots:** `off`（默认 🔁）、`playlist`（默认 🔁）、`track`（默认 🔂）

**事件：**

```js
el.addEventListener('fb-repeat-change', e => {
  console.log(e.detail.mode, e.detail.order);
});
```

## `<fb-stop-after-current>` {#fb-stop-after-current}

切换 foobar2000「播放完当前曲目后停止」设置的控制按钮。

```html
<fb-stop-after-current>⏏</fb-stop-after-current>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `active` | boolean | — | 只读反映当前是否已开启该设置 |

**CSS Parts:** `button`  
**Slots:** 默认 slot（默认 ⏏）  
**ARIA:** `aria-pressed` 反映激活状态

**事件：**

```js
el.addEventListener('fb-stop-after-current-toggle', e => {
  console.log(e.detail.active);
});
```
