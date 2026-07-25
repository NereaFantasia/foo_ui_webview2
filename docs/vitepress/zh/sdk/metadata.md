# fb.metadata metadata

`fb.metadata` 用于读取和写入曲目标签、直接读取原始文件，以及管理嵌入式或同目录封面。标签写入方法异步派发，并通过 `metadata:writeComplete` 报告最终结果。

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## SDK 方法 stub

> 该区块用于补齐 SDK 视角方法覆盖，后续可人工扩展为完整示例与最佳实践。

### read()

签名：`fb.metadata.read(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `metadata.read` 调用结果。

```javascript
const result = await fb.metadata.read();
```

### readBatch()

签名：`fb.metadata.readBatch(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `metadata.readBatch` 调用结果。

```javascript
const result = await fb.metadata.readBatch();
```

### readByPath()

签名：`fb.metadata.readByPath(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `metadata.readByPath`, `metadata.readRaw` 调用结果。

```javascript
const result = await fb.metadata.readByPath();
```

### removeField()

签名：`fb.metadata.removeField(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `metadata.removeField` 调用结果。

```javascript
const result = await fb.metadata.removeField();
```

### removeTag()

签名：`fb.metadata.removeTag(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `metadata.removeTag` 调用结果。

```javascript
const result = await fb.metadata.removeTag();
```

### write()

签名：`fb.metadata.write(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `metadata.write` 调用结果。

```javascript
const result = await fb.metadata.write();
```

### writeBatch()

签名：`fb.metadata.writeBatch(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `metadata.embedArtwork`, `metadata.removeEmbeddedArt` 调用结果。

```javascript
const result = await fb.metadata.writeBatch();
```

<!-- END AUTO-GENERATED SDK STUBS -->

## 直接读取文件

`fb.metadata.readRaw(path, options?)` 绕过 metadb 缓存并直接读取文件。`options` 类型为 `Omit<MetadataReadRawParams, 'path'>`，可包含 `cueIndex`。返回的 `MetadataReadRawResponse` 在提供 `source` 时其值为 `'file'`。

```javascript
const raw = await fb.metadata.readRaw('E:\\Music\\album.flac', {
	cueIndex: 2,
});
```

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

- `'embedded'` 通过主机标签容器写入；CUE 等格式可能不支持。
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
