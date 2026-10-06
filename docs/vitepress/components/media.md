# F. Lyrics and Visualization

## `<fb-lyrics-panel>` {#fb-lyrics-panel}

Lyrics panel for synchronized LRC or plain-text lyrics. Synchronized lyrics highlight the active line, and clicking a timestamped line seeks to that time.

```html
<fb-lyrics-panel></fb-lyrics-panel>
```

| Attribute | Type | Default | Description |
| --- | --- | --- | --- |
| `source` | string | `'any'` | Lyrics source passed to the SDK wrapper |
| `scroll` | string | `'smooth'` | Uses smooth scrolling for the exact value `smooth`; otherwise uses automatic scrolling |
| `has-lyrics` | boolean | — | Read-only reflection of whether lyrics were rendered |
| `current-line` | string | — | Read-only zero-based active-line index, or `-1` |

**CSS Parts:** `container`, `line`

Each `line` part receives one of the `past`, `current`, or `future` CSS classes.

**Events:**

```js
el.addEventListener('fb-lyrics-loaded', e => {
  console.log('Lyrics loaded:', e.detail.lineCount, 'synced:', e.detail.synced);
});
el.addEventListener('fb-lyrics-seek', e => {
  console.log('Lyrics seek:', e.detail.time);
});
el.addEventListener('fb-lyrics-line-change', e => {
  console.log('Current line:', e.detail.index, e.detail.text, e.detail.time);
});
```

## `<fb-spectrum-visualizer>` {#fb-spectrum-visualizer}

Real-time spectrum visualization with device-pixel-ratio scaling and `ResizeObserver` support. In `mode="raw"`, the component emits band data instead of drawing it. Each element runs its own host subscription and draws only that subscription's frames, so visualizers with different `bands` can share a page.

```html
<fb-spectrum-visualizer bands="64" fps="30" mode="bars"></fb-spectrum-visualizer>
```

| Attribute | Type | Default | Description |
| --- | --- | --- | --- |
| `bands` | string | `'64'` | Requested band count |
| `fps` | string | `'30'` | Host spectrum-stream rate |
| `mode` | `'raw' \| 'bars' \| 'wave'` | `'bars'` | Rendering mode |
| `fft-size` | string | automatic | FFT size override. Values below 256 are raised, the result is rounded up to a power of two, and anything above 65536 is clamped |
| `dpr` | string | `devicePixelRatio` | Pixel-ratio override; lower values such as `dpr="1"` reduce canvas cost |
| `fall-speed` | string | `'0.28'` | Fall interpolation coefficient strictly between 0 and 1 (observed) |
| `rise-speed` | string | `'0.65'` | Rise interpolation coefficient strictly between 0 and 1 (observed) |

**CSS Parts:** `canvas`

**JavaScript API:**

```js
const ctx = el.getContext(); // CanvasRenderingContext2D or null
```

**Events:**

```js
el.addEventListener('fb-spectrum-data', e => {
  // Emitted only in mode="raw".
  console.log('Spectrum data:', e.detail.bands); // Float32Array
});
```

## `<fb-waveform>` {#fb-waveform}

Full-track waveform overview. Clicking the canvas seeks, while the cursor follows playback progress.

When the playing track changes or the element is removed while its waveform is still being decoded, the element cancels that host request, so a stale request does not keep one of the host's two decode slots busy. Page-level listeners on `audio:fullWaveformFailed` see these cancellations with `code: "CANCELLED"`.

```html
<fb-waveform resolution="200"></fb-waveform>
```

| Attribute | Type | Default | Description |
| --- | --- | --- | --- |
| `src` | string | current track | Explicit track path; when present, track-change reloads and playback-time cursor updates are not subscribed |
| `resolution` | string | `'200'` | Requested waveform sample count |
| `mode` | `'peak' \| 'rms'` | `'rms'` | Host-side sampling method |
| `cue-index` | string | — | Optional cue-sheet sub-song index |
| `normalize` | `'adaptive' \| 'gamma' \| 'histogram'` | `'adaptive'` | Visual normalization mode (observed) |
| `gamma` | string | `'1.5'` | Gamma exponent in the range $(0, 2]$; observed and used by `normalize="gamma"` |
| `status` | `'pending' \| 'ready' \| 'failed'` | — | Read-only generation state |
| `progress` | string | — | Read-only cursor ratio from 0 to 1 |
| `error` | string | — | Read-only failure code when `status="failed"` |

**CSS Parts:** `canvas`, `cursor`

**Events:**

```js
el.addEventListener('fb-seek', e => {
  console.log('Waveform seek:', e.detail.position, e.detail.progress);
});
```
