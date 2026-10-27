# Metadata 元数据 API

`metadata` 命名空间提供元数据读写、封面嵌入与批量操作能力；`metadata.removeField` 与 `metadata.removeTag` 行为完全一致。

## 定位容器内的单曲 {#subsong-addressing}

CUE、ISO 镜像与多轨文件均表现为一个文件路径包含多首曲目；所有读取方法采用统一规范定位具体曲目：

- 在路径后追加 `|subsong:N`（例如 `D:\album.cue|subsong:2`）
- 或在普通路径之外另传 `cueIndex: N`（两者同时给出时以 `cueIndex` 为准）

编号沿用 foobar2000 宿主规则，**CUE 从 1 开始**：`|subsong:1` 代表第一首曲目。不带后缀的容器路径（或 `|subsong:0`）指向 subsong 0，因 CUE 中不存在编号 0 会返回 `Failed to get track info`（此为宿主索引设计，非调用格式错误）。

读取普通单轨文件无需后缀；`|subsong:0` 与缺省完全等价。

## 读取

### metadata.read

读取指定文件的元数据（结构化格式）。

<!-- api-schema:begin metadata.read -->
读取单个曲目的元数据标签与音频技术信息；优先读取宿主内存缓存，若缓存缺失或未收录标题标签则回退至直接读取音频文件；文件无法打开或读取失败时以 `OPERATION_FAILED` 失败；字段值中的非法 UTF-8 字符替换为 U+FFFD，字段名包含非法 UTF-8 的项自动省略且不修改原文件

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 目标曲目路径；支持 `\|subsong:N` 后缀或尾部 `#N` 指定子曲目不能为空。 |
| `cueIndex` | `integer` | 否 | 显式指定的子曲目序号（优先级高于 `path` 中的后缀）；传入负数时忽略默认 `-1`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `path` | `string` | 请求传入的原样路径（包含后缀） |
| `tags` | `Record<string, any>` | 曲目包含的全部标签字段（以文件内原始大小写为键名）：单值标签解析为字符串，多值标签解析为字符串数组 |
| `info` | `TrackTechnicalInfo` | 曲目的音频技术参数 |
| `info.duration` | `number` | 曲目时长（秒） |
| `info.bitrate` | `integer` | 解码器报告的音频平均码率（kbit/s）；未知时为 `0` |
| `info.sampleRate` | `integer` | 音频采样率（Hz）；未知时为 `0` |
| `info.channels` | `integer` | 声道数量；未知时为 `0` |
| `info.codec` | `string` | 解码器报告的音频编码名称（如 `FLAC`、`MP3`）；未知时为空字符串 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

> 读取链路会先剥离 `|subsong:N` 后缀并 canonicalize 路径，再按解析出的 subsong 通过 `handle_create()` 读取 cached info；若 cached info 缺少关键元数据，则以同一 subsong 退回 direct file read。CUE / ISO 等多轨容器的寻址规则见[定位容器内的单曲](#subsong-addressing)。

> `tags` 保留文件内原始字段名，不强制转为大写；如果你需要统一的大写扁平字段，请使用 `metadata.readByPath`。

### metadata.readRaw

（v1.4.1+）直接从文件读取元数据，绕过 metadb 缓存。

与 `metadata.read` 返回格式一致，但始终从磁盘文件直接解码读取，不走 foobar2000 metadb 内存缓存。适用于需要获取最新文件标签的场景（如刚写入标签后立即读回验证）。

<!-- api-schema:begin metadata.readRaw -->
绕过宿主内存缓存，直接从物理文件读取单个曲目的元数据标签与技术信息；返回结构同 `metadata.read` 且额外附带 `source: "file"` 字段；文件读取失败以 `OPERATION_FAILED` 失败

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 目标曲目路径；支持 `\|subsong:N` 后缀或尾部 `#N` 指定子曲目不能为空。 |
| `cueIndex` | `integer` | 否 | 显式指定的子曲目序号（优先级高于 `path` 中的后缀）；传入负数时忽略默认 `-1`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `path` | `string` | 请求传入的原样路径（包含后缀） |
| `tags` | `Record<string, any>` | 曲目包含的全部标签字段（结构同 `metadata.read`） |
| `info` | `TrackTechnicalInfo` | 曲目的音频技术参数 |
| `info.duration` | `number` | 曲目时长（秒） |
| `info.bitrate` | `integer` | 解码器报告的音频平均码率（kbit/s）；未知时为 `0` |
| `info.sampleRate` | `integer` | 音频采样率（Hz）；未知时为 `0` |
| `info.channels` | `integer` | 声道数量；未知时为 `0` |
| `info.codec` | `string` | 解码器报告的音频编码名称（如 `FLAC`、`MP3`）；未知时为空字符串 |
| `source` | `string` | 元数据来源标识，固定为 `file`（直接读取物理文件而非内存缓存） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

> 始终直接打开文件解码器读取，不受 metadb 缓存影响。`source` 字段固定为 `"file"`。

```javascript
const raw = await fb2k.invoke('metadata.readRaw', { path: 'E:\\Music\\song.flac' });
if (raw.success === false) throw new Error(raw.error);
console.log(raw.tags.TITLE, raw.source); // "file"
```

### metadata.readByPath

（v1.1.0+）读取元数据（扁平格式）。

与 `metadata.read` 不同，此 API 返回扁平结构，所有标签键名转为大写；但两者共享同一条 fallback 读取链路。

<!-- api-schema:begin metadata.readByPath -->
以扁平键值对象格式读取单个曲目元数据；每个标签与技术信息字段均转为大写键名（仅转换 ASCII 字母）与 `path` 并列输出为顶层属性；读取机制、失败规则与 UTF-8 处理逻辑与 `metadata.read` 完全一致

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 目标曲目路径；支持 `\|subsong:N` 后缀或尾部 `#N` 指定子曲目不能为空。 |
| `cueIndex` | `integer` | 否 | 显式指定的子曲目序号（优先级高于 `path` 中的后缀）；传入负数时忽略默认 `-1`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `path` | `string` | 请求传入的原样路径（包含后缀） |
| `[每个键]` | `any` | 曲目的单个元数据标签或音频技术信息字段（键名均为大写）；单值标签为字符串，多值标签表现为字符串数组，技术信息字段为字符串；`DURATION` 始终存在（以秒为单位保留 3 位小数的持续时长，如 `215.400`）；`FILESIZE` 表示文件字节数（仅在宿主获知时存在，媒体库尚未索引的文件不含该字段）；若文件缺失 `TRACKNUMBER` 标签，则自动尝试从文件名起始处不超过 3 位的数字推断（支持 `07 - Song.flac`、`(07) Song.flac`、`[07] Song.flac` 等格式），无法提取时不输出 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

> 若文件缺少 `TRACKNUMBER` 标签，会尝试从文件名提取。若 cached info 不完整，会自动退回 direct file read。多轨容器寻址见[定位容器内的单曲](#subsong-addressing)。

```javascript
const meta = await fb2k.invoke('metadata.readByPath', { path: 'E:\\Music\\song.flac' });
if (meta.success === false) throw new Error(meta.error);
console.log(meta.TITLE, meta.ARTIST, meta.DURATION);
```

### metadata.readBatch

（v1.1.11+）批量读取多个文件的元数据，包含所有标签和技术信息（大写键名）。

<!-- api-schema:begin metadata.readBatch -->
批量读取多个曲目的扁平元数据字段，按输入路径顺序逐行返回；单项路径读取失败在对应行报告自身错误而不导致整次调用失败；在宿主主线程同步执行（大量文件推荐改用 `metadata.probeBatchAsync`）；UTF-8 字符处理规则同 `metadata.read`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `string[]` | 是 | 待逐条独立解析读取的曲目路径列表；支持 `\|subsong:N` 后缀或 `#N` 语法；传入空列表直接返回空成功 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `total` | `integer` | 本次批量请求受理的路径总数 |
| `successCount` | `integer` | 成功读取并解析元数据的路径总数 |
| `errorCount` | `integer` | 读取或解析失败的路径总数 |
| `results` | `MetadataReadBatchItem[]` | 批量读取结果列表，严格保持与请求一致的顺序 |
| `results[].path` | `string` | 请求传入的原样路径 |
| `results[].success` | `boolean` | 该曲目是否成功读取 |
| `results[].tags` | `Record<string, any>` | 曲目的扁平元数据字段字典（结构同 `metadata.readByPath`，但不含 `path` 且不从文件名猜测 `TRACKNUMBER`）；仅在 `success: true` 时返回 |
| `results[].error` | `string` | 该曲目读取失败的原因说明；仅在 `success: false` 时返回 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

> 本方法没有 `cueIndex` 参数：批量调用中每个路径只能通过 `|subsong:N` 指定曲目。见[定位容器内的单曲](#subsong-addressing)。

```javascript
const batch = await fb2k.invoke('metadata.readBatch', {
    paths: ['E:\\Music\\a.flac', 'E:\\Music\\b.flac']
});
if (batch.success === false) throw new Error(batch.error);
batch.results.forEach(r => {
    if (r.success) console.log(r.tags.TITLE);
});
```

### metadata.probeBatchAsync

可取消的批量元数据探测；读盘在 worker 线程，不阻塞 UI。

<!-- api-schema:begin metadata.probeBatchAsync -->
在后台工作线程启动可取消的异步批量元数据探测任务并立即返回任务回执；探测结果分批通过 `metadata:probeProgress` 事件推送（每累积 64 条或每隔 100 毫秒推送一批），并在任务结束时投递 `metadata:probeComplete` 事件；单项结果标明数据源来自缓存还是磁盘，失败项分类标记为 `not-found`、`unsupported-format` 或 `read-error`；UTF-8 处理规则同 `metadata.read`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `string[]` | 是 | 待异步探测的路径列表，按传入顺序逐条上报；支持 `\|subsong:N` 后缀（不支持 `#N` 语法以避免误切文件名）；开始前预检全部路径，任一受拒整次调用以 `PERMISSION_DENIED` 失败不能为空。 |
| `includeTags` | `boolean` | 否 | 是否为每条成功结果附带扁平元数据字段；设为 `false` 时仅返回音频技术参数默认 `true`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `operationId` | `string` | 异步探测任务唯一标识符（以 `probe_` 开头），后续事件携带此 ID，取消操作亦须传入 |
| `totalCount` | `integer` | 本次任务受理的路径总数（后续事件中以 `total` 报告） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

返回值只是派工回执，结果经事件送达：`metadata:probeProgress`（分批）与收尾的 `metadata:probeComplete`（被取消的批次同样以 `cancelled: true` 收尾）。每条结果带 `infoSource`（`cached` / `direct` / `none`），失败项带 `failure`（`not-found` / `unsupported-format` / `read-error`）。

```javascript
const receipt = await fb2k.invoke('metadata.probeBatchAsync', {
    paths: ['E:\\Music\\a.flac', 'E:\\Music\\b.flac']
});
```

### metadata.cancelProbe

取消进行中的 `metadata.probeBatchAsync` 操作。

<!-- api-schema:begin metadata.cancelProbe -->
停止由 `metadata.probeBatchAsync` 启动的正在运行的异步探测任务；立即中断当前正在执行的文件读取，未到达的后续路径不再上报，收尾依然触发标记为 `cancelled: true` 的 `metadata:probeComplete` 事件

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `operationId` | `string` | 是 | 对应 `metadata.probeBatchAsync` 返回的操作标识符 `operationId`不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `cancelled` | `boolean` | 探测任务是否成功发送停止信号；若任务已结束或不存在则返回 `false`（两者故意不可区分） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

`cancelled: false` 表示该操作已结束或从未存在，两者对调用方故意不可区分。

```javascript
const res = await fb2k.invoke('metadata.cancelProbe', { operationId });
if (res.success === false) throw new Error(res.error);
const { cancelled } = res;
```

## 写入

### metadata.write

写入元数据标签到文件。使用 `metadb_io_v2::update_info_async` 异步写入。标签键名自动转为大写。

<!-- api-schema:begin metadata.write -->
将针对单个曲目的标签写入请求加入后台队列并立即返回；foobar2000 宿主在后台线程执行物理文件写入，并在操作完成后向所有窗口广播 `metadata:writeComplete` 事件；请求中未包含有效待写标签时直接返回成功且不派发后台写入

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 目标曲目路径；支持 `\|subsong:N` 后缀或尾部 `#N` 指定子曲目不能为空。 |
| `tags` | `Record<string, any>` | 是 | 待写入或修改的标签字典（键名不区分大小写自动转为大写）；传入 `null`、空字符串或空数组表示删除该标签；非空字符串数组全量替换该标签的所有取值并保留原序与空白（不按逗号或分号拆分）；数组元素必须为非空且不含 NUL 字符的字符串；标量数字转为十进制文本；布尔值与对象自动忽略；最终写入效果以完成事件为准 |
| `cueIndex` | `integer` | 否 | 显式指定的子曲目序号（优先级高于 `path` 中的后缀）；传入负数时忽略默认 `-1`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `path` | `string` | 请求传入的原样路径 |
| `note` | `string` | 状态提示说明（无待写内容时返回 `No tags to update`，否则提示通过 `metadata:writeComplete` 广播最终结果） |
| `dispatched` | `boolean` | 写入请求是否已成功派发至后台线程；无待写内容时缺省 |
| `handlePath` | `string` | 写入操作所针对的曲目句柄存储路径；派发时返回 |
| `subsong` | `integer` | 写入操作所针对的目标子曲目序号；派发时返回 |
| `tagsApplied` | `Record<string, any>` | 排入写入队列的全部标签字典（键名为大写标签名，值为字符串、数字或字符串数组，删除项记为 `null`）；派发时返回 |
| `tagsSet` | `integer` | 本次成功设置的标签字段数量（按字段数而非值数统计）；派发时返回 |
| `tagsRemoved` | `integer` | 本次成功删除的标签字段数量（包含传入空数组的项）；派发时返回 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

::: tip 异步派发模式
`metadata.write` 立即返回 `dispatched: true`，表示写入已提交给 foobar2000 引擎。实际完成后会广播 `metadata:writeComplete` 事件（见下文）。
:::

```javascript
await fb2k.invoke('metadata.write', {
    path: 'E:\\Music\\song.flac',
    tags: { TITLE: 'New Title', ARTIST: 'New Artist', COMMENT: null }
});

// CUE 子轨写入
await fb2k.invoke('metadata.write', {
    path: 'E:\\Music\\album.flac|subsong:2',
    tags: { COMMENT: 'Track 3 comment' }
});
```

### metadata.removeTag

移除指定标签。标签名自动转为大写。

<!-- api-schema:begin metadata.removeTag -->
将针对单个曲目的标签删除请求加入后台队列并立即返回；操作完成后向所有窗口广播 `operation: "removeTag"` 的 `metadata:writeComplete` 事件

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 目标曲目路径；支持 `\|subsong:N` 后缀或尾部 `#N` 指定子曲目不能为空。 |
| `tags` | `string[]` | 是 | 待删除的标签名称数组（自动转为大写比对）；传入空数组不执行删除且直接返回成功 |
| `cueIndex` | `integer` | 否 | 显式指定的子曲目序号（优先级高于 `path` 中的后缀）；传入负数时忽略默认 `-1`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `path` | `string` | 请求传入的原样路径 |
| `removedTags` | `string[]` | 成功排入删除队列的标签名称列表（已转为大写并保持输入顺序） |
| `removedCount` | `integer` | 成功排入删除队列的标签字段数量 |
| `dispatched` | `boolean` | 删除请求是否已成功派发至后台线程；`tags` 为空时缺省 |
| `subsong` | `integer` | 删除操作所针对的目标子曲目序号；派发时返回 |
| `note` | `string` | 状态提示说明（提示最终删除结果由 `metadata:writeComplete` 广播） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
await fb2k.invoke('metadata.removeTag', {
    path: 'E:\\Music\\song.flac',
    tags: ['COMMENT', 'LYRICS']
});

// CUE 子轨移除
await fb2k.invoke('metadata.removeTag', {
    path: 'E:\\Music\\album.flac|subsong:2',
    tags: ['COMMENT']
});
```

### metadata.writeBatch

批量写入多个文件的元数据。逐个调用 `metadata.write`。

<!-- api-schema:begin metadata.writeBatch -->
批量排入多个曲目的标签写入请求，并报告成功入队的数量；已派发的单项写入在完成后独立广播 `metadata:writeComplete` 事件（不存在的文件在此阶段亦计入入队成功）；若有条目参数非法则整次调用以 `OPERATION_FAILED` 失败（错误详情附带 `successCount`、`failCount` 与 `errors` 列表，其他合法项照常入队）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `items` | `MetadataWriteBatchItem[]` | 是 | 待批量排入的写入条目列表；条目结构非法以 `INVALID_PARAMS` 失败；开始前预检全部路径，任一路径受拒整次调用以 `PERMISSION_DENIED` 失败；空列表返回空成功 |
| `items[].path` | `string` | 是 | 目标曲目路径不能为空。 |
| `items[].tags` | `any` | 否 | 待写入或修改的标签字典；缺失或非对象时在 `errors` 中报告 `Missing tags` 而不拒绝整批任务 |
| `items[].cueIndex` | `integer` | 否 | 显式指定的子曲目序号；负数忽略默认 `-1`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `successCount` | `integer` | 成功派发写入或无须写入的条目总数 |
| `failCount` | `integer` | 派发失败的条目总数（整批成功时固定为 `0`） |
| `errors` | `MetadataWriteBatchError[]` | 派发失败的条目详情列表（整批成功时固定为空数组） |
| `errors[].path` | `string` | 失败条目的曲目路径 |
| `errors[].error` | `string` | 失败原因说明 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
await fb2k.invoke('metadata.writeBatch', {
    items: [
        { path: 'E:\\Music\\a.flac', tags: { GENRE: 'Rock' } },
        { path: 'E:\\Music\\b.flac', tags: { GENRE: 'Pop' } }
    ]
});
```

### metadata.embedArtwork

将封面图嵌入到音频文件。使用 foobar2000 SDK 的 `album_art_editor`。

<!-- api-schema:begin metadata.embedArtwork -->
将封面图片写入音频文件内嵌标签、写为同目录下的独立图片文件或两处同时写入；指定单个目标时返回该目标的写入结果；同时指定两处目标时在 `results` 下分别列出各自状态，仅当两处写入均失败时整次调用以 `OPERATION_FAILED` 失败

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 目标曲目路径；`embedded` 目标写入该文件（须格式支持），`file` 目标写入所在目录并剥离 `\|subsong:N` 后缀（同 CUE 曲目共享单张图）不能为空。 |
| `imageData` | `string` | 是 | 待内嵌图片的裸 Base64 字符串（不带 Data URL 前缀与 `base64:` 标记）；非字母表字符自动忽略并在首个 `=` 处终止解码；无有效字节时以 `INVALID_PARAMS` 失败不能为空。 |
| `type` | `"front" \| "cover_front" \| "back" \| "cover_back" \| "disc" \| "icon" \| "artist"` | 否 | 待内嵌的图片类型默认 `"front"`。 |
| `target` | `string[]` | 否 | 写入目标位置数组：`embedded`（写入音频内嵌标签）、`file`（音频同目录下外置图片，正面图命名为 `cover`，扩展名由字节解析）或 `all`（两处均写）；省略时默认 `["embedded"]`，非法值以 `INVALID_PARAMS` 失败不能为空。 |
| `filename` | `string` | 否 | `file` 目标指定的自定义文件名（原样使用不补扩展名）；必须为不含路径分隔符或穿越序列的纯文件名 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `path` | `string` | 请求传入的原样路径 |
| `type` | `string` | 请求传入的原样图片类型 |
| `size` | `integer` | 解码后的图片二进制字节数；仅在指定单个目标时返回 |
| `savedTo` | `string` | 写入的独立图片文件的完整物理路径；仅在指定单个 `file` 目标时返回 |
| `results` | `Record<string, any>` | 同时写入两处目标时各自的写入信封详情字典（键名为 `embedded` 与 `file`） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

**封面类型**：`cover_front` 与 `cover_back` 分别等同于 `front` 与 `back`；其他取值以 `INVALID_PARAMS` 失败。

```javascript
// 将 Base64 图片嵌入为封面
await fb2k.invoke('metadata.embedArtwork', {
    path: 'E:\\Music\\song.flac',
    imageData: base64String,
    type: 'front'
});
```

`imageData` 必须是**裸 Base64 图片字节**。不要传
`data:image/...;base64,` 头、`base64:` 标记或 `fb2k://` URL。若来源是
Artwork API 返回的标准 Data URL，应先取第一个逗号之后的 payload：

```javascript
const cover = await fb2k.invoke('artwork.getCurrent', { type: 'front' });
if (cover.success === false) throw new Error(cover.error);
const payload = cover.dataUrl.slice(cover.dataUrl.indexOf(',') + 1);
await fb2k.invoke('metadata.embedArtwork', {
    path: 'E:\\Music\\song.flac',
    imageData: payload,
    type: 'front',
});
```

### metadata.removeEmbeddedArt

移除音频文件中的嵌入封面。

<!-- api-schema:begin metadata.removeEmbeddedArt -->
从音频文件中移除内嵌的封面图片；支持按 `type` 删除指定类型，或在省略 `type` 及设置 `removeAll: true` 时全量清空；音频格式不受专辑封面编辑器支持时以 `NOT_SUPPORTED` 失败

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 目标曲目路径（音频格式须受专辑封面编辑器支持）不能为空。 |
| `type` | `"front" \| "cover_front" \| "back" \| "cover_back" \| "disc" \| "icon" \| "artist"` | 否 | 待删除的图片类型；省略时默认删除所有内嵌图片 |
| `removeAll` | `boolean` | 否 | 是否全量删除所有内嵌图片（优先级高于 `type`）默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `path` | `string` | 请求传入的原样路径 |
| `removedTypes` | `string[]` | 实际执行删除的图片类型列表：全量删除时为 `["all"]`，否则为逐个删除的类型名称列表 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### metadata.removeField

`metadata.removeTag` 的别名。移除指定标签。

<!-- api-schema:begin metadata.removeField -->
从单个曲目中删除指定标签（旧名兼容接口，功能同 `metadata.removeTag`，完成事件同样标识为 `removeTag`）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 目标曲目路径；支持 `\|subsong:N` 后缀或尾部 `#N` 指定子曲目不能为空。 |
| `tags` | `string[]` | 是 | 待删除的标签名称数组（自动转为大写比对）；传入空数组不执行删除且直接返回成功 |
| `cueIndex` | `integer` | 否 | 显式指定的子曲目序号（优先级高于 `path` 中的后缀）；传入负数时忽略默认 `-1`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `path` | `string` | 请求传入的原样路径 |
| `removedTags` | `string[]` | 成功排入删除队列的标签名称列表（已转为大写并保持输入顺序） |
| `removedCount` | `integer` | 成功排入删除队列的标签字段数量 |
| `dispatched` | `boolean` | 删除请求是否已成功派发至后台线程；`tags` 为空时缺省 |
| `subsong` | `integer` | 删除操作所针对的目标子曲目序号；派发时返回 |
| `note` | `string` | 状态提示说明（提示最终删除结果由 `metadata:writeComplete` 广播） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

## 事件

### metadata:writeComplete

`metadata.write` 或 `metadata.removeTag` 异步写入完成时向所有打开的窗口广播此事件。

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `operation` | `string` | 触发操作名称（`"write"` 或 `"removeTag"`） |
| `path` | `string` | 音频文件路径 |
| `subsong` | `number` | 子曲目 subsong 序号 |
| `code` | `number` | 完成状态码（0 成功、1 中止、2 错误） |
| `success` | `boolean` | 写入是否成功 |
| `status` | `string` | 状态字符串（`"success"`、`"aborted"` 或 `"error"`） |

```javascript
fb2k.on('metadata:writeComplete', (e) => {
    if (e.success) {
        console.log(`写入完成: ${e.path} subsong=${e.subsong}`);
    } else {
        console.error(`写入失败: ${e.status}`);
    }
});
```

## 使用说明

- `metadata.read`、`metadata.readByPath` 与 `metadata.readRaw` 均需要 `path`，并支持可选的 `cueIndex`（默认 `-1`）；可通过 `path|subsong:N` 或 `cueIndex` 寻址容器子曲目；`readRaw` 绕过 metadb 缓存直接读盘，返回结果在 `metadata.read` 的结构化 `{ success, path, tags, info }` 基础上追加 `source: "file"`
- `metadata.write`、`metadata.removeTag` 与兼容接口 `metadata.removeField` 均采用异步派发；派发成功仅表示任务已入队，不代表已持久化落盘；请通过监听广播事件 `metadata:writeComplete`（payload 为 `{ operation, path, subsong, code, success, status }`）获取最终写入结果
- `metadata.embedArtwork` 必须传入非空 `path` 与裸 Base64 字符串 `imageData`；`type` 默认 `front`（`cover_front` 与 `cover_back` 分别等同 `front` 与 `back`，其余取值以 `INVALID_PARAMS` 失败）；`target` 为数组（省略时默认 `["embedded"]`，支持 `embedded`、`file`、`all` 的任意组合，直接传入裸字符串会以 `INVALID_PARAMS` 失败；SDK 封装会自动将单目标包装为数组）；同时指定两处目标时各目标状态记录于 `results`，任一目标成功即整次调用成功，仅当两处均失败时以 `OPERATION_FAILED` 失败（错误信封同样附带 `results`）
- `metadata.removeEmbeddedArt` 移除指定 `type` 的内嵌图片；省略 `type` 或显式设置 `removeAll: true` 时全量清空所有内嵌封面；目标音频格式必须受 SDK 的 `album_art_editor` 接口支持
