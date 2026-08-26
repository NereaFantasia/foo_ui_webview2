# Files, Networking, and Native I/O

This page summarizes the `fb.file`, `fb.http`, `fb.dialog`, and `fb.clipboard` namespaces. See each namespace page for the complete typed contract.

## fb.file File System

### read(path, options?)

Reads a text file and resolves with `{ content }`.

| Parameter | Type | Description |
| --- | --- | --- |
| `path` | `string` | File path |
| `options.encoding` | `string` | Optional encoding; defaults to `utf-8` |

```javascript
const { content } = await fb.file.read('%profile%\\my-panel\\settings.json');
```

### write(path, content, options?)

Writes text and returns a response that may include `bytesWritten`.

| Parameter | Type | Description |
| --- | --- | --- |
| `path` | `string` | File path |
| `content` | `string` | Text content |
| `options.encoding` | `string` | Optional encoding; defaults to `utf-8` |
| `options.append` | `boolean` | Append instead of replacing; defaults to `false` |

```javascript
await fb.file.write('%profile%\\my-panel\\theme.log', 'Ready\n', { append: true });
```

### exists(path)

Checks whether a file-system entry exists and returns `{ exists }`.

```javascript
const r = await fb.file.exists('E:\\Music\\song.flac');
if (r.exists) { /* ... */ }
```

### list(path, options?)

Lists files and directories.

| Parameter | Type | Description |
| --- | --- | --- |
| `path` | `string` | Directory path |
| `options.pattern` | `string` | Defaults to `*`. Only `*`, `*.*` and a single extension glob such as `*.flac` are interpreted. A pattern not starting with `*.` silently matches every file; one that does start with `*.` is compared literally, so `*.{flac,mp3}` usually matches nothing |
| `options.recursive` | `boolean` | Recursively enumerate entries; defaults to `false` |

```javascript
const r = await fb.file.list('E:\\Music', { pattern: '*.flac', recursive: true });
// r.files, r.directories, r.items
```

### delete(path, options?)

Deletes an entry.

| Parameter | Type | Description |
| --- | --- | --- |
| `path` | `string` | Entry to delete |
| `options.moveToTrash` | `boolean` | Send to the Recycle Bin; defaults to `true`. Pass `false` for a permanent delete, which fails on a non-empty directory |

```javascript
await fb.file.delete('%profile%\\my-panel\\cache.dat');
await fb.file.delete('%profile%\\my-panel\\stale.tmp', { moveToTrash: false });
```

### mkdir(path)

Creates a directory and any missing parents.

```javascript
await fb.file.mkdir('%profile%\\my-panel\\logs');
```

### copy(source, destination, options?)

Copies a file or directory tree. Blocks until the whole copy finishes; use [`copyAsync()`](#copyasync-items-options) for anything large.

| Parameter | Type | Description |
| --- | --- | --- |
| `source` | `string` | File or directory to copy |
| `destination` | `string` | Target path; its parent directory must already exist |
| `options.overwrite` | `boolean` | Replace existing files instead of skipping them; defaults to `false` |

```javascript
await fb.file.copy('E:\\Music\\track.flac', 'E:\\Backup\\track.flac');
await fb.file.copy('E:\\Music\\Album', 'E:\\Backup\\Album', { overwrite: true });
```

### move(source, destination)

Moves a file-system entry.

```javascript
await fb.file.move('E:\\inbox\\file.txt', 'E:\\archive\\file.txt');
```

### rename(path, newName)

Renames an entry within its current parent directory.

| Parameter | Type | Description |
| --- | --- | --- |
| `path` | `string` | Current path |
| `newName` | `string` | New name only, not a destination path |

```javascript
await fb.file.rename('E:\\Music\\old.mp3', 'new.mp3');
```

### getInfo(path)

Returns file-system metadata. Available fields include `exists`, `isDirectory`, `isFile`, `size`, `modified`, `name`, `extension`, and `parent`; `modified` is a JavaScript timestamp in milliseconds.

```javascript
const info = await fb.file.getInfo('E:\\Music\\song.flac');
// info.size, info.modified, ...
```

### copyAsync(items, options?)

Copies a batch on a host worker thread instead of blocking the UI. Resolves with a `{ operationId, totalCount }` receipt as soon as the work is dispatched; the outcome arrives on `file:opProgress` and a final `file:opComplete`.

| Parameter | Type | Description |
| --- | --- | --- |
| `items` | `FileOpEntry[]` | `{ source, destination }` pairs; must not be empty |
| `options.overwrite` | `boolean` | Replace existing destinations instead of skipping them; defaults to `false` |

```javascript
const { operationId } = await fb.file.copyAsync([
	{ source: 'E:\\Music\\Album', destination: 'D:\\Backup\\Album' },
	{ source: 'E:\\Music\\song.flac', destination: 'D:\\Backup\\song.flac' },
]);
```

One result is reported per entry, not per file. Copying a directory onto an existing directory merges into it and skips files already present without reporting them one by one, so the entry still ends as `status: 'ok'`; a file whose destination exists is reported as `skipped` with `reason: 'already-exists'` unless `overwrite` is set.

### moveAsync(items, options?)

Same batch contract as `copyAsync()`. Within one volume a move is a rename; across volumes the host copies and then deletes the source, and that entry reports `status: 'ok'` with `reason: 'cross-volume'`.

| Parameter | Type | Description |
| --- | --- | --- |
| `items` | `FileOpEntry[]` | `{ source, destination }` pairs; must not be empty |
| `options.overwrite` | `boolean` | Replace an existing **file** destination; defaults to `false`, unlike the synchronous `move()` which always replaces one. An existing **directory** destination is never replaced - on the same volume such an entry ends as `skipped` or `failed` |

```javascript
const { operationId } = await fb.file.moveAsync(
	[{ source: 'E:\\inbox\\Album', destination: 'D:\\Music\\Album' }],
	{ overwrite: false },
);
```

A directory whose destination already exists is reported as `skipped` / `already-exists` rather than merged - the one place where `moveAsync()` and `copyAsync()` differ on directories.

### deleteAsync(paths, options?)

Deletes a batch without blocking the UI. Results carry no `destination`.

| Parameter | Type | Description |
| --- | --- | --- |
| `paths` | `string[]` | Paths to delete; must not be empty |
| `options.moveToTrash` | `boolean` | Send to the Recycle Bin instead of deleting permanently; defaults to `true` |

```javascript
const { operationId } = await fb.file.deleteAsync(
	['%profile%\\my-panel\\a.log', '%profile%\\my-panel\\old-cache'],
	{ moveToTrash: false },
);
```

Recycle Bin deletes run on the host main thread in batches of 16, because the shell API requires it. Permanent deletes run on a worker thread and remove non-empty directories, which the synchronous `delete()` refuses to do in that mode.

### cancelOp(operationId)

Cancels an in-flight `copyAsync()` / `moveAsync()` / `deleteAsync()` run.

```javascript
const { cancelled } = await fb.file.cancelOp(operationId);
```

`cancelled: false` means the operation had already finished or never existed; the two are intentionally indistinguishable. A copy or move stops within one file, a delete at the next entry; entries already done keep their results, the rest are reported as `skipped` / `cancelled`, and the run still ends with `file:opComplete` carrying `cancelled: true`.

Closing a **popup** that started an operation cancels it the same way, as does quitting foobar2000. A panel host has no such hook, so its operations run to the end unless this method stops them; events emitted after the originating window is gone fall back to the main instance, or are dropped if that instance has no WebView attached.

### Consuming the progress events

Both events go to the window that started the operation, which is why their `results` may carry real paths. Once that window is gone they fall back to the main instance instead, or are dropped when it has no WebView attached - see [`cancelOp()`](#cancelop-operationid). Progress is batched: entries accumulate until 64 are pending or 100 ms have passed, whichever comes first, and the final partial batch always arrives before `file:opComplete`.

```javascript
const off = fb.on('file:opProgress', (e) => {
	if (e.operationId !== operationId) return;
	updateBar(e.done / e.total);
	for (const r of e.results) {
		// r.source, r.destination?, r.status: 'ok' | 'skipped' | 'failed'
		// r.reason?: 'already-exists' | 'not-found' | 'permission'
		//          | 'cross-volume' | 'io-error' | 'cancelled'
		if (r.status === 'failed') report(r.source, r.reason);
	}
});

fb.on('file:opComplete', (e) => {
	if (e.operationId !== operationId) return;
	off();
	// e.op, e.total, e.successCount, e.skippedCount, e.failureCount, e.cancelled
});
```

## fb.http HTTP Client

### get(url, options?)

Dispatches a GET request through the host. Requests are asynchronous by default: the immediate result contains `requestId`, and the final response arrives through `http:response`. Use `{ async: false }` for a direct response or `request()` for an awaited event-driven GET.

```javascript
const r = await fb.http.get('https://api.example.com/data', { async: false });
// r.status, r.body, r.headers
```

### post(url, body?, options?)

Dispatches a POST request. Non-string JSON bodies are serialized by the host.

| Parameter | Type | Description |
| --- | --- | --- |
| `url` | `string` | Request URL |
| `body` | `JsonValue` | Optional request body |
| `options` | `HttpRequestOptions` | Headers, timeout, async mode, redirects, response decoding, and TLS policy |

```javascript
await fb.http.post('https://api.example.com/submit', { title: 'test' });
```

### head(url, options?)

Dispatches a HEAD request. A synchronous response may include `contentLength` parsed from the response headers.

```javascript
const r = await fb.http.head('https://example.com/file.zip');
// Final async result: http:response; direct result when async: false
```

### download(url, saveTo, options?)

Downloads a URL to a local path. Download mode is synchronous by default; `{ async: true }` reports final completion through `http:downloadComplete`.

| Parameter | Type | Description |
| --- | --- | --- |
| `url` | `string` | Download URL |
| `saveTo` | `string` | Destination path |
| `options` | `HttpDownloadOptions` | Optional headers, timeout, redirects, async mode, request ID, and TLS policy |

```javascript
await fb.http.download('https://example.com/cover.jpg', '%profile%\\my-panel\\cover.jpg');
```

### put(url, body?, options?)

Dispatches a PUT request with the same body and option semantics as `post()`.

### delete(url, body?, options?)

Dispatches a DELETE request. The optional body is the second positional argument.

### patch(url, body?, options?)

Dispatches a PATCH request with the same body and option semantics as `post()`.

### request(url, options?)

Performs a GET and waits for either a synchronous host reply or the matching `http:response` event. The SDK cleans up its event listener and timeout on every completion path.

### abort(requestId)

Cancels an in-flight request using the correlation ID returned by an asynchronous dispatch.

## fb.dialog Native Dialogs

### openFile(options?)

Opens the native file picker and returns `{ canceled, filePaths, error? }`.

```javascript
const r = await fb.dialog.openFile({
	multiple: true,
	filters: ['*.flac', '*.mp3']
});
// r.filePaths
```

### saveFile(options?)

Opens the native save picker and returns `{ canceled, filePath, error? }`.

```javascript
const r = await fb.dialog.saveFile({ defaultName: 'playlist.m3u8' });
// r.filePath
```

### openFolder(options?)

Opens the native folder picker and returns `{ canceled, folderPath, error? }`.

```javascript
const r = await fb.dialog.openFolder({ title: 'Choose a music folder' });
// r.folderPath
```

### confirm(options?)

Displays a native confirmation dialog and returns `{ response }`, the zero-based index of the clicked button. With the default buttons (`OK` / `Cancel`) that means `0` for confirmed and `1` for cancelled; pass `buttons` to define your own set. There is no `confirmed` flag.

```javascript
const r = await fb.dialog.confirm({ title: 'Confirm', message: 'Delete this item?' });
if (r.response === 0) { /* confirmed */ }
```

## fb.clipboard Clipboard

### read()

Reads clipboard text.

```javascript
const r = await fb.clipboard.read();
console.log(r.text);
```

### write(text)

Writes plain text.

```javascript
await fb.clipboard.write('Copied text');
```

### writeHTML(html, plainText?)

Writes HTML and an optional non-empty plain-text fallback.

```javascript
await fb.clipboard.writeHTML('<strong>Now playing</strong>', 'Now playing');
```

### writeFiles(paths)

Writes a file-list payload for pasting into file-aware applications.

```javascript
await fb.clipboard.writeFiles(['E:\\Music\\a.flac', 'E:\\Music\\b.flac']);
```
