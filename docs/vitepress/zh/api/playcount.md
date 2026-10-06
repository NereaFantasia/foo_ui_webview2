# Playcount API

foo_playcount 播放统计数据查询。

> 需要安装 foo_playcount 组件才能获取完整数据。

## 查询

### playcount.get

<!-- api-schema:begin playcount.get -->
读取曲目的 foo_playcount 播放统计。没装 foo_playcount 时次数读作 0，日期不出现。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `string[]` | 是 | 曲目路径；带 `\|subsong:N` 后缀时选 CUE 子曲目。读不出序号的后缀（如 `\|subsong:abc`）被去掉，路径指第一首。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `count` | `integer` | `results` 的行数。 |
| `results` | `PlaycountRow[]` | 每个请求路径一行，按请求顺序排列。 |
| `results[].path` | `string` | 请求时给的路径，后缀 `\|subsong:N` 原样保留；成功行与失败行都一样。 |
| `results[].success` | `boolean` | 是否解析到了这首曲目。 |
| `results[].error` | `string` | 解析不到这首曲目的原因；`success` 为 `false` 时出现。 |
| `results[].playCount` | `integer` | 播放次数，从未播放时为 `0`。`success` 为 `true` 时出现。 |
| `results[].firstPlayed` | `string` | 首次播放时间，格式由 foo_playcount 决定。未知时不出现。 |
| `results[].lastPlayed` | `string` | 最近一次播放时间。未知时不出现。 |
| `results[].added` | `string` | 曲目加入媒体库的时间。未知时不出现。 |
| `results[].rating` | `integer` | 评分，1 到 5。未评分时不出现，不会是 `0`。 |
| `results[].inLibrary` | `boolean` | 曲目是否在媒体库中。`success` 为 `true` 时出现。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const result = await fb2k.invoke('playcount.get', {
    paths: ['C:\\Music\\song.flac']
});
if (result.success === false) throw new Error(result.error);
console.log(`播放次数: ${result.results[0].playCount}`);
```

### playcount.getBatch

<!-- api-schema:begin playcount.getBatch -->
与 `playcount.get` 相同，保留给按批调用的写法。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `string[]` | 是 | 曲目路径；带 `\|subsong:N` 后缀时选 CUE 子曲目。读不出序号的后缀（如 `\|subsong:abc`）被去掉，路径指第一首。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `count` | `integer` | `results` 的行数。 |
| `results` | `PlaycountRow[]` | 每个请求路径一行，按请求顺序排列。 |
| `results[].path` | `string` | 请求时给的路径，后缀 `\|subsong:N` 原样保留；成功行与失败行都一样。 |
| `results[].success` | `boolean` | 是否解析到了这首曲目。 |
| `results[].error` | `string` | 解析不到这首曲目的原因；`success` 为 `false` 时出现。 |
| `results[].playCount` | `integer` | 播放次数，从未播放时为 `0`。`success` 为 `true` 时出现。 |
| `results[].firstPlayed` | `string` | 首次播放时间，格式由 foo_playcount 决定。未知时不出现。 |
| `results[].lastPlayed` | `string` | 最近一次播放时间。未知时不出现。 |
| `results[].added` | `string` | 曲目加入媒体库的时间。未知时不出现。 |
| `results[].rating` | `integer` | 评分，1 到 5。未评分时不出现，不会是 `0`。 |
| `results[].inLibrary` | `boolean` | 曲目是否在媒体库中。`success` 为 `true` 时出现。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const result = await fb2k.invoke('playcount.getBatch', {
    paths: ['C:\\Music\\a.flac', 'C:\\Music\\b.flac']
});
```

### playcount.getStats

<!-- api-schema:begin playcount.getStats -->
汇总整个媒体库的播放次数与评分。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `totalTracks` | `integer` | 媒体库里的曲目数。 |
| `playedTracks` | `integer` | 至少播放过一次的曲目数。 |
| `unplayedTracks` | `integer` | 从未播放过的曲目数。 |
| `ratedTracks` | `integer` | 有评分的曲目数。 |
| `totalPlayCount` | `integer` | 所有播放次数之和。 |
| `maxPlayCount` | `integer` | 单首曲目的最高播放次数。 |
| `averagePlayCount` | `number` | 已播放曲目的平均播放次数；一首都没播放过时为 `0`。 |
| `averageRating` | `number` | 有评分曲目的平均评分；一首都没有评分时为 `0`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

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
if (stats.success === false) throw new Error(stats.error);
console.log(`已播放: ${stats.playedTracks}/${stats.totalTracks}`);
```

## 写入

### playcount.set

<!-- api-schema:begin playcount.set -->
占位方法：foo_playcount 不提供修改统计的途径，所以总是以 `NOT_SUPPORTED` 失败。改评分请用 `rating.set`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 曲目路径。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
// 注意：此 API 始终返回失败，评分请使用 rating.set
const result = await fb2k.invoke('playcount.set', { path: 'E:\\Music\\song.flac' });
if (result.success === false) console.log(result.error); // "Direct playcount modification not supported..."
```
