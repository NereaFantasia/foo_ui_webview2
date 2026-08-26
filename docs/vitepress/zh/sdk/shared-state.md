# fb.sharedState 跨窗口共享状态

`fb.sharedState` 提供跨窗口键值存储。它不同于同步的 `fb.state` 播放状态镜像：共享状态值可设置 TTL，并通过 `state:*` 事件族发布变化。

该存储是组件 `PortHub` 单例持有的进程内内存。在当前 foobar2000 进程中，
本组件的多个 WebView 窗口可共享；它不写磁盘、重启后丢失，也不与其它进程
或 SMP 运行时共享。

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## 其余方法

### delete()

封装 `state.delete`。显式删除成功后会发出 `state:deleted`，其 `reason` 为 `'deleted'`。

```javascript
const result = await fb.sharedState.delete('playlist:active-filter');
```

### keys()

封装 `state.keys`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
const { keys } = await fb.sharedState.keys('playlist:*');
```

### set()

封装 `state.set`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.sharedState.set('playlist:active-filter', 'favorites', false, 60_000);
```

<!-- END AUTO-GENERATED SDK STUBS -->

## 其他方法

### get()

`fb.sharedState.get(key: string): Promise<{ value: unknown }>` 读取一个值。

```javascript
const { value } = await fb.sharedState.get('playlist:active-filter');
```

## 事件

- `fb.sharedState.onChange(handler)` 订阅 `state:changed` 并返回取消订阅函数。类型化 payload 为 `StateChangedPayload<T>`，包含 `key`、`value`、`previousValue`、`sourceWindowId` 和可选的 `expiresAt`。
- `fb.sharedState.onDelete(handler)` 订阅 `state:deleted` 并返回取消订阅函数。`StateDeletedPayload.reason` 为 `'deleted'` 或 `'expired'`。

```javascript
const off = fb.sharedState.onChange(({ key, value }) => {
	console.log(key, value);
});

off();
```
