# fb.selection Selection

`fb.selection` reads and updates the current foobar2000 selection and resolves the track currently being viewed.

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## Additional methods

> This block records SDK method coverage and may later be expanded with complete examples and best practices.

### getViewerMode()

Signature: `fb.selection.getViewerMode(): Promise<{ mode: string }>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| - | - | - | No parameters |

Returns the current viewer mode as `{ mode }`. Related methods include `get({ offset?, limit? })`, `getType()`, `set(handles)`, `setPlaylistTracking(mode)`, and `getViewingTrack({ includeTrackInfo? })`. Playlist tracking defaults to `'selection'`; pass `'playlist'` to track the active playlist instead.

```javascript
const result = await fb.selection.getViewerMode();
console.log(result.mode);
```

<!-- END AUTO-GENERATED SDK STUBS -->
