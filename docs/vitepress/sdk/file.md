# fb.file File System

`fb.file` exposes host-side file and directory operations to the WebView.

## cancelOp(operationId)

Signature: `fb.file.cancelOp(operationId: string): Promise<FileCancelOpResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `operationId` | `string` | Yes | Id from the receipt of `copyAsync()`, `moveAsync()` or `deleteAsync()` |

Stops an operation started by `copyAsync()`, `moveAsync()`, or `deleteAsync()`. Resolves with `cancelled: true` when the operation was still running and has been instructed to stop. `cancelled: false` indicates that it had already finished or the `operationId` was not found.

Cancellation takes effect during batch processing: a copy or move operation aborts at the current file and removes any incomplete target copy; a delete operation aborts before the next entry. Completed entries retain their outcomes, remaining entries are marked as `skipped` with `reason: 'cancelled'`, and execution concludes with a `file:opComplete` event carrying `cancelled: true`.

Closing a popup window automatically cancels operations initiated by that window. Operations initiated by a panel run until completion unless explicitly stopped via this method. Quitting foobar2000 cancels all active operations and emits no further events.

```javascript
const started = await fb.file.deleteAsync(['%profile%\\my-panel\\old-cache']);
if (started.success === false) throw new Error(started.error);

const res = await fb.file.cancelOp(started.operationId);
if (res.success === false) throw new Error(res.error);
const { cancelled } = res;
```

## copy(source, destination, opts?)

Signature: `fb.file.copy(source: string, destination: string, opts?: Omit<FileCopyParams, 'source' | 'destination'>): Promise<FileCopyResponse>`

Synchronously copies a file or directory tree, blocking until completion; use
[`copyAsync()`](#copyasync-items-options) for large datasets. The parent
directory of `destination` must already exist. `opts.overwrite` defaults to
`false`, leaving existing destinations untouched unless explicitly enabled.

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
| `items` | `FileOpEntry[]` | Yes | Array of `{ source, destination }` entries, at least one. Missing parent directories of `destination` are created automatically; when `destination` is an existing directory and `source` is a file, the file is placed inside it using its source filename |
| `options.overwrite` | `boolean` | No | Replace an existing file destination instead of skipping it; defaults to `false` |

Copies a batch of files and directories on a host worker thread, so the UI does not block. Resolves at once with a receipt: `operationId`, which starts with `fileop_`, and `totalCount`, the number of entries accepted. The results arrive in batches on `file:opProgress`, followed by one `file:opComplete`; see [Events](#events).

Progress is reported per task entry rather than per individual file: a directory entry reports once its entire subtree has been processed. Copying a directory onto an existing destination directory merges contents and skips conflicting existing files without separate reports, completing the entry as `status: 'ok'`. A file entry whose destination exists reports `skipped` with `reason: 'already-exists'` unless `overwrite` is enabled.

All paths undergo pre-validation before execution: `source` is checked for read permissions and `destination` for write permissions. If any path fails validation, the entire call fails with `PERMISSION_DENIED` without creating an `operationId`. The process enforces a limit of 8 concurrent batch operations, failing with `OPERATION_FAILED` when exceeded.

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

Deletes a file or directory. `opts.moveToTrash` defaults to `true` (Recycle Bin); passing `false` deletes permanently. Synchronous permanent deletion of a non-empty directory fails; use [`deleteAsync()`](#deleteasync-paths-options) to delete non-empty directories.

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

Asynchronously deletes a batch of files and directories without blocking the UI; receipts and events match `copyAsync()` (omitting `destination` in results). Recycle Bin deletions execute on the host main thread via Shell APIs in batches of 16 entries. Permanent deletions execute on worker threads and remove non-empty directories, which the synchronous `delete()` refuses to do.

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

`options.pattern` filters files only; directories are always listed. Patterns support `*`, `*.*`, and single extension patterns (such as `*.flac`): patterns not starting with `*.` match all files; `*.ext` patterns perform case-insensitive comparison against file extensions (compound patterns such as `*.{flac,mp3}` are not supported).

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

Moves a file or directory to a new path; an existing file at `destination` is replaced.

```javascript
await fb.file.move('C:\\Inbox\\notes.txt', 'C:\\Archive\\notes.txt');
```

## moveAsync(items, options?)

Signature: `fb.file.moveAsync(items: FileOpEntry[], options?: FileOpAsyncOptions): Promise<FileMoveAsyncResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `items` | `FileOpEntry[]` | Yes | `{ source, destination }` pairs, at least one |
| `options.overwrite` | `boolean` | No | Applies only to **file** destinations: whether to replace existing destination files; defaults to `false` (synchronous `move()` always replaces). Existing **directory** destinations are never replaced; on the same volume such entries complete as `skipped` or `failed` |

Asynchronously moves a batch of files and directories without blocking the UI; receipts and events match `copyAsync()`. Moves within the same volume execute as renames; across volumes, the host copies the entry and deletes the source, reporting `status: 'ok'` with `reason: 'cross-volume'`. Unlike `copyAsync()`, if the destination directory exists, the entry reports `skipped` with `reason: 'already-exists'` without merging.

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

Writes text to a file. `options.encoding` defaults to `utf-8`; set `options.append` to `true` to append content instead of overwriting (defaults to `false`). Resolves with `bytesWritten`, representing the total file size in bytes after the write; with `append` enabled, this reflects the entire file size rather than only the appended bytes.

For critical files that require transactional integrity (such as configuration files or version markers), enable `options.atomic`: the host writes to a temporary file in the target directory and replaces the destination via an atomic rename. This option cannot be combined with `append`. The destination directory must accommodate temporary filename lengths; see [Path length](/api/file#path-length).

```javascript
await fb.file.write('%profile%\\webview-ui\\my-theme\\current.json', JSON.stringify({ version: '2.1.0' }), {
		atomic: true,
});
```

```javascript
await fb.file.write('C:\\Logs\\theme.log', 'Theme initialized\n', { append: true });
```

## Binary payloads

When handling binary byte arrays or Data URLs, use the binary helper methods:

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

- `file:opProgress` carries `operationId`, `op` (`copy`, `move` or `delete`), `done` (cumulative processed count, including failures), `total` (accepted entries count), and `results` (current batch results, preserving request order). Batches dispatch when 64 entries accumulate or 100 ms elapse since the previous batch; the final partial batch arrives before `file:opComplete`.
- `file:opComplete` is the final event of an operation, carrying `operationId`, `op`, `total`, `successCount`, `skippedCount`, `failureCount`, and `cancelled`. It is emitted following cancellations or partial failures; it is omitted only during host process shutdown or abnormal worker crashes, so listeners awaiting completion should maintain a timeout guard. Calling [`cancelOp()`](#cancelop-operationid) after completion yields `cancelled: false`.

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
