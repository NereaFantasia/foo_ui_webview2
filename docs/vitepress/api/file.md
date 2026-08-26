# File API

English API reference for the `dialog`, `file`, `shell` family.

This page is the primary owner for the namespaces listed below. Method names, parameter keys, and return fields follow the C++ `RegisterApi` handlers.

## dialog

### dialog.confirm


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `title` | `string` | No | `Confirm` | Localized to the UI language. |
| `message` | `string` | No | — | Body text. |
| `type` | `string` | No | `question` | Icon type, e.g. `warning`. |
| `buttons` | `array` | No | — | Custom button labels; `response` is the clicked index. |
| `defaultButton` | `integer` | No | `0` | Index of the initially focused button. |

**Returns**: `{"response":"..."}`

```js
const { response } = await fb2k.invoke('dialog.confirm', {
	title: 'Remove Track',
	message: 'Remove this track from the playlist?',
});
```

### dialog.openFile


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `title` | `string` | No | `Open File` | Localized to the UI language. |
| `defaultPath` | `string` | No | — | Initial directory; supports `%music%` expansion. |
| `filters` | `array` | No | — | Filter specs `{ name, extensions[] }`. |
| `multiple` | `boolean` | No | `false` | Allow selecting multiple files. |

**Returns**: `{"canceled":"...","error":"...","filePaths":"..."}`

```js
const { canceled, filePaths } = await fb2k.invoke('dialog.openFile', {
	title: 'Add Audio Files',
	multiple: true,
	filters: [{ name: 'Audio', extensions: ['flac', 'mp3', 'm4a'] }],
});
```

### dialog.openFolder


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `title` | `string` | No | `Select Folder` | Localized to the UI language. |

**Returns**: `{"canceled":"...","error":"...","folderPath":"..."}`

```js
const { canceled, folderPath } = await fb2k.invoke('dialog.openFolder', {
	title: 'Choose Music Folder',
});
```

### dialog.saveFile


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `title` | `string` | No | `Save File` | Localized to the UI language. |
| `defaultName` | `string` | No | — | Pre-filled file name. |
| `filters` | `array` | No | — | Filter specs `{ name, extensions[] }`. |

**Returns**: `{"canceled":"...","error":"...","filePath":"..."}`

```js
const { canceled, filePath } = await fb2k.invoke('dialog.saveFile', {
	defaultName: 'export.m3u8',
	filters: [{ name: 'Playlist', extensions: ['m3u8'] }],
});
```

## file

### file.cancelOp


Cancels an in-flight `file.copyAsync` / `file.moveAsync` / `file.deleteAsync` operation.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `operationId` | `string` | Yes | Id from the dispatch receipt. A missing, non-string, or empty value fails with `INVALID_PARAMS`. |

**Returns**: `{"cancelled":true,"success":true}`

`cancelled: false` means the operation already finished or never existed; the two are intentionally indistinguishable. A copy or move stops within one file (the file in flight is aborted and its partial copy removed), a delete stops at the next entry. Entries already processed keep their results, the remainder is reported as `skipped` / `cancelled`, and the run still ends with a `file:opComplete` carrying `cancelled: true`.

Closing a **popup** that started an operation cancels it the same way, and so does quitting foobar2000. A panel host has no such hook: its operations run to the end unless this method stops them, and their events then take the fallback route described above.

```js
const { cancelled } = await fb2k.invoke('file.cancelOp', { operationId });
```

### file.copy


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `source` | `string` | Yes | — | Source file or directory path. |
| `destination` | `string` | Yes | — | Destination path. Its parent directory must already exist. |
| `overwrite` | `boolean` | No | `false` | Existing destinations are skipped when false. |

**Returns**: `{"destination":"...","error":"...","source":"...","success":true}`

```js
await fb2k.invoke('file.copy', {
	source: 'C:\\Music\\song.flac',
	destination: '%profile%\\backup\\song.flac',
});
```

### file.copyAsync


Cancellable batch copy; the work runs on a worker thread instead of blocking the UI.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `items` | `array<object>` | Yes | — | `{ source, destination }` pairs. A missing or empty array fails with `INVALID_PARAMS`, as does an entry that is not an object or whose `source` / `destination` is missing or not a string. Path validation is fail-fast: `source` is checked as `Read` and `destination` as `FileWrite`, and one rejected path fails the whole batch with `PERMISSION_DENIED` and no `operationId`. An empty string lands there as well, since the path check runs before the handler. |
| `overwrite` | `boolean` | No | `false` | Replace existing destinations instead of skipping them. A non-boolean value fails with `INVALID_PARAMS`. |

**Returns**: `{"operationId":"fileop_...","success":true,"totalCount":2}`

The return value is only a dispatch receipt; results arrive via `file:opProgress` (batched) and a final `file:opComplete`. One result is reported per entry, never per file: a directory entry is reported once its whole tree has been walked. At most 8 operations may be in flight process-wide; beyond that the call fails with `OPERATION_FAILED`.

A directory copied onto an existing directory is **merged** into it: the entry is not rejected as `already-exists`, files already present inside are skipped without individual reporting (unless `overwrite` is set), and the entry still reports `status: "ok"`. The `already-exists` skip applies to file entries only.

```js
const receipt = await fb2k.invoke('file.copyAsync', {
	items: [{ source: 'C:\\Music\\Album', destination: 'D:\\Backup\\Album' }],
});
```

### file.delete


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `path` | `string` | Yes | — | Path of the file to delete. |
| `moveToTrash` | `boolean` | No | `true` | Set false for a permanent delete. |

**Returns**: `{"error":"...","success":true}`

```js
await fb2k.invoke('file.delete', { path: '%profile%\\cache\\stale.json' });
```

### file.deleteAsync


Cancellable batch delete; results carry no `destination`.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `paths` | `array<string>` | Yes | — | Paths to delete. A missing or empty array fails with `INVALID_PARAMS`, as does a non-string entry. Per-item `FileWrite` validation is fail-fast: one rejected path fails the whole batch with `PERMISSION_DENIED` and no `operationId`; an empty string lands there too, since the path check runs before the handler. |
| `moveToTrash` | `boolean` | No | `true` | Set false for a permanent delete. A non-boolean value fails with `INVALID_PARAMS`. |

**Returns**: `{"operationId":"fileop_...","success":true,"totalCount":2}`

Dispatch receipt only; results arrive via `file:opProgress` and `file:opComplete`. Recycle Bin deletes run on the host main thread in batches of 16 because the shell API requires it; permanent deletes run on a worker thread and remove non-empty directories, which the synchronous `file.delete` refuses to do with `moveToTrash: false`.

```js
const receipt = await fb2k.invoke('file.deleteAsync', {
	paths: ['%profile%\\cache\\a.json', '%profile%\\cache\\old'],
	moveToTrash: false,
});
```

### file.exists


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Path to test. |

**Returns**: `{"exists":true,"isFile":true,"isDirectory":false}`

A successful check carries no `success` field; a failure returns the ordinary `{ success: false, error, code }` envelope.

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


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `path` | `string` | Yes | — | Directory to enumerate. |
| `pattern` | `string` | No | `*` | Only `*`, `*.*` and a single extension glob such as `*.flac` are interpreted, and nothing is ever rejected. A pattern that does **not** start with `*.` (`song*`, `?.txt`, `data`) silently matches every file. A pattern that **does** start with `*.` is compared literally against the text from the file's last dot onwards, so a compound form such as `*.{flac,mp3}` usually matches nothing at all - as does any `*.ext` against a file with no extension. |
| `recursive` | `boolean` | No | `false` | Returns full paths instead of names. |

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

Moving a *file* across volumes succeeds — Windows copies and deletes it. Moving a *directory* across volumes fails with `code: "NOT_SUPPORTED"` and `details.reason: "cross-volume"`.

```js
await fb2k.invoke('file.move', {
	source: '%profile%\\inbox\\song.flac',
	destination: 'C:\\Music\\song.flac',
});
```

### file.moveAsync


Cancellable batch move with a built-in cross-volume fallback.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `items` | `array<object>` | Yes | — | `{ source, destination }` pairs, validated exactly as for `file.copyAsync` except that both ends are checked as `FileWrite`, since a move deletes the source. |
| `overwrite` | `boolean` | No | `false` | Replace an existing **file** destination; the synchronous `file.move` always replaces one, this default does not. An existing **directory** destination is never replaced - Windows cannot swap a directory in place, so on the same volume such an entry ends as `skipped` or `failed` rather than overwriting. |

**Returns**: `{"operationId":"fileop_...","success":true,"totalCount":2}`

Dispatch receipt only; results arrive via `file:opProgress` and `file:opComplete`. Within one volume a move is a rename regardless of size. Across volumes the host copies and then deletes the source; that entry still reports `status: "ok"` but carries `reason: "cross-volume"`, so the extra cost is visible. Directories cross volumes the same way, which the synchronous `file.move` cannot do.

Unlike `file.copyAsync`, a directory whose destination already exists is reported as `skipped` / `already-exists` rather than merged. `overwrite` does not change that: see the parameter note above.

```js
const receipt = await fb2k.invoke('file.moveAsync', {
	items: [{ source: 'C:\\Inbox\\Album', destination: 'D:\\Music\\Album' }],
});
```

### file.read


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `path` | `string` | Yes | — | Path of the file to read. |
| `encoding` | `string` | No | `utf-8` | Pass `binary` for a Base64 payload. |

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
| `path` | `string` | Yes | Path of the existing file or directory. |
| `newName` | `string` | Yes | New file name only; path separators are rejected. |

**Returns**: `{"error":"...","newPath":"...","oldPath":"...","success":true}`

```js
await fb2k.invoke('file.rename', {
	path: 'C:\\Music\\track01.flac',
	newName: '01 - Intro.flac',
});
```

### file.write


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `path` | `string` | Yes | — | Destination path. Missing parent directories are created. |
| `content` | `string` | No | `""` | An empty string truncates the file. |
| `encoding` | `string` | No | `utf-8` | Pass `binary` together with a `base64:` prefixed `content`. |
| `append` | `boolean` | No | `false` | When false the file is truncated. |

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

## Async file operation events {#file-op-events}

`file.copyAsync`, `file.moveAsync`, and `file.deleteAsync` report their outcome through two events. Both go to the window that started the operation, which is why their `results` may carry real paths; error envelopes and the console log still never contain one.

One exception: the host resolves the target window on every emit, so once that window has been destroyed it can no longer be found and the event falls back to the main instance. A popup cancels its own in-flight operations on close, which limits how many such events exist; a panel host has no equivalent hook.

### file:opProgress

| Field | Type | Description |
| --- | --- | --- |
| `operationId` | `string` | Correlation id from the dispatch receipt. |
| `op` | `string` | `copy` / `move` / `delete`. |
| `done` | `integer` | Entries reported so far across all batches. |
| `total` | `integer` | Entries accepted by the call; equals the receipt's `totalCount`. |
| `results` | `array<object>` | This batch only, not the cumulative list. |

Batched, never one event per entry: results accumulate until 64 are pending or 100 ms have passed since the previous batch, whichever comes first. The final partial batch always arrives before `file:opComplete`.

Each `results` entry:

| Field | Type | Description |
| --- | --- | --- |
| `source` | `string` | Requested source path, echoed verbatim with `%variable%` placeholders unexpanded. |
| `destination` | `string` | Requested destination, echoed verbatim. Absent for `file.deleteAsync`. |
| `status` | `string` | `ok` / `skipped` / `failed`. |
| `reason` | `string` | Absent when the entry succeeded outright; otherwise `already-exists` / `not-found` / `permission` / `cross-volume` / `io-error` / `cancelled`. |

`skipped` means the entry was deliberately not carried out - it already existed, or the run was cancelled before reaching it - so it is not an error. `cross-volume` is the one reason that accompanies `status: "ok"`: the move succeeded through the copy-then-delete fallback.

### file:opComplete

| Field | Type | Description |
| --- | --- | --- |
| `operationId` | `string` | Correlation id from the dispatch receipt. |
| `op` | `string` | `copy` / `move` / `delete`. |
| `total` | `integer` | Entries accepted by the call. |
| `successCount` | `integer` | Entries carried out, cross-volume fallbacks included. |
| `skippedCount` | `integer` | Entries deliberately not carried out. |
| `failureCount` | `integer` | Entries that failed. |
| `cancelled` | `boolean` | True when at least one entry was reported with `reason: "cancelled"`. |

The last event for an `operationId` when it arrives, and it is emitted on the cancelled and failed paths too. Two paths skip it entirely: the host shutting down mid-run, and an unexpected host-side failure in the worker. A listener that must not leak state should therefore carry its own timeout rather than wait on this event indefinitely.

Cancelling does not drop entries: the untouched remainder is still reported as `skipped` / `cancelled`, so the three counts normally add up to `total`.

```js
fb2k.on('file:opProgress', ({ operationId, done, total, results }) => {
	// results[].status: 'ok' | 'skipped' | 'failed'
});
fb2k.on('file:opComplete', ({ operationId, successCount, cancelled }) => {
	// last event for this operationId
});
```

## shell

### shell.exec


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `command` | `string` | Yes | — | Command or executable to run. |
| `args` | `array` | No | — | Extra arguments passed to the process. |
| `cwd` | `string` | No | — | Working directory; validated when present. |
| `hidden` | `boolean` | No | `true` | Hide the process window. |

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


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `executable` | `string` | Yes | — | Executable to launch. Absolute paths are validated. |
| `args` | `array` | No | — | Argument vector passed to the process. |
| `cwd` | `string` | No | — | Working directory; validated when present. |
| `hidden` | `boolean` | No | `true` | Hide the process window. |
| `waitForExitMs` | `integer` | No | `0` | Wait up to this many ms to report early exit (`exited` / `exitCode`); 0 does not wait. |

**Returns**: `{"error":"...","exitCode":"...","exited":"...","processId":"...","success":true}`

```js
const { exited, exitCode } = await fb2k.invoke('shell.spawn', {
	executable: 'ffprobe.exe',
	args: ['C:\\Music\\song.flac'],
	waitForExitMs: 5000,
});
```

## Files, dialogs, and shell boundaries

- File APIs expand `%profile%`, `%component%`, `%music%`, `%APPDATA%` and `%TEMP%` before access. Each endpoint is validated at its registered `SecurityLevel` - `Read` for the read endpoints, `FileWrite` for every `file.*` write endpoint - and the full per-endpoint table is in the [permissions reference](/reference/permissions). `file.write` creates missing parent directories, while `file.delete` defaults to the Recycle Bin.
- Every `file.*` failure carries a `code` alongside `error`. A failure raised by the filesystem itself also carries `details.value`, the raw Win32 error number. Neither `error` nor `details` ever contains a path.
- `file.list` returns names in non-recursive mode and full paths in recursive mode. `file.getInfo` returns `exists: false` as a successful absence result.
- The async family (`copyAsync` / `moveAsync` / `deleteAsync`) returns only a dispatch receipt; per-entry outcomes appear solely in the `file:opProgress` / `file:opComplete` payloads. Those events are delivered to a single window, which is why their `results` carry paths while error envelopes and logs still do not. The synchronous `file.copy` / `file.move` / `file.delete` are unchanged by this family.
- Native dialog cancellation returns `canceled: true` with an empty result path/list. Dialog initialization failures add `error` and set `canceled: false`.
- `shell.openExternal` accepts only `http://`, `https://`, or `mailto:` URLs. `shell.openWith` rejects executable, script, installer, shortcut, library, and related dangerous extensions.
- `shell.exec` and `shell.spawn` intentionally do not impose a command allowlist. Their `cwd` and any absolute executable path are validated; `shell.spawn.waitForExitMs` optionally reports early process exit.
