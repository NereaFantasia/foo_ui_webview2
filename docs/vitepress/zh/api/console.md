# Console 控制台 API

`console` 命名空间的方法。

## Console API - 控制台 (3 个 API)

### console.log

<!-- api-schema:begin console.log -->
向 foobar2000 控制台写一行，前缀为 `[WebView]`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `message` | `any` | 否 | 要写的内容。字符串原样写出，其他 JSON 值写成 JSON 文本。与 `args` 同时给出时以它为准。 |
| `args` | `any[]` | 否 | 要写的若干值，以空格分隔：字符串原样写出，其他值写成 JSON 文本。仅在没有 `message` 时使用。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

需提供 `message` 或 `args` 之一；两者都没有时以 `INVALID_PARAMS`（`message is required`）失败。

```javascript
await fb2k.invoke('console.log', { message: '普通日志' });
await fb2k.invoke('console.log', { args: ['用户:', userName, '播放次数:', 42] });
```

### console.warn

<!-- api-schema:begin console.warn -->
向 foobar2000 控制台写一行警告，前缀为 `[WebView][WARN]`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `message` | `any` | 否 | 要写的内容。字符串原样写出，其他 JSON 值写成 JSON 文本。与 `args` 同时给出时以它为准。 |
| `args` | `any[]` | 否 | 要写的若干值，以空格分隔：字符串原样写出，其他值写成 JSON 文本。仅在没有 `message` 时使用。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

需提供 `message` 或 `args` 之一；两者都没有时以 `INVALID_PARAMS`（`message is required`）失败。

### console.error

<!-- api-schema:begin console.error -->
向 foobar2000 控制台写一行错误，前缀为 `[WebView][ERROR]`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `message` | `any` | 否 | 要写的内容。字符串原样写出，其他 JSON 值写成 JSON 文本。与 `args` 同时给出时以它为准。 |
| `args` | `any[]` | 否 | 要写的若干值，以空格分隔：字符串原样写出，其他值写成 JSON 文本。仅在没有 `message` 时使用。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

需提供 `message` 或 `args` 之一；两者都没有时以 `INVALID_PARAMS`（`message is required`）失败。
