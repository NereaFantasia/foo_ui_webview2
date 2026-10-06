# fb.file 文件系统

`fb.file` 让 WebView 页面在宿主端操作文件与目录。

## cancelOp(operationId)

签名：`fb.file.cancelOp(operationId: string): Promise<FileCancelOpResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `operationId` | `string` | 是 | `copyAsync()`、`moveAsync()` 或 `deleteAsync()` 回执里的 id |

停止由 `copyAsync()`、`moveAsync()` 或 `deleteAsync()` 启动的操作。操作仍在进行、已通知它停止时返回 `cancelled: true`；`cancelled: false` 表示操作已结束或从未存在，这两种情况故意不作区分。

取消在批次中途生效：复制或移动在当前文件内停下，并删掉这个文件复制了一半的副本；删除在下一条之前停下。已完成的条目保留各自的结果，其余条目记为 `skipped`、`reason: 'cancelled'`，收尾仍会发一条 `cancelled: true` 的 `file:opComplete`。

关闭 popup 窗口会取消它发起的操作。panel 没有这个时机，它发起的操作会一直跑到结束，除非用本方法停掉。退出 foobar2000 会取消所有进行中的操作，之后不再为它们发送任何事件。

```javascript
const started = await fb.file.deleteAsync(['%profile%\\my-panel\\old-cache']);
if (started.success === false) throw new Error(started.error);

const res = await fb.file.cancelOp(started.operationId);
if (res.success === false) throw new Error(res.error);
const { cancelled } = res;
```

## copy(source, destination, opts?)

签名：`fb.file.copy(source: string, destination: string, opts?: Omit<FileCopyParams, 'source' | 'destination'>): Promise<FileCopyResponse>`

复制文件或整棵目录树，复制完成前一直阻塞；体量大时改用 [`copyAsync()`](#copyasync-items-options)。`destination` 的父目录必须已经存在。`opts.overwrite` 默认为 `false`，目标已存在时不覆盖，除非显式开启。

```javascript
await fb.file.copy('C:\\Music\\track.flac', 'C:\\Backup\\track.flac');

// 覆盖已存在的目标
await fb.file.copy('C:\\Music\\track.flac', 'C:\\Backup\\track.flac', {
	overwrite: true
});
```

## copyAsync(items, options?)

签名：`fb.file.copyAsync(items: FileOpEntry[], options?: FileOpAsyncOptions): Promise<FileCopyAsyncResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `items` | `FileOpEntry[]` | 是 | `{ source, destination }` 数组，至少一条。`destination` 缺失的父目录会自动创建；`destination` 是已存在的目录而 `source` 是文件时，文件以原名放进该目录 |
| `options.overwrite` | `boolean` | 否 | 目标文件已存在时覆盖而不是跳过，默认 `false` |

在宿主 worker 线程上批量复制文件和目录，不阻塞 UI。调用立即返回回执：`operationId`（以 `fileop_` 开头）和 `totalCount`（受理的条目数）。结果经 `file:opProgress` 分批送达，最后是一条 `file:opComplete`，见[事件](#事件)。

结果按条目上报，不按文件：目录条目要等整棵树走完才上报一次。目录复制到已存在的目录时会并入其中，目录里已有的文件直接跳过、不单独上报，该条目仍记为 `status: 'ok'`。文件条目的目标已存在时记为 `skipped`、`reason: 'already-exists'`，除非设置了 `overwrite`。

开始之前逐条检查全部路径，`source` 按读取、`destination` 按写入检查。任何一条被拒，整次调用都以 `PERMISSION_DENIED` 失败，不产生 `operationId`。全进程同时最多进行 8 个批量操作，超出时以 `OPERATION_FAILED` 失败。

```javascript
const res = await fb.file.copyAsync([
	{ source: 'E:\\Music\\Album', destination: 'D:\\Backup\\Album' },
	{ source: 'E:\\Music\\song.flac', destination: 'D:\\Backup\\song.flac' },
]);
if (res.success === false) throw new Error(res.error);
const { operationId } = res;
```

## delete(path, opts?)

签名：`fb.file.delete(path: string, opts?: Omit<FileDeleteParams, 'path'>): Promise<FileDeleteResponse>`

删除文件或目录。宿主端 `opts.moveToTrash` 默认为 `true`；传 `false` 则永久删除，此时非空目录会删除失败，要删非空目录请用 [`deleteAsync()`](#deleteasync-paths-options)。

```javascript
// 移到回收站（默认）
await fb.file.delete('C:\\Config\\old-theme.json');

// 永久删除，不经回收站
await fb.file.delete('C:\\Config\\cache.tmp', { moveToTrash: false });
```

## deleteAsync(paths, options?)

签名：`fb.file.deleteAsync(paths: string[], options?: FileDeleteAsyncOptions): Promise<FileDeleteAsyncResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `string[]` | 是 | 要删除的路径，至少一条 |
| `options.moveToTrash` | `boolean` | 否 | 把每一条移入回收站，默认 `true`；为 `false` 时永久删除 |

批量删除，不阻塞 UI，回执和事件同 `copyAsync()`，结果不带 `destination`。移入回收站要经过 shell，而 shell 只能在宿主主线程上调用，所以这类删除在主线程上每 16 条一批执行。永久删除走 worker 线程，并且能删除非空目录，这是同步的 `delete()` 做不到的。

开始之前逐条按写入检查全部路径。任何一条被拒，整次调用都以 `PERMISSION_DENIED` 失败，不产生 `operationId`。

```javascript
const res = await fb.file.deleteAsync(
	['%profile%\\my-panel\\a.log', '%profile%\\my-panel\\old-cache'],
	{ moveToTrash: false },
);
if (res.success === false) throw new Error(res.error);
const { operationId } = res;
```

## exists(path)

签名：`fb.file.exists(path: string): Promise<FileExistsResponse>`

检查文件或目录是否存在。返回 `exists`、`isFile` 和 `isDirectory`。

```javascript
const res = await fb.file.exists('C:\\Music\\track.flac');
if (res.success === false) throw new Error(res.error);
if (res.exists && res.isFile) {
	// ...
}
```

## getInfo(path)

签名：`fb.file.getInfo(path: string): Promise<FileGetInfoResponse>`

返回 `exists`，条目存在时另带 `isDirectory`、`isFile`、`size`、`modified`、`name`、`extension` 与 `parent`。`modified` 是毫秒级 JavaScript 时间戳。

```javascript
const info = await fb.file.getInfo('C:\\Music\\track.flac');
if (info.success === false) throw new Error(info.error);
console.log(info.size, info.modified);
```

## list(path, options?)

签名：`fb.file.list(path: string, options?: Omit<FileListParams, 'path'>): Promise<FileListResponse>`

列出匹配的文件与目录。`options.pattern` 默认为 `*`，`options.recursive` 默认为 `false`。响应带 `files`、`directories` 与兼容别名 `items`；设置 `recursive` 时各项是完整路径而不是名称。

`options.pattern` 只过滤文件，目录总会列出。只认 `*`、`*.*` 和单个扩展名（如 `*.flac`）：不以 `*.` 开头的写法匹配全部文件；`*.ext` 写法不区分大小写地与文件扩展名比较，所以 `*.{flac,mp3}` 一个都匹配不上。

```javascript
const result = await fb.file.list('C:\\Music', {
	pattern: '*.flac',
	recursive: true
});
```

## mkdir(path)

签名：`fb.file.mkdir(path: string): Promise<FileMkdirResponse>`

创建目录，缺失的上级目录一并创建。

```javascript
await fb.file.mkdir('C:\\Config\\my-panel\\logs');
```

## move(source, destination)

签名：`fb.file.move(source: string, destination: string): Promise<FileMoveResponse>`

把文件或目录移到新路径，`destination` 处已有的文件直接替换。

```javascript
await fb.file.move('C:\\Inbox\\notes.txt', 'C:\\Archive\\notes.txt');
```

## moveAsync(items, options?)

签名：`fb.file.moveAsync(items: FileOpEntry[], options?: FileOpAsyncOptions): Promise<FileMoveAsyncResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `items` | `FileOpEntry[]` | 是 | `{ source, destination }` 数组，至少一条 |
| `options.overwrite` | `boolean` | 否 | 只对**文件**目标生效：目标文件已存在时替换，默认 `false`；同步的 `move()` 总是替换。已存在的**目录**目标永远不替换，同卷下这样的条目以 `skipped` 或 `failed` 结束 |

批量移动，不阻塞 UI，回执和事件同 `copyAsync()`。同卷移动是一次改名；跨卷时宿主先复制该条目再删除源，该条目记为 `status: 'ok'`、`reason: 'cross-volume'`。和 `copyAsync()` 不同，目标目录已存在时记为 `skipped`、`reason: 'already-exists'`，不会并入。

开始之前逐条按写入检查全部路径，`source` 也不例外，因为移动会删掉它。任何一条被拒，整次调用都以 `PERMISSION_DENIED` 失败，不产生 `operationId`。

```javascript
const res = await fb.file.moveAsync(
	[{ source: 'E:\\Inbox\\Album', destination: 'D:\\Music\\Album' }],
	{ overwrite: false },
);
if (res.success === false) throw new Error(res.error);
const { operationId } = res;
```

## read(path, options?)

签名：`fb.file.read(path: string, options?: Omit<FileReadParams, 'path'>): Promise<FileReadResponse>`

读取文本文件，`options.encoding` 默认为 `utf-8`。返回 `content` 和 `size`（文件在磁盘上的字节数）。二进制读取见下文「二进制 payload」。

```javascript
const res = await fb.file.read('C:\\Config\\settings.json');
if (res.success === false) throw new Error(res.error);
const { content } = res;
```

## rename(path, newName)

签名：`fb.file.rename(path: string, newName: string): Promise<FileRenameResponse>`

在原父目录内改名。`newName` 是名称，不是目标路径。

```javascript
await fb.file.rename('C:\\Music\\old.flac', 'new.flac');
```

## write(path, content, options?)

签名：`fb.file.write(path: string, content: string, options?: Omit<FileWriteParams, 'path' | 'content'>): Promise<FileWriteResponse>`

将文本写入文件。`options.encoding` 默认为 `utf-8`；设置 `options.append` 可追加内容而不是覆盖文件（默认 `false`）。返回的 `bytesWritten` 是写入后文件的字节数；设置 `append` 时是整个文件的大小，不只是新增的部分。

设置、指向当前版本的指针这类绝不能被读到一半的文件，设置 `options.atomic`：宿主先在目标旁边写一个临时文件，再用一次改名换上去。它不能与 `append` 同用。临时文件也要在目标所在文件夹放得下，长度上限见[路径长度](/zh/api/file#路径长度)。

```javascript
await fb.file.write('%profile%\\webview-ui\\my-theme\\current.json', JSON.stringify({ version: '2.1.0' }), {
		atomic: true,
});
```

```javascript
await fb.file.write('C:\\Logs\\theme.log', '主题已初始化\n', { append: true });
```

## 二进制 payload

页面手里已经是字节或 Data URL 时，用这几个字节辅助方法：

- `readBinary(path): Promise<Uint8Array>` 解码宿主响应。
- `writeBinary(path, bytes, options?)` 接受 `ArrayBuffer | Uint8Array`。
- `writeDataUrl(path, dataUrl, options?)` 接受规范的 Base64 Data URL，先严格
	校验，再只写入 payload。media type 会被校验，但不会决定目标文件扩展名。

```javascript
const bytes = await fb.file.readBinary('C:\\Config\\icon.ico');
await fb.file.writeBinary('C:\\Config\\icon-copy.ico', bytes);

await fb.file.writeDataUrl('C:\\Config\\cover.png', coverDataUrl);
```

这些辅助方法建立在 `read()` 与 `write()` 之上，这两个方法本身也能直接传二进制内容：

- `read(path, { encoding: 'binary' })` 在 `content` 返回 Base64，并将返回值
	的 `encoding` 设为 `'base64'`；该 payload 不带 `base64:` 前缀。
- `write(path, content, { encoding: 'binary' })` 只有在 `content` 以
	`base64:` 开头时才会解码。
- Data URL（`data:image/...;base64,...`）和 `fb2k://` URL 都不是二进制文件
	payload，不能直接传给 binary `write`。

```javascript
const source = await fb.file.read('C:\\Config\\icon.ico', { encoding: 'binary' });
if (source.success === false) throw new Error(source.error);
await fb.file.write('C:\\Config\\icon-copy.ico', `base64:${source.content}`, {
		encoding: 'binary',
});
```

## 事件

`copyAsync()`、`moveAsync()` 和 `deleteAsync()` 通过两个事件报告结果。两个事件都发给发起操作的页面；发起窗口届时已关闭的，改发给主窗口的页面，主窗口没有挂 WebView 时事件直接丢弃。

- `file:opProgress` 带 `operationId`、`op`（`copy`、`move` 或 `delete`）、`done`（各批累计上报的条目数，含失败的）、`total`（受理的条目数）和 `results`（仅本批的结果，顺序与请求一致）。积满 64 条或距上一批过了 100 ms 就发一批，最后不满一批的结果在 `file:opComplete` 之前送达。
- `file:opComplete` 是一次操作的最后一个事件，带 `operationId`、`op`、`total`、`successCount`、`skippedCount`、`failureCount` 和 `cancelled`。取消之后、有条目失败时也会发；只有 foobar2000 正在退出或 worker 意外失败时不发，所以靠它收尾的监听方应自带超时。之后对该 `operationId` 调用 [`cancelOp()`](#cancelop-operationid) 会得到 `cancelled: false`。

每条结果带请求里原样的 `source` 和 `destination`（不展开路径变量；`deleteAsync()` 的结果没有 `destination`），以及 `status`：`ok`、`skipped` 或 `failed`。条目直接完成时没有 `reason`；否则 `skipped` 配 `already-exists` 或 `cancelled`，`failed` 配 `not-found`、`permission`、`path-too-long` 或 `io-error`，`ok` 配 `cross-volume`。`path-too-long` 的含义见[路径长度](/zh/api/file#路径长度)。

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
