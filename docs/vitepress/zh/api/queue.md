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

`playlist` 与 `playlistItem` 两个键始终存在：条目带播放列表位置时是精确整数；不带位置时两者同为 `null`——经 `queue.insertNext` 的 `paths` 入队的条目，以及所引用的列表项已被移除的条目都属于这种情形。判断只需一句 `item.playlist == null`。

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

### queue.setContents

用一份有序引用列表替换整个队列。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `items` | `array<{ queueIndex: number } \| { playlist: number, item: number }>` | 是 | 有序引用；`{ queueIndex }` 保留/重排一个已在队列中的槽位，`{ playlist, item }` 添加一个尚未入队的播放列表曲目。 |

**返回值**: `{ "success": true, "queueCount": 5 }`

任一条目形态无法识别时，整次调用在写入前就整体失败——与 `queue.add` 逐项静默跳过越界下标不同，这里任何一条无效引用都会拒绝整次调用并保持队列不变。`queueCount` 无论成功还是失败都会带上。传入空数组会清空队列，等价于 `queue.clear`。`items.length` 上限为 `max(256, 当前队列长度)`，超限会失败且不改变队列。

::: tip 拖拽重排的调用方式
应在用户**松手时**调用一次 `setContents`，不要在拖拽过程中逐帧调用——每次调用都会清空并重建整个队列，拖拽期间逐帧调用是无意义的额外开销，还会造成明显卡顿。
:::

```javascript
// 重排：把队列第 2 项移到队首，其余保持原序
await fb2k.invoke('queue.setContents', {
    items: [{ queueIndex: 2 }, { queueIndex: 0 }, { queueIndex: 1 }],
});
// 清空队列
await fb2k.invoke('queue.setContents', { items: [] });
```

### queue.insertNext

插入曲目使其成为下一首播放，插入到当前队列所有曲目之前。条目可以按文件路径给、按播放列表位置给，或两者都给。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `array<string>` | 否¹ | 文件路径或 URL，可带 `\|subsong:N` 后缀。入队的条目不带播放列表位置。 |
| `items` | `array<{ playlist, item }>` | 否¹ | 播放列表位置，形态与 `queue.setContents` 接受的相同。入队的条目带该位置，播放游标会跟随。 |
| `position` | `integer` | 否 | 移除所有被移动的已入队项**之后**的队列插入下标，默认 `0`（队首）。 |

¹ `paths` 与 `items` 至少一个非空。

**返回值**: `{ "success": true, "insertedCount": 1, "movedCount": 1, "queueCount": 5, "invalidCount": 0 }`

已在队列中的曲目会被移动到 `position`，而不是重复入队；每首只移动一个条目，选择规则见下文。同一次调用里 `items` 块排在前、`paths` 块排在后，各块内部保持传入顺序。`insertedCount` 统计新入队的曲目数，`movedCount` 统计被移动的既有条目数。`invalidCount` 是输入路径数减去路径解析所得的曲目数，负数按零返回；它不是精确的失败路径数，因为文件夹或容器展开出的多首曲目可能抵消失败项。`items` 不计入 `invalidCount`：任一条目不是对象、缺少 `playlist` 或 `item`、位置越界时，整次调用失败，`error` 指明 `items[i]`，队列不变，同一次调用的 `paths` 也不会入队。

所有输入条目按曲目去重，不按位置。重复 `items` 中，优先保留首个已在队列中的输入坐标；都不匹配时才保留首个输入坐标，曲目在输入块内的顺序仍取首次出现的位置。移动时优先选择坐标完全匹配的既有条目，否则移动该曲目的首个队列条目并赋予所选坐标。仅按路径移动时保留既有条目的有效坐标，只有新插入的路径条目不带坐标。`queue.add` 和 `queue.setContents` 保留重复引用。`insertedCount + movedCount` 少于输入条目数，可能是去重或无效路径所致；文件夹和容器路径也可能展开为多首曲目。

两个数组都为空或都未传时返回 `{ "success": false, "error": "No paths or items specified" }`；`items` 不是数组时返回 `"items must be an array"`；没有 `items` 且 `paths` 一个有效曲目都解析不出时返回 `{ "success": false, "error": "No valid tracks found", "invalidCount": ... }`。

::: warning 已知限定：旧版 `|subsong:N` 匹配
经旧版 `queue.addPaths` 的 `path|subsong:N` 语法进入队列的曲目，再次传给 `insertNext` 时可能匹配不上"已在队列中"——结果是重复入队而不是被移动。这只影响升级前已入队的曲目，新调用不受影响。多 subsong 文件的裸路径与该路径显式带 `|subsong:0` 后缀也被视为两个不同的身份，而非等价——不要假设它们会互相去重。
:::

::: warning 已知限定：播放游标不会跟随按路径插播的曲目
经 `paths` 入队的条目不带播放列表位置（它们不写入任何播放列表，这正是保持播放列表干净的代价）。foobar2000 因而把它们视为"列表外播放"：播放期间 `playback.getCurrentTrackIndex` 查不到位置；播完后从**插播前正在播放的那一项的下一项**继续，而不是从插播曲目在列表中的位置继续。当插播曲目本身也在正在播放的列表里时有两个可见后果：顺序在原处接续（如第 8 项 → 插播项 → 第 9 项），且顺序播到该曲目时它会**再播一次**。经 `items` 入队的条目与 `queue.add({ playlist, tracks })` 入队的条目一样带位置，没有这一行为——游标会跳到被消费项处继续。已知曲目在列表中的位置时，请改传 `items`。已入队条目若其列表位置失效（所引用的列表被清空），经 `moveToTop` / `setContents` / `playNow` 重建队列后也会退化为同样的无位置形态。

`playback.previous` 受同一机制影响。foobar2000 的"上一曲"按游标在列表中的位置推算，不按播放历史，已消费的队列条目也不会放回。无位置条目播放期间按上一曲，落到**被打断那一项的前一项**（被打断的曲目本身被跳过）；带位置条目播放期间按上一曲，落到该条目自身列表位置的前一项。两种情况都不会回到被打断的曲目。
:::

```javascript
// 按路径：条目不带播放列表位置
await fb2k.invoke('queue.insertNext', { paths: ['C:\\Music\\a.flac'], position: 0 });
// 按播放列表位置：播放时游标跟随该条目
await fb2k.invoke('queue.insertNext', { items: [{ playlist: 0, item: 12 }] });
```

### queue.playNow

立即播放队列中 `index` 处的曲目；若该曲目尚不在队首，会先把它移到队首。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `index` | `integer` | 否 | 要播放的队列下标，默认 `0`（当前队首）。 |

**返回值**: `{ "success": true, "playedIndex": 0, "queueCount": 4 }`

队列为空时返回 `{ "success": false, "error": "Queue is empty" }`；`index` 为负数、非整数或越界时返回 `{ "success": false, "error": "Invalid queue index" }`。

::: warning `queueCount` 的时序不作保证
`queueCount` 是在播放启动后立即读取的。宿主不保证队首曲目的消费与这次读取同步发生，因此该字段可能反映的是播放曲目被移除**之前**或**之后**的队列状态。如果需要精确的播放后长度，请随后调用 `queue.getCount` 确认。
:::

```javascript
// 播放当前队首曲目
await fb2k.invoke('queue.playNow');
// 播放第 3 项，先移到队首再播放
await fb2k.invoke('queue.playNow', { index: 2 });
```

### 显式起播会清空队列

`playlist.playTrack`、`playlist.replaceAllAndPlay`、`playback.playPath`、`playback.playPaths`，以及 `jitQueue.preloadBatch` 在实际起播时（首次调用或 `replace: true`），都走 foobar2000 的默认动作路径，该路径会**先清空整个播放队列**。这是核心行为，不是组件附加的：核心把"播放这一项"实现为*清空队列 → 目标项入队 → 立即消费*，所以你会连续收到三次 `playback:queueChanged`（`user_removed` 且 `count: 0`、`user_added`、`playback_advance`）。这些调用都没有保留队列的选项。

**播放列表项且保留队列。** 先用 `setContents` 把目标项放到队首，再用 `playNow` 消费它：

```javascript
const { count } = await fb2k.invoke('queue.getCount');
await fb2k.invoke('queue.setContents', {
    items: [
        { playlist, item },                                            // 现在要播的曲目
        ...Array.from({ length: count }, (_, i) => ({ queueIndex: i })), // 原有条目，保持原序
    ],
});
await fb2k.invoke('queue.playNow'); // 消费队首，其余条目留在队列里
```

查询条目数之后，再用两次调用完成一次队列重建和起播。被播放的曲目带有列表位置，游标会跟随它，上文“游标不会跟随”的限定不适用。

**恢复被清空的队列。** 宿主不保存被清掉的条目，所以要在起播调用**之前**用 `queue.get` 取得快照。`playlist` 不为 `null` 的条目可通过 `setContents` 或 `insertNext({ items })` 加入；`playlist: null` 的条目需要通过 `insertNext({ paths })` 重建。两种形态可放在同一次 `insertNext` 调用中，但坐标项会排在路径项之前，且按曲目去重。随后可用 `setContents` 恢复顺序和重复次数，但不保证恢复原来的坐标形态：同一曲目原先若同时有带坐标项和无坐标项，复制剩余条目的 `queueIndex` 也会复制它现有的坐标。全部按路径重建会丢失列表坐标，也可能合并重复项。

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
