# Queue API

English API reference for the `jitQueue`, `queue`, `selection` family.

This page is the primary owner for the namespaces listed below. Method names, parameter keys, and return fields follow the C++ `RegisterApi` handlers.

## jitQueue

### jitQueue.clear

Public API method. Runtime authority: `src/api/QueueApi.cpp:660`.

_No parameters._

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('jitQueue.clear');
```

### jitQueue.enqueueNext

Public API method. Runtime authority: `src/api/QueueApi.cpp:657`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `title` | `string` | No | Optional; default . |
| `trackId` | `string` | No | Optional; default . |
| `url` | `string` | No | Optional; default . |

**Returns**: `{"bufferSize":"...","error":"...","success":true,"trackId":"..."}`

```js
const result = await fb2k.invoke('jitQueue.enqueueNext', { title: /* value */, trackId: /* value */, url: /* value */ });
```

### jitQueue.getState

Public API method. Runtime authority: `src/api/QueueApi.cpp:661`.

_No parameters._

**Returns**: `{"bufferSize":"...","currentTrackId":"...","isActive":"...","nextTrackId":"...","shadowPlaylist":"...","state":"..."}`

```js
const result = await fb2k.invoke('jitQueue.getState');
```

### jitQueue.notifyEmpty

Public API method. Runtime authority: `src/api/QueueApi.cpp:662`.

_No parameters._

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('jitQueue.notifyEmpty');
```

### jitQueue.playNow

Public API method. Runtime authority: `src/api/QueueApi.cpp:656`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `title` | `string` | No | Optional; default . |
| `trackId` | `string` | No | Optional; default . |
| `url` | `string` | No | Optional; default . |

**Returns**: `{"error":"...","shadowPlaylist":"...","success":true,"trackId":"..."}`

```js
const result = await fb2k.invoke('jitQueue.playNow', { title: /* value */, trackId: /* value */, url: /* value */ });
```

### jitQueue.preloadBatch


<!-- phase3-major1-review:jitQueue.preloadBatch -->
#### Source-reviewed contract
Authority: `src/api/QueueApi.cpp:600-645`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `urls` | `array<string>` | No | `[]` |
| `startIndex` | `integer` | No | `0` |
| `replace` | `boolean` | No | `true` |

**Returns**: `{"error":"...","invalidCount":"...","success":true,"tracksAdded":"..."}`

**Semantics**: The handler accepts an omitted URLs array, delegates batch construction to QueueManager, and returns its success/error result. replace defaults to clearing the JIT buffer before preload; URL/path validity is evaluated by QueueManager.

<!-- phase3-major1-review-end:jitQueue.preloadBatch -->
Public API method. Runtime authority: `src/api/QueueApi.cpp:663`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `urls` | `array<string>` | No | Optional; default []. |
| `startIndex` | `integer` | No | Optional; default 0. |
| `replace` | `boolean` | No | Optional; default true. |


```js
const result = await fb2k.invoke('jitQueue.preloadBatch', { replace: /* value */, startIndex: /* value */, urls: /* value */ });
```

### jitQueue.skip

Public API method. Runtime authority: `src/api/QueueApi.cpp:658`.

_No parameters._

**Returns**: `{"currentTrackId":"...","success":true}`

```js
const result = await fb2k.invoke('jitQueue.skip');
```

### jitQueue.stop

Public API method. Runtime authority: `src/api/QueueApi.cpp:659`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `clearBuffer` | `boolean` | No | Optional; default true. |

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('jitQueue.stop', { clearBuffer: /* value */ });
```

## queue

### queue.add


<!-- phase3-major1-review:queue.add -->
#### Source-reviewed contract
Authority: `src/api/QueueApi.cpp:221-261`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | `active playlist` |
| `tracks` | `array<integer>` | No | `[]` |
| `track` | `integer` | No | `not supplied` |

**Return keys (vary by response variant)**: `error`, `success`; `addedCount`, `queueCount`, `success`

**Semantics**: playlist defaults to the active playlist. tracks takes precedence when it is an array; otherwise track is used. Invalid playlist or item indices produce a false result without queueing an item.

<!-- phase3-major1-review-end:queue.add -->
Public API method. Runtime authority: `src/api/QueueApi.cpp:471`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |
| `tracks` | `array<integer>` | No | Optional; default []. |
| `track` | `integer` | No | Optional; default not supplied. |

| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |
| `tracks` | `array<integer>` | No | Optional; default []. |
| `track` | `integer` | No | Optional; default not supplied. |

**Returns**: `{"addedCount":"...","error":"...","queueCount":"...","success":true}`

```js
const result = await fb2k.invoke('queue.add', { playlist: /* value */, track: /* value */, tracks: /* value */ });
```

### queue.addPaths


<!-- phase3-major1-review:queue.addPaths -->
#### Source-reviewed contract
Authority: `src/api/QueueApi.cpp:266-337`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `paths` | `array<string>` | No | `[]` |
| `useQueuePlaylist` | `boolean` | No | `true` |
| `playlist` | `integer` | No | `active playlist when useQueuePlaylist is false` |

**Return keys (vary by response variant)**: `error`, `success`; `error`, `success`; `error`, `success`; `error`, `isLocked`, `playlist`, `success`; `error`, `invalidCount`, `success`; `addedCount`, `invalidCount`, `playlist`, `queueCount`, `success`

**Semantics**: An empty paths array fails. The registration protects paths as a non-empty MediaRead array; each path is parsed for an optional subsong suffix, rejects oversized streams, then resolves to queueable handles. The handler rejects locked target playlists.

<!-- phase3-major1-review-end:queue.addPaths -->
Public API method. Runtime authority: `src/api/QueueApi.cpp:472`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `paths` | `array<string>` | No | Optional; default []. |
| `useQueuePlaylist` | `boolean` | No | Optional; default true. |
| `playlist` | `integer` | No | Optional; default active playlist when useQueuePlaylist is false. |

**Returns**: `{"addedCount":"...","error":"...","invalidCount":"...","isLocked":"...","playlist":"...","queueCount":"...","success":true}`

```js
const result = await fb2k.invoke('queue.addPaths', { paths: /* value */, playlist: /* value */, useQueuePlaylist: /* value */ });
```

### queue.clear

Public API method. Runtime authority: `src/api/QueueApi.cpp:474`.

_No parameters._

**Returns**: `{"clearedCount":"...","success":true}`

```js
const result = await fb2k.invoke('queue.clear');
```

### queue.flush

Public API method. Runtime authority: `src/api/QueueApi.cpp:479`.

_No parameters._

**Returns**: `{"clearedCount":"...","success":true}`

```js
const result = await fb2k.invoke('queue.flush');
```

### queue.get

Public API method. Runtime authority: `src/api/QueueApi.cpp:470`.

_No parameters._

**Returns**: `{"count":"...","items":"..."}`

```js
const result = await fb2k.invoke('queue.get');
```

### queue.getCount

Public API method. Runtime authority: `src/api/QueueApi.cpp:475`.

_No parameters._

**Returns**: `{"count":"...","hasItems":"..."}`

```js
const result = await fb2k.invoke('queue.getCount');
```

### queue.moveToTop


<!-- phase3-major1-review:queue.moveToTop -->
#### Source-reviewed contract
Authority: `src/api/QueueApi.cpp:422-459`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `index` | `integer` | No | `not supplied` |

**Return keys (vary by response variant)**: `error`, `success`; `movedIndex`, `queueCount`, `success`

**Semantics**: index must identify a non-first item in a non-empty queue. The operation rebuilds the native queue with that item first, so queue order—not playlist membership—is changed.

<!-- phase3-major1-review-end:queue.moveToTop -->
Public API method. Runtime authority: `src/api/QueueApi.cpp:476`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `index` | `integer` | No | Optional; default not supplied. |

**Returns**: `{"error":"...","movedIndex":"...","queueCount":"...","success":true}`

```js
const result = await fb2k.invoke('queue.moveToTop', { index: /* value */ });
```

### queue.remove


<!-- phase3-major1-review:queue.remove -->
#### Source-reviewed contract
Authority: `src/api/QueueApi.cpp:341-392`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `index` | `integer` | No | `not supplied` |
| `indices` | `array<integer>` | No | `[]` |

**Return keys (vary by response variant)**: `error`, `success`; `error`, `success`; `queueCount`, `removedIndex`, `success`; `queueCount`, `removedCount`, `success`; `error`, `success`

**Semantics**: The handler prefers index when present; otherwise it removes distinct valid entries from indices. An empty queue, invalid single index, or neither field returns success:false with an error.

<!-- phase3-major1-review-end:queue.remove -->
Public API method. Runtime authority: `src/api/QueueApi.cpp:473`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `index` | `integer` | No | Optional; default not supplied. |
| `indices` | `array<integer>` | No | Optional; default []. |

**Returns**: `{"error":"...","queueCount":"...","removedCount":"...","removedIndex":"...","success":true}`

```js
const result = await fb2k.invoke('queue.remove', { index: /* value */, indices: /* value */ });
```

## selection

### selection.get


<!-- phase3-major1-review:selection.get -->
#### Source-reviewed contract
Authority: `src/api/SelectionApi.cpp:192-250`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `offset` | `integer` | No | `0` |
| `limit` | `integer` | No | `100` |

**Returns**: `{"count":0,"handles":"...","hasMore":true,"offset":"...","truncated":"...","type":"..."}`

**Semantics**: Selection paging uses offset and a bounded limit in the selection service. The method observes global selection state and does not mutate the active playlist.

<!-- phase3-major1-review-end:selection.get -->
Public API method. Runtime authority: `src/api/SelectionApi.cpp:406`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `offset` | `integer` | No | Optional; default 0. |
| `limit` | `integer` | No | Optional; default 100. |


```js
const result = await fb2k.invoke('selection.get', { limit: /* value */, offset: /* value */ });
```

### selection.getType


<!-- phase3-major1-review:selection.getType -->
#### Source-reviewed contract
Authority: `src/api/SelectionApi.cpp:255-278`.

_No public parameters._

**Return keys (vary by response variant)**: `type`, `typeName`

**Semantics**: No request fields are read. The returned numeric type and typeName describe the current foobar2000 selection source.

<!-- phase3-major1-review-end:selection.getType -->
Public API method. Runtime authority: `src/api/SelectionApi.cpp:407`.

_No parameters._

**Returns**: `{"type":"...","typeName":"..."}`

```js
const result = await fb2k.invoke('selection.getType');
```

### selection.getViewerMode

Public API method. Runtime authority: `src/api/SelectionApi.cpp:404`.

_No parameters._

**Returns**: `{"mode":"..."}`

```js
const result = await fb2k.invoke('selection.getViewerMode');
```

### selection.getViewingTrack

Public API method. Runtime authority: `src/api/SelectionApi.cpp:405`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `includeTrackInfo` | `boolean` | No | Optional; default false. |

**Returns**: `{"found":true,"handle":"...","itemIndex":"...","mode":"...","playlistIndex":"...","source":"...","success":true,"track":{}}`

```js
const result = await fb2k.invoke('selection.getViewingTrack', { includeTrackInfo: /* value */ });
```

### selection.set


<!-- phase3-major1-review:selection.set -->
#### Source-reviewed contract
Authority: `src/api/SelectionApi.cpp:283-361`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `handles` | `array<object or string>` | Yes | none |

**Return keys (vary by response variant)**: `error`, `success`; `error`, `success`; `error`, `success`; `count`, `success`; `error`, `success`; `error`, `success`

**Semantics**: handles is required and must be an array. Each item is resolved to a media handle by the selection service; conversion failures are reported by the false/error response variant.

<!-- phase3-major1-review-end:selection.set -->
Public API method. Runtime authority: `src/api/SelectionApi.cpp:408`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `handles` | `array<object or string>` | Yes | Required. |

| --- | --- | --- | --- |
| `handles` | `array<object\\\|string>` | Yes | | Required. |

**Returns**: `{"count":"...","error":"...","success":true}`

```js
const result = await fb2k.invoke('selection.set', { handles: /* value */ });
```

### selection.setPlaylistTracking

Public API method. Runtime authority: `src/api/SelectionApi.cpp:409`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `mode` | `string` | No | Optional; default selection. |

**Returns**: `{"error":"...","mode":"...","success":true}`

```js
const result = await fb2k.invoke('selection.setPlaylistTracking', { mode: /* value */ });
```

## Selection behavior

`selection.getViewerMode` returns either `prefer_playing` or `prefer_selection`. `selection.getViewingTrack` applies that preference, falling back to the other source when the preferred source has no track. `selection:changed` is broadcast by `src/selection/SelectionWatcher.cpp` after a selection update; its documented payload is maintained in the event reference.

Paths supplied to `queue.addPaths` or JIT Queue batch operations may use the `path|subsong:N` form when a specific subsong must be selected.

## JIT Queue events

`src/core/QueueManager.cpp` emits these events while it maintains the JIT shadow playlist. Subscribe before issuing operations when the frontend needs to refill or observe that buffer.

| Event | Meaning | Payload keys |
| --- | --- | --- |
| `jitQueue:needNext` | The manager needs the next logical track. | `{ currentTrackId, reason }` |
| `jitQueue:trackChanged` | The JIT current track changed. | `{ trackId, title }` |
| `jitQueue:listExhausted` | The frontend reported that no additional tracks are available. | `{ lastTrackId }` |
| `jitQueue:preloadComplete` | A batch preload completed. | `{ count, startIndex, replace }` |
| `jitQueue:error` | A JIT operation failed for a track. | `{ trackId, error, path }` |

## Contract supplements

The sections below close public-contract findings from the strict parameter audit without replacing existing explanations.

<!-- phase3-supplement:jitQueue.preloadBatch -->
### Contract supplement: `jitQueue.preloadBatch`

Verified contract supplement. Runtime authority: `src/api/QueueApi.cpp:600-645`.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `urls` | `array<string>` | No | `[]` | Optional; default []. |
| `startIndex` | `integer` | No | `0` | Optional; default 0. |
| `replace` | `boolean` | No | `true` | Optional; default true. |

#### Return fields

| Field | Type | Optional |
| --- | --- | --- |
| `error` | `string` | Yes |
| `success` | `boolean` | No |

Semantics: omitted optional parameters use handler defaults; failure branches and error fields are defined by this source file.

```js
const result = await fb2k.invoke('jitQueue.preloadBatch', { urls: /* value */, startIndex: /* value */, replace: /* value */ });
```
