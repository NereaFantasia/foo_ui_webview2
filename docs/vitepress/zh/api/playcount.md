# Playcount API

提供 foo_playcount 播放统计数据查询与全库指标聚合能力。

> **环境要求**：须安装 foo_playcount 组件方可获取真实播放统计频次与时间戳；未安装时统计数值将回退为默认值。

## 查询

### playcount.get

<!-- api-schema:begin playcount.get -->
批量读取曲目的 foo_playcount 播放统计指标（未安装 foo_playcount 时播放次数统一返回 0 且日期字段缺省）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `string[]` | 是 | 待查询的曲目路径列表（支持 \|subsong:N 语法指定 CUE 子曲目；若后缀索引无法解析为有效数字则自动剥除并回退指向首轨） |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `count` | `integer` | results 数组包含的统计数据行总数 |
| `results` | `PlaycountRow[]` | 逐路径统计结果列表（保持与请求路径列表相同的顺序） |
| `results[].path` | `string` | 请求时传入的原始曲目路径（在成功与失败行中均原样保留 \|subsong:N 等后缀） |
| `results[].success` | `boolean` | 宿主是否成功解析到该曲目句柄 |
| `results[].error` | `string` | 曲目解析失败的具体原因（仅在 success 为 false 时提供） |
| `results[].playCount` | `integer` | 累计播放总次数（未曾播放时为 0；仅在 success 为 true 时提供） |
| `results[].firstPlayed` | `string` | 首次播放时间戳文本（格式由 foo_playcount 决定；时间未知时缺省） |
| `results[].lastPlayed` | `string` | 最近一次播放时间戳文本（时间未知时缺省） |
| `results[].added` | `string` | 曲目被添加至媒体库的时间戳文本（时间未知时缺省） |
| `results[].rating` | `integer` | 星级评分数值（取值范围 1 至 5；未评分曲目缺省，不会返回 0） |
| `results[].inLibrary` | `boolean` | 当前曲目是否已纳入媒体库（仅在 success 为 true 时提供） |

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
批量读取曲目播放统计（功能与参数完全等同于 playcount.get，保留供兼容批量调用命名习惯）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `string[]` | 是 | 待查询的曲目路径列表（支持 \|subsong:N 语法指定 CUE 子曲目；若后缀索引无法解析为有效数字则自动剥除并回退指向首轨） |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `count` | `integer` | results 数组包含的统计数据行总数 |
| `results` | `PlaycountRow[]` | 逐路径统计结果列表（保持与请求路径列表相同的顺序） |
| `results[].path` | `string` | 请求时传入的原始曲目路径（在成功与失败行中均原样保留 \|subsong:N 等后缀） |
| `results[].success` | `boolean` | 宿主是否成功解析到该曲目句柄 |
| `results[].error` | `string` | 曲目解析失败的具体原因（仅在 success 为 false 时提供） |
| `results[].playCount` | `integer` | 累计播放总次数（未曾播放时为 0；仅在 success 为 true 时提供） |
| `results[].firstPlayed` | `string` | 首次播放时间戳文本（格式由 foo_playcount 决定；时间未知时缺省） |
| `results[].lastPlayed` | `string` | 最近一次播放时间戳文本（时间未知时缺省） |
| `results[].added` | `string` | 曲目被添加至媒体库的时间戳文本（时间未知时缺省） |
| `results[].rating` | `integer` | 星级评分数值（取值范围 1 至 5；未评分曲目缺省，不会返回 0） |
| `results[].inLibrary` | `boolean` | 当前曲目是否已纳入媒体库（仅在 success 为 true 时提供） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const result = await fb2k.invoke('playcount.getBatch', {
    paths: ['C:\\Music\\a.flac', 'C:\\Music\\b.flac']
});
```

### playcount.getStats

<!-- api-schema:begin playcount.getStats -->
汇总全媒体库曲目的播放次数与星级评分综合统计指标

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `totalTracks` | `integer` | 媒体库中索引的曲目总数 |
| `playedTracks` | `integer` | 至少播放过一次的曲目总数 |
| `unplayedTracks` | `integer` | 从未播放过的曲目总数 |
| `ratedTracks` | `integer` | 已设定星级评分（1 至 5 星）的曲目总数 |
| `totalPlayCount` | `integer` | 媒体库中所有曲目的累计播放总次数之和 |
| `maxPlayCount` | `integer` | 单首曲目的最高累计播放次数 |
| `averagePlayCount` | `number` | 已播放曲目的平均播放次数（若无任何播放记录则返回 0） |
| `averageRating` | `number` | 已评分曲目的平均星级评分（若无任何评分曲目则返回 0） |

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
尝试写入播放统计数据（占位接口：因 foo_playcount 未提供外部修改统计机制，调用恒以 NOT_SUPPORTED 失败；如需修改评分请使用 rating.set）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 目标曲目文件路径 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
// 注意：此 API 恒返回失败，修改星级评分请调用 rating.set
const result = await fb2k.invoke('playcount.set', { path: 'E:\\Music\\song.flac' });
if (result.success === false) console.log(result.error); // "Direct playcount modification not supported..."
```
