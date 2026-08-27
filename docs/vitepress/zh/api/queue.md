# Queue & Selection

## Selection API - 选择同步

### selection.getViewerMode

（v1.1.16+）获取用户的 Selection Viewer 偏好设置。

**返回值**: `{ "mode": "prefer_playing" }` 或 `{ "mode": "prefer_selection" }`

### selection.getViewingTrack

（v1.1.16+）获取当前应该显示的曲目，自动根据 Viewer 模式执行 Fallback。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `includeTrackInfo` | `boolean` | 否 | `false` | 附带 `track` 曲目信息对象。 |


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


（v1.1.16+）获取当前全局选择的曲目列表，支持分页。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `offset` | `integer` | 否 | 起始下标，默认 `0`。 |
| `limit` | `integer` | 否 | 最多返回条数，默认 `100`；传 `0` 表示全部。 |

::: warning 性能提示
未指定 `limit` 时，选择超过 100 个曲目会自动截断为 100 条。显式传入的 `limit` **不会**被收窄，需要全部数据请传 `limit: 0`。
:::

**返回值**: `{ "count": 250, "type": "...", "handles": [...], "offset": 0, "hasMore": true }`；结果被自动截断时另有 `truncated: true`。

本方法**不返回** `success`，也没有失败分支。`limit: 0` 表示不限条数，从 `offset` 开始返回其后的全部条目。

`truncated: true` 仅在自动上限**真正生效**时出现，即选择数超过 100 **且**未传 `limit`。在 250 条选择中显式传 `limit: 10` 不属于截断，不会带该字段。它从不以 `false` 出现，因此应判断该字段是否存在而非判断其取值；是否需要继续分页请看 `hasMore`。`count` 是选择的总数，不是本次返回的条数。

```js
const { handles, count, hasMore } = await fb2k.invoke('selection.get', { limit: 0 });
```

### selection.getType


（v1.1.16+）获取当前选择类型。

| type | typeName | 说明 |
| --- | --- | --- |
| 0 | now_playing | 正在播放 |
| 1 | active_playlist_selection | 活动播放列表的选择 |
| 2 | active_playlist | 活动播放列表 |
| 3 | playlist_manager | 播放列表管理器 |
| 5 | media_library_viewer | 媒体库查看器 |

### selection.set


（v1.1.16+）设置当前全局选择。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `handles` | `array<string>` | 是 | 非空路径数组，可带 `\|subsong:N` 后缀。 |

**返回值**: `{ "success": true, "count": 2 }`

非字符串条目会被静默跳过；`|subsong:` 后缀格式错误时回退为 subsong `0`。`handles` 缺失、非数组、为空数组，或无一条可解析时，返回 `{ "success": false, "error": "..." }`。

### selection.setPlaylistTracking

（v1.1.16+）设置播放列表跟踪模式。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `mode` | `string` | 否 | `selection`（默认）或 `playlist`。 |

| mode 值 | 说明 |
| --- | --- |
| `selection` | 跟踪播放列表中用户选择的曲目 |
| `playlist` | 跟踪整个播放列表 |

**返回值**: `{ "success": true, "mode": "selection" }`

只有 `playlist` 会切换为整表跟踪；其余任何取值（包括拼写错误）都会**静默回退**为 selection 跟踪，且 `mode` 会把你传入的原值回显出来——因此不能用返回的 `mode` 来确认取值是否合法。

### queue.addPaths


一步添加 URL/本地路径到播放队列。自动处理添加到播放列表和入队操作。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `paths` | `array<string>` | 是 | — | 文件路径或 URL，可带 `\|subsong:N` 后缀；空数组返回 `No paths specified`。 |
| `useQueuePlaylist` | `boolean` | 否 | `true` | 使用专用 `[WebView Queue]` 播放列表，不存在则创建。 |
| `playlist` | `integer` | 否 | — | 目标播放列表索引；仅 `useQueuePlaylist: false` 时读取。 |

```javascript
await fb2k.invoke('queue.addPaths', {
    paths: ['C:/Music/song.mp3', 'http://stream.example.com/audio.mp3']
});
```

## Queue API - 播放队列

### queue.add


将播放列表中的曲目添加到队列。支持单个曲目或批量添加。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 活动播放列表 |  |
| `tracks` | `array<integer>` | 否 | — | 曲目下标数组；优先于 `track`。 |
| `track` | `integer` | 否 | — | 单个曲目下标；仅在未传 `tracks` 时生效。 |

**返回值**: `{ "success": true, "addedCount": 3, "queueCount": 5 }`

`tracks` 与 `track` 二选一——`tracks` 为数组时优先，`track` 被忽略。`success` 即 `addedCount > 0`。越界下标会被静默跳过，因此一条都没匹配上时返回 `success: false`、`addedCount: 0` 且**不带** `error`；空 `tracks` 数组同理。`playlist` 无效时返回 `{ "success": false, "error": "Invalid playlist index" }`。`tracks` 的元素必须是数字——非数字元素属于参数类型错误，不会被当作可跳过的条目。

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
            "absolutePath": "C:\\Music\\song.mp3",
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

> `artist` / `albumArtist` / `genre` / `composer`（仅指该 API 实际返回的字段）的多值标签按 `, ` 原序拼接，不去重。

### queue.remove


从队列中移除指定项。支持单个索引或批量移除。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `index` | `integer` | 否 | 单个队列下标；优先于 `indices`。 |
| `indices` | `array<integer>` | 否 | 批量队列下标；仅在未传 `index` 时生效。 |

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

获取队列项数量。返回 `{ "count": 3, "hasItems": true }`。本方法不返回 `success`；`hasItems` 就是 `count > 0`，仅为方便判断而提供。

```javascript
const result = await fb2k.invoke('queue.getCount');
console.log(`队列中有 ${result.count} 项`);
```

### queue.moveToTop


将队列中的指定项移动到队首（下一首播放）。内部通过清空队列并重建实现。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `index` | `integer` | 是 | 要移到队首的队列下标；不能已是 `0`。 |

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

后端只驻留 2-3 首曲目的缓冲并在播放前才解析 URL，完整的队列逻辑留在前端。

### jitQueue.playNow

立即播放指定曲目。清空缓冲区并开始新的播放会话。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `trackId` | `string` | 是 | 调用方自定义的曲目标识；为空返回 `trackId is required`。 |
| `url` | `string` | 是 | 流媒体或文件 URL；为空返回 `url is required`。 |
| `title` | `string` | 否 | 显示标题。 |


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
| `trackId` | `string` | 是 | 调用方自定义的曲目标识；为空返回 `trackId is required`。 |
| `url` | `string` | 是 | 流媒体或文件 URL；为空返回 `url is required`。 |
| `title` | `string` | 否 | 显示标题。 |


**返回值**: `{"bufferSize":"...","success":true,"trackId":"..."}`

### jitQueue.skip

跳到缓冲区中的下一首曲目。

- **参数**: 无
- **返回值**: `{ "success": true, "currentTrackId": "..." }`

### jitQueue.stop

停止播放并可选清空缓冲区。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `clearBuffer` | `boolean` | 否 | `true` | 停止时同时清空缓冲区。 |


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


批量预加载曲目到 shadow playlist。使用 `handle_create()` 纯内存创建句柄，零 I/O 开销。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `urls` | `array<string>` | 是 | 曲目 URL 或 `path\|subsong:N`，单次最多 10000 条。 |
| `startIndex` | `integer` | 否 | 起始播放位置，默认 `0`。 |
| `replace` | `boolean` | 否 | 默认 `true`，会**先清空 shadow playlist**；传 `false` 表示追加。 |

**返回值**: `{ "success": true, "tracksAdded": 2 }`；存在被拒条目时另有 `invalidCount`。

`replace` 默认为 `true`，插入前会清空 shadow playlist；如需在不打扰已入队内容的前提下追加，请显式传 `false`。非字符串条目或长度超过 2048 的条目会被丢弃并计入 `invalidCount`，不会导致整次调用失败。

10000 上限是在丢弃上述无效条目**之后**才判断的，因此只统计有效条目。超限会**整批失败**而非截断，且该分支返回 `{ "success": false, "error": "Batch exceeds maximum size (10000)" }`，**不含** `tracksAdded`。其他失败（如 `urls` 为空数组或 `startIndex` 越界）返回 `{ "success": false, "tracksAdded": 0, "error": "..." }`。

```js
// 替换模式
await fb2k.invoke('jitQueue.preloadBatch', {
  urls: ['C:\\Music\\a.flac', 'C:\\Music\\b.flac'],
  startIndex: 0,
  replace: true
});

// 追加模式（不中断当前播放）
await fb2k.invoke('jitQueue.preloadBatch', {
  urls: moreUrls,
  replace: false
});
```

## 选择行为

`selection.getViewerMode` 返回 `prefer_playing` 或 `prefer_selection`，该值由当前的选择类型推导，而非一项独立设置。`selection.getViewingTrack` 先按该偏好选择来源；若首选来源没有曲目，则回退到另一个来源——它**恒返回** `success: true`，请改判 `found`。`selection:changed` 在选择更新后广播到每个 WebView，并有 50 ms 节流；其 payload 见事件参考页。

队列与选择使用同一种 handle 字符串形式：原生路径，仅当 subsong 大于 `0` 时才附加 `|subsong:N`。传给 `queue.addPaths` 或 JIT Queue 操作的路径接受同样的后缀。单条路径或 URL 上限为 2048 字符。

## JIT Queue 事件

维护 JIT shadow playlist 期间会发出以下事件。若前端需要补充或观察缓冲区，请在调用操作前订阅。

| 事件 | 含义 | Payload keys |
| --- | --- | --- |
| `jitQueue:needNext` | 管理器需要下一个逻辑曲目。 | `{ currentTrackId, reason }` |
| `jitQueue:trackChanged` | JIT 当前曲目发生变化。 | `{ trackId, title }` |
| `jitQueue:listExhausted` | 前端报告没有更多可用曲目。 | `{ lastTrackId }` |
| `jitQueue:preloadComplete` | 批量预加载完成。 | `{ count, startIndex, replace }` |
| `jitQueue:error` | 某首曲目的 JIT 操作失败。 | URL 分支为 `{ trackId, error, url }`，本地路径分支为 `{ trackId, error, path }`。 |
