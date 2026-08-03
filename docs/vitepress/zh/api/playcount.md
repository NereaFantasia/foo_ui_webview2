# Playcount API

foo_playcount 播放统计数据查询。共 4 个 API。

> 需要安装 foo_playcount 组件才能获取完整数据。

## 查询

### playcount.get

获取文件的播放统计数据。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `array` | 是 | 必填。 |

**返回值**: `{"count":0,"results":[],"success":true}`


| 字段 | 类型 | 描述 |
| --- | --- | --- |
| `playCount` | number | playCount |
| `firstPlayed` | string | 首次播放时间 |
| `lastPlayed` | string | lastPlayed |
| `added` | string | 添加到媒体库时间 |
| `rating` | number | rating |
| `inLibrary` | boolean | 是否在媒体库中 |

```javascript
const result = await fb2k.invoke('playcount.get', {
    paths: ['C:\\Music\\song.flac']
});
console.log(`播放次数: ${result.results[0].playCount}`);
```

### playcount.getBatch

`playcount.get` 的别名，参数和返回值完全相同；handler 直接转发同一公开载荷。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `array` | 是 | 必填文件路径列表；条目可使用 `path|subsong:N`。 |


**返回值**: `{"count":0,"error":"...","results":[],"success":true}`

```javascript
const result = await fb2k.invoke('playcount.getBatch', {
    paths: ['C:\\Music\\a.flac', 'C:\\Music\\b.flac']
});
```

### playcount.getStats

获取媒体库整体播放统计。

- **参数**: 无

**返回值**:

```json
{
    "success": true,
    "totalTracks": 5000,
    "playedTracks": 3200,
    "unplayedTracks": 1800,
    "ratedTracks": 800,
    "totalPlayCount": 15000,
    "maxPlayCount": 120,
    "averagePlayCount": 4.7,
    "averageRating": 3.8
}
```

```javascript
const stats = await fb2k.invoke('playcount.getStats');
console.log(`已播放: ${stats.playedTracks}/${stats.totalTracks}`);
```

## 写入

### playcount.set

占位 API。foo_playcount 不提供直接修改接口，评分请使用 `rating.set`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 必填。 |

**返回值**: `{ "success": false, "error": "Direct playcount modification not supported. Use rating.set for ratings." }`

> 缺少 `path` 参数时返回 `{ "success": false, "error": "path is required" }`

```javascript
// 注意：此 API 始终返回失败，评分请使用 rating.set
const result = await fb2k.invoke('playcount.set', { path: 'E:\\Music\\song.flac' });
console.log(result.error); // "Direct playcount modification not supported..."
```

## 使用说明

- `playcount.get` 和 `playcount.getBatch` 都需要 `paths` 数组。每个有效结果含原始 `path`、`success`、`playCount` 和 `inLibrary`；`firstPlayed`、`lastPlayed`、`added` 和 `rating` 只会在 foo_playcount 提供有效值时出现。
- 输入路径可以使用 `path|subsong:N`。handler 会分别解析文件路径和 subsong，并在对应结果项中保留原始路径。
- `playcount.set` 需要 `path`，但它有意保留为占位 API：始终返回 `success: false`，并提示使用 `rating.set` 修改评分；它不会修改播放次数。
- `playcount.getStats` 没有参数，返回 `{ success, totalTracks, playedTracks, unplayedTracks, ratedTracks, totalPlayCount, maxPlayCount, averagePlayCount, averageRating }`。
