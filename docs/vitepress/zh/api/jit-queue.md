# JIT Queue 即时队列 API

提供面向网络流媒体与动态播放场景的即时动态队列（Just-In-Time Queue）调度能力。

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
基于指定曲目开启全新 JIT 会话（清空当前缓冲并立即起播；宿主自动识别 URL 协议与本地文件路径；原生播放队列保留并在本会话结束后接续播放）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `trackId` | `string` | 是 | 调用方分配的曲目唯一标识符 trackId（在状态查询与事件中回显）不能为空。 |
| `title` | `string` | 否 | 用于界面展示的曲目标题默认 `""`。 |
| `url` | `string` | 是 | 音频流 URL 或本地文件物理路径（最多 2048 字符）不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `trackId` | `string` | 传入的曲目标识符 trackId |
| `shadowPlaylist` | `integer` | 影子播放列表在播放列表管理器中的索引（不存在时由宿主自动创建接管，创建失败返回 -1） |

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
将下一首曲目预载进播放缓冲（用于响应 jitQueue:needNext 事件；仅在存在活跃会话播放或等待下一首时受理，否则返回 NO_ACTIVE_ITEM）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `trackId` | `string` | 是 | 调用方分配的曲目唯一标识符 trackId不能为空。 |
| `title` | `string` | 否 | 用于界面展示的曲目标题默认 `""`。 |
| `url` | `string` | 是 | 音频流 URL 或本地文件物理路径（最多 2048 字符）不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `trackId` | `string` | 传入的曲目标识符 trackId |
| `bufferSize` | `integer` | 预载完成后缓冲内的曲目总数 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

预加载下一首曲目到缓冲区。响应 `jitQueue:needNext` 事件时调用。

### jitQueue.skip

<!-- api-schema:begin jitQueue.skip -->
跳过当前曲目并播放缓冲中的下一首（若缓冲存在后续曲目则直接播放且不改动原生队列；否则执行原生下一首逻辑；无活跃会话时返回 NO_ACTIVE_ITEM）

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `currentTrackId` | `string` | 调用返回时会话记录的曲目标识符（此时新曲目尚未起播，仍为被跳过的曲目 ID，起播后由 jitQueue:trackChanged 通知） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

跳到缓冲区中的下一首曲目。

### jitQueue.stop

<!-- api-schema:begin jitQueue.stop -->
停止当前播放（支持选择是否保留已缓冲内容）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `clearBuffer` | `boolean` | 否 | 停止播放时是否同步清空已缓冲的曲目默认 `true`。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

停止播放并可选清空缓冲区。

### jitQueue.clear

<!-- api-schema:begin jitQueue.clear -->
清空当前播放缓冲并使会话恢复空闲状态

无参数。

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

清空影子播放列表缓冲区。

### jitQueue.getState

<!-- api-schema:begin jitQueue.getState -->
查询 JIT 会话当前运行状态

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `isActive` | `boolean` | 当前是否存在活跃运行的 JIT 会话 |
| `state` | `"Idle" \| "Active" \| "WaitingNext" \| "Exhausted" \| "Unknown"` | 会话当前运行状态 |
| `currentTrackId` | `string` | 当前正在播放的曲目标识符 trackId（缺省时为空字符串） |
| `nextTrackId` | `string` | 已预载到缓冲的下一首曲目标识符（缺省时为空字符串） |
| `bufferSize` | `integer` | 当前缓冲队列中待播放的曲目总数 |
| `shadowPlaylist` | `integer` | 影子播放列表索引（被外部删除时重置为 -1 并终止会话；停止或清空操作会保留空列表的索引） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

获取 JIT 队列状态。

`state` 可能的值：`"Idle"` / `"Active"` / `"WaitingNext"` / `"Exhausted"`

### jitQueue.notifyEmpty

<!-- api-schema:begin jitQueue.notifyEmpty -->
通知宿主当前会话无更多后续曲目（会话进入 Exhausted 状态并派发 jitQueue:listExhausted 事件）

无参数。

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

显式通知后端前端已无更多曲目。

### jitQueue.preloadBatch

<!-- api-schema:begin jitQueue.preloadBatch -->
将一组曲目批量写入影子播放列表（若会话尚未播放则自动从 startIndex 处开始起播）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `urls` | `string[]` | 否 | 待批量预载的音频 URL 或「path\|subsong:N」条目列表（超出 2048 字符的条目将被丢弃并计入 invalidCount；单次最多支持 10000 条） |
| `startIndex` | `integer` | 否 | 批量插入后起始起播的曲目索引 startIndex不小于 `0`。默认 `0`。 |
| `replace` | `boolean` | 否 | 插入前是否清空影子播放列表（false 为追加到末尾）默认 `true`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `tracksAdded` | `integer` | 实际成功追加或写入的曲目数量 |
| `invalidCount` | `integer` | 因长度超出限制等原因被丢弃的无效条目数 |

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
