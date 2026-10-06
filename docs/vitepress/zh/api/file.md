# File 文件 API

安全的文件系统操作。所有路径支持变量替换：`%profile%`、`%component%`、`%music%`、`%APPDATA%`、`%TEMP%`。

每个端点走哪一档路径校验、各档实际放行什么，见[权限系统](/zh/reference/permissions)。

## 路径长度

foobar2000 没有声明长路径支持，宿主受 Windows 传统的路径长度限制：展开变量后的完整路径最多 259 个字符，宿主新建的文件夹最多 247 个字符。同步方法遇到超长路径以 `OPERATION_FAILED` 失败，`details.value` 为 206（Windows 表示名称过长的错误号），不会把存在的文件报成不存在。原子写入的 `file.write` 还要在目标所在文件夹放下临时文件 `.~<进程号>-<序号>.tmp`。`file.copyAsync`、`file.moveAsync`、`file.deleteAsync` 把这样的条目报成 `failed`，原因为 `path-too-long`；复制的文件夹里面有路径超长时也一样。

## File API - 文件系统

### file.read

<!-- api-schema:begin file.read -->
以文本读取文件，`encoding: "binary"` 时以 Base64 读取。路径不存在时以 `NOT_FOUND` 失败，不是普通文件时以 `INVALID_PATH` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 要读取的文件。不能为空。 |
| `encoding` | `string` | 否 | 只有恰为 `binary` 才按字节读取并以 Base64 返回；其他任何值都按文本读取。默认 `"utf-8"`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `content` | `string` | 文件的文本；二进制读取时为其字节的裸 Base64，不带 `base64:` 前缀。 |
| `size` | `integer` | 文件在磁盘上的字节数；文本读取时可能与 `content` 的长度不同。 |
| `encoding` | `string` | 只在二进制读取时出现，恒为 `base64`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

二进制模式额外返回 `"encoding": "base64"`。

二进制读取时，`content` 是**不带 `base64:` 前缀的裸 Base64 payload**；它是传输表示，不是文本，也不是 Data URL。要把读取结果原样写回，必须在写入时补上 `base64:`，并同时保持 `encoding: 'binary'`。

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
把文本或由 Base64 解码出的字节写入文件。父目录不存在时先创建。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 要写入的文件；缺失的父目录会被创建。不能为空。 |
| `content` | `string` | 否 | 要写入的内容。`encoding: "binary"` 时，以 `base64:` 开头的值被解码后按字节写入：Base64 字母表以外的字符被跳过，遇到第一个 `=` 即停止解码。其他任何值（包括裸 Base64 与 Data URL）都按原样写入。空串会清空文件，除非设置了 `append`。默认 `""`。 |
| `encoding` | `string` | 否 | 只有恰为 `binary` 才有区别：以二进制模式写入，并解码以 `base64:` 开头的 `content`。其他任何值都按文本写入。默认 `"utf-8"`。 |
| `append` | `boolean` | 否 | 追加到文件末尾，而不是替换原有内容。默认 `false`。 |
| `atomic` | `boolean` | 否 | 先在目标旁边写一个临时文件并落盘，再用一次改名替换目标：读者只会看到旧内容或新内容，不会看到写了一半的文件。替换失败时（例如目标被其他程序打开着）调用失败，目标保留原内容。临时文件名为 `.~<进程号>-<序号>.tmp`，它的路径同样不能超过 259 个字符。不能与 `append` 同用，两者同时传时以 `INVALID_PARAMS` 失败。默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `bytesWritten` | `integer` | 写入后文件的字节数；`append` 时是整个文件的大小，而不只是新增的部分。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

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
if (binary.success === false) throw new Error(binary.error);
await fb2k.invoke('file.write', {
    path: '%profile%\\data-copy.bin',
    content: `base64:${binary.content}`,
    encoding: 'binary'
});
```

### file.exists

<!-- api-schema:begin file.exists -->
检查路径是否存在，以及它是文件还是目录。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 要检查的路径。不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `exists` | `boolean` | 路径是否存在。 |
| `isFile` | `boolean` | 路径是否是普通文件。 |
| `isDirectory` | `boolean` | 路径是否是目录。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const res = await fb2k.invoke('file.exists', { path: '%profile%\\config.json' });
if (res.success === false) throw new Error(res.error);
const { exists, isFile } = res;
```

### file.list

<!-- api-schema:begin file.list -->
列出目录下的文件与子目录。路径不存在时以 `NOT_FOUND` 失败，不是目录时以 `INVALID_PATH` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 要列出的目录。不能为空。 |
| `pattern` | `string` | 否 | 只过滤文件，目录总会列出。只认 `*`、`*.*` 和单个扩展名（如 `*.flac`），任何写法都不会被拒绝：不以 `*.` 开头的写法（`song*`、`?.txt`）匹配全部文件；`*.ext` 写法按不区分大小写的方式与文件名最后一个点起的文本比较，所以 `*.{flac,mp3}` 一个都匹配不上，没有扩展名的文件也匹配不上任何 `*.ext`。默认 `"*"`。 |
| `recursive` | `boolean` | 否 | 同时遍历子目录；此时各项是完整路径而不是名称。遇到读不了的子目录时调用以 `OPERATION_FAILED` 失败。默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `files` | `string[]` | 匹配 `pattern` 的文件：名称；设置 `recursive` 时为完整路径。 |
| `directories` | `string[]` | 子目录，不受 `pattern` 影响：名称；设置 `recursive` 时为完整路径。 |
| `items` | `string[]` | 与 `files` 相同的列表；为读取这个名字的调用方保留。 |

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
删除文件或目录；除非 `moveToTrash` 为 `false`，否则移入回收站。路径不存在时以 `NOT_FOUND` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 要删除的文件或目录。不能为空。 |
| `moveToTrash` | `boolean` | 否 | 移入回收站。为 `false` 时永久删除，此时非空目录会删除失败。默认 `true`。 |

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
创建目录，缺失的上级目录一并创建。目录已存在时成功并返回 `created: false`；该路径是文件时以 `INVALID_PATH` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 要创建的目录。不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `created` | `boolean` | 本次调用是否创建了目录；目录已存在时为 `false`。 |
| `message` | `string` | 目录已存在时为 `Directory already exists`；否则不出现。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### file.copy

<!-- api-schema:begin file.copy -->
复制文件或整棵目录树，复制完成前一直阻塞。源不存在时以 `NOT_FOUND` 失败。体量大时改用 `file.copyAsync`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `source` | `string` | 是 | 要复制的文件或目录；目录连同其下全部内容一起复制。不能为空。 |
| `destination` | `string` | 是 | 目标路径。其父目录必须已经存在。不能为空。 |
| `overwrite` | `boolean` | 否 | 替换目标处已存在的文件。否则保留它们不动，调用仍然成功。默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `source` | `string` | 原样的源路径。 |
| `destination` | `string` | 原样的目标路径。 |

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
移动文件或目录，移动完成前一直阻塞。文件可以跨卷移动；目录不能，跨卷时以 `NOT_SUPPORTED` 失败并带 `details.reason: "cross-volume"`。源不存在时以 `NOT_FOUND` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `source` | `string` | 是 | 要移动的文件或目录。不能为空。 |
| `destination` | `string` | 是 | 目标路径。该处已有的文件会被替换；把文件移到已存在的目录上、或移到不存在的父目录下，都会失败。不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `source` | `string` | 原样的源路径。 |
| `destination` | `string` | 原样的目标路径。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

跨卷移动**文件**会成功 —— Windows 会自动复制后删除。跨卷移动**目录**失败，返回 `code: "NOT_SUPPORTED"` 与 `details.reason: "cross-volume"`。

### file.rename

<!-- api-schema:begin file.rename -->
在所在目录内重命名文件或目录。路径不存在时以 `NOT_FOUND` 失败，新名字已被占用时以 `OPERATION_FAILED` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 要重命名的文件或目录。不能为空。 |
| `newName` | `string` | 是 | 新名字，仍位于同一目录。含 `/` 或 `\` 的名字以 `INVALID_PARAMS` 失败。不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `oldPath` | `string` | 原样的路径。 |
| `newPath` | `string` | 展开后的 `path` 的父目录拼上 `newName`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### file.getInfo

<!-- api-schema:begin file.getInfo -->
描述文件或目录。路径不存在时成功返回 `exists: false`，不带其他字段。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 要描述的文件或目录。不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `exists` | `boolean` | 路径是否存在；为 `false` 时不带其他字段。 |
| `isDirectory` | `boolean` | 路径是否是目录。 |
| `isFile` | `boolean` | 路径是否是普通文件。 |
| `size` | `integer` | 字节数；目录为 `0`。 |
| `modified` | `integer` | 最后写入时间，JavaScript 毫秒时间戳，截到整秒。 |
| `name` | `string` | 带扩展名的文件名。 |
| `extension` | `string` | 带前导点的扩展名，如 `.flac`；没有扩展名时为空串。 |
| `parent` | `string` | 展开 `%变量%` 之后的路径的父目录。路径不做归一化，展开留下的双分隔符会原样保留。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### file.copyAsync

<!-- api-schema:begin file.copyAsync -->
在 worker 线程上启动可取消的批量复制，立即返回回执。结果经 `file:opProgress` 分批送达，最后是一条 `file:opComplete`。全进程同时最多进行 8 个批量操作，超出时以 `OPERATION_FAILED` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `items` | `FileOpEntry[]` | 是 | 要复制的条目，按此顺序每条上报一个结果。条目不是恰好含字符串成员 `source` 与 `destination` 的对象时，调用以 `INVALID_PARAMS` 失败。开始之前逐条检查全部路径，`source` 按读取、`destination` 按写入检查；任何一条被拒（空串也算），整次调用以 `PERMISSION_DENIED` 失败，不产生 `operationId`。不能为空。 |
| `items[].source` | `string` | 是 | 要复制或移动的文件或目录。在 `file:opProgress` 的结果里原样回显，`%变量%` 不展开。不能为空。 |
| `items[].destination` | `string` | 是 | 目标路径；缺失的父目录会被创建。它指向已存在的目录且 `source` 是文件时，文件以原名放进该目录。与 `source` 一样原样回显。不能为空。 |
| `overwrite` | `boolean` | 否 | 替换已存在的文件目标，而不是把该条记为 `skipped` / `already-exists`。目录复制到已存在的目录时总会并入其中；除非设置本项，目录内已存在的文件会被跳过且不单独上报。默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `operationId` | `string` | 操作的 id，以 `fileop_` 开头。该操作的事件带有它，`file.cancelOp` 也用它。 |
| `totalCount` | `integer` | 受理的条目数；事件里以 `total` 报告。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

返回值只是派工回执；结果经 `file:opProgress`（分批）与最后一条 `file:opComplete` 送达。结果按**条目**上报而非按文件：目录条目在整棵树走完后只出一条结果。全进程同时进行的操作上限为 8 个，超出时调用返回 `OPERATION_FAILED`。

目录复制到已存在的目录会**并入**其中：该条目不会被判成 `already-exists`，目录内已存在的文件被跳过且不单独上报（除非传了 `overwrite`），该条目仍记 `status: "ok"`。`already-exists` 跳过只适用于单文件条目。

```javascript
const receipt = await fb2k.invoke('file.copyAsync', {
    items: [{ source: 'C:\\Music\\Album', destination: 'D:\\Backup\\Album' }]
});
```

### file.moveAsync

<!-- api-schema:begin file.moveAsync -->
在 worker 线程上启动可取消的批量移动，回执与事件同 `file.copyAsync`。同卷移动是一次改名；跨卷时先复制该条目再删除源。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `items` | `FileOpEntry[]` | 是 | 要移动的条目，按此顺序每条上报一个结果。条目不是恰好含字符串成员 `source` 与 `destination` 的对象时，调用以 `INVALID_PARAMS` 失败。开始之前逐条按写入检查全部路径，`source` 也不例外，因为移动会删掉它；任何一条被拒（空串也算），整次调用以 `PERMISSION_DENIED` 失败，不产生 `operationId`。不能为空。 |
| `items[].source` | `string` | 是 | 要复制或移动的文件或目录。在 `file:opProgress` 的结果里原样回显，`%变量%` 不展开。不能为空。 |
| `items[].destination` | `string` | 是 | 目标路径；缺失的父目录会被创建。它指向已存在的目录且 `source` 是文件时，文件以原名放进该目录。与 `source` 一样原样回显。不能为空。 |
| `overwrite` | `boolean` | 否 | 替换已存在的文件目标，而不是把该条记为 `skipped` / `already-exists`；同步的 `file.move` 总是替换。已存在的目录目标永远不会被替换：Windows 不能原地换掉目录，同卷下这样的条目以 `skipped` 或 `failed` 结束。默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `operationId` | `string` | 操作的 id，以 `fileop_` 开头。该操作的事件带有它，`file.cancelOp` 也用它。 |
| `totalCount` | `integer` | 受理的条目数；事件里以 `total` 报告。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

派工回执；结果经 `file:opProgress` 与 `file:opComplete` 送达。同卷移动是一次改名，与体积无关；跨卷时宿主自动复制后删源，该条目仍记 `status: "ok"` 但带 `reason: "cross-volume"`，让调用方看得见这一条的代价。目录跨卷同样走这条回退 —— 这是同步版 `file.move` 做不到的。

与 `file.copyAsync` 不同：目标目录已存在时该条目记 `skipped` / `already-exists` 而非并入。传 `overwrite` 也改不了这一点，见上面参数表的说明。

```javascript
const receipt = await fb2k.invoke('file.moveAsync', {
    items: [{ source: 'C:\\Inbox\\Album', destination: 'D:\\Music\\Album' }]
});
```

### file.deleteAsync

<!-- api-schema:begin file.deleteAsync -->
启动可取消的批量删除，回执与事件同 `file.copyAsync`，结果不带 `destination`。回收站删除在主线程上每 16 条一批执行，永久删除走 worker 线程。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `string[]` | 是 | 要删除的路径，按此顺序每条上报一个结果。有非字符串元素时调用以 `INVALID_PARAMS` 失败。开始之前逐条按写入检查全部路径；任何一条被拒（空串也算），整次调用以 `PERMISSION_DENIED` 失败，不产生 `operationId`。不能为空。 |
| `moveToTrash` | `boolean` | 否 | 把每一条移入回收站。为 `false` 时永久删除，并且能删除非空目录，这是同步的 `file.delete` 做不到的。默认 `true`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `operationId` | `string` | 操作的 id，以 `fileop_` 开头。该操作的事件带有它，`file.cancelOp` 也用它。 |
| `totalCount` | `integer` | 受理的条目数；事件里以 `total` 报告。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

派工回执；结果经 `file:opProgress` 与 `file:opComplete` 送达。回收站删除受 shell API 的 STA 约束，在宿主主线程上每 16 条一批执行；永久删除走 worker 线程且能删非空目录 —— 同步版 `file.delete` 在 `moveToTrash: false` 下删不了非空目录。

```javascript
const receipt = await fb2k.invoke('file.deleteAsync', {
    paths: ['%profile%\\cache\\a.json', '%profile%\\cache\\old'],
    moveToTrash: false
});
```

### file.cancelOp

<!-- api-schema:begin file.cancelOp -->
停止由 `file.copyAsync`、`file.moveAsync` 或 `file.deleteAsync` 启动的操作。尚未处理的条目记为 `skipped` / `cancelled`，收尾仍会发出 `file:opComplete`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `operationId` | `string` | 是 | `file.copyAsync`、`file.moveAsync` 或 `file.deleteAsync` 回执里的 id。不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `cancelled` | `boolean` | 操作仍在进行并已被通知停止时为 `true`；已结束或从未存在时为 `false`，两者故意不可区分。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

`cancelled: false` 表示该操作已结束或从未存在，两者故意不可区分。复制/移动在一个文件之内停下（正在传输的那个文件被中止，其残片被删除），删除在下一条之前停下。已处理的条目保留各自结果，其余记 `skipped` / `cancelled`，收尾仍会发一条 `cancelled: true` 的 `file:opComplete`。

关闭发起该操作的 **popup 窗口**等效于取消，退出 foobar2000 同理。panel 宿主没有这个钩子：它发起的操作会一直跑到结束，除非用本方法停掉，其事件此后走上文所述的回退路径。

```javascript
const res = await fb2k.invoke('file.cancelOp', { operationId });
if (res.success === false) throw new Error(res.error);
const { cancelled } = res;
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
| `reason` | `string` | 条目干净成功时无此字段；否则为 `already-exists` / `not-found` / `permission` / `cross-volume` / `path-too-long` / `io-error` / `cancelled`。 |

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

## 文件边界

- 文件 API 会在访问前展开 `%profile%`、`%component%`、`%music%`、`%APPDATA%`、`%TEMP%`。每个端点按注册的 `SecurityLevel` 校验 —— 读端点走 `Read`，`file.*` 的全部写端点走 `FileWrite`，逐端点对照表见[权限系统](/zh/reference/permissions)。`file.write` 会创建缺失的父目录，`file.delete` 默认移入回收站。
- `file.*` 的每一条失败都在 `error` 之外带 `code`。由文件系统本身抛出的失败还带 `details.value`，即原始 Win32 错误号。`error` 与 `details` 都不会包含路径。
- `file.list` 在非递归模式下返回名称，在递归模式下返回完整路径。`file.getInfo` 以成功的不存在结果返回 `exists: false`。
- 异步族（`copyAsync` / `moveAsync` / `deleteAsync`）的返回值只是派工回执，逐条结果只出现在 `file:opProgress` / `file:opComplete` 的 payload 里；这两个事件单窗口投递，因此 `results` 里带路径，而错误信封与日志仍然不带。同步的 `file.copy` / `file.move` / `file.delete` 行为不受本组影响。
