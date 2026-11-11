# fb.log 日志文件

`fb.log` 提供宿主文件日志的写入、读取与清空操作，默认文件位于 foobar2000 配置目录下的 `webview_ui.log`。若需输出日志至 foobar2000 宿主控制台窗口，请使用 `fb.console`。

## clear()

签名：`fb.log.clear(): Promise<LogClearResponse>`

清空主日志文件 `webview_ui.log`。通过 `write()` 中 `options.file` 独立输出的自定义日志文件不受影响。

```javascript
const result = await fb.log.clear();
```

## read(lines?)

签名：`fb.log.read(lines?: number): Promise<LogReadResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `lines` | `number` | 否 | 最大返回行数，缺省为 100 |

读取 `webview_ui.log` 末尾的 `lines` 行。响应包含 `content`（以 `\n` 拼接的完整文本）、`lines` 行数组、`lineCount` 以及整个文件的总行数 `totalLines`。若目标文件尚未创建，将按空文件处理返回，此时响应对象中不包含 `lines` 与 `totalLines` 字段。

```javascript
const res = await fb.log.read(100); // 最近 100 行
if (res.success === false) throw new Error(res.error);
const { lines } = res;
```

## write(message, options?)

签名：`fb.log.write(message: string, options?: LogWriteParams): Promise<LogWriteResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `message` | `string` | 是 | 待写入文本内容。空字符串将以 `INVALID_PARAMS` 错误拒绝 |
| `options.level` | `string` | 否 | 日志级别标识，格式化为大写并以方括号包裹追加于消息前；缺省为 `'info'` |
| `options.append` | `boolean` | 否 | 写入模式；为 `true` 时执行追加写入，为 `false` 时覆盖文件全部内容。缺省为 `true` |
| `options.timestamp` | `boolean` | 否 | 时间戳开关；启用时在行首添加本地时间戳 `[YYYY-MM-DD HH:MM:SS.mmm]`。缺省为 `true` |
| `options.file` | `string` | 否 | 自定义输出文件名，仅限 foobar2000 配置目录内的直接文件：须以 `.log` 或 `.txt` 为扩展名，且不可为 Windows 保留设备名称（如 `CON`、`PRN`、`NUL` 等）。非法或不符合规则的值将被忽略并回退写入至默认 `webview_ui.log` |
| `options.args` | `JsonValue[]` | 否 | 保留参数：C++ 宿主仅在缺少 `message` 字段时解析 `args` 数组；SDK 封装始终传递 `message`，此参数不生效 |

响应中的 `path` 字段为实际写入文件的绝对路径。

```javascript
const result = await fb.log.write('播放开始', {
	level: 'info',
	timestamp: true,
});
```
