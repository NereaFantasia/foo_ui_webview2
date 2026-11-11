# fb.clipboard 剪贴板

`fb.clipboard` 经宿主进程读写剪贴板上的文本、HTML 和文件列表。

## read()

签名：`fb.clipboard.read(): Promise<ClipboardReadResponse>`

读取系统剪贴板内容，返回剪贴板数据格式标志（是否存在文本、文件列表或图片）以及提取的文本内容与文件路径列表。

```javascript
const res = await fb.clipboard.read();
if (res.success === false) throw new Error(res.error);
const { text } = res;
```

## write(text)

签名：`fb.clipboard.write(text: string): Promise<ClipboardWriteResponse>`

把纯文本写入剪贴板。

```javascript
await fb.clipboard.write('来自主题的文本');
```

## writeFiles(paths)

签名：`fb.clipboard.writeFiles(paths: string[]): Promise<ClipboardWriteFilesResponse>`

将文件路径列表以原生文件拖拽格式写入剪贴板，支持粘贴到资源管理器及其他支持文件操作的外部应用程序。

```javascript
const result = await fb.clipboard.writeFiles([
	'C:\\Music\\one.flac',
	'C:\\Music\\two.flac'
]);
```

## writeHTML(html, plainText?)

签名：`fb.clipboard.writeHTML(html: string, plainText?: string): Promise<ClipboardWriteHTMLResponse>`

以 `HTML Format` 格式将富文本写入剪贴板，同时写入纯文本回退内容：指定非空 `plainText` 时采用该文本，若缺省或为空字符串则以 HTML 源码作为纯文本回退。

```javascript
await fb.clipboard.writeHTML('<strong>正在播放</strong>', '正在播放');
```
