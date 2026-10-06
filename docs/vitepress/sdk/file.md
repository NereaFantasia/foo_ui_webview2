# fb.file File System

`fb.file` exposes host-side file and directory operations to the WebView.

## cancelOp(operationId)

Signature: `fb.file.cancelOp(operationId: string): Promise<FileCancelOpResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `operationId` | `string` | Yes | Id from the receipt of `copyAsync()`, `moveAsync()` or `deleteAsync()` |

Stops an operation started by `copyAsync()`, `moveAsync()` or `deleteAsync()`. Resolves with `cancelled: true` when the operation was still running and has been told to stop. `cancelled: false` means it had already finished or never existed; the two cases are deliberately indistinguishable.

Cancellation takes effect part-way through a batch. A copy or move stops within the current file and removes its partial copy; a delete stops at the next entry. Entries already done keep their results, every remaining entry is reported as `skipped` with `reason: 'cancelled'`, and the run still ends with a `file:opComplete` carrying `cancelled: true`.

Closing a popup cancels the operations that popup started. A panel has no such hook, so its operations run to the end unless this method stops them. Quitting foobar2000 cancels every running operation, and no further events are sent for them.

```javascript
const started = await fb.file.deleteAsync(['%profile%\\my-panel\\old-cache']);
if (started.success === false) throw new Error(started.error);

const res = await fb.file.cancelOp(started.operationId);
if (res.success === false) throw new Error(res.error);
const { cancelled } = res;
```

## copy(source, destination, opts?)

Signature: `fb.file.copy(source: string, destination: string, opts?: Omit<FileCopyParams, 'source' | 'destination'>): Promise<FileCopyResponse>`

Copies a file or a whole directory tree and blocks until the copy is done; use
[`copyAsync()`](#copyasync-items-options) for anything large. The parent
directory of `destination` must already exist. `opts.overwrite` defaults to
`false`, so an existing destination is left untouched unless you opt in.

```javascript
await fb.file.copy('C:\\Music\\track.flac', 'C:\\Backup\\track.flac');

// Replace an existing destination
await fb.file.copy('C:\\Music\\track.flac', 'C:\\Backup\\track.flac', {
	overwrite: true
});
```

## copyAsync(items, options?)

Signature: `fb.file.copyAsync(items: FileOpEntry[], options?: FileOpAsyncOptions): Promise<FileCopyAsyncResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `items` | `FileOpEntry[]` | Yes | `{ source, destination }` pairs, at least one. A missing parent directory of `destination` is created; when `destination` is an existing directory and `source` is a file, the file goes inside it under its own name |
| `options.overwrite` | `boolean` | No | Replace an existing file destination instead of skipping it; defaults to `false` |

Copies a batch of files and directories on a host worker thread, so the UI does not block. Resolves at once with a receipt: `operationId`, which starts with `fileop_`, and `totalCount`, the number of entries accepted. The results arrive in batches on `file:opProgress`, followed by one `file:opComplete`; see [Events](#events).

One result is reported per entry, not per file: a directory entry is reported once its whole tree has been walked. Copying a directory onto an existing directory merges into it and skips the files already present without reporting them, so the entry still ends as `status: 'ok'`. A file entry whose destination exists is reported as `skipped` with `reason: 'already-exists'` unless `overwrite` is set.

Every path is checked before anything starts, `source` for reading and `destination` for writing. One refused path fails the whole call with `PERMISSION_DENIED`, and no `operationId` is produced. At most 8 batch operations run at a time across the process; beyond that the call fails with `OPERATION_FAILED`.

```javascript
const res = await fb.file.copyAsync([
	{ source: 'E:\\Music\\Album', destination: 'D:\\Backup\\Album' },
	{ source: 'E:\\Music\\song.flac', destination: 'D:\\Backup\\song.flac' },
]);
if (res.success === false) throw new Error(res.error);
const { operationId } = res;
```

## delete(path, opts?)

Signature: `fb.file.delete(path: string, opts?: Omit<FileDeleteParams, 'path'>): Promise<FileDeleteResponse>`

Deletes the file-system entry. `opts.moveToTrash` defaults to `true` on the
host; pass `false` for a permanent delete, which fails on a directory that is
not empty. [`deleteAsync()`](#deleteasync-paths-options) can remove one.

```javascript
// Recycle Bin (default)
await fb.file.delete('C:\\Config\\old-theme.json');

// Permanent, bypassing the Recycle Bin
await fb.file.delete('C:\\Config\\cache.tmp', { moveToTrash: false });
```

## deleteAsync(paths, options?)

Signature: `fb.file.deleteAsync(paths: string[], options?: FileDeleteAsyncOptions): Promise<FileDeleteAsyncResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `paths` | `string[]` | Yes | Paths to delete, at least one |
| `options.moveToTrash` | `boolean` | No | Send each entry to the Recycle Bin; defaults to `true`. `false` deletes permanently |

Deletes a batch without blocking the UI, with the same receipt and events as `copyAsync()`; its results carry no `destination`. Recycle Bin deletes go through the shell, which needs the host's main thread, so they run there in batches of 16. Permanent deletes run on a worker thread and remove directories that are not empty, which the synchronous `delete()` refuses to do.

Every path is checked for writing before anything starts. One refused path fails the whole call with `PERMISSION_DENIED`, and no `operationId` is produced.

```javascript
const res = await fb.file.deleteAsync(
	['%profile%\\my-panel\\a.log', '%profile%\\my-panel\\old-cache'],
	{ moveToTrash: false },
);
if (res.success === false) throw new Error(res.error);
const { operationId } = res;
```

## exists(path)

Signature: `fb.file.exists(path: string): Promise<FileExistsResponse>`

Tests whether a file-system entry exists. Resolves with `exists`, `isFile` and `isDirectory`.

```javascript
const res = await fb.file.exists('C:\\Music\\track.flac');
if (res.success === false) throw new Error(res.error);
if (res.exists && res.isFile) {
	// ...
}
```

## getInfo(path)

Signature: `fb.file.getInfo(path: string): Promise<FileGetInfoResponse>`

Returns `exists` and, when available, `isDirectory`, `isFile`, `size`, `modified`, `name`, `extension`, and `parent`. `modified` is a JavaScript timestamp in milliseconds.

```javascript
const info = await fb.file.getInfo('C:\\Music\\track.flac');
if (info.success === false) throw new Error(info.error);
console.log(info.size, info.modified);
```

## list(path, options?)

Signature: `fb.file.list(path: string, options?: Omit<FileListParams, 'path'>): Promise<FileListResponse>`

Lists matching files and directories. `options.pattern` defaults to `*`; `options.recursive` defaults to `false`. The response exposes `files`, `directories`, and the compatibility alias `items`; with `recursive` set, the entries are full paths instead of names.

`options.pattern` filters files only; directories are always listed. Only `*`,
`*.*` and a single extension such as `*.flac` are understood. A pattern that
does not start with `*.` matches every file, and a `*.ext` pattern is compared,
ignoring case, with the file name's extension, so `*.{flac,mp3}` matches
nothing.

```javascript
const result = await fb.file.list('C:\\Music', {
	pattern: '*.flac',
	recursive: true
});
```

## mkdir(path)

Signature: `fb.file.mkdir(path: string): Promise<FileMkdirResponse>`

Creates a directory, including missing parent directories.

```javascript
await fb.file.mkdir('C:\\Config\\my-panel\\logs');
```

## move(source, destination)

Signature: `fb.file.move(source: string, destination: string): Promise<FileMoveResponse>`

Moves a file-system entry to a new path. An existing file at `destination` is replaced.

```javascript
await fb.file.move('C:\\Inbox\\notes.txt', 'C:\\Archive\\notes.txt');
```

## moveAsync(items, options?)

Signature: `fb.file.moveAsync(items: FileOpEntry[], options?: FileOpAsyncOptions): Promise<FileMoveAsyncResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `items` | `FileOpEntry[]` | Yes | `{ source, destination }` pairs, at least one |
| `options.overwrite` | `boolean` | No | Replace an existing **file** destination; defaults to `false`, while the synchronous `move()` always replaces one. An existing **directory** destination is never replaced: on the same volume such an entry ends as `skipped` or `failed` |

Moves a batch without blocking the UI, with the same receipt and events as `copyAsync()`. Within one volume a move is a rename. Across volumes the host copies the entry and then deletes the source; that entry reports `status: 'ok'` with `reason: 'cross-volume'`. Unlike `copyAsync()`, a directory whose destination already exists is reported as `skipped` with `reason: 'already-exists'` rather than merged.

Every path is checked for writing before anything starts, `source` included, since a move deletes it. One refused path fails the whole call with `PERMISSION_DENIED`, and no `operationId` is produced.

```javascript
const res = await fb.file.moveAsync(
	[{ source: 'E:\\Inbox\\Album', destination: 'D:\\Music\\Album' }],
	{ overwrite: false },
);
if (res.success === false) throw new Error(res.error);
const { operationId } = res;
```

## read(path, options?)

Signature: `fb.file.read(path: string, options?: Omit<FileReadParams, 'path'>): Promise<FileReadResponse>`

Reads a text file. `options.encoding` defaults to `utf-8`. Resolves with `content` and `size`, the size of the file on disk in bytes.

```javascript
const res = await fb.file.read('C:\\Config\\settings.json');
if (res.success === false) throw new Error(res.error);
const { content } = res;
```

## rename(path, newName)

Signature: `fb.file.rename(path: string, newName: string): Promise<FileRenameResponse>`

Renames an entry within its current parent directory. `newName` is a name, not a destination path.

```javascript
await fb.file.rename('C:\\Music\\old.flac', 'new.flac');
```

## write(path, content, options?)

Signature: `fb.file.write(path: string, content: string, options?: Omit<FileWriteParams, 'path' | 'content'>): Promise<FileWriteResponse>`

Writes text to a file. `options.encoding` defaults to `utf-8`; set `options.append` to append instead of replacing the file (it defaults to `false`). Resolves with `bytesWritten`, the size of the file after the write; with `append` that is the whole file, not just the part added.

Set `options.atomic` for files that must never be read half-written, such as settings or a pointer to the current version: the host writes a temporary file next to the target and swaps it in with one rename. It cannot be combined with `append`. The temporary file needs room in the target's folder too; see [Path length](/api/file#path-length) for the limits.

```javascript
await fb.file.write('%profile%\\webview-ui\\my-theme\\current.json', JSON.stringify({ version: '2.1.0' }), {
		atomic: true,
});
```

```javascript
await fb.file.write('C:\\Logs\\theme.log', 'Theme initialized\n', { append: true });
```

## Binary payloads

When the page already holds bytes or a Data URL, use the byte helpers:

- `readBinary(path): Promise<Uint8Array>` decodes the host response.
- `writeBinary(path, bytes, options?)` accepts `ArrayBuffer | Uint8Array`.
- `writeDataUrl(path, dataUrl, options?)` accepts a canonical Base64 Data URL,
  validates it, and writes only its payload. The media type is validated but
  does not determine the destination extension.

```javascript
const bytes = await fb.file.readBinary('C:\\Config\\icon.ico');
await fb.file.writeBinary('C:\\Config\\icon-copy.ico', bytes);

await fb.file.writeDataUrl('C:\\Config\\cover.png', coverDataUrl);
```

The helpers are built on `read()` and `write()`, which can also carry binary
content directly:

- `read(path, { encoding: 'binary' })` returns Base64 in `content` and sets
	`encoding` to `'base64'`; the returned payload has no `base64:` prefix.
- `write(path, content, { encoding: 'binary' })` decodes only when `content`
	starts with `base64:`.
- A Data URL (`data:image/...;base64,...`) and an `fb2k://` URL are not binary
	file payloads and must not be passed directly to binary `write`.

```javascript
const source = await fb.file.read('C:\\Config\\icon.ico', { encoding: 'binary' });
if (source.success === false) throw new Error(source.error);
await fb.file.write('C:\\Config\\icon-copy.ico', `base64:${source.content}`, {
		encoding: 'binary',
});
```

## Events

`copyAsync()`, `moveAsync()` and `deleteAsync()` report their results through two events. Both go to the page that started the operation; when that window is gone by then, they go to the main window's page instead, and are dropped if the main window has no WebView attached.

- `file:opProgress` carries `operationId`, `op` (`copy`, `move` or `delete`), `done` (entries reported so far, failures included), `total` (entries accepted) and `results`, the results of this batch only, in request order. A batch goes out once 64 results are pending or 100 ms have passed since the previous one, and the last partial batch arrives before `file:opComplete`.
- `file:opComplete` is the last event of a run and carries `operationId`, `op`, `total`, `successCount`, `skippedCount`, `failureCount` and `cancelled`. It is sent after a cancel and when entries failed too. It is skipped only when foobar2000 is shutting down or the worker fails unexpectedly, so a listener that cleans up on it should keep its own timeout. Afterwards [`cancelOp()`](#cancelop-operationid) reports `cancelled: false` for that `operationId`.

Each result carries `source` and `destination` as requested, with path variables unexpanded (`deleteAsync()` results have no `destination`), and `status`: `ok`, `skipped` or `failed`. `reason` is absent when the entry was carried out plainly; otherwise it is `already-exists` or `cancelled` with `skipped`, `not-found`, `permission`, `path-too-long` or `io-error` with `failed`, and `cross-volume` with `ok`. `path-too-long` is explained under [Path length](/api/file#path-length).

```javascript
const res = await fb.file.copyAsync([
	{ source: 'E:\\Music\\Album', destination: 'D:\\Backup\\Album' },
]);
if (res.success === false) throw new Error(res.error);
const { operationId } = res;

const offProgress = fb.on('file:opProgress', (e) => {
	if (e.operationId !== operationId) return;
	console.log(`${e.done} / ${e.total}`);
	for (const r of e.results) {
		if (r.status === 'failed') console.warn(r.source, r.reason);
	}
});

const offComplete = fb.on('file:opComplete', (e) => {
	if (e.operationId !== operationId) return;
	offProgress();
	offComplete();
	console.log(e.successCount, e.skippedCount, e.failureCount, e.cancelled);
});
```
