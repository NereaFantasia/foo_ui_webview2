# fb.artwork 封面 

封面获取建议按优先级使用：

1. **展示用途（推荐）**: `fb.artwork.getFb2kUrl()` / `getFb2kUrlByPath()` — 返回 `fb2k://` URL，高性能二进制响应
2. **需要图片内容**: `fb.artwork.getForTrack()` — 返回 `dataUrl`，适合保存/上传/写入标签

> 本项目已采用"方案B（一刀切）"，统一使用 `available` / `dataUrl` / `url` 字段。

获取当前播放曲目的 `fb2k://` 封面 URL（高性能展示）。

- `type?`: `'front'` | `'back'` | `'disc'` | `'icon'` | `'artist'`，默认 `'front'`
- `options?`: `{ maxSize?: number }` — 缩略图最大边长（像素）

```javascript
const res = await fb.artwork.getFb2kUrl('front', { maxSize: 300 });
if (res.available && res.dataUrl) {
    document.getElementById('cover').src = res.dataUrl;
}
```

根据文件路径获取 `fb2k://` 封面 URL。

```javascript
const res = await fb.artwork.getFb2kUrlByPath('E:\\\\Music\\\\song.flac', 'front', { maxSize: 200 });
```

## fb2k://artwork 协议说明 

- URL 结构: `fb2k://artwork/<编码后的path>/<type>?maxSize=200`
- path 必须做 URL 编码（SDK 已处理）
- `maxSize` 可选，插件端会在协议层缩放后再返回二进制

## getForTrack(path, type?, options?) 

获取指定曲目封面内容（返回 `dataUrl`）。适合保存/上传/写入标签。

```javascript
const res = await fb.artwork.getForTrack('E:\\\\Music\\\\song.flac', 'front', { maxSize: 300 });
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

## withMaxSize(url, maxSize) 

工具函数：给 `fb2k://` URL 追加 `maxSize` 参数。

```javascript
const url = fb.artwork.withMaxSize('fb2k://artwork/...', 300);
// 'fb2k://artwork/...?maxSize=300'
```

批量获取封面。`paths` 为 `[{path, type?}, ...]`。

```javascript
const results = await fb.artwork.getBatch([
    { path: 'E:\\\\Music\\\\a.flac' },
    { path: 'E:\\\\Music\\\\b.mp3', type: 'back' }
]);
```

## 补全方法参考

### getAvailableArtwork(path?)

签名：`fb.artwork.getAvailableArtwork(path?: string): Promise<ArtworkAvailableResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| path | string | 否 | 音频文件路径；省略时检查当前播放曲目 |

返回值：可用封面类型与状态。

```javascript
const available = await fb.artwork.getAvailableArtwork('E:\\Music\\song.flac');
```

### getAvailableTypes(path?)

签名：`fb.artwork.getAvailableTypes(path?: string): Promise<ArtworkAvailableTypesResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| path | string | 否 | 音频文件路径；省略时检查当前播放曲目 |

返回值：可用封面类型列表。

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

返回值：指定文件的封面内容或可用状态。

```javascript
const cover = await fb.artwork.getByPath('E:\\Music\\song.flac', 'front');
```

### getFolderImages(path)

签名：`fb.artwork.getFolderImages(path: string): Promise<ArtworkFolderImagesResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| path | string | 是 | 音频文件或目录路径 |

返回值：同目录下可识别的图片文件列表。

```javascript
const images = await fb.artwork.getFolderImages('E:\\Music\\Album\\song.flac');
```

### getLyrics(path?)

签名：`fb.artwork.getLyrics(path?: string): Promise<ArtworkLyricsResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| path | string | 否 | 音频文件路径；省略时读取当前播放曲目 |

返回值：歌词文本或可用状态。

```javascript
const lyrics = await fb.artwork.getLyrics('E:\\Music\\song.flac');
```

### getMetadata(path)

签名：`fb.artwork.getMetadata(path: string): Promise<ArtworkMetadataResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| path | string | 是 | 音频文件路径 |

返回值：与封面/图片相关的元数据。

```javascript
const metadata = await fb.artwork.getMetadata('E:\\Music\\song.flac');
```
