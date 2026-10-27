# Log 日志文件 API

提供 foobar2000 配置目录日志文件的格式化写入、尾部按行回读与清空能力。

## Log API - 日志文件 (3 个 API)

### log.write

<!-- api-schema:begin log.write -->
向 foobar2000 配置目录内的日志文件写入一行日志（默认写入 webview_ui.log，支持通过 file 指定自定义文件名）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `message` | `any` | 否 | 待写入的日志正文内容（字符串原样输出，对象或数组序列化为 JSON 文本；若与 args 同时提供则以本字段为准） |
| `args` | `any[]` | 否 | 待写入的参数值列表（以空格连接输出；字符串原样输出，其他类型序列化为 JSON 文本；仅在缺省 message 时生效） |
| `level` | `string` | 否 | 日志记录级别（自动转为大写并置于行首方括号内）默认 `"info"`。 |
| `append` | `boolean` | 否 | 是否以追加模式写入（默认 true；为 false 时清空并替换文件内容）默认 `true`。 |
| `timestamp` | `boolean` | 否 | 是否在行首添加本地时间戳（格式为 [YYYY-MM-DD HH:MM:SS.mmm]）默认 `true`。 |
| `file` | `string` | 否 | 配置目录内的自定义日志文件名（须为以 .log 或 .txt 结尾的合法纯文件名，且不可使用 CON 等 Windows 保留设备名；不合法时将自动写入 webview_ui.log） |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `path` | `string` | 实际写入的日志文件完整绝对路径 |

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
读取 webview_ui.log 文件末尾的指定行数（文件不存在时返回空内容）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `lines` | `integer` | 否 | 从日志文件末尾逆向读取的最大行数不小于 `0`。默认 `100`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `content` | `string` | 读取的各行日志内容（以换行符 \n 拼接） |
| `lines` | `string[]` | 读取的各行日志字符串数组（文件不存在时缺省） |
| `lineCount` | `integer` | 实际读取返回的日志行数 |
| `totalLines` | `integer` | 日志文件包含的总行数（文件不存在时缺省） |

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
清空 webview_ui.log 日志文件内容

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
