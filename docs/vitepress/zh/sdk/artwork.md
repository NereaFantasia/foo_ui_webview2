# fb.artwork 封面

封面获取建议按优先级使用：

1. **展示用途（推荐）**: `fb.artwork.getFb2kUrl()` / `getFb2kUrlByPath()` — 返回 `fb2k://` URL，高性能二进制响应
2. **需要图片内容**: `fb.artwork.getForTrack()` — 返回 `dataUrl`，适合保存/上传/写入标签

两种表示不可互换：`fb2k://` 只由本组件 WebView2 解析，不是可持久化的图片内容；直接读取返回标准 Data URL，保存前必须提取 Base64 payload，并按目标 API 的 wire 契约转换。

> 封面接口统一返回 `available` / `dataUrl` / `url` 字段。

## getFb2kUrl(type?, options?)

获取当前播放曲目的 `fb2k://` 封面 URL（高性能展示）。

- `type?`: `'front'` | `'back'` | `'disc'` | `'icon'` | `'artist'`，默认 `'front'`
- `options?`: `{ maxSize?: number }` — 缩略图最大边长（像素）

```javascript
const res = await fb.artwork.getFb2kUrl('front', { maxSize: 300 });
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
if (res.available) img.src = res.dataUrl;
```

## getByPath(path, type?)

根据文件路径获取封面内容。返回 `{available, dataUrl?, type, mimeType?, size?}`。纯展示建议用 `getFb2kUrlByPath()`。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| path | string | 音频文件路径 |
| type | string | 封面类型（默认 'front'） |

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

## 其余方法

### getAvailableArtwork(path?)

签名：`fb.artwork.getAvailableArtwork(path?: string): Promise<ArtworkAvailableResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| path | string | 否 | 音频文件路径；省略时检查当前播放曲目 |

```javascript
const available = await fb.artwork.getAvailableArtwork('E:\\Music\\song.flac');
```

### getAvailableTypes(path?)

签名：`fb.artwork.getAvailableTypes(path?: string): Promise<ArtworkAvailableTypesResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| path | string | 否 | 音频文件路径；省略时检查当前播放曲目 |

```javascript
const types = await fb.artwork.getAvailableTypes();
```

### getByPath(path, type?, options?)

签名：`fb.artwork.getByPath(path: string, type?: string, options?: object): Promise<ArtworkResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| path | string | 是 | 音频文件路径 |
| type | string | 否 | 封面类型，默认 `front` |
| options | object | 否 | 缩略图或返回格式选项 |

```javascript
const cover = await fb.artwork.getByPath('E:\\Music\\song.flac', 'front');
```

### getFolderImages(path)

签名：`fb.artwork.getFolderImages(path: string): Promise<ArtworkFolderImagesResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| path | string | 是 | 音频文件或目录路径 |

```javascript
const images = await fb.artwork.getFolderImages('E:\\Music\\Album\\song.flac');
```

### getLyrics(path?)

签名：`fb.artwork.getLyrics(path?: string): Promise<ArtworkLyricsResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| path | string | 否 | 音频文件路径；省略时读取当前播放曲目 |

```javascript
const lyrics = await fb.artwork.getLyrics('E:\\Music\\song.flac');
```

### getMetadata(path)

签名：`fb.artwork.getMetadata(path: string): Promise<ArtworkMetadataResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| path | string | 是 | 音频文件路径 |

```javascript
const metadata = await fb.artwork.getMetadata('E:\\Music\\song.flac');
```
