# Shell API

Methods of the `shell` namespace.

## shell

### shell.exec

<!-- api-schema:begin shell.exec -->
Start a process from a command line and return at once; nothing is waited for. Commands are not allow-listed: a theme is trusted like an installed component. `cwd` goes through path security.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `command` | `string` | Yes | Command line, run as given. Must not be empty. |
| `args` | `string[]` | No | Arguments appended to the command line, each quoted. |
| `cwd` | `string` | No | Working directory; the process inherits foobar2000's when omitted or empty. |
| `hidden` | `boolean` | No | Start the process without a visible window. Default: `true`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `processId` | `integer` | Windows process id of the started process. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('shell.exec', {
	command: 'ffprobe',
	args: ['-hide_banner', 'C:\\Music\\song.flac'],
});
```

### shell.openExternal

<!-- api-schema:begin shell.openExternal -->
Open an `http://`, `https://` or `mailto:` URL with the default handler.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `url` | `string` | Yes | URL to open; any other scheme fails with `INVALID_PARAMS`. Must not be empty. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('shell.openExternal', {
	url: 'https://www.foobar2000.org/',
});
```

### shell.openWith

<!-- api-schema:begin shell.openWith -->
Open a file with the application Windows associates with its type. Executables, scripts, installers, shortcuts and libraries are refused with `PERMISSION_DENIED`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | File to hand to its associated application. Must not be empty. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('shell.openWith', { path: 'C:\\Music\\cover.jpg' });
```

### shell.showInExplorer

<!-- api-schema:begin shell.showInExplorer -->
Reveal a file or folder in Windows Explorer, selecting it.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | File or folder to reveal. Must not be empty. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('shell.showInExplorer', { path: 'C:\\Music\\song.flac' });
```

### shell.spawn

<!-- api-schema:begin shell.spawn -->
Start a process from an executable and an argument list. With `waitForExitMs` the call waits that long for an early exit; a non-zero exit within the wait fails with `OPERATION_FAILED` and the failure carries `processId`, `exited` and `exitCode`. An absolute executable path and `cwd` go through path security.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `executable` | `string` | Yes | Executable name (resolved through PATH) or path. Surrounding whitespace and quotes are removed. Must not be empty. |
| `args` | `string[]` | No | Arguments passed to the process, each quoted. |
| `cwd` | `string` | No | Working directory, which has to exist; the process inherits foobar2000's when omitted or empty. |
| `hidden` | `boolean` | No | Start the process without a visible window. Default: `true`. |
| `waitForExitMs` | `integer` | No | Milliseconds to wait for an early exit; `0` returns as soon as the process starts. At least `0`. Default: `0`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `processId` | `integer` | Windows process id of the started process. |
| `exited` | `boolean` | Whether the process exited within `waitForExitMs`; absent when no wait was requested. |
| `exitCode` | `integer` | The exit code, present when `exited` is `true`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('shell.spawn', {
	executable: 'ffprobe.exe',
	args: ['C:\\Music\\song.flac'],
	waitForExitMs: 5000,
});
if (res.success === false) throw new Error(res.error);
const { exited, exitCode } = res;
```

## Shell boundaries

- `shell.openExternal` accepts only `http://`, `https://`, or `mailto:` URLs. `shell.openWith` rejects executable, script, installer, shortcut, library, and related dangerous extensions.
- `shell.exec` and `shell.spawn` intentionally do not impose a command allowlist. Their `cwd` and any absolute executable path are validated; `shell.spawn.waitForExitMs` optionally reports early process exit.
