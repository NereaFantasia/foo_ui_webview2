# 文件与网络

涵盖 `fb.file`、`fb.http`、`fb.dialog`、`fb.clipboard` 四个命名空间。

## fb.file 文件系统

### read(path, options?)

读取文本文件，resolve 出 `{ content }`。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| path | string | 文件路径 |
| options.encoding | string | 可选编码，默认 `utf-8` |

```javascript
const { content } = await fb.file.read('E:\\config.json');
```

### write(path, content, options?)

写入文件内容。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| path | string | 文件路径 |
| content | string | 文件内容 |
| options | object | 可选，如 { encoding: 'utf-8' } |

```javascript
await fb.file.write('E:\\output.txt', 'Hello World');
```

### exists(path)

检查文件是否存在。返回 `{exists: boolean}`。

```javascript
const r = await fb.file.exists('E:\\Music\\song.flac');
if (r.exists) { /* ... */ }
```

### list(path, options?)

列出目录内容。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| path | string | 目录路径 |
| options.pattern | string | 默认 `*`。只认 `*`、`*.*` 和单个扩展名通配（如 `*.flac`）。不以 `*.` 开头的写法静默匹配全部文件；以 `*.` 开头的写法按字面比较扩展名，所以 `*.{flac,mp3}` 通常一个都匹配不上 |
| options.recursive | boolean | 递归枚举，默认 `false` |

```javascript
const r = await fb.file.list('E:\\Music', { pattern: '*.flac', recursive: true });
// r.files, r.directories, r.items
```

### delete(path, options?)

删除一个条目。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| path | string | 待删除路径 |
| options.moveToTrash | boolean | 是否移入回收站，默认 `true`；传 `false` 为永久删除，且删不了非空目录 |

```javascript
await fb.file.delete('E:\\temp\\cache.dat');
await fb.file.delete('E:\\temp\\stale.tmp', { moveToTrash: false });
```

### mkdir(path)

创建目录（含递归创建父目录）。

```javascript
await fb.file.mkdir('E:\\output\\logs');
```

### copy(source, destination, options?)

复制文件或整棵目录树。整段复制完成前一直阻塞，大体量请改用 [`copyAsync()`](#copyasync-items-options)。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| source | string | 源文件或目录 |
| destination | string | 目标路径；其父目录必须已存在 |
| options.overwrite | boolean | 已存在的文件是否覆盖，默认 `false`（跳过） |

```javascript
await fb.file.copy('E:\\src.txt', 'E:\\backup\\src.txt');
await fb.file.copy('E:\\Music\\Album', 'E:\\Backup\\Album', { overwrite: true });
```

### move(source, destination)

移动文件。

```javascript
await fb.file.move('E:\\old\\file.txt', 'E:\\new\\file.txt');
```

### rename(path, newName)

重命名文件。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| path | string | 当前路径 |
| newName | string | 新文件名（仅名称，非完整路径） |

```javascript
await fb.file.rename('E:\\Music\\old.mp3', 'new.mp3');
```

### getInfo(path)

获取文件系统元信息。可用字段有 `exists`、`isDirectory`、`isFile`、`size`、`modified`、`name`、`extension`、`parent`；`modified` 是毫秒时间戳。

```javascript
const info = await fb.file.getInfo('E:\\Music\\song.flac');
// info.size、info.modified, ...
```

### copyAsync(items, options?)

在宿主 worker 线程上批量复制，不阻塞 UI。派工后立即返回 `{ operationId, totalCount }` 回执，结果经 `file:opProgress` 与最后一条 `file:opComplete` 送达。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| items | FileOpEntry[] | `{ source, destination }` 数组，不得为空 |
| options.overwrite | boolean | 目标已存在时覆盖而非跳过，默认 `false` |

```javascript
const { operationId } = await fb.file.copyAsync([
	{ source: 'E:\\Music\\Album', destination: 'D:\\Backup\\Album' },
	{ source: 'E:\\Music\\song.flac', destination: 'D:\\Backup\\song.flac' },
]);
```

结果按**条目**上报而非按文件：目录条目在整棵树走完后只出一条结果。目录复制到已存在的目录会并入其中，目录内已存在的文件被静默跳过、不单独上报，该条目仍记 `status: 'ok'`；单个文件的目标已存在时记 `skipped` + `reason: 'already-exists'`（除非传 `overwrite`）。

### moveAsync(items, options?)

批量契约与 `copyAsync()` 相同。同卷移动是一次改名；跨卷时宿主自动回退为"复制后删源"，该条目记 `status: 'ok'` + `reason: 'cross-volume'`。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| items | FileOpEntry[] | `{ source, destination }` 数组，不得为空 |
| options.overwrite | boolean | 只对**文件**目标生效，默认 `false`；同步版 `move()` 对文件目标无条件覆盖。已存在的**目录**目标永远不会被替换 —— 同卷下这样的条目以 `skipped` 或 `failed` 收场 |

```javascript
const { operationId } = await fb.file.moveAsync(
	[{ source: 'E:\\Inbox\\Album', destination: 'D:\\Music\\Album' }],
	{ overwrite: false },
);
```

目标目录已存在时记 `skipped` / `already-exists` 而不是并入 —— 这是 `moveAsync()` 与 `copyAsync()` 在目录语义上唯一的差别。

### deleteAsync(paths, options?)

批量删除，不阻塞 UI。结果条目不带 `destination` 字段。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| paths | string[] | 待删除路径数组，不得为空 |
| options.moveToTrash | boolean | 送回收站而非永久删除，默认 `true` |

```javascript
const { operationId } = await fb.file.deleteAsync(
	['E:\\Temp\\a.log', 'E:\\Temp\\old-cache'],
	{ moveToTrash: false },
);
```

回收站删除受 shell API 约束，在宿主主线程上每 16 条一批执行；永久删除走 worker 线程且能删非空目录 —— 同步版 `delete()` 在该模式下删不了非空目录。

### cancelOp(operationId)

取消一个进行中的 `copyAsync()` / `moveAsync()` / `deleteAsync()`。

```javascript
const { cancelled } = await fb.file.cancelOp(operationId);
```

`cancelled: false` 表示该操作已结束或从未存在，两者故意不可区分。复制/移动在一个文件之内停下，删除在下一条之前停下；已完成的条目保留各自结果，其余记 `skipped` / `cancelled`，收尾仍会发一条 `cancelled: true` 的 `file:opComplete`。

关闭发起该操作的 **popup 窗口**等效于取消，退出 foobar2000 同理。panel 宿主没有这个钩子，它发起的操作会跑到结束，除非用本方法停掉；发起窗口销毁之后发出的事件会回退投递到主实例，主实例未挂 WebView 时则被丢弃。

### 消费进度事件

两个事件都投递给发起该操作的那个窗口，这也是 `results` 里可以带真实路径的前提。该窗口销毁后事件改投主实例，主实例未挂 WebView 时被静默丢弃 —— 见 [`cancelOp()`](#cancelop-operationid)。进度是分批的：攒够 64 条或距上一批满 100 ms 先到者触发，最后的残余批必定排在 `file:opComplete` 之前。

```javascript
const off = fb.on('file:opProgress', (e) => {
	if (e.operationId !== operationId) return;
	updateBar(e.done / e.total);
	for (const r of e.results) {
		// r.source、r.destination?、r.status: 'ok' | 'skipped' | 'failed'
		// r.reason?: 'already-exists' | 'not-found' | 'permission'
		//          | 'cross-volume' | 'io-error' | 'cancelled'
		if (r.status === 'failed') report(r.source, r.reason);
	}
});

fb.on('file:opComplete', (e) => {
	if (e.operationId !== operationId) return;
	off();
	// e.op、e.total、e.successCount、e.skippedCount、e.failureCount、e.cancelled
});
```

## fb.http HTTP 请求

### get(url, options?)

通过宿主派发 GET 请求。请求默认异步执行：当前响应包含 `requestId`，最终响应通过 `http:response` 到达。若要直接取得完整响应，请传入 `{ async: false }`；若要等待事件驱动结果，请使用 `request()`。

```javascript
const r = await fb.http.get('https://api.example.com/data', { async: false });
// r.status, r.body, r.headers
```

### post(url, body, options?)

发送 POST 请求。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| url | string | 请求 URL |
| body | string/object | 请求体 |
| options | object | 可选，如 { headers: {...} } |

```javascript
await fb.http.post('https://api.example.com/submit', { title: 'test' });
```

### head(url, options?)

发送 HEAD 请求，仅获取响应头。与 `get()` 一样默认异步：直接拿完整响应要传 `{ async: false }`，否则最终结果经 `http:response` 到达。同步响应可能带一个从响应头解析出的 `contentLength`。

```javascript
const r = await fb.http.head('https://example.com/file.zip', { async: false });
// r.contentLength、r.headers
```

### download(url, saveTo, options?)

下载文件到本地。下载默认同步执行；传入 `{ async: true }` 时，最终结果通过 `http:downloadComplete` 到达。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| url | string | 下载 URL |
| saveTo | string | 保存路径 |
| options | object | 可选配置 |

```javascript
await fb.http.download('https://example.com/cover.jpg', 'E:\\covers\\cover.jpg');
```

### put(url, body, options?)

发送 PUT 请求。参数与 `post` 相同。

### delete(url, body?, options?)

发送 DELETE 请求。

### patch(url, body, options?)

发送 PATCH 请求。参数与 `post` 相同。

### request(url, options?)

发一个 GET，并等待宿主的同步回复或与之匹配的 `http:response` 事件，先到者返回。无论走哪条路径，SDK 都会清理自己的事件监听与超时。

### abort(requestId)

中止正在进行的 HTTP 请求。`requestId` 由异步派发返回。

## fb.dialog 对话框

### openFile(options?)

打开文件选择对话框，返回 `{ canceled, filePaths, error? }`。过滤器写法是 `filters: [{ name, extensions }]`，`extensions` 里只写扩展名本身（不带 `*.`）。

```javascript
const r = await fb.dialog.openFile({
	multiple: true,
	filters: [{ name: '音频文件', extensions: ['flac', 'mp3'] }]
});
// r.filePaths
```

### saveFile(options?)

打开文件保存对话框，返回 `{ canceled, filePath, error? }`。

```javascript
const r = await fb.dialog.saveFile({ defaultName: 'playlist.m3u8' });
// r.filePath
```

### openFolder(options?)

打开文件夹选择对话框，返回 `{ canceled, folderPath, error? }`。

```javascript
const r = await fb.dialog.openFolder({ title: '选择音乐目录' });
// r.folderPath
```

### confirm(options?)

显示确认对话框，返回 `{ response }` —— 被点击按钮在 `buttons` 里的从 0 起下标。默认按钮组是 `确定` / `取消`，因此 `0` 表示确认、`1` 表示取消；传 `buttons` 可自定义。**没有 `confirmed` 字段。**

```javascript
const r = await fb.dialog.confirm({ title: '确认', message: '是否删除？' });
if (r.response === 0) { /* 用户点击确认 */ }
```

## fb.clipboard 剪贴板

### read()

读取剪贴板文本。

```javascript
const r = await fb.clipboard.read();
console.log(r.text);
```

### write(text)

写入文本到剪贴板。

```javascript
await fb.clipboard.write('复制的内容');
```

### writeHTML(html, plainText?)

写入 HTML 到剪贴板，可选提供纯文本备选。

```javascript
await fb.clipboard.writeHTML('<b>粗体</b>', '粗体');
```

### writeFiles(paths)

将文件路径列表写入剪贴板（用于粘贴文件）。

```javascript
await fb.clipboard.writeFiles(['E:\\Music\\a.flac', 'E:\\Music\\b.flac']);
```
