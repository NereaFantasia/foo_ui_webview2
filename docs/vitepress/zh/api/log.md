# Log 日志文件 API

`log` 命名空间的方法。

## Log API - 日志文件 (3 个 API)

### log.write

<!-- api-schema:begin log.write -->
向 foobar2000 配置目录里的日志文件写一行：默认是 `webview_ui.log`，也可以用 `file` 指定。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `message` | `any` | 否 | 要写的内容。字符串原样写出，其他 JSON 值写成 JSON 文本。与 `args` 同时给出时以它为准。 |
| `args` | `any[]` | 否 | 要写的若干值，以空格分隔：字符串原样写出，其他值写成 JSON 文本。仅在没有 `message` 时使用。 |
| `level` | `string` | 否 | 写在消息前方括号里的级别，会转成大写。默认 `"info"`。 |
| `append` | `boolean` | 否 | 追加到文件末尾；为 `false` 时用这一行替换文件内容。默认 `true`。 |
| `timestamp` | `boolean` | 否 | 在行首加上本地时间，形如 `[YYYY-MM-DD HH:MM:SS.mmm]`。默认 `true`。 |
| `file` | `string` | 否 | 配置目录里另一个文件的名字。必须是单纯的文件名、以 `.log` 或 `.txt` 结尾，且不是 `CON` 这类 Windows 设备名；否则忽略，这一行写进 `webview_ui.log`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `path` | `string` | 实际写入的文件的完整路径。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

需提供 `message` 或 `args` 之一；两者都没有时以 `INVALID_PARAMS`（`message is required`）失败。

```javascript
await fb2k.invoke('log.write', {
    message: '播放开始',
    level: 'info',
    file: 'playback.log'
});
```

### log.read

<!-- api-schema:begin log.read -->
读取 `webview_ui.log` 末尾的若干行。文件还不存在时按空文件返回。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `lines` | `integer` | 否 | 从文件末尾返回多少行。不小于 `0`。默认 `100`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `content` | `string` | 返回的各行以 `\n` 连接。 |
| `lines` | `string[]` | 返回的各行。文件不存在时不出现。 |
| `lineCount` | `integer` | 返回的行数。 |
| `totalLines` | `integer` | 整个文件的行数。文件不存在时不出现。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```json
{
    "success": true,
    "content": "...",
    "lines": ["[2026-02-10 12:00:00.123][INFO] ..."],
    "lineCount": 50,
    "totalLines": 200
}
```

### log.clear

<!-- api-schema:begin log.clear -->
清空 `webview_ui.log`。

无参数。

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

## 契约补充

以下补充这些方法的完整参数契约，不改变前文的已有说明。

<!-- phase3-supplement:log.write -->
### Contract 补充：`log.write`

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `message` | `string` | 否 | 可省略 | 日志文本；非字符串会序列化。与 `args` 至少提供其一，否则返回 `message is required`。 |
| `args` | `array` | 否 | `[]` | 当省略 `message` 时，用空格拼接的参数列表。 |
| `file` | `string` | 否 | 可省略 | 配置目录下的目标文件名；默认 `webview_ui.log`。 |
| `level` | `string` | 否 | `info` | 级别文本，转为大写后写入行前缀。 |
| `append` | `boolean` | 否 | `true` | `true` 追加写入，`false` 覆盖重写。 |
| `timestamp` | `boolean` | 否 | `true` | 是否在行首写入时间戳。 |

#### 返回字段

| 字段 | 类型 | 可选 |
| --- | --- | --- |
| `error` | `string` | 是 |
| `success` | `boolean` | 否 |
| `path` | `json` | 否 |

```js
await fb2k.invoke('log.write', { message: '播放开始' });
```
