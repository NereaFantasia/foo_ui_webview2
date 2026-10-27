# G. Rating and Audio Settings

## `<fb-rating>` {#fb-rating}

Interactive star rating component. Activating the currently selected star clears the rating to 0.

```html
<fb-rating max="5"></fb-rating>
```

| Attribute | Type | Default | Description |
| --- | --- | --- | --- |
| `max` | string | `'5'` | Maximum number of stars (observed) |
| `readonly` | boolean | false | Disables click, keyboard, and hover-preview interactions when present (observed) |
| `value` | string | — | Read-only reflection of the current rating |

**CSS Parts:** `stars`, `star`

Each `star` part exposes `data-value` and `aria-checked`. Filled or previewed stars also receive `data-filled`.

**Events:**

```js
el.addEventListener('fb-rating-change', e => {
  console.log('Rating:', e.detail.value, 'path:', e.detail.path);
});
```

## `<fb-output-selector>` {#fb-output-selector}

Audio output device dropdown selector component.

```html
<fb-output-selector></fb-output-selector>
```

| Attribute | Type | Default | Description |
| --- | --- | --- | --- |
| `device-name` | string | — | Read-only reflection of the current device name |

**CSS Parts:** `select`

**Events:**

```js
el.addEventListener('fb-output-change', e => {
  console.log('Output device:', e.detail.index, e.detail.name);
  console.log('outputId:', e.detail.outputId, 'deviceId:', e.detail.deviceId);
});
```

## `<fb-dsp-preset-selector>` {#fb-dsp-preset-selector}

DSP preset dropdown selector component.

```html
<fb-dsp-preset-selector></fb-dsp-preset-selector>
```

| Attribute | Type | Default | Description |
| --- | --- | --- | --- |
| `preset-name` | string | — | Read-only reflection of the active preset name |

**CSS Parts:** `select`

**Events:**

```js
el.addEventListener('fb-dsp-change', e => {
  console.log('DSP preset:', e.detail.index, e.detail.name);
});
```

## `<fb-replaygain-selector>` {#fb-replaygain-selector}

ReplayGain mode dropdown selector component.

```html
<fb-replaygain-selector></fb-replaygain-selector>
```

| Attribute | Type | Default | Description |
| --- | --- | --- | --- |
| `mode` | string | — | Read-only reflection: `none`, `track`, `album`, or `auto` |

**CSS Parts:** `select`

**Events:**

```js
el.addEventListener('fb-replaygain-change', e => {
  console.log('ReplayGain mode:', e.detail.mode);
});
```
