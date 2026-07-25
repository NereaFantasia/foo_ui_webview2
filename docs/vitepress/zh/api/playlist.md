# Playlist API

播放列表管理、曲目操作、智能播放列表和工具函数。共 47 个 API。

> **参数兼容性**: 所有 Playlist API 同时支持 `playlist` 和 `index` 参数名指定播放列表索引。

## 列表管理

### playlist.getCount

获取播放列表数量。

- **参数**: 无
- **返回值**: `{ "count": 5 }`

```javascript
const { count } = await fb2k.invoke('playlist.getCount');
```

### playlist.getAll

<!-- phase3-major1-review:playlist.getAll -->
#### 源码复核 contract
权威源: `src/api/PlaylistApi.cpp:750-772`.

_无公开参数。_

**返回字段（按变体取值）**: 无命名字段。

**语义**: No request fields are read. The method returns an array of playlist summaries containing index, name, trackCount, active/playing/locked, and autoplaylist state.

<!-- phase3-major1-review-end:playlist.getAll -->

获取所有播放列表信息。

- **参数**: 无

**返回值**:

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
`playlist.getAll` 不再返回 `duration` 字段（避免 N 个播放列表 × M 首曲目的全量加载开销）。如需获取单个播放列表的 duration，请使用 `playlist.getActive` 或 `playlist.getPlaying`。
:::

::: tip v1.1.18 新增
`isAutoplaylist` 字段已内联到 `playlist.getAll` 返回值中，无需再逐个调用 `playlist.isAutoplaylist`。
:::

### playlist.getActive

获取当前激活的播放列表。包含 `duration` 字段。

- **参数**: 无

**返回值**: `{"duration":"...","found":true,"index":0,"isActive":true,"isLocked":true,"isPlaying":true,"name":"...","success":true,"trackCount":"..."}`


> 无激活播放列表时返回 `{ "success": true, "found": false }`。

### playlist.setActive

<!-- phase3-major1-review:playlist.setActive -->
#### 源码复核 contract
权威源: `src/api/PlaylistApi.cpp:796-808`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | `not supplied` |

**返回字段（按变体取值）**: `error`, `success`；`success`

**语义**: setActive requires a valid playlist index in practice: its SIZE_MAX default is rejected and does not fall back to the current active playlist.

<!-- phase3-major1-review-end:playlist.setActive -->

设置激活的播放列表。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 可选；默认 not supplied。 |

**返回值**: `{ "success": true, "error": "..." }`

```javascript
await fb2k.invoke('playlist.setActive', { playlist: 1 });
```

### playlist.getPlaying

获取当前正在播放的播放列表。包含 `duration` 字段。

- **参数**: 无

**返回值**: `{"duration":"...","found":true,"index":0,"isActive":true,"isLocked":true,"isPlaying":true,"name":"...","success":true,"trackCount":"..."}`


> 无正在播放的播放列表时返回 `{ "success": true, "found": false }`。

### playlist.create

<!-- phase3-major1-review:playlist.create -->
#### 源码复核 contract
权威源: `src/api/PlaylistApi.cpp:832-845`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `name` | `string` | 否 | `New Playlist` |
| `position` | `integer` | 否 | `append` |

**返回字段（按变体取值）**: `index`, `success`

**语义**: create forwards the optional name and insertion position to the playlist service; the SIZE_MAX sentinel appends and the returned index is the created playlist.

<!-- phase3-major1-review-end:playlist.create -->

创建新的播放列表。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `name` | `string` | 否 | 可选；默认 New Playlist。 |
| `position` | `integer` | 否 | 可选；默认 append。 |

**返回值**: `{ "success": true, "index": 2 }`

```javascript
const result = await fb2k.invoke('playlist.create', { name: 'Rock Music' });
```

### playlist.remove

<!-- phase3-major1-review:playlist.remove -->
#### 源码复核 contract
权威源: `src/api/PlaylistApi.cpp:845-867`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | `active playlist` |

**返回字段（按变体取值）**: `error`, `success`；`success`

**语义**: The target defaults to active. The service remove result is returned; a pre-existing lock is surfaced through the structured locked error path.

<!-- phase3-major1-review-end:playlist.remove -->

删除播放列表。如果播放列表被锁定则无法删除。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 可选；默认 active playlist。 |

**返回值**: `{ "success": true, "error": "..." }`

### playlist.rename

<!-- phase3-major1-review:playlist.rename -->
#### 源码复核 contract
权威源: `src/api/PlaylistApi.cpp:867-880`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | `not supplied` |
| `name` | `string` | 否 | `` |

**返回字段（按变体取值）**: `error`, `success`；`success`

**语义**: Unlike most playlist operations, rename does not substitute the active playlist when omitted: the SIZE_MAX default is invalid. The response success mirrors the rename service result.

<!-- phase3-major1-review-end:playlist.rename -->

重命名播放列表。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 可选；默认 not supplied。 |
| `name` | `string` | 否 | 可选；默认 。 |

**返回值**: `{ "success": true }`

```javascript
await fb2k.invoke('playlist.rename', { playlist: 0, name: 'My Favorites' });
```

### playlist.clear

<!-- phase3-major1-review:playlist.clear -->
#### 源码复核 contract
权威源: `src/api/PlaylistApi.cpp:880-910`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | `active playlist` |

**返回字段（按变体取值）**: `error`, `success`；`clearedCount`, `playlist`, `remainingCount`, `success`

**语义**: The omitted playlist sentinel resolves to the active playlist. Valid unlocked targets receive an undo backup before clear; the result exposes pre-clear and remaining counts.

<!-- phase3-major1-review-end:playlist.clear -->

清空播放列表中的所有曲目。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 可选；默认 active playlist。 |

**返回值**:

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
#### 源码复核 contract
权威源: `src/api/PlaylistApi.cpp:1402-1436`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | `active playlist` |
| `name` | `string` | 否 | `source name + ' (Copy)'` |

**返回字段（按变体取值）**: `error`, `success`；`error`, `success`；`index`, `name`, `newPlaylist`, `sourcePlaylist`, `success`, `trackCount`

**语义**: The target defaults to the active playlist. An empty name is replaced with the source name plus (Copy); the result identifies sourcePlaylist and newPlaylist.

<!-- phase3-major1-review-end:playlist.duplicate -->

复制播放列表。新列表插入到源列表后方。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 可选；默认 active playlist。 |
| `name` | `string` | 否 | 可选；默认 source name + ' (Copy)'。 |

**返回值**: `{ "success": true, "index": 1, "sourcePlaylist": 0, "newPlaylist": 1, "name": "Default (Copy)", "trackCount": 150 }`

## 曲目操作

### playlist.getTrackCount

<!-- phase3-major1-review:playlist.getTrackCount -->
#### 源码复核 contract
权威源: `src/api/PlaylistApi.cpp:948-961`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | `active playlist` |
| `index` | `integer` | 否 | `used only if playlist is absent` |

**返回字段（按变体取值）**: `count`；`count`

**语义**: The shared selector accepts playlist or index and defaults to active. Invalid resolution deliberately returns count:0 rather than an error envelope.

<!-- phase3-major1-review-end:playlist.getTrackCount -->

获取播放列表中的曲目数量。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 可选；默认 active playlist。 |
| `index` | `integer` | 否 | 可选；默认 used only if playlist is absent。 |

**返回值**: `{ "count": 150 }`

### playlist.getTracks

<!-- phase3-major1-review:playlist.getTracks -->
#### 源码复核 contract
权威源: `src/api/PlaylistApi.cpp:961-981`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | `active playlist` |
| `index` | `integer` | 否 | `used only if playlist is absent` |
| `start` | `integer` | 否 | `0` |
| `count` | `integer` | 否 | `100` |
| `formats` | `object` | 否 | `{}` |

**返回字段（按变体取值）**: `count`, `playlist`, `start`, `total`, `tracks`

**语义**: The handler pages the resolved playlist and returns an empty tracks variant for an invalid target. formats accepts extra titleformat columns, while start/count bound the returned range.

<!-- phase3-major1-review-end:playlist.getTracks -->

获取播放列表中的曲目列表（分页）。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 可选；默认 active playlist。 |
| `index` | `integer` | 否 | 可选；默认 used only if playlist is absent。 |
| `start` | `integer` | 否 | 可选；默认 0。 |
| `count` | `integer` | 否 | 可选；默认 100。 |
| `formats` | `object` | 否 | 可选；默认 {}。 |

**返回值**:

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

::: tip 自定义动态列 (`formats` 参数)
`playlist.getTracks` 支持通过 `formats` 参数追加任意 TitleFormat 动态列：

```javascript
const result = await fb2k.invoke('playlist.getTracks', {
    start: 0, count: 50,
    formats: {
        myRating: '%rating%',
        codec: '%codec%'
    }
});
// 每个 track 对象会额外包含 myRating 和 codec 字段
```
:::

::: tip TIP
`absolutePath` 是本地文件系统路径，可直接用于 `artwork.getForTrack` 等 API。`path` 是 foobar2000 内部格式。
:::

### playlist.playTrack

<!-- phase3-major1-review:playlist.playTrack -->
#### 源码复核 contract
权威源: `src/api/PlaylistApi.cpp:1089-1134`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | `active playlist` |
| `index` | `integer` | 否 | `track or 0` |
| `track` | `integer` | 否 | `0` |
| `deferred` | `boolean` | 否 | `false` |
| `muted` | `boolean` | 否 | `false` |

**返回字段（按变体取值）**: `error`, `success`；`error`, `success`；`success`

**语义**: index has precedence over the legacy track alias. deferred schedules the default action on the main thread; muted only mutes before play and does not restore a prior mute state.

<!-- phase3-major1-review-end:playlist.playTrack -->

播放播放列表中的指定曲目。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 可选；默认 active playlist。 |
| `index` | `integer` | 否 | 可选；默认 track or 0。 |
| `track` | `integer` | 否 | 可选；默认 0。 |
| `deferred` | `boolean` | 否 | 可选；默认 false。 |
| `muted` | `boolean` | 否 | 可选；默认 false。 |

**返回值**: `{ "success": true }`

```javascript
await fb2k.invoke('playlist.playTrack', { playlist: 0, index: 5 });

// 延迟执行（流媒体场景推荐）
await fb2k.invoke('playlist.playTrack', { playlist: 0, index: 0, deferred: true });
```

### playlist.removeTracks

<!-- phase3-major1-review:playlist.removeTracks -->
#### 源码复核 contract
权威源: `src/api/PlaylistApi.cpp:1017-1041`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | `active playlist` |
| `index` | `integer` | 否 | `used only if playlist is absent` |
| `items` | `array<integer>` | 否 | `[]` |

**返回字段（按变体取值）**: `error`, `success`；`success`

**语义**: The shared selector resolves playlist/index. items identifies track indices to remove; locked and invalid targets are rejected before mutation.

<!-- phase3-major1-review-end:playlist.removeTracks -->

从播放列表中删除指定曲目。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 可选；默认 active playlist。 |
| `index` | `integer` | 否 | 可选；默认 used only if playlist is absent。 |
| `items` | `array<integer>` | 否 | 可选；默认 []。 |

**返回值**: `{ "success": true, "error": "..." }`

### playlist.removeSelectedTracks

<!-- phase3-major1-review:playlist.removeSelectedTracks -->
#### 源码复核 contract
权威源: `src/api/PlaylistApi.cpp:1041-1059`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | `active playlist` |
| `index` | `integer` | 否 | `used only if playlist is absent` |

**返回字段（按变体取值）**: `error`, `success`；`success`

**语义**: The shared selector resolves the target and removes its existing selection. Locked and invalid playlists return false/error; no explicit items argument is consumed.

<!-- phase3-major1-review-end:playlist.removeSelectedTracks -->

删除播放列表中当前选中的曲目。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 可选；默认 active playlist。 |
| `index` | `integer` | 否 | 可选；默认 used only if playlist is absent。 |

**返回值**: `{ "success": true }`

### playlist.moveTracks

<!-- phase3-major1-review:playlist.moveTracks -->
#### 源码复核 contract
权威源: `src/api/PlaylistApi.cpp:1059-1089`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | `active playlist` |
| `index` | `integer` | 否 | `used only if playlist is absent` |
| `items` | `array<integer>` | 否 | `[]` |
| `delta` | `integer` | 否 | `0` |

**返回字段（按变体取值）**: `error`, `success`；`success`

**语义**: The shared selector chooses playlist over index. Non-empty items replace the current selection before moving it by delta; empty items move the existing selection, with locked targets rejected.

<!-- phase3-major1-review-end:playlist.moveTracks -->

移动曲目（向上或向下）。当 `items` 非空时会先设置选区再移动；当 `items` 为空时直接移动当前选区（SMP 兼容语义）。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 可选；默认 active playlist。 |
| `index` | `integer` | 否 | 可选；默认 used only if playlist is absent。 |
| `items` | `array<integer>` | 否 | 可选；默认 []。 |
| `delta` | `integer` | 否 | 可选；默认 0。 |

**返回值**: `{ "success": true, "error": "..." }`

```javascript
await fb2k.invoke('playlist.moveTracks', { items: [0, 1, 2], delta: 3 });
await fb2k.invoke('playlist.moveTracks', { items: [5, 6], delta: -2 });
```

### playlist.addPaths

<!-- phase3-major1-review:playlist.addPaths -->
#### 源码复核 contract
权威源: `src/api/PlaylistApi.cpp:1439-1474`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | `active playlist` |
| `paths` | `array<string>` | 否 | `[]` |

**返回字段（按变体取值）**: `error`, `success`；`error`, `success`；`countBefore`, `error`, `invalidCount`, `playlist`, `requestedPaths`, `success`；`addedCount`, `countBefore`, `invalidCount`, `playlist`, `requestedPaths`, `success`, `totalCount`

**语义**: The non-empty MediaRead paths array is resolved by playlist_incoming_item_filter with subsong handling. The handler rejects invalid or locked targets and reports requestedPaths, addedCount, invalidCount, and count totals.

<!-- phase3-major1-review-end:playlist.addPaths -->

添加文件/文件夹到播放列表。使用 `playlist_incoming_item_filter` 同步解析，自动展开 CUE 文件。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 可选；默认 active playlist。 |
| `paths` | `array<string>` | 否 | 可选；默认 []。 |

**返回值**:

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

## 补充的公开 API

以下章节按 `RegisterApi` 动态补齐，参数键来自 C++ handler 静态提取。

### playlist.addHandles


<!-- phase3-major1-review:playlist.addHandles -->
#### 源码复核 contract
权威源: `src/api/PlaylistApi.cpp:1478-1513`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | `active playlist` |
| `handles` | `array<object or string>` | 否 | `[]` |

**返回字段（按变体取值）**: `error`, `success`；`error`, `success`；`error`, `invalidCount`, `playlist`, `requestedCount`, `success`；`addedCount`, `countBefore`, `invalidCount`, `playlist`, `requestedCount`, `success`, `totalCount`

**语义**: The target defaults to the active playlist. Handles accept { path, subsong } objects or strings; malformed, empty, oversized, or unresolvable entries increase invalidCount, and a locked target is rejected.

<!-- phase3-major1-review-end:playlist.addHandles -->
公开 API。运行时权威：`src/api/PlaylistApi.cpp:1887`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 可选；默认 active playlist。 |
| `handles` | `array<object or string>` | 否 | 可选；默认 []。 |

| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 可选；默认 active playlist。 |
| `handles` | `array<object\\\ | string>` | 否 可选；默认 []。 |

**返回值**: `{"addedCount":"...","countBefore":"...","error":"...","invalidCount":"...","playlist":"...","requestedCount":"...","success":true,"totalCount":"..."}`

```js
const result = await fb2k.invoke('playlist.addHandles', { handles: /* value */, playlist: /* value */ });
```


### playlist.addPathsAsync


<!-- phase3-major1-review:playlist.addPathsAsync -->
#### 源码复核 contract
权威源: `src/api/PlaylistApi.cpp:1678-1722`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | `active playlist` |
| `paths` | `array<string>` | 否 | `[]` |

**返回字段（按变体取值）**: `error`, `success`；`error`, `success`；`error`, `invalidCount`, `success`；`invalidCount`, `operationId`, `status`, `success`, `totalCount`

**语义**: A non-empty protected paths array starts an asynchronous add and returns operationId plus pending status. Completion is later broadcast as playlist:addComplete; immediate validation failure returns success:false and invalidCount.

<!-- phase3-major1-review-end:playlist.addPathsAsync -->
公开 API。运行时权威：`src/api/PlaylistApi.cpp:1898`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 可选；默认 active playlist。 |
| `paths` | `array<string>` | 否 | 可选；默认 []。 |

**返回值**: `{"error":"...","invalidCount":"...","operationId":"...","status":"...","success":true,"totalCount":"..."}`

```js
const result = await fb2k.invoke('playlist.addPathsAsync', { paths: /* value */, playlist: /* value */ });
```


### playlist.addPathsSequential


<!-- phase3-major1-review:playlist.addPathsSequential -->
#### 源码复核 contract
权威源: `src/api/PlaylistApi.cpp:1650-1675`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | `active playlist` |
| `paths` | `array<string>` | 否 | `[]` |

**返回字段（按变体取值）**: `error`, `success`；`error`, `success`；`addedCount`, `order`, `playlist`, `success`

**语义**: A non-empty protected paths array is resolved and inserted in the service’s sequential result order. Locked or invalid target playlists fail before the add; the return order lists inserted indices.

<!-- phase3-major1-review-end:playlist.addPathsSequential -->
公开 API。运行时权威：`src/api/PlaylistApi.cpp:1897`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 可选；默认 active playlist。 |
| `paths` | `array<string>` | 否 | 可选；默认 []。 |

**返回值**: `{"addedCount":"...","error":"...","order":"...","playlist":"...","success":true}`

```js
const result = await fb2k.invoke('playlist.addPathsSequential', { paths: /* value */, playlist: /* value */ });
```


### playlist.convertToAutoplaylist


<!-- phase3-major1-review:playlist.convertToAutoplaylist -->
#### 源码复核 contract
权威源: `src/api/PlaylistApi.cpp:1294-1315`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | `active playlist` |
| `query` | `string` | 否 | `` |
| `sort` | `string` | 否 | `` |
| `keepSorted` | `boolean` | 否 | `false` |

**返回字段（按变体取值）**: `error`, `success`；`error`, `success`；`playlist`, `success`；`error`, `success`

**语义**: A non-empty query is required despite its empty default. The target defaults to the active playlist; keepSorted controls the autoplaylist sort flag and service errors return success:false.

<!-- phase3-major1-review-end:playlist.convertToAutoplaylist -->
公开 API。运行时权威：`src/api/PlaylistApi.cpp:1881`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 可选；默认 active playlist。 |
| `query` | `string` | 否 | 可选；默认 。 |
| `sort` | `string` | 否 | 可选；默认 。 |
| `keepSorted` | `boolean` | 否 | 可选；默认 false。 |

**返回值**: `{"error":"...","playlist":"...","success":true}`

```js
const result = await fb2k.invoke('playlist.convertToAutoplaylist', { keepSorted: /* value */, playlist: /* value */, query: /* value */, sort: /* value */ });
```


### playlist.createAutoplaylist

公开 API。运行时权威：`src/api/PlaylistApi.cpp:1880`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `keepSorted` | `boolean` | 否 | 可选；默认 false。 |
| `name` | `string` | 否 | 可选；默认 New Autoplaylist。 |
| `query` | `string` | 否 | 可选；默认 。 |
| `sort` | `string` | 否 | 可选；默认 。 |

**返回值**: `{"error":"...","index":"...","name":"...","playlist":"...","query":"...","success":true}`

```js
const result = await fb2k.invoke('playlist.createAutoplaylist', { keepSorted: /* value */, name: /* value */, query: /* value */, sort: /* value */ });
```


### playlist.deselectAll


<!-- phase3-major1-review:playlist.deselectAll -->
#### 源码复核 contract
权威源: `src/api/PlaylistApi.cpp:1562-1572`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | `active playlist` |

**返回字段（按变体取值）**: `success`；`success`

**语义**: The optional playlist resolves to the active playlist. Invalid targets return success:false; otherwise all selection bits in that playlist are cleared.

<!-- phase3-major1-review-end:playlist.deselectAll -->
公开 API。运行时权威：`src/api/PlaylistApi.cpp:1892`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 可选；默认 active playlist。 |

**返回值**: `{"success":true}`

```js
const result = await fb2k.invoke('playlist.deselectAll', { playlist: /* value */ });
```


### playlist.focusTrack


<!-- phase3-major1-review:playlist.focusTrack -->
#### 源码复核 contract
权威源: `src/api/PlaylistApi.cpp:1137-1151`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | `active playlist` |
| `index` | `integer` | 否 | `no focused item` |
| `track` | `integer` | 否 | `no focused item` |

**返回字段（按变体取值）**: `error`, `success`；`error`, `success`；`success`

**语义**: Deprecated compatibility method: index wins over track. Omitted track clears focus using the infinite-size sentinel; invalid target or supplied track index returns success:false.

<!-- phase3-major1-review-end:playlist.focusTrack -->
公开 API。运行时权威：`src/api/PlaylistApi.cpp:1873`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 可选；默认 active playlist。 |
| `index` | `integer` | 否 | 可选；默认 no focused item。 |
| `track` | `integer` | 否 | 可选；默认 no focused item。 |

**返回值**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('playlist.focusTrack', { index: /* value */, playlist: /* value */, track: /* value */ });
```


### playlist.getAutoplaylistInfo


<!-- phase3-major1-review:playlist.getAutoplaylistInfo -->
#### 源码复核 contract
权威源: `src/api/PlaylistApi.cpp:1344-1369`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | `active playlist` |

**返回值**: `{"error":"...","isAutoplaylist":true,"keepSorted":"...","lockName":"...","playlist":0,"source":"...","success":true}`

**语义**: The target defaults to active. The response distinguishes non-autoplaylists from SDK and DUI autoplaylists and reports keepSorted/source/lockName when available.

<!-- phase3-major1-review-end:playlist.getAutoplaylistInfo -->
公开 API。运行时权威：`src/api/PlaylistApi.cpp:1883`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 可选；默认 active playlist。 |


```js
const result = await fb2k.invoke('playlist.getAutoplaylistInfo', { playlist: /* value */ });
```


### playlist.getAutoplaylistQuery


<!-- phase3-major1-review:playlist.getAutoplaylistQuery -->
#### 源码复核 contract
权威源: `src/api/PlaylistApi.cpp:1372-1399`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | `active playlist` |

**返回值**: `{"error":"...","isAutoplaylist":true,"keepSorted":"...","lockName":"...","note":"...","playlist":0,"query":"...","source":"...","success":true}`

**语义**: The target defaults to active. foobar2000 does not expose the query string, so an autoplaylist response deliberately carries query:null plus a note and source metadata.

<!-- phase3-major1-review-end:playlist.getAutoplaylistQuery -->
公开 API。运行时权威：`src/api/PlaylistApi.cpp:1884`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 可选；默认 active playlist。 |


```js
const result = await fb2k.invoke('playlist.getAutoplaylistQuery', { playlist: /* value */ });
```


### playlist.getAvailableColumns


<!-- phase3-major1-review:playlist.getAvailableColumns -->
#### 源码复核 contract
权威源: `src/api/PlaylistApi.cpp:1816-1848`.

_无公开参数。_

**返回字段（按变体取值）**: 无命名字段。

**语义**: No request fields are read. The return is an array assembled from DUI column providers; each item carries id, name, pattern, alignment, numeric, and optional sortPattern.

<!-- phase3-major1-review-end:playlist.getAvailableColumns -->
公开 API。运行时权威：`src/api/PlaylistApi.cpp:1901`。

_无参数。_

**返回值**: 见运行时 handler 返回的 JSON 对象。

```js
const result = await fb2k.invoke('playlist.getAvailableColumns');
```


### playlist.getFocusTrack


<!-- phase3-major1-review:playlist.getFocusTrack -->
#### 源码复核 contract
权威源: `src/api/PlaylistApi.cpp:1154-1163`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | `active playlist` |

**返回字段（按变体取值）**: `error`, `success`；`index`, `playlist`, `success`

**语义**: Deprecated compatibility getter. It resolves the active playlist when omitted and returns success:false for an invalid target; no focus is represented by index:-1.

<!-- phase3-major1-review-end:playlist.getFocusTrack -->
公开 API。运行时权威：`src/api/PlaylistApi.cpp:1874`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 可选；默认 active playlist。 |

**返回值**: `{"error":"...","index":"...","playlist":"...","success":true}`

```js
const result = await fb2k.invoke('playlist.getFocusTrack', { playlist: /* value */ });
```


### playlist.getFocusedTrack


<!-- phase3-major1-review:playlist.getFocusedTrack -->
#### 源码复核 contract
权威源: `src/api/PlaylistApi.cpp:1572-1582`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | `active playlist` |

**返回字段（按变体取值）**: `index`, `success`；`index`, `playlist`, `success`

**语义**: The target defaults to active. Valid calls return the playlist and focused index, using -1 where no item has focus; an invalid target returns success:true with index:-1 for compatibility.

<!-- phase3-major1-review-end:playlist.getFocusedTrack -->
公开 API。运行时权威：`src/api/PlaylistApi.cpp:1893`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 可选；默认 active playlist。 |

**返回值**: `{"index":"...","playlist":"...","success":true}`

```js
const result = await fb2k.invoke('playlist.getFocusedTrack', { playlist: /* value */ });
```


### playlist.getLockInfo


<!-- phase3-major1-review:playlist.getLockInfo -->
#### 源码复核 contract
权威源: `src/api/PlaylistApi.cpp:1516-1525`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | `active playlist` |

**返回字段（按变体取值）**: `error`, `success`；`isLocked`, `playlist`

**语义**: The target defaults to active and returns playlist plus isLocked. Invalid targets use the false/error response variant.

<!-- phase3-major1-review-end:playlist.getLockInfo -->
公开 API。运行时权威：`src/api/PlaylistApi.cpp:1888`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 可选；默认 active playlist。 |

**返回值**: `{"error":"...","isLocked":"...","playlist":"...","success":true}`

```js
const result = await fb2k.invoke('playlist.getLockInfo', { playlist: /* value */ });
```


### playlist.getSelectedTracks


<!-- phase3-major1-review:playlist.getSelectedTracks -->
#### 源码复核 contract
权威源: `src/api/PlaylistApi.cpp:981-995`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | `active playlist` |
| `index` | `integer` | 否 | `ignored when playlist is supplied` |

**返回字段（按变体取值）**: `error`, `success`, `tracks`

**语义**: Both selector names are supported by the shared helper, with playlist taking precedence. The return tracks array contains currently selected track records or a false/error variant for invalid targets.

<!-- phase3-major1-review-end:playlist.getSelectedTracks -->
公开 API。运行时权威：`src/api/PlaylistApi.cpp:1867`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 可选；默认 active playlist。 |
| `index` | `integer` | 否 | 可选；默认 ignored when playlist is supplied。 |

**返回值**: `{"error":"...","success":true,"tracks":"..."}`

```js
const result = await fb2k.invoke('playlist.getSelectedTracks');
```


### playlist.getSelection


<!-- phase3-major1-review:playlist.getSelection -->
#### 源码复核 contract
权威源: `src/api/PlaylistApi.cpp:1538-1552`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | `active playlist` |

**返回字段（按变体取值）**: `error`, `success`；`count`, `items`, `playlist`, `success`

**语义**: The target defaults to active. The response contains the selected item indices, count, and resolved playlist; invalid targets return false/error.

<!-- phase3-major1-review-end:playlist.getSelection -->
公开 API。运行时权威：`src/api/PlaylistApi.cpp:1890`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 可选；默认 active playlist。 |

**返回值**: `{"count":"...","error":"...","items":"...","playlist":"...","success":true}`

```js
const result = await fb2k.invoke('playlist.getSelection', { playlist: /* value */ });
```


### playlist.insertTracks


<!-- phase3-major1-review:playlist.insertTracks -->
#### 源码复核 contract
权威源: `src/api/PlaylistApi.cpp:913-948`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | `active playlist` |
| `position` | `integer` | 否 | `index or 0` |
| `index` | `integer` | 否 | `0` |
| `handles` | `array<object or string>` | 否 | `[]` |

**返回字段（按变体取值）**: `error`, `success`；`error`, `success`；`error`, `invalidCount`, `playlist`, `requestedCount`, `success`；`addedCount`, `countBefore`, `insertIndex`, `invalidCount`, `playlist`, `requestedCount`, `success`, `totalCount`

**语义**: position wins over index as the insertion location. A non-empty handles array is required in practice; items are validated by the service and locked targets fail before mutation.

<!-- phase3-major1-review-end:playlist.insertTracks -->
公开 API。运行时权威：`src/api/PlaylistApi.cpp:1864`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 可选；默认 active playlist。 |
| `position` | `integer` | 否 | 可选；默认 index or 0。 |
| `index` | `integer` | 否 | 可选；默认 0。 |
| `handles` | `array<object or string>` | 否 | 可选；默认 []。 |

| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 可选；默认 active playlist。 |
| `position` | `integer` | 否 | 可选；默认 index or 0。 |
| `index` | `integer` | 否 | 可选；默认 0。 |
| `handles` | `array<object\\\ | string>` | 否 可选；默认 []。 |

**返回值**: `{"addedCount":"...","countBefore":"...","error":"...","insertIndex":"...","invalidCount":"...","playlist":"...","requestedCount":"...","success":true,"totalCount":"..."}`

```js
const result = await fb2k.invoke('playlist.insertTracks', { handles: /* value */, index: /* value */, playlist: /* value */, position: /* value */ });
```


### playlist.isAutoplaylist


<!-- phase3-major1-review:playlist.isAutoplaylist -->
#### 源码复核 contract
权威源: `src/api/PlaylistApi.cpp:1250-1262`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | `active playlist` |

**返回值**: `{"error":"...","isAutoplaylist":true,"lockName":"...","playlist":0,"success":true}`

**语义**: The target defaults to active and reports playlist/isAutoplaylist with optional lockName. Invalid targets use success:false with an error.

<!-- phase3-major1-review-end:playlist.isAutoplaylist -->
公开 API。运行时权威：`src/api/PlaylistApi.cpp:1879`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 可选；默认 active playlist。 |


```js
const result = await fb2k.invoke('playlist.isAutoplaylist', { playlist: /* value */ });
```


### playlist.isLocked


<!-- phase3-major1-review:playlist.isLocked -->
#### 源码复核 contract
权威源: `src/api/PlaylistApi.cpp:1528-1535`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | `active playlist` |

**返回字段（按变体取值）**: `error`, `isLocked`, `success`；`isLocked`, `success`

**语义**: The target defaults to active. Valid responses include success and isLocked; an invalid target explicitly returns isLocked:false with an error.

<!-- phase3-major1-review-end:playlist.isLocked -->
公开 API。运行时权威：`src/api/PlaylistApi.cpp:1889`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 可选；默认 active playlist。 |

**返回值**: `{"error":"...","isLocked":"...","success":true}`

```js
const result = await fb2k.invoke('playlist.isLocked', { playlist: /* value */ });
```


### playlist.redo


<!-- phase3-major1-review:playlist.redo -->
#### 源码复核 contract
权威源: `src/api/PlaylistApi.cpp:1234-1245`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | `active playlist` |

**返回字段（按变体取值）**: `error`, `success`；`success`

**语义**: The target defaults to active. Invalid targets return success:false; otherwise success reflects whether the playlist redo stack had an action to restore.

<!-- phase3-major1-review-end:playlist.redo -->
公开 API。运行时权威：`src/api/PlaylistApi.cpp:1878`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 可选；默认 active playlist。 |

**返回值**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('playlist.redo', { playlist: /* value */ });
```


### playlist.removeAutoplaylist


<!-- phase3-major1-review:playlist.removeAutoplaylist -->
#### 源码复核 contract
权威源: `src/api/PlaylistApi.cpp:1318-1341`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | `active playlist` |

**返回字段（按变体取值）**: `error`, `success`；`playlist`, `source`, `success`；`note`, `playlist`, `source`, `success`；`error`, `success`

**语义**: SDK autoplaylists are converted back to normal playlists. DUI-detected autoplaylists return a successful informational dui/source/note variant rather than directly removing the lock.

<!-- phase3-major1-review-end:playlist.removeAutoplaylist -->
公开 API。运行时权威：`src/api/PlaylistApi.cpp:1882`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 可选；默认 active playlist。 |

**返回值**: `{"error":"...","note":"...","playlist":"...","source":"...","success":true}`

```js
const result = await fb2k.invoke('playlist.removeAutoplaylist', { playlist: /* value */ });
```


### playlist.reorder


<!-- phase3-major1-review:playlist.reorder -->
#### 源码复核 contract
权威源: `src/api/PlaylistApi.cpp:1618-1647`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | `active playlist` |
| `newOrder` | `array<integer>` | 否 | `[]` |

**返回字段（按变体取值）**: `error`, `success`；`error`, `expected`, `got`, `success`；`error`, `success`；`error`, `index`, `success`；`itemCount`, `playlist`, `success`

**语义**: newOrder must have exactly one numeric in-range source index per playlist item. The handler validates length and elements, records undo, then applies the order; it does not prove uniqueness.

<!-- phase3-major1-review-end:playlist.reorder -->
公开 API。运行时权威：`src/api/PlaylistApi.cpp:1896`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 可选；默认 active playlist。 |
| `newOrder` | `array<integer>` | 否 | 可选；默认 []。 |

**返回值**: `{"error":"...","expected":"...","got":"...","index":"...","itemCount":"...","playlist":"...","success":true}`

```js
const result = await fb2k.invoke('playlist.reorder', { newOrder: /* value */, playlist: /* value */ });
```


### playlist.reorderPlaylists


<!-- phase3-major1-review:playlist.reorderPlaylists -->
#### 源码复核 contract
权威源: `src/api/PlaylistApi.cpp:1782-1813`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `newOrder` | `array<integer>` | 否 | `[]` |

**返回字段（按变体取值）**: `error`, `expected`, `got`, `success`；`error`, `success`；`error`, `index`, `success`；`count`, `success`

**语义**: newOrder must match the current playlist count and contain in-range numeric indices. The service reorder result becomes success and the count is always returned on the normal result path.

<!-- phase3-major1-review-end:playlist.reorderPlaylists -->
公开 API。运行时权威：`src/api/PlaylistApi.cpp:1900`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `newOrder` | `array<integer>` | 否 | 可选；默认 []。 |

**返回值**: `{"count":"...","error":"...","expected":"...","got":"...","index":"...","success":true}`

```js
const result = await fb2k.invoke('playlist.reorderPlaylists', { newOrder: /* value */ });
```


### playlist.replaceAllAndPlay


<!-- phase3-major1-review:playlist.replaceAllAndPlay -->
#### 源码复核 contract
权威源: `src/api/PlaylistApi.cpp:1725-1779`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | `active playlist` |
| `paths` | `array<string>` | 否 | `[]` |
| `playIndex` | `integer` | 否 | `0` |
| `stopFirst` | `boolean` | 否 | `true` |
| `autoPlay` | `boolean` | 否 | `true` |

**返回字段（按变体取值）**: `error`, `success`；`error`, `success`；`clearedCount`, `error`, `invalidCount`, `success`；`addedCount`, `clearedCount`, `playIndex`, `playlist`, `success`, `totalCount`

**语义**: The non-empty MediaRead paths array replaces the resolved unlocked playlist atomically after optional stop. An out-of-range playIndex becomes zero; autoPlay false focuses the item instead of starting it.

<!-- phase3-major1-review-end:playlist.replaceAllAndPlay -->
公开 API。运行时权威：`src/api/PlaylistApi.cpp:1899`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 可选；默认 active playlist。 |
| `paths` | `array<string>` | 否 | 可选；默认 []。 |
| `playIndex` | `integer` | 否 | 可选；默认 0。 |
| `stopFirst` | `boolean` | 否 | 可选；默认 true。 |
| `autoPlay` | `boolean` | 否 | 可选；默认 true。 |

**返回值**: `{"addedCount":"...","clearedCount":"...","error":"...","invalidCount":"...","playIndex":"...","playlist":"...","success":true,"totalCount":"..."}`

```js
const result = await fb2k.invoke('playlist.replaceAllAndPlay', { autoPlay: /* value */, paths: /* value */, playIndex: /* value */, playlist: /* value */, stopFirst: /* value */ });
```


### playlist.reverse


<!-- phase3-major1-review:playlist.reverse -->
#### 源码复核 contract
权威源: `src/api/PlaylistApi.cpp:1597-1615`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | `active playlist` |

**返回字段（按变体取值）**: `success`；`success`；`success`

**语义**: The target defaults to active. Locked or invalid playlists fail; lists with fewer than two items succeed without mutation, otherwise the handler stores undo and reverses all positions.

<!-- phase3-major1-review-end:playlist.reverse -->
公开 API。运行时权威：`src/api/PlaylistApi.cpp:1895`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 可选；默认 active playlist。 |

**返回值**: `{"success":true}`

```js
const result = await fb2k.invoke('playlist.reverse', { playlist: /* value */ });
```


### playlist.selectAll


<!-- phase3-major1-review:playlist.selectAll -->
#### 源码复核 contract
权威源: `src/api/PlaylistApi.cpp:1552-1562`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | `active playlist` |

**返回字段（按变体取值）**: `success`；`success`

**语义**: The target defaults to active. Invalid targets return success:false; a valid target selects every item through the playlist service.

<!-- phase3-major1-review-end:playlist.selectAll -->
公开 API。运行时权威：`src/api/PlaylistApi.cpp:1891`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 可选；默认 active playlist。 |

**返回值**: `{"success":true}`

```js
const result = await fb2k.invoke('playlist.selectAll', { playlist: /* value */ });
```


### playlist.setFocusedTrack


<!-- phase3-major1-review:playlist.setFocusedTrack -->
#### 源码复核 contract
权威源: `src/api/PlaylistApi.cpp:1582-1597`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | `active playlist` |
| `index` | `integer` | 否 | `no focused item` |

**返回字段（按变体取值）**: `error`, `success`；`error`, `success`；`success`

**语义**: The target defaults to active. index may be omitted to set the no-focus sentinel; supplied indices must be within the playlist track count.

<!-- phase3-major1-review-end:playlist.setFocusedTrack -->
公开 API。运行时权威：`src/api/PlaylistApi.cpp:1894`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 可选；默认 active playlist。 |
| `index` | `integer` | 否 | 可选；默认 no focused item。 |

**返回值**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('playlist.setFocusedTrack', { index: /* value */, playlist: /* value */ });
```


### playlist.setSelection


<!-- phase3-major1-review:playlist.setSelection -->
#### 源码复核 contract
权威源: `src/api/PlaylistApi.cpp:995-1017`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | `active playlist` |
| `index` | `integer` | 否 | `used only if playlist is absent` |
| `indices` | `array<integer>` | 否 | `[]` |
| `clearOthers` | `boolean` | 否 | `true` |

**返回字段（按变体取值）**: `error`, `success`；`success`

**语义**: The shared selector resolves playlist/index. indices are converted to item indices and clearOthers selects replacement versus additive selection; invalid target selection fails.

<!-- phase3-major1-review-end:playlist.setSelection -->
公开 API。运行时权威：`src/api/PlaylistApi.cpp:1868`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 可选；默认 active playlist。 |
| `index` | `integer` | 否 | 可选；默认 used only if playlist is absent。 |
| `indices` | `array<integer>` | 否 | 可选；默认 []。 |
| `clearOthers` | `boolean` | 否 | 可选；默认 true。 |

**返回值**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('playlist.setSelection', { clearOthers: /* value */, indices: /* value */ });
```


### playlist.shuffle

公开 API。运行时权威：`src/api/PlaylistApi.cpp:1876`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 可选；默认 active playlist。 |
| `index` | `integer` | 否 | 可选；默认 used only if playlist is absent。 |

**返回值**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('playlist.shuffle', { playlist: /* value */, index: /* value */ });
```


### playlist.sort

公开 API。运行时权威：`src/api/PlaylistApi.cpp:1875`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 可选；默认 active playlist。 |
| `index` | `integer` | 否 | 可选；默认 used only if playlist is absent。 |
| `pattern` | `string` | 否 | 可选；默认 %title%。 |
| `descending` | `boolean` | 否 | 可选；默认 false。 |
| `selectedOnly` | `boolean` | 否 | 可选；默认 false。 |

**返回值**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('playlist.sort', { descending: /* value */, pattern: /* value */, selectedOnly: /* value */, playlist: /* value */, index: /* value */ });
```


### playlist.undo


<!-- phase3-major1-review:playlist.undo -->
#### 源码复核 contract
权威源: `src/api/PlaylistApi.cpp:1223-1234`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | `active playlist` |

**返回字段（按变体取值）**: `error`, `success`；`success`

**语义**: The target defaults to active. Invalid targets return success:false; otherwise success reflects whether the playlist undo stack restored an action.

<!-- phase3-major1-review-end:playlist.undo -->
公开 API。运行时权威：`src/api/PlaylistApi.cpp:1877`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 可选；默认 active playlist。 |

**返回值**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('playlist.undo', { playlist: /* value */ });
```

## 关联的播放列表事件

`src/callbacks/PlaylistCallback.cpp` 会广播以下公开生命周期事件。回调会有意忽略 JIT shadow playlist 的条目事件。

| 事件 | 触发时机 | Payload keys |
| --- | --- | --- |
| `playlist:itemsAdded` | 条目插入播放列表后。 | `{ playlist, start, count }` |
| `playlist:itemsRemoved` | 条目从播放列表移除后。 | `{ playlist, oldCount, newCount }` |
| `playlist:itemsReordered` | 单个播放列表中的条目重排后。 | `{ playlist, count }` |
| `playlist:selectionChanged` | 播放列表选择变化后。 | `{ playlist }` |
| `playlist:focusChanged` | 播放列表条目焦点变化后。 | `{ playlist, from, to }` |
| `playlist:itemsReplaced` | 播放列表条目替换后。 | `{ playlist, count }` |
| `playlist:created` | 新建播放列表后。 | `{ index, name }` |
| `playlist:removed` | 删除播放列表后。 | `{ oldCount, newCount }` |
| `playlist:reordered` | 播放列表集合重排后。 | `{ count }` |
| `playlist:activated` | 活动播放列表变化后。 | `{ oldIndex, newIndex }` |
| `playlist:renamed` | 播放列表重命名后。 | `{ index, name }` |
| `playlist:lockChanged` | 播放列表锁状态变化后。 | `{ playlist, locked }` |
| `playlist:defaultFormatChanged` | 默认播放列表格式变化后。 | `{}` |
| `playlist:addComplete` | 异步路径添加操作完成后。 | `{ operationId, success, addedCount, totalCount }` |

当 foobar2000 回调没有具体的前一个或后一个索引时，`from`、`to`、`oldIndex` 与 `newIndex` 可以为 `-1`。

## 合同补充

以下章节补齐严格参数审计发现的公开 contract；不会改变前文的已有说明。

<!-- phase3-supplement:playlist.moveTracks -->
### Contract 补充：`playlist.moveTracks`

经复核的补充 contract。权威源：`src/api/PlaylistApi.cpp:1059-1089`。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | 否 | `active playlist` | 可选；默认 active playlist。 |
| `index` | `integer` | 否 | `used only if playlist is absent` | 可选；默认 used only if playlist is absent。 |
| `items` | `array<integer>` | 否 | `[]` | 可选；默认 []。 |
| `delta` | `integer` | 否 | `0` | 可选；默认 0。 |

#### 返回字段

| 字段 | 类型 | 可选 |
| --- | --- | --- |
| `error` | `string` | 是 |
| `success` | `boolean` | 否 |

语义：省略可选参数时使用 handler 默认值；失败分支及错误字段以该源文件为准。

```js
const result = await fb2k.invoke('playlist.moveTracks', { playlist: /* value */, index: /* value */, items: /* value */, delta: /* value */ });
```
<!-- phase3-supplement:playlist.playTrack -->
### Contract 补充：`playlist.playTrack`

经复核的补充 contract。权威源：`src/api/PlaylistApi.cpp:1089-1134`。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | 否 | `active playlist` | 可选；默认 active playlist。 |
| `index` | `integer` | 否 | `track or 0` | 可选；默认 track or 0。 |
| `track` | `integer` | 否 | `0` | 可选；默认 0。 |
| `deferred` | `boolean` | 否 | `false` | 可选；默认 false。 |
| `muted` | `boolean` | 否 | `false` | 可选；默认 false。 |

#### 返回字段

| 字段 | 类型 | 可选 |
| --- | --- | --- |
| `error` | `string` | 是 |
| `success` | `boolean` | 否 |

语义：省略可选参数时使用 handler 默认值；失败分支及错误字段以该源文件为准。

```js
const result = await fb2k.invoke('playlist.playTrack', { playlist: /* value */, index: /* value */, track: /* value */, deferred: /* value */, muted: /* value */ });
```
<!-- phase3-supplement:playlist.removeSelectedTracks -->
### Contract 补充：`playlist.removeSelectedTracks`

经复核的补充 contract。权威源：`src/api/PlaylistApi.cpp:1041-1059`。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | 否 | `active playlist` | 可选；默认 active playlist。 |
| `index` | `integer` | 否 | `used only if playlist is absent` | 可选；默认 used only if playlist is absent。 |

#### 返回字段

| 字段 | 类型 | 可选 |
| --- | --- | --- |
| `error` | `string` | 是 |
| `success` | `boolean` | 否 |

语义：省略可选参数时使用 handler 默认值；失败分支及错误字段以该源文件为准。

```js
const result = await fb2k.invoke('playlist.removeSelectedTracks', { playlist: /* value */, index: /* value */ });
```
<!-- phase3-supplement:playlist.removeTracks -->
### Contract 补充：`playlist.removeTracks`

经复核的补充 contract。权威源：`src/api/PlaylistApi.cpp:1017-1041`。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | 否 | `active playlist` | 可选；默认 active playlist。 |
| `index` | `integer` | 否 | `used only if playlist is absent` | 可选；默认 used only if playlist is absent。 |
| `items` | `array<integer>` | 否 | `[]` | 可选；默认 []。 |

#### 返回字段

| 字段 | 类型 | 可选 |
| --- | --- | --- |
| `error` | `string` | 是 |
| `success` | `boolean` | 否 |

语义：省略可选参数时使用 handler 默认值；失败分支及错误字段以该源文件为准。

```js
const result = await fb2k.invoke('playlist.removeTracks', { playlist: /* value */, index: /* value */, items: /* value */ });
```
<!-- phase3-supplement:playlist.setSelection -->
### Contract 补充：`playlist.setSelection`

经复核的补充 contract。权威源：`src/api/PlaylistApi.cpp:995-1017`。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | 否 | `active playlist` | 可选；默认 active playlist。 |
| `index` | `integer` | 否 | `used only if playlist is absent` | 可选；默认 used only if playlist is absent。 |
| `indices` | `array<integer>` | 否 | `[]` | 可选；默认 []。 |
| `clearOthers` | `boolean` | 否 | `true` | 可选；默认 true。 |

#### 返回字段

| 字段 | 类型 | 可选 |
| --- | --- | --- |
| `error` | `string` | 是 |
| `success` | `boolean` | 否 |

语义：省略可选参数时使用 handler 默认值；失败分支及错误字段以该源文件为准。

```js
const result = await fb2k.invoke('playlist.setSelection', { playlist: /* value */, index: /* value */, indices: /* value */, clearOthers: /* value */ });
```
<!-- phase3-supplement:playlist.shuffle -->
### Contract 补充：`playlist.shuffle`

经复核的补充 contract。权威源：`src/api/PlaylistApi.cpp:1190-1223`。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | 否 | `active playlist` | 可选；默认 active playlist。 |
| `index` | `integer` | 否 | `used only if playlist is absent` | 可选；默认 used only if playlist is absent。 |

#### 返回字段

| 字段 | 类型 | 可选 |
| --- | --- | --- |
| `error` | `string` | 是 |
| `success` | `boolean` | 否 |

语义：省略可选参数时使用 handler 默认值；失败分支及错误字段以该源文件为准。

```js
const result = await fb2k.invoke('playlist.shuffle', { playlist: /* value */, index: /* value */ });
```
<!-- phase3-supplement:playlist.sort -->
### Contract 补充：`playlist.sort`

经复核的补充 contract。权威源：`src/api/PlaylistApi.cpp:1163-1190`。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | 否 | `active playlist` | 可选；默认 active playlist。 |
| `index` | `integer` | 否 | `used only if playlist is absent` | 可选；默认 used only if playlist is absent。 |
| `pattern` | `string` | 否 | `%title%` | 可选；默认 %title%。 |
| `descending` | `boolean` | 否 | `false` | 可选；默认 false。 |
| `selectedOnly` | `boolean` | 否 | `false` | 可选；默认 false。 |

#### 返回字段

| 字段 | 类型 | 可选 |
| --- | --- | --- |
| `error` | `string` | 是 |
| `success` | `boolean` | 否 |

语义：省略可选参数时使用 handler 默认值；失败分支及错误字段以该源文件为准。

```js
const result = await fb2k.invoke('playlist.sort', { playlist: /* value */, index: /* value */, pattern: /* value */, descending: /* value */, selectedOnly: /* value */ });
```
