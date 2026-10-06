# Selection 选择 API

`selection` 命名空间的方法：读取与设置全局选择，并解析查看器应当显示的曲目。

## Selection API - 选择同步

### selection.getViewerMode

<!-- api-schema:begin selection.getViewerMode -->
报告用户的 Selection Viewer 偏好倾向哪个来源；由当前的选择类型推导，不是读取某项设置。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `mode` | `"prefer_playing" \| "prefer_selection"` | 选择类型是正在播放的曲目时为 `prefer_playing`，否则为 `prefer_selection`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### selection.getViewingTrack

<!-- api-schema:begin selection.getViewingTrack -->
解析查看器应当显示的曲目：先按偏好来源，没有曲目时回退到另一来源。恒成功，有没有东西可显示看 `found`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `includeTrackInfo` | `boolean` | 否 | 是否把完整的曲目行作为 `track` 一并返回。默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `found` | `boolean` | 两个来源里是否有曲目。 |
| `mode` | `"prefer_playing" \| "prefer_selection"` | 本次解析依据的偏好。 |
| `source` | `"now_playing" \| "selection"` | 曲目来自哪个来源；`found` 为 false 时缺省。 |
| `handle` | `string` | 曲目的 handle（原生路径，子曲目不为 `0` 时附加 `\|subsong:N`）；`found` 为 false 时缺省。 |
| `playlistIndex` | `integer` | 曲目所在的播放列表；不知道它在哪一行时缺省。正在播放的曲目取播放时的来源位置；选中的曲目只在活动播放列表里找，所以在别处（媒体库、另一张列表）做的选择没有行。恰在 `itemIndex` 出现时出现。 |
| `playlistGuid` | `string` | 该播放列表的 GUID，写法与 `playlistGuid` 接受的一致；恰在 `playlistIndex` 出现时出现。 |
| `itemIndex` | `integer` | 曲目在该播放列表中的行号，取第一处；`playlistIndex` 缺省时缺省。 |
| `track` | [Track](../reference/types.md#track) | 完整的曲目行；只在 `includeTrackInfo` 时返回。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

**Fallback 逻辑**:

- `prefer_playing`: 优先返回正在播放 → 回退到当前选择
- `prefer_selection`: 优先返回当前选择 → 回退到正在播放
- 均无: 返回 `found: false`

```javascript
const r = await fb2k.invoke('selection.getViewingTrack', { includeTrackInfo: true });
if (r.success === false) throw new Error(r.error);
if (r.found) {
    console.log(`显示: ${r.handle} (来源: ${r.source})`);
}
```

### selection.get

<!-- api-schema:begin selection.get -->
按页读取当前的全局选择（handle 列表）。只读状态，不做修改。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `offset` | `integer` | 否 | 返回的第一个 handle 的下标。不小于 `0`。默认 `0`。 |
| `limit` | `integer` | 否 | 最多返回的 handle 数；`0` 表示从 `offset` 起的全部。省略时按 100 封顶，`truncated` 报告上限是否生效。不小于 `0`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `count` | `integer` | 整个选择的条数，不是本页的条数。 |
| `type` | `"now_playing" \| "active_playlist_selection" \| "active_playlist" \| "playlist_manager" \| "media_library_viewer" \| "unknown"` | 选择的来源。 |
| `handles` | `string[]` | 本页的 handle：原生路径，子曲目不为 `0` 时附加 `\|subsong:N`。 |
| `offset` | `integer` | 实际生效的 `offset`。 |
| `hasMore` | `boolean` | 本页之后是否还有条目。 |
| `truncated` | `boolean` | 只在省略 `limit` 且选择超过 100 条时出现，且为 true。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

::: warning 性能提示
未指定 `limit` 时，选择超过 100 个曲目会自动截断为 100 条。显式传入的 `limit` **不会**被收窄，需要全部数据请传 `limit: 0`。
:::

`truncated: true` 仅在自动上限**真正生效**时出现，即选择数超过 100 **且**未传 `limit`。在 250 条选择中显式传 `limit: 10` 不属于截断，不会带该字段。它从不以 `false` 出现，因此应判断该字段是否存在而非判断其取值；是否需要继续分页请看 `hasMore`。`count` 是选择的总数，不是本次返回的条数。

```js
const res = await fb2k.invoke('selection.get', { limit: 0 });
if (res.success === false) throw new Error(res.error);
const { handles, count, hasMore } = res;
```

### selection.getType

<!-- api-schema:begin selection.getType -->
报告当前全局选择的类型，同时给出数字序号与名称。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `type` | `integer` | 类型的数字序号：`0` now_playing、`1` active_playlist_selection、`2` active_playlist、`3` playlist_manager、`5` media_library_viewer，未知时为 `0`。 |
| `typeName` | `"now_playing" \| "active_playlist_selection" \| "active_playlist" \| "playlist_manager" \| "media_library_viewer" \| "unknown"` | 类型名。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

| type | typeName | 说明 |
| --- | --- | --- |
| 0 | now_playing | 正在播放 |
| 1 | active_playlist_selection | 活动播放列表的选择 |
| 2 | active_playlist | 活动播放列表 |
| 3 | playlist_manager | 播放列表管理器 |
| 5 | media_library_viewer | 媒体库查看器 |

### selection.set

<!-- api-schema:begin selection.set -->
用给定曲目替换全局选择。handle 按字符串铸出、不做查找，不存在的路径同样会被选中。宿主只在调用期间持有选择持有者；foobar2000 在没有任何持有者时清空选择，所以选择要在调用结束后留下，得有别的持有者在，比如开着 `grabFocus` 且有焦点的调用方面板自己持有的那个。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `handles` | `string[]` | 是 | 要选中的曲目，原生路径加可选的 `\|subsong:N` 后缀。后缀不是数字时按子曲目 `0` 处理。不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `count` | `integer` | 交给选择持有者的曲目数，即能铸出 handle 的条数。不是回读的结果，选择能留多久见 `set`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

`|subsong:` 后缀格式错误时回退为 subsong `0`。`handles` 缺失、元素不是字符串、为空数组，或无一条能铸出 handle，都以 `INVALID_PARAMS` 拒绝；取不到选择持有者是 `OPERATION_FAILED`。

### selection.setPlaylistTracking

<!-- api-schema:begin selection.setPlaylistTracking -->
把选择设为整个活动播放列表，或该列表自己的选中行。foobar2000 在发起跟踪的持有者被释放时结束跟踪，而宿主在调用返回时就释放它，所以之后播放列表的变化不会跟进；选择本身能留多久同 `set`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `mode` | `"selection" \| "playlist"` | 否 | `playlist` 取整个活动播放列表；`selection` 取其中的选中行。默认 `"selection"`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `mode` | `"selection" \| "playlist"` | 实际生效的模式。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

| mode 值 | 说明 |
| --- | --- |
| `selection` | 跟踪播放列表中用户选择的曲目 |
| `playlist` | 跟踪整个播放列表 |

`mode` 只接受这两个值，其他取值以 `INVALID_PARAMS` 拒绝。

## 选择行为

`selection.getViewerMode` 返回 `prefer_playing` 或 `prefer_selection`，该值由当前的选择类型推导，而非一项独立设置。`selection.getViewingTrack` 先按该偏好选择来源；若首选来源没有曲目，则回退到另一个来源——它**恒返回** `success: true`，请改判 `found`。传 `includeTrackInfo` 时它的 `track` 是共享的 [Track](../reference/types.md#track) 行。`selection:changed` 在选择更新后广播到每个 WebView，并有 50 ms 节流；其 payload 见事件参考页。

队列与选择使用同一种 handle 字符串形式：原生路径，仅当 subsong 大于 `0` 时才附加 `|subsong:N`。传给 `queue.addPaths` 或 JIT Queue 操作的路径接受同样的后缀。单条路径或 URL 上限为 2048 字符。
