# File 文件 API

安全的文件系统操作。所有路径支持变量替换：`%profile%`、`%component%`、`%music%`、`%APPDATA%`、`%TEMP%`。

每个端点走哪一档路径校验、各档实际放行什么，见[权限系统](/zh/reference/permissions)。

## 路径长度

由于 foobar2000 未声明 Windows 长路径支持，组件受经典路径长度限制：展开变量后的完整文件路径最多 259 个字符，新建文件夹路径最多 247 个字符。同步方法遇到超长路径时以 `OPERATION_FAILED` 失败，并附带 `details.value: 206`（Windows 路径过长错误号），避免将已有文件误判为不存在。`file.write` 启用原子写入时，需要在目标目录下创建临时文件 `.~<进程号>-<序号>.tmp`，该临时路径同样不得超过 259 个字符。异步批量方法（`file.copyAsync`、`file.moveAsync`、`file.deleteAsync`）遇到超长路径（包括复制文件夹内部的超长路径）时，会将对应条目上报为 `failed`，原因为 `path-too-long`。

## File API - 文件系统

### file.read

<!-- api-schema:begin file.read -->
读取文件为文本；`encoding: "binary"` 时读取原始字节并编码为 Base64；路径不存在时以 `NOT_FOUND` 失败，不是普通文件时以 `INVALID_PATH` 失败

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 要读取的文件路径不能为空。 |
| `encoding` | `string` | 否 | 字符编码；仅当取值为 `"binary"` 时按原始字节读取并编码为 Base64；其他任何取值均按文本读取默认 `"utf-8"`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `content` | `string` | 文件内容；文本读取时为文本字符串，二进制读取时为不带 `base64:` 前缀的裸 Base64 字符串 |
| `size` | `integer` | 文件在磁盘上的字节大小；文本读取时可能与 `content` 字符串长度不同 |
| `encoding` | `string` | 内容编码标识；仅在二进制读取时出现，固定为 `"base64"` |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

在二进制模式下，接口额外返回 `"encoding": "base64"`。

以二进制模式读取时，返回的 `content` 为**不带 `base64:` 前缀的裸 Base64 字符串**。该内容属于底层传输载荷，既不是纯文本，也不是 Data URL。若需将读取结果原样写回文件，必须在调用 `file.write` 时显式添加 `base64:` 前缀，并保持 `encoding: 'binary'`。

```javascript
// 读取文本文件
const res = await fb2k.invoke('file.read', { path: '%profile%\\config.json' });
if (res.success === false) throw new Error(res.error);
const { content } = res;

// 读取二进制文件
const bin = await fb2k.invoke('file.read', { path: '%profile%\\data.bin', encoding: 'binary' });
if (bin.success === false) throw new Error(bin.error);
console.log(bin.encoding); // "base64"
```

### file.write

<!-- api-schema:begin file.write -->
将文本或 Base64 解码后的字节写入文件；父目录不存在时会自动创建

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 要写入的文件路径；父目录不存在时会自动创建不能为空。 |
| `content` | `string` | 否 | 待写入的内容；未设置 `append` 时空字符串会清空文件；当 `encoding: "binary"` 且内容以 `base64:` 开头时，宿主将解码为字节后写入（跳过 Base64 字母表以外字符，遇首个 `=` 停止解码）；其他任何取值（含裸 Base64 与 Data URL）均按原样文本写入默认 `""`。 |
| `encoding` | `string` | 否 | 字符编码；仅当取值为 `"binary"` 时进入二进制写入模式并解码以 `base64:` 开头的 `content`；其他任何取值均按文本写入默认 `"utf-8"`。 |
| `append` | `boolean` | 否 | 是否追加到文件末尾，为 `false` 时替换原有内容默认 `false`。 |
| `atomic` | `boolean` | 否 | 是否启用原子写入；开启时先在目标同目录下写入临时文件并落盘，再通过原子重命名替换目标，确保读取方只能读到完整的新内容或旧内容；若替换失败（如目标文件被其他程序占用），调用将失败且目标保留原内容；临时文件命名为 `.~<进程号>-<序号>.tmp`，路径长度同样受 259 字符限制；不可与 `append` 同时使用，两者同时传入时以 `INVALID_PARAMS` 失败默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `bytesWritten` | `integer` | 写入后文件的字节总大小；`append` 为 `true` 时是追加后的完整文件大小，而非仅新增字节数 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

二进制写入仅在同时满足两个条件时触发解码：`encoding` 参数必须精确为 `'binary'`，且 `content` 必须以 `base64:` 前缀开头。该前缀属于 Bridge 传输标记，宿主解码落盘前会自动剥离。裸 Base64 字符串、`data:image/...;base64,...` Data URL 或 `fb2k://` 封面 URL 均不满足解码前缀要求；此时调用虽可能返回 `success: true`，但写入的文件内容会出现乱码或数据异常。

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
if (binary.success === false) throw new Error(binary.error);
await fb2k.invoke('file.write', {
    path: '%profile%\\data-copy.bin',
    content: `base64:${binary.content}`,
    encoding: 'binary'
});
```

### file.exists

<!-- api-schema:begin file.exists -->
检查路径是否存在，并区分它是文件还是目录

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 要检查的路径不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `exists` | `boolean` | 路径是否存在 |
| `isFile` | `boolean` | 路径是否是普通文件 |
| `isDirectory` | `boolean` | 路径是否是目录 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const res = await fb2k.invoke('file.exists', { path: '%profile%\\config.json' });
if (res.success === false) throw new Error(res.error);
const { exists, isFile } = res;
```

### file.list

<!-- api-schema:begin file.list -->
列出目录下的文件与子目录；路径不存在时以 `NOT_FOUND` 失败，不是目录时以 `INVALID_PATH` 失败

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 要列出的目录路径不能为空。 |
| `pattern` | `string` | 否 | 文件匹配规则，仅过滤文件，目录始终列出；仅支持 `*`、`*.*` 和单个扩展名（如 `*.flac`）；不以 `*.` 开头的模式（如 `song*`、`?.txt`）匹配全部文件；`*.ext` 模式按不区分大小写比对文件名末尾点号起的文本，因此 `*.{flac,mp3}` 无法匹配任何文件，无扩展名文件也无法匹配任何 `*.ext`默认 `"*"`。 |
| `recursive` | `boolean` | 否 | 是否遍历子目录；开启时条目为完整路径，关闭时仅为名称；遇到无法读取的子目录时调用以 `OPERATION_FAILED` 失败默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `files` | `string[]` | 匹配 `pattern` 的文件列表：未开启 `recursive` 时为文件名，开启时为完整路径 |
| `directories` | `string[]` | 子目录列表，不受 `pattern` 影响：未开启 `recursive` 时为子目录名，开启时为完整路径 |
| `items` | `string[]` | 与 `files` 相同的列表，为读取此字段的旧调用方保留 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

> 非递归模式下 `files` 返回文件名；递归模式下返回完整路径。

```javascript
// 列出配置目录下的 JSON 文件
const res = await fb2k.invoke('file.list', {
    path: '%profile%', pattern: '*.json'
});
if (res.success === false) throw new Error(res.error);
const { files } = res;
```

### file.delete

<!-- api-schema:begin file.delete -->
删除文件或目录，默认移入回收站，`moveToTrash` 为 `false` 时永久删除；路径不存在时以 `NOT_FOUND` 失败

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 要删除的文件或目录路径不能为空。 |
| `moveToTrash` | `boolean` | 否 | 是否移入回收站；为 `false` 时永久删除，且无法删除非空目录默认 `true`。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
// 删除到回收站（安全）
await fb2k.invoke('file.delete', { path: '%profile%\\old-config.json' });

// 永久删除
await fb2k.invoke('file.delete', { path: '%temp%\\cache.tmp', moveToTrash: false });
```

### file.mkdir

<!-- api-schema:begin file.mkdir -->
创建目录并自动补齐缺失的父目录；目录已存在时成功并返回 `created: false`，目标路径为已有文件时以 `INVALID_PATH` 失败

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 要创建的目录路径不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `created` | `boolean` | 本次调用是否创建了新目录；目录已存在时为 `false` |
| `message` | `string` | 提示信息；目录已存在时为 `"Directory already exists"`，否则不出现 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### file.copy

<!-- api-schema:begin file.copy -->
复制文件或整棵目录树，复制完成前一直阻塞；源路径不存在时以 `NOT_FOUND` 失败；体量较大时建议改用 `file.copyAsync`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `source` | `string` | 是 | 要复制的源文件或目录；若为目录则连同其下全部内容递归复制不能为空。 |
| `destination` | `string` | 是 | 目标路径；其父目录必须已经存在不能为空。 |
| `overwrite` | `boolean` | 否 | 是否替换目标处已存在的文件；为 `false` 时保留已有文件不动且调用仍然成功默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `source` | `string` | 请求中传入的源路径 |
| `destination` | `string` | 请求中传入的目标路径 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
await fb2k.invoke('file.copy', {
    source: '%profile%\\config.json',
    destination: '%profile%\\config.bak.json'
});
```

### file.move

<!-- api-schema:begin file.move -->
移动文件或目录，移动完成前一直阻塞；文件支持跨卷移动，目录跨卷移动则会以 `NOT_SUPPORTED` 失败且附带 `details.reason: "cross-volume"`；源路径不存在时以 `NOT_FOUND` 失败

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `source` | `string` | 是 | 要移动的文件或目录路径不能为空。 |
| `destination` | `string` | 是 | 目标路径；该处已有的同名文件会被替换；若将文件移到已存在的目录上，或父目录不存在，调用均会失败不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `source` | `string` | 请求中传入的源路径 |
| `destination` | `string` | 请求中传入的目标路径 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

跨卷移动**文件**会执行成功，底层由 Windows 自动完成复制与删源；跨卷移动**目录**则不支持，调用将失败并返回错误码 `code: "NOT_SUPPORTED"` 与详细原因 `details.reason: "cross-volume"`。

### file.rename

<!-- api-schema:begin file.rename -->
在所在目录内重命名文件或目录；原路径不存在时以 `NOT_FOUND` 失败，新名称已被占用时以 `OPERATION_FAILED` 失败

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 要重命名的文件或目录路径不能为空。 |
| `newName` | `string` | 是 | 新名称，仍位于同一目录；包含 `/` 或 `\` 时以 `INVALID_PARAMS` 失败不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `oldPath` | `string` | 请求中传入的原路径 |
| `newPath` | `string` | 展开路径变量后的父目录与 `newName` 拼接所得的新路径 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### file.getInfo

<!-- api-schema:begin file.getInfo -->
获取文件或目录的元信息；路径不存在时仍然成功返回，结果中 `exists: false` 且不包含其他字段

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 要查询信息的文件或目录路径不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `exists` | `boolean` | 路径是否存在；为 `false` 时不包含其他字段 |
| `isDirectory` | `boolean` | 路径是否是目录 |
| `isFile` | `boolean` | 路径是否是普通文件 |
| `size` | `integer` | 文件字节大小；目录为 `0` |
| `modified` | `integer` | 最后修改时间，截断到整秒的 JavaScript 毫秒时间戳 |
| `name` | `string` | 文件或目录名称，包含扩展名 |
| `extension` | `string` | 带前导点的扩展名（如 `".flac"`）；无扩展名时为空字符串 |
| `parent` | `string` | 展开 `%变量%` 后的父目录路径；由于不做路径归一化，展开留下的多余斜杠会原样保留 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### file.copyAsync

<!-- api-schema:begin file.copyAsync -->
在 worker 线程启动可取消的批量复制并立即返回任务回执；进度结果由 `file:opProgress` 分批推送，最终发送一条 `file:opComplete`；全进程最多同时进行 8 个批量操作，超出时以 `OPERATION_FAILED` 失败

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `items` | `FileOpEntry[]` | 是 | 要复制的条目列表，按此顺序每条上报一个结果；若条目不是仅包含字符串成员 `source` 与 `destination` 的对象，调用以 `INVALID_PARAMS` 失败；开始前逐条校验路径权限（`source` 按读取、`destination` 按写入），任一路径被拒绝（含空字符串）都会导致整次调用以 `PERMISSION_DENIED` 失败且不产生 `operationId`不能为空。 |
| `items[].source` | `string` | 是 | 要复制或移动的文件或目录路径；在 `file:opProgress` 结果中原样回显且不展开 `%变量%`不能为空。 |
| `items[].destination` | `string` | 是 | 目标路径；缺失的父目录会被自动创建；若目标指向已存在的目录且 `source` 为文件，则以原文件名存入该目录；结果中同样原样回显不能为空。 |
| `overwrite` | `boolean` | 否 | 是否替换已存在的目标文件；为 `false` 时将该条记录为 `skipped` 且原因为 `already-exists`；目录复制到已存在的目录时总会并入其中，目录内已存在的同名文件仅在此项开启时才被替换，否则静默跳过且不单独上报默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `operationId` | `string` | 批量操作任务 ID，以 `fileop_` 开头；该操作的事件带有此 ID，`file.cancelOp` 也用它 |
| `totalCount` | `integer` | 本次操作受理的条目总数，对应事件里的 `total` |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

方法的返回值仅为任务回执，实际操作结果通过 `file:opProgress` 分批推送，并在完成时发送 `file:opComplete`。结果按**请求条目**上报而非按单文件：整个目录树处理完毕后仅产生一条条目结果。全进程最多同时运行 8 个批量操作，达到上限时后续调用将直接返回 `OPERATION_FAILED`。

当目标目录已存在时，复制目录操作会自动**合并**入目标目录：该目录条目不会被判定为 `already-exists`，其处理状态仍记录为 `status: "ok"`；目录内已有的同名文件将静默跳过且不单独上报（除非传入 `overwrite: true` 覆盖）。`already-exists` 仅适用于目标已存在且未开启覆盖的单文件条目。

```javascript
const receipt = await fb2k.invoke('file.copyAsync', {
    items: [{ source: 'C:\\Music\\Album', destination: 'D:\\Backup\\Album' }]
});
```

### file.moveAsync

<!-- api-schema:begin file.moveAsync -->
在 worker 线程启动可取消的批量移动，回执与事件同 `file.copyAsync`；同卷移动直接重命名，跨卷移动则先复制该条目再删除源

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `items` | `FileOpEntry[]` | 是 | 要移动的条目列表，按此顺序每条上报一个结果；若条目不是仅包含字符串成员 `source` 与 `destination` 的对象，调用以 `INVALID_PARAMS` 失败；开始前逐条按写入权限校验全部路径（包括 `source`，因为移动会删除源文件），任一路径被拒绝（含空字符串）都会导致整次调用以 `PERMISSION_DENIED` 失败且不产生 `operationId`不能为空。 |
| `items[].source` | `string` | 是 | 要复制或移动的文件或目录路径；在 `file:opProgress` 结果中原样回显且不展开 `%变量%`不能为空。 |
| `items[].destination` | `string` | 是 | 目标路径；缺失的父目录会被自动创建；若目标指向已存在的目录且 `source` 为文件，则以原文件名存入该目录；结果中同样原样回显不能为空。 |
| `overwrite` | `boolean` | 否 | 是否替换已存在的目标文件；为 `false` 时将该条记录为 `skipped` 且原因为 `already-exists`（同步 `file.move` 总是替换）；已存在的目标目录永远不会被替换（Windows 不支持原地替换目录），同卷移动遇到已有同名目录时条目将以 `skipped` 或 `failed` 结束默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `operationId` | `string` | 批量操作任务 ID，以 `fileop_` 开头；该操作的事件带有此 ID，`file.cancelOp` 也用它 |
| `totalCount` | `integer` | 本次操作受理的条目总数，对应事件里的 `total` |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

方法返回值仅为任务回执，具体处理进度由 `file:opProgress` 与 `file:opComplete` 事件推送。同卷移动仅执行重命名操作，耗时与体积无关；跨卷移动时宿主自动转为复制后删除源条目，该条目仍记录为 `status: "ok"`，但附带 `reason: "cross-volume"` 以显式标识跨卷开销。目录跨卷同样支持该自动回退机制（同步版 `file.move` 不支持目录跨卷）。

与 `file.copyAsync` 的目录合并策略不同：若目标目录已存在，`file.moveAsync` 会直接将该条目记为 `skipped` 且原因为 `already-exists`，不会执行合并。即使传入 `overwrite: true` 也无法覆盖已有目录。

```javascript
const receipt = await fb2k.invoke('file.moveAsync', {
    items: [{ source: 'C:\\Inbox\\Album', destination: 'D:\\Music\\Album' }]
});
```

### file.deleteAsync

<!-- api-schema:begin file.deleteAsync -->
启动可取消的批量删除，回执与事件同 `file.copyAsync`（结果中不带 `destination`）；回收站删除在主线程上每 16 条一批执行，永久删除走 worker 线程

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `string[]` | 是 | 要删除的路径列表，按此顺序每条上报一个结果；数组中存在非字符串元素时调用以 `INVALID_PARAMS` 失败；开始前逐条按写入权限校验全部路径，任一路径被拒绝（含空字符串）都会导致整次调用以 `PERMISSION_DENIED` 失败且不产生 `operationId`不能为空。 |
| `moveToTrash` | `boolean` | 否 | 是否移入回收站；为 `false` 时执行永久删除，且支持递归删除非空目录（同步 `file.delete` 拒绝删除非空目录）默认 `true`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `operationId` | `string` | 批量操作任务 ID，以 `fileop_` 开头；该操作的事件带有此 ID，`file.cancelOp` 也用它 |
| `totalCount` | `integer` | 本次操作受理的条目总数，对应事件里的 `total` |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

方法返回值仅为任务回执，具体结果通过 `file:opProgress` 与 `file:opComplete` 事件推送。受 Windows Shell API 的 STA 线程模型约束，回收站删除操作在宿主主线程分批执行（每批 16 条）；永久删除（`moveToTrash: false`）则在 worker 线程执行，且支持递归删除非空目录（同步版 `file.delete` 无法永久删除非空目录）。

```javascript
const receipt = await fb2k.invoke('file.deleteAsync', {
    paths: ['%profile%\\cache\\a.json', '%profile%\\cache\\old'],
    moveToTrash: false
});
```

### file.cancelOp

<!-- api-schema:begin file.cancelOp -->
停止由 `file.copyAsync`、`file.moveAsync` 或 `file.deleteAsync` 启动的操作；尚未处理的条目记为 `skipped` 且原因为 `cancelled`，收尾仍会发出 `file:opComplete`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `operationId` | `string` | 是 | `file.copyAsync`、`file.moveAsync` 或 `file.deleteAsync` 回执里的任务 ID不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `cancelled` | `boolean` | 操作是否成功收到停止通知；若操作仍在进行并已通知停止则为 `true`；若操作已结束或从未存在则为 `false`（两者故意不可区分） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

返回 `cancelled: false` 表示操作已经结束或任务 ID 不存在，设计上刻意不区分这两种状态。调用取消后：复制与移动操作会在当前文件粒度中断（正在传输的文件会被中止并清理未完成的残片），删除操作则在下一条开始前停止。已处理完成的条目保留其实际结果，尚未处理的条目统一记录为 `skipped` 且原因为 `cancelled`；任务终止时仍会发出附带 `cancelled: true` 的 `file:opComplete` 事件。

关闭发起操作的 **popup 弹窗**或退出 foobar2000 时，宿主会自动取消进行中的任务。panel 宿主无此自动取消机制，其发起的任务会持续执行直至完成或调用本方法取消；若窗口提前销毁，事件将按前文规则回退投递至主窗口。

```javascript
const res = await fb2k.invoke('file.cancelOp', { operationId });
if (res.success === false) throw new Error(res.error);
const { cancelled } = res;
```

## 异步文件操作事件 {#file-op-events}

`file.copyAsync`、`file.moveAsync` 与 `file.deleteAsync` 通过两组事件推送处理进度与最终结果。事件默认仅定向投递给发起操作的目标窗口，因此事件载荷中的 `results` 能够安全附带真实路径；作为安全防护，通用错误信封与控制台日志中始终不会包含任何文件路径。

窗口生命周期异常处理：宿主在每次派发事件前会重新解析目标窗口；若该窗口已销毁，事件将改投至主窗口。popup 弹窗关闭时会自动取消未完成任务，因此改投事件极少；panel 宿主没有自动取消机制，需注意窗口生命周期。

### file:opProgress

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `operationId` | `string` | 派工回执里的关联 ID |
| `op` | `string` | `copy` / `move` / `delete` |
| `done` | `integer` | 截至当前所有批次累计上报的条目数 |
| `total` | `integer` | 本次调用受理的条目总数，等于回执中的 `totalCount` |
| `results` | `array<object>` | 当前批次的条目结果列表，非累计数据 |

事件按批次聚合派发，而非单条独立触发：待推送结果达到 64 条或距离上次推送满 100 ms 时触发一次派发。所有残余条目的最终批次一定会排在 `file:opComplete` 事件之前送达。

`results` 逐条：

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `source` | `string` | 请求中传入的 `source` 原样回显，不展开 `%变量%` |
| `destination` | `string` | 请求中传入的 `destination` 原样回显，`file.deleteAsync` 结果中无此字段 |
| `status` | `string` | `ok` / `skipped` / `failed` |
| `reason` | `string` | 条目直接成功时无此字段，异常或特殊状态时为 `already-exists` / `not-found` / `permission` / `cross-volume` / `path-too-long` / `io-error` / `cancelled` |

`status: "skipped"` 表示该条目因目标已存在或操作提前取消而跳过，并非发生错误。`reason: "cross-volume"` 是唯一能与 `status: "ok"` 同时出现的理由字段，表明跨卷移动已通过先复制后删除的回退机制顺利完成。

### file:opComplete

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `operationId` | `string` | 派工回执里的关联 ID |
| `op` | `string` | `copy` / `move` / `delete` |
| `total` | `integer` | 本次调用受理的条目总数 |
| `successCount` | `integer` | 成功执行的条目数，包含跨卷回退成功的条目 |
| `skippedCount` | `integer` | 有意跳过的条目数 |
| `failureCount` | `integer` | 执行失败的条目数 |
| `cancelled` | `boolean` | 至少存在一条结果带 `reason: "cancelled"` 时为 true |

`file:opComplete` 是该 `operationId` 任务周期的终末事件，操作正常完成、被取消或部分条目失败均会触发。仅在两种极端异常情况下该事件不会发出：宿主退出，以及 worker 线程发生非预期致命故障。因此，需要严格防范状态泄漏的监听方应自行设置超时保护，避免无限等待。

操作取消时不会丢失条目统计：尚未处理的条目均会记录为 `skipped` 且原因为 `cancelled`，因此成功数、跳过数与失败数三者之和通常等于 `total`。

```javascript
fb2k.on('file:opProgress', ({ operationId, done, total, results }) => {
    // results[].status: 'ok' | 'skipped' | 'failed'
});
fb2k.on('file:opComplete', ({ operationId, successCount, cancelled }) => {
    // 该 operationId 的最后一个事件
});
```

## 文件边界

- 路径展开与安全校验：文件 API 在访问前自动展开 `%profile%`、`%component%`、`%music%`、`%APPDATA%` 与 `%TEMP%` 变量。每个端点按照注册的 `SecurityLevel` 进行权限校验：读取端点走 `Read` 级别，所有写入端点走 `FileWrite` 级别，逐端点权限映射详见[权限系统](/zh/reference/permissions)。`file.write` 自动创建缺失的父目录，`file.delete` 默认移入回收站。
- 错误与隐私安全：`file.*` 所有调用失败均包含标准错误信息 `error` 与错误码 `code`。由底层文件系统抛出的错误还会携带原始 Win32 错误号 `details.value`。为保护用户隐私，`error` 与 `details` 中绝不包含任何文件路径。
- 路径与元数据返回形式：`file.list` 在非递归模式下返回文件名列表，递归模式下返回完整路径列表。`file.getInfo` 查询不存在的路径时返回成功响应，结果中 `exists: false` 且不包含其余元数据字段。
- 异步批量与同步操作边界：异步方法（`copyAsync`、`moveAsync`、`deleteAsync`）仅返回任务回执，逐条明细仅通过 `file:opProgress` 与 `file:opComplete` 事件载荷推送；这两类事件采用单窗口定向投递，因此载荷中的 `results` 包含路径，而错误信封和系统日志中始终脱敏。同步方法（`copy`、`move`、`delete`）的行为不受异步操作影响。
