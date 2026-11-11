# fb.sharedState 跨窗口共享状态

`fb.sharedState` 提供跨窗口键值存储。它不同于同步的 `fb.state` 播放状态镜像：共享状态值可设置 TTL，并通过 `state:*` 事件族发布变化。

该存储由组件 `PortHub` 单例在进程内存中维护。在当前 foobar2000 进程生命周期内，本组件创建的所有 WebView 窗口均可共享该状态。数据仅保存在内存中，进程退出后丢失，且不与其他进程或 SMP 脚本运行时共享。

## delete(key)

签名：`fb.sharedState.delete(key: string): Promise<StateDeleteResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `key` | `string` | 是 | 要移除的键 |

返回对象中的 `existed` 表示删除前该键是否存在；若删除已存在的键，将广播 `state:deleted` 事件并携带 `reason: 'deleted'`。

```javascript
const result = await fb.sharedState.delete('playlist:active-filter');
```

## keys(pattern?)

签名：`fb.sharedState.keys(pattern?: string): Promise<StateKeysResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `pattern` | `string` | 否 | 键的模式，默认 `'*'` |

返回匹配的键列表 `{ keys }`，元素顺序未作保证。`'*'` 匹配所有键；以 `*` 结尾的模式按前缀匹配（如 `'playlist:*'`）；其余模式执行全键精确匹配，中间出现的 `*` 视为普通字符。查询前自动清理已过期的键，每个过期被清理的键均触发一次 `reason` 为 `'expired'` 的 `state:deleted` 事件。

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
| `value` | `unknown` | 是 | 可序列化为 JSON 的数据值；顶层传入 `null` 将被拒绝，移除键请使用 `delete()` |
| `silent` | `boolean` | 否 | 为 `true` 时不发出变更事件，默认 `false` |
| `ttlMs` | `number` | 否 | 存活时间（毫秒）；若省略或小于等于 0，则该键永不过期 |

写入完成后广播 `state:changed` 事件（`silent` 为 `true` 时静默写入不发送事件），载荷包含 `key`、`value`、`previousValue`（键首次创建时为 `null`）与 `sourceWindowId`。若指定了正数 `ttlMs`，方法返回值及变更事件载荷中均包含过期时间戳 `expiresAt`（Unix 时间戳，毫秒）。

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
