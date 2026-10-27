# State 共享状态 API

管理跨窗口通信中枢（`PortHub`）持有的内存共享键值状态，支持 TTL 过期与变更广播。

## State API

### state.get

<!-- api-schema:begin state.get -->
读取所有窗口共享状态中的指定键值；读取前先行清理已过期键，并针对每个清理项广播 `reason` 为 `expired` 的 `state:deleted` 事件

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `key` | `string` | 是 | 待读取的存储键名不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `exists` | `boolean` | 目标键是否存在于共享状态中 |
| `value` | `any` | 存储的键值；目标键不存在时为 `null` |
| `key` | `string` | 目标键名；仅在键存在时返回 |
| `expiresAt` | `integer` | 键值的过期时间戳（自 Unix 纪元起的毫秒数）；仅在写入时指定了 `ttlMs` 且尚未过期时返回 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const result = await fb2k.invoke('state.get', { key: 'lyrics:offset' });
if (result.success === false) throw new Error(result.error);
if (result.exists) console.log('偏移:', result.value);
```

### state.set

<!-- api-schema:begin state.set -->
在跨窗口共享状态中存入指定键值；默认广播 `state:changed` 事件通告所有窗口，传入 `silent: true` 时静默写入

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `key` | `string` | 是 | 待存储的键名不能为空。 |
| `value` | `any` | 是 | 待存储的键值；`null` 会被视作参数缺失报错拒绝，如需清除键请调用 `state.delete` |
| `silent` | `boolean` | 否 | 是否静默写入而不广播 `state:changed` 事件默认 `false`。 |
| `ttlMs` | `integer` | 否 | 键值存活时长（毫秒），超时后将被自动移除；`0` 或负数表示永久保存不过期 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `expiresAt` | `integer` | 键值计算得出的过期时间戳（自 Unix 纪元起的毫秒数）；仅在传入正数 `ttlMs` 时返回 |

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
从跨窗口共享状态中移除指定键；若目标键此前存在，广播 `reason` 为 `deleted` 的 `state:deleted` 事件通告所有窗口

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `key` | `string` | 是 | 待删除的存储键名不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `existed` | `boolean` | 本次删除操作前该键是否存在于共享状态中 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
await fb2k.invoke('state.delete', { key: 'lyrics:offset' });
```

### state.keys

<!-- api-schema:begin state.keys -->
列出跨窗口共享状态中所有匹配的键名；与 `state.get` 相同，返回前先行清理已过期键

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `pattern` | `string` | 否 | 匹配模式：`*` 匹配所有键，`前缀:*`（如 `lyrics:*`）匹配指定前缀，不带通配符时按完整键名精确匹配默认 `"*"`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `keys` | `string[]` | 匹配到的所有键名列表，不保证特定顺序 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const result = await fb2k.invoke('state.keys', { pattern: 'lyrics:*' });
if (result.success === false) throw new Error(result.error);
console.log('状态键:', result.keys);
```

## 状态行为

- 键名是普通的不透明字符串，示例中的 `lyrics:offset` 或 `lyrics:theme` 为常见应用键名，并非运行时保留状态。
- `state.*` 由进程内 `PortHub` 单例统一维护在内存中。当前 foobar2000 进程内的多个 WebView 窗口可实时共享访问；数据不会写入磁盘且随进程退出清空，既非跨进程机制，亦非全局/SMP 持久化存储。本模块独立于 SDK 内的 `fb.state` 播放状态镜像。
- 目标键不存在时，`state.get` 返回 `{ exists: false, value: null }`。`state.set` 必须同时传入 `key` 与 `value`；传入正数 `ttlMs` 会计算过期时间戳，传入 `silent: true` 会抑制广播 `state:changed`。
- `state.delete` 返回 `existed` 反映删除前状态。显式删除成功后广播 `reason: "deleted"` 的 `state:deleted`；超时过期清理时广播相同事件，但 `reason` 标记为 `"expired"` 且 `sourceWindowId` 为空字符串。
- 完整的 PortHub 公开事件及字段说明详见 [事件列表（PortHub）](./port.md#事件列表-porthub)。
