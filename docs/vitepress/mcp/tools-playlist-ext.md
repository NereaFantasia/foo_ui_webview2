# Playlist Extended 工具 

播放列表扩展操作：选区、排序、自动播放列表等。共 40 个工具。

## 查询 

### fb2k_playlist_get_count 

获取播放列表总数。

- **参数**: 无
- **Bridge 方法**: `playlist.getCount`

**返回值**:

```json
{ "count": 5 }
```

### fb2k_playlist_get_playing 

获取正在播放的播放列表信息。结构与 `playlist.getActive` 相同。

- **参数**: 无
- **Bridge 方法**: `playlist.getPlaying`

**返回值**:

```json
{
  "index": 0,
  "name": "Default",
  "trackCount": 128,
  "isActive": true,
  "isPlaying": true,
  "isLocked": false,
  "duration": 34567.8
}
```

### fb2k_playlist_get_track_count 

获取播放列表曲目数。

- **Bridge 方法**: `playlist.getTrackCount`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| playlist | integer | ? | 播放列表索引（默认活动列表） |

**返回值**:

```json
{ "count": 128 }
```

### fb2k_playlist_get_available_columns 

获取可用列定义。

- **参数**: 无
- **Bridge 方法**: `playlist.getAvailableColumns`

## 管理 

### fb2k_playlist_rename 

重命名播放列表。

- **Bridge 方法**: `playlist.rename`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| playlist | integer | ? | 播放列表索引 |
| name | string | ? | 新名称 |

### fb2k_playlist_clear 

清空播放列表。

- **Bridge 方法**: `playlist.clear`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| playlist | integer | ? | 播放列表索引 |

### fb2k_playlist_duplicate 

复制播放列表。

- **Bridge 方法**: `playlist.duplicate`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| playlist | integer | ? | 源播放列表索引 |
| name | string | ? | 新名称 |

### fb2k_playlist_reorder_playlists 

重排播放列表顺序。

- **Bridge 方法**: `playlist.reorderPlaylists`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| newOrder | array | ? | 新的播放列表索引顺序 |

## 曲目操作 

### fb2k_playlist_insert_tracks 

插入曲目到播放列表。

- **Bridge 方法**: `playlist.insertTracks`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| handles | array | ? | 曲目句柄数组 |
| playlist | integer | ? | 目标播放列表 |
| position | integer | ? | 插入位置 |

### fb2k_playlist_remove_tracks 

按索引移除曲目。

- **Bridge 方法**: `playlist.removeTracks`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| items | array | ? | 要移除的曲目索引数组 |
| playlist | integer | ? | 播放列表索引 |

### fb2k_playlist_remove_selected_tracks 

移除选中的曲目。

- **Bridge 方法**: `playlist.removeSelectedTracks`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| playlist | integer | ? | 播放列表索引 |

### fb2k_playlist_move_tracks 

移动曲目位置。

- **Bridge 方法**: `playlist.moveTracks`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| delta | integer | ? | 移动距离（正数下移，负数上移） |
| playlist | integer | ? | 播放列表索引 |
| items | array | ? | 要移动的曲目索引数组 |

### fb2k_playlist_reorder 

自定义曲目排序。

- **Bridge 方法**: `playlist.reorder`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| newOrder | array | ? | 新的曲目索引顺序 |
| playlist | integer | ? | 播放列表索引 |

### fb2k_playlist_reverse 

反转曲目顺序。

- **Bridge 方法**: `playlist.reverse`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| playlist | integer | ? | 播放列表索引 |

### fb2k_playlist_sort 

按格式模式排序。

- **Bridge 方法**: `playlist.sort`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| playlist | integer | ? | 播放列表索引 |
| pattern | string | ? | Title Formatting 排序模式 |
| descending | boolean | ? | 是否降序 |
| selectedOnly | boolean | ? | 是否仅排序选中项 |

::: tip Title Formatting 模式示例

- `%title%` — 按标题排序
- `%album artist% - %album% - %tracknumber%` — 按专辑+曲号排序
- `%rating%` — 按评分排序

:::

### fb2k_playlist_shuffle 

随机打乱曲目顺序。

- **Bridge 方法**: `playlist.shuffle`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| playlist | integer | ? | 播放列表索引 |

## 添加曲目 

### fb2k_playlist_add_paths 

按文件路径添加曲目。

- **Bridge 方法**: `playlist.addPaths`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| paths | array | ? | 文件路径数组 |
| playlist | integer | ? | 目标播放列表 |

### fb2k_playlist_add_handles 

按句柄添加曲目。

- **Bridge 方法**: `playlist.addHandles`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| handles | array | ? | 曲目句柄数组 |
| playlist | integer | ? | 目标播放列表 |

### fb2k_playlist_add_paths_sequential 

顺序添加文件路径（保持顺序）。

- **Bridge 方法**: `playlist.addPathsSequential`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| paths | array | ? | 文件路径数组 |
| playlist | integer | ? | 目标播放列表 |

### fb2k_playlist_add_paths_async 

异步添加文件路径。

- **Bridge 方法**: `playlist.addPathsAsync`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| paths | array | ? | 文件路径数组 |
| playlist | integer | ? | 目标播放列表 |

### fb2k_playlist_replace_all_and_play 

原子替换播放列表内容并播放。

- **Bridge 方法**: `playlist.replaceAllAndPlay`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| paths | array | ? | 文件路径数组 |
| playlist | integer | ? | 目标播放列表 |
| playIndex | integer | ? | 开始播放的索引 |
| stopFirst | boolean | ? | 是否先停止播放 |
| autoPlay | boolean | ? | 是否自动开始播放 |

## 选区 

### fb2k_playlist_get_selected_tracks 

获取选中曲目的详细信息。

- **Bridge 方法**: `playlist.getSelectedTracks`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| playlist | integer | ? | 播放列表索引 |

### fb2k_playlist_get_selection 

获取选中曲目索引。

- **Bridge 方法**: `playlist.getSelection`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| playlist | integer | ? | 播放列表索引 |

### fb2k_playlist_set_selection 

设置选中项。

- **Bridge 方法**: `playlist.setSelection`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| indices | array | ? | 要选中的曲目索引数组 |
| playlist | integer | ? | 播放列表索引 |
| clearOthers | boolean | ? | 是否清除其他选中项 |

### fb2k_playlist_select_all 

全选播放列表中的曲目。

- **Bridge 方法**: `playlist.selectAll`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| playlist | integer | ? | 播放列表索引 |

### fb2k_playlist_deselect_all 

取消所有选中。

- **Bridge 方法**: `playlist.deselectAll`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| playlist | integer | ? | 播放列表索引 |

## 焦点 

### fb2k_playlist_get_focused_track 

获取焦点曲目索引。

- **Bridge 方法**: `playlist.getFocusedTrack`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| playlist | integer | ? | 播放列表索引 |

### fb2k_playlist_set_focused_track 

设置焦点曲目。

- **Bridge 方法**: `playlist.setFocusedTrack`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| index | integer | ? | 曲目索引 |
| playlist | integer | ? | 播放列表索引 |

### fb2k_playlist_focus_track 

::: warning 已弃用
请使用 `fb2k_playlist_set_focused_track` 代替。
:::

- **Bridge 方法**: `playlist.focusTrack`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| playlist | integer | ? | 播放列表索引 |
| index | integer | ? | 曲目索引 |

### fb2k_playlist_get_focus_track 

::: warning 已弃用
请使用 `fb2k_playlist_get_focused_track` 代替。
:::

- **Bridge 方法**: `playlist.getFocusTrack`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| playlist | integer | ? | 播放列表索引 |

## 撤销 / 重做 

### fb2k_playlist_undo 

撤销播放列表操作。

- **Bridge 方法**: `playlist.undo`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| playlist | integer | ? | 播放列表索引 |

### fb2k_playlist_redo 

重做播放列表操作。

- **Bridge 方法**: `playlist.redo`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| playlist | integer | ? | 播放列表索引 |

## 锁定 

### fb2k_playlist_get_lock_info 

获取播放列表锁定信息。

- **Bridge 方法**: `playlist.getLockInfo`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| playlist | integer | ? | 播放列表索引 |

### fb2k_playlist_is_locked 

检查播放列表是否锁定。

- **Bridge 方法**: `playlist.isLocked`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| playlist | integer | ? | 播放列表索引 |

## 自动播放列表 

### fb2k_playlist_is_autoplaylist 

检查是否为自动播放列表。

- **Bridge 方法**: `playlist.isAutoplaylist`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| playlist | integer | ? | 播放列表索引 |

### fb2k_playlist_create_autoplaylist 

创建自动播放列表。

- **Bridge 方法**: `playlist.createAutoplaylist`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| query | string | ? | 查询表达式（同 library.search 语法） |
| name | string | ? | 播放列表名称 |

::: tip 自动播放列表
自动播放列表的内容由查询表达式自动填充，不可手动编辑曲目。示例：`artist IS Mili`。
:::

| `sort` | `string` | ? | 排序模式 | | `keepSorted` | `boolean` | ? | 是否保持排序 |

### fb2k_playlist_convert_to_autoplaylist 

将现有播放列表转换为自动播放列表。

- **Bridge 方法**: `playlist.convertToAutoplaylist`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| query | string | ? | 查询表达式 |
| playlist | integer | ? | 目标播放列表 |
| sort | string | ? | 排序模式 |
| keepSorted | boolean | ? | 是否保持排序 |

### fb2k_playlist_remove_autoplaylist 

移除自动播放列表状态（保留曲目）。

- **Bridge 方法**: `playlist.removeAutoplaylist`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| playlist | integer | ? | 播放列表索引 |

### fb2k_playlist_get_autoplaylist_info 

获取自动播放列表详细信息。

- **Bridge 方法**: `playlist.getAutoplaylistInfo`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| playlist | integer | ? | 播放列表索引 |

### fb2k_playlist_get_autoplaylist_query 

获取自动播放列表的查询表达式。

- **Bridge 方法**: `playlist.getAutoplaylistQuery`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| playlist | integer | ? | 播放列表索引 |
