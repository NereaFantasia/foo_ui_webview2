# fb.metadata 元数据与封面写入

`fb.metadata` 用于读取和写入曲目标签、直接读取原始文件，以及管理嵌入式或同目录封面。标签写入方法异步派发，并通过 `metadata:writeComplete` 报告最终结果。

## read(path, opts?)

签名：`fb.metadata.read(path: string, opts?: Omit<MetadataReadParams, 'path'>): Promise<MetadataReadResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 曲目路径。 |
| `opts.cueIndex` | `number` | 否 | CUE 或镜像内的曲目序号，从 1 开始。等同于路径后缀 `\|subsong:<n>`，两者都给时以该选项为准。 |

返回 `{ success, path, tags, info }`。`tags` 的键保留文件里原样的大小写，每个值是 `string` 或 `string[]`。`info` 含 `duration`、`bitrate`、`sampleRate`、`channels` 和 `codec`。

宿主先读缓存的信息，拿不到或其中没有标题标签时改为直接读文件。打不开或读不出的曲目以 `OPERATION_FAILED` 失败。

```javascript
const result = await fb.metadata.read('E:\\Music\\song.flac');
if (result.success === false) throw new Error(result.error);
console.log(result.tags, result.info.sampleRate);

// CUE 里的第 3 首
const track3 = await fb.metadata.read('E:\\Music\\album.cue', { cueIndex: 3 });
```

## readBatch(paths)

签名：`fb.metadata.readBatch(paths: string[]): Promise<MetadataReadBatchResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `string[]` | 是 | 要读取的曲目路径。 |

返回 `results`，每个请求路径一个条目，另有汇总计数 `total`、`successCount` 和 `errorCount`。

每个路径独立解析，可以各带 `|subsong:N` 后缀；没有整批的 `cueIndex`。成功的行在 `tags` 里放键名大写的扁平字段表，同 `readByPath()` 的返回，但不带 `path`，也不从文件名取曲目号。读不出的路径那一行为 `success: false`，带自己的 `error`，不会让整次调用失败。读取在宿主主线程上进行；文件多时改用 `probeBatchAsync()`。

```javascript
const result = await fb.metadata.readBatch([
	'E:\\Music\\one.flac',
	'E:\\Music\\two.flac',
]);
```

## readByPath(path, opts?)

签名：`fb.metadata.readByPath(path: string, opts?: Omit<MetadataReadByPathParams, 'path'>): Promise<MetadataReadByPathResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 曲目路径。 |
| `opts.cueIndex` | `number` | 否 | CUE 或镜像内的曲目序号，从 1 开始，同 `read()`。 |

返回扁平的 `metadata.readByPath` 对象：标签键和宿主的状态、路径字段并列在顶层。本方法不会调用 `metadata.readRaw`。

每个标签和技术信息字段都以大写名为键，如 `TITLE`、`SAMPLERATE`。多值标签是 `string[]`，技术信息字段是字符串。`DURATION` 总会出现，以秒计，保留三位小数（`215.400`）。`FILESIZE` 只在宿主知道文件大小时出现，所以媒体库还没索引的文件没有它。文件没有 `TRACKNUMBER` 标签时，从文件名开头不超过三位的数字取得（`07 - Song.flac`），取不到时不出现。读取方式和失败情形同 `read()`；`canonicalPath` 只出现在打不开文件时的失败信封里。

```javascript
const fields = await fb.metadata.readByPath('E:\\Music\\song.flac');
if (fields.success === false) throw new Error(fields.error);
console.log(fields.TITLE, fields.DURATION);
```

## removeField(path, field, opts?)

签名：`fb.metadata.removeField(path: string, field: string, opts?: Omit<MetadataRemoveFieldParams, 'path' | 'tags'>): Promise<MetadataRemoveFieldResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 曲目路径。 |
| `field` | `string` | 是 | 要删除的单个标签名。 |
| `opts.cueIndex` | `number` | 否 | CUE 或镜像内的曲目序号，从 1 开始；只作用于其中这一首，不作用于整个容器。 |

以 `tags: [field]` 派发 `metadata.removeField`。回执可能含 `dispatched`、`subsong`、`removedTags`、`removedCount` 和 `note`；最终结果由 `operation` 为 `'removeTag'` 的 `metadata:writeComplete` 报告。

```javascript
const receipt = await fb.metadata.removeField(
	'E:\\Music\\song.flac',
	'COMMENT',
);
```

## removeTag(path, tags, opts?)

签名：`fb.metadata.removeTag(path: string, tags: string[], opts?: Omit<MetadataRemoveTagParams, 'path' | 'tags'>): Promise<MetadataRemoveTagResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 曲目路径。 |
| `tags` | `string[]` | 是 | 要删除的标签名。 |
| `opts.cueIndex` | `number` | 否 | CUE 或镜像内的曲目序号，从 1 开始；只作用于其中这一首，不作用于整个容器。 |

异步派发删除并返回回执。标签名会转成大写，`removedTags` 按给出的顺序列出它们；`tags` 为空时不删除任何标签，直接成功，不派发。最终结果看 `metadata:writeComplete`。

`removeField()` 和 `removeTag()` 共用同一个宿主 handler，所以都接受同样的 `cueIndex` 选项。

```javascript
await fb.metadata.removeTag('E:\\Music\\song.flac', ['COMMENT', 'GROUPING']);

// 只清除 CUE 第 3 首的标签
await fb.metadata.removeTag('E:\\Music\\album.cue', ['COMMENT'], {
	cueIndex: 3,
});
```

## write(path, tags, opts?)

签名：`fb.metadata.write(path: string, tags: JsonObject, opts?: Omit<MetadataWriteParams, 'path' | 'tags'>): Promise<MetadataWriteResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 曲目路径。 |
| `tags` | `JsonObject` | 是 | 要改的标签；值为 `null`、空字符串或空数组时删除对应标签。 |
| `opts.cueIndex` | `number` | 否 | CUE 或镜像内的曲目序号，从 1 开始；只把标签写到其中这一首，不写到整个容器。 |

标签名会转成大写，所以只有大小写不同的键指同一个标签。字符串原样写入，整数或小数写成十进制文本（`2.5` 写成 `2.500000`），布尔或对象被忽略。

非空字符串数组替换该标签的全部值，保留顺序、重复值和空白，值里的逗号、分号不会被当作分隔符。空数组删除该标签。数组含非字符串、空字符串或 NUL 时，该曲目以 `INVALID_PARAMS` 失败，不派发它的任何字段；错误中包含标签名和从 0 起的元素下标。

数组写入时，`tagsApplied` 保留数组；删除时（含空数组）记为 `null`。`tagsSet`、`tagsRemoved` 按字段计数，不按值的数量计数。文件格式或标签写入器可能限制多值支持，最终写入结果仍须查看完成事件。

派发写入并返回回执，含 `dispatched`、`handlePath`、`subsong`、`tagsApplied` 和标签计数；没有可写内容时只有 `note: "No tags to update"`。`canonicalPath` 只出现在打不开文件时的失败信封里。回执不是最终的写入结果。

```javascript
await fb.metadata.write('E:\\Music\\song.flac', {
	TITLE: 'New title',
	ARTIST: ['First artist', 'Second artist'],
	COMMENT: null,
	GENRE: [],
});

// 给 CUE 的第 3 首写标签
await fb.metadata.write('E:\\Music\\album.cue', { TITLE: 'Track three' }, {
	cueIndex: 3,
});
```

## writeBatch(items)

签名：`fb.metadata.writeBatch(items: MetadataWriteBatchItem[]): Promise<MetadataWriteBatchResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `items` | `MetadataWriteBatchItem[]` | 是 | 逐曲目的标签修改：`path`、`tags`，以及可选的 `cueIndex`。 |

调用 `metadata.writeBatch`，返回 `successCount`、`failCount` 和逐路径的 `errors`。有任何条目失败时，调用以失败信封返回，信封里仍带这三个字段，其他条目照常派发。本方法不调用任何封面端点。

标签值规则与 `write()` 相同。非法数组使该条目的全部字段都不写入，其他合法条目照常派发；批量失败码为 `OPERATION_FAILED`，受影响路径列在 `errors` 中。

```javascript
const result = await fb.metadata.writeBatch([
	{ path: 'E:\\Music\\one.flac', tags: { GENRE: ['Ambient', 'Electronic'] } },
	{ path: 'E:\\Music\\two.flac', tags: { GENRE: 'Ambient' } },
]);
```

## 直接读取文件

### readRaw(path, opts?)

签名：`fb.metadata.readRaw(path: string, opts?: Omit<MetadataReadRawParams, 'path'>): Promise<MetadataReadRawResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 曲目路径。 |
| `opts.cueIndex` | `number` | 否 | CUE 或镜像内的曲目序号，从 1 开始，同 `read()`。 |

`fb.metadata.readRaw(path, options?)` 绕过 metadb 缓存并直接读取文件。`options` 类型为 `Omit<MetadataReadRawParams, 'path'>`，可包含 `cueIndex`。返回的 `MetadataReadRawResponse` 是 `read()` 的各字段加上 `source`，`source` 恒为 `'file'`。读不出的文件以 `OPERATION_FAILED` 失败。

```javascript
const raw = await fb.metadata.readRaw('E:\\Music\\album.flac', {
	cueIndex: 2,
});
```

## 可取消的批量探测

`readBatch()` 在宿主主线程上逐个读取，几百个未入库文件会把 UI 冻到读完为止，而且中途停不下来。`probeBatchAsync()` 做同一件事但没有这两个问题：读盘在 worker 线程、调用可取消、每个失败都有分类，不再合并成一条通用错误。

`read()`、`readBatch()`、`readRaw()`、`readByPath()` 对未入库文件同样返回真实的 `duration` / `bitrate` / `sampleRate`；`probeBatchAsync()` 用于批量大到阻塞主线程会成问题的场合。

### probeBatchAsync(paths, options?)

立即返回 `{ success, operationId, totalCount }`。结果通过 `metadata:probeProgress` 分批到达，最后必有且仅有一次 `metadata:probeComplete`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `string[]` | 是 | 待探测路径，不能为空。每一条独立识别 `\|subsong:N` 后缀。 |
| `options.includeTags` | `boolean` | 否 | 默认 `true`。为每条成功结果附带扁平标签表。 |

每条结果是一个 `MetadataProbeResultItem`：

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `path` | `string` | 原样回显（含 `\|subsong:N`），可直接当查找键用。 |
| `success` | `boolean` | 是否取到信息。 |
| `infoSource` | `'cached' \| 'direct' \| 'none'` | `cached` 为 metadb 缓存命中，`direct` 为实际读盘，`none` 伴随失败出现。 |
| `failure` | `'not-found' \| 'unsupported-format' \| 'read-error'` | 仅当 `success` 为 `false` 时存在。 |
| `info` | `TrackTechnicalInfo` | `duration`、`bitrate`、`sampleRate`、`channels`、`codec`。 |
| `tags` | `Record<string, string \| string[]>` | 键名大写的扁平表，与 `readBatch()` 一致。`includeTags` 为 `false` 时不返回。 |

```javascript
const off = fb.on('metadata:probeProgress', (event) => {
	console.log(`${event.done} / ${event.total}`);
	for (const item of event.results) {
		if (item.success) {
			console.log(item.path, item.infoSource, item.info.bitrate);
		} else {
			console.warn(item.path, item.failure);
		}
	}
});

fb.on('metadata:probeComplete', (event) => {
	console.log('done', event.successCount, event.failureCount, event.cancelled);
	off();
});

const receipt = await fb.metadata.probeBatchAsync(droppedPaths, {
	includeTags: false,
});
```

### cancelProbe(operationId)

```javascript
const res = await fb.metadata.cancelProbe(receipt.operationId);
if (res.success === false) throw new Error(res.error);
const { cancelled } = res;
```

取消会打断正在进行的读盘，而不是等它读完。`metadata:probeComplete` 仍会到达并带 `cancelled: true`；还没轮到的路径不会出现在任何结果里，被打断的那一条既不计成功也不计失败。`cancelled` 为 `false` 表示该操作已结束或从未存在 —— 这两种情况故意不可区分。

### 分批与事件量

进度事件分批发射，绝不逐条：累计 64 条或距上次发射满 100ms，先到者触发。所以 10000 条全缓存命中的批次大约收敛到 `ceil(10000 / 64)` 个事件。读盘为主的批次会用更多事件换一个持续走动的进度信号 —— 100ms 上界换来的正是这个。最后一批一定排在 `metadata:probeComplete` 之前。

### 路径校验是整批全过或整批拒绝

`paths` 的每一条都在 handler 执行**之前**过宿主的媒体读策略。任一条不过，整个调用就以 `PERMISSION_DENIED` 被拒且不产生 `operationId` —— 没有部分执行，也没有逐条的 `invalid-path` 结果。需要容忍混合批次就在页面侧先自行过滤。

同一道前置校验也会拒掉非数组的 `paths` 与非字符串的条目，但这类返回的是 `INVALID_PARAMS` 而不是 `PERMISSION_DENIED`。两者需要区别处理时按 `code` 分支。

### 已知边界

- 批量面不认 `read()` 仍然接受的 `#N` 旧式 subsong 写法。`#N` 会把「无扩展名且以 `#<数字>` 结尾」的文件名切错，而本端点的输入正是用户拖进来的任意文件名。请用 `|subsong:N`。
- `read()` / `readBatch()` / `readRaw()` / `readByPath()` 判断要不要重新读盘时只看有没有 `title` 标签，所以「缓存里有 title 但没有 `bitrate`」不会被重读。`probeBatchAsync()` 改用宿主自己的 partial-info 标志，没有这个缺口；那四个 API 保持原样。

## 封面

### 字节与 Data URL helper

图片已经是 `ArrayBuffer` 或 `Uint8Array` 时，优先使用
`embedArtworkBytes(path, bytes, options?)`。规范的 Base64 `data:image/*` URL
应使用 `embedArtworkFromDataUrl(path, dataUrl, options?)`；后者会在调用 Host
前拒绝非图片或畸形 Data URL。

两个 helper 都只负责生成或提取裸 Base64 `imageData`，并调用现有
`metadata.embedArtwork` 端点；原始 facade 行为保持不变。

```javascript
await fb.metadata.embedArtworkBytes(
	'E:\\Music\\song.flac',
	coverBytes,
	{ type: 'front', target: 'embedded' },
);

await fb.metadata.embedArtworkFromDataUrl(
	'E:\\Music\\song.flac',
	coverDataUrl,
	{ type: 'front', target: ['embedded', 'file'] },
);
```

### embedArtwork(path, options?)

`fb.metadata.embedArtwork()` 可将图片写入音频文件、写为同目录图片，或同时写入两个目标。`MetadataEmbedArtworkParams` 包含 `imageData`、`type`、`filename` 与 `target`。`target` 也可以传单个字符串，SDK 会把它包成单元素数组再发送。

`imageData` 只接受裸 Base64 payload，不能包含 `data:image/...;base64,`
头、`file.write` 专用的 `base64:` 标记或 `fb2k://` URL。

- 省略 `target` 时只写嵌入图片。
- `'embedded'` 通过宿主标签容器写入；CUE 等格式可能不支持。
- `'file'` 写入 `cover.<ext>` 等同目录图片，扩展名根据图片字节推断。`|subsong:N` 后缀会被去掉，所以同一个 CUE 的全部曲目共用一张图。
- `['embedded', 'file']` 或 `'all'` 同时执行两个目标。此时结果在 `results.embedded` 和 `results.file` 下分别给出各目标的结果，只有两者都失败时调用才以 `OPERATION_FAILED` 失败。
- `filename` 只作用于文件输出；路径分隔符和 `..` 会被拒绝。

`type` 默认为 `'front'`。图片数据解不出任何字节时以 `INVALID_PARAMS` 失败。

```javascript
const comma = coverDataUrl.indexOf(',');
const coverBase64 = coverDataUrl.slice(comma + 1);
const result = await fb.metadata.embedArtwork(
	'E:\\Music\\song.flac',
	{
		imageData: coverBase64,
		type: 'front',
		target: ['embedded', 'file'],
	},
);
```

### removeEmbeddedArt(path, options?)

`fb.metadata.removeEmbeddedArt()` 通过 `MetadataRemoveEmbeddedArtParams` 接受 `type` 与 `removeAll`。省略 `type` 或设置 `removeAll: true` 时删除全部图片。响应可能包含 `removedTypes`：一次删掉全部图片时为 `['all']`，否则是删除的各个类型。专辑封面编辑器不支持的格式以 `NOT_SUPPORTED` 失败。

```javascript
await fb.metadata.removeEmbeddedArt('E:\\Music\\song.flac', {
	type: 'front',
});
```

## 异步完成与默认日志

`metadata.write`、`metadata.writeBatch`、`metadata.removeField` 和 `metadata.removeTag` 会在文件操作完成前返回派发回执。最终的 `metadata:writeComplete` 事件使用 `MetadataWriteCompletePayload`，包含 `operation`、`path`、`subsong`、`code`、`success` 与 `status`。

`write()` 和 `writeBatch()` 的 `operation` 为 `'write'`，`removeTag()` 和 `removeField()` 的为 `'removeTag'`。`code` 是 foobar2000 的完成代码（`0` 成功，`1` 中止，`2` 出错），`status` 按 `code` 取 `'success'`、`'aborted'` 或 `'error'`，`code` 为 `0` 时 `success` 为 `true`。该事件广播给所有窗口。

### disableDefaultLogger()

签名：`fb.metadata.disableDefaultLogger(): void`

SDK 默认安装一个监听器，把失败结果写入 JavaScript 控制台。如需自定义 UI 处理，可先调用 `fb.metadata.disableDefaultLogger()` 移除它；该操作可重复调用。

```javascript
fb.metadata.disableDefaultLogger();

const off = fb.on('metadata:writeComplete', (event) => {
	if (!event.success) {
		console.error(event.operation, event.path, event.status, event.code);
	}
});
```
