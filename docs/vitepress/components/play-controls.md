# A. Playback Controls

## `<fb-play-button>` {#fb-play-button}

Play/pause toggle button.

```html
<fb-play-button>
  <span slot="play-icon">▶</span>
  <span slot="pause-icon">⏸</span>
</fb-play-button>
```

| Attribute | Type | Default | Description |
| --- | --- | --- | --- |
| `playing` | boolean | — | Read-only reflection of the playing state |

**CSS Parts:** `button`  
**Slots:** `play-icon` (default: ▶), `pause-icon` (default: ⏸)  
**ARIA:** `role="button"`; `aria-label` changes between `Play` and `Pause`

**Events:**

```js
el.addEventListener('fb-play', e => { /* entered the playing state */ });
el.addEventListener('fb-pause', e => { /* entered the paused state */ });
```

## `<fb-stop-button>` {#fb-stop-button}

Stops playback.

```html
<fb-stop-button>⏹</fb-stop-button>
```

**CSS Parts:** `button`
**Slots:** default slot (default: ⏹)

**Events:**

```js
el.addEventListener('fb-stop', e => { /* playback stopped */ });
```

## `<fb-prev-button>` {#fb-prev-button}

Skips to the previous track.

```html
<fb-prev-button>⏮</fb-prev-button>
```

**CSS Parts:** `button`
**Slots:** default slot (default: ⏮)

**Events:**

```js
el.addEventListener('fb-prev', e => { /* previous track selected */ });
```

## `<fb-next-button>` {#fb-next-button}

Skips to the next track.

```html
<fb-next-button>⏭</fb-next-button>
```

**CSS Parts:** `button`
**Slots:** default slot (default: ⏭)

**Events:**

```js
el.addEventListener('fb-next', e => { /* next track selected */ });
```

## `<fb-shuffle-button>` {#fb-shuffle-button}

Two-state shuffle control. Activating it from any non-shuffle order selects `shuffle-tracks` (order 4); activating it from any shuffle order (3–6) selects `default` (order 0).

```html
<fb-shuffle-button>🔀</fb-shuffle-button>
```

| Attribute | Type | Default | Description |
| --- | --- | --- | --- |
| `active` | boolean | — | Read-only reflection; present for playback orders 3–6 |

**CSS Parts:** `button`
**Slots:** default slot (default: 🔀)
**ARIA:** `aria-pressed` reflects the active state

**Events:**

```js
el.addEventListener('fb-shuffle-toggle', e => {
  console.log(e.detail.active, e.detail.order);
});
```

## `<fb-repeat-button>` {#fb-repeat-button}

Cycles through `off` → `repeat-playlist` → `repeat-track` → `off`. If the current order is random or a shuffle order, the first activation selects `repeat-playlist`.

```html
<fb-repeat-button>
  <span slot="off">🔁</span>
  <span slot="playlist">🔁</span>
  <span slot="track">🔂</span>
</fb-repeat-button>
```

| Attribute | Type | Default | Description |
| --- | --- | --- | --- |
| `mode` | `'off' \| 'playlist' \| 'track'` | `'off'` | Read-only reflection of the resolved repeat mode |

**CSS Parts:** `button`
**Slots:** `off` (default: 🔁), `playlist` (default: 🔁), `track` (default: 🔂)

**Events:**

```js
el.addEventListener('fb-repeat-change', e => {
  console.log(e.detail.mode, e.detail.order);
});
```

## `<fb-stop-after-current>` {#fb-stop-after-current}

Toggles the foobar2000 “stop after current track” setting.

```html
<fb-stop-after-current>⏏</fb-stop-after-current>
```

| Attribute | Type | Default | Description |
| --- | --- | --- | --- |
| `active` | boolean | — | Read-only reflection of whether the setting is enabled |

**CSS Parts:** `button`
**Slots:** default slot (default: ⏏)
**ARIA:** `aria-pressed` reflects the active state

**Events:**

```js
el.addEventListener('fb-stop-after-current-toggle', e => {
  console.log(e.detail.active);
});
```
