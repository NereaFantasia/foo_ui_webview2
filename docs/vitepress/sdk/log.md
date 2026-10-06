# fb.log Log File

`fb.log` writes, reads, and clears the SDK host log file, `webview_ui.log` in the foobar2000 profile directory. For messages sent to the foobar2000 console window instead, use `fb.console`.

## clear()

Signature: `fb.log.clear(): Promise<LogClearResponse>`

Empties `webview_ui.log`. A file chosen with `write()`'s `options.file` is not touched.

```javascript
const result = await fb.log.clear();
```

## read(lines?)

Signature: `fb.log.read(lines?: number): Promise<LogReadResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `lines` | `number` | No | Maximum number of lines; the host defaults to 100. |

Reads the last `lines` lines of `webview_ui.log`. The response carries `content` (the lines joined with `\n`), `lines`, `lineCount`, and `totalLines`, the line count of the whole file. A file that does not exist yet reads as empty, and then `lines` and `totalLines` are absent.

```javascript
const res = await fb.log.read(100);
if (res.success === false) throw new Error(res.error);
const { lines } = res;
```

## write(message, options?)

Signature: `fb.log.write(message: string, options?: LogWriteParams): Promise<LogWriteResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `message` | `string` | Yes | Text to write. An empty string fails with `INVALID_PARAMS`. |
| `options.level` | `string` | No | Log level, written upper-cased in brackets before the message; defaults to `'info'`. |
| `options.append` | `boolean` | No | Appends when `true`; `false` replaces the file's content with this line. Defaults to `true`. |
| `options.timestamp` | `boolean` | No | Prefixes the line with the local time as `[YYYY-MM-DD HH:MM:SS.mmm]`; defaults to `true`. |
| `options.file` | `string` | No | Another file in the profile directory: a plain file name ending in `.log` or `.txt` that is not a Windows device name such as `CON`. Any other value is ignored and the line goes to `webview_ui.log`. |
| `options.args` | `JsonValue[]` | No | Ignored here: the host reads `args` only when `message` is absent, and `fb.log.write` always sends `message`. |

The response's `path` is the full path of the file that was written.

```javascript
const result = await fb.log.write('Playback started', {
	level: 'info',
	timestamp: true,
});
```
