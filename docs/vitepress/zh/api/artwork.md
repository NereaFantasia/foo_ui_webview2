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

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `maxSize` | `integer` | 否 | 可选；默认 0。 |
| `type` | `string` | 否 | 可选；默认 front。 |


**返回值**: `{"available":true,"dataUrl":"...","error":"...","reason":"...","type":"..."}`

```javascript
const result = await fb2k.invoke('artwork.getFb2kUrl', { maxSize: 300 });
if (result.available) document.getElementById('cover').src = result.dataUrl;
```

### artwork.getFb2kUrlByPath

根据曲目路径生成 `fb2k://artwork/...` URL。v1.1.7 新增

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `maxSize` | `integer` | 否 | 可选；默认 0。 |
| `path` | `string` | 否 | 可选；默认 。 |
| `type` | `string` | 否 | 可选；默认 front。 |


**返回值**: `{"available":true,"dataUrl":"...","error":"...","path":"...","type":"..."}`

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

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `items` | `array` | 否 | 可选；默认 omitted。 |
| `maxSize` | `integer` | 否 | 可选；默认 0。 |
| `paths` | `array` | 否 | 可选；默认 omitted。 |
| `type` | `string` | 否 | 可选；默认 front。 |


**返回值**: `{"artworks":"...","success":true}`

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

`getFb2kUrl*` 虽然把结果放在 `dataUrl` 字段中，但该值不是标准 Data URL，也不是图片字节。它只由本组件 WebView2 的资源拦截器解析，适合当前 WebView 中即时赋给 `<img src>`；不要把它持久化、传给 `file.write`，也不要当作系统级 URL 使用。

需要真实图片内容时，应使用 `getCurrent`、`getByPath`、`getForTrack`、`getByPlaylistItem` 或 `getBatch`，它们返回标准 `data:image/...;base64,...`。落盘时取逗号后的裸 Base64 payload，再以 `content: 'base64:' + payload` 和 `encoding: 'binary'` 调用 `file.write`；传给 `metadata.embedArtwork` 时则只传裸 payload，不要带 Data URL 头或 `base64:`。

::: tip 新格式（v1.4.0+）
路径放在 query string 的 `path` 参数中，避免 Chromium URL 规范化解码 `%5C`/`%2F` 导致 Windows 路径损坏。 旧格式 `fb2k://artwork/{encodedPath}?type=...` 仍向后兼容。
:::

```javascript
const artwork = await fb2k.invoke('artwork.getCurrent', { type: 'front' });
if (artwork.available) {
    const comma = artwork.dataUrl.indexOf(',');
    const payload = artwork.dataUrl.slice(comma + 1);
    await fb2k.invoke('file.write', {
        path: '%profile%\\\\cover.jpg',
        content: `base64:${payload}`,
        encoding: 'binary',
    });
}
```

| 用途 | maxSize | 预计大小 |
| --- | --- | --- |
| 播放列表缩略图 | 50-100 | 2-5 KB |
| 专辑网格 | 200-300 | 10-20 KB |
| 当前播放封面 | 400-600 | 30-60 KB |
| 高清大图 | 1000+ | 100-500 KB |

## Base64 封面

### artwork.getCurrent

获取当前播放曲目的封面图片。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `type` | `string` | 否 | 可选；默认 front。 |

**返回值**: `{"available":true,"dataUrl":"...","error":"...","mimeType":"...","path":"...","reason":"...","size":0,"source":"...","type":"..."}`



| source 值 | 说明 |
| --- | --- |
| `now_playing_manager` | 当前 front 封面的缓存数据。 |
| `album_art_manager_v2` | 由 album-art manager fallback 解析的封面。 |
| `extractor` | 由文件 extractor fallback 直接解析的封面。 |

### artwork.getByPath

通过文件路径获取封面。直接使用 `album_art_extractor` 提取器。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 否 | 可选；默认 。 |
| `type` | `string` | 否 | 可选；默认 front。 |


**返回值**: `{"available":true,"dataUrl":"...","error":"...","mimeType":"...","path":"...","size":0,"type":"..."}`

::: tip 路径 Contract

- **支持**: 原生路径、`file://` 前缀、`path|subsong:N`（subsong 会被剥离，提取器以文件级别操作）
- **拒绝**: `file-relative://` 路径会返回显式错误，请改用 `artwork.getByPlaylistItem`
- 路径会在内部自动规范化（canonical path）

:::

### artwork.getForTrack

获取指定曲目的封面。支持嵌入式封面和外部封面文件。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 否 | 可选；默认 。 |
| `type` | `string` | 否 | 可选；默认 front。 |

**返回值**: `{"available":true,"dataUrl":"...","error":"...","height":0,"mimeType":"...","path":"...","size":0,"type":"...","width":0}`


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

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `index` | `integer` | 否 | 可选；默认 -1。 |
| `playlist` | `integer` | 否 | 可选；默认 -1。 |
| `type` | `string` | 否 | 可选；默认 front。 |


**返回值**: `{"available":true,"dataUrl":"...","error":"...","index":0,"mimeType":"...","playlist":0,"size":0,"type":"..."}`

### artwork.getAvailableTypes

获取曲目可用的封面类型列表。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 否 | 可选；默认 。 |

**返回值**: `{"error":"...","success":true,"types":"..."}`

### artwork.getAvailableArtwork

获取曲目所有可用的封面类型及来源。同时检查嵌入式封面和外部封面文件（cover.jpg/folder.jpg 等）。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 否 | 可选；默认 。 |

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

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `directory` | `string` | 否 | 可选；默认 。 |

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

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 否 | 可选；默认 。 |

**返回值**: `{"available":true,"error":"...","lyrics":"...","synced":"...","tag":"..."}`



支持的歌词标签: `LYRICS`, `UNSYNCED LYRICS`, `UNSYNCEDLYRICS`, `SYNCEDLYRICS`, `SYNCED LYRICS`

### artwork.getMetadata

获取曲目的基本元数据信息。显式传入 path 时会自动规范化路径。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 否 | 可选；默认 。 |

**返回值**: `{"album":"...","albumArtist":"...","artist":"...","available":true,"discNumber":"...","error":"...","genre":"...","hasEmbedded":true,"hasLyrics":true,"title":"...","trackNumber":"...","year":"..."}`


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

## 补充的公开 API

以下章节按 `RegisterApi` 动态补齐，参数键来自 C++ handler 静态提取。

### artwork.getBatch

公开 API。运行时权威：`src/api/ArtworkApi.cpp:1370`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `array` | 是 | 必填。 |
| `type` | `string` | 否 | 可选；默认 front。 |

**返回值**: `{"artworks":"...","error":"...","success":true}`

```js
const result = await fb2k.invoke('artwork.getBatch', { paths: /* value */, type: /* value */ });
```

## Contract 说明

- 有效的 artwork `type` 为 `front`（也可用 `cover_front`）、`back`（也可用 `cover_back`）、`disc`、`icon` 和 `artist`。省略 `type` 时使用 `front`；未知值会返回 `INVALID_PARAMS`。
- `artwork.getByPath` 和 `artwork.getForTrack` 接受原生路径、`file://` 路径和 `path|subsong:N`。它们拒绝 `file-relative://`，因为 extractor 没有播放列表上下文；此类条目请用 `artwork.getByPlaylistItem`。
- 直接读取封面会返回 `data:image/...` URL。`artwork.getFb2kUrl` 及其路径变体则在 `dataUrl` 字段返回 `fb2k://artwork/` URL。仅当 `maxSize` 大于 `0` 时才应用缩放。
- `artwork.getFb2kUrlByPathBatch` 必须提供一个名为 `paths` 或 `items` 的数组。数组条目可以是字符串，或含有 `path` 成员的对象；它没有顶层 `path` 参数。返回 `{ success, artworks }`，每个输入条目对应一个 `available` 或 `error` 结果。
- `artwork.getAvailableArtwork` 会报告嵌入项和 `folder:cover.jpg` 等外部来源标签。它通过 `album_art_extractor` 打开文件；未找到封面以 `available: false` 表示，不一定是错误。
- `artwork.getFolderImages` 读取目录中的 `.jpg`、`.jpeg`、`.png`、`.gif`、`.bmp` 和 `.webp` 文件；`directory` 参数受运行时 `Read` 安全级别保护。
