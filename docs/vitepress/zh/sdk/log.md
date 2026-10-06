# fb.log 日志文件

`fb.log` 写入、读取和清空 SDK 宿主的日志文件，即 foobar2000 配置目录里的 `webview_ui.log`。要把消息写到 foobar2000 控制台窗口，用 `fb.console`。

## clear()

签名：`fb.log.clear(): Promise<LogClearResponse>`

清空 `webview_ui.log`。用 `write()` 的 `options.file` 指定的文件不受影响。

```javascript
const result = await fb.log.clear();
```

## read(lines?)

签名：`fb.log.read(lines?: number): Promise<LogReadResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `lines` | `number` | 否 | 最多返回的行数，宿主默认 100。 |

读取 `webview_ui.log` 末尾的 `lines` 行。响应含 `content`（各行以 `\n` 连接）、`lines`、`lineCount`，以及整个文件的行数 `totalLines`。文件还不存在时按空文件返回，此时没有 `lines` 与 `totalLines`。

```javascript
const res = await fb.log.read(100); // 最近 100 行
if (res.success === false) throw new Error(res.error);
const { lines } = res;
```

## write(message, options?)

签名：`fb.log.write(message: string, options?: LogWriteParams): Promise<LogWriteResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `message` | `string` | 是 | 要写的文本。空串以 `INVALID_PARAMS` 失败。 |
| `options.level` | `string` | 否 | 日志级别，转成大写后写在消息前的方括号里；默认 `'info'`。 |
| `options.append` | `boolean` | 否 | 为 `true` 时追加；为 `false` 时用这一行替换文件内容。默认 `true`。 |
| `options.timestamp` | `boolean` | 否 | 在行首加上本地时间，形如 `[YYYY-MM-DD HH:MM:SS.mmm]`；默认 `true`。 |
| `options.file` | `string` | 否 | 配置目录里的另一个文件：单纯的文件名，以 `.log` 或 `.txt` 结尾，且不是 `CON` 这类 Windows 设备名。其他值一律忽略，这一行写进 `webview_ui.log`。 |
| `options.args` | `JsonValue[]` | 否 | 在这里不起作用：宿主只在没有 `message` 时才读 `args`，而 `fb.log.write` 总会发出 `message`。 |

响应里的 `path` 是实际写入的文件的完整路径。

```javascript
const result = await fb.log.write('播放开始', {
	level: 'info',
	timestamp: true,
});
```
