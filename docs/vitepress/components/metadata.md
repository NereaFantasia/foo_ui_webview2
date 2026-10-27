# H. Metadata and Search

## `<fb-properties-panel>` {#fb-properties-panel}

Read-only track properties panel grouped into metadata, technical information, and file location.

```html
<fb-properties-panel></fb-properties-panel>
```

| Attribute | Type | Default | Description |
| --- | --- | --- | --- |
| `path` | string | current track | Explicit track path; suppresses track-change reloads |
| `groups` | string | `'metadata,technical,location'` | Comma-separated subset of groups to render |
| `track-path` | string | — | Read-only reflection of the loaded track path |

**CSS Parts:** `container`, `group`, `group-title`, `row`, `label`, `value`

**Group Contents:**

- **metadata** — TITLE, ARTIST, ALBUM, ALBUMARTIST, DATE, GENRE, TRACKNUMBER, COMMENT
- **technical** — codec, bitrate, sample rate, channels, duration
- **location** — absolute file path

## `<fb-search-bar>` {#fb-search-bar}

Debounced media-library search input with a minimum-length filter. Enter searches immediately and bypasses the debounce delay; an empty query is ignored.

```html
<fb-search-bar placeholder="Search tracks..." debounce="300" min-length="2">
  <span slot="icon">🔍</span>
</fb-search-bar>
```

| Attribute | Type | Default | Description |
| --- | --- | --- | --- |
| `placeholder` | string | `'Search library...'` | Input placeholder |
| `debounce` | string | `'300'` | Keystroke debounce delay in milliseconds |
| `min-length` | string | `'2'` | Minimum query length for debounced input searches |

**CSS Parts:** `container`, `input`  
**Slots:** `icon` (default: 🔍)

**Events:**

```js
el.addEventListener('fb-search', e => {
  console.log('Search:', e.detail.query);
});
el.addEventListener('fb-search-result', e => {
  console.log('Results:', e.detail.count, 'for', e.detail.query);
  console.log('Tracks:', e.detail.tracks);
});
```

## `<fb-console>` {#fb-console}

foobar2000 console viewer. It polls for updates and escapes every rendered log line.

```html
<fb-console></fb-console>
```

| Attribute | Type | Default | Description |
| --- | --- | --- | --- |
| `max-lines` | string | `'200'` | Maximum number of lines requested per poll |
| `auto-scroll` | string | enabled | Set to the exact string `false` to disable scrolling to the bottom |
| `refresh` | string | `'1000'` | Poll interval in milliseconds |

**CSS Parts:** `container`, `line`

**Events:**

```js
el.addEventListener('fb-console-update', e => {
  console.log('Console update:', e.detail.lineCount, 'lines');
});
```
