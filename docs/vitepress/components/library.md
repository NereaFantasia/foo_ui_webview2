# I. Media Library

## `<fb-library-tree>` {#fb-library-tree}

Tree browser for artist, album, and genre groupings, supporting Ctrl/Cmd toggling, Shift range selection, and lazy child loading.

```html
<fb-library-tree view="artist"></fb-library-tree>
```

| Attribute | Type | Default | Description |
| --- | --- | --- | --- |
| `view` | `'artist' \| 'album' \| 'genre'` | `'artist'` | Grouping view (observed and reflected) |
| `sort` | string | `'name'` | Sort value passed through by the component (observed) |
| `filter` | string | `''` | Filter value passed through by the component (observed) |
| `item-count` | string | — | Read-only top-level item count |

**CSS Parts:** `container`, `header`, `node`, `child`, `node-toggle`, `node-label`, `node-count`, `node-children`

Node elements reflect `expanded` and `selected` states as attributes.

**Events:**

```js
// Selection changed
el.addEventListener('fb-library-select', e => {
  console.log('Selected:', e.detail.key, e.detail.type, e.detail.view);
  console.log('Selection set:', e.detail.selected);
});

// Row double-clicked
el.addEventListener('fb-library-play', e => {
  console.log('Play request:', e.detail.key, e.detail.type, e.detail.view);
});

// Context menu request; the component dispatches an event without opening a menu
el.addEventListener('fb-library-context', e => {
  const { key, type, view, x, y, selected } = e.detail;
  // Call fb.ui.showCustomMenu(...) from theme code to open a menu
});

// Emitted after addToPlaylist() completes
el.addEventListener('fb-library-added', e => {
  console.log('Added:', e.detail.count, e.detail.type, e.detail.key);
});
```

### View Modes

- **artist**: artist groups expand to albums (`album` child nodes)
- **album**: album groups expand to tracks (`track` child nodes)
- **genre**: genre groups expand to matching tracks

Event payloads include `artist` from `data-artist`. On an `album` child in the artist view, this reflects the parent artist representing tracks by that artist on the album. On an album-view group, it reflects the album artist. Events and `selected` entries can also include `albumId` or exact member `tracks`. Pass the event detail as the fourth parameter to `el.addToPlaylist(key, type, artist, detail)` to preserve track identity (including subsongs). A provided empty `tracks` array signifies zero member tracks rather than triggering a fallback name-based query. Display names alone may not distinguish albums whose underlying tags contain invalid UTF-8.

## `<fb-library-filesystem-tree>` {#fb-library-filesystem-tree}

Filesystem-style media library browser. Root folders are loaded via `library.getRoots()`, and child directories expand lazily through `library.browseTree()`. Item selection supports Ctrl/Cmd toggling and Shift range selection.

```html
<fb-library-filesystem-tree></fb-library-filesystem-tree>
```

| Attribute | Type | Default | Description |
| --- | --- | --- | --- |
| `root-id` | string | — | Pins the view to one root and omits the multi-root header (observed) |
| `include-files` | string | `'false'` | File nodes are rendered only for the exact value `true` (observed) |

**CSS Parts:** `container`, `header`, `node`, `root`, `directory`, `file`, `node-toggle`, `node-label`, `node-count`, `node-children`

`data-node-type` identifies `header`, `root`, `directory`, and `file` nodes.

Nodes also expose `data-root-id`, `data-path-id`, and `data-absolute-path`.

**Events:**

```js
// Selection changed
el.addEventListener('fb-library-fs-select', e => {
  console.log('Selected:', e.detail.type, e.detail.absolutePath);
  if (e.detail.file) console.log('File details:', e.detail.file);
});

// Row double-clicked
el.addEventListener('fb-library-fs-play', e => {
  console.log('Play request:', e.detail.type, e.detail.absolutePath);
});

// Context menu request; the component dispatches an event without opening a menu
el.addEventListener('fb-library-fs-context', e => {
  const { type, rootId, pathId, absolutePath, x, y } = e.detail;
  // Call fb.ui.showCustomMenu(...) from theme code to open a menu
});
```

### Node Types

| `data-node-type` | Description |
| --- | --- |
| `header` | Top-level “All Music” header |
| `root` | Media-library root |
| `directory` | Child directory |
| `file` | Audio file, rendered only with `include-files="true"` |
