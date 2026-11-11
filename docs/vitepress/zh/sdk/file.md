# fb.file 文件系统

`fb.file` 让 WebView 页面在宿主端操作文件与目录。

## cancelOp(operationId)

签名：`fb.file.cancelOp(operationId: string): Promise<FileCancelOpResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `operationId` | `string` | 是 | `copyAsync()`、`moveAsync()` 或 `deleteAsync()` 回执里的 id |

停止由 `copyAsync()`、`moveAsync()` 或 `deleteAsync()` 启动的操作。若操作仍在进行并已收到停止信号，返回 `cancelled: true`；若操作已结束或不存在对应 ID，返回 `cancelled: false`。

取消操作在批次执行中途即时生效：复制或移动操作在当前文件处理边界终止，并清理该文件已写入的未完成副本；删除操作在处理下一条目之前终止。已完成的条目保留其实际结果，未执行的条目标记为 `skipped` 且 `reason` 为 `'cancelled'`，最后由携带 `cancelled: true` 的 `file:opComplete` 事件完成收尾。

关闭弹出窗口（popup）会自动取消由该窗口发起的文件操作。面板（panel）生命周期内发起的操作将持续执行直至完成，除非显式调用本方法终止。退出 foobar2000 主程序将取消所有正在进行的操作，且不再派发后续事件。

```javascript
const started = await fb.file.deleteAsync(['%profile%\\my-panel\\old-cache']);
if (started.success === false) throw new Error(started.error);

const res = await fb.file.cancelOp(started.operationId);
if (res.success === false) throw new Error(res.error);
const { cancelled } = res;
```

## copy(source, destination, opts?)

签名：`fb.file.copy(source: string, destination: string, opts?: Omit<FileCopyParams, 'source' | 'destination'>): Promise<FileCopyResponse>`

同步复制文件或完整目录树，操作阻塞至复制完成；大体量数据建议使用 [`copyAsync()`](#copyasync-items-options)。`destination` 的父级目录必须预先存在。`opts.overwrite` 默认为 `false`，目标已存在时不覆盖现有文件。

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
| `items` | `FileOpEntry[]` | 是 | `{ source, destination }` 对象数组，至少包含一条。若 `destination` 缺失上级目录将自动创建；若 `destination` 为已存在的目录且 `source` 为文件，文件将使用原文件名存入该目录 |
| `options.overwrite` | `boolean` | 否 | 目标文件已存在时覆盖而不是跳过，默认 `false` |

在宿主 worker 线程上批量复制文件和目录，不阻塞 UI。调用立即返回回执：`operationId`（以 `fileop_` 开头）和 `totalCount`（受理的条目数）。结果经 `file:opProgress` 分批送达，最后是一条 `file:opComplete`，见[事件](#事件)。

进度结果按任务条目而非单个物理文件上报：目录条目在遍历并处理完整棵子树后上报一次。当目录复制到已存在的同名目录时将执行合并，目录内已存在的冲突文件直接跳过且不单独上报，该目录条目仍记录为 `status: 'ok'`。文件条目的目标已存在时标记为 `skipped` 且 `reason` 为 `'already-exists'`（显式开启 `overwrite` 除外）。

操作开始前对所有路径执行预检：`source` 校验读取权限，`destination` 校验写入权限。若任意路径校验未通过，整个调用直接失败并返回 `PERMISSION_DENIED`，不分配 `operationId`。当前进程并发批量操作上限为 8 个，超出上限时返回 `OPERATION_FAILED`。

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

删除文件或目录。`opts.moveToTrash` 默认为 `true`（移至回收站）；若设置为 `false` 则执行永久删除。同步永久删除非空目录会失败，清除非空目录请使用 [`deleteAsync()`](#deleteasync-paths-options)。

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

批量异步删除文件与目录，不阻塞用户界面；回执与事件结构与 `copyAsync()` 一致（结果中不含 `destination`）。移入回收站通过 Shell API 在宿主主线程以每批 16 条执行。永久删除在工作线程异步执行，并能删除非空目录，同步的 `delete()` 则拒绝这样做。

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

`options.pattern` 仅用于过滤文件，目录项始终全部列出。模式支持 `*`、`*.*` 以及单扩展名格式（如 `*.flac`）：不以 `*.` 开头的模式匹配所有文件；`*.ext` 模式与文件扩展名执行不区分大小写的比较（通配符组合如 `*.{flac,mp3}` 不受支持）。

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

将文件或目录移动到新路径；若 `destination` 处已存在同名文件将被覆盖。

```javascript
await fb.file.move('C:\\Inbox\\notes.txt', 'C:\\Archive\\notes.txt');
```

## moveAsync(items, options?)

签名：`fb.file.moveAsync(items: FileOpEntry[], options?: FileOpAsyncOptions): Promise<FileMoveAsyncResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `items` | `FileOpEntry[]` | 是 | `{ source, destination }` 数组，至少一条 |
| `options.overwrite` | `boolean` | 否 | 仅对目标为**文件**时生效：目标文件已存在时是否覆盖，默认 `false`（同步的 `move()` 始终覆盖）。已存在的目标**目录**不可被覆盖，同卷冲突时条目以 `skipped` 或 `failed` 结束 |

批量异步移动文件与目录，不阻塞用户界面；回执与事件结构与 `copyAsync()` 一致。同一卷内的移动执行重命名；跨卷移动时宿主先执行复制再删除源文件，成功条目标记为 `status: 'ok'` 且 `reason` 为 `'cross-volume'`。与 `copyAsync()` 的合并行为不同，若目标目录已存在，条目直接标记为 `skipped` 且 `reason` 为 `'already-exists'`，不执行合并。

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

将文本写入文件。`options.encoding` 默认为 `utf-8`；设置 `options.append` 为 `true` 时追加到文件末尾（默认 `false` 为覆盖写入）。返回的 `bytesWritten` 为写入后文件的总字节数；追加模式下该值为整个文件的当前大小。

对于配置或版本指针等需要保证写入完整性的关键文件，可启用 `options.atomic`（原子写入）：宿主在同目录下创建临时文件写入完整内容后，再通过一次改名替换目标文件。该选项不能与 `append` 同时使用。目标所在目录需具备足够的路径长度余量，详见[路径长度](/zh/api/file#路径长度)。

```javascript
await fb.file.write('%profile%\\webview-ui\\my-theme\\current.json', JSON.stringify({ version: '2.1.0' }), {
		atomic: true,
});
```

```javascript
await fb.file.write('C:\\Logs\\theme.log', '主题已初始化\n', { append: true });
```

## 二进制 payload

当已持有二进制字节或 Data URL 时，可使用以下二进制辅助方法：

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

- `file:opProgress` 携带 `operationId`、`op`（`copy`、`move` 或 `delete`）、`done`（已处理条目累计计数，含失败项）、`total`（已受理条目总数）及 `results`（当前批次处理结果，与请求顺序一致）。每累计 64 条结果或距上次派发间隔满 100 毫秒派发一次，最终批次在 `file:opComplete` 之前送达。
- `file:opComplete` 为批次操作的最终事件，携带 `operationId`、`op`、`total`、`successCount`、`skippedCount`、`failureCount` 及 `cancelled`。在取消操作后或部分条目失败时该事件仍会派发；仅在 foobar2000 主程序退出或工作线程异常崩溃时静默终止，依赖该事件进行清理的监听逻辑应设置超时保护。操作结束后调用 [`cancelOp()`](#cancelop-operationid) 返回 `cancelled: false`。

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
