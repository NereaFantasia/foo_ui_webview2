# fb.clipboard 剪贴板

`fb.clipboard` 经宿主进程读写剪贴板上的文本、HTML 和文件列表。

## read()

签名：`fb.clipboard.read(): Promise<ClipboardReadResponse>`

读取剪贴板：告知其中有没有文本、文件列表与图片，并给出文本与文件路径。

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

写入文件路径列表，可粘贴到能接收文件的程序里。

```javascript
const result = await fb.clipboard.writeFiles([
	'C:\\Music\\one.flac',
	'C:\\Music\\two.flac'
]);
```

## writeHTML(html, plainText?)

签名：`fb.clipboard.writeHTML(html: string, plainText?: string): Promise<ClipboardWriteHTMLResponse>`

以 `HTML Format` 把 HTML 写入剪贴板，同时写入纯文本回退：`plainText` 非空时用它，缺省或为空时用 HTML 文本本身。

```javascript
await fb.clipboard.writeHTML('<strong>正在播放</strong>', '正在播放');
```
