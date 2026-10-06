# Selection API

Methods of the `selection` namespace.

## selection

### selection.get

<!-- api-schema:begin selection.get -->
Read the current global selection as a page of handles. Observes state only; nothing is modified.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `offset` | `integer` | No | Index of the first handle to return. At least `0`. Default: `0`. |
| `limit` | `integer` | No | Maximum number of handles to return; `0` means every handle from `offset` on. When omitted the page is capped at 100 and `truncated` reports whether the cap applied. At least `0`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `count` | `integer` | Size of the whole selection, not of this page. |
| `type` | `"now_playing" \| "active_playlist_selection" \| "active_playlist" \| "playlist_manager" \| "media_library_viewer" \| "unknown"` | Where the selection comes from. |
| `handles` | `string[]` | Handles of this page: native paths with `\|subsong:N` appended when the subsong is not `0`. |
| `offset` | `integer` | The `offset` that applied. |
| `hasMore` | `boolean` | Whether entries remain after this page. |
| `truncated` | `boolean` | Present, and true, only when `limit` was omitted and the selection exceeds 100. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

The `100` cap applies only when `limit` is omitted — an explicit `limit` is honored as given, and `limit: 0` means "no limit", returning every entry from `offset` onward.

`truncated: true` appears only when the automatic cap was actually applied, that is when the selection exceeds 100 **and** `limit` was omitted. Asking for `limit: 10` out of a 250-item selection is not truncation and reports nothing. The field is never present with a `false` value, so test for its presence rather than its value, and use `hasMore` to decide whether to page further. `count` is the total size of the selection, not the number of entries returned.

```js
const res = await fb2k.invoke('selection.get', { limit: 0 });
if (res.success === false) throw new Error(res.error);
const { handles, count, hasMore } = res;
```

### selection.getType

<!-- api-schema:begin selection.getType -->
Report the type of the current global selection, as a numeric index and as a name.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `type` | `integer` | Numeric index of the type: `0` now_playing, `1` active_playlist_selection, `2` active_playlist, `3` playlist_manager, `5` media_library_viewer, `0` when unknown. |
| `typeName` | `"now_playing" \| "active_playlist_selection" \| "active_playlist" \| "playlist_manager" \| "media_library_viewer" \| "unknown"` | Name of the type. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('selection.getType');
```

### selection.getViewerMode

<!-- api-schema:begin selection.getViewerMode -->
Report which source the user's Selection Viewer preference favours, derived from the live selection type rather than read from a stored setting.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `mode` | `"prefer_playing" \| "prefer_selection"` | `prefer_playing` when the selection type is the playing track, else `prefer_selection`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('selection.getViewerMode');
```

### selection.getViewingTrack

<!-- api-schema:begin selection.getViewingTrack -->
Resolve the track a viewer should show: the preferred source first, the other one when it has no track. Always succeeds; `found` says whether there was anything to show.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `includeTrackInfo` | `boolean` | No | Also return the full track row as `track`. Default: `false`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `found` | `boolean` | Whether either source had a track. |
| `mode` | `"prefer_playing" \| "prefer_selection"` | The preference the resolution used. |
| `source` | `"now_playing" \| "selection"` | Which source supplied the track; absent when `found` is false. |
| `handle` | `string` | The track's handle (native path, `\|subsong:N` appended when the subsong is not `0`); absent when `found` is false. |
| `playlistIndex` | `integer` | Playlist holding the track; absent when no playlist row is known for it. For the playing track this is where playback took it from; for the selection only the active playlist is looked through, so a selection made elsewhere (the Media Library, another playlist) has no row. Present exactly when `itemIndex` is. |
| `playlistGuid` | `string` | GUID of that playlist, as `playlistGuid` takes it; present exactly when `playlistIndex` is. |
| `itemIndex` | `integer` | Row of the track in that playlist, the first one holding it; absent when `playlistIndex` is. |
| `track` | [Track](../reference/types.md#track) | The full track row; only with `includeTrackInfo`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('selection.getViewingTrack', { includeTrackInfo: true });
if (res.success === false) throw new Error(res.error);
const { found, track } = res;
```

### selection.set

<!-- api-schema:begin selection.set -->
Replace the global selection with the given tracks. Handles are minted from the strings, not looked up, so a path that does not exist is selected just the same. The host holds its selection holder only for the duration of the call; foobar2000 clears the selection once no holder is left, so it outlives the call only while another one is held, such as the calling panel's own while that panel has focus with `grabFocus` on.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `handles` | `string[]` | Yes | Tracks to select, as native paths with an optional `\|subsong:N` suffix. A suffix that is not a number is read as subsong `0`. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `count` | `integer` | How many tracks were handed to the selection holder, i.e. the handles that resolved. Not a read-back: see `set` for how long the selection lasts. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

A malformed `|subsong:` suffix falls back to subsong `0`. A missing `handles`, an element that is not a string, or an empty array is refused with `INVALID_PARAMS`, as is an array none of whose entries resolves to a handle; a failure to acquire the selection holder is `OPERATION_FAILED`.

```js
await fb2k.invoke('selection.set', { handles: ['C:\\Music\\a.flac'] });
```

### selection.setPlaylistTracking

<!-- api-schema:begin selection.setPlaylistTracking -->
Set the selection to the whole active playlist, or to that playlist's own selected rows. foobar2000 ends tracking when the holder that started it is released, and the host releases it when the call returns, so later playlist changes are not followed; how long the selection itself lasts is as for `set`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `mode` | `"selection" \| "playlist"` | No | `playlist` takes the whole active playlist; `selection` takes its selected rows. Default: `"selection"`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `mode` | `"selection" \| "playlist"` | The mode that applied. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('selection.setPlaylistTracking', { mode: 'playlist' });
```

## Selection behavior

`selection.getViewerMode` returns either `prefer_playing` or `prefer_selection`, derived from the live selection type rather than a stored setting. `selection.getViewingTrack` applies that preference and falls back to the other source when the preferred one has no track; it always reports `success: true`, so test `found` instead. With `includeTrackInfo` its `track` is the shared [Track](../reference/types.md#track) row. `selection:changed` is broadcast to every WebView after a selection update and is throttled to 50 ms; its payload is documented in the event reference.

Queue and selection handles share one string form: a native path with `|subsong:N` appended only when the subsong is greater than `0`. Paths supplied to `queue.addPaths` or the JIT Queue operations accept the same suffix. Individual paths and URLs are capped at 2048 characters.
