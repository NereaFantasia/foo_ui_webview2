# fb.artwork 封面

封面获取建议按优先级使用：

1. **展示用途（推荐）**: `fb.artwork.getFb2kUrl()` / `getFb2kUrlByPath()` — 返回 `fb2k://` URL，高性能二进制响应
2. **需要图片内容**: `fb.artwork.getForTrack()` — 返回 `dataUrl`，适合保存/上传/写入标签

两类 URI 格式具有不同用途：`fb2k://` URL 只能在本组件的 WebView2 内解析，不是可持久化的图片内容；直接读取则返回标准 Data URL，保存或嵌入前须提取 Base64 载荷并按目标端点要求进行编码转换。

> 封面接口统一返回 `available` / `dataUrl` / `url` 字段。

## getFb2kUrl(type?, options?)

获取当前播放曲目的 `fb2k://` 封面 URL（高性能展示）。

- `type?`: `'front'` | `'back'` | `'disc'` | `'icon'` | `'artist'`，默认 `'front'`
- `options?`: `{ maxSize?: number }` — 缩略图最大边长（像素）

```javascript
const res = await fb.artwork.getFb2kUrl('front', { maxSize: 300 });
if (res.success === false) throw new Error(res.error);
if (res.available && res.dataUrl) {
    document.getElementById('cover').src = res.dataUrl;
}
```

## getFb2kUrlByPath(path, type?, options?)

根据文件路径获取 `fb2k://` 封面 URL。

```javascript
const res = await fb.artwork.getFb2kUrlByPath('E:\\Music\\song.flac', 'front', { maxSize: 200 });
```

## fb2k://artwork 协议说明

- URL 结构: `fb2k://artwork/<编码后的path>/<type>?maxSize=200`
- path 必须做 URL 编码（SDK 已处理）
- `maxSize` 可选，插件端会在协议层缩放后再返回二进制
- 该 URL 只适合当前组件 WebView 中即时展示，不能传给 `fb.file.write()` 或 `fb.metadata.embedArtwork()`。

## 保存返回的封面

直接封面读取返回 `data:<mime>;base64,<payload>`。`fb.file.write()` 需要
`base64:<payload>` 并同时传 `{ encoding: 'binary' }`；
`fb.metadata.embedArtwork()` 则只接受裸 `<payload>`。

```javascript
const cover = await fb.artwork.getCurrent('front');
if (cover.success === false) throw new Error(cover.error);
if (cover.available && cover.dataUrl) {
    const comma = cover.dataUrl.indexOf(',');
    const payload = cover.dataUrl.slice(comma + 1);
    await fb.file.write('C:\\Config\\cover.jpg', `base64:${payload}`, {
        encoding: 'binary',
    });
}
```

## getForTrack(path, type?, options?)

获取指定曲目封面内容（返回 `dataUrl`）。适合保存/上传/写入标签。

```javascript
const res = await fb.artwork.getForTrack('E:\\Music\\song.flac', 'front', { maxSize: 300 });
```

## getCurrent(type?)

获取当前播放曲目封面内容。返回 `{available, dataUrl?, type, mimeType?, size?}`。纯展示建议用 `getFb2kUrl()`。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| type | string | 'front' / 'back' / 'disc' / 'icon' / 'artist'（默认 'front'） |

```javascript
const res = await fb.artwork.getCurrent('front');
if (res.success === false) throw new Error(res.error);
if (res.available) img.src = res.dataUrl;
```

## getByPath(path, type?)

签名：`fb.artwork.getByPath(path: string, type?: AlbumArtType): Promise<ArtworkGetByPathResponse>`

根据文件路径获取封面内容。返回 `{available, dataUrl?, type, mimeType?, size?}`。纯展示建议用 `getFb2kUrlByPath()`。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| path | string | 音频文件路径 |
| type | AlbumArtType | 封面类型（默认 'front'） |

```javascript
const cover = await fb.artwork.getByPath('E:\\Music\\song.flac', 'front');
```

## withMaxSize(url, maxSize?)

工具函数：给 `fb2k://` URL 追加 `maxSize` 参数。

```javascript
const url = fb.artwork.withMaxSize('fb2k://artwork/...', 300);
// 'fb2k://artwork/...?maxSize=300'
```

## getBatch(paths)

批量获取封面。`paths` 为字符串数组（`string[]`）。

```javascript
const results = await fb.artwork.getBatch([
    'E:\\Music\\a.flac',
    'E:\\Music\\b.mp3'
]);
```

## getByPlaylistItem(playlist, index, type?)

获取播放列表中指定项目的封面。

```javascript
const artwork = await fb.artwork.getByPlaylistItem(0, 12, 'front');
```

## getFb2kUrlByPathBatch(items, opts?)

`getFb2kUrlByPath()` 的批量版本。`items` 可以是 `string[]` 或 `ArtworkBatchItem[]`；`opts` 可提供整批共用的 `type` 与 `maxSize`。返回完整的 `ArtworkBatchResponse` 信封。

```javascript
const batch = await fb.artwork.getFb2kUrlByPathBatch(
    ['E:\\Music\\a.flac', 'E:\\Music\\b.flac'],
    { type: 'front', maxSize: 256 },
);
```

## getAvailableArtwork(path)

签名：`fb.artwork.getAvailableArtwork(path: string): Promise<ArtworkGetAvailableArtworkResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| path | string | 是 | 音频文件路径；`\|subsong:N` 后缀会被去掉 |

返回音频文件内嵌的图片类型以及同目录下的关联封面文件。`artworks` 按探测顺序列出内嵌图片类型，`available` 指示文件是否内嵌至少一张图片。若包含内嵌图片，`sources` 优先包含 `embedded` 项，随后列出同级目录下匹配命名规范（`cover`、`folder`、`front`、`album`，扩展名为 `.jpg` 或 `.png`）的外部图片文件，格式为 `folder:<文件名>`。

```javascript
const available = await fb.artwork.getAvailableArtwork('E:\\Music\\song.flac');
```

## getAvailableTypes(path?)

签名：`fb.artwork.getAvailableTypes(path?: string): Promise<ArtworkGetAvailableTypesResponse>`

以响应里的 `types` 数组返回 `path` 所指文件内嵌的封面类型；省略 `path` 时取当前播放曲目。

```javascript
const types = await fb.artwork.getAvailableTypes();
```

## getFolderImages(directory)

签名：`fb.artwork.getFolderImages(directory: string): Promise<ArtworkGetFolderImagesResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| directory | string | 是 | 目录路径 |

以 `images` 数组返回目录里能识别的图片文件，每行为 `{ name, path, size }`。

```javascript
const images = await fb.artwork.getFolderImages('E:\\Music\\Album');
```

## getLyrics(path?)

签名：`fb.artwork.getLyrics(path?: string): Promise<ArtworkGetLyricsResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| path | string | 否 | 音频文件路径；省略时读取当前播放曲目 |

读取曲目内置的歌词元数据标签。系统按 `LYRICS`、`UNSYNCED LYRICS`、`UNSYNCEDLYRICS`、`SYNCEDLYRICS`、`SYNCED LYRICS` 的优先级依次探测，匹配首个非空标签：`available` 指示是否检索到歌词，`tag` 返回匹配的标签名称，`lyrics` 返回歌词文本，标签名表明歌词为同步歌词时，`synced` 为 `true`。宿主缓存的曲目信息完整时直接使用缓存，尚未读取过的本地文件从磁盘读取。省略 `path` 且当前未播放时以 `NO_ACTIVE_ITEM` 失败；无法解析为有效曲目的路径以 `NOT_FOUND` 失败；无法读取的本地文件以 `OPERATION_FAILED` 失败。

```javascript
const lyrics = await fb.artwork.getLyrics('E:\\Music\\song.flac');
```

## getMetadata(path?)

签名：`fb.artwork.getMetadata(path?: string): Promise<ArtworkGetMetadataResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| path | string | 否 | 音频文件路径；省略时读取当前播放曲目 |

读取曲目的专辑级标签：包含 `album`、`artist`、`albumArtist`、`title`、`year`、`genre`、`trackNumber` 与 `discNumber`，缺失时返回空值，多值标签以 `, ` 连接。`hasEmbedded` 指示文件是否内嵌图像资源，`hasLyrics` 指示是否存在歌词标签（包含空歌词字段）。对于远程路径或无法识别的路径，结果只包含宿主缓存中已有的内容，可能为空。错误处理规则与 `getLyrics()` 保持一致。

```javascript
const metadata = await fb.artwork.getMetadata('E:\\Music\\song.flac');
```
