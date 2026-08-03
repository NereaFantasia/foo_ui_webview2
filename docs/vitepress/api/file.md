# File API

English API reference for the `dialog`, `file`, `shell` family.

This page is the primary owner for the namespaces listed below. Method names, parameter keys, and return fields follow the C++ `RegisterApi` handlers.

## dialog

### dialog.confirm


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `buttons` | `array` | No | Optional; omitted by default. |
| `defaultButton` | `integer` | No | Optional; default 0. |
| `message` | `string` | No | Optional. |
| `title` | `string` | No | Optional; default Confirm. |
| `type` | `string` | No | Optional; default question. |

**Returns**: `{"response":"..."}`

```js
const { response } = await fb2k.invoke('dialog.confirm', {
	title: 'Remove Track',
	message: 'Remove this track from the playlist?',
});
```

### dialog.openFile


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `defaultPath` | `string` | No | Optional; default empty. Supports `%music%` expansion. |
| `filters` | `array` | No | Optional filter specs `{ name, extensions[] }` parsed by `ParseFilterSpecs`. |
| `multiple` | `boolean` | No | Optional; default false. |
| `title` | `string` | No | Optional; default Open File. |

**Returns**: `{"canceled":"...","error":"...","filePaths":"..."}`

```js
const { canceled, filePaths } = await fb2k.invoke('dialog.openFile', {
	title: 'Add Audio Files',
	multiple: true,
	filters: [{ name: 'Audio', extensions: ['flac', 'mp3', 'm4a'] }],
});
```

### dialog.openFolder


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `title` | `string` | No | Optional; default Select Folder. |

**Returns**: `{"canceled":"...","error":"...","folderPath":"..."}`

```js
const { canceled, folderPath } = await fb2k.invoke('dialog.openFolder', {
	title: 'Choose Music Folder',
});
```

### dialog.saveFile


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `defaultName` | `string` | No | Optional; default empty. |
| `filters` | `array` | No | Optional filter specs `{ name, extensions[] }` parsed by `ParseFilterSpecs`. |
| `title` | `string` | No | Optional; default Save File. |

**Returns**: `{"canceled":"...","error":"...","filePath":"..."}`

```js
const { canceled, filePath } = await fb2k.invoke('dialog.saveFile', {
	defaultName: 'export.m3u8',
	filters: [{ name: 'Playlist', extensions: ['m3u8'] }],
});
```

## file

### file.copy


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `destination` | `string` | Yes | Destination path. Missing parent directories are created by the copy. |
| `overwrite` | `boolean` | No | Optional; default false. Existing destinations are skipped when false. |
| `source` | `string` | Yes | Source file or directory path. |

**Returns**: `{"destination":"...","error":"...","source":"...","success":true}`

```js
await fb2k.invoke('file.copy', {
	source: 'C:\\Music\\song.flac',
	destination: '%profile%\\backup\\song.flac',
});
```

### file.delete


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `moveToTrash` | `boolean` | No | Optional; default true. Set false for a permanent delete. |
| `path` | `string` | Yes | Path of the file to delete. |

**Returns**: `{"error":"...","success":true}`

```js
await fb2k.invoke('file.delete', { path: '%profile%\\cache\\stale.json' });
```

### file.exists


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Path to test. |

**Returns**: `{"error":"...","exists":"...","isDirectory":"...","isFile":"...","success":true}`

```js
const { exists, isFile } = await fb2k.invoke('file.exists', {
	path: 'C:\\Music\\song.flac',
});
```

### file.getInfo


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Path to inspect. |

**Returns**: `{"error":"...","exists":"...","extension":"...","isDirectory":"...","isFile":"...","modified":"...","name":"...","parent":"...","size":"...","success":true}`

```js
const info = await fb2k.invoke('file.getInfo', {
	path: 'C:\\Music\\song.flac',
});
```

### file.list


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Directory to enumerate. |
| `pattern` | `string` | No | Optional; default *. Also accepts a single extension glob such as `*.flac`. |
| `recursive` | `boolean` | No | Optional; default false. |

**Returns**: `{"directories":"...","error":"...","files":"...","items":"...","success":true}`

```js
const { files } = await fb2k.invoke('file.list', {
	path: 'C:\\Music',
	pattern: '*.flac',
});
```

### file.mkdir


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Directory to create. Intermediate directories are created as needed. |

**Returns**: `{"created":"...","error":"...","message":"...","success":true}`

```js
await fb2k.invoke('file.mkdir', { path: '%profile%\\my-panel\\cache' });
```

### file.move


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `destination` | `string` | Yes | Destination path. |
| `source` | `string` | Yes | Source file or directory path. |

**Returns**: `{"destination":"...","error":"...","source":"...","success":true}`

```js
await fb2k.invoke('file.move', {
	source: '%profile%\\inbox\\song.flac',
	destination: 'C:\\Music\\song.flac',
});
```

### file.read


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `encoding` | `string` | No | Optional; default utf-8. Pass `binary` for a Base64 payload. |
| `path` | `string` | Yes | Path of the file to read. |

**Returns**: `{"content":"...","encoding":"...","error":"...","size":"...","success":true}`

```js
const { content } = await fb2k.invoke('file.read', {
	path: '%profile%\\my-panel\\settings.json',
});
```

When `encoding: 'binary'`, `content` is a **raw Base64 payload** without a
`base64:` prefix and the response sets `encoding: 'base64'`. This is a
transport representation, not text and not a Data URL. To write the bytes
back, add the `base64:` prefix required by `file.write` and keep
`encoding: 'binary'`.

### file.rename


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `newName` | `string` | Yes | New file name only; path separators are rejected. |
| `path` | `string` | Yes | Path of the existing file or directory. |

**Returns**: `{"error":"...","newPath":"...","oldPath":"...","success":true}`

```js
await fb2k.invoke('file.rename', {
	path: 'C:\\Music\\track01.flac',
	newName: '01 - Intro.flac',
});
```

### file.write


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `append` | `boolean` | No | Optional; default false. When false the file is truncated. |
| `content` | `string` | No | Optional; defaults to an empty string, which truncates the file. |
| `encoding` | `string` | No | Optional; default utf-8. Pass `binary` together with a `base64:` prefixed `content`. |
| `path` | `string` | Yes | Destination path. Missing parent directories are created. |

**Returns**: `{"bytesWritten":"...","error":"...","success":true}`

```js
await fb2k.invoke('file.write', {
	path: '%profile%\\my-panel\\settings.json',
	content: JSON.stringify({ theme: 'dark' }),
});
```

For binary writes, decoding happens only when **both** conditions are true:
`encoding` is exactly `'binary'` and `content` starts with `base64:`. The
prefix is a Bridge wire marker and is removed before decoding. A raw Base64
string, a `data:image/...;base64,...` Data URL, or a `fb2k://` artwork URL is
not decoded by this branch; such input can still produce `success: true` while
writing the wrong bytes.

```js
const binary = await fb2k.invoke('file.read', {
	path: '%profile%\\data.bin',
	encoding: 'binary',
});

await fb2k.invoke('file.write', {
	path: '%profile%\\data-copy.bin',
	content: `base64:${binary.content}`,
	encoding: 'binary',
});
```

## shell

### shell.exec


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `args` | `array` | No | Optional; omitted by default. |
| `command` | `string` | Yes | Command or executable to run. |
| `cwd` | `string` | No | Optional working directory; validated when present. |
| `hidden` | `boolean` | No | Optional; default true. |

**Returns**: `{"error":"...","processId":"...","success":true}`

```js
await fb2k.invoke('shell.exec', {
	command: 'ffprobe',
	args: ['-hide_banner', 'C:\\Music\\song.flac'],
});
```

### shell.openExternal


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `url` | `string` | Yes | Must use the `http://`, `https://`, or `mailto:` scheme. |

**Returns**: `{"error":"...","success":true}`

```js
await fb2k.invoke('shell.openExternal', {
	url: 'https://www.foobar2000.org/',
});
```

### shell.openWith


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | File to hand to the shell's default handler. |

**Returns**: `{"error":"...","success":true}`

```js
await fb2k.invoke('shell.openWith', { path: 'C:\\Music\\cover.jpg' });
```

### shell.showInExplorer


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | File or folder to reveal in Explorer. |

**Returns**: `{"error":"...","success":true}`

```js
await fb2k.invoke('shell.showInExplorer', { path: 'C:\\Music\\song.flac' });
```

### shell.spawn


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `args` | `array` | No | Optional; omitted by default. |
| `cwd` | `string` | No | Optional working directory; validated when present. |
| `executable` | `string` | Yes | Executable to launch. Absolute paths are validated. |
| `hidden` | `boolean` | No | Optional; default true. |
| `waitForExitMs` | `integer` | No | Optional; default 0 (do not wait). |

**Returns**: `{"error":"...","exitCode":"...","exited":"...","processId":"...","success":true}`

```js
const { exited, exitCode } = await fb2k.invoke('shell.spawn', {
	executable: 'ffprobe.exe',
	args: ['C:\\Music\\song.flac'],
	waitForExitMs: 5000,
});
```

## Files, dialogs, and shell boundaries

- File APIs expand the documented path variables before access. Read and media-write permissions are enforced by their registered `SecurityLevel`; `file.write` creates missing parent directories, while `file.delete` defaults to the Recycle Bin.
- `file.list` returns names in non-recursive mode and full paths in recursive mode. `file.getInfo` returns `exists: false` as a successful absence result.
- Native dialog cancellation returns `canceled: true` with an empty result path/list. Dialog initialization failures add `error` and set `canceled: false`.
- `shell.openExternal` accepts only `http://`, `https://`, or `mailto:` URLs. `shell.openWith` rejects executable, script, installer, shortcut, library, and related dangerous extensions.
- `shell.exec` and `shell.spawn` intentionally do not impose a command allowlist. Their `cwd` and any absolute executable path are validated; `shell.spawn.waitForExitMs` optionally reports early process exit.
