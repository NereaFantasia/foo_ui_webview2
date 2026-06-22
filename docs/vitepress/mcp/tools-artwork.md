# Artwork 工具 

封面图片获取。共 2 个工具。返回 base64 编码的图片数据。

## fb2k_artwork_get_current 

获取当前播放曲目的封面。

- **Bridge 方法**: `artwork.getCurrent`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| type | string | ✗ | 封面类型（见下方表格，默认 front） |

**返回值**（有封面时）:

```json
{
  "available": true,
  "type": "front",
  "source": "album_art_manager_v2",
  "mimeType": "image/jpeg",
  "size": 245678,
  "dataUrl": "data:image/jpeg;base64,/9j/4AAQ..."
}
```

| 字段 | 类型 | 描述 |
| --- | --- | --- |
| available | boolean | 是否有可用封面 |
| type | string | 封面类型 |
| source | string | 封面来源（now_playing_manager / album_art_manager_v2 / direct_extractor） |
| mimeType | string | MIME 类型（image/jpeg / image/png / image/webp） |
| size | integer | 图片大小（字节） |
| dataUrl | string | base64 编码的 Data URL |

**返回值**（无封面时）:

```json
{ "available": false, "type": "front", "reason": "no_track" }
```

## fb2k_artwork_get_for_track 

获取指定曲目的封面。

- **Bridge 方法**: `artwork.getForTrack`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| path | string | ✓ | 文件路径 |
| type | string | ✗ | 封面类型（默认 front） |

**返回值**: 结构同 `fb2k_artwork_get_current`。

## 封面类型 

| 值 | 说明 |
| --- | --- |
| front | 正面封面（默认） |
| back | 背面封面 |
| disc | 光盘图 |
| icon | 图标 |
| artist | 艺术家图片 |
