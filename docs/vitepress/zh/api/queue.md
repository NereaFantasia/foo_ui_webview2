# Queue & Selection

## Selection API - 选择同步

### selection.getViewerMode

获取用户的 Selection Viewer 偏好设置。v1.1.16

**返回值**: `{ "mode": "prefer_playing" }` 或 `{ "mode": "prefer_selection" }`

### selection.getViewingTrack

获取当前应该显示的曲目，自动根据 Viewer 模式执行 Fallback。v1.1.16

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `includeTrackInfo` | `boolean` | 否 | 可选；默认 false。 |


**返回值**: `{"found":true,"handle":"...","itemIndex":"...","mode":"...","playlistIndex":"...","source":"...","success":true,"track":{}}`

**Fallback 逻辑**:

- `prefer_playing`: 优先返回正在播放 → 回退到当前选择
- `prefer_selection`: 优先返回当前选择 → 回退到正在播放
- 均无: 返回 `found: false`

```javascript
const r = await fb2k.invoke('selection.getViewingTrack', { includeTrackInfo: true });
if (r.found) {
    console.log(`显示: ${r.handle} (来源: ${r.source})`);
}
```

### selection.get

<!-- phase3-major1-review:selection.get -->
#### 源码复核 contract
权威源: `src/api/SelectionApi.cpp:192-250`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `offset` | `integer` | 否 | `0` |
| `limit` | `integer` | 否 | `100` |

**返回值**: `{"count":0,"handles":"...","hasMore":true,"offset":"...","truncated":"...","type":"..."}`

**语义**: Selection paging uses offset and a bounded limit in the selection service. The method observes global selection state and does not mutate the active playlist.

<!-- phase3-major1-review-end:selection.get -->

获取当前全局选择的曲目列表，支持分页。v1.1.16

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `offset` | `integer` | 否 | 可选；默认 0。 |
| `limit` | `integer` | 否 | 可选；默认 100。 |

::: warning 性能提示
选择超过 100 个曲目时，未指定 limit 会自动截断为 100 个。
:::

### selection.getType

<!-- phase3-major1-review:selection.getType -->
#### 源码复核 contract
权威源: `src/api/SelectionApi.cpp:255-278`.

_无公开参数。_

**返回字段（按变体取值）**: `type`, `typeName`

**语义**: No request fields are read. The returned numeric type and typeName describe the current foobar2000 selection source.

<!-- phase3-major1-review-end:selection.getType -->

获取当前选择类型。v1.1.16

| type | typeName | 说明 |
| --- | --- | --- |
| 0 | now_playing | 正在播放 |
| 1 | active_playlist_selection | 活动播放列表的选择 |
| 2 | active_playlist | 活动播放列表 |
| 3 | playlist_manager | 播放列表管理器 |
| 5 | media_library_viewer | 媒体库查看器 |

### selection.set

<!-- phase3-major1-review:selection.set -->
#### 源码复核 contract
权威源: `src/api/SelectionApi.cpp:283-361`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `handles` | `array<object or string>` | 是 | 无 |

**返回字段（按变体取值）**: `error`, `success`；`error`, `success`；`error`, `success`；`count`, `success`；`error`, `success`；`error`, `success`

**语义**: handles is required and must be an array. Each item is resolved to a media handle by the selection service; conversion failures are reported by the false/error response variant.

<!-- phase3-major1-review-end:selection.set -->

设置当前全局选择。v1.1.16

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `handles` | `array<object or string>` | 是 | 必填。 |

| --- | --- | --- | --- |
| `handles` | `array<object\\\|string>` | 是 | | 必填。 |

### selection.setPlaylistTracking

设置播放列表跟踪模式。v1.1.16

| mode 值 | 说明 |
| --- | --- |
| "selection" | 跟踪播放列表中用户选择的曲目 |
| "playlist" | 跟踪整个播放列表 |


**返回值**: `{"mode":"...","success":true}`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `mode` | `string` | 否 | 可选；默认 selection。 |

### queue.addPaths


<!-- phase3-major1-review:queue.addPaths -->
#### 源码复核 contract
权威源: `src/api/QueueApi.cpp:266-337`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `paths` | `array<string>` | 否 | `[]` |
| `useQueuePlaylist` | `boolean` | 否 | `true` |
| `playlist` | `integer` | 否 | `active playlist when useQueuePlaylist is false` |

**返回字段（按变体取值）**: `error`, `success`；`error`, `success`；`error`, `success`；`error`, `isLocked`, `playlist`, `success`；`error`, `invalidCount`, `success`；`addedCount`, `invalidCount`, `playlist`, `queueCount`, `success`

**语义**: An empty paths array fails. The registration protects paths as a non-empty MediaRead array; each path is parsed for an optional subsong suffix, rejects oversized streams, then resolves to queueable handles. The handler rejects locked target playlists.

<!-- phase3-major1-review-end:queue.addPaths -->
一步添加 URL/本地路径到播放队列。自动处理添加到播放列表和入队操作。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `array<string>` | 否 | 可选；默认 []。 |
| `useQueuePlaylist` | `boolean` | 否 | 可选；默认 true。 |
| `playlist` | `integer` | 否 | 可选；默认 active playlist when useQueuePlaylist is false。 |

```javascript
await fb2k.invoke('queue.addPaths', {
    paths: ['C:/Music/song.mp3', 'http://stream.example.com/audio.mp3']
});
```

## Queue API - 播放队列

### queue.add

<!-- phase3-major1-review:queue.add -->
#### 源码复核 contract
权威源: `src/api/QueueApi.cpp:221-261`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | `active playlist` |
| `tracks` | `array<integer>` | 否 | `[]` |
| `track` | `integer` | 否 | `not supplied` |

**返回字段（按变体取值）**: `error`, `success`；`addedCount`, `queueCount`, `success`

**语义**: playlist defaults to the active playlist. tracks takes precedence when it is an array; otherwise track is used. Invalid playlist or item indices produce a false result without queueing an item.

<!-- phase3-major1-review-end:queue.add -->

将播放列表中的曲目添加到队列。支持单个曲目或批量添加。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 可选；默认 active playlist。 |
| `tracks` | `array<integer>` | 否 | 可选；默认 []。 |
| `track` | `integer` | 否 | 可选；默认 not supplied。 |

| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 可选；默认 active playlist。 |
| `tracks` | `array<integer>` | 否 | 可选；默认 []。 |
| `track` | `integer` | 否 | 可选；默认 not supplied。 |

**返回值**: `{ "success": true, "addedCount": 3, "queueCount": 5 }`

```javascript
// 添加单个曲目
await fb2k.invoke('queue.add', { track: 5 });

// 批量添加
await fb2k.invoke('queue.add', { tracks: [0, 1, 2], playlist: 0 });
```

### queue.get

获取当前播放队列内容。

**返回值**:

```json
{
    "items": [
        {
            "queueIndex": 0,
            "path": "file://C:/Music/song.mp3",
            "absolutePath": "C:\\\\Music\\\\song.mp3",
            "subsong": 0,
            "fileSize": 10485760,
            "title": "Song Title",
            "artist": "Artist Name",
            "album": "Album Name",
            "albumArtist": "Album Artist",
            "genre": "Rock",
            "date": "2024",
            "trackNumber": 1,
            "discNumber": 1,
            "duration": 245.5,
            "bitrate": 1411,
            "sampleRate": 44100,
            "channels": 2,
            "codec": "FLAC",
            "playlist": 0,
            "playlistItem": 15
        }
    ],
    "count": 3
}
```

### queue.remove

<!-- phase3-major1-review:queue.remove -->
#### 源码复核 contract
权威源: `src/api/QueueApi.cpp:341-392`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `index` | `integer` | 否 | `not supplied` |
| `indices` | `array<integer>` | 否 | `[]` |

**返回字段（按变体取值）**: `error`, `success`；`error`, `success`；`queueCount`, `removedIndex`, `success`；`queueCount`, `removedCount`, `success`；`error`, `success`

**语义**: The handler prefers index when present; otherwise it removes distinct valid entries from indices. An empty queue, invalid single index, or neither field returns success:false with an error.

<!-- phase3-major1-review-end:queue.remove -->

从队列中移除指定项。支持单个索引或批量移除。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `index` | `integer` | 否 | 可选；默认 not supplied。 |
| `indices` | `array<integer>` | 否 | 可选；默认 []。 |

**单个移除返回**: `{ "success": true, "removedIndex": 0, "queueCount": 2 }`

**批量移除返回**: `{ "success": true, "removedCount": 2, "queueCount": 1 }`

```javascript
// 移除第一项
await fb2k.invoke('queue.remove', { index: 0 });

// 批量移除
await fb2k.invoke('queue.remove', { indices: [0, 2, 4] });
```

### queue.clear

清空整个播放队列。

**返回值**: `{ "success": true, "clearedCount": 3 }`

### queue.flush


**返回值**: `{"clearedCount":"...","success":true}`

`queue.clear` 的别名。清空整个播放队列。

### queue.getCount


**返回值**: `{"count":0,"hasItems":true}`

获取队列项数量。返回 `{ "count": 3, "hasItems": true }`

```javascript
const result = await fb2k.invoke('queue.getCount');
console.log(`队列中有 ${result.count} 项`);
```

### queue.moveToTop

<!-- phase3-major1-review:queue.moveToTop -->
#### 源码复核 contract
权威源: `src/api/QueueApi.cpp:422-459`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `index` | `integer` | 否 | `not supplied` |

**返回字段（按变体取值）**: `error`, `success`；`movedIndex`, `queueCount`, `success`

**语义**: index must identify a non-first item in a non-empty queue. The operation rebuilds the native queue with that item first, so queue order—not playlist membership—is changed.

<!-- phase3-major1-review-end:queue.moveToTop -->

将队列中的指定项移动到队首（下一首播放）。内部通过清空队列并重建实现。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `index` | `integer` | 否 | 可选；默认 not supplied。 |

**返回值**: `{ "success": true, "movedIndex": 3, "queueCount": 5 }`

```javascript
// 将队列第 3 项移到队首
await fb2k.invoke('queue.moveToTop', { index: 3 });
```

## JIT Queue API（流媒体即时队列）

JIT Queue（Just-In-Time Queue）是专为流媒体设计的双层队列架构。与原生 Queue API 不同，采用"前端负责逻辑，后端负责执行"的模式，解决了流媒体 URL 时效性问题。

### 架构说明

```text
┌─────────────────────────────────────────────────┐
│  Frontend (Web/Vue3)                            │
│  ┌────────────────────────────────────────────┐ │
│  │  逻辑队列 (Pinia Store)                     │ │
│  │  tracks: Track[] / playMode / currentIndex │ │
│  └────────────────────────────────────────────┘ │
│                    ↓ fb2k.invoke()              │
├─────────────────────────────────────────────────┤
│  C++ Backend                                    │
│  ┌────────────────────────────────────────────┐ │
│  │  QueueManager (影子播放列表)                 │ │
│  │  只维护 2-3 首歌的缓冲区，URL 即时解析      │ │
│  └────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────┘
```

**核心优势**: URL 永不过期、内存极低、逻辑灵活、无缝衔接。

### jitQueue.playNow

立即播放指定曲目。清空缓冲区并开始新的播放会话。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `title` | `string` | 否 | 可选；默认 。 |
| `trackId` | `string` | 否 | 可选；默认 。 |
| `url` | `string` | 否 | 可选；默认 。 |


**返回值**: `{"shadowPlaylist":"...","success":true,"trackId":"..."}`

**URL 类型自动检测**: `http://`/`https://` → 流媒体模式；Windows 绝对路径/UNC → 本地文件模式。

```javascript
const url = await fetchRealUrl(track.id);
await fb2k.invoke('jitQueue.playNow', {
    trackId: 'netease_12345',
    title: '让我留在你身边',
    url: url
});
```

### jitQueue.enqueueNext

预加载下一首曲目到缓冲区。响应 `jitQueue:needNext` 事件时调用。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `title` | `string` | 否 | 可选；默认 。 |
| `trackId` | `string` | 否 | 可选；默认 。 |
| `url` | `string` | 否 | 可选；默认 。 |


**返回值**: `{"bufferSize":"...","success":true,"trackId":"..."}`

### jitQueue.skip

跳到缓冲区中的下一首曲目。

- **参数**: 无
- **返回值**: `{ "success": true, "currentTrackId": "..." }`

### jitQueue.stop

停止播放并可选清空缓冲区。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `clearBuffer` | `boolean` | 否 | 可选；默认 true。 |


**返回值**: `{"success":true}`

### jitQueue.clear

清空影子播放列表缓冲区。

- **参数**: 无
- **返回值**: `{ "success": true }`

### jitQueue.getState

获取 JIT 队列状态。

**返回值**:

```json
{
    "isActive": true,
    "state": "Active",
    "currentTrackId": "netease_12345",
    "nextTrackId": "netease_67890",
    "bufferSize": 2,
    "shadowPlaylist": 3
}
```

`state` 可能的值：`"Idle"` / `"Active"` / `"WaitingNext"` / `"Exhausted"`

### jitQueue.notifyEmpty

显式通知后端前端已无更多曲目。

- **参数**: 无
- **返回值**: `{ "success": true }`

### jitQueue.preloadBatch

<!-- phase3-major1-review:jitQueue.preloadBatch -->
#### 源码复核 contract
权威源: `src/api/QueueApi.cpp:600-645`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `urls` | `array<string>` | 否 | `[]` |
| `startIndex` | `integer` | 否 | `0` |
| `replace` | `boolean` | 否 | `true` |

**返回值**: `{"error":"...","invalidCount":"...","success":true,"tracksAdded":"..."}`

**语义**: The handler accepts an omitted URLs array, delegates batch construction to QueueManager, and returns its success/error result. replace defaults to clearing the JIT buffer before preload; URL/path validity is evaluated by QueueManager.

<!-- phase3-major1-review-end:jitQueue.preloadBatch -->

批量预加载曲目到 shadow playlist。使用 `handle_create()` 纯内存创建句柄，零 I/O 开销。

- **参数**:`urls` (string[], 必填) — 曲目 URL 列表，支持 `path|subsong:N` 格式
- `startIndex` (number, 可选, 默认 0) — 起始播放位置
- `replace` (boolean, 可选, 默认 true) — 是否清空现有 buffer

**限制**: 单次最多 10000 条


```js
// 替换模式
await fb2k.invoke('jitQueue.preloadBatch', {
  urls: ['C:\\\\Music\\\\a.flac', 'C:\\\\Music\\\\b.flac'],
  startIndex: 0,
  replace: true
});

// 追加模式（不中断当前播放）
await fb2k.invoke('jitQueue.preloadBatch', {
  urls: moreUrls,
  replace: false
});
```

### JIT Queue 事件

| 事件 | 描述 | 数据 |
| --- | --- | --- |
| jitQueue:needNext | 后端请求下一首曲目 | { currentTrackId, reason } |
| jitQueue:trackChanged | 播放曲目变化 | { trackId, title } |
| jitQueue:listExhausted | 缓冲区耗尽 | { lastTrackId } |
| jitQueue:preloadComplete | 批量预加载完成 | { count, startIndex, replace } |
| jitQueue:error | JIT 操作中的曲目失败 | { trackId, error, path } |

## 选择行为

`selection.getViewerMode` 返回 `prefer_playing` 或 `prefer_selection`。`selection.getViewingTrack` 先按该偏好选择来源；若首选来源没有曲目，则回退到另一个来源。`selection:changed` 由 `src/selection/SelectionWatcher.cpp` 在选择更新后广播；其 payload 以事件参考页为准。

传给 `queue.addPaths` 或 JIT Queue 批量操作的路径可使用 `path|subsong:N` 形式来指定某个 subsong。

## JIT Queue 事件

`src/core/QueueManager.cpp` 在维护 JIT shadow playlist 时会发出以下事件。若前端需要补充或观察缓冲区，请在调用操作前订阅。

| 事件 | 含义 | Payload keys |
| --- | --- | --- |
| `jitQueue:needNext` | 管理器需要下一个逻辑曲目。 | `{ currentTrackId, reason }` |
| `jitQueue:trackChanged` | JIT 当前曲目发生变化。 | `{ trackId, title }` |
| `jitQueue:listExhausted` | 前端报告没有更多可用曲目。 | `{ lastTrackId }` |
| `jitQueue:preloadComplete` | 批量预加载完成。 | `{ count, startIndex, replace }` |
| `jitQueue:error` | 某首曲目的 JIT 操作失败。 | URL 分支为 `{ trackId, error, url }`，本地路径分支为 `{ trackId, error, path }`。 |

## 合同补充

以下章节补齐严格参数审计发现的公开 contract；不会改变前文的已有说明。

<!-- phase3-supplement:jitQueue.preloadBatch -->
### Contract 补充：`jitQueue.preloadBatch`

经复核的补充 contract。权威源：`src/api/QueueApi.cpp:600-645`。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `urls` | `array<string>` | 否 | `[]` | 可选；默认 []。 |
| `startIndex` | `integer` | 否 | `0` | 可选；默认 0。 |
| `replace` | `boolean` | 否 | `true` | 可选；默认 true。 |

#### 返回字段

| 字段 | 类型 | 可选 |
| --- | --- | --- |
| `error` | `string` | 是 |
| `success` | `boolean` | 否 |

语义：省略可选参数时使用 handler 默认值；失败分支及错误字段以该源文件为准。

```js
const result = await fb2k.invoke('jitQueue.preloadBatch', { urls: /* value */, startIndex: /* value */, replace: /* value */ });
```
