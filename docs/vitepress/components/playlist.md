# D. Playlists

## `<fb-playlist-tabs>` {#fb-playlist-tabs}

Playlist tab strip with ArrowLeft/ArrowRight keyboard navigation, drag reordering, edge resizing, and overflow scrolling.

```html
<fb-playlist-tabs></fb-playlist-tabs>
```

| Attribute | Type | Default | Description |
| --- | --- | --- | --- |
| `active-index` | string | — | Read-only active playlist index |
| `count` | string | — | Read-only playlist count |
| `native-context` | boolean | false | Also opens the native playlist context menu when present |

**Read-only Host States:** `dragging` while a tab is being dragged; `overflow` while the tab strip overflows

**CSS Parts:** `tabs-container`, `tab`, `tab-name`, `tab-count`, `drop-indicator`, `add-button`

Each `tab` part receives a `locked` attribute when the playlist is locked, and carries its playlist's GUID as `data-guid`.

The tabs address playlists by GUID: a click activates the tab's playlist even if others were added or removed since the tabs were drawn, and a tab whose playlist is gone activates nothing. `fb-playlist-select` and `fb-playlist-reorder` fire only once the host has done it.

**Events:**

```js
el.addEventListener('fb-playlist-select', e => {
  console.log('Selected playlist:', e.detail.index, e.detail.guid);
});
el.addEventListener('fb-playlist-context', e => {
  // Act on e.detail.guid once the menu closes; the index may name another playlist by then.
  console.log('Context request:', e.detail.guid, e.detail.x, e.detail.y);
});
el.addEventListener('fb-playlist-add', e => {
  console.log('New playlist created');
});
el.addEventListener('fb-playlist-reorder', e => {
  console.log('Reordered:', e.detail.fromIndex, e.detail.toIndex, e.detail.newOrderGuids);
});
```

## `<fb-resizable-header>` {#fb-resizable-header}

Resizable column header with drag reordering and click-to-sort behavior. It is typically paired with `<fb-playlist-view>`.

```html
<fb-resizable-header columns='[{"id":"title","label":"Title","width":"2fr"}]'></fb-resizable-header>
```

| Attribute | Type | Default | Description |
| --- | --- | --- | --- |
| `columns` | string (JSON) | `[]` | Column descriptors with `id`, optional `label`/`name`, `width` (for example `1fr`), `sortable`, and `fixed` fields |
| `sort-column` | string | — | Current sort column ID (observed and reflected) |
| `sort-direction` | `'asc' \| 'desc'` | `'asc'` | Current sort direction (observed and reflected) |

`fixed` columns cannot be resized or reordered. **Read-only Host States:** `resizing` and `reordering`

**CSS Parts:** `header`, `cell`, `cell-{id}`, `resize-handle`, `sort-icon`, `sort-active`, `drop-indicator`, `drag-ghost`, `label`

**Read-only JavaScript Properties:**

- `gridTemplate` — current CSS `grid-template-columns` value for synchronizing a playlist view
- `columnIds` — current visual order of column IDs

**Events:**

```js
// Column resize
el.addEventListener('fb-column-resize', e => {
  console.log('Width:', e.detail.column, e.detail.width);
  // Keep the playlist view aligned with the header.
  playlistView.setAttribute('grid-template', e.detail.gridTemplate);
});

// Column reorder
el.addEventListener('fb-column-reorder', e => {
  console.log('Reordered:', e.detail.fromIndex, '→', e.detail.toIndex);
  console.log('New order:', e.detail.columns);
});

// Column sort
el.addEventListener('fb-column-sort', e => {
  console.log('Sort:', e.detail.column, e.detail.direction);
});

// Header context request
el.addEventListener('fb-header-context', e => {
  console.log('Header context:', e.detail.x, e.detail.y);
});
```

## `<fb-playlist-view>` {#fb-playlist-view}

Virtualized playlist view with an rAF-driven DOM pool. It supports Ctrl/Shift selection, Enter to play, Delete to remove, and Ctrl+A to select all.

```html
<fb-playlist-view row-height="24"></fb-playlist-view>
```

| Attribute | Type | Default | Description |
| --- | --- | --- | --- |
| `playlist` | string | — | The playlist's `guid` (a value starting with `{`) or its index; omitted, the active playlist (observed). See below |
| `columns` | string | `'index,title,artist,album,duration'` | Comma-separated column IDs (observed) |
| `row-height` | string | `'32'` | Row height in pixels (observed) |
| `grid-template` | string | — | CSS `grid-template-columns` value, typically copied from `<fb-resizable-header>` (observed) |
| `formats` | string (JSON) | `{}` | Map of custom column IDs to Title Formatting expressions (observed) |
| `track-count` | string | — | Read-only total track count |
| `selected-count` | string | — | Read-only selected track count |

**CSS Parts:** `viewport`, `scroll-content`, `row`, `row-{column}`

Each `row` part may reflect the `selected`, `focused`, and `playing` states as attributes.

A GUID in `playlist` keeps the view on that playlist while others are added, removed or reordered, and leaves it empty once the playlist is removed. An index shows whichever playlist is at that position and is looked up again after playlists are added, removed or reordered. Without the attribute the view follows the active playlist and shows nothing while there is none. Once it knows which playlist it shows, the view addresses it by GUID in every host call, and each event detail below carries that GUID as `playlistGuid`.

**Events:**

```js
el.addEventListener('fb-track-select', e => {
  console.log('Selected:', e.detail.index, e.detail.indices, e.detail.playlistGuid);
});
el.addEventListener('fb-track-play', e => {
  console.log('Play:', e.detail.index, e.detail.playlistGuid);
});
el.addEventListener('fb-track-context', e => {
  console.log('Context request:', e.detail.indices, e.detail.playlistGuid, e.detail.x, e.detail.y);
});
```

## `<fb-queue-view>` {#fb-queue-view}

Playback queue view. Double-clicking a row removes that queue entry.

```html
<fb-queue-view columns="index,title,artist,duration">
  <span slot="empty">The queue is empty</span>
</fb-queue-view>
```

| Attribute | Type | Default | Description |
| --- | --- | --- | --- |
| `columns` | string | `'index,title,artist,duration'` | Comma-separated column IDs |
| `empty` | boolean | — | Read-only reflection of whether the queue is empty |
| `count` | string | — | Read-only queue-entry count |

**CSS Parts:** `container`, `row`, `row-{column}`, `empty`
**Slots:** `empty`

**Events:**

```js
el.addEventListener('fb-queue-context', e => {
  console.log('Context request:', e.detail.index, e.detail.x, e.detail.y);
});
el.addEventListener('fb-queue-remove', e => {
  console.log('Removed:', e.detail.index);
});
```

## `<fb-playlist-selector>` {#fb-playlist-selector}

Playlist dropdown selector.

```html
<fb-playlist-selector></fb-playlist-selector>
```

| Attribute | Type | Default | Description |
| --- | --- | --- | --- |
| `selected-index` | string | — | Read-only selected playlist index |
| `selected-name` | string | — | Read-only selected playlist name |

**CSS Parts:** `select`

**Events:**

```js
el.addEventListener('fb-playlist-pick', e => {
  console.log('Selected:', e.detail.index, e.detail.guid, e.detail.name);
});
```
