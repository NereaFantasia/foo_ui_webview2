# fb.sharedState 跨窗口共享状态

`fb.sharedState` 提供跨窗口键值存储。它不同于同步的 `fb.state` 播放状态镜像：共享状态值可设置 TTL，并通过 `state:*` 事件族发布变化。

该存储是组件 `PortHub` 单例持有的进程内内存。在当前 foobar2000 进程中，
本组件的多个 WebView 窗口可共享；它不写磁盘、重启后丢失，也不与其它进程
或 SMP 运行时共享。

## delete(key)

签名：`fb.sharedState.delete(key: string): Promise<StateDeleteResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `key` | `string` | 是 | 要移除的键 |

响应里的 `existed` 表示调用前键是否存在；删掉一个存在的键时发出 `state:deleted`，其 `reason` 为 `'deleted'`。

```javascript
const result = await fb.sharedState.delete('playlist:active-filter');
```

## keys(pattern?)

签名：`fb.sharedState.keys(pattern?: string): Promise<StateKeysResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `pattern` | `string` | 否 | 键的模式，默认 `'*'` |

返回 `{ keys }`，顺序不固定。`'*'` 匹配全部键；以 `*` 结尾的模式按前缀匹配，如 `'playlist:*'`；其余按整个键精确匹配，中间的 `*` 只是普通字符。查询前先清除已过期的键，每个过期的键都会发出 `reason` 为 `'expired'` 的 `state:deleted`。

```javascript
const res = await fb.sharedState.keys('playlist:*');
if (res.success === false) throw new Error(res.error);
const { keys } = res;
```

## set(key, value, silent?, ttlMs?)

签名：`fb.sharedState.set(key: string, value: JsonValue, silent?: boolean, ttlMs?: number): Promise<StateSetResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `key` | `string` | 是 | 要写入的键 |
| `value` | `unknown` | 是 | 可序列化为 JSON 的值；顶层 `null` 会被拒绝，删除键用 `delete()` |
| `silent` | `boolean` | 否 | 为 `true` 时不发出变更事件，默认 `false` |
| `ttlMs` | `number` | 否 | 存活时间，单位毫秒；不给或不大于 0 时不过期 |

写入后广播 `state:changed`（`silent` 为 `true` 时不发），载荷带 `key`、`value`、`previousValue`（新键为 `null`）与 `sourceWindowId`。给了正的 `ttlMs` 时，结果与事件都带 `expiresAt`，即 Unix 纪元起的毫秒数。

```javascript
await fb.sharedState.set('playlist:active-filter', 'favorites', false, 60_000);
```

## get()

`fb.sharedState.get(key: string): Promise<StateGetResponse>` 读取一个值。

```javascript
const res = await fb.sharedState.get('playlist:active-filter');
if (res.success === false) throw new Error(res.error);
const { value } = res;
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
