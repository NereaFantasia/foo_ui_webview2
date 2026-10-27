# B. Progress and Volume

## `<fb-seek-bar>` {#fb-seek-bar}

Playback seek bar with mouse, drag, and keyboard controls.

```html
<fb-seek-bar></fb-seek-bar>
```

| Attribute | Type | Default | Description |
| --- | --- | --- | --- |
| `position` | string | — | Read-only current position in whole seconds |
| `duration` | string | — | Read-only total duration in whole seconds |
| `progress` | string | — | Read-only progress ratio from 0 to 1 |

**CSS Parts:** `track`, `fill`, `thumb`
**CSS Custom Properties:** `--fb-seek-progress` (0–1), `--fb-seek-hover-progress` (0–1 while hovering)
**Keyboard:** Arrow keys seek by ±5 seconds; Home/End seek to the start/end

**Events:**

```js
el.addEventListener('fb-seek', e => {
  console.log(e.detail.position); // committed position in seconds
});
el.addEventListener('fb-seeking', e => {
  console.log(e.detail.position); // tentative position during a drag
});
```

**Styling Example:**

```css
fb-seek-bar::part(track) { background: #333; height: 4px; }
fb-seek-bar::part(fill) { background: #1db954; }
fb-seek-bar::part(thumb) { width: 12px; height: 12px; background: white; border-radius: 50%; }
```

## `<fb-volume-control>` {#fb-volume-control}

Volume slider with a mute button.

```html
<fb-volume-control></fb-volume-control>
```

| Attribute | Type | Default | Description |
| --- | --- | --- | --- |
| `vertical` | boolean | false | Uses a vertical, bottom-to-top slider layout (observed) |
| `no-icon` | boolean | false | Hides the mute button (observed) |
| `volume` | string | — | Read-only current volume on the 0–100 linear scale |
| `muted` | boolean | — | Read-only reflection of the mute state |

**CSS Parts:** `mute-button`, `track`, `fill`, `thumb`  
**Slots:** `icon` (default: 🔊)  
**CSS Custom Properties:** `--fb-volume` (0–1)  
**Keyboard:** Arrow keys change volume by ±5; Home sets 0 and End sets 100  
**Mouse Wheel:** changes volume by ±5

**Events:**

```js
el.addEventListener('fb-volume-change', e => {
  console.log(e.detail.volume); // 0–100
});
el.addEventListener('fb-mute-toggle', e => { /* mute toggled */ });
```

## `<fb-playback-order>` {#fb-playback-order}

Seven-state playback-order picker with dropdown and cycling-button modes.

```html
<fb-playback-order mode="button"></fb-playback-order>
```

| Attribute | Type | Default | Description |
| --- | --- | --- | --- |
| `mode` | `'select' \| 'button'` | `'button'` | Display mode (observed); only the exact value `select` selects the dropdown |
| `order` | string | — | Read-only reflection of the current order name |

**CSS Parts:** `button` (button mode) or `select` (select mode)  
**Slots:** default slot (button mode label)  
**Orders:** `default`, `repeat-playlist`, `repeat-track`, `random`, `shuffle-tracks`, `shuffle-albums`, `shuffle-folders`

**Events:**

```js
el.addEventListener('fb-order-change', e => {
  console.log(e.detail.order, e.detail.name);
});
```
