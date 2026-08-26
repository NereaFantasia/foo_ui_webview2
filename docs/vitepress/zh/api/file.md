# File & Dialog & Shell

安全的文件系统操作。所有路径支持变量替换：`%profile%`、`%component%`、`%music%`、`%APPDATA%`、`%TEMP%`。

每个端点走哪一档路径校验、各档实际放行什么，见[权限系统](/zh/reference/permissions)。

## File API - 文件系统

### file.read

读取文件内容。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `path` | `string` | 是 | — |  |
| `encoding` | `string` | 否 | `utf-8` | 传 `binary` 时 `content` 返回裸 Base64。 |

**返回值**: `{"content":"...","encoding":"...","size":0,"success":true}`

二进制模式额外返回 `"encoding": "base64"`。

二进制读取时，`content` 是**不带 `base64:` 前缀的裸 Base64 payload**；它是传输表示，不是文本，也不是 Data URL。要把读取结果原样写回，必须在写入时补上 `base64:`，并同时保持 `encoding: 'binary'`。

```javascript
// 读取文本文件
const { content } = await fb2k.invoke('file.read', { path: '%profile%\\config.json' });

// 读取二进制文件
const bin = await fb2k.invoke('file.read', { path: '%profile%\\data.bin', encoding: 'binary' });
console.log(bin.encoding); // "base64"
```

### file.write

写入文件内容。父目录不存在时自动创建。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `path` | `string` | 是 | — |  |
| `content` | `string` | 否 | — | 二进制内容须以 `base64:` 前缀开头（见下）。 |
| `encoding` | `string` | 否 | `utf-8` | 须精确为 `binary` 才启用 Base64 解码分支。 |
| `append` | `boolean` | 否 | `false` | 追加到文件末尾而非覆盖。 |

**返回值**: `{ "success": true, "bytesWritten": 1024 }`

二进制写入只有在以下两个条件同时满足时才会解码：`encoding` 必须精确为 `'binary'`，且 `content` 必须以 `base64:` 开头。该前缀是 Bridge wire 标记，解码前会被移除。裸 Base64、`data:image/...;base64,...` Data URL 或 `fb2k://` 封面 URL 都不会进入该解码分支；调用仍可能返回 `success: true`，但文件内容会错误。

```javascript
// 写入 JSON 配置
await fb2k.invoke('file.write', {
    path: '%profile%\\my-skin\\config.json',
    content: JSON.stringify({ theme: 'dark' })
});

// 追加日志
await fb2k.invoke('file.write', {
    path: '%profile%\\debug.log', content: 'log entry\\n', append: true
});

// binary read → write：必须补回 base64: wire 前缀
const binary = await fb2k.invoke('file.read', {
    path: '%profile%\\data.bin', encoding: 'binary'
});
await fb2k.invoke('file.write', {
    path: '%profile%\\data-copy.bin',
    content: `base64:${binary.content}`,
    encoding: 'binary'
});
```

### file.exists

检查文件或目录是否存在。

| 参数 | 类型 | 必填 |
| --- | --- | --- |
| `path` | `string` | 是 |

**返回值**: `{ "exists": true, "isFile": true, "isDirectory": false }`

```javascript
const { exists, isFile } = await fb2k.invoke('file.exists', { path: '%profile%\\config.json' });
```

### file.list

列出目录内容。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `path` | `string` | 是 | — |  |
| `pattern` | `string` | 否 | `*` | 只认 `*`、`*.*` 和单个扩展名通配（如 `*.json`），任何写法都不会报错。**不以 `*.` 开头**的写法（`song*`、`?.txt`、`data`）静默匹配全部文件；**以 `*.` 开头**的写法会把 `*` 之后的部分与文件名最后一个点起的文本逐字比较，所以复合写法如 `*.{flac,mp3}` 通常一个都匹配不上 —— 无扩展名的文件对任何 `*.ext` 也匹配不上。 |
| `recursive` | `boolean` | 否 | `false` | 递归时 `files` 返回完整路径而非文件名。 |

**返回值**: `{"directories":[],"files":[],"items":[],"success":true}`

> 非递归模式下 `files` 返回文件名；递归模式下返回完整路径。

```javascript
// 列出配置目录下的 JSON 文件
const { files } = await fb2k.invoke('file.list', {
    path: '%profile%', pattern: '*.json'
});
```

### file.delete

删除文件。默认移至回收站。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `path` | `string` | 是 | — |  |
| `moveToTrash` | `boolean` | 否 | `true` | 为 `false` 时绕过回收站永久删除。 |

**返回值**: `{ "success": true }`

```javascript
// 删除到回收站（安全）
await fb2k.invoke('file.delete', { path: '%profile%\\old-config.json' });

// 永久删除
await fb2k.invoke('file.delete', { path: '%temp%\\cache.tmp', moveToTrash: false });
```

### file.mkdir

创建目录（支持多级创建）。

| 参数 | 类型 | 必填 |
| --- | --- | --- |
| `path` | `string` | 是 |

**返回值**: `{"created":"...","message":"...","success":true}`

### file.copy

复制文件或目录（支持递归）。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `source` | `string` | 是 | — |  |
| `destination` | `string` | 是 | — |  |
| `overwrite` | `boolean` | 否 | `false` | 目标已存在时是否覆盖。 |

**返回值**: `{ "success": true, "source": "...", "destination": "..." }`

```javascript
await fb2k.invoke('file.copy', {
    source: '%profile%\\config.json',
    destination: '%profile%\\config.bak.json'
});
```

### file.move

移动文件或目录。

| 参数 | 类型 | 必填 |
| --- | --- | --- |
| `source` | `string` | 是 |
| `destination` | `string` | 是 |

**返回值**: `{ "success": true, "source": "...", "destination": "..." }`

跨卷移动**文件**会成功 —— Windows 会自动复制后删除。跨卷移动**目录**失败，返回 `code: "NOT_SUPPORTED"` 与 `details.reason: "cross-volume"`。

### file.rename

重命名文件或目录。新名称不能包含路径分隔符。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 |  |
| `newName` | `string` | 是 | 不能包含路径分隔符。 |

**返回值**: `{ "success": true, "oldPath": "...", "newPath": "..." }`

### file.getInfo

获取文件或目录的详细信息。

| 参数 | 类型 | 必填 |
| --- | --- | --- |
| `path` | `string` | 是 |

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
    "parent": "C:\\Music"
}
```

### file.copyAsync

可取消的批量复制，落在 worker 线程上，不阻塞 UI。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `items` | `array<object>` | 是 | — | `{ source, destination }` 数组。缺参或空数组返回 `INVALID_PARAMS`；元素不是对象、或 `source` / `destination` 缺失或非字符串同样如此。逐条路径校验是 fail-fast：`source` 按 `Read` 档、`destination` 按 `FileWrite` 档校验，任一条被拒即整批失败，返回 `PERMISSION_DENIED` 且不产生 `operationId`；**空串也落在这里**，因为路径校验跑在 handler 之前。 |
| `overwrite` | `boolean` | 否 | `false` | 目标已存在时覆盖而非跳过。传非布尔值返回 `INVALID_PARAMS`。 |

**返回值**: `{"operationId":"fileop_...","success":true,"totalCount":2}`

返回值只是派工回执；结果经 `file:opProgress`（分批）与最后一条 `file:opComplete` 送达。结果按**条目**上报而非按文件：目录条目在整棵树走完后只出一条结果。全进程同时进行的操作上限为 8 个，超出时调用返回 `OPERATION_FAILED`。

目录复制到已存在的目录会**并入**其中：该条目不会被判成 `already-exists`，目录内已存在的文件被跳过且不单独上报（除非传了 `overwrite`），该条目仍记 `status: "ok"`。`already-exists` 跳过只适用于单文件条目。

```javascript
const receipt = await fb2k.invoke('file.copyAsync', {
    items: [{ source: 'C:\\Music\\Album', destination: 'D:\\Backup\\Album' }]
});
```

### file.moveAsync

可取消的批量移动，内建跨卷回退。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `items` | `array<object>` | 是 | — | `{ source, destination }` 数组，校验规则同 `file.copyAsync`，区别是两端都按 `FileWrite` 档校验 —— 移动会删掉 source。 |
| `overwrite` | `boolean` | 否 | `false` | 只对**文件**目标生效；同步版 `file.move` 对文件目标无条件覆盖，本方法的默认值不是。已存在的**目录**目标永远不会被替换 —— Windows 不支持原地换掉一个目录，同卷下这样的条目会以 `skipped` 或 `failed` 收场而不是覆盖。 |

**返回值**: `{"operationId":"fileop_...","success":true,"totalCount":2}`

派工回执；结果经 `file:opProgress` 与 `file:opComplete` 送达。同卷移动是一次改名，与体积无关；跨卷时宿主自动复制后删源，该条目仍记 `status: "ok"` 但带 `reason: "cross-volume"`，让调用方看得见这一条的代价。目录跨卷同样走这条回退 —— 这是同步版 `file.move` 做不到的。

与 `file.copyAsync` 不同：目标目录已存在时该条目记 `skipped` / `already-exists` 而非并入。传 `overwrite` 也改不了这一点，见上面参数表的说明。

```javascript
const receipt = await fb2k.invoke('file.moveAsync', {
    items: [{ source: 'C:\\Inbox\\Album', destination: 'D:\\Music\\Album' }]
});
```

### file.deleteAsync

可取消的批量删除，结果条目不带 `destination`。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `paths` | `array<string>` | 是 | — | 待删除路径数组。缺参或空数组返回 `INVALID_PARAMS`，非字符串元素同样如此。逐条 `FileWrite` 校验是 fail-fast：任一条被拒即整批失败，返回 `PERMISSION_DENIED` 且不产生 `operationId`；**空串也落在这里**，因为路径校验跑在 handler 之前。 |
| `moveToTrash` | `boolean` | 否 | `true` | 为 `false` 时永久删除。传非布尔值返回 `INVALID_PARAMS`。 |

**返回值**: `{"operationId":"fileop_...","success":true,"totalCount":2}`

派工回执；结果经 `file:opProgress` 与 `file:opComplete` 送达。回收站删除受 shell API 的 STA 约束，在宿主主线程上每 16 条一批执行；永久删除走 worker 线程且能删非空目录 —— 同步版 `file.delete` 在 `moveToTrash: false` 下删不了非空目录。

```javascript
const receipt = await fb2k.invoke('file.deleteAsync', {
    paths: ['%profile%\\cache\\a.json', '%profile%\\cache\\old'],
    moveToTrash: false
});
```

### file.cancelOp

取消一个进行中的 `file.copyAsync` / `file.moveAsync` / `file.deleteAsync`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `operationId` | `string` | 是 | 派工回执里的 id。缺参、非字符串或空串返回 `INVALID_PARAMS`。 |

**返回值**: `{"cancelled":true,"success":true}`

`cancelled: false` 表示该操作已结束或从未存在，两者故意不可区分。复制/移动在一个文件之内停下（正在传输的那个文件被中止，其残片被删除），删除在下一条之前停下。已处理的条目保留各自结果，其余记 `skipped` / `cancelled`，收尾仍会发一条 `cancelled: true` 的 `file:opComplete`。

关闭发起该操作的 **popup 窗口**等效于取消，退出 foobar2000 同理。panel 宿主没有这个钩子：它发起的操作会一直跑到结束，除非用本方法停掉，其事件此后走上文所述的回退路径。

```javascript
const { cancelled } = await fb2k.invoke('file.cancelOp', { operationId });
```

## 异步文件操作事件 {#file-op-events}

`file.copyAsync`、`file.moveAsync`、`file.deleteAsync` 通过两个事件回报结果。两个事件都投递给发起该操作的那个窗口 —— 这是 `results` 里可以带真实路径的前提；错误信封与 console 日志里仍然一个路径都不会出现。

一个例外：宿主在每次发射时重新解析目标窗口，该窗口一旦销毁就再也解析不到，事件会改投主实例。popup 关闭时会取消自己发起的未完成操作，这类事件因此有限；panel 宿主没有对应的取消钩子。

### file:opProgress

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `operationId` | `string` | 派工回执里的关联 id。 |
| `op` | `string` | `copy` / `move` / `delete`。 |
| `done` | `integer` | 截至目前所有批次已上报的条目数。 |
| `total` | `integer` | 本次调用受理的条目数，等于回执里的 `totalCount`。 |
| `results` | `array<object>` | 仅本批，不是累计列表。 |

分批发射，不是一条一个事件：攒够 64 条或距上一批满 100 ms，先到者触发。最后的残余批必定排在 `file:opComplete` 之前。

`results` 逐条：

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `source` | `string` | 请求时的 source 原样回显，`%变量%` 不展开。 |
| `destination` | `string` | 请求时的 destination 原样回显。`file.deleteAsync` 的结果无此字段。 |
| `status` | `string` | `ok` / `skipped` / `failed`。 |
| `reason` | `string` | 条目干净成功时无此字段；否则为 `already-exists` / `not-found` / `permission` / `cross-volume` / `io-error` / `cancelled`。 |

`skipped` 表示这一条是"没做"（目标已存在，或取消时还没轮到它），不是错误。`cross-volume` 是唯一与 `status: "ok"` 同时出现的 reason：该条移动经复制加删源的回退成功了。

### file:opComplete

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `operationId` | `string` | 派工回执里的关联 id。 |
| `op` | `string` | `copy` / `move` / `delete`。 |
| `total` | `integer` | 本次调用受理的条目数。 |
| `successCount` | `integer` | 做成的条目数，含跨卷回退成功的那些。 |
| `skippedCount` | `integer` | 有意没做的条目数。 |
| `failureCount` | `integer` | 失败的条目数。 |
| `cancelled` | `boolean` | 至少有一条结果带 `reason: "cancelled"` 时为 true。 |

它一旦到达就是该 `operationId` 的最后一个事件，取消与失败路径上照发。但有两条路径根本不会发它：宿主中途退出、以及 worker 侧出现意外失败被最外层兜底接住。因此不允许状态泄漏的监听方应自带超时，而不是无限等这个事件。

取消不会丢条目：没轮到的那些仍记 `skipped` / `cancelled`，所以三个计数通常加起来等于 `total`。

```javascript
fb2k.on('file:opProgress', ({ operationId, done, total, results }) => {
    // results[].status: 'ok' | 'skipped' | 'failed'
});
fb2k.on('file:opComplete', ({ operationId, successCount, cancelled }) => {
    // 该 operationId 的最后一个事件
});
```

## Dialog API - 对话框

系统原生对话框。

### dialog.openFile

打开文件选择对话框。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `title` | `string` | 否 | `Open File` | 随界面语言本地化。 |
| `defaultPath` | `string` | 否 | — | 初始目录，支持 `%music%` 等变量展开。 |
| `filters` | `array` | 否 | — | 文件类型过滤器，`{ name, extensions[] }` 数组。 |
| `multiple` | `boolean` | 否 | `false` | 允许多选。 |

**返回值**: `{ "canceled": false, "filePaths": ["C:\\Music\\song.mp3"] }`

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

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `title` | `string` | 否 | `Save File` | 随界面语言本地化。 |
| `defaultName` | `string` | 否 | — | 预填的文件名。 |
| `filters` | `array` | 否 | — | 文件类型过滤器，`{ name, extensions[] }` 数组。 |

**返回值**: `{ "canceled": false, "filePath": "C:\\Music\\export.json" }`

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

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `title` | `string` | 否 | `Select Folder` | 随界面语言本地化。 |

**返回值**: `{"canceled":true,"error":"...","folderPath":"..."}`

```javascript
const result = await fb2k.invoke('dialog.openFolder', { title: '选择音乐文件夹' });
if (!result.canceled) {
    console.log('选中文件夹:', result.folderPath);
}
```

### dialog.confirm

显示确认对话框。使用 Windows TaskDialog 实现，支持自定义按钮。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `title` | `string` | 否 | `Confirm` | 随界面语言本地化。 |
| `message` | `string` | 否 | — | 正文文本。 |
| `type` | `string` | 否 | `question` | 图标类型，如 `warning`。 |
| `buttons` | `array` | 否 | — | 自定义按钮文本数组，`response` 返回被点按钮的索引。 |
| `defaultButton` | `integer` | 否 | `0` | 默认聚焦的按钮索引。 |

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

| 参数 | 类型 | 必填 |
| --- | --- | --- |
| `path` | `string` | 是 |

**返回值**: `{ "success": true }`

```javascript
await fb2k.invoke('shell.showInExplorer', { path: 'C:\\Music\\song.flac' });
```

### shell.openExternal

用默认程序打开 URL 或文件。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `url` | `string` | 是 | 仅接受 `http://`、`https://`、`mailto:`。 |

**返回值**: `{ "success": true }`

```javascript
await fb2k.invoke('shell.openExternal', { url: 'https://www.foobar2000.org' });
```

### shell.exec

执行系统命令。

::: warning 安全限制
不限制可执行命令（信任主题作者，信任边界等同于安装一个 foobar2000 组件）。若提供 `cwd`，会经 PathSecurity 路径校验拒绝越界路径。破坏性文件操作请用 `fb.file.*`（受路径黑名单保护）。
:::

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `command` | `string` | 是 | — | 要执行的命令行。 |
| `args` | `array` | 否 | — | 附加参数数组。 |
| `cwd` | `string` | 否 | — | 工作目录，经路径安全校验。 |
| `hidden` | `boolean` | 否 | `true` | 隐藏进程窗口。 |

**返回值**: `{ "success": true, "processId": 12345 }`

::: tip 行为说明
`shell.exec` 是 fire-and-forget 语义，只表示命令进程已发起，不保证目标服务已经就绪。
:::

```javascript
// 启动 Node.js 服务器
await fb2k.invoke('shell.exec', { command: 'cmd /c start /b node "E:\\server.js"' });

// 无命令白名单：任意命令均可执行（信任主题作者）
await fb2k.invoke('shell.exec', { command: 'curl http://example.com' });
```

### shell.spawn

结构化启动进程（推荐用于启动本地服务）。

::: warning 安全限制
不限制可执行文件（信任主题作者）。绝对路径可执行文件与 `cwd` 会经路径安全校验，拒绝指向系统目录等越界路径。
:::

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `executable` | `string` | 是 | — | 可执行文件路径，绝对路径会经路径安全校验。 |
| `args` | `array` | 否 | — | 参数数组，逐项传给进程。 |
| `cwd` | `string` | 否 | — | 工作目录，经路径安全校验。 |
| `hidden` | `boolean` | 否 | `true` | 隐藏进程窗口。 |
| `waitForExitMs` | `integer` | 否 | `0` | 等待该毫秒数以检测进程提前退出（返回 `exited` / `exitCode`）。 |

**返回值**: `{"error":"...","exitCode":"...","exited":"...","processId":"...","success":true}`


```javascript
// ✅ 推荐：直接启动 node server.js（可检测 CreateProcess 失败）
const result = await fb2k.invoke('shell.spawn', {
  executable: 'E:\\FB2K\\Runtime\\node.exe',
  args: ['E:\\FB2K\\NeteaseApi\\server.js'],
  cwd: 'E:\\FB2K\\NeteaseApi',
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

| 参数 | 类型 | 必填 |
| --- | --- | --- |
| `path` | `string` | 是 |

**返回值**: `{ "success": true }`

```javascript
await fb2k.invoke('shell.openWith', { path: 'C:\\Music\\notes.txt' });
```

## 文件、对话框与 Shell 边界

- 文件 API 会在访问前展开 `%profile%`、`%component%`、`%music%`、`%APPDATA%`、`%TEMP%`。每个端点按注册的 `SecurityLevel` 校验 —— 读端点走 `Read`，`file.*` 的全部写端点走 `FileWrite`，逐端点对照表见[权限系统](/zh/reference/permissions)。`file.write` 会创建缺失的父目录，`file.delete` 默认移入回收站。
- `file.*` 的每一条失败都在 `error` 之外带 `code`。由文件系统本身抛出的失败还带 `details.value`，即原始 Win32 错误号。`error` 与 `details` 都不会包含路径。
- `file.list` 在非递归模式下返回名称，在递归模式下返回完整路径。`file.getInfo` 以成功的不存在结果返回 `exists: false`。
- 异步族（`copyAsync` / `moveAsync` / `deleteAsync`）的返回值只是派工回执，逐条结果只出现在 `file:opProgress` / `file:opComplete` 的 payload 里；这两个事件单窗口投递，因此 `results` 里带路径，而错误信封与日志仍然不带。同步的 `file.copy` / `file.move` / `file.delete` 行为不受本组影响。
- 原生对话框取消时返回 `canceled: true`，并提供空的结果路径或列表。对话框初始化失败会添加 `error`，并将 `canceled` 设为 `false`。
- `shell.openExternal` 仅接受 `http://`、`https://` 或 `mailto:` URL。`shell.openWith` 会拒绝可执行文件、脚本、安装包、快捷方式、库及相关危险扩展名。
- `shell.exec` 与 `shell.spawn` 有意不设置命令白名单。它们的 `cwd` 与绝对可执行文件路径都会被校验；`shell.spawn.waitForExitMs` 可选地报告进程是否提前退出。
