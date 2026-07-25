# File API

English API reference for the `dialog`, `file`, `shell` family.

This page is the primary owner for the namespaces listed below. Method names, parameter keys, and return fields follow the C++ `RegisterApi` handlers.

## dialog

### dialog.confirm

Public API method. Runtime authority: `src/api/DialogApi.cpp:407`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `buttons` | `array` | No | Optional; default omitted. |
| `defaultButton` | `integer` | No | Optional; default 0. |
| `message` | `string` | No | Optional; default . |
| `title` | `string` | No | Optional; default Confirm. |
| `type` | `string` | No | Optional; default question. |

**Returns**: `{"response":"..."}`

```js
const result = await fb2k.invoke('dialog.confirm', { buttons: /* value */, defaultButton: /* value */, message: /* value */, title: /* value */, type: /* value */ });
```

### dialog.openFile

Public API method. Runtime authority: `src/api/DialogApi.cpp:398`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `defaultPath` | `string` | No | Optional; default empty. Supports `%music%` expansion. |
| `filters` | `array` | No | Optional filter specs `{ name, extensions[] }` parsed by `ParseFilterSpecs`. |
| `multiple` | `boolean` | No | Optional; default false. |
| `title` | `string` | No | Optional; default Open File. |

**Returns**: `{"canceled":"...","error":"...","filePaths":"..."}`

```js
const result = await fb2k.invoke('dialog.openFile', { defaultPath: /* value */, multiple: /* value */, title: /* value */, filters: /* value */ });
```

### dialog.openFolder

Public API method. Runtime authority: `src/api/DialogApi.cpp:404`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `title` | `string` | No | Optional; default Select Folder. |

**Returns**: `{"canceled":"...","error":"...","folderPath":"..."}`

```js
const result = await fb2k.invoke('dialog.openFolder', { title: /* value */ });
```

### dialog.saveFile

Public API method. Runtime authority: `src/api/DialogApi.cpp:401`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `defaultName` | `string` | No | Optional; default empty. |
| `filters` | `array` | No | Optional filter specs `{ name, extensions[] }` parsed by `ParseFilterSpecs`. |
| `title` | `string` | No | Optional; default Save File. |

**Returns**: `{"canceled":"...","error":"...","filePath":"..."}`

```js
const result = await fb2k.invoke('dialog.saveFile', { defaultName: /* value */, title: /* value */, filters: /* value */ });
```

## file

### file.copy

Public API method. Runtime authority: `src/api/FileApi.cpp:656`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `destination` | `string` | No | Optional; default . |
| `overwrite` | `boolean` | No | Optional; default false. |
| `source` | `string` | No | Optional; default . |

**Returns**: `{"destination":"...","error":"...","source":"...","success":true}`

```js
const result = await fb2k.invoke('file.copy', { destination: /* value */, overwrite: /* value */, source: /* value */ });
```

### file.delete

Public API method. Runtime authority: `src/api/FileApi.cpp:650`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `moveToTrash` | `boolean` | No | Optional; default true. |
| `path` | `string` | No | Optional; default . |

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('file.delete', { moveToTrash: /* value */, path: /* value */ });
```

### file.exists

Public API method. Runtime authority: `src/api/FileApi.cpp:644`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | No | Optional; default . |

**Returns**: `{"error":"...","exists":"...","isDirectory":"...","isFile":"...","success":true}`

```js
const result = await fb2k.invoke('file.exists', { path: /* value */ });
```

### file.getInfo

Public API method. Runtime authority: `src/api/FileApi.cpp:671`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | No | Optional; default . |

**Returns**: `{"error":"...","exists":"...","extension":"...","isDirectory":"...","isFile":"...","modified":"...","name":"...","parent":"...","size":"...","success":true}`

```js
const result = await fb2k.invoke('file.getInfo', { path: /* value */ });
```

### file.list

Public API method. Runtime authority: `src/api/FileApi.cpp:647`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | No | Optional; default . |
| `pattern` | `string` | No | Optional; default *. |
| `recursive` | `boolean` | No | Optional; default false. |

**Returns**: `{"directories":"...","error":"...","files":"...","items":"...","success":true}`

```js
const result = await fb2k.invoke('file.list', { path: /* value */, pattern: /* value */, recursive: /* value */ });
```

### file.mkdir

Public API method. Runtime authority: `src/api/FileApi.cpp:653`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | No | Optional; default . |

**Returns**: `{"created":"...","error":"...","message":"...","success":true}`

```js
const result = await fb2k.invoke('file.mkdir', { path: /* value */ });
```

### file.move

Public API method. Runtime authority: `src/api/FileApi.cpp:662`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `destination` | `string` | No | Optional; default . |
| `source` | `string` | No | Optional; default . |

**Returns**: `{"destination":"...","error":"...","source":"...","success":true}`

```js
const result = await fb2k.invoke('file.move', { destination: /* value */, source: /* value */ });
```

### file.read

Public API method. Runtime authority: `src/api/FileApi.cpp:638`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `encoding` | `string` | No | Optional; default utf-8. |
| `path` | `string` | No | Optional; default . |

**Returns**: `{"content":"...","encoding":"...","error":"...","size":"...","success":true}`

```js
const result = await fb2k.invoke('file.read', { encoding: /* value */, path: /* value */ });
```

When `encoding: 'binary'`, `content` is a **raw Base64 payload** without a
`base64:` prefix and the response sets `encoding: 'base64'`. This is a
transport representation, not text and not a Data URL. To write the bytes
back, add the `base64:` prefix required by `file.write` and keep
`encoding: 'binary'`.

### file.rename

Public API method. Runtime authority: `src/api/FileApi.cpp:668`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `newName` | `string` | No | Optional; default . |
| `path` | `string` | No | Optional; default . |

**Returns**: `{"error":"...","newPath":"...","oldPath":"...","success":true}`

```js
const result = await fb2k.invoke('file.rename', { newName: /* value */, path: /* value */ });
```

### file.write

Public API method. Runtime authority: `src/api/FileApi.cpp:641`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `append` | `boolean` | No | Optional; default false. |
| `content` | `string` | No | Optional; default . |
| `encoding` | `string` | No | Optional; default utf-8. |
| `path` | `string` | No | Optional; default . |

**Returns**: `{"bytesWritten":"...","error":"...","success":true}`

```js
const result = await fb2k.invoke('file.write', { append: /* value */, content: /* value */, encoding: /* value */, path: /* value */ });
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

Public API method. Runtime authority: `src/api/ShellApi.cpp:427`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `args` | `array` | No | Optional; default omitted. |
| `command` | `string` | No | Optional; default . |
| `cwd` | `string` | No | Optional; default . |
| `hidden` | `boolean` | No | Optional; default true. |

**Returns**: `{"error":"...","processId":"...","success":true}`

```js
const result = await fb2k.invoke('shell.exec', { args: /* value */, command: /* value */, cwd: /* value */, hidden: /* value */ });
```

### shell.openExternal

Public API method. Runtime authority: `src/api/ShellApi.cpp:424`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `url` | `string` | No | Optional; default . |

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('shell.openExternal', { url: /* value */ });
```

### shell.openWith

Public API method. Runtime authority: `src/api/ShellApi.cpp:421`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | No | Optional; default . |

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('shell.openWith', { path: /* value */ });
```

### shell.showInExplorer

Public API method. Runtime authority: `src/api/ShellApi.cpp:418`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | No | Optional; default . |

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('shell.showInExplorer', { path: /* value */ });
```

### shell.spawn

Public API method. Runtime authority: `src/api/ShellApi.cpp:430`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `args` | `array` | No | Optional; default omitted. |
| `cwd` | `string` | No | Optional; default . |
| `executable` | `string` | No | Optional; default . |
| `hidden` | `boolean` | No | Optional; default true. |
| `waitForExitMs` | `integer` | No | Optional; default 0. |

**Returns**: `{"error":"...","exitCode":"...","exited":"...","processId":"...","success":true}`

```js
const result = await fb2k.invoke('shell.spawn', { args: /* value */, cwd: /* value */, executable: /* value */, hidden: /* value */, waitForExitMs: /* value */ });
```

## Contract supplements

The sections below close public-contract findings from the strict parameter audit without replacing existing explanations.

<!-- phase3-supplement:dialog.openFile -->
### Contract supplement: `dialog.openFile`

Verified contract supplement. Runtime authority: `src/api/DialogApi.cpp:62-167`.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `defaultPath` | `string` | No | `` | Optional; default empty. Supports `%music%` expansion. |
| `filters` | `array` | No | `omitted` | Optional filter specs `{ name, extensions[] }` parsed by `ParseFilterSpecs`. |
| `multiple` | `boolean` | No | `false` | Optional; default false. |
| `title` | `string` | No | `Open File` | Optional; default Open File. |

#### Return fields

| Field | Type | Optional |
| --- | --- | --- |
| `canceled` | `json` | No |
| `error` | `string` | Yes |
| `filePaths` | `json` | No |

Semantics: omitted optional parameters use handler defaults; failure branches and error fields are defined by this source file.

```js
const result = await fb2k.invoke('dialog.openFile', { defaultPath: /* value */, filters: /* value */, multiple: /* value */, title: /* value */ });
```
<!-- phase3-supplement:dialog.saveFile -->
### Contract supplement: `dialog.saveFile`

Verified contract supplement. Runtime authority: `src/api/DialogApi.cpp:171-231`.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `defaultName` | `string` | No | `` | Optional; default empty. |
| `filters` | `array` | No | `omitted` | Optional filter specs `{ name, extensions[] }` parsed by `ParseFilterSpecs`. |
| `title` | `string` | No | `Save File` | Optional; default Save File. |

#### Return fields

| Field | Type | Optional |
| --- | --- | --- |
| `canceled` | `json` | No |
| `error` | `string` | Yes |
| `filePath` | `json` | No |

Semantics: omitted optional parameters use handler defaults; failure branches and error fields are defined by this source file.

```js
const result = await fb2k.invoke('dialog.saveFile', { defaultName: /* value */, filters: /* value */, title: /* value */ });
```

## Files, dialogs, and shell boundaries

- File APIs expand the documented path variables before access. Read and media-write permissions are enforced by their registered `SecurityLevel`; `file.write` creates missing parent directories, while `file.delete` defaults to the Recycle Bin.
- `file.list` returns names in non-recursive mode and full paths in recursive mode. `file.getInfo` returns `exists: false` as a successful absence result.
- Native dialog cancellation returns `canceled: true` with an empty result path/list. Dialog initialization failures add `error` and set `canceled: false`.
- `shell.openExternal` accepts only `http://`, `https://`, or `mailto:` URLs. `shell.openWith` rejects executable, script, installer, shortcut, library, and related dangerous extensions.
- `shell.exec` and `shell.spawn` intentionally do not impose a command allowlist. Their `cwd` and any absolute executable path are validated; `shell.spawn.waitForExitMs` optionally reports early process exit.
