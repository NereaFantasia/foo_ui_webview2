# fb.metadata 元数据与封面写入

`fb.metadata` 用于读取和写入曲目标签、直接读取原始文件，以及管理嵌入式或同目录封面。标签写入方法异步派发，并通过 `metadata:writeComplete` 报告最终结果。

## read(path, opts?)

签名：`fb.metadata.read(path: string, opts?: Omit<MetadataReadParams, 'path'>): Promise<MetadataReadResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 曲目路径 |
| `opts.cueIndex` | `number` | 否 | CUE 或镜像内的曲目序号，从 1 开始。等同于路径后缀 `\|subsong:<n>`，两者都给时以该选项为准 |

返回 `{ success, path, tags, info }`。`tags` 的键保留文件里原样的大小写，每个值是 `string` 或 `string[]`。`info` 含 `duration`、`bitrate`、`sampleRate`、`channels` 和 `codec`。

宿主优先读取缓存信息；若未命中缓存或缓存中不含标题标签，则直接从底层音频文件读取。无法打开或读取失败的曲目返回 `OPERATION_FAILED`。

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
| `paths` | `string[]` | 是 | 要读取的曲目路径 |

返回 `results`，每个请求路径一个条目，另有汇总计数 `total`、`successCount` 和 `errorCount`。

各路径独立解析，可分别指定 `|subsong:N` 后缀；该方法不支持整批指定 `cueIndex`。读取成功的条目在 `tags` 中提供大写键名的扁平字段表（与 `readByPath()` 结构一致，但不包含 `path` 且不尝试从文件名推断音轨编号）。无法读取的路径条目标记为 `success: false` 并包含独立 `error`，不会导致整批调用失败。读取操作在宿主主线程同步执行；大量文件场景建议使用 `probeBatchAsync()`。

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
| `path` | `string` | 是 | 曲目路径 |
| `opts.cueIndex` | `number` | 否 | CUE 或镜像内的曲目序号，从 1 开始，同 `read()` |

返回扁平的 `metadata.readByPath` 对象：标签键和宿主的状态、路径字段并列在顶层。本方法不会调用 `metadata.readRaw`。

标签及技术信息字段均以大写键名表示（如 `TITLE`、`SAMPLERATE`）。多值标签类型为 `string[]`，技术信息字段为字符串。`DURATION` 始终存在，以秒为单位保留三位小数（如 `215.400`）。`FILESIZE` 仅在宿主已解析文件大小时返回；未入库索引的文件可能缺失该字段。若音频元数据未包含 `TRACKNUMBER` 标签，方法尝试提取文件名开头至多三位的前导数字（如 `07 - Song.flac`）；若无法提取则省略此字段。读取逻辑与错误处理与 `read()` 相同；规范化路径 `canonicalPath` 仅在文件无法打开时返回于错误信封中。

```javascript
const fields = await fb.metadata.readByPath('E:\\Music\\song.flac');
if (fields.success === false) throw new Error(fields.error);
console.log(fields.TITLE, fields.DURATION);
```

## removeField(path, field, opts?)

签名：`fb.metadata.removeField(path: string, field: string, opts?: Omit<MetadataRemoveFieldParams, 'path' | 'tags'>): Promise<MetadataRemoveFieldResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 曲目路径 |
| `field` | `string` | 是 | 要删除的单个标签名 |
| `opts.cueIndex` | `number` | 否 | CUE 或镜像内的曲目序号，从 1 开始；只作用于其中这一首，不作用于整个容器 |

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
| `path` | `string` | 是 | 曲目路径 |
| `tags` | `string[]` | 是 | 要删除的标签名 |
| `opts.cueIndex` | `number` | 否 | CUE 或镜像内的曲目序号，从 1 开始；只作用于其中这一首，不作用于整个容器 |

异步派发标签删除任务并返回操作回执。标签名自动转换为大写，`removedTags` 按输入顺序返回；若 `tags` 为空，操作立即成功返回且不派发任何写入任务。最终执行结果通过 `metadata:writeComplete` 事件通知。

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
| `path` | `string` | 是 | 曲目路径 |
| `tags` | `JsonObject` | 是 | 要改的标签；值为 `null`、空字符串或空数组时删除对应标签 |
| `opts.cueIndex` | `number` | 否 | CUE 或镜像内的曲目序号，从 1 开始；只把标签写到其中这一首，不写到整个容器 |

标签名自动转换为大写规范化，仅大小写不同的键视为同一标签。字符串值原样写入；数值转换为十进制文本（如 `2.5` 格式化为 `2.500000`）；布尔值与对象类型将被忽略。

非空字符串数组将替换该标签的所有现有值，保留元素顺序、重复项及空白字符，字符串内的逗号或分号不作为分隔符解析。空数组用于删除对应标签。若数组包含非字符串、空字符串或 NUL 字符，该曲目整体以 `INVALID_PARAMS` 失败且不派发任何字段变更；错误载荷中包含违规标签名及首个错误元素的从 0 计数下标。

数组写入时，`tagsApplied` 回显该数组；执行删除时（含传入空数组）标记为 `null`。`tagsSet` 与 `tagsRemoved` 统计受影响的字段数，而非值元素数量。受底层音频格式及标签写入器限制，多值标签支持可能存在差异，最终状态应以完成事件为准。

派发成功后返回操作回执，包含 `dispatched`、`handlePath`、`subsong`、`tagsApplied` 及受影响字段计数；若无可更新字段，则回执仅包含 `note: "No tags to update"`。`canonicalPath` 仅在文件无法打开时包含于失败信封中。回执仅代表任务受理，不代表磁盘写入已完成。

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
| `items` | `MetadataWriteBatchItem[]` | 是 | 逐曲目的标签修改：`path`、`tags`，以及可选的 `cueIndex` |

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
| `path` | `string` | 是 | 曲目路径 |
| `opts.cueIndex` | `number` | 否 | CUE 或镜像内的曲目序号，从 1 开始，同 `read()` |

`fb.metadata.readRaw(path, options?)` 绕过 metadb 缓存并直接读取文件。`options` 类型为 `Omit<MetadataReadRawParams, 'path'>`，可包含 `cueIndex`。返回的 `MetadataReadRawResponse` 是 `read()` 的各字段加上 `source`，`source` 恒为 `'file'`。读不出的文件以 `OPERATION_FAILED` 失败。

```javascript
const raw = await fb.metadata.readRaw('E:\\Music\\album.flac', {
	cueIndex: 2,
});
```

## 可取消的批量探测

`readBatch()` 在宿主主线程同步读取元数据，大量未入库文件可能导致界面短暂停顿且无法中途取消。`probeBatchAsync()` 将磁盘探测移至工作线程异步执行，支持中途取消，并提供细分的错误分类。

`read()`、`readBatch()`、`readRaw()` 及 `readByPath()` 针对未入库文件同样返回准确的 `duration`、`bitrate` 及 `sampleRate`；`probeBatchAsync()` 适用于避免主线程阻塞的大批量文件探测场景。

### probeBatchAsync(paths, options?)

立即返回 `{ success, operationId, totalCount }`。结果通过 `metadata:probeProgress` 分批到达，最后必有且仅有一次 `metadata:probeComplete`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `string[]` | 是 | 待探测路径，不能为空。每一条独立识别 `\|subsong:N` 后缀 |
| `options.includeTags` | `boolean` | 否 | 默认 `true`。为每条成功结果附带扁平标签表 |

每条结果是一个 `MetadataProbeResultItem`：

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `path` | `string` | 原样回显（含 `\|subsong:N`），可直接当查找键用 |
| `success` | `boolean` | 是否取到信息 |
| `infoSource` | `'cached' \| 'direct' \| 'none'` | `cached` 为 metadb 缓存命中，`direct` 为实际读盘，`none` 伴随失败出现 |
| `failure` | `'not-found' \| 'unsupported-format' \| 'read-error'` | 仅当 `success` 为 `false` 时存在 |
| `info` | `TrackTechnicalInfo` | `duration`、`bitrate`、`sampleRate`、`channels`、`codec` |
| `tags` | `Record<string, string \| string[]>` | 键名大写的扁平表，与 `readBatch()` 一致。`includeTags` 为 `false` 时不返回 |

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

取消操作会立即中断正在执行的磁盘读取。`metadata:probeComplete` 事件仍会派发并携带 `cancelled: true`；尚未开始处理的路径不会包含在后续事件中，被打断的当前条目不计入成功或失败计数。若 `cancelled` 为 `false`，表示指定操作已完成或不存在对应 ID。

### 分批与事件量

进度事件采用批量合并策略派发：每累计 64 条结果或距离上次派发达到 100 毫秒时触发一次。例如完全命中缓存的 10000 条路径将聚合为约 `ceil(10000 / 64)` 次事件派发。磁盘读取场景则依靠 100 毫秒的时间窗口保持平滑的进度反馈。最终批次的进度事件始终优先于 `metadata:probeComplete` 事件派发。

### 路径整批校验机制 {#路径校验是整批全过或整批拒绝}

`paths` 列表中的所有路径均在请求处理前接受宿主媒体读取策略校验。若其中任何一条路径未通过校验，整个调用直接拒绝并返回 `PERMISSION_DENIED`，不生成 `operationId`，不执行部分探测，也不会为单条路径返回 `invalid-path` 结果。如需处理混合来源路径，应在前端预先过滤校验。

若 `paths` 参数非数组或元素包含非字符串类型，前置校验将返回 `INVALID_PARAMS`。业务逻辑可根据返回的 `code` 区分处理。

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

- 省略 `target` 时默认仅写入嵌入式封面。
- `'embedded'` 写入音频文件内置标签容器；部分格式（如 CUE 关联文件）可能不支持。
- `'file'` 写入同目录旁注图片（如 `cover.<ext>`），文件扩展名根据图片实际字节自动推断。路径中的 `|subsong:N` 后缀将被自动剥除，确保同一 CUE 下的所有曲目引用同一封面文件。
- `['embedded', 'file']` 或 `'all'` 同时写入内置封面与同目录文件。返回结果分别通过 `results.embedded` 与 `results.file` 提供各目标的执行状态；仅当两个写入目标均失败时，整体调用才返回 `OPERATION_FAILED`。
- `filename` 仅适用于同目录文件写入；参数中禁止包含路径分隔符或 `..`。

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

`fb.metadata.removeEmbeddedArt()` 接受 `type` 与 `removeAll` 参数。省略 `type` 或设置 `removeAll: true` 时将移除所有嵌入图片。成功响应包含 `removedTypes`：全量删除时返回 `['all']`，否则列出已移除的具体封面类型。若当前音频文件格式不支持专辑封面编辑，调用返回 `NOT_SUPPORTED`。

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
