# State 共享状态 API

`state` 命名空间的方法：跨窗口通信中枢（`PortHub`）持有的共享键值状态，支持 TTL。

## State API

### state.get

<!-- api-schema:begin state.get -->
读取所有窗口共享的状态里的一个值。会先移除过期的值，每个都以原因为 `expired` 的 `state:deleted` 事件通告。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `key` | `string` | 是 | 键。不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `exists` | `boolean` | 键是否存在。 |
| `value` | `any` | 值；键不存在时为 `null`。 |
| `key` | `string` | 键；存在时出现。 |
| `expiresAt` | `integer` | 值的过期时间，自 Unix 纪元起的毫秒数；用 `ttlMs` 存的值才有。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const result = await fb2k.invoke('state.get', { key: 'lyrics:offset' });
if (result.success === false) throw new Error(result.error);
if (result.exists) console.log('偏移:', result.value);
```

### state.set

<!-- api-schema:begin state.set -->
在共享状态里存一个值，并以 `state:changed` 事件通告，除非 `silent`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `key` | `string` | 是 | 键。不能为空。 |
| `value` | `any` | 是 | 要存的值。`null` 按没传处理；移除键用 `state.delete`。 |
| `silent` | `boolean` | 否 | 存值但不通告 `state:changed`。默认 `false`。 |
| `ttlMs` | `integer` | 否 | 存活时长，单位毫秒，过后值被移除；`0` 或负数表示不过期。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `expiresAt` | `integer` | 值的过期时间，自 Unix 纪元起的毫秒数；`ttlMs` 为正数时出现。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
await fb2k.invoke('state.set', {
    key: 'lyrics:offset',
    value: 120,
    ttlMs: 60000
});
```

### state.delete

<!-- api-schema:begin state.delete -->
从共享状态里移除一个值；该值存在时以原因为 `deleted` 的 `state:deleted` 事件通告。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `key` | `string` | 是 | 键。不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `existed` | `boolean` | 调用前键是否存在。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
await fb2k.invoke('state.delete', { key: 'lyrics:offset' });
```

### state.keys

<!-- api-schema:begin state.keys -->
列出共享状态的键。与 `state.get` 一样，会先移除过期的值。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `pattern` | `string` | 否 | `*` 表示全部键，前缀加 `*`（例如 `lyrics:*`）表示该前缀下的键，否则按完整键精确匹配。默认 `"*"`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `keys` | `string[]` | 匹配的键，不保证顺序。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const result = await fb2k.invoke('state.keys', { pattern: 'lyrics:*' });
if (result.success === false) throw new Error(result.error);
console.log('状态键:', result.keys);
```

## 状态行为

- `state.*` 是进程级 `PortHub` 单例持有的内存状态。在当前 foobar2000 进程中，本组件的多个 WebView 窗口可共享它；但它不会写入磁盘、进程重启后丢失，也不是跨进程或 SMP/全局持久化机制。它与 SDK 的 `fb.state` 播放状态镜像不同。
- 键不存在时，`state.get` 返回 `exists: false` 与 `value: null`。`state.set` 同时要求 `key` 和 `value`；正数 `ttlMs` 会创建过期时间戳，`silent: true` 会抑制 `state:changed`。
- `state.delete` 返回 `existed`。显式删除会发出 `reason: "deleted"` 的 `state:deleted`；过期会发出相同事件，但 `reason` 为 `"expired"`，且 `sourceWindowId` 为空。
- 全部公开 PortHub 事件及其字段见 [事件列表（PortHub）](./port.md#事件列表-porthub)。
