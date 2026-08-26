# fb.jitQueue JIT 即时队列

本页是 `fb.jitQueue` 的 SDK 视角文档入口。

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## 其余方法

### getState()

签名：`fb.jitQueue.getState(): Promise<JitQueueStateInfo>`

返回当前队列状态，包括 `isActive`、`state`、`currentTrackId`、`nextTrackId`、`bufferSize` 与 `shadowPlaylist`。

```javascript
const state = await fb.jitQueue.getState();
```

### enqueueNext(opts)

签名：`fb.jitQueue.enqueueNext(opts: JitQueueEnqueueNextParams): Promise<BaseResponse & { bufferSize?: number }>`

将下一项加入自适应播放队列。`opts` 接受 `trackId`、`title` 和 `url`；URL 超过 2048 个字符时返回 `success: false`。

```javascript
await fb.jitQueue.enqueueNext({
	trackId: 'track-42',
	title: '下一首',
	url: 'https://media.example.com/next.flac'
});
```

### playNow(opts)

签名：`fb.jitQueue.playNow(opts: JitQueuePlayNowParams): Promise<BaseResponse & { shadowPlaylist?: number }>`

立即播放指定项目。参数同样包含 `trackId`、`title` 与 `url`，并采用 2048 字符的 URL 上限。

```javascript
await fb.jitQueue.playNow({
	trackId: 'track-41',
	title: '当前曲目',
	url: 'https://media.example.com/current.flac'
});
```

### clear()

封装 `jitQueue.clear`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.jitQueue.clear(/* 参数见 TypeScript 声明 */);
```

### notifyEmpty()

封装 `jitQueue.notifyEmpty`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.jitQueue.notifyEmpty(/* 参数见 TypeScript 声明 */);
```

### preloadBatch()

封装 `jitQueue.preloadBatch`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.jitQueue.preloadBatch(/* 参数见 TypeScript 声明 */);
```

### skip()

封装 `jitQueue.skip`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.jitQueue.skip(/* 参数见 TypeScript 声明 */);
```

### stop()

封装 `jitQueue.stop`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.jitQueue.stop(/* 参数见 TypeScript 声明 */);
```

## 事件

通过 `fb.on()` 订阅以下冒号格式事件：

- `jitQueue:needNext` — `{ currentTrackId, reason }`
- `jitQueue:trackChanged` — `{ trackId, title }`
- `jitQueue:listExhausted` — `{ lastTrackId }`
- `jitQueue:preloadComplete` — `{ count, startIndex, replace }`
- `jitQueue:error` — `{ trackId, error, url? }` 或 `{ trackId, error, path? }`

<!-- END AUTO-GENERATED SDK STUBS -->
