# JIT 会话客户端

`JitSessionClient` 管理绑定当前文档的控制凭据，并串行发送实验性 `jitSession` 协议的命令。
在创建或认领会话前构造客户端，它会保存早于命令应答到达的事件。
使用前检查[宿主能力](../api/jit-session.md)。

## 开始一次尝试

```typescript
import { JitSessionClient } from 'foo-webview-sdk/bridge';

const client = new JitSessionClient();
await client.create();
const accepted = await client.play({
  entryId: 'queue-entry-1',
  location: 'https://media.example.com/track.flac',
});
const result = await client.waitForOperation(accepted.operationId);
console.log(result.status, result.reason);
```

命令被受理不表示资源能播。`waitForOperation()` 返回操作终态，包含 `failed` 与 `superseded`，
需要检查 `status`。播放尝试的结局另外通过 `attemptFinished` 报告；宿主无法确认结束原因时
结果为 `unknown`。

停止原因来自 foobar2000 回调。播放列表的默认动作可能先报告 `user`，此时尝试以 `stopped`
结束；即使之后另有曲目起播，会话仍保持停止。活动会话未收到这次停止、直接切到缓冲之外的
曲目时，在新曲目起播后报告 `external`。两条路径都保留控制权。

通过 script 标签加载的 bundle 提供同一个类：`fb.JitSessionClient`。

## 提供或撤销下一条

```typescript
import { JitSessionClient } from 'foo-webview-sdk/bridge';

const client = new JitSessionClient();
await client.create();
const accepted = await client.play({ entryId: 'one', location: 'C:/Music/one.flac' });
await client.setNext({
  afterAttemptId: accepted.attemptId,
  entryId: 'two',
  location: 'C:/Music/two.flac',
});
// 撤销下一条，当前尝试继续播放
await client.setNext({ afterAttemptId: accepted.attemptId });
// 声明当前条目是最后一条
await client.setNext({ afterAttemptId: accepted.attemptId, endOfQueue: true });
```

每次更新都要指明接在哪次尝试之后。同一条目重播会得到新的 `attemptId`，针对旧尝试的更新
会被拒绝。不能同时提供新条目和 `endOfQueue: true`。

foobar2000 会在当前曲目快结束时选定接下来播哪一首，选定后无法撤回。距结尾不到输出缓冲长度
加 1 秒时，撤销或替换已缓冲的下一条以 `CONFLICT` 拒绝，`details.reason` 为 `nextPreselected`；
已缓冲的下一条照常接着播放，等它开始后再调整之后的条目。

`client.skip(attemptId)` 要求下一条已经就绪。没有下一条或仍在准备时，以 `NO_ACTIVE_ITEM`
拒绝；当前尝试继续播放，版本号不变，不创建操作，也不改动原生播放队列。

## 页面重载后接续

```typescript
import { jitSession, JitSessionClient, unwrap } from 'foo-webview-sdk/bridge';

const client = new JitSessionClient();
const { session } = unwrap(await jitSession.discover());
if (session?.canResume) {
  await client.claimControl({ sessionId: session.sessionId, mode: 'resume' });
  console.log(client.state);
}
```

恢复只认领控制并读取最新快照，不发送 `play`。其他窗口使用 `mode: 'handover'`，并携带读到的
`expectedOwnerEpoch`。令牌不能跨文档使用，也不会出现在 `client.state` 中。

## 接管原生播放

```typescript
import { JitSessionClient } from 'foo-webview-sdk/bridge';

const client = new JitSessionClient();
await client.create();
const snapshot = await client.prepareAdoption();
const queue = await client.getNativeContext({
  contextId: snapshot.contextId, expectedContextRevision: snapshot.contextRevision,
  collection: 'manual', offset: 0, limit: 128,
});
const adopted = await client.adoptNative({
  contextId: snapshot.contextId, snapshotToken: snapshot.snapshotToken, currentEntryId: 'native-current',
});
const first = queue.items[0];
if (first) {
  await client.setNativeNext({
    contextId: snapshot.contextId, afterAttemptId: adopted.attemptId,
    nativeRef: first.nativeRef, entryId: 'queued-1',
  });
}
// 之后：交还剩余的队列出现，接着播放来源
await client.release();
```

`prepareAdoption()` 不改变任何状态；快照须在 30 秒内由 `adoptNative()` 接纳。正在播放的曲目继续解码，
原生队列被清空，其中的出现由宿主保存。`playNative()` 与 `setNativeNext()` 按 `nativeRef` 供给来源行或
队列出现；`setNativeQueue()` 保留并调整仍待交还的出现。`stop()` 与 `release()` 交还剩余的出现；
`release(requestId, 'detach')` 不动原生播放。`getCapabilities().adoption` 表示宿主是否支持接管；
当前这次播放是否符合条件由 `prepareAdoption()` 判定。

## 事件、冲突与清理

`client.subscribe(listener)` 返回取消订阅的函数。回调的事件包含 `name`（`stateChanged`、
`needNext`、`operationCompleted`、`attemptFinished`、`attemptRechecked` 或 `controlChanged`）和对应类型的 `payload`。
尝试以 `unknown` 结束且未播满 1 秒时，随后会收到 `attemptRechecked`：宿主重开了该资源，报告是否再次
失败及原因（`accessDenied`、`notFound`、`unsupportedContent`、`network`）。它用来决定下一步，例如
重新解析地址或跳过该条目，不能当作原来那次失败原因的证明。
客户端按 `eventSeq` 去重；尚未处理的操作终态即使早于最新快照，也会保留。发现序号缺口时
自动刷新快照；`synchronizationError` 保存刷新失败的错误，成功的 `refresh()` 会清除此错误。
自动刷新和显式刷新都会补查已有等待者的操作，因此开始等待后丢失的终态也能补回。
查询结果仍为 `pending` 时继续等待；补偿期间再次出现事件缺口，会再执行一轮查询。

客户端自动填入 `sessionId`、`controlToken` 和 `expectedRevision`，串行发送自身的写命令。
宿主播放仍可能在事件与命令之间变化。遇到 `CONFLICT` 时，调用 `refresh()` 后判断命令是否
仍适用，客户端不会自动重放。只有重试同一条命令时才复用 `requestId`。收到 `NOT_OWNER`
或控制权交接事件后，本地写入停止，重新认领控制后才能继续。

`waitForOperation(id, { timeoutMs, signal })` 先检查已保存的事件，再查询宿主；尚未结束时继续
等待终态。默认超时为 30000 毫秒。超时或 abort 只拒绝本地等待，不取消宿主操作。

`stop()` 停止播放，保留会话供查询。`release()` 释放控制与缓冲，正在解码的资源继续播放。释放失败时（例如接管的队列出现只交还了一部分），客户端保留凭据并刷新 `client.state`。
`dispose()` 取消订阅、拒绝未结束的等待并禁止继续发命令，不停止播放，也不释放宿主会话。
在所属组件或页面销毁时调用它。

## 直接使用命名空间

`fb.jitSession` 转发声明中的参数，返回应答信封。直接使用时，应用须先订阅事件，再调用方法，
自行保存凭据和核对状态。[API 参考](../api/jit-session.md) 定义参数约束、记录保留期和事件投递。
下面是各个方法的独立调用示例；应将示例标识与版本替换为实际读到的值。

```typescript
await fb.jitSession.getCapabilities();
await fb.jitSession.discover();
await fb.jitSession.create({ requestId: 'create-1' });
await fb.jitSession.getState({ sessionId: 'session-id' });
await fb.jitSession.claimControl({
  sessionId: 'session-id', mode: 'handover', expectedOwnerEpoch: 1, requestId: 'claim-1',
});
const guard = {
  sessionId: 'session-id', controlToken: 'document-token', expectedRevision: 1, requestId: 'command-1',
};
await fb.jitSession.play({ ...guard, entryId: 'entry-id', location: 'C:/Music/track.flac' });
await fb.jitSession.setNext({ ...guard, afterAttemptId: 'attempt-id', endOfQueue: true });
await fb.jitSession.skip({ ...guard, attemptId: 'attempt-id' });
await fb.jitSession.stop(guard);
await fb.jitSession.getOperation({ sessionId: 'session-id', operationId: 'operation-id' });
await fb.jitSession.prepareAdoption(guard);
await fb.jitSession.getNativeContext({
  sessionId: 'session-id', contextId: 'context-id', expectedContextRevision: 1,
  collection: 'manual', offset: 0, limit: 128,
});
await fb.jitSession.adoptNative({
  ...guard, contextId: 'context-id', snapshotToken: 'snapshot-token', currentEntryId: 'entry-id',
});
await fb.jitSession.playNative({ ...guard, contextId: 'context-id', nativeRef: 'native-ref', entryId: 'entry-2' });
await fb.jitSession.setNativeNext({
  ...guard, contextId: 'context-id', afterAttemptId: 'attempt-id', nativeRef: 'native-ref', entryId: 'entry-3',
});
await fb.jitSession.setNativeQueue({
  ...guard, contextId: 'context-id', expectedContextRevision: 2, remainingOccurrenceIds: ['occurrence-id'],
});
await fb.jitSession.release(guard);
```
