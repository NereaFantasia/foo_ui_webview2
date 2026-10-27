# Rating 评分 API

提供曲目星级评分（0 至 5 星）读取与写入能力，支持 foo_playcount 统计数据库与音频文件 RATING 标签双向降级回退。CUE 等容器内曲目的寻址见[定位容器内的单曲](./metadata.md#subsong-addressing)。

## 评分

### rating.get

<!-- api-schema:begin rating.get -->
获取指定曲目的星级评分（范围 0 至 5；优先读取 foo_playcount 维护的 1 至 5 星 %rating% 统计数据，未找到时回退读取音频文件的 RATING 标签并夹取至 0 至 5 区间；0 表示未评分；无法读取标签或文件不存在时不返回错误，统一视作无 RATING 标签处理）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 待查询曲目的文件路径（支持 \|subsong:N 或末尾 #N 语法指定子曲目）不能为空。 |
| `cueIndex` | `integer` | 否 | 子曲目索引（优先级高于 path 中的后缀；传入负数时忽略该参数）默认 `-1`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `path` | `string` | 查询时传入的原样曲目路径（包含子曲目后缀） |
| `rating` | `integer` | 曲目星级评分（取值范围 0 至 5，0 表示未评分） |
| `storage` | `"stats" \| "file"` | 评分数据存储来源（stats 表示来自 foo_playcount 统计数据库；file 表示来自音频文件 RATING 标签，两处均未评分时亦返回 file） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### rating.set

<!-- api-schema:begin rating.set -->
设置指定曲目的星级评分（优先通过 foo_playcount 上下文菜单写入统计，菜单不可用时回退写入音频文件的 RATING 标签；省略 path 时优先为正在播放的曲目评分，无播放曲目时回退为当前活动播放列表的选中项，两者均无时返回 NO_ACTIVE_ITEM 失败）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 否 | 待评分曲目的文件路径（支持 \|subsong:N 或末尾 #N 语法指定子曲目；省略时优先对正在播放曲目评分，无播放曲目时对活动播放列表选中项评分）不能为空。 |
| `rating` | `integer` | 是 | 待设置的星级评分（取值范围 0 至 5，传入 0 表示清除评分）取值 `0` 到 `5`（含端点）。 |
| `cueIndex` | `integer` | 否 | 子曲目索引（优先级高于 path 中的后缀；传入负数时忽略该参数）默认 `-1`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `path` | `string` | 实际评分的曲目路径（省略 path 且由 foo_playcount 处理时固定为 (current)） |
| `rating` | `integer` | 实际成功设置的星级评分数值 |
| `storage` | `"stats" \| "file"` | 实际生效的评分写入目标（stats 表示通过 foo_playcount 上下文菜单写入；file 表示派发写入音频文件 RATING 标签） |
| `menuPath` | `string` | 实际执行的上下文菜单命令路径（仅在 storage 为 stats 时提供） |
| `note` | `string` | 回退写入文件标签的具体原因说明（仅在 storage 为 file 时提供） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
await fb2k.invoke('rating.set', { path: 'C:\\Music\\song.flac', rating: 5 });
await fb2k.invoke('rating.set', { rating: 0 }); // 清除当前播放曲目评分
```

## 评分存储机制 {#使用说明}

- `rating.set` 评分范围为 `0` 至 `5` 整数，传入 `0` 表示清除评分。宿主优先检索并执行 foo_playcount 上下文菜单命令将评分记录至统计数据库；若未安装该组件或未匹配到菜单，则回退派发写入音频文件元数据 `RATING` 标签。`rating.get` 通过 `storage` 字段标识当前读取来源（`stats` 或 `file`）。
