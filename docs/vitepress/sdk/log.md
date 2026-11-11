# fb.log Log File

`fb.log` provides file logging operations (write, read, and clear) targeting the host log file `webview_ui.log` in the foobar2000 profile directory. To send messages to the foobar2000 console window instead, use `fb.console`.

## clear()

Signature: `fb.log.clear(): Promise<LogClearResponse>`

Clears `webview_ui.log`. Custom files written through `options.file` of `write()` remain intact.

```javascript
const result = await fb.log.clear();
```

## read(lines?)

Signature: `fb.log.read(lines?: number): Promise<LogReadResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `lines` | `number` | No | Maximum number of trailing lines to return; defaults to 100 |

Reads the trailing `lines` lines of `webview_ui.log`. The response contains `content` (concatenated lines joined with `\n`), `lines` array, `lineCount`, and `totalLines` (total line count of the entire file). If the log file does not exist, it is treated as empty, and `lines` and `totalLines` are omitted from the response.

```javascript
const res = await fb.log.read(100);
if (res.success === false) throw new Error(res.error);
const { lines } = res;
```

## write(message, options?)

Signature: `fb.log.write(message: string, options?: LogWriteParams): Promise<LogWriteResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `message` | `string` | Yes | Text payload to write. Passing an empty string fails with `INVALID_PARAMS` |
| `options.level` | `string` | No | Log level identifier, formatted uppercase in square brackets preceding the message; defaults to `'info'` |
| `options.append` | `boolean` | No | Write mode; appends when `true`, overwrites the entire file content when `false`. Defaults to `true` |
| `options.timestamp` | `boolean` | No | Timestamp toggle; prefixes the entry with local timestamp formatted as `[YYYY-MM-DD HH:MM:SS.mmm]`. Defaults to `true` |
| `options.file` | `string` | No | Alternative target file within the profile directory: must be a bare filename ending with `.log` or `.txt`, excluding reserved Windows device names (such as `CON`, `PRN`, `NUL`). Any non-conforming value is ignored and falls back to writing to `webview_ui.log` |
| `options.args` | `JsonValue[]` | No | Reserved parameter: the host parser examines `args` only when `message` is omitted; because `fb.log.write` always provides `message`, this parameter is ignored |

The `path` field in the response contains the resolved absolute path of the target log file.

```javascript
const result = await fb.log.write('Playback started', {
	level: 'info',
	timestamp: true,
});
```
