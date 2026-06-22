# Playcount API 

foo_playcount 播放统计数据查询。共 4 个 API。

> 需要安装 foo_playcount 组件才能获取完整数据。

## 查询 

### playcount.get 

获取文件的播放统计数据。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| paths | string[] | ✓ | 文件路径数组（支持 \|subsong:N 格式） |

**返回值**:

```json
{
    "success": true,
    "count": 2,
    "results": [
        {
            "path": "C:\\\\Music\\\\song.flac",
            "success": true,
            "playCount": 42,
            "firstPlayed": "2025-01-15 10:30:00",
            "lastPlayed": "2026-02-09 12:00:00",
            "added": "2025-01-10 08:00:00",
            "rating": 5,
            "inLibrary": true
        }
    ]
}
```

| 字段 | 类型 | 描述 |
| --- | --- | --- |
| playCount | number | 播放次数 |
| firstPlayed | string | 首次播放时间 |
| lastPlayed | string | 最后播放时间 |
| added | string | 添加到媒体库时间 |
| rating | number | 评分 (1-5, 省略则未评分) |
| inLibrary | boolean | 是否在媒体库中 |

```javascript
const result = await fb2k.invoke('playcount.get', {
    paths: ['C:\\\\Music\\\\song.flac']
});
console.log(`播放次数: ${result.results[0].playCount}`);
```

### playcount.getBatch 

`playcount.get` 的别名，参数和返回值完全相同。

```javascript
const result = await fb2k.invoke('playcount.getBatch', {
    paths: ['C:\\\\Music\\\\a.flac', 'C:\\\\Music\\\\b.flac']
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

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| path | string | ✓ | 音频文件路径 |

**返回值**: `{ "success": false, "error": "Direct playcount modification not supported. Use rating.set for ratings." }`

> 缺少 `path` 参数时返回 `{ "success": false, "error": "path is required" }`

```javascript
// 注意：此 API 始终返回失败，评分请使用 rating.set
const result = await fb2k.invoke('playcount.set', { path: 'E:\\\\Music\\\\song.flac' });
console.log(result.error); // "Direct playcount modification not supported..."
```
