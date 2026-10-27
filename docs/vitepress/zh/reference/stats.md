# Playcount & Rating

> 底层 API 完整参考：[Playcount API](/zh/api/playcount) · [Rating API](/zh/api/rating) · [Titleformat API](/zh/api/titleformat)（可用 `%play_count%`、`%rating%` 等字段）

## rating.set

设置曲目评分。优先调用 foo_playcount 上下文菜单；若未安装该组件则回退至向音频文件写入 `RATING` 元数据标签。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | string | 否 | 音频文件路径（支持 `path\|subsong:N` 格式）；省略时按 handler 规则默认操作当前播放曲目 |
| `rating` | number | 是 | 评分星级（`0`–`5`，`0` 表示清除评分） |
| `cueIndex` | number | 否 | CUE 单轨子曲目索引（优先级高于路径中的 `\|subsong:N` 标识） |

::: warning 依赖说明
需要安装 [foo_playcount](https://www.foobar2000.org/components/view/foo_playcount) 插件才能获得 stats 后端评分。
:::


## rating.get

获取曲目评分。优先从 foo_playcount 统计数据库中读取（对应 `%rating%` TitleFormat），不可用时回退读取音频文件 `RATING` 标签。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | string | 是 | 音频文件路径（支持 `path\|subsong:N` 格式） |
| `cueIndex` | number | 否 | CUE 单轨子曲目索引 |

**返回值**:

```json
{
  "success": true,
  "path": "C:\\Music\\song.flac",
  "rating": 4,
  "storage": "stats"
}
```

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `rating` | number | 评分星级（`0`–`5`，`0` 表示未评分） |
| `storage` | string | 评分存储来源：`"stats"`（源自 foo_playcount 数据库）或 `"file"`（源自音频文件元数据标签） |


## playcount.get

获取播放统计信息（源自 foo_playcount）。详细参数与返回值契约见 [Playcount API](/zh/api/playcount#playcount-get)。

## playcount.getBatch

`playcount.get` 的功能别名，参数与返回数据结构完全一致，详见 [Playcount API](/zh/api/playcount#playcount-getbatch)。

## playcount.getStats

获取媒体库全量播放统计聚合数据，详见 [Playcount API](/zh/api/playcount#playcount-getstats)。

## playcount.set

已注册的占位兼容端点。必须传入 `path` 参数但始终返回 `success: false`，并提示调用方使用 `rating.set` 处理评分；本接口不修改播放次数。

## 相关 titleformat 字段

安装 foo_playcount 插件后，可通过 Titleformat API 评估并读取 `%play_count%`、`%rating%`、`%last_played%` 等扩展统计字段。
