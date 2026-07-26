# Metadata API

元数据读写、封面嵌入、批量操作、评分。共 10 个 API（含别名 `metadata.removeField` → `metadata.removeTag` 共享同一 handler）。

## 定位容器内的单曲 {#subsong-addressing}

CUE、ISO 镜像与多轨文件都是一个文件路径下含多首曲目。所有读取方法用同一种方式定位其中一首：

- 在路径后追加 `|subsong:N`，例如 `D:\album.cue|subsong:2`；
- 或在普通路径之外另传 `cueIndex: N`。两者同时给出时以 `cueIndex` 为准。

编号沿用 foobar2000 的规则，**CUE 从 1 开始**：`|subsong:1` 是第一首。不带后缀的容器路径（或 `|subsong:0`）指向 subsong 0，而 CUE 中并不存在该编号，因此会返回 `Failed to get track info` —— 这是宿主的编号方式，不是请求写错了。

读取普通单轨文件无需后缀；`|subsong:0` 与不写等价。

## 读取

### metadata.read

读取指定文件的元数据（结构化格式）。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 必填。支持 `路径|subsong:N`。 |
| `cueIndex` | `integer` | 否 | 可选；默认 -1。显式指定容器内曲目序号，优先级高于路径后缀。 |

> 读取链路会先剥离 `|subsong:N` 后缀并 canonicalize 路径，再按解析出的 subsong 通过 `handle_create()` 读取 cached info；若 cached info 缺少关键元数据，则以同一 subsong 退回 direct file read。CUE / ISO 等多轨容器的寻址规则见[定位容器内的单曲](#subsong-addressing)。

**返回值**: `{"error":"...","info":"...","path":"...","success":true,"tags":"..."}`


> `tags` 保留文件内原始字段名，不强制转为大写；如果你需要统一的大写扁平字段，请使用 `metadata.readByPath`。

### metadata.readRaw

直接从文件读取元数据，绕过 metadb 缓存。v1.4.1 新增。

与 `metadata.read` 返回格式一致，但始终从磁盘文件直接解码读取，不走 foobar2000 metadb 内存缓存。适用于需要获取最新文件标签的场景（如刚写入标签后立即读回验证）。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `cueIndex` | `integer` | 否 | `-1` | 可选；默认 -1。 |
| `path` | `string` | 否 | `` | 可选；默认 。 |

**返回值**: `{"error":"...","info":"...","path":"...","source":"...","success":true,"tags":"..."}`


> 始终直接打开文件解码器读取，不受 metadb 缓存影响。`source` 字段固定为 `"file"`。

```javascript
const raw = await fb2k.invoke('metadata.readRaw', { path: 'E:\\\\Music\\\\song.flac' });
console.log(raw.tags.TITLE, raw.source); // "file"
```

### metadata.readByPath

读取元数据（扁平格式）。v1.1.0 新增

与 `metadata.read` 不同，此 API 返回扁平结构，所有标签键名转为大写；但两者共享同一条 fallback 读取链路。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 必填。支持 `路径|subsong:N`。 |
| `cueIndex` | `integer` | 否 | 可选；默认 -1。显式指定容器内曲目序号，优先级高于路径后缀。 |

**返回值**: `{"TRACKNUMBER":"...","canonicalPath":"...","error":"...","path":"...","success":true}`

> 若文件缺少 `TRACKNUMBER` 标签，会尝试从文件名提取。若 cached info 不完整，会自动退回 direct file read。多轨容器寻址见[定位容器内的单曲](#subsong-addressing)。

```javascript
const meta = await fb2k.invoke('metadata.readByPath', { path: 'E:\\\\Music\\\\song.flac' });
console.log(meta.TITLE, meta.ARTIST, meta.DURATION);
```

### metadata.readBatch

批量读取多个文件的元数据。v1.1.11 新增。包含所有标签和技术信息（大写键名）。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `array` | 是 | 必填。每个元素都可带 `|subsong:N` 后缀，逐项独立解析。 |

> 本方法没有 `cueIndex` 参数：批量调用中每个路径只能通过 `|subsong:N` 指定曲目。见[定位容器内的单曲](#subsong-addressing)。

**返回值**:

```json
{
    "success": true,
    "total": 3,
    "successCount": 3,
    "errorCount": 0,
    "results": [
        { "path": "...", "success": true, "tags": { "TITLE": "...", "ARTIST": "..." } }
    ]
}
```

```javascript
const batch = await fb2k.invoke('metadata.readBatch', {
    paths: ['E:\\\\Music\\\\a.flac', 'E:\\\\Music\\\\b.flac']
});
batch.results.forEach(r => {
    if (r.success) console.log(r.tags.TITLE);
});
```

## 写入

### metadata.write

写入元数据标签到文件。使用 `metadb_io_v2::update_info_async` 异步写入。标签键名自动转为大写。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `cueIndex` | `integer` | 否 | 可选；默认 -1。 |
| `path` | `string` | 是 | 必填。 |
| `tags` | `object` | 是 | 必填。 |

**返回值**: `{"canonicalPath":"...","dispatched":"...","error":"...","handlePath":"...","note":"...","path":"...","subsong":"...","success":true,"tagsApplied":"...","tagsRemoved":"...","tagsSet":"..."}`


::: tip 异步派发模式
`metadata.write` 立即返回 `dispatched: true`，表示写入已提交给 foobar2000 引擎。实际完成后会广播 `metadata:writeComplete` 事件（见下文）。
:::

```javascript
await fb2k.invoke('metadata.write', {
    path: 'E:\\\\Music\\\\song.flac',
    tags: { TITLE: 'New Title', ARTIST: 'New Artist', COMMENT: null }
});

// CUE 子轨写入
await fb2k.invoke('metadata.write', {
    path: 'E:\\\\Music\\\\album.flac|subsong:2',
    tags: { COMMENT: 'Track 3 comment' }
});
```

### metadata.removeTag

移除指定标签。标签名自动转为大写。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `cueIndex` | `integer` | 否 | 可选；默认 -1。 |
| `path` | `string` | 是 | 必填。 |
| `tags` | `array` | 是 | 必填。 |

**返回值**: `{"dispatched":"...","error":"...","note":"...","path":"...","removedCount":"...","removedTags":"...","subsong":"...","success":true}`

```javascript
await fb2k.invoke('metadata.removeTag', {
    path: 'E:\\\\Music\\\\song.flac',
    tags: ['COMMENT', 'LYRICS']
});

// CUE 子轨移除
await fb2k.invoke('metadata.removeTag', {
    path: 'E:\\\\Music\\\\album.flac|subsong:2',
    tags: ['COMMENT']
});
```

### metadata.writeBatch

批量写入多个文件的元数据。逐个调用 `metadata.write`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `items` | `array` | 是 | 必填。 |

**返回值**: `{ "success": true, "successCount": 3, "failCount": 0, "errors": [] }`

```javascript
await fb2k.invoke('metadata.writeBatch', {
    items: [
        { path: 'E:\\\\Music\\\\a.flac', tags: { GENRE: 'Rock' } },
        { path: 'E:\\\\Music\\\\b.flac', tags: { GENRE: 'Pop' } }
    ]
});
```

### metadata.embedArtwork

将封面图嵌入到音频文件。使用 foobar2000 SDK 的 `album_art_editor`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `filename` | `string` | 否 | 可选；默认 。 |
| `imageData` | `string` | 否 | 可选；默认 。 |
| `path` | `string` | 是 | 必填。 |
| `target` | `array` | 否 | 可选；默认 embedded。 |
| `type` | `string` | 否 | 可选；默认 front。 |

**支持的封面类型**: `front`/`cover_front`, `back`/`cover_back`, `disc`, `icon`, `artist`

**返回值**: `{"path":"...","results":[],"success":true,"type":"..."}`

```javascript
// 将 Base64 图片嵌入为封面
await fb2k.invoke('metadata.embedArtwork', {
    path: 'E:\\\\Music\\\\song.flac',
    imageData: base64String,
    type: 'front'
});
```

`imageData` 必须是**裸 Base64 图片字节**。不要传
`data:image/...;base64,` 头、`base64:` 标记或 `fb2k://` URL。若来源是
Artwork API 返回的标准 Data URL，应先取第一个逗号之后的 payload：

```javascript
const cover = await fb2k.invoke('artwork.getCurrent', { type: 'front' });
const payload = cover.dataUrl.slice(cover.dataUrl.indexOf(',') + 1);
await fb2k.invoke('metadata.embedArtwork', {
    path: 'E:\\\\Music\\\\song.flac',
    imageData: payload,
    type: 'front',
});
```

### metadata.removeEmbeddedArt

移除音频文件中的嵌入封面。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 必填。 |
| `removeAll` | `boolean` | 否 | 可选；默认 false。 |
| `type` | `string` | 否 | 可选；默认 。 |

**返回值**: `{"error":"...","path":"...","removedTypes":"...","success":true}`

### metadata.removeField

`metadata.removeTag` 的别名。移除指定标签。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `cueIndex` | `integer` | 否 | 可选；默认 -1。 |
| `path` | `string` | 是 | 必填。 |
| `tags` | `array` | 是 | 必填。 |


**返回值**: `{"dispatched":"...","error":"...","note":"...","path":"...","removedCount":"...","removedTags":"...","subsong":"...","success":true}`

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

## 评分

### rating.get

获取曲目评分。优先从 foo_playcount 读取，回退到文件 RATING 标签。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `cueIndex` | `integer` | 否 | 可选；默认 -1。 |
| `path` | `string` | 是 | 必填。 |

**返回值**:

```json
{
    "success": true,
    "path": "C:\\\\Music\\\\song.flac",
    "rating": 5,
    "storage": "stats"
}
```

| storage 值 | 含义 |
| --- | --- |
| "stats" | 来自 foo_playcount |
| "file" | 来自文件 RATING 标签 |

### rating.set

设置曲目评分。优先通过 foo_playcount 上下文菜单设置，回退到写入文件 RATING 标签。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `cueIndex` | `integer` | 否 | 可选；默认 -1。 |
| `path` | `string` | 是 | 必填。 |
| `rating` | `integer` | 否 | 可选；默认 -1。 |


**返回值**: `{"menuPath":"...","note":"...","path":"...","rating":"...","storage":"...","success":true}`

```javascript
await fb2k.invoke('rating.set', { path: 'C:\\\\Music\\\\song.flac', rating: 5 });
await fb2k.invoke('rating.set', { rating: 0 }); // 清除当前播放曲目评分
```

## Contract 说明

- `metadata.read`、`metadata.readByPath` 和 `metadata.readRaw` 都需要 `path`。`readRaw` 绕过 metadb 缓存，接受默认值为 `-1` 的 `cueIndex`；`path|subsong:N` 可选择容器 subsong。成功结果会在结构化 `{ success, path, tags, info }` 中添加 `source: "file"`。
- `metadata.write`、`metadata.removeTag` 和兼容端点 `metadata.removeField` 都会异步派发更新。派发成功不等于已经持久化完成：请监听广播事件 `metadata:writeComplete`，其 payload 为 `{ operation, path, subsong, code, success, status }`。
- `metadata.embedArtwork` 需要非空 `path` 和 Base64 `imageData`。`type` 默认 `front`，`filename` 默认空字符串，`target` 默认 `embedded`，可取 `embedded`、`file`、`all` 或由 `embedded` 与 `file` 组成的数组。多个 target 时，`{ success, path, type, results }` 在任一 target 成功时即为成功。
- `metadata.removeEmbeddedArt` 接受 `removeAll` 和可选的 `type`；空 `type` 同样表示删除全部封面。目标格式必须支持 `album_art_editor` 工作流。
- `rating.set` 只接受 `0` 到 `5`，`0` 表示清除评分。存在匹配的 foo_playcount 上下文菜单时优先使用它，否则写入文件 `RATING` 标签。`rating.get` 通过 `storage` 返回 `stats` 或 `file`。
