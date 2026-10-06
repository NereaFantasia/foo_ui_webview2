# fb.jitQueue JIT 即时队列

`fb.jitQueue` 管理自适应播放用的流式预载队列，和代表 foobar2000 播放队列的 `fb.queue` 是两回事。曲目用调用方自定的 `trackId` 加 `title`、`url` 标识，不用播放列表下标。

## getState()

签名：`fb.jitQueue.getState(): Promise<JitQueueGetStateResponse>`

返回当前队列状态，包括 `isActive`、`state`、`currentTrackId`、`nextTrackId`、`bufferSize` 与 `shadowPlaylist`。`state` 取 `Idle`、`Active`、`WaitingNext`、`Exhausted`、`Unknown` 之一；影子播放列表创建之前 `shadowPlaylist` 为 `-1`。

```javascript
const state = await fb.jitQueue.getState();
```

## enqueueNext(opts)

签名：`fb.jitQueue.enqueueNext(opts: JitQueueEnqueueNextParams): Promise<JitQueueEnqueueNextResponse>`

将下一项加入自适应播放队列，通常用来响应 `jitQueue:needNext`。`opts` 接受 `trackId`、`title` 和 `url`；URL 超过 2048 个字符时返回 `success: false`，错误码为 `INVALID_PARAMS`。除非有会话正在播放或正在等下一首，否则以 `NO_ACTIVE_ITEM` 拒绝。

返回值带传入的 `trackId` 和 `bufferSize`（之后缓冲里的曲目数）。曲目在调用返回之后才加入，所以加入失败只通过 `jitQueue:error` 告知。

```javascript
await fb.jitQueue.enqueueNext({
	trackId: 'track-42',
	title: '下一首',
	url: 'https://media.example.com/next.flac'
});
```

## playNow(opts)

签名：`fb.jitQueue.playNow(opts: JitQueuePlayNowParams): Promise<JitQueuePlayNowResponse>`

立即播放指定项目。参数同样包含 `trackId`、`title` 与 `url`，并采用 2048 字符的 URL 上限。

每次调用都开启新会话：清空缓冲并开始播放。宿主自己判断 `url` 的类型：`http(s)://` 是流，Windows 或 UNC 路径是本地文件。返回值带传入的 `trackId` 和 `shadowPlaylist`（影子播放列表的索引）。和 `enqueueNext()` 一样，曲目加入失败通过 `jitQueue:error` 告知。

```javascript
await fb.jitQueue.playNow({
	trackId: 'track-41',
	title: '当前曲目',
	url: 'https://media.example.com/current.flac'
});
```

## clear()

签名：`fb.jitQueue.clear(): Promise<JitQueueClearResponse>`

清空 JIT 队列的缓冲，并让会话回到空闲。

```javascript
await fb.jitQueue.clear();
```

## notifyEmpty()

签名：`fb.jitQueue.notifyEmpty(): Promise<JitQueueNotifyEmptyResponse>`

告诉宿主页面没有更多曲目可以入队。会话进入 `Exhausted`，宿主发出 `jitQueue:listExhausted`；没有活动会话时也照样发出。

```javascript
await fb.jitQueue.notifyEmpty();
```

## preloadBatch(opts)

签名：`fb.jitQueue.preloadBatch(opts: JitQueuePreloadBatchParams): Promise<JitQueuePreloadBatchResponse>`

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `urls` | `string[]` | 要插入的曲目 URL 或 `path\|subsong:N` |
| `startIndex` | `number` | 可选，会话尚未在播放时从第几条起播；默认 `0` |
| `replace` | `boolean` | 是否替换现有的预载列表，`false` 为追加；默认 `true` |

把一批曲目插入影子播放列表。每条 URL 最多 2048 字符，超长的条目跳过并计入 `invalidCount`，剩余最多 10000 条。

返回值带 `tracksAdded` 和 `invalidCount`。`jitQueue:preloadComplete` 在调用返回之前发出；调用失败时不发。

```javascript
const result = await fb.jitQueue.preloadBatch({
	urls: ['https://media.example.com/1.flac', 'https://media.example.com/2.flac'],
	startIndex: 0,
	replace: true
});
```

## skip()

签名：`fb.jitQueue.skip(): Promise<JitQueueSkipResponse>`

跳到缓冲中的下一项。返回值带 `currentTrackId`，即跳过后正在播放的曲目。没有活动会话时以 `NO_ACTIVE_ITEM` 拒绝。

```javascript
const result = await fb.jitQueue.skip();
```

## stop(opts?)

签名：`fb.jitQueue.stop(opts?: JitQueueStopParams): Promise<JitQueueStopResponse>`

停止 JIT 播放。`opts.clearBuffer` 决定是否同时清空缓冲，默认 `true`；要保留已缓冲的曲目就传 `{ clearBuffer: false }`。

```javascript
await fb.jitQueue.stop();
await fb.jitQueue.stop({ clearBuffer: false });
```

## 事件

通过 `fb.on()` 订阅以下冒号格式事件：

- `jitQueue:needNext` — `{ currentTrackId, reason }`
- `jitQueue:trackChanged` — `{ trackId, title }`
- `jitQueue:listExhausted` — `{ lastTrackId }`
- `jitQueue:preloadComplete` — `{ count, startIndex, replace }`
- `jitQueue:error` — `{ trackId, error, url? }` 或 `{ trackId, error, path? }`

载荷类型 `JitQueueNeedNextPayload`、`JitQueueTrackChangedPayload`、`JitQueueListExhaustedPayload`、`JitQueuePreloadCompletePayload`、`JitQueueErrorPayload` 都从包根导出。

这些事件发给开启会话的页面，即调用 `playNow()` 或调用了开始播放的 `preloadBatch()` 的页面。缓冲里没有下一首、也没有仍在等回复的请求时，宿主发出 `jitQueue:needNext`；页面用 `enqueueNext()` 作答，没有更多曲目时用 `notifyEmpty()` 作答。

```javascript
const off = fb.on('jitQueue:needNext', ({ currentTrackId, reason }) => {
	console.log(currentTrackId, reason);
});
```
