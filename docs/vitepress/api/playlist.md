# Playlist API

Playlist management, track operations, autoplaylists, and helpers. 47 APIs.

> **Parameter compatibility**: every Playlist API accepts both `playlist` and `index` for the playlist index.

## List management

### playlist.getCount

Get the number of playlists.

- **Parameters**: none
- **Returns**: `{ "count": 5 }`

```javascript
const { count } = await fb2k.invoke('playlist.getCount');
```

### playlist.getAll

<!-- phase3-major1-review:playlist.getAll -->
#### Source-reviewed contract
Authority: `src/api/PlaylistApi.cpp:750-772`.

_No public parameters._

**Return keys (vary by response variant)**: No named fields.

**Semantics**: No request fields are read. The method returns an array of playlist summaries containing index, name, trackCount, active/playing/locked, and autoplaylist state.

<!-- phase3-major1-review-end:playlist.getAll -->

Get information for all playlists.

- **Parameters**: none

**Returns**:

```json
[
    {
        "index": 0,
        "name": "Default",
        "trackCount": 150,
        "isActive": true,
        "isPlaying": true,
        "isLocked": false,
        "isAutoplaylist": false
    }
]
```

::: warning Breaking Change (v1.1.18)
`playlist.getAll` no longer returns `duration` (avoids loading every track across every playlist). Use `playlist.getActive` or `playlist.getPlaying` when you need a single playlist duration.
:::

::: tip v1.1.18 added
The `isAutoplaylist` field is now inlined in `playlist.getAll`; you no longer need per-playlist `playlist.isAutoplaylist` calls.
:::

### playlist.getActive

Get the active playlist. Includes a `duration` field.

- **Parameters**: none

**Returns**: `{"duration":"...","found":true,"index":0,"isActive":true,"isLocked":true,"isPlaying":true,"name":"...","success":true,"trackCount":"..."}`


> noneactivePlay column  when return `{ "success": true, "found": false }`..

### playlist.setActive

<!-- phase3-major1-review:playlist.setActive -->
#### Source-reviewed contract
Authority: `src/api/PlaylistApi.cpp:796-808`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | `not supplied` |

**Return keys (vary by response variant)**: `error`, `success`; `success`

**Semantics**: setActive requires a valid playlist index in practice: its SIZE_MAX default is rejected and does not fall back to the current active playlist.

<!-- phase3-major1-review-end:playlist.setActive -->

Set active Play column ...

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default not supplied. |

**Returns**: `{ "success": true, "error": "..." }`

```javascript
await fb2k.invoke('playlist.setActive', { playlist: 1 });
```

### playlist.getPlaying

Get the currently playing playlist.includes `duration` Field.

- **Parameters**: none

**Returns**: `{"duration":"...","found":true,"index":0,"isActive":true,"isLocked":true,"isPlaying":true,"name":"...","success":true,"trackCount":"..."}`


> nonePlay playlist when return `{ "success": true, "found": false }`..

### playlist.create

<!-- phase3-major1-review:playlist.create -->
#### Source-reviewed contract
Authority: `src/api/PlaylistApi.cpp:832-845`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `name` | `string` | No | `New Playlist` |
| `position` | `integer` | No | `append` |

**Return keys (vary by response variant)**: `index`, `success`

**Semantics**: create forwards the optional name and insertion position to the playlist service; the SIZE_MAX sentinel appends and the returned index is the created playlist.

<!-- phase3-major1-review-end:playlist.create -->

Create  Play column ...

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `name` | `string` | No | Optional; default New Playlist. |
| `position` | `integer` | No | Optional; default append. |

**Returns**: `{ "success": true, "index": 2 }`

```javascript
const result = await fb2k.invoke('playlist.create', { name: 'Rock Music' });
```

### playlist.remove

<!-- phase3-major1-review:playlist.remove -->
#### Source-reviewed contract
Authority: `src/api/PlaylistApi.cpp:845-867`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | `active playlist` |

**Return keys (vary by response variant)**: `error`, `success`; `success`

**Semantics**: The target defaults to active. The service remove result is returned; a pre-existing lock is surfaced through the structured locked error path.

<!-- phase3-major1-review-end:playlist.remove -->

Remove a playlist.if Play column lockedthen noneRemove ...

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |

**Returns**: `{ "success": true, "error": "..." }`

### playlist.rename

<!-- phase3-major1-review:playlist.rename -->
#### Source-reviewed contract
Authority: `src/api/PlaylistApi.cpp:867-880`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | `not supplied` |
| `name` | `string` | No | `` |

**Return keys (vary by response variant)**: `error`, `success`; `success`

**Semantics**: Unlike most playlist operations, rename does not substitute the active playlist when omitted: the SIZE_MAX default is invalid. The response success mirrors the rename service result.

<!-- phase3-major1-review-end:playlist.rename -->

Rename a playlist.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default not supplied. |
| `name` | `string` | No | Optional; default . |

**Returns**: `{ "success": true }`

```javascript
await fb2k.invoke('playlist.rename', { playlist: 0, name: 'My Favorites' });
```

### playlist.clear

<!-- phase3-major1-review:playlist.clear -->
#### Source-reviewed contract
Authority: `src/api/PlaylistApi.cpp:880-910`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | `active playlist` |

**Return keys (vary by response variant)**: `error`, `success`; `clearedCount`, `playlist`, `remainingCount`, `success`

**Semantics**: The omitted playlist sentinel resolves to the active playlist. Valid unlocked targets receive an undo backup before clear; the result exposes pre-clear and remaining counts.

<!-- phase3-major1-review-end:playlist.clear -->

Clear Play column  in all track ...

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |

**Returns**:

```json
{
    "success": true,
    "playlist": 0,
    "clearedCount": 22,
    "remainingCount": 0
}
```

### playlist.duplicate

<!-- phase3-major1-review:playlist.duplicate -->
#### Source-reviewed contract
Authority: `src/api/PlaylistApi.cpp:1402-1436`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | `active playlist` |
| `name` | `string` | No | `source name + ' (Copy)'` |

**Return keys (vary by response variant)**: `error`, `success`; `error`, `success`; `index`, `name`, `newPlaylist`, `sourcePlaylist`, `success`, `trackCount`

**Semantics**: The target defaults to the active playlist. An empty name is replaced with the source name plus (Copy); the result identifies sourcePlaylist and newPlaylist.

<!-- phase3-major1-review-end:playlist.duplicate -->

Duplicate a playlist.column Insert to column  after ..

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |
| `name` | `string` | No | Optional; default source name + ' (Copy)'. |

**Returns**: `{ "success": true, "index": 1, "sourcePlaylist": 0, "newPlaylist": 1, "name": "Default (Copy)", "trackCount": 150 }`

## Track operations

### playlist.getTrackCount

<!-- phase3-major1-review:playlist.getTrackCount -->
#### Source-reviewed contract
Authority: `src/api/PlaylistApi.cpp:948-961`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | `active playlist` |
| `index` | `integer` | No | `used only if playlist is absent` |

**Return keys (vary by response variant)**: `count`; `count`

**Semantics**: The shared selector accepts playlist or index and defaults to active. Invalid resolution deliberately returns count:0 rather than an error envelope.

<!-- phase3-major1-review-end:playlist.getTrackCount -->

Get Play column  in track count...

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |
| `index` | `integer` | No | Optional; default used only if playlist is absent. |

**Returns**: `{ "count": 150 }`

### playlist.getTracks

<!-- phase3-major1-review:playlist.getTracks -->
#### Source-reviewed contract
Authority: `src/api/PlaylistApi.cpp:961-981`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | `active playlist` |
| `index` | `integer` | No | `used only if playlist is absent` |
| `start` | `integer` | No | `0` |
| `count` | `integer` | No | `100` |
| `formats` | `object` | No | `{}` |

**Return keys (vary by response variant)**: `count`, `playlist`, `start`, `total`, `tracks`

**Semantics**: The handler pages the resolved playlist and returns an empty tracks variant for an invalid target. formats accepts extra titleformat columns, while start/count bound the returned range.

<!-- phase3-major1-review-end:playlist.getTracks -->

Get Play column  in track column  () ...

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |
| `index` | `integer` | No | Optional; default used only if playlist is absent. |
| `start` | `integer` | No | Optional; default 0. |
| `count` | `integer` | No | Optional; default 100. |
| `formats` | `object` | No | Optional; default {}. |

**Returns**:

```json
{
    "playlist": 0,
    "start": 0,
    "count": 20,
    "total": 150,
    "tracks": [
        {
            "index": 0,
            "title": "Song 1",
            "artist": "Artist 1",
            "album": "Album 1",
            "albumArtist": "Artist 1",
            "genre": "Rock",
            "date": "2024",
            "trackNumber": 1,
            "discNumber": 1,
            "duration": 180.5,
            "path": "file://C:/Music/song1.flac",
            "absolutePath": "C:\\\\Music\\\\song1.flac",
            "fileSize": 25600000,
            "subsong": 0,
            "rating": 5,
            "codec": "FLAC",
            "bitrate": 1411,
            "sampleRate": 44100,
            "channels": 2,
            "composer": "Lennon/McCartney",
            "comment": "",
            "playCount": "15",
            "firstPlayed": "2024-01-15 10:30:00",
            "lastPlayed": "2026-02-10 20:00:00",
            "added": "2024-01-10 08:00:00"
        }
    ]
}
```

::: tip column (`formats` Parameter)
`playlist.getTracks` supports  `formats` Parameter TitleFormat column :

```javascript
const result = await fb2k.invoke('playlist.getTracks', {
    start: 0, count: 50,
    formats: {
        myRating: '%rating%',
        codec: '%codec%'
    }
});
//  track  myRating  codec Field
```
:::

::: tip TIP
`absolutePath` Yesfile path, can used for `artwork.getForTrack` etc API.`path` Yes foobar2000 .
:::

### playlist.playTrack

<!-- phase3-major1-review:playlist.playTrack -->
#### Source-reviewed contract
Authority: `src/api/PlaylistApi.cpp:1089-1134`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | `active playlist` |
| `index` | `integer` | No | `track or 0` |
| `track` | `integer` | No | `0` |
| `deferred` | `boolean` | No | `false` |
| `muted` | `boolean` | No | `false` |

**Return keys (vary by response variant)**: `error`, `success`; `error`, `success`; `success`

**Semantics**: index has precedence over the legacy track alias. deferred schedules the default action on the main thread; muted only mutes before play and does not restore a prior mute state.

<!-- phase3-major1-review-end:playlist.playTrack -->

Play playlist in specified track ..

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |
| `index` | `integer` | No | Optional; default track or 0. |
| `track` | `integer` | No | Optional; default 0. |
| `deferred` | `boolean` | No | Optional; default false. |
| `muted` | `boolean` | No | Optional; default false. |

**Returns**: `{ "success": true }`

```javascript
await fb2k.invoke('playlist.playTrack', { playlist: 0, index: 5 });

// （）
await fb2k.invoke('playlist.playTrack', { playlist: 0, index: 0, deferred: true });
```

### playlist.removeTracks

<!-- phase3-major1-review:playlist.removeTracks -->
#### Source-reviewed contract
Authority: `src/api/PlaylistApi.cpp:1017-1041`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | `active playlist` |
| `index` | `integer` | No | `used only if playlist is absent` |
| `items` | `array<integer>` | No | `[]` |

**Return keys (vary by response variant)**: `error`, `success`; `success`

**Semantics**: The shared selector resolves playlist/index. items identifies track indices to remove; locked and invalid targets are rejected before mutation.

<!-- phase3-major1-review-end:playlist.removeTracks -->

 from Play column  in Remove specified track ...

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |
| `index` | `integer` | No | Optional; default used only if playlist is absent. |
| `items` | `array<integer>` | No | Optional; default []. |

**Returns**: `{ "success": true }`

### playlist.removeSelectedTracks

<!-- phase3-major1-review:playlist.removeSelectedTracks -->
#### Source-reviewed contract
Authority: `src/api/PlaylistApi.cpp:1041-1059`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | `active playlist` |
| `index` | `integer` | No | `used only if playlist is absent` |

**Return keys (vary by response variant)**: `error`, `success`; `success`

**Semantics**: The shared selector resolves the target and removes its existing selection. Locked and invalid playlists return false/error; no explicit items argument is consumed.

<!-- phase3-major1-review-end:playlist.removeSelectedTracks -->

Remove Play column  in current selected track ...

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |
| `index` | `integer` | No | Optional; default used only if playlist is absent. |

**Returns**: `{ "success": true }`

### playlist.moveTracks

<!-- phase3-major1-review:playlist.moveTracks -->
#### Source-reviewed contract
Authority: `src/api/PlaylistApi.cpp:1059-1089`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | `active playlist` |
| `index` | `integer` | No | `used only if playlist is absent` |
| `items` | `array<integer>` | No | `[]` |
| `delta` | `integer` | No | `0` |

**Return keys (vary by response variant)**: `error`, `success`; `success`

**Semantics**: The shared selector chooses playlist over index. Non-empty items replace the current selection before moving it by delta; empty items move the existing selection, with locked targets rejected.

<!-- phase3-major1-review-end:playlist.moveTracks -->

Move selected tracks by `delta`. When `items` is non-empty, those indices become the selection first; when `items` is empty, the current selection is moved (SMP-compatible).

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |
| `index` | `integer` | No | Optional; default used only if playlist is absent. |
| `items` | `array<integer>` | No | Optional; default []. |
| `delta` | `integer` | No | Optional; default 0. |

**Returns**: `{ "success": true }`

```javascript
await fb2k.invoke('playlist.moveTracks', { items: [0, 1, 2], delta: 3 });
await fb2k.invoke('playlist.moveTracks', { items: [5, 6], delta: -2 });
```

### playlist.addPaths

<!-- phase3-major1-review:playlist.addPaths -->
#### Source-reviewed contract
Authority: `src/api/PlaylistApi.cpp:1439-1474`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | `active playlist` |
| `paths` | `array<string>` | No | `[]` |

**Return keys (vary by response variant)**: `error`, `success`; `error`, `success`; `countBefore`, `error`, `invalidCount`, `playlist`, `requestedPaths`, `success`; `addedCount`, `countBefore`, `invalidCount`, `playlist`, `requestedPaths`, `success`, `totalCount`

**Semantics**: The non-empty MediaRead paths array is resolved by playlist_incoming_item_filter with subsong handling. The handler rejects invalid or locked targets and reports requestedPaths, addedCount, invalidCount, and count totals.

<!-- phase3-major1-review-end:playlist.addPaths -->

Add file /file  to Play column .use `playlist_incoming_item_filter` sync,  CUE file ...

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |
| `paths` | `array<string>` | No | Optional; default []. |

**Returns**:

```json
{
    "success": true,
    "playlist": 0,
    "requestedPaths": 25,
    "addedCount": 25,
    "invalidCount": 0,
    "countBefore": 0,
    "totalCount": 25
}
```

## Additional public APIs

The sections below are generated from `RegisterApi`; parameter keys are taken from the C++ handler.

### playlist.addHandles

<!-- phase3-major1-review:playlist.addHandles -->
#### Source-reviewed contract
Authority: `src/api/PlaylistApi.cpp:1478-1513`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | `active playlist` |
| `handles` | `array<object or string>` | No | `[]` |

**Return keys (vary by response variant)**: `error`, `success`; `error`, `success`; `error`, `invalidCount`, `playlist`, `requestedCount`, `success`; `addedCount`, `countBefore`, `invalidCount`, `playlist`, `requestedCount`, `success`, `totalCount`

**Semantics**: The target defaults to the active playlist. Handles accept { path, subsong } objects or strings; malformed, empty, oversized, or unresolvable entries increase invalidCount, and a locked target is rejected.

<!-- phase3-major1-review-end:playlist.addHandles -->
Public API method. Runtime authority: `src/api/PlaylistApi.cpp:1887`。

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |
| `handles` | `array<object or string>` | No | Optional; default []. |

| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |
| `handles` | `array<object\\\ | string>` | No Optional; default []. |

**Returns**: `{"addedCount":"...","countBefore":"...","error":"...","invalidCount":"...","playlist":"...","requestedCount":"...","success":true,"totalCount":"..."}`

```js
const result = await fb2k.invoke('playlist.addHandles', { handles: /* value */, playlist: /* value */ });
```

### playlist.addPathsAsync

<!-- phase3-major1-review:playlist.addPathsAsync -->
#### Source-reviewed contract
Authority: `src/api/PlaylistApi.cpp:1678-1722`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | `active playlist` |
| `paths` | `array<string>` | No | `[]` |

**Return keys (vary by response variant)**: `error`, `success`; `error`, `success`; `error`, `invalidCount`, `success`; `invalidCount`, `operationId`, `status`, `success`, `totalCount`

**Semantics**: A non-empty protected paths array starts an asynchronous add and returns operationId plus pending status. Completion is later broadcast as playlist:addComplete; immediate validation failure returns success:false and invalidCount.

<!-- phase3-major1-review-end:playlist.addPathsAsync -->
Public API method. Runtime authority: `src/api/PlaylistApi.cpp:1898`。

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |
| `paths` | `array<string>` | No | Optional; default []. |

**Returns**: `{"error":"...","invalidCount":"...","operationId":"...","status":"...","success":true,"totalCount":"..."}`

```js
const result = await fb2k.invoke('playlist.addPathsAsync', { paths: /* value */, playlist: /* value */ });
```

### playlist.addPathsSequential

<!-- phase3-major1-review:playlist.addPathsSequential -->
#### Source-reviewed contract
Authority: `src/api/PlaylistApi.cpp:1650-1675`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | `active playlist` |
| `paths` | `array<string>` | No | `[]` |

**Return keys (vary by response variant)**: `error`, `success`; `error`, `success`; `addedCount`, `order`, `playlist`, `success`

**Semantics**: A non-empty protected paths array is resolved and inserted in the service’s sequential result order. Locked or invalid target playlists fail before the add; the return order lists inserted indices.

<!-- phase3-major1-review-end:playlist.addPathsSequential -->
Public API method. Runtime authority: `src/api/PlaylistApi.cpp:1897`。

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |
| `paths` | `array<string>` | No | Optional; default []. |

**Returns**: `{"addedCount":"...","error":"...","order":"...","playlist":"...","success":true}`

```js
const result = await fb2k.invoke('playlist.addPathsSequential', { paths: /* value */, playlist: /* value */ });
```

### playlist.convertToAutoplaylist

<!-- phase3-major1-review:playlist.convertToAutoplaylist -->
#### Source-reviewed contract
Authority: `src/api/PlaylistApi.cpp:1294-1315`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | `active playlist` |
| `query` | `string` | No | `` |
| `sort` | `string` | No | `` |
| `keepSorted` | `boolean` | No | `false` |

**Return keys (vary by response variant)**: `error`, `success`; `error`, `success`; `playlist`, `success`; `error`, `success`

**Semantics**: A non-empty query is required despite its empty default. The target defaults to the active playlist; keepSorted controls the autoplaylist sort flag and service errors return success:false.

<!-- phase3-major1-review-end:playlist.convertToAutoplaylist -->
Public API method. Runtime authority: `src/api/PlaylistApi.cpp:1881`。

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |
| `query` | `string` | No | Optional; default . |
| `sort` | `string` | No | Optional; default . |
| `keepSorted` | `boolean` | No | Optional; default false. |

**Returns**: `{"error":"...","playlist":"...","success":true}`

```js
const result = await fb2k.invoke('playlist.convertToAutoplaylist', { keepSorted: /* value */, playlist: /* value */, query: /* value */, sort: /* value */ });
```

### playlist.createAutoplaylist

Public API method. Runtime authority: `src/api/PlaylistApi.cpp:1880`。

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `keepSorted` | `boolean` | No | Optional; default false. |
| `name` | `string` | No | Optional; default New Autoplaylist. |
| `query` | `string` | No | Optional; default . |
| `sort` | `string` | No | Optional; default . |

**Returns**: `{"error":"...","index":"...","name":"...","playlist":"...","query":"...","success":true}`

```js
const result = await fb2k.invoke('playlist.createAutoplaylist', { keepSorted: /* value */, name: /* value */, query: /* value */, sort: /* value */ });
```

### playlist.deselectAll

<!-- phase3-major1-review:playlist.deselectAll -->
#### Source-reviewed contract
Authority: `src/api/PlaylistApi.cpp:1562-1572`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | `active playlist` |

**Return keys (vary by response variant)**: `success`; `success`

**Semantics**: The optional playlist resolves to the active playlist. Invalid targets return success:false; otherwise all selection bits in that playlist are cleared.

<!-- phase3-major1-review-end:playlist.deselectAll -->
Public API method. Runtime authority: `src/api/PlaylistApi.cpp:1892`。

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('playlist.deselectAll', { playlist: /* value */ });
```

### playlist.focusTrack

<!-- phase3-major1-review:playlist.focusTrack -->
#### Source-reviewed contract
Authority: `src/api/PlaylistApi.cpp:1137-1151`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | `active playlist` |
| `index` | `integer` | No | `no focused item` |
| `track` | `integer` | No | `no focused item` |

**Return keys (vary by response variant)**: `error`, `success`; `error`, `success`; `success`

**Semantics**: Deprecated compatibility method: index wins over track. Omitted track clears focus using the infinite-size sentinel; invalid target or supplied track index returns success:false.

<!-- phase3-major1-review-end:playlist.focusTrack -->
Public API method. Runtime authority: `src/api/PlaylistApi.cpp:1873`。

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |
| `index` | `integer` | No | Optional; default no focused item. |
| `track` | `integer` | No | Optional; default no focused item. |

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('playlist.focusTrack', { index: /* value */, playlist: /* value */, track: /* value */ });
```

### playlist.getAutoplaylistInfo

<!-- phase3-major1-review:playlist.getAutoplaylistInfo -->
#### Source-reviewed contract
Authority: `src/api/PlaylistApi.cpp:1344-1369`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | `active playlist` |

**Returns**: `{"error":"...","isAutoplaylist":true,"keepSorted":"...","lockName":"...","playlist":0,"source":"...","success":true}`

**Semantics**: The target defaults to active. The response distinguishes non-autoplaylists from SDK and DUI autoplaylists and reports keepSorted/source/lockName when available.

<!-- phase3-major1-review-end:playlist.getAutoplaylistInfo -->
Public API method. Runtime authority: `src/api/PlaylistApi.cpp:1883`。

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |


```js
const result = await fb2k.invoke('playlist.getAutoplaylistInfo', { playlist: /* value */ });
```

### playlist.getAutoplaylistQuery

<!-- phase3-major1-review:playlist.getAutoplaylistQuery -->
#### Source-reviewed contract
Authority: `src/api/PlaylistApi.cpp:1372-1399`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | `active playlist` |

**Returns**: `{"error":"...","isAutoplaylist":true,"keepSorted":"...","lockName":"...","note":"...","playlist":0,"query":"...","source":"...","success":true}`

**Semantics**: The target defaults to active. foobar2000 does not expose the query string, so an autoplaylist response deliberately carries query:null plus a note and source metadata.

<!-- phase3-major1-review-end:playlist.getAutoplaylistQuery -->
Public API method. Runtime authority: `src/api/PlaylistApi.cpp:1884`。

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |


```js
const result = await fb2k.invoke('playlist.getAutoplaylistQuery', { playlist: /* value */ });
```

### playlist.getAvailableColumns

<!-- phase3-major1-review:playlist.getAvailableColumns -->
#### Source-reviewed contract
Authority: `src/api/PlaylistApi.cpp:1816-1848`.

_No public parameters._

**Return keys (vary by response variant)**: No named fields.

**Semantics**: No request fields are read. The return is an array assembled from DUI column providers; each item carries id, name, pattern, alignment, numeric, and optional sortPattern.

<!-- phase3-major1-review-end:playlist.getAvailableColumns -->
Public API method. Runtime authority: `src/api/PlaylistApi.cpp:1901`。

_No parameters._

**Returns**:  when handler return JSON .

```js
const result = await fb2k.invoke('playlist.getAvailableColumns');
```

### playlist.getFocusTrack

<!-- phase3-major1-review:playlist.getFocusTrack -->
#### Source-reviewed contract
Authority: `src/api/PlaylistApi.cpp:1154-1163`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | `active playlist` |

**Return keys (vary by response variant)**: `error`, `success`; `index`, `playlist`, `success`

**Semantics**: Deprecated compatibility getter. It resolves the active playlist when omitted and returns success:false for an invalid target; no focus is represented by index:-1.

<!-- phase3-major1-review-end:playlist.getFocusTrack -->
Public API method. Runtime authority: `src/api/PlaylistApi.cpp:1874`。

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |

**Returns**: `{"error":"...","index":"...","playlist":"...","success":true}`

```js
const result = await fb2k.invoke('playlist.getFocusTrack', { playlist: /* value */ });
```

### playlist.getFocusedTrack

<!-- phase3-major1-review:playlist.getFocusedTrack -->
#### Source-reviewed contract
Authority: `src/api/PlaylistApi.cpp:1572-1582`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | `active playlist` |

**Return keys (vary by response variant)**: `index`, `success`; `index`, `playlist`, `success`

**Semantics**: The target defaults to active. Valid calls return the playlist and focused index, using -1 where no item has focus; an invalid target returns success:true with index:-1 for compatibility.

<!-- phase3-major1-review-end:playlist.getFocusedTrack -->
Public API method. Runtime authority: `src/api/PlaylistApi.cpp:1893`。

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |

**Returns**: `{"index":"...","playlist":"...","success":true}`

```js
const result = await fb2k.invoke('playlist.getFocusedTrack', { playlist: /* value */ });
```

### playlist.getLockInfo

<!-- phase3-major1-review:playlist.getLockInfo -->
#### Source-reviewed contract
Authority: `src/api/PlaylistApi.cpp:1516-1525`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | `active playlist` |

**Return keys (vary by response variant)**: `error`, `success`; `isLocked`, `playlist`

**Semantics**: The target defaults to active and returns playlist plus isLocked. Invalid targets use the false/error response variant.

<!-- phase3-major1-review-end:playlist.getLockInfo -->
Public API method. Runtime authority: `src/api/PlaylistApi.cpp:1888`。

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |

**Returns**: `{"error":"...","isLocked":"...","playlist":"...","success":true}`

```js
const result = await fb2k.invoke('playlist.getLockInfo', { playlist: /* value */ });
```

### playlist.getSelectedTracks

<!-- phase3-major1-review:playlist.getSelectedTracks -->
#### Source-reviewed contract
Authority: `src/api/PlaylistApi.cpp:981-995`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | `active playlist` |
| `index` | `integer` | No | `ignored when playlist is supplied` |

**Return keys (vary by response variant)**: `error`, `success`, `tracks`

**Semantics**: Both selector names are supported by the shared helper, with playlist taking precedence. The return tracks array contains currently selected track records or a false/error variant for invalid targets.

<!-- phase3-major1-review-end:playlist.getSelectedTracks -->
Public API method. Runtime authority: `src/api/PlaylistApi.cpp:1867`。

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |
| `index` | `integer` | No | Optional; default ignored when playlist is supplied. |

**Returns**: `{"error":"...","success":true,"tracks":"..."}`

```js
const result = await fb2k.invoke('playlist.getSelectedTracks');
```

### playlist.getSelection

<!-- phase3-major1-review:playlist.getSelection -->
#### Source-reviewed contract
Authority: `src/api/PlaylistApi.cpp:1538-1552`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | `active playlist` |

**Return keys (vary by response variant)**: `error`, `success`; `count`, `items`, `playlist`, `success`

**Semantics**: The target defaults to active. The response contains the selected item indices, count, and resolved playlist; invalid targets return false/error.

<!-- phase3-major1-review-end:playlist.getSelection -->
Public API method. Runtime authority: `src/api/PlaylistApi.cpp:1890`。

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |

**Returns**: `{"count":"...","error":"...","items":"...","playlist":"...","success":true}`

```js
const result = await fb2k.invoke('playlist.getSelection', { playlist: /* value */ });
```

### playlist.insertTracks

<!-- phase3-major1-review:playlist.insertTracks -->
#### Source-reviewed contract
Authority: `src/api/PlaylistApi.cpp:913-948`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | `active playlist` |
| `position` | `integer` | No | `index or 0` |
| `index` | `integer` | No | `0` |
| `handles` | `array<object or string>` | No | `[]` |

**Return keys (vary by response variant)**: `error`, `success`; `error`, `success`; `error`, `invalidCount`, `playlist`, `requestedCount`, `success`; `addedCount`, `countBefore`, `insertIndex`, `invalidCount`, `playlist`, `requestedCount`, `success`, `totalCount`

**Semantics**: position wins over index as the insertion location. A non-empty handles array is required in practice; items are validated by the service and locked targets fail before mutation.

<!-- phase3-major1-review-end:playlist.insertTracks -->
Public API method. Runtime authority: `src/api/PlaylistApi.cpp:1864`。

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |
| `position` | `integer` | No | Optional; default index or 0. |
| `index` | `integer` | No | Optional; default 0. |
| `handles` | `array<object or string>` | No | Optional; default []. |

| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |
| `position` | `integer` | No | Optional; default index or 0. |
| `index` | `integer` | No | Optional; default 0. |
| `handles` | `array<object\\\ | string>` | No Optional; default []. |

**Returns**: `{"addedCount":"...","countBefore":"...","error":"...","insertIndex":"...","invalidCount":"...","playlist":"...","requestedCount":"...","success":true,"totalCount":"..."}`

```js
const result = await fb2k.invoke('playlist.insertTracks', { handles: /* value */, index: /* value */, playlist: /* value */, position: /* value */ });
```

### playlist.isAutoplaylist

<!-- phase3-major1-review:playlist.isAutoplaylist -->
#### Source-reviewed contract
Authority: `src/api/PlaylistApi.cpp:1250-1262`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | `active playlist` |

**Returns**: `{"error":"...","isAutoplaylist":true,"lockName":"...","playlist":0,"success":true}`

**Semantics**: The target defaults to active and reports playlist/isAutoplaylist with optional lockName. Invalid targets use success:false with an error.

<!-- phase3-major1-review-end:playlist.isAutoplaylist -->
Public API method. Runtime authority: `src/api/PlaylistApi.cpp:1879`。

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |


```js
const result = await fb2k.invoke('playlist.isAutoplaylist', { playlist: /* value */ });
```

### playlist.isLocked

<!-- phase3-major1-review:playlist.isLocked -->
#### Source-reviewed contract
Authority: `src/api/PlaylistApi.cpp:1528-1535`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | `active playlist` |

**Return keys (vary by response variant)**: `error`, `isLocked`, `success`; `isLocked`, `success`

**Semantics**: The target defaults to active. Valid responses include success and isLocked; an invalid target explicitly returns isLocked:false with an error.

<!-- phase3-major1-review-end:playlist.isLocked -->
Public API method. Runtime authority: `src/api/PlaylistApi.cpp:1889`。

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |

**Returns**: `{"error":"...","isLocked":"...","success":true}`

```js
const result = await fb2k.invoke('playlist.isLocked', { playlist: /* value */ });
```

### playlist.redo

<!-- phase3-major1-review:playlist.redo -->
#### Source-reviewed contract
Authority: `src/api/PlaylistApi.cpp:1234-1245`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | `active playlist` |

**Return keys (vary by response variant)**: `error`, `success`; `success`

**Semantics**: The target defaults to active. Invalid targets return success:false; otherwise success reflects whether the playlist redo stack had an action to restore.

<!-- phase3-major1-review-end:playlist.redo -->
Public API method. Runtime authority: `src/api/PlaylistApi.cpp:1878`。

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('playlist.redo', { playlist: /* value */ });
```

### playlist.removeAutoplaylist

<!-- phase3-major1-review:playlist.removeAutoplaylist -->
#### Source-reviewed contract
Authority: `src/api/PlaylistApi.cpp:1318-1341`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | `active playlist` |

**Return keys (vary by response variant)**: `error`, `success`; `playlist`, `source`, `success`; `note`, `playlist`, `source`, `success`; `error`, `success`

**Semantics**: SDK autoplaylists are converted back to normal playlists. DUI-detected autoplaylists return a successful informational dui/source/note variant rather than directly removing the lock.

<!-- phase3-major1-review-end:playlist.removeAutoplaylist -->
Public API method. Runtime authority: `src/api/PlaylistApi.cpp:1882`。

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |

**Returns**: `{"error":"...","note":"...","playlist":"...","source":"...","success":true}`

```js
const result = await fb2k.invoke('playlist.removeAutoplaylist', { playlist: /* value */ });
```

### playlist.reorder

<!-- phase3-major1-review:playlist.reorder -->
#### Source-reviewed contract
Authority: `src/api/PlaylistApi.cpp:1618-1647`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | `active playlist` |
| `newOrder` | `array<integer>` | No | `[]` |

**Return keys (vary by response variant)**: `error`, `success`; `error`, `expected`, `got`, `success`; `error`, `success`; `error`, `index`, `success`; `itemCount`, `playlist`, `success`

**Semantics**: newOrder must have exactly one numeric in-range source index per playlist item. The handler validates length and elements, records undo, then applies the order; it does not prove uniqueness.

<!-- phase3-major1-review-end:playlist.reorder -->
Public API method. Runtime authority: `src/api/PlaylistApi.cpp:1896`。

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |
| `newOrder` | `array<integer>` | No | Optional; default []. |

**Returns**: `{"error":"...","expected":"...","got":"...","index":"...","itemCount":"...","playlist":"...","success":true}`

```js
const result = await fb2k.invoke('playlist.reorder', { newOrder: /* value */, playlist: /* value */ });
```

### playlist.reorderPlaylists

<!-- phase3-major1-review:playlist.reorderPlaylists -->
#### Source-reviewed contract
Authority: `src/api/PlaylistApi.cpp:1782-1813`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `newOrder` | `array<integer>` | No | `[]` |

**Return keys (vary by response variant)**: `error`, `expected`, `got`, `success`; `error`, `success`; `error`, `index`, `success`; `count`, `success`

**Semantics**: newOrder must match the current playlist count and contain in-range numeric indices. The service reorder result becomes success and the count is always returned on the normal result path.

<!-- phase3-major1-review-end:playlist.reorderPlaylists -->
Public API method. Runtime authority: `src/api/PlaylistApi.cpp:1900`。

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `newOrder` | `array<integer>` | No | Optional; default []. |

**Returns**: `{"count":"...","error":"...","expected":"...","got":"...","index":"...","success":true}`

```js
const result = await fb2k.invoke('playlist.reorderPlaylists', { newOrder: /* value */ });
```

### playlist.replaceAllAndPlay

<!-- phase3-major1-review:playlist.replaceAllAndPlay -->
#### Source-reviewed contract
Authority: `src/api/PlaylistApi.cpp:1725-1779`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | `active playlist` |
| `paths` | `array<string>` | No | `[]` |
| `playIndex` | `integer` | No | `0` |
| `stopFirst` | `boolean` | No | `true` |
| `autoPlay` | `boolean` | No | `true` |

**Return keys (vary by response variant)**: `error`, `success`; `error`, `success`; `clearedCount`, `error`, `invalidCount`, `success`; `addedCount`, `clearedCount`, `playIndex`, `playlist`, `success`, `totalCount`

**Semantics**: The non-empty MediaRead paths array replaces the resolved unlocked playlist atomically after optional stop. An out-of-range playIndex becomes zero; autoPlay false focuses the item instead of starting it.

<!-- phase3-major1-review-end:playlist.replaceAllAndPlay -->
Public API method. Runtime authority: `src/api/PlaylistApi.cpp:1899`。

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |
| `paths` | `array<string>` | No | Optional; default []. |
| `playIndex` | `integer` | No | Optional; default 0. |
| `stopFirst` | `boolean` | No | Optional; default true. |
| `autoPlay` | `boolean` | No | Optional; default true. |

**Returns**: `{"addedCount":"...","clearedCount":"...","error":"...","invalidCount":"...","playIndex":"...","playlist":"...","success":true,"totalCount":"..."}`

```js
const result = await fb2k.invoke('playlist.replaceAllAndPlay', { autoPlay: /* value */, paths: /* value */, playIndex: /* value */, playlist: /* value */, stopFirst: /* value */ });
```

### playlist.reverse

<!-- phase3-major1-review:playlist.reverse -->
#### Source-reviewed contract
Authority: `src/api/PlaylistApi.cpp:1597-1615`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | `active playlist` |

**Return keys (vary by response variant)**: `success`; `success`; `success`

**Semantics**: The target defaults to active. Locked or invalid playlists fail; lists with fewer than two items succeed without mutation, otherwise the handler stores undo and reverses all positions.

<!-- phase3-major1-review-end:playlist.reverse -->
Public API method. Runtime authority: `src/api/PlaylistApi.cpp:1895`。

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('playlist.reverse', { playlist: /* value */ });
```

### playlist.selectAll

<!-- phase3-major1-review:playlist.selectAll -->
#### Source-reviewed contract
Authority: `src/api/PlaylistApi.cpp:1552-1562`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | `active playlist` |

**Return keys (vary by response variant)**: `success`; `success`

**Semantics**: The target defaults to active. Invalid targets return success:false; a valid target selects every item through the playlist service.

<!-- phase3-major1-review-end:playlist.selectAll -->
Public API method. Runtime authority: `src/api/PlaylistApi.cpp:1891`。

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('playlist.selectAll', { playlist: /* value */ });
```

### playlist.setFocusedTrack

<!-- phase3-major1-review:playlist.setFocusedTrack -->
#### Source-reviewed contract
Authority: `src/api/PlaylistApi.cpp:1582-1597`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | `active playlist` |
| `index` | `integer` | No | `no focused item` |

**Return keys (vary by response variant)**: `error`, `success`; `error`, `success`; `success`

**Semantics**: The target defaults to active. index may be omitted to set the no-focus sentinel; supplied indices must be within the playlist track count.

<!-- phase3-major1-review-end:playlist.setFocusedTrack -->
Public API method. Runtime authority: `src/api/PlaylistApi.cpp:1894`。

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |
| `index` | `integer` | No | Optional; default no focused item. |

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('playlist.setFocusedTrack', { index: /* value */, playlist: /* value */ });
```

### playlist.setSelection

<!-- phase3-major1-review:playlist.setSelection -->
#### Source-reviewed contract
Authority: `src/api/PlaylistApi.cpp:995-1017`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | `active playlist` |
| `index` | `integer` | No | `used only if playlist is absent` |
| `indices` | `array<integer>` | No | `[]` |
| `clearOthers` | `boolean` | No | `true` |

**Return keys (vary by response variant)**: `error`, `success`; `success`

**Semantics**: The shared selector resolves playlist/index. indices are converted to item indices and clearOthers selects replacement versus additive selection; invalid target selection fails.

<!-- phase3-major1-review-end:playlist.setSelection -->
Public API method. Runtime authority: `src/api/PlaylistApi.cpp:1868`。

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |
| `index` | `integer` | No | Optional; default used only if playlist is absent. |
| `indices` | `array<integer>` | No | Optional; default []. |
| `clearOthers` | `boolean` | No | Optional; default true. |

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('playlist.setSelection', { clearOthers: /* value */, indices: /* value */ });
```

### playlist.shuffle

Public API method. Runtime authority: `src/api/PlaylistApi.cpp:1876`。

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |
| `index` | `integer` | No | Optional; default used only if playlist is absent. |

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('playlist.shuffle', { playlist: /* value */, index: /* value */ });
```

### playlist.sort

Public API method. Runtime authority: `src/api/PlaylistApi.cpp:1875`。

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |
| `index` | `integer` | No | Optional; default used only if playlist is absent. |
| `pattern` | `string` | No | Optional; default %title%. |
| `descending` | `boolean` | No | Optional; default false. |
| `selectedOnly` | `boolean` | No | Optional; default false. |

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('playlist.sort', { descending: /* value */, pattern: /* value */, selectedOnly: /* value */, playlist: /* value */, index: /* value */ });
```

### playlist.undo

<!-- phase3-major1-review:playlist.undo -->
#### Source-reviewed contract
Authority: `src/api/PlaylistApi.cpp:1223-1234`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | `active playlist` |

**Return keys (vary by response variant)**: `error`, `success`; `success`

**Semantics**: The target defaults to active. Invalid targets return success:false; otherwise success reflects whether the playlist undo stack restored an action.

<!-- phase3-major1-review-end:playlist.undo -->
Public API method. Runtime authority: `src/api/PlaylistApi.cpp:1877`。

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('playlist.undo', { playlist: /* value */ });
```

## Related playlist events

`src/callbacks/PlaylistCallback.cpp` will event. will  JIT shadow playlist event.

| event |  when  | Payload keys |
| --- | --- | --- |
| `playlist:itemsAdded` | Insert Play column  after . | `{ playlist, start, count }` .. |
| `playlist:itemsRemoved` | from Play column Remove after . | `{ playlist, oldCount, newCount }` .. |
| `playlist:itemsReordered` | single Play column  in  after . | `{ playlist, count }` . |
| `playlist:selectionChanged` | Play column Select  after . | `{ playlist }` .. |
| `playlist:focusChanged` | Play column focus  after . | `{ playlist, from, to }` . |
| `playlist:itemsReplaced` | Play column Replace after . | `{ playlist, count }` .. |
| `playlist:created` | Play column  after . | `{ index, name }` . |
| `playlist:removed` | Remove Play column  after . | `{ oldCount, newCount }` .. |
| `playlist:reordered` | Play column  after . | `{ count }` . |
| `playlist:activated` | Play column  after . | `{ oldIndex, newIndex }` . |
| `playlist:renamed` | Play column Rename after . | `{ index, name }` .. |
| `playlist:lockChanged` | Play column state  after . | `{ playlist, locked }` . |
| `playlist:defaultFormatChanged` | default Play column  after . | `{}` . |
| `playlist:addComplete` | asyncpathAdd  after . | `{ operationId, success, addedCount, totalCount }` . |

 foobar2000 may report `from`, `to`, `oldIndex`, and `newIndex` as `-1` when an index is unavailable.

## Contract supplements

The sections below close public-contract findings from the strict parameter audit without replacing existing explanations.

<!-- phase3-supplement:playlist.moveTracks -->
### Contract supplement: `playlist.moveTracks`

Verified contract supplement. Runtime authority: `src/api/PlaylistApi.cpp:1059-1089`.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | No | `active playlist` | Optional; default active playlist. |
| `index` | `integer` | No | `used only if playlist is absent` | Optional; default used only if playlist is absent. |
| `items` | `array<integer>` | No | `[]` | Optional; default []. |
| `delta` | `integer` | No | `0` | Optional; default 0. |

#### Return fields

| Field | Type | Optional |
| --- | --- | --- |
| `error` | `string` | Yes |
| `success` | `boolean` | No |

Semantics: omitted optional parameters use handler defaults; failure branches and error fields are defined by this source file.

```js
const result = await fb2k.invoke('playlist.moveTracks', { playlist: /* value */, index: /* value */, items: /* value */, delta: /* value */ });
```
<!-- phase3-supplement:playlist.playTrack -->
### Contract supplement: `playlist.playTrack`

Verified contract supplement. Runtime authority: `src/api/PlaylistApi.cpp:1089-1134`.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | No | `active playlist` | Optional; default active playlist. |
| `index` | `integer` | No | `track or 0` | Optional; default track or 0. |
| `track` | `integer` | No | `0` | Optional; default 0. |
| `deferred` | `boolean` | No | `false` | Optional; default false. |
| `muted` | `boolean` | No | `false` | Optional; default false. |

#### Return fields

| Field | Type | Optional |
| --- | --- | --- |
| `error` | `string` | Yes |
| `success` | `boolean` | No |

Semantics: omitted optional parameters use handler defaults; failure branches and error fields are defined by this source file.

```js
const result = await fb2k.invoke('playlist.playTrack', { playlist: /* value */, index: /* value */, track: /* value */, deferred: /* value */, muted: /* value */ });
```
<!-- phase3-supplement:playlist.removeSelectedTracks -->
### Contract supplement: `playlist.removeSelectedTracks`

Verified contract supplement. Runtime authority: `src/api/PlaylistApi.cpp:1041-1059`.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | No | `active playlist` | Optional; default active playlist. |
| `index` | `integer` | No | `used only if playlist is absent` | Optional; default used only if playlist is absent. |

#### Return fields

| Field | Type | Optional |
| --- | --- | --- |
| `error` | `string` | Yes |
| `success` | `boolean` | No |

Semantics: omitted optional parameters use handler defaults; failure branches and error fields are defined by this source file.

```js
const result = await fb2k.invoke('playlist.removeSelectedTracks', { playlist: /* value */, index: /* value */ });
```
<!-- phase3-supplement:playlist.removeTracks -->
### Contract supplement: `playlist.removeTracks`

Verified contract supplement. Runtime authority: `src/api/PlaylistApi.cpp:1017-1041`.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | No | `active playlist` | Optional; default active playlist. |
| `index` | `integer` | No | `used only if playlist is absent` | Optional; default used only if playlist is absent. |
| `items` | `array<integer>` | No | `[]` | Optional; default []. |

#### Return fields

| Field | Type | Optional |
| --- | --- | --- |
| `error` | `string` | Yes |
| `success` | `boolean` | No |

Semantics: omitted optional parameters use handler defaults; failure branches and error fields are defined by this source file.

```js
const result = await fb2k.invoke('playlist.removeTracks', { playlist: /* value */, index: /* value */, items: /* value */ });
```
<!-- phase3-supplement:playlist.setSelection -->
### Contract supplement: `playlist.setSelection`

Verified contract supplement. Runtime authority: `src/api/PlaylistApi.cpp:995-1017`.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | No | `active playlist` | Optional; default active playlist. |
| `index` | `integer` | No | `used only if playlist is absent` | Optional; default used only if playlist is absent. |
| `indices` | `array<integer>` | No | `[]` | Optional; default []. |
| `clearOthers` | `boolean` | No | `true` | Optional; default true. |

#### Return fields

| Field | Type | Optional |
| --- | --- | --- |
| `error` | `string` | Yes |
| `success` | `boolean` | No |

Semantics: omitted optional parameters use handler defaults; failure branches and error fields are defined by this source file.

```js
const result = await fb2k.invoke('playlist.setSelection', { playlist: /* value */, index: /* value */, indices: /* value */, clearOthers: /* value */ });
```
<!-- phase3-supplement:playlist.shuffle -->
### Contract supplement: `playlist.shuffle`

Verified contract supplement. Runtime authority: `src/api/PlaylistApi.cpp:1190-1223`.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | No | `active playlist` | Optional; default active playlist. |
| `index` | `integer` | No | `used only if playlist is absent` | Optional; default used only if playlist is absent. |

#### Return fields

| Field | Type | Optional |
| --- | --- | --- |
| `error` | `string` | Yes |
| `success` | `boolean` | No |

Semantics: omitted optional parameters use handler defaults; failure branches and error fields are defined by this source file.

```js
const result = await fb2k.invoke('playlist.shuffle', { playlist: /* value */, index: /* value */ });
```
<!-- phase3-supplement:playlist.sort -->
### Contract supplement: `playlist.sort`

Verified contract supplement. Runtime authority: `src/api/PlaylistApi.cpp:1163-1190`.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | No | `active playlist` | Optional; default active playlist. |
| `index` | `integer` | No | `used only if playlist is absent` | Optional; default used only if playlist is absent. |
| `pattern` | `string` | No | `%title%` | Optional; default %title%. |
| `descending` | `boolean` | No | `false` | Optional; default false. |
| `selectedOnly` | `boolean` | No | `false` | Optional; default false. |

#### Return fields

| Field | Type | Optional |
| --- | --- | --- |
| `error` | `string` | Yes |
| `success` | `boolean` | No |

Semantics: omitted optional parameters use handler defaults; failure branches and error fields are defined by this source file.

```js
const result = await fb2k.invoke('playlist.sort', { playlist: /* value */, index: /* value */, pattern: /* value */, descending: /* value */, selectedOnly: /* value */ });
```
