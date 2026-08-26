# fb.file File System

`fb.file` exposes host-side file and directory operations to the WebView.

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## Additional methods

### read(path, options?)

Signature: `fb.file.read(path: string, options?: Omit<FileReadParams, 'path'>): Promise<{ content: string }>`

Reads a text file. `options.encoding` defaults to `utf-8`.

```javascript
const { content } = await fb.file.read('C:\\Config\\settings.json');
```

### write(path, content, options?)

Signature: `fb.file.write(path: string, content: string, options?: Omit<FileWriteParams, 'path' | 'content'>): Promise<BaseResponse & { bytesWritten?: number }>`

Writes text to a file. `options.encoding` defaults to `utf-8`; set `options.append` to append instead of replacing the file.

```javascript
await fb.file.write('C:\\Logs\\theme.log', 'Theme initialized\n', { append: true });
```

### Binary payloads

Prefer the additive byte helpers when application code already has bytes or a
Data URL:

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

These helpers adapt to the existing Host contract; they do not add a new Host
endpoint. The low-level `read()` and `write()` methods remain available and
preserve the raw wire behavior:

- `read(path, { encoding: 'binary' })` returns Base64 in `content` and sets
	`encoding` to `'base64'`; the returned payload has no `base64:` prefix.
- `write(path, content, { encoding: 'binary' })` decodes only when `content`
	starts with `base64:`.
- A Data URL (`data:image/...;base64,...`) and an `fb2k://` URL are not binary
	file payloads and must not be passed directly to binary `write`.

```javascript
const source = await fb.file.read('C:\\Config\\icon.ico', { encoding: 'binary' });
await fb.file.write('C:\\Config\\icon-copy.ico', `base64:${source.content}`, {
		encoding: 'binary',
});
```

### exists(path)

Signature: `fb.file.exists(path: string): Promise<{ exists: boolean }>`

Tests whether a file-system entry exists.

### list(path, options?)

Signature: `fb.file.list(path: string, options?: Omit<FileListParams, 'path'>): Promise<FileListResponse>`

Lists matching files and directories. `options.pattern` defaults to `*`; `options.recursive` defaults to `false`. The response exposes `files`, `directories`, and the compatibility alias `items`.

```javascript
const result = await fb.file.list('C:\\Music', {
	pattern: '*.flac',
	recursive: true
});
```

### delete(path, opts?)

Signature: `fb.file.delete(path: string, opts?: Omit<FileDeleteParams, 'path'>): Promise<BaseResponse>`

Deletes the file-system entry. `opts.moveToTrash` defaults to `true` on the
host; pass `false` for a permanent delete.

```javascript
// Recycle Bin (default)
await fb.file.delete('C:\\Config\\old-theme.json');

// Permanent, bypassing the Recycle Bin
await fb.file.delete('C:\\Config\\cache.tmp', { moveToTrash: false });
```

### mkdir(path)

Signature: `fb.file.mkdir(path: string): Promise<BaseResponse & { created?: boolean }>`

Creates a directory, including missing parent directories.

### copy(source, destination, opts?)

Signature: `fb.file.copy(source: string, destination: string, opts?: Omit<FileCopyParams, 'source' | 'destination'>): Promise<BaseResponse>`

Copies a file. `opts.overwrite` defaults to `false`, so an existing
destination is left untouched unless you opt in.

```javascript
await fb.file.copy('C:\\Music\\track.flac', 'C:\\Backup\\track.flac');

// Replace an existing destination
await fb.file.copy('C:\\Music\\track.flac', 'C:\\Backup\\track.flac', {
	overwrite: true
});
```

### move(source, destination)

Signature: `fb.file.move(source: string, destination: string): Promise<BaseResponse>`

Moves a file-system entry to a new path.

### rename(path, newName)

Signature: `fb.file.rename(path: string, newName: string): Promise<BaseResponse & { oldPath?: string; newPath?: string }>`

Renames an entry within its current parent directory. `newName` is a name, not a destination path.

```javascript
await fb.file.rename('C:\\Music\\old.flac', 'new.flac');
```

### getInfo(path)

Signature: `fb.file.getInfo(path: string): Promise<FileGetInfoResponse>`

Returns `exists` and, when available, `isDirectory`, `isFile`, `size`, `modified`, `name`, `extension`, and `parent`. `modified` is a JavaScript timestamp in milliseconds.

```javascript
const info = await fb.file.getInfo('C:\\Music\\track.flac');
console.log(info.size, info.modified);
```

<!-- END AUTO-GENERATED SDK STUBS -->
