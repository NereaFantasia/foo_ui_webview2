# Library 工具 

媒体库搜索与统计。共 4 个工具。

## fb2k_library_search 

搜索媒体库。支持 foobar2000 查询语法（如 `artist IS Mili`、`%rating% GREATER 3`）。

- **Bridge 方法**: `library.search`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| query | string | ? | 搜索查询表达式 |
| offset | integer | ? | 结果偏移（默认 0） |
| limit | integer | ? | 返回数量限制（默认 50，最大 500） |

**返回值**:

```json
{
  "tracks": [
    { "title": "Redo", "artist": "Mili", "album": "Millennium Mother", "path": "..." }
  ],
  "total": 42
}
```

::: tip 查询语法
支持标准 foobar2000 查询语法：
- `artist IS Mili` — 精确匹配
- `title HAS love` — 包含匹配
- `%rating% GREATER 3` — 数值比较
- `artist IS Mili AND album IS "Miracle Milk"` — 组合查询

:::

## fb2k_library_get_albums 

获取所有专辑列表。

- **参数**: 无
- **Bridge 方法**: `library.getAlbums`

## fb2k_library_get_artists 

获取所有艺术家列表。

- **参数**: 无
- **Bridge 方法**: `library.getArtists`

## fb2k_library_get_stats 

获取媒体库统计信息。

- **参数**: 无
- **Bridge 方法**: `library.getStats`

**返回值**:

```json
{
  "totalTracks": 12345,
  "totalAlbums": 678,
  "totalArtists": 234,
  "totalDuration": 3456789
}
```
