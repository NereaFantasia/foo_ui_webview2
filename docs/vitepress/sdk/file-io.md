# 文件与网络

涵盖 `fb.file`、`fb.http`、`fb.dialog`、`fb.clipboard` 四个命名空间。

## fb.file 文件系统 

### read(path, options?) 

读取文件内容。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| path | string | 文件路径 |
| options | object | 可选，如 { encoding: 'utf-8' } |

```javascript
const content = await fb.file.read('E:\\\\config.json');
```

### write(path, content, options?) 

写入文件内容。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| path | string | 文件路径 |
| content | string | 文件内容 |
| options | object | 可选，如 { encoding: 'utf-8' } |

```javascript
await fb.file.write('E:\\\\output.txt', 'Hello World');
```

### exists(path) 

检查文件是否存在。返回 `{exists: boolean}`。

```javascript
const r = await fb.file.exists('E:\\\\Music\\\\song.flac');
if (r.exists) { /* ... */ }
```

### list(path, options?) 

列出目录内容。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| path | string | 目录路径 |
| options | object | 可选，如 { recursive: true } |

```javascript
const r = await fb.file.list('E:\\\\Music');
// r.files, r.directories
```

### delete(path) 

删除文件。

```javascript
await fb.file.delete('E:\\\\temp\\\\cache.dat');
```

### mkdir(path) 

创建目录（含递归创建父目录）。

```javascript
await fb.file.mkdir('E:\\\\output\\\\logs');
```

### copy(source, destination) 

复制文件。

```javascript
await fb.file.copy('E:\\\\src.txt', 'E:\\\\backup\\\\src.txt');
```

### move(source, destination) 

移动文件。

```javascript
await fb.file.move('E:\\\\old\\\\file.txt', 'E:\\\\new\\\\file.txt');
```

### rename(path, newName) 

重命名文件。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| path | string | 当前路径 |
| newName | string | 新文件名（仅名称，非完整路径） |

```javascript
await fb.file.rename('E:\\\\Music\\\\old.mp3', 'new.mp3');
```

### getInfo(path) 

获取文件信息（大小、修改时间等）。

```javascript
const info = await fb.file.getInfo('E:\\\\Music\\\\song.flac');
// info.size, info.lastModified, ...
```

## fb.http HTTP 请求 

### get(url, options?) 

发送 GET 请求。

```javascript
const r = await fb.http.get('https://api.example.com/data');
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

发送 HEAD 请求。仅获取响应头。

```javascript
const r = await fb.http.head('https://example.com/file.zip');
// r.headers['content-length']
```

### download(url, saveTo, options?) 

下载文件到本地。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| url | string | 下载 URL |
| saveTo | string | 保存路径 |
| options | object | 可选配置 |

```javascript
await fb.http.download('https://example.com/cover.jpg', 'E:\\\\covers\\\\cover.jpg');
```

### put(url, body, options?) 

发送 PUT 请求。参数与 `post` 相同。

### delete(url, body?, options?) 

发送 DELETE 请求。

### patch(url, body, options?) 

发送 PATCH 请求。参数与 `post` 相同。

### abort(requestId) 

中止正在进行的 HTTP 请求。`requestId` 由请求方法返回。

## fb.dialog 对话框 

### openFile(options?) 

打开文件选择对话框。返回用户选择的文件路径。

```javascript
const r = await fb.dialog.openFile({ filter: '音频文件|*.flac;*.mp3' });
// r.path
```

### saveFile(options?) 

打开文件保存对话框。

```javascript
const r = await fb.dialog.saveFile({ defaultName: 'playlist.m3u8' });
// r.path
```

### openFolder(options?) 

打开文件夹选择对话框。

```javascript
const r = await fb.dialog.openFolder({ title: '选择音乐目录' });
// r.path
```

### confirm(options?) 

显示确认对话框。返回 `{result: boolean}`。

```javascript
const r = await fb.dialog.confirm({ title: '确认', message: '是否删除？' });
if (r.result) { /* 用户点击确认 */ }
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
await fb.clipboard.writeFiles(['E:\\\\Music\\\\a.flac', 'E:\\\\Music\\\\b.flac']);
```
