# Queue & Selection

## Selection API - 选择同步

### selection.getViewerMode 

获取用户的 Selection Viewer 偏好设置。v1.1.16

**返回值**: `{ "mode": "prefer_playing" }` 或 `{ "mode": "prefer_selection" }`

### selection.getViewingTrack 

获取当前应该显示的曲目，自动根据 Viewer 模式执行 Fallback。v1.1.16

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| includeTrackInfo | boolean | ✗ | 是否包含完整元数据 |

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

获取当前全局选择的曲目列表，支持分页。v1.1.16

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| offset | number | ✗ | 起始位置（默认 0） |
| limit | number | ✗ | 获取数量（默认 100，0=全部） |

::: warning 性能提示
选择超过 100 个曲目时，未指定 limit 会自动截断为 100 个。
:::

### selection.getType 

获取当前选择类型。v1.1.16

| type | typeName | 说明 |
| --- | --- | --- |
| 0 | now_playing | 正在播放 |
| 1 | active_playlist_selection | 活动播放列表的选择 |
| 2 | active_playlist | 活动播放列表 |
| 3 | playlist_manager | 播放列表管理器 |
| 5 | media_library_viewer | 媒体库查看器 |

### selection.set 

设置当前全局选择。v1.1.16

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| handles | string[] | ✓ | metadb handle 字符串数组 |

### selection.setPlaylistTracking 

设置播放列表跟踪模式。v1.1.16

| mode 值 | 说明 |
| --- | --- |
| "selection" | 跟踪播放列表中用户选择的曲目 |
| "playlist" | 跟踪整个播放列表 |

一步添加 URL/本地路径到播放队列。自动处理添加到播放列表和入队操作。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| paths | array | ✓ | 文件路径或 URL 数组 |
| playlist | number | ✗ | 目标播放列表索引 |
| useQueuePlaylist | boolean | ✗ | 使用专用 "[WebView Queue]" 播放列表（默认 true） |

```javascript
await fb2k.invoke('queue.addPaths', {
    paths: ['C:/Music/song.mp3', 'http://stream.example.com/audio.mp3']
});
```

## Queue API - 播放队列

### queue.add 

将播放列表中的曲目添加到队列。支持单个曲目或批量添加。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| playlist | number | ✗ | 播放列表索引（默认当前活动） |
| track | number | ✗ | 单个曲目索引 |
| tracks | number[] | ✗ | 曲目索引数组 |

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

从队列中移除指定项。支持单个索引或批量移除。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| index | number | ✗ | 要移除的队列索引（单个） |
| indices | number[] | ✗ | 要移除的多个队列索引 |

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

`queue.clear` 的别名。清空整个播放队列。

### queue.getCount 

获取队列项数量。返回 `{ "count": 3, "hasItems": true }`

```javascript
const result = await fb2k.invoke('queue.getCount');
console.log(`队列中有 ${result.count} 项`);
```

### queue.moveToTop 

将队列中的指定项移动到队首（下一首播放）。内部通过清空队列并重建实现。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| index | number | ✓ | 要移动的队列索引（不能为 0） |

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

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| trackId | string | ✓ | 前端唯一标识符 |
| title | string | ✗ | 曲目标题 |
| url | string | ✓ | 流媒体 URL 或本地文件路径（自动检测） |

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

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| trackId | string | ✓ | 前端唯一标识符 |
| title | string | ✗ | 曲目标题 |
| url | string | ✓ | 流媒体 URL 或本地文件路径 |

### jitQueue.skip 

跳到缓冲区中的下一首曲目。

- **参数**: 无
- **返回值**: `{ "success": true, "currentTrackId": "..." }`

### jitQueue.stop 

停止播放并可选清空缓冲区。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| clearBuffer | boolean | ✗ | 是否清空缓冲区（默认 true） |

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

- **参数**:`urls` (string[], 必填) — 曲目 URL 列表，支持 `path|subsong:N` 格式
- `startIndex` (number, 可选, 默认 0) — 起始播放位置
- `replace` (boolean, 可选, 默认 true) — 是否清空现有 buffer

**限制**: 单次最多 10000 条

**返回值**: `{ "success": true, "tracksAdded": 50 }`

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
| jitQueue:error | URL 解析失败等错误 | { trackId, error, url } |
