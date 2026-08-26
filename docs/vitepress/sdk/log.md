# fb.log Log File

`fb.log` writes, reads, and clears the SDK host log file. For messages sent to the foobar2000 console window instead, use `fb.console`.

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## Additional methods

> This block maintains SDK-facing method coverage and may be expanded with complete examples and best practices.

### clear()

Signature: `fb.log.clear(): Promise<BaseResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| None | — | — | This method takes no arguments. |

Returns the `log.clear` response envelope.

```javascript
const result = await fb.log.clear();
```

### read()

Signature: `fb.log.read(lines?: number): Promise<{ lines: string[] }>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `lines` | `number` | No | Maximum number of lines; the host defaults to 100. |

Returns the requested log text in `{ lines }`.

```javascript
const { lines } = await fb.log.read(100);
```

### write()

Signature: `fb.log.write(message: string, options?: LogWriteParams): Promise<BaseResponse & { path?: string }>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `message` | `string` | Yes | Text to write. |
| `options.level` | `string` | No | Log level; defaults to `'info'`. |
| `options.append` | `boolean` | No | Appends when `true`; defaults to `true`. |
| `options.timestamp` | `boolean` | No | Adds a timestamp when `true`; defaults to `true`. |
| `options.file` | `string` | No | Optional log filename. |
| `options.args` | `string[]` | No | Additional string arguments accepted by the host contract. |

Returns the write response envelope and may include the resolved log-file `path`.

```javascript
const result = await fb.log.write('Playback started', {
	level: 'info',
	timestamp: true,
});
```

<!-- END AUTO-GENERATED SDK STUBS -->
