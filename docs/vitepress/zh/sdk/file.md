# fb.file file

本页是 `fb.file` 的 SDK 视角文档入口。

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## SDK 方法 stub

> 该区块用于补齐 SDK 视角方法覆盖，后续可人工扩展为完整示例与最佳实践。

### copy()

签名：`fb.file.copy(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `file.copy` 调用结果。

```javascript
const result = await fb.file.copy();
```

### delete()

签名：`fb.file.delete(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `file.delete` 调用结果。

```javascript
const result = await fb.file.delete();
```

### exists()

签名：`fb.file.exists(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `file.exists` 调用结果。

```javascript
const result = await fb.file.exists();
```

### getInfo()

签名：`fb.file.getInfo(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `file.getInfo` 调用结果。

```javascript
const result = await fb.file.getInfo();
```

### list()

签名：`fb.file.list(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `file.list` 调用结果。

```javascript
const result = await fb.file.list();
```

### mkdir()

签名：`fb.file.mkdir(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `file.mkdir` 调用结果。

```javascript
const result = await fb.file.mkdir();
```

### move()

签名：`fb.file.move(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `file.move` 调用结果。

```javascript
const result = await fb.file.move();
```

### read()

签名：`fb.file.read(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `file.read`, `file.write` 调用结果。

```javascript
const result = await fb.file.read();
```

### write(path, content, options?)

签名：`fb.file.write(path: string, content: string, options?: Omit<FileWriteParams, 'path' | 'content'>): Promise<BaseResponse & { bytesWritten?: number }>`

将文本写入文件。`options.encoding` 默认为 `utf-8`；设置 `options.append` 可追加内容而不是覆盖文件。

```javascript
await fb.file.write('C:\\Logs\\theme.log', '主题已初始化\n', { append: true });
```

## 二进制 payload

应用代码已有字节或 Data URL 时，应优先使用新增的 additive helper：

- `readBinary(path): Promise<Uint8Array>` 解码 host 响应。
- `writeBinary(path, bytes, options?)` 接受 `ArrayBuffer | Uint8Array`。
- `writeDataUrl(path, dataUrl, options?)` 接受规范的 Base64 Data URL，先严格
	校验，再只写入 payload。media type 会被校验，但不会决定目标文件扩展名。

```javascript
const bytes = await fb.file.readBinary('C:\\Config\\icon.ico');
await fb.file.writeBinary('C:\\Config\\icon-copy.ico', bytes);

await fb.file.writeDataUrl('C:\\Config\\cover.png', coverDataUrl);
```

这些 helper 只是适配已有 Host 契约，不会增加新 Host 端点。底层 `read()` 与
`write()` 仍然可用，并保持原始 wire 行为：

- `read(path, { encoding: 'binary' })` 在 `content` 返回 Base64，并将返回值
	的 `encoding` 设为 `'base64'`；该 payload 不带 `base64:` 前缀。
- `write(path, content, { encoding: 'binary' })` 只有在 `content` 以
	`base64:` 开头时才会解码。
- Data URL（`data:image/...;base64,...`）和 `fb2k://` URL 都不是二进制文件
	payload，不能直接传给 binary `write`。

```javascript
const source = await fb.file.read('C:\\Config\\icon.ico', { encoding: 'binary' });
await fb.file.write('C:\\Config\\icon-copy.ico', `base64:${source.content}`, {
		encoding: 'binary',
});
```

### rename()

签名：`fb.file.rename(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `file.rename` 调用结果。

```javascript
const result = await fb.file.rename();
```

<!-- END AUTO-GENERATED SDK STUBS -->
