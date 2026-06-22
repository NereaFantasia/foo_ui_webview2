# Metadata API 

元数据读写、封面嵌入、批量操作、评分。共 10 个 API（含别名 `metadata.removeField` → `metadata.removeTag` 共享同一 handler）。

## 读取 

### metadata.read 

读取指定文件的元数据（结构化格式）。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| path | string | ✓ | 音频文件路径 |

> 读取链路会先 canonicalize 路径，再通过 `handle_create()` 读取 cached info；若 cached info 缺少关键元数据，则自动退回 direct file read。

**返回值**:

```json
{
    "success": true,
    "path": "E:\\\\Music\\\\song.flac",
    "tags": {
        "TITLE": "Song Title",
        "ARTIST": "Artist Name",
        "ALBUM": "Album Name"
    },
    "info": {
        "duration": 180.5,
        "bitrate": 1411,
        "sampleRate": 44100,
        "channels": 2,
        "codec": "FLAC"
    }
}
```

> `tags` 保留文件内原始字段名，不强制转为大写；如果你需要统一的大写扁平字段，请使用 `metadata.readByPath`。

直接从文件读取元数据，绕过 metadb 缓存。v1.4.1 新增。

与 `metadata.read` 返回格式一致，但始终从磁盘文件直接解码读取，不走 foobar2000 metadb 内存缓存。适用于需要获取最新文件标签的场景（如刚写入标签后立即读回验证）。

| 参数 | 类型 | 必填 | 默认值 | 描述 |
| --- | --- | --- | --- | --- |
| path | string | ✓ |  | 音频文件路径 |
| cueIndex | number |  | -1 | CUE subsong 索引（也支持 \|subsong:N / #N 路径格式） |

**返回值**:

```json
{
    "success": true,
    "path": "E:\\\\Music\\\\song.flac",
    "tags": {
        "TITLE": "Song Title",
        "ARTIST": ["Artist1", "Artist2"]
    },
    "info": {
        "duration": 180.5,
        "bitrate": 1024,
        "sampleRate": 44100,
        "channels": 2,
        "codec": "FLAC"
    },
    "source": "file"
}
```

> 始终直接打开文件解码器读取，不受 metadb 缓存影响。`source` 字段固定为 `"file"`。

```javascript
const raw = await fb2k.invoke('metadata.readRaw', { path: 'E:\\\\Music\\\\song.flac' });
console.log(raw.tags.TITLE, raw.source); // "file"
```

### metadata.readByPath 

读取元数据（扁平格式）。v1.1.0 新增

与 `metadata.read` 不同，此 API 返回扁平结构，所有标签键名转为大写；但两者共享同一条 fallback 读取链路。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| path | string | ✓ | 音频文件路径 |

**返回值**: 所有标签和技术信息为大写键名的扁平对象，含 `success`、`path`、`TITLE`、`ARTIST`、`DURATION`、`FILESIZE` 等。

> 若文件缺少 `TRACKNUMBER` 标签，会尝试从文件名提取。若 cached info 不完整，会自动退回 direct file read。

```javascript
const meta = await fb2k.invoke('metadata.readByPath', { path: 'E:\\\\Music\\\\song.flac' });
console.log(meta.TITLE, meta.ARTIST, meta.DURATION);
```

批量读取多个文件的元数据。v1.1.11 新增。包含所有标签和技术信息（大写键名）。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| paths | string[] | ✓ | 音频文件路径数组 |

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

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| path | string | ✓ | 音频文件路径（支持 \|subsong:N 格式指定子轨） |
| tags | object | ✓ | 标签键值对，值为 null 或空字符串则删除该标签 |
| cueIndex | number | ✗ | CUE subsong 索引（优先级高于 \|subsong:N） |

**返回值**:

```json
{
    "success": true,
    "dispatched": true,
    "path": "E:\\\\Music\\\\song.flac|subsong:2",
    "handlePath": "file://E:\\\\Music\\\\song.flac",
    "subsong": 2,
    "tagsApplied": { "TITLE": "New Title" },
    "tagsSet": 1,
    "tagsRemoved": 0
}
```

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

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| path | string | ✓ | 音频文件路径（支持 \|subsong:N 格式指定子轨） |
| tags | string[] | ✓ | 要移除的标签名数组 |
| cueIndex | number | ✗ | CUE subsong 索引（优先级高于 \|subsong:N） |

**返回值**: `{ "success": true, "dispatched": true, "path": "...", "subsong": 2, "removedTags": ["COMMENT"], "removedCount": 1 }`

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

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| items | array | ✓ | 包含 path 和 tags 的对象数组 |

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

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| path | string | ✓ | 音频文件路径 |
| imageData | string | ✓ | Base64 编码的图片数据 |
| type | string | ✗ | 封面类型，默认 "front" |

**支持的封面类型**: `front`/`cover_front`, `back`/`cover_back`, `disc`, `icon`, `artist`

**返回值**: `{ "success": true, "path": "...", "type": "front", "size": 25600 }`

```javascript
// 将 Base64 图片嵌入为封面
await fb2k.invoke('metadata.embedArtwork', {
    path: 'E:\\\\Music\\\\song.flac',
    imageData: base64String,
    type: 'front'
});
```

### metadata.removeEmbeddedArt 

移除音频文件中的嵌入封面。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| path | string | ✓ | 音频文件路径 |
| type | string | ✗ | 封面类型（空 = 删除全部） |
| removeAll | boolean | ✗ | 是否删除所有封面 |

**返回值**: `{ "success": true, "path": "...", "removedTypes": ["front", "back"] }`

### metadata.removeField 

`metadata.removeTag` 的别名。移除指定标签。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| path | string | ✓ | 音频文件路径（支持 \|subsong:N） |
| tags | string[] | ✓ | 要移除的标签名数组 |

## 事件 

### metadata:writeComplete 

当 `metadata.write` 或 `metadata.removeTag` 的异步写入完成时广播此事件。

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| operation | `"write" \| "removeTag"` | 触发操作 |
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

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| path | string | ✓ | 文件路径（支持 \|subsong:N 格式） |
| cueIndex | number | ✗ | CUE subsong 索引（优先级最高） |

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

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| path | string | ✗ | 文件路径（空 = 当前播放/选中） |
| rating | number | ✓ | 评分 0-5（0 = 清除评分） |
| cueIndex | number | ✗ | CUE subsong 索引 |

```javascript
await fb2k.invoke('rating.set', { path: 'C:\\\\Music\\\\song.flac', rating: 5 });
await fb2k.invoke('rating.set', { rating: 0 }); // 清除当前播放曲目评分
```
