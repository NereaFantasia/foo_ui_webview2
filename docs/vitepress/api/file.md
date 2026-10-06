# File API

Methods of the `file` namespace.

## Path length

foobar2000 does not opt in to long paths, so the host is held to the classic Windows limit: a path, with its variables expanded, can have at most 259 characters, and a folder the host creates at most 247. The synchronous methods fail a longer path with `OPERATION_FAILED` and `details.value` 206, the Windows error for a name that is too long, instead of reporting an existing file as missing. An atomic `file.write` also needs room for its temporary file, `.~<process id>-<sequence>.tmp`, in the target's folder. `file.copyAsync`, `file.moveAsync` and `file.deleteAsync` report such an entry as `failed` with reason `path-too-long`, including when a path inside a copied folder goes past the limit.

## file

### file.cancelOp

<!-- api-schema:begin file.cancelOp -->
Stop an operation started by `file.copyAsync`, `file.moveAsync` or `file.deleteAsync`. The entries it has not reached are reported as `skipped` / `cancelled`, and the run still ends with `file:opComplete`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `operationId` | `string` | Yes | Id from the receipt of `file.copyAsync`, `file.moveAsync` or `file.deleteAsync`. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `cancelled` | `boolean` | `true` when the operation was still running and has been told to stop; `false` when it had already finished or never existed, which are deliberately indistinguishable. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

`cancelled: false` means the operation already finished or never existed; the two are intentionally indistinguishable. A copy or move stops within one file (the file in flight is aborted and its partial copy removed), a delete stops at the next entry. Entries already processed keep their results, the remainder is reported as `skipped` / `cancelled`, and the run still ends with a `file:opComplete` carrying `cancelled: true`.

Closing a **popup** that started an operation cancels it the same way, and so does quitting foobar2000. A panel host has no such hook: its operations run to the end unless this method stops them, and their events then take the fallback route described above.

```js
const res = await fb2k.invoke('file.cancelOp', { operationId });
if (res.success === false) throw new Error(res.error);
const { cancelled } = res;
```

### file.copy

<!-- api-schema:begin file.copy -->
Copy a file or a whole directory tree, blocking until the copy is done. A source that does not exist fails with `NOT_FOUND`. Prefer `file.copyAsync` for anything large.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `source` | `string` | Yes | File or directory to copy; a directory is copied with everything below it. Must not be empty. |
| `destination` | `string` | Yes | Target path. Its parent directory has to exist already. Must not be empty. |
| `overwrite` | `boolean` | No | Replace files that already exist at the target. Otherwise they are left as they are and the call still succeeds. Default: `false`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `source` | `string` | The source as given. |
| `destination` | `string` | The destination as given. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('file.copy', {
	source: 'C:\\Music\\song.flac',
	destination: '%profile%\\backup\\song.flac',
});
```

### file.copyAsync

<!-- api-schema:begin file.copyAsync -->
Start a cancellable batch copy on a worker thread and return a receipt at once. The results arrive in batches on `file:opProgress`, followed by one `file:opComplete`. At most 8 batch operations run at a time across the process; beyond that the call fails with `OPERATION_FAILED`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `items` | `FileOpEntry[]` | Yes | Entries to copy, reported one result each, in this order. An entry that is not an object holding exactly the string members `source` and `destination` fails the call with `INVALID_PARAMS`. Every path is checked before anything starts, `source` for reading and `destination` for writing; one refused path, an empty string included, fails the whole call with `PERMISSION_DENIED` and no `operationId`. Must not be empty. |
| `items[].source` | `string` | Yes | File or directory to copy or move. Echoed back verbatim, `%variable%` placeholders unexpanded, in the `file:opProgress` results. Must not be empty. |
| `items[].destination` | `string` | Yes | Target path; a missing parent directory is created. When it names an existing directory and `source` is a file, the file is placed inside it under its own name. Echoed back verbatim like `source`. Must not be empty. |
| `overwrite` | `boolean` | No | Replace an existing file destination instead of reporting the entry as `skipped` / `already-exists`. A directory copied onto an existing directory is merged into it either way; the files already inside are skipped, without being reported, unless this is set. Default: `false`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `operationId` | `string` | Id of the operation, starting with `fileop_`. The events of the operation carry it, and `file.cancelOp` takes it. |
| `totalCount` | `integer` | Number of entries accepted; the events report it as `total`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

The return value is only a dispatch receipt; results arrive via `file:opProgress` (batched) and a final `file:opComplete`. One result is reported per entry, never per file: a directory entry is reported once its whole tree has been walked. At most 8 operations may be in flight process-wide; beyond that the call fails with `OPERATION_FAILED`.

A directory copied onto an existing directory is **merged** into it: the entry is not rejected as `already-exists`, files already present inside are skipped without individual reporting (unless `overwrite` is set), and the entry still reports `status: "ok"`. The `already-exists` skip applies to file entries only.

```js
const receipt = await fb2k.invoke('file.copyAsync', {
	items: [{ source: 'C:\\Music\\Album', destination: 'D:\\Backup\\Album' }],
});
```

### file.delete

<!-- api-schema:begin file.delete -->
Delete a file or directory, to the Recycle Bin unless `moveToTrash` is `false`. A path that does not exist fails with `NOT_FOUND`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | File or directory to delete. Must not be empty. |
| `moveToTrash` | `boolean` | No | Send it to the Recycle Bin. `false` deletes it permanently, which fails on a directory that is not empty. Default: `true`. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('file.delete', { path: '%profile%\\cache\\stale.json' });
```

### file.deleteAsync

<!-- api-schema:begin file.deleteAsync -->
Start a cancellable batch delete, with the same receipt and events as `file.copyAsync`; its results carry no `destination`. Recycle Bin deletes run on the main thread in batches of 16, permanent deletes on a worker thread.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `paths` | `string[]` | Yes | Paths to delete, reported one result each, in this order. An entry that is not a string fails the call with `INVALID_PARAMS`. Every path is checked for writing before anything starts; one refused path, an empty string included, fails the whole call with `PERMISSION_DENIED` and no `operationId`. Must not be empty. |
| `moveToTrash` | `boolean` | No | Send each entry to the Recycle Bin. `false` deletes permanently and removes directories that are not empty, which the synchronous `file.delete` refuses to do. Default: `true`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `operationId` | `string` | Id of the operation, starting with `fileop_`. The events of the operation carry it, and `file.cancelOp` takes it. |
| `totalCount` | `integer` | Number of entries accepted; the events report it as `total`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

Dispatch receipt only; results arrive via `file:opProgress` and `file:opComplete`. Recycle Bin deletes run on the host main thread in batches of 16 because the shell API requires it; permanent deletes run on a worker thread and remove non-empty directories, which the synchronous `file.delete` refuses to do with `moveToTrash: false`.

```js
const receipt = await fb2k.invoke('file.deleteAsync', {
	paths: ['%profile%\\cache\\a.json', '%profile%\\cache\\old'],
	moveToTrash: false,
});
```

### file.exists

<!-- api-schema:begin file.exists -->
Check whether a path exists and whether it is a file or a directory.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Path to check. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `exists` | `boolean` | Whether the path exists. |
| `isFile` | `boolean` | Whether the path is a regular file. |
| `isDirectory` | `boolean` | Whether the path is a directory. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('file.exists', {
	path: 'C:\\Music\\song.flac',
});
if (res.success === false) throw new Error(res.error);
const { exists, isFile } = res;
```

### file.getInfo

<!-- api-schema:begin file.getInfo -->
Describe a file or directory. A path that does not exist is a success with `exists: false` and no other field.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | File or directory to describe. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `exists` | `boolean` | Whether the path exists; when `false`, no other field is present. |
| `isDirectory` | `boolean` | Whether the path is a directory. |
| `isFile` | `boolean` | Whether the path is a regular file. |
| `size` | `integer` | Size in bytes; `0` for a directory. |
| `modified` | `integer` | Last write time as a JavaScript timestamp in milliseconds, truncated to whole seconds. |
| `name` | `string` | File name with its extension. |
| `extension` | `string` | Extension with its leading dot, such as `.flac`; empty when there is none. |
| `parent` | `string` | Parent directory of the path after `%variable%` expansion. The path is not normalized, so a doubled separator left by the expansion stays. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const info = await fb2k.invoke('file.getInfo', {
	path: 'C:\\Music\\song.flac',
});
```

### file.list

<!-- api-schema:begin file.list -->
List the files and subdirectories of a directory. A path that does not exist fails with `NOT_FOUND`, and one that is not a directory with `INVALID_PATH`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Directory to list. Must not be empty. |
| `pattern` | `string` | No | Filter for files; directories are always listed. Only `*`, `*.*` and a single extension such as `*.flac` are understood, and nothing is refused: a pattern that does not start with `*.` (`song*`, `?.txt`) matches every file, while a `*.ext` pattern is compared, ignoring case, with the text from the file name's last dot on, so `*.{flac,mp3}` matches nothing and no `*.ext` matches a file without an extension. Default: `"*"`. |
| `recursive` | `boolean` | No | Walk the subdirectories as well; the entries are then full paths instead of names. A subdirectory that cannot be read makes the call fail with `OPERATION_FAILED`. Default: `false`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `files` | `string[]` | Files that match `pattern`: names, or full paths when `recursive` is set. |
| `directories` | `string[]` | Subdirectories, whatever `pattern` says: names, or full paths when `recursive` is set. |
| `items` | `string[]` | The same list as `files`; kept for callers that read this name. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('file.list', {
	path: 'C:\\Music',
	pattern: '*.flac',
});
if (res.success === false) throw new Error(res.error);
const { files } = res;
```

### file.mkdir

<!-- api-schema:begin file.mkdir -->
Create a directory together with any missing parents. A directory that already exists is a success with `created: false`; a file at the path fails with `INVALID_PATH`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Directory to create. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `created` | `boolean` | Whether this call created the directory; `false` when it already existed. |
| `message` | `string` | `Directory already exists` when the directory was already there; absent otherwise. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('file.mkdir', { path: '%profile%\\my-panel\\cache' });
```

### file.move

<!-- api-schema:begin file.move -->
Move a file or directory, blocking until the move is done. A file can move to another volume; a directory cannot, and that fails with `NOT_SUPPORTED` and `details.reason: "cross-volume"`. A source that does not exist fails with `NOT_FOUND`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `source` | `string` | Yes | File or directory to move. Must not be empty. |
| `destination` | `string` | Yes | Target path. An existing file there is replaced; moving a file onto an existing directory, or under a parent directory that does not exist, fails. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `source` | `string` | The source as given. |
| `destination` | `string` | The destination as given. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

Moving a *file* across volumes succeeds — Windows copies and deletes it. Moving a *directory* across volumes fails with `code: "NOT_SUPPORTED"` and `details.reason: "cross-volume"`.

```js
await fb2k.invoke('file.move', {
	source: '%profile%\\inbox\\song.flac',
	destination: 'C:\\Music\\song.flac',
});
```

### file.moveAsync

<!-- api-schema:begin file.moveAsync -->
Start a cancellable batch move on a worker thread, with the same receipt and events as `file.copyAsync`. Within one volume a move is a rename; across volumes an entry is copied and its source then deleted.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `items` | `FileOpEntry[]` | Yes | Entries to move, reported one result each, in this order. An entry that is not an object holding exactly the string members `source` and `destination` fails the call with `INVALID_PARAMS`. Every path is checked for writing before anything starts, `source` included, since a move deletes it; one refused path, an empty string included, fails the whole call with `PERMISSION_DENIED` and no `operationId`. Must not be empty. |
| `items[].source` | `string` | Yes | File or directory to copy or move. Echoed back verbatim, `%variable%` placeholders unexpanded, in the `file:opProgress` results. Must not be empty. |
| `items[].destination` | `string` | Yes | Target path; a missing parent directory is created. When it names an existing directory and `source` is a file, the file is placed inside it under its own name. Echoed back verbatim like `source`. Must not be empty. |
| `overwrite` | `boolean` | No | Replace an existing file destination instead of reporting the entry as `skipped` / `already-exists`; the synchronous `file.move` always replaces one. An existing directory destination is never replaced: Windows cannot swap a directory in place, so on the same volume such an entry ends as `skipped` or `failed`. Default: `false`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `operationId` | `string` | Id of the operation, starting with `fileop_`. The events of the operation carry it, and `file.cancelOp` takes it. |
| `totalCount` | `integer` | Number of entries accepted; the events report it as `total`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

Dispatch receipt only; results arrive via `file:opProgress` and `file:opComplete`. Within one volume a move is a rename regardless of size. Across volumes the host copies and then deletes the source; that entry still reports `status: "ok"` but carries `reason: "cross-volume"`, so the extra cost is visible. Directories cross volumes the same way, which the synchronous `file.move` cannot do.

Unlike `file.copyAsync`, a directory whose destination already exists is reported as `skipped` / `already-exists` rather than merged. `overwrite` does not change that: see the parameter note above.

```js
const receipt = await fb2k.invoke('file.moveAsync', {
	items: [{ source: 'C:\\Inbox\\Album', destination: 'D:\\Music\\Album' }],
});
```

### file.read

<!-- api-schema:begin file.read -->
Read a file as text, or as Base64 with `encoding: "binary"`. A path that does not exist fails with `NOT_FOUND`, and one that is not a regular file with `INVALID_PATH`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | File to read. Must not be empty. |
| `encoding` | `string` | No | Only exactly `binary` reads the raw bytes and returns them as Base64; any other value reads the file as text. Default: `"utf-8"`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `content` | `string` | The file's text; for a binary read, its bytes as plain Base64 without a `base64:` prefix. |
| `size` | `integer` | Size of the file on disk in bytes; for a text read it can differ from the length of `content`. |
| `encoding` | `string` | Present only for a binary read, and then always `base64`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('file.read', {
	path: '%profile%\\my-panel\\settings.json',
});
if (res.success === false) throw new Error(res.error);
const { content } = res;
```

When `encoding: 'binary'`, `content` is a **raw Base64 payload** without a
`base64:` prefix and the response sets `encoding: 'base64'`. This is a
transport representation, not text and not a Data URL. To write the bytes
back, add the `base64:` prefix required by `file.write` and keep
`encoding: 'binary'`.

### file.rename

<!-- api-schema:begin file.rename -->
Rename a file or directory within its parent directory. A path that does not exist fails with `NOT_FOUND`, and a name that is already taken with `OPERATION_FAILED`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | File or directory to rename. Must not be empty. |
| `newName` | `string` | Yes | New name, kept in the same directory. A name containing `/` or `\` fails with `INVALID_PARAMS`. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `oldPath` | `string` | The path as given. |
| `newPath` | `string` | The parent directory of the expanded `path`, joined with `newName`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('file.rename', {
	path: 'C:\\Music\\track01.flac',
	newName: '01 - Intro.flac',
});
```

### file.write

<!-- api-schema:begin file.write -->
Write text, or bytes decoded from Base64, to a file. Missing parent directories are created first.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | File to write; missing parent directories are created. Must not be empty. |
| `content` | `string` | No | What to write. With `encoding: "binary"` a value starting with `base64:` is decoded and the bytes are written: characters outside the Base64 alphabet are skipped and decoding stops at the first `=`. Any other value, plain Base64 or a Data URL included, is written as it is. An empty string empties the file unless `append` is set. Default: `""`. |
| `encoding` | `string` | No | Only exactly `binary` changes anything: the file is written in binary mode and a `content` starting with `base64:` is decoded. Any other value writes text. Default: `"utf-8"`. |
| `append` | `boolean` | No | Add to the end of the file instead of replacing its content. Default: `false`. |
| `atomic` | `boolean` | No | Write a temporary file next to the target, flush it to disk, then replace the target with one rename: readers see the old content or the new content, never a partly written file. If the replacement fails, for example because another program has the target open, the call fails and the target keeps its old content. The temporary file is named `.~<process id>-<sequence>.tmp`, and its path has to fit in 259 characters as well. Cannot be combined with `append`; passing both fails with `INVALID_PARAMS`. Default: `false`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `bytesWritten` | `integer` | Size of the file after the write, in bytes; with `append` it is the whole file, not just the part added. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

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
if (binary.success === false) throw new Error(binary.error);

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
| `reason` | `string` | Absent when the entry succeeded outright; otherwise `already-exists` / `not-found` / `permission` / `cross-volume` / `path-too-long` / `io-error` / `cancelled`. |

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

## File boundaries

- File APIs expand `%profile%`, `%component%`, `%music%`, `%APPDATA%` and `%TEMP%` before access. Each endpoint is validated at its registered `SecurityLevel` - `Read` for the read endpoints, `FileWrite` for every `file.*` write endpoint - and the full per-endpoint table is in the [permissions reference](/reference/permissions). `file.write` creates missing parent directories, while `file.delete` defaults to the Recycle Bin.
- Every `file.*` failure carries a `code` alongside `error`. A failure raised by the filesystem itself also carries `details.value`, the raw Win32 error number. Neither `error` nor `details` ever contains a path.
- `file.list` returns names in non-recursive mode and full paths in recursive mode. `file.getInfo` returns `exists: false` as a successful absence result.
- The async family (`copyAsync` / `moveAsync` / `deleteAsync`) returns only a dispatch receipt; per-entry outcomes appear solely in the `file:opProgress` / `file:opComplete` payloads. Those events are delivered to a single window, which is why their `results` carry paths while error envelopes and logs still do not. The synchronous `file.copy` / `file.move` / `file.delete` are unchanged by this family.
