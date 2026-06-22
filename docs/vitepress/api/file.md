# File & Dialog & Shell

安全的文件系统操作。所有路径支持变量替换：`%profile%`、`%component%`、`%music%`、`%APPDATA%`、`%TEMP%`。

::: warning 安全限制
读取仅允许白名单目录（profile/component/music/appdata/temp）。写入更严格，仅允许 profile/temp 目录。
:::

## File API - 文件系统

### file.read 

读取文件内容。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| path | string | ✓ | 文件路径（支持路径变量） |
| encoding | string | ✗ | "utf-8"（默认）或 "binary"（返回 base64） |

**返回值**: `{ "success": true, "content": "...", "size": 1024 }`

二进制模式额外返回 `"encoding": "base64"`。

```javascript
// 读取文本文件
const { content } = await fb2k.invoke('file.read', { path: '%profile%\\\\config.json' });

// 读取二进制文件
const bin = await fb2k.invoke('file.read', { path: '%profile%\\\\data.bin', encoding: 'binary' });
console.log(bin.encoding); // "base64"
```

### file.write 

写入文件内容。父目录不存在时自动创建。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| path | string | ✓ | 文件路径 |
| content | string | ✓ | 写入内容（二进制时使用 "base64:..." 前缀） |
| encoding | string | ✗ | "utf-8"（默认）或 "binary" |
| append | boolean | ✗ | 是否追加模式（默认 false） |

**返回值**: `{ "success": true, "bytesWritten": 1024 }`

```javascript
// 写入 JSON 配置
await fb2k.invoke('file.write', {
    path: '%profile%\\\\my-skin\\\\config.json',
    content: JSON.stringify({ theme: 'dark' })
});

// 追加日志
await fb2k.invoke('file.write', {
    path: '%profile%\\\\debug.log', content: 'log entry\\n', append: true
});
```

### file.exists 

检查文件或目录是否存在。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| path | string | ✓ | 文件或目录路径 |

**返回值**: `{ "exists": true, "isFile": true, "isDirectory": false }`

```javascript
const { exists, isFile } = await fb2k.invoke('file.exists', { path: '%profile%\\\\config.json' });
```

### file.list 

列出目录内容。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| path | string | ✓ | 目录路径 |
| pattern | string | ✗ | 通配符过滤（默认 "*"），如 "*.flac" |
| recursive | boolean | ✗ | 是否递归（默认 false） |

**返回值**: `{ "success": true, "files": [...], "directories": [...] }`

> 非递归模式下 `files` 返回文件名；递归模式下返回完整路径。

```javascript
// 列出配置目录下的 JSON 文件
const { files } = await fb2k.invoke('file.list', {
    path: '%profile%', pattern: '*.json'
});
```

### file.delete 

删除文件。默认移至回收站。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| path | string | ✓ | 文件路径 |
| moveToTrash | boolean | ✗ | 是否移至回收站（默认 true） |

**返回值**: `{ "success": true }`

```javascript
// 删除到回收站（安全）
await fb2k.invoke('file.delete', { path: '%profile%\\\\old-config.json' });

// 永久删除
await fb2k.invoke('file.delete', { path: '%temp%\\\\cache.tmp', moveToTrash: false });
```

### file.mkdir 

创建目录（支持多级创建）。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| path | string | ✓ | 目录路径 |

**返回值**: `{ "success": true, "created": true }`

### file.copy 

复制文件或目录（支持递归）。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| source | string | ✓ | 源路径 |
| destination | string | ✓ | 目标路径 |
| overwrite | boolean | ✗ | 是否覆盖（默认 false） |

**返回值**: `{ "success": true, "source": "...", "destination": "..." }`

```javascript
await fb2k.invoke('file.copy', {
    source: '%profile%\\\\config.json',
    destination: '%profile%\\\\config.bak.json'
});
```

### file.move 

移动文件或目录。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| source | string | ✓ | 源路径 |
| destination | string | ✓ | 目标路径 |

**返回值**: `{ "success": true, "source": "...", "destination": "..." }`

### file.rename 

重命名文件或目录。新名称不能包含路径分隔符。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| path | string | ✓ | 原始路径 |
| newName | string | ✓ | 新文件名 |

**返回值**: `{ "success": true, "oldPath": "...", "newPath": "..." }`

### file.getInfo 

获取文件或目录的详细信息。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| path | string | ✓ | 文件或目录路径 |

**返回值**:

```json
{
    "success": true,
    "exists": true,
    "isDirectory": false,
    "isFile": true,
    "size": 5242880,
    "modified": 1736064000000,
    "name": "song.flac",
    "extension": ".flac",
    "parent": "C:\\\\Music"
}
```

## Dialog API - 对话框 

系统原生对话框。

### dialog.openFile 

打开文件选择对话框。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| title | string | ✗ | 对话框标题（默认 "Open File"） |
| filters | array | ✗ | 文件过滤器，每项 { name, extensions } |
| multiple | boolean | ✗ | 允许多选（默认 false） |
| defaultPath | string | ✗ | 默认打开路径（支持 %music% 变量） |

**返回值**: `{ "canceled": false, "filePaths": ["C:\\\\Music\\\\song.mp3"] }`

用户取消时 `canceled` 为 `true`，`filePaths` 为空数组。

```javascript
const result = await fb2k.invoke('dialog.openFile', {
    title: '选择音频文件',
    filters: [{ name: 'Audio', extensions: ['mp3', 'flac', 'wav'] }],
    multiple: true,
    defaultPath: '%music%'
});
if (!result.canceled) {
    console.log('选中文件:', result.filePaths);
}
```

### dialog.saveFile 

打开文件保存对话框。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| title | string | ✗ | 对话框标题（默认 "Save File"） |
| defaultName | string | ✗ | 默认文件名 |
| filters | array | ✗ | 文件过滤器，每项 { name, extensions } |

**返回值**: `{ "canceled": false, "filePath": "C:\\\\Music\\\\export.json" }`

```javascript
const result = await fb2k.invoke('dialog.saveFile', {
    title: '导出播放列表',
    defaultName: 'playlist.json',
    filters: [{ name: 'JSON', extensions: ['json'] }]
});
if (!result.canceled) {
    await fb2k.invoke('file.write', { path: result.filePath, content: data });
}
```

### dialog.openFolder 

打开文件夹选择对话框。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| title | string | ✗ | 对话框标题（默认 "Select Folder"） |

**返回值**: `{ "canceled": false, "folderPath": "C:\\\\Music" }`

```javascript
const result = await fb2k.invoke('dialog.openFolder', { title: '选择音乐文件夹' });
if (!result.canceled) {
    console.log('选中文件夹:', result.folderPath);
}
```

### dialog.confirm 

显示确认对话框。使用 Windows TaskDialog 实现，支持自定义按钮。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| title | string | ✗ | 对话框标题（默认 "Confirm"） |
| message | string | ✓ | 提示消息 |
| type | string | ✗ | 图标类型: info, warning, error, question（默认 question） |
| buttons | string[] | ✗ | 按钮标签数组（默认 ["OK", "Cancel"]） |
| defaultButton | number | ✗ | 默认选中的按钮索引（默认 0） |

**返回值**: `{ "response": 0 }`

`response` 为用户点击的按钮索引（从 0 开始）。

```javascript
const { response } = await fb2k.invoke('dialog.confirm', {
    title: '确认删除',
    message: '确定要删除选中的曲目吗？',
    type: 'warning',
    buttons: ['删除', '取消']
});
if (response === 0) {
    // 用户点击了"删除"
}
```

## Shell API - 系统集成 

### shell.showInExplorer 

在资源管理器中显示文件（选中该文件）。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| path | string | ✓ | 文件路径 |

**返回值**: `{ "success": true }`

```javascript
await fb2k.invoke('shell.showInExplorer', { path: 'C:\\\\Music\\\\song.flac' });
```

### shell.openExternal 

用默认程序打开 URL 或文件。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| url | string | ✓ | URL 或文件路径 |

**返回值**: `{ "success": true }`

```javascript
await fb2k.invoke('shell.openExternal', { url: 'https://www.foobar2000.org' });
```

### shell.exec 

执行系统命令。

::: warning 安全限制
不限制可执行命令（信任主题作者，信任边界等同于安装一个 foobar2000 组件）。若提供 `cwd`，会经 PathSecurity 路径校验拒绝越界路径。破坏性文件操作请用 `fb.file.*`（受路径黑名单保护）。
:::

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| command | string | ✓ | 要执行的命令 |
| args | string[] | ✗ | 命令参数数组 |
| cwd | string | ✗ | 工作目录 |
| hidden | boolean | ✗ | 是否隐藏窗口（默认 true） |

**返回值**: `{ "success": true, "processId": 12345 }`

::: tip 行为说明
`shell.exec` 是 fire-and-forget 语义，只表示命令进程已发起，不保证目标服务已经就绪。
:::

```javascript
// 启动 Node.js 服务器
await fb2k.invoke('shell.exec', { command: 'cmd /c start /b node "E:\\\\server.js"' });

// 无命令白名单：任意命令均可执行（信任主题作者）
await fb2k.invoke('shell.exec', { command: 'curl http://example.com' });
```

### shell.spawn 

结构化启动进程（推荐用于启动本地服务）。

::: warning 安全限制
不限制可执行文件（信任主题作者）。绝对路径可执行文件与 `cwd` 会经路径安全校验，拒绝指向系统目录等越界路径。
:::

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| executable | string | ✓ | 可执行文件名或绝对路径 |
| args | string[] | ✗ | 参数数组 |
| cwd | string | ✗ | 工作目录 |
| hidden | boolean | ✗ | 是否隐藏窗口（默认 true） |
| waitForExitMs | number | ✗ | 启动后等待退出毫秒数（用于检测早退） |

**返回值**:

```json
{
  "success": true,
  "processId": 12345,
  "exited": false
}
```

```javascript
// ✅ 推荐：直接启动 node server.js（可检测 CreateProcess 失败）
const result = await fb2k.invoke('shell.spawn', {
  executable: 'E:\\\\FB2K\\\\Runtime\\\\node.exe',
  args: ['E:\\\\FB2K\\\\NeteaseApi\\\\server.js'],
  cwd: 'E:\\\\FB2K\\\\NeteaseApi',
  hidden: true,
  waitForExitMs: 900
});

if (result.success === false) {
  console.error(result.error, result.exitCode);
}
```

### shell.openWith 

使用系统默认程序打开文件。

::: danger 安全限制
禁止打开可执行文件（.exe/.bat/.cmd 等 30+ 种扩展名）。
:::

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| path | string | ✓ | 文件路径（不能是可执行文件） |

**返回值**: `{ "success": true }`

```javascript
await fb2k.invoke('shell.openWith', { path: 'C:\\\\Music\\\\notes.txt' });
```
