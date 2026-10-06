# Metadata 元数据 API

`metadata` 命名空间的方法：元数据读写、封面嵌入、批量操作。`metadata.removeField` 与 `metadata.removeTag` 行为相同。

## 定位容器内的单曲 {#subsong-addressing}

CUE、ISO 镜像与多轨文件都是一个文件路径下含多首曲目。所有读取方法用同一种方式定位其中一首：

- 在路径后追加 `|subsong:N`，例如 `D:\album.cue|subsong:2`；
- 或在普通路径之外另传 `cueIndex: N`。两者同时给出时以 `cueIndex` 为准。

编号沿用 foobar2000 的规则，**CUE 从 1 开始**：`|subsong:1` 是第一首。不带后缀的容器路径（或 `|subsong:0`）指向 subsong 0，而 CUE 中并不存在该编号，因此会返回 `Failed to get track info` —— 这是宿主的编号方式，不是请求写错了。

读取普通单轨文件无需后缀；`|subsong:0` 与不写等价。

## 读取

### metadata.read

读取指定文件的元数据（结构化格式）。

<!-- api-schema:begin metadata.read -->
读取单个曲目的标签与技术信息。先读宿主缓存的信息，拿不到或其中没有标题标签时改为直接读文件；打不开或读不出的曲目以 `OPERATION_FAILED` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 曲目路径；`\|subsong:N` 后缀或结尾的 `#N` 选子曲目。不能为空。 |
| `cueIndex` | `integer` | 否 | 子曲目序号，优先于 `path` 里的后缀；负数忽略。默认 `-1`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `path` | `string` | 原样的路径，含后缀。 |
| `tags` | `Record<string, any>` | 曲目的全部标签，键为文件里原样的字段名（保留大小写）：单个值的标签是字符串，多个值的是字符串数组。 |
| `info` | `TrackTechnicalInfo` | 曲目的技术信息。 |
| `info.duration` | `number` | 时长，单位秒。 |
| `info.bitrate` | `integer` | 解码器报告的码率，单位 kbit/s；未知时为 `0`。 |
| `info.sampleRate` | `integer` | 采样率，单位 Hz；未知时为 `0`。 |
| `info.channels` | `integer` | 声道数；未知时为 `0`。 |
| `info.codec` | `string` | 解码器报告的编码名，如 `FLAC`、`MP3`；未知时为空。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

> 读取链路会先剥离 `|subsong:N` 后缀并 canonicalize 路径，再按解析出的 subsong 通过 `handle_create()` 读取 cached info；若 cached info 缺少关键元数据，则以同一 subsong 退回 direct file read。CUE / ISO 等多轨容器的寻址规则见[定位容器内的单曲](#subsong-addressing)。

> `tags` 保留文件内原始字段名，不强制转为大写；如果你需要统一的大写扁平字段，请使用 `metadata.readByPath`。

### metadata.readRaw

（v1.4.1+）直接从文件读取元数据，绕过 metadb 缓存。

与 `metadata.read` 返回格式一致，但始终从磁盘文件直接解码读取，不走 foobar2000 metadb 内存缓存。适用于需要获取最新文件标签的场景（如刚写入标签后立即读回验证）。

<!-- api-schema:begin metadata.readRaw -->
绕过宿主缓存，直接从文件读取单个曲目的标签与技术信息；结果同 `metadata.read`，另加 `source`。读不出的文件以 `OPERATION_FAILED` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 曲目路径；`\|subsong:N` 后缀或结尾的 `#N` 选子曲目。不能为空。 |
| `cueIndex` | `integer` | 否 | 子曲目序号，优先于 `path` 里的后缀；负数忽略。默认 `-1`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `path` | `string` | 原样的路径，含后缀。 |
| `tags` | `Record<string, any>` | 曲目的全部标签，同 `metadata.read`。 |
| `info` | `TrackTechnicalInfo` | 曲目的技术信息。 |
| `info.duration` | `number` | 时长，单位秒。 |
| `info.bitrate` | `integer` | 解码器报告的码率，单位 kbit/s；未知时为 `0`。 |
| `info.sampleRate` | `integer` | 采样率，单位 Hz；未知时为 `0`。 |
| `info.channels` | `integer` | 声道数；未知时为 `0`。 |
| `info.codec` | `string` | 解码器报告的编码名，如 `FLAC`、`MP3`；未知时为空。 |
| `source` | `string` | 恒为 `file`：标签直接读自文件，而不是宿主的缓存。 |

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
以扁平对象读取单个曲目：每个标签与技术信息字段都以大写名成为顶层键，与 `path` 并列。读取方式与失败情形同 `metadata.read`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 曲目路径；`\|subsong:N` 后缀或结尾的 `#N` 选子曲目。不能为空。 |
| `cueIndex` | `integer` | 否 | 子曲目序号，优先于 `path` 里的后缀；负数忽略。默认 `-1`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `path` | `string` | 原样的路径，含后缀。 |
| `[每个键]` | `any` | 曲目的一个标签或技术信息字段，键为大写名。单个值的标签是字符串，多个值的是字符串数组；技术信息字段是字符串。`DURATION` 总会出现：以秒计、保留三位小数的时长，如 `215.400`。`FILESIZE` 是字节数，只在宿主知道时出现，所以媒体库还没索引的文件没有它。文件没有 `TRACKNUMBER` 标签时，从文件名开头不超过三位的数字取得（`07 - Song.flac`、`(07) Song.flac`、`[07] Song.flac`），取不到时不出现。 |

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
读取多个曲目的扁平字段，每个路径一行。读不出的路径那一行带自己的错误，不会让整次调用失败。读取在主线程上进行；文件多时改用 `metadata.probeBatchAsync`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `string[]` | 是 | 曲目路径，逐条独立解析；`\|subsong:N` 后缀或结尾的 `#N` 选子曲目，没有整批的 `cueIndex`。空列表返回空的成功。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `total` | `integer` | 收到的路径数。 |
| `successCount` | `integer` | 读取成功的行数。 |
| `errorCount` | `integer` | 失败的行数。 |
| `results` | `MetadataReadBatchItem[]` | 每个路径一行，顺序与请求一致。 |
| `results[].path` | `string` | 该行的路径，原样。 |
| `results[].success` | `boolean` | 该行是否读取成功。 |
| `results[].tags` | `Record<string, any>` | 曲目的扁平字段，同 `metadata.readByPath`，但不带 `path`，也不从文件名取 `TRACKNUMBER`；`success` 为 `true` 时出现。 |
| `results[].error` | `string` | 该行失败的原因，如 `Failed to get track info`；`success` 为 `false` 时出现。 |

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
在 worker 线程上启动可取消的批量探测，立即返回回执。结果经 `metadata:probeProgress` 分批送达（每 64 条或每 100 ms 一批），最后是一条 `metadata:probeComplete`。每条结果说明信息来自缓存还是磁盘，失败分类为 `not-found`、`unsupported-format` 或 `read-error`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `string[]` | 是 | 待探测的路径，按此顺序每条上报一个结果。`\|subsong:N` 后缀选子曲目；不认 `#N` 写法，因为它会把以 `#<数字>` 结尾的文件名切开。开始之前逐条检查全部路径；任何一条被拒，整次调用以 `PERMISSION_DENIED` 失败，不产生 `operationId`。不能为空。 |
| `includeTags` | `boolean` | 否 | 为每条成功结果附上扁平字段（同 `metadata.readByPath`，不带 `path`，也不从文件名取 `TRACKNUMBER`）；为 `false` 时只报技术信息。默认 `true`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `operationId` | `string` | 操作的 id，以 `probe_` 开头。该操作的事件带有它，`metadata.cancelProbe` 也用它。 |
| `totalCount` | `integer` | 受理的路径数；事件里以 `total` 报告。 |

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
停止由 `metadata.probeBatchAsync` 启动的探测。正在进行的读取被打断，尚未轮到的路径不会上报，收尾仍会发出带 `cancelled: true` 的 `metadata:probeComplete`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `operationId` | `string` | 是 | `metadata.probeBatchAsync` 回执里的 id。不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `cancelled` | `boolean` | 探测仍在进行并已被通知停止时为 `true`；已结束或从未存在时为 `false`，两者故意不可区分。 |

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
把对单个曲目的标签写入排入队列并立即返回。foobar2000 在后台写文件，结果以广播给所有窗口的 `metadata:writeComplete` 事件报告。请求里没有可写的内容时直接成功，不派发。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 曲目路径；`\|subsong:N` 后缀或结尾的 `#N` 选子曲目。不能为空。 |
| `tags` | `Record<string, any>` | 是 | 要改的标签，以名字为键。名字会转成大写，所以只有大小写不同的键指同一个标签。`null`、空字符串或空数组删除该标签。非空字符串数组替换该标签的全部值，保留顺序、重复值和空白，不按逗号或分号拆分。数组元素必须是非空且不含 NUL 的字符串；非法元素使该曲目以 `INVALID_PARAMS` 失败，不派发它的任何字段，错误中包含标签名和从 0 起的元素下标。单个字符串原样写入；整数或小数写成十进制文本（`2.5` 写成 `2.500000`）；布尔或对象被忽略。文件格式或标签写入器可能限制多值支持，最终写入结果以完成事件为准。 |
| `cueIndex` | `integer` | 否 | 子曲目序号，优先于 `path` 里的后缀；负数忽略。默认 `-1`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `path` | `string` | 原样的路径。 |
| `note` | `string` | 没有可写的内容时为 `No tags to update`；否则提醒最终结果由 `metadata:writeComplete` 事件报告。 |
| `dispatched` | `boolean` | 写入已派发时为 `true`；没有可写的内容时不出现。 |
| `handlePath` | `string` | 写入所针对句柄的路径（foobar2000 存储的形式）；派发时出现。 |
| `subsong` | `integer` | 写入所针对的子曲目；派发时出现。 |
| `tagsApplied` | `Record<string, any>` | 排入的全部标签，键是大写标签名：写入时为给出的字符串、数字或非空字符串数组，删除时（含空数组）为 `null`。派发时出现。 |
| `tagsSet` | `integer` | 设置的标签字段数，不是值的数量；派发时出现。 |
| `tagsRemoved` | `integer` | 删除的标签字段数，包含以空数组指定的字段；派发时出现。 |

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
把对单个曲目的标签删除排入队列并立即返回。结果以广播给所有窗口、`operation` 为 `"removeTag"` 的 `metadata:writeComplete` 事件报告。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 曲目路径；`\|subsong:N` 后缀或结尾的 `#N` 选子曲目。不能为空。 |
| `tags` | `string[]` | 是 | 要删除的标签名，会转成大写。空列表不删除任何标签，直接成功，不派发。 |
| `cueIndex` | `integer` | 否 | 子曲目序号，优先于 `path` 里的后缀；负数忽略。默认 `-1`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `path` | `string` | 原样的路径。 |
| `removedTags` | `string[]` | 排入删除的标签名，已转大写，顺序与给出的一致。 |
| `removedCount` | `integer` | `removedTags` 里的名字数。 |
| `dispatched` | `boolean` | 删除已派发时为 `true`；`tags` 为空时不出现。 |
| `subsong` | `integer` | 删除所针对的子曲目；派发时出现。 |
| `note` | `string` | 提醒最终结果由 `metadata:writeComplete` 事件报告；派发时出现。 |

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
每条排入一次 `metadata.write`，报告成功排入的条数。已派发的写入稍后以 `metadata:writeComplete` 报告各自的结果，所以不存在的文件在这里也计为成功。有任何条目失败时调用以 `OPERATION_FAILED` 失败，失败信封同样带 `successCount`、`failCount` 与 `errors`；其他条目照常排入。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `items` | `MetadataWriteBatchItem[]` | 是 | 要排入的写入，每条一个，按此顺序。条目不是对象、带有 `path`、`tags`、`cueIndex` 以外的成员、或没有字符串 `path` 时，调用以 `INVALID_PARAMS` 失败。开始之前逐条检查全部路径；任何一条被拒（空串也算），整次调用以 `PERMISSION_DENIED` 失败。空列表返回空的成功。 |
| `items[].path` | `string` | 是 | 曲目路径，同 `metadata.write`。不能为空。 |
| `items[].tags` | `any` | 否 | 要改的标签，同 `metadata.write`。`tags` 缺失或不是对象的条目以 `Missing tags` 记进 `errors`，不会整批拒绝。非法数组也只使本条目失败，不派发它的任何字段；其他条目照常处理。 |
| `items[].cueIndex` | `integer` | 否 | 这一条的子曲目序号，同 `metadata.write`。默认 `-1`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `successCount` | `integer` | 已派发写入或没有可写内容的条目数；每个已派发的写入以 `metadata:writeComplete` 报告结果。 |
| `failCount` | `integer` | 失败的条目数；成功时恒为 `0`，因为有条目失败时整次调用失败。 |
| `errors` | `MetadataWriteBatchError[]` | 失败的条目；成功时恒为空，因为有条目失败时整次调用失败。 |
| `errors[].path` | `string` | 该条目的路径。 |
| `errors[].error` | `string` | 失败原因：`Missing tags`，或 `metadata.write` 对该路径给出的错误。 |

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
把图片写进曲目的标签、写成音频旁的图片文件，或两者都写。单个目标时结果描述这次写入；两个目标时各目标的结果在 `results` 下，只有两者都失败才以 `OPERATION_FAILED` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 曲目路径。`embedded` 目标写入这个文件，文件格式须受专辑封面编辑器支持；`file` 目标写入它所在的目录（去掉 `\|subsong:N` 后缀），所以同一个 CUE 的全部曲目共用一张图。不能为空。 |
| `imageData` | `string` | 是 | 图片的裸 Base64，不带 Data URL 头或 `base64:` 标记。Base64 字母表以外的字符被跳过，遇到第一个 `=` 即停止解码；解不出任何字节时以 `INVALID_PARAMS` 失败。不能为空。 |
| `type` | `"front" \| "cover_front" \| "back" \| "cover_back" \| "disc" \| "icon" \| "artist"` | 否 | 图片类型。默认 `"front"`。 |
| `target` | `string[]` | 否 | 写到哪里：`embedded`（写进文件的标签）、`file`（音频旁的图片，按 `type` 命名，正面图为 `cover`，扩展名由图片字节判断），或 `all` 表示两处；给多项就各写一处。省略时写 `embedded`。其他值以 `INVALID_PARAMS` 失败。不能为空。 |
| `filename` | `string` | 否 | `file` 目标用的文件名，原样代替推导出来的名字（不补扩展名）；必须是没有分隔符与穿越序列的纯文件名。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `path` | `string` | 原样的路径。 |
| `type` | `string` | 请求的类型，原样。 |
| `size` | `integer` | 解码后的图片字节数；单个目标时出现。 |
| `savedTo` | `string` | 写出的图片文件的完整路径；单个 `file` 目标时出现。 |
| `results` | `Record<string, any>` | 两个目标都写时：每个目标自己的信封（`{ success, path, type, size }`，`file` 另带 `savedTo`；或 `{ success: false, error, code }`），键为 `embedded` 与 `file`。 |

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
删除文件里嵌入的图片：删一种类型，或在省略 `type`、设置 `removeAll` 时删除全部。专辑封面编辑器不支持的格式以 `NOT_SUPPORTED` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 曲目路径；文件格式须受专辑封面编辑器支持。不能为空。 |
| `type` | `"front" \| "cover_front" \| "back" \| "cover_back" \| "disc" \| "icon" \| "artist"` | 否 | 要删除的图片类型；省略时删除全部图片。 |
| `removeAll` | `boolean` | 否 | 不论 `type` 为何，删除全部图片。默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `path` | `string` | 原样的路径。 |
| `removedTypes` | `string[]` | 删除了什么：一次删掉全部图片时为 `["all"]`，否则是逐个删除的类型（`front`、`back`、`disc`、`icon`、`artist`），或原样的单个 `type`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### metadata.removeField

`metadata.removeTag` 的别名。移除指定标签。

<!-- api-schema:begin metadata.removeField -->
与 `metadata.removeTag` 相同，保留给沿用旧名的调用方；完成事件同样报 `removeTag`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 曲目路径；`\|subsong:N` 后缀或结尾的 `#N` 选子曲目。不能为空。 |
| `tags` | `string[]` | 是 | 要删除的标签名，会转成大写。空列表不删除任何标签，直接成功，不派发。 |
| `cueIndex` | `integer` | 否 | 子曲目序号，优先于 `path` 里的后缀；负数忽略。默认 `-1`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `path` | `string` | 原样的路径。 |
| `removedTags` | `string[]` | 排入删除的标签名，已转大写，顺序与给出的一致。 |
| `removedCount` | `integer` | `removedTags` 里的名字数。 |
| `dispatched` | `boolean` | 删除已派发时为 `true`；`tags` 为空时不出现。 |
| `subsong` | `integer` | 删除所针对的子曲目；派发时出现。 |
| `note` | `string` | 提醒最终结果由 `metadata:writeComplete` 事件报告；派发时出现。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

## 事件

### metadata:writeComplete

当 `metadata.write` 或 `metadata.removeTag` 的异步写入完成时广播此事件。

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| operation | string | `"write" "removeTag"` 触发操作 |
| path | string | 音频文件路径 |
| subsong | number | subsong 索引 |
| code | number | 完成码：0=成功, 1=中止, 2=错误 |
| success | boolean | 是否成功 |
| status | string | "success" / "aborted" / "error" |

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

- `metadata.read`、`metadata.readByPath` 和 `metadata.readRaw` 都需要 `path`，都接受默认值为 `-1` 的可选 `cueIndex`；`path|subsong:N` 或 `cueIndex` 可选择容器 subsong。`readRaw` 绕过 metadb 缓存，成功结果在 `metadata.read` 的结构化 `{ success, path, tags, info }` 之外再加 `source: "file"`。
- `metadata.write`、`metadata.removeTag` 和兼容端点 `metadata.removeField` 都会异步派发更新。派发成功不等于已经持久化完成：请监听广播事件 `metadata:writeComplete`，其 payload 为 `{ operation, path, subsong, code, success, status }`。
- `metadata.embedArtwork` 需要非空 `path` 和裸 Base64 `imageData`。`type` 默认 `front`，`cover_front`、`cover_back` 分别等同 `front`、`back`，其他取值以 `INVALID_PARAMS` 失败。`target` 是数组：省略时写 `embedded`，可取 `embedded`、`file`、`all`（两处）的任意组合；直接传字符串（如 `"file"`）以 `INVALID_PARAMS` 失败（SDK 的封装会把单个目标包成单元素数组）。两个目标都写时各自的结果在 `results` 下，任一目标成功即为成功，两者都失败时以 `OPERATION_FAILED` 失败，失败信封同样带 `results`。
- `metadata.removeEmbeddedArt` 删除一种 `type` 的图片；省略 `type` 或 `removeAll` 为 `true` 时删除全部封面。目标格式必须支持 `album_art_editor`。
