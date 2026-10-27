# C. Track Information

## `<fb-track-text>` {#fb-track-text}

General-purpose track text display. It replaces the legacy `fb-title`, `fb-artist`, and `fb-album` elements; use a built-in field or a Title Formatting expression.

```html
<fb-track-text field="title"></fb-track-text>
```

| Attribute | Type | Default | Description |
| --- | --- | --- | --- |
| `field` | string | `'title'` | Field to display (observed) |
| `tf` | string | — | Title Formatting expression; takes precedence over `field` (observed) |
| `placeholder` | string | — | Text shown when no value is available (observed) |

**CSS Parts:** `text`

## `<fb-cover-art>` {#fb-cover-art}

Cover-art display. Add `use-fb2k` to try the SDK's `getFb2kUrl()` helper first; the component falls back to `artwork.getCurrent()` when that path does not return usable artwork.

```html
<fb-cover-art use-fb2k>
  <span slot="placeholder">🎵</span>
</fb-cover-art>
```

| Attribute | Type | Default | Description |
| --- | --- | --- | --- |
| `type` | string | `'front'` | Artwork type: `front`, `back`, `disc`, `icon`, or `artist` (observed) |
| `use-fb2k` | boolean | false | Tries the SDK URL-helper lookup before the standard artwork request (observed) |
| `loaded` | boolean | — | Read-only reflection of successful image loading |
| `src` | string | — | Read-only reflection of the current non-data image source |

**CSS Parts:** `container`, `image`  
**Slots:** `placeholder`

**Events:**

```js
el.addEventListener('fb-cover-load', e => { /* artwork loaded */ });
el.addEventListener('fb-cover-error', e => { /* artwork failed to load */ });
```

## `<fb-time-current>` {#fb-time-current}

Current playback time, formatted as `m:ss` or `h:mm:ss`.

```html
<fb-time-current></fb-time-current>
```

| Attribute | Type | Default | Description |
| --- | --- | --- | --- |
| `seconds` | string | — | Read-only current position in whole seconds |

**CSS Parts:** `text`

## `<fb-time-total>` {#fb-time-total}

Total track duration.

```html
<fb-time-total></fb-time-total>
```

| Attribute | Type | Default | Description |
| --- | --- | --- | --- |
| `seconds` | string | — | Read-only total duration in whole seconds |

**CSS Parts:** `text`

## `<fb-time-remaining>` {#fb-time-remaining}

Remaining playback time, shown with a `-` prefix.

```html
<fb-time-remaining></fb-time-remaining>
```

| Attribute | Type | Default | Description |
| --- | --- | --- | --- |
| `seconds` | string | — | Read-only remaining time in whole seconds |

**CSS Parts:** `text`

## `<fb-tech-info>` {#fb-tech-info}

Technical track information. With `field="all"`, the output is formatted like `FLAC | 1411 kbps | 44.1 kHz | Stereo`.

```html
<fb-tech-info field="all"></fb-tech-info>
```

| Attribute | Type | Default | Description |
| --- | --- | --- | --- |
| `field` | string | `'all'` | Display field: `all`, `codec`, `bitrate`, `samplerate`, or `channels` (observed) |

**CSS Parts:** `text`
