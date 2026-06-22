# Metadata 工具 

元数据读写与评分管理。共 12 个工具。

## 读取 

### fb2k_metadata_read 

读取文件元数据（结构化格式：tags + info 分离）。

- **Bridge 方法**: `metadata.read`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| path | string | ? | 文件路径 |

**返回值**:

```json
{
  "success": true,
  "path": "D:\\\\Music\\\\track.flac",
  "tags": {
    "TITLE": "天ノ弱",
    "ARTIST": "164 feat. GUMI",
    "ALBUM": "天ノ弱",
    "GENRE": ["Vocaloid", "J-Pop"]
  },
  "info": {
    "duration": 263.5,
    "bitrate": 876,
    "sampleRate": 44100,
    "channels": 2,
    "codec": "FLAC"
  }
}
```

::: tip 多值标签
当一个标签有多个值时（如多个艺术家），返回字符串数组而非单个字符串。
:::

### fb2k_metadata_read_by_path 

按路径读取元数据（扁平格式）。

- **Bridge 方法**: `metadata.readByPath`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| path | string | ? | 文件路径 |

### fb2k_metadata_read_raw 

直接从文件读取原始元数据，绕过 metadb 缓存。

- **Bridge 方法**: `metadata.readRaw`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| path | string | ? | 文件路径，支持 \|subsong:N 或 #N 格式 |
| cueIndex | integer | ? | CUE 子曲目索引（覆盖路径中的 subsong 标记） |

::: tip 与 `metadata.read` 的区别
`readRaw` 确保从文件直接读取最新数据，而 `read` 可能返回缓存的元数据。当写入后需要立即验证时使用此方法。
:::

### fb2k_metadata_read_batch 

批量读取多个文件的元数据。

- **Bridge 方法**: `metadata.readBatch`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| paths | array | ? | 文件路径数组 |

## 写入 

### fb2k_metadata_write 

写入元数据标签。值为 `null` 或空字符串时删除标签。

- **Bridge 方法**: `metadata.write`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| path | string | ? | 文件路径 |
| tags | object | ? | 键值对，如 {"TITLE": "新标题", "ARTIST": "新艺术家"} |

::: warning WARNING
写入操作会直接修改文件。请确保文件可写且已备份。
:::

### fb2k_metadata_write_batch 

批量写入元数据。

- **Bridge 方法**: `metadata.writeBatch`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| items | array | ? | [{ path, tags }] 格式的数组 |

## 封面嵌入 

### fb2k_metadata_embed_artwork 

将封面图片嵌入到文件中。

- **Bridge 方法**: `metadata.embedArtwork`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| path | string | ? | 文件路径 |
| imageData | string | ? | base64 编码的图片数据 |
| type | string | ? | 封面类型（front / back / disc / icon / artist，默认 front） |

### fb2k_metadata_remove_embedded_art 

移除嵌入的封面图片。

- **Bridge 方法**: `metadata.removeEmbeddedArt`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| path | string | ? | 文件路径 |
| type | string | ? | 要移除的封面类型（省略时需配合 removeAll） |
| removeAll | boolean | ? | 是否移除所有类型的封面（默认 false） |

## 标签删除 

### fb2k_metadata_remove_tag 

移除指定标签。

- **Bridge 方法**: `metadata.removeTag`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| path | string | ? | 文件路径 |
| tags | array | ? | 要移除的标签名数组 |

### fb2k_metadata_remove_field 

移除指定字段。

- **Bridge 方法**: `metadata.removeField`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| path | string | ? | 文件路径 |
| tags | array | ? | 要移除的字段名数组 |

## 评分 

### fb2k_rating_set 

设置曲目评分。优先使用 foo_playcount 的上下文菜单，回退到写入 RATING 标签。

- **Bridge 方法**: `rating.set`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| rating | integer | ? | 评分值（0=清除, 1–5） |
| path | string | ? | 文件路径（省略时使用当前播放或选中的曲目） |
| cueIndex | integer | ? | CUE 子曲目索引 |

### fb2k_rating_get 

获取曲目评分。优先读取 foo_playcount 统计数据，回退到 RATING 标签。

- **Bridge 方法**: `rating.get`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| path | string | ? | 文件路径 |
| cueIndex | integer | ? | CUE 子曲目索引 |

**返回值**:

```json
{
  "success": true,
  "path": "D:\\\\Music\\\\track.flac",
  "rating": 4,
  "storage": "stats"
}
```

| 字段 | 类型 | 描述 |
| --- | --- | --- |
| rating | integer | 评分值（0=未评分, 1–5） |
| storage | string | 数据来源（"stats" = foo_playcount, "file" = RATING 标签） |
