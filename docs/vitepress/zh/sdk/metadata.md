# fb.metadata metadata

`fb.metadata` 用于读取和写入曲目标签、直接读取原始文件，以及管理嵌入式或同目录封面。标签写入方法异步派发，并通过 `metadata:writeComplete` 报告最终结果。

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## 其余方法

### read()

封装 `metadata.read`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
const result = await fb.metadata.read('E:\\Music\\song.flac');
```

### readBatch()

封装 `metadata.readBatch`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
const result = await fb.metadata.readBatch(['E:\\Music\\a.flac', 'E:\\Music\\b.flac']);
```

### readByPath()

封装 `metadata.readByPath`（不会调用 `metadata.readRaw`）。

```javascript
const result = await fb.metadata.readByPath('E:\\Music\\song.flac');
```

### removeField()

封装 `metadata.removeField`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
const result = await fb.metadata.removeField('E:\\Music\\song.flac', 'COMMENT');
```

### removeTag()

封装 `metadata.removeTag`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
const result = await fb.metadata.removeTag('E:\\Music\\song.flac', ['COMMENT', 'LYRICS']);
```

### write()

封装 `metadata.write`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
const result = await fb.metadata.write('E:\\Music\\song.flac', { COMMENT: 'nice' });
```

### writeBatch()

封装 `metadata.writeBatch`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
const result = await fb.metadata.writeBatch([
    { path: 'E:\\Music\\a.flac', tags: { COMMENT: 'nice' } },
]);
```

<!-- END AUTO-GENERATED SDK STUBS -->

## 直接读取文件

`fb.metadata.readRaw(path, options?)` 绕过 metadb 缓存并直接读取文件。`options` 类型为 `Omit<MetadataReadRawParams, 'path'>`，可包含 `cueIndex`。返回的 `MetadataReadRawResponse` 在提供 `source` 时其值为 `'file'`。

```javascript
const raw = await fb.metadata.readRaw('E:\\Music\\album.flac', {
	cueIndex: 2,
});
```

## 可取消的批量探测

`readBatch()` 在宿主主线程上逐个读取，几百个未入库文件会把 UI 冻到读完为止，而且中途停不下来。`probeBatchAsync()` 做同一件事但没有这两个问题：读盘在 worker 线程、调用可取消、每个失败都有分类，不再合并成一条通用错误。

它是新增面而非替代：`read()`、`readBatch()`、`readRaw()`、`readByPath()` 行为不变，而且这四个本来就能对未入库文件返回真实的 `duration` / `bitrate` / `sampleRate`。

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
const { cancelled } = await fb.metadata.cancelProbe(receipt.operationId);
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

`fb.metadata.embedArtwork()` 可将图片写入音频文件、写为同目录图片，或同时写入两个目标。`MetadataEmbedArtworkParams` 包含 `imageData`、`type`、`filename` 与 `target`。

`imageData` 只接受裸 Base64 payload，不能包含 `data:image/...;base64,`
头、`file.write` 专用的 `base64:` 标记或 `fb2k://` URL。

- `'embedded'` 通过宿主标签容器写入；CUE 等格式可能不支持。
- `'file'` 写入 `cover.<ext>` 等同目录图片，扩展名根据图片字节推断。
- `['embedded', 'file']` 按 SDK 声明类型同时执行两个目标。
- `filename` 只作用于文件输出；路径分隔符和 `..` 会被拒绝。

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

`fb.metadata.removeEmbeddedArt()` 通过 `MetadataRemoveEmbeddedArtParams` 接受 `type` 与 `removeAll`；响应可能包含 `removedTypes`。

```javascript
await fb.metadata.removeEmbeddedArt('E:\\Music\\song.flac', {
	type: 'front',
});
```

## 异步完成与默认日志

`metadata.write`、`metadata.removeField`、`metadata.removeTag` 及其批量变体会在文件操作完成前返回派发回执。最终的 `metadata:writeComplete` 事件使用 `MetadataWriteCompletePayload`，包含 `operation`、`path`、`subsong`、`code`、`success` 与 `status`。

SDK 默认安装一个监听器，把失败结果写入 JavaScript 控制台。如需自定义 UI 处理，可先调用 `fb.metadata.disableDefaultLogger()` 移除它；该操作可重复调用。

```javascript
fb.metadata.disableDefaultLogger();

const off = fb.on('metadata:writeComplete', (event) => {
	if (!event.success) {
		console.error(event.operation, event.path, event.status, event.code);
	}
});
```
