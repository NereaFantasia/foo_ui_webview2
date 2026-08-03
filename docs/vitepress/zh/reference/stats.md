# Playcount & Rating

> 底层 API 完整参考：[Playcount API](/zh/api/playcount) · [Metadata / Rating API](/zh/api/metadata) · [Titleformat API](/zh/api/titleformat)（可用 `%play_count%`、`%rating%` 等字段）

## rating.set

设置曲目评分。优先使用 foo_playcount 上下文菜单，不可用时回退到写入文件 `RATING` 标签。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| `path` | string | ✗ | 音频文件路径（支持 `path\|subsong:N`）。省略时按 handler 规则对当前播放曲目操作 |
| `rating` | number | ✓ | `0`-`5`，`0`=取消评分 |
| `cueIndex` | number | ✗ | CUE 子曲目索引（优先级高于路径中的 `\|subsong:N`） |

::: warning 依赖说明
需要安装 [foo_playcount](https://www.foobar2000.org/components/view/foo_playcount) 插件才能获得 stats 后端评分。
:::


## rating.get

获取曲目评分。优先从 foo_playcount 读取（`%rating%` titleformat），不可用时回退读取文件 `RATING` 标签。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| `path` | string | ✓ | 音频文件路径（支持 `path\|subsong:N`） |
| `cueIndex` | number | ✗ | CUE 子曲目索引 |

**返回值**:

```json
{
  "success": true,
  "path": "C:\\Music\\song.flac",
  "rating": 4,
  "storage": "stats"
}
```

| 字段 | 类型 | 描述 |
| --- | --- | --- |
| `rating` | number | `0`-`5`，`0`=未评分 |
| `storage` | string | `"stats"`（来自 foo_playcount）或 `"file"`（来自文件标签） |


## playcount.get

获取播放统计信息（来自 foo_playcount）。详细参数和返回值见 [Playcount API](/zh/api/playcount#playcount-get)。

## playcount.getBatch

`playcount.get` 的别名，参数和返回值完全相同。见 [Playcount API](/zh/api/playcount#playcount-getbatch)。

## playcount.getStats

获取媒体库整体播放统计。详见 [Playcount API](/zh/api/playcount#playcount-getstats)。

## playcount.set

已注册的占位接口。需要 `path`，但始终返回 `success: false`，并提示改用 `rating.set` 处理评分；不会修改播放次数。

## 相关 titleformat 字段

安装 foo_playcount 后，可通过 Titleformat API 使用 `%play_count%`、`%rating%`、`%last_played%` 等字段。
