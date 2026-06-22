# Artwork API 

专辑封面获取。支持 `fb2k://` 协议 URL、Base64 dataUrl、批量获取等多种方式。共 13 个 API。

> 此 API 命名空间为 `artwork.*`，不支持别名。

## 封面获取方式对比 

| 方法 | 返回格式 | 适用场景 | 推荐度 |
| --- | --- | --- | --- |
| artwork.getFb2kUrl() | fb2k:// URL | 当前播放曲目 | ★★★★★ |
| artwork.getFb2kUrlByPath() | fb2k:// URL | 任意曲目懒加载 | ★★★★★ |
| artwork.getFb2kUrlByPathBatch() | fb2k:// URL 数组 | 批量封面 | ★★★★★ |
| artwork.getCurrent() | Base64 dataUrl | 需要立即显示 | ★★★ |
| artwork.getForTrack() | Base64 dataUrl | 单曲封面+缩放 | ★★★ |

### artwork.getFb2kUrl 

获取当前播放曲目的 `fb2k://artwork/...` URL。v1.1.7 新增

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| type | string | ✗ | 封面类型: front, back, disc, artist, icon |
| maxSize | number | ✗ | 图片最大边长（像素） |

```javascript
const result = await fb2k.invoke('artwork.getFb2kUrl', { maxSize: 300 });
if (result.available) document.getElementById('cover').src = result.dataUrl;
```

### artwork.getFb2kUrlByPath 

根据曲目路径生成 `fb2k://artwork/...` URL。v1.1.7 新增

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| path | string | ✓ | 曲目文件路径 |
| type | string | ✗ | 封面类型 |
| maxSize | number | ✗ | 最大边长 |

```javascript
// 推荐：通过 API 获取 URL（最安全）
const result = await fb2k.invoke('artwork.getFb2kUrlByPath', {
    path: trackPath, type: 'front', maxSize: 300
});
img.src = result.dataUrl;

// 或手动拼接（使用 query param 格式，避免 Chromium 路径规范化问题）
function getCoverUrl(trackPath, maxSize = 300) {
    return `fb2k://artwork/?path=${encodeURIComponent(trackPath)}&type=front&maxSize=${maxSize}`;
}
```

### artwork.getFb2kUrlByPathBatch 

批量返回多个曲目的 `fb2k://` URL。纯字符串拼接，无 SDK 访问，性能极佳。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| paths | string[] | ✓ | 曲目路径数组 |
| type | string | ✗ | 封面类型 |
| maxSize | number | ✗ | 最大边长 |

```javascript
// 100 张封面，单次 IPC 往返 (~2ms)
const tracks = await fb2k.invoke('playlist.getTracks', { count: 100 });
const result = await fb2k.invoke('artwork.getFb2kUrlByPathBatch', {
    paths: tracks.tracks.map(t => t.absolutePath),
    type: 'front', maxSize: 300
});
```

### fb2k:// URL 格式参考 

`fb2k://artwork/?path={encodedPath}&type={type}&maxSize={size}`

::: tip 新格式（v1.4.0+）
路径放在 query string 的 `path` 参数中，避免 Chromium URL 规范化解码 `%5C`/`%2F` 导致 Windows 路径损坏。 旧格式 `fb2k://artwork/{encodedPath}?type=...` 仍向后兼容。
:::

| 用途 | maxSize | 预计大小 |
| --- | --- | --- |
| 播放列表缩略图 | 50-100 | 2-5 KB |
| 专辑网格 | 200-300 | 10-20 KB |
| 当前播放封面 | 400-600 | 30-60 KB |
| 高清大图 | 1000+ | 100-500 KB |

## Base64 封面 

### artwork.getCurrent 

获取当前播放曲目的封面图片。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| type | string | ✗ | front, back, disc, artist, icon |

**返回值**（成功）:

```json
{
    "available": true,
    "type": "front",
    "source": "now_playing_manager",
    "mimeType": "image/jpeg",
    "size": 123456,
    "dataUrl": "data:image/jpeg;base64,..."
}
```

**返回值**（失败）: `{ "available": false, "reason": "no_track" }`

| source 值 | 说明 |
| --- | --- |
| now_playing_manager | 最快，缓存 |
| album_art_manager_v2 | 更全面 |
| extractor | 直接提取 |

### artwork.getByPath 

通过文件路径获取封面。直接使用 `album_art_extractor` 提取器。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| path | string | ✓ | 音频文件路径 |
| type | string | ✗ | 图片类型 |

::: tip 路径 Contract (Phase 9)

- **支持**: 原生路径、`file://` 前缀、`path|subsong:N`（subsong 会被剥离，提取器以文件级别操作）
- **拒绝**: `file-relative://` 路径会返回显式错误，请改用 `artwork.getByPlaylistItem`
- 路径会在内部自动规范化（canonical path）

:::

### artwork.getForTrack 

获取指定曲目的封面。支持嵌入式封面和外部封面文件。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| path | string | ✓ | 曲目文件路径 |
| type | string | ✗ | 图片类型 |

**返回值**:

```json
{
    "available": true,
    "type": "front",
    "mimeType": "image/jpeg",
    "width": 200,
    "height": 200,
    "size": 15234,
    "dataUrl": "data:image/jpeg;base64,..."
}
```

::: tip 性能建议
列表/网格视图中使用 `artwork.getCurrent` 或 `artwork.getByPlaylistItem` 的 `maxSize: 200` 可将数据传输量从 ~2MB 减少到 ~15KB。`artwork.getForTrack` 不支持 `maxSize` 参数。
:::

::: warning width / height 限制
`width` 和 `height` 仅对 **PNG** 格式封面返回实际尺寸。JPEG、GIF、BMP、WebP 格式返回值为 `0`。
:::

::: warning WARNING
如果曲目路径是 `file-relative://` 格式，请使用 `artwork.getByPlaylistItem`。
:::

### artwork.getByPlaylistItem 

通过播放列表索引获取封面。推荐用于播放列表中的曲目，能正确处理相对路径。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| playlist | number | ✗ | 播放列表索引 |
| index | number | ✗ | 曲目索引（默认 0） |
| type | string | ✗ | 图片类型 |

### artwork.getAvailableTypes 

获取曲目可用的封面类型列表。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| path | string | ✗ | 音频文件路径（省略则使用当前播放曲目） |

**返回值**: `{ "types": ["front", "back", "disc"] }`

### artwork.getAvailableArtwork 

获取曲目所有可用的封面类型及来源。同时检查嵌入式封面和外部封面文件（cover.jpg/folder.jpg 等）。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| path | string | ✓ | 音频文件路径 |

**返回值**:

```json
{
    "success": true,
    "available": true,
    "artworks": [
        { "type": "front", "source": "embedded" }
    ],
    "sources": ["embedded", "folder:cover.jpg"]
}
```

### artwork.getFolderImages 

获取文件夹中所有图片文件。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| directory | string | ✓ | 文件夹路径 |

支持的图片格式: `.jpg`, `.jpeg`, `.png`, `.gif`, `.bmp`, `.webp`

**返回值**:

```json
{
    "success": true,
    "images": [
        { "name": "cover.jpg", "path": "C:\\\\Music\\\\Album\\\\cover.jpg", "size": 123456 }
    ]
}
```

### artwork.getLyrics 

获取曲目的歌词（从元数据标签中读取）。显式传入 path 时会自动规范化路径。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| path | string | ✗ | 音频文件路径（省略则使用当前播放曲目） |

**返回值**（找到时）:

```json
{
    "available": true,
    "tag": "LYRICS",
    "lyrics": "[00:12.34]First line...",
    "synced": false
}
```

**返回值**（未找到）: `{ "available": false }`

支持的歌词标签: `LYRICS`, `UNSYNCED LYRICS`, `UNSYNCEDLYRICS`, `SYNCEDLYRICS`, `SYNCED LYRICS`

### artwork.getMetadata 

获取曲目的基本元数据信息。显式传入 path 时会自动规范化路径。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| path | string | ✗ | 音频文件路径（省略则使用当前播放曲目） |

**返回值**:

```json
{
    "title": "Let It Be",
    "artist": "The Beatles",
    "album": "Let It Be",
    "albumArtist": "The Beatles",
    "year": "1970",
    "genre": "Rock",
    "trackNumber": "6",
    "discNumber": "1",
    "hasEmbedded": true,
    "hasLyrics": true
}
```

### artwork.getBatch（DEPRECATED）

::: danger 已废弃
请迁移到 `artwork.getFb2kUrlByPathBatch`。此 API 仍返回 Base64 dataUrl，性能较差。
:::

```javascript
// ❌ 旧方式
const result = await fb2k.invoke('artwork.getBatch', { paths });

// ✅ 新方式
const result = await fb2k.invoke('artwork.getFb2kUrlByPathBatch', { paths, type: 'front' });
```
