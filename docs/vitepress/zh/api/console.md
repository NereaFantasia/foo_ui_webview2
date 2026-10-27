# Console 控制台 API

提供向 foobar2000 宿主系统控制台重定向输出信息、警告与错误日志能力。

## Console API - 控制台 (3 个 API)

### console.log

<!-- api-schema:begin console.log -->
向 foobar2000 系统控制台输出普通日志信息（前缀为 [WebView]）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `message` | `any` | 否 | 待输出的控制台正文内容（字符串原样输出，其余类型序列化为 JSON 文本；与 args 同时提供时优先采用） |
| `args` | `any[]` | 否 | 待输出的参数值列表（以空格连接输出；字符串原样输出，其余类型序列化为 JSON 文本；仅在缺省 message 时使用） |

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
向 foobar2000 系统控制台输出警告日志信息（前缀为 [WebView][WARN]）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `message` | `any` | 否 | 待输出的控制台正文内容（字符串原样输出，其余类型序列化为 JSON 文本；与 args 同时提供时优先采用） |
| `args` | `any[]` | 否 | 待输出的参数值列表（以空格连接输出；字符串原样输出，其余类型序列化为 JSON 文本；仅在缺省 message 时使用） |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

需提供 `message` 或 `args` 之一；两者都没有时以 `INVALID_PARAMS`（`message is required`）失败。

### console.error

<!-- api-schema:begin console.error -->
向 foobar2000 系统控制台输出错误日志信息（前缀为 [WebView][ERROR]）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `message` | `any` | 否 | 待输出的控制台正文内容（字符串原样输出，其余类型序列化为 JSON 文本；与 args 同时提供时优先采用） |
| `args` | `any[]` | 否 | 待输出的参数值列表（以空格连接输出；字符串原样输出，其余类型序列化为 JSON 文本；仅在缺省 message 时使用） |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

需提供 `message` 或 `args` 之一；两者都没有时以 `INVALID_PARAMS`（`message is required`）失败。
