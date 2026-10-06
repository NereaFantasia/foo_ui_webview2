# Artwork API

专辑封面获取。支持 `fb2k://` 协议 URL、Base64 dataUrl、批量获取等多种方式。

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

<!-- api-schema:begin artwork.getFb2kUrl -->
生成正在播放曲目图片的 `fb2k://artwork/` URL；页面加载它时由资源处理器读图并缩放。没在播放时 `available: false` 且 `reason: "no_track"`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `type` | `"front" \| "cover_front" \| "back" \| "cover_back" \| "disc" \| "icon" \| "artist"` | 否 | 图片类型。默认 `"front"`。 |
| `maxSize` | `integer` | 否 | 处理器把图片缩到的最长边（像素）；省略或 `0` 保持原尺寸。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `available` | `boolean` | 是否有正在播放的曲目。 |
| `type` | `string` | URL 里带的类型：`cover_` 写法归一为 `front` 或 `back`。 |
| `dataUrl` | `string` | `fb2k://artwork/?path=...` URL；不是 data URL，只有本组件的 WebView2 资源处理器认得它。 |
| `reason` | `"no_track" \| "not_found"` | 没有 URL 的原因；这里只会出现 `no_track`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const result = await fb2k.invoke('artwork.getFb2kUrl', { maxSize: 300 });
if (result.success === false) throw new Error(result.error);
if (result.available) document.getElementById('cover').src = result.dataUrl;
```

### artwork.getFb2kUrlByPath

<!-- api-schema:begin artwork.getFb2kUrlByPath -->
为某个路径生成 `fb2k://artwork/` URL。只拼字符串，不打开文件，所以没有文件的路径同样报可用。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 曲目路径。不能为空。 |
| `type` | `"front" \| "cover_front" \| "back" \| "cover_back" \| "disc" \| "icon" \| "artist"` | 否 | 图片类型。默认 `"front"`。 |
| `maxSize` | `integer` | 否 | 处理器把图片缩到的最长边（像素）；省略或 `0` 保持原尺寸。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `available` | `boolean` | 恒为 `true`：URL 不打开文件就能生成。 |
| `type` | `string` | URL 里带的类型：`cover_` 写法归一为 `front` 或 `back`。 |
| `path` | `string` | 原样的路径。 |
| `dataUrl` | `string` | `fb2k://artwork/?path=...` URL；不是 data URL，只有本组件的 WebView2 资源处理器认得它。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
// 推荐：通过 API 获取 URL（最安全）
const result = await fb2k.invoke('artwork.getFb2kUrlByPath', {
    path: trackPath, type: 'front', maxSize: 300
});
if (result.success === false) throw new Error(result.error);
img.src = result.dataUrl;

// 或手动拼接（使用 query param 格式，避免 Chromium 路径规范化问题）
function getCoverUrl(trackPath, maxSize = 300) {
    return `fb2k://artwork/?path=${encodeURIComponent(trackPath)}&type=front&maxSize=${maxSize}`;
}
```

### artwork.getFb2kUrlByPathBatch

<!-- api-schema:begin artwork.getFb2kUrlByPathBatch -->
为最多 100 个条目生成 `fb2k://artwork/` URL，条目由 `paths` 与 `items` 之一给出；两个都给或都不给以 `INVALID_PARAMS` 失败。每行报各自的结果，坏条目不会让整次调用失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `string[]` | 否 | 曲目路径；`paths` 与 `items` 恰好给一个。 |
| `items` | `ArtworkBatchItem[]` | 否 | 带各自类型或缩放上限的条目；`paths` 与 `items` 恰好给一个。 |
| `items[].path` | `string` | 是 | 曲目路径。 |
| `items[].type` | `"front" \| "cover_front" \| "back" \| "cover_back" \| "disc" \| "icon" \| "artist"` | 否 | 这一条的图片类型；省略则用整批的 `type`。 |
| `items[].maxSize` | `integer` | 否 | 这一条的缩放上限；省略则用整批的 `maxSize`。 |
| `type` | `"front" \| "cover_front" \| "back" \| "cover_back" \| "disc" \| "icon" \| "artist"` | 否 | 没写自己类型的条目用的图片类型。默认 `"front"`。 |
| `maxSize` | `integer` | 否 | 没写自己上限的条目用的最长边（像素）；省略或 `0` 保持原尺寸。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `artworks` | `ArtworkUrlRow[]` | 每个条目一行，按给定顺序。 |
| `artworks[].path` | `string` | 该条目的路径。 |
| `artworks[].available` | `boolean` | 是否生成了 URL。 |
| `artworks[].type` | `string` | URL 里带的类型。 |
| `artworks[].dataUrl` | `string` | `fb2k://artwork/?path=...` URL。 |
| `artworks[].error` | `string` | 没生成 URL 的原因，是 URL 生成器的错误名，如 `invalid_type`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
// 100 张封面，单次 IPC 往返 (~2ms)
const tracks = await fb2k.invoke('playlist.getTracks', { count: 100 });
if (tracks.success === false) throw new Error(tracks.error);
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
if (artwork.success === false) throw new Error(artwork.error);
if (artwork.available) {
    const comma = artwork.dataUrl.indexOf(',');
    const payload = artwork.dataUrl.slice(comma + 1);
    await fb2k.invoke('file.write', {
        path: '%profile%\\cover.jpg',
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

<!-- api-schema:begin artwork.getCurrent -->
读正在播放曲目的图片。按顺序尝试三种查法，`source` 报是哪一种答的；没在播放时 `available: false` 且 `reason: "no_track"`，三种都没有图片时 `reason: "not_found"`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `type` | `"front" \| "cover_front" \| "back" \| "cover_back" \| "disc" \| "icon" \| "artist"` | 否 | 图片类型。默认 `"front"`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `available` | `boolean` | 是否找到图片。 |
| `type` | `string` | 请求的类型。 |
| `source` | `"now_playing_manager" \| "album_art_manager_v2" \| "extractor"` | 答复的查法：正在播放缓存、专辑封面管理器或提取器。 |
| `mimeType` | `string` | 从字节判断出的 MIME 类型。 |
| `size` | `integer` | 图片字节数。 |
| `dataUrl` | `string` | 图片的 `data:<mime>;base64,...` URL。 |
| `reason` | `"no_track" \| "not_found"` | 没找到图片的原因。 |
| `path` | `string` | 正在播放曲目的路径；没为它找到图片时报出。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

| source 值 | 说明 |

### artwork.getByPath

<!-- api-schema:begin artwork.getByPath -->
用专辑封面提取器直接从 `path` 的文件里读图片。`|subsong:N` 后缀被去掉，因为图片属于文件；`file-relative://` 路径以 `INVALID_PATH` 拒绝，只有播放列表行才能解析它。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 曲目路径：原生路径、`file://`，或带 `\|subsong:N` 后缀。不能为空。 |
| `type` | `"front" \| "cover_front" \| "back" \| "cover_back" \| "disc" \| "icon" \| "artist"` | 否 | 图片类型。默认 `"front"`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `available` | `boolean` | 文件是否有这张图片。 |
| `type` | `string` | 请求的类型。 |
| `path` | `string` | 原样的路径。 |
| `mimeType` | `string` | 从字节判断出的 MIME 类型。 |
| `size` | `integer` | 图片字节数。 |
| `dataUrl` | `string` | 图片的 `data:<mime>;base64,...` URL。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

::: tip 路径 Contract

- **支持**: 原生路径、`file://` 前缀、`path|subsong:N`（subsong 会被剥离，提取器以文件级别操作）
- **拒绝**: `file-relative://` 路径会返回显式错误，请改用 `artwork.getByPlaylistItem`
- 路径会在内部自动规范化（canonical path）

:::

### artwork.getForTrack

<!-- api-schema:begin artwork.getForTrack -->
经专辑封面管理器读图片（也会找到文件夹封面）并量尺寸。`width` 与 `height` 从 PNG 头读出，其他格式为 `0`。`file-relative://` 路径以 `INVALID_PATH` 拒绝。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 曲目路径：原生路径、`file://`，或带 `\|subsong:N` 后缀。不能为空。 |
| `type` | `"front" \| "cover_front" \| "back" \| "cover_back" \| "disc" \| "icon" \| "artist"` | 否 | 图片类型。默认 `"front"`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `available` | `boolean` | 是否找到图片。 |
| `type` | `string` | 请求的类型。 |
| `path` | `string` | 原样的路径。 |
| `mimeType` | `string` | 从字节判断出的 MIME 类型。 |
| `width` | `integer` | 从 PNG 头读出的宽度（像素）；其他格式为 `0`。 |
| `height` | `integer` | 从 PNG 头读出的高度（像素）；其他格式为 `0`。 |
| `size` | `integer` | 图片字节数。 |
| `dataUrl` | `string` | 图片的 `data:<mime>;base64,...` URL。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

::: tip 性能建议
列表/网格视图中使用 `artwork.getCurrent` 或 `artwork.getByPlaylistItem` 的 `maxSize: 200` 可将数据传输量从 ~2MB 减少到 ~15KB。`artwork.getForTrack` 不支持 `maxSize` 参数。
:::

::: warning width / height 限制
`width` 和 `height` 仅对 **PNG** 格式封面返回实际尺寸。JPEG、GIF、BMP、WebP 格式返回值为 `0`。
:::

::: warning
如果曲目路径是 `file-relative://` 格式，请使用 `artwork.getByPlaylistItem`。
:::

### artwork.getByPlaylistItem

<!-- api-schema:begin artwork.getByPlaylistItem -->
经专辑封面管理器读播放列表某一行的图片。`playlist` 为负表示活动播放列表，`index` 为负表示第一行。越界的行（含空列表）以 `NOT_FOUND` 失败。超出最后一个播放列表的序号以 `INVALID_INDEX` 失败，没有列表持有的 `playlistGuid` 以 `NOT_FOUND` 失败，要用活动播放列表而没有时以 `NO_ACTIVE_ITEM` 失败。那一行没有这张图片时成功，`available` 为 `false`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 播放列表序号；负数表示活动播放列表。超出最后一个播放列表的序号以 `INVALID_INDEX` 失败。默认 `-1`。 |
| `playlistGuid` | `string` | 否 | 播放列表的 GUID，用来代替序号；写法带花括号，与 `playlist.getAll` 以及所有指明播放列表的结果、事件报出的一致；十六进制大小写均可。别的列表增删或重排时它仍指向同一个列表，序号做不到。同时给了它和序号，或 GUID 格式不对，以 `INVALID_PARAMS` 失败。列表已不存在时以 `NOT_FOUND` 失败，不会改用别的列表。GUID 格式不对与列表不存在这两种失败都以 `details.playlistGuid` 带回传入的 GUID。 |
| `index` | `integer` | 否 | 行号；负数表示第一行。默认 `-1`。 |
| `type` | `"front" \| "cover_front" \| "back" \| "cover_back" \| "disc" \| "icon" \| "artist"` | 否 | 图片类型。默认 `"front"`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `available` | `boolean` | 是否找到图片。 |
| `type` | `string` | 请求的类型。 |
| `playlist` | `integer` | 实际读的播放列表。 |
| `playlistGuid` | `string` | 播放列表的 GUID，写成 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。作为 `playlistGuid` 传入，播放列表清单变了之后仍能指定这个列表。 |
| `index` | `integer` | 实际读的行。 |
| `mimeType` | `string` | 从字节判断出的 MIME 类型。 |
| `size` | `integer` | 图片字节数。 |
| `dataUrl` | `string` | 图片的 `data:<mime>;base64,...` URL。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### artwork.getAvailableTypes

<!-- api-schema:begin artwork.getAvailableTypes -->
列出文件里嵌入的图片类型，按固定探测顺序 front、back、disc、icon、artist。不给 `path` 时用正在播放的曲目；没在播放时以 `NO_ACTIVE_ITEM` 失败。文件不存在、读不了或格式不带嵌入图片都不算错误，只是列不出类型。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 否 | 曲目路径；省略则取正在播放的曲目。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `types` | `string[]` | 嵌入的图片类型，按探测顺序。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### artwork.getAvailableArtwork

<!-- api-schema:begin artwork.getAvailableArtwork -->
报告文件嵌入了哪些图片类型，以及旁边有哪些封面文件。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 曲目路径；`\|subsong:N` 后缀被去掉。不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `available` | `boolean` | 文件是否至少嵌有一张图片。 |
| `artworks` | `ArtworkAvailableEntry[]` | 嵌入的图片，按探测顺序。 |
| `artworks[].type` | `string` | 图片类型。 |
| `artworks[].source` | `string` | 来源；恒为 `embedded`，因为只列嵌入的图片。 |
| `sources` | `string[]` | 嵌有图片时先有一个 `embedded`，然后是文件旁每个封面文件（`cover`、`folder`、`front`、`album` 的 `.jpg` 或 `.png`）对应的 `folder:<名字>`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### artwork.getFolderImages

<!-- api-schema:begin artwork.getFolderImages -->
列出目录下直接包含的图片文件（`.jpg`、`.jpeg`、`.png`、`.gif`、`.bmp`、`.webp`）。目录不存在、是文件或列不出内容都不算错误，只是列不出图片。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `directory` | `string` | 是 | 要列的目录。不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `images` | `ArtworkFolderImage[]` | 图片文件，按目录顺序。 |
| `images[].name` | `string` | 文件名。 |
| `images[].path` | `string` | 完整路径。 |
| `images[].size` | `integer` | 文件字节数。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

支持的图片格式: `.jpg`, `.jpeg`, `.png`, `.gif`, `.bmp`, `.webp`

### artwork.getLyrics

<!-- api-schema:begin artwork.getLyrics -->
读曲目的歌词标签。宿主缓存的曲目信息齐全时用缓存；没读过的本地文件从磁盘读，远程或未识别的路径只用缓存里有的。按 `LYRICS`、`UNSYNCED LYRICS`、`UNSYNCEDLYRICS`、`SYNCEDLYRICS`、`SYNCED LYRICS` 的顺序探测，第一个非空的胜出。不给 `path` 时用正在播放的曲目；没在播放时以 `NO_ACTIVE_ITEM` 失败。建不出曲目的路径以 `NOT_FOUND` 失败，读不了的本地文件以 `OPERATION_FAILED` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 否 | 曲目路径；省略则取正在播放的曲目。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `available` | `boolean` | 是否找到非空的歌词标签。 |
| `tag` | `string` | 提供歌词的标签。 |
| `lyrics` | `string` | 歌词文本。 |
| `synced` | `boolean` | 标签名表明是同步歌词时为 `true`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

支持的歌词标签: `LYRICS`, `UNSYNCED LYRICS`, `UNSYNCEDLYRICS`, `SYNCEDLYRICS`, `SYNCED LYRICS`

### artwork.getMetadata

<!-- api-schema:begin artwork.getMetadata -->
读曲目的专辑级标签，并报文件是否嵌有图片、是否带歌词标签。标签在宿主缓存的曲目信息齐全时取自缓存；没读过的本地文件从磁盘读，远程或未识别的路径只用缓存里有的，可能是空的。不给 `path` 时用正在播放的曲目；没在播放时以 `NO_ACTIVE_ITEM` 失败。建不出曲目的路径以 `NOT_FOUND` 失败，读不了的本地文件以 `OPERATION_FAILED` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 否 | 曲目路径；省略则取正在播放的曲目。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `available` | `boolean` | 恒为 `true`：曲目已解析，不管读没读到它的标签。 |
| `album` | `string` | `ALBUM` 标签；没有时为空。 |
| `artist` | `string` | `ARTIST` 各值按标签顺序以 `, ` 连接。 |
| `albumArtist` | `string` | `ALBUM ARTIST` 各值按标签顺序以 `, ` 连接。 |
| `title` | `string` | `TITLE` 标签；没有时为空。 |
| `year` | `string` | 原样的 `DATE` 标签；没有时为空。 |
| `genre` | `string` | `GENRE` 各值按标签顺序以 `, ` 连接。 |
| `trackNumber` | `string` | 原样的 `TRACKNUMBER` 标签，如 `3` 或 `3/12`；没有时为空。 |
| `discNumber` | `string` | 原样的 `DISCNUMBER` 标签；没有时为空。 |
| `hasEmbedded` | `boolean` | 文件是否嵌有五种图片类型中的任何一种。 |
| `hasLyrics` | `boolean` | 是否存在 `artwork.getLyrics` 识别的五种标签中的任意一种，空的也算。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

> `artist` / `albumArtist` / `genre` / `composer`（仅指该 API 实际返回的字段）的多值标签按 `, ` 原序拼接，不去重。

### artwork.getBatch

<!-- api-schema:begin artwork.getBatch -->
用专辑封面提取器从多个文件读同一种图片，按给定顺序每个路径一行；没有该图片的文件那一行 `available: false`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `string[]` | 是 | 曲目路径；`\|subsong:N` 后缀被去掉。 |
| `type` | `"front" \| "cover_front" \| "back" \| "cover_back" \| "disc" \| "icon" \| "artist"` | 否 | 每个路径用的图片类型。默认 `"front"`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `artworks` | `ArtworkBatchRow[]` | 每个路径一行，按给定顺序。 |
| `artworks[].path` | `string` | 原样的路径。 |
| `artworks[].available` | `boolean` | 文件是否有这张图片。 |
| `artworks[].mimeType` | `string` | 从字节判断出的 MIME 类型。 |
| `artworks[].size` | `integer` | 图片字节数。 |
| `artworks[].dataUrl` | `string` | 图片的 `data:<mime>;base64,...` URL。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

::: danger 已废弃
请迁移到 `artwork.getFb2kUrlByPathBatch`。此 API 仍返回 Base64 dataUrl，性能较差。
:::

```javascript
// ❌ 旧方式
const result = await fb2k.invoke('artwork.getBatch', { paths });

// ✅ 新方式
const result = await fb2k.invoke('artwork.getFb2kUrlByPathBatch', { paths, type: 'front' });
```

## 使用说明

- 有效的 artwork `type` 为 `front`（也可用 `cover_front`）、`back`（也可用 `cover_back`）、`disc`、`icon` 和 `artist`。省略 `type` 时使用 `front`；未知值会返回 `INVALID_PARAMS`。
- `artwork.getByPath` 和 `artwork.getForTrack` 接受原生路径、`file://` 路径和 `path|subsong:N`。它们拒绝 `file-relative://`，因为 extractor 没有播放列表上下文；此类条目请用 `artwork.getByPlaylistItem`。
- 直接读取封面会返回 `data:image/...` URL。`artwork.getFb2kUrl` 及其路径变体则在 `dataUrl` 字段返回 `fb2k://artwork/` URL。仅当 `maxSize` 大于 `0` 时才应用缩放。
- `artwork.getFb2kUrlByPathBatch` 必须提供一个名为 `paths` 或 `items` 的数组。数组条目可以是字符串，或含有 `path` 成员的对象；它没有顶层 `path` 参数。返回 `{ success, artworks }`，每个输入条目对应一个 `available` 或 `error` 结果。
- `artwork.getAvailableArtwork` 会报告嵌入项和 `folder:cover.jpg` 等外部来源标签。它通过 `album_art_extractor` 打开文件；未找到封面以 `available: false` 表示，不一定是错误。
- `artwork.getFolderImages` 读取目录中的 `.jpg`、`.jpeg`、`.png`、`.gif`、`.bmp` 和 `.webp` 文件；`directory` 参数受运行时 `Read` 安全级别保护。
