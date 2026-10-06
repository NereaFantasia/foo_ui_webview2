# JIT Queue 即时队列 API

`jitQueue` 命名空间的方法：为流媒体设计的即时队列。

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

<!-- api-schema:begin jitQueue.playNow -->
用这首曲目开启新的 JIT 会话：清空缓冲并开始播放。宿主自己判断 URL 类型（`http(s)://` 是流，Windows 或 UNC 路径是本地文件）。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `trackId` | `string` | 是 | 调用方给曲目的标识，会在会话状态与事件里回显。不能为空。 |
| `title` | `string` | 否 | 显示标题。默认 `""`。 |
| `url` | `string` | 是 | 流 URL 或文件路径。最多 2048 字符。不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `trackId` | `string` | 传入的标识。 |
| `shadowPlaylist` | `integer` | 影子播放列表的索引；本次会话还没有时由这次调用创建或接管；只有 foobar2000 建不出来时才是 `-1`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

立即播放指定曲目。清空缓冲区并开始新的播放会话。

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

<!-- api-schema:begin jitQueue.enqueueNext -->
把下一首预载进缓冲，用于响应 `jitQueue:needNext`。除非有 JIT 会话正在播放或正在等下一首，否则以 `NO_ACTIVE_ITEM` 拒绝。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `trackId` | `string` | 是 | 调用方给曲目的标识。不能为空。 |
| `title` | `string` | 否 | 显示标题。默认 `""`。 |
| `url` | `string` | 是 | 流 URL 或文件路径。最多 2048 字符。不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `trackId` | `string` | 传入的标识。 |
| `bufferSize` | `integer` | 之后缓冲里的曲目数。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

预加载下一首曲目到缓冲区。响应 `jitQueue:needNext` 事件时调用。

### jitQueue.skip

<!-- api-schema:begin jitQueue.skip -->
跳到缓冲中的下一首。没有活动的 JIT 会话时以 `NO_ACTIVE_ITEM` 拒绝。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `currentTrackId` | `string` | 跳过后正在播放的曲目标识。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

跳到缓冲区中的下一首曲目。

### jitQueue.stop

<!-- api-schema:begin jitQueue.stop -->
停止播放，可选保留缓冲。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `clearBuffer` | `boolean` | 否 | 是否同时清空缓冲。默认 `true`。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

停止播放并可选清空缓冲区。

### jitQueue.clear

<!-- api-schema:begin jitQueue.clear -->
清空缓冲并让会话回到空闲。

无参数。

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

清空影子播放列表缓冲区。

### jitQueue.getState

<!-- api-schema:begin jitQueue.getState -->
报告 JIT 会话的状态。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `isActive` | `boolean` | 是否有活动的 JIT 会话。 |
| `state` | `"Idle" \| "Active" \| "WaitingNext" \| "Exhausted" \| "Unknown"` | 会话状态。 |
| `currentTrackId` | `string` | 正在播放的曲目标识；没有时为空。 |
| `nextTrackId` | `string` | 已缓冲的下一首标识；没有时为空。 |
| `bufferSize` | `integer` | 缓冲里的曲目数。 |
| `shadowPlaylist` | `integer` | 影子播放列表的索引；在本次 foobar2000 运行中的 `jitQueue.playNow` 或 `jitQueue.preloadBatch` 创建或接管它之前为 `-1`，即便上次运行留下了一个；列表被删掉之后也回到 `-1`，同时 JIT 会话结束。停止或清空保留那张已清空列表的索引。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

获取 JIT 队列状态。

`state` 可能的值：`"Idle"` / `"Active"` / `"WaitingNext"` / `"Exhausted"`

### jitQueue.notifyEmpty

<!-- api-schema:begin jitQueue.notifyEmpty -->
告诉宿主页面没有更多曲目可供；会话进入 `Exhausted` 并发出 `jitQueue:listExhausted`。

无参数。

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

显式通知后端前端已无更多曲目。

### jitQueue.preloadBatch

<!-- api-schema:begin jitQueue.preloadBatch -->
把一批曲目插入影子播放列表，会话尚未在播放时从 `startIndex` 起播。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `urls` | `string[]` | 否 | 曲目 URL 或 `path\|subsong:N`。超过 2048 字符的条目丢弃并计入 `invalidCount`；剩余最多 10000 条。 |
| `startIndex` | `integer` | 否 | 从第几条起播。不小于 `0`。默认 `0`。 |
| `replace` | `boolean` | 否 | 插入前先清空影子播放列表。`false` 为追加。默认 `true`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `tracksAdded` | `integer` | 插入的曲目数。 |
| `invalidCount` | `integer` | 因过长被丢弃的条数。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

批量预加载曲目到 shadow playlist。使用 `handle_create()` 纯内存创建句柄，零 I/O 开销。

`replace` 默认为 `true`，插入前会清空 shadow playlist；如需在不打扰已入队内容的前提下追加，请显式传 `false`。非字符串条目以 `INVALID_PARAMS` 拒绝；长度超过 2048 的条目会被丢弃并计入 `invalidCount`，不会导致整次调用失败。

10000 上限是在丢弃上述无效条目**之后**才判断的，因此只统计有效条目。超限会**整批失败**而非截断，以 `INVALID_PARAMS` 报 `Batch exceeds maximum size (10000)` 且**不含** `tracksAdded`。`urls` 为空数组或 `startIndex` 越界同样是 `INVALID_PARAMS`，附带 `tracksAdded: 0` 与 `invalidCount`；宿主插入失败是 `OPERATION_FAILED`。

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

## JIT Queue 事件

维护 JIT shadow playlist 期间会发出以下事件。若前端需要补充或观察缓冲区，请在调用操作前订阅。

| 事件 | 含义 | Payload keys |
| --- | --- | --- |
| `jitQueue:needNext` | 管理器需要下一个逻辑曲目。 | `{ currentTrackId, reason }` |
| `jitQueue:trackChanged` | JIT 当前曲目发生变化。 | `{ trackId, title }` |
| `jitQueue:listExhausted` | 前端报告没有更多可用曲目。 | `{ lastTrackId }` |
| `jitQueue:preloadComplete` | 批量预加载完成。 | `{ count, startIndex, replace }` |
| `jitQueue:error` | 某首曲目的 JIT 操作失败。 | URL 分支为 `{ trackId, error, url }`，本地路径分支为 `{ trackId, error, path }`。 |
