# fb.file file

本页是 `fb.file` 的 SDK 视角文档入口。

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## 其余方法

### copy()

封装 `file.copy`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.file.copy(/* 参数见 TypeScript 声明 */);
```

### delete()

封装 `file.delete`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.file.delete(/* 参数见 TypeScript 声明 */);
```

### exists()

封装 `file.exists`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.file.exists(/* 参数见 TypeScript 声明 */);
```

### getInfo()

封装 `file.getInfo`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.file.getInfo(/* 参数见 TypeScript 声明 */);
```

### list()

封装 `file.list`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.file.list(/* 参数见 TypeScript 声明 */);
```

### mkdir()

封装 `file.mkdir`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.file.mkdir(/* 参数见 TypeScript 声明 */);
```

### move()

封装 `file.move`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.file.move(/* 参数见 TypeScript 声明 */);
```

### read()

封装 `file.read`, `file.write`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.file.read(/* 参数见 TypeScript 声明 */);
```

### rename()

封装 `file.rename`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.file.rename(/* 参数见 TypeScript 声明 */);
```

<!-- END AUTO-GENERATED SDK STUBS -->

## 文本写入

`fb.file.write(path, content, options?)` 将文本写入文件，返回 `BaseResponse & { bytesWritten?: number }`。`options.encoding` 默认为 `utf-8`；设置 `options.append` 可追加内容而不是覆盖文件。

```javascript
await fb.file.write('C:\\Logs\\theme.log', '主题已初始化\n', { append: true });
```

## 二进制 payload

应用代码已有字节或 Data URL 时，应优先使用新增的 additive helper：

- `readBinary(path): Promise<Uint8Array>` 解码宿主响应。
- `writeBinary(path, bytes, options?)` 接受 `ArrayBuffer | Uint8Array`。
- `writeDataUrl(path, dataUrl, options?)` 接受规范的 Base64 Data URL，先严格
	校验，再只写入 payload。media type 会被校验，但不会决定目标文件扩展名。

```javascript
const bytes = await fb.file.readBinary('C:\\Config\\icon.ico');
await fb.file.writeBinary('C:\\Config\\icon-copy.ico', bytes);

await fb.file.writeDataUrl('C:\\Config\\cover.png', coverDataUrl);
```

这些 helper 只是适配既有宿主契约，不新增宿主端点。底层 `read()` 与
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
