# fb.selection Selection

`fb.selection` reads and updates the current foobar2000 selection and resolves the track currently being viewed.

## get(opts?)

Signature: `fb.selection.get(opts?: SelectionGetParams): Promise<SelectionGetResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `opts.offset` | `number` | No | Index of the first handle to return; defaults to `0` |
| `opts.limit` | `number` | No | Maximum number of handles to return; `0` returns every handle from `offset` on. When omitted, the page is capped at 100 |

Reads the current global selection as a page of handles. Returns `count` (the size of the whole selection, not of this page), `type` (where the selection comes from, such as `active_playlist_selection` or `media_library_viewer`), `handles`, the `offset` that applied, and `hasMore`. Handles are native paths with `|subsong:N` appended when the subsong is not `0`. `truncated: true` appears only when `limit` was omitted and the selection holds more than 100 tracks. The call only reads state.

```javascript
const sel = await fb.selection.get({ offset: 0, limit: 50 });
if (sel.success === false) throw new Error(sel.error);
console.log(sel.count, sel.type, sel.handles, sel.hasMore);
```

## getType()

Signature: `fb.selection.getType(): Promise<SelectionGetTypeResponse>`

Reports the type of the current global selection as a numeric `type` and a `typeName`: `0` `now_playing`, `1` `active_playlist_selection`, `2` `active_playlist`, `3` `playlist_manager`, `5` `media_library_viewer`. An unrecognized type reports `type: 0` with `typeName: 'unknown'`, so compare `typeName` rather than `type`.

```javascript
const result = await fb.selection.getType();
if (result.success === false) throw new Error(result.error);
console.log(result.type, result.typeName);
```

## getViewerMode()

Signature: `fb.selection.getViewerMode(): Promise<SelectionGetViewerModeResponse>`

Returns the current viewer mode as `{ mode }`: `prefer_playing` when the selection type is the playing track, otherwise `prefer_selection`. The mode is derived from the live selection type, not read from a stored setting.

```javascript
const result = await fb.selection.getViewerMode();
if (result.success === false) throw new Error(result.error);
console.log(result.mode);
```

## getViewingTrack(opts?)

Signature: `fb.selection.getViewingTrack(opts?: SelectionGetViewingTrackParams): Promise<SelectionGetViewingTrackResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `opts.includeTrackInfo` | `boolean` | No | Also return the full track row as `track`; defaults to `false` |

Resolves the track a viewer should show: the source the viewer mode prefers first, the other source when that one has no track. The call always succeeds. `found` says whether either source had a track and `mode` gives the preference used. When a track is found, the response also carries `source` (`now_playing` or `selection`), `handle`, and `playlistIndex` / `itemIndex` when the track's playlist position is known.

```javascript
const view = await fb.selection.getViewingTrack({ includeTrackInfo: true });
if (view.success === false) throw new Error(view.error);
if (view.found) console.log(view.source, view.handle, view.track?.title);
```

## set(handles)

Signature: `fb.selection.set(handles: string[]): Promise<SelectionSetResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `handles` | `string[]` | Yes | Tracks to select, as native paths with an optional `\|subsong:N` suffix; at least one |

Replaces the global selection with the given tracks and returns `count`, the number of tracks handed to the selection. Handles are made from the strings without a lookup, so a path that does not exist is selected all the same; a suffix that is not a number is read as subsong `0`. When no string yields a handle, the call fails with `INVALID_PARAMS`.

The host holds the selection only for the duration of the call, and foobar2000 clears it once nothing holds it. The new selection therefore outlasts the call only while another holder exists, such as the calling panel's own while that panel has focus with `grabFocus` on.

```javascript
const result = await fb.selection.set(['E:\\Music\\one.flac', 'E:\\Music\\two.flac']);
if (result.success === false) throw new Error(result.error);
console.log(result.count);
```

## setPlaylistTracking(mode?)

Signature: `fb.selection.setPlaylistTracking(mode?: NonNullable<SelectionSetPlaylistTrackingParams['mode']>): Promise<SelectionSetPlaylistTrackingResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `mode` | `'selection' \| 'playlist'` | No | `'selection'` (default) takes the active playlist's selected rows; `'playlist'` takes the whole active playlist |

Sets the global selection from the active playlist and returns the `mode` that applied. The selection is taken once: the host releases its holder when the call returns, which ends foobar2000's tracking, so later playlist changes are not followed. How long the selection itself lasts is as for `set()`.

```javascript
const result = await fb.selection.setPlaylistTracking('playlist');
if (result.success === false) throw new Error(result.error);
```
