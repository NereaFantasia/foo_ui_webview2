# I. Media Library

## `<fb-library-tree>` {#fb-library-tree}

Tree browser for artist, album, and genre groupings. It supports Ctrl/Cmd toggling, Shift range selection, and lazy child loading.

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

// Context request; the component does not open a menu itself.
el.addEventListener('fb-library-context', e => {
  const { key, type, view, x, y, selected } = e.detail;
  // Call fb.ui.showCustomMenu(...) from theme code if desired.
});

// Emitted after the component's addToPlaylist() helper completes.
el.addEventListener('fb-library-added', e => {
  console.log('Added:', e.detail.count, e.detail.type, e.detail.key);
});
```

### View Modes

- **artist**: artist groups expand to albums (`album` child nodes)
- **album**: album groups expand to tracks (`track` child nodes)
- **genre**: genre groups expand to matching tracks

Event details carry `artist`, read from the row's `data-artist`. On an `album` child in the artist view it is the parent artist, and the row stands for that artist's tracks on the album. On a group in the album view it is the album artist: `key` alone can name several albums, and `library.getAlbumTracks(key, artist)` names exactly one.

## `<fb-library-filesystem-tree>` {#fb-library-filesystem-tree}

Filesystem-style media-library browser. It starts with `library.getRoots()` and lazily expands directories through `library.browseTree()`. Selection supports Ctrl/Cmd toggling and Shift ranges.

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

// Context request; the component does not open a menu itself.
el.addEventListener('fb-library-fs-context', e => {
  const { type, rootId, pathId, absolutePath, x, y } = e.detail;
  // Call fb.ui.showCustomMenu(...) from theme code if desired.
});
```

### Node Types

| `data-node-type` | Description |
| --- | --- |
| `header` | Top-level “All Music” header |
| `root` | Media-library root |
| `directory` | Child directory |
| `file` | Audio file, rendered only with `include-files="true"` |
