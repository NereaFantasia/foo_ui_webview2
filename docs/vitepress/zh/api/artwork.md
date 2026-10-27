# Artwork API

获取与解析音频内嵌及外置专辑封面，支持 `fb2k://` 协议 URL、Base64 Data URL、批量探测与缩放管理。

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
生成当前播放曲目封面的 `fb2k://artwork/` 专用协议 URL；前端页面加载该 URL 时由宿主资源处理器动态读取并缩放；未在播放时返回 `available: false`（`reason: "no_track"`）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `type` | `"front" \| "cover_front" \| "back" \| "cover_back" \| "disc" \| "icon" \| "artist"` | 否 | 待读取的图片类型默认 `"front"`。 |
| `maxSize` | `integer` | 否 | 图片缩放的最长边像素上限；省略或传 `0` 时保持原始尺寸 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `available` | `boolean` | 当前是否有正在播放的有效曲目 |
| `type` | `string` | 最终写入 URL 的类型标识（`cover_front` / `cover_back` 归一为 `front` / `back`） |
| `dataUrl` | `string` | 生成的 `fb2k://artwork/?path=...` 专用协议 URL（仅供本组件 WebView2 内部加载解析） |
| `reason` | `"no_track" \| "not_found"` | 未生成 URL 的原因说明（此处仅可能为 `no_track`） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const result = await fb2k.invoke('artwork.getFb2kUrl', { maxSize: 300 });
if (result.success === false) throw new Error(result.error);
if (result.available) document.getElementById('cover').src = result.dataUrl;
```

### artwork.getFb2kUrlByPath

<!-- api-schema:begin artwork.getFb2kUrlByPath -->
为指定曲目路径生成 `fb2k://artwork/` 协议 URL；仅执行字符串拼接而不实际打开文件（目标文件不存在时亦正常返回可用 URL）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 目标曲目路径不能为空。 |
| `type` | `"front" \| "cover_front" \| "back" \| "cover_back" \| "disc" \| "icon" \| "artist"` | 否 | 待读取的图片类型默认 `"front"`。 |
| `maxSize` | `integer` | 否 | 图片缩放的最长边像素上限；省略或传 `0` 时保持原始尺寸 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `available` | `boolean` | URL 是否成功构建，固定为 `true`（仅拼接路径不打开文件） |
| `type` | `string` | 最终写入 URL 的类型标识（`cover_front` / `cover_back` 归一为 `front` / `back`） |
| `path` | `string` | 请求传入的原样路径 |
| `dataUrl` | `string` | 生成的 `fb2k://artwork/?path=...` 专用协议 URL（仅供本组件 WebView2 内部加载解析） |

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
批量生成最多 100 个条目的 `fb2k://artwork/` 协议 URL；`paths` 与 `items` 必须二选一传入，同时传入或均未传入以 `INVALID_PARAMS` 失败；逐项返回生成结果，单项错误不影响整批调用

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `string[]` | 否 | 批量曲目路径列表；与 `items` 严格二选一传入 |
| `items` | `ArtworkBatchItem[]` | 否 | 附带独立类型或尺寸选项的批量条目列表；与 `paths` 严格二选一传入 |
| `items[].path` | `string` | 是 | 目标曲目路径 |
| `items[].type` | `"front" \| "cover_front" \| "back" \| "cover_back" \| "disc" \| "icon" \| "artist"` | 否 | 该项请求的图片类型；省略时沿用整批的 `type` 参数 |
| `items[].maxSize` | `integer` | 否 | 该项缩放的最长边像素上限；省略时沿用整批的 `maxSize` 参数 |
| `type` | `"front" \| "cover_front" \| "back" \| "cover_back" \| "disc" \| "icon" \| "artist"` | 否 | 未单独指定类型的条目所使用的全局默认图片类型默认 `"front"`。 |
| `maxSize` | `integer` | 否 | 未单独指定尺寸的条目所使用的全局最长边像素上限；省略或传 `0` 时保持原始尺寸 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `artworks` | `ArtworkUrlRow[]` | 批量生成的图片 URL 结果列表，按输入顺序排列 |
| `artworks[].path` | `string` | 该条目对应的曲目路径 |
| `artworks[].available` | `boolean` | 该条目是否成功生成 URL |
| `artworks[].type` | `string` | URL 中生效的图片类型 |
| `artworks[].dataUrl` | `string` | 生成的 `fb2k://artwork/?path=...` 专用协议 URL |
| `artworks[].error` | `string` | URL 生成失败的原因代码（如 `invalid_type`） |

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
读取当前正在播放曲目的封面图片；按顺序尝试三种检索策略并在 `source` 中指明胜出源；未在播放时返回 `available: false`（`reason: "no_track"`），均未查得图片时返回 `reason: "not_found"`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `type` | `"front" \| "cover_front" \| "back" \| "cover_back" \| "disc" \| "icon" \| "artist"` | 否 | 待读取的图片类型默认 `"front"`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `available` | `boolean` | 是否成功获取到图片 |
| `type` | `string` | 请求的图片类型 |
| `source` | `"now_playing_manager" \| "album_art_manager_v2" \| "extractor"` | 实际响应的检索来源：正在播放缓存（`now_playing_manager`）、专辑封面管理器（`album_art_manager_v2`）或提取器（`extractor`） |
| `mimeType` | `string` | 根据二进制数据特征检测出的 MIME 类型 |
| `size` | `integer` | 图片二进制数据大小（字节） |
| `dataUrl` | `string` | 图片的 Base64 Data URL（`data:<mime>;base64,...`） |
| `reason` | `"no_track" \| "not_found"` | 未找到图片的原因代码 |
| `path` | `string` | 当前播放曲目的文件路径（未找到图片时返回） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

| source 值 | 说明 |

### artwork.getByPath

<!-- api-schema:begin artwork.getByPath -->
使用专辑封面提取器直接从指定文件路径中读取内嵌图片；自动剥离 `|subsong:N` 子曲目后缀（图片归属于物理文件本身）；`file-relative://` 相对路径以 `INVALID_PATH` 拒绝（仅播放列表行可解析该协议）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 目标曲目路径（支持本地路径、`file://` 或附带 `\|subsong:N` 后缀）不能为空。 |
| `type` | `"front" \| "cover_front" \| "back" \| "cover_back" \| "disc" \| "icon" \| "artist"` | 否 | 待读取的图片类型默认 `"front"`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `available` | `boolean` | 目标文件是否包含该图片 |
| `type` | `string` | 请求的图片类型 |
| `path` | `string` | 请求传入的原样路径 |
| `mimeType` | `string` | 根据二进制数据特征检测出的 MIME 类型 |
| `size` | `integer` | 图片二进制数据大小（字节） |
| `dataUrl` | `string` | 图片的 Base64 Data URL（`data:<mime>;base64,...`） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

::: tip 路径 Contract

- **支持**: 原生路径、`file://` 前缀、`path|subsong:N`（subsong 会被剥离，提取器以文件级别操作）
- **拒绝**: `file-relative://` 路径会返回显式错误，请改用 `artwork.getByPlaylistItem`
- 路径会在内部自动规范化（canonical path）

:::

### artwork.getForTrack

<!-- api-schema:begin artwork.getForTrack -->
通过专辑封面管理器读取曲目封面（包含内嵌封面及所在目录的外置封面文件）并获取尺寸；从 PNG 头信息解析 `width` 与 `height`（其他格式返回 `0`）；`file-relative://` 路径以 `INVALID_PATH` 拒绝

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 目标曲目路径（支持本地路径、`file://` 或附带 `\|subsong:N` 后缀）不能为空。 |
| `type` | `"front" \| "cover_front" \| "back" \| "cover_back" \| "disc" \| "icon" \| "artist"` | 否 | 待读取的图片类型默认 `"front"`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `available` | `boolean` | 是否成功获取到图片 |
| `type` | `string` | 请求的图片类型 |
| `path` | `string` | 请求传入的原样路径 |
| `mimeType` | `string` | 根据二进制数据特征检测出的 MIME 类型 |
| `width` | `integer` | 图片像素宽度（从 PNG 头解析得出，其他格式为 `0`） |
| `height` | `integer` | 图片像素高度（从 PNG 头解析得出，其他格式为 `0`） |
| `size` | `integer` | 图片二进制数据大小（字节） |
| `dataUrl` | `string` | 图片的 Base64 Data URL（`data:<mime>;base64,...`） |

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
通过专辑封面管理器读取指定播放列表行的封面图片；传入负数 `playlist` 表示活动播放列表，传入负数 `index` 表示首行；行索引越界或空列表以 `NOT_FOUND` 失败，播放列表索引越界以 `INVALID_INDEX` 失败，`playlistGuid` 不存在以 `NOT_FOUND` 失败，未指定且无活动播放列表时以 `NO_ACTIVE_ITEM` 失败；该行无图片时成功返回 `available: false`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 目标播放列表序号；传入负数表示活动播放列表；超出范围以 `INVALID_INDEX` 失败默认 `-1`。 |
| `playlistGuid` | `string` | 否 | 播放列表的持久唯一标识（带花括号 GUID，大小写均可）。用于替代易变序号。与序号互斥，同时传入或格式非法以 `INVALID_PARAMS` 失败。列表不存在时以 `NOT_FOUND` 失败，不回退至其他列表。格式非法或列表不存在时，错误详情均带回 `details.playlistGuid`。 |
| `index` | `integer` | 否 | 目标行号索引；传入负数表示首行默认 `-1`。 |
| `type` | `"front" \| "cover_front" \| "back" \| "cover_back" \| "disc" \| "icon" \| "artist"` | 否 | 待读取的图片类型默认 `"front"`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `available` | `boolean` | 是否成功获取到图片 |
| `type` | `string` | 请求的图片类型 |
| `playlist` | `integer` | 实际读取的播放列表索引 |
| `playlistGuid` | `string` | 播放列表的唯一标识，格式为 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。后续可作为 `playlistGuid` 稳定引用该列表。 |
| `index` | `integer` | 实际读取的播放列表行索引 |
| `mimeType` | `string` | 根据二进制数据特征检测出的 MIME 类型 |
| `size` | `integer` | 图片二进制数据大小（字节） |
| `dataUrl` | `string` | 图片的 Base64 Data URL（`data:<mime>;base64,...`） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### artwork.getAvailableTypes

<!-- api-schema:begin artwork.getAvailableTypes -->
列出指定音频文件内嵌的图片类型列表，按固定探测顺序（front、back、disc、icon、artist）排列；省略 `path` 时检测当前播放曲目（未播放时以 `NO_ACTIVE_ITEM` 失败）；文件缺失、不可读或格式不支持内嵌图片时不报错，返回空类型列表

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 否 | 目标曲目路径；省略时默认检测当前播放曲目 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `types` | `string[]` | 文件中内嵌的图片类型列表，按探测先后顺序排列 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### artwork.getAvailableArtwork

<!-- api-schema:begin artwork.getAvailableArtwork -->
探测指定文件内嵌的图片类型以及同目录下存在的外置封面文件

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 待批量读取的曲目路径列表；自动剥离 `\|subsong:N` 后缀不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `available` | `boolean` | 文件是否至少内嵌有一张图片 |
| `artworks` | `ArtworkAvailableEntry[]` | 探测到的所有内嵌图片列表，按探测先后顺序排列 |
| `artworks[].type` | `string` | 待读取的图片类型 |
| `artworks[].source` | `string` | 封面来源标识；固定为 `embedded`（仅列出内嵌图片） |
| `sources` | `string[]` | 封面来源清单：内嵌图片标记为 `embedded`，同目录下外置封面文件按 `folder:<名字>` 格式列出（匹配 `cover`、`folder`、`front`、`album` 的 `.jpg` 或 `.png`） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### artwork.getFolderImages

<!-- api-schema:begin artwork.getFolderImages -->
列出指定目录下一级直接包含的常用图片文件（支持 `.jpg`、`.jpeg`、`.png`、`.gif`、`.bmp`、`.webp`）；目录不存在、路径为文件或无读取权限时不报错，返回空图片列表

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `directory` | `string` | 是 | 待扫描的目标目录路径不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `images` | `ArtworkFolderImage[]` | 目录下扫描到的图片文件列表，按目录遍历顺序排列 |
| `images[].name` | `string` | 图片文件名称 |
| `images[].path` | `string` | 图片完整物理路径 |
| `images[].size` | `integer` | 图片文件大小（字节） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

支持的图片格式: `.jpg`, `.jpeg`, `.png`, `.gif`, `.bmp`, `.webp`

### artwork.getLyrics

<!-- api-schema:begin artwork.getLyrics -->
读取曲目内嵌的歌词标签；宿主缓存完备时优先从缓存读取，未缓存的本地文件从磁盘读取，远程网络流或无法识别的路径仅使用现有缓存；依次按 `LYRICS`、`UNSYNCED LYRICS`、`UNSYNCEDLYRICS`、`SYNCEDLYRICS`、`SYNCED LYRICS` 顺序探测并取首个非空标签；省略 `path` 时读取当前播放曲目（未播放时以 `NO_ACTIVE_ITEM` 失败）；无法解析曲目以 `NOT_FOUND` 失败，文件读取失败以 `OPERATION_FAILED` 失败；歌词中的非法 UTF-8 字符自动替换为 U+FFFD 且不修改原文件

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 否 | 目标曲目路径；省略时默认检测当前播放曲目 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `available` | `boolean` | 是否成功读取到非空歌词标签 |
| `tag` | `string` | 最终胜出提供歌词的标签名称 |
| `lyrics` | `string` | 读取出的完整歌词文本 |
| `synced` | `boolean` | 标签名是否指示该歌词为时间同步歌词 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

支持的歌词标签: `LYRICS`, `UNSYNCED LYRICS`, `UNSYNCEDLYRICS`, `SYNCEDLYRICS`, `SYNCED LYRICS`

### artwork.getMetadata

<!-- api-schema:begin artwork.getMetadata -->
读取曲目的专辑级基本标签，并检测文件是否内嵌图片或包含歌词标签；缓存完备时优先取自缓存，未缓存的本地文件从磁盘读取；省略 `path` 时检测当前播放曲目（未播放时以 `NO_ACTIVE_ITEM` 失败）；无法解析曲目以 `NOT_FOUND` 失败，文件不可读以 `OPERATION_FAILED` 失败；返回标签中的非法 UTF-8 替换为 U+FFFD 且不修改原文件

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 否 | 目标曲目路径；省略时默认检测当前播放曲目 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `available` | `boolean` | 曲目是否成功解析，固定为 `true`（即使所有标签均为空） |
| `album` | `string` | `ALBUM` 标签值；缺失时为空字符串 |
| `artist` | `string` | 所有 `ARTIST` 标签值按原顺序以 `, ` 连接 |
| `albumArtist` | `string` | 所有 `ALBUM ARTIST` 标签值按原顺序以 `, ` 连接 |
| `title` | `string` | `TITLE` 标签值；缺失时为空字符串 |
| `year` | `string` | `DATE` 标签原始字符串；缺失时为空字符串 |
| `genre` | `string` | 所有 `GENRE` 标签值按原顺序以 `, ` 连接 |
| `trackNumber` | `string` | `TRACKNUMBER` 标签原始书写格式（如 `3` 或 `3/12`）；缺失时为空字符串 |
| `discNumber` | `string` | `DISCNUMBER` 标签原始书写格式；缺失时为空字符串 |
| `hasEmbedded` | `boolean` | 文件是否内嵌了五种封面图片类型中的任意一种 |
| `hasLyrics` | `boolean` | 文件是否包含五种常见歌词标签中的任意一种（空标签亦计入） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

> `artist` / `albumArtist` / `genre` / `composer`（仅指该 API 实际返回的字段）的多值标签按 `, ` 原序拼接，不去重。

### artwork.getBatch

<!-- api-schema:begin artwork.getBatch -->
使用专辑封面提取器批量读取多个音频文件的同一种封面图片；按输入路径顺序逐行返回，目标文件未包含该图片类型时对应行返回 `available: false`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `string[]` | 是 | 待批量读取的曲目路径列表；自动剥离 `\|subsong:N` 后缀 |
| `type` | `"front" \| "cover_front" \| "back" \| "cover_back" \| "disc" \| "icon" \| "artist"` | 否 | 整批读取的统一图片类型默认 `"front"`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `artworks` | `ArtworkBatchRow[]` | 批量读取结果列表，按输入路径顺序排列 |
| `artworks[].path` | `string` | 请求传入的原样路径 |
| `artworks[].available` | `boolean` | 目标文件是否包含该图片 |
| `artworks[].mimeType` | `string` | 根据二进制数据特征检测出的 MIME 类型 |
| `artworks[].size` | `integer` | 图片二进制数据大小（字节） |
| `artworks[].dataUrl` | `string` | 图片的 Base64 Data URL（`data:<mime>;base64,...`） |

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
- `artwork.getFolderImages` 读取目录中的 `.jpg`、`.jpeg`、`.png`、`.gif`、`.bmp` 和 `.webp` 文件；`directory` 参数按 `MediaRead` 级别检查，媒体库监视目录下的文件夹即使 `Read` 不放行也能读取。
