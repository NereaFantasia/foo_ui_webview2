# A. 播放控制 

## `<fb-play-button>` {#fb-play-button}

播放/暂停切换按钮。

```html
<fb-play-button>
  <span slot="play-icon">▶</span>
  <span slot="pause-icon">⏸</span>
</fb-play-button>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| playing | boolean | — | 只读反映：是否正在播放 |

**CSS Parts:** `button`
**Slots:** `play-icon`（默认 ▶）、`pause-icon`（默认 ⏸）
**ARIA:** `role=button`，`aria-label` 自动切换 Play/Pause

**事件：**

```js
el.addEventListener('fb-play', e => { /* 播放 */ });
el.addEventListener('fb-pause', e => { /* 暂停 */ });
```

## `<fb-stop-button>` {#fb-stop-button}

停止按钮。

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

上一首按钮。

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

下一首按钮。

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

随机播放切换。点击时在 shuffle (order 4) 和 default (order 0) 之间切换。

```html
<fb-shuffle-button>🔀</fb-shuffle-button>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| active | boolean | — | 只读反映：是否处于 shuffle 模式 |

**CSS Parts:** `button`
**Slots:** 默认 slot（默认 🔀）
**ARIA:** `aria-pressed` 自动切换

**事件：**

```js
el.addEventListener('fb-shuffle-toggle', e => {
  console.log(e.detail.active, e.detail.order);
});
```

## `<fb-repeat-button>` {#fb-repeat-button}

重复播放三态循环：off → repeat-playlist → repeat-track → off。

```html
<fb-repeat-button>
  <span slot="off">🔁</span>
  <span slot="playlist">🔁</span>
  <span slot="track">🔂</span>
</fb-repeat-button>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| mode | `'off' \| 'playlist' \| 'track'` | 'off' | 只读反映：当前模式 |

**CSS Parts:** `button`
**Slots:** `off`（默认 🔁）、`playlist`（默认 🔁）、`track`（默认 🔂）

**事件：**

```js
el.addEventListener('fb-repeat-change', e => {
  console.log(e.detail.mode, e.detail.order);
});
```

## `<fb-stop-after-current>` {#fb-stop-after-current}

播放完当前曲目后停止。

```html
<fb-stop-after-current>⏏</fb-stop-after-current>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| active | boolean | — | 只读反映：是否激活 |

**CSS Parts:** `button`
**Slots:** 默认 slot（默认 ⏏）
**ARIA:** `aria-pressed` 自动切换

**事件：**

```js
el.addEventListener('fb-stop-after-current-toggle', e => {
  console.log(e.detail.active);
});
```
